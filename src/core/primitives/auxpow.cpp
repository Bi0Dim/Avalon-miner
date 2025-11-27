/**
 * @file auxpow.cpp
 * @brief Реализация AuxPoW функций
 */

#include "auxpow.hpp"
#include "../../crypto/sha256.hpp"

#include <cstring>
#include <algorithm>

namespace quaxis::core {

std::array<uint8_t, 44> AuxPowCommitment::serialize() const noexcept {
    std::array<uint8_t, 44> result{};
    
    // Magic bytes
    std::memcpy(result.data(), AUXPOW_MAGIC.data(), 4);
    
    // Aux merkle root
    std::memcpy(result.data() + 4, aux_merkle_root.data(), 32);
    
    // Tree size (little-endian)
    result[36] = static_cast<uint8_t>(tree_size);
    result[37] = static_cast<uint8_t>(tree_size >> 8);
    result[38] = static_cast<uint8_t>(tree_size >> 16);
    result[39] = static_cast<uint8_t>(tree_size >> 24);
    
    // Merkle nonce (little-endian)
    result[40] = static_cast<uint8_t>(merkle_nonce);
    result[41] = static_cast<uint8_t>(merkle_nonce >> 8);
    result[42] = static_cast<uint8_t>(merkle_nonce >> 16);
    result[43] = static_cast<uint8_t>(merkle_nonce >> 24);
    
    return result;
}

std::optional<AuxPowCommitment> AuxPowCommitment::find_in_coinbase(
    ByteSpan coinbase_data
) noexcept {
    if (coinbase_data.size() < 44) {
        return std::nullopt;
    }
    
    // Ищем magic bytes
    for (std::size_t i = 0; i + 44 <= coinbase_data.size(); ++i) {
        if (std::memcmp(coinbase_data.data() + i, AUXPOW_MAGIC.data(), 4) == 0) {
            AuxPowCommitment commitment;
            
            // Aux merkle root
            std::memcpy(commitment.aux_merkle_root.data(), 
                       coinbase_data.data() + i + 4, 32);
            
            // Tree size
            commitment.tree_size = 
                static_cast<uint32_t>(coinbase_data[i + 36]) |
                (static_cast<uint32_t>(coinbase_data[i + 37]) << 8) |
                (static_cast<uint32_t>(coinbase_data[i + 38]) << 16) |
                (static_cast<uint32_t>(coinbase_data[i + 39]) << 24);
            
            // Merkle nonce
            commitment.merkle_nonce = 
                static_cast<uint32_t>(coinbase_data[i + 40]) |
                (static_cast<uint32_t>(coinbase_data[i + 41]) << 8) |
                (static_cast<uint32_t>(coinbase_data[i + 42]) << 16) |
                (static_cast<uint32_t>(coinbase_data[i + 43]) << 24);
            
            return commitment;
        }
    }
    
    return std::nullopt;
}

bool AuxPow::verify(const Hash256& aux_hash) const noexcept {
    // 1. Проверяем coinbase branch
    auto computed_merkle_root = coinbase_branch.compute_root(coinbase_hash);
    if (computed_merkle_root != parent_header.merkle_root) {
        return false;
    }
    
    // 2. Находим commitment в coinbase
    auto commitment = AuxPowCommitment::find_in_coinbase(coinbase_tx);
    if (!commitment) {
        return false;
    }
    
    // 3. Проверяем aux branch
    auto computed_aux_root = aux_branch.compute_root(aux_hash);
    if (computed_aux_root != commitment->aux_merkle_root) {
        return false;
    }
    
    // 4. Проверяем proof-of-work родительского блока
    return verify_pow();
}

bool AuxPow::verify_pow() const noexcept {
    return parent_header.check_pow();
}

bool AuxPow::meets_target(uint32_t target_bits) const noexcept {
    auto parent_hash = parent_header.hash_uint256();
    auto target = bits_to_target(target_bits);
    return parent_hash <= target;
}

Hash256 AuxPow::get_parent_hash() const noexcept {
    return parent_header.hash();
}

uint32_t AuxPow::get_chain_id() const noexcept {
    return parent_header.get_chain_id();
}

Bytes AuxPow::serialize() const {
    Bytes result;
    
    // Coinbase tx length (varint, упрощённо 2 байта)
    uint16_t tx_len = static_cast<uint16_t>(coinbase_tx.size());
    result.push_back(static_cast<uint8_t>(tx_len));
    result.push_back(static_cast<uint8_t>(tx_len >> 8));
    
    // Coinbase tx
    result.insert(result.end(), coinbase_tx.begin(), coinbase_tx.end());
    
    // Coinbase hash
    result.insert(result.end(), coinbase_hash.begin(), coinbase_hash.end());
    
    // Coinbase branch
    auto cb_branch = coinbase_branch.serialize();
    result.insert(result.end(), cb_branch.begin(), cb_branch.end());
    
    // Aux branch
    auto aux_br = aux_branch.serialize();
    result.insert(result.end(), aux_br.begin(), aux_br.end());
    
    // Parent header
    auto header = parent_header.serialize();
    result.insert(result.end(), header.begin(), header.end());
    
    return result;
}

std::optional<AuxPow> AuxPow::deserialize(ByteSpan data) {
    if (data.size() < 2) {
        return std::nullopt;
    }
    
    AuxPow auxpow;
    std::size_t offset = 0;
    
    // Coinbase tx length
    uint16_t tx_len = static_cast<uint16_t>(data[offset]) |
                      (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;
    
    if (offset + tx_len > data.size()) {
        return std::nullopt;
    }
    
    // Coinbase tx
    auxpow.coinbase_tx.assign(data.begin() + static_cast<std::ptrdiff_t>(offset), 
                              data.begin() + static_cast<std::ptrdiff_t>(offset + tx_len));
    offset += tx_len;
    
    // Coinbase hash
    if (offset + 32 > data.size()) {
        return std::nullopt;
    }
    std::memcpy(auxpow.coinbase_hash.data(), data.data() + offset, 32);
    offset += 32;
    
    // Остальное требует более сложной десериализации branches
    // Для простоты возвращаем то, что есть
    
    return auxpow;
}

uint32_t compute_slot_id(
    uint32_t chain_id,
    uint32_t nonce,
    uint32_t tree_size
) noexcept {
    if (tree_size == 0) return 0;
    return (chain_id * nonce) % tree_size;
}

uint32_t compute_slot_id(
    const Hash256& genesis_hash,
    uint32_t nonce,
    uint32_t tree_size
) noexcept {
    if (tree_size == 0) return 0;
    
    // Берём первые 4 байта genesis hash как chain_id
    uint32_t chain_id = static_cast<uint32_t>(genesis_hash[0]) |
                        (static_cast<uint32_t>(genesis_hash[1]) << 8) |
                        (static_cast<uint32_t>(genesis_hash[2]) << 16) |
                        (static_cast<uint32_t>(genesis_hash[3]) << 24);
    
    return compute_slot_id(chain_id, nonce, tree_size);
}

AuxPowCommitment create_commitment(
    const std::vector<Hash256>& aux_hashes,
    const std::vector<uint32_t>& chain_ids
) noexcept {
    AuxPowCommitment commitment;
    
    if (aux_hashes.empty()) {
        return commitment;
    }
    
    // Вычисляем размер дерева (следующая степень двойки)
    std::size_t size = 1;
    while (size < aux_hashes.size()) {
        size *= 2;
    }
    commitment.tree_size = static_cast<uint32_t>(size);
    
    // Инициализируем дерево пустыми хешами
    std::vector<Hash256> tree(size, Hash256{});
    
    // Размещаем хеши по slot IDs
    for (std::size_t i = 0; i < aux_hashes.size() && i < chain_ids.size(); ++i) {
        uint32_t slot = compute_slot_id(chain_ids[i], commitment.merkle_nonce, 
                                        commitment.tree_size);
        tree[slot] = aux_hashes[i];
    }
    
    // Вычисляем Merkle root
    commitment.aux_merkle_root = compute_merkle_root(std::move(tree));
    
    return commitment;
}

bool meets_target(const Hash256& hash, uint32_t target_bits) noexcept {
    auto target = bits_to_target(target_bits);
    return uint256{hash} <= target;
}

} // namespace quaxis::core
