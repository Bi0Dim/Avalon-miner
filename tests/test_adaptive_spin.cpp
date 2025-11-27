/**
 * @file test_adaptive_spin.cpp
 * @brief Tests for adaptive spin-wait SHM implementation
 */

#include <gtest/gtest.h>

#include "shm/adaptive_spin.hpp"

#include <chrono>
#include <thread>

namespace quaxis::shm::test {

// =============================================================================
// AdaptiveSpinConfig Tests
// =============================================================================

TEST(AdaptiveSpinConfigTest, DefaultValues) {
    AdaptiveSpinConfig config;
    
    EXPECT_EQ(config.spin_iterations, 1000u);
    EXPECT_EQ(config.yield_iterations, 100u);
    EXPECT_EQ(config.sleep_us, 100u);
    EXPECT_TRUE(config.reset_on_change);
}

TEST(AdaptiveSpinConfigTest, HighPerformancePreset) {
    auto config = AdaptiveSpinConfig::high_performance();
    
    EXPECT_EQ(config.spin_iterations, 10000u);
    EXPECT_EQ(config.yield_iterations, 1000u);
    EXPECT_EQ(config.sleep_us, 50u);
    EXPECT_TRUE(config.reset_on_change);
}

TEST(AdaptiveSpinConfigTest, BalancedPreset) {
    auto config = AdaptiveSpinConfig::balanced();
    
    EXPECT_EQ(config.spin_iterations, 1000u);
    EXPECT_EQ(config.yield_iterations, 100u);
    EXPECT_EQ(config.sleep_us, 100u);
}

TEST(AdaptiveSpinConfigTest, PowerSavingPreset) {
    auto config = AdaptiveSpinConfig::power_saving();
    
    EXPECT_EQ(config.spin_iterations, 100u);
    EXPECT_EQ(config.yield_iterations, 10u);
    EXPECT_EQ(config.sleep_us, 1000u);
}

// =============================================================================
// SpinStage Tests
// =============================================================================

TEST(SpinStageTest, ToString) {
    EXPECT_STREQ(to_string(SpinStage::Spin), "spin");
    EXPECT_STREQ(to_string(SpinStage::Yield), "yield");
    EXPECT_STREQ(to_string(SpinStage::Sleep), "sleep");
}

// =============================================================================
// AdaptiveSpinStats Tests
// =============================================================================

TEST(AdaptiveSpinStatsTest, InitialValues) {
    AdaptiveSpinStats stats;
    
    EXPECT_EQ(stats.total_iterations.load(), 0u);
    EXPECT_EQ(stats.spin_iterations.load(), 0u);
    EXPECT_EQ(stats.yield_iterations.load(), 0u);
    EXPECT_EQ(stats.sleep_iterations.load(), 0u);
    EXPECT_EQ(stats.stage_transitions.load(), 0u);
    EXPECT_EQ(stats.change_resets.load(), 0u);
}

TEST(AdaptiveSpinStatsTest, Reset) {
    AdaptiveSpinStats stats;
    
    stats.total_iterations.store(100);
    stats.spin_iterations.store(50);
    stats.yield_iterations.store(30);
    stats.sleep_iterations.store(20);
    
    stats.reset();
    
    EXPECT_EQ(stats.total_iterations.load(), 0u);
    EXPECT_EQ(stats.spin_iterations.load(), 0u);
    EXPECT_EQ(stats.yield_iterations.load(), 0u);
    EXPECT_EQ(stats.sleep_iterations.load(), 0u);
}

TEST(AdaptiveSpinStatsTest, EstimatedCpuUsage) {
    AdaptiveSpinStats stats;
    
    // No iterations = 0 CPU usage
    EXPECT_DOUBLE_EQ(stats.estimated_cpu_usage(), 0.0);
    
    // 100% spin = 100% CPU
    stats.total_iterations.store(100);
    stats.spin_iterations.store(100);
    EXPECT_DOUBLE_EQ(stats.estimated_cpu_usage(), 100.0);
    
    // 100% yield = ~50% CPU
    stats.spin_iterations.store(0);
    stats.yield_iterations.store(100);
    EXPECT_DOUBLE_EQ(stats.estimated_cpu_usage(), 50.0);
    
    // 100% sleep = ~0% CPU (only spin and yield contribute)
    stats.yield_iterations.store(0);
    stats.sleep_iterations.store(100);
    EXPECT_DOUBLE_EQ(stats.estimated_cpu_usage(), 0.0);
    
    // 50% spin, 50% yield = ~75% CPU
    stats.spin_iterations.store(50);
    stats.yield_iterations.store(50);
    stats.sleep_iterations.store(0);
    EXPECT_DOUBLE_EQ(stats.estimated_cpu_usage(), 75.0);
}

// =============================================================================
// AdaptiveSpinWaiter Tests
// =============================================================================

class AdaptiveSpinWaiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Small iteration counts for fast tests
        config_.spin_iterations = 10;
        config_.yield_iterations = 5;
        config_.sleep_us = 1;
        config_.reset_on_change = true;
    }
    
    AdaptiveSpinConfig config_;
};

