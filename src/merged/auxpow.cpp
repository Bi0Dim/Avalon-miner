/**
 * @file auxpow.cpp
 * @brief Реализация AuxPoW структур и функций
 * 
 * Вспомогательный Proof of Work для merged mining.
 */

#include "auxpow.hpp"
#include "../core/byte_order.hpp"

#include <algorithm>
#include <cstring>

namespace quaxis::merged {

// =============================================================================
// MerkleBranch Implementation
// =============================================================================

Hash256 MerkleBranch::compute_root(const Hash256& leaf_hash) const noexcept {
    Hash256 current = leaf_hash;
    uint32_t idx = index;
    
    for (const auto& hash : hashes) {
        // Конкатенируем в правильном порядке (index bit определяет позицию)
        std::array<uint8_t, 64> combined{};
        
        if ((idx & 1) == 0) {
            // Текущий хеш слева, branch хеш справа
            std::copy(current.begin(), current.end(), combined.begin());
            std::copy(hash.begin(), hash.end(), combined.begin() + 32);
        } else {
            // Branch хеш слева, текущий хеш справа
            std::copy(hash.begin(), hash.end(), combined.begin());
            std::copy(current.begin(), current.end(), combined.begin() + 32);
        }
        
        // SHA256d для следующего уровня
        current = crypto::sha256d(combined);
        idx >>= 1;
    }
    
    return current;
}

bool MerkleBranch::verify(
    const Hash256& leaf_hash,
    const Hash256& expected_root
) const noexcept {
    Hash256 computed = compute_root(leaf_hash);
    return computed == expected_root;
}

Bytes MerkleBranch::serialize() const {
    Bytes result;
    
    // Количество хешей
    result.push_back(static_cast<uint8_t>(hashes.size()));
    
    // Хеши
    for (const auto& hash : hashes) {
        result.insert(result.end(), hash.begin(), hash.end());
    }
    
    // Индекс (little-endian)
    result.push_back(static_cast<uint8_t>(index & 0xFF));
    result.push_back(static_cast<uint8_t>((index >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((index >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((index >> 24) & 0xFF));
    
    return result;
}

Result<MerkleBranch> MerkleBranch::deserialize(ByteSpan data) {
    if (data.empty()) {
        return std::unexpected(Error{ErrorCode::CryptoInvalidLength, 
            "Пустые данные для десериализации MerkleBranch"});
    }
    
    MerkleBranch result;
    std::size_t offset = 0;
    
    // Количество хешей
    std::size_t hash_count = data[offset++];
    
    if (data.size() < offset + hash_count * 32 + 4) {
        return std::unexpected(Error{ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для десериализации MerkleBranch"});
    }
    
    // Хеши
    result.hashes.reserve(hash_count);
    for (std::size_t i = 0; i < hash_count; ++i) {
        Hash256 hash;
        std::copy_n(data.data() + offset, 32, hash.begin());
        result.hashes.push_back(hash);
        offset += 32;
    }
    
    // Индекс
    result.index = static_cast<uint32_t>(data[offset]) |
                   (static_cast<uint32_t>(data[offset + 1]) << 8) |
                   (static_cast<uint32_t>(data[offset + 2]) << 16) |
                   (static_cast<uint32_t>(data[offset + 3]) << 24);
    
    return result;
}

// =============================================================================
// AuxPow Implementation
// =============================================================================

bool AuxPow::verify(const Hash256& aux_hash) const noexcept {
    // 1. Проверяем aux_branch: aux_hash должен быть в aux merkle root
    // aux merkle root находится в coinbase после AUXPOW_MAGIC
    
    // Найти commitment в coinbase
    auto commitment = AuxCommitment::find_in_coinbase(coinbase_tx);
    if (!commitment) {
        return false;
    }
    
    // Проверить aux_branch
    if (!aux_branch.verify(aux_hash, commitment->aux_merkle_root)) {
        return false;
    }
    
    // 2. Проверяем coinbase_branch: coinbase должна быть в merkle root заголовка
    // Извлекаем merkle root из parent_header
    Hash256 parent_merkle_root;
    std::copy_n(parent_header.data() + 36, 32, parent_merkle_root.begin());
    
    Hash256 coinbase_txid = crypto::sha256d(coinbase_tx);
    if (!coinbase_branch.verify(coinbase_txid, parent_merkle_root)) {
        return false;
    }
    
    return true;
}

Hash256 AuxPow::get_parent_hash() const noexcept {
    return crypto::sha256d(parent_header);
}

Bytes AuxPow::serialize() const {
    Bytes result;
    
    // Длина coinbase транзакции (4 байта, little-endian)
    uint32_t coinbase_len = static_cast<uint32_t>(coinbase_tx.size());
    result.push_back(static_cast<uint8_t>(coinbase_len & 0xFF));
    result.push_back(static_cast<uint8_t>((coinbase_len >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((coinbase_len >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((coinbase_len >> 24) & 0xFF));
    
    // Coinbase транзакция
    result.insert(result.end(), coinbase_tx.begin(), coinbase_tx.end());
    
    // Coinbase hash
    result.insert(result.end(), coinbase_hash.begin(), coinbase_hash.end());
    
    // Coinbase branch
    auto coinbase_branch_data = coinbase_branch.serialize();
    result.insert(result.end(), coinbase_branch_data.begin(), coinbase_branch_data.end());
    
    // Aux branch
    auto aux_branch_data = aux_branch.serialize();
    result.insert(result.end(), aux_branch_data.begin(), aux_branch_data.end());
    
    // Parent header
    result.insert(result.end(), parent_header.begin(), parent_header.end());
    
    return result;
}

Result<AuxPow> AuxPow::deserialize(ByteSpan data) {
    if (data.size() < 4) {
        return std::unexpected(Error{ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для десериализации AuxPow"});
    }
    
    AuxPow result;
    std::size_t offset = 0;
    
    // Длина coinbase
    uint32_t coinbase_len = static_cast<uint32_t>(data[offset]) |
                            (static_cast<uint32_t>(data[offset + 1]) << 8) |
                            (static_cast<uint32_t>(data[offset + 2]) << 16) |
                            (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    
    if (data.size() < offset + coinbase_len) {
        return std::unexpected(Error{ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для coinbase"});
    }
    
    // Coinbase
    result.coinbase_tx.assign(data.data() + offset, data.data() + offset + coinbase_len);
    offset += coinbase_len;
    
    // Coinbase hash
    if (data.size() < offset + 32) {
        return std::unexpected(Error{ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для coinbase hash"});
    }
    std::copy_n(data.data() + offset, 32, result.coinbase_hash.begin());
    offset += 32;
    
    // Coinbase branch
    auto coinbase_branch_result = MerkleBranch::deserialize(data.subspan(offset));
    if (!coinbase_branch_result) {
        return std::unexpected(coinbase_branch_result.error());
    }
    result.coinbase_branch = std::move(*coinbase_branch_result);
    offset += result.coinbase_branch.serialize().size();
    
    // Aux branch
    auto aux_branch_result = MerkleBranch::deserialize(data.subspan(offset));
    if (!aux_branch_result) {
        return std::unexpected(aux_branch_result.error());
    }
    result.aux_branch = std::move(*aux_branch_result);
    offset += result.aux_branch.serialize().size();
    
    // Parent header
    if (data.size() < offset + 80) {
        return std::unexpected(Error{ErrorCode::CryptoInvalidLength,
            "Недостаточно данных для parent header"});
    }
    std::copy_n(data.data() + offset, 80, result.parent_header.begin());
    
    return result;
}

// =============================================================================
// AuxCommitment Implementation
// =============================================================================

std::array<uint8_t, 44> AuxCommitment::serialize() const noexcept {
    std::array<uint8_t, 44> result{};
    
    // Magic bytes
    std::copy(AUXPOW_MAGIC.begin(), AUXPOW_MAGIC.end(), result.begin());
    
    // Aux merkle root
    std::copy(aux_merkle_root.begin(), aux_merkle_root.end(), result.begin() + 4);
    
    // Tree size (little-endian)
    result[36] = static_cast<uint8_t>(tree_size & 0xFF);
    result[37] = static_cast<uint8_t>((tree_size >> 8) & 0xFF);
    result[38] = static_cast<uint8_t>((tree_size >> 16) & 0xFF);
    result[39] = static_cast<uint8_t>((tree_size >> 24) & 0xFF);
    
    // Merkle nonce (little-endian)
    result[40] = static_cast<uint8_t>(merkle_nonce & 0xFF);
    result[41] = static_cast<uint8_t>((merkle_nonce >> 8) & 0xFF);
    result[42] = static_cast<uint8_t>((merkle_nonce >> 16) & 0xFF);
    result[43] = static_cast<uint8_t>((merkle_nonce >> 24) & 0xFF);
    
    return result;
}

std::optional<AuxCommitment> AuxCommitment::find_in_coinbase(
    ByteSpan coinbase_data
) noexcept {
    // Ищем AUXPOW_MAGIC в coinbase
    if (coinbase_data.size() < 44) {
        return std::nullopt;
    }
    
    for (std::size_t i = 0; i <= coinbase_data.size() - 44; ++i) {
        if (std::equal(AUXPOW_MAGIC.begin(), AUXPOW_MAGIC.end(), 
                       coinbase_data.data() + i)) {
            AuxCommitment result;
            
            // Aux merkle root
            std::copy_n(coinbase_data.data() + i + 4, 32, 
                       result.aux_merkle_root.begin());
            
            // Tree size
            result.tree_size = 
                static_cast<uint32_t>(coinbase_data[i + 36]) |
                (static_cast<uint32_t>(coinbase_data[i + 37]) << 8) |
                (static_cast<uint32_t>(coinbase_data[i + 38]) << 16) |
                (static_cast<uint32_t>(coinbase_data[i + 39]) << 24);
            
            // Merkle nonce
            result.merkle_nonce = 
                static_cast<uint32_t>(coinbase_data[i + 40]) |
                (static_cast<uint32_t>(coinbase_data[i + 41]) << 8) |
                (static_cast<uint32_t>(coinbase_data[i + 42]) << 16) |
                (static_cast<uint32_t>(coinbase_data[i + 43]) << 24);
            
            return result;
        }
    }
    
    return std::nullopt;
}

// =============================================================================
// Вспомогательные функции
// =============================================================================

std::vector<Hash256> build_merkle_tree(
    const std::vector<Hash256>& leaves
) noexcept {
    if (leaves.empty()) {
        return {};
    }
    
    std::vector<Hash256> tree = leaves;
    
    // Дополняем до степени двойки последним элементом
    std::size_t n = 1;
    while (n < leaves.size()) {
        n *= 2;
    }
    
    while (tree.size() < n) {
        tree.push_back(tree.back());
    }
    
    // Строим дерево снизу вверх
    std::size_t level_start = 0;
    std::size_t level_size = n;
    
    while (level_size > 1) {
        for (std::size_t i = 0; i < level_size; i += 2) {
            std::array<uint8_t, 64> combined{};
            std::copy(tree[level_start + i].begin(), 
                     tree[level_start + i].end(), combined.begin());
            std::copy(tree[level_start + i + 1].begin(),
                     tree[level_start + i + 1].end(), combined.begin() + 32);
            
            tree.push_back(crypto::sha256d(combined));
        }
        
        level_start += level_size;
        level_size /= 2;
    }
    
    return tree;
}

MerkleBranch get_merkle_branch(
    const std::vector<Hash256>& tree,
    std::size_t index
) noexcept {
    MerkleBranch branch;
    branch.index = static_cast<uint32_t>(index);
    
    if (tree.empty()) {
        return branch;
    }
    
    // Вычисляем размер первого уровня (степень двойки)
    std::size_t level_size = 1;
    while (level_size < tree.size() / 2 + 1) {
        level_size *= 2;
    }
    
    std::size_t level_start = 0;
    std::size_t current_index = index;
    
    while (level_size > 1) {
        // Индекс соседнего узла
        std::size_t sibling_index = (current_index % 2 == 0) 
            ? current_index + 1 
            : current_index - 1;
        
        if (level_start + sibling_index < tree.size()) {
            branch.hashes.push_back(tree[level_start + sibling_index]);
        }
        
        level_start += level_size;
        level_size /= 2;
        current_index /= 2;
    }
    
    return branch;
}

uint32_t compute_slot_id(
    const Hash256& chain_id,
    uint32_t nonce,
    uint32_t tree_size
) noexcept {
    // Используем первые 4 байта chain_id XOR nonce
    uint32_t id = static_cast<uint32_t>(chain_id[0]) |
                  (static_cast<uint32_t>(chain_id[1]) << 8) |
                  (static_cast<uint32_t>(chain_id[2]) << 16) |
                  (static_cast<uint32_t>(chain_id[3]) << 24);
    
    return (id ^ nonce) % tree_size;
}

AuxCommitment create_aux_commitment(
    const std::vector<Hash256>& aux_hashes,
    const std::vector<Hash256>& chain_ids
) noexcept {
    AuxCommitment commitment;
    
    if (aux_hashes.empty() || chain_ids.empty()) {
        return commitment;
    }
    
    // Вычисляем размер дерева (следующая степень двойки)
    std::size_t n = 1;
    while (n < aux_hashes.size()) {
        n *= 2;
    }
    commitment.tree_size = static_cast<uint32_t>(n);
    
    // Подбираем nonce чтобы избежать коллизий slot ID
    // (упрощённая версия - используем 0)
    commitment.merkle_nonce = 0;
    
    // Размещаем хеши по slot ID
    std::vector<Hash256> slots(n, Hash256{});
    for (std::size_t i = 0; i < aux_hashes.size() && i < chain_ids.size(); ++i) {
        uint32_t slot = compute_slot_id(chain_ids[i], commitment.merkle_nonce, 
                                        commitment.tree_size);
        slots[slot] = aux_hashes[i];
    }
    
    // Строим Merkle tree и получаем root
    auto tree = build_merkle_tree(slots);
    if (!tree.empty()) {
        commitment.aux_merkle_root = tree.back();
    }
    
    return commitment;
}

bool meets_target(const Hash256& hash, uint32_t target_bits) noexcept {
    Hash256 target = bits_to_target(target_bits);
    
    // Сравниваем как big-endian числа (старшие байты в конце массива)
    for (int i = 31; i >= 0; --i) {
        if (hash[static_cast<std::size_t>(i)] < target[static_cast<std::size_t>(i)]) {
            return true;
        }
        if (hash[static_cast<std::size_t>(i)] > target[static_cast<std::size_t>(i)]) {
            return false;
        }
    }
    
    return true; // Равны
}

Hash256 bits_to_target(uint32_t bits) noexcept {
    Hash256 target{};
    
    // Compact format: EEXXXXXX
    // EE = exponent (количество байт от конца)
    // XXXXXX = mantissa (3 байта)
    uint32_t exponent = (bits >> 24) & 0xFF;
    uint32_t mantissa = bits & 0x00FFFFFF;
    
    if (exponent <= 3) {
        mantissa >>= 8 * (3 - exponent);
        target[0] = static_cast<uint8_t>(mantissa & 0xFF);
        target[1] = static_cast<uint8_t>((mantissa >> 8) & 0xFF);
        target[2] = static_cast<uint8_t>((mantissa >> 16) & 0xFF);
    } else {
        std::size_t shift = exponent - 3;
        if (shift < 29) {
            target[shift] = static_cast<uint8_t>(mantissa & 0xFF);
            target[shift + 1] = static_cast<uint8_t>((mantissa >> 8) & 0xFF);
            target[shift + 2] = static_cast<uint8_t>((mantissa >> 16) & 0xFF);
        }
    }
    
    return target;
}

} // namespace quaxis::merged
