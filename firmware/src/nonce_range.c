/**
 * @file nonce_range.c
 * @brief Реализация управления диапазонами nonce на ASIC
 */

#include "nonce_range.h"
#include "config.h"

/**
 * @brief Инициализировать диапазон для чипа (sequential стратегия)
 */
void nonce_range_init_sequential(
    nonce_range_t* range,
    uint16_t chip_id,
    uint16_t total_chips
) {
    if (!range || total_chips == 0) return;
    
    range->chip_id = (uint8_t)chip_id;
    range->strategy = NONCE_STRATEGY_SEQUENTIAL;
    range->step = 1;
    range->exhausted = 0;
    
    /* Вычисляем размер диапазона */
    uint64_t range_size = NONCE_SPACE / total_chips;
    uint32_t remainder = (uint32_t)(NONCE_SPACE % total_chips);
    
    /* Вычисляем начало и конец */
    uint64_t start = (uint64_t)chip_id * range_size;
    
    /* Распределяем остаток равномерно */
    if (chip_id < remainder) {
        start += chip_id;
        range_size++;
    } else {
        start += remainder;
    }
    
    range->start = (uint32_t)start;
    range->end = (uint32_t)(start + range_size - 1);
    range->current = range->start;
}

/**
 * @brief Инициализировать диапазон для чипа (interleaved стратегия)
 */
void nonce_range_init_interleaved(
    nonce_range_t* range,
    uint16_t chip_id,
    uint16_t total_chips
) {
    if (!range || total_chips == 0) return;
    
    range->chip_id = (uint8_t)chip_id;
    range->strategy = NONCE_STRATEGY_INTERLEAVED;
    range->step = total_chips;
    range->exhausted = 0;
    
    /* Для interleaved: chip[i] получает nonce = i, i+total_chips, i+2*total_chips, ... */
    range->start = chip_id;
    range->end = 0xFFFFFFFF;
    range->current = range->start;
}

/**
 * @brief Получить следующий nonce
 */
uint32_t nonce_range_next(nonce_range_t* range) {
    if (!range || range->exhausted) {
        return range ? range->current : 0;
    }
    
    uint32_t result = range->current;
    
    /* Вычисляем следующий nonce */
    uint64_t next = (uint64_t)range->current + range->step;
    
    if (next > range->end) {
        range->exhausted = 1;
    } else {
        range->current = (uint32_t)next;
    }
    
    return result;
}

/**
 * @brief Инициализировать контекст распределения
 */
void nonce_distributor_init(
    nonce_distributor_ctx_t* ctx,
    nonce_range_t* ranges,
    uint16_t chips_per_asic,
    uint16_t total_chips,
    uint8_t asic_id,
    uint8_t strategy
) {
    if (!ctx || !ranges) return;
    
    ctx->ranges = ranges;
    ctx->chips_per_asic = chips_per_asic;
    ctx->total_chips = total_chips;
    ctx->asic_id = asic_id;
    ctx->strategy = strategy;
    ctx->active_chips = chips_per_asic;
    
    /* Инициализируем диапазоны для всех чипов этого ASIC */
    uint16_t global_chip_start = (uint16_t)asic_id * chips_per_asic;
    
    for (uint16_t i = 0; i < chips_per_asic; i++) {
        uint16_t global_id = global_chip_start + i;
        
        if (strategy == NONCE_STRATEGY_INTERLEAVED) {
            nonce_range_init_interleaved(&ranges[i], global_id, total_chips);
        } else {
            /* Sequential или Random (random обрабатывается сервером) */
            nonce_range_init_sequential(&ranges[i], global_id, total_chips);
        }
    }
}

/**
 * @brief Сбросить все диапазоны для нового задания
 */
void nonce_distributor_reset_all(nonce_distributor_ctx_t* ctx) {
    if (!ctx || !ctx->ranges) return;
    
    for (uint16_t i = 0; i < ctx->chips_per_asic; i++) {
        nonce_range_reset(&ctx->ranges[i]);
    }
}

/**
 * @brief Получить следующий nonce для чипа
 */
uint32_t nonce_distributor_next(
    nonce_distributor_ctx_t* ctx,
    uint8_t local_chip_id
) {
    if (!ctx || !ctx->ranges || local_chip_id >= ctx->chips_per_asic) {
        return 0;
    }
    
    return nonce_range_next(&ctx->ranges[local_chip_id]);
}

/**
 * @brief Проверить, все ли диапазоны исчерпаны
 */
int nonce_distributor_exhausted(const nonce_distributor_ctx_t* ctx) {
    if (!ctx || !ctx->ranges) return 1;
    
    for (uint16_t i = 0; i < ctx->chips_per_asic; i++) {
        if (!ctx->ranges[i].exhausted) {
            return 0;
        }
    }
    
    return 1;
}
