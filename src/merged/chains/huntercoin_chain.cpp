/**
 * @file huntercoin_chain.cpp
 * @brief Реализация Huntercoin chain
 * 
 * Huntercoin - игровая криптовалюта с блокчейн-игрой.
 * Использует классический AuxPoW механизм.
 */

#include "huntercoin_chain.hpp"

namespace quaxis::merged {

HuntercoinChain::HuntercoinChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string HuntercoinChain::get_chain_name() const {
    return "huntercoin";
}

std::string HuntercoinChain::get_chain_ticker() const {
    return "HUC";
}

Hash256 HuntercoinChain::get_chain_id() const {
    // Huntercoin genesis block hash
    // Genesis: 0x00000000db7eb7a9e1a06cf995363dcdc4c28e8ae04827a961942dbe20875f06
    Hash256 id{};
    id[0] = 0x06;
    id[1] = 0x5f;
    id[2] = 0x87;
    id[3] = 0x20;
    return id;
}

} // namespace quaxis::merged
