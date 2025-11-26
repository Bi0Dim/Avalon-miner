/**
 * @file utils.c
 * @brief Вспомогательные функции
 */

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Задержка в миллисекундах (заглушка)
 */
void delay_ms(uint32_t ms) {
    /* TODO: Реализовать для конкретной платформы */
    /* Использовать таймер или busy-wait */
    volatile uint32_t count = ms * 1000;
    while (count > 0) {
        count--;
    }
}

/**
 * @brief Получить текущее время в миллисекундах (заглушка)
 */
uint32_t millis(void) {
    /* TODO: Реализовать с использованием системного таймера */
    static uint32_t tick = 0;
    return tick++;
}

/**
 * @brief Конвертировать uint32_t в little-endian байты
 */
void uint32_to_le(uint32_t value, uint8_t* buf) {
    buf[0] = (uint8_t)(value & 0xFF);
    buf[1] = (uint8_t)((value >> 8) & 0xFF);
    buf[2] = (uint8_t)((value >> 16) & 0xFF);
    buf[3] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * @brief Конвертировать little-endian байты в uint32_t
 */
uint32_t le_to_uint32(const uint8_t* buf) {
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

/**
 * @brief Конвертировать uint32_t в big-endian байты
 */
void uint32_to_be(uint32_t value, uint8_t* buf) {
    buf[0] = (uint8_t)((value >> 24) & 0xFF);
    buf[1] = (uint8_t)((value >> 16) & 0xFF);
    buf[2] = (uint8_t)((value >> 8) & 0xFF);
    buf[3] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Конвертировать big-endian байты в uint32_t
 */
uint32_t be_to_uint32(const uint8_t* buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           (uint32_t)buf[3];
}

/**
 * @brief Hex to string для отладки
 */
void hex_to_str(const uint8_t* data, size_t len, char* out) {
    static const char hex_chars[] = "0123456789abcdef";
    
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex_chars[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
}
