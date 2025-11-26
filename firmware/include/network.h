/**
 * @file network.h
 * @brief Сетевой клиент для связи с сервером
 */

#ifndef QUAXIS_NETWORK_H
#define QUAXIS_NETWORK_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

/**
 * @brief Состояние соединения
 */
typedef enum {
    NET_STATE_DISCONNECTED = 0,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_ERROR
} net_state_t;

/**
 * @brief Инициализировать сетевой стек
 * 
 * @return 0 при успехе, -1 при ошибке
 */
int net_init(void);

/**
 * @brief Подключиться к серверу
 * 
 * @param server_ip IP адрес сервера
 * @param port Порт сервера
 * @return 0 при успехе, -1 при ошибке
 */
int net_connect(const char* server_ip, uint16_t port);

/**
 * @brief Закрыть соединение
 */
void net_disconnect(void);

/**
 * @brief Получить состояние соединения
 * 
 * @return Текущее состояние
 */
net_state_t net_get_state(void);

/**
 * @brief Отправить данные
 * 
 * @param data Данные для отправки
 * @param len Длина данных
 * @return Количество отправленных байт, -1 при ошибке
 */
int net_send(const uint8_t* data, size_t len);

/**
 * @brief Получить данные
 * 
 * @param buf Буфер для данных
 * @param max_len Максимальный размер буфера
 * @param timeout_ms Таймаут в миллисекундах (0 = без блокировки)
 * @return Количество полученных байт, 0 если нет данных, -1 при ошибке
 */
int net_recv(uint8_t* buf, size_t max_len, uint32_t timeout_ms);

/**
 * @brief Отправить share на сервер
 * 
 * @param share Указатель на share
 * @return 0 при успехе, -1 при ошибке
 */
int net_send_share(const quaxis_share_t* share);

/**
 * @brief Отправить heartbeat на сервер
 * 
 * @return 0 при успехе, -1 при ошибке
 */
int net_send_heartbeat(void);

/**
 * @brief Отправить статус на сервер
 * 
 * @param status Указатель на статус
 * @return 0 при успехе, -1 при ошибке
 */
int net_send_status(const quaxis_status_t* status);

/**
 * @brief Получить задание от сервера
 * 
 * @param job Указатель на структуру для задания
 * @param timeout_ms Таймаут в миллисекундах
 * @return 1 если получено задание, 0 если таймаут, -1 при ошибке
 */
int net_recv_job(quaxis_job_t* job, uint32_t timeout_ms);

#endif /* QUAXIS_NETWORK_H */
