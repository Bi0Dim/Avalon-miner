/**
 * @file hathor_chain.cpp
 * @brief Реализация Hathor chain
 */

#include "hathor_chain.hpp"

namespace quaxis::merged {

HathorChain::HathorChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string HathorChain::get_chain_name() const {
    return "hathor";
}

std::string HathorChain::get_chain_ticker() const {
    return "HTR";
}

Hash256 HathorChain::get_chain_id() const {
    // Hathor genesis block hash
    Hash256 id{};
    id[0] = 0x48;
    id[1] = 0x54;
    id[2] = 0x52;
    id[3] = 0x00; // "HTR" marker
    return id;
}

std::string HathorChain::get_create_aux_block_method() const {
    // Hathor использует REST API, но мы адаптируем его через RPC клиент
    return "mining/block-template";
}

std::string HathorChain::get_submit_aux_block_method() const {
    return "mining/submit-job";
}

} // namespace quaxis::merged
