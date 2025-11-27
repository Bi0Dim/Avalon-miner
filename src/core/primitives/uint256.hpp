/**
 * @file uint256.hpp
 * @brief 256-битное беззнаковое целое число
 * 
 * Предоставляет тип для работы с 256-битными числами,
 * используемыми в Bitcoin для хешей и targets.
 */

#pragma once

#include "../types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <algorithm>
#include <compare>

namespace quaxis::core {

/**
 * @brief 256-битное беззнаковое целое число
 * 
 * Хранится в little-endian формате (как в Bitcoin).
 * Используется для:
 * - Хешей блоков и транзакций
 * - Targets (порогов сложности)
 * - Сравнения proof-of-work
 */
class uint256 {
public:
    /// @brief Размер в байтах
    static constexpr std::size_t SIZE = 32;
    
    /// @brief Конструктор по умолчанию (нулевое значение)
    constexpr uint256() noexcept : data_{} {}
    
    /// @brief Конструктор из массива байт
    constexpr explicit uint256(const Hash256& hash) noexcept : data_(hash) {}
    
    /// @brief Конструктор из массива байт (rvalue)
    constexpr explicit uint256(Hash256&& hash) noexcept : data_(std::move(hash)) {}
    
    /// @brief Конструктор из 64-битного числа
    constexpr explicit uint256(uint64_t value) noexcept : data_{} {
        data_[0] = static_cast<uint8_t>(value);
        data_[1] = static_cast<uint8_t>(value >> 8);
        data_[2] = static_cast<uint8_t>(value >> 16);
        data_[3] = static_cast<uint8_t>(value >> 24);
        data_[4] = static_cast<uint8_t>(value >> 32);
        data_[5] = static_cast<uint8_t>(value >> 40);
        data_[6] = static_cast<uint8_t>(value >> 48);
        data_[7] = static_cast<uint8_t>(value >> 56);
    }
    
    // =========================================================================
    // Доступ к данным
    // =========================================================================
    
    /**
     * @brief Получить указатель на данные
     */
    [[nodiscard]] constexpr const uint8_t* data() const noexcept {
        return data_.data();
    }
    
    /**
     * @brief Получить изменяемый указатель на данные
     */
    [[nodiscard]] constexpr uint8_t* data() noexcept {
        return data_.data();
    }
    
    /**
     * @brief Получить размер
     */
    [[nodiscard]] static constexpr std::size_t size() noexcept {
        return SIZE;
    }
    
    /**
     * @brief Получить байт по индексу
     */
    [[nodiscard]] constexpr uint8_t operator[](std::size_t i) const noexcept {
        return data_[i];
    }
    
    /**
     * @brief Получить изменяемый байт по индексу
     */
    [[nodiscard]] constexpr uint8_t& operator[](std::size_t i) noexcept {
        return data_[i];
    }
    
    /**
     * @brief Преобразовать в Hash256
     */
    [[nodiscard]] constexpr const Hash256& to_hash256() const noexcept {
        return data_;
    }
    
    /**
     * @brief Проверить, является ли нулём
     */
    [[nodiscard]] constexpr bool is_zero() const noexcept {
        for (auto b : data_) {
            if (b != 0) return false;
        }
        return true;
    }
    
    // =========================================================================
    // Сравнение
    // =========================================================================
    
    /**
     * @brief Оператор сравнения (трёхстороннее)
     * 
     * Сравнивает числа как big-endian (старшие байты сначала).
     */
    [[nodiscard]] constexpr std::strong_ordering operator<=>(
        const uint256& other
    ) const noexcept {
        // Сравниваем с конца (big-endian)
        for (std::size_t i = SIZE; i-- > 0;) {
            if (data_[i] < other.data_[i]) return std::strong_ordering::less;
            if (data_[i] > other.data_[i]) return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }
    
    /**
     * @brief Оператор равенства
     */
    [[nodiscard]] constexpr bool operator==(const uint256& other) const noexcept {
        return data_ == other.data_;
    }
    
    // =========================================================================
    // Строковое представление
    // =========================================================================
    
    /**
     * @brief Преобразовать в hex строку (big-endian, как в explorer)
     */
    [[nodiscard]] std::string to_hex() const;
    
    /**
     * @brief Преобразовать в hex строку (little-endian, как в wire)
     */
    [[nodiscard]] std::string to_hex_le() const;
    
    /**
     * @brief Создать из hex строки (big-endian)
     */
    [[nodiscard]] static uint256 from_hex(std::string_view hex);
    
    /**
     * @brief Создать из hex строки (little-endian)
     */
    [[nodiscard]] static uint256 from_hex_le(std::string_view hex);
    
    // =========================================================================
    // Статические константы
    // =========================================================================
    
    /**
     * @brief Нулевое значение
     */
    [[nodiscard]] static constexpr uint256 zero() noexcept {
        return uint256{};
    }
    
    /**
     * @brief Максимальное значение (все биты = 1)
     */
    [[nodiscard]] static constexpr uint256 max() noexcept {
        uint256 result;
        for (auto& b : result.data_) {
            b = 0xFF;
        }
        return result;
    }
    
    /**
     * @brief Единица (1)
     */
    [[nodiscard]] static constexpr uint256 one() noexcept {
        return uint256{1ULL};
    }
    
private:
    Hash256 data_;
};

} // namespace quaxis::core
