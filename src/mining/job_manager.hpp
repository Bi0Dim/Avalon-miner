/**
 * @file job_manager.hpp
 * @brief Менеджер заданий для майнинга
 * 
 * Отвечает за:
 * - Создание новых заданий при появлении блоков
 * - Распределение extranonce между заданиями
 * - Отслеживание активных заданий
 * - Инвалидацию устаревших заданий
 */

#pragma once

#include "job.hpp"
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
 * 2. Генерирует задания с уникальными extranonce
 * 3. Отслеживает активные задания
 * 4. Валидирует shares от ASIC
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
     * @return std::optional<Job> Задание или nullopt если нет шаблона
     */
    [[nodiscard]] std::optional<Job> get_next_job();
    
    /**
     * @brief Получить задание по ID
     * 
     * @param job_id ID задания
     * @return std::optional<Job> Задание или nullopt если не найдено
     */
    [[nodiscard]] std::optional<Job> get_job(uint32_t job_id) const;
    
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
