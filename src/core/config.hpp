/**
 * @file config.hpp
 * @brief Конфигурация Quaxis Solo Miner
 * 
 * Загрузка и парсинг конфигурации из TOML файла.
 * Поддерживает все настройки сервера, Bitcoin RPC, майнинга и мониторинга.
 * 
 * Пример конфигурации (quaxis.toml):
 * @code
 * [server]
 * bind_address = "0.0.0.0"
 * port = 3333
 * max_connections = 10
 * 
 * [bitcoin]
 * rpc_host = "127.0.0.1"
 * rpc_port = 8332
 * rpc_user = "quaxis"
 * rpc_password = "password"
 * payout_address = "bc1q..."
 * 
 * [mining]
 * coinbase_tag = "quaxis"
 * extranonce_size = 6
 * job_queue_size = 100
 * use_spy_mining = true
 * use_mtp_timestamp = true
 * 
 * [shm]
 * enabled = true
 * path = "/quaxis_block"
 * spin_wait = true
 * 
 * [monitoring]
 * stats_interval = 60
 * log_level = "info"
 * @endcode
 */

#pragma once

#include "types.hpp"
#include "constants.hpp"

#include <string>
#include <filesystem>
#include <optional>

namespace quaxis {

// =============================================================================
// Структуры конфигурации
// =============================================================================

/**
 * @brief Настройки TCP сервера для ASIC
 */
struct ServerConfig {
    /// @brief Адрес для прослушивания (по умолчанию "0.0.0.0")
    std::string bind_address = "0.0.0.0";
    
    /// @brief Порт для прослушивания (по умолчанию 3333)
    uint16_t port = constants::DEFAULT_SERVER_PORT;
    
    /// @brief Максимальное количество подключений ASIC
    std::size_t max_connections = constants::DEFAULT_MAX_CONNECTIONS;
};

/**
 * @brief Настройки подключения к Bitcoin Core
 */
struct BitcoinConfig {
    /// @brief Хост Bitcoin Core RPC
    std::string rpc_host = "127.0.0.1";
    
    /// @brief Порт Bitcoin Core RPC
    uint16_t rpc_port = constants::BITCOIN_RPC_PORT_MAINNET;
    
    /// @brief Имя пользователя RPC
    std::string rpc_user = "quaxis";
    
    /// @brief Пароль RPC
    std::string rpc_password;
    
    /// @brief Адрес для выплаты награды (P2WPKH формат bc1q...)
    std::string payout_address;
    
    /**
     * @brief Получить URL для RPC запросов
     * @return URL в формате "http://host:port/"
     */
    [[nodiscard]] std::string get_rpc_url() const;
};

/**
 * @brief Настройки майнинга
 */
struct MiningConfig {
    /// @brief Тег в coinbase scriptsig (по умолчанию "quaxis")
    std::string coinbase_tag = "quaxis";
    
    /// @brief Размер extranonce в байтах (1-8, по умолчанию 6)
    std::size_t extranonce_size = constants::EXTRANONCE_SIZE;
    
    /// @brief Размер очереди заданий для ASIC
    std::size_t job_queue_size = constants::DEFAULT_JOB_QUEUE_SIZE;
    
    /// @brief Включить spy mining (начинать майнинг до полной валидации)
    bool use_spy_mining = true;
    
    /// @brief Использовать MTP+1 timestamp (минимально допустимый)
    bool use_mtp_timestamp = true;
    
    /// @brief Создавать пустые блоки (только coinbase, без других транзакций)
    bool empty_blocks_only = true;
};

/**
 * @brief Настройки Shared Memory
 */
struct ShmConfig {
    /// @brief Включить shared memory для получения блоков
    bool enabled = true;
    
    /// @brief Путь к shared memory объекту
    std::string path = constants::DEFAULT_SHM_PATH;
    
    /// @brief Использовать spin-wait вместо блокировки
    bool spin_wait = true;
};

/**
 * @brief Настройки мониторинга и логирования
 */
struct MonitoringConfig {
    /// @brief Интервал вывода статистики в секундах
    uint32_t stats_interval = 60;
    
    /// @brief Уровень логирования: "debug", "info", "warning", "error"
    std::string log_level = "info";
    
    /// @brief Путь к файлу лога (пустой = stdout)
    std::string log_file;
};

/**
 * @brief Полная конфигурация Quaxis Solo Miner
 */
struct Config {
    ServerConfig server;
    BitcoinConfig bitcoin;
    MiningConfig mining;
    ShmConfig shm;
    MonitoringConfig monitoring;
    
    /**
     * @brief Загрузить конфигурацию из TOML файла
     * 
     * @param path Путь к файлу конфигурации
     * @return Result<Config> Конфигурация или ошибка
     */
    [[nodiscard]] static Result<Config> load(const std::filesystem::path& path);
    
    /**
     * @brief Загрузить конфигурацию с поиском файла
     * 
     * Ищет файл в следующем порядке:
     * 1. Указанный путь
     * 2. ./quaxis.toml
     * 3. /etc/quaxis/quaxis.toml
     * 4. ~/.config/quaxis/quaxis.toml
     * 
     * @param path Опциональный путь к файлу
     * @return Result<Config> Конфигурация или ошибка
     */
    [[nodiscard]] static Result<Config> load_with_search(
        const std::optional<std::filesystem::path>& path = std::nullopt
    );
    
    /**
     * @brief Валидация конфигурации
     * 
     * Проверяет корректность всех значений:
     * - Валидность Bitcoin адреса
     * - Непустые обязательные поля
     * - Диапазоны числовых значений
     * 
     * @return Result<void> Успех или ошибка валидации
     */
    [[nodiscard]] Result<void> validate() const;
};

} // namespace quaxis
