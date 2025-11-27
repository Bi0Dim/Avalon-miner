/**
 * @file adaptive_spin.hpp
 * @brief Адаптивное ожидание изменений в shared memory
 * 
 * Трёхфазный алгоритм ожидания:
 * - Фаза 1: Spin с pause (~2000 итераций)
 * - Фаза 2: Yield (~2000 итераций)
 * - Фаза 3: Sleep (200 мкс)
 * 
 * Сброс при обнаружении изменения.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace quaxis::shm {

/**
 * @brief Конфигурация адаптивного ожидания
 */
struct AdaptiveSpinConfig {
    /// @brief Включить адаптивный режим
    bool enabled = true;
    
    /// @brief Количество итераций spin-pause (фаза 1)
    uint32_t spin_phase1_iterations = 2000;
    
    /// @brief Количество итераций yield (фаза 2)
    uint32_t spin_phase2_iterations = 2000;
    
    /// @brief Время sleep в микросекундах (фаза 3)
    uint32_t sleep_us = 200;
};

/**
 * @brief Адаптивное ожидание изменений
 * 
 * Оптимизирует баланс между латентностью и CPU usage.
 */
class AdaptiveSpinWait {
public:
    /**
     * @brief Создать с конфигурацией по умолчанию
     */
    AdaptiveSpinWait() = default;
    
    /**
     * @brief Создать с заданной конфигурацией
     */
    explicit AdaptiveSpinWait(const AdaptiveSpinConfig& config);
    
    /**
     * @brief Ждать изменения sequence
     * 
     * Блокирует до тех пор, пока sequence не изменится.
     * 
     * @param sequence Атомарная переменная для отслеживания
     * @param expected Ожидаемое текущее значение
     * @return uint64_t Новое значение sequence
     */
    uint64_t wait_for_change(
        std::atomic<uint64_t>& sequence,
        uint64_t expected
    );
    
    /**
     * @brief Сбросить состояние ожидания
     * 
     * Вызывается при обнаружении изменения для возврата к фазе 1.
     */
    void reset();
    
    /**
     * @brief Получить текущую фазу (1-3)
     */
    [[nodiscard]] int current_phase() const noexcept;
    
    /**
     * @brief Получить приблизительный % использования CPU
     * 
     * Основано на текущей фазе:
     * - Фаза 1: ~100%
     * - Фаза 2: ~50%
     * - Фаза 3: ~5%
     */
    [[nodiscard]] double estimated_cpu_percent() const noexcept;
    
    /**
     * @brief Обновить конфигурацию
     */
    void set_config(const AdaptiveSpinConfig& config);
    
    /**
     * @brief Получить текущую конфигурацию
     */
    [[nodiscard]] const AdaptiveSpinConfig& config() const noexcept;

private:
    AdaptiveSpinConfig config_;
    
    // Состояние
    int phase_{1};
    uint32_t iteration_{0};
    
    /**
     * @brief Выполнить одну итерацию ожидания
     * 
     * @return true если нужно продолжать ожидание
     */
    bool do_wait_step();
    
    /**
     * @brief Перейти к следующей фазе
     */
    void advance_phase();
};

/**
 * @brief CPU pause инструкция для spin-wait
 * 
 * Снижает энергопотребление и улучшает производительность
 * при spin-wait на x86.
 */
inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    // Fallback для других архитектур
    std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

} // namespace quaxis::shm
