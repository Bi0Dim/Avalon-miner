/**
 * @file config.h
 * @brief Конфигурация прошивки Avalon 1126 Pro
 */

#ifndef QUAXIS_CONFIG_H
#define QUAXIS_CONFIG_H

/*
 * Версия прошивки
 */
#define FIRMWARE_VERSION_MAJOR  1
#define FIRMWARE_VERSION_MINOR  0
#define FIRMWARE_VERSION_PATCH  0

/*
 * Конфигурация сети
 */
#define DEFAULT_SERVER_IP       "192.168.1.100"
#define DEFAULT_SERVER_PORT     3333
#define RECONNECT_DELAY_MS      1000
#define HEARTBEAT_INTERVAL_MS   30000
#define RECV_TIMEOUT_MS         5000

/*
 * Конфигурация ASIC чипов
 */
#define A1126_CHIP_COUNT        114     /* Количество чипов на плате */
#define A1126_CORE_PER_CHIP     12      /* Ядер на чип */
#define A1126_TOTAL_CORES       (A1126_CHIP_COUNT * A1126_CORE_PER_CHIP)

/*
 * Конфигурация SPI
 */
#define SPI_CLOCK_HZ            10000000    /* 10 MHz */
#define SPI_MODE                0           /* CPOL=0, CPHA=0 */

/*
 * Конфигурация очереди заданий
 */
#define JOB_QUEUE_SIZE          100     /* Максимум заданий в очереди */
#define JOB_STALE_TIMEOUT_MS    60000   /* Таймаут устаревания задания */

/*
 * Размеры буферов
 */
#define RECV_BUFFER_SIZE        1024
#define SEND_BUFFER_SIZE        256

/*
 * Протокол
 */
#define JOB_MESSAGE_SIZE        48      /* midstate[32] + tail[12] + job_id[4] */
#define SHARE_MESSAGE_SIZE      8       /* job_id[4] + nonce[4] */

/*
 * Target для partial shares (опционально)
 */
#define PARTIAL_DIFFICULTY      1.0     /* Минимальная сложность для логирования */

/*
 * Аппаратные пины (зависят от конкретной платы)
 */
#define PIN_SPI_CLK             0
#define PIN_SPI_MOSI            1
#define PIN_SPI_MISO            2
#define PIN_SPI_CS              3
#define PIN_CHIP_RESET          4
#define PIN_STATUS_LED          5

/*
 * Диагностика
 */
#define ENABLE_DEBUG_LOG        0       /* 1 = включить отладочные сообщения */
#define ENABLE_HASHRATE_LOG     1       /* 1 = логировать хешрейт */
#define LOG_INTERVAL_MS         60000   /* Интервал вывода статистики */

#endif /* QUAXIS_CONFIG_H */
