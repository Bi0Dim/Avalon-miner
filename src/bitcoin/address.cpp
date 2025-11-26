/**
 * @file address.cpp
 * @brief Реализация парсинга Bitcoin адресов (bech32)
 * 
 * Bech32 спецификация: BIP-173, BIP-350
 * 
 * Алгоритм декодирования:
 * 1. Проверяем HRP (human-readable part): bc, tb, bcrt
 * 2. Декодируем base32 данные
 * 3. Проверяем контрольную сумму
 * 4. Извлекаем witness version и program
 */

#include "address.hpp"

#include <array>
#include <algorithm>
#include <format>

namespace quaxis::bitcoin {

// =============================================================================
// Bech32 константы
// =============================================================================

/// @brief Алфавит Bech32
static constexpr std::string_view BECH32_CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

/// @brief Генератор полинома для bech32
static constexpr std::array<uint32_t, 5> BECH32_GEN = {
    0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

namespace {

/**
 * @brief Вычисление полиномиальной контрольной суммы
 */
uint32_t polymod(const std::vector<uint8_t>& values) noexcept {
    uint32_t chk = 1;
    for (auto v : values) {
        uint32_t top = chk >> 25;
        chk = (chk & 0x1ffffff) << 5 ^ v;
        for (int i = 0; i < 5; ++i) {
            if ((top >> i) & 1) {
                chk ^= BECH32_GEN[static_cast<std::size_t>(i)];
            }
        }
    }
    return chk;
}

/**
 * @brief Расширение HRP для контрольной суммы
 */
std::vector<uint8_t> hrp_expand(std::string_view hrp) {
    std::vector<uint8_t> result;
    result.reserve(hrp.size() * 2 + 1);
    
    // Высокие биты каждого символа
    for (char c : hrp) {
        result.push_back(static_cast<uint8_t>(c >> 5));
    }
    
    // Разделитель
    result.push_back(0);
    
    // Низкие биты каждого символа
    for (char c : hrp) {
        result.push_back(static_cast<uint8_t>(c & 0x1f));
    }
    
    return result;
}

/**
 * @brief Проверка контрольной суммы bech32
 */
bool verify_checksum(std::string_view hrp, const std::vector<uint8_t>& data) {
    auto expanded = hrp_expand(hrp);
    expanded.insert(expanded.end(), data.begin(), data.end());
    // Для bech32 (не bech32m) константа = 1
    return polymod(expanded) == 1;
}

/**
 * @brief Конвертация из 5-битных групп в 8-битные
 */
bool convert_bits(
    std::vector<uint8_t>& out,
    const std::vector<uint8_t>& in,
    int from_bits,
    int to_bits,
    bool pad
) {
    int acc = 0;
    int bits = 0;
    int max_v = (1 << to_bits) - 1;
    
    for (auto value : in) {
        if (value >> from_bits) {
            return false;
        }
        acc = (acc << from_bits) | value;
        bits += from_bits;
        while (bits >= to_bits) {
            bits -= to_bits;
            out.push_back(static_cast<uint8_t>((acc >> bits) & max_v));
        }
    }
    
    if (pad) {
        if (bits > 0) {
            out.push_back(static_cast<uint8_t>((acc << (to_bits - bits)) & max_v));
        }
    } else if (bits >= from_bits || ((acc << (to_bits - bits)) & max_v)) {
        return false;
    }
    
    return true;
}

/**
 * @brief Декодирование символа bech32
 */
int decode_char(char c) {
    auto pos = BECH32_CHARSET.find(c);
    if (pos == std::string_view::npos) {
        return -1;
    }
    return static_cast<int>(pos);
}

} // anonymous namespace

// =============================================================================
// Публичные функции
// =============================================================================

Result<Hash160> parse_p2wpkh_address(std::string_view address) {
    // Минимальная длина: "bc1q" + 32 символа + 6 символов checksum = 42
    // Для regtest "bcrt1q" длина будет 44
    if (address.size() < 42) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            "Адрес слишком короткий"
        );
    }
    
    // Конвертируем в нижний регистр
    std::string addr_lower;
    addr_lower.reserve(address.size());
    bool has_upper = false;
    bool has_lower = false;
    
    for (char c : address) {
        if (c >= 'A' && c <= 'Z') {
            has_upper = true;
            addr_lower.push_back(static_cast<char>(c - 'A' + 'a'));
        } else if (c >= 'a' && c <= 'z') {
            has_lower = true;
            addr_lower.push_back(c);
        } else {
            addr_lower.push_back(c);
        }
    }
    
