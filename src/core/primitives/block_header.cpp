/**
 * @file block_header.cpp
 * @brief Реализация заголовка блока
 */

#include "block_header.hpp"
#include "../../crypto/sha256.hpp"

#include <cstring>
#include <cmath>

namespace quaxis::core {

namespace {

// Вспомогательные функции для работы с little-endian

inline void write_le32(uint8_t* dest, uint32_t value) noexcept {
    dest[0] = static_cast<uint8_t>(value);
    dest[1] = static_cast<uint8_t>(value >> 8);
    dest[2] = static_cast<uint8_t>(value >> 16);
    dest[3] = static_cast<uint8_t>(value >> 24);
}

inline void write_le32_signed(uint8_t* dest, int32_t value) noexcept {
    write_le32(dest, static_cast<uint32_t>(value));
}

inline uint32_t read_le32(const uint8_t* src) noexcept {
    return static_cast<uint32_t>(src[0]) |
           (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) |
           (static_cast<uint32_t>(src[3]) << 24);
}

inline int32_t read_le32_signed(const uint8_t* src) noexcept {
    return static_cast<int32_t>(read_le32(src));
}

} // namespace

std::array<uint8_t, BLOCK_HEADER_SIZE> BlockHeader::serialize() const noexcept {
    std::array<uint8_t, BLOCK_HEADER_SIZE> result{};
    
    // version (4 байта)
    write_le32_signed(result.data(), version);
    
    // prev_hash (32 байта)
    std::memcpy(result.data() + 4, prev_hash.data(), 32);
    
    // merkle_root (32 байта)
    std::memcpy(result.data() + 36, merkle_root.data(), 32);
    
    // timestamp (4 байта)
    write_le32(result.data() + 68, timestamp);
    
    // bits (4 байта)
    write_le32(result.data() + 72, bits);
    
    // nonce (4 байта)
    write_le32(result.data() + 76, nonce);
    
    return result;
}

BlockHeader BlockHeader::deserialize(const uint8_t* data) noexcept {
    BlockHeader header;
    
    // version (4 байта)
    header.version = read_le32_signed(data);
    
    // prev_hash (32 байта)
    std::memcpy(header.prev_hash.data(), data + 4, 32);
    
    // merkle_root (32 байта)
    std::memcpy(header.merkle_root.data(), data + 36, 32);
    
    // timestamp (4 байта)
    header.timestamp = read_le32(data + 68);
    
    // bits (4 байта)
    header.bits = read_le32(data + 72);
    
    // nonce (4 байта)
    header.nonce = read_le32(data + 76);
    
    return header;
}

BlockHeader BlockHeader::deserialize(std::span<const uint8_t> data) noexcept {
    return deserialize(data.data());
}

Hash256 BlockHeader::hash() const noexcept {
    auto serialized = serialize();
    return quaxis::crypto::sha256d(serialized);
}

uint256 BlockHeader::hash_uint256() const noexcept {
    return uint256{hash()};
}

uint256 BlockHeader::get_target() const noexcept {
    return bits_to_target(bits);
}

bool BlockHeader::check_pow() const noexcept {
    auto block_hash = hash_uint256();
    auto target = get_target();
    return block_hash <= target;
}

double BlockHeader::get_difficulty() const noexcept {
    return bits_to_difficulty(bits);
}

bool BlockHeader::is_auxpow() const noexcept {
    // AuxPoW блоки имеют специальный флаг в версии
    // Обычно это 0x100 (256) в старших битах
    return (version & 0x100) != 0;
}

uint32_t BlockHeader::get_chain_id() const noexcept {
    if (!is_auxpow()) {
        return 0;
    }
    // Chain ID находится в битах 16-22 версии
    return static_cast<uint32_t>((version >> 16) & 0x7F);
}

std::array<uint8_t, 64> BlockHeader::get_first_chunk() const noexcept {
    auto serialized = serialize();
    std::array<uint8_t, 64> result{};
    std::memcpy(result.data(), serialized.data(), 64);
    return result;
}

std::array<uint8_t, 16> BlockHeader::get_tail() const noexcept {
    auto serialized = serialize();
    std::array<uint8_t, 16> result{};
    std::memcpy(result.data(), serialized.data() + 64, 16);
    return result;
}

uint256 bits_to_target(uint32_t bits) noexcept {
    uint256 target;
    
    // Извлекаем экспоненту и мантиссу
    uint32_t exponent = bits >> 24;
    uint32_t mantissa = bits & 0x007FFFFF;
    
    // Проверяем отрицательный флаг (не должен быть установлен)
    if (bits & 0x00800000) {
        return uint256::zero();
    }
    
    // Вычисляем позицию мантиссы
    if (exponent <= 3) {
        mantissa >>= 8 * (3 - exponent);
        target.data()[0] = static_cast<uint8_t>(mantissa);
        target.data()[1] = static_cast<uint8_t>(mantissa >> 8);
        target.data()[2] = static_cast<uint8_t>(mantissa >> 16);
    } else {
        std::size_t shift = exponent - 3;
        if (shift < 32 - 3) {
            target.data()[shift] = static_cast<uint8_t>(mantissa);
            target.data()[shift + 1] = static_cast<uint8_t>(mantissa >> 8);
            target.data()[shift + 2] = static_cast<uint8_t>(mantissa >> 16);
        }
    }
    
    return target;
}

uint32_t target_to_bits(const uint256& target) noexcept {
    // Находим старший ненулевой байт
    int i = 31;
    while (i > 0 && target[i] == 0) {
        --i;
    }
    
    uint32_t mantissa;
    uint32_t exponent;
    
    if (i >= 2) {
        mantissa = (static_cast<uint32_t>(target[i]) << 16) |
                   (static_cast<uint32_t>(target[i - 1]) << 8) |
                   static_cast<uint32_t>(target[i - 2]);
        exponent = static_cast<uint32_t>(i + 1);
    } else if (i == 1) {
        mantissa = (static_cast<uint32_t>(target[1]) << 16) |
                   (static_cast<uint32_t>(target[0]) << 8);
        exponent = 2;
    } else {
        mantissa = static_cast<uint32_t>(target[0]) << 16;
        exponent = 1;
    }
    
    // Если старший бит мантиссы установлен, сдвигаем
    if (mantissa & 0x00800000) {
        mantissa >>= 8;
        exponent++;
    }
    
    return (exponent << 24) | mantissa;
}

double bits_to_difficulty(uint32_t bits) noexcept {
    // Difficulty 1 target (Bitcoin genesis)
    constexpr uint32_t DIFF1_BITS = 0x1d00ffff;
    
    auto target = bits_to_target(bits);
    auto diff1_target = bits_to_target(DIFF1_BITS);
    
    // Вычисляем difficulty = diff1_target / target
    // Упрощённая версия для небольших чисел
    
    // Находим старший байт каждого target
    int target_msb = 31;
    while (target_msb > 0 && target[target_msb] == 0) --target_msb;
    
    int diff1_msb = 31;
    while (diff1_msb > 0 && diff1_target[diff1_msb] == 0) --diff1_msb;
    
    // Грубое приближение через double
    double target_approx = 0;
    for (int j = target_msb; j >= 0 && j > target_msb - 8; --j) {
        target_approx = target_approx * 256.0 + static_cast<double>(target[j]);
    }
    target_approx *= std::pow(256.0, target_msb - 7);
    
    double diff1_approx = 0;
    for (int j = diff1_msb; j >= 0 && j > diff1_msb - 8; --j) {
        diff1_approx = diff1_approx * 256.0 + static_cast<double>(diff1_target[j]);
    }
    diff1_approx *= std::pow(256.0, diff1_msb - 7);
    
    if (target_approx == 0) return 0;
    
    return diff1_approx / target_approx;
}

} // namespace quaxis::core
