/**
 * @file fibre_protocol.cpp
 * @brief Реализация парсера FIBRE протокола
 */

#include "fibre_protocol.hpp"
#include "../core/byte_order.hpp"

#include <cstring>
#include <format>
#include <sstream>

namespace quaxis::relay {

// =============================================================================
// Вспомогательные функции для чтения/записи
// =============================================================================

namespace {

/**
 * @brief Читать uint16_t в big-endian формате
 */
[[nodiscard]] uint16_t read_be16(const uint8_t* data) noexcept {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(data[0]) << 8) |
        static_cast<uint16_t>(data[1])
    );
}

/**
 * @brief Читать uint32_t в big-endian формате
 */
[[nodiscard]] uint32_t read_be32(const uint8_t* data) noexcept {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

/**
 * @brief Записать uint16_t в big-endian формате
 */
void write_be16(uint8_t* data, uint16_t value) noexcept {
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value);
}

/**
 * @brief Записать uint32_t в big-endian формате
 */
void write_be32(uint8_t* data, uint32_t value) noexcept {
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

} // anonymous namespace

// =============================================================================
// Реализация FibreParser
// =============================================================================

Result<FibrePacket> FibreParser::parse(ByteSpan data) const {
    // Парсим заголовок
    auto header_result = parse_header(data);
    if (!header_result) {
        return std::unexpected(header_result.error());
    }
    
    FibrePacket packet;
    packet.header = *header_result;
    
    // Извлекаем payload
    if (data.size() < FIBRE_HEADER_SIZE + packet.header.payload_size) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для payload"
        });
    }
    
    const uint8_t* payload_start = data.data() + FIBRE_HEADER_SIZE;
    packet.payload.assign(payload_start, payload_start + packet.header.payload_size);
    
    return packet;
}

Result<FibreHeader> FibreParser::parse_header(ByteSpan data) const {
    if (data.size() < FIBRE_HEADER_SIZE) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Пакет слишком короткий для FIBRE заголовка"
        });
    }
    
    const uint8_t* ptr = data.data();
    FibreHeader header;
    
    // Читаем поля
    header.magic = read_be32(ptr); ptr += 4;
    
    if (header.magic != FIBRE_MAGIC) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Некорректный magic number FIBRE"
        });
    }
    
    header.version = *ptr++;
    header.flags = *ptr++;
    header.chunk_id = read_be16(ptr); ptr += 2;
    header.block_height = read_be32(ptr); ptr += 4;
    
    // Block hash (32 байта)
    std::memcpy(header.block_hash.data(), ptr, 32);
    ptr += 32;
    
    header.total_chunks = read_be16(ptr); ptr += 2;
    header.data_chunks = read_be16(ptr); ptr += 2;
    header.payload_size = read_be16(ptr);
    
    // Валидация
    if (!header.is_valid()) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Некорректный заголовок FIBRE"
        });
    }
    
    return header;
}

std::vector<uint8_t> FibreParser::serialize(const FibrePacket& packet) const {
    std::vector<uint8_t> data(FIBRE_HEADER_SIZE + packet.payload.size());
    uint8_t* ptr = data.data();
    
    // Записываем заголовок
    write_be32(ptr, packet.header.magic); ptr += 4;
    *ptr++ = packet.header.version;
    *ptr++ = packet.header.flags;
    write_be16(ptr, packet.header.chunk_id); ptr += 2;
    write_be32(ptr, packet.header.block_height); ptr += 4;
    
    std::memcpy(ptr, packet.header.block_hash.data(), 32);
    ptr += 32;
    
    write_be16(ptr, packet.header.total_chunks); ptr += 2;
    write_be16(ptr, packet.header.data_chunks); ptr += 2;
    write_be16(ptr, packet.header.payload_size);
    ptr += 2;
    
    // Записываем payload
    if (!packet.payload.empty()) {
        std::memcpy(ptr, packet.payload.data(), packet.payload.size());
    }
    
    return data;
}

bool FibreParser::check_magic(ByteSpan data) noexcept {
    if (data.size() < 4) {
        return false;
    }
    return read_be32(data.data()) == FIBRE_MAGIC;
}

std::vector<uint8_t> FibreParser::create_keepalive() {
    std::vector<uint8_t> data(FIBRE_HEADER_SIZE);
    uint8_t* ptr = data.data();
    
    write_be32(ptr, FIBRE_MAGIC); ptr += 4;
    *ptr++ = FIBRE_VERSION;
    *ptr++ = static_cast<uint8_t>(FibreFlags::Keepalive);
    
    // Остальные поля нулевые
    std::memset(ptr, 0, FIBRE_HEADER_SIZE - 6);
    
    return data;
}

std::vector<uint8_t> FibreParser::create_ack(
    const Hash256& block_hash,
    uint16_t chunk_id
) {
    std::vector<uint8_t> data(FIBRE_HEADER_SIZE);
    uint8_t* ptr = data.data();
    
    write_be32(ptr, FIBRE_MAGIC); ptr += 4;
    *ptr++ = FIBRE_VERSION;
    *ptr++ = static_cast<uint8_t>(FibreFlags::Ack);
    write_be16(ptr, chunk_id); ptr += 2;
    write_be32(ptr, 0); ptr += 4;  // block_height не важен для ACK
    
    std::memcpy(ptr, block_hash.data(), 32);
    ptr += 32;
    
    // Остальные поля нулевые
    std::memset(ptr, 0, 6);
    
    return data;
}

// =============================================================================
// Вспомогательные функции
// =============================================================================

std::string flags_to_string(uint8_t flags) {
    if (flags == 0) {
        return "None";
    }
    
    std::ostringstream oss;
    bool first = true;
    
    auto add_flag = [&](FibreFlags f, const char* name) {
        if (has_flag(flags, f)) {
            if (!first) oss << "|";
            oss << name;
            first = false;
        }
    };
    
    add_flag(FibreFlags::FecChunk, "FEC");
    add_flag(FibreFlags::LastChunk, "Last");
    add_flag(FibreFlags::Retransmit, "Retransmit");
    add_flag(FibreFlags::Keepalive, "Keepalive");
    add_flag(FibreFlags::Ack, "Ack");
    
    return oss.str();
}

std::string header_to_string(const FibreHeader& header) {
    return std::format(
        "FibreHeader {{ magic=0x{:08X}, version={}, flags={}, chunk_id={}, "
        "height={}, total={}, data={}, payload={} }}",
        header.magic,
        header.version,
        flags_to_string(header.flags),
        header.chunk_id,
        header.block_height,
        header.total_chunks,
        header.data_chunks,
        header.payload_size
    );
}

} // namespace quaxis::relay
