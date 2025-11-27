/**
 * @file stratum_client.hpp
 * @brief Stratum v1 клиент для fallback на пул
 * 
 * Реализует минимальный набор Stratum v1 протокола:
 * - mining.subscribe
 * - mining.authorize
 * - mining.notify
 * - mining.submit
 */

#pragma once

#include "../core/types.hpp"
#include "pool_config.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace quaxis::fallback {

// =============================================================================
// Структуры данных Stratum
// =============================================================================

/**
 * @brief Параметры подписки от пула
 */
struct SubscribeResult {
    /// @brief Session ID от пула
    std::string session_id;
    
    /// @brief Extranonce1 (hex)
    std::string extranonce1;
    
    /// @brief Размер extranonce2 в байтах
    uint32_t extranonce2_size{4};
};

/**
 * @brief Уведомление о новом задании (mining.notify)
 */
struct StratumJob {
    /// @brief ID задания
    std::string job_id;
    
    /// @brief Previous block hash (hex, little-endian)
    std::string prevhash;
    
    /// @brief Coinbase часть 1 (hex)
    std::string coinbase1;
    
    /// @brief Coinbase часть 2 (hex)
    std::string coinbase2;
    
    /// @brief Merkle branches (hex)
    std::vector<std::string> merkle_branch;
    
    /// @brief Block version (hex)
    std::string version;
    
    /// @brief nBits (hex)
    std::string nbits;
    
    /// @brief Timestamp (hex)
    std::string ntime;
    
    /// @brief Сбрасывать ли текущую работу
    bool clean_jobs{false};
    
    /// @brief Время получения
    std::chrono::steady_clock::time_point received_at;
};

/**
 * @brief Результат отправки share
 */
struct SubmitResult {
    /// @brief Успешно ли принят share
    bool accepted{false};
    
    /// @brief Сообщение об ошибке (если есть)
    std::string error_message;
    
    /// @brief Код ошибки (если есть)
    int error_code{0};
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback при получении нового задания
 */
using JobCallback = std::function<void(const StratumJob&)>;

/**
 * @brief Callback при изменении сложности
 */
using DifficultyCallback = std::function<void(double)>;

/**
 * @brief Callback при отключении
 */
using DisconnectCallback = std::function<void(const std::string& reason)>;

// =============================================================================
// Stratum Client
// =============================================================================

/**
 * @brief Состояние клиента
 */
enum class StratumState {
    Disconnected,   ///< Не подключён
    Connecting,     ///< Подключение
    Subscribing,    ///< Отправлена подписка
    Authorizing,    ///< Отправлена авторизация
    Connected,      ///< Подключён и готов к работе
    Error           ///< Ошибка
};

/**
 * @brief Преобразовать состояние в строку
 */
[[nodiscard]] constexpr std::string_view to_string(StratumState state) noexcept {
    switch (state) {
        case StratumState::Disconnected: return "disconnected";
        case StratumState::Connecting:   return "connecting";
        case StratumState::Subscribing:  return "subscribing";
        case StratumState::Authorizing:  return "authorizing";
        case StratumState::Connected:    return "connected";
        case StratumState::Error:        return "error";
        default: return "unknown";
    }
}

/**
 * @brief Stratum v1 клиент
 * 
 * Асинхронный клиент для подключения к Stratum пулу.
 * Поддерживает автоматическое переподключение.
 */
class StratumClient {
public:
    /**
     * @brief Создать клиент с конфигурацией пула
     */
    explicit StratumClient(const StratumPoolConfig& config);
    
    ~StratumClient();
    
    // Запрещаем копирование
    StratumClient(const StratumClient&) = delete;
    StratumClient& operator=(const StratumClient&) = delete;
    
    // ==========================================================================
    // Подключение
    // ==========================================================================
    
    /**
     * @brief Подключиться к пулу
     * 
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> connect();
    
    /**
     * @brief Отключиться от пула
     */
    void disconnect();
    
    /**
     * @brief Проверить состояние подключения
     */
    [[nodiscard]] bool is_connected() const noexcept;
    
    /**
     * @brief Получить текущее состояние
     */
    [[nodiscard]] StratumState get_state() const noexcept;
    
    // ==========================================================================
    // Отправка данных
    // ==========================================================================
    
    /**
     * @brief Отправить share
     * 
     * @param job_id ID задания
     * @param extranonce2 Extranonce2 (hex)
     * @param ntime Timestamp (hex)
     * @param nonce Nonce (hex)
     * @return Result<SubmitResult> Результат или ошибка
     */
    [[nodiscard]] Result<SubmitResult> submit(
        const std::string& job_id,
        const std::string& extranonce2,
        const std::string& ntime,
        const std::string& nonce
    );
    
    // ==========================================================================
    // Callbacks
    // ==========================================================================
    
    /**
     * @brief Установить callback для новых заданий
     */
    void set_job_callback(JobCallback callback);
    
    /**
     * @brief Установить callback для изменения сложности
     */
    void set_difficulty_callback(DifficultyCallback callback);
    
    /**
     * @brief Установить callback для отключения
     */
    void set_disconnect_callback(DisconnectCallback callback);
    
    // ==========================================================================
    // Информация
    // ==========================================================================
    
    /**
     * @brief Получить параметры подписки
     */
    [[nodiscard]] std::optional<SubscribeResult> get_subscribe_result() const;
    
    /**
     * @brief Получить текущее задание
     */
    [[nodiscard]] std::optional<StratumJob> get_current_job() const;
    
    /**
     * @brief Получить текущую сложность
     */
    [[nodiscard]] double get_difficulty() const noexcept;
    
    /**
     * @brief Получить extranonce1
     */
    [[nodiscard]] std::string get_extranonce1() const;
    
    /**
     * @brief Получить размер extranonce2
     */
    [[nodiscard]] uint32_t get_extranonce2_size() const noexcept;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::fallback
