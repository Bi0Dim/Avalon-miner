/**
 * @file reward_dispatcher.hpp
 * @brief Диспетчер отправки найденных блоков
 * 
 * Отвечает за проверку найденных блоков и отправку их
 * в соответствующие chains (Bitcoin и auxiliary chains).
 */

#pragma once

#include "chain_manager.hpp"
#include "merged_job_creator.hpp"
#include "../bitcoin/block.hpp"

#include <memory>
#include <functional>
#include <vector>

namespace quaxis::merged {

/**
 * @brief Результат отправки блока
 */
struct DispatchResult {
    /// @brief Название chain
    std::string chain_name;
    
    /// @brief Успешно ли отправлен блок
    bool success{false};
    
    /// @brief Сообщение об ошибке (если есть)
    std::string error_message;
    
    /// @brief Высота блока
    uint32_t height{0};
    
    /// @brief Хеш блока
    Hash256 block_hash{};
};

/**
 * @brief Callback при успешной отправке блока
 */
using BlockDispatchedCallback = std::function<void(
    const DispatchResult& result
)>;

/**
 * @brief Диспетчер отправки блоков
 * 
 * Когда найден блок Bitcoin:
 * 1. Проверяет хеш против target Bitcoin
 * 2. Проверяет хеш против target каждой auxiliary chain
 * 3. Отправляет блок во все подходящие chains
 */
class RewardDispatcher {
public:
    /**
     * @brief Создать диспетчер
     * 
     * @param chain_manager Менеджер auxiliary chains
     */
    explicit RewardDispatcher(ChainManager& chain_manager);
    
    ~RewardDispatcher();
    
    // Запрещаем копирование
    RewardDispatcher(const RewardDispatcher&) = delete;
    RewardDispatcher& operator=(const RewardDispatcher&) = delete;
    
    /**
     * @brief Проверить и отправить найденный блок
     * 
     * @param header Заголовок Bitcoin блока (80 байт)
     * @param coinbase_tx Coinbase транзакция
     * @param nonce Найденный nonce
     * @param merged_job Исходное merged mining задание
     * @return std::vector<DispatchResult> Результаты отправки
     */
    [[nodiscard]] std::vector<DispatchResult> dispatch_block(
        const bitcoin::BlockHeader& header,
        const Bytes& coinbase_tx,
        uint32_t nonce,
        const MergedJob& merged_job
    );
    
    /**
     * @brief Проверить блок для всех chains
     * 
     * Не отправляет блок, только проверяет.
     * 
     * @param header Заголовок блока
     * @return std::vector<std::string> Имена chains, для которых блок подходит
     */
    [[nodiscard]] std::vector<std::string> check_all_chains(
        const bitcoin::BlockHeader& header
    ) const;
    
    /**
     * @brief Установить callback для отправленных блоков
     */
    void set_dispatch_callback(BlockDispatchedCallback callback);
    
    /**
     * @brief Получить статистику отправленных блоков
     */
    [[nodiscard]] std::unordered_map<std::string, uint32_t> get_dispatch_stats() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::merged
