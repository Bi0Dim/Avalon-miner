/**
 * @file test_status_reporter.cpp
 * @brief Тесты для StatusReporter
 */

#include <gtest/gtest.h>

#include "log/status_reporter.hpp"

namespace quaxis::tests {

// =============================================================================
// Тесты конфигурации
// =============================================================================

class LoggingConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

/**
 * @brief Тест: значения по умолчанию
 */
TEST_F(LoggingConfigTest, DefaultValues) {
    log::LoggingConfig config;
    
    EXPECT_EQ(config.refresh_interval_ms, 1000);
    EXPECT_EQ(config.level, "info");
    EXPECT_EQ(config.event_history, 200);
    EXPECT_TRUE(config.color);
    EXPECT_TRUE(config.show_hashrate);
    EXPECT_TRUE(config.highlight_found_blocks);
    EXPECT_TRUE(config.show_chain_block_counts);
    EXPECT_DOUBLE_EQ(config.rated_ths, 90.0);
}

// =============================================================================
// Тесты типов событий
// =============================================================================

TEST(EventTypeTest, ToString) {
    EXPECT_EQ(log::to_string(log::EventType::NEW_BLOCK), "NEW_BLOCK");
    EXPECT_EQ(log::to_string(log::EventType::AUX_BLOCK_FOUND), "AUX_FOUND");
    EXPECT_EQ(log::to_string(log::EventType::BTC_BLOCK_FOUND), "BTC_FOUND");
    EXPECT_EQ(log::to_string(log::EventType::FALLBACK_ENTER), "FB_ENTER");
    EXPECT_EQ(log::to_string(log::EventType::FALLBACK_EXIT), "FB_EXIT");
    EXPECT_EQ(log::to_string(log::EventType::SUBMIT_OK), "SUBMIT_OK");
    EXPECT_EQ(log::to_string(log::EventType::SUBMIT_FAIL), "SUBMIT_FAIL");
    EXPECT_EQ(log::to_string(log::EventType::ERROR), "ERROR");
}

// =============================================================================
// Тесты StatusReporter
// =============================================================================

class StatusReporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.color = false;  // Отключаем цвета для тестирования
        config_.refresh_interval_ms = 100;
    }
    
    log::LoggingConfig config_;
};

/**
 * @brief Тест: создание репортёра
 */
TEST_F(StatusReporterTest, Create) {
    log::StatusReporter reporter(config_);
    
    EXPECT_FALSE(reporter.is_running());
}

/**
 * @brief Тест: запуск и остановка
 */
TEST_F(StatusReporterTest, StartStop) {
    log::StatusReporter reporter(config_);
    
    reporter.start();
    EXPECT_TRUE(reporter.is_running());
    
    reporter.stop();
    EXPECT_FALSE(reporter.is_running());
}

/**
 * @brief Тест: обновление статистики Bitcoin
 */
TEST_F(StatusReporterTest, UpdateBitcoinStats) {
    log::StatusReporter reporter(config_);
    
    log::BitcoinStats stats;
    stats.height = 800000;
    stats.tip_age_seconds = 120;
    stats.connected = true;
    
    reporter.update_bitcoin_stats(stats);
    
    std::string output = reporter.render_plain();
    EXPECT_TRUE(output.find("800000") != std::string::npos);
    EXPECT_TRUE(output.find("CONNECTED") != std::string::npos);
}

/**
 * @brief Тест: обновление статистики ASIC
 */
TEST_F(StatusReporterTest, UpdateAsicStats) {
    log::StatusReporter reporter(config_);
    
    log::AsicStats stats;
    stats.connected_count = 3;
    stats.estimated_hashrate_ths = 90.5;
    
    reporter.update_asic_stats(stats);
    
    std::string output = reporter.render_plain();
    EXPECT_TRUE(output.find("3") != std::string::npos);  // connected count
    EXPECT_TRUE(output.find("90.5") != std::string::npos);  // hashrate
}

/**
 * @brief Тест: обновление активных chains
 */
