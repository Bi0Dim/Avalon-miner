/**
 * @file address.hpp
 * @brief Парсинг Bitcoin адресов
 * 
 * Поддерживает:
 * - P2WPKH (bech32): bc1q... / tb1q... / bcrt1q...
 * 
 * Для соло-майнинга используем только P2WPKH (native SegWit),
 * так как это самый эффективный формат для выплаты награды.
 */

#pragma once

#include "../core/types.hpp"

#include <string_view>

namespace quaxis::bitcoin {

/**
 * @brief Парсинг P2WPKH адреса (bech32)
 * 
 * Формат: bc1q + 32 символа base32 = 42 символа (mainnet)
 *         tb1q + 32 символа base32 = 42 символа (testnet)
 *         bcrt1q + ... (regtest)
 * 
 * @param address Bitcoin адрес в формате bc1q...
 * @return Result<Hash160> 20-байтный pubkey hash или ошибка
 */
[[nodiscard]] Result<Hash160> parse_p2wpkh_address(std::string_view address);

/**
 * @brief Создать P2WPKH адрес из pubkey hash
 * 
 * @param pubkey_hash 20-байтный хеш публичного ключа
 * @param testnet Использовать testnet префикс (tb1)
 * @return std::string Bitcoin адрес
 */
[[nodiscard]] std::string create_p2wpkh_address(
    const Hash160& pubkey_hash,
    bool testnet = false
);

/**
 * @brief Проверить валидность адреса
 * 
 * @param address Bitcoin адрес
 * @return true если адрес валиден
 */
[[nodiscard]] bool is_valid_address(std::string_view address);

/**
 * @brief Определить тип сети из адреса
 * 
 * @param address Bitcoin адрес
 * @return std::string_view "mainnet", "testnet", "regtest" или "unknown"
 */
[[nodiscard]] std::string_view get_network_from_address(std::string_view address);

} // namespace quaxis::bitcoin
