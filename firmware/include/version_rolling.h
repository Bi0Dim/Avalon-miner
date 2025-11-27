/**
 * @file version_rolling.h
 * @brief Поддержка Version Rolling (AsicBoost) в прошивке
 * 
 * Version Rolling использует биты 13-28 поля version как дополнительный nonce.
 * Это даёт прирост производительности +15-20%.
 */

#ifndef QUAXIS_VERSION_ROLLING_H
#define QUAXIS_VERSION_ROLLING_H

#include <stdint.h>

/*
 * Константы Version Rolling
 */
#define VERSION_ROLLING_MASK_DEFAULT    0x1FFFE000  /* Биты 13-28 */
#define VERSION_BASE_DEFAULT            0x20000000  /* BIP9 base version */
#define VERSION_ROLLING_BITS            16          /* Количество rolling битов */
#define VERSION_ROLLING_MAX             0xFFFF      /* Максимальное значение (16 бит) */
#define VERSION_ROLLING_SHIFT           13          /* Сдвиг rolling битов */

/*
 * Размеры сообщений V2 (с version rolling)
 */
#define JOB_MESSAGE_V2_SIZE     56  /* midstate[32] + tail[12] + job_id[4] + version_base[4] + mask[2] + reserved[2] */
#define SHARE_MESSAGE_V2_SIZE   12  /* job_id[4] + nonce[4] + version[4] */

/*
 * Структура задания V2 (56 байт)
 * 
 * Расширенный формат с поддержкой version rolling.
 */
typedef struct __attribute__((packed)) {
    uint8_t  midstate[32];      /* SHA256 state после первых 64 байт header */
    uint8_t  header_tail[12];   /* Последние 12 байт: merkle[28:32] + time + bits */
    uint32_t job_id;            /* ID задания (little-endian) */
    uint32_t version_base;      /* Базовая версия блока (little-endian) */
    uint16_t version_mask;      /* Маска rolling битов (16 бит, сдвинутая) */
    uint16_t reserved;          /* Зарезервировано */
} quaxis_job_v2_t;

/*
 * Структура share V2 (12 байт)
 * 
 * Расширенный формат с найденной версией.
 */
typedef struct __attribute__((packed)) {
    uint32_t job_id;    /* ID задания */
    uint32_t nonce;     /* Найденный nonce */
    uint32_t version;   /* Найденная версия с rolling битами */
} quaxis_share_v2_t;

/*
 * Контекст version rolling для чипа
 */
typedef struct {
    uint32_t version_base;      /* Базовая версия */
    uint32_t version_mask;      /* Полная маска rolling */
    uint16_t current_rolling;   /* Текущее значение rolling */
    uint16_t chip_id;           /* ID чипа (для распределения диапазонов) */
    uint16_t rolling_start;     /* Начало диапазона rolling для этого чипа */
    uint16_t rolling_end;       /* Конец диапазона rolling для этого чипа */
} version_rolling_ctx_t;

/**
 * @brief Инициализировать контекст version rolling
 * 
 * @param ctx Указатель на контекст
 * @param chip_id ID чипа (0 - NUM_CHIPS-1)
 * @param total_chips Общее количество чипов
 * @param version_base Базовая версия блока
 * @param version_mask Маска rolling битов
 */
void version_rolling_init(
    version_rolling_ctx_t* ctx,
    uint16_t chip_id,
    uint16_t total_chips,
    uint32_t version_base,
    uint32_t version_mask
);

/**
 * @brief Получить следующую версию для перебора
 * 
 * @param ctx Контекст version rolling
 * @return Версия с применёнными rolling битами
 */
uint32_t version_rolling_next(version_rolling_ctx_t* ctx);

/**
 * @brief Применить rolling значение к базовой версии
 * 
 * @param ctx Контекст version rolling
 * @param rolling_value Значение rolling (0 - VERSION_ROLLING_MAX)
 * @return Версия с применёнными rolling битами
 */
