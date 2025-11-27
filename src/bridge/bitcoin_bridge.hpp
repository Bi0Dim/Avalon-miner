/**
 * @file bitcoin_bridge.hpp
 * @brief Мост к Bitcoin Core с поддержкой fallback
 * 
 * Объединяет три источника получения данных:
 * 1. SHM (Shared Memory) - основной, минимальная латентность
 * 2. ZMQ - первый резерв
 * 3. Stratum - второй резерв (пул)
 * 
 * Автоматически переключается между источниками при проблемах.
 */

#pragma once

#include "../core/types.hpp"
#include "../bitcoin/block.hpp"
#include "../bitcoin/shm_subscriber.hpp"
#include "../fallback/fallback_manager.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace quaxis::bridge {

// =============================================================================
// Block Template
// =============================================================================

/**
 * @brief Шаблон блока для майнинга
 * 
 * Унифицированная структура независимо от источника данных.
 */
struct BlockTemplate {
    /// @brief Заголовок блока
    bitcoin::BlockHeader header;
    
    /// @brief Высота блока
    uint32_t height{0};
    
    /// @brief Сложность (bits)
    uint32_t bits{0};
    
    /// @brief Target для проверки PoW
    Hash256 target{};
    
    /// @brief Награда за блок (satoshi)
    int64_t coinbase_value{0};
    
    /// @brief Previous block hash
    Hash256 prev_block_hash{};
    
    /// @brief Merkle root
    Hash256 merkle_root{};
    
    /// @brief Время получения
    std::chrono::steady_clock::time_point received_at;
    
    /// @brief Источник данных
    fallback::FallbackMode source{fallback::FallbackMode::PrimarySHM};
    
    /// @brief Это speculative (spy mining)?
    bool is_speculative{false};
    
    /// @brief Job ID (для Stratum)
    std::string job_id;
    
    /// @brief Coinbase части (для Stratum)
    std::string coinbase1;
    std::string coinbase2;
    
    /// @brief Extranonce1 (для Stratum)
    std::string extranonce1;
    
    /// @brief Размер extranonce2 (для Stratum)
    uint32_t extranonce2_size{4};
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback при получении нового блока/задания
 */
using NewTemplateCallback = std::function<void(const BlockTemplate&)>;

/**
 * @brief Callback при смене источника
 */
using SourceChangeCallback = std::function<void(
    fallback::FallbackMode old_source, 
    fallback::FallbackMode new_source
)>;

// =============================================================================
// Bitcoin Bridge
// =============================================================================

/**
 * @brief Конфигурация моста
 */
struct BridgeConfig {
    /// @brief Конфигурация SHM
    ShmConfig shm;
    
    /// @brief Конфигурация fallback
    fallback::FallbackConfig fallback;
    
    /// @brief Включить автоматическое переключение
    bool auto_switch{true};
    
    /// @brief Интервал health check (секунды)
    uint32_t health_check_interval{1};
};

/**
 * @brief Мост к Bitcoin Core с поддержкой fallback
 * 
 * Обеспечивает единый интерфейс для получения заданий майнинга
 * независимо от источника данных.
 */
class BitcoinBridge {
public:
    /**
     * @brief Создать мост с конфигурацией
     */
    explicit BitcoinBridge(const BridgeConfig& config);
    
    ~BitcoinBridge();
    
    // Запрещаем копирование
    BitcoinBridge(const BitcoinBridge&) = delete;
    BitcoinBridge& operator=(const BitcoinBridge&) = delete;
    
    // ==========================================================================
    // Управление
    // ==========================================================================
    
    /**
     * @brief Запустить мост
     * 
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> start();
    
    /**
     * @brief Остановить мост
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли мост
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    // ==========================================================================
    // Получение данных
    // ==========================================================================
    
    /**
     * @brief Получить текущий шаблон блока
     * 
     * @return Шаблон или nullopt если недоступен
     */
    [[nodiscard]] std::optional<BlockTemplate> get_template() const;
    
    /**
     * @brief Получить текущий источник данных
     */
    [[nodiscard]] fallback::FallbackMode current_source() const noexcept;
    
    /**
     * @brief Проверить, подключён ли к Bitcoin Core
     */
    [[nodiscard]] bool is_bitcoin_connected() const;
    
    /**
     * @brief Получить возраст текущего задания (ms)
     */
    [[nodiscard]] uint64_t get_current_job_age_ms() const;
    
    // ==========================================================================
    // Отправка данных
    // ==========================================================================
    
    /**
     * @brief Отправить найденный share
     * 
     * @param job_id ID задания (для Stratum)
     * @param extranonce2 Extranonce2
     * @param ntime Timestamp
     * @param nonce Nonce
     * @return Result<bool> true если принят
     */
    [[nodiscard]] Result<bool> submit_share(
        const std::string& job_id,
        const std::string& extranonce2,
        const std::string& ntime,
        const std::string& nonce
    );
    
    // ==========================================================================
    // Callbacks
    // ==========================================================================
    
    /**
     * @brief Установить callback для новых шаблонов
     */
    void set_template_callback(NewTemplateCallback callback);
    
    /**
     * @brief Установить callback для смены источника
     */
    void set_source_change_callback(SourceChangeCallback callback);
    
    // ==========================================================================
    // Fallback Manager
    // ==========================================================================
    
    /**
     * @brief Получить доступ к менеджеру fallback
     */
    [[nodiscard]] fallback::FallbackManager& get_fallback_manager();
    
    /**
     * @brief Получить доступ к менеджеру fallback (const)
     */
    [[nodiscard]] const fallback::FallbackManager& get_fallback_manager() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::bridge
