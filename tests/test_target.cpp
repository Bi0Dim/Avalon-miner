/**
 * @file test_target.cpp
 * @brief Тесты работы с difficulty и target
 * 
 * Проверяет преобразование bits в target,
 * вычисление difficulty и сравнение хешей с target.
 */

#include <gtest/gtest.h>
#include <array>
#include <cstring>

#include "bitcoin/target.hpp"
#include "core/types.hpp"

namespace quaxis::tests {

/**
 * @brief Класс тестов для Target
 */
class TargetTest : public ::testing::Test {
};

/**
 * @brief Тест: преобразование bits в target
 * 
 * Формат compact target (bits):
 * - Первый байт: показатель степени
 * - Следующие 3 байта: мантисса
 * 
 * target = mantissa * 256^(exponent - 3)
 */
TEST_F(TargetTest, BitsToTarget) {
    // bits = 0x1d00ffff (Genesis block difficulty)
    uint32_t bits = 0x1d00ffff;
    
    auto target = bitcoin::bits_to_target(bits);
    
    EXPECT_EQ(target.size(), 32);
    
    // Для 0x1d00ffff:
    // exponent = 0x1d = 29
    // mantissa = 0x00ffff
    // target = 0x00ffff * 256^(29-3) = 0x00ffff * 256^26
    // Это очень большое число, первые ненулевые байты начинаются с позиции ~3
    
    // Проверяем, что target не нулевой
    bool all_zeros = true;
    for (auto byte : target) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros) << "Target не должен быть нулевым";
}

/**
 * @brief Тест: минимальный target (максимальная сложность)
 */
TEST_F(TargetTest, MinimalTarget) {
    // Очень высокая сложность
    uint32_t bits = 0x17034b33; // Примерно 50 триллионов difficulty
    
    auto target = bitcoin::bits_to_target(bits);
    
    EXPECT_EQ(target.size(), 32);
    
    // Первые несколько байт должны быть нулями
    EXPECT_EQ(target[0], 0);
    EXPECT_EQ(target[1], 0);
}

/**
 * @brief Тест: сравнение хеша с target
 */
TEST_F(TargetTest, HashComparison) {
    // Простой target: все единицы (максимально лёгкий)
    Hash256 easy_target{};
    std::fill(easy_target.begin(), easy_target.end(), 0xFF);
    
    // Хеш с ведущими нулями
    Hash256 good_hash{};
    good_hash[0] = 0x00;
    good_hash[1] = 0x00;
    good_hash[2] = 0x00;
    good_hash[3] = 0x00;
    std::fill(good_hash.begin() + 4, good_hash.end(), 0xAB);
    
    // Проверяем, что good_hash <= easy_target
    EXPECT_TRUE(bitcoin::meets_target(good_hash, easy_target));
    
    // Хеш без ведущих нулей
    Hash256 bad_hash{};
    std::fill(bad_hash.begin(), bad_hash.end(), 0xFF);
    
    // Строгий target
    Hash256 strict_target{};
    strict_target[0] = 0x00;
    strict_target[1] = 0x00;
    strict_target[2] = 0x00;
    strict_target[3] = 0x01;
    
    EXPECT_FALSE(bitcoin::meets_target(bad_hash, strict_target));
}

/**
 * @brief Тест: преобразование target в bits
 */
TEST_F(TargetTest, TargetToBits) {
    // Исходные bits
    uint32_t original_bits = 0x1d00ffff;
    
    // Преобразуем в target
    auto target = bitcoin::bits_to_target(original_bits);
    
    // Преобразуем обратно в bits
    auto recovered_bits = bitcoin::target_to_bits(target);
    
    // Должны получить исходное значение (или эквивалентное)
    EXPECT_EQ(recovered_bits, original_bits);
}

/**
 * @brief Тест: вычисление difficulty
 * 
 * difficulty = max_target / current_target
 * где max_target = target для bits=0x1d00ffff
 */
