/**
 * @file a1126_driver.h
 * @brief Драйвер чипов Avalon A1126
 * 
 * Управление ASIC чипами Avalon 1126 Pro через SPI интерфейс.
 */

#ifndef QUAXIS_A1126_DRIVER_H
#define QUAXIS_A1126_DRIVER_H

#include <stdint.h>
#include "protocol.h"

/**
 * @brief Статус чипа
 */
typedef struct {
    uint8_t  chip_id;           /* ID чипа (0-113) */
    uint8_t  temperature;       /* Температура (°C) */
    uint8_t  voltage;           /* Напряжение (x10 mV) */
    uint8_t  status;            /* Статус: 0=OK, 1=Error */
    uint32_t nonce_count;       /* Количество проверенных nonce */
    uint32_t error_count;       /* Количество ошибок */
} a1126_chip_status_t;

/**
 * @brief Результат от чипа
 */
typedef struct {
    uint8_t  chip_id;           /* ID чипа */
    uint32_t nonce;             /* Найденный nonce */
    uint8_t  valid;             /* 1 если результат валиден */
} a1126_result_t;

/**
 * @brief Инициализировать драйвер чипов
 * 
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_init(void);

/**
 * @brief Сбросить все чипы
 * 
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_reset(void);

/**
 * @brief Загрузить задание во все чипы
 * 
 * @param job Указатель на задание
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_load_job(const quaxis_job_t* job);

/**
 * @brief Загрузить target во все чипы
 * 
 * @param target 32-байтный target
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_set_target(const uint8_t* target);

/**
 * @brief Запустить майнинг на всех чипах
 * 
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_start(void);

/**
 * @brief Остановить майнинг на всех чипах
 * 
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_stop(void);

/**
 * @brief Проверить наличие результатов от чипов
 * 
 * @param result Указатель на структуру для результата
 * @return 1 если есть результат, 0 если нет, -1 при ошибке
 */
int a1126_poll_result(a1126_result_t* result);

/**
 * @brief Получить статус чипа
 * 
 * @param chip_id ID чипа (0-113)
 * @param status Указатель на структуру для статуса
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_get_chip_status(uint8_t chip_id, a1126_chip_status_t* status);

/**
 * @brief Получить общий хешрейт
 * 
 * @return Хешрейт в H/s
 */
uint32_t a1126_get_hashrate(void);

/**
 * @brief Получить среднюю температуру
 * 
 * @return Температура в °C
 */
uint8_t a1126_get_temperature(void);

/**
 * @brief Установить частоту чипов
 * 
 * @param freq_mhz Частота в MHz
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_set_frequency(uint16_t freq_mhz);

/**
 * @brief Установить напряжение чипов
 * 
 * @param voltage_mv Напряжение в mV
 * @return 0 при успехе, -1 при ошибке
 */
int a1126_set_voltage(uint16_t voltage_mv);

/**
 * @brief Выполнить самодиагностику
 * 
 * @return Количество рабочих чипов
 */
int a1126_self_test(void);

#endif /* QUAXIS_A1126_DRIVER_H */
