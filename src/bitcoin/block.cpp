/**
 * @file block.cpp
 * @brief Реализация работы с Bitcoin блоками
 */

#include "block.hpp"
#include "../core/byte_order.hpp"

#include <cstring>

namespace quaxis::bitcoin {

// =============================================================================
// BlockHeader
// =============================================================================

std::array<uint8_t, constants::BLOCK_HEADER_SIZE> BlockHeader::serialize() const noexcept {
    std::array<uint8_t, constants::BLOCK_HEADER_SIZE> result{};
    uint8_t* ptr = result.data();
    
    // version (4 байта, little-endian)
    write_le32(ptr, version);
    ptr += 4;
    
    // prev_block (32 байта, уже в little-endian)
    std::memcpy(ptr, prev_block.data(), 32);
    ptr += 32;
    
    // merkle_root (32 байта, уже в little-endian)
    std::memcpy(ptr, merkle_root.data(), 32);
    ptr += 32;
    
    // timestamp (4 байта, little-endian)
    write_le32(ptr, timestamp);
    ptr += 4;
    
    // bits (4 байта, little-endian)
    write_le32(ptr, bits);
    ptr += 4;
    
    // nonce (4 байта, little-endian)
    write_le32(ptr, nonce);
    
    return result;
}

Hash256 BlockHeader::hash() const noexcept {
    auto serialized = serialize();
    return crypto::sha256d(ByteSpan(serialized.data(), serialized.size()));
}

crypto::Sha256State BlockHeader::compute_midstate() const noexcept {
    auto serialized = serialize();
    return crypto::compute_midstate(serialized.data());
}

std::array<uint8_t, 16> BlockHeader::get_tail() const noexcept {
    auto serialized = serialize();
    std::array<uint8_t, 16> tail{};
    // Последние 16 байт: merkle_root[28:32] + timestamp + bits + nonce
    std::memcpy(tail.data(), serialized.data() + 64, 16);
    return tail;
}

Result<BlockHeader> BlockHeader::deserialize(ByteSpan data) {
    if (data.size() < constants::BLOCK_HEADER_SIZE) {
        return Err<BlockHeader>(
            ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для заголовка блока"
        );
    }
    
    BlockHeader header;
    const uint8_t* ptr = data.data();
    
    // version
    header.version = read_le32(ptr);
    ptr += 4;
    
    // prev_block
    std::memcpy(header.prev_block.data(), ptr, 32);
    ptr += 32;
    
    // merkle_root
    std::memcpy(header.merkle_root.data(), ptr, 32);
    ptr += 32;
    
    // timestamp
    header.timestamp = read_le32(ptr);
    ptr += 4;
    
    // bits
    header.bits = read_le32(ptr);
    ptr += 4;
    
    // nonce
    header.nonce = read_le32(ptr);
    
    return header;
}

// =============================================================================
// BlockTemplate
// =============================================================================

void BlockTemplate::update_extranonce(uint64_t extranonce) noexcept {
    // Обновляем extranonce в coinbase
    // extranonce находится на позиции 64-69 в coinbase (см. структуру в спецификации)
    if (coinbase_tx.size() >= 70) {
        // Записываем 6 байт extranonce в little-endian
        for (std::size_t i = 0; i < constants::EXTRANONCE_SIZE; ++i) {
            coinbase_tx[64 + i] = static_cast<uint8_t>((extranonce >> (i * 8)) & 0xFF);
        }
    }
    
    // Пересчитываем coinbase txid
    Hash256 coinbase_txid = crypto::sha256d(
        ByteSpan(coinbase_tx.data(), coinbase_tx.size())
    );
    
    // Обновляем merkle_root (для пустого блока merkle_root = coinbase_txid)
    header.merkle_root = coinbase_txid;
    
    // Пересчитываем midstate заголовка
    header_midstate = header.compute_midstate();
}

std::array<uint8_t, constants::JOB_MESSAGE_SIZE> BlockTemplate::create_job(
    uint32_t job_id
) const noexcept {
    std::array<uint8_t, constants::JOB_MESSAGE_SIZE> job{};
    uint8_t* ptr = job.data();
    
    // midstate (32 байта)
    auto midstate_bytes = crypto::state_to_bytes(header_midstate);
    std::memcpy(ptr, midstate_bytes.data(), 32);
    ptr += 32;
    
    // header_tail (12 байт = timestamp + bits + nonce)
    // Берём последние 12 байт заголовка (не 16, так как merkle_root уже в midstate)
    auto header_serialized = header.serialize();
    // timestamp на позиции 68, bits на 72, nonce на 76
    std::memcpy(ptr, header_serialized.data() + 68, 12);
    ptr += 12;
    
    // job_id (4 байта)
    write_le32(ptr, job_id);
    
    return job;
}

// =============================================================================
// Merkle Root
// =============================================================================

Hash256 compute_merkle_root(const std::vector<Hash256>& txids) noexcept {
    if (txids.empty()) {
        // Пустой Merkle root (не должно случаться, всегда есть coinbase)
        Hash256 empty{};
        return empty;
    }
    
    if (txids.size() == 1) {
        return compute_merkle_root_single(txids[0]);
    }
    
    // Рекурсивное построение Merkle дерева
    std::vector<Hash256> level = txids;
    
    while (level.size() > 1) {
        std::vector<Hash256> next_level;
        
        for (std::size_t i = 0; i < level.size(); i += 2) {
            Hash256 left = level[i];
            // Если нечётное количество, дублируем последний элемент
            Hash256 right = (i + 1 < level.size()) ? level[i + 1] : level[i];
            
            // Конкатенация и хеширование
            std::array<uint8_t, 64> combined{};
            std::memcpy(combined.data(), left.data(), 32);
            std::memcpy(combined.data() + 32, right.data(), 32);
            
            next_level.push_back(crypto::sha256d(ByteSpan(combined.data(), 64)));
        }
        
        level = std::move(next_level);
    }
    
    return level[0];
}

Hash256 compute_merkle_root_single(const Hash256& coinbase_txid) noexcept {
    // Для одной транзакции Merkle root = txid
    // Но в Bitcoin Merkle дерево требует минимум пары,
    // так что для одной транзакции root = hash(txid || txid)? 
    // Нет, для одной транзакции root = txid
    return coinbase_txid;
}

} // namespace quaxis::bitcoin
