/**
 * @file namecoin_chain.hpp
 * @brief Namecoin (NMC) chain implementation
 * 
 * Namecoin - первая auxiliary chain в истории Bitcoin.
 * Децентрализованная система DNS.
 * Ожидаемый доход: $8-10/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Namecoin chain
 */
class NamecoinChain : public BaseChain {
public:
    explicit NamecoinChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
