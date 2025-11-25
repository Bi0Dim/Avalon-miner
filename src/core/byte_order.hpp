/**
 * @file byte_order.hpp
 * @brief Функции для работы с порядком байт (endianness)
 * 
 * Bitcoin использует little-endian для большинства числовых полей,
 * но некоторые поля (например, target) используют big-endian.
 * 
 * Этот модуль предоставляет функции для корректного преобразования
 * между форматами хоста и сетевым/дисковым форматом.
 * 
 * @note Все функции помечены constexpr для compile-time вычислений.
 * @note Все функции помечены noexcept так как не выбрасывают исключений.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <span>
#include <bit>
#include <concepts>

namespace quaxis {

// =============================================================================
// Concepts для числовых типов
// =============================================================================

/**
 * @brief Concept для целочисленных типов фиксированного размера
 */
template<typename T>
concept UnsignedInteger = std::unsigned_integral<T> && 
                          (sizeof(T) == 1 || sizeof(T) == 2 || 
                           sizeof(T) == 4 || sizeof(T) == 8);

// =============================================================================
// Детекция порядка байт системы
// =============================================================================

/**
 * @brief Проверка: система little-endian?
 * 
 * @return true если система little-endian (x86, ARM в LE режиме)
 */
[[nodiscard]] consteval bool is_little_endian() noexcept {
    return std::endian::native == std::endian::little;
}

/**
 * @brief Проверка: система big-endian?
 * 
 * @return true если система big-endian (некоторые ARM, PowerPC)
 */
[[nodiscard]] consteval bool is_big_endian() noexcept {
    return std::endian::native == std::endian::big;
}

// =============================================================================
// Преобразование порядка байт
// =============================================================================

/**
 * @brief Поменять порядок байт (byte swap)
 * 
 * @tparam T Тип целого числа (uint16_t, uint32_t, uint64_t)
 * @param value Значение для преобразования
 * @return Значение с обратным порядком байт
 */
template<UnsignedInteger T>
[[nodiscard]] constexpr T byte_swap(T value) noexcept {
    if constexpr (sizeof(T) == 1) {
        return value;
    } else if constexpr (sizeof(T) == 2) {
        return static_cast<T>(((value & 0x00FF) << 8) |
                              ((value & 0xFF00) >> 8));
    } else if constexpr (sizeof(T) == 4) {
        return static_cast<T>(((value & 0x000000FF) << 24) |
                              ((value & 0x0000FF00) << 8)  |
                              ((value & 0x00FF0000) >> 8)  |
                              ((value & 0xFF000000) >> 24));
    } else { // sizeof(T) == 8
        return static_cast<T>(((value & 0x00000000000000FFULL) << 56) |
                              ((value & 0x000000000000FF00ULL) << 40) |
                              ((value & 0x0000000000FF0000ULL) << 24) |
                              ((value & 0x00000000FF000000ULL) << 8)  |
                              ((value & 0x000000FF00000000ULL) >> 8)  |
                              ((value & 0x0000FF0000000000ULL) >> 24) |
                              ((value & 0x00FF000000000000ULL) >> 40) |
                              ((value & 0xFF00000000000000ULL) >> 56));
    }
}

// =============================================================================
// Преобразование в/из little-endian
// =============================================================================

/**
 * @brief Преобразовать число из формата хоста в little-endian
 * 
 * На little-endian системах (x86) - ничего не делает.
 * На big-endian системах - меняет порядок байт.
 * 
 * @tparam T Тип целого числа
 * @param value Значение в формате хоста
 * @return Значение в little-endian формате
 */
template<UnsignedInteger T>
[[nodiscard]] constexpr T to_little_endian(T value) noexcept {
    if constexpr (is_little_endian()) {
        return value;
    } else {
        return byte_swap(value);
    }
}

/**
 * @brief Преобразовать число из little-endian в формат хоста
 * 
 * @tparam T Тип целого числа
 * @param value Значение в little-endian формате
 * @return Значение в формате хоста
 */
template<UnsignedInteger T>
[[nodiscard]] constexpr T from_little_endian(T value) noexcept {
    return to_little_endian(value); // Симметричная операция
}

// =============================================================================
// Преобразование в/из big-endian
// =============================================================================

/**
 * @brief Преобразовать число из формата хоста в big-endian
 * 
 * На big-endian системах - ничего не делает.
 * На little-endian системах - меняет порядок байт.
 * 
 * @tparam T Тип целого числа
 * @param value Значение в формате хоста
 * @return Значение в big-endian формате
 */
