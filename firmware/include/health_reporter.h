/**
 * @file health_reporter.h
 * @brief Отправка метрик здоровья с прошивки ASIC
 * 
 * Health Reporter собирает и отправляет метрики:
 * - Температура (avg, max, min)
 * - Хешрейт
 * - Error rate
 * - Power consumption
 */

#ifndef QUAXIS_HEALTH_REPORTER_H
#define QUAXIS_HEALTH_REPORTER_H

#include <stdint.h>

/*
 * Константы Health Reporter
 */
#define HEALTH_REPORT_INTERVAL_MS   5000    /* Интервал отправки отчёта */
#define TEMP_SAMPLES_COUNT          10      /* Количество сэмплов для усреднения */

/*
 * Коды типов метрик
 */
#define METRIC_TYPE_TEMPERATURE     0x01
#define METRIC_TYPE_HASHRATE        0x02
#define METRIC_TYPE_ERRORS          0x03
#define METRIC_TYPE_POWER           0x04
#define METRIC_TYPE_UPTIME          0x05
#define METRIC_TYPE_CHIP_STATUS     0x06

/*
 * Размер сообщения с метриками здоровья
 */
#define HEALTH_MESSAGE_SIZE         48

/*
 * Статус чипа
 */
#define CHIP_STATUS_OK              0
#define CHIP_STATUS_WARNING         1
#define CHIP_STATUS_ERROR           2
#define CHIP_STATUS_OFFLINE         3

/**
 * @brief Структура метрик температуры
 */
typedef struct {
    int16_t  current;       /* Текущая температура (°C * 10) */
    int16_t  average;       /* Средняя температура (°C * 10) */
    int16_t  max;           /* Максимальная температура (°C * 10) */
    int16_t  min;           /* Минимальная температура (°C * 10) */
} temp_metrics_t;

/**
 * @brief Структура метрик хешрейта
 */
typedef struct {
    uint32_t current_hps;   /* Текущий хешрейт (H/s) */
    uint32_t average_hps;   /* Средний хешрейт (H/s) */
    uint8_t  efficiency;    /* Эффективность (0-100%) */
    uint8_t  reserved[3];
} hashrate_metrics_t;

/**
 * @brief Структура метрик ошибок
 */
typedef struct {
    uint32_t hw_errors;         /* HW ошибки */
    uint32_t rejected_shares;   /* Отклонённые шары */
    uint32_t stale_shares;      /* Устаревшие шары */
    uint32_t total_shares;      /* Всего шар */
} error_metrics_t;

/**
 * @brief Структура метрик питания
 */
typedef struct {
    uint16_t voltage_mv;    /* Напряжение (мВ) */
    uint16_t current_ma;    /* Ток (мА) */
    uint32_t power_mw;      /* Мощность (мВт) */
} power_metrics_t;

/**
 * @brief Структура метрик uptime
 */
typedef struct {
    uint32_t uptime_sec;    /* Время работы (секунды) */
    uint16_t restarts;      /* Количество перезапусков */
    uint16_t availability;  /* Доступность (0-10000 = 0-100.00%) */
} uptime_metrics_t;

/**
 * @brief Структура статуса чипа
 */
typedef struct {
    uint8_t  chip_id;       /* ID чипа */
    uint8_t  status;        /* Статус (CHIP_STATUS_*) */
    int16_t  temperature;   /* Температура (°C * 10) */
    uint32_t hashrate;      /* Хешрейт (H/s) */
    uint32_t errors;        /* Ошибки */
} chip_status_t;

/**
 * @brief Полный отчёт о здоровье (48 байт)
 */
typedef struct __attribute__((packed)) {
    uint8_t  message_type;      /* 0x83 = MSG_HEALTH_REPORT */
    uint8_t  overall_status;    /* Общий статус (0=ok, 1=warn, 2=crit, 3=emergency) */
    uint16_t flags;             /* Флаги (какие метрики включены) */
    
    temp_metrics_t temperature; /* 8 байт */
    hashrate_metrics_t hashrate;/* 12 байт */
    error_metrics_t errors;     /* 16 байт */
    power_metrics_t power;      /* 8 байт */
    
    uint16_t active_chips;      /* Количество активных чипов */
    uint16_t total_chips;       /* Общее количество чипов */
} health_report_t;

/**
 * @brief Контекст health reporter
 */
