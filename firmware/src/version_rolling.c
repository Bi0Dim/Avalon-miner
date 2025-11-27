/**
 * @file version_rolling.c
 * @brief Реализация Version Rolling для прошивки ASIC
 * 
 * Version Rolling (AsicBoost) использует биты 13-28 поля version
 * как дополнительное пространство nonce для увеличения хешрейта.
 */

#include "version_rolling.h"
#include "config.h"

/**
 * @brief Инициализировать контекст version rolling
 * 
 * Распределяет диапазоны rolling между чипами для параллельной работы.
 * Каждый чип получает свой уникальный диапазон rolling значений.
 */
void version_rolling_init(
    version_rolling_ctx_t* ctx,
    uint16_t chip_id,
    uint16_t total_chips,
    uint32_t version_base,
    uint32_t version_mask
) {
    if (!ctx || total_chips == 0) {
        return;
    }
    
    ctx->version_base = version_base;
    ctx->version_mask = version_mask;
    ctx->chip_id = chip_id;
    
    /* Вычисляем размер диапазона для каждого чипа */
    uint32_t total_range = VERSION_ROLLING_MAX + 1;  /* 65536 значений */
    uint32_t range_per_chip = total_range / total_chips;
    
    /* Вычисляем диапазон для этого чипа */
    ctx->rolling_start = (uint16_t)(chip_id * range_per_chip);
    
    /* Последний чип получает остаток */
    if (chip_id == total_chips - 1) {
        ctx->rolling_end = VERSION_ROLLING_MAX;
    } else {
        ctx->rolling_end = (uint16_t)((chip_id + 1) * range_per_chip - 1);
    }
    
    /* Начинаем с начала диапазона */
    ctx->current_rolling = ctx->rolling_start;
}

/**
 * @brief Получить следующую версию для перебора
 * 
 * Возвращает версию с текущим значением rolling и инкрементирует счётчик.
 */
uint32_t version_rolling_next(version_rolling_ctx_t* ctx) {
    if (!ctx) {
        return VERSION_BASE_DEFAULT;
    }
    
    /* Получаем текущее значение rolling */
    uint16_t rolling = ctx->current_rolling;
    
    /* Инкрементируем счётчик */
    if (ctx->current_rolling < ctx->rolling_end) {
        ctx->current_rolling++;
    } else {
        /* Wrap-around к началу диапазона */
        ctx->current_rolling = ctx->rolling_start;
    }
    
    /* Применяем rolling к версии */
    return version_rolling_apply(ctx, rolling);
}
