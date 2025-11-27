/**
 * @file terracoin_chain.hpp
 * @brief Terracoin (TRC) chain implementation
 * 
 * Terracoin - один из старейших форков Bitcoin.
 * Использует классический AuxPoW.
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Terracoin chain
 * 
 * Старый форк Bitcoin с поддержкой merged mining.
 */
class TerracoinChain : public BaseChain {
public:
    explicit TerracoinChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
