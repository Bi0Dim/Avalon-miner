/**
 * @file chain_manager.hpp
 * @brief Менеджер auxiliary chains для merged mining
 * 
 * Отвечает за:
 * - Управление подключениями к auxiliary chains
 * - Получение и обновление шаблонов блоков
 * - Проверку найденных блоков на соответствие target каждой chain
 * - Отправку блоков в соответствующие chains
 */

#pragma once

#include "chain_interface.hpp"
#include "auxpow.hpp"

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <functional>
#include <chrono>

namespace quaxis::merged {

// =============================================================================
// Конфигурация chain
// =============================================================================

/**
 * @brief Конфигурация одной auxiliary chain
 */
struct ChainConfig {
    /// @brief Название chain (например, "fractal", "namecoin")
    std::string name;
    
    /// @brief Включена ли chain
    bool enabled{true};
    
    /// @brief URL для RPC подключения
    std::string rpc_url;
    
    /// @brief Имя пользователя RPC (опционально)
    std::string rpc_user;
    
    /// @brief Пароль RPC (опционально)
    std::string rpc_password;
    
    /// @brief Адрес для получения награды за найденный блок
    /// КРИТИЧЕСКИ ВАЖНО: Без этого адреса награды будут потеряны!
    /// Формат зависит от chain:
    /// - Namecoin: N... или nc1q... (bech32)
    /// - Syscoin: sys1q... (bech32)
    /// - RSK: 0x... (Ethereum-style)
    /// - Elastos: E...
    /// - и т.д.
    std::string payout_address;
    
    /// @brief Приоритет (выше = важнее)
    uint32_t priority{50};
    
    /// @brief Таймаут RPC запросов (секунды)
    uint32_t rpc_timeout{30};
    
    /// @brief Интервал обновления шаблона (секунды)
    uint32_t update_interval{5};
};

/**
 * @brief Конфигурация merged mining
 */
struct MergedMiningConfig {
    /// @brief Включен ли merged mining
    bool enabled{false};
    
    /// @brief Конфигурации отдельных chains
    std::vector<ChainConfig> chains;
    
    /// @brief Интервал проверки состояния chains (секунды)
    uint32_t health_check_interval{60};
};

// =============================================================================
// Callbacks
// =============================================================================

/**
 * @brief Callback при нахождении блока auxiliary chain
 */
using AuxBlockFoundCallback = std::function<void(
    const std::string& chain_name,
    uint32_t height,
    const Hash256& block_hash
)>;

// =============================================================================
// Chain Manager
// =============================================================================

/**
 * @brief Менеджер auxiliary chains
 * 
 * Централизованное управление всеми auxiliary chains:
 * - Автоматическое подключение/переподключение
 * - Обновление шаблонов блоков
 * - Проверка и отправка найденных блоков
 */
class ChainManager {
public:
    /**
     * @brief Создать менеджер с конфигурацией
     * 
     * @param config Конфигурация merged mining
     */
    explicit ChainManager(const MergedMiningConfig& config);
    
    ~ChainManager();
    
    // Запрещаем копирование
    ChainManager(const ChainManager&) = delete;
    ChainManager& operator=(const ChainManager&) = delete;
    
    // =========================================================================
    // Управление chains
    // =========================================================================
    
    /**
     * @brief Запустить менеджер
     * 
     * Начинает подключение к chains и обновление шаблонов.
     */
    void start();
    
    /**
     * @brief Остановить менеджер
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли менеджер
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    /**
     * @brief Получить список всех chains
     */
    [[nodiscard]] std::vector<std::string> get_chain_names() const;
    
    /**
     * @brief Получить информацию о chain
     * 
     * @param name Название chain
     * @return std::optional<ChainInfo> Информация или nullopt
     */
    [[nodiscard]] std::optional<ChainInfo> get_chain_info(
        std::string_view name
    ) const;
    
    /**
     * @brief Получить информацию обо всех chains
     */
    [[nodiscard]] std::vector<ChainInfo> get_all_chain_info() const;
    
    /**
     * @brief Включить/выключить chain
     * 
     * @param name Название chain
     * @param enabled true для включения
     * @return true если chain найден и состояние изменено
     */
    bool set_chain_enabled(std::string_view name, bool enabled);
    
    // =========================================================================
    // AuxPoW Commitment
    // =========================================================================
    
    /**
     * @brief Получить текущий AuxPoW commitment для coinbase
     * 
     * Создаёт commitment, включающий все активные auxiliary chains.
     * 
     * @return std::optional<AuxCommitment> Commitment или nullopt если нет активных chains
     */
    [[nodiscard]] std::optional<AuxCommitment> get_aux_commitment() const;
    
    /**
     * @brief Получить текущие шаблоны всех активных chains
     * 
     * @return std::vector<std::pair<std::string, AuxBlockTemplate>> Пары (name, template)
     */
    [[nodiscard]] std::vector<std::pair<std::string, AuxBlockTemplate>> 
        get_active_templates() const;
    
    // =========================================================================
    // Проверка и отправка блоков
    // =========================================================================
    
    /**
     * @brief Проверить найденный блок для всех chains
     * 
     * Проверяет, соответствует ли хеш родительского блока (Bitcoin)
     * target'у какой-либо из auxiliary chains.
     * 
     * @param parent_header Заголовок родительского блока (Bitcoin)
     * @param coinbase_tx Coinbase транзакция с AuxPoW commitment
     * @param coinbase_branch Merkle branch от coinbase к merkle root
     * @return std::vector<std::string> Имена chains, для которых блок подходит
     */
    [[nodiscard]] std::vector<std::string> check_aux_chains(
        const std::array<uint8_t, 80>& parent_header,
        const Bytes& coinbase_tx,
        const MerkleBranch& coinbase_branch
    ) const;
    
    /**
     * @brief Отправить блок в указанную chain
     * 
     * @param chain_name Название chain
     * @param auxpow AuxPoW доказательство
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> submit_aux_block(
        std::string_view chain_name,
        const AuxPow& auxpow
    );
    
    /**
     * @brief Отправить блок во все подходящие chains
     * 
     * @param parent_header Заголовок родительского блока
     * @param coinbase_tx Coinbase транзакция
     * @param coinbase_branch Merkle branch
     * @return std::vector<std::pair<std::string, bool>> Результаты (chain_name, success)
     */
    [[nodiscard]] std::vector<std::pair<std::string, bool>> submit_to_matching_chains(
        const std::array<uint8_t, 80>& parent_header,
        const Bytes& coinbase_tx,
        const MerkleBranch& coinbase_branch
    );
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для найденных блоков
     */
    void set_block_found_callback(AuxBlockFoundCallback callback);
    
    // =========================================================================
    // Статистика
    // =========================================================================
    
    /**
     * @brief Количество активных chains
     */
    [[nodiscard]] std::size_t active_chain_count() const noexcept;
    
    /**
     * @brief Количество найденных блоков по chain
     */
    [[nodiscard]] std::unordered_map<std::string, uint32_t> get_block_counts() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::merged
