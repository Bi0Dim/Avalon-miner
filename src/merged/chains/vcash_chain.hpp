/**
 * @file vcash_chain.hpp
 * @brief VCash (XVC) chain implementation
 * 
 * VCash - анонимная криптовалюта.
 * Ожидаемый доход: <$1/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация VCash chain
 */
class VCashChain : public BaseChain {
public:
    explicit VCashChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
