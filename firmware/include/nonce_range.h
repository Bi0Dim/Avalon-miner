/**
 * @file nonce_range.h
 * @brief Управление диапазонами nonce на ASIC
 * 
 * Оптимизирует распределение nonce между 114 чипами для
 * минимизации дублирования работы и увеличения эффективности.
 */

#ifndef QUAXIS_NONCE_RANGE_H
#define QUAXIS_NONCE_RANGE_H

#include <stdint.h>

/*
 * Константы
 */
#define NONCE_RANGE_SIZE        8   /* Размер сериализованного диапазона */
#define NONCE_SPACE             0x100000000ULL  /* 2^32 */

/*
 * Стратегии распределения
 */
#define NONCE_STRATEGY_SEQUENTIAL   0
#define NONCE_STRATEGY_INTERLEAVED  1
#define NONCE_STRATEGY_RANDOM       2

/**
 * @brief Диапазон nonce для чипа
 */
typedef struct {
    uint32_t start;     /* Начало диапазона */
    uint32_t end;       /* Конец диапазона (включительно) */
    uint32_t step;      /* Шаг (1 для sequential/random, num_chips для interleaved) */
    uint32_t current;   /* Текущий nonce */
    uint8_t  strategy;  /* Стратегия (NONCE_STRATEGY_*) */
    uint8_t  chip_id;   /* ID чипа */
    uint8_t  exhausted; /* Диапазон исчерпан */
    uint8_t  reserved;
} nonce_range_t;

/**
 * @brief Контекст распределения nonce
 */
typedef struct {
    uint16_t total_chips;       /* Общее количество чипов */
    uint16_t chips_per_asic;    /* Чипов на ASIC */
    uint8_t  asic_id;           /* ID этого ASIC */
    uint8_t  strategy;          /* Стратегия */
    uint16_t active_chips;      /* Количество активных чипов */
    nonce_range_t* ranges;      /* Указатель на массив диапазонов */
} nonce_distributor_ctx_t;

/**
 * @brief Инициализировать диапазон для чипа (sequential стратегия)
 * 
 * @param range Указатель на диапазон
 * @param chip_id Глобальный ID чипа
 * @param total_chips Общее количество чипов
 */
void nonce_range_init_sequential(
    nonce_range_t* range,
    uint16_t chip_id,
    uint16_t total_chips
);

/**
 * @brief Инициализировать диапазон для чипа (interleaved стратегия)
 * 
 * @param range Указатель на диапазон
 * @param chip_id Глобальный ID чипа
 * @param total_chips Общее количество чипов
 */
void nonce_range_init_interleaved(
    nonce_range_t* range,
    uint16_t chip_id,
    uint16_t total_chips
);

/**
 * @brief Получить следующий nonce
 * 
 * @param range Указатель на диапазон
 * @return Следующий nonce, или current если диапазон исчерпан
 */
uint32_t nonce_range_next(nonce_range_t* range);

/**
 * @brief Проверить, исчерпан ли диапазон
 * 
 * @param range Указатель на диапазон
 * @return 1 если диапазон исчерпан
 */
static inline int nonce_range_exhausted(const nonce_range_t* range) {
    return range ? range->exhausted : 1;
}

/**
 * @brief Сбросить диапазон к начальному состоянию
 * 
 * @param range Указатель на диапазон
 */
static inline void nonce_range_reset(nonce_range_t* range) {
    if (range) {
        range->current = range->start;
        range->exhausted = 0;
    }
}

/**
 * @brief Получить оставшееся количество nonce в диапазоне
 * 
 * @param range Указатель на диапазон
 * @return Количество оставшихся nonce
 */
static inline uint64_t nonce_range_remaining(const nonce_range_t* range) {
    if (!range || range->exhausted) return 0;
    
    if (range->step == 1) {
        return (uint64_t)(range->end - range->current + 1);
    } else {
        /* Для interleaved */
        return ((uint64_t)(range->end - range->current)) / range->step + 1;
    }
}

