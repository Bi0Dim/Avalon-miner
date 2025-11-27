/**
 * @file alert_manager.hpp
 * @brief Управление алертами для Predictive Maintenance
 * 
 * Уровни алертов:
 * - Info: информационные сообщения
 * - Warning: предупреждения, требующие внимания
 * - Critical: критические ситуации
 * - Emergency: аварийные ситуации, требующие немедленного действия
 */

#pragma once

#include "../core/types.hpp"
#include "health_monitor.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace quaxis::monitoring {

// =============================================================================
// Уровни алертов
// =============================================================================

/**
 * @brief Уровень алерта
 */
enum class AlertLevel {
    Info,       ///< Информационное сообщение
    Warning,    ///< Предупреждение
    Critical,   ///< Критическое состояние
    Emergency   ///< Аварийное состояние
};

/**
 * @brief Преобразовать уровень в строку
 */
[[nodiscard]] constexpr std::string_view to_string(AlertLevel level) noexcept {
    switch (level) {
        case AlertLevel::Info:      return "INFO";
        case AlertLevel::Warning:   return "WARNING";
        case AlertLevel::Critical:  return "CRITICAL";
        case AlertLevel::Emergency: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Типы алертов
// =============================================================================

/**
 * @brief Тип алерта
 */
enum class AlertType {
    TemperatureHigh,        ///< Высокая температура
    TemperatureTrend,       ///< Быстрый рост температуры
    HashrateDropped,        ///< Падение хешрейта
    ErrorRateHigh,          ///< Высокий error rate
    ChipOffline,            ///< Чип отключился
    PowerAnomaly,           ///< Аномалия питания
    ConnectionLost,         ///< Потеря соединения
    JobTimeout,             ///< Таймаут получения задания
    BlockFound,             ///< Найден блок (info)
    SystemRestart,          ///< Система перезапущена
    ThrottleActivated,      ///< Активировано снижение частоты
    Custom                  ///< Пользовательский алерт
};

/**
 * @brief Преобразовать тип в строку
 */
[[nodiscard]] constexpr std::string_view to_string(AlertType type) noexcept {
    switch (type) {
        case AlertType::TemperatureHigh:    return "TEMP_HIGH";
        case AlertType::TemperatureTrend:   return "TEMP_TREND";
        case AlertType::HashrateDropped:    return "HASHRATE_DROP";
        case AlertType::ErrorRateHigh:      return "ERROR_RATE";
        case AlertType::ChipOffline:        return "CHIP_OFFLINE";
        case AlertType::PowerAnomaly:       return "POWER_ANOMALY";
        case AlertType::ConnectionLost:     return "CONN_LOST";
        case AlertType::JobTimeout:         return "JOB_TIMEOUT";
        case AlertType::BlockFound:         return "BLOCK_FOUND";
        case AlertType::SystemRestart:      return "SYS_RESTART";
        case AlertType::ThrottleActivated:  return "THROTTLE_ON";
        case AlertType::Custom:             return "CUSTOM";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Автоматические действия
// =============================================================================

/**
 * @brief Автоматическое действие при алерте
 */
enum class AlertAction {
    None,               ///< Нет действия
    LogOnly,            ///< Только логирование
    Notify,             ///< Уведомление
    ThrottleFrequency,  ///< Снизить частоту
    RestartMining,      ///< Перезапустить майнинг
    EmergencyShutdown   ///< Аварийное отключение
};

/**
 * @brief Преобразовать действие в строку
 */
[[nodiscard]] constexpr std::string_view to_string(AlertAction action) noexcept {
    switch (action) {
        case AlertAction::None:              return "NONE";
        case AlertAction::LogOnly:           return "LOG";
        case AlertAction::Notify:            return "NOTIFY";
        case AlertAction::ThrottleFrequency: return "THROTTLE";
        case AlertAction::RestartMining:     return "RESTART";
        case AlertAction::EmergencyShutdown: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Структура алерта
// =============================================================================

/**
 * @brief Алерт
 */
struct Alert {
    /// @brief Уникальный ID алерта
    uint64_t id = 0;
    
    /// @brief Уровень
    AlertLevel level = AlertLevel::Info;
    
    /// @brief Тип
    AlertType type = AlertType::Custom;
    
    /// @brief Сообщение
    std::string message;
    
    /// @brief Детали (опционально)
    std::string details;
    
    /// @brief Время создания
    std::chrono::steady_clock::time_point created_at;
    
    /// @brief Подтверждён (acknowledged)
    bool acknowledged = false;
    
    /// @brief Время подтверждения
    std::optional<std::chrono::steady_clock::time_point> acknowledged_at;
    
    /// @brief Разрешён (resolved)
    bool resolved = false;
    
    /// @brief Время разрешения
    std::optional<std::chrono::steady_clock::time_point> resolved_at;
    
    /// @brief Рекомендуемое действие
    AlertAction recommended_action = AlertAction::None;
    
    /// @brief Действие выполнено
    bool action_taken = false;
    
    /// @brief Связанные данные (например, chip_id или температура)
    double value = 0.0;
    uint8_t chip_id = 0;
};

// =============================================================================
// Конфигурация Alert Manager
// =============================================================================

/**
 * @brief Конфигурация Alert Manager
 */
struct AlertConfig {
    /// @brief Максимальное количество хранимых алертов
    std::size_t max_alerts = 1000;
    
    /// @brief Автоматически разрешать старые алерты (секунды)
    uint32_t auto_resolve_timeout = 3600;
    
    /// @brief Минимальный интервал между одинаковыми алертами (секунды)
    uint32_t duplicate_cooldown = 60;
    
    /// @brief Включить автодействия
    bool auto_actions_enabled = true;
};

// =============================================================================
// Alert Manager
// =============================================================================

/**
 * @brief Callback для обработки алертов
 */
using AlertCallback = std::function<void(const Alert&)>;

/**
 * @brief Callback для выполнения действия
 */
using ActionCallback = std::function<bool(AlertAction, const Alert&)>;

/**
 * @brief Менеджер алертов
 */
class AlertManager {
public:
    /**
     * @brief Создать менеджер с конфигурацией
     */
    explicit AlertManager(const AlertConfig& config);
    
    ~AlertManager();
    
    // Запрещаем копирование
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;
    
    // =========================================================================
    // Создание алертов
    // =========================================================================
    
    /**
     * @brief Создать алерт
     * 
     * @param level Уровень
     * @param type Тип
     * @param message Сообщение
     * @param action Рекомендуемое действие
     * @return ID алерта
     */
    uint64_t create_alert(
        AlertLevel level,
        AlertType type,
        const std::string& message,
        AlertAction action = AlertAction::LogOnly
    );
    
    /**
     * @brief Создать алерт с деталями
     */
    uint64_t create_alert_detailed(
        AlertLevel level,
        AlertType type,
        const std::string& message,
        const std::string& details,
        AlertAction action = AlertAction::LogOnly,
        double value = 0.0,
        uint8_t chip_id = 0
    );
    
    /**
     * @brief Создать алерт на основе метрик здоровья
     */
    void check_health_metrics(const HealthMetrics& metrics);
    
    // =========================================================================
    // Управление алертами
    // =========================================================================
    
    /**
     * @brief Подтвердить алерт
     */
    bool acknowledge(uint64_t alert_id);
    
    /**
     * @brief Разрешить алерт
     */
    bool resolve(uint64_t alert_id);
    
    /**
     * @brief Подтвердить все алерты указанного уровня
     */
    void acknowledge_all(AlertLevel level);
    
    /**
     * @brief Разрешить все алерты указанного типа
     */
    void resolve_all_of_type(AlertType type);
    
    // =========================================================================
    // Получение алертов
    // =========================================================================
    
    /**
     * @brief Получить алерт по ID
     */
    [[nodiscard]] std::optional<Alert> get_alert(uint64_t alert_id) const;
    
    /**
     * @brief Получить все активные (неразрешённые) алерты
     */
    [[nodiscard]] std::vector<Alert> get_active_alerts() const;
    
    /**
     * @brief Получить алерты указанного уровня
     */
    [[nodiscard]] std::vector<Alert> get_alerts_by_level(AlertLevel level) const;
    
    /**
     * @brief Получить алерты указанного типа
     */
    [[nodiscard]] std::vector<Alert> get_alerts_by_type(AlertType type) const;
    
    /**
     * @brief Получить последние N алертов
     */
    [[nodiscard]] std::vector<Alert> get_recent_alerts(std::size_t count) const;
    
    /**
     * @brief Получить количество активных алертов по уровням
     */
    struct AlertCounts {
        std::size_t info = 0;
        std::size_t warning = 0;
        std::size_t critical = 0;
        std::size_t emergency = 0;
        std::size_t total = 0;
    };
    
    [[nodiscard]] AlertCounts get_counts() const;
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для новых алертов
     */
    void set_alert_callback(AlertCallback callback);
    
    /**
     * @brief Установить callback для выполнения действий
     */
    void set_action_callback(ActionCallback callback);
    
    // =========================================================================
    // Очистка
    // =========================================================================
    
    /**
     * @brief Очистить все алерты
     */
    void clear_all();
    
    /**
     * @brief Очистить разрешённые алерты старше указанного времени
     */
    void cleanup_old(std::chrono::seconds max_age);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::monitoring
