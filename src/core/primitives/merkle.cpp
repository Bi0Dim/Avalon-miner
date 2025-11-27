/**
 * @file merkle.cpp
 * @brief Реализация Merkle tree функций
 */

#include "merkle.hpp"
#include "../../crypto/sha256.hpp"

#include <cstring>
#include <stdexcept>

namespace quaxis::core {

Hash256 MerkleBranch::compute_root(const Hash256& leaf_hash) const noexcept {
    Hash256 current = leaf_hash;
    uint32_t idx = index;
    
    for (const auto& hash : hashes) {
        if (idx & 1) {
            // Текущий узел справа, hash слева
            current = merkle_hash(hash, current);
        } else {
            // Текущий узел слева, hash справа
            current = merkle_hash(current, hash);
        }
        idx >>= 1;
    }
    
    return current;
}

bool MerkleBranch::verify(
    const Hash256& leaf_hash,
    const Hash256& expected_root
) const noexcept {
    auto computed = compute_root(leaf_hash);
    return computed == expected_root;
}

Bytes MerkleBranch::serialize() const {
    Bytes result;
    
    // Количество хешей (varint, упрощённо 1 байт)
    result.push_back(static_cast<uint8_t>(hashes.size()));
    
    // Хеши
    for (const auto& hash : hashes) {
        result.insert(result.end(), hash.begin(), hash.end());
    }
    
    // Индекс (4 байта, little-endian)
    result.push_back(static_cast<uint8_t>(index));
    result.push_back(static_cast<uint8_t>(index >> 8));
    result.push_back(static_cast<uint8_t>(index >> 16));
    result.push_back(static_cast<uint8_t>(index >> 24));
    
    return result;
}

MerkleBranch MerkleBranch::deserialize(ByteSpan data) {
    MerkleBranch branch;
    
    if (data.empty()) {
        return branch;
    }
    
    std::size_t offset = 0;
    
    // Количество хешей
    uint8_t count = data[offset++];
    
    // Хеши
    branch.hashes.reserve(count);
    for (uint8_t i = 0; i < count && offset + 32 <= data.size(); ++i) {
        Hash256 hash;
        std::memcpy(hash.data(), data.data() + offset, 32);
        branch.hashes.push_back(hash);
        offset += 32;
    }
    
    // Индекс
    if (offset + 4 <= data.size()) {
        branch.index = static_cast<uint32_t>(data[offset]) |
                       (static_cast<uint32_t>(data[offset + 1]) << 8) |
                       (static_cast<uint32_t>(data[offset + 2]) << 16) |
                       (static_cast<uint32_t>(data[offset + 3]) << 24);
    }
    
    return branch;
}

MerkleTree::MerkleTree(std::vector<Hash256> leaves) 
    : leaf_count_(leaves.size()) {
    if (leaves.empty()) {
        nodes_.push_back(Hash256{});
        return;
    }
    
    // Дополняем до степени двойки (Bitcoin дублирует последний)
    while (leaves.size() > 1 && (leaves.size() & (leaves.size() - 1)) != 0) {
        leaves.push_back(leaves.back());
    }
    
    // Копируем листья
    nodes_ = std::move(leaves);
    
    // Строим дерево снизу вверх
    std::size_t level_start = 0;
    std::size_t level_size = nodes_.size();
    
    while (level_size > 1) {
        std::size_t next_level_size = level_size / 2;
        
        for (std::size_t i = 0; i < next_level_size; ++i) {
            auto combined = merkle_hash(
                nodes_[level_start + i * 2],
                nodes_[level_start + i * 2 + 1]
            );
            nodes_.push_back(combined);
        }
        
        level_start += level_size;
        level_size = next_level_size;
    }
}

const Hash256& MerkleTree::root() const noexcept {
    return nodes_.back();
}

MerkleBranch MerkleTree::get_branch(std::size_t index) const {
    MerkleBranch branch;
    
    if (nodes_.empty() || index >= leaf_count_) {
        return branch;
    }
    
    branch.index = static_cast<uint32_t>(index);
    
    std::size_t level_start = 0;
    std::size_t level_size = leaf_count_;
    
    // Дополняем до размера, использованного при построении
    while (level_size > 1 && (level_size & (level_size - 1)) != 0) {
        level_size++;
    }
    
    std::size_t idx = index;
    
    while (level_size > 1) {
        std::size_t sibling_idx;
        if (idx & 1) {
            sibling_idx = idx - 1;
        } else {
            sibling_idx = (idx + 1 < level_size) ? idx + 1 : idx;
        }
        
        if (level_start + sibling_idx < nodes_.size()) {
            branch.hashes.push_back(nodes_[level_start + sibling_idx]);
        }
        
        level_start += level_size;
        level_size /= 2;
        idx /= 2;
    }
    
    return branch;
}

std::size_t MerkleTree::leaf_count() const noexcept {
    return leaf_count_;
}

std::size_t MerkleTree::depth() const noexcept {
    if (leaf_count_ <= 1) return 0;
    std::size_t d = 0;
    std::size_t n = leaf_count_ - 1;
    while (n > 0) {
        n >>= 1;
        d++;
    }
    return d;
}

const std::vector<Hash256>& MerkleTree::nodes() const noexcept {
    return nodes_;
}

Hash256 compute_merkle_root(std::vector<Hash256> leaves) noexcept {
    if (leaves.empty()) {
        return Hash256{};
    }
    
    if (leaves.size() == 1) {
        return leaves[0];
    }
    
    // Дублируем последний при нечётном количестве
    while (leaves.size() > 1) {
        if (leaves.size() % 2 != 0) {
            leaves.push_back(leaves.back());
        }
        
        std::vector<Hash256> next_level;
        next_level.reserve(leaves.size() / 2);
        
        for (std::size_t i = 0; i < leaves.size(); i += 2) {
            next_level.push_back(merkle_hash(leaves[i], leaves[i + 1]));
        }
        
        leaves = std::move(next_level);
    }
    
    return leaves[0];
}

Hash256 merkle_hash(const Hash256& left, const Hash256& right) noexcept {
    std::array<uint8_t, 64> combined;
    std::memcpy(combined.data(), left.data(), 32);
    std::memcpy(combined.data() + 32, right.data(), 32);
    return quaxis::crypto::sha256d(combined);
}

Hash256 compute_witness_merkle_root(const std::vector<Hash256>& wtxids) noexcept {
    if (wtxids.empty()) {
        return Hash256{};
    }
    
    std::vector<Hash256> leaves = wtxids;
    
    // Coinbase witness txid всегда 0x0000...
    if (!leaves.empty()) {
        leaves[0] = Hash256{};
    }
    
    return compute_merkle_root(std::move(leaves));
}

} // namespace quaxis::core
