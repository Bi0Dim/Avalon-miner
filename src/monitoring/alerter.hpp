/**
 * @file alerter.hpp
 * @brief Система алертинга для быстрого обнаружения проблем
 * 
 * Предоставляет:
 * - Предопределённые алерты для типичных ситуаций
 * - Callback для внешних систем (webhook, telegram)
 * - Интеграция с логированием
 */

#pragma once

#include "../core/types.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <string_view>

namespace quaxis::monitoring {

// =============================================================================
// Уровни алертов
// =============================================================================

/**
 * @brief Уровень алерта
 */
enum class AlertLevel {
    Info,      ///< Информационное сообщение
    Warning,   ///< Предупреждение
    Critical   ///< Критическая ситуация
};

/**
 * @brief Преобразовать уровень в строку
 */
[[nodiscard]] constexpr std::string_view alert_level_to_string(AlertLevel level) noexcept {
    switch (level) {
        case AlertLevel::Info:     return "INFO";
        case AlertLevel::Warning:  return "WARNING";
        case AlertLevel::Critical: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// Callback для внешних систем
// =============================================================================

/**
 * @brief Callback для обработки алертов
 * 
 * @param level Уровень алерта
 * @param message Сообщение
 */
using AlertCallback = std::function<void(AlertLevel level, std::string_view message)>;

// =============================================================================
// Alerter
// =============================================================================

/**
 * @brief Конфигурация Alerter
 */
struct AlerterConfig {
    /// @brief Минимальный уровень для логирования
    AlertLevel log_level{AlertLevel::Warning};
    
    /// @brief URL для webhook (опционально)
    std::string webhook_url;
    
    /// @brief Включить вывод в консоль
    bool console_output{true};
    
    /// @brief Минимальный интервал между одинаковыми алертами (секунды)
    uint32_t dedup_interval_seconds{60};
};

/**
 * @brief Система алертинга
 * 
 * Централизованная обработка алертов с дедупликацией и внешними уведомлениями.
 */
class Alerter {
public:
    /**
     * @brief Получить единственный экземпляр
     */
    static Alerter& instance();
    
    // Запрещаем копирование
    Alerter(const Alerter&) = delete;
    Alerter& operator=(const Alerter&) = delete;
    
    // =========================================================================
    // Конфигурация
    // =========================================================================
    
    /**
     * @brief Установить конфигурацию
     */
    void configure(const AlerterConfig& config);
    
    /**
     * @brief Установить callback для внешних систем
     */
    void set_callback(AlertCallback callback);
    
    // =========================================================================
    // Общие алерты
    // =========================================================================
    
    /**
     * @brief Отправить алерт
     * 
     * @param level Уровень
     * @param message Сообщение
     */
    void alert(AlertLevel level, std::string_view message);
    
    // =========================================================================
    // Предопределённые алерты
    // =========================================================================
    
    /**
     * @brief Bitcoin Core отключился
     */
    void alert_bitcoin_disconnected();
    
    /**
     * @brief Bitcoin Core подключён
     */
    void alert_bitcoin_connected();
    
    /**
     * @brief ASIC отключился
     * 
     * @param asic_id Идентификатор ASIC
     */
    void alert_asic_disconnected(const std::string& asic_id);
    
    /**
     * @brief ASIC подключился
     * 
     * @param asic_id Идентификатор ASIC
     */
    void alert_asic_connected(const std::string& asic_id);
    
    /**
     * @brief Очередь заданий пуста
     */
    void alert_queue_empty();
    
    /**
     * @brief Высокая латентность
     * 
     * @param latency_ms Латентность в миллисекундах
     */
    void alert_high_latency(double latency_ms);
    
    /**
     * @brief Активирован fallback режим
     * 
     * @param mode Название режима
     */
    void alert_fallback_activated(std::string_view mode);
    
    /**
     * @brief Восстановлен primary режим
     */
    void alert_primary_restored();
    
    /**
     * @brief Найден блок
     * 
     * @param chain Название цепи (bitcoin или merged mining chain)
     * @param reward Награда в satoshi
     */
    void alert_block_found(const std::string& chain, uint64_t reward);
    
    /**
     * @brief Высокая температура
     * 
     * @param temperature Температура в градусах Цельсия
     */
    void alert_high_temperature(double temperature);
    
    /**
     * @brief Падение хешрейта
     * 
     * @param current Текущий хешрейт
     * @param expected Ожидаемый хешрейт
     */
    void alert_hashrate_drop(double current, double expected);
    
    /**
     * @brief Ошибка Stratum подключения
     * 
     * @param pool Название пула
     * @param error Описание ошибки
     */
    void alert_stratum_error(const std::string& pool, const std::string& error);
    
    // =========================================================================
    // Статистика
    // =========================================================================
    
    /**
     * @brief Получить количество отправленных алертов
     */
    [[nodiscard]] uint64_t get_alerts_count() const noexcept;
    
    /**
     * @brief Получить количество critical алертов
     */
    [[nodiscard]] uint64_t get_critical_count() const noexcept;
    
    /**
     * @brief Сбросить статистику
     */
    void reset_stats();
    
private:
    Alerter();
    ~Alerter() = default;
    
    /**
     * @brief Проверка дедупликации
     * 
     * @param key Ключ алерта
     * @return true если алерт можно отправить
     */
    bool should_send(const std::string& key);
    
    /**
     * @brief Вывод в консоль
     */
    void log_to_console(AlertLevel level, std::string_view message);
    
    /**
     * @brief Отправка webhook (если настроен)
     */
    void send_webhook(AlertLevel level, std::string_view message);
    
    // Конфигурация
    AlerterConfig config_;
    
    // Callback
    AlertCallback callback_;
    
    // Дедупликация
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_alerts_;
    std::mutex mutex_;
    
    // Статистика
    std::atomic<uint64_t> alerts_count_{0};
    std::atomic<uint64_t> critical_count_{0};
};

} // namespace quaxis::monitoring
