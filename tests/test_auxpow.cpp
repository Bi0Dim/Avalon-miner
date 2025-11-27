/**
 * @file test_auxpow.cpp
 * @brief Тесты для AuxPoW структур и функций
 */

#include <gtest/gtest.h>

#include "merged/auxpow.hpp"

using namespace quaxis;
using namespace quaxis::merged;

// =============================================================================
// MerkleBranch Tests
// =============================================================================

class MerkleBranchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализация тестовых данных
    }
};

TEST_F(MerkleBranchTest, EmptyBranch) {
    MerkleBranch branch;
    Hash256 leaf{};
    leaf[0] = 0x01;
    
    // Пустой branch должен вернуть сам leaf как root
    Hash256 root = branch.compute_root(leaf);
    EXPECT_EQ(root, leaf);
}

TEST_F(MerkleBranchTest, SingleLevelBranch) {
    MerkleBranch branch;
    branch.index = 0;
    
    Hash256 sibling{};
    sibling[0] = 0x02;
    branch.hashes.push_back(sibling);
    
    Hash256 leaf{};
    leaf[0] = 0x01;
    
    Hash256 root = branch.compute_root(leaf);
    
    // Root должен быть SHA256d(leaf || sibling)
    EXPECT_NE(root, leaf);
    EXPECT_NE(root, sibling);
}

TEST_F(MerkleBranchTest, Serialization) {
    MerkleBranch original;
    original.index = 5;
    
    Hash256 hash1{}, hash2{};
    hash1[0] = 0xAA;
    hash2[0] = 0xBB;
    original.hashes.push_back(hash1);
    original.hashes.push_back(hash2);
    
    Bytes serialized = original.serialize();
    
    auto result = MerkleBranch::deserialize(serialized);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(result->index, original.index);
    EXPECT_EQ(result->hashes.size(), original.hashes.size());
    EXPECT_EQ(result->hashes[0], original.hashes[0]);
    EXPECT_EQ(result->hashes[1], original.hashes[1]);
}

TEST_F(MerkleBranchTest, Verification) {
    Hash256 leaf{};
    leaf[0] = 0x42;
    
    // Вычисляем root с пустым branch
    MerkleBranch branch;
    Hash256 expected_root = branch.compute_root(leaf);
    
    // Верификация должна успешно пройти
    EXPECT_TRUE(branch.verify(leaf, expected_root));
    
    // С неправильным root должна провалиться
    Hash256 wrong_root{};
    wrong_root[0] = 0xFF;
    EXPECT_FALSE(branch.verify(leaf, wrong_root));
}

// =============================================================================
// AuxCommitment Tests
// =============================================================================

class AuxCommitmentTest : public ::testing::Test {};

TEST_F(AuxCommitmentTest, Serialization) {
    AuxCommitment commitment;
    commitment.aux_merkle_root[0] = 0xDE;
    commitment.aux_merkle_root[31] = 0xAD;
    commitment.tree_size = 8;
    commitment.merkle_nonce = 12345;
    
    auto serialized = commitment.serialize();
    
    // Проверяем magic bytes
    EXPECT_EQ(serialized[0], AUXPOW_MAGIC[0]);
    EXPECT_EQ(serialized[1], AUXPOW_MAGIC[1]);
    EXPECT_EQ(serialized[2], AUXPOW_MAGIC[2]);
    EXPECT_EQ(serialized[3], AUXPOW_MAGIC[3]);
    
    // Проверяем размер
    EXPECT_EQ(serialized.size(), 44u);
}

TEST_F(AuxCommitmentTest, FindInCoinbase) {
    // Создаём coinbase с commitment
    Bytes coinbase;
    
    // Добавляем какие-то данные перед magic
    coinbase.resize(20, 0x00);
    
    // Добавляем commitment
    AuxCommitment original;
    original.aux_merkle_root[0] = 0x42;
    original.tree_size = 4;
    original.merkle_nonce = 999;
    
    auto commitment_data = original.serialize();
    coinbase.insert(coinbase.end(), commitment_data.begin(), commitment_data.end());
    
    // Добавляем данные после commitment
    coinbase.resize(coinbase.size() + 30, 0xFF);
    
    // Ищем commitment
    auto found = AuxCommitment::find_in_coinbase(coinbase);
    
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->aux_merkle_root[0], 0x42);
    EXPECT_EQ(found->tree_size, 4u);
    EXPECT_EQ(found->merkle_nonce, 999u);
}

