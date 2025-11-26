/**
 * @file job.hpp
 * @brief Структура задания для майнинга
 * 
 * Задание (job) - это минимальный набор данных, передаваемых на ASIC
 * для выполнения Proof of Work.
 * 
 * Формат задания (48 байт):
 * - midstate[32]: SHA256 состояние после первых 64 байт заголовка
 * - header_tail[12]: timestamp + bits + nonce (шаблон для изменения)
 * - job_id[4]: идентификатор задания
 * 
 * ASIC получает задание и:
 * 1. Перебирает nonce от 0 до 2^32-1
 * 2. Для каждого nonce вычисляет SHA256(SHA256(midstate || tail))
 * 3. Сравнивает хеш с target
 * 4. При нахождении валидного nonce отправляет share (job_id + nonce)
 */

#pragma once

#include "../core/types.hpp"
#include "../core/constants.hpp"
#include "../crypto/sha256.hpp"

#include <array>
#include <cstdint>
#include <chrono>

namespace quaxis::mining {

// =============================================================================
// Структура задания
// =============================================================================

/**
 * @brief Задание для ASIC майнера
 * 
 * Компактное представление данных для вычисления Proof of Work.
 * Размер: 48 байт.
 */
struct Job {
    /// @brief Уникальный идентификатор задания
    uint32_t job_id = 0;
    
    /// @brief SHA256 midstate (первые 64 байта заголовка)
    crypto::Sha256State midstate{};
    
    /// @brief Timestamp блока
    uint32_t timestamp = 0;
    
    /// @brief Compact target (bits)
    uint32_t bits = 0;
    
    /// @brief Начальный nonce (обычно 0)
    uint32_t nonce = 0;
    
    /// @brief Высота блока
    uint32_t height = 0;
    
    /// @brief Target в 256-битном формате
    Hash256 target{};
    
    /// @brief Это speculative (spy mining) задание?
    bool is_speculative = false;
    
    /// @brief Время создания задания
    std::chrono::steady_clock::time_point created_at;
    
    /**
     * @brief Сериализовать задание в 48-байтный формат для ASIC
     * 
     * Формат:
     * - [0-31]: midstate в байтовом представлении
     * - [32-35]: timestamp (little-endian)
     * - [36-39]: bits (little-endian)
     * - [40-43]: nonce (little-endian)
     * - [44-47]: job_id (little-endian)
     * 
     * @return std::array<uint8_t, 48> Сериализованное задание
     */
    [[nodiscard]] std::array<uint8_t, constants::JOB_MESSAGE_SIZE> serialize() const noexcept;
    
    /**
     * @brief Десериализовать задание из 48-байтного формата
     * 
     * @param data 48 байт сериализованного задания
     * @return Result<Job> Задание или ошибка
     */
    [[nodiscard]] static Result<Job> deserialize(ByteSpan data);
    
    /**
     * @brief Проверить, устарело ли задание
     * 
     * Задание считается устаревшим если:
     * - Прошло более 60 секунд с момента создания
     * - Появился новый блок (проверяется снаружи)
     * 
     * @param max_age Максимальный возраст в секундах
     * @return true если задание устарело
     */
    [[nodiscard]] bool is_stale(uint32_t max_age = 60) const noexcept;
};

// =============================================================================
// Структура share (ответ от ASIC)
// =============================================================================

/**
 * @brief Ответ от ASIC (найденный nonce)
 * 
 * Размер: 8 байт
 */
struct Share {
    /// @brief ID задания
    uint32_t job_id = 0;
    
    /// @brief Найденный nonce
    uint32_t nonce = 0;
    
    /**
     * @brief Сериализовать share в 8-байтный формат
     * 
     * @return std::array<uint8_t, 8> Сериализованный share
     */
    [[nodiscard]] std::array<uint8_t, constants::SHARE_MESSAGE_SIZE> serialize() const noexcept;
    
    /**
     * @brief Десериализовать share из 8-байтного формата
     * 
     * @param data 8 байт данных
     * @return Result<Share> Share или ошибка
     */
    [[nodiscard]] static Result<Share> deserialize(ByteSpan data);
};

// =============================================================================
// Результат валидации share
// =============================================================================

/**
 * @brief Результат проверки share
 */
enum class ShareResult {
    Valid,           ///< Share валиден, блок найден!
    ValidPartial,    ///< Share валиден как partial (для статистики)
    InvalidNonce,    ///< Неверный nonce
    InvalidJobId,    ///< Неизвестный job_id
    StaleJob,        ///< Задание устарело
    TargetNotMet,    ///< Хеш не соответствует target
    DuplicateShare   ///< Дубликат share
};

/**
 * @brief Преобразовать результат в строку
 */
[[nodiscard]] constexpr std::string_view to_string(ShareResult result) noexcept {
    switch (result) {
        case ShareResult::Valid: return "valid_block";
        case ShareResult::ValidPartial: return "valid_partial";
        case ShareResult::InvalidNonce: return "invalid_nonce";
        case ShareResult::InvalidJobId: return "invalid_job_id";
        case ShareResult::StaleJob: return "stale_job";
        case ShareResult::TargetNotMet: return "target_not_met";
        case ShareResult::DuplicateShare: return "duplicate_share";
        default: return "unknown";
    }
}

} // namespace quaxis::mining