typedef struct {
    /* Сэмплы температуры для усреднения */
    int16_t temp_samples[TEMP_SAMPLES_COUNT];
    uint8_t temp_sample_idx;
    
    /* Накопленные метрики */
    temp_metrics_t temp;
    hashrate_metrics_t hashrate;
    error_metrics_t errors;
    power_metrics_t power;
    uptime_metrics_t uptime;
    
    /* Статус чипов */
    uint16_t active_chips;
    uint16_t total_chips;
    
    /* Время начала работы */
    uint32_t start_time_ms;
    
    /* Время последнего отчёта */
    uint32_t last_report_time_ms;
    
    /* Общий статус */
    uint8_t overall_status;
    
    /* Пороги */
    int16_t temp_warning;   /* °C * 10 */
    int16_t temp_critical;  /* °C * 10 */
    int16_t temp_emergency; /* °C * 10 */
} health_reporter_ctx_t;

/**
 * @brief Инициализировать health reporter
 * 
 * @param ctx Контекст
 * @param total_chips Общее количество чипов
 * @param current_time_ms Текущее время (мс)
 */
void health_reporter_init(
    health_reporter_ctx_t* ctx,
    uint16_t total_chips,
    uint32_t current_time_ms
);

/**
 * @brief Установить пороги температуры
 * 
 * @param ctx Контекст
 * @param warning Порог предупреждения (°C)
 * @param critical Критический порог (°C)
 * @param emergency Аварийный порог (°C)
 */
void health_reporter_set_thresholds(
    health_reporter_ctx_t* ctx,
    float warning,
    float critical,
    float emergency
);

/**
 * @brief Обновить температуру
 * 
 * @param ctx Контекст
 * @param temperature Температура (°C)
 */
void health_reporter_update_temp(
    health_reporter_ctx_t* ctx,
    float temperature
);

/**
 * @brief Обновить хешрейт
 * 
 * @param ctx Контекст
 * @param hashrate Хешрейт (H/s)
 * @param nominal_hashrate Номинальный хешрейт (H/s)
 */
void health_reporter_update_hashrate(
    health_reporter_ctx_t* ctx,
    uint32_t hashrate,
    uint32_t nominal_hashrate
);

/**
 * @brief Записать ошибку
 * 
 * @param ctx Контекст
 * @param hw_error HW ошибка
 * @param rejected Отклонённый share
 * @param stale Устаревший share
 */
void health_reporter_record_error(
    health_reporter_ctx_t* ctx,
    int hw_error,
    int rejected,
    int stale
);

/**
 * @brief Записать успешный share
 */
void health_reporter_record_share(health_reporter_ctx_t* ctx);

/**
 * @brief Обновить питание
 * 
 * @param ctx Контекст
 * @param voltage_v Напряжение (В)
 * @param current_a Ток (А)
 */
void health_reporter_update_power(
    health_reporter_ctx_t* ctx,
    float voltage_v,
    float current_a
);

/**
 * @brief Обновить статус чипа
 * 
 * @param ctx Контекст
 * @param chip_id ID чипа
 * @param active Чип активен
 */
void health_reporter_update_chip(
    health_reporter_ctx_t* ctx,
    uint8_t chip_id,
    int active
);

/**
 * @brief Записать перезапуск
 */
void health_reporter_record_restart(health_reporter_ctx_t* ctx);

/**
 * @brief Проверить, нужно ли отправлять отчёт
 * 
 * @param ctx Контекст
 * @param current_time_ms Текущее время (мс)
 * @return 1 если нужно отправить отчёт
 */
int health_reporter_should_report(
    const health_reporter_ctx_t* ctx,
    uint32_t current_time_ms
);

/**
 * @brief Сформировать отчёт
 * 
 * @param ctx Контекст
 * @param current_time_ms Текущее время (мс)
 * @param report Указатель на структуру отчёта
 */
void health_reporter_build_report(
    health_reporter_ctx_t* ctx,
    uint32_t current_time_ms,
    health_report_t* report
);

/**
 * @brief Сериализовать отчёт в буфер
 * 
 * @param report Отчёт
 * @param buf Буфер (минимум HEALTH_MESSAGE_SIZE байт)
 * @return Количество записанных байт
 */
int health_report_serialize(
    const health_report_t* report,
    uint8_t* buf
);

/**
 * @brief Получить общий статус
 * 
 * @param ctx Контекст
 * @return Статус (0=ok, 1=warn, 2=crit, 3=emergency)
 */
static inline uint8_t health_reporter_get_status(
    const health_reporter_ctx_t* ctx
) {
    return ctx->overall_status;
}

/**
 * @brief Проверить, требуется ли действие
 * 
 * @param ctx Контекст
 * @return 1 если требуется действие (статус != OK)
 */
static inline int health_reporter_requires_action(
    const health_reporter_ctx_t* ctx
) {
    return ctx->overall_status > 0;
}

#endif /* QUAXIS_HEALTH_REPORTER_H */
