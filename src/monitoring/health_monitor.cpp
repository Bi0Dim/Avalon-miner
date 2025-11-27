/**
 * @file health_monitor.cpp
 * @brief Реализация мониторинга здоровья ASIC
 * 
 * Predictive Maintenance: мониторинг, анализ трендов, превентивные действия.
 */

#include "health_monitor.hpp"

#include <thread>
#include <numeric>
#include <algorithm>
#include <cmath>

namespace quaxis::monitoring {

// =============================================================================
// Внутренняя реализация
// =============================================================================

struct HealthMonitor::Impl {
    HealthConfig config;
    
    // Текущие метрики
    mutable std::mutex metrics_mutex;
    TemperatureMetrics temperature;
    HashrateMetrics hashrate;
    ErrorMetrics errors;
    PowerMetrics power;
    UptimeMetrics uptime;
    std::vector<ChipHealth> chips;
    HealthMetrics::Status current_status = HealthMetrics::Status::Healthy;
    std::string status_message;
    
    // История для анализа трендов
    std::deque<double> temperature_history;
    std::deque<double> hashrate_history;
    std::deque<std::chrono::steady_clock::time_point> history_times;
    
    // Фоновый поток
    std::atomic<bool> running{false};
    std::thread monitor_thread;
    
    explicit Impl(const HealthConfig& cfg)
        : config(cfg)
    {
        uptime.start_time = std::chrono::steady_clock::now();
        uptime.last_restart = uptime.start_time;
    }
    
    void update_status() {
        // Проверяем температуру
        if (temperature.current >= config.temp_emergency) {
            current_status = HealthMetrics::Status::Emergency;
            status_message = "EMERGENCY: Критический перегрев!";
            return;
        }
        if (temperature.current >= config.temp_critical) {
            current_status = HealthMetrics::Status::Critical;
            status_message = "CRITICAL: Высокая температура";
            return;
        }
        
        // Проверяем error rate
        if (errors.error_rate >= config.error_rate_critical) {
            current_status = HealthMetrics::Status::Critical;
            status_message = "CRITICAL: Высокий процент ошибок";
            return;
        }
        
        // Проверяем падение хешрейта
        if (hashrate.nominal > 0) {
            double drop = (1.0 - hashrate.efficiency) * 100.0;
            if (drop >= config.hashrate_critical_drop) {
                current_status = HealthMetrics::Status::Critical;
                status_message = "CRITICAL: Значительное падение хешрейта";
                return;
            }
            if (drop >= config.hashrate_warning_drop) {
                current_status = HealthMetrics::Status::Warning;
                status_message = "WARNING: Падение хешрейта";
                return;
            }
        }
        
        // Проверяем warning уровни
        if (temperature.current >= config.temp_warning) {
            current_status = HealthMetrics::Status::Warning;
            status_message = "WARNING: Повышенная температура";
            return;
        }
        if (errors.error_rate >= config.error_rate_warning) {
            current_status = HealthMetrics::Status::Warning;
            status_message = "WARNING: Повышенный error rate";
            return;
        }
        
        // Проверяем тренд температуры
        if (temperature.trend > 0.5) {  // Рост более 0.5°C/мин
            current_status = HealthMetrics::Status::Warning;
            status_message = "WARNING: Быстрый рост температуры";
            return;
        }
        
        current_status = HealthMetrics::Status::Healthy;
        status_message = "OK";
    }
    
    double calculate_temperature_trend() const {
        if (temperature_history.size() < 2) {
            return 0.0;
        }
        return calculate_trend(temperature_history);
    }
    
    double calculate_hashrate_trend() const {
        if (hashrate_history.size() < 2) {
            return 0.0;
        }
        return calculate_trend(hashrate_history);
    }
    
    void add_to_history(double temp, double hr) {
        temperature_history.push_back(temp);
        hashrate_history.push_back(hr);
        
        // Ограничиваем размер истории
        while (temperature_history.size() > config.trend_window_size) {
            temperature_history.pop_front();
            hashrate_history.pop_front();
        }
    }
    
    void update_uptime() {
        auto now = std::chrono::steady_clock::now();
        uptime.uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - uptime.start_time
        );
        