TEST_F(AdaptiveSpinWaiterTest, InitialState) {
    AdaptiveSpinWaiter waiter(config_);
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Spin);
    EXPECT_EQ(waiter.stats().total_iterations.load(), 0u);
}

TEST_F(AdaptiveSpinWaiterTest, StaysInSpinStage) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Fewer than spin_iterations should stay in Spin stage
    for (int i = 0; i < 5; ++i) {
        waiter.wait();
    }
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Spin);
    EXPECT_EQ(waiter.stats().spin_iterations.load(), 5u);
    EXPECT_EQ(waiter.stats().yield_iterations.load(), 0u);
}

TEST_F(AdaptiveSpinWaiterTest, TransitionsToYield) {
    AdaptiveSpinWaiter waiter(config_);
    
    // After spin_iterations, should transition to Yield
    for (uint32_t i = 0; i <= config_.spin_iterations; ++i) {
        waiter.wait();
    }
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Yield);
    EXPECT_EQ(waiter.stats().stage_transitions.load(), 1u);
}

TEST_F(AdaptiveSpinWaiterTest, TransitionsToSleep) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Transition through all stages to Sleep
    uint32_t total_iterations = config_.spin_iterations + config_.yield_iterations + 1;
    for (uint32_t i = 0; i <= total_iterations; ++i) {
        waiter.wait();
    }
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Sleep);
    EXPECT_EQ(waiter.stats().stage_transitions.load(), 2u);
}

TEST_F(AdaptiveSpinWaiterTest, StaysInSleep) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Get to sleep stage
    uint32_t total = config_.spin_iterations + config_.yield_iterations + 5;
    for (uint32_t i = 0; i <= total; ++i) {
        waiter.wait();
    }
    
    // Additional waits should stay in sleep
    SpinStage stage_before = waiter.current_stage();
    waiter.wait();
    waiter.wait();
    waiter.wait();
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Sleep);
    EXPECT_EQ(stage_before, SpinStage::Sleep);
}

TEST_F(AdaptiveSpinWaiterTest, OnChangeDetectedResetsToSpin) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Get to yield stage
    for (uint32_t i = 0; i <= config_.spin_iterations; ++i) {
        waiter.wait();
    }
    EXPECT_EQ(waiter.current_stage(), SpinStage::Yield);
    
    // Simulate change detection
    waiter.on_change_detected();
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Spin);
    EXPECT_EQ(waiter.stats().change_resets.load(), 1u);
}

TEST_F(AdaptiveSpinWaiterTest, OnChangeDoesNotResetFromSpin) {
    AdaptiveSpinWaiter waiter(config_);
    
    waiter.wait();  // In Spin stage
    EXPECT_EQ(waiter.current_stage(), SpinStage::Spin);
    
    waiter.on_change_detected();
    
    // Should not count as reset since we're already in Spin
    EXPECT_EQ(waiter.stats().change_resets.load(), 0u);
}

