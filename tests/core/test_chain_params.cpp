/**
 * @file test_chain_params.cpp
 * @brief Тесты для ChainParams и ChainRegistry
 */

#include <gtest/gtest.h>

#include "core/chain/chain_params.hpp"
#include "core/chain/chain_registry.hpp"

namespace quaxis::core::test {

// =============================================================================
// Тесты ChainRegistry
// =============================================================================

TEST(ChainRegistryTest, InstanceIsSingleton) {
    auto& registry1 = ChainRegistry::instance();
    auto& registry2 = ChainRegistry::instance();
    
    EXPECT_EQ(&registry1, &registry2);
}

TEST(ChainRegistryTest, HasBuiltinChains) {
    auto& registry = ChainRegistry::instance();
    
    // Проверяем наличие основных chains
    EXPECT_TRUE(registry.has_chain("bitcoin"));
    EXPECT_TRUE(registry.has_chain("namecoin"));
    EXPECT_TRUE(registry.has_chain("syscoin"));
    EXPECT_TRUE(registry.has_chain("elastos"));
    EXPECT_TRUE(registry.has_chain("emercoin"));
    EXPECT_TRUE(registry.has_chain("rsk"));
    EXPECT_TRUE(registry.has_chain("hathor"));
    EXPECT_TRUE(registry.has_chain("vcash"));
    EXPECT_TRUE(registry.has_chain("fractal"));
    EXPECT_TRUE(registry.has_chain("myriad"));
    EXPECT_TRUE(registry.has_chain("huntercoin"));
    EXPECT_TRUE(registry.has_chain("unobtanium"));
    EXPECT_TRUE(registry.has_chain("terracoin"));
}

TEST(ChainRegistryTest, GetByName) {
    auto& registry = ChainRegistry::instance();
    
    auto* btc = registry.get_by_name("bitcoin");
    ASSERT_NE(btc, nullptr);
    EXPECT_EQ(btc->name, "bitcoin");
    EXPECT_EQ(btc->ticker, "BTC");
    
    auto* nmc = registry.get_by_name("namecoin");
    ASSERT_NE(nmc, nullptr);
    EXPECT_EQ(nmc->auxpow.chain_id, 1);
}

TEST(ChainRegistryTest, GetByNameCaseInsensitive) {
    auto& registry = ChainRegistry::instance();
    
    auto* btc1 = registry.get_by_name("bitcoin");
    auto* btc2 = registry.get_by_name("BITCOIN");
    auto* btc3 = registry.get_by_name("Bitcoin");
    
    EXPECT_EQ(btc1, btc2);
    EXPECT_EQ(btc2, btc3);
}

TEST(ChainRegistryTest, GetByTicker) {
    auto& registry = ChainRegistry::instance();
    
    auto* btc = registry.get_by_ticker("BTC");
    ASSERT_NE(btc, nullptr);
    EXPECT_EQ(btc->name, "bitcoin");
    
    auto* sys = registry.get_by_ticker("SYS");
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->name, "syscoin");
}

TEST(ChainRegistryTest, GetByChainId) {
    auto& registry = ChainRegistry::instance();
    
    auto* nmc = registry.get_by_chain_id(1);
    ASSERT_NE(nmc, nullptr);
    EXPECT_EQ(nmc->name, "namecoin");
    
    auto* sys = registry.get_by_chain_id(57);
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->name, "syscoin");
}

TEST(ChainRegistryTest, GetAllNames) {
    auto& registry = ChainRegistry::instance();
    
    auto names = registry.get_all_names();
    EXPECT_GE(names.size(), 13);  // Минимум 13 монет
    
    // Проверяем что bitcoin в списке
    bool found_bitcoin = false;
    for (const auto& name : names) {
        if (name == "bitcoin") {
            found_bitcoin = true;
            break;
        }
    }
    EXPECT_TRUE(found_bitcoin);
}

TEST(ChainRegistryTest, GetByConsensusType) {
    auto& registry = ChainRegistry::instance();
    
    auto pure_auxpow = registry.get_by_consensus_type(ConsensusType::PURE_AUXPOW);
    EXPECT_GE(pure_auxpow.size(), 5);  // Namecoin, VCash, Fractal, Myriad, и т.д.
    
    auto chainlock = registry.get_by_consensus_type(ConsensusType::AUXPOW_CHAINLOCK);
    EXPECT_GE(chainlock.size(), 1);  // Syscoin
}

TEST(ChainRegistryTest, ForEach) {
    auto& registry = ChainRegistry::instance();
    
    std::size_t count = 0;
    registry.for_each([&count](const ChainParams&) {
        count++;
    });
    
    EXPECT_EQ(count, registry.count());
}

// =============================================================================
// Тесты ChainParams
// =============================================================================

TEST(ChainParamsTest, BitcoinParams) {
    const auto& btc = bitcoin_params();
    
    EXPECT_EQ(btc.name, "bitcoin");
    EXPECT_EQ(btc.ticker, "BTC");
    EXPECT_EQ(btc.difficulty.target_spacing, 600);
    EXPECT_EQ(btc.difficulty.adjustment_interval, 2016);
    EXPECT_EQ(btc.rewards.halving_interval, 210000);
    EXPECT_EQ(btc.mainnet.default_port, 8333);
    EXPECT_EQ(btc.mainnet.rpc_port, 8332);
}

