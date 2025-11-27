/**
 * @file unobtanium_chain.cpp
 * @brief Реализация Unobtanium chain
 * 
 * Unobtanium - одна из самых редких криптовалют.
 * Максимальное предложение: 250,000 UNO.
 */

#include "unobtanium_chain.hpp"

namespace quaxis::merged {

UnobtaniumChain::UnobtaniumChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string UnobtaniumChain::get_chain_name() const {
    return "unobtanium";
}

std::string UnobtaniumChain::get_chain_ticker() const {
    return "UNO";
}

Hash256 UnobtaniumChain::get_chain_id() const {
    // Unobtanium genesis block hash
    // Genesis: 0x000004c2fc5fffb810dccc197d603690099a68305232e552d96ccbe8e2c52b75
    Hash256 id{};
    id[0] = 0x75;
    id[1] = 0x2b;
    id[2] = 0xc5;
    id[3] = 0xe2;
    return id;
}

} // namespace quaxis::merged
