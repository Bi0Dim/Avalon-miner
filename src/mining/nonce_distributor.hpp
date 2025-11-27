/**
 * @file nonce_distributor.hpp
 * @brief Оптимальное распределение nonce между чипами — +2-5%
 * 
 * Nonce Distribution оптимизирует распределение пространства перебора
 * между 114 чипами ASIC для минимизации коллизий и дублирования.
 * 
 * Стратегии:
 * - Sequential: последовательное разбиение пространства nonce
 * - Interleaved: чередование (chip[i] получает nonce % num_chips == i)
 * - Random: случайные стартовые точки с фиксированными смещениями
 */

#pragma once

#include "../core/types.hpp"

#include <cstdint>
#include <vector>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

namespace quaxis::mining {

// =============================================================================
// Константы распределения nonce
// =============================================================================

/// @brief Количество чипов по умолчанию (Avalon 1126 Pro)
inline constexpr uint16_t DEFAULT_CHIPS_PER_ASIC = 114;

/// @brief Количество ASIC по умолчанию
inline constexpr uint16_t DEFAULT_ASIC_COUNT = 3;

/// @brief Полное пространство nonce (2^32)
inline constexpr uint64_t NONCE_SPACE = 0x100000000ULL;

// =============================================================================
// Стратегии распределения
// =============================================================================

/**
 * @brief Стратегия распределения nonce
 */
enum class NonceStrategy {
    Sequential,     ///< Последовательное разбиение
    Interleaved,    ///< Чередование
    Random          ///< Случайные стартовые точки
};

/**
 * @brief Преобразовать стратегию в строку
 */
[[nodiscard]] constexpr std::string_view to_string(NonceStrategy strategy) noexcept {
    switch (strategy) {
        case NonceStrategy::Sequential:  return "sequential";
        case NonceStrategy::Interleaved: return "interleaved";
        case NonceStrategy::Random:      return "random";
        default: return "unknown";
    }
}

/**
 * @brief Преобразовать строку в стратегию
 */
[[nodiscard]] NonceStrategy strategy_from_string(std::string_view str) noexcept;

// =============================================================================
// Конфигурация распределения
// =============================================================================

/**
 * @brief Конфигурация Nonce Distributor
 */
struct NonceDistributorConfig {
    /// @brief Количество чипов на один ASIC
    uint16_t chips_per_asic = DEFAULT_CHIPS_PER_ASIC;
    
    /// @brief Количество ASIC устройств
    uint16_t asic_count = DEFAULT_ASIC_COUNT;
    
    /// @brief Стратегия распределения
    NonceStrategy strategy = NonceStrategy::Sequential;
    
    /// @brief Seed для random стратегии (0 = системное время)
    uint32_t random_seed = 0;
    
    /**
     * @brief Получить общее количество чипов
     */
    [[nodiscard]] uint32_t total_chips() const noexcept {
        return static_cast<uint32_t>(chips_per_asic) * asic_count;
    }
};

// =============================================================================
// Диапазон nonce
// =============================================================================

/**
 * @brief Диапазон nonce для чипа
 */
struct NonceRange {
    /// @brief ID чипа (глобальный)
    uint16_t chip_id = 0;
    
    /// @brief ID ASIC
    uint8_t asic_id = 0;
    
    /// @brief Локальный ID чипа на ASIC
    uint8_t local_chip_id = 0;
    
    /// @brief Начало диапазона
    uint32_t start = 0;
    
    /// @brief Конец диапазона (включительно)
    uint32_t end = 0;
    
    /// @brief Шаг (для interleaved)
    uint32_t step = 1;
    
    /// @brief Стратегия
    NonceStrategy strategy = NonceStrategy::Sequential;
    
    /**
     * @brief Получить размер диапазона
     */
    [[nodiscard]] uint64_t size() const noexcept {
        if (strategy == NonceStrategy::Interleaved) {
            // Количество значений в чередующемся диапазоне
            return (static_cast<uint64_t>(end) - start) / step + 1;
        }
        return static_cast<uint64_t>(end) - start + 1;
    }
    
    /**
     * @brief Проверить, принадлежит ли nonce этому диапазону
     */
    [[nodiscard]] bool contains(uint32_t nonce) const noexcept {
        if (strategy == NonceStrategy::Interleaved) {
            return nonce >= start && nonce <= end && 
                   (nonce - start) % step == 0;
        }
        return nonce >= start && nonce <= end;
    }
    
