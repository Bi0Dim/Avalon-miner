/**
 * @file adaptive_spin.cpp
 * @brief Реализация адаптивного spin-wait для SHM
 */

#include "adaptive_spin.hpp"

#include <algorithm>
#include <cmath>

namespace quaxis::shm {

// =============================================================================
// Helper: CPU pause hint for spin-wait
// =============================================================================

namespace {

/**
 * @brief CPU hint for spin-wait loop
 * 
 * Reduces CPU power consumption during spin-wait by hinting
 * to the processor that we're in a spin loop.
 */
inline void cpu_pause_hint() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#endif
}

} // anonymous namespace

// =============================================================================
// Реализация
// =============================================================================

struct AdaptiveSpin::Impl {
    AdaptiveSpinConfig config;
    
    // Состояние
    std::atomic<bool> enabled{true};
    
    // Текущие параметры backoff
    std::atomic<uint32_t> current_sleep_us{0};
    
    // Статистика
    std::atomic<uint64_t> spin_time_ns{0};
    std::atomic<uint64_t> yield_time_ns{0};
    std::atomic<uint64_t> sleep_time_ns{0};
    std::atomic<uint64_t> successful_receives{0};
    std::atomic<uint64_t> spin_iterations{0};
    std::atomic<uint64_t> yield_count{0};
    std::atomic<uint64_t> sleep_count{0};
    
    // Для расчёта CPU usage
    std::chrono::steady_clock::time_point last_cpu_calc_time;
    uint64_t last_active_time_ns{0};
    std::atomic<double> cpu_usage{0.0};
    
    explicit Impl(const AdaptiveSpinConfig& cfg) 
        : config(cfg)
        , current_sleep_us(cfg.initial_sleep_us) {
        last_cpu_calc_time = std::chrono::steady_clock::now();
    }
    
