/**
 * @file myriad_chain.hpp
 * @brief Myriad (XMY) chain implementation
 * 
 * Myriad - multi-algorithm монета с 5 алгоритмами:
 * - SHA256d (поддерживает merged mining с Bitcoin)
 * - Scrypt
 * - Groestl
 * - Skein
 * - Yescrypt
 * 
 * Только SHA256 алгоритм совместим с merged mining.
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Myriad chain
 * 
 * Multi-algo монета, только SHA256 алгоритм поддерживает merged mining.
 */
class MyriadChain : public BaseChain {
public:
    explicit MyriadChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
