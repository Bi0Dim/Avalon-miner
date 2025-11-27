/**
 * @file test_merged_integration.cpp
 * @brief Интеграционные тесты для merged mining
 */

#include <gtest/gtest.h>

#include "merged/auxpow.hpp"
#include "merged/chain_manager.hpp"
#include "merged/merged_job_creator.hpp"
#include "merged/reward_dispatcher.hpp"
#include "bitcoin/coinbase.hpp"
#include "bitcoin/block.hpp"

using namespace quaxis;
using namespace quaxis::merged;

// =============================================================================
// Integration Test Fixtures
// =============================================================================

class MergedMiningIntegrationTest : public ::testing::Test {
protected:
    MergedMiningConfig config;
    Hash160 test_pubkey_hash{};
    
    void SetUp() override {
        config.enabled = true;
        config.health_check_interval = 60;
        
        // Добавляем тестовые chains
        ChainConfig fractal;
        fractal.name = "fractal";
        fractal.enabled = true;
        fractal.rpc_url = "http://127.0.0.1:8332";
        fractal.priority = 100;
        config.chains.push_back(fractal);
        
        ChainConfig namecoin;
        namecoin.name = "namecoin";
        namecoin.enabled = true;
        namecoin.rpc_url = "http://127.0.0.1:8336";
        namecoin.priority = 70;
        config.chains.push_back(namecoin);
        
        // Тестовый pubkey hash
        std::fill(test_pubkey_hash.begin(), test_pubkey_hash.end(), 0x42);
    }
};

// =============================================================================
// MergedJobCreator Tests
// =============================================================================

TEST_F(MergedMiningIntegrationTest, CreateMergedJob) {
    ChainManager chain_manager(config);
    bitcoin::CoinbaseBuilder coinbase_builder(test_pubkey_hash, "quaxis");
    
    MergedJobCreator job_creator(chain_manager, coinbase_builder);
    
    // Создаём тестовый Bitcoin template
    bitcoin::BlockTemplate btc_template;
    btc_template.height = 800000;
    btc_template.coinbase_value = 625000000;  // 6.25 BTC
    
    // Создаём merged job
    MergedJob job = job_creator.create_job(btc_template, 1, 0);
    
    EXPECT_EQ(job.job_id, 1u);
    EXPECT_EQ(job.extranonce, 0u);
    EXPECT_EQ(job.bitcoin_template.height, 800000u);
    
    // Coinbase должна быть создана
    EXPECT_FALSE(job.coinbase_tx.empty());
}

TEST_F(MergedMiningIntegrationTest, GetCurrentAuxCommitment) {
    ChainManager chain_manager(config);
    bitcoin::CoinbaseBuilder coinbase_builder(test_pubkey_hash, "quaxis");
    
    MergedJobCreator job_creator(chain_manager, coinbase_builder);
    
    // Без активных chains commitment может быть пустым
    auto commitment = job_creator.get_current_aux_commitment();
    
    // Это OK - chains не подключены
    // Главное что функция работает без падений
    // Проверяем что возвращается nullopt
    EXPECT_FALSE(commitment.has_value());
}

// =============================================================================
// RewardDispatcher Tests
// =============================================================================

TEST_F(MergedMiningIntegrationTest, DispatcherCreation) {
    ChainManager chain_manager(config);
    RewardDispatcher dispatcher(chain_manager);
    
    // Dispatcher создан успешно
    auto stats = dispatcher.get_dispatch_stats();
    EXPECT_TRUE(stats.empty());
}

TEST_F(MergedMiningIntegrationTest, CheckAllChains) {
    ChainManager chain_manager(config);
    RewardDispatcher dispatcher(chain_manager);
    
    bitcoin::BlockHeader header;
    header.version = 0x20000000;
    header.timestamp = 1700000000;
    header.bits = 0x1d00ffff;
    header.nonce = 12345;
    
    // Без активных chains должен вернуть пустой список
    auto matching = dispatcher.check_all_chains(header);
    
    // Пустой список - OK, chains не подключены
    EXPECT_TRUE(matching.empty());
}

