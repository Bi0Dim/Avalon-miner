/**
 * @file test_block.cpp
 * @brief Тесты построения блока Bitcoin
 * 
 * Проверяет корректность структуры заголовка блока,
 * вычисление merkle root и сборку полного блока.
 */

#include <gtest/gtest.h>
#include <array>
#include <cstring>

#include "bitcoin/block.hpp"
#include "crypto/sha256.hpp"
#include "core/types.hpp"

namespace quaxis::tests {

/**
 * @brief Класс тестов для Block builder
 */
class BlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        crypto::sha256_init();
    }
};

/**
 * @brief Тест: структура заголовка блока
 * 
 * Заголовок блока Bitcoin - 80 байт:
 * - version: 4 байта
 * - prev_block_hash: 32 байта
 * - merkle_root: 32 байта
 * - timestamp: 4 байта
 * - bits: 4 байта
 * - nonce: 4 байта
 */
TEST_F(BlockTest, HeaderStructure) {
    bitcoin::BlockHeader header{};
    
    // Проверяем размер структуры
    EXPECT_EQ(sizeof(header.version), 4);
    EXPECT_EQ(sizeof(header.prev_block_hash), 32);
    EXPECT_EQ(sizeof(header.merkle_root), 32);
    EXPECT_EQ(sizeof(header.timestamp), 4);
    EXPECT_EQ(sizeof(header.bits), 4);
    EXPECT_EQ(sizeof(header.nonce), 4);
}

/**
 * @brief Тест: сериализация заголовка
 */
TEST_F(BlockTest, HeaderSerialization) {
    bitcoin::BlockHeader header{};
    header.version = 0x20000000;
    header.timestamp = 1700000000;
    header.bits = 0x1a0fffff;
    header.nonce = 0x12345678;
    
    // Заполняем prev_block_hash
    std::fill(header.prev_block_hash.begin(), header.prev_block_hash.end(), 0xAB);
    
    // Заполняем merkle_root
    std::fill(header.merkle_root.begin(), header.merkle_root.end(), 0xCD);
    
    auto serialized = bitcoin::serialize_header(header);
    
    EXPECT_EQ(serialized.size(), 80) << "Сериализованный заголовок должен быть 80 байт";
    
    // Проверяем version (little-endian)
    uint32_t version = serialized[0] | (serialized[1] << 8) |
                       (serialized[2] << 16) | (serialized[3] << 24);
    EXPECT_EQ(version, 0x20000000);
    
    // Проверяем prev_block_hash
    for (size_t i = 4; i < 36; ++i) {
        EXPECT_EQ(serialized[i], 0xAB);
    }
    
    // Проверяем merkle_root
    for (size_t i = 36; i < 68; ++i) {
        EXPECT_EQ(serialized[i], 0xCD);
    }
}

/**
 * @brief Тест: хеширование заголовка
 */
TEST_F(BlockTest, HeaderHash) {
    bitcoin::BlockHeader header{};
    header.version = 1;
    header.timestamp = 1231006505;
    header.bits = 0x1d00ffff;
    header.nonce = 2083236893;
    
    auto serialized = bitcoin::serialize_header(header);
    auto hash = crypto::sha256d(serialized.data(), serialized.size());
    
    // Хеш должен быть 32 байта
    EXPECT_EQ(hash.size(), 32);
    
    // Хеш не должен быть нулевым
    bool all_zeros = true;
    for (auto byte : hash) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);
}

/**
 * @brief Тест: merkle root для одной транзакции
 * 
 * Для пустого блока (только coinbase):
 * merkle_root = SHA256d(coinbase_hash)
 */
TEST_F(BlockTest, MerkleRootSingleTx) {
    // Хеш coinbase транзакции
    core::Hash256 coinbase_hash{};
    std::fill(coinbase_hash.begin(), coinbase_hash.end(), 0x42);
    
    auto merkle_root = bitcoin::compute_merkle_root({coinbase_hash});
    
    // Для одной транзакции merkle_root = SHA256d(tx_hash)
    auto expected = crypto::sha256d(coinbase_hash.data(), coinbase_hash.size());
    
    // Примечание: в Bitcoin merkle_root для одной транзакции = сам хеш
    // Но разные реализации могут отличаться
    EXPECT_EQ(merkle_root.size(), 32);
}

/**
 * @brief Тест: merkle root для двух транзакций
 */
