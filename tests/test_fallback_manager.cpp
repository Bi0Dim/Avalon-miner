/**
 * @file test_fallback_manager.cpp
 * @brief Тесты для FallbackManager и связанных компонентов
 */

#include <gtest/gtest.h>

#include "fallback/fallback_manager.hpp"
#include "fallback/pool_config.hpp"

namespace quaxis::tests {

// =============================================================================
// Тесты Pool Config
// =============================================================================

class PoolConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

/**
 * @brief Тест: парсинг Stratum URL
 */
TEST_F(PoolConfigTest, ParseStratumUrl) {
    fallback::StratumPoolConfig config;
    
    EXPECT_TRUE(config.parse_url("stratum+tcp://solo.ckpool.org:3333"));
    EXPECT_EQ(config.host, "solo.ckpool.org");
    EXPECT_EQ(config.port, 3333);
}

/**
 * @brief Тест: парсинг TCP URL без stratum+ префикса
 */
TEST_F(PoolConfigTest, ParseTcpUrl) {
    fallback::StratumPoolConfig config;
    
    EXPECT_TRUE(config.parse_url("tcp://pool.example.com:4444"));
    EXPECT_EQ(config.host, "pool.example.com");
    EXPECT_EQ(config.port, 4444);
}

/**
 * @brief Тест: некорректный URL
 */
TEST_F(PoolConfigTest, ParseInvalidUrl) {
    fallback::StratumPoolConfig config;
    
    EXPECT_FALSE(config.parse_url("invalid-url"));
    EXPECT_FALSE(config.parse_url("http://example.com"));
}

/**
 * @brief Тест: получение активного пула
 */
TEST_F(PoolConfigTest, GetActivePool) {
    fallback::FallbackConfig config;
    
    fallback::StratumPoolConfig pool1;
    pool1.enabled = true;
    pool1.priority = 100;
    pool1.host = "pool1.example.com";
    
    fallback::StratumPoolConfig pool2;
    pool2.enabled = true;
    pool2.priority = 50;  // Выше приоритет (меньше число)
    pool2.host = "pool2.example.com";
    
    fallback::StratumPoolConfig pool3;
    pool3.enabled = false;  // Отключён
    pool3.priority = 10;
    pool3.host = "pool3.example.com";
    
    config.stratum_pools = {pool1, pool2, pool3};
    
    const auto* active = config.get_active_pool();
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->host, "pool2.example.com");
}

/**
 * @brief Тест: нет активных пулов
 */
TEST_F(PoolConfigTest, NoActivePools) {
    fallback::FallbackConfig config;
    
    fallback::StratumPoolConfig pool;
    pool.enabled = false;
    config.stratum_pools = {pool};
    
    const auto* active = config.get_active_pool();
    EXPECT_EQ(active, nullptr);
}

// =============================================================================
// Тесты Fallback Manager
// =============================================================================

class FallbackManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.enabled = true;
        config_.timeouts.primary_health_check = std::chrono::milliseconds(100);
        config_.timeouts.primary_timeout = std::chrono::milliseconds(500);
    }
    
    fallback::FallbackConfig config_;
};

/**
 * @brief Тест: начальный режим - Primary SHM
 */
TEST_F(FallbackManagerTest, InitialModePrimary) {
    fallback::FallbackManager manager(config_);
    
    EXPECT_EQ(manager.current_mode(), fallback::FallbackMode::PrimarySHM);
}

/**
 * @brief Тест: проверка работы (не запущен)
 */
TEST_F(FallbackManagerTest, NotRunningInitially) {
    fallback::FallbackManager manager(config_);
    
    EXPECT_FALSE(manager.is_running());
}

/**
 * @brief Тест: запуск и остановка
 */
TEST_F(FallbackManagerTest, StartStop) {
    fallback::FallbackManager manager(config_);
    
    manager.start();
    EXPECT_TRUE(manager.is_running());
    
    manager.stop();
    EXPECT_FALSE(manager.is_running());
}

/**
 * @brief Тест: сигнал получения задания
 */
TEST_F(FallbackManagerTest, SignalJobReceived) {
    fallback::FallbackManager manager(config_);
    manager.start();
    
    // Сигнализируем о получении задания
    manager.signal_job_received();
    
    auto health = manager.get_shm_health();
    EXPECT_TRUE(health.available);
    
    manager.stop();
}

/**
 * @brief Тест: принудительная установка режима
 */
TEST_F(FallbackManagerTest, SetMode) {
    fallback::FallbackManager manager(config_);
    
    manager.set_mode(fallback::FallbackMode::FallbackZMQ);
    EXPECT_EQ(manager.current_mode(), fallback::FallbackMode::FallbackZMQ);
    
    manager.set_mode(fallback::FallbackMode::PrimarySHM);
    EXPECT_EQ(manager.current_mode(), fallback::FallbackMode::PrimarySHM);
}

/**
 * @brief Тест: получение статистики
 */
TEST_F(FallbackManagerTest, GetStats) {
    fallback::FallbackManager manager(config_);
    
    auto stats = manager.get_stats();
    EXPECT_EQ(stats.zmq_switches, 0);
    EXPECT_EQ(stats.stratum_switches, 0);
    EXPECT_EQ(stats.primary_restorations, 0);
}

/**
 * @brief Тест: callback при смене режима
 */
TEST_F(FallbackManagerTest, ModeChangeCallback) {
    fallback::FallbackManager manager(config_);
    
    bool callback_called = false;
    fallback::FallbackMode old_mode_received;
    fallback::FallbackMode new_mode_received;
    
    manager.set_mode_change_callback([&](fallback::FallbackMode old_mode, 
                                         fallback::FallbackMode new_mode) {
        callback_called = true;
        old_mode_received = old_mode;
        new_mode_received = new_mode;
    });
    
    manager.set_mode(fallback::FallbackMode::FallbackZMQ);
    
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(old_mode_received, fallback::FallbackMode::PrimarySHM);
    EXPECT_EQ(new_mode_received, fallback::FallbackMode::FallbackZMQ);
}

// =============================================================================
// Тесты преобразования режимов
// =============================================================================

TEST(FallbackModeTest, ToString) {
    EXPECT_EQ(fallback::to_string(fallback::FallbackMode::PrimarySHM), "primary_shm");
    EXPECT_EQ(fallback::to_string(fallback::FallbackMode::FallbackZMQ), "fallback_zmq");
    EXPECT_EQ(fallback::to_string(fallback::FallbackMode::FallbackStratum), "fallback_stratum");
}

TEST(FallbackModeTest, ToModeValue) {
    EXPECT_EQ(fallback::to_mode_value(fallback::FallbackMode::PrimarySHM), 0);
    EXPECT_EQ(fallback::to_mode_value(fallback::FallbackMode::FallbackZMQ), 1);
    EXPECT_EQ(fallback::to_mode_value(fallback::FallbackMode::FallbackStratum), 2);
}

} // namespace quaxis::tests
