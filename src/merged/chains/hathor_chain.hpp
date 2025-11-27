/**
 * @file hathor_chain.hpp
 * @brief Hathor (HTR) chain implementation
 * 
 * Hathor - платформа с уникальной DAG-архитектурой.
 * Ожидаемый доход: $5-15/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Hathor chain
 */
class HathorChain : public BaseChain {
public:
    explicit HathorChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
    
    // Hathor использует HTTP API вместо JSON-RPC
    [[nodiscard]] std::string get_create_aux_block_method() const override;
    [[nodiscard]] std::string get_submit_aux_block_method() const override;
};

} // namespace quaxis::merged
