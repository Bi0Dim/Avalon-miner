/**
 * @file alert_manager.cpp
 * @brief Реализация менеджера алертов
 */

#include "alert_manager.hpp"

#include <algorithm>
#include <map>

namespace quaxis::monitoring {

// =============================================================================
// Внутренняя реализация
// =============================================================================

struct AlertManager::Impl {
    AlertConfig config;
    
    mutable std::mutex alerts_mutex;
    std::vector<Alert> alerts;
    uint64_t next_alert_id = 1;
    
    AlertCallback alert_callback;
    ActionCallback action_callback;
    
    // Последние алерты по типу (для deduplication)
    std::map<AlertType, std::chrono::steady_clock::time_point> last_alert_time;
    
    explicit Impl(const AlertConfig& cfg) : config(cfg) {}
    
    bool should_deduplicate(AlertType type) {
        auto now = std::chrono::steady_clock::now();
        auto it = last_alert_time.find(type);
        
        if (it != last_alert_time.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second
            ).count();
            
            if (elapsed < static_cast<int64_t>(config.duplicate_cooldown)) {
                return true;  // Дедуплицировать
            }
        }
        
        last_alert_time[type] = now;
        return false;
    }
    
    void execute_action(AlertAction action, const Alert& alert) {
        if (!config.auto_actions_enabled) {
            return;
        }
        
        if (action == AlertAction::None || action == AlertAction::LogOnly) {
            return;
        }
        
        if (action_callback) {
            action_callback(action, alert);
        }
    }
    
    void cleanup_if_needed() {
        // Ограничиваем размер списка алертов
        while (alerts.size() > config.max_alerts) {
            // Удаляем самые старые разрешённые алерты
            auto it = std::find_if(alerts.begin(), alerts.end(),
                [](const Alert& a) { return a.resolved; });
            
            if (it != alerts.end()) {
                alerts.erase(it);
            } else {
                // Если все активные, удаляем самый старый
                alerts.erase(alerts.begin());
            }
        }
    }
};

// =============================================================================
// AlertManager публичный интерфейс
// =============================================================================

AlertManager::AlertManager(const AlertConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

AlertManager::~AlertManager() = default;

uint64_t AlertManager::create_alert(
    AlertLevel level,
    AlertType type,
    const std::string& message,
    AlertAction action
) {
    return create_alert_detailed(level, type, message, "", action, 0.0, 0);
}

uint64_t AlertManager::create_alert_detailed(
    AlertLevel level,
    AlertType type,
    const std::string& message,
    const std::string& details,
    AlertAction action,
    double value,
    uint8_t chip_id
) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    // Проверяем дедупликацию
    if (impl_->should_deduplicate(type)) {
        return 0;  // Дедуплицировано
    }
    
    Alert alert;
    alert.id = impl_->next_alert_id++;
    alert.level = level;
    alert.type = type;
    alert.message = message;
    alert.details = details;
    alert.created_at = std::chrono::steady_clock::now();
    alert.recommended_action = action;
    alert.value = value;
    alert.chip_id = chip_id;
    
    impl_->alerts.push_back(alert);
    impl_->cleanup_if_needed();
    
    // Вызываем callback
    if (impl_->alert_callback) {
        impl_->alert_callback(alert);
    }
    
    // Выполняем автодействие
    impl_->execute_action(action, alert);
    
    return alert.id;
}

void AlertManager::check_health_metrics(const HealthMetrics& metrics) {
    switch (metrics.overall_status) {
        case HealthMetrics::Status::Emergency:
            create_alert_detailed(
                AlertLevel::Emergency,
                AlertType::TemperatureHigh,
                "Аварийное состояние!",
                metrics.status_message,
                AlertAction::EmergencyShutdown,
                metrics.temperature.current,
                0
            );
            break;
            
        case HealthMetrics::Status::Critical:
            if (metrics.temperature.current >= 85.0) {
                create_alert_detailed(
                    AlertLevel::Critical,
                    AlertType::TemperatureHigh,
                    "Критическая температура",
                    metrics.status_message,
                    AlertAction::ThrottleFrequency,
                    metrics.temperature.current,
                    0
                );
            } else if (metrics.hashrate.efficiency < 0.75) {
                create_alert_detailed(
                    AlertLevel::Critical,
                    AlertType::HashrateDropped,
                    "Критическое падение хешрейта",
                    metrics.status_message,
                    AlertAction::RestartMining,
                    (1.0 - metrics.hashrate.efficiency) * 100.0,
                    0
                );
            } else if (metrics.errors.error_rate >= 5.0) {
                create_alert_detailed(
                    AlertLevel::Critical,
                    AlertType::ErrorRateHigh,
                    "Критический error rate",
                    metrics.status_message,
                    AlertAction::RestartMining,
                    metrics.errors.error_rate,
                    0
                );
            }
            break;
            
        case HealthMetrics::Status::Warning:
            if (metrics.temperature.current >= 75.0) {
                create_alert_detailed(
                    AlertLevel::Warning,
                    AlertType::TemperatureHigh,
                    "Повышенная температура",
                    metrics.status_message,
                    AlertAction::Notify,
                    metrics.temperature.current,
                    0
                );
            } else if (metrics.temperature.trend > 0.5) {
                create_alert_detailed(
                    AlertLevel::Warning,
                    AlertType::TemperatureTrend,
                    "Быстрый рост температуры",
                    metrics.status_message,
                    AlertAction::Notify,
                    metrics.temperature.trend,
                    0
                );
            }
            break;
            
        case HealthMetrics::Status::Healthy:
            // Всё в норме, ничего не делаем
            break;
    }
    
    // Проверяем отключённые чипы
    for (const auto& chip : metrics.chips) {
        if (!chip.active) {
            create_alert_detailed(
                AlertLevel::Warning,
                AlertType::ChipOffline,
                "Чип отключён",
                "",
                AlertAction::Notify,
                0.0,
                chip.chip_id
            );
        }
    }
}

