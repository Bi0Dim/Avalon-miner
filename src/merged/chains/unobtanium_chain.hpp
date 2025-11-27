/**
 * @file unobtanium_chain.hpp
 * @brief Unobtanium (UNO) chain implementation
 * 
 * Unobtanium - редкая криптовалюта с ограниченным supply (250,000 UNO max).
 * Использует классический AuxPoW.
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Unobtanium chain
 * 
 * Редкая монета с очень ограниченным максимальным предложением.
 */
class UnobtaniumChain : public BaseChain {
public:
    explicit UnobtaniumChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