        // Простой расчёт доступности
        if (uptime.uptime.count() > 0) {
            // Доступность = время работы / (время работы + время простоя)
            // Упрощённо считаем что каждый рестарт = 30 секунд простоя
            auto downtime = static_cast<double>(uptime.restarts) * 30.0;
            auto total = static_cast<double>(uptime.uptime.count());
            if (total > downtime) {
                uptime.availability = (total - downtime) / total * 100.0;
            } else {
                uptime.availability = 0.0;
            }
        }
    }
};

// =============================================================================
// HealthMonitor публичный интерфейс
// =============================================================================

HealthMonitor::HealthMonitor(const HealthConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

HealthMonitor::~HealthMonitor() {
    stop();
}

void HealthMonitor::update_temperature(uint8_t chip_id, double temperature) {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    if (chip_id == 0) {
        // Общая температура
        impl_->temperature.current = temperature;
        impl_->temperature.last_update = now;
        
        // Обновляем min/max
        if (temperature > impl_->temperature.max || impl_->temperature.max == 0) {
            impl_->temperature.max = temperature;
        }
        if (temperature < impl_->temperature.min || impl_->temperature.min == 0) {
            impl_->temperature.min = temperature;
        }
        
        // Добавляем в историю для анализа тренда
        impl_->add_to_history(temperature, impl_->hashrate.current);
        
        // Пересчитываем тренд
        impl_->temperature.trend = impl_->calculate_temperature_trend();
        
        // Обновляем статус
        impl_->update_status();
    } else {
        // Температура конкретного чипа
        size_t idx = static_cast<size_t>(chip_id - 1);
        if (idx < impl_->chips.size()) {
            impl_->chips[idx].temperature = temperature;
        }
    }
}

void HealthMonitor::update_hashrate(double hashrate) {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    auto now = std::chrono::steady_clock::now();
    
    impl_->hashrate.current = hashrate;
    impl_->hashrate.last_update = now;
    
    if (impl_->hashrate.nominal > 0) {
        impl_->hashrate.efficiency = hashrate / impl_->hashrate.nominal;
    }
    
    impl_->update_status();
}

void HealthMonitor::set_nominal_hashrate(double hashrate) {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    impl_->hashrate.nominal = hashrate;
    
    if (impl_->hashrate.nominal > 0) {
        impl_->hashrate.efficiency = impl_->hashrate.current / impl_->hashrate.nominal;
    }
}

void HealthMonitor::record_error(bool hw_error, bool rejected, bool stale) {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    if (hw_error) impl_->errors.hw_errors++;
    if (rejected) impl_->errors.rejected_shares++;
    if (stale) impl_->errors.stale_shares++;
    
    // Пересчитываем error rate
    if (impl_->errors.total_shares > 0) {
        double errors = static_cast<double>(impl_->errors.hw_errors + 
                                           impl_->errors.rejected_shares +
                                           impl_->errors.stale_shares);
        impl_->errors.error_rate = errors / static_cast<double>(impl_->errors.total_shares) * 100.0;
    }
    
    impl_->errors.last_update = std::chrono::steady_clock::now();
    impl_->update_status();
}

void HealthMonitor::record_share() {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    impl_->errors.total_shares++;
    impl_->errors.last_update = std::chrono::steady_clock::now();
    
    // Пересчитываем error rate
    if (impl_->errors.total_shares > 0) {
        double errors = static_cast<double>(impl_->errors.hw_errors + 
                                           impl_->errors.rejected_shares +
                                           impl_->errors.stale_shares);
        impl_->errors.error_rate = errors / static_cast<double>(impl_->errors.total_shares) * 100.0;
    }
}

void HealthMonitor::update_power(double voltage, double current) {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    impl_->power.voltage = voltage;
    impl_->power.current = current;
    impl_->power.power = voltage * current;
    
    // Энергоэффективность в J/TH
    if (impl_->hashrate.current > 0) {
        double th_per_second = impl_->hashrate.current / 1e12;
        if (th_per_second > 0) {
            impl_->power.efficiency = impl_->power.power / th_per_second;
        }
    }
    
    impl_->power.last_update = std::chrono::steady_clock::now();
}

void HealthMonitor::record_restart() {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    impl_->uptime.restarts++;
    impl_->uptime.last_restart = std::chrono::steady_clock::now();
    impl_->update_uptime();
}

void HealthMonitor::update_chip_status(uint8_t chip_id, bool active, double hashrate, uint32_t errors) {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    // Увеличиваем размер вектора если нужно
    size_t idx = static_cast<size_t>(chip_id);
    if (idx >= impl_->chips.size()) {
        impl_->chips.resize(idx + 1);
    }
    
    impl_->chips[idx].chip_id = chip_id;
    impl_->chips[idx].active = active;
    impl_->chips[idx].hashrate = hashrate;
    impl_->chips[idx].errors = errors;
}

HealthMetrics HealthMonitor::get_metrics() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    HealthMetrics metrics;
    metrics.temperature = impl_->temperature;
    metrics.hashrate = impl_->hashrate;
    metrics.errors = impl_->errors;
    metrics.power = impl_->power;
    metrics.uptime = impl_->uptime;
    metrics.chips = impl_->chips;
    metrics.overall_status = impl_->current_status;
    metrics.status_message = impl_->status_message;
    metrics.collected_at = std::chrono::steady_clock::now();
    
    return metrics;
}

