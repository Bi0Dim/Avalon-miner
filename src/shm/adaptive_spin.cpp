/**
 * @file adaptive_spin.cpp
 * @brief Реализация адаптивного ожидания
 */

#include "adaptive_spin.hpp"

#include <thread>

namespace quaxis::shm {

AdaptiveSpinWait::AdaptiveSpinWait(const AdaptiveSpinConfig& config)
    : config_(config) {}

uint64_t AdaptiveSpinWait::wait_for_change(
    std::atomic<uint64_t>& sequence,
    uint64_t expected
) {
    // Начинаем с фазы 1
    reset();
    
    while (true) {
        // Проверяем на изменение
        uint64_t current = sequence.load(std::memory_order_acquire);
        if (current != expected) {
            reset();  // Сбрасываем для следующего ожидания
            return current;
        }
        
        // Выполняем шаг ожидания
        do_wait_step();
    }
}

void AdaptiveSpinWait::reset() {
    phase_ = 1;
    iteration_ = 0;
}

int AdaptiveSpinWait::current_phase() const noexcept {
    return phase_;
}

double AdaptiveSpinWait::estimated_cpu_percent() const noexcept {
    switch (phase_) {
        case 1: return 100.0;
        case 2: return 50.0;
        case 3: return 5.0;
        default: return 0.0;
    }
}

void AdaptiveSpinWait::set_config(const AdaptiveSpinConfig& config) {
    config_ = config;
}

const AdaptiveSpinConfig& AdaptiveSpinWait::config() const noexcept {
    return config_;
}

bool AdaptiveSpinWait::do_wait_step() {
    if (!config_.enabled) {
        // Если адаптивный режим отключён - всегда spin
        cpu_pause();
        return true;
    }
    
    ++iteration_;
    
    switch (phase_) {
        case 1:
            // Фаза 1: Spin с pause
            cpu_pause();
            if (iteration_ >= config_.spin_phase1_iterations) {
                advance_phase();
            }
            break;
            
        case 2:
            // Фаза 2: Yield
            std::this_thread::yield();
            if (iteration_ >= config_.spin_phase2_iterations) {
                advance_phase();
            }
            break;
            
        case 3:
            // Фаза 3: Sleep
            std::this_thread::sleep_for(
                std::chrono::microseconds(config_.sleep_us)
            );
            // Остаёмся в фазе 3 пока не произойдёт изменение
            break;
    }
    
    return true;
}

void AdaptiveSpinWait::advance_phase() {
    if (phase_ < 3) {
        ++phase_;
        iteration_ = 0;
    }
}

} // namespace quaxis::shm
