/**
 * @file test_nonce_distributor.cpp
 * @brief Тесты Nonce Distributor (распределение nonce между чипами)
 */

#include <gtest/gtest.h>

#include "mining/nonce_distributor.hpp"

namespace quaxis::tests {

class NonceDistributorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.chips_per_asic = 114;
        config_.asic_count = 3;
        config_.strategy = mining::NonceStrategy::Sequential;
    }
    
    mining::NonceDistributorConfig config_;
};

/**
 * @brief Тест: общее количество чипов
 */
TEST_F(NonceDistributorTest, TotalChips) {
    mining::NonceDistributor distributor(config_);
    
    EXPECT_EQ(distributor.total_chips(), 114 * 3);
}

/**
 * @brief Тест: получение стратегии
 */
TEST_F(NonceDistributorTest, GetStrategy) {
    mining::NonceDistributor distributor(config_);
    
    EXPECT_EQ(distributor.get_strategy(), mining::NonceStrategy::Sequential);
}

/**
 * @brief Тест: sequential стратегия - диапазоны
 */
TEST_F(NonceDistributorTest, SequentialRanges) {
    mining::NonceDistributor distributor(config_);
    
    // Первый чип должен начинаться с 0
    auto range0 = distributor.get_range(0);
    EXPECT_EQ(range0.start, 0u);
    EXPECT_EQ(range0.chip_id, 0);
    EXPECT_EQ(range0.strategy, mining::NonceStrategy::Sequential);
    
    // Второй чип должен начинаться после первого
    auto range1 = distributor.get_range(1);
    EXPECT_GT(range1.start, range0.end);
    
    // Последний чип должен заканчиваться на 0xFFFFFFFF
    auto last_range = distributor.get_range(static_cast<uint16_t>(distributor.total_chips() - 1));
    EXPECT_EQ(last_range.end, 0xFFFFFFFFu);
}

/**
 * @brief Тест: sequential стратегия - валидация покрытия
 */
TEST_F(NonceDistributorTest, SequentialCoverage) {
    mining::NonceDistributor distributor(config_);
    
    // Всё пространство nonce должно быть покрыто
    EXPECT_TRUE(distributor.validate_coverage());
}

/**
 * @brief Тест: sequential стратегия - нет пересечений
 */
TEST_F(NonceDistributorTest, SequentialNoOverlap) {
    mining::NonceDistributor distributor(config_);
    
    EXPECT_TRUE(distributor.validate_no_overlap());
}

/**
 * @brief Тест: interleaved стратегия
 */
TEST_F(NonceDistributorTest, InterleavedRanges) {
    config_.strategy = mining::NonceStrategy::Interleaved;
    mining::NonceDistributor distributor(config_);
    
    auto range0 = distributor.get_range(0);
    EXPECT_EQ(range0.start, 0u);
    EXPECT_EQ(range0.step, distributor.total_chips());
    
    auto range1 = distributor.get_range(1);
    EXPECT_EQ(range1.start, 1u);
    EXPECT_EQ(range1.step, distributor.total_chips());
}

/**
 * @brief Тест: получение диапазона по ASIC и локальному ID
 */
TEST_F(NonceDistributorTest, GetRangeByAsicAndLocal) {
    mining::NonceDistributor distributor(config_);
    
    // Чип 42 на ASIC 1 = глобальный ID 114 + 42 = 156
    auto range_by_global = distributor.get_range(156);
    auto range_by_local = distributor.get_range(1, 42);
    
    EXPECT_EQ(range_by_global.start, range_by_local.start);
    EXPECT_EQ(range_by_global.end, range_by_local.end);
}

/**
 * @brief Тест: получение всех диапазонов для ASIC
 */
TEST_F(NonceDistributorTest, GetAsicRanges) {
    mining::NonceDistributor distributor(config_);
    
    auto asic_ranges = distributor.get_asic_ranges(0);
    EXPECT_EQ(asic_ranges.size(), config_.chips_per_asic);
    
    // Все диапазоны должны принадлежать ASIC 0
    for (const auto& range : asic_ranges) {
        EXPECT_EQ(range.asic_id, 0);
    }
}

/**
 * @brief Тест: поиск чипа для nonce
 */
