/**
 * @file syscoin_chain.cpp
 * @brief Реализация Syscoin chain
 */

#include "syscoin_chain.hpp"

namespace quaxis::merged {

SyscoinChain::SyscoinChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string SyscoinChain::get_chain_name() const {
    return "syscoin";
}

std::string SyscoinChain::get_chain_ticker() const {
    return "SYS";
}

Hash256 SyscoinChain::get_chain_id() const {
    // Syscoin genesis block hash (first 4 bytes for slot calc)
    Hash256 id{};
    id[0] = 0x04;
    id[1] = 0x53;
    id[2] = 0x59;
    id[3] = 0x53; // "SYS" marker
    return id;
}

} // namespace quaxis::merged
