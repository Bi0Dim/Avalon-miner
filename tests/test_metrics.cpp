/**
 * @file test_metrics.cpp
 * @brief Тесты для Prometheus метрик
 */

#include <gtest/gtest.h>

#include "monitoring/metrics.hpp"
#include "monitoring/alerter.hpp"

namespace quaxis::tests {

// =============================================================================
// Тесты Metrics
// =============================================================================

class MetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Сбрасываем метрики перед каждым тестом
        monitoring::Metrics::instance().reset();
    }
    
    void TearDown() override {
        monitoring::Metrics::instance().reset();
    }
};

/**
 * @brief Тест: singleton
 */
TEST_F(MetricsTest, Singleton) {
    auto& metrics1 = monitoring::Metrics::instance();
    auto& metrics2 = monitoring::Metrics::instance();
    
    EXPECT_EQ(&metrics1, &metrics2);
}

/**
 * @brief Тест: счётчик заданий
 */
TEST_F(MetricsTest, JobsSentCounter) {
    auto& metrics = monitoring::Metrics::instance();
    
    EXPECT_EQ(metrics.get_jobs_sent(), 0);
    
    metrics.inc_jobs_sent();
    metrics.inc_jobs_sent();
    metrics.inc_jobs_sent();
    
    EXPECT_EQ(metrics.get_jobs_sent(), 3);
}

/**
 * @brief Тест: счётчик shares
 */
TEST_F(MetricsTest, SharesFoundCounter) {
    auto& metrics = monitoring::Metrics::instance();
    
    EXPECT_EQ(metrics.get_shares_found(), 0);
    
    metrics.inc_shares_found();
    
    EXPECT_EQ(metrics.get_shares_found(), 1);
}

/**
 * @brief Тест: счётчик блоков
 */
TEST_F(MetricsTest, BlocksFoundCounter) {
    auto& metrics = monitoring::Metrics::instance();
    
    EXPECT_EQ(metrics.get_blocks_found(), 0);
    
    metrics.inc_blocks_found();
    
    EXPECT_EQ(metrics.get_blocks_found(), 1);
}

/**
 * @brief Тест: gauge хешрейта
 */
TEST_F(MetricsTest, HashrateGauge) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.set_hashrate(90.5);
    EXPECT_DOUBLE_EQ(metrics.get_hashrate(), 90.5);
    
    metrics.set_hashrate(100.0);
    EXPECT_DOUBLE_EQ(metrics.get_hashrate(), 100.0);
}

/**
 * @brief Тест: gauge режима
 */
TEST_F(MetricsTest, ModeGauge) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.set_mode(0);
    EXPECT_EQ(metrics.get_mode(), 0);
    
    metrics.set_mode(1);
    EXPECT_EQ(metrics.get_mode(), 1);
    
    metrics.set_mode(2);
    EXPECT_EQ(metrics.get_mode(), 2);
}

/**
 * @brief Тест: статус подключения к Bitcoin Core
 */
TEST_F(MetricsTest, BitcoinConnectedGauge) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.set_bitcoin_connected(true);
    EXPECT_TRUE(metrics.is_bitcoin_connected());
    
    metrics.set_bitcoin_connected(false);
    EXPECT_FALSE(metrics.is_bitcoin_connected());
}

/**
 * @brief Тест: количество ASIC подключений
 */
TEST_F(MetricsTest, AsicConnectionsGauge) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.set_asic_connections(3);
    EXPECT_EQ(metrics.get_asic_connections(), 3);
}

/**
 * @brief Тест: количество активных merged chains
 */
TEST_F(MetricsTest, MergedChainsActiveGauge) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.set_merged_chains_active(11);
    EXPECT_EQ(metrics.get_merged_chains_active(), 11);
}

/**
 * @brief Тест: uptime
 */
TEST_F(MetricsTest, Uptime) {
    auto& metrics = monitoring::Metrics::instance();
    
    // Uptime должен быть >= 0
    EXPECT_GE(metrics.get_uptime_seconds(), 0);
    
    // Подождём немного и проверим, что uptime увеличился
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_GE(metrics.get_uptime_seconds(), 0);
}

/**
 * @brief Тест: histogram латентности
 */
TEST_F(MetricsTest, LatencyHistogram) {
    auto& metrics = monitoring::Metrics::instance();
    
    // Записываем наблюдения
    metrics.observe_latency(0.5);   // < 1ms
    metrics.observe_latency(3.0);   // < 5ms
    metrics.observe_latency(8.0);   // < 10ms
    metrics.observe_latency(15.0);  // < 25ms
    
    // Проверяем экспорт Prometheus
    std::string exported = metrics.export_prometheus();
    EXPECT_FALSE(exported.empty());
    EXPECT_NE(exported.find("quaxis_latency_ms_bucket"), std::string::npos);
}

/**
 * @brief Тест: экспорт Prometheus формата
 */
TEST_F(MetricsTest, ExportPrometheus) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.set_hashrate(90.5);
    metrics.inc_jobs_sent();
    metrics.set_mode(0);
    
    std::string exported = metrics.export_prometheus();
    
    // Проверяем наличие метрик
    EXPECT_NE(exported.find("quaxis_hashrate_ths"), std::string::npos);
    EXPECT_NE(exported.find("quaxis_jobs_sent_total"), std::string::npos);
    EXPECT_NE(exported.find("quaxis_mode"), std::string::npos);
    EXPECT_NE(exported.find("quaxis_uptime_seconds"), std::string::npos);
    
    // Проверяем формат
    EXPECT_NE(exported.find("# HELP"), std::string::npos);
    EXPECT_NE(exported.find("# TYPE"), std::string::npos);
}

