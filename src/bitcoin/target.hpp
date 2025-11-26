/**
 * @file target.hpp
 * @brief Работа с Bitcoin difficulty и target
 * 
 * Bitcoin использует compact формат "bits" для представления target.
 * Target - это 256-битное число, хеш блока должен быть меньше или равен target.
 * 
 * Формат bits (4 байта):
 * - Первый байт: exponent (показатель степени)
 * - Следующие 3 байта: mantissa (мантисса)
 * 
 * target = mantissa * 2^(8 * (exponent - 3))
 * 
 * Пример:
 * bits = 0x1b0404cb
 * exponent = 0x1b = 27
 * mantissa = 0x0404cb
 * target = 0x0404cb * 2^(8 * (27 - 3)) = 0x0404cb * 2^192
 */

#pragma once

#include "../core/types.hpp"

#include <cstdint>
#include <string>

namespace quaxis::bitcoin {

// =============================================================================
// Преобразования bits <-> target
// =============================================================================

/**
 * @brief Преобразовать compact bits в 256-битный target
 * 
 * @param bits Compact target (4 байта из заголовка блока)
 * @return Hash256 256-битный target
 */
[[nodiscard]] Hash256 bits_to_target(uint32_t bits) noexcept;

/**
 * @brief Преобразовать 256-битный target в compact bits
 * 
 * @param target 256-битный target
 * @return uint32_t Compact bits
 */
[[nodiscard]] uint32_t target_to_bits(const Hash256& target) noexcept;

// =============================================================================
// Difficulty
// =============================================================================

/**
 * @brief Вычислить difficulty из compact bits
 * 
 * difficulty = max_target / current_target
 * 
 * где max_target = 0x00000000FFFF0000...0000 (genesis difficulty)
 * 
 * @param bits Compact target
 * @return double Difficulty
 */
[[nodiscard]] double bits_to_difficulty(uint32_t bits) noexcept;

/**
 * @brief Вычислить difficulty из 256-битного target
 * 
 * @param target 256-битный target
 * @return double Difficulty
 */
[[nodiscard]] double target_to_difficulty(const Hash256& target) noexcept;

// =============================================================================
// Проверки
// =============================================================================

/**
 * @brief Проверить, что хеш удовлетворяет target
 * 
 * @param hash Хеш блока
 * @param target 256-битный target
 * @return true если hash <= target
 */
[[nodiscard]] bool meets_target(const Hash256& hash, const Hash256& target) noexcept;

/**
 * @brief Проверить, что хеш удовлетворяет compact bits
 * 
 * @param hash Хеш блока
 * @param bits Compact target
 * @return true если hash <= target
 */
[[nodiscard]] bool meets_bits(const Hash256& hash, uint32_t bits) noexcept;

// =============================================================================
// Форматирование
// =============================================================================

/**
 * @brief Форматировать difficulty для отображения
 * 
 * @param difficulty Значение difficulty
 * @return std::string Форматированная строка (например, "1.23 T" или "456.78 P")
 */
[[nodiscard]] std::string format_difficulty(double difficulty);

/**
 * @brief Преобразовать target в hex строку
 * 
 * @param target 256-битный target
 * @return std::string Hex строка (64 символа)
 */
[[nodiscard]] std::string target_to_hex(const Hash256& target);

/**
 * @brief Преобразовать hex строку в target
 * 
 * @param hex Hex строка (64 символа)
 * @return Result<Hash256> Target или ошибка парсинга
 */
[[nodiscard]] Result<Hash256> hex_to_target(std::string_view hex);

} // namespace quaxis::bitcoin
