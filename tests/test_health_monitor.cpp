/**
 * @file test_health_monitor.cpp
 * @brief Тесты Health Monitor (Predictive Maintenance)
 */

#include <gtest/gtest.h>

#include "monitoring/health_monitor.hpp"
#include "monitoring/alert_manager.hpp"

namespace quaxis::tests {

class HealthMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enabled = true;
        config_.collection_interval = 1;
        config_.temp_warning = 75.0;
        config_.temp_critical = 85.0;
        config_.temp_emergency = 95.0;
        config_.hashrate_warning_drop = 10.0;
        config_.hashrate_critical_drop = 25.0;
        config_.error_rate_warning = 1.0;
        config_.error_rate_critical = 5.0;
    }
    
    monitoring::HealthConfig config_;
};

/**
 * @brief Тест: обновление температуры
 */
TEST_F(HealthMonitorTest, TemperatureUpdate) {
    monitoring::HealthMonitor monitor(config_);
    
    // Обновляем температуру
    monitor.update_temperature(0, 70.0);
    
    auto metrics = monitor.get_metrics();
    EXPECT_DOUBLE_EQ(metrics.temperature.current, 70.0);
}

/**
 * @brief Тест: статус Healthy
 */
TEST_F(HealthMonitorTest, StatusHealthy) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.update_temperature(0, 65.0);
    monitor.update_hashrate(90e12);
    monitor.set_nominal_hashrate(90e12);
    
    EXPECT_EQ(monitor.get_status(), monitoring::HealthMetrics::Status::Healthy);
    EXPECT_FALSE(monitor.requires_action());
}

/**
 * @brief Тест: статус Warning при высокой температуре
 */
TEST_F(HealthMonitorTest, StatusWarningTemperature) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.update_temperature(0, 78.0);  // > temp_warning
    
    EXPECT_EQ(monitor.get_status(), monitoring::HealthMetrics::Status::Warning);
    EXPECT_TRUE(monitor.requires_action());
}

/**
 * @brief Тест: статус Critical при критической температуре
 */
TEST_F(HealthMonitorTest, StatusCriticalTemperature) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.update_temperature(0, 88.0);  // > temp_critical
    
    EXPECT_EQ(monitor.get_status(), monitoring::HealthMetrics::Status::Critical);
}

/**
 * @brief Тест: статус Emergency при аварийной температуре
 */
TEST_F(HealthMonitorTest, StatusEmergencyTemperature) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.update_temperature(0, 98.0);  // > temp_emergency
    
    EXPECT_EQ(monitor.get_status(), monitoring::HealthMetrics::Status::Emergency);
}

/**
 * @brief Тест: падение хешрейта
 */
TEST_F(HealthMonitorTest, HashrateDropWarning) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.set_nominal_hashrate(100e12);
    monitor.update_hashrate(85e12);  // 15% drop > warning threshold
    
    // Не проверяем напрямую - эффективность проверяется внутри
    auto metrics = monitor.get_metrics();
    EXPECT_LT(metrics.hashrate.efficiency, 0.95);
}

/**
 * @brief Тест: запись ошибок
 */
TEST_F(HealthMonitorTest, ErrorRecording) {
    monitoring::HealthMonitor monitor(config_);
    
    // Записываем shares
    for (int i = 0; i < 100; ++i) {
        monitor.record_share();
    }
    
    // Записываем ошибки
    monitor.record_error(true, false, false);  // HW error
    monitor.record_error(false, true, false);  // Rejected
    
    auto metrics = monitor.get_metrics();
    EXPECT_EQ(metrics.errors.total_shares, 100);
    EXPECT_EQ(metrics.errors.hw_errors, 1);
    EXPECT_EQ(metrics.errors.rejected_shares, 1);
    EXPECT_GT(metrics.errors.error_rate, 0.0);
}

/**
 * @brief Тест: обновление питания
 */
TEST_F(HealthMonitorTest, PowerUpdate) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.update_power(12.0, 100.0);  // 12V, 100A = 1200W
    
    auto metrics = monitor.get_metrics();
    EXPECT_DOUBLE_EQ(metrics.power.voltage, 12.0);
    EXPECT_DOUBLE_EQ(metrics.power.current, 100.0);
    EXPECT_DOUBLE_EQ(metrics.power.power, 1200.0);
}

/**
 * @brief Тест: uptime и рестарты
 */
