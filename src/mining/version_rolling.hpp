/**
 * @file version_rolling.hpp
 * @brief Менеджер Version Rolling (AsicBoost) — оптимизация +15-20%
 * 
 * Version Rolling (также известный как AsicBoost) использует биты 13-28
 * поля version заголовка блока как дополнительное пространство nonce.
 * 
 * Это позволяет увеличить пространство перебора nonce с 2^32 до 2^48,
 * что особенно полезно для высокопроизводительных ASIC.
 * 
 * Маска по умолчанию: 0x1FFFE000 (биты 13-28)
 * - Биты 0-12: зарезервированы для BIP9 version bits
 * - Биты 13-28: используются для version rolling (16 бит)
 * - Биты 29-31: зарезервированы
 * 
 * @note Требует поддержки со стороны пула/ноды (BIP 310)
 */

#pragma once

#include "../core/types.hpp"
#include "../core/constants.hpp"
#include "../crypto/sha256.hpp"

#include <cstdint>
#include <optional>
#include <atomic>
#include <mutex>

namespace quaxis::mining {

// =============================================================================
// Константы Version Rolling
// =============================================================================

/// @brief Маска для version rolling битов (биты 13-28)
inline constexpr uint32_t VERSION_ROLLING_MASK_DEFAULT = 0x1FFFE000;

/// @brief Минимальная версия блока (BIP9)
inline constexpr uint32_t VERSION_BASE = 0x20000000;

/// @brief Количество rolling битов
inline constexpr uint32_t VERSION_ROLLING_BITS = 16;

/// @brief Максимальное значение version rolling
inline constexpr uint32_t VERSION_ROLLING_MAX = (1u << VERSION_ROLLING_BITS) - 1;

// =============================================================================
// Структуры данных для Version Rolling
// =============================================================================

/**
 * @brief Конфигурация Version Rolling
 */
struct VersionRollingConfig {
    /// @brief Включить version rolling
    bool enabled = true;
    
    /// @brief Маска для rolling битов
    uint32_t version_mask = VERSION_ROLLING_MASK_DEFAULT;
    
    /// @brief Базовая версия блока
    uint32_t version_base = VERSION_BASE;
};

/**
 * @brief Расширенное задание для ASIC с поддержкой version rolling (56 байт)
 * 
 * Формат:
 * - [0-31]: midstate в байтовом представлении
 * - [32-43]: header_tail (12 байт)
 * - [44-47]: job_id (little-endian)
 * - [48-51]: version_base (little-endian) — NEW
 * - [52-53]: version_mask (16 бит, little-endian) — NEW
 * - [54-55]: reserved
 */
struct MiningJobV2 {
    /// @brief SHA256 midstate
    crypto::Sha256Midstate midstate{};
    
    /// @brief Хвост заголовка (последние 12 байт: последние 4 байта merkle + time + bits)
    std::array<uint8_t, 12> header_tail{};
    
    /// @brief ID задания
    uint32_t job_id = 0;
    
    /// @brief Базовая версия блока
    uint32_t version_base = VERSION_BASE;
    
    /// @brief Маска rolling битов (16 бит достаточно)
    uint16_t version_mask = static_cast<uint16_t>(VERSION_ROLLING_MASK_DEFAULT >> 13);
    
    /// @brief Зарезервировано
    uint16_t reserved = 0;
    
    /**
     * @brief Сериализовать задание в 56-байтный формат
     */
    [[nodiscard]] std::array<uint8_t, 56> serialize() const noexcept;
    
    /**
     * @brief Десериализовать задание из 56-байтного формата
     */
    [[nodiscard]] static Result<MiningJobV2> deserialize(ByteSpan data);
};

/**
 * @brief Расширенный share с версией (12 байт)
 * 
 * Формат:
 * - [0-3]: job_id (little-endian)
 * - [4-7]: nonce (little-endian)
 * - [8-11]: version (little-endian) — найденная версия
 */
struct MiningShareV2 {
    /// @brief ID задания
    uint32_t job_id = 0;
    
    /// @brief Найденный nonce
    uint32_t nonce = 0;
    
    /// @brief Найденная версия с rolling битами
    uint32_t version = VERSION_BASE;
    
    /**
     * @brief Сериализовать share в 12-байтный формат
     */
    [[nodiscard]] std::array<uint8_t, 12> serialize() const noexcept;
    