/**
 * @brief Проверить, принадлежит ли nonce диапазону
 * 
 * @param range Указатель на диапазон
 * @param nonce Nonce для проверки
 * @return 1 если принадлежит
 */
static inline int nonce_range_contains(
    const nonce_range_t* range,
    uint32_t nonce
) {
    if (!range) return 0;
    
    if (range->strategy == NONCE_STRATEGY_INTERLEAVED) {
        return nonce >= range->start && 
               nonce <= range->end && 
               ((nonce - range->start) % range->step == 0);
    }
    
    return nonce >= range->start && nonce <= range->end;
}

/**
 * @brief Парсить диапазон из буфера (8 байт)
 * 
 * Формат:
 * [0-3]: start (little-endian)
 * [4-7]: end (little-endian)
 * 
 * @param buf Буфер с данными
 * @param range Указатель на диапазон для заполнения
 * @return 0 при успехе, -1 при ошибке
 */
static inline int nonce_range_parse(
    const uint8_t* buf,
    nonce_range_t* range
) {
    if (!buf || !range) return -1;
    
    range->start = (uint32_t)buf[0] |
                   ((uint32_t)buf[1] << 8) |
                   ((uint32_t)buf[2] << 16) |
                   ((uint32_t)buf[3] << 24);
    
    range->end = (uint32_t)buf[4] |
                 ((uint32_t)buf[5] << 8) |
                 ((uint32_t)buf[6] << 16) |
                 ((uint32_t)buf[7] << 24);
    
    range->current = range->start;
    range->step = 1;
    range->strategy = NONCE_STRATEGY_SEQUENTIAL;
    range->exhausted = 0;
    
    return 0;
}

/**
 * @brief Сериализовать диапазон в буфер (8 байт)
 * 
 * @param range Указатель на диапазон
 * @param buf Буфер для записи
 * @return 8 при успехе, -1 при ошибке
 */
static inline int nonce_range_serialize(
    const nonce_range_t* range,
    uint8_t* buf
) {
    if (!range || !buf) return -1;
    
    buf[0] = (uint8_t)(range->start & 0xFF);
    buf[1] = (uint8_t)((range->start >> 8) & 0xFF);
    buf[2] = (uint8_t)((range->start >> 16) & 0xFF);
    buf[3] = (uint8_t)((range->start >> 24) & 0xFF);
    
    buf[4] = (uint8_t)(range->end & 0xFF);
    buf[5] = (uint8_t)((range->end >> 8) & 0xFF);
    buf[6] = (uint8_t)((range->end >> 16) & 0xFF);
    buf[7] = (uint8_t)((range->end >> 24) & 0xFF);
    
    return 8;
}

/**
 * @brief Инициализировать контекст распределения
 * 
 * @param ctx Контекст
 * @param ranges Массив диапазонов (должен быть выделен заранее)
 * @param chips_per_asic Количество чипов на ASIC
 * @param total_chips Общее количество чипов во всей системе
 * @param asic_id ID этого ASIC
 * @param strategy Стратегия (NONCE_STRATEGY_*)
 */
void nonce_distributor_init(
    nonce_distributor_ctx_t* ctx,
    nonce_range_t* ranges,
    uint16_t chips_per_asic,
    uint16_t total_chips,
    uint8_t asic_id,
    uint8_t strategy
);

/**
 * @brief Сбросить все диапазоны для нового задания
 * 
 * @param ctx Контекст
 */
void nonce_distributor_reset_all(nonce_distributor_ctx_t* ctx);

/**
 * @brief Получить следующий nonce для чипа
 * 
 * @param ctx Контекст
 * @param local_chip_id Локальный ID чипа (0 - chips_per_asic-1)
 * @return Следующий nonce
 */
uint32_t nonce_distributor_next(
    nonce_distributor_ctx_t* ctx,
    uint8_t local_chip_id
);

/**
 * @brief Проверить, все ли диапазоны исчерпаны
 * 
 * @param ctx Контекст
 * @return 1 если все исчерпаны
 */
int nonce_distributor_exhausted(const nonce_distributor_ctx_t* ctx);

#endif /* QUAXIS_NONCE_RANGE_H */