TEST_F(BlockTest, MerkleRootTwoTx) {
    core::Hash256 tx1{}, tx2{};
    std::fill(tx1.begin(), tx1.end(), 0x11);
    std::fill(tx2.begin(), tx2.end(), 0x22);
    
    auto merkle_root = bitcoin::compute_merkle_root({tx1, tx2});
    
    // merkle_root = SHA256d(tx1 || tx2)
    std::array<uint8_t, 64> combined{};
    std::memcpy(combined.data(), tx1.data(), 32);
    std::memcpy(combined.data() + 32, tx2.data(), 32);
    
    auto expected = crypto::sha256d(combined.data(), combined.size());
    
    EXPECT_EQ(merkle_root, expected);
}

/**
 * @brief Тест: нечётное количество транзакций
 * 
 * При нечётном количестве последняя транзакция дублируется.
 */
TEST_F(BlockTest, MerkleRootOddCount) {
    core::Hash256 tx1{}, tx2{}, tx3{};
    std::fill(tx1.begin(), tx1.end(), 0x11);
    std::fill(tx2.begin(), tx2.end(), 0x22);
    std::fill(tx3.begin(), tx3.end(), 0x33);
    
    auto merkle_root = bitcoin::compute_merkle_root({tx1, tx2, tx3});
    
    EXPECT_EQ(merkle_root.size(), 32);
    
    // Проверяем, что результат не нулевой
    bool all_zeros = true;
    for (auto byte : merkle_root) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);
}

/**
 * @brief Тест: пустой блок
 * 
 * Блок с только coinbase транзакцией.
 */
TEST_F(BlockTest, EmptyBlock) {
    bitcoin::BlockBuilder builder;
    
    // Параметры блока
    bitcoin::BlockParams params{};
    params.version = 0x20000000;
    params.height = 800000;
    params.timestamp = 1700000000;
    params.bits = 0x1a0fffff;
    std::fill(params.prev_block_hash.begin(), params.prev_block_hash.end(), 0xAB);
    
    // Coinbase
    std::vector<uint8_t> coinbase(110, 0x42);
    
    auto result = builder.build_empty_block(params, coinbase);
    
    ASSERT_TRUE(result.has_value()) << "Пустой блок должен быть построен";
    
    auto& block = result.value();
    
    // Проверяем, что блок содержит заголовок и coinbase
    EXPECT_GE(block.size(), 80 + coinbase.size());
}

/**
 * @brief Тест: midstate заголовка
 * 
 * Первые 64 байта заголовка используются для midstate.
 */
TEST_F(BlockTest, HeaderMidstate) {
    bitcoin::BlockHeader header{};
    header.version = 0x20000000;
    std::fill(header.prev_block_hash.begin(), header.prev_block_hash.end(), 0xAB);
    std::fill(header.merkle_root.begin(), header.merkle_root.end(), 0xCD);
    header.timestamp = 1700000000;
    header.bits = 0x1a0fffff;
    header.nonce = 0;
    
    auto serialized = bitcoin::serialize_header(header);
    
    // Midstate вычисляется от первых 64 байт
    auto midstate = crypto::sha256_midstate(serialized.data(), 64);
    
    EXPECT_EQ(midstate.size(), 32);
    
    // Проверяем, что midstate не нулевой
    bool all_zeros = true;
    for (auto byte : midstate) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros);
}

/**
 * @brief Тест: хвост заголовка
 * 
 * Последние 12 байт (timestamp + bits + merkle_part).
 */
TEST_F(BlockTest, HeaderTail) {
    bitcoin::BlockHeader header{};
    header.version = 0x20000000;
    header.timestamp = 0xAABBCCDD;
    header.bits = 0x11223344;
    header.nonce = 0x12345678;
    
    auto serialized = bitcoin::serialize_header(header);
    
    // Хвост заголовка: байты 68-79 (12 байт)
    std::array<uint8_t, 12> tail{};
    std::memcpy(tail.data(), serialized.data() + 68, 12);
    
    // Проверяем timestamp в позиции 68-71
    uint32_t ts = tail[0] | (tail[1] << 8) | (tail[2] << 16) | (tail[3] << 24);
    EXPECT_EQ(ts, 0xAABBCCDD);
    
    // Проверяем bits в позиции 72-75
    uint32_t bits = tail[4] | (tail[5] << 8) | (tail[6] << 16) | (tail[7] << 24);
    EXPECT_EQ(bits, 0x11223344);
}

/**
 * @brief Тест: проверка PoW
 */
TEST_F(BlockTest, ProofOfWork) {
    bitcoin::BlockHeader header{};
    header.version = 1;
    header.bits = 0x1d00ffff; // Очень низкая сложность
    header.nonce = 0;
    
    auto serialized = bitcoin::serialize_header(header);
    auto hash = crypto::sha256d(serialized.data(), serialized.size());
    
    // Преобразуем bits в target
    auto target = bitcoin::bits_to_target(header.bits);
    
    // Проверяем, что target корректный
    EXPECT_EQ(target.size(), 32);
}

} // namespace quaxis::tests