TEST_F(TargetTest, DifficultyCalculation) {
    // Genesis block bits
    uint32_t genesis_bits = 0x1d00ffff;
    
    double difficulty = bitcoin::bits_to_difficulty(genesis_bits);
    
    // Difficulty для genesis block = 1.0
    EXPECT_NEAR(difficulty, 1.0, 0.001);
    
    // Более высокая сложность
    uint32_t hard_bits = 0x1a0fffff;
    double hard_difficulty = bitcoin::bits_to_difficulty(hard_bits);
    
    // Должна быть > 1
    EXPECT_GT(hard_difficulty, 1.0);
}

/**
 * @brief Тест: граничные случаи bits
 */
TEST_F(TargetTest, EdgeCases) {
    // Минимальный экспонент
    uint32_t min_exp_bits = 0x01010000;
    auto target1 = bitcoin::bits_to_target(min_exp_bits);
    EXPECT_EQ(target1.size(), 32);
    
    // Максимальный экспонент (ограничено 32 байтами)
    uint32_t max_exp_bits = 0x20010000;
    auto target2 = bitcoin::bits_to_target(max_exp_bits);
    EXPECT_EQ(target2.size(), 32);
    
    // Нулевая мантисса
    uint32_t zero_mantissa = 0x1d000000;
    auto target3 = bitcoin::bits_to_target(zero_mantissa);
    
    // Target должен быть нулевым или минимальным
    bool all_zeros = true;
    for (auto byte : target3) {
        if (byte != 0) {
            all_zeros = false;
            break;
        }
    }
    // Нулевая мантисса = нулевой target
    EXPECT_TRUE(all_zeros);
}

/**
 * @brief Тест: реальные значения difficulty из mainnet
 */
TEST_F(TargetTest, MainnetDifficulty) {
    // Block 800000: bits = 0x1705ae3a, difficulty ≈ 53 trillion
    uint32_t block_800000_bits = 0x1705ae3a;
    
    double difficulty = bitcoin::bits_to_difficulty(block_800000_bits);
    
    // Проверяем порядок величины (50-60 триллионов)
    EXPECT_GT(difficulty, 40e12);
    EXPECT_LT(difficulty, 100e12);
    
    std::cout << "Block 800000 difficulty: " << difficulty << std::endl;
}

/**
 * @brief Тест: проверка PoW
 */
TEST_F(TargetTest, PoWCheck) {
    // Создаём валидный случай
    uint32_t bits = 0x1d00ffff;
    
    // Хеш с множеством ведущих нулей (валидный)
    Hash256 valid_hash{};
    std::fill(valid_hash.begin(), valid_hash.end(), 0x00);
    valid_hash[31] = 0x01; // Только последний байт ненулевой
    
    EXPECT_TRUE(bitcoin::meets_bits(valid_hash, bits));
    
    // Невалидный хеш (слишком большой)
    Hash256 invalid_hash{};
    std::fill(invalid_hash.begin(), invalid_hash.end(), 0xFF);
    
    EXPECT_FALSE(bitcoin::meets_bits(invalid_hash, bits));
}

/**
 * @brief Тест: порядок байт в хеше
 * 
 * Bitcoin использует little-endian для сравнения.
 */
TEST_F(TargetTest, HashEndianness) {
    // В Bitcoin хеши сравниваются как 256-битные числа в little-endian
    // Первый байт массива - младший байт числа
    
    Hash256 hash1{};
    Hash256 hash2{};
    
    // hash1 = 0x0100...00 (1 в младшем байте)
    hash1[0] = 0x01;
    
    // hash2 = 0x0001...00 (1 во втором байте = 256)
    hash2[1] = 0x01;
    
    // В little-endian: hash2 > hash1 (256 > 1)
    // meets_target compares hash <= target
    // hash1 should meet hash2 as target (1 <= 256)
    EXPECT_TRUE(bitcoin::meets_target(hash1, hash2));
    // hash2 should NOT meet hash1 as target (256 > 1)
    EXPECT_FALSE(bitcoin::meets_target(hash2, hash1));
}

} // namespace quaxis::tests
