/**
 * @file test_chain_manager.cpp
 * @brief Тесты для ChainManager
 */

#include <gtest/gtest.h>

#include "merged/chain_manager.hpp"
#include "merged/chain_interface.hpp"

using namespace quaxis;
using namespace quaxis::merged;

// =============================================================================
// ChainManager Configuration Tests
// =============================================================================

class ChainManagerConfigTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    
    void SetUp() override {
        config.enabled = true;
        config.health_check_interval = 60;
    }
};

TEST_F(ChainManagerConfigTest, CreateWithEmptyConfig) {
    MergedMiningConfig empty_config;
    empty_config.enabled = false;
    
    ChainManager manager(empty_config);
    
    EXPECT_FALSE(manager.is_running());
    EXPECT_EQ(manager.get_chain_names().size(), 0u);
}

TEST_F(ChainManagerConfigTest, CreateWithChains) {
    ChainConfig fractal_config;
    fractal_config.name = "fractal";
    fractal_config.enabled = true;
    fractal_config.rpc_url = "http://127.0.0.1:8332";
    fractal_config.priority = 100;
    
    ChainConfig namecoin_config;
    namecoin_config.name = "namecoin";
    namecoin_config.enabled = true;
    namecoin_config.rpc_url = "http://127.0.0.1:8336";
    namecoin_config.priority = 70;
    
    config.chains.push_back(fractal_config);
    config.chains.push_back(namecoin_config);
    
    ChainManager manager(config);
    
    auto names = manager.get_chain_names();
    EXPECT_EQ(names.size(), 2u);
    
    // Проверяем что chains созданы
    auto fractal_info = manager.get_chain_info("fractal");
    ASSERT_TRUE(fractal_info.has_value());
    EXPECT_EQ(fractal_info->name, "fractal");
    
    auto namecoin_info = manager.get_chain_info("namecoin");
    ASSERT_TRUE(namecoin_info.has_value());
    EXPECT_EQ(namecoin_info->name, "namecoin");
}

TEST_F(ChainManagerConfigTest, UnknownChainReturnsNullopt) {
    ChainManager manager(config);
    
    auto info = manager.get_chain_info("unknown_chain");
    EXPECT_FALSE(info.has_value());
}

// =============================================================================
// ChainManager Lifecycle Tests
// =============================================================================

class ChainManagerLifecycleTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    
    void SetUp() override {
        config.enabled = true;
    }
};

TEST_F(ChainManagerLifecycleTest, StartStop) {
    ChainManager manager(config);
    
    EXPECT_FALSE(manager.is_running());
    
    manager.start();
    EXPECT_TRUE(manager.is_running());
    
    manager.stop();
    EXPECT_FALSE(manager.is_running());
}

TEST_F(ChainManagerLifecycleTest, DoubleStart) {
    ChainManager manager(config);
    
    manager.start();
    EXPECT_TRUE(manager.is_running());
    
    // Второй start не должен ломать ничего
    manager.start();
    EXPECT_TRUE(manager.is_running());
    
    manager.stop();
}

TEST_F(ChainManagerLifecycleTest, DoubleStop) {
    ChainManager manager(config);
    
    manager.start();
    manager.stop();
    EXPECT_FALSE(manager.is_running());
    
    // Второй stop не должен ломать ничего
    manager.stop();
    EXPECT_FALSE(manager.is_running());
}

// =============================================================================
// ChainManager Chain Control Tests
// =============================================================================

class ChainManagerControlTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    
    void SetUp() override {
        config.enabled = true;
        
        ChainConfig chain_config;
        chain_config.name = "fractal";
        chain_config.enabled = true;
        chain_config.rpc_url = "http://127.0.0.1:8332";
        config.chains.push_back(chain_config);
    }
};

TEST_F(ChainManagerControlTest, EnableDisableChain) {
    ChainManager manager(config);
    
    // По умолчанию chain включен
    auto info = manager.get_chain_info("fractal");
    ASSERT_TRUE(info.has_value());
    
    // Отключаем chain
    bool result = manager.set_chain_enabled("fractal", false);
    EXPECT_TRUE(result);
    
    // Включаем обратно
    result = manager.set_chain_enabled("fractal", true);
    EXPECT_TRUE(result);
}

TEST_F(ChainManagerControlTest, EnableUnknownChain) {
    ChainManager manager(config);
    
    bool result = manager.set_chain_enabled("unknown", true);
    EXPECT_FALSE(result);
}

TEST_F(ChainManagerControlTest, GetAllChainInfo) {
    ChainConfig another_config;
    another_config.name = "namecoin";
    another_config.enabled = true;
    another_config.rpc_url = "http://127.0.0.1:8336";
    config.chains.push_back(another_config);
    
    ChainManager manager(config);
    
    auto all_info = manager.get_all_chain_info();
    EXPECT_EQ(all_info.size(), 2u);
}

// =============================================================================
// ChainManager AuxPoW Tests
// =============================================================================

class ChainManagerAuxPowTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    
    void SetUp() override {
        config.enabled = true;
    }
};

