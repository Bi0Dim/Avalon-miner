/**
 * @file vcash_chain.cpp
 * @brief Реализация VCash chain
 */

#include "vcash_chain.hpp"

namespace quaxis::merged {

VCashChain::VCashChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string VCashChain::get_chain_name() const {
    return "vcash";
}

std::string VCashChain::get_chain_ticker() const {
    return "XVC";
}

Hash256 VCashChain::get_chain_id() const {
    // VCash genesis block hash
    Hash256 id{};
    id[0] = 0x58;
    id[1] = 0x56;
    id[2] = 0x43;
    id[3] = 0x00; // "XVC" marker
    return id;
}

} // namespace quaxis::merged
