/**
 * @file asic_connection.hpp
 * @brief Соединение с ASIC майнером
 * 
 * Управляет TCP соединением с одним ASIC устройством:
 * - Асинхронный приём/отправка данных
 * - Парсинг протокола
 * - Heartbeat для проверки соединения
 * - Очередь заданий
 */

#pragma once

#include "protocol.hpp"
#include "../mining/job.hpp"
#include "../core/config.hpp"

#include <memory>
#include <functional>
#include <string>
#include <queue>

namespace quaxis::network {

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback при получении share
 */
using ShareReceivedCallback = std::function<void(const mining::Share& share)>;

/**
 * @brief Callback при отключении
 */
using DisconnectedCallback = std::function<void()>;

/**
 * @brief Callback при получении статуса
 */
using StatusReceivedCallback = std::function<void(const StatusMessage& status)>;

// =============================================================================
// Статистика соединения
// =============================================================================

/**
 * @brief Статистика ASIC соединения
 */
struct ConnectionStats {
    uint64_t shares_received = 0;     ///< Количество полученных shares
    uint64_t jobs_sent = 0;           ///< Количество отправленных заданий
    uint64_t bytes_received = 0;      ///< Байт получено
    uint64_t bytes_sent = 0;          ///< Байт отправлено
    uint32_t last_hashrate = 0;       ///< Последний известный хешрейт
    uint8_t last_temperature = 0;     ///< Последняя температура
    std::chrono::steady_clock::time_point connected_at;  ///< Время подключения
    std::chrono::steady_clock::time_point last_share_at; ///< Время последнего share
};

// =============================================================================
// ASIC Connection
// =============================================================================

/**
 * @brief Соединение с ASIC майнером
 */
class AsicConnection {
public:
    /**
     * @brief Создать соединение из socket fd
     * 
     * @param socket_fd Файловый дескриптор сокета
     * @param remote_addr Адрес удалённой стороны
     */
    AsicConnection(int socket_fd, std::string remote_addr);
    
    ~AsicConnection();
    
    // Запрещаем копирование
    AsicConnection(const AsicConnection&) = delete;
    AsicConnection& operator=(const AsicConnection&) = delete;
    
    // Разрешаем перемещение
    AsicConnection(AsicConnection&&) noexcept;
    AsicConnection& operator=(AsicConnection&&) noexcept;
    
    // =========================================================================
    // Управление соединением
    // =========================================================================
    
    /**
     * @brief Запустить обработку соединения
     */
    void start();
    
    /**
     * @brief Остановить и закрыть соединение
     */
    void stop();
    
    /**
     * @brief Проверить, активно ли соединение
     */
    [[nodiscard]] bool is_connected() const noexcept;
    
    // =========================================================================
    // Отправка данных
    // =========================================================================
    
    /**
     * @brief Отправить новое задание
     * 
     * @param job Задание для отправки
     * @return true если успешно добавлено в очередь
     */
    bool send_job(const mining::Job& job);
    
    /**
     * @brief Отправить команду остановки
     */
    bool send_stop();
    
    /**
     * @brief Отправить heartbeat
     */
    bool send_heartbeat();
    
    /**
     * @brief Отправить новый target
     */
    bool send_target(const Hash256& target);
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    void set_share_callback(ShareReceivedCallback callback);
    void set_disconnected_callback(DisconnectedCallback callback);
    void set_status_callback(StatusReceivedCallback callback);
    
    // =========================================================================
    // Информация
    // =========================================================================
    
    /**
     * @brief Получить адрес удалённой стороны
     */
    [[nodiscard]] const std::string& remote_address() const noexcept;
    
    /**
     * @brief Получить статистику соединения
     */
    [[nodiscard]] ConnectionStats stats() const;
    
    /**
     * @brief Получить количество заданий в очереди
     */
    [[nodiscard]] std::size_t pending_jobs() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::network