    bool wait_for_data(DataAvailableCheck check, std::chrono::milliseconds timeout) {
        if (!enabled.load(std::memory_order_relaxed)) {
            // Простой spin без адаптации
            return simple_spin(check, timeout);
        }
        
        auto start = std::chrono::steady_clock::now();
        auto deadline = timeout.count() > 0 
            ? start + timeout 
            : std::chrono::steady_clock::time_point::max();
        
        uint32_t spin_count = 0;
        uint32_t yield_count_local = 0;
        
        while (true) {
            // Проверка данных
            if (check()) {
                // Данные получены
                signal_success();
                update_stats_on_receive(start);
                return true;
            }
            
            // Проверка таймаута
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return false;
            }
            
            // Адаптивное ожидание
            if (spin_count < config.spin_iterations) {
                // Фаза 1: Активный spin
                ++spin_count;
                spin_iterations.fetch_add(1, std::memory_order_relaxed);
                
                cpu_pause_hint();
            } else if (yield_count_local < config.yield_iterations) {
                // Фаза 2: Yield
                ++yield_count_local;
                this->yield_count.fetch_add(1, std::memory_order_relaxed);
                
                auto yield_start = std::chrono::steady_clock::now();
                std::this_thread::yield();
                auto yield_end = std::chrono::steady_clock::now();
                
                yield_time_ns.fetch_add(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            yield_end - yield_start
                        ).count()
                    ),
                    std::memory_order_relaxed
                );
            } else {
                // Фаза 3: Sleep с backoff
                sleep_count.fetch_add(1, std::memory_order_relaxed);
                
                uint32_t sleep_duration = current_sleep_us.load(std::memory_order_relaxed);
                
                auto sleep_start = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::microseconds(sleep_duration));
                auto sleep_end = std::chrono::steady_clock::now();
                
                sleep_time_ns.fetch_add(
                    static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            sleep_end - sleep_start
                        ).count()
                    ),
                    std::memory_order_relaxed
                );
                
                // Увеличить sleep с backoff
                increase_sleep();
                
                // Сброс spin/yield счётчиков для следующей итерации
                spin_count = 0;
                yield_count_local = 0;
            }
        }
    }
    
    bool simple_spin(DataAvailableCheck check, std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();
        auto deadline = timeout.count() > 0 
            ? start + timeout 
            : std::chrono::steady_clock::time_point::max();
        
        while (true) {
            if (check()) {
                return true;
            }
            
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            
            cpu_pause_hint();
        }
    }
    
    void signal_success() {
        successful_receives.fetch_add(1, std::memory_order_relaxed);
        
        // Сбрасываем sleep к минимуму при успехе
        current_sleep_us.store(config.initial_sleep_us, std::memory_order_relaxed);
    }
    
    void increase_sleep() {
        uint32_t current = current_sleep_us.load(std::memory_order_relaxed);
        uint32_t next = static_cast<uint32_t>(
            static_cast<double>(current) * config.sleep_backoff_multiplier
        );
        next = std::min(next, config.max_sleep_us);
        current_sleep_us.store(next, std::memory_order_relaxed);
    }
    
    void update_stats_on_receive(std::chrono::steady_clock::time_point start) {
        if (!config.track_cpu_usage) {
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start);
        
        // Приблизительное время в активном spin (исключая yield/sleep)
        uint64_t active_ns = static_cast<uint64_t>(elapsed.count()) 
            - yield_time_ns.load(std::memory_order_relaxed)
            - sleep_time_ns.load(std::memory_order_relaxed);
        
        spin_time_ns.fetch_add(active_ns, std::memory_order_relaxed);
        
        // Обновляем CPU usage периодически
        update_cpu_usage();
    }
    
    void update_cpu_usage() {
        auto now = std::chrono::steady_clock::now();
        auto window = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_cpu_calc_time
        );
        
        if (window.count() < static_cast<int64_t>(config.cpu_usage_window_ms)) {
            return;
        }
        
        uint64_t total_ns = static_cast<uint64_t>(window.count()) * 1'000'000;
        uint64_t active_ns = spin_time_ns.load(std::memory_order_relaxed) - last_active_time_ns;
        
        if (total_ns > 0) {
            double usage = static_cast<double>(active_ns) / static_cast<double>(total_ns);
            usage = std::clamp(usage, 0.0, 1.0);
            cpu_usage.store(usage, std::memory_order_relaxed);
        }
        
        last_active_time_ns = spin_time_ns.load(std::memory_order_relaxed);
        last_cpu_calc_time = now;
    }
    
    AdaptiveSpinStats get_stats() const {
        AdaptiveSpinStats stats;
        stats.spin_time_ns = spin_time_ns.load(std::memory_order_relaxed);
        stats.yield_time_ns = yield_time_ns.load(std::memory_order_relaxed);
        stats.sleep_time_ns = sleep_time_ns.load(std::memory_order_relaxed);
        stats.successful_receives = successful_receives.load(std::memory_order_relaxed);
        stats.spin_iterations = spin_iterations.load(std::memory_order_relaxed);
        stats.yield_count = yield_count.load(std::memory_order_relaxed);
        stats.sleep_count = sleep_count.load(std::memory_order_relaxed);
        stats.estimated_cpu_usage = cpu_usage.load(std::memory_order_relaxed);
        return stats;
    }
    
    void reset() {
        current_sleep_us.store(config.initial_sleep_us, std::memory_order_relaxed);
    }
    
    void reset_stats() {
        spin_time_ns.store(0, std::memory_order_relaxed);
        yield_time_ns.store(0, std::memory_order_relaxed);
        sleep_time_ns.store(0, std::memory_order_relaxed);
        successful_receives.store(0, std::memory_order_relaxed);
        spin_iterations.store(0, std::memory_order_relaxed);
        yield_count.store(0, std::memory_order_relaxed);
        sleep_count.store(0, std::memory_order_relaxed);
        cpu_usage.store(0.0, std::memory_order_relaxed);
        last_active_time_ns = 0;
        last_cpu_calc_time = std::chrono::steady_clock::now();
    }
};

// =============================================================================
// Публичный API
// =============================================================================

AdaptiveSpin::AdaptiveSpin(const AdaptiveSpinConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

AdaptiveSpin::~AdaptiveSpin() = default;

bool AdaptiveSpin::wait_for_data(DataAvailableCheck check, std::chrono::milliseconds timeout) {
    return impl_->wait_for_data(std::move(check), timeout);
}

void AdaptiveSpin::signal_data_received() {
    impl_->signal_success();
}

void AdaptiveSpin::set_enabled(bool enabled) {
    impl_->enabled.store(enabled, std::memory_order_relaxed);
}

bool AdaptiveSpin::is_enabled() const noexcept {
    return impl_->enabled.load(std::memory_order_relaxed);
}

void AdaptiveSpin::reset() {
    impl_->reset();
}

AdaptiveSpinStats AdaptiveSpin::get_stats() const {
    return impl_->get_stats();
}

double AdaptiveSpin::get_cpu_usage_percent() const {
    return impl_->cpu_usage.load(std::memory_order_relaxed) * 100.0;
}

void AdaptiveSpin::reset_stats() {
    impl_->reset_stats();
}

} // namespace quaxis::shm
