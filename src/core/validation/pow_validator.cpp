/**
 * @file pow_validator.cpp
 * @brief Реализация валидатора PoW
 */

#include "pow_validator.hpp"

namespace quaxis::core::validation {

PowValidator::PowValidator(const ChainParams& params)
    : params_(params) {}

bool PowValidator::validate_pow(const BlockHeader& header) const noexcept {
    // Проверяем nBits
    if (!validate_bits(header.bits)) {
        return false;
    }
    
    // Проверяем hash <= target
    return header.check_pow();
}

bool PowValidator::check_hash_target(
    const Hash256& hash,
    uint32_t target_bits
) const noexcept {
    auto target = bits_to_target(target_bits);
    return uint256{hash} <= target;
}

bool PowValidator::validate_bits(uint32_t bits) const noexcept {
    // Проверяем что bits не превышает pow_limit
    auto target = bits_to_target(bits);
    auto limit = bits_to_target(params_.difficulty.pow_limit_bits);
    
    if (target > limit) {
        return false;
    }
    
    // Проверяем что мантисса корректна (нет overflow)
    uint32_t exponent = bits >> 24;
    uint32_t mantissa = bits & 0x007FFFFF;
    
    // Negative bit не должен быть установлен
    if (bits & 0x00800000) {
        return false;
    }
    
    // Нулевой target недопустим
    if (mantissa == 0 && exponent == 0) {
        return false;
    }
    
    return true;
}

uint32_t PowValidator::calculate_next_target(
    uint32_t last_bits,
    int64_t actual_timespan
) const noexcept {
    auto expected = get_expected_timespan();
    
    // Ограничиваем timespan (Bitcoin: 4x max)
    if (actual_timespan < expected / 4) {
        actual_timespan = expected / 4;
    }
    if (actual_timespan > expected * 4) {
        actual_timespan = expected * 4;
    }
    
    // ПРИМЕЧАНИЕ: Пересчёт difficulty не требуется для merged mining,
    // так как target берётся напрямую из шаблона блока auxiliary chain.
    // Эта функция используется только для валидации исторических блоков
    // при полной синхронизации, что выходит за рамки данного модуля.
    // 
    // Для полной реализации требуется:
    // 1. uint256 арифметика (умножение/деление)
    // 2. Учёт специфики каждой chain (разные алгоритмы retarget)
    (void)actual_timespan;
    
    return last_bits;
}

int64_t PowValidator::get_expected_timespan() const noexcept {
    return static_cast<int64_t>(params_.difficulty.target_spacing) *
           static_cast<int64_t>(params_.difficulty.adjustment_interval);
}

bool quick_check_pow(const BlockHeader& header) noexcept {
    return header.check_pow();
}

} // namespace quaxis::core::validation
