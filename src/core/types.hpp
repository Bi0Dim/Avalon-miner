/**
 * @file types.hpp
 * @brief Базовые типы для Quaxis Solo Miner
 * 
 * Определяет основные типы данных, используемые во всём проекте:
 * - Hash256: 32-байтный хеш (SHA256, block hash, txid)
 * - Bytes: динамический массив байт
 * - Result<T>: обёртка std::expected для обработки ошибок
 * 
 * @note Все типы оптимизированы для минимального копирования и
 *       максимальной производительности в критическом пути майнинга.
 */

#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <system_error>

namespace quaxis {

// =============================================================================
// Базовые типы данных
// =============================================================================

/**
 * @brief 256-битный хеш (32 байта)
 * 
 * Используется для:
 * - SHA256 хешей
 * - Block hash
 * - Transaction ID (txid)
 * - Merkle root
 * 
 * Хранится в little-endian формате (как в Bitcoin протоколе).
 */
using Hash256 = std::array<uint8_t, 32>;

/**
 * @brief 160-битный хеш (20 байт)
 * 
 * Используется для:
 * - RIPEMD160(SHA256(pubkey)) в Bitcoin адресах
 * - P2PKH и P2WPKH выходах
 */
using Hash160 = std::array<uint8_t, 20>;

/**
 * @brief Динамический массив байт
 * 
 * Используется для сериализации транзакций, блоков и других данных
 * переменной длины.
 */
using Bytes = std::vector<uint8_t>;

/**
 * @brief Представление (view) на массив байт без владения
 * 
 * Используется для передачи данных без копирования.
 */
using ByteSpan = std::span<const uint8_t>;

/**
 * @brief Изменяемое представление на массив байт
 */
using MutableByteSpan = std::span<uint8_t>;

// =============================================================================
// Коды ошибок Quaxis
// =============================================================================

/**
 * @brief Перечисление кодов ошибок
 * 
 * Используется вместо исключений для обработки ошибок в критическом пути.
 * Соответствует требованию "никаких исключений в критическом пути".
 */
enum class ErrorCode {
    Success = 0,
    
    // Ошибки конфигурации (100-199)
    ConfigNotFound = 100,
    ConfigParseError = 101,
    ConfigInvalidValue = 102,
    
    // Ошибки сети (200-299)
    NetworkConnectionFailed = 200,
    NetworkTimeout = 201,
    NetworkSendFailed = 202,
    NetworkRecvFailed = 203,
    
    // Ошибки RPC (300-399)
    RpcConnectionFailed = 300,
    RpcAuthFailed = 301,
    RpcParseError = 302,
    RpcMethodNotFound = 303,
    RpcInvalidParams = 304,
    RpcInternalError = 305,
    
    // Ошибки Shared Memory (400-499)
    ShmOpenFailed = 400,
    ShmMapFailed = 401,
    ShmInvalidState = 402,
    
    // Ошибки майнинга (500-599)
    MiningInvalidJob = 500,
    MiningInvalidNonce = 501,
    MiningStaleJob = 502,
    MiningBlockRejected = 503,
    
    // Ошибки Bitcoin (600-699)
    BitcoinInvalidAddress = 600,
    BitcoinInvalidBlock = 601,
    BitcoinInvalidTransaction = 602,
    BitcoinTargetNotMet = 603,
    
    // Криптографические ошибки (700-799)
    CryptoHashError = 700,
    CryptoInvalidLength = 701,
    
