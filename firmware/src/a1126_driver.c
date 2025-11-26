/**
 * @file a1126_driver.c
 * @brief Драйвер чипов Avalon A1126
 * 
 * Реализация управления ASIC чипами через SPI.
 */

#include "a1126_driver.h"
#include "spi.h"
#include "sha256.h"
#include "config.h"

#include <string.h>

/* Регистры чипа A1126 */
#define A1126_REG_CTRL      0x00    /* Регистр управления */
#define A1126_REG_STATUS    0x01    /* Регистр статуса */
#define A1126_REG_MIDSTATE  0x10    /* Регистры midstate (32 байта) */
#define A1126_REG_TARGET    0x30    /* Регистры target (32 байта) */
#define A1126_REG_NONCE     0x50    /* Регистр найденного nonce */
#define A1126_REG_WORK      0x60    /* Регистры рабочих данных */
#define A1126_REG_TEMP      0x70    /* Регистр температуры */
#define A1126_REG_FREQ      0x80    /* Регистр частоты */

/* Команды управления */
#define A1126_CMD_START     0x01
#define A1126_CMD_STOP      0x02
#define A1126_CMD_RESET     0x04

/* Статус чипа */
#define A1126_STATUS_IDLE   0x00
#define A1126_STATUS_MINING 0x01
#define A1126_STATUS_FOUND  0x02
#define A1126_STATUS_ERROR  0x80

/* Глобальные переменные */
static uint8_t g_target[32];
static uint32_t g_hashrate = 0;
static uint8_t g_avg_temperature = 0;

/* Внутренние функции */

static int chip_write_reg(uint8_t chip_id, uint8_t reg, const uint8_t* data, size_t len) {
    uint8_t cmd[2];
    cmd[0] = 0x80 | reg;  /* Бит записи */
    cmd[1] = (uint8_t)len;
    
    spi_select(chip_id);
    spi_write(cmd, 2);
    spi_write(data, len);
    spi_deselect();
    
    return 0;
}

static int chip_read_reg(uint8_t chip_id, uint8_t reg, uint8_t* data, size_t len) {
    uint8_t cmd[2];
    cmd[0] = reg;  /* Бит чтения = 0 */
    cmd[1] = (uint8_t)len;
    
    spi_select(chip_id);
    spi_write(cmd, 2);
    spi_read(data, len);
    spi_deselect();
    
    return 0;
}

static uint8_t chip_read_status(uint8_t chip_id) {
    uint8_t status;
    chip_read_reg(chip_id, A1126_REG_STATUS, &status, 1);
    return status;
}

/* Публичные функции */

int a1126_init(void) {
    /* Сброс всех чипов */
    return a1126_reset();
}

int a1126_reset(void) {
    uint8_t cmd = A1126_CMD_RESET;
    
    /* Broadcast сброс на все чипы */
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        chip_write_reg((uint8_t)chip, A1126_REG_CTRL, &cmd, 1);
    }
    
    /* Ждём завершения сброса */
    /* delay_ms(100); */
    
    return 0;
}

int a1126_load_job(const quaxis_job_t* job) {
    if (!job) return -1;
    
    uint8_t work_data[48];
    
    /* Копируем midstate */
    memcpy(work_data, job->midstate, 32);
    
    /* Копируем timestamp, bits, nonce_start (little-endian) */
    work_data[32] = (uint8_t)(job->timestamp & 0xFF);
    work_data[33] = (uint8_t)((job->timestamp >> 8) & 0xFF);
    work_data[34] = (uint8_t)((job->timestamp >> 16) & 0xFF);
    work_data[35] = (uint8_t)((job->timestamp >> 24) & 0xFF);
    
    work_data[36] = (uint8_t)(job->bits & 0xFF);
    work_data[37] = (uint8_t)((job->bits >> 8) & 0xFF);
    work_data[38] = (uint8_t)((job->bits >> 16) & 0xFF);
    work_data[39] = (uint8_t)((job->bits >> 24) & 0xFF);
    
    work_data[40] = (uint8_t)(job->nonce_start & 0xFF);
    work_data[41] = (uint8_t)((job->nonce_start >> 8) & 0xFF);
    work_data[42] = (uint8_t)((job->nonce_start >> 16) & 0xFF);
    work_data[43] = (uint8_t)((job->nonce_start >> 24) & 0xFF);
    
    /* job_id не нужен чипу, он хранится на контроллере */
    
    /* Загружаем задание во все чипы */
    /* В реальности каждый чип получает свой диапазон nonce */
    uint32_t nonce_per_chip = 0xFFFFFFFF / A1126_CHIP_COUNT;
    
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        /* Устанавливаем начальный nonce для этого чипа */
        uint32_t start_nonce = (uint32_t)chip * nonce_per_chip;
        work_data[40] = (uint8_t)(start_nonce & 0xFF);
        work_data[41] = (uint8_t)((start_nonce >> 8) & 0xFF);
        work_data[42] = (uint8_t)((start_nonce >> 16) & 0xFF);
        work_data[43] = (uint8_t)((start_nonce >> 24) & 0xFF);
        
        chip_write_reg((uint8_t)chip, A1126_REG_WORK, work_data, 44);
    }
    
    return 0;
}