TEST(ChainParamsTest, NamecoinParams) {
    const auto& nmc = namecoin_params();
    
    EXPECT_EQ(nmc.name, "namecoin");
    EXPECT_EQ(nmc.ticker, "NMC");
    EXPECT_EQ(nmc.auxpow.chain_id, 1);
    EXPECT_EQ(nmc.auxpow.start_height, 19200);
    EXPECT_EQ(nmc.mainnet.default_port, 8334);
}

TEST(ChainParamsTest, SyscoinParams) {
    const auto& sys = syscoin_params();
    
    EXPECT_EQ(sys.name, "syscoin");
    EXPECT_EQ(sys.ticker, "SYS");
    EXPECT_EQ(sys.consensus_type, ConsensusType::AUXPOW_CHAINLOCK);
    EXPECT_EQ(sys.auxpow.chain_id, 57);
    EXPECT_EQ(sys.difficulty.target_spacing, 150);
}

TEST(ChainParamsTest, ElastosParams) {
    const auto& ela = elastos_params();
    
    EXPECT_EQ(ela.name, "elastos");
    EXPECT_EQ(ela.ticker, "ELA");
    EXPECT_EQ(ela.consensus_type, ConsensusType::AUXPOW_HYBRID_BPOS);
    EXPECT_DOUBLE_EQ(ela.rewards.miner_share, 0.35);
    EXPECT_EQ(ela.difficulty.target_spacing, 120);
}

TEST(ChainParamsTest, RskParams) {
    const auto& rsk = rsk_params();
    
    EXPECT_EQ(rsk.name, "rsk");
    EXPECT_EQ(rsk.ticker, "RBTC");
    EXPECT_EQ(rsk.consensus_type, ConsensusType::AUXPOW_DECOR);
    EXPECT_EQ(rsk.auxpow.chain_id, 30);
    EXPECT_EQ(rsk.difficulty.target_spacing, 30);
}

TEST(ChainParamsTest, HathorParams) {
    const auto& htr = hathor_params();
    
    EXPECT_EQ(htr.name, "hathor");
    EXPECT_EQ(htr.ticker, "HTR");
    EXPECT_EQ(htr.consensus_type, ConsensusType::AUXPOW_DAG);
}

TEST(ChainParamsTest, IsAuxPowActive) {
    const auto& nmc = namecoin_params();
    
    EXPECT_FALSE(nmc.is_auxpow_active(19199));
    EXPECT_TRUE(nmc.is_auxpow_active(19200));
    EXPECT_TRUE(nmc.is_auxpow_active(1000000));
}

TEST(ChainParamsTest, GetMinerRewardShare) {
    const auto& btc = bitcoin_params();
    EXPECT_DOUBLE_EQ(btc.get_miner_reward_share(), 1.0);
    
    const auto& ela = elastos_params();
    EXPECT_DOUBLE_EQ(ela.get_miner_reward_share(), 0.35);
}

// =============================================================================
// Тесты ConsensusType
// =============================================================================

TEST(ConsensusTypeTest, ToString) {
    EXPECT_EQ(to_string(ConsensusType::PURE_AUXPOW), "PURE_AUXPOW");
    EXPECT_EQ(to_string(ConsensusType::AUXPOW_CHAINLOCK), "AUXPOW_CHAINLOCK");
    EXPECT_EQ(to_string(ConsensusType::AUXPOW_HYBRID_POS), "AUXPOW_HYBRID_POS");
    EXPECT_EQ(to_string(ConsensusType::AUXPOW_HYBRID_BPOS), "AUXPOW_HYBRID_BPOS");
    EXPECT_EQ(to_string(ConsensusType::AUXPOW_DECOR), "AUXPOW_DECOR");
    EXPECT_EQ(to_string(ConsensusType::AUXPOW_DAG), "AUXPOW_DAG");
}

TEST(ConsensusTypeTest, SupportsStandardAuxPow) {
    EXPECT_TRUE(supports_standard_auxpow(ConsensusType::PURE_AUXPOW));
    EXPECT_TRUE(supports_standard_auxpow(ConsensusType::AUXPOW_CHAINLOCK));
    EXPECT_TRUE(supports_standard_auxpow(ConsensusType::AUXPOW_HYBRID_POS));
    EXPECT_TRUE(supports_standard_auxpow(ConsensusType::AUXPOW_HYBRID_BPOS));
    EXPECT_FALSE(supports_standard_auxpow(ConsensusType::AUXPOW_DECOR));
    EXPECT_FALSE(supports_standard_auxpow(ConsensusType::AUXPOW_DAG));
}

TEST(ConsensusTypeTest, HasRewardSplitting) {
    EXPECT_FALSE(has_reward_splitting(ConsensusType::PURE_AUXPOW));
    EXPECT_FALSE(has_reward_splitting(ConsensusType::AUXPOW_CHAINLOCK));
    EXPECT_TRUE(has_reward_splitting(ConsensusType::AUXPOW_HYBRID_BPOS));
}

} // namespace quaxis::core::test
