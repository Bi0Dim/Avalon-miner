/**
 * @file base_chain.hpp
 * @brief Базовая реализация auxiliary chain
 * 
 * Предоставляет общую функциональность для всех auxiliary chains,
 * которую можно переопределить в специфичных реализациях.
 */

#pragma once

#include "../chain_interface.hpp"
#include "../chain_manager.hpp"
#include "../rpc/aux_rpc_client.hpp"

#include <atomic>
#include <mutex>
#include <optional>

namespace quaxis::merged {

/**
 * @brief Базовая реализация auxiliary chain
 * 
 * Предоставляет:
 * - Управление состоянием (enabled, connected)
 * - RPC подключение
 * - Кэширование шаблонов
 */
class BaseChain : public IChain {
public:
    /**
     * @brief Создать chain с конфигурацией
     * 
     * @param config Конфигурация chain
     */
    explicit BaseChain(const ChainConfig& config);
    
    ~BaseChain() override;
    
    // =========================================================================
    // IChain Interface - Информация
    // =========================================================================
    
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] std::string_view ticker() const noexcept override;
    [[nodiscard]] const Hash256& chain_id() const noexcept override;
    [[nodiscard]] uint32_t priority() const noexcept override;
    [[nodiscard]] ChainInfo get_info() const noexcept override;
    
    // =========================================================================
    // IChain Interface - Статус
    // =========================================================================
    
    [[nodiscard]] ChainStatus status() const noexcept override;
    [[nodiscard]] Result<void> connect() override;
    void disconnect() override;
    [[nodiscard]] bool is_connected() const noexcept override;
    
    // =========================================================================
    // IChain Interface - Майнинг
    // =========================================================================
    
    [[nodiscard]] Result<AuxBlockTemplate> get_block_template() override;
    [[nodiscard]] Result<void> submit_block(
        const AuxPow& auxpow,
        const AuxBlockTemplate& block_template
    ) override;
    [[nodiscard]] bool meets_target(
        const Hash256& pow_hash,
        const AuxBlockTemplate& current_template
    ) const noexcept override;
    
    // =========================================================================
    // IChain Interface - Конфигурация
    // =========================================================================
    
    void set_enabled(bool enabled) override;
    [[nodiscard]] bool is_enabled() const noexcept override;
    void set_priority(uint32_t priority) override;
    
protected:
    /**
     * @brief Инициализировать информацию о chain
     * 
     * Вызывается из конструкторов наследников после полной инициализации.
     */
    void init_chain_info();
    
    /**
     * @brief Получить имя chain (для переопределения в наследниках)
     */
    [[nodiscard]] virtual std::string get_chain_name() const = 0;
    
    /**
     * @brief Получить тикер (для переопределения в наследниках)
     */
    [[nodiscard]] virtual std::string get_chain_ticker() const = 0;
    
    /**
     * @brief Получить chain ID (для переопределения в наследниках)
     */
    [[nodiscard]] virtual Hash256 get_chain_id() const = 0;
    
    /**
     * @brief Получить метод RPC для createauxblock
     */
    [[nodiscard]] virtual std::string get_create_aux_block_method() const;
    
    /**
     * @brief Получить метод RPC для submitauxblock
     */
    [[nodiscard]] virtual std::string get_submit_aux_block_method() const;
    
    /**
     * @brief Парсить ответ createauxblock
     */
    [[nodiscard]] virtual Result<AuxBlockTemplate> parse_aux_block_response(
        const std::string& response
    ) const;
    
    // Конфигурация
    ChainConfig config_;
    
    // RPC клиент
    std::unique_ptr<AuxRpcClient> rpc_client_;
    
    // Состояние
    std::atomic<bool> enabled_{true};
    std::atomic<ChainStatus> status_{ChainStatus::Disconnected};
    mutable std::mutex mutex_;
    
    // Кэш
    std::string name_cache_;
    std::string ticker_cache_;
    Hash256 chain_id_cache_{};
    
    // Информация
    mutable ChainInfo info_;
    mutable std::mutex info_mutex_;
};

} // namespace quaxis::merged
