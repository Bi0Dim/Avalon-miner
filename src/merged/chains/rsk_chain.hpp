/**
 * @file rsk_chain.hpp
 * @brief RSK/Rootstock (RBTC) chain implementation
 * 
 * RSK - платформа smart contracts на базе Bitcoin.
 * Ожидаемый доход: $10-20/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация RSK chain
 */
class RSKChain : public BaseChain {
public:
    explicit RSKChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
    
    // RSK использует специфичный RPC
    [[nodiscard]] std::string get_create_aux_block_method() const override;
    [[nodiscard]] std::string get_submit_aux_block_method() const override;
};

} // namespace quaxis::merged
