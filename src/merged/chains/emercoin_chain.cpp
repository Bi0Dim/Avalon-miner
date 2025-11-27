/**
 * @file emercoin_chain.cpp
 * @brief Реализация Emercoin chain
 * 
 * Emercoin - блокчейн платформа для DNS, SSL, SSH на основе NVS.
 * Использует классический AuxPoW механизм.
 */

#include "emercoin_chain.hpp"

namespace quaxis::merged {

EmercoinChain::EmercoinChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string EmercoinChain::get_chain_name() const {
    return "emercoin";
}

std::string EmercoinChain::get_chain_ticker() const {
    return "EMC";
}

Hash256 EmercoinChain::get_chain_id() const {
    // Emercoin genesis block hash
    // Genesis: 0x00000000bcccd459d036a588d1008fce8da3754b205736f32ddfd35350e84c2d
    Hash256 id{};
    id[0] = 0x2d;
    id[1] = 0x4c;
    id[2] = 0xe8;
    id[3] = 0x50;
    return id;
}

} // namespace quaxis::merged