    // Системные ошибки (800-899)
    SystemOutOfMemory = 800,
    SystemIOError = 801,
};

/**
 * @brief Преобразование кода ошибки в строку
 */
[[nodiscard]] constexpr std::string_view to_string(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success: return "Success";
        case ErrorCode::ConfigNotFound: return "Файл конфигурации не найден";
        case ErrorCode::ConfigParseError: return "Ошибка парсинга конфигурации";
        case ErrorCode::ConfigInvalidValue: return "Некорректное значение в конфигурации";
        case ErrorCode::NetworkConnectionFailed: return "Ошибка подключения к сети";
        case ErrorCode::NetworkTimeout: return "Таймаут сети";
        case ErrorCode::NetworkSendFailed: return "Ошибка отправки данных";
        case ErrorCode::NetworkRecvFailed: return "Ошибка получения данных";
        case ErrorCode::RpcConnectionFailed: return "Ошибка подключения к RPC";
        case ErrorCode::RpcAuthFailed: return "Ошибка авторизации RPC";
        case ErrorCode::RpcParseError: return "Ошибка парсинга ответа RPC";
        case ErrorCode::RpcMethodNotFound: return "RPC метод не найден";
        case ErrorCode::RpcInvalidParams: return "Некорректные параметры RPC";
        case ErrorCode::RpcInternalError: return "Внутренняя ошибка RPC";
        case ErrorCode::ShmOpenFailed: return "Ошибка открытия shared memory";
        case ErrorCode::ShmMapFailed: return "Ошибка маппинга shared memory";
        case ErrorCode::ShmInvalidState: return "Некорректное состояние shared memory";
        case ErrorCode::MiningInvalidJob: return "Некорректное задание майнинга";
        case ErrorCode::MiningInvalidNonce: return "Некорректный nonce";
        case ErrorCode::MiningStaleJob: return "Устаревшее задание";
        case ErrorCode::MiningBlockRejected: return "Блок отклонён";
        case ErrorCode::BitcoinInvalidAddress: return "Некорректный Bitcoin адрес";
        case ErrorCode::BitcoinInvalidBlock: return "Некорректный блок";
        case ErrorCode::BitcoinInvalidTransaction: return "Некорректная транзакция";
        case ErrorCode::BitcoinTargetNotMet: return "Хеш не соответствует target";
        case ErrorCode::CryptoHashError: return "Ошибка хеширования";
        case ErrorCode::CryptoInvalidLength: return "Некорректная длина данных";
        case ErrorCode::SystemOutOfMemory: return "Недостаточно памяти";
        case ErrorCode::SystemIOError: return "Ошибка ввода/вывода";
        default: return "Неизвестная ошибка";
    }
}

// =============================================================================
// Result тип (std::expected wrapper)
// =============================================================================

/**
 * @brief Ошибка с кодом и опциональным сообщением
 * 
 * Используется как error type в std::expected.
 */
struct Error {
    ErrorCode code;
    std::string message;
    
    /**
     * @brief Создать ошибку только с кодом
     */
    constexpr explicit Error(ErrorCode c) noexcept 
        : code(c), message(std::string(to_string(c))) {}
    
    /**
     * @brief Создать ошибку с кодом и сообщением
     */
    Error(ErrorCode c, std::string msg) noexcept 
        : code(c), message(std::move(msg)) {}
    
    /**
     * @brief Оператор сравнения
     */
    [[nodiscard]] bool operator==(const Error& other) const noexcept {
        return code == other.code;
    }
};

/**
 * @brief Результат операции: значение или ошибка
 * 
 * Обёртка над std::expected для единообразной обработки ошибок.
 * Используется вместо исключений в критическом пути.
 * 
 * @tparam T Тип возвращаемого значения
 * 
 * Пример использования:
 * @code
 * Result<int> parse_number(std::string_view str) {
 *     if (str.empty()) {
 *         return std::unexpected(Error{ErrorCode::ConfigInvalidValue});
 *     }
 *     return std::stoi(std::string(str));
 * }
 * 
 * auto result = parse_number("42");
 * if (result) {
 *     std::cout << *result << std::endl;
 * } else {
 *     std::cerr << result.error().message << std::endl;
 * }
 * @endcode
 */
template<typename T>
using Result = std::expected<T, Error>;

/**
 * @brief Создать успешный результат
 */
template<typename T>
[[nodiscard]] constexpr Result<T> Ok(T&& value) {
    return Result<T>(std::forward<T>(value));
}

/**
 * @brief Создать результат с ошибкой
 */
template<typename T>
[[nodiscard]] constexpr Result<T> Err(ErrorCode code) {
    return std::unexpected(Error{code});
}

/**
 * @brief Создать результат с ошибкой и сообщением
 */
template<typename T>
[[nodiscard]] Result<T> Err(ErrorCode code, std::string message) {
    return std::unexpected(Error{code, std::move(message)});
}

// =============================================================================
// Concepts для type constraints
// =============================================================================

/**
 * @brief Concept для типов, которые можно хешировать (SHA256)
 */
template<typename T>
concept Hashable = requires(T t) {
    { std::data(t) } -> std::convertible_to<const uint8_t*>;
    { std::size(t) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept для типов фиксированного размера
 */
template<typename T, std::size_t N>
concept FixedSize = (sizeof(T) == N);

/**
 * @brief Concept для байтовых контейнеров
 */
template<typename T>
concept ByteContainer = requires(T t) {
    { t.data() } -> std::convertible_to<const uint8_t*>;
    { t.size() } -> std::convertible_to<std::size_t>;
};

} // namespace quaxis
