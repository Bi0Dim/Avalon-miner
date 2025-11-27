/**
 * @file health_monitor.hpp
 * @brief Мониторинг здоровья ASIC и предсказание сбоев
 * 
 * Predictive Maintenance обеспечивает +5-15% uptime за счёт:
 * - Мониторинга температуры с трендами
 * - Отслеживания хешрейта и error rate
 * - Предсказания сбоев и превентивных действий
 * - Автоматического снижения частоты при перегреве
 */

#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"

#include <cstdint>
#include <chrono>
#include <vector>
#include <deque>
#include <optional>
#include <atomic>
#include <mutex>
#include <memory>

namespace quaxis::monitoring {

// =============================================================================
// Конфигурация мониторинга здоровья
// =============================================================================

/**
 * @brief Конфигурация Health Monitor
 */
struct HealthConfig {
    /// @brief Включить мониторинг здоровья
    bool enabled = true;
    
    /// @brief Интервал сбора метрик (секунды)
    uint32_t collection_interval = 5;
    
    /// @brief Порог температуры для предупреждения (°C)
    double temp_warning = 75.0;
    
    /// @brief Порог температуры для критического состояния (°C)
    double temp_critical = 85.0;
    
    /// @brief Порог температуры для аварийного отключения (°C)
    double temp_emergency = 95.0;
    
    /// @brief Порог падения хешрейта для предупреждения (%)
    double hashrate_warning_drop = 10.0;
    
    /// @brief Порог падения хешрейта для критического состояния (%)
    double hashrate_critical_drop = 25.0;
    
    /// @brief Порог error rate для предупреждения (%)
    double error_rate_warning = 1.0;
    
    /// @brief Порог error rate для критического состояния (%)
    double error_rate_critical = 5.0;
    
    /// @brief Количество точек для анализа тренда
    std::size_t trend_window_size = 60;
    
    /// @brief Включить автоматическое снижение частоты при перегреве
    bool auto_throttle = true;
    
    /// @brief Включить автоматический перезапуск при падении хешрейта
    bool auto_restart = true;
    
    /// @brief Время cooldown после перезапуска (секунды)
    uint32_t restart_cooldown = 300;
};

// =============================================================================
// Метрики здоровья
// =============================================================================

/**
 * @brief Метрики температуры
 */
struct TemperatureMetrics {
    double current = 0.0;       ///< Текущая температура (°C)
    double average = 0.0;       ///< Средняя за период (°C)
    double max = 0.0;           ///< Максимальная за период (°C)
    double min = 0.0;           ///< Минимальная за период (°C)
    double trend = 0.0;         ///< Тренд изменения (°C/мин)
    
    std::chrono::steady_clock::time_point last_update;
};

/**
 * @brief Метрики хешрейта
 */
struct HashrateMetrics {
    double current = 0.0;       ///< Текущий хешрейт (H/s)
    double nominal = 0.0;       ///< Номинальный хешрейт (H/s)
    double average = 0.0;       ///< Средний за период (H/s)
    double efficiency = 0.0;    ///< Эффективность (current/nominal)
    double variance = 0.0;      ///< Дисперсия
    
    std::chrono::steady_clock::time_point last_update;
};

/**
 * @brief Метрики ошибок
 */
struct ErrorMetrics {
    uint64_t hw_errors = 0;         ///< Hardware ошибки
    uint64_t rejected_shares = 0;   ///< Отклонённые шары
    uint64_t stale_shares = 0;      ///< Устаревшие шары
    uint64_t total_shares = 0;      ///< Всего шар
    double error_rate = 0.0;        ///< Процент ошибок
    
    std::chrono::steady_clock::time_point last_update;
};

/**
 * @brief Метрики питания
 */
struct PowerMetrics {
    double voltage = 0.0;       ///< Напряжение (V)
    double current = 0.0;       ///< Ток (A)
    double power = 0.0;         ///< Мощность (W)
    double efficiency = 0.0;    ///< Энергоэффективность (J/TH)
    
    std::chrono::steady_clock::time_point last_update;
};

/**
 * @brief Метрики uptime
 */
struct UptimeMetrics {
    std::chrono::steady_clock::time_point start_time;   ///< Время запуска
    std::chrono::seconds uptime{0};                     ///< Время работы
    uint32_t restarts = 0;                              ///< Количество перезапусков
    double availability = 0.0;                          ///< Доступность (%)
    
    std::chrono::steady_clock::time_point last_restart;
};

/**
 * @brief Состояние здоровья чипа
 */
struct ChipHealth {
    uint8_t chip_id = 0;
    double temperature = 0.0;
    double hashrate = 0.0;
    uint32_t errors = 0;
    bool active = true;
};

/**
 * @brief Полные метрики здоровья ASIC
 */
struct HealthMetrics {
    TemperatureMetrics temperature;
    HashrateMetrics hashrate;
    ErrorMetrics errors;
    PowerMetrics power;
    UptimeMetrics uptime;
    