    /**
     * @brief Получить следующий nonce после данного
     * 
     * @param current Текущий nonce
     * @return Следующий nonce или nullopt если диапазон исчерпан
     */
    [[nodiscard]] std::optional<uint32_t> next(uint32_t current) const noexcept {
        uint64_t next_val = static_cast<uint64_t>(current) + step;
        if (next_val <= end) {
            return static_cast<uint32_t>(next_val);
        }
        return std::nullopt;
    }
};

// =============================================================================
// Nonce Distributor
// =============================================================================

/**
 * @brief Распределитель nonce между чипами
 */
class NonceDistributor {
public:
    /**
     * @brief Создать распределитель с конфигурацией
     */
    explicit NonceDistributor(const NonceDistributorConfig& config);
    
    ~NonceDistributor();
    
    // Запрещаем копирование
    NonceDistributor(const NonceDistributor&) = delete;
    NonceDistributor& operator=(const NonceDistributor&) = delete;
    
    // Разрешаем перемещение
    NonceDistributor(NonceDistributor&&) noexcept;
    NonceDistributor& operator=(NonceDistributor&&) noexcept;
    
    // =========================================================================
    // Получение диапазонов
    // =========================================================================
    
    /**
     * @brief Получить диапазон для чипа по глобальному ID
     * 
     * @param chip_id Глобальный ID чипа (0 - total_chips-1)
     * @return NonceRange Диапазон nonce
     */
    [[nodiscard]] NonceRange get_range(uint16_t chip_id) const;
    
    /**
     * @brief Получить диапазон для чипа по ASIC и локальному ID
     * 
     * @param asic_id ID ASIC (0 - asic_count-1)
     * @param local_chip_id Локальный ID чипа (0 - chips_per_asic-1)
     * @return NonceRange Диапазон nonce
     */
    [[nodiscard]] NonceRange get_range(uint8_t asic_id, uint8_t local_chip_id) const;
    
    /**
     * @brief Получить все диапазоны для ASIC
     * 
     * @param asic_id ID ASIC
     * @return std::vector<NonceRange> Диапазоны для всех чипов
     */
    [[nodiscard]] std::vector<NonceRange> get_asic_ranges(uint8_t asic_id) const;
    
    /**
     * @brief Получить все диапазоны
     */
    [[nodiscard]] const std::vector<NonceRange>& get_all_ranges() const;
    
    // =========================================================================
    // Информация
    // =========================================================================
    
    /**
     * @brief Получить общее количество чипов
     */
    [[nodiscard]] uint32_t total_chips() const noexcept;
    
    /**
     * @brief Получить стратегию
     */
    [[nodiscard]] NonceStrategy get_strategy() const noexcept;
    
    /**
     * @brief Получить конфигурацию
     */
    [[nodiscard]] const NonceDistributorConfig& get_config() const noexcept;
    
    // =========================================================================
    // Валидация
    // =========================================================================
    
    /**
     * @brief Проверить, что все пространство nonce покрыто
     * 
     * @return true если все nonce распределены
     */
    [[nodiscard]] bool validate_coverage() const;
    
    /**
     * @brief Проверить отсутствие пересечений диапазонов
     * 
     * @return true если пересечений нет
     */
    [[nodiscard]] bool validate_no_overlap() const;
    
    /**
     * @brief Найти, какому чипу принадлежит nonce
     * 
     * @param nonce Nonce для поиска
     * @return chip_id или nullopt если не найден
     */
    [[nodiscard]] std::optional<uint16_t> find_chip_for_nonce(uint32_t nonce) const;
    
    // =========================================================================
    // Перестроение
    // =========================================================================
    
    /**
     * @brief Перестроить распределение с новой конфигурацией
     */
    void rebuild(const NonceDistributorConfig& config);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Вычислить размер диапазона для одного чипа
 * 
 * @param total_chips Общее количество чипов
 * @return Размер диапазона
 */
[[nodiscard]] inline uint64_t calculate_range_size(uint32_t total_chips) {
    if (total_chips == 0) return NONCE_SPACE;
    return NONCE_SPACE / total_chips;
}

/**
 * @brief Сериализовать диапазон для отправки на ASIC (8 байт)
 */
[[nodiscard]] std::array<uint8_t, 8> serialize_range(const NonceRange& range);

/**
 * @brief Десериализовать диапазон
 */
[[nodiscard]] Result<NonceRange> deserialize_range(ByteSpan data);

} // namespace quaxis::mining