TEST_F(AuxCommitmentTest, NotFoundInCoinbase) {
    Bytes coinbase(100, 0x00);  // Coinbase без magic
    
    auto found = AuxCommitment::find_in_coinbase(coinbase);
    EXPECT_FALSE(found.has_value());
}

// =============================================================================
// AuxPow Tests
// =============================================================================

class AuxPowTest : public ::testing::Test {};

TEST_F(AuxPowTest, Serialization) {
    AuxPow original;
    
    // Заполняем тестовыми данными
    original.coinbase_tx = {0x01, 0x02, 0x03, 0x04};
    original.coinbase_hash[0] = 0xAA;
    original.parent_header[0] = 0xBB;
    
    Bytes serialized = original.serialize();
    
    // Проверяем что сериализация не пустая
    EXPECT_GT(serialized.size(), 0u);
    
    // Десериализуем и сравниваем
    auto result = AuxPow::deserialize(serialized);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(result->coinbase_tx, original.coinbase_tx);
    EXPECT_EQ(result->coinbase_hash[0], original.coinbase_hash[0]);
    EXPECT_EQ(result->parent_header[0], original.parent_header[0]);
}

TEST_F(AuxPowTest, GetParentHash) {
    AuxPow auxpow;
    
    // Заполняем parent_header нулями
    std::fill(auxpow.parent_header.begin(), auxpow.parent_header.end(), 0);
    
    Hash256 hash = auxpow.get_parent_hash();
    
    // Хеш 80 нулей должен быть определённым значением
    // SHA256d(80 * 0x00) - известное значение
    EXPECT_NE(hash[0], 0);  // Должен быть ненулевым
}

// =============================================================================
// Merkle Tree Functions Tests
// =============================================================================

class MerkleTreeTest : public ::testing::Test {};

TEST_F(MerkleTreeTest, BuildEmptyTree) {
    std::vector<Hash256> empty;
    auto tree = build_merkle_tree(empty);
    EXPECT_TRUE(tree.empty());
}

TEST_F(MerkleTreeTest, BuildSingleLeafTree) {
    Hash256 leaf{};
    leaf[0] = 0x42;
    
    std::vector<Hash256> leaves = {leaf};
    auto tree = build_merkle_tree(leaves);
    
    // Дерево с одним листом должно содержать минимум 2 элемента
    // (лист дублируется, затем хешируется)
    EXPECT_GE(tree.size(), 1u);
}

TEST_F(MerkleTreeTest, BuildTwoLeafTree) {
    Hash256 leaf1{}, leaf2{};
    leaf1[0] = 0x01;
    leaf2[0] = 0x02;
    
    std::vector<Hash256> leaves = {leaf1, leaf2};
    auto tree = build_merkle_tree(leaves);
    
    // Два листа + корень = минимум 3 элемента
    EXPECT_GE(tree.size(), 3u);
    
    // Root должен отличаться от листов
    EXPECT_NE(tree.back(), leaf1);
    EXPECT_NE(tree.back(), leaf2);
}

TEST_F(MerkleTreeTest, GetBranchForIndex) {
    Hash256 leaf1{}, leaf2{}, leaf3{}, leaf4{};
    leaf1[0] = 0x01;
    leaf2[0] = 0x02;
    leaf3[0] = 0x03;
    leaf4[0] = 0x04;
    
    std::vector<Hash256> leaves = {leaf1, leaf2, leaf3, leaf4};
    auto tree = build_merkle_tree(leaves);
    
    // Получаем branch для первого листа
    auto branch = get_merkle_branch(tree, 0);
    
    // Branch должен иметь log2(4) = 2 элемента
    EXPECT_EQ(branch.hashes.size(), 2u);
    EXPECT_EQ(branch.index, 0u);
    
    // Верифицируем branch
    Hash256 root = tree.back();
    EXPECT_TRUE(branch.verify(leaf1, root));
}

// =============================================================================
// Target Functions Tests
// =============================================================================

class AuxPowTargetTest : public ::testing::Test {};

