/**
 * @file terracoin_chain.cpp
 * @brief Реализация Terracoin chain
 * 
 * Terracoin - старый форк Bitcoin (с 2012 года).
 * Использует классический AuxPoW механизм.
 */

#include "terracoin_chain.hpp"

namespace quaxis::merged {

TerracoinChain::TerracoinChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string TerracoinChain::get_chain_name() const {
    return "terracoin";
}

std::string TerracoinChain::get_chain_ticker() const {
    return "TRC";
}

Hash256 TerracoinChain::get_chain_id() const {
    // Terracoin genesis block hash
    // Genesis: 0x00000000804bbc6a621a9dbb564f1bd37e4a3a4a7c1d5e7be0b1f6a0b8c5d7e9
    Hash256 id{};
    id[0] = 0xe9;
    id[1] = 0xd7;
    id[2] = 0xc5;
    id[3] = 0xb8;
    return id;
}

} // namespace quaxis::merged
