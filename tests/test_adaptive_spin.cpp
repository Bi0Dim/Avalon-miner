/**
 * @file test_adaptive_spin.cpp
 * @brief Тесты для адаптивного spin-wait
 */

#include <gtest/gtest.h>

#include "shm/adaptive_spin.hpp"

#include <thread>
#include <chrono>
#include <atomic>

namespace quaxis::tests {

// =============================================================================
// Тесты конфигурации
// =============================================================================

TEST(AdaptiveSpinConfigTest, DefaultValues) {
    shm::AdaptiveSpinConfig config;
    
    EXPECT_EQ(config.spin_iterations, 1000u);
    EXPECT_EQ(config.yield_iterations, 100u);
    EXPECT_EQ(config.initial_sleep_us, 10u);
    EXPECT_EQ(config.max_sleep_us, 1000u);
    EXPECT_DOUBLE_EQ(config.sleep_backoff_multiplier, 1.5);
    EXPECT_TRUE(config.track_cpu_usage);
}

// =============================================================================
// Тесты AdaptiveSpin
// =============================================================================

class AdaptiveSpinTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.spin_iterations = 100;
        config_.yield_iterations = 10;
        config_.initial_sleep_us = 1;
        config_.max_sleep_us = 100;
    }
    
    shm::AdaptiveSpinConfig config_;
};

/**
 * @brief Тест: базовое ожидание с немедленными данными
 */
TEST_F(AdaptiveSpinTest, ImmediateData) {
    shm::AdaptiveSpin spin(config_);
    
    bool result = spin.wait_for_data([]() { return true; });
    
    EXPECT_TRUE(result);
}

/**
 * @brief Тест: таймаут при отсутствии данных
 */
TEST_F(AdaptiveSpinTest, TimeoutNoData) {
    shm::AdaptiveSpin spin(config_);
    
    auto start = std::chrono::steady_clock::now();
    bool result = spin.wait_for_data(
        []() { return false; },
        std::chrono::milliseconds(50)
    );
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    EXPECT_FALSE(result);
    // Должно занять примерно 50мс
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 45);
}

/**
 * @brief Тест: данные появляются через некоторое время
 */
TEST_F(AdaptiveSpinTest, DelayedData) {
    shm::AdaptiveSpin spin(config_);
    
    std::atomic<bool> data_ready{false};
    
    // Запускаем поток, который установит данные через 20мс
    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        data_ready.store(true, std::memory_order_release);
    });
    
    bool result = spin.wait_for_data(
        [&]() { return data_ready.load(std::memory_order_acquire); },
        std::chrono::milliseconds(100)
    );
    
    producer.join();
    
    EXPECT_TRUE(result);
}

/**
 * @brief Тест: включение/выключение
 */
TEST_F(AdaptiveSpinTest, EnableDisable) {
    shm::AdaptiveSpin spin(config_);
    
    EXPECT_TRUE(spin.is_enabled());
    
    spin.set_enabled(false);
    EXPECT_FALSE(spin.is_enabled());
    
    spin.set_enabled(true);
    EXPECT_TRUE(spin.is_enabled());
}

/**
 * @brief Тест: статистика
 */
TEST_F(AdaptiveSpinTest, Statistics) {
    shm::AdaptiveSpin spin(config_);
    
    auto stats = spin.get_stats();
    EXPECT_EQ(stats.successful_receives, 0u);
    
    // Выполняем успешное ожидание
    spin.wait_for_data([]() { return true; });
    
    stats = spin.get_stats();
    EXPECT_EQ(stats.successful_receives, 1u);
}

/**
 * @brief Тест: сигнализация о получении данных
 */
TEST_F(AdaptiveSpinTest, SignalDataReceived) {
    shm::AdaptiveSpin spin(config_);
    
    spin.signal_data_received();
    
    auto stats = spin.get_stats();
    EXPECT_EQ(stats.successful_receives, 1u);
}

/**
 * @brief Тест: сброс
 */
TEST_F(AdaptiveSpinTest, Reset) {
    shm::AdaptiveSpin spin(config_);
    
    // Вызываем несколько раз signal_data_received
    spin.signal_data_received();
    spin.signal_data_received();
    
    // Reset не сбрасывает статистику, только состояние backoff
    spin.reset();
    
    auto stats = spin.get_stats();
    EXPECT_EQ(stats.successful_receives, 2u);
}

/**
 * @brief Тест: сброс статистики
 */
TEST_F(AdaptiveSpinTest, ResetStats) {
    shm::AdaptiveSpin spin(config_);
    
    spin.signal_data_received();
    spin.signal_data_received();
    
    auto stats = spin.get_stats();
    EXPECT_EQ(stats.successful_receives, 2u);
    
    spin.reset_stats();
    
    stats = spin.get_stats();
    EXPECT_EQ(stats.successful_receives, 0u);
}

/**
 * @brief Тест: CPU usage
 */
TEST_F(AdaptiveSpinTest, CpuUsage) {
    shm::AdaptiveSpin spin(config_);
    
    // Начальное значение должно быть 0
    EXPECT_DOUBLE_EQ(spin.get_cpu_usage_percent(), 0.0);
}

/**
 * @brief Тест: SpinGuard RAII
 */
TEST_F(AdaptiveSpinTest, SpinGuard) {
    shm::AdaptiveSpin spin(config_);
    
    {
        shm::SpinGuard guard(spin);
        // В этом скоупе guard активен
    }
    // После выхода из скоупа состояние должно быть сброшено
    
    // Это не тестирует конкретное поведение, просто проверяет компиляцию
    EXPECT_TRUE(true);
}

} // namespace quaxis::tests
