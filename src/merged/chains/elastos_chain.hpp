/**
 * @file elastos_chain.hpp
 * @brief Elastos (ELA) chain implementation
 * 
 * Elastos - платформа для децентрализованного интернета.
 * Ожидаемый доход: $3-5/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Elastos chain
 */
class ElastosChain : public BaseChain {
public:
    explicit ElastosChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
    
    // Elastos использует специфичные RPC методы
    [[nodiscard]] std::string get_create_aux_block_method() const override;
    [[nodiscard]] std::string get_submit_aux_block_method() const override;
};

} // namespace quaxis::merged