TEST_F(AdaptiveSpinWaiterTest, ManualReset) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Get to yield
    for (uint32_t i = 0; i <= config_.spin_iterations; ++i) {
        waiter.wait();
    }
    EXPECT_EQ(waiter.current_stage(), SpinStage::Yield);
    
    waiter.reset();
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Spin);
}

TEST_F(AdaptiveSpinWaiterTest, ResetStats) {
    AdaptiveSpinWaiter waiter(config_);
    
    for (int i = 0; i < 5; ++i) {
        waiter.wait();
    }
    EXPECT_GT(waiter.stats().total_iterations.load(), 0u);
    
    waiter.reset_stats();
    
    EXPECT_EQ(waiter.stats().total_iterations.load(), 0u);
}

TEST_F(AdaptiveSpinWaiterTest, SetConfig) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Get to yield
    for (uint32_t i = 0; i <= config_.spin_iterations; ++i) {
        waiter.wait();
    }
    EXPECT_EQ(waiter.current_stage(), SpinStage::Yield);
    
    // Change config resets stage
    auto new_config = AdaptiveSpinConfig::power_saving();
    waiter.set_config(new_config);
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Spin);
    EXPECT_EQ(waiter.config().spin_iterations, new_config.spin_iterations);
}

TEST_F(AdaptiveSpinWaiterTest, DisableResetOnChange) {
    config_.reset_on_change = false;
    AdaptiveSpinWaiter waiter(config_);
    
    // Get to yield
    for (uint32_t i = 0; i <= config_.spin_iterations; ++i) {
        waiter.wait();
    }
    EXPECT_EQ(waiter.current_stage(), SpinStage::Yield);
    
    waiter.on_change_detected();
    
    // Should stay in yield since reset_on_change is false
    EXPECT_EQ(waiter.current_stage(), SpinStage::Yield);
    EXPECT_EQ(waiter.stats().change_resets.load(), 0u);
}

TEST_F(AdaptiveSpinWaiterTest, SleepStageTiming) {
    AdaptiveSpinConfig sleep_config;
    sleep_config.spin_iterations = 1;
    sleep_config.yield_iterations = 1;
    sleep_config.sleep_us = 1000;  // 1ms sleep
    
    AdaptiveSpinWaiter waiter(sleep_config);
    
    // Get to sleep stage quickly
    waiter.wait();  // spin
    waiter.wait();  // transitions to yield
    waiter.wait();  // yield
    waiter.wait();  // transitions to sleep
    
    EXPECT_EQ(waiter.current_stage(), SpinStage::Sleep);
    
    // Time a few sleep waits
    auto start = std::chrono::steady_clock::now();
    waiter.wait();
    waiter.wait();
    waiter.wait();
    auto end = std::chrono::steady_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Should take at least 3ms (3 x 1ms sleeps)
    EXPECT_GE(duration_ms, 2);  // Allow some margin for timing
}

// =============================================================================
// CPU Reduction Verification Test
// =============================================================================

TEST_F(AdaptiveSpinWaiterTest, ReducesCpuUsageOverTime) {
    AdaptiveSpinWaiter waiter(config_);
    
    // Total iterations to go through all stages
    uint32_t total = config_.spin_iterations + config_.yield_iterations + 10;
    for (uint32_t i = 0; i <= total; ++i) {
        waiter.wait();
    }
    
    const auto& stats = waiter.stats();
    
    // Verify we went through all stages
    EXPECT_GT(stats.spin_iterations.load(), 0u);
    EXPECT_GT(stats.yield_iterations.load(), 0u);
    EXPECT_GT(stats.sleep_iterations.load(), 0u);
    
    // CPU usage should be less than 100% due to yield and sleep phases
    double cpu = stats.estimated_cpu_usage();
    EXPECT_LT(cpu, 100.0);
    EXPECT_GT(cpu, 0.0);
}

} // namespace quaxis::shm::test
