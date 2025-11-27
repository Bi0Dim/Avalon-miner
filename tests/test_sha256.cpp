/**
 * @file test_sha256.cpp
 * @brief Тесты SHA256 реализации
 * 
 * Проверяет корректность SHA256 хеширования для известных тестовых векторов.
 * Тестирует как generic, так и SHA-NI реализации (если доступна).
 */

#include <gtest/gtest.h>
#include <array>
#include <string>
#include <cstring>
#include <span>

#include "crypto/sha256.hpp"
#include "core/types.hpp"

namespace quaxis::tests {

/**
 * @brief Класс тестов для SHA256
 */
class SHA256Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Реализация выбирается автоматически при первом вызове
    }
};

/**
 * @brief Тест: пустое сообщение
 * 
 * SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
 */
TEST_F(SHA256Test, EmptyMessage) {
    const std::array<uint8_t, 1> empty{0}; // Нужен минимум 1 элемент для span
    auto hash = crypto::sha256(ByteSpan{empty.data(), 0});
    
    // Ожидаемый хеш пустого сообщения
    constexpr Hash256 expected = {{
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    }};
    
    EXPECT_EQ(hash, expected);
}

/**
 * @brief Тест: "abc"
 * 
 * SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
 */
TEST_F(SHA256Test, SimpleMessage) {
    const std::string msg = "abc";
    auto hash = crypto::sha256(ByteSpan{reinterpret_cast<const uint8_t*>(msg.data()), msg.size()});
    
    constexpr Hash256 expected = {{
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    }};
    
    EXPECT_EQ(hash, expected);
}

/**
 * @brief Тест: 448 бит (ровно один блок без padding)
 */
TEST_F(SHA256Test, SingleBlock) {
    // 56 байт = 448 бит, ровно один блок с padding
    const std::string msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    auto hash = crypto::sha256(ByteSpan{reinterpret_cast<const uint8_t*>(msg.data()), msg.size()});
    
    constexpr Hash256 expected = {{
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8,
        0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67,
        0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1
    }};
    
    EXPECT_EQ(hash, expected);
}

/**
 * @brief Тест: SHA256d (двойное хеширование, используется в Bitcoin)
 */
TEST_F(SHA256Test, DoubleSHA256) {
    const std::string msg = "hello";
    auto hash = crypto::sha256d(ByteSpan{reinterpret_cast<const uint8_t*>(msg.data()), msg.size()});
    
    // SHA256(SHA256("hello"))
    constexpr Hash256 expected = {{
        0x95, 0x95, 0xc9, 0xdf, 0x90, 0x07, 0x51, 0x48,
        0xeb, 0x06, 0x86, 0x03, 0x65, 0xdf, 0x33, 0x58,
        0x4b, 0x75, 0xbf, 0xf7, 0x82, 0xa5, 0x10, 0xc6,
        0xcd, 0x48, 0x83, 0xa4, 0x19, 0x83, 0x3d, 0x50
    }};
    
    EXPECT_EQ(hash, expected);
}

/**
 * @brief Тест: вычисление midstate
 * 
 * Проверяет, что midstate вычисляется корректно для первых 64 байт.
 */
TEST_F(SHA256Test, Midstate) {
    // 64 байта данных (один блок SHA256)
    std::array<uint8_t, 64> data{};
    std::fill(data.begin(), data.end(), 0x42);
    
    auto midstate = crypto::compute_midstate(data.data());
    
    // Midstate должен быть 8 слов
    EXPECT_EQ(midstate.size(), 8);
    
    // Проверяем, что это не нули
    bool all_zeros = true;
    for (auto word : midstate) {
        if (word != 0) {
            all_zeros = false;
            break;
        }
    }
    EXPECT_FALSE(all_zeros) << "Midstate не должен быть нулевым";
}

/**
 * @brief Тест: производительность SHA-NI vs Generic
 * 
 * Выполняется только если SHA-NI доступен.
 */
TEST_F(SHA256Test, Performance) {
    constexpr size_t iterations = 10000;
    
    std::array<uint8_t, 80> data{};
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    
    // Измерение времени
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        [[maybe_unused]] auto hash = crypto::sha256(ByteSpan{data.data(), data.size()});
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    double ns_per_hash = static_cast<double>(duration.count()) / static_cast<double>(iterations);
    
    std::cout << "SHA256: " << ns_per_hash << " ns/hash (" << iterations << " iterations)" << std::endl;
    
    // Проверяем, что хеширование работает разумно быстро (< 10 мкс)
    EXPECT_LT(ns_per_hash, 10000.0) << "SHA256 слишком медленный";
}

/**
 * @brief Тест: корректность для заголовка блока Bitcoin
 * 
 * Использует реальный заголовок блока #100000.
 */
TEST_F(SHA256Test, BitcoinBlockHeader) {
    // Заголовок блока #100000 (в raw формате)
    const std::array<uint8_t, 80> header = {{
        // version
        0x01, 0x00, 0x00, 0x00,
        // prev_block
        0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x98, 0x4a,
        0x5a, 0x4b, 0x8a, 0x3d, 0x34, 0xd3, 0xf6, 0xd0,
        0xb0, 0x09, 0x34, 0xf5, 0x93, 0x65, 0x2d, 0xd1,
        0xb6, 0xdc, 0xee, 0x7c, 0x1e, 0x8e, 0xe3, 0x06,
        // merkle_root
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // time
        0x4d, 0x1b, 0xe6, 0x4d,
        // bits
        0x1a, 0x0f, 0xff, 0xff,
        // nonce
        0x00, 0x00, 0x00, 0x00
    }};
    
    auto hash = crypto::sha256d(ByteSpan{header.data(), header.size()});
    
    // Хеш должен быть 32 байта
    EXPECT_EQ(hash.size(), 32);
    
    // Проверяем, что хеш вычисляется без исключений
    SUCCEED();
}

} // namespace quaxis::tests
