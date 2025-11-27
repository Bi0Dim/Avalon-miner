/**
 * @file auxpow_validator.hpp
 * @brief Валидатор Auxiliary Proof-of-Work
 * 
 * Проверяет корректность AuxPoW для merged mining.
 */

#pragma once

#include "../chain/chain_params.hpp"
#include "../primitives/auxpow.hpp"

#include <optional>

namespace quaxis::core::validation {

/**
 * @brief Результат валидации AuxPoW
 */
struct AuxPowValidationResult {
    bool valid{false};
    std::string error_message;
    
    [[nodiscard]] explicit operator bool() const noexcept { return valid; }
    
    [[nodiscard]] static AuxPowValidationResult success() {
        return {true, ""};
    }
    
    [[nodiscard]] static AuxPowValidationResult failure(std::string msg) {
        return {false, std::move(msg)};
    }
};

/**
 * @brief Валидатор AuxPoW
 * 
 * Проверяет:
 * - Корректность coinbase branch
 * - Наличие и корректность AuxPoW commitment
 * - Корректность aux branch
 * - PoW родительского блока
 */
class AuxPowValidator {
public:
    /**
     * @brief Создать валидатор для chain
     * 
     * @param params Параметры chain
     */
    explicit AuxPowValidator(const ChainParams& params);
    
    /**
     * @brief Полная валидация AuxPoW
     * 
     * @param auxpow AuxPoW для проверки
     * @param aux_hash Хеш блока auxiliary chain
     * @param height Высота блока (для проверки активации)
     * @return AuxPowValidationResult Результат валидации
     */
    [[nodiscard]] AuxPowValidationResult validate(
        const AuxPow& auxpow,
        const Hash256& aux_hash,
        uint32_t height
    ) const;
    
    /**
     * @brief Проверить только PoW (быстрая проверка)
     * 
     * @param auxpow AuxPoW
     * @param target_bits Target для проверки
     * @return true если hash(parent) <= target
     */
    [[nodiscard]] bool validate_pow(
        const AuxPow& auxpow,
        uint32_t target_bits
    ) const noexcept;
    
    /**
     * @brief Проверить coinbase branch
     * 
     * @param auxpow AuxPoW
     * @return AuxPowValidationResult Результат
     */
    [[nodiscard]] AuxPowValidationResult validate_coinbase_branch(
        const AuxPow& auxpow
    ) const;
    
    /**
     * @brief Проверить aux branch и commitment
     * 
     * @param auxpow AuxPoW
     * @param aux_hash Хеш aux блока
     * @return AuxPowValidationResult Результат
     */
    [[nodiscard]] AuxPowValidationResult validate_aux_branch(
        const AuxPow& auxpow,
        const Hash256& aux_hash
    ) const;
    
    /**
     * @brief Проверить chain ID в parent header
     * 
     * @param auxpow AuxPoW
     * @return AuxPowValidationResult Результат
     */
    [[nodiscard]] AuxPowValidationResult validate_chain_id(
        const AuxPow& auxpow
    ) const;
    
    /**
     * @brief Получить chain ID
     */
    [[nodiscard]] uint32_t get_chain_id() const noexcept;
    
private:
    const ChainParams& params_;
};

} // namespace quaxis::core::validation
