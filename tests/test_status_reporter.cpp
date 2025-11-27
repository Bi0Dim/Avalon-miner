/**
 * @file test_status_reporter.cpp
 * @brief Тесты для терминального вывода статуса
 */

#include <gtest/gtest.h>

#include "log/status_reporter.hpp"

#include <thread>
#include <chrono>

namespace quaxis::tests {

// =============================================================================
// Тесты конфигурации
// =============================================================================

TEST(StatusReporterConfigTest, DefaultValues) {
    log::StatusReporterConfig config;
    
    EXPECT_EQ(config.level, "info");
    EXPECT_EQ(config.refresh_interval_ms, 1000u);
    EXPECT_EQ(config.event_history, 200u);
    EXPECT_TRUE(config.color);
    EXPECT_TRUE(config.highlight_found_blocks);
    EXPECT_TRUE(config.show_chain_block_counts);
    EXPECT_TRUE(config.show_hashrate);
}

// =============================================================================
// Тесты StatusData
// =============================================================================

TEST(StatusDataTest, DefaultValues) {
    log::StatusData data;
    
    EXPECT_EQ(data.uptime.count(), 0);
    EXPECT_FALSE(data.fallback_active);
    EXPECT_DOUBLE_EQ(data.hashrate_ths, 0.0);
    EXPECT_EQ(data.asic_connections, 0u);
    EXPECT_EQ(data.btc_height, 0u);
    EXPECT_EQ(data.tip_age_ms, 0u);
    EXPECT_EQ(data.job_queue_depth, 0u);
    EXPECT_EQ(data.prepared_templates, 0u);
    EXPECT_TRUE(data.active_chains.empty());
    EXPECT_TRUE(data.found_blocks.empty());
    EXPECT_FALSE(data.adaptive_spin_active);
    EXPECT_DOUBLE_EQ(data.shm_cpu_usage_percent, 0.0);
}

// =============================================================================
// Тесты EventType
// =============================================================================

TEST(EventTypeTest, ToString) {
    EXPECT_EQ(log::event_type_to_string(log::EventType::NewBlock), "NEW_BLOCK");
    EXPECT_EQ(log::event_type_to_string(log::EventType::AuxBlockFound), "AUX_BLOCK_FOUND");
    EXPECT_EQ(log::event_type_to_string(log::EventType::BtcBlockFound), "BTC_BLOCK_FOUND");
    EXPECT_EQ(log::event_type_to_string(log::EventType::FallbackEnter), "FALLBACK_ENTER");
    EXPECT_EQ(log::event_type_to_string(log::EventType::FallbackExit), "FALLBACK_EXIT");
    EXPECT_EQ(log::event_type_to_string(log::EventType::SubmitOk), "SUBMIT_OK");
    EXPECT_EQ(log::event_type_to_string(log::EventType::SubmitFail), "SUBMIT_FAIL");
    EXPECT_EQ(log::event_type_to_string(log::EventType::Error), "ERROR");
}

// =============================================================================
// Тесты StatusReporter
// =============================================================================

class StatusReporterTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.color = false;  // Отключаем цвета для тестов
        config_.refresh_interval_ms = 100;
        config_.event_history = 10;
    }
    
    log::StatusReporterConfig config_;
};

/**
 * @brief Тест: создание репортера
 */
TEST_F(StatusReporterTest, Creation) {
    log::StatusReporter reporter(config_);
    
    EXPECT_FALSE(reporter.is_running());
}

/**
 * @brief Тест: добавление событий
 */
TEST_F(StatusReporterTest, AddEvents) {
    log::StatusReporter reporter(config_);
    
    reporter.add_event(log::EventType::NewBlock, "Block 100", "");
    reporter.add_event(log::EventType::SubmitOk, "Share accepted", "");
    
    auto events = reporter.get_events();
    EXPECT_EQ(events.size(), 2u);
}

/**
 * @brief Тест: ограничение истории событий
 */
TEST_F(StatusReporterTest, EventHistoryLimit) {
    config_.event_history = 5;
    log::StatusReporter reporter(config_);
    
    // Добавляем 10 событий
    for (int i = 0; i < 10; ++i) {
        reporter.add_event(log::EventType::NewBlock, "Block " + std::to_string(i), "");
    }
    
    auto events = reporter.get_events();
    EXPECT_EQ(events.size(), 5u);
    
    // Проверяем что остались последние 5
    EXPECT_NE(events[0].message.find("Block 5"), std::string::npos);
}