template<UnsignedInteger T>
[[nodiscard]] constexpr T to_big_endian(T value) noexcept {
    if constexpr (is_big_endian()) {
        return value;
    } else {
        return byte_swap(value);
    }
}

/**
 * @brief Преобразовать число из big-endian в формат хоста
 * 
 * @tparam T Тип целого числа
 * @param value Значение в big-endian формате
 * @return Значение в формате хоста
 */
template<UnsignedInteger T>
[[nodiscard]] constexpr T from_big_endian(T value) noexcept {
    return to_big_endian(value); // Симметричная операция
}

// =============================================================================
// Чтение/запись из/в байтовый массив
// =============================================================================

/**
 * @brief Записать uint16_t в little-endian формате
 * 
 * @param dest Указатель на буфер (минимум 2 байта)
 * @param value Значение для записи
 */
inline void write_le16(uint8_t* dest, uint16_t value) noexcept {
    value = to_little_endian(value);
    std::memcpy(dest, &value, sizeof(value));
}

/**
 * @brief Записать uint32_t в little-endian формате
 * 
 * @param dest Указатель на буфер (минимум 4 байта)
 * @param value Значение для записи
 */
inline void write_le32(uint8_t* dest, uint32_t value) noexcept {
    value = to_little_endian(value);
    std::memcpy(dest, &value, sizeof(value));
}

/**
 * @brief Записать uint64_t в little-endian формате
 * 
 * @param dest Указатель на буфер (минимум 8 байт)
 * @param value Значение для записи
 */
inline void write_le64(uint8_t* dest, uint64_t value) noexcept {
    value = to_little_endian(value);
    std::memcpy(dest, &value, sizeof(value));
}

/**
 * @brief Прочитать uint16_t из little-endian буфера
 * 
 * @param src Указатель на буфер (минимум 2 байта)
 * @return Значение в формате хоста
 */
[[nodiscard]] inline uint16_t read_le16(const uint8_t* src) noexcept {
    uint16_t value;
    std::memcpy(&value, src, sizeof(value));
    return from_little_endian(value);
}

/**
 * @brief Прочитать uint32_t из little-endian буфера
 * 
 * @param src Указатель на буфер (минимум 4 байта)
 * @return Значение в формате хоста
 */
[[nodiscard]] inline uint32_t read_le32(const uint8_t* src) noexcept {
    uint32_t value;
    std::memcpy(&value, src, sizeof(value));
    return from_little_endian(value);
}

/**
 * @brief Прочитать uint64_t из little-endian буфера
 * 
 * @param src Указатель на буфер (минимум 8 байт)
 * @return Значение в формате хоста
 */
[[nodiscard]] inline uint64_t read_le64(const uint8_t* src) noexcept {
    uint64_t value;
    std::memcpy(&value, src, sizeof(value));
    return from_little_endian(value);
}

/**
 * @brief Записать uint32_t в big-endian формате
 * 
 * @param dest Указатель на буфер (минимум 4 байта)
 * @param value Значение для записи
 */
inline void write_be32(uint8_t* dest, uint32_t value) noexcept {
    value = to_big_endian(value);
    std::memcpy(dest, &value, sizeof(value));
}

/**
 * @brief Прочитать uint32_t из big-endian буфера
 * 
 * @param src Указатель на буфер (минимум 4 байта)
 * @return Значение в формате хоста
 */
[[nodiscard]] inline uint32_t read_be32(const uint8_t* src) noexcept {
    uint32_t value;
    std::memcpy(&value, src, sizeof(value));
    return from_big_endian(value);
}

// =============================================================================
// Реверс массива байт (для хешей)
// =============================================================================

/**
 * @brief Реверсировать массив байт на месте
 * 
 * Используется для преобразования хешей между internal/display форматами.
 * В Bitcoin block hash отображается в reversed формате.
 * 
 * @param data Span байт для реверса
 */
inline void reverse_bytes(std::span<uint8_t> data) noexcept {
    auto begin = data.begin();
    auto end = data.end();
    while (begin < end) {
        --end;
        std::swap(*begin, *end);
        ++begin;
    }
}

/**
 * @brief Создать реверсированную копию массива байт
 * 
 * @tparam N Размер массива
 * @param input Входной массив
 * @return Реверсированный массив
 */
template<std::size_t N>
[[nodiscard]] constexpr std::array<uint8_t, N> reverse_copy(
    const std::array<uint8_t, N>& input
) noexcept {
    std::array<uint8_t, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = input[N - 1 - i];
    }
    return result;
}

} // namespace quaxis