HealthMetrics::Status HealthMonitor::get_status() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    return impl_->current_status;
}

std::string HealthMonitor::get_status_message() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    return impl_->status_message;
}

bool HealthMonitor::requires_action() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    return impl_->current_status != HealthMetrics::Status::Healthy;
}

double HealthMonitor::get_temperature_trend() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    return impl_->temperature.trend;
}

double HealthMonitor::get_hashrate_trend() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    return impl_->calculate_hashrate_trend();
}

std::optional<std::chrono::seconds> HealthMonitor::predict_thermal_critical() const {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    if (impl_->temperature.trend <= 0) {
        return std::nullopt;  // Температура не растёт
    }
    
    double delta = impl_->config.temp_critical - impl_->temperature.current;
    if (delta <= 0) {
        return std::chrono::seconds{0};  // Уже критическая
    }
    
    // Тренд в °C/мин, нужно время в секундах
    double minutes = delta / impl_->temperature.trend;
    return std::chrono::seconds{static_cast<int64_t>(minutes * 60)};
}

void HealthMonitor::start() {
    if (impl_->running.exchange(true)) {
        return;  // Уже запущен
    }
    
    impl_->monitor_thread = std::thread([this]() {
        while (impl_->running.load()) {
            // Обновляем uptime
            {
                std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
                impl_->update_uptime();
            }
            
            // Спим до следующего цикла
            std::this_thread::sleep_for(
                std::chrono::seconds(impl_->config.collection_interval)
            );
        }
    });
}

void HealthMonitor::stop() {
    impl_->running.store(false);
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }
}

bool HealthMonitor::check_and_act() {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    impl_->update_status();
    
    // В текущей реализации просто возвращаем статус
    // Реальные действия (throttle, restart) должны выполняться вызывающим кодом
    return impl_->current_status != HealthMetrics::Status::Healthy;
}

void HealthMonitor::reset() {
    std::lock_guard<std::mutex> lock(impl_->metrics_mutex);
    
    impl_->temperature = TemperatureMetrics{};
    impl_->hashrate = HashrateMetrics{};
    impl_->errors = ErrorMetrics{};
    impl_->power = PowerMetrics{};
    impl_->uptime = UptimeMetrics{};
    impl_->uptime.start_time = std::chrono::steady_clock::now();
    impl_->uptime.last_restart = impl_->uptime.start_time;
    impl_->chips.clear();
    impl_->temperature_history.clear();
    impl_->hashrate_history.clear();
    impl_->current_status = HealthMetrics::Status::Healthy;
    impl_->status_message.clear();
}

// =============================================================================
// Вспомогательные функции
// =============================================================================

double calculate_trend(const std::deque<double>& values) {
    if (values.size() < 2) {
        return 0.0;
    }
    
    // Простая линейная регрессия методом наименьших квадратов
    size_t n = values.size();
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    
    for (size_t i = 0; i < n; ++i) {
        double x = static_cast<double>(i);
        double y = values[i];
        
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    
    double dn = static_cast<double>(n);
    double denominator = dn * sum_xx - sum_x * sum_x;
    
    if (std::abs(denominator) < 1e-10) {
        return 0.0;
    }
    
    // Наклон = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x^2)
    double slope = (dn * sum_xy - sum_x * sum_y) / denominator;
    
    return slope;
}

} // namespace quaxis::monitoring
