/**
 * @file target.cpp
 * @brief Реализация работы с difficulty и target
 */

#include "target.hpp"
#include "../core/byte_order.hpp"

#include <format>
#include <cmath>
#include <algorithm>

namespace quaxis::bitcoin {

// =============================================================================
// Константы
// =============================================================================

// Максимальный target (genesis block difficulty 1)
// В compact формате: 0x1d00ffff
// В полном формате: 0x00000000FFFF0000000000000000000000000000000000000000000000000000
static constexpr uint32_t MAX_TARGET_BITS = 0x1d00ffff;

// =============================================================================
// bits <-> target
// =============================================================================

Hash256 bits_to_target(uint32_t bits) noexcept {
    Hash256 target{};
    
    // Извлекаем exponent и mantissa
    uint32_t exponent = (bits >> 24) & 0xFF;
    uint32_t mantissa = bits & 0x00FFFFFF;
    
    // Проверка на отрицательный target (бит 0x00800000)
    if (mantissa & 0x00800000) {
        return target;  // Возвращаем нулевой target
    }
    
    // Позиция в массиве target (от конца)
    // exponent указывает на количество байт от младшего разряда
    if (exponent <= 3) {
        // Mantissa помещается в первые байты
        mantissa >>= 8 * (3 - exponent);
        target[0] = static_cast<uint8_t>(mantissa & 0xFF);
        target[1] = static_cast<uint8_t>((mantissa >> 8) & 0xFF);
        target[2] = static_cast<uint8_t>((mantissa >> 16) & 0xFF);
    } else {
        // Позиция старшего байта мантиссы
        std::size_t pos = static_cast<std::size_t>(exponent - 3);
        
        if (pos < 32) {
            target[pos] = static_cast<uint8_t>((mantissa >> 16) & 0xFF);
        }
        if (pos > 0 && pos - 1 < 32) {
            target[pos - 1] = static_cast<uint8_t>((mantissa >> 8) & 0xFF);
        }
        if (pos > 1 && pos - 2 < 32) {
            target[pos - 2] = static_cast<uint8_t>(mantissa & 0xFF);
        }
    }
    
    return target;
}

uint32_t target_to_bits(const Hash256& target) noexcept {
    // Находим первый ненулевой байт (от старшего)
    int first_nonzero = 31;
    while (first_nonzero >= 0 && target[static_cast<std::size_t>(first_nonzero)] == 0) {
        --first_nonzero;
    }
    
    if (first_nonzero < 0) {
        return 0;  // Нулевой target
    }
    
    // Извлекаем mantissa (3 старших значащих байта)
    uint32_t mantissa = 0;
    uint32_t exponent = static_cast<uint32_t>(first_nonzero + 1);
    
    mantissa = static_cast<uint32_t>(target[static_cast<std::size_t>(first_nonzero)]) << 16;
    if (first_nonzero > 0) {
        mantissa |= static_cast<uint32_t>(target[static_cast<std::size_t>(first_nonzero - 1)]) << 8;
    }
    if (first_nonzero > 1) {
        mantissa |= static_cast<uint32_t>(target[static_cast<std::size_t>(first_nonzero - 2)]);
    }
    
    // Нормализация: если mantissa >= 0x800000, сдвигаем
    if (mantissa & 0x00800000) {
        mantissa >>= 8;
        exponent++;
    }
    
    return (exponent << 24) | mantissa;
}

// =============================================================================
// Difficulty
// =============================================================================

double bits_to_difficulty(uint32_t bits) noexcept {
    // Difficulty = max_target / current_target
    // 
    // Для упрощения вычислений используем формулу:
    // max_target = 0xFFFF * 2^208
    // current_target = mantissa * 2^(8 * (exponent - 3))
    // 
    // difficulty = (0xFFFF * 2^208) / (mantissa * 2^(8 * (exponent - 3)))
    //            = 0xFFFF / mantissa * 2^(208 - 8 * (exponent - 3))
    //            = 0xFFFF / mantissa * 2^(208 - 8*exponent + 24)
    //            = 0xFFFF / mantissa * 2^(232 - 8*exponent)
    
    uint32_t exponent = (bits >> 24) & 0xFF;
    uint32_t mantissa = bits & 0x00FFFFFF;
    
    if (mantissa == 0) {
        return 0.0;
    }
    
    // Базовая difficulty
    double diff = static_cast<double>(0xFFFF) / static_cast<double>(mantissa);
    
    // Применяем степень двойки
    int shift = 232 - static_cast<int>(exponent) * 8;
    
    if (shift > 0) {
        diff = std::ldexp(diff, shift);
    } else if (shift < 0) {
        diff = std::ldexp(diff, shift);
    }
    
    return diff;
}

double target_to_difficulty(const Hash256& target) noexcept {
    uint32_t bits = target_to_bits(target);
    return bits_to_difficulty(bits);
}

// =============================================================================
// Проверки
// =============================================================================

bool meets_target(const Hash256& hash, const Hash256& target) noexcept {
    // Сравниваем от старших байт к младшим
    // hash должен быть <= target
    for (int i = 31; i >= 0; --i) {
        auto idx = static_cast<std::size_t>(i);
        if (hash[idx] < target[idx]) {
            return true;  // hash < target
        }
        if (hash[idx] > target[idx]) {
            return false; // hash > target
        }
    }
    return true;  // hash == target
}

bool meets_bits(const Hash256& hash, uint32_t bits) noexcept {
    Hash256 target = bits_to_target(bits);
    return meets_target(hash, target);
}

// =============================================================================
// Форматирование
// =============================================================================

std::string format_difficulty(double difficulty) {
    const char* suffixes[] = {"", "K", "M", "G", "T", "P", "E", "Z", "Y"};
    int suffix_idx = 0;
    
    while (difficulty >= 1000.0 && suffix_idx < 8) {
        difficulty /= 1000.0;
        ++suffix_idx;
    }
    
    return std::format("{:.2f} {}", difficulty, suffixes[suffix_idx]);
}

std::string target_to_hex(const Hash256& target) {
    std::string hex;
    hex.reserve(64);
    
    // От старших байт к младшим (обратный порядок для отображения)
    for (int i = 31; i >= 0; --i) {
        hex += std::format("{:02x}", target[static_cast<std::size_t>(i)]);
    }
    
    return hex;
}

Result<Hash256> hex_to_target(std::string_view hex) {
    if (hex.size() != 64) {
        return Err<Hash256>(
            ErrorCode::CryptoInvalidLength,
            std::format("Неверная длина hex строки: {} (ожидается 64)", hex.size())
        );
    }
    
    Hash256 target{};
    
    for (std::size_t i = 0; i < 32; ++i) {
        std::string_view byte_hex = hex.substr((31 - i) * 2, 2);
        
        unsigned int value = 0;
        for (char c : byte_hex) {
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<unsigned int>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<unsigned int>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<unsigned int>(c - 'A' + 10);
            } else {
                return Err<Hash256>(
                    ErrorCode::ConfigParseError,
                    std::format("Неверный символ в hex строке: '{}'", c)
                );
            }
        }
        
        target[i] = static_cast<uint8_t>(value);
    }
    
    return target;
}

} // namespace quaxis::bitcoin
