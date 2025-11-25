/**
 * @file coinbase.hpp
 * @brief Построение coinbase транзакции
 * 
 * Coinbase - это первая транзакция в каждом блоке, которая:
 * 1. Создаёт новые биткоины (награда за блок)
 * 2. Собирает комиссии с транзакций (для пустого блока = 0)
 * 3. Содержит произвольные данные в scriptsig (тег "quaxis", extranonce)
 * 
 * Структура coinbase транзакции Quaxis (110 байт):
 * 
 * ЧАСТЬ 1 (64 байта) — КОНСТАНТА ДЛЯ MIDSTATE:
 * [0-3]    version         = 01 00 00 00
 * [4]      input_count     = 01
 * [5-36]   prev_tx_hash    = 00 × 32
 * [37-40]  prev_tx_index   = FF FF FF FF
 * [41]     scriptsig_len   = 1A (26 байт)
 * [42]     height_push     = 03
 * [43-45]  height          = XX XX XX (LE)
 * [46-51]  "quaxis"        = 71 75 61 78 69 73
 * [52-63]  padding         = 00 × 12
 * 
 * ЧАСТЬ 2 (46 байт) — СОДЕРЖИТ EXTRANONCE:
 * [64-69]  extranonce      = XX × 6
 * [70-73]  sequence        = FF FF FF FF
 * [74]     output_count    = 01
 * [75-82]  value           = reward (LE, 8 байт)
 * [83]     script_len      = 16 (22 байта)
 * [84-105] scriptPubKey    = 00 14 [pubkey_hash × 20]
 * [106-109] locktime       = 00 00 00 00
 */

#pragma once

#include "../core/types.hpp"
#include "../core/constants.hpp"
#include "../crypto/sha256.hpp"

#include <string_view>
#include <optional>

namespace quaxis::bitcoin {

// =============================================================================
// Coinbase Builder
// =============================================================================

/**
 * @brief Построитель coinbase транзакции
 * 
 * Создаёт оптимизированную coinbase транзакцию для соло-майнинга:
 * - Фиксированный размер 110 байт
 * - Тег "quaxis" в scriptsig
 * - 6-байтный extranonce
 * - P2WPKH выход (SegWit native)
 */
class CoinbaseBuilder {
public:
    /**
     * @brief Создать builder с параметрами
     * 
     * @param payout_pubkey_hash 20-байтный хеш публичного ключа (для P2WPKH)
     * @param coinbase_tag Тег в scriptsig (по умолчанию "quaxis")
     */
    explicit CoinbaseBuilder(
        const Hash160& payout_pubkey_hash,
        std::string_view coinbase_tag = "quaxis"
    );
    
    /**
     * @brief Создать builder из Bitcoin адреса
     * 
     * @param payout_address Bitcoin адрес в формате bc1q... (P2WPKH)
     * @param coinbase_tag Тег в scriptsig
     * @return Result<CoinbaseBuilder> Builder или ошибка парсинга адреса
     */
    [[nodiscard]] static Result<CoinbaseBuilder> from_address(
        std::string_view payout_address,
        std::string_view coinbase_tag = "quaxis"
    );
    
    /**
     * @brief Построить coinbase транзакцию
     * 
     * @param height Высота блока
     * @param value Награда + комиссии в satoshi
     * @param extranonce Значение extranonce (6 байт)
     * @return Bytes Сериализованная coinbase транзакция
     */
    [[nodiscard]] Bytes build(
        uint32_t height,
        int64_t value,
        uint64_t extranonce
    ) const;
    
    /**
     * @brief Построить coinbase и вычислить midstate
     * 
     * Оптимизированная версия для майнинга.
     * Возвращает coinbase и предвычисленный midstate первых 64 байт.
     * 
     * @param height Высота блока
     * @param value Награда в satoshi
     * @param extranonce Значение extranonce
     * @return std::pair<Bytes, crypto::Sha256State> Coinbase и midstate
     */
    [[nodiscard]] std::pair<Bytes, crypto::Sha256State> build_with_midstate(
        uint32_t height,
        int64_t value,
        uint64_t extranonce
    ) const;
    
    /**
     * @brief Получить размер coinbase транзакции
     * 
     * @return std::size_t Размер в байтах (110)
     */
    [[nodiscard]] static constexpr std::size_t size() noexcept {
        return constants::COINBASE_SIZE;
    }
    
private:
    /// @brief 20-байтный хеш публичного ключа для P2WPKH
    Hash160 pubkey_hash_;
    
    /// @brief Тег coinbase (обычно "quaxis")
    std::string coinbase_tag_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Вычислить txid coinbase транзакции
 * 
 * txid = SHA256d(coinbase_raw)
 * 
 * @param coinbase_raw Сериализованная coinbase транзакция
 * @return Hash256 Transaction ID
 */
[[nodiscard]] Hash256 compute_txid(const Bytes& coinbase_raw) noexcept;

/**
 * @brief Создать scriptsig для coinbase
 * 
 * Формат: OP_PUSH(height) + height + tag + padding + extranonce
 * 
 * @param height Высота блока
 * @param tag Тег (6 байт)
 * @param extranonce 6-байтное значение extranonce
 * @return Bytes Scriptsig
 */
[[nodiscard]] Bytes create_coinbase_scriptsig(
    uint32_t height,
    std::string_view tag,
    uint64_t extranonce
);

/**
 * @brief Создать scriptPubKey для P2WPKH
 * 
 * Формат: OP_0 OP_PUSH20 <pubkey_hash>
 * 
 * @param pubkey_hash 20-байтный хеш публичного ключа
 * @return std::array<uint8_t, 22> ScriptPubKey
 */
[[nodiscard]] std::array<uint8_t, 22> create_p2wpkh_script(
    const Hash160& pubkey_hash
) noexcept;

} // namespace quaxis::bitcoin