TEST_F(NonceDistributorTest, FindChipForNonce) {
    mining::NonceDistributor distributor(config_);
    
    // Nonce 0 должен принадлежать первому чипу
    auto chip0 = distributor.find_chip_for_nonce(0);
    ASSERT_TRUE(chip0.has_value());
    EXPECT_EQ(*chip0, 0);
    
    // Nonce 0xFFFFFFFF должен принадлежать последнему чипу
    auto last_chip = distributor.find_chip_for_nonce(0xFFFFFFFF);
    ASSERT_TRUE(last_chip.has_value());
    EXPECT_EQ(*last_chip, static_cast<uint16_t>(distributor.total_chips() - 1));
}

/**
 * @brief Тест: проверка принадлежности nonce диапазону
 */
TEST_F(NonceDistributorTest, RangeContains) {
    mining::NonceDistributor distributor(config_);
    
    auto range = distributor.get_range(0);
    
    // Начало и конец диапазона должны принадлежать
    EXPECT_TRUE(range.contains(range.start));
    EXPECT_TRUE(range.contains(range.end));
    
    // Середина диапазона должна принадлежать
    uint32_t middle = range.start + (range.end - range.start) / 2;
    EXPECT_TRUE(range.contains(middle));
    
    // За пределами диапазона
    if (range.end < 0xFFFFFFFF) {
        EXPECT_FALSE(range.contains(range.end + 1));
    }
}

/**
 * @brief Тест: размер диапазона
 */
TEST_F(NonceDistributorTest, RangeSize) {
    mining::NonceDistributor distributor(config_);
    
    // Сумма всех размеров должна равняться NONCE_SPACE (2^32)
    uint64_t total_size = 0;
    for (const auto& range : distributor.get_all_ranges()) {
        total_size += range.size();
    }
    
    EXPECT_EQ(total_size, mining::NONCE_SPACE);
}

/**
 * @brief Тест: перестроение с новой конфигурацией
 */
TEST_F(NonceDistributorTest, Rebuild) {
    mining::NonceDistributor distributor(config_);
    
    EXPECT_EQ(distributor.get_strategy(), mining::NonceStrategy::Sequential);
    
    config_.strategy = mining::NonceStrategy::Interleaved;
    distributor.rebuild(config_);
    
    EXPECT_EQ(distributor.get_strategy(), mining::NonceStrategy::Interleaved);
}

/**
 * @brief Тест: преобразование строки в стратегию
 */
TEST_F(NonceDistributorTest, StrategyFromString) {
    EXPECT_EQ(mining::strategy_from_string("sequential"), mining::NonceStrategy::Sequential);
    EXPECT_EQ(mining::strategy_from_string("seq"), mining::NonceStrategy::Sequential);
    EXPECT_EQ(mining::strategy_from_string("interleaved"), mining::NonceStrategy::Interleaved);
    EXPECT_EQ(mining::strategy_from_string("int"), mining::NonceStrategy::Interleaved);
    EXPECT_EQ(mining::strategy_from_string("random"), mining::NonceStrategy::Random);
    EXPECT_EQ(mining::strategy_from_string("rand"), mining::NonceStrategy::Random);
    
    // По умолчанию Sequential
    EXPECT_EQ(mining::strategy_from_string("unknown"), mining::NonceStrategy::Sequential);
}

/**
 * @brief Тест: сериализация диапазона
 */
TEST_F(NonceDistributorTest, RangeSerialization) {
    mining::NonceDistributor distributor(config_);
    
    auto range = distributor.get_range(42);
    auto serialized = mining::serialize_range(range);
    
    EXPECT_EQ(serialized.size(), 8);
    
    auto result = mining::deserialize_range(ByteSpan{serialized.data(), serialized.size()});
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(result->start, range.start);
    EXPECT_EQ(result->end, range.end);
}

/**
 * @brief Тест: следующий nonce в диапазоне
 */
TEST_F(NonceDistributorTest, RangeNext) {
    mining::NonceRange range;
    range.start = 100;
    range.end = 200;
    range.step = 1;
    range.strategy = mining::NonceStrategy::Sequential;
    
    auto next1 = range.next(100);
    ASSERT_TRUE(next1.has_value());
    EXPECT_EQ(*next1, 101);
    
    auto next_last = range.next(200);
    EXPECT_FALSE(next_last.has_value());  // Конец диапазона
}

} // namespace quaxis::tests
