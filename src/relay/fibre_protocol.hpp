/**
 * @file fibre_protocol.hpp
 * @brief Парсер FIBRE протокола для быстрого распространения блоков
 * 
 * FIBRE (Fast Internet Bitcoin Relay Engine) - протокол для сверхбыстрого
 * распространения блоков через UDP с использованием FEC.
 * 
 * Структура пакета:
 * - Magic number (4 байта): идентификатор протокола
 * - Version (1 байт): версия протокола
 * - Flags (1 байт): флаги пакета
 * - Chunk ID (2 байта): ID чанка в блоке
 * - Block height (4 байта): высота блока
 * - Block hash (32 байта): хеш блока
 * - Total chunks (2 байта): общее количество чанков
 * - Data chunks (2 байта): количество data чанков
 * - Payload size (2 байта): размер полезной нагрузки
 * - Payload (переменный): данные или FEC
 */

#pragma once

#include "../core/types.hpp"
#include "fec_decoder.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace quaxis::relay {

// =============================================================================
// Константы FIBRE протокола
// =============================================================================

/// @brief Magic number FIBRE протокола
inline constexpr uint32_t FIBRE_MAGIC = 0xF1B3E001;

/// @brief Текущая версия протокола
inline constexpr uint8_t FIBRE_VERSION = 1;

/// @brief Минимальный размер заголовка пакета
inline constexpr std::size_t FIBRE_HEADER_SIZE = 50;

/// @brief Максимальный размер payload
inline constexpr std::size_t FIBRE_MAX_PAYLOAD_SIZE = 1450;

// =============================================================================
// Флаги пакета
// =============================================================================

/**
 * @brief Флаги FIBRE пакета
 */
enum class FibreFlags : uint8_t {
    None = 0x00,
    
    /// @brief Это FEC (parity) чанк
    FecChunk = 0x01,
    
    /// @brief Это последний чанк блока
    LastChunk = 0x02,
    
    /// @brief Запрос повторной отправки
    Retransmit = 0x04,
    
    /// @brief Keepalive пакет
    Keepalive = 0x08,
    
    /// @brief Пакет-подтверждение
    Ack = 0x10,
};

/**
 * @brief Проверить флаг
 */
[[nodiscard]] constexpr bool has_flag(uint8_t flags, FibreFlags flag) noexcept {
    return (flags & static_cast<uint8_t>(flag)) != 0;
}

// =============================================================================
// Структура FIBRE пакета
// =============================================================================

/**
 * @brief Заголовок FIBRE пакета
 */
struct FibreHeader {
    /// @brief Magic number (должен быть FIBRE_MAGIC)
    uint32_t magic{0};
    
    /// @brief Версия протокола
    uint8_t version{0};
    
    /// @brief Флаги пакета
    uint8_t flags{0};
    
    /// @brief ID чанка (0..total_chunks-1)
    uint16_t chunk_id{0};
    
    /// @brief Высота блока
    uint32_t block_height{0};
    
    /// @brief Хеш блока
    Hash256 block_hash{};
    
    /// @brief Общее количество чанков (data + FEC)
    uint16_t total_chunks{0};
    
    /// @brief Количество data чанков
    uint16_t data_chunks{0};
    
    /// @brief Размер payload
    uint16_t payload_size{0};
    
    /**
     * @brief Это FEC чанк?
     */
    [[nodiscard]] bool is_fec() const noexcept {
        return has_flag(flags, FibreFlags::FecChunk);
    }
    
    /**
     * @brief Это последний чанк?
     */
    [[nodiscard]] bool is_last() const noexcept {
        return has_flag(flags, FibreFlags::LastChunk);
    }
    
    /**
     * @brief Это keepalive?
     */
    [[nodiscard]] bool is_keepalive() const noexcept {
        return has_flag(flags, FibreFlags::Keepalive);
    }
    
    /**
     * @brief Количество FEC чанков
     */
    [[nodiscard]] uint16_t fec_chunks() const noexcept {
        return total_chunks > data_chunks ? total_chunks - data_chunks : 0;
    }
    
    /**
     * @brief Валидный заголовок?
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return magic == FIBRE_MAGIC &&
               version >= 1 &&
               payload_size <= FIBRE_MAX_PAYLOAD_SIZE &&
               chunk_id < total_chunks &&
               data_chunks <= total_chunks;
    }
};

/**
 * @brief Полный FIBRE пакет
 */
struct FibrePacket {
    /// @brief Заголовок
    FibreHeader header;
    
    /// @brief Payload (данные или FEC)
    std::vector<uint8_t> payload;
    
    /**
     * @brief Пакет валиден?
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return header.is_valid() && payload.size() == header.payload_size;
    }
    
    /**
     * @brief Это data чанк?
     */
    [[nodiscard]] bool is_data_chunk() const noexcept {
        return !header.is_fec();
    }
    
    /**
     * @brief Это FEC чанк?
     */
    [[nodiscard]] bool is_fec_chunk() const noexcept {
        return header.is_fec();
    }
    
    /**
     * @brief Преобразовать в FEC чанк
     */
    [[nodiscard]] FecChunk to_fec_chunk() const {
        FecChunk chunk;
        chunk.chunk_id = header.is_fec() 
            ? static_cast<uint16_t>(header.chunk_id - header.data_chunks)
            : header.chunk_id;
        chunk.is_fec = header.is_fec();
        chunk.data = payload;
        return chunk;
    }
};

// =============================================================================
// Класс парсера FIBRE протокола
// =============================================================================

/**
 * @brief Парсер FIBRE протокола
 * 
 * Парсит UDP пакеты FIBRE протокола и извлекает данные блоков.
 */
class FibreParser {
public:
    /**
     * @brief Создать парсер
     */
    FibreParser() = default;
    
    /**
     * @brief Парсить пакет
     * 
     * @param data Сырые данные UDP пакета
     * @return Распарсенный пакет или ошибка
     */
    [[nodiscard]] Result<FibrePacket> parse(ByteSpan data) const;
    
    /**
     * @brief Парсить только заголовок
     * 
     * @param data Данные пакета
     * @return Заголовок или ошибка
     */
    [[nodiscard]] Result<FibreHeader> parse_header(ByteSpan data) const;
    
    /**
     * @brief Сериализовать пакет
     * 
     * @param packet Пакет для сериализации
     * @return Сериализованные данные
     */
    [[nodiscard]] std::vector<uint8_t> serialize(const FibrePacket& packet) const;
    
    /**
     * @brief Проверить magic number
     * 
     * @param data Данные пакета
     * @return true если magic корректный
     */
    [[nodiscard]] static bool check_magic(ByteSpan data) noexcept;
    
    /**
     * @brief Создать keepalive пакет
     * 
     * @return Keepalive пакет
     */
    [[nodiscard]] static std::vector<uint8_t> create_keepalive();
    
    /**
     * @brief Создать ACK пакет
     * 
     * @param block_hash Хеш блока
     * @param chunk_id ID подтверждённого чанка
     * @return ACK пакет
     */
    [[nodiscard]] static std::vector<uint8_t> create_ack(
        const Hash256& block_hash,
        uint16_t chunk_id
    );
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Получить строковое описание флагов
 */
[[nodiscard]] std::string flags_to_string(uint8_t flags);

/**
 * @brief Вывести заголовок в строку для отладки
 */
[[nodiscard]] std::string header_to_string(const FibreHeader& header);

} // namespace quaxis::relay
