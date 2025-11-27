/**
 * @file emercoin_chain.hpp
 * @brief Emercoin (EMC) chain implementation
 * 
 * Emercoin - платформа для децентрализованных DNS, SSL, SSH сервисов.
 * Использует классический AuxPoW.
 */

#pragma once

#include "base_chain.hpp"

namespace quaxis::merged {

/**
 * @brief Реализация Emercoin chain
 * 
 * Платформа с NVS (Name-Value Storage) на блокчейне.
 * Поддерживает EMCDNS, EMCSSL, EMCSSH сервисы.
 */
class EmercoinChain : public BaseChain {
public:
    explicit EmercoinChain(const ChainConfig& config);
    
protected:
    [[nodiscard]] std::string get_chain_name() const override;
    [[nodiscard]] std::string get_chain_ticker() const override;
    [[nodiscard]] Hash256 get_chain_id() const override;
};

} // namespace quaxis::merged
