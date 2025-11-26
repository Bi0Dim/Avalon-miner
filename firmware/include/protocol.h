/**
 * @file protocol.h
 * @brief Определения бинарного протокола Quaxis
 */

#ifndef QUAXIS_PROTOCOL_H
#define QUAXIS_PROTOCOL_H

#include <stdint.h>

/*
 * Коды команд от сервера
 */
#define CMD_NEW_JOB         0x01    /* Новое задание */
#define CMD_STOP            0x02    /* Остановить майнинг */
#define CMD_HEARTBEAT       0x03    /* Ping */
#define CMD_SET_TARGET      0x04    /* Установить target */
#define CMD_SET_DIFFICULTY  0x05    /* Установить difficulty */

/*
 * Коды ответов к серверу
 */
#define RSP_SHARE           0x81    /* Найден nonce */
#define RSP_HEARTBEAT       0x83    /* Pong */
#define RSP_STATUS          0x84    /* Статус ASIC */
#define RSP_ERROR           0x8F    /* Ошибка */

/*
 * Структура задания (48 байт)
 * 
 * Содержит все данные для вычисления хеша блока.
 * ASIC использует midstate и перебирает nonce.
 */
typedef struct __attribute__((packed)) {
    uint8_t  midstate[32];  /* SHA256 state после первых 64 байт header */
    uint32_t timestamp;     /* Timestamp блока (little-endian) */
    uint32_t bits;          /* Compact target (little-endian) */
    uint32_t nonce_start;   /* Начальный nonce (little-endian) */
    uint32_t job_id;        /* ID задания (little-endian) */
} quaxis_job_t;

/*
 * Структура share (8 байт)
 * 
 * Отправляется на сервер при нахождении валидного nonce.
 */
typedef struct __attribute__((packed)) {
    uint32_t job_id;        /* ID задания */
    uint32_t nonce;         /* Найденный nonce */
} quaxis_share_t;

/*
 * Структура статуса ASIC
 */
typedef struct __attribute__((packed)) {
    uint32_t hashrate;      /* Текущий хешрейт (H/s) */
    uint8_t  temperature;   /* Температура чипа (°C) */
    uint8_t  fan_speed;     /* Скорость вентилятора (%) */
    uint16_t errors;        /* Количество ошибок */
} quaxis_status_t;

/*
 * Структура target (32 байта)
 */
typedef struct __attribute__((packed)) {
    uint8_t target[32];     /* 256-битный target */
} quaxis_target_t;

/*
 * Функции сериализации
 */

/**
 * @brief Десериализовать задание из буфера
 * 
 * @param buf Буфер с данными (минимум 48 байт)
 * @param job Указатель на структуру для заполнения
 * @return 0 при успехе, -1 при ошибке
 */
static inline int quaxis_parse_job(const uint8_t* buf, quaxis_job_t* job) {
    if (!buf || !job) return -1;
    
    /* Копируем midstate */
    for (int i = 0; i < 32; i++) {
        job->midstate[i] = buf[i];
    }
    
    /* Читаем поля в little-endian */
    job->timestamp = (uint32_t)buf[32] | 
                     ((uint32_t)buf[33] << 8) |
                     ((uint32_t)buf[34] << 16) |
                     ((uint32_t)buf[35] << 24);
    
    job->bits = (uint32_t)buf[36] | 
                ((uint32_t)buf[37] << 8) |
                ((uint32_t)buf[38] << 16) |
                ((uint32_t)buf[39] << 24);
    
    job->nonce_start = (uint32_t)buf[40] | 
                       ((uint32_t)buf[41] << 8) |
                       ((uint32_t)buf[42] << 16) |
                       ((uint32_t)buf[43] << 24);
    
    job->job_id = (uint32_t)buf[44] | 
                  ((uint32_t)buf[45] << 8) |
                  ((uint32_t)buf[46] << 16) |
                  ((uint32_t)buf[47] << 24);
    
    return 0;
}

/**
 * @brief Сериализовать share в буфер
 * 
 * @param share Указатель на share
 * @param buf Буфер для записи (минимум 9 байт: 1 + 8)
 * @return Количество записанных байт
 */
static inline int quaxis_serialize_share(const quaxis_share_t* share, uint8_t* buf) {
    if (!share || !buf) return -1;
    
    /* Тип сообщения */
    buf[0] = RSP_SHARE;
    
    /* job_id (little-endian) */
    buf[1] = (uint8_t)(share->job_id & 0xFF);
    buf[2] = (uint8_t)((share->job_id >> 8) & 0xFF);
    buf[3] = (uint8_t)((share->job_id >> 16) & 0xFF);
    buf[4] = (uint8_t)((share->job_id >> 24) & 0xFF);
    
    /* nonce (little-endian) */
    buf[5] = (uint8_t)(share->nonce & 0xFF);
    buf[6] = (uint8_t)((share->nonce >> 8) & 0xFF);
    buf[7] = (uint8_t)((share->nonce >> 16) & 0xFF);
    buf[8] = (uint8_t)((share->nonce >> 24) & 0xFF);
    
    return 9;
}

#endif /* QUAXIS_PROTOCOL_H */
