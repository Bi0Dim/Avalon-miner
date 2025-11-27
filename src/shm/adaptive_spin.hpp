/**
 * @file adaptive_spin.hpp
 * @brief Adaptive spin-wait for SHM subscriber
 * 
 * Multi-stage waiting strategy to balance latency vs CPU usage:
 * 1. Spin stage: Pure CPU spin for ultra-low latency
 * 2. Yield stage: Thread yield for moderate latency
 * 3. Sleep stage: Timed sleep to reduce CPU load
 * 
 * Parameters are configurable via ShmConfig.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace quaxis::shm {

// =============================================================================
// Adaptive Spin Configuration
// =============================================================================

/**
 * @brief Configuration for adaptive spin-wait stages
 */
struct AdaptiveSpinConfig {
    /// @brief Number of iterations in spin stage before moving to yield
    uint32_t spin_iterations = 1000;
    
    /// @brief Number of yields before moving to sleep stage
    uint32_t yield_iterations = 100;
    
    /// @brief Sleep duration in microseconds during sleep stage
    uint32_t sleep_us = 100;
    
    /// @brief Reset stage to spin after detecting change
    bool reset_on_change = true;
    
    /**
     * @brief Create default high-performance config (low latency, high CPU)
     */
    static AdaptiveSpinConfig high_performance() {
        return AdaptiveSpinConfig{
            .spin_iterations = 10000,
            .yield_iterations = 1000,
            .sleep_us = 50,
            .reset_on_change = true
        };
    }
    
    /**
     * @brief Create balanced config (moderate latency, moderate CPU)
     */
    static AdaptiveSpinConfig balanced() {
        return AdaptiveSpinConfig{
            .spin_iterations = 1000,
            .yield_iterations = 100,
            .sleep_us = 100,
            .reset_on_change = true
        };
    }
    
    /**
     * @brief Create power-saving config (higher latency, low CPU)
     */
    static AdaptiveSpinConfig power_saving() {
        return AdaptiveSpinConfig{
            .spin_iterations = 100,
            .yield_iterations = 10,
            .sleep_us = 1000,
            .reset_on_change = true
        };
    }
};

// =============================================================================
// Adaptive Spin Stage
// =============================================================================

/**
 * @brief Current stage of adaptive waiting
 */
enum class SpinStage : uint8_t {
    Spin = 0,   ///< Pure CPU spin (lowest latency, highest CPU)
    Yield = 1,  ///< Thread yield (low latency, moderate CPU)
    Sleep = 2   ///< Timed sleep (moderate latency, lowest CPU)
};

/**
 * @brief Convert SpinStage to string
 */
constexpr const char* to_string(SpinStage stage) noexcept {
    switch (stage) {
        case SpinStage::Spin:  return "spin";
        case SpinStage::Yield: return "yield";
        case SpinStage::Sleep: return "sleep";
        default: return "unknown";
    }
}

// =============================================================================
// Adaptive Spin Statistics
// =============================================================================

/**
 * @brief Statistics for adaptive spin-wait
 */
struct AdaptiveSpinStats {
    /// @brief Total number of wait iterations
    std::atomic<uint64_t> total_iterations{0};
    
    /// @brief Iterations spent in spin stage
    std::atomic<uint64_t> spin_iterations{0};
    
    /// @brief Iterations spent in yield stage
    std::atomic<uint64_t> yield_iterations{0};
    
    /// @brief Iterations spent in sleep stage
    std::atomic<uint64_t> sleep_iterations{0};
    
    /// @brief Number of stage transitions
    std::atomic<uint64_t> stage_transitions{0};
    
    /// @brief Number of times reset to spin due to change detection
    std::atomic<uint64_t> change_resets{0};
    