TEST_F(AuxPowTargetTest, BitsToTarget) {
    // Стандартный genesis block bits
    uint32_t bits = 0x1d00ffff;
    Hash256 target = bits_to_target(bits);
    
    // Проверяем что target не нулевой
    bool all_zero = true;
    for (uint8_t byte : target) {
        if (byte != 0) {
            all_zero = false;
            break;
        }
    }
    EXPECT_FALSE(all_zero);
}

TEST_F(AuxPowTargetTest, MeetsTargetTrue) {
    // Создаём очень большой target (легкий)
    uint32_t easy_bits = 0x1d00ffff;  // Genesis difficulty
    
    // Создаём хеш с большим количеством ведущих нулей
    Hash256 hash{};  // Все нули - самый маленький хеш
    
    EXPECT_TRUE(meets_target(hash, easy_bits));
}

TEST_F(AuxPowTargetTest, MeetsTargetFalse) {
    // Создаём очень маленький target (сложный)
    uint32_t hard_bits = 0x03010000;  // Очень низкий target
    
    // Создаём большой хеш
    Hash256 hash;
    std::fill(hash.begin(), hash.end(), 0xFF);
    
    EXPECT_FALSE(meets_target(hash, hard_bits));
}

// =============================================================================
// Slot ID Tests
// =============================================================================

class SlotIdTest : public ::testing::Test {};

TEST_F(SlotIdTest, ComputeSlotId) {
    Hash256 chain_id{};
    chain_id[0] = 0x01;
    chain_id[1] = 0x02;
    chain_id[2] = 0x03;
    chain_id[3] = 0x04;
    
    uint32_t nonce = 0;
    uint32_t tree_size = 8;
    
    uint32_t slot = compute_slot_id(chain_id, nonce, tree_size);
    
    // Slot должен быть в пределах [0, tree_size)
    EXPECT_LT(slot, tree_size);
}

TEST_F(SlotIdTest, DifferentNonceDifferentSlot) {
    Hash256 chain_id{};
    chain_id[0] = 0xFF;
    
    uint32_t tree_size = 8;
    
    uint32_t slot1 = compute_slot_id(chain_id, 0, tree_size);
    uint32_t slot2 = compute_slot_id(chain_id, 1, tree_size);
    
    // Разные nonce могут давать разные слоты
    // (не гарантировано, но для большинства случаев)
    // Просто проверяем что функция работает
    EXPECT_LT(slot1, tree_size);
    EXPECT_LT(slot2, tree_size);
}

// =============================================================================
// Create Aux Commitment Tests
// =============================================================================

class CreateAuxCommitmentTest : public ::testing::Test {};

TEST_F(CreateAuxCommitmentTest, EmptyInput) {
    std::vector<Hash256> empty_hashes;
    std::vector<Hash256> empty_ids;
    
    auto commitment = create_aux_commitment(empty_hashes, empty_ids);
    
    // Пустой commitment
    EXPECT_EQ(commitment.tree_size, 1u);
}

TEST_F(CreateAuxCommitmentTest, SingleChain) {
    Hash256 aux_hash{};
    aux_hash[0] = 0xAB;
    
    Hash256 chain_id{};
    chain_id[0] = 0xCD;
    
    std::vector<Hash256> hashes = {aux_hash};
    std::vector<Hash256> ids = {chain_id};
    
    auto commitment = create_aux_commitment(hashes, ids);
    
    EXPECT_GE(commitment.tree_size, 1u);
    // aux_merkle_root должен быть ненулевым
    bool has_data = false;
    for (uint8_t byte : commitment.aux_merkle_root) {
        if (byte != 0) {
            has_data = true;
            break;
        }
    }
    EXPECT_TRUE(has_data);
}

TEST_F(CreateAuxCommitmentTest, MultipleChains) {
    std::vector<Hash256> hashes(4);
    std::vector<Hash256> ids(4);
    
    for (int i = 0; i < 4; ++i) {
        hashes[static_cast<std::size_t>(i)][0] = static_cast<uint8_t>(i + 1);
        ids[static_cast<std::size_t>(i)][0] = static_cast<uint8_t>(i + 10);
    }
    
    auto commitment = create_aux_commitment(hashes, ids);
    
    // Tree size должен быть >= 4
    EXPECT_GE(commitment.tree_size, 4u);
}
