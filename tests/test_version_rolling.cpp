/**
 * @file test_version_rolling.cpp
 * @brief Тесты Version Rolling (AsicBoost)
 */

#include <gtest/gtest.h>
#include <array>

#include "mining/version_rolling.hpp"

namespace quaxis::tests {

class VersionRollingTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enabled = true;
        config_.version_mask = mining::VERSION_ROLLING_MASK_DEFAULT;
        config_.version_base = mining::VERSION_BASE;
    }
    
    mining::VersionRollingConfig config_;
};

/**
 * @brief Тест: применение rolling к версии
 */
TEST_F(VersionRollingTest, ApplyRolling) {
    mining::VersionRollingManager manager(config_);
    
    // Rolling = 0 должен дать базовую версию
    uint32_t version0 = manager.apply_rolling(0);
    EXPECT_EQ(version0, config_.version_base);
    
    // Rolling = 1 должен сдвинуть на позицию 13
    uint32_t version1 = manager.apply_rolling(1);
    EXPECT_EQ(version1, config_.version_base | (1 << 13));
    
    // Rolling = 0xFFFF (максимум)
    uint32_t version_max = manager.apply_rolling(0xFFFF);
    EXPECT_EQ(version_max & config_.version_mask, config_.version_mask);
}

/**
 * @brief Тест: извлечение rolling из версии
 */
TEST_F(VersionRollingTest, ExtractRolling) {
    mining::VersionRollingManager manager(config_);
    
    // Применяем и извлекаем
    for (uint16_t i = 0; i < 100; ++i) {
        uint32_t version = manager.apply_rolling(i);
        uint16_t extracted = manager.extract_rolling(version);
        EXPECT_EQ(extracted, i);
    }
}

/**
 * @brief Тест: валидация версии
 */
TEST_F(VersionRollingTest, ValidateVersion) {
    mining::VersionRollingManager manager(config_);
    
    // Валидные версии
    for (uint16_t i = 0; i < 100; ++i) {
        uint32_t version = manager.apply_rolling(i);
        EXPECT_TRUE(manager.validate_version(version));
    }
    
    // Невалидная версия (изменены BIP9 биты)
    uint32_t invalid = manager.apply_rolling(42) | 0x01;
    EXPECT_FALSE(manager.validate_version(invalid));
}

/**
 * @brief Тест: счётчик rolling
 */
TEST_F(VersionRollingTest, RollingCounter) {
    mining::VersionRollingManager manager(config_);
    
    // Первое значение должно быть 0
    EXPECT_EQ(manager.next_rolling_value(), 0);
    EXPECT_EQ(manager.next_rolling_value(), 1);
    EXPECT_EQ(manager.next_rolling_value(), 2);
    
    // Сброс
    manager.reset_rolling_counter();
    EXPECT_EQ(manager.next_rolling_value(), 0);
}

/**
 * @brief Тест: сериализация MiningJobV2
 */
TEST_F(VersionRollingTest, JobV2Serialization) {
    mining::MiningJobV2 job;
    std::fill(job.midstate.begin(), job.midstate.end(), 0xAB);
    std::fill(job.header_tail.begin(), job.header_tail.end(), 0xCD);
    job.job_id = 0x12345678;
    job.version_base = 0x20000000;
    job.version_mask = 0xFFFF;
    job.reserved = 0;
    
    auto serialized = job.serialize();
    EXPECT_EQ(serialized.size(), 56);
    
    // Десериализуем
    auto result = mining::MiningJobV2::deserialize(ByteSpan{serialized.data(), serialized.size()});
    ASSERT_TRUE(result.has_value());
    
    auto& deserialized = result.value();
    EXPECT_EQ(deserialized.midstate, job.midstate);
    EXPECT_EQ(deserialized.header_tail, job.header_tail);
    EXPECT_EQ(deserialized.job_id, job.job_id);
    EXPECT_EQ(deserialized.version_base, job.version_base);
    EXPECT_EQ(deserialized.version_mask, job.version_mask);
}

/**
 * @brief Тест: сериализация MiningShareV2
 */
TEST_F(VersionRollingTest, ShareV2Serialization) {
    mining::MiningShareV2 share;
    share.job_id = 0xDEADBEEF;
    share.nonce = 0xCAFEBABE;
    share.version = 0x20001000;
    
    auto serialized = share.serialize();
    EXPECT_EQ(serialized.size(), 12);
    
    // Десериализуем
    auto result = mining::MiningShareV2::deserialize(ByteSpan{serialized.data(), serialized.size()});
    ASSERT_TRUE(result.has_value());
    
    auto& deserialized = result.value();
    EXPECT_EQ(deserialized.job_id, share.job_id);
    EXPECT_EQ(deserialized.nonce, share.nonce);
    EXPECT_EQ(deserialized.version, share.version);
}

/**
 * @brief Тест: маска по умолчанию
 */
TEST_F(VersionRollingTest, DefaultMask) {
    // Проверяем, что маска захватывает биты 13-28
    uint32_t mask = mining::VERSION_ROLLING_MASK_DEFAULT;
    
    // Биты 0-12 должны быть нулями
    EXPECT_EQ(mask & 0x1FFF, 0);
    
    // Биты 13-28 должны быть единицами (16 бит = 0xFFFF)
    EXPECT_EQ((mask >> 13) & 0xFFFF, 0xFFFF);
    
    // Биты 29-31 должны быть нулями
    EXPECT_EQ((mask >> 29) & 0x7, 0);
}

} // namespace quaxis::tests
