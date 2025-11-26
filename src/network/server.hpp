/**
 * @file server.hpp
 * @brief TCP сервер для ASIC майнеров
 * 
 * Принимает входящие подключения от ASIC устройств,
 * распределяет задания и собирает найденные shares.
 */

#pragma once

#include "asic_connection.hpp"
#include "../mining/job_manager.hpp"
#include "../core/config.hpp"

#include <memory>
#include <vector>
#include <functional>

namespace quaxis::network {

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback при подключении нового ASIC
 */
using AsicConnectedCallback = std::function<void(const std::string& address)>;

/**
 * @brief Callback при отключении ASIC
 */
using AsicDisconnectedCallback = std::function<void(const std::string& address)>;

// =============================================================================
// Server Statistics
// =============================================================================

/**
 * @brief Статистика сервера
 */
struct ServerStats {
    std::size_t active_connections = 0;
    uint64_t total_connections = 0;
    uint64_t total_shares = 0;
    uint64_t total_jobs_sent = 0;
    uint32_t total_hashrate = 0;  ///< Суммарный хешрейт всех ASIC
};

// =============================================================================
// Server
// =============================================================================

/**
 * @brief TCP сервер для ASIC майнеров
 * 
 * Функции:
 * - Принимает входящие TCP подключения
 * - Управляет соединениями с ASIC
 * - Распределяет задания от JobManager
 * - Собирает и обрабатывает shares
 */
class Server {
public:
    /**
     * @brief Создать сервер
     * 
     * @param config Конфигурация сервера
     * @param job_manager Менеджер заданий
     */
    Server(const ServerConfig& config, mining::JobManager& job_manager);
    
    ~Server();
    
    // Запрещаем копирование
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    // =========================================================================
    // Управление сервером
    // =========================================================================
    
    /**
     * @brief Запустить сервер
     * 
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> start();
    
    /**
     * @brief Остановить сервер
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли сервер
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    // =========================================================================
    // Broadcast
    // =========================================================================
    
    /**
     * @brief Отправить задание всем подключённым ASIC
     * 
     * @param job Задание для отправки
     */
    void broadcast_job(const mining::Job& job);
    
    /**
     * @brief Отправить команду остановки всем ASIC
     */
    void broadcast_stop();
    
    /**
     * @brief Отправить новый target всем ASIC
     */
    void broadcast_target(const Hash256& target);
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    void set_connected_callback(AsicConnectedCallback callback);
    void set_disconnected_callback(AsicDisconnectedCallback callback);
    
    // =========================================================================
    // Информация
    // =========================================================================
    
    /**
     * @brief Получить статистику сервера
     */
    [[nodiscard]] ServerStats stats() const;
    
    /**
     * @brief Получить количество активных подключений
     */
    [[nodiscard]] std::size_t connection_count() const;
    
    /**
     * @brief Получить список адресов подключённых ASIC
     */
    [[nodiscard]] std::vector<std::string> connected_addresses() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::network
