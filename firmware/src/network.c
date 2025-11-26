/**
 * @file network.c
 * @brief Реализация сетевого клиента
 * 
 * Заглушка - требует реализации для конкретной платформы.
 */

#include "network.h"
#include "config.h"

#include <string.h>

/* Состояние соединения */
static net_state_t g_state = NET_STATE_DISCONNECTED;

/* Заглушки для сетевых функций */
/* TODO: Реализовать для конкретной платформы (lwIP, etc.) */

int net_init(void) {
    /* Инициализация сетевого стека */
    g_state = NET_STATE_DISCONNECTED;
    return 0;
}

int net_connect(const char* server_ip, uint16_t port) {
    (void)server_ip;
    (void)port;
    
    /* TODO: Реализовать TCP соединение */
    g_state = NET_STATE_CONNECTED;
    return 0;
}

void net_disconnect(void) {
    /* TODO: Закрыть соединение */
    g_state = NET_STATE_DISCONNECTED;
}

net_state_t net_get_state(void) {
    return g_state;
}

int net_send(const uint8_t* data, size_t len) {
    if (g_state != NET_STATE_CONNECTED) {
        return -1;
    }
    
    /* TODO: Отправить данные через TCP */
    (void)data;
    return (int)len;
}

int net_recv(uint8_t* buf, size_t max_len, uint32_t timeout_ms) {
    if (g_state != NET_STATE_CONNECTED) {
        return -1;
    }
    
    /* TODO: Получить данные из TCP буфера */
    (void)buf;
    (void)max_len;
    (void)timeout_ms;
    
    return 0;  /* Нет данных */
}

int net_send_share(const quaxis_share_t* share) {
    uint8_t buf[9];
    int len = quaxis_serialize_share(share, buf);
    if (len < 0) return -1;
    
    return net_send(buf, (size_t)len);
}

int net_send_heartbeat(void) {
    uint8_t cmd = RSP_HEARTBEAT;
    return net_send(&cmd, 1);
}

int net_send_status(const quaxis_status_t* status) {
    uint8_t buf[9];
    
    buf[0] = RSP_STATUS;
    buf[1] = (uint8_t)(status->hashrate & 0xFF);
    buf[2] = (uint8_t)((status->hashrate >> 8) & 0xFF);
    buf[3] = (uint8_t)((status->hashrate >> 16) & 0xFF);
    buf[4] = (uint8_t)((status->hashrate >> 24) & 0xFF);
    buf[5] = status->temperature;
    buf[6] = status->fan_speed;
    buf[7] = (uint8_t)(status->errors & 0xFF);
    buf[8] = (uint8_t)((status->errors >> 8) & 0xFF);
    
    return net_send(buf, 9);
}

int net_recv_job(quaxis_job_t* job, uint32_t timeout_ms) {
    uint8_t buf[RECV_BUFFER_SIZE];
    
    int received = net_recv(buf, sizeof(buf), timeout_ms);
    if (received < 0) {
        return -1;  /* Ошибка */
    }
    
    if (received == 0) {
        return 0;  /* Нет данных */
    }
    
    /* Проверяем тип сообщения */
    if (buf[0] != CMD_NEW_JOB) {
        /* Обрабатываем другие команды */
        if (buf[0] == CMD_STOP) {
            /* Команда остановки */
            return 0;
        }
        if (buf[0] == CMD_HEARTBEAT) {
            /* Отвечаем на heartbeat */
            net_send_heartbeat();
            return 0;
        }
        return 0;
    }
    
    /* Проверяем длину */
    if (received < 1 + JOB_MESSAGE_SIZE) {
        return 0;  /* Неполное сообщение */
    }
    
    /* Парсим задание */
    if (quaxis_parse_job(buf + 1, job) != 0) {
        return -1;
    }
    
    return 1;  /* Получено задание */
}
