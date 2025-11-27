/**
 * @file pow_validator.hpp
 * @brief Валидатор Proof-of-Work
 * 
 * Проверяет корректность proof-of-work для блоков.
 */

#pragma once

#include "../chain/chain_params.hpp"
#include "../primitives/block_header.hpp"

namespace quaxis::core::validation {

/**
 * @brief Валидатор Proof-of-Work
 * 
 * Проверяет:
 * - Корректность nBits
 * - hash(header) <= target
 * - Правила пересчёта сложности
 */
class PowValidator {
public:
    /**
     * @brief Создать валидатор для chain
     * 
     * @param params Параметры chain
     */
    explicit PowValidator(const ChainParams& params);
    
    /**
     * @brief Проверить proof-of-work заголовка
     * 
     * @param header Заголовок блока
     * @return true если PoW валиден
     */
    [[nodiscard]] bool validate_pow(const BlockHeader& header) const noexcept;
    
    /**
     * @brief Проверить, соответствует ли хеш target
     * 
     * @param hash Хеш блока
     * @param target_bits Compact target
     * @return true если hash <= target
     */
    [[nodiscard]] bool check_hash_target(
        const Hash256& hash,
        uint32_t target_bits
    ) const noexcept;
    
    /**
     * @brief Проверить валидность nBits
     * 
     * @param bits Compact target
     * @return true если bits в допустимых пределах
     */
    [[nodiscard]] bool validate_bits(uint32_t bits) const noexcept;
    
    /**
     * @brief Вычислить следующий target
     * 
     * @param last_bits Текущий compact target
     * @param actual_timespan Фактическое время между adjustment intervals
     * @return uint32_t Новый compact target
     */
    [[nodiscard]] uint32_t calculate_next_target(
        uint32_t last_bits,
        int64_t actual_timespan
    ) const noexcept;
    
    /**
     * @brief Получить ожидаемое время для пересчёта
     * 
     * @return int64_t Ожидаемое время в секундах
     */
    [[nodiscard]] int64_t get_expected_timespan() const noexcept;
    
private:
    const ChainParams& params_;
};

/**
 * @brief Быстрая проверка PoW без создания валидатора
 * 
 * @param header Заголовок блока
 * @return true если hash(header) <= target(bits)
 */
[[nodiscard]] bool quick_check_pow(const BlockHeader& header) noexcept;

} // namespace quaxis::core::validation
