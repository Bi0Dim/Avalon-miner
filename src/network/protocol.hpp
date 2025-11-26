/**
 * @file protocol.hpp
 * @brief Бинарный протокол связи с ASIC
 * 
 * Протокол оптимизирован для минимального размера и латентности:
 * 
 * Задание (сервер -> ASIC): 48 байт
 * ├─ midstate[32]     : SHA256 состояние после первых 64 байт header
 * ├─ header_tail[12]  : timestamp(4) + bits(4) + nonce_template(4)
 * └─ job_id[4]        : уникальный ID задания
 * 
 * Share (ASIC -> сервер): 8 байт
 * ├─ job_id[4]        : ID задания
 * └─ nonce[4]         : найденный nonce
 * 
 * Команды (сервер -> ASIC): 1 байт + payload
 * ├─ CMD_NEW_JOB (0x01)     : новое задание (48 байт)
 * ├─ CMD_STOP (0x02)        : остановить майнинг (0 байт)
 * ├─ CMD_HEARTBEAT (0x03)   : ping (0 байт)
 * └─ CMD_SET_TARGET (0x04)  : установить target (32 байта)
 * 
 * Ответы (ASIC -> сервер): 1 байт + payload
 * ├─ RSP_SHARE (0x81)       : найден nonce (8 байт)
 * ├─ RSP_HEARTBEAT (0x83)   : pong (0 байт)
 * └─ RSP_STATUS (0x84)      : статус ASIC (переменная длина)
 */

#pragma once

#include "../core/types.hpp"
#include "../core/constants.hpp"
#include "../mining/job.hpp"

#include <span>
#include <variant>

namespace quaxis::network {

// =============================================================================
// Коды команд и ответов
// =============================================================================

/**
 * @brief Команды от сервера к ASIC
 */
enum class Command : uint8_t {
    NewJob = 0x01,        ///< Новое задание для майнинга
    Stop = 0x02,          ///< Остановить майнинг
    Heartbeat = 0x03,     ///< Ping для проверки соединения
    SetTarget = 0x04,     ///< Установить target
    SetDifficulty = 0x05, ///< Установить difficulty
};

/**
 * @brief Ответы от ASIC к серверу
 */
enum class Response : uint8_t {
    Share = 0x81,         ///< Найден валидный nonce
    Heartbeat = 0x83,     ///< Pong ответ
    Status = 0x84,        ///< Статус ASIC
    Error = 0x8F,         ///< Ошибка
};

// =============================================================================
// Структуры сообщений
// =============================================================================

/**
 * @brief Сообщение NewJob
 */
struct NewJobMessage {
    mining::Job job;
    
    [[nodiscard]] Bytes serialize() const;
    [[nodiscard]] static Result<NewJobMessage> deserialize(ByteSpan data);
};

/**
 * @brief Сообщение Share
 */
struct ShareMessage {
    mining::Share share;
    
    [[nodiscard]] Bytes serialize() const;
    [[nodiscard]] static Result<ShareMessage> deserialize(ByteSpan data);
};

/**
 * @brief Сообщение SetTarget
 */
struct SetTargetMessage {
    Hash256 target;
    
    [[nodiscard]] Bytes serialize() const;
    [[nodiscard]] static Result<SetTargetMessage> deserialize(ByteSpan data);
};

/**
 * @brief Сообщение Status от ASIC
 */
struct StatusMessage {
    uint32_t hashrate;     ///< Текущий хешрейт (H/s)
    uint8_t temperature;   ///< Температура чипа (°C)
    uint8_t fan_speed;     ///< Скорость вентилятора (%)
    uint16_t errors;       ///< Количество ошибок
    
    [[nodiscard]] Bytes serialize() const;
    [[nodiscard]] static Result<StatusMessage> deserialize(ByteSpan data);
};

/**
 * @brief Сообщение об ошибке
 */
struct ErrorMessage {
    uint8_t error_code;
    std::string message;
    
    [[nodiscard]] Bytes serialize() const;
    [[nodiscard]] static Result<ErrorMessage> deserialize(ByteSpan data);
};

// =============================================================================
// Парсер протокола
// =============================================================================

/**
 * @brief Результат парсинга входящего сообщения
 */
using ParsedMessage = std::variant<
    ShareMessage,
    StatusMessage,
    ErrorMessage
>;

/**
 * @brief Парсер бинарного протокола
 */
class ProtocolParser {
public:
    /**
     * @brief Добавить данные в буфер
     * 
     * @param data Входящие байты
     */
    void add_data(ByteSpan data);
    
    /**
     * @brief Попытаться извлечь полное сообщение
     * 
     * @return std::optional<ParsedMessage> Сообщение или nullopt
     */
    [[nodiscard]] std::optional<ParsedMessage> try_parse();
    
    /**
     * @brief Количество байт в буфере
     */
    [[nodiscard]] std::size_t buffered_size() const;
    
    /**
     * @brief Очистить буфер
     */
    void clear();
    
private:
    Bytes buffer_;
};

// =============================================================================
// Сериализация команд
// =============================================================================

/**
 * @brief Сериализовать команду NewJob
 */
[[nodiscard]] Bytes serialize_new_job(const mining::Job& job);

/**
 * @brief Сериализовать команду Stop
 */
[[nodiscard]] Bytes serialize_stop();

/**
 * @brief Сериализовать команду Heartbeat
 */
[[nodiscard]] Bytes serialize_heartbeat();

/**
 * @brief Сериализовать команду SetTarget
 */
[[nodiscard]] Bytes serialize_set_target(const Hash256& target);

} // namespace quaxis::network