    /**
     * @brief Десериализовать share из 12-байтного формата
     */
    [[nodiscard]] static Result<MiningShareV2> deserialize(ByteSpan data);
};

// =============================================================================
// Менеджер Version Rolling
// =============================================================================

/**
 * @brief Менеджер version rolling
 * 
 * Управляет генерацией версий с rolling битами для заданий майнинга.
 */
class VersionRollingManager {
public:
    /**
     * @brief Создать менеджер с конфигурацией
     */
    explicit VersionRollingManager(const VersionRollingConfig& config);
    
    ~VersionRollingManager() = default;
    
    // Запрещаем копирование
    VersionRollingManager(const VersionRollingManager&) = delete;
    VersionRollingManager& operator=(const VersionRollingManager&) = delete;
    
    // Разрешаем перемещение
    VersionRollingManager(VersionRollingManager&&) noexcept = default;
    VersionRollingManager& operator=(VersionRollingManager&&) noexcept = default;
    
    /**
     * @brief Проверить, включён ли version rolling
     */
    [[nodiscard]] bool is_enabled() const noexcept { return config_.enabled; }
    
    /**
     * @brief Получить маску version rolling
     */
    [[nodiscard]] uint32_t get_mask() const noexcept { return config_.version_mask; }
    
    /**
     * @brief Получить базовую версию
     */
    [[nodiscard]] uint32_t get_base_version() const noexcept { return config_.version_base; }
    
    /**
     * @brief Применить rolling bits к версии
     * 
     * @param rolling_value Значение rolling (0 - VERSION_ROLLING_MAX)
     * @return uint32_t Версия с применёнными rolling битами
     */
    [[nodiscard]] uint32_t apply_rolling(uint16_t rolling_value) const noexcept;
    
    /**
     * @brief Извлечь rolling bits из версии
     * 
     * @param version Полная версия блока
     * @return uint16_t Значение rolling bits
     */
    [[nodiscard]] uint16_t extract_rolling(uint32_t version) const noexcept;
    
    /**
     * @brief Проверить валидность версии
     * 
     * @param version Версия для проверки
     * @return true если версия валидна
     */
    [[nodiscard]] bool validate_version(uint32_t version) const noexcept;
    
    /**
     * @brief Получить следующее значение rolling для перебора
     * 
     * Атомарно инкрементирует счётчик и возвращает новое значение.
     * 
     * @return uint16_t Следующее значение rolling
     */
    [[nodiscard]] uint16_t next_rolling_value() noexcept;
    
    /**
     * @brief Сбросить счётчик rolling
     */
    void reset_rolling_counter() noexcept;
    
    /**
     * @brief Получить статистику
     */
    struct Stats {
        uint64_t versions_generated = 0;  ///< Количество сгенерированных версий
        uint64_t versions_validated = 0;  ///< Количество проверенных версий
        uint64_t invalid_versions = 0;    ///< Количество невалидных версий
    };
    
    [[nodiscard]] Stats get_stats() const noexcept;
    
private:
    VersionRollingConfig config_;
    
    /// @brief Атомарный счётчик для version rolling
    std::atomic<uint16_t> rolling_counter_{0};
    
    /// @brief Статистика
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Вычислить midstate с учётом версии
 * 
 * Пересчитывает midstate для заголовка с указанной версией.
 * 
 * @param header_data Данные заголовка (80 байт)
 * @param version Новая версия блока
 * @return crypto::Sha256State Новый midstate
 */
[[nodiscard]] crypto::Sha256State compute_versioned_midstate(
    ByteSpan header_data,
    uint32_t version
) noexcept;

/**
 * @brief Создать расширенное задание с version rolling
 * 
 * @param midstate Предвычисленный midstate
 * @param header_tail Хвост заголовка (12 байт)
 * @param job_id ID задания
 * @param version_base Базовая версия
 * @param version_mask Маска rolling
 * @return MiningJobV2 Расширенное задание
 */
[[nodiscard]] MiningJobV2 create_job_v2(
    const crypto::Sha256State& midstate,
    const std::array<uint8_t, 12>& header_tail,
    uint32_t job_id,
    uint32_t version_base,
    uint32_t version_mask
) noexcept;

} // namespace quaxis::mining
