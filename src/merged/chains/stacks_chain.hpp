/**
 * @file stacks_chain.hpp
 * @brief Stacks (STX) chain implementation
 * 
 * Stacks - платформа смарт-контрактов и DeFi на Bitcoin.
 * Использует Proof of Transfer (PoX) — особый механизм,
 * который требует исследования совместимости с AuxPoW.
 * 
 * EXPERIMENTAL: Помечен как экспериментальный, по умолчанию выключен.
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Stacks chain
 * 
 * Stacks использует Proof of Transfer (PoX) вместо классического AuxPoW.
 * Это экспериментальная интеграция, требующая дополнительного тестирования.
 */
class StacksChain : public BaseChain {
public:
    explicit StacksChain(const ChainConfig& config);
    
    /**
     * @brief Проверить, поддерживает ли chain классический AuxPoW
     * @return false - Stacks использует PoX
     * @note Stacks-специфичный метод, не является частью IChain интерфейса
     */
    [[nodiscard]] bool supports_classic_auxpow() const noexcept { return false; }
    
    /**
     * @brief Проверить, поддерживает ли chain Proof of Transfer
     * @return true - Stacks использует PoX
     * @note Stacks-специфичный метод, не является частью IChain интерфейса
     */
    [[nodiscard]] bool supports_pox() const noexcept { return true; }
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
    
    // Stacks использует особый RPC API
    [[nodiscard]] std::string get_create_aux_block_method() const override;
    [[nodiscard]] std::string get_submit_aux_block_method() const override;
};

} // namespace quaxis::merged