TEST_F(StatusReporterTest, UpdateActiveChains) {
    log::StatusReporter reporter(config_);
    
    std::vector<std::string> chains = {"namecoin", "syscoin", "rsk"};
    reporter.update_active_chains(chains);
    
    std::string output = reporter.render_plain();
    EXPECT_TRUE(output.find("namecoin") != std::string::npos);
    EXPECT_TRUE(output.find("syscoin") != std::string::npos);
    EXPECT_TRUE(output.find("rsk") != std::string::npos);
}

/**
 * @brief Тест: логирование событий
 */
TEST_F(StatusReporterTest, LogEvents) {
    log::StatusReporter reporter(config_);
    
    reporter.log_new_block(800000);
    reporter.log_aux_block_found("namecoin", 600000);
    reporter.log_btc_block_found(800001);
    
    std::string output = reporter.render_plain();
    EXPECT_TRUE(output.find("NEW_BLOCK") != std::string::npos);
    EXPECT_TRUE(output.find("AUX_FOUND") != std::string::npos);
    EXPECT_TRUE(output.find("BTC_FOUND") != std::string::npos);
}

/**
 * @brief Тест: fallback mode display
 */
TEST_F(StatusReporterTest, FallbackMode) {
    log::StatusReporter reporter(config_);
    
    reporter.update_fallback_mode(fallback::FallbackMode::PrimarySHM);
    std::string output1 = reporter.render_plain();
    EXPECT_TRUE(output1.find("SHM") != std::string::npos);
    
    reporter.update_fallback_mode(fallback::FallbackMode::FallbackZMQ);
    std::string output2 = reporter.render_plain();
    EXPECT_TRUE(output2.find("ZMQ") != std::string::npos);
    
    reporter.update_fallback_mode(fallback::FallbackMode::FallbackStratum);
    std::string output3 = reporter.render_plain();
    EXPECT_TRUE(output3.find("Stratum") != std::string::npos);
}

/**
 * @brief Тест: счётчики блоков
 */
TEST_F(StatusReporterTest, BlockCounts) {
    log::StatusReporter reporter(config_);
    
    std::vector<std::string> chains = {"namecoin", "syscoin"};
    reporter.update_active_chains(chains);
    
    reporter.update_block_count("namecoin", 5);
    reporter.update_block_count("syscoin", 3);
    
    std::string output = reporter.render_plain();
    EXPECT_TRUE(output.find("5 blocks") != std::string::npos);
    EXPECT_TRUE(output.find("3 blocks") != std::string::npos);
}

/**
 * @brief Тест: ограничение истории событий
 */
TEST_F(StatusReporterTest, EventHistoryLimit) {
    log::LoggingConfig config;
    config.event_history = 5;
    config.color = false;
    
    log::StatusReporter reporter(config);
    
    // Добавляем больше событий чем лимит
    for (int i = 0; i < 10; ++i) {
        reporter.log_new_block(static_cast<uint32_t>(800000 + i));
    }
    
    // Вывод должен содержать только последние события
    std::string output = reporter.render_plain();
    EXPECT_TRUE(output.find("800009") != std::string::npos);  // Последний блок
}

/**
 * @brief Тест: рендер содержит основные секции
 */
TEST_F(StatusReporterTest, RenderContainsSections) {
    log::StatusReporter reporter(config_);
    
    std::string output = reporter.render_plain();
    
    EXPECT_TRUE(output.find("QUAXIS SOLO MINER") != std::string::npos);
    EXPECT_TRUE(output.find("Uptime:") != std::string::npos);
    EXPECT_TRUE(output.find("Bitcoin:") != std::string::npos);
    EXPECT_TRUE(output.find("ASIC:") != std::string::npos);
    EXPECT_TRUE(output.find("Source:") != std::string::npos);
    EXPECT_TRUE(output.find("Merged Mining Chains:") != std::string::npos);
    EXPECT_TRUE(output.find("Recent Events:") != std::string::npos);
}

} // namespace quaxis::tests