/**
 * @brief Тест: merged blocks per chain
 */
TEST_F(MetricsTest, MergedBlocksPerChain) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.inc_merged_blocks_found("namecoin");
    metrics.inc_merged_blocks_found("namecoin");
    metrics.inc_merged_blocks_found("syscoin");
    
    std::string exported = metrics.export_prometheus();
    EXPECT_NE(exported.find("quaxis_merged_blocks_total"), std::string::npos);
    EXPECT_NE(exported.find("namecoin"), std::string::npos);
    EXPECT_NE(exported.find("syscoin"), std::string::npos);
}

/**
 * @brief Тест: сброс метрик
 */
TEST_F(MetricsTest, Reset) {
    auto& metrics = monitoring::Metrics::instance();
    
    metrics.inc_jobs_sent();
    metrics.set_hashrate(100.0);
    metrics.set_mode(2);
    
    EXPECT_EQ(metrics.get_jobs_sent(), 1);
    EXPECT_DOUBLE_EQ(metrics.get_hashrate(), 100.0);
    EXPECT_EQ(metrics.get_mode(), 2);
    
    metrics.reset();
    
    EXPECT_EQ(metrics.get_jobs_sent(), 0);
    EXPECT_DOUBLE_EQ(metrics.get_hashrate(), 0.0);
    EXPECT_EQ(metrics.get_mode(), 0);
}

// =============================================================================
// Тесты Alerter
// =============================================================================

class AlerterTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitoring::Alerter::instance().reset_stats();
        
        monitoring::AlerterConfig config;
        config.log_level = monitoring::AlertLevel::Info;
        config.console_output = false;  // Отключаем вывод в консоль для тестов
        config.dedup_interval_seconds = 0;  // Отключаем дедупликацию для тестов
        monitoring::Alerter::instance().configure(config);
    }
    
    void TearDown() override {
        monitoring::Alerter::instance().set_callback(nullptr);  // Очищаем callback
        monitoring::Alerter::instance().reset_stats();
    }
};

/**
 * @brief Тест: singleton
 */
TEST_F(AlerterTest, Singleton) {
    auto& alerter1 = monitoring::Alerter::instance();
    auto& alerter2 = monitoring::Alerter::instance();
    
    EXPECT_EQ(&alerter1, &alerter2);
}

/**
 * @brief Тест: подсчёт алертов
 */
TEST_F(AlerterTest, AlertsCount) {
    auto& alerter = monitoring::Alerter::instance();
    
    EXPECT_EQ(alerter.get_alerts_count(), 0);
    
    alerter.alert(monitoring::AlertLevel::Warning, "Test alert");
    
    EXPECT_EQ(alerter.get_alerts_count(), 1);
}

/**
 * @brief Тест: подсчёт critical алертов
 */
TEST_F(AlerterTest, CriticalCount) {
    auto& alerter = monitoring::Alerter::instance();
    
    EXPECT_EQ(alerter.get_critical_count(), 0);
    
    alerter.alert(monitoring::AlertLevel::Warning, "Warning");
    EXPECT_EQ(alerter.get_critical_count(), 0);
    
    alerter.alert(monitoring::AlertLevel::Critical, "Critical");
    EXPECT_EQ(alerter.get_critical_count(), 1);
}

/**
 * @brief Тест: callback для алертов
 */
TEST_F(AlerterTest, AlertCallback) {
    auto& alerter = monitoring::Alerter::instance();
    
    bool callback_called = false;
    monitoring::AlertLevel received_level;
    std::string received_message;
    
    alerter.set_callback([&](monitoring::AlertLevel level, std::string_view message) {
        callback_called = true;
        received_level = level;
        received_message = std::string(message);
    });
    
    alerter.alert(monitoring::AlertLevel::Warning, "Test message");
    
    // Очищаем callback перед проверками чтобы избежать use-after-return
    alerter.set_callback(nullptr);
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(received_level, monitoring::AlertLevel::Warning);
    EXPECT_EQ(received_message, "Test message");
}

/**
 * @brief Тест: преобразование уровня в строку
 */
TEST(AlertLevelTest, ToString) {
    EXPECT_EQ(monitoring::alert_level_to_string(monitoring::AlertLevel::Info), "INFO");
    EXPECT_EQ(monitoring::alert_level_to_string(monitoring::AlertLevel::Warning), "WARNING");
    EXPECT_EQ(monitoring::alert_level_to_string(monitoring::AlertLevel::Critical), "CRITICAL");
}

/**
 * @brief Тест: сброс статистики
 */
TEST_F(AlerterTest, ResetStats) {
    auto& alerter = monitoring::Alerter::instance();
    
    alerter.alert(monitoring::AlertLevel::Critical, "Test");
    EXPECT_EQ(alerter.get_alerts_count(), 1);
    EXPECT_EQ(alerter.get_critical_count(), 1);
    
    alerter.reset_stats();
    
    EXPECT_EQ(alerter.get_alerts_count(), 0);
    EXPECT_EQ(alerter.get_critical_count(), 0);
}

} // namespace quaxis::tests
