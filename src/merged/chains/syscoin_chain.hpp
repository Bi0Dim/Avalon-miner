/**
 * @file syscoin_chain.hpp
 * @brief Syscoin (SYS) chain implementation
 * 
 * Syscoin - платформа для NEVM (Network-Enhanced Virtual Machine).
 * Ожидаемый доход: $8-12/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Syscoin chain
 */
class SyscoinChain : public BaseChain {
public:
    explicit SyscoinChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
