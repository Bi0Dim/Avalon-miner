/**
 * @file rpc_client.hpp
 * @brief HTTP клиент для Bitcoin Core RPC
 * 
 * Асинхронный RPC клиент для взаимодействия с Bitcoin Core.
 * Использует libcurl для HTTP запросов.
 * 
 * Поддерживаемые методы:
 * - getblocktemplate: получение шаблона блока для майнинга
 * - submitblock: отправка найденного блока
 * - getblockchaininfo: информация о блокчейне
 * - getbestblockhash: хеш лучшего блока
 */

#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "block.hpp"

#include <memory>
#include <functional>
#include <optional>

namespace quaxis::bitcoin {

// =============================================================================
// Структуры данных RPC
// =============================================================================

/**
 * @brief Информация о блокчейне от getblockchaininfo
 */
struct BlockchainInfo {
    std::string chain;           ///< "main", "test", "regtest"
    uint32_t blocks;             ///< Количество блоков
    uint32_t headers;            ///< Количество заголовков
    std::string best_blockhash;  ///< Хеш лучшего блока
    double difficulty;           ///< Текущая сложность
    uint64_t median_time;        ///< Median time past
    bool initial_block_download; ///< В процессе IBD?
};

/**
 * @brief Шаблон блока от getblocktemplate
 */
struct BlockTemplateData {
    uint32_t version;
    Hash256 prev_blockhash;
    uint32_t curtime;
    uint32_t bits;
    uint32_t height;
    int64_t coinbase_value;    ///< Награда + комиссии
    std::string target;        ///< Target в hex
    uint64_t mintime;          ///< Минимальный допустимый timestamp
    std::vector<std::string> transactions;  ///< Транзакции в hex (для полного блока)
};

// =============================================================================
// RPC Client
// =============================================================================

/**
 * @brief Асинхронный RPC клиент для Bitcoin Core
 */
class RpcClient {
public:
    /**
     * @brief Создать клиент с конфигурацией
     * 
     * @param config Конфигурация Bitcoin RPC
     */
    explicit RpcClient(const BitcoinConfig& config);
    
    /**
     * @brief Деструктор
     */
    ~RpcClient();
    
    // Запрещаем копирование
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    
    // Разрешаем перемещение
    RpcClient(RpcClient&&) noexcept;
    RpcClient& operator=(RpcClient&&) noexcept;
    
    // =========================================================================
    // Методы RPC
    // =========================================================================
    
    /**
     * @brief Получить информацию о блокчейне
     * 
     * @return Result<BlockchainInfo> Информация или ошибка
     */
    [[nodiscard]] Result<BlockchainInfo> get_blockchain_info();
    
    /**
     * @brief Получить хеш лучшего блока
     * 
     * @return Result<Hash256> Хеш блока или ошибка
     */
    [[nodiscard]] Result<Hash256> get_best_block_hash();
    
    /**
     * @brief Получить шаблон блока для майнинга
     * 
     * @return Result<BlockTemplateData> Шаблон или ошибка
     */
    [[nodiscard]] Result<BlockTemplateData> get_block_template();
    
    /**
     * @brief Отправить найденный блок
     * 
     * @param block_hex Блок в hex формате
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> submit_block(std::string_view block_hex);
    
    /**
     * @brief Проверить соединение с Bitcoin Core
     * 
     * @return Result<void> Успех если соединение работает
     */
    [[nodiscard]] Result<void> ping();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Фабрика
// =============================================================================

/**
 * @brief Создать RPC клиент с автоматическим определением конфигурации
 * 
 * Пытается подключиться к Bitcoin Core на стандартных портах.
 * 
 * @return Result<RpcClient> Клиент или ошибка
 */
[[nodiscard]] Result<RpcClient> create_rpc_client_auto();

} // namespace quaxis::bitcoin
