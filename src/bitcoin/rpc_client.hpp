/**
 * @file rpc_client.hpp
 * @brief HTTP клиент для RPC запросов
 * 
 * RPC клиент для взаимодействия с Bitcoin-совместимыми нодами.
 * Используется для fallback и отправки найденных блоков.
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
#include "block.hpp"

#include <memory>
#include <functional>
#include <optional>

namespace quaxis::bitcoin {

// =============================================================================
// Конфигурация RPC
// =============================================================================

/**
 * @brief Конфигурация RPC клиента
 * 
 * Используется для fallback подключения к Bitcoin-совместимым нодам.
 */
struct RpcConfig {
    /// @brief Хост RPC сервера
    std::string host = "127.0.0.1";
    
    /// @brief Порт RPC сервера
    uint16_t port = 8332;
    
    /// @brief Имя пользователя
    std::string user = "";
    
    /// @brief Пароль
    std::string password = "";
    
    /// @brief Таймаут в секундах
    uint32_t timeout = 30;
    
    /**
     * @brief Получить URL для RPC запросов
     */
    [[nodiscard]] std::string get_url() const {
        return "http://" + host + ":" + std::to_string(port) + "/";
    }
};

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
 * @brief RPC клиент для Bitcoin-совместимых нод
 * 
 * Используется для fallback и отправки блоков.
 */
class RpcClient {
public:
    /**
     * @brief Создать клиент с конфигурацией
     * 
     * @param config Конфигурация RPC
     */
    explicit RpcClient(const RpcConfig& config);
    
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
     * @brief Проверить соединение
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
 * Пытается подключиться на стандартных портах.
 * 
 * @return Result<RpcClient> Клиент или ошибка
 */
[[nodiscard]] Result<RpcClient> create_rpc_client_auto();

} // namespace quaxis::bitcoin
