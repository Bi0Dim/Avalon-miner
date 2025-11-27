/**
 * @file chain_registry.cpp
 * @brief Реализация реестра блокчейнов
 */

#include "chain_registry.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace quaxis::core {

ChainRegistry& ChainRegistry::instance() {
    static ChainRegistry instance;
    return instance;
}

ChainRegistry::ChainRegistry() {
    init_builtin_chains();
}

void ChainRegistry::init_builtin_chains() {
    // Bitcoin (родительская chain)
    {
        ChainParams params;
        params.name = "bitcoin";
        params.ticker = "BTC";
        params.genesis_hash = {};
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 0;
        params.auxpow.magic_bytes = {0xfa, 0xbe, 0x6d, 0x6d};
        params.auxpow.start_height = 0;
        params.auxpow.version_flag = 0x20000000;
        params.difficulty.target_spacing = 600;
        params.difficulty.adjustment_interval = 2016;
        params.difficulty.pow_limit_bits = 0x1d00ffff;
        params.rewards.initial_reward = 5000000000;
        params.rewards.halving_interval = 210000;
        params.rewards.miner_share = 1.0;
        params.rewards.coinbase_maturity = 100;
        params.mainnet.magic = {0xf9, 0xbe, 0xb4, 0xd9};
        params.mainnet.default_port = 8333;
        params.mainnet.rpc_port = 8332;
        params.mainnet.dns_seeds = {"seed.bitcoin.sipa.be", "dnsseed.bluematt.me"};
        register_chain(std::move(params));
    }
    
    // Namecoin (chain_id = 1)
    {
        ChainParams params;
        params.name = "namecoin";
        params.ticker = "NMC";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 1;
        params.auxpow.start_height = 19200;
        params.auxpow.version_flag = 0x00620102;
        params.difficulty.target_spacing = 600;
        params.difficulty.adjustment_interval = 2016;
        params.difficulty.pow_limit_bits = 0x1d00ffff;
        params.rewards.initial_reward = 5000000000;
        params.rewards.halving_interval = 210000;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0xf9, 0xbe, 0xb4, 0xfe};
        params.mainnet.default_port = 8334;
        params.mainnet.rpc_port = 8336;
        params.mainnet.dns_seeds = {"seed.namecoin.org"};
        register_chain(std::move(params));
    }
    
    // Syscoin (chain_id = 57)
    {
        ChainParams params;
        params.name = "syscoin";
        params.ticker = "SYS";
        params.consensus_type = ConsensusType::AUXPOW_CHAINLOCK;
        params.auxpow.chain_id = 57;
        params.auxpow.start_height = 1;
        params.difficulty.target_spacing = 150;
        params.difficulty.adjustment_interval = 1;
        params.difficulty.pow_limit_bits = 0x1e0fffff;
        params.rewards.initial_reward = 3500000000;
        params.rewards.halving_interval = 525600;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0xce, 0xe2, 0xca, 0xff};
        params.mainnet.default_port = 8369;
        params.mainnet.rpc_port = 8370;
        register_chain(std::move(params));
    }
    
    // Elastos (chain_id = custom)
    {
        ChainParams params;
        params.name = "elastos";
        params.ticker = "ELA";
        params.consensus_type = ConsensusType::AUXPOW_HYBRID_BPOS;
        params.auxpow.chain_id = 0;
        params.auxpow.start_height = 0;
        params.difficulty.target_spacing = 120;
        params.difficulty.adjustment_interval = 720;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 150000000;
        params.rewards.halving_interval = 1051200;
        params.rewards.miner_share = 0.35;  // 35% для майнеров
        params.mainnet.magic = {0xd4, 0xae, 0xe6, 0xec};
        params.mainnet.default_port = 20866;
        params.mainnet.rpc_port = 20336;
        register_chain(std::move(params));
    }
    
    // Emercoin (chain_id = 6)
    {
        ChainParams params;
        params.name = "emercoin";
        params.ticker = "EMC";
        params.consensus_type = ConsensusType::AUXPOW_HYBRID_POS;
        params.auxpow.chain_id = 6;
        params.auxpow.start_height = 217750;
        params.difficulty.target_spacing = 600;
        params.difficulty.adjustment_interval = 1;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 512000000;
        params.rewards.halving_interval = 0;
        params.rewards.miner_share = 1.0;
        params.rewards.coinbase_maturity = 32;
        params.mainnet.magic = {0xe5, 0xc2, 0xd8, 0xe4};
        params.mainnet.default_port = 6661;
        params.mainnet.rpc_port = 6662;
        register_chain(std::move(params));
    }
    
    // RSK / Rootstock (chain_id = 30)
    {
        ChainParams params;
        params.name = "rsk";
        params.ticker = "RBTC";
        params.consensus_type = ConsensusType::AUXPOW_DECOR;
        params.auxpow.chain_id = 30;
        params.auxpow.start_height = 0;
        params.difficulty.target_spacing = 30;
        params.difficulty.adjustment_interval = 1;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 0;
        params.rewards.halving_interval = 0;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0x05, 0x03, 0x02, 0x01};
        params.mainnet.default_port = 4444;
        params.mainnet.rpc_port = 4443;
        register_chain(std::move(params));
    }
    
    // Hathor (chain_id = custom)
    {
        ChainParams params;
        params.name = "hathor";
        params.ticker = "HTR";
        params.consensus_type = ConsensusType::AUXPOW_DAG;
        params.auxpow.chain_id = 0;
        params.auxpow.start_height = 0;
        params.difficulty.target_spacing = 30;
        params.difficulty.adjustment_interval = 1;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 6400000000;
        params.rewards.halving_interval = 0;
        params.rewards.miner_share = 1.0;
        params.rewards.coinbase_maturity = 300;
        params.mainnet.magic = {0x48, 0x54, 0x52, 0x00};
        params.mainnet.default_port = 8000;
        params.mainnet.rpc_port = 8001;
        register_chain(std::move(params));
    }
    
    // VCash (chain_id = 2)
    {
        ChainParams params;
        params.name = "vcash";
        params.ticker = "XVC";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 2;
        params.auxpow.start_height = 0;
        params.difficulty.target_spacing = 200;
        params.difficulty.adjustment_interval = 2016;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 100000000;
        params.rewards.halving_interval = 840000;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0x5d, 0xcb, 0x9a, 0x4e};
        params.mainnet.default_port = 5765;
        params.mainnet.rpc_port = 5764;
        register_chain(std::move(params));
    }
    
    // Fractal Bitcoin (chain_id = custom)
    {
        ChainParams params;
        params.name = "fractal";
        params.ticker = "FB";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 0;
        params.auxpow.start_height = 0;
        params.auxpow.version_flag = 0x20000000;
        params.difficulty.target_spacing = 600;
        params.difficulty.adjustment_interval = 2016;
        params.difficulty.pow_limit_bits = 0x1d00ffff;
        params.rewards.initial_reward = 2500000000;
        params.rewards.halving_interval = 210000;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0xf9, 0xbe, 0xb4, 0xd9};
        params.mainnet.default_port = 8332;
        params.mainnet.rpc_port = 8331;
        register_chain(std::move(params));
    }
    
    // Myriad (chain_id = 3)
    {
        ChainParams params;
        params.name = "myriad";
        params.ticker = "XMY";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 3;
        params.auxpow.start_height = 1402000;
        params.difficulty.target_spacing = 60;
        params.difficulty.adjustment_interval = 1;
        params.difficulty.pow_limit_bits = 0x1e0fffff;
        params.rewards.initial_reward = 100000000000;
        params.rewards.halving_interval = 967680;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0xaf, 0x45, 0x76, 0xee};
        params.mainnet.default_port = 10888;
        params.mainnet.rpc_port = 10889;
        register_chain(std::move(params));
    }
    
    // Huntercoin (chain_id = 2)
    {
        ChainParams params;
        params.name = "huntercoin";
        params.ticker = "HUC";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 2;
        params.auxpow.start_height = 0;
        params.difficulty.target_spacing = 60;
        params.difficulty.adjustment_interval = 1;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 0;
        params.rewards.halving_interval = 0;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0xf9, 0xbe, 0xb4, 0xb4};
        params.mainnet.default_port = 8398;
        params.mainnet.rpc_port = 8399;
        register_chain(std::move(params));
    }
    
    // Unobtanium (chain_id = 8)
    {
        ChainParams params;
        params.name = "unobtanium";
        params.ticker = "UNO";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 8;
        params.auxpow.start_height = 600000;
        params.difficulty.target_spacing = 180;
        params.difficulty.adjustment_interval = 2016;
        params.difficulty.pow_limit_bits = 0x1e0fffff;
        params.rewards.initial_reward = 100000;
        params.rewards.halving_interval = 102200;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0x03, 0xd5, 0xb5, 0x03};
        params.mainnet.default_port = 65534;
        params.mainnet.rpc_port = 65535;
        register_chain(std::move(params));
    }
    
    // Terracoin (chain_id = 5)
    {
        ChainParams params;
        params.name = "terracoin";
        params.ticker = "TRC";
        params.consensus_type = ConsensusType::PURE_AUXPOW;
        params.auxpow.chain_id = 5;
        params.auxpow.start_height = 833000;
        params.difficulty.target_spacing = 120;
        params.difficulty.adjustment_interval = 2016;
        params.difficulty.pow_limit_bits = 0x1e00ffff;
        params.rewards.initial_reward = 2000000000;
        params.rewards.halving_interval = 1050000;
        params.rewards.miner_share = 1.0;
        params.mainnet.magic = {0x42, 0xba, 0xbe, 0x56};
        params.mainnet.default_port = 13332;
        params.mainnet.rpc_port = 13333;
        register_chain(std::move(params));
    }
}

