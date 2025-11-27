/**
 * @file health_reporter.c
 * @brief Реализация отправки метрик здоровья с прошивки ASIC
 */

#include "health_reporter.h"
#include "config.h"

#include <string.h>

/**
 * @brief Инициализировать health reporter
 */
void health_reporter_init(
    health_reporter_ctx_t* ctx,
    uint16_t total_chips,
    uint32_t current_time_ms
) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(health_reporter_ctx_t));
    
    ctx->total_chips = total_chips;
    ctx->active_chips = total_chips;  /* Изначально все активны */
    ctx->start_time_ms = current_time_ms;
    ctx->last_report_time_ms = current_time_ms;
    
    /* Пороги по умолчанию */
    ctx->temp_warning = 750;    /* 75.0°C */
    ctx->temp_critical = 850;   /* 85.0°C */
    ctx->temp_emergency = 950;  /* 95.0°C */
    
    /* Инициализируем min температуру большим значением */
    ctx->temp.min = 32767;
}

/**
 * @brief Установить пороги температуры
 */
void health_reporter_set_thresholds(
    health_reporter_ctx_t* ctx,
    float warning,
    float critical,
    float emergency
) {
    if (!ctx) return;
    
    ctx->temp_warning = (int16_t)(warning * 10.0f);
    ctx->temp_critical = (int16_t)(critical * 10.0f);
    ctx->temp_emergency = (int16_t)(emergency * 10.0f);
}

/**
 * @brief Обновить температуру
 */
void health_reporter_update_temp(
    health_reporter_ctx_t* ctx,
    float temperature
) {
    if (!ctx) return;
    
    int16_t temp_int = (int16_t)(temperature * 10.0f);
    
    /* Добавляем сэмпл */
    ctx->temp_samples[ctx->temp_sample_idx] = temp_int;
    ctx->temp_sample_idx = (ctx->temp_sample_idx + 1) % TEMP_SAMPLES_COUNT;
    
    /* Обновляем текущую температуру */
    ctx->temp.current = temp_int;
    
    /* Обновляем max/min */
    if (temp_int > ctx->temp.max) {
        ctx->temp.max = temp_int;
    }
    if (temp_int < ctx->temp.min) {
        ctx->temp.min = temp_int;
    }
    
    /* Вычисляем среднюю */
    int32_t sum = 0;
    for (int i = 0; i < TEMP_SAMPLES_COUNT; i++) {
        sum += ctx->temp_samples[i];
    }
    ctx->temp.average = (int16_t)(sum / TEMP_SAMPLES_COUNT);
    
    /* Обновляем статус */
    if (temp_int >= ctx->temp_emergency) {
        ctx->overall_status = 3;  /* Emergency */
    } else if (temp_int >= ctx->temp_critical) {
        ctx->overall_status = 2;  /* Critical */
    } else if (temp_int >= ctx->temp_warning) {
        if (ctx->overall_status < 1) {
            ctx->overall_status = 1;  /* Warning */
        }
    }
}

/**
 * @brief Обновить хешрейт
 */
void health_reporter_update_hashrate(
    health_reporter_ctx_t* ctx,
    uint32_t hashrate,
    uint32_t nominal_hashrate
) {
    if (!ctx) return;
    
    ctx->hashrate.current_hps = hashrate;
    
    /* Простое экспоненциальное скользящее среднее */
    if (ctx->hashrate.average_hps == 0) {
        ctx->hashrate.average_hps = hashrate;
    } else {
        ctx->hashrate.average_hps = (ctx->hashrate.average_hps * 7 + hashrate) / 8;
    }
    
    /* Эффективность */
    if (nominal_hashrate > 0) {
        uint32_t efficiency = (hashrate * 100) / nominal_hashrate;
        ctx->hashrate.efficiency = (uint8_t)(efficiency > 100 ? 100 : efficiency);
        
        /* Проверяем падение хешрейта */
        if (efficiency < 75) {
            if (ctx->overall_status < 2) {
                ctx->overall_status = 2;  /* Critical */
            }
        } else if (efficiency < 90) {
            if (ctx->overall_status < 1) {
                ctx->overall_status = 1;  /* Warning */
            }
        }
    } else {
        ctx->hashrate.efficiency = 100;
    }
}

