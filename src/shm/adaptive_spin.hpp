/**
 * @file adaptive_spin.hpp
 * @brief Адаптивный spin-wait для SHM подписчика
 * 
 * Многоступенчатый цикл ожидания:
 * 1. Активный spin (минимальная латентность)
 * 2. Yield (освобождение CPU другим потокам)
 * 3. Sleep (экономия энергии при длительном ожидании)
 * 
 * Автоматически настраивается на основе паттернов получения данных.
 */

#pragma once

#include "../core/types.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace quaxis::shm {

// =============================================================================
// Конфигурация
// =============================================================================

/**
 * @brief Конфигурация адаптивного spin-wait
 */
struct AdaptiveSpinConfig {
    /// @brief Максимальное количество spin итераций перед yield
    uint32_t spin_iterations{1000};
    
    /// @brief Максимальное количество yield перед sleep
    uint32_t yield_iterations{100};
    
    /// @brief Начальная длительность sleep (микросекунды)
    uint32_t initial_sleep_us{10};
    
    /// @brief Максимальная длительность sleep (микросекунды)
    uint32_t max_sleep_us{1000};
    
    /// @brief Множитель увеличения sleep
    double sleep_backoff_multiplier{1.5};
    
    /// @brief Включить сбор статистики CPU usage
    bool track_cpu_usage{true};
    
    /// @brief Окно для расчёта CPU usage (мс)
    uint32_t cpu_usage_window_ms{1000};
};

// =============================================================================
// Статистика
// =============================================================================

/**
 * @brief Статистика адаптивного spin-wait
 */
struct AdaptiveSpinStats {
    /// @brief Общее время в spin режиме (нс)
    uint64_t spin_time_ns{0};
    
    /// @brief Общее время в yield режиме (нс)
    uint64_t yield_time_ns{0};
    
    /// @brief Общее время в sleep режиме (нс)
    uint64_t sleep_time_ns{0};
    
    /// @brief Количество успешных получений данных
    uint64_t successful_receives{0};
    
    /// @brief Количество итераций spin
    uint64_t spin_iterations{0};
    
    /// @brief Количество yield вызовов
    uint64_t yield_count{0};
    
    /// @brief Количество sleep вызовов
    uint64_t sleep_count{0};
    
    /// @brief Оценка использования CPU (0.0 - 1.0)
    double estimated_cpu_usage{0.0};
};

// =============================================================================
// Адаптивный Spin
// =============================================================================

/**
 * @brief Callback для проверки наличия данных
 * 
 * @return true если данные доступны
 */
using DataAvailableCheck = std::function<bool()>;

/**
 * @brief Адаптивный spin-wait механизм
 * 
 * Обеспечивает оптимальный баланс между латентностью и CPU usage:
 * - При частых данных: преимущественно spin (низкая латентность)
 * - При редких данных: переход к yield/sleep (экономия CPU)
 */
class AdaptiveSpin {
public:
    /**
     * @brief Создать с конфигурацией
     */
    explicit AdaptiveSpin(const AdaptiveSpinConfig& config);
    
    ~AdaptiveSpin();
    
    // Запрещаем копирование
    AdaptiveSpin(const AdaptiveSpin&) = delete;
    AdaptiveSpin& operator=(const AdaptiveSpin&) = delete;
    
    // =========================================================================
    // Ожидание данных
    // =========================================================================
    
    /**
     * @brief Ожидать данные с адаптивным spin-wait
     * 
     * @param check Функция проверки наличия данных
     * @param timeout Таймаут (0 = без таймаута)
     * @return true если данные получены, false если таймаут
     */
    bool wait_for_data(DataAvailableCheck check, 
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(0));
    
    /**
     * @brief Сигнализировать об успешном получении данных
     * 
     * Сбрасывает счётчики backoff.
     */
    void signal_data_received();
    
    // =========================================================================
    // Управление
    // =========================================================================
    
    /**
     * @brief Включить/выключить адаптивный режим
     * 
     * При выключении используется простой spin.
     */
    void set_enabled(bool enabled);
    
    /**
     * @brief Проверить, включён ли адаптивный режим
     */
    [[nodiscard]] bool is_enabled() const noexcept;
    
    /**
     * @brief Сбросить состояние к начальному
     */
    void reset();
    
    // =========================================================================
    // Статистика
    // =========================================================================
    
    /**
     * @brief Получить статистику
     */
    [[nodiscard]] AdaptiveSpinStats get_stats() const;
    
    /**
     * @brief Получить текущую оценку CPU usage (%)
     */
    [[nodiscard]] double get_cpu_usage_percent() const;
    
    /**
     * @brief Сбросить статистику
     */
    void reset_stats();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief RAII guard для сброса состояния spin
 */
class SpinGuard {
public:
    explicit SpinGuard(AdaptiveSpin& spin) : spin_(spin) {}
    ~SpinGuard() { spin_.reset(); }
    
    SpinGuard(const SpinGuard&) = delete;
    SpinGuard& operator=(const SpinGuard&) = delete;
    
private:
    AdaptiveSpin& spin_;
};

} // namespace quaxis::shm
