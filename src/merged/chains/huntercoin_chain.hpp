/**
 * @file huntercoin_chain.hpp
 * @brief Huntercoin (HUC) chain implementation
 * 
 * Huntercoin - игровая криптовалюта с уникальной механикой.
 * Использует классический AuxPoW как Namecoin.
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Huntercoin chain
 * 
 * Игровая монета с классическим AuxPoW.
 */
class HuntercoinChain : public BaseChain {
public:
    explicit HuntercoinChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
