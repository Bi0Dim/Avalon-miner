/**
 * @file auxpow_validator.cpp
 * @brief Реализация валидатора AuxPoW
 */

#include "auxpow_validator.hpp"

namespace quaxis::core::validation {

AuxPowValidator::AuxPowValidator(const ChainParams& params)
    : params_(params) {}

AuxPowValidationResult AuxPowValidator::validate(
    const AuxPow& auxpow,
    const Hash256& aux_hash,
    uint32_t height
) const {
    // Проверяем активацию AuxPoW
    if (!params_.auxpow.is_active(height)) {
        return AuxPowValidationResult::failure("AuxPoW not active at this height");
    }
    
    // Проверяем coinbase branch
    auto coinbase_result = validate_coinbase_branch(auxpow);
    if (!coinbase_result) {
        return coinbase_result;
    }
    
    // Проверяем aux branch
    auto aux_result = validate_aux_branch(auxpow, aux_hash);
    if (!aux_result) {
        return aux_result;
    }
    
    // Проверяем chain ID (если требуется)
    if (params_.auxpow.chain_id != 0) {
        auto chain_id_result = validate_chain_id(auxpow);
        if (!chain_id_result) {
            return chain_id_result;
        }
    }
    
    // Проверяем PoW родительского блока
    if (!auxpow.verify_pow()) {
        return AuxPowValidationResult::failure("Parent block PoW invalid");
    }
    
    return AuxPowValidationResult::success();
}

bool AuxPowValidator::validate_pow(
    const AuxPow& auxpow,
    uint32_t target_bits
) const noexcept {
    return auxpow.meets_target(target_bits);
}

AuxPowValidationResult AuxPowValidator::validate_coinbase_branch(
    const AuxPow& auxpow
) const {
    // Проверяем что coinbase branch ведёт к merkle root
    auto computed_root = auxpow.coinbase_branch.compute_root(auxpow.coinbase_hash);
    
    if (computed_root != auxpow.parent_header.merkle_root) {
        return AuxPowValidationResult::failure(
            "Coinbase branch does not lead to merkle root"
        );
    }
    
    return AuxPowValidationResult::success();
}

AuxPowValidationResult AuxPowValidator::validate_aux_branch(
    const AuxPow& auxpow,
    const Hash256& aux_hash
) const {
    // Находим commitment в coinbase
    auto commitment = AuxPowCommitment::find_in_coinbase(auxpow.coinbase_tx);
    if (!commitment) {
        return AuxPowValidationResult::failure(
            "AuxPoW commitment not found in coinbase"
        );
    }
    
    // Проверяем aux branch
    auto computed_aux_root = auxpow.aux_branch.compute_root(aux_hash);
    if (computed_aux_root != commitment->aux_merkle_root) {
        return AuxPowValidationResult::failure(
            "Aux branch does not lead to aux merkle root"
        );
    }
    
    return AuxPowValidationResult::success();
}

AuxPowValidationResult AuxPowValidator::validate_chain_id(
    const AuxPow& auxpow
) const {
    uint32_t parent_chain_id = auxpow.get_chain_id();
    
    // Chain ID в parent header должен соответствовать нашей chain
    // или быть 0 (Bitcoin без AuxPoW флага)
    if (parent_chain_id != 0 && parent_chain_id != params_.auxpow.chain_id) {
        return AuxPowValidationResult::failure(
            "Parent block chain ID mismatch"
        );
    }
    
    return AuxPowValidationResult::success();
}

uint32_t AuxPowValidator::get_chain_id() const noexcept {
    return params_.auxpow.chain_id;
}

} // namespace quaxis::core::validation