/**
 * @brief Тест: получение N последних событий
 */
TEST_F(StatusReporterTest, GetLastNEvents) {
    log::StatusReporter reporter(config_);
    
    for (int i = 0; i < 5; ++i) {
        reporter.add_event(log::EventType::NewBlock, "Block " + std::to_string(i), "");
    }
    
    auto events = reporter.get_events(3);
    EXPECT_EQ(events.size(), 3u);
}

/**
 * @brief Тест: очистка событий
 */
TEST_F(StatusReporterTest, ClearEvents) {
    log::StatusReporter reporter(config_);
    
    reporter.add_event(log::EventType::NewBlock, "Block 1", "");
    reporter.add_event(log::EventType::NewBlock, "Block 2", "");
    
    EXPECT_EQ(reporter.get_events().size(), 2u);
    
    reporter.clear_events();
    
    EXPECT_EQ(reporter.get_events().size(), 0u);
}

/**
 * @brief Тест: рендеринг статуса без цветов
 */
TEST_F(StatusReporterTest, RenderStatusPlain) {
    log::StatusReporter reporter(config_);
    
    log::StatusData data;
    data.uptime = std::chrono::seconds(3661); // 1h 1m 1s
    data.fallback_active = false;
    data.hashrate_ths = 90.5;
    data.asic_connections = 3;
    data.btc_height = 800000;
    data.tip_age_ms = 150;
    data.job_queue_depth = 50;
    data.prepared_templates = 2;
    data.active_chains = {"NMC", "SYS", "ELA"};
    data.found_blocks["NMC"] = 2;
    data.adaptive_spin_active = true;
    data.shm_cpu_usage_percent = 15.5;
    
    std::string output = reporter.render_status_plain(data);
    
    // Проверяем наличие ключевых элементов
    EXPECT_NE(output.find("Uptime"), std::string::npos);
    EXPECT_NE(output.find("Fallback: OFF"), std::string::npos);
    EXPECT_NE(output.find("90.5"), std::string::npos);  // Hashrate
    EXPECT_NE(output.find("ASICs: 3"), std::string::npos);
    EXPECT_NE(output.find("BTC Height: 800000"), std::string::npos);
    EXPECT_NE(output.find("Chains:"), std::string::npos);
    EXPECT_NE(output.find("Adaptive Spin: ON"), std::string::npos);
}

/**
 * @brief Тест: рендеринг события
 */
TEST_F(StatusReporterTest, RenderEvent) {
    log::StatusReporter reporter(config_);
    
    log::Event event;
    event.type = log::EventType::NewBlock;
    event.timestamp = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    event.message = "Height: 800000";
    event.data = "hash123";
    
    std::string output = reporter.render_event(event, false);
    
    EXPECT_NE(output.find("NEW_BLOCK"), std::string::npos);
    EXPECT_NE(output.find("Height: 800000"), std::string::npos);
    EXPECT_NE(output.find("[hash123]"), std::string::npos);
}

/**
 * @brief Тест: запуск и остановка
 */
TEST_F(StatusReporterTest, StartStop) {
    log::StatusReporter reporter(config_);
    
    // Устанавливаем провайдер данных
    reporter.set_data_provider([]() {
        log::StatusData data;
        data.uptime = std::chrono::seconds(100);
        return data;
    });
    
    EXPECT_FALSE(reporter.is_running());
    
    reporter.start();
    EXPECT_TRUE(reporter.is_running());
    
    // Даём немного поработать
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    reporter.stop();
    EXPECT_FALSE(reporter.is_running());
}

/**
 * @brief Тест: fallback активен
 */
TEST_F(StatusReporterTest, FallbackActive) {
    log::StatusReporter reporter(config_);
    
    log::StatusData data;
    data.fallback_active = true;
    
    std::string output = reporter.render_status_plain(data);
    
    EXPECT_NE(output.find("Fallback: ON"), std::string::npos);
}

/**
 * @brief Тест: пустые chains
 */
TEST_F(StatusReporterTest, NoActiveChains) {
    log::StatusReporter reporter(config_);
    
    log::StatusData data;
    data.active_chains.clear();
    
    std::string output = reporter.render_status_plain(data);
    
    EXPECT_NE(output.find("Chains: none"), std::string::npos);
}

} // namespace quaxis::tests