/**
 * @brief Записать ошибку
 */
void health_reporter_record_error(
    health_reporter_ctx_t* ctx,
    int hw_error,
    int rejected,
    int stale
) {
    if (!ctx) return;
    
    if (hw_error) ctx->errors.hw_errors++;
    if (rejected) ctx->errors.rejected_shares++;
    if (stale) ctx->errors.stale_shares++;
}

/**
 * @brief Записать успешный share
 */
void health_reporter_record_share(health_reporter_ctx_t* ctx) {
    if (!ctx) return;
    ctx->errors.total_shares++;
}

/**
 * @brief Обновить питание
 */
void health_reporter_update_power(
    health_reporter_ctx_t* ctx,
    float voltage_v,
    float current_a
) {
    if (!ctx) return;
    
    ctx->power.voltage_mv = (uint16_t)(voltage_v * 1000.0f);
    ctx->power.current_ma = (uint16_t)(current_a * 1000.0f);
    ctx->power.power_mw = (uint32_t)(voltage_v * current_a * 1000.0f);
}

/**
 * @brief Обновить статус чипа
 */
void health_reporter_update_chip(
    health_reporter_ctx_t* ctx,
    uint8_t chip_id,
    int active
) {
    if (!ctx) return;
    (void)chip_id;  /* Не используем ID, просто считаем */
    
    if (active) {
        if (ctx->active_chips < ctx->total_chips) {
            ctx->active_chips++;
        }
    } else {
        if (ctx->active_chips > 0) {
            ctx->active_chips--;
        }
    }
}

/**
 * @brief Записать перезапуск
 */
void health_reporter_record_restart(health_reporter_ctx_t* ctx) {
    if (!ctx) return;
    ctx->uptime.restarts++;
}

/**
 * @brief Проверить, нужно ли отправлять отчёт
 */
int health_reporter_should_report(
    const health_reporter_ctx_t* ctx,
    uint32_t current_time_ms
) {
    if (!ctx) return 0;
    
    uint32_t elapsed = current_time_ms - ctx->last_report_time_ms;
    return elapsed >= HEALTH_REPORT_INTERVAL_MS;
}

/**
 * @brief Сформировать отчёт
 */
void health_reporter_build_report(
    health_reporter_ctx_t* ctx,
    uint32_t current_time_ms,
    health_report_t* report
) {
    if (!ctx || !report) return;
    
    memset(report, 0, sizeof(health_report_t));
    
    report->message_type = 0x83;  /* MSG_HEALTH_REPORT */
    report->overall_status = ctx->overall_status;
    report->flags = 0x1F;  /* Все метрики включены */
    
    report->temperature = ctx->temp;
    report->hashrate = ctx->hashrate;
    report->errors = ctx->errors;
    report->power = ctx->power;
    
    report->active_chips = ctx->active_chips;
    report->total_chips = ctx->total_chips;
    
    /* Обновляем uptime */
    ctx->uptime.uptime_sec = (current_time_ms - ctx->start_time_ms) / 1000;
    
    /* Вычисляем availability */
    if (ctx->uptime.uptime_sec > 0) {
        /* Каждый рестарт = ~30 секунд простоя */
        uint32_t downtime = ctx->uptime.restarts * 30;
        if (downtime < ctx->uptime.uptime_sec) {
            ctx->uptime.availability = (uint16_t)(
                ((ctx->uptime.uptime_sec - downtime) * 10000) / ctx->uptime.uptime_sec
            );
        } else {
            ctx->uptime.availability = 0;
        }
    } else {
        ctx->uptime.availability = 10000;  /* 100% */
    }
    
    ctx->last_report_time_ms = current_time_ms;
}

/**
 * @brief Сериализовать отчёт в буфер
 */
int health_report_serialize(
    const health_report_t* report,
    uint8_t* buf
) {
    if (!report || !buf) return -1;
    
    memcpy(buf, report, sizeof(health_report_t));
    return sizeof(health_report_t);
}
