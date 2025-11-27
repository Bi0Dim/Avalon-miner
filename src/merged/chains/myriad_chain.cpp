/**
 * @file myriad_chain.cpp
 * @brief Реализация Myriad chain
 * 
 * Myriad - первая монета с несколькими алгоритмами хеширования.
 * Для merged mining используется только SHA256d алгоритм.
 */

#include "myriad_chain.hpp"

namespace quaxis::merged {

MyriadChain::MyriadChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string MyriadChain::get_chain_name() const {
    return "myriad";
}

std::string MyriadChain::get_chain_ticker() const {
    return "XMY";
}

Hash256 MyriadChain::get_chain_id() const {
    // Myriad genesis block hash (SHA256)
    // Mainnet genesis: 0x00000ffde4c020b5938441a0ea3d314bf619ead3acc966f714c5220e24b9e453
    Hash256 id{};
    id[0] = 0x53;
    id[1] = 0xe4;
    id[2] = 0xb9;
    id[3] = 0x24;
    return id;
}

} // namespace quaxis::merged
