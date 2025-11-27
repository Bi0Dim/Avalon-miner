/**
 * @file status_reporter.hpp
 * @brief Компактный терминальный вывод статуса майнера
 * 
 * Предоставляет:
 * - Периодический вывод статуса с ANSI цветами
 * - Кольцевой буфер событий
 * - Минимальный шум в терминале (перерисовка на месте)
 */

#pragma once

#include "../core/types.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace quaxis::log {

// =============================================================================
// Конфигурация
// =============================================================================

/**
 * @brief Конфигурация Status Reporter
 */
struct StatusReporterConfig {
    /// @brief Уровень логирования
    std::string level{"info"};  // error|warn|info|debug
    
    /// @brief Интервал обновления статуса (мс)
    uint32_t refresh_interval_ms{1000};
    
    /// @brief Размер истории событий
    uint32_t event_history{200};
    
    /// @brief Включить ANSI цвета
    bool color{true};
    
    /// @brief Подсвечивать найденные блоки
    bool highlight_found_blocks{true};
    
    /// @brief Показывать счётчики блоков по chains
    bool show_chain_block_counts{true};
    
    /// @brief Показывать хешрейт
    bool show_hashrate{true};
};

// =============================================================================
// Типы событий
// =============================================================================

/**
 * @brief Тип события
 */
enum class EventType {
    NewBlock,           ///< Новый блок Bitcoin
    AuxBlockFound,      ///< Найден блок auxiliary chain
    BtcBlockFound,      ///< Найден блок Bitcoin
    FallbackEnter,      ///< Переход в fallback режим
    FallbackExit,       ///< Выход из fallback режима
    SubmitOk,           ///< Успешная отправка
    SubmitFail,         ///< Неудачная отправка
    Error               ///< Ошибка
};

/**
 * @brief Преобразовать тип события в строку
 */
[[nodiscard]] constexpr std::string_view event_type_to_string(EventType type) noexcept {
    switch (type) {
        case EventType::NewBlock:       return "NEW_BLOCK";
        case EventType::AuxBlockFound:  return "AUX_BLOCK_FOUND";
        case EventType::BtcBlockFound:  return "BTC_BLOCK_FOUND";
        case EventType::FallbackEnter:  return "FALLBACK_ENTER";
        case EventType::FallbackExit:   return "FALLBACK_EXIT";
        case EventType::SubmitOk:       return "SUBMIT_OK";
        case EventType::SubmitFail:     return "SUBMIT_FAIL";
        case EventType::Error:          return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Событие
 */
struct Event {
    /// @brief Тип события
    EventType type;
    
    /// @brief Время события
    std::chrono::steady_clock::time_point timestamp;
    
    /// @brief Сообщение
    std::string message;
    
    /// @brief Дополнительные данные (например, chain name)
    std::string data;
};

// =============================================================================
// Данные статуса
// =============================================================================

/**
 * @brief Данные для отображения статуса
 */
struct StatusData {
    /// @brief Время работы
    std::chrono::seconds uptime{0};
    
    /// @brief Режим fallback активен
    bool fallback_active{false};
    
    /// @brief Хешрейт (TH/s)
    double hashrate_ths{0.0};
    
    /// @brief Количество подключённых ASIC
    uint32_t asic_connections{0};
    
    /// @brief Высота блока Bitcoin
    uint32_t btc_height{0};
    
    /// @brief Возраст текущего tip (мс)
    uint64_t tip_age_ms{0};
    
    /// @brief Глубина очереди заданий
    uint32_t job_queue_depth{0};
    
    /// @brief Количество подготовленных шаблонов
    uint32_t prepared_templates{0};
    
    /// @brief Список активных chains
    std::vector<std::string> active_chains;
    
    /// @brief Счётчики найденных блоков по chains
    std::unordered_map<std::string, uint32_t> found_blocks;
    
    /// @brief Адаптивный spin активен
    bool adaptive_spin_active{false};
    
    /// @brief Оценка CPU usage SHM (%)
    double shm_cpu_usage_percent{0.0};
};

/**
 * @brief Провайдер данных статуса
 */
using StatusDataProvider = std::function<StatusData()>;

// =============================================================================
// ANSI цвета
// =============================================================================

namespace ansi {
    constexpr std::string_view RESET   = "\033[0m";
    constexpr std::string_view BOLD    = "\033[1m";
    constexpr std::string_view RED     = "\033[31m";
    constexpr std::string_view GREEN   = "\033[32m";
    constexpr std::string_view YELLOW  = "\033[33m";
    constexpr std::string_view BLUE    = "\033[34m";
    constexpr std::string_view MAGENTA = "\033[35m";
    constexpr std::string_view CYAN    = "\033[36m";
    constexpr std::string_view WHITE   = "\033[37m";
    constexpr std::string_view CLEAR_LINE = "\033[2K";
    constexpr std::string_view MOVE_UP = "\033[A";
} // namespace ansi

// =============================================================================
// Status Reporter
// =============================================================================

/**
 * @brief Репортер статуса в терминал
 * 
 * Периодически выводит компактный блок статуса с ANSI цветами.
 * Поддерживает кольцевой буфер событий.
 */
class StatusReporter {
public:
    /**
     * @brief Создать репортер с конфигурацией
     */
    explicit StatusReporter(const StatusReporterConfig& config);
    
    ~StatusReporter();
    
    // Запрещаем копирование
    StatusReporter(const StatusReporter&) = delete;
    StatusReporter& operator=(const StatusReporter&) = delete;
    
    // =========================================================================
    // Управление
    // =========================================================================
    
    /**
     * @brief Запустить периодический вывод
     */
    void start();
    
    /**
     * @brief Остановить вывод
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли вывод
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    // =========================================================================
    // Провайдер данных
    // =========================================================================
    
    /**
     * @brief Установить провайдер данных статуса
     */
    void set_data_provider(StatusDataProvider provider);
    
    // =========================================================================
    // События
    // =========================================================================
    
    /**
     * @brief Добавить событие
     */
    void add_event(EventType type, std::string_view message, std::string_view data = "");
    
    /**
     * @brief Получить последние события
     * 
     * @param count Количество событий (0 = все)
     */
    [[nodiscard]] std::vector<Event> get_events(size_t count = 0) const;
    
    /**
     * @brief Очистить историю событий
     */
    void clear_events();
    
    // =========================================================================
    // Рендеринг
    // =========================================================================
    
    /**
     * @brief Сформировать строку статуса (без ANSI для тестов)
     */
    [[nodiscard]] std::string render_status_plain(const StatusData& data) const;
    
    /**
     * @brief Сформировать строку статуса с ANSI цветами
     */
    [[nodiscard]] std::string render_status_ansi(const StatusData& data) const;
    
    /**
     * @brief Сформировать строку события
     */
    [[nodiscard]] std::string render_event(const Event& event, bool with_color) const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::log
