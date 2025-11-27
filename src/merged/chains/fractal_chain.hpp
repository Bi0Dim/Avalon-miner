/**
 * @file fractal_chain.hpp
 * @brief Fractal Bitcoin (FB) chain implementation
 * 
 * Fractal Bitcoin - одна из наиболее прибыльных auxiliary chains.
 * Ожидаемый доход: $25-41/месяц на 90 TH/s
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Fractal Bitcoin chain
 */
class FractalChain : public BaseChain {
public:
    explicit FractalChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
