/**
 * @file test_additional_chains.cpp
 * @brief Тесты для дополнительных chain implementations
 * 
 * Тестирует 6 новых chains:
 * - Stacks (STX) — Experimental PoX
 * - Myriad (XMY) — Multi-algo
 * - Huntercoin (HUC)
 * - Emercoin (EMC)
 * - Unobtanium (UNO)
 * - Terracoin (TRC)
 */

#include <gtest/gtest.h>

#include "merged/chain_manager.hpp"
#include "merged/chain_interface.hpp"

using namespace quaxis;
using namespace quaxis::merged;

// =============================================================================
// Stacks Chain Tests
// =============================================================================

class StacksChainTest : public ::testing::Test {
protected:
    ChainConfig config;
    
    void SetUp() override {
        config.name = "stacks";
        config.enabled = false;  // По умолчанию выключен
        config.rpc_url = "http://127.0.0.1:20443";
        config.priority = 85;
    }
};

TEST_F(StacksChainTest, CreateChain) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("stacks");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "stacks");
    EXPECT_EQ(info->ticker, "STX");
}

TEST_F(StacksChainTest, DefaultDisabled) {
    // Stacks должен быть выключен по умолчанию из-за PoX
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    config.enabled = false;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto names = manager.get_chain_names();
    EXPECT_EQ(names.size(), 1u);
}

// =============================================================================
// Myriad Chain Tests
// =============================================================================

class MyriadChainTest : public ::testing::Test {
protected:
    ChainConfig config;
    
    void SetUp() override {
        config.name = "myriad";
        config.enabled = true;
        config.rpc_url = "http://127.0.0.1:10888";
        config.priority = 40;
    }
};

TEST_F(MyriadChainTest, CreateChain) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("myriad");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "myriad");
    EXPECT_EQ(info->ticker, "XMY");
}

TEST_F(MyriadChainTest, Priority) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("myriad");
    ASSERT_TRUE(info.has_value());
}

// =============================================================================
// Huntercoin Chain Tests
// =============================================================================

class HuntercoinChainTest : public ::testing::Test {
protected:
    ChainConfig config;
    
    void SetUp() override {
        config.name = "huntercoin";
        config.enabled = true;
        config.rpc_url = "http://127.0.0.1:8398";
        config.priority = 30;
    }
};

TEST_F(HuntercoinChainTest, CreateChain) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("huntercoin");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "huntercoin");
    EXPECT_EQ(info->ticker, "HUC");
}

// =============================================================================
// Emercoin Chain Tests
// =============================================================================

class EmercoinChainTest : public ::testing::Test {
protected:
    ChainConfig config;
    
    void SetUp() override {
        config.name = "emercoin";
        config.enabled = true;
        config.rpc_url = "http://127.0.0.1:6662";
        config.priority = 50;
    }
};

TEST_F(EmercoinChainTest, CreateChain) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("emercoin");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "emercoin");
    EXPECT_EQ(info->ticker, "EMC");
}

// =============================================================================
// Unobtanium Chain Tests
// =============================================================================

class UnobtaniumChainTest : public ::testing::Test {
protected:
    ChainConfig config;
    
    void SetUp() override {
        config.name = "unobtanium";
        config.enabled = true;
        config.rpc_url = "http://127.0.0.1:65530";
        config.priority = 35;
    }
};

TEST_F(UnobtaniumChainTest, CreateChain) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("unobtanium");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "unobtanium");
    EXPECT_EQ(info->ticker, "UNO");
}

// =============================================================================
// Terracoin Chain Tests
// =============================================================================

class TerracoinChainTest : public ::testing::Test {
protected:
    ChainConfig config;
    
    void SetUp() override {
        config.name = "terracoin";
        config.enabled = true;
        config.rpc_url = "http://127.0.0.1:13332";
        config.priority = 25;
    }
};

TEST_F(TerracoinChainTest, CreateChain) {
    MergedMiningConfig mm_config;
    mm_config.enabled = true;
    mm_config.chains.push_back(config);
    
    ChainManager manager(mm_config);
    
    auto info = manager.get_chain_info("terracoin");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->name, "terracoin");
    EXPECT_EQ(info->ticker, "TRC");
}

// =============================================================================
// Combined Chain Tests
// =============================================================================

class AllAdditionalChainsTest : public ::testing::Test {
protected:
    MergedMiningConfig mm_config;
    
    void SetUp() override {
        mm_config.enabled = true;
        
        // Добавляем все 6 новых chains
        ChainConfig stacks;
        stacks.name = "stacks";
        stacks.enabled = false;
        stacks.rpc_url = "http://127.0.0.1:20443";
        stacks.priority = 85;
        mm_config.chains.push_back(stacks);
        
        ChainConfig myriad;
        myriad.name = "myriad";
        myriad.enabled = true;
        myriad.rpc_url = "http://127.0.0.1:10888";
        myriad.priority = 40;
        mm_config.chains.push_back(myriad);
        
        ChainConfig huntercoin;
        huntercoin.name = "huntercoin";
        huntercoin.enabled = true;
        huntercoin.rpc_url = "http://127.0.0.1:8398";
        huntercoin.priority = 30;
        mm_config.chains.push_back(huntercoin);
        
        ChainConfig emercoin;
        emercoin.name = "emercoin";
        emercoin.enabled = true;
        emercoin.rpc_url = "http://127.0.0.1:6662";
        emercoin.priority = 50;
        mm_config.chains.push_back(emercoin);
        
        ChainConfig unobtanium;
        unobtanium.name = "unobtanium";
        unobtanium.enabled = true;
        unobtanium.rpc_url = "http://127.0.0.1:65530";
        unobtanium.priority = 35;
        mm_config.chains.push_back(unobtanium);
        
        ChainConfig terracoin;
        terracoin.name = "terracoin";
        terracoin.enabled = true;
        terracoin.rpc_url = "http://127.0.0.1:13332";
        terracoin.priority = 25;
        mm_config.chains.push_back(terracoin);
    }
};

