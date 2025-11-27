/**
 * @file status_reporter.hpp
 * @brief Терминальный репортёр статуса майнера
 * 
 * Периодически выводит в терминал состояние системы с ANSI форматированием:
 * - Uptime, Bitcoin высота и возраст tip
 * - Хешрейт (настроенный/оценочный)
 * - Количество подключённых ASIC
 * - Список активных merged mining chains
 * - Счётчики найденных блоков по каждой chain
 * - Состояние fallback
 * - Адаптивное состояние spin и примерная загрузка CPU SHM
 * - Кольцевой буфер событий
 */

#pragma once

#include "../core/types.hpp"
#include "../fallback/fallback_manager.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace quaxis::log {

// =============================================================================
// Типы событий
// =============================================================================

/**
 * @brief Тип события для логирования
 */
enum class EventType {
    NEW_BLOCK,          ///< Новый блок Bitcoin получен
    AUX_BLOCK_FOUND,    ///< Найден блок merged mining chain
    BTC_BLOCK_FOUND,    ///< Найден блок Bitcoin
    FALLBACK_ENTER,     ///< Переход в fallback режим
    FALLBACK_EXIT,      ///< Выход из fallback режима
    SUBMIT_OK,          ///< Успешная отправка share/block
    SUBMIT_FAIL,        ///< Неудачная отправка share/block
    ERROR               ///< Ошибка
};

/**
 * @brief Преобразование типа события в строку
 */
[[nodiscard]] constexpr std::string_view to_string(EventType type) noexcept {
    switch (type) {
        case EventType::NEW_BLOCK:       return "NEW_BLOCK";
        case EventType::AUX_BLOCK_FOUND: return "AUX_FOUND";
        case EventType::BTC_BLOCK_FOUND: return "BTC_FOUND";
        case EventType::FALLBACK_ENTER:  return "FB_ENTER";
        case EventType::FALLBACK_EXIT:   return "FB_EXIT";
        case EventType::SUBMIT_OK:       return "SUBMIT_OK";
        case EventType::SUBMIT_FAIL:     return "SUBMIT_FAIL";
        case EventType::ERROR:           return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Запись события
 */
struct EventRecord {
    EventType type;
    std::chrono::system_clock::time_point timestamp;
    std::string message;
    std::string chain_name;  ///< Для событий связанных с chain
};

// =============================================================================
// Конфигурация
// =============================================================================

/**
 * @brief Конфигурация логирования
 */
struct LoggingConfig {
    /// @brief Интервал обновления экрана (мс)
    uint32_t refresh_interval_ms = 1000;
    
    /// @brief Уровень логирования: "error", "warn", "info", "debug"
    std::string level = "info";
    
    /// @brief Размер истории событий
    size_t event_history = 200;
    
    /// @brief Использовать цветной вывод
    bool color = true;
    
    /// @brief Показывать хешрейт
    bool show_hashrate = true;
    
    /// @brief Подсвечивать найденные блоки
    bool highlight_found_blocks = true;
    
    /// @brief Показывать счётчики блоков по chains
    bool show_chain_block_counts = true;
    
    /// @brief Номинальная мощность (TH/s) для отображения
    double rated_ths = 90.0;
};

// =============================================================================
// Провайдеры данных
// =============================================================================

/**
 * @brief Провайдер статистики Bitcoin
 */
struct BitcoinStats {
    uint32_t height = 0;
    uint32_t tip_age_seconds = 0;  ///< Возраст последнего блока
    bool connected = false;
};

/**
 * @brief Провайдер статистики ASIC
 */
struct AsicStats {
    uint32_t connected_count = 0;
    double estimated_hashrate_ths = 0.0;
    double temperature_avg = 0.0;
};

/**
 * @brief Провайдер статистики SHM
 */
struct ShmStats {
    bool spin_wait_active = false;
    double cpu_usage_percent = 0.0;
    bool adaptive_mode = false;
};

/**
 * @brief Информация о merged chain
 */
struct ChainBlockCount {
    std::string name;
    uint64_t found_blocks = 0;
    bool enabled = false;
};

// =============================================================================
// Status Reporter
// =============================================================================

/**
 * @brief Терминальный репортёр статуса
 * 
 * Периодически выводит состояние системы в терминал.
 */
class StatusReporter {
public:
    /**
     * @brief Создать репортёр с конфигурацией
     */
    explicit StatusReporter(const LoggingConfig& config);
    
    ~StatusReporter();
    
    // Запрещаем копирование
    StatusReporter(const StatusReporter&) = delete;
    StatusReporter& operator=(const StatusReporter&) = delete;
    
    // ==========================================================================
    // Управление
    // ==========================================================================
    
    /**
     * @brief Запустить периодический вывод статуса
     */
    void start();
    
    /**
     * @brief Остановить вывод статуса
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли репортёр
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    // ==========================================================================
    // Обновление данных
    // ==========================================================================
    
    /**
     * @brief Обновить статистику Bitcoin
     */
    void update_bitcoin_stats(const BitcoinStats& stats);
    
    /**
     * @brief Обновить статистику ASIC
     */
    void update_asic_stats(const AsicStats& stats);
    
    /**
     * @brief Обновить статистику SHM
     */
    void update_shm_stats(const ShmStats& stats);
    
    /**
     * @brief Обновить режим fallback
     */
    void update_fallback_mode(fallback::FallbackMode mode);
    
    /**
     * @brief Обновить список активных chains
     */
    void update_active_chains(const std::vector<std::string>& chains);
    
    /**
     * @brief Обновить счётчик найденных блоков
     */
    void update_block_count(const std::string& chain_name, uint64_t count);
    
    // ==========================================================================
    // События
    // ==========================================================================
    
    /**
     * @brief Записать событие
     */
    void log_event(EventType type, const std::string& message, 
                   const std::string& chain_name = "");
    
    /**
     * @brief Записать событие NEW_BLOCK
     */
    void log_new_block(uint32_t height);
    
    /**
     * @brief Записать событие AUX_BLOCK_FOUND
     */
    void log_aux_block_found(const std::string& chain_name, uint32_t height);
    
    /**
     * @brief Записать событие BTC_BLOCK_FOUND
     */
    void log_btc_block_found(uint32_t height);
    
    /**
     * @brief Записать событие FALLBACK
     */
    void log_fallback_change(fallback::FallbackMode old_mode, 
                             fallback::FallbackMode new_mode);
    
    /**
     * @brief Записать событие SUBMIT
     */
    void log_submit(bool success, const std::string& chain_name = "bitcoin");
    
    /**
     * @brief Записать ошибку
     */
    void log_error(const std::string& message);
    
    // ==========================================================================
    // Рендеринг
    // ==========================================================================
    
    /**
     * @brief Получить текущий вывод статуса (без ANSI кодов)
     * 
     * Используется для тестирования.
     */
    [[nodiscard]] std::string render_plain() const;
    
    /**
     * @brief Получить текущий вывод статуса (с ANSI кодами)
     */
    [[nodiscard]] std::string render() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::log
