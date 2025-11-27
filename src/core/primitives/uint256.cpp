/**
 * @file uint256.cpp
 * @brief Реализация 256-битного целого числа
 */

#include "uint256.hpp"

#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace quaxis::core {

namespace {

/**
 * @brief Преобразовать hex символ в число
 */
[[nodiscard]] inline uint8_t hex_char_to_int(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
    throw std::invalid_argument("Invalid hex character");
}

} // namespace

std::string uint256::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    // Big-endian (старшие байты сначала)
    for (std::size_t i = SIZE; i-- > 0;) {
        oss << std::setw(2) << static_cast<unsigned>(data_[i]);
    }
    return oss.str();
}

std::string uint256::to_hex_le() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    // Little-endian (младшие байты сначала)
    for (std::size_t i = 0; i < SIZE; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(data_[i]);
    }
    return oss.str();
}

uint256 uint256::from_hex(std::string_view hex) {
    if (hex.size() != SIZE * 2) {
        throw std::invalid_argument("Invalid hex string length");
    }
    
    uint256 result;
    for (std::size_t i = 0; i < SIZE; ++i) {
        std::size_t hex_idx = (SIZE - 1 - i) * 2;
        char high = hex[hex_idx];
        char low = hex[hex_idx + 1];
        result.data_[i] = static_cast<uint8_t>((hex_char_to_int(high) << 4) | hex_char_to_int(low));
    }
    
    return result;
}

uint256 uint256::from_hex_le(std::string_view hex) {
    if (hex.size() != SIZE * 2) {
        throw std::invalid_argument("Invalid hex string length");
    }
    
    uint256 result;
    for (std::size_t i = 0; i < SIZE; ++i) {
        std::size_t hex_idx = i * 2;
        char high = hex[hex_idx];
        char low = hex[hex_idx + 1];
        result.data_[i] = static_cast<uint8_t>((hex_char_to_int(high) << 4) | hex_char_to_int(low));
    }
    
    return result;
}

} // namespace quaxis::core