TEST_F(ChainManagerAuxPowTest, NoCommitmentWithoutChains) {
    ChainManager manager(config);
    
    auto commitment = manager.get_aux_commitment();
    EXPECT_FALSE(commitment.has_value());
}

TEST_F(ChainManagerAuxPowTest, ActiveChainCount) {
    ChainConfig chain_config;
    chain_config.name = "fractal";
    chain_config.enabled = true;
    chain_config.rpc_url = "http://127.0.0.1:8332";
    config.chains.push_back(chain_config);
    
    ChainManager manager(config);
    
    // Без запуска - 0 активных chains
    EXPECT_EQ(manager.active_chain_count(), 0u);
}

TEST_F(ChainManagerAuxPowTest, GetActiveTemplates) {
    ChainManager manager(config);
    
    auto templates = manager.get_active_templates();
    EXPECT_TRUE(templates.empty());
}

// =============================================================================
// ChainManager Block Dispatch Tests
// =============================================================================

class ChainManagerDispatchTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    
    void SetUp() override {
        config.enabled = true;
        
        ChainConfig chain_config;
        chain_config.name = "fractal";
        chain_config.enabled = true;
        chain_config.rpc_url = "http://127.0.0.1:8332";
        config.chains.push_back(chain_config);
    }
};

TEST_F(ChainManagerDispatchTest, CheckAuxChainsEmpty) {
    ChainManager manager(config);
    
    std::array<uint8_t, 80> header{};
    Bytes coinbase;
    MerkleBranch branch;
    
    auto matching = manager.check_aux_chains(header, coinbase, branch);
    
    // Без подключения и шаблонов - пустой список
    EXPECT_TRUE(matching.empty());
}

TEST_F(ChainManagerDispatchTest, SubmitToUnknownChain) {
    ChainManager manager(config);
    
    AuxPow auxpow;
    auto result = manager.submit_aux_block("unknown_chain", auxpow);
    
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::MiningInvalidJob);
}

TEST_F(ChainManagerDispatchTest, GetBlockCounts) {
    ChainManager manager(config);
    
    auto counts = manager.get_block_counts();
    EXPECT_TRUE(counts.empty());
}

// =============================================================================
// ChainManager Callback Tests
// =============================================================================

class ChainManagerCallbackTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    bool callback_called{false};
    std::string last_chain_name;
    
    void SetUp() override {
        config.enabled = true;
        callback_called = false;
        last_chain_name.clear();
    }
};

TEST_F(ChainManagerCallbackTest, SetBlockFoundCallback) {
    ChainManager manager(config);
    
    manager.set_block_found_callback([this](
        const std::string& chain_name,
        [[maybe_unused]] uint32_t height,
        [[maybe_unused]] const Hash256& block_hash
    ) {
        callback_called = true;
        last_chain_name = chain_name;
    });
    
    // Callback не должен вызываться сам по себе
    EXPECT_FALSE(callback_called);
}

// =============================================================================
// ChainConfig Tests
// =============================================================================

class ChainConfigTest : public ::testing::Test {};

TEST_F(ChainConfigTest, DefaultValues) {
    ChainConfig config;
    
    EXPECT_TRUE(config.name.empty());
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.priority, 50u);
    EXPECT_EQ(config.rpc_timeout, 30u);
    EXPECT_EQ(config.update_interval, 5u);
}

TEST_F(ChainConfigTest, CustomValues) {
    ChainConfig config;
    config.name = "test_chain";
    config.enabled = false;
    config.rpc_url = "http://localhost:1234";
    config.rpc_user = "user";
    config.rpc_password = "pass";
    config.priority = 100;
    config.rpc_timeout = 60;
    config.update_interval = 10;
    
    EXPECT_EQ(config.name, "test_chain");
    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.rpc_url, "http://localhost:1234");
    EXPECT_EQ(config.rpc_user, "user");
    EXPECT_EQ(config.rpc_password, "pass");
    EXPECT_EQ(config.priority, 100u);
    EXPECT_EQ(config.rpc_timeout, 60u);
    EXPECT_EQ(config.update_interval, 10u);
}

// =============================================================================
// MergedMiningConfig Tests
// =============================================================================

class MergedMiningConfigTest : public ::testing::Test {};

TEST_F(MergedMiningConfigTest, DefaultValues) {
    MergedMiningConfig config;
    
    EXPECT_FALSE(config.enabled);
    EXPECT_TRUE(config.chains.empty());
    EXPECT_EQ(config.health_check_interval, 60u);
}

TEST_F(MergedMiningConfigTest, AddChains) {
    MergedMiningConfig config;
    config.enabled = true;
    
    ChainConfig chain1;
    chain1.name = "chain1";
    config.chains.push_back(chain1);
    
    ChainConfig chain2;
    chain2.name = "chain2";
    config.chains.push_back(chain2);
    
    EXPECT_EQ(config.chains.size(), 2u);
    EXPECT_EQ(config.chains[0].name, "chain1");
    EXPECT_EQ(config.chains[1].name, "chain2");
}
