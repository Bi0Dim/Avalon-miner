/**
 * @file version_rolling.cpp
 * @brief Реализация менеджера Version Rolling (AsicBoost)
 * 
 * Version Rolling использует биты 13-28 поля version как дополнительный nonce.
 * Это даёт прирост производительности +15-20% за счёт увеличения пространства
 * перебора.
 */

#include "version_rolling.hpp"
#include "../core/byte_order.hpp"

#include <cstring>

namespace quaxis::mining {

// =============================================================================
// MiningJobV2 - Расширенное задание
// =============================================================================

std::array<uint8_t, 56> MiningJobV2::serialize() const noexcept {
    std::array<uint8_t, 56> result{};
    
    // [0-31]: midstate
    std::memcpy(result.data(), midstate.data(), 32);
    
    // [32-43]: header_tail
    std::memcpy(result.data() + 32, header_tail.data(), 12);
    
    // [44-47]: job_id (little-endian)
    result[44] = static_cast<uint8_t>(job_id & 0xFF);
    result[45] = static_cast<uint8_t>((job_id >> 8) & 0xFF);
    result[46] = static_cast<uint8_t>((job_id >> 16) & 0xFF);
    result[47] = static_cast<uint8_t>((job_id >> 24) & 0xFF);
    
    // [48-51]: version_base (little-endian)
    result[48] = static_cast<uint8_t>(version_base & 0xFF);
    result[49] = static_cast<uint8_t>((version_base >> 8) & 0xFF);
    result[50] = static_cast<uint8_t>((version_base >> 16) & 0xFF);
    result[51] = static_cast<uint8_t>((version_base >> 24) & 0xFF);
    
    // [52-53]: version_mask (little-endian)
    result[52] = static_cast<uint8_t>(version_mask & 0xFF);
    result[53] = static_cast<uint8_t>((version_mask >> 8) & 0xFF);
    
    // [54-55]: reserved
    result[54] = static_cast<uint8_t>(reserved & 0xFF);
    result[55] = static_cast<uint8_t>((reserved >> 8) & 0xFF);
    
    return result;
}

Result<MiningJobV2> MiningJobV2::deserialize(ByteSpan data) {
    if (data.size() < 56) {
        return Err<MiningJobV2>(
            ErrorCode::CryptoInvalidLength,
            "MiningJobV2: ожидается 56 байт"
        );
    }
    
    MiningJobV2 job;
    
    // [0-31]: midstate
    std::memcpy(job.midstate.data(), data.data(), 32);
    
    // [32-43]: header_tail
    std::memcpy(job.header_tail.data(), data.data() + 32, 12);
    
    // [44-47]: job_id (little-endian)
    job.job_id = static_cast<uint32_t>(data[44]) |
                 (static_cast<uint32_t>(data[45]) << 8) |
                 (static_cast<uint32_t>(data[46]) << 16) |
                 (static_cast<uint32_t>(data[47]) << 24);
    
    // [48-51]: version_base (little-endian)
    job.version_base = static_cast<uint32_t>(data[48]) |
                       (static_cast<uint32_t>(data[49]) << 8) |
                       (static_cast<uint32_t>(data[50]) << 16) |
                       (static_cast<uint32_t>(data[51]) << 24);
    
    // [52-53]: version_mask (little-endian)
    job.version_mask = static_cast<uint16_t>(
        static_cast<uint32_t>(data[52]) |
        (static_cast<uint32_t>(data[53]) << 8)
    );
    
    // [54-55]: reserved
    job.reserved = static_cast<uint16_t>(
        static_cast<uint32_t>(data[54]) |
        (static_cast<uint32_t>(data[55]) << 8)
    );
    
    return job;
}

// =============================================================================
// MiningShareV2 - Расширенный ответ
// =============================================================================

std::array<uint8_t, 12> MiningShareV2::serialize() const noexcept {
    std::array<uint8_t, 12> result{};
    
    // [0-3]: job_id (little-endian)
    result[0] = static_cast<uint8_t>(job_id & 0xFF);
    result[1] = static_cast<uint8_t>((job_id >> 8) & 0xFF);
    result[2] = static_cast<uint8_t>((job_id >> 16) & 0xFF);
    result[3] = static_cast<uint8_t>((job_id >> 24) & 0xFF);
    
    // [4-7]: nonce (little-endian)
    result[4] = static_cast<uint8_t>(nonce & 0xFF);
    result[5] = static_cast<uint8_t>((nonce >> 8) & 0xFF);
    result[6] = static_cast<uint8_t>((nonce >> 16) & 0xFF);
    result[7] = static_cast<uint8_t>((nonce >> 24) & 0xFF);
    
    // [8-11]: version (little-endian)
    result[8] = static_cast<uint8_t>(version & 0xFF);
    result[9] = static_cast<uint8_t>((version >> 8) & 0xFF);
    result[10] = static_cast<uint8_t>((version >> 16) & 0xFF);
    result[11] = static_cast<uint8_t>((version >> 24) & 0xFF);
    
    return result;
}

Result<MiningShareV2> MiningShareV2::deserialize(ByteSpan data) {
    if (data.size() < 12) {
        return Err<MiningShareV2>(
            ErrorCode::CryptoInvalidLength,
            "MiningShareV2: ожидается 12 байт"
        );
    }
    
    MiningShareV2 share;
    
    // [0-3]: job_id (little-endian)
    share.job_id = static_cast<uint32_t>(data[0]) |
                   (static_cast<uint32_t>(data[1]) << 8) |
                   (static_cast<uint32_t>(data[2]) << 16) |
                   (static_cast<uint32_t>(data[3]) << 24);
    
    // [4-7]: nonce (little-endian)
    share.nonce = static_cast<uint32_t>(data[4]) |
                  (static_cast<uint32_t>(data[5]) << 8) |
                  (static_cast<uint32_t>(data[6]) << 16) |
                  (static_cast<uint32_t>(data[7]) << 24);
    
    // [8-11]: version (little-endian)
    share.version = static_cast<uint32_t>(data[8]) |
                    (static_cast<uint32_t>(data[9]) << 8) |
                    (static_cast<uint32_t>(data[10]) << 16) |
                    (static_cast<uint32_t>(data[11]) << 24);
    
    return share;
}

// =============================================================================
// VersionRollingManager
// =============================================================================

VersionRollingManager::VersionRollingManager(const VersionRollingConfig& config)
    : config_(config)
{
}

uint32_t VersionRollingManager::apply_rolling(uint16_t rolling_value) const noexcept {
    // Сдвигаем rolling_value на позицию 13 и применяем маску
    uint32_t rolling_bits = (static_cast<uint32_t>(rolling_value) << 13) & config_.version_mask;
    
    // Очищаем rolling биты в базовой версии и применяем новые
    return (config_.version_base & ~config_.version_mask) | rolling_bits;
}

uint16_t VersionRollingManager::extract_rolling(uint32_t version) const noexcept {
    // Извлекаем rolling биты и сдвигаем на позицию 0
    return static_cast<uint16_t>((version & config_.version_mask) >> 13);
}

bool VersionRollingManager::validate_version(uint32_t version) const noexcept {
    // Проверяем, что non-rolling биты совпадают с базовой версией
    uint32_t non_rolling_bits = version & ~config_.version_mask;
    uint32_t expected_non_rolling = config_.version_base & ~config_.version_mask;
    
    return non_rolling_bits == expected_non_rolling;
}

uint16_t VersionRollingManager::next_rolling_value() noexcept {
    // Атомарный инкремент с wrap-around
    uint16_t current = rolling_counter_.fetch_add(1, std::memory_order_relaxed);
    
    // Обновляем статистику
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.versions_generated++;
    }
    
