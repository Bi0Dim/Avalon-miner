/**
 * @file stacks_chain.cpp
 * @brief Реализация Stacks chain
 * 
 * Stacks использует Proof of Transfer (PoX) - особый механизм консенсуса,
 * который требует дополнительного исследования для полной интеграции.
 */

#include "stacks_chain.hpp"

namespace quaxis::merged {

StacksChain::StacksChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string StacksChain::get_chain_name() const {
    return "stacks";
}

std::string StacksChain::get_chain_ticker() const {
    return "STX";
}

Hash256 StacksChain::get_chain_id() const {
    // Stacks mainnet chain ID
    // Stacks использует числовой chain_id: 1 для mainnet
    // Представляем как хеш для совместимости с AuxPoW структурой
    Hash256 id{};
    id[0] = 0x53; // 'S'
    id[1] = 0x54; // 'T'
    id[2] = 0x58; // 'X'
    id[3] = 0x01; // mainnet identifier
    return id;
}

std::string StacksChain::get_create_aux_block_method() const {
    // Stacks использует HTTP API вместо JSON-RPC
    // Требуется адаптация для PoX механизма
    return "v2/mining/block";
}

std::string StacksChain::get_submit_aux_block_method() const {
    // Метод для отправки найденного блока
    return "v2/mining/submit";
}

} // namespace quaxis::merged
