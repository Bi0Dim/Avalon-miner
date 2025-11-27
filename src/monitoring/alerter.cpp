/**
 * @file alerter.cpp
 * @brief Реализация системы алертинга
 */

#include "alerter.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace quaxis::monitoring {

// =============================================================================
// Singleton
// =============================================================================

Alerter& Alerter::instance() {
    static Alerter instance;
    return instance;
}

Alerter::Alerter() = default;

// =============================================================================
// Конфигурация
// =============================================================================

void Alerter::configure(const AlerterConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void Alerter::set_callback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = std::move(callback);
}

// =============================================================================
// Общие алерты
// =============================================================================

void Alerter::alert(AlertLevel level, std::string_view message) {
    // Проверяем уровень
    if (level < config_.log_level) {
        return;
    }
    
    // Увеличиваем счётчики
    alerts_count_++;
    if (level == AlertLevel::Critical) {
        critical_count_++;
    }
    
    // Выводим в консоль
    if (config_.console_output) {
        log_to_console(level, message);
    }
    
    // Вызываем callback
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (callback_) {
            callback_(level, message);
        }
    }
    
    // Отправляем webhook
    if (!config_.webhook_url.empty()) {
        send_webhook(level, message);
    }
}

// =============================================================================
// Предопределённые алерты
// =============================================================================

void Alerter::alert_bitcoin_disconnected() {
    if (!should_send("bitcoin_disconnected")) return;
    alert(AlertLevel::Critical, "Bitcoin Core отключён");
}

void Alerter::alert_bitcoin_connected() {
    if (!should_send("bitcoin_connected")) return;
    alert(AlertLevel::Info, "Bitcoin Core подключён");
}

void Alerter::alert_asic_disconnected(const std::string& asic_id) {
    std::string key = "asic_disconnected_" + asic_id;
    if (!should_send(key)) return;
    
    std::ostringstream ss;
    ss << "ASIC отключился: " << asic_id;
    alert(AlertLevel::Warning, ss.str());
}

void Alerter::alert_asic_connected(const std::string& asic_id) {
    std::string key = "asic_connected_" + asic_id;
    if (!should_send(key)) return;
    
    std::ostringstream ss;
    ss << "ASIC подключился: " << asic_id;
    alert(AlertLevel::Info, ss.str());
}

void Alerter::alert_queue_empty() {
    if (!should_send("queue_empty")) return;
    alert(AlertLevel::Warning, "Очередь заданий пуста");
}

void Alerter::alert_high_latency(double latency_ms) {
    if (!should_send("high_latency")) return;
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Высокая латентность: " << latency_ms << " мс";
    alert(AlertLevel::Warning, ss.str());
}

void Alerter::alert_fallback_activated(std::string_view mode) {
    if (!should_send("fallback_activated")) return;
    
    std::ostringstream ss;
    ss << "Активирован fallback режим: " << mode;
    alert(AlertLevel::Warning, ss.str());
}

void Alerter::alert_primary_restored() {
    if (!should_send("primary_restored")) return;
    alert(AlertLevel::Info, "Восстановлен primary режим (SHM)");
}

void Alerter::alert_block_found(const std::string& chain, uint64_t reward) {
    // Блок найден - всегда отправляем (без дедупликации)
    std::ostringstream ss;
    ss << "БЛОК НАЙДЕН! Цепь: " << chain 
       << ", награда: " << (static_cast<double>(reward) / 100000000.0) << " BTC";
    alert(AlertLevel::Info, ss.str());
}

void Alerter::alert_high_temperature(double temperature) {
    if (!should_send("high_temperature")) return;
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Высокая температура: " << temperature << "°C";
    alert(AlertLevel::Warning, ss.str());
}

void Alerter::alert_hashrate_drop(double current, double expected) {
    if (!should_send("hashrate_drop")) return;
    
    double drop_percent = ((expected - current) / expected) * 100.0;
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Падение хешрейта: " << current << " TH/s (ожидалось " 
       << expected << " TH/s, падение " << drop_percent << "%)";
    alert(AlertLevel::Warning, ss.str());
}

void Alerter::alert_stratum_error(const std::string& pool, const std::string& error) {
    std::string key = "stratum_error_" + pool;
    if (!should_send(key)) return;
    
    std::ostringstream ss;
    ss << "Ошибка Stratum пула " << pool << ": " << error;
    alert(AlertLevel::Critical, ss.str());
}

// =============================================================================
// Статистика
// =============================================================================

uint64_t Alerter::get_alerts_count() const noexcept {
    return alerts_count_;
}

uint64_t Alerter::get_critical_count() const noexcept {
    return critical_count_;
}

void Alerter::reset_stats() {
    alerts_count_ = 0;
    critical_count_ = 0;
    
    std::lock_guard<std::mutex> lock(mutex_);
    last_alerts_.clear();
}

// =============================================================================
// Приватные методы
// =============================================================================

bool Alerter::should_send(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto it = last_alerts_.find(key);
    
    if (it != last_alerts_.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second
        );
        
        if (elapsed.count() < static_cast<long>(config_.dedup_interval_seconds)) {
            return false;
        }
    }
    
    last_alerts_[key] = now;
    return true;
}

void Alerter::log_to_console(AlertLevel level, std::string_view message) {
    // Получаем текущее время
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto tm = std::localtime(&time);
    
    // Формируем вывод
    std::ostringstream ss;
    ss << "[" << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "] ";
    ss << "[" << alert_level_to_string(level) << "] ";
    ss << message;
    
    // Выводим с цветом в зависимости от уровня
    switch (level) {
        case AlertLevel::Info:
            std::cout << "\033[32m" << ss.str() << "\033[0m" << std::endl;
            break;
        case AlertLevel::Warning:
            std::cout << "\033[33m" << ss.str() << "\033[0m" << std::endl;
            break;
        case AlertLevel::Critical:
            std::cerr << "\033[31m" << ss.str() << "\033[0m" << std::endl;
            break;
    }
}

void Alerter::send_webhook(AlertLevel level, std::string_view message) {
    // Простая реализация без внешних зависимостей
    // В реальном коде здесь будет HTTP POST запрос
    
    // Формируем JSON
    std::ostringstream json;
    json << "{";
    json << "\"level\":\"" << alert_level_to_string(level) << "\",";
    json << "\"message\":\"" << message << "\",";
    json << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    json << "}";
    
    // TODO: Отправить HTTP POST на config_.webhook_url
    // Для минимальной реализации без зависимостей пропускаем
    (void)json;
}

} // namespace quaxis::monitoring
