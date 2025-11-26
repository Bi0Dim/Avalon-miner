/**
 * @file spi.h
 * @brief SPI драйвер для связи с чипами
 */

#ifndef QUAXIS_SPI_H
#define QUAXIS_SPI_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Инициализировать SPI интерфейс
 * 
 * @param clock_hz Частота SPI в Hz
 * @param mode SPI режим (0-3)
 * @return 0 при успехе, -1 при ошибке
 */
int spi_init(uint32_t clock_hz, uint8_t mode);

/**
 * @brief Выбрать чип (активировать CS)
 * 
 * @param chip_id ID чипа (0-113)
 */
void spi_select(uint8_t chip_id);

/**
 * @brief Снять выбор чипа (деактивировать CS)
 */
void spi_deselect(void);

/**
 * @brief Передать и принять данные
 * 
 * @param tx_data Данные для передачи (может быть NULL)
 * @param rx_data Буфер для приёма (может быть NULL)
 * @param len Длина данных
 * @return 0 при успехе, -1 при ошибке
 */
int spi_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len);

/**
 * @brief Передать данные
 * 
 * @param data Данные для передачи
 * @param len Длина данных
 * @return 0 при успехе, -1 при ошибке
 */
int spi_write(const uint8_t* data, size_t len);

/**
 * @brief Принять данные
 * 
 * @param data Буфер для приёма
 * @param len Длина данных
 * @return 0 при успехе, -1 при ошибке
 */
int spi_read(uint8_t* data, size_t len);

/**
 * @brief Передать один байт и принять ответ
 * 
 * @param byte Байт для передачи
 * @return Принятый байт
 */
uint8_t spi_exchange(uint8_t byte);

/**
 * @brief Broadcast команды всем чипам
 * 
 * @param data Данные для передачи
 * @param len Длина данных
 * @return 0 при успехе, -1 при ошибке
 */
int spi_broadcast(const uint8_t* data, size_t len);

#endif /* QUAXIS_SPI_H */