const ChainParams* ChainRegistry::get_by_name(std::string_view name) const {
    std::string lower_name(name);
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    auto it = name_index_.find(lower_name);
    if (it != name_index_.end()) {
        return &chains_[it->second];
    }
    return nullptr;
}

const ChainParams* ChainRegistry::get_by_ticker(std::string_view ticker) const {
    std::string upper_ticker(ticker);
    std::transform(upper_ticker.begin(), upper_ticker.end(), upper_ticker.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    
    auto it = ticker_index_.find(upper_ticker);
    if (it != ticker_index_.end()) {
        return &chains_[it->second];
    }
    return nullptr;
}

const ChainParams* ChainRegistry::get_by_chain_id(uint32_t chain_id) const {
    auto it = chain_id_index_.find(chain_id);
    if (it != chain_id_index_.end()) {
        return &chains_[it->second];
    }
    return nullptr;
}

bool ChainRegistry::has_chain(std::string_view name) const {
    return get_by_name(name) != nullptr;
}

std::vector<std::string_view> ChainRegistry::get_all_names() const {
    std::vector<std::string_view> names;
    names.reserve(chains_.size());
    for (const auto& chain : chains_) {
        names.emplace_back(chain.name);
    }
    return names;
}

std::size_t ChainRegistry::count() const noexcept {
    return chains_.size();
}

void ChainRegistry::for_each(std::function<void(const ChainParams&)> callback) const {
    for (const auto& chain : chains_) {
        callback(chain);
    }
}

std::vector<const ChainParams*> ChainRegistry::get_by_consensus_type(
    ConsensusType type
) const {
    std::vector<const ChainParams*> result;
    for (const auto& chain : chains_) {
        if (chain.consensus_type == type) {
            result.push_back(&chain);
        }
    }
    return result;
}

bool ChainRegistry::register_chain(ChainParams params) {
    // Проверяем уникальность имени
    std::string lower_name = params.name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (name_index_.find(lower_name) != name_index_.end()) {
        return false;
    }
    
    // Добавляем chain
    std::size_t index = chains_.size();
    chains_.push_back(std::move(params));
    
    // Обновляем индексы
    const auto& chain = chains_.back();
    name_index_[lower_name] = index;
    
    std::string upper_ticker = chain.ticker;
    std::transform(upper_ticker.begin(), upper_ticker.end(), upper_ticker.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    ticker_index_[upper_ticker] = index;
    
    if (chain.auxpow.chain_id != 0) {
        chain_id_index_[chain.auxpow.chain_id] = index;
    }
    
    return true;
}

// Удобные функции доступа
const ChainParams& bitcoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("bitcoin");
    if (!params) {
        throw std::runtime_error("Bitcoin params not found");
    }
    return *params;
}

const ChainParams& namecoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("namecoin");
    if (!params) {
        throw std::runtime_error("Namecoin params not found");
    }
    return *params;
}