    // Проверка на смешанный регистр
    if (has_upper && has_lower) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            "Смешанный регистр в адресе"
        );
    }
    
    // Находим разделитель '1'
    auto sep_pos = addr_lower.rfind('1');
    if (sep_pos == std::string::npos || sep_pos < 1 || sep_pos + 7 > addr_lower.size()) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            "Неверный формат bech32 адреса"
        );
    }
    
    // Извлекаем HRP и данные
    std::string_view hrp = std::string_view(addr_lower).substr(0, sep_pos);
    std::string_view data_str = std::string_view(addr_lower).substr(sep_pos + 1);
    
    // Проверяем HRP
    if (hrp != "bc" && hrp != "tb" && hrp != "bcrt") {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            std::format("Неизвестный HRP: {}", hrp)
        );
    }
    
    // Декодируем данные
    std::vector<uint8_t> data;
    data.reserve(data_str.size());
    
    for (char c : data_str) {
        int val = decode_char(c);
        if (val == -1) {
            return Err<Hash160>(
                ErrorCode::BitcoinInvalidAddress,
                std::format("Неверный символ в адресе: '{}'", c)
            );
        }
        data.push_back(static_cast<uint8_t>(val));
    }
    
    // Проверяем контрольную сумму
    if (!verify_checksum(hrp, data)) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            "Неверная контрольная сумма"
        );
    }
    
    // Убираем контрольную сумму (6 символов)
    data.resize(data.size() - 6);
    
    if (data.empty()) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            "Пустые данные"
        );
    }
    
    // Первый байт - witness version
    uint8_t witness_version = data[0];
    if (witness_version != 0) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            std::format("Неподдерживаемая witness версия: {}", witness_version)
        );
    }
    
    // Конвертируем из 5-битных в 8-битные
    std::vector<uint8_t> program;
    if (!convert_bits(program, std::vector<uint8_t>(data.begin() + 1, data.end()), 5, 8, false)) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            "Ошибка конвертации битов"
        );
    }
    
    // Проверяем длину программы (должна быть 20 байт для P2WPKH)
    if (program.size() != 20) {
        return Err<Hash160>(
            ErrorCode::BitcoinInvalidAddress,
            std::format("Неверная длина witness программы: {} (ожидается 20)", program.size())
        );
    }
    
    Hash160 result;
    std::copy(program.begin(), program.end(), result.begin());
    
    return result;
}

std::string create_p2wpkh_address(const Hash160& pubkey_hash, bool testnet) {
    std::string_view hrp = testnet ? "tb" : "bc";
    
    // Конвертируем 8-битные байты в 5-битные
    std::vector<uint8_t> data;
    data.push_back(0);  // witness version 0
    
    std::vector<uint8_t> hash_vec(pubkey_hash.begin(), pubkey_hash.end());
    convert_bits(data, hash_vec, 8, 5, true);
    
    // Вычисляем контрольную сумму
    auto expanded = hrp_expand(hrp);
    expanded.insert(expanded.end(), data.begin(), data.end());
    expanded.insert(expanded.end(), {0, 0, 0, 0, 0, 0});
    
    uint32_t poly = polymod(expanded) ^ 1;
    for (int i = 0; i < 6; ++i) {
        data.push_back(static_cast<uint8_t>((poly >> (5 * (5 - i))) & 0x1f));
    }
    
    // Формируем адрес
    std::string result;
    result.reserve(hrp.size() + 1 + data.size());
    result += hrp;
    result += '1';
    
    for (auto d : data) {
        result += BECH32_CHARSET[d];
    }
    
    return result;
}

bool is_valid_address(std::string_view address) {
    auto result = parse_p2wpkh_address(address);
    return result.has_value();
}

std::string_view get_network_from_address(std::string_view address) {
    std::string addr_lower;
    for (char c : address) {
        if (c >= 'A' && c <= 'Z') {
            addr_lower.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            addr_lower.push_back(c);
        }
    }
    
    if (addr_lower.starts_with("bc1")) {
        return "mainnet";
    } else if (addr_lower.starts_with("tb1")) {
        return "testnet";
    } else if (addr_lower.starts_with("bcrt1")) {
        return "regtest";
    }
    
    return "unknown";
}

} // namespace quaxis::bitcoin