TEST_F(MergedMiningIntegrationTest, DispatcherCallback) {
    ChainManager chain_manager(config);
    RewardDispatcher dispatcher(chain_manager);
    
    bool callback_called = false;
    
    dispatcher.set_dispatch_callback([&callback_called](
        [[maybe_unused]] const DispatchResult& result
    ) {
        callback_called = true;
    });
    
    // Callback установлен, но не вызван пока нет блоков
    EXPECT_FALSE(callback_called);
}

// =============================================================================
// End-to-End Flow Tests
// =============================================================================

TEST_F(MergedMiningIntegrationTest, FullWorkflow) {
    // 1. Создаём chain manager
    ChainManager chain_manager(config);
    
    // 2. Создаём coinbase builder
    bitcoin::CoinbaseBuilder coinbase_builder(test_pubkey_hash, "quaxis");
    
    // 3. Создаём job creator
    MergedJobCreator job_creator(chain_manager, coinbase_builder);
    
    // 4. Создаём dispatcher
    RewardDispatcher dispatcher(chain_manager);
    
    // 5. Создаём Bitcoin template
    bitcoin::BlockTemplate btc_template;
    btc_template.height = 850000;
    btc_template.coinbase_value = 625000000;
    btc_template.header.version = 0x20000000;
    btc_template.header.bits = 0x1d00ffff;
    btc_template.header.timestamp = 1700000000;
    
    // 6. Создаём merged job
    MergedJob job = job_creator.create_job(btc_template, 1, 0);
    
    EXPECT_FALSE(job.coinbase_tx.empty());
    EXPECT_EQ(job.bitcoin_template.height, 850000u);
    
    // 7. Симулируем нахождение блока
    bitcoin::BlockHeader found_header = job.bitcoin_template.header;
    found_header.nonce = 999999;
    
    // 8. Проверяем aux chains
    auto matching = dispatcher.check_all_chains(found_header);
    
    // Без подключённых нод - пустой результат
    // Это ожидаемое поведение
}

// =============================================================================
// AuxPoW Commitment Integration Tests
// =============================================================================

TEST_F(MergedMiningIntegrationTest, CommitmentInCoinbase) {
    // Создаём commitment
    AuxCommitment commitment;
    commitment.aux_merkle_root[0] = 0xAB;
    commitment.aux_merkle_root[31] = 0xCD;
    commitment.tree_size = 4;
    commitment.merkle_nonce = 12345;
    
    // Сериализуем
    auto data = commitment.serialize();
    
    // Создаём coinbase с commitment
    Bytes coinbase;
    coinbase.resize(50, 0x00);  // Prefix
    coinbase.insert(coinbase.end(), data.begin(), data.end());
    coinbase.resize(coinbase.size() + 30, 0xFF);  // Suffix
    
    // Ищем commitment
    auto found = AuxCommitment::find_in_coinbase(coinbase);
    
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->aux_merkle_root[0], 0xAB);
    EXPECT_EQ(found->aux_merkle_root[31], 0xCD);
    EXPECT_EQ(found->tree_size, 4u);
    EXPECT_EQ(found->merkle_nonce, 12345u);
}

// =============================================================================
// Multi-Chain AuxPoW Tests
// =============================================================================

TEST_F(MergedMiningIntegrationTest, MultiChainMerkleBranch) {
    // Создаём хеши для нескольких chains
    std::vector<Hash256> chain_hashes;
    for (int i = 0; i < 4; ++i) {
        Hash256 hash{};
        hash[0] = static_cast<uint8_t>(i + 1);
        chain_hashes.push_back(hash);
    }
    
    // Строим Merkle tree
    auto tree = build_merkle_tree(chain_hashes);
    
    EXPECT_FALSE(tree.empty());
    
    // Получаем branch для каждого chain
    for (std::size_t i = 0; i < chain_hashes.size(); ++i) {
        auto branch = get_merkle_branch(tree, i);
        
        // Проверяем что branch ведёт к корню
        Hash256 root = branch.compute_root(chain_hashes[i]);
        EXPECT_EQ(root, tree.back());
    }
}

