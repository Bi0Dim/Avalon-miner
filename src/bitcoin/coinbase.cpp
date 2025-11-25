/**
 * @file coinbase.cpp
 * @brief Реализация построения coinbase транзакции
 */

#include "coinbase.hpp"
#include "address.hpp"
#include "../core/byte_order.hpp"

#include <cstring>

namespace quaxis::bitcoin {

// =============================================================================
// CoinbaseBuilder
// =============================================================================

CoinbaseBuilder::CoinbaseBuilder(
    const Hash160& payout_pubkey_hash,
    std::string_view coinbase_tag
) : pubkey_hash_(payout_pubkey_hash)
  , coinbase_tag_(coinbase_tag)
{
    // Обрезаем тег до 6 символов если длиннее
    if (coinbase_tag_.size() > constants::COINBASE_TAG_SIZE) {
        coinbase_tag_.resize(constants::COINBASE_TAG_SIZE);
    }
}

Result<CoinbaseBuilder> CoinbaseBuilder::from_address(
    std::string_view payout_address,
    std::string_view coinbase_tag
) {
    // Парсим адрес
    auto result = parse_p2wpkh_address(payout_address);
    if (!result) {
        return Err<CoinbaseBuilder>(result.error().code, result.error().message);
    }
    
    return CoinbaseBuilder(*result, coinbase_tag);
}

Bytes CoinbaseBuilder::build(
    uint32_t height,
    int64_t value,
    uint64_t extranonce
) const {
    Bytes tx;
    tx.reserve(constants::COINBASE_SIZE);
    
    // === ЧАСТЬ 1: Первые 64 байта (константа для midstate) ===
    
    // [0-3] version = 1
    tx.push_back(0x01);
    tx.push_back(0x00);
    tx.push_back(0x00);
    tx.push_back(0x00);
    
    // [4] input_count = 1
    tx.push_back(0x01);
    
    // [5-36] prev_tx_hash = 32 нуля (coinbase не имеет входа)
    for (int i = 0; i < 32; ++i) {
        tx.push_back(0x00);
    }
    
    // [37-40] prev_tx_index = 0xFFFFFFFF
    tx.push_back(0xFF);
    tx.push_back(0xFF);
    tx.push_back(0xFF);
    tx.push_back(0xFF);
    
    // [41] scriptsig_len = 26 байт (0x1A)
    // scriptsig = height_push(1) + height(3) + tag(6) + padding(10) + extranonce(6) = 26
    tx.push_back(0x1A);
    
    // [42] height_push = OP_PUSH3 (0x03) - высота занимает 3 байта
    tx.push_back(0x03);
    
    // [43-45] height (little-endian, 3 байта)
    tx.push_back(static_cast<uint8_t>(height & 0xFF));
    tx.push_back(static_cast<uint8_t>((height >> 8) & 0xFF));
    tx.push_back(static_cast<uint8_t>((height >> 16) & 0xFF));
    
    // [46-51] "quaxis" tag (6 байт)
    for (std::size_t i = 0; i < constants::COINBASE_TAG_SIZE; ++i) {
        if (i < coinbase_tag_.size()) {
            tx.push_back(static_cast<uint8_t>(coinbase_tag_[i]));
        } else {
            tx.push_back(0x00);
        }
    }
    
    // [52-63] padding (12 байт нулей чтобы заполнить до 64 байт)
    for (int i = 0; i < 12; ++i) {
        tx.push_back(0x00);
    }
    
    // === ЧАСТЬ 2: Следующие 46 байт (содержит extranonce) ===
    
    // [64-69] extranonce (6 байт, little-endian)
    for (std::size_t i = 0; i < constants::EXTRANONCE_SIZE; ++i) {
        tx.push_back(static_cast<uint8_t>((extranonce >> (i * 8)) & 0xFF));
    }
    
    // [70-73] sequence = 0xFFFFFFFF
    tx.push_back(0xFF);
    tx.push_back(0xFF);
    tx.push_back(0xFF);
    tx.push_back(0xFF);
    
    // [74] output_count = 1
    tx.push_back(0x01);
    
    // [75-82] value (8 байт, little-endian)
    for (int i = 0; i < 8; ++i) {
        tx.push_back(static_cast<uint8_t>((static_cast<uint64_t>(value) >> (i * 8)) & 0xFF));
    }
    
    // [83] script_len = 22 байта (P2WPKH: OP_0 OP_PUSH20 <20 bytes>)
    tx.push_back(0x16);
    
    // [84-105] scriptPubKey (22 байта)
    auto script = create_p2wpkh_script(pubkey_hash_);
    for (auto byte : script) {
        tx.push_back(byte);
    }
    
    // [106-109] locktime = 0
    tx.push_back(0x00);
    tx.push_back(0x00);
    tx.push_back(0x00);
    tx.push_back(0x00);
    
    return tx;
}

std::pair<Bytes, crypto::Sha256State> CoinbaseBuilder::build_with_midstate(
    uint32_t height,
    int64_t value,
    uint64_t extranonce
) const {
    Bytes tx = build(height, value, extranonce);
    
    // Вычисляем midstate первых 64 байт
    crypto::Sha256State midstate = crypto::compute_midstate(tx.data());
    
    return {std::move(tx), midstate};
}

// =============================================================================
// Вспомогательные функции
// =============================================================================

Hash256 compute_txid(const Bytes& coinbase_raw) noexcept {
    return crypto::sha256d(ByteSpan(coinbase_raw.data(), coinbase_raw.size()));
}

Bytes create_coinbase_scriptsig(
    uint32_t height,
    std::string_view tag,
    uint64_t extranonce
) {
    Bytes scriptsig;
    scriptsig.reserve(26);
    
    // OP_PUSH3 + height (3 байта)
    scriptsig.push_back(0x03);
    scriptsig.push_back(static_cast<uint8_t>(height & 0xFF));
    scriptsig.push_back(static_cast<uint8_t>((height >> 8) & 0xFF));
    scriptsig.push_back(static_cast<uint8_t>((height >> 16) & 0xFF));
    
    // Tag (6 байт)
    for (std::size_t i = 0; i < constants::COINBASE_TAG_SIZE; ++i) {
        if (i < tag.size()) {
            scriptsig.push_back(static_cast<uint8_t>(tag[i]));
        } else {
            scriptsig.push_back(0x00);
        }
    }
    
    // Padding (10 байт)
    for (int i = 0; i < 10; ++i) {
        scriptsig.push_back(0x00);
    }
    
    // Extranonce (6 байт)
    for (std::size_t i = 0; i < constants::EXTRANONCE_SIZE; ++i) {
        scriptsig.push_back(static_cast<uint8_t>((extranonce >> (i * 8)) & 0xFF));
    }
    
    return scriptsig;
}

std::array<uint8_t, 22> create_p2wpkh_script(const Hash160& pubkey_hash) noexcept {
    std::array<uint8_t, 22> script{};
    
    // OP_0 (0x00) - версия witness программы
    script[0] = 0x00;
    
    // OP_PUSHBYTES_20 (0x14) - длина данных
    script[1] = 0x14;
    
    // Pubkey hash (20 байт)
    std::memcpy(script.data() + 2, pubkey_hash.data(), 20);
    
    return script;
}

} // namespace quaxis::bitcoin
