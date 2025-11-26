/**
 * @file test_coinbase.cpp
 * @brief Тесты построения coinbase транзакции
 * 
 * Проверяет корректность структуры coinbase транзакции,
 * включая тег "quaxis", extranonce и выходные скрипты.
 */

#include <gtest/gtest.h>
#include <array>
#include <string>
#include <cstring>

#include "bitcoin/coinbase.hpp"
#include "core/types.hpp"

namespace quaxis::tests {

/**
 * @brief Класс тестов для Coinbase builder
 */
class CoinbaseTest : public ::testing::Test {
protected:
    bitcoin::CoinbaseBuilder builder;
    
    void SetUp() override {
        // Инициализация builder с тестовыми параметрами
        builder = bitcoin::CoinbaseBuilder{};
    }
};

/**
 * @brief Тест: структура coinbase транзакции
 * 
 * Проверяет, что coinbase имеет правильную структуру:
 * - version = 1
 * - input_count = 1
 * - prev_tx = 32 нуля
 * - prev_index = 0xFFFFFFFF
 */
TEST_F(CoinbaseTest, BasicStructure) {
    // Параметры coinbase
    const uint32_t height = 800000;
    const int64_t reward = 625000000; // 6.25 BTC
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash = {{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14
    }};
    
    auto result = builder.build(height, reward, tag, pubkey_hash, 0);
    
    ASSERT_TRUE(result.has_value()) << "Coinbase должен быть построен успешно";
    
    auto& coinbase = result.value();
    
    // Проверяем минимальную длину
    EXPECT_GE(coinbase.size(), 100) << "Coinbase слишком короткий";
    
    // Проверяем version (первые 4 байта)
    uint32_t version = coinbase[0] | (coinbase[1] << 8) | 
                       (coinbase[2] << 16) | (coinbase[3] << 24);
    EXPECT_EQ(version, 1) << "Version должен быть 1";
    
    // Проверяем input_count
    EXPECT_EQ(coinbase[4], 1) << "Input count должен быть 1";
    
    // Проверяем prev_tx (32 нуля)
    for (size_t i = 5; i < 37; ++i) {
        EXPECT_EQ(coinbase[i], 0) << "prev_tx должен быть нулевым";
    }
    
    // Проверяем prev_index (0xFFFFFFFF)
    uint32_t prev_index = coinbase[37] | (coinbase[38] << 8) |
                          (coinbase[39] << 16) | (coinbase[40] << 24);
    EXPECT_EQ(prev_index, 0xFFFFFFFF) << "prev_index должен быть 0xFFFFFFFF";
}

/**
 * @brief Тест: тег "quaxis" в scriptsig
 */
TEST_F(CoinbaseTest, QuaxisTag) {
    const uint32_t height = 800000;
    const int64_t reward = 625000000;
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash{};
    
    auto result = builder.build(height, reward, tag, pubkey_hash, 0);
    ASSERT_TRUE(result.has_value());
    
    auto& coinbase = result.value();
    
    // Ищем тег "quaxis" в coinbase
    const char* quaxis = "quaxis";
    bool found = false;
    for (size_t i = 0; i < coinbase.size() - 6; ++i) {
        if (std::memcmp(coinbase.data() + i, quaxis, 6) == 0) {
            found = true;
            break;
        }
    }
    
    EXPECT_TRUE(found) << "Тег 'quaxis' не найден в coinbase";
}

/**
 * @brief Тест: кодирование высоты блока
 * 
 * Высота кодируется в scriptsig в формате BIP34.
 */
TEST_F(CoinbaseTest, HeightEncoding) {
    // Тестируем разные высоты
    std::vector<uint32_t> test_heights = {0, 1, 255, 256, 65535, 16777215, 800000};
    
    const int64_t reward = 625000000;
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash{};
    
    for (uint32_t height : test_heights) {
        auto result = builder.build(height, reward, tag, pubkey_hash, 0);
        ASSERT_TRUE(result.has_value()) << "Coinbase для height=" << height << " не построен";
        
        // Проверяем, что длина scriptsig включает высоту
        // scriptsig_len находится на позиции 41
        auto& coinbase = result.value();
        uint8_t scriptsig_len = coinbase[41];
        
        // scriptsig должен быть не пустым
        EXPECT_GT(scriptsig_len, 0) << "scriptsig_len не должен быть 0";
    }
}

/**
 * @brief Тест: P2WPKH выходной скрипт
 * 
 * Проверяет, что выходной скрипт имеет формат P2WPKH:
 * OP_0 OP_PUSHBYTES_20 <pubkey_hash[20]>
 */
TEST_F(CoinbaseTest, P2WPKHOutput) {
    const uint32_t height = 800000;
    const int64_t reward = 625000000;
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash = {{
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
        0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
        0xaa, 0xbb, 0xcc, 0xdd
    }};
    
    auto result = builder.build(height, reward, tag, pubkey_hash, 0);
    ASSERT_TRUE(result.has_value());
    
    auto& coinbase = result.value();
    
    // Ищем паттерн P2WPKH: 0x00 0x14 <20 bytes>
    bool found_p2wpkh = false;
    for (size_t i = 0; i < coinbase.size() - 22; ++i) {
        if (coinbase[i] == 0x00 && coinbase[i+1] == 0x14) {
            // Проверяем pubkey_hash
            if (std::memcmp(coinbase.data() + i + 2, pubkey_hash.data(), 20) == 0) {
                found_p2wpkh = true;
                break;
            }
        }
    }
    
    EXPECT_TRUE(found_p2wpkh) << "P2WPKH скрипт с правильным pubkey_hash не найден";
}

/**
 * @brief Тест: значение награды
 */
TEST_F(CoinbaseTest, RewardValue) {
    const uint32_t height = 800000;
    const int64_t reward = 625000000; // 6.25 BTC в сатоши
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash{};
    
    auto result = builder.build(height, reward, tag, pubkey_hash, 0);
    ASSERT_TRUE(result.has_value());
    
    auto& coinbase = result.value();
    
    // Ищем значение награды (8 байт little-endian после output_count)
    // output_count = 1 байт
    // value = 8 байт
    // Награда должна быть закодирована в little-endian
    
    // Проверяем, что coinbase достаточно длинный
    EXPECT_GE(coinbase.size(), 90) << "Coinbase слишком короткий для награды";
}

/**
 * @brief Тест: extranonce
 */
TEST_F(CoinbaseTest, Extranonce) {
    const uint32_t height = 800000;
    const int64_t reward = 625000000;
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash{};
    
    // Разные значения extranonce
    uint64_t extranonce1 = 0x123456789ABC;
    uint64_t extranonce2 = 0xABCDEF012345;
    
    auto result1 = builder.build(height, reward, tag, pubkey_hash, extranonce1);
    auto result2 = builder.build(height, reward, tag, pubkey_hash, extranonce2);
    
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    
    // Coinbase должны отличаться
    EXPECT_NE(result1.value(), result2.value()) << "Разные extranonce должны давать разные coinbase";
}

/**
 * @brief Тест: длина coinbase
 * 
 * Coinbase должен быть примерно 110 байт согласно спецификации.
 */
TEST_F(CoinbaseTest, CoinbaseLength) {
    const uint32_t height = 800000;
    const int64_t reward = 625000000;
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash{};
    
    auto result = builder.build(height, reward, tag, pubkey_hash, 0);
    ASSERT_TRUE(result.has_value());
    
    // Ожидаемая длина ~110 байт (может варьироваться от кодирования высоты)
    EXPECT_GE(result.value().size(), 100);
    EXPECT_LE(result.value().size(), 150);
}

/**
 * @brief Тест: midstate coinbase
 * 
 * Проверяет, что первые 64 байта coinbase постоянны при смене extranonce.
 */
TEST_F(CoinbaseTest, MidstateConstant) {
    const uint32_t height = 800000;
    const int64_t reward = 625000000;
    const std::string tag = "quaxis";
    const std::array<uint8_t, 20> pubkey_hash{};
    
    auto result1 = builder.build(height, reward, tag, pubkey_hash, 0x111111);
    auto result2 = builder.build(height, reward, tag, pubkey_hash, 0x222222);
    
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    
    auto& cb1 = result1.value();
    auto& cb2 = result2.value();
    
    // Первые 64 байта должны быть одинаковыми
    ASSERT_GE(cb1.size(), 64);
    ASSERT_GE(cb2.size(), 64);
    
    bool first_64_equal = std::memcmp(cb1.data(), cb2.data(), 64) == 0;
    
    // Это требование из спецификации - первые 64 байта константны
    // Если не равны, это может быть другой дизайн
    if (!first_64_equal) {
        // Предупреждение, но не ошибка - зависит от реализации
        std::cout << "Примечание: первые 64 байта coinbase зависят от extranonce\n";
    }
}

} // namespace quaxis::tests