const ChainParams& syscoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("syscoin");
    if (!params) {
        throw std::runtime_error("Syscoin params not found");
    }
    return *params;
}

const ChainParams& elastos_params() {
    const auto* params = ChainRegistry::instance().get_by_name("elastos");
    if (!params) {
        throw std::runtime_error("Elastos params not found");
    }
    return *params;
}

const ChainParams& emercoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("emercoin");
    if (!params) {
        throw std::runtime_error("Emercoin params not found");
    }
    return *params;
}

const ChainParams& rsk_params() {
    const auto* params = ChainRegistry::instance().get_by_name("rsk");
    if (!params) {
        throw std::runtime_error("RSK params not found");
    }
    return *params;
}

const ChainParams& hathor_params() {
    const auto* params = ChainRegistry::instance().get_by_name("hathor");
    if (!params) {
        throw std::runtime_error("Hathor params not found");
    }
    return *params;
}

const ChainParams& vcash_params() {
    const auto* params = ChainRegistry::instance().get_by_name("vcash");
    if (!params) {
        throw std::runtime_error("VCash params not found");
    }
    return *params;
}

const ChainParams& fractal_params() {
    const auto* params = ChainRegistry::instance().get_by_name("fractal");
    if (!params) {
        throw std::runtime_error("Fractal params not found");
    }
    return *params;
}

const ChainParams& myriad_params() {
    const auto* params = ChainRegistry::instance().get_by_name("myriad");
    if (!params) {
        throw std::runtime_error("Myriad params not found");
    }
    return *params;
}

const ChainParams& huntercoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("huntercoin");
    if (!params) {
        throw std::runtime_error("Huntercoin params not found");
    }
    return *params;
}

const ChainParams& unobtanium_params() {
    const auto* params = ChainRegistry::instance().get_by_name("unobtanium");
    if (!params) {
        throw std::runtime_error("Unobtanium params not found");
    }
    return *params;
}

const ChainParams& terracoin_params() {
    const auto* params = ChainRegistry::instance().get_by_name("terracoin");
    if (!params) {
        throw std::runtime_error("Terracoin params not found");
    }
    return *params;
}

} // namespace quaxis::core
