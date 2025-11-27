/**
 * @file fractal_chain.cpp
 * @brief Реализация Fractal Bitcoin chain
 */

#include "fractal_chain.hpp"

namespace quaxis::merged {

FractalChain::FractalChain(const ChainConfig& config)
    : BaseChain(config) {
    init_chain_info();
}

std::string FractalChain::get_chain_name() const {
    return "fractal";
}

std::string FractalChain::get_chain_ticker() const {
    return "FB";
}

Hash256 FractalChain::get_chain_id() const {
    // Fractal Bitcoin genesis block hash
    // Используем примерный chain ID, реальный будет получен от ноды
    Hash256 id{};
    // Chain ID: первые 4 байта используются для slot calculation
    id[0] = 0x01;
    id[1] = 0xfb;
    id[2] = 0x00;
    id[3] = 0x00;
    return id;
}

} // namespace quaxis::merged
