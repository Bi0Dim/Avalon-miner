/**
 * @file main.c
 * @brief Главный цикл прошивки Avalon 1126 Pro
 * 
 * Основной алгоритм работы:
 * 1. Инициализация оборудования (SPI, сеть, чипы)
 * 2. Подключение к серверу Quaxis
 * 3. Цикл майнинга:
 *    - Получение заданий от сервера
 *    - Загрузка в чипы
 *    - Опрос результатов
 *    - Отправка shares
 */

#include "config.h"
#include "protocol.h"
#include "sha256.h"
#include "a1126_driver.h"
#include "network.h"
#include "spi.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Глобальные переменные */
static quaxis_job_t g_current_job;
static uint8_t g_target[32];
static volatile int g_running = 1;
static uint64_t g_shares_found = 0;
static uint64_t g_shares_sent = 0;

/* Статистика */
static uint32_t g_last_log_time = 0;

/**
 * @brief Получить текущее время в миллисекундах
 * 
 * @note Заглушка - нужна реализация для конкретной платформы
 */
static uint32_t get_time_ms(void) {
    /* TODO: Реализовать для конкретной платформы */
    static uint32_t time = 0;
    return time++;
}

/**
 * @brief Задержка в миллисекундах
 */
static void delay_ms(uint32_t ms) {
    /* TODO: Реализовать для конкретной платформы */
    (void)ms;
}

/**
 * @brief Вывести сообщение в лог
 */
static void log_message(const char* msg) {
#if ENABLE_DEBUG_LOG
    printf("[QUAXIS] %s\n", msg);
#else
    (void)msg;
#endif
}

/**
 * @brief Вывести статистику
 */
static void log_stats(void) {
#if ENABLE_HASHRATE_LOG
    uint32_t hashrate = a1126_get_hashrate();
    uint8_t temp = a1126_get_temperature();
    
    printf("[STATS] Hashrate: %u H/s, Temp: %u°C, Shares: %llu/%llu\n",
           hashrate, temp, 
           (unsigned long long)g_shares_found,
           (unsigned long long)g_shares_sent);
#endif
}

/**
 * @brief Инициализация оборудования
 */
static int init_hardware(void) {
    log_message("Инициализация SPI...");
    if (spi_init(SPI_CLOCK_HZ, SPI_MODE) != 0) {
        log_message("Ошибка инициализации SPI");
        return -1;
    }
    
    log_message("Инициализация чипов A1126...");
    if (a1126_init() != 0) {
        log_message("Ошибка инициализации чипов");
        return -1;
    }
    
    log_message("Самодиагностика чипов...");
    int working_chips = a1126_self_test();
    if (working_chips <= 0) {
        log_message("Не найдено рабочих чипов");
        return -1;
    }
    
    printf("[INFO] Обнаружено %d рабочих чипов\n", working_chips);
    
    return 0;
}

/**
 * @brief Инициализация сети
 */
static int init_network(const char* server_ip, uint16_t port) {
    log_message("Инициализация сети...");
    if (net_init() != 0) {
        log_message("Ошибка инициализации сети");
        return -1;
    }
    
    printf("[INFO] Подключение к %s:%u...\n", server_ip, port);
    if (net_connect(server_ip, port) != 0) {
        log_message("Ошибка подключения к серверу");
        return -1;
    }
    
    log_message("Подключено к серверу");
    return 0;
}

/**
 * @brief Обработать полученное задание
 */
static void process_job(const quaxis_job_t* job) {
    /* Сохраняем текущее задание */
    memcpy(&g_current_job, job, sizeof(quaxis_job_t));
    
#if ENABLE_DEBUG_LOG
    printf("[JOB] ID: %u, timestamp: %u, bits: 0x%08X\n",
           job->job_id, job->timestamp, job->bits);
#endif
    
    /* Останавливаем текущий майнинг */
    a1126_stop();
    
    /* Загружаем новое задание в чипы */
    if (a1126_load_job(job) != 0) {
        log_message("Ошибка загрузки задания в чипы");
        return;
    }
    
    /* Запускаем майнинг */
    a1126_start();
}

/**
 * @brief Обработать результат от чипа
 */
static void process_result(const a1126_result_t* result) {
    if (!result->valid) {
        return;
    }
    
    g_shares_found++;
    
#if ENABLE_DEBUG_LOG
    printf("[SHARE] Чип %u нашёл nonce: 0x%08X\n", 
           result->chip_id, result->nonce);
#endif
    
    /* Формируем share */
    quaxis_share_t share;
    share.job_id = g_current_job.job_id;
    share.nonce = result->nonce;
    
    /* Отправляем на сервер */
    if (net_send_share(&share) == 0) {
        g_shares_sent++;
    } else {
        log_message("Ошибка отправки share");
    }
}

/**
 * @brief Главный цикл майнинга
 */
static void mining_loop(void) {
    a1126_result_t result;
    quaxis_job_t new_job;
    uint32_t last_heartbeat = 0;
    
    while (g_running) {
        /* Проверяем состояние сети */
        if (net_get_state() != NET_STATE_CONNECTED) {
            log_message("Соединение потеряно, переподключаемся...");
            a1126_stop();
            
            delay_ms(RECONNECT_DELAY_MS);
            
            if (net_connect(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT) != 0) {
                continue;
            }
        }
        
        /* Проверяем новые задания */
        int job_result = net_recv_job(&new_job, 10);  /* 10ms таймаут */
        if (job_result > 0) {
            process_job(&new_job);
        } else if (job_result < 0) {
            log_message("Ошибка получения задания");
        }
        
        /* Опрашиваем чипы на наличие результатов */
        while (a1126_poll_result(&result) > 0) {
            process_result(&result);
        }
        
        /* Heartbeat */
        uint32_t now = get_time_ms();
        if (now - last_heartbeat >= HEARTBEAT_INTERVAL_MS) {
            net_send_heartbeat();
            last_heartbeat = now;
        }
        
        /* Периодический вывод статистики */
        if (now - g_last_log_time >= LOG_INTERVAL_MS) {
            log_stats();
            g_last_log_time = now;
        }
    }
}

/**
 * @brief Точка входа
 */
int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║  Quaxis Solo Miner - Avalon 1126 Pro FW   ║\n");
    printf("║  Version %d.%d.%d                            ║\n",
           FIRMWARE_VERSION_MAJOR, 
           FIRMWARE_VERSION_MINOR, 
           FIRMWARE_VERSION_PATCH);
    printf("╚═══════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Инициализация оборудования */
    if (init_hardware() != 0) {
        printf("[FATAL] Ошибка инициализации оборудования\n");
        return 1;
    }
    
    /* Установка начального target (максимально простой) */
    memset(g_target, 0xFF, sizeof(g_target));
    g_target[31] = 0x00;  /* Минимальная сложность */
    a1126_set_target(g_target);
    
    /* Подключение к серверу */
    if (init_network(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT) != 0) {
        printf("[FATAL] Ошибка подключения к серверу\n");
        return 1;
    }
    
    printf("[INFO] Запуск майнинга...\n");
    
    /* Главный цикл */
    mining_loop();
    
    /* Остановка */
    a1126_stop();
    net_disconnect();
    
    printf("[INFO] Прошивка остановлена\n");
    
    return 0;
}
