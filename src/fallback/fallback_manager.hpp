/**
 * @file fallback_manager.hpp
 * @brief Управление автоматическим переключением между источниками
 * 
 * Обеспечивает надёжность получения заданий майнинга:
 * - Приоритет: SHM → ZMQ → Stratum Pool
 * - Автоматическое переключение при недоступности
 * - Автоматический возврат к основному источнику
 */

#pragma once

#include "../core/types.hpp"
#include "pool_config.hpp"
#include "stratum_client.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace quaxis::fallback {

// =============================================================================
// Режимы работы
// =============================================================================

/**
 * @brief Текущий режим работы (источник заданий)
 */
enum class FallbackMode {
    PrimarySHM,      ///< Основной режим - Shared Memory
    FallbackZMQ,     ///< Первый резерв - ZMQ
    FallbackStratum  ///< Второй резерв - Stratum пул
};

/**
 * @brief Преобразовать режим в строку
 */
[[nodiscard]] constexpr std::string_view to_string(FallbackMode mode) noexcept {
    switch (mode) {
        case FallbackMode::PrimarySHM:      return "primary_shm";
        case FallbackMode::FallbackZMQ:     return "fallback_zmq";
        case FallbackMode::FallbackStratum: return "fallback_stratum";
        default: return "unknown";
    }
}

/**
 * @brief Преобразовать режим в числовое значение для Prometheus
 */
[[nodiscard]] constexpr int to_prometheus_value(FallbackMode mode) noexcept {
    switch (mode) {
        case FallbackMode::PrimarySHM:      return 0;
        case FallbackMode::FallbackZMQ:     return 1;
        case FallbackMode::FallbackStratum: return 2;
        default: return -1;
    }
}

// =============================================================================
// Состояние здоровья
// =============================================================================

/**
 * @brief Состояние здоровья источника
 */
struct SourceHealth {
    /// @brief Источник доступен
    bool available{false};
    
    /// @brief Последняя проверка
    std::chrono::steady_clock::time_point last_check;
    
    /// @brief Последняя успешная проверка
    std::chrono::steady_clock::time_point last_success;
    
    /// @brief Количество последовательных неудач
    uint32_t consecutive_failures{0};
    
    /// @brief Время последнего полученного задания
    std::chrono::steady_clock::time_point last_job_received;
    
    /// @brief Общее количество проверок
    uint64_t total_checks{0};
    
    /// @brief Успешных проверок
    uint64_t successful_checks{0};
};

/**
 * @brief Статистика fallback
 */
struct FallbackStats {
    /// @brief Количество переключений на ZMQ
    uint64_t zmq_switches{0};
    
    /// @brief Количество переключений на Stratum
    uint64_t stratum_switches{0};
    
    /// @brief Количество возвратов к primary
    uint64_t primary_restorations{0};
    
    /// @brief Общее время в fallback режиме (секунды)
    uint64_t fallback_duration_seconds{0};
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback при смене режима
 */
using ModeChangeCallback = std::function<void(FallbackMode old_mode, FallbackMode new_mode)>;

/**
 * @brief Callback для проверки здоровья SHM
 */
using ShmHealthCheck = std::function<bool()>;

/**
 * @brief Callback для проверки здоровья ZMQ
 */
using ZmqHealthCheck = std::function<bool()>;

// =============================================================================
// Fallback Manager
// =============================================================================

/**
 * @brief Менеджер автоматического переключения источников
 * 
 * Отслеживает состояние всех источников и автоматически
 * переключается на резервный при проблемах с основным.
 */
class FallbackManager {
public:
    /**
     * @brief Создать менеджер с конфигурацией
     */
    explicit FallbackManager(const FallbackConfig& config);
    
    ~FallbackManager();
    
    // Запрещаем копирование
    FallbackManager(const FallbackManager&) = delete;
    FallbackManager& operator=(const FallbackManager&) = delete;
    
    // ==========================================================================
    // Управление
    // ==========================================================================
    
    /**
     * @brief Запустить мониторинг
     */
    void start();
    
    /**
     * @brief Остановить мониторинг
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли мониторинг
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    // ==========================================================================
    // Проверка здоровья
    // ==========================================================================
    
    /**
     * @brief Установить функцию проверки здоровья SHM
     */
    void set_shm_health_check(ShmHealthCheck check);
    
    /**
     * @brief Установить функцию проверки здоровья ZMQ
     */
    void set_zmq_health_check(ZmqHealthCheck check);
    
    /**
     * @brief Выполнить проверку здоровья основного источника
     * 
     * Вызывается автоматически по таймеру, но можно вызвать вручную.
     */
    void check_primary_health();
    
    /**
     * @brief Сигнализировать о получении задания от текущего источника
     * 
     * Сбрасывает счётчик неудач.
     */
    void signal_job_received();
    
    // ==========================================================================
    // Переключение
    // ==========================================================================
    
    /**
     * @brief Переключиться на fallback режим
     * 
     * Автоматически выбирает лучший доступный fallback.
     */
    void switch_to_fallback();
    
    /**
     * @brief Попытаться вернуться к основному источнику
     */
    void try_restore_primary();
    
    /**
     * @brief Принудительно установить режим
     */
    void set_mode(FallbackMode mode);
    
    // ==========================================================================
    // Статус
    // ==========================================================================
    
    /**
     * @brief Получить текущий режим
     */
    [[nodiscard]] FallbackMode current_mode() const noexcept;
    
    /**
     * @brief Получить здоровье SHM
     */
    [[nodiscard]] SourceHealth get_shm_health() const;
    
    /**
     * @brief Получить здоровье ZMQ
     */
    [[nodiscard]] SourceHealth get_zmq_health() const;
    
    /**
     * @brief Получить здоровье Stratum
     */
    [[nodiscard]] SourceHealth get_stratum_health() const;
    
    /**
     * @brief Получить статистику
     */
    [[nodiscard]] FallbackStats get_stats() const;
    
    /**
     * @brief Проверить, подключён ли Stratum клиент
     */
    [[nodiscard]] bool is_stratum_connected() const;
    
    // ==========================================================================
    // Stratum клиент
    // ==========================================================================
    
    /**
     * @brief Получить Stratum клиент (если активен)
     */
    [[nodiscard]] StratumClient* get_stratum_client();
    
    /**
     * @brief Получить текущее задание от Stratum (если есть)
     */
    [[nodiscard]] std::optional<StratumJob> get_stratum_job() const;
    
    // ==========================================================================
    // Callbacks
    // ==========================================================================
    
    /**
     * @brief Установить callback при смене режима
     */
    void set_mode_change_callback(ModeChangeCallback callback);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::fallback