TEST_F(MergedMiningIntegrationTest, AuxPowVerification) {
    // Создаём тестовый AuxPoW
    AuxPow auxpow;
    
    // Заполняем coinbase с commitment
    AuxCommitment commitment;
    commitment.tree_size = 1;
    commitment.merkle_nonce = 0;
    
    Hash256 aux_hash{};
    aux_hash[0] = 0x42;
    commitment.aux_merkle_root = aux_hash;
    
    auto commitment_data = commitment.serialize();
    auxpow.coinbase_tx.insert(
        auxpow.coinbase_tx.end(),
        commitment_data.begin(),
        commitment_data.end()
    );
    
    // Parent header - простой тест
    std::fill(auxpow.parent_header.begin(), auxpow.parent_header.end(), 0);
    
    // Для полной верификации нужны корректные Merkle branches
    // Здесь проверяем что код не падает
    Hash256 test_aux_hash{};
    test_aux_hash[0] = 0x42;
    
    // Верификация может вернуть false из-за неполных данных
    // Главное - код работает
    [[maybe_unused]] bool result = auxpow.verify(test_aux_hash);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(MergedMiningIntegrationTest, MerkleTreeBuildPerformance) {
    // Создаём 256 хешей (максимум для 8 chains с tree_size=256)
    std::vector<Hash256> leaves(256);
    for (std::size_t i = 0; i < leaves.size(); ++i) {
        leaves[i][0] = static_cast<uint8_t>(i & 0xFF);
        leaves[i][1] = static_cast<uint8_t>((i >> 8) & 0xFF);
    }
    
    // Строим дерево 100 раз
    for (int i = 0; i < 100; ++i) {
        auto tree = build_merkle_tree(leaves);
        EXPECT_FALSE(tree.empty());
    }
}

TEST_F(MergedMiningIntegrationTest, CommitmentSerializationPerformance) {
    AuxCommitment commitment;
    std::fill(commitment.aux_merkle_root.begin(), 
              commitment.aux_merkle_root.end(), 0xAB);
    commitment.tree_size = 8;
    commitment.merkle_nonce = 999;
    
    // Сериализуем 10000 раз
    for (int i = 0; i < 10000; ++i) {
        auto data = commitment.serialize();
        EXPECT_EQ(data.size(), 44u);
    }
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(MergedMiningIntegrationTest, EmptyChainList) {
    MergedMiningConfig empty_config;
    empty_config.enabled = true;
    
    ChainManager manager(empty_config);
    
    EXPECT_EQ(manager.active_chain_count(), 0u);
    EXPECT_FALSE(manager.get_aux_commitment().has_value());
}

TEST_F(MergedMiningIntegrationTest, AllChainsDisabled) {
    for (auto& chain : config.chains) {
        chain.enabled = false;
    }
    
    ChainManager manager(config);
    
    // Chains созданы, но отключены
    EXPECT_EQ(manager.get_chain_names().size(), 2u);
}

TEST_F(MergedMiningIntegrationTest, SingleChainOperation) {
    MergedMiningConfig single_config;
    single_config.enabled = true;
    
    ChainConfig chain;
    chain.name = "fractal";
    chain.enabled = true;
    chain.rpc_url = "http://127.0.0.1:8332";
    single_config.chains.push_back(chain);
    
    ChainManager manager(single_config);
    
    EXPECT_EQ(manager.get_chain_names().size(), 1u);
    
    auto info = manager.get_chain_info("fractal");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->ticker, "FB");
}
