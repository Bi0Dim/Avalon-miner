/**
 * @file namecoin_chain.cpp
 * @brief Реализация Namecoin chain
 */

#include "namecoin_chain.hpp"

namespace quaxis::merged {

NamecoinChain::NamecoinChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string NamecoinChain::get_chain_name() const {
    return "namecoin";
}

std::string NamecoinChain::get_chain_ticker() const {
    return "NMC";
}

Hash256 NamecoinChain::get_chain_id() const {
    // Namecoin genesis block hash
    // Real hash: 000000000062b72c5e2ceb45fbc8587e807c155b0da735e6483dfba2f0a9c770
    Hash256 id{};
    id[0] = 0x70;
    id[1] = 0xc7;
    id[2] = 0xa9;
    id[3] = 0xf0;
    return id;
}

} // namespace quaxis::merged
