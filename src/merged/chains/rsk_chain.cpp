/**
 * @file rsk_chain.cpp
 * @brief Реализация RSK chain
 */

#include "rsk_chain.hpp"

namespace quaxis::merged {

RSKChain::RSKChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string RSKChain::get_chain_name() const {
    return "rsk";
}

std::string RSKChain::get_chain_ticker() const {
    return "RBTC";
}

Hash256 RSKChain::get_chain_id() const {
    // RSK mainnet chain ID
    Hash256 id{};
    id[0] = 0x00;
    id[1] = 0x00;
    id[2] = 0x00;
    id[3] = 0x1e; // 30 = RSK mainnet
    return id;
}

std::string RSKChain::get_create_aux_block_method() const {
    return "mnr_getWork";
}

std::string RSKChain::get_submit_aux_block_method() const {
    return "mnr_submitBitcoinBlockPartialMerkle";
}

} // namespace quaxis::merged