    /**
     * @brief Reset all statistics
     */
    void reset() noexcept {
        total_iterations.store(0, std::memory_order_relaxed);
        spin_iterations.store(0, std::memory_order_relaxed);
        yield_iterations.store(0, std::memory_order_relaxed);
        sleep_iterations.store(0, std::memory_order_relaxed);
        stage_transitions.store(0, std::memory_order_relaxed);
        change_resets.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get CPU usage percentage (estimated)
     * 
     * Spin = 100%, Yield = ~50%, Sleep = low
     */
    [[nodiscard]] double estimated_cpu_usage() const noexcept {
        uint64_t total = total_iterations.load(std::memory_order_relaxed);
        if (total == 0) return 0.0;
        
        uint64_t spin = spin_iterations.load(std::memory_order_relaxed);
        uint64_t yield = yield_iterations.load(std::memory_order_relaxed);
        // Sleep has negligible CPU usage
        
        return (static_cast<double>(spin) * 100.0 + static_cast<double>(yield) * 50.0) / static_cast<double>(total);
    }
};

// =============================================================================
// Adaptive Spin Waiter
// =============================================================================

/**
 * @brief Adaptive spin-wait implementation
 * 
 * Automatically transitions between stages based on iteration counts.
 * Thread-safe for single-writer usage (one waiting thread).
 */
class AdaptiveSpinWaiter {
public:
    /**
     * @brief Create waiter with given configuration
     */
    explicit AdaptiveSpinWaiter(const AdaptiveSpinConfig& config = AdaptiveSpinConfig::balanced())
        : config_(config)
        , current_stage_(SpinStage::Spin)
        , iterations_in_stage_(0)
    {}
    
    /**
     * @brief Wait for one iteration
     * 
     * Performs appropriate wait action based on current stage,
     * then potentially transitions to next stage.
     */
    void wait() noexcept {
        stats_.total_iterations.fetch_add(1, std::memory_order_relaxed);
        
        switch (current_stage_) {
            case SpinStage::Spin:
                spin_wait();
                break;
            case SpinStage::Yield:
                yield_wait();
                break;
            case SpinStage::Sleep:
                sleep_wait();
                break;
        }
        
        advance_stage();
    }
    
    /**
     * @brief Signal that a change was detected
     * 
     * If configured, resets to spin stage for lowest latency.
     */
    void on_change_detected() noexcept {
        if (config_.reset_on_change && current_stage_ != SpinStage::Spin) {
            current_stage_ = SpinStage::Spin;
            iterations_in_stage_ = 0;
            stats_.change_resets.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    /**
     * @brief Reset to initial spin state
     */
    void reset() noexcept {
        current_stage_ = SpinStage::Spin;
        iterations_in_stage_ = 0;
    }
    
    /**
     * @brief Get current stage
     */
    [[nodiscard]] SpinStage current_stage() const noexcept {
        return current_stage_;
    }
    
    /**
     * @brief Get statistics
     */
    [[nodiscard]] const AdaptiveSpinStats& stats() const noexcept {
        return stats_;
    }
    
    /**
     * @brief Reset statistics
     */
    void reset_stats() noexcept {
        stats_.reset();
    }
    
    /**
     * @brief Get configuration
     */
    [[nodiscard]] const AdaptiveSpinConfig& config() const noexcept {
        return config_;
    }
    
    /**
     * @brief Update configuration
     */
    void set_config(const AdaptiveSpinConfig& config) noexcept {
        config_ = config;
        reset();
    }

private:
    void spin_wait() noexcept {
        stats_.spin_iterations.fetch_add(1, std::memory_order_relaxed);
        // CPU pause instruction (x86: _mm_pause, or compiler barrier)
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            __builtin_ia32_pause();
        #else
            std::atomic_thread_fence(std::memory_order_acquire);
        #endif
    }
    
    void yield_wait() noexcept {
        stats_.yield_iterations.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::yield();
    }
    
    void sleep_wait() noexcept {
        stats_.sleep_iterations.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::microseconds(config_.sleep_us));
    }
    
    void advance_stage() noexcept {
        ++iterations_in_stage_;
        
        switch (current_stage_) {
            case SpinStage::Spin:
                if (iterations_in_stage_ >= config_.spin_iterations) {
                    current_stage_ = SpinStage::Yield;
                    iterations_in_stage_ = 0;
                    stats_.stage_transitions.fetch_add(1, std::memory_order_relaxed);
                }
                break;
                
            case SpinStage::Yield:
                if (iterations_in_stage_ >= config_.yield_iterations) {
                    current_stage_ = SpinStage::Sleep;
                    iterations_in_stage_ = 0;
                    stats_.stage_transitions.fetch_add(1, std::memory_order_relaxed);
                }
                break;
                
            case SpinStage::Sleep:
                // Stay in sleep stage until change is detected
                break;
        }
    }
    
    AdaptiveSpinConfig config_;
    SpinStage current_stage_;
    uint32_t iterations_in_stage_;
    AdaptiveSpinStats stats_;
};

} // namespace quaxis::shm