int a1126_set_target(const uint8_t* target) {
    if (!target) return -1;
    
    memcpy(g_target, target, 32);
    
    /* Отправляем target на все чипы */
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        chip_write_reg((uint8_t)chip, A1126_REG_TARGET, target, 32);
    }
    
    return 0;
}

int a1126_start(void) {
    uint8_t cmd = A1126_CMD_START;
    
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        chip_write_reg((uint8_t)chip, A1126_REG_CTRL, &cmd, 1);
    }
    
    return 0;
}

int a1126_stop(void) {
    uint8_t cmd = A1126_CMD_STOP;
    
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        chip_write_reg((uint8_t)chip, A1126_REG_CTRL, &cmd, 1);
    }
    
    return 0;
}

int a1126_poll_result(a1126_result_t* result) {
    if (!result) return -1;
    
    result->valid = 0;
    
    /* Опрашиваем каждый чип */
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        uint8_t status = chip_read_status((uint8_t)chip);
        
        if (status & A1126_STATUS_FOUND) {
            /* Читаем найденный nonce */
            uint8_t nonce_bytes[4];
            chip_read_reg((uint8_t)chip, A1126_REG_NONCE, nonce_bytes, 4);
            
            result->chip_id = (uint8_t)chip;
            result->nonce = (uint32_t)nonce_bytes[0] |
                           ((uint32_t)nonce_bytes[1] << 8) |
                           ((uint32_t)nonce_bytes[2] << 16) |
                           ((uint32_t)nonce_bytes[3] << 24);
            result->valid = 1;
            
            /* Сбрасываем флаг найденного результата */
            uint8_t clear = 0;
            chip_write_reg((uint8_t)chip, A1126_REG_STATUS, &clear, 1);
            
            return 1;
        }
    }
    
    return 0;
}

int a1126_get_chip_status(uint8_t chip_id, a1126_chip_status_t* status) {
    if (!status || chip_id >= A1126_CHIP_COUNT) return -1;
    
    status->chip_id = chip_id;
    
    /* Читаем температуру */
    chip_read_reg(chip_id, A1126_REG_TEMP, &status->temperature, 1);
    
    /* Читаем статус */
    uint8_t chip_status = chip_read_status(chip_id);
    status->status = (chip_status & A1126_STATUS_ERROR) ? 1 : 0;
    
    /* Остальные поля */
    status->voltage = 0;  /* TODO */
    status->nonce_count = 0;  /* TODO */
    status->error_count = 0;  /* TODO */
    
    return 0;
}

uint32_t a1126_get_hashrate(void) {
    /* В реальности нужно измерять на основе времени и количества проверенных nonce */
    /* Для Avalon 1126 Pro: ~90 TH/s = 90 * 10^12 H/s */
    return g_hashrate;
}

uint8_t a1126_get_temperature(void) {
    uint32_t sum = 0;
    int count = 0;
    
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        uint8_t temp;
        chip_read_reg((uint8_t)chip, A1126_REG_TEMP, &temp, 1);
        if (temp > 0 && temp < 150) {  /* Фильтруем невалидные значения */
            sum += temp;
            count++;
        }
    }
    
    g_avg_temperature = (count > 0) ? (uint8_t)(sum / count) : 0;
    return g_avg_temperature;
}

int a1126_set_frequency(uint16_t freq_mhz) {
    uint8_t freq_data[2];
    freq_data[0] = (uint8_t)(freq_mhz & 0xFF);
    freq_data[1] = (uint8_t)((freq_mhz >> 8) & 0xFF);
    
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        chip_write_reg((uint8_t)chip, A1126_REG_FREQ, freq_data, 2);
    }
    
    return 0;
}

int a1126_set_voltage(uint16_t voltage_mv) {
    /* TODO: Реализация через регулятор напряжения */
    (void)voltage_mv;
    return 0;
}

int a1126_self_test(void) {
    int working = 0;
    
    for (int chip = 0; chip < A1126_CHIP_COUNT; chip++) {
        uint8_t status = chip_read_status((uint8_t)chip);
        
        /* Проверяем, что чип отвечает и нет ошибок */
        if (status != 0xFF && !(status & A1126_STATUS_ERROR)) {
            working++;
        }
    }
    
    return working;
}