static inline uint32_t version_rolling_apply(
    const version_rolling_ctx_t* ctx,
    uint16_t rolling_value
) {
    /* Сдвигаем rolling на позицию 13 и применяем маску */
    uint32_t rolling_bits = ((uint32_t)rolling_value << VERSION_ROLLING_SHIFT) & ctx->version_mask;
    
    /* Очищаем rolling биты в базовой версии и применяем новые */
    return (ctx->version_base & ~ctx->version_mask) | rolling_bits;
}

/**
 * @brief Извлечь rolling значение из версии
 * 
 * @param ctx Контекст version rolling
 * @param version Полная версия блока
 * @return Значение rolling bits
 */
static inline uint16_t version_rolling_extract(
    const version_rolling_ctx_t* ctx,
    uint32_t version
) {
    return (uint16_t)((version & ctx->version_mask) >> VERSION_ROLLING_SHIFT);
}

/**
 * @brief Проверить, завершён ли перебор диапазона
 * 
 * @param ctx Контекст version rolling
 * @return 1 если диапазон исчерпан, 0 если нет
 */
static inline int version_rolling_exhausted(const version_rolling_ctx_t* ctx) {
    return ctx->current_rolling >= ctx->rolling_end;
}

/**
 * @brief Сбросить счётчик rolling
 * 
 * @param ctx Контекст version rolling
 */
static inline void version_rolling_reset(version_rolling_ctx_t* ctx) {
    ctx->current_rolling = ctx->rolling_start;
}

/**
 * @brief Парсить задание V2 из буфера
 * 
 * @param buf Буфер с данными (минимум 56 байт)
 * @param job Указатель на структуру для заполнения
 * @return 0 при успехе, -1 при ошибке
 */
static inline int quaxis_parse_job_v2(const uint8_t* buf, quaxis_job_v2_t* job) {
    if (!buf || !job) return -1;
    
    /* Копируем midstate */
    for (int i = 0; i < 32; i++) {
        job->midstate[i] = buf[i];
    }
    
    /* Копируем header_tail */
    for (int i = 0; i < 12; i++) {
        job->header_tail[i] = buf[32 + i];
    }
    
    /* Читаем job_id (little-endian) */
    job->job_id = (uint32_t)buf[44] | 
                  ((uint32_t)buf[45] << 8) |
                  ((uint32_t)buf[46] << 16) |
                  ((uint32_t)buf[47] << 24);
    
    /* Читаем version_base (little-endian) */
    job->version_base = (uint32_t)buf[48] | 
                        ((uint32_t)buf[49] << 8) |
                        ((uint32_t)buf[50] << 16) |
                        ((uint32_t)buf[51] << 24);
    
    /* Читаем version_mask (little-endian) */
    job->version_mask = (uint16_t)buf[52] | ((uint16_t)buf[53] << 8);
    
    /* Читаем reserved */
    job->reserved = (uint16_t)buf[54] | ((uint16_t)buf[55] << 8);
    
    return 0;
}

/**
 * @brief Сериализовать share V2 в буфер
 * 
 * @param share Указатель на share
 * @param buf Буфер для записи (минимум 13 байт: 1 + 12)
 * @return Количество записанных байт
 */
static inline int quaxis_serialize_share_v2(const quaxis_share_v2_t* share, uint8_t* buf) {
    if (!share || !buf) return -1;
    
    /* Тип сообщения (используем новый код для V2) */
    buf[0] = 0x82;  /* RSP_SHARE_V2 */
    
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
    
    /* version (little-endian) */
    buf[9] = (uint8_t)(share->version & 0xFF);
    buf[10] = (uint8_t)((share->version >> 8) & 0xFF);
    buf[11] = (uint8_t)((share->version >> 16) & 0xFF);
    buf[12] = (uint8_t)((share->version >> 24) & 0xFF);
    
    return 13;
}

#endif /* QUAXIS_VERSION_ROLLING_H */
