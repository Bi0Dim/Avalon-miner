/**
 * @file elastos_chain.cpp
 * @brief Реализация Elastos chain
 */

#include "elastos_chain.hpp"

namespace quaxis::merged {

ElastosChain::ElastosChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string ElastosChain::get_chain_name() const {
    return "elastos";
}

std::string ElastosChain::get_chain_ticker() const {
    return "ELA";
}

Hash256 ElastosChain::get_chain_id() const {
    // Elastos genesis block hash
    Hash256 id{};
    id[0] = 0x45;
    id[1] = 0x4c;
    id[2] = 0x41;
    id[3] = 0x00; // "ELA" marker
    return id;
}

std::string ElastosChain::get_create_aux_block_method() const {
    return "createauxblock";
}

std::string ElastosChain::get_submit_aux_block_method() const {
    return "submitauxblock";
}

} // namespace quaxis::merged
