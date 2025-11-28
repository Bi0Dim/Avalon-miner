/**
 * @file job_manager.hpp
 * @brief Менеджер заданий для майнинга
 * 
 * Отвечает за:
 * - Создание новых заданий при появлении блоков
 * - Распределение extranonce между заданиями (per-ASIC connection)
 * - Отслеживание активных заданий
 * - Инвалидацию устаревших заданий
 * 
 * IMPORTANT: Each ASIC connection has a unique extranonce managed by
 * ExtrannonceManager to prevent duplicate work across connections.
 */

#pragma once

#include "job.hpp"
#include "extranonce_manager.hpp"
#include "../core/config.hpp"
#include "../bitcoin/block.hpp"
#include "../bitcoin/coinbase.hpp"

#include <memory>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <queue>

namespace quaxis::mining {

// =============================================================================
// Callback типы
// =============================================================================

/**
 * @brief Callback при создании нового задания
 */
using NewJobCallback = std::function<void(const Job& job)>;

/**
 * @brief Callback при нахождении блока
 */
using BlockFoundCallback = std::function<void(
    const bitcoin::BlockHeader& header,
    const Bytes& coinbase_tx,
    uint32_t nonce
)>;

// =============================================================================
// Job Manager
// =============================================================================

/**
 * @brief Менеджер заданий для майнинга
 * 
 * Thread-safe менеджер, который:
 * 1. Принимает шаблоны блоков от Bitcoin Core
 * 2. Генерирует задания с уникальными extranonce (per connection)
 * 3. Отслеживает активные задания
 * 4. Валидирует shares от ASIC
 * 
 * IMPORTANT: Uses ExtrannonceManager internally to ensure each ASIC
 * connection has a unique extranonce, preventing duplicate work.
 */
class JobManager {
public:
    /**
     * @brief Создать менеджер заданий
     * 
     * @param config Конфигурация майнинга
     * @param coinbase_builder Построитель coinbase транзакций
     */
    explicit JobManager(
        const MiningConfig& config,
        bitcoin::CoinbaseBuilder coinbase_builder
    );
    
    ~JobManager();
    
    // Запрещаем копирование
    JobManager(const JobManager&) = delete;
    JobManager& operator=(const JobManager&) = delete;
    
    // =========================================================================
    // Управление заданиями
    // =========================================================================
    
    /**
     * @brief Обработать новый блок
     * 
     * Создаёт новые задания на основе шаблона блока.
     * Инвалидирует все предыдущие задания.
     * 
     * NOTE: This method does NOT increment a global extranonce.
     * Each connection has its own extranonce managed by ExtrannonceManager.
     * The template will be updated with connection-specific extranonce
     * when get_next_job_for_connection() is called.
     * 
     * @param block_template Шаблон блока от Bitcoin Core
     * @param is_speculative true для spy mining (блок ещё не валидирован)
     */
    void on_new_block(
        const bitcoin::BlockTemplate& block_template,
        bool is_speculative = false
    );
    
    /**
     * @brief Подтвердить speculative блок
     * 
     * Вызывается когда spy mining блок подтверждён как валидный.
     */
    void confirm_speculative_block();
    
    /**
     * @brief Отменить speculative блок
     * 
     * Вызывается когда spy mining блок оказался невалидным.
     * Все задания на основе этого блока инвалидируются.
     */
    void invalidate_speculative_block();
    
    /**
     * @brief Получить следующее задание
     * 
     * Возвращает готовое задание для отправки на ASIC.
     * Автоматически инкрементирует extranonce.
     * 
     * @deprecated Use get_next_job_for_connection() instead for proper
     *             per-connection extranonce management.
     * 
     * @return std::optional<Job> Задание или nullopt если нет шаблона
     */
    [[nodiscard]] std::optional<Job> get_next_job();
    
    /**
     * @brief Get next job for a specific ASIC connection
     * 
     * Each connection has a unique extranonce assigned by ExtrannonceManager.
     * This method creates a job with that connection's specific extranonce,
     * ensuring no duplicate work across connections.
     * 
     * @param connection_id Unique identifier for the ASIC connection
     * @return std::optional<Job> Job with this connection's extranonce, or nullopt
     */
    [[nodiscard]] std::optional<Job> get_next_job_for_connection(uint32_t connection_id);
    
    /**
     * @brief Получить задание по ID
     * 
     * @param job_id ID задания
     * @return std::optional<Job> Задание или nullopt если не найдено
     */
    [[nodiscard]] std::optional<Job> get_job(uint32_t job_id) const;
    
    // =========================================================================
    // Connection Management (ExtrannonceManager integration)
    // =========================================================================
    
    /**
     * @brief Register a new ASIC connection
     * 
     * Assigns a unique extranonce to the connection.
     * Call this when a new ASIC connects.
     * 
     * @param connection_id Unique identifier for the connection
     * @return uint64_t The assigned extranonce
     */
    uint64_t register_connection(uint32_t connection_id);
    
    /**
     * @brief Unregister an ASIC connection
     * 
     * Releases the extranonce for the connection.
     * Call this when an ASIC disconnects.
     * 
     * @param connection_id Connection to unregister
     */
    void unregister_connection(uint32_t connection_id);
    
    /**
     * @brief Get extranonce for a specific connection
     * 
     * @param connection_id Connection to look up
     * @return std::optional<uint64_t> Extranonce or nullopt if not found
     */
    [[nodiscard]] std::optional<uint64_t> get_connection_extranonce(uint32_t connection_id) const;
    
    /**
     * @brief Get the number of active ASIC connections
     * 
     * @return std::size_t Number of connections with assigned extranonces
     */
    [[nodiscard]] std::size_t active_connection_count() const;
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для новых заданий
     */
    void set_new_job_callback(NewJobCallback callback);
    
    /**
     * @brief Установить callback для найденных блоков
     */
    void set_block_found_callback(BlockFoundCallback callback);
    
    // =========================================================================
    // Статистика
    // =========================================================================
    
    /**
     * @brief Получить количество активных заданий
     */
    [[nodiscard]] std::size_t active_job_count() const;
    
    /**
     * @brief Получить текущий extranonce
     * 
     * @deprecated Use get_connection_extranonce() for per-connection extranonce.
     *             This returns the next extranonce that will be assigned.
     */
    [[nodiscard]] uint64_t current_extranonce() const;
    
    /**
     * @brief Получить текущую высоту блока
     */
    [[nodiscard]] uint32_t current_height() const;
    
    /**
     * @brief Есть ли активный шаблон?
     */
    [[nodiscard]] bool has_template() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::mining