TEST_F(AllAdditionalChainsTest, CreateAllChains) {
    ChainManager manager(mm_config);
    
    auto names = manager.get_chain_names();
    EXPECT_EQ(names.size(), 6u);
}

TEST_F(AllAdditionalChainsTest, GetAllChainInfo) {
    ChainManager manager(mm_config);
    
    auto all_info = manager.get_all_chain_info();
    EXPECT_EQ(all_info.size(), 6u);
    
    // Проверяем что все chains имеют уникальные имена
    std::set<std::string> unique_names;
    for (const auto& info : all_info) {
        unique_names.insert(info.name);
    }
    EXPECT_EQ(unique_names.size(), 6u);
}

TEST_F(AllAdditionalChainsTest, UniqueTickers) {
    ChainManager manager(mm_config);
    
    auto all_info = manager.get_all_chain_info();
    
    // Проверяем что все tickers уникальны
    std::set<std::string> unique_tickers;
    for (const auto& info : all_info) {
        unique_tickers.insert(info.ticker);
    }
    EXPECT_EQ(unique_tickers.size(), 6u);
    
    // Проверяем конкретные тикеры
    std::set<std::string> expected_tickers = {"STX", "XMY", "HUC", "EMC", "UNO", "TRC"};
    EXPECT_EQ(unique_tickers, expected_tickers);
}

TEST_F(AllAdditionalChainsTest, EnableDisableChains) {
    ChainManager manager(mm_config);
    
    // Stacks изначально выключен
    auto stacks_info = manager.get_chain_info("stacks");
    ASSERT_TRUE(stacks_info.has_value());
    
    // Включаем Stacks
    bool result = manager.set_chain_enabled("stacks", true);
    EXPECT_TRUE(result);
    
    // Выключаем Myriad
    result = manager.set_chain_enabled("myriad", false);
    EXPECT_TRUE(result);
}

// =============================================================================
// Integration with Existing Chains Tests
// =============================================================================

class IntegrationWithExistingChainsTest : public ::testing::Test {
protected:
    MergedMiningConfig mm_config;
    
    void SetUp() override {
        mm_config.enabled = true;
        
        // Существующие chains
        ChainConfig fractal;
        fractal.name = "fractal";
        fractal.enabled = true;
        fractal.rpc_url = "http://127.0.0.1:8332";
        fractal.priority = 100;
        mm_config.chains.push_back(fractal);
        
        ChainConfig namecoin;
        namecoin.name = "namecoin";
        namecoin.enabled = true;
        namecoin.rpc_url = "http://127.0.0.1:8336";
        namecoin.priority = 70;
        mm_config.chains.push_back(namecoin);
        
        // Новые chains
        ChainConfig myriad;
        myriad.name = "myriad";
        myriad.enabled = true;
        myriad.rpc_url = "http://127.0.0.1:10888";
        myriad.priority = 40;
        mm_config.chains.push_back(myriad);
        
        ChainConfig emercoin;
        emercoin.name = "emercoin";
        emercoin.enabled = true;
        emercoin.rpc_url = "http://127.0.0.1:6662";
        emercoin.priority = 50;
        mm_config.chains.push_back(emercoin);
    }
};

TEST_F(IntegrationWithExistingChainsTest, MixedChains) {
    ChainManager manager(mm_config);
    
    auto names = manager.get_chain_names();
    EXPECT_EQ(names.size(), 4u);
    
    // Проверяем существующие chains
    auto fractal_info = manager.get_chain_info("fractal");
    ASSERT_TRUE(fractal_info.has_value());
    EXPECT_EQ(fractal_info->ticker, "FB");
    
    auto namecoin_info = manager.get_chain_info("namecoin");
    ASSERT_TRUE(namecoin_info.has_value());
    EXPECT_EQ(namecoin_info->ticker, "NMC");
    
    // Проверяем новые chains
    auto myriad_info = manager.get_chain_info("myriad");
    ASSERT_TRUE(myriad_info.has_value());
    EXPECT_EQ(myriad_info->ticker, "XMY");
    
    auto emercoin_info = manager.get_chain_info("emercoin");
    ASSERT_TRUE(emercoin_info.has_value());
    EXPECT_EQ(emercoin_info->ticker, "EMC");
}

TEST_F(IntegrationWithExistingChainsTest, StartStopWithMixedChains) {
    ChainManager manager(mm_config);
    
    EXPECT_FALSE(manager.is_running());
    
    manager.start();
    EXPECT_TRUE(manager.is_running());
    
    manager.stop();
    EXPECT_FALSE(manager.is_running());
}