    /// @brief Метрики по чипам
    std::vector<ChipHealth> chips;
    
    /// @brief Общий статус
    enum class Status {
        Healthy,        ///< Всё в норме
        Warning,        ///< Предупреждение
        Critical,       ///< Критическое состояние
        Emergency       ///< Аварийное состояние
    };
    
    Status overall_status = Status::Healthy;
    std::string status_message;
    
    std::chrono::steady_clock::time_point collected_at;
};

// =============================================================================
// Health Monitor
// =============================================================================

/**
 * @brief Монитор здоровья ASIC
 * 
 * Собирает метрики, анализирует тренды и принимает превентивные меры.
 */
class HealthMonitor {
public:
    /**
     * @brief Создать монитор с конфигурацией
     */
    explicit HealthMonitor(const HealthConfig& config);
    
    ~HealthMonitor();
    
    // Запрещаем копирование
    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;
    
    // =========================================================================
    // Обновление метрик
    // =========================================================================
    
    /**
     * @brief Обновить температуру
     * 
     * @param chip_id ID чипа (0 = общая температура)
     * @param temperature Температура в °C
     */
    void update_temperature(uint8_t chip_id, double temperature);
    
    /**
     * @brief Обновить хешрейт
     * 
     * @param hashrate Текущий хешрейт (H/s)
     */
    void update_hashrate(double hashrate);
    
    /**
     * @brief Установить номинальный хешрейт
     */
    void set_nominal_hashrate(double hashrate);
    
    /**
     * @brief Записать ошибку
     * 
     * @param hw_error true если это hardware ошибка
     * @param rejected true если share отклонён
     * @param stale true если share устарел
     */
    void record_error(bool hw_error = false, bool rejected = false, bool stale = false);
    
    /**
     * @brief Записать успешный share
     */
    void record_share();
    
    /**
     * @brief Обновить метрики питания
     */
    void update_power(double voltage, double current);
    
    /**
     * @brief Записать перезапуск
     */
    void record_restart();
    
    /**
     * @brief Обновить статус чипа
     */
    void update_chip_status(uint8_t chip_id, bool active, double hashrate, uint32_t errors);
    
    // =========================================================================
    // Получение метрик
    // =========================================================================
    
    /**
     * @brief Получить текущие метрики здоровья
     */
    [[nodiscard]] HealthMetrics get_metrics() const;
    
    /**
     * @brief Получить текущий статус
     */
    [[nodiscard]] HealthMetrics::Status get_status() const;
    
    /**
     * @brief Получить сообщение о статусе
     */
    [[nodiscard]] std::string get_status_message() const;
    
    /**
     * @brief Проверить, требуется ли действие
     */
    [[nodiscard]] bool requires_action() const;
    
    // =========================================================================
    // Анализ трендов
    // =========================================================================
    
    /**
     * @brief Получить тренд температуры (°C/мин)
     */
    [[nodiscard]] double get_temperature_trend() const;
    
    /**
     * @brief Получить тренд хешрейта (H/s/мин)
     */
    [[nodiscard]] double get_hashrate_trend() const;
    
    /**
     * @brief Предсказать время до критической температуры
     * 
     * @return Время в секундах или nullopt если тренд не позволяет предсказать
     */
    [[nodiscard]] std::optional<std::chrono::seconds> predict_thermal_critical() const;
    
    // =========================================================================
    // Управление
    // =========================================================================
    
    /**
     * @brief Запустить фоновый мониторинг
     */
    void start();
    
    /**
     * @brief Остановить фоновый мониторинг
     */
    void stop();
    
    /**
     * @brief Проверить состояние и принять меры
     * 
     * @return true если требуется немедленное действие
     */
    bool check_and_act();
    
    /**
     * @brief Сбросить метрики
     */
    void reset();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Преобразовать статус в строку
 */
[[nodiscard]] constexpr std::string_view to_string(HealthMetrics::Status status) noexcept {
    switch (status) {
        case HealthMetrics::Status::Healthy:   return "Healthy";
        case HealthMetrics::Status::Warning:   return "Warning";
        case HealthMetrics::Status::Critical:  return "Critical";
        case HealthMetrics::Status::Emergency: return "Emergency";
        default: return "Unknown";
    }
}

/**
 * @brief Вычислить линейный тренд по точкам
 * 
 * @param values Значения для анализа
 * @return Наклон тренда (единица/шаг)
 */
[[nodiscard]] double calculate_trend(const std::deque<double>& values);

} // namespace quaxis::monitoring