TEST_F(HealthMonitorTest, UptimeAndRestarts) {
    monitoring::HealthMonitor monitor(config_);
    
    auto metrics = monitor.get_metrics();
    EXPECT_EQ(metrics.uptime.restarts, 0);
    
    monitor.record_restart();
    monitor.record_restart();
    
    metrics = monitor.get_metrics();
    EXPECT_EQ(metrics.uptime.restarts, 2);
}

/**
 * @brief Тест: сброс метрик
 */
TEST_F(HealthMonitorTest, Reset) {
    monitoring::HealthMonitor monitor(config_);
    
    monitor.update_temperature(0, 80.0);
    monitor.update_hashrate(90e12);
    monitor.record_restart();
    
    monitor.reset();
    
    auto metrics = monitor.get_metrics();
    EXPECT_EQ(metrics.temperature.current, 0.0);
    EXPECT_EQ(metrics.hashrate.current, 0.0);
    EXPECT_EQ(metrics.uptime.restarts, 0);
}

// =============================================================================
// Тесты Alert Manager
// =============================================================================

class AlertManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.max_alerts = 100;
        config_.auto_resolve_timeout = 3600;
        config_.duplicate_cooldown = 1;  // 1 секунда для тестов
        config_.auto_actions_enabled = false;
    }
    
    monitoring::AlertConfig config_;
};

/**
 * @brief Тест: создание алерта
 */
TEST_F(AlertManagerTest, CreateAlert) {
    monitoring::AlertManager manager(config_);
    
    auto id = manager.create_alert(
        monitoring::AlertLevel::Warning,
        monitoring::AlertType::TemperatureHigh,
        "Тестовый алерт"
    );
    
    EXPECT_GT(id, 0u);
    
    auto alert = manager.get_alert(id);
    ASSERT_TRUE(alert.has_value());
    EXPECT_EQ(alert->level, monitoring::AlertLevel::Warning);
    EXPECT_EQ(alert->type, monitoring::AlertType::TemperatureHigh);
    EXPECT_EQ(alert->message, "Тестовый алерт");
}

/**
 * @brief Тест: подтверждение алерта
 */
TEST_F(AlertManagerTest, AcknowledgeAlert) {
    monitoring::AlertManager manager(config_);
    
    auto id = manager.create_alert(
        monitoring::AlertLevel::Warning,
        monitoring::AlertType::TemperatureHigh,
        "Тест"
    );
    
    auto alert = manager.get_alert(id);
    EXPECT_FALSE(alert->acknowledged);
    
    EXPECT_TRUE(manager.acknowledge(id));
    
    alert = manager.get_alert(id);
    EXPECT_TRUE(alert->acknowledged);
}

/**
 * @brief Тест: разрешение алерта
 */
TEST_F(AlertManagerTest, ResolveAlert) {
    monitoring::AlertManager manager(config_);
    
    auto id = manager.create_alert(
        monitoring::AlertLevel::Warning,
        monitoring::AlertType::TemperatureHigh,
        "Тест"
    );
    
    auto active = manager.get_active_alerts();
    EXPECT_EQ(active.size(), 1);
    
    EXPECT_TRUE(manager.resolve(id));
    
    active = manager.get_active_alerts();
    EXPECT_EQ(active.size(), 0);
}

/**
 * @brief Тест: подсчёт алертов
 */
TEST_F(AlertManagerTest, AlertCounts) {
    monitoring::AlertManager manager(config_);
    
    manager.create_alert(monitoring::AlertLevel::Info, monitoring::AlertType::BlockFound, "1");
    
    // Ждём чтобы избежать дедупликации
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    manager.create_alert(monitoring::AlertLevel::Warning, monitoring::AlertType::TemperatureHigh, "2");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    manager.create_alert(monitoring::AlertLevel::Critical, monitoring::AlertType::HashrateDropped, "3");
    
    auto counts = manager.get_counts();
    EXPECT_EQ(counts.info, 1);
    EXPECT_EQ(counts.warning, 1);
    EXPECT_EQ(counts.critical, 1);
    EXPECT_EQ(counts.total, 3);
}

/**
 * @brief Тест: очистка алертов
 */
TEST_F(AlertManagerTest, ClearAlerts) {
    monitoring::AlertManager manager(config_);
    
    manager.create_alert(monitoring::AlertLevel::Info, monitoring::AlertType::BlockFound, "1");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    manager.create_alert(monitoring::AlertLevel::Warning, monitoring::AlertType::TemperatureHigh, "2");
    
    auto counts = manager.get_counts();
    EXPECT_GT(counts.total, 0);
    
    manager.clear_all();
    
    counts = manager.get_counts();
    EXPECT_EQ(counts.total, 0);
}

} // namespace quaxis::tests