    return current & static_cast<uint16_t>(VERSION_ROLLING_MAX);
}

void VersionRollingManager::reset_rolling_counter() noexcept {
    rolling_counter_.store(0, std::memory_order_relaxed);
}

VersionRollingManager::Stats VersionRollingManager::get_stats() const noexcept {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// =============================================================================
// Вспомогательные функции
// =============================================================================

crypto::Sha256State compute_versioned_midstate(
    ByteSpan header_data,
    uint32_t version
) noexcept {
    if (header_data.size() < 64) {
        return crypto::Sha256State{};
    }
    
    // Копируем первые 64 байта заголовка
    std::array<uint8_t, 64> block{};
    std::memcpy(block.data(), header_data.data(), 64);
    
    // Заменяем версию (первые 4 байта, little-endian)
    block[0] = static_cast<uint8_t>(version & 0xFF);
    block[1] = static_cast<uint8_t>((version >> 8) & 0xFF);
    block[2] = static_cast<uint8_t>((version >> 16) & 0xFF);
    block[3] = static_cast<uint8_t>((version >> 24) & 0xFF);
    
    // Вычисляем midstate
    return crypto::compute_midstate(block.data());
}

MiningJobV2 create_job_v2(
    const crypto::Sha256State& midstate,
    const std::array<uint8_t, 12>& header_tail,
    uint32_t job_id,
    uint32_t version_base,
    uint32_t version_mask
) noexcept {
    MiningJobV2 job;
    
    // Конвертируем midstate в байты
    job.midstate = crypto::state_to_bytes(midstate);
    job.header_tail = header_tail;
    job.job_id = job_id;
    job.version_base = version_base;
    
    // Сохраняем только 16-битную маску (сдвинутую)
    job.version_mask = static_cast<uint16_t>((version_mask >> 13) & 0xFFFF);
    job.reserved = 0;
    
    return job;
}

} // namespace quaxis::mining