bool AlertManager::acknowledge(uint64_t alert_id) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    auto it = std::find_if(impl_->alerts.begin(), impl_->alerts.end(),
        [alert_id](const Alert& a) { return a.id == alert_id; });
    
    if (it != impl_->alerts.end() && !it->acknowledged) {
        it->acknowledged = true;
        it->acknowledged_at = std::chrono::steady_clock::now();
        return true;
    }
    
    return false;
}

bool AlertManager::resolve(uint64_t alert_id) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    auto it = std::find_if(impl_->alerts.begin(), impl_->alerts.end(),
        [alert_id](const Alert& a) { return a.id == alert_id; });
    
    if (it != impl_->alerts.end() && !it->resolved) {
        it->resolved = true;
        it->resolved_at = std::chrono::steady_clock::now();
        return true;
    }
    
    return false;
}

void AlertManager::acknowledge_all(AlertLevel level) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    auto now = std::chrono::steady_clock::now();
    for (auto& alert : impl_->alerts) {
        if (alert.level == level && !alert.acknowledged) {
            alert.acknowledged = true;
            alert.acknowledged_at = now;
        }
    }
}

void AlertManager::resolve_all_of_type(AlertType type) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    auto now = std::chrono::steady_clock::now();
    for (auto& alert : impl_->alerts) {
        if (alert.type == type && !alert.resolved) {
            alert.resolved = true;
            alert.resolved_at = now;
        }
    }
}

std::optional<Alert> AlertManager::get_alert(uint64_t alert_id) const {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    auto it = std::find_if(impl_->alerts.begin(), impl_->alerts.end(),
        [alert_id](const Alert& a) { return a.id == alert_id; });
    
    if (it != impl_->alerts.end()) {
        return *it;
    }
    
    return std::nullopt;
}

std::vector<Alert> AlertManager::get_active_alerts() const {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    std::vector<Alert> active;
    std::copy_if(impl_->alerts.begin(), impl_->alerts.end(),
        std::back_inserter(active),
        [](const Alert& a) { return !a.resolved; });
    
    return active;
}

std::vector<Alert> AlertManager::get_alerts_by_level(AlertLevel level) const {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    std::vector<Alert> result;
    std::copy_if(impl_->alerts.begin(), impl_->alerts.end(),
        std::back_inserter(result),
        [level](const Alert& a) { return a.level == level; });
    
    return result;
}

std::vector<Alert> AlertManager::get_alerts_by_type(AlertType type) const {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    std::vector<Alert> result;
    std::copy_if(impl_->alerts.begin(), impl_->alerts.end(),
        std::back_inserter(result),
        [type](const Alert& a) { return a.type == type; });
    
    return result;
}

std::vector<Alert> AlertManager::get_recent_alerts(std::size_t count) const {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    std::vector<Alert> result;
    
    auto start = impl_->alerts.size() > count 
        ? impl_->alerts.end() - static_cast<std::ptrdiff_t>(count)
        : impl_->alerts.begin();
    
    std::copy(start, impl_->alerts.end(), std::back_inserter(result));
    
    // Сортируем по времени (новые первыми)
    std::sort(result.begin(), result.end(),
        [](const Alert& a, const Alert& b) {
            return a.created_at > b.created_at;
        });
    
    return result;
}

AlertManager::AlertCounts AlertManager::get_counts() const {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    AlertCounts counts;
    
    for (const auto& alert : impl_->alerts) {
        if (alert.resolved) continue;
        
        counts.total++;
        
        switch (alert.level) {
            case AlertLevel::Info:      counts.info++; break;
            case AlertLevel::Warning:   counts.warning++; break;
            case AlertLevel::Critical:  counts.critical++; break;
            case AlertLevel::Emergency: counts.emergency++; break;
        }
    }
    
    return counts;
}

void AlertManager::set_alert_callback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    impl_->alert_callback = std::move(callback);
}

void AlertManager::set_action_callback(ActionCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    impl_->action_callback = std::move(callback);
}

void AlertManager::clear_all() {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    impl_->alerts.clear();
}

void AlertManager::cleanup_old(std::chrono::seconds max_age) {
    std::lock_guard<std::mutex> lock(impl_->alerts_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    impl_->alerts.erase(
        std::remove_if(impl_->alerts.begin(), impl_->alerts.end(),
            [now, max_age](const Alert& a) {
                if (!a.resolved) return false;
                
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - a.created_at
                );
                return age > max_age;
            }),
        impl_->alerts.end()
    );
}

} // namespace quaxis::monitoring
