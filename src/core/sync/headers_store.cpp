/**
 * @file headers_store.cpp
 * @brief Реализация хранилища заголовков
 */

#include "headers_store.hpp"

#include <stdexcept>

namespace quaxis::core::sync {

HeadersStore::HeadersStore(const ChainParams& params)
    : params_(params) {
    // Инициализируем genesis
    genesis_ = BlockHeader{};
    genesis_.version = 1;
    genesis_.prev_hash = Hash256{};
    genesis_.merkle_root = Hash256{};
    genesis_.timestamp = 1231006505;  // Bitcoin genesis timestamp
    genesis_.bits = params_.difficulty.pow_limit_bits;
    genesis_.nonce = 2083236893;
    
    // Добавляем genesis
    headers_.push_back(genesis_);
    hash_index_[genesis_.hash()] = 0;
}

bool HeadersStore::add_header(const BlockHeader& header, uint32_t height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Проверяем что высота соответствует следующему блоку
    if (height != headers_.size()) {
        return false;
    }
    
    // Проверяем prev_hash
    if (height > 0) {
        auto tip_hash = headers_.back().hash();
        if (header.prev_hash != tip_hash) {
            return false;
        }
    }
    
    // Добавляем заголовок
    headers_.push_back(header);
    hash_index_[header.hash()] = height;
    
    return true;
}

std::optional<BlockHeader> HeadersStore::get_by_hash(
    const Hash256& hash
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end() && it->second < headers_.size()) {
        return headers_[it->second];
    }
    return std::nullopt;
}

std::optional<BlockHeader> HeadersStore::get_by_height(
    uint32_t height
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (height < headers_.size()) {
        return headers_[height];
    }
    return std::nullopt;
}

std::optional<uint32_t> HeadersStore::get_height(
    const Hash256& hash
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = hash_index_.find(hash);
    if (it != hash_index_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const BlockHeader& HeadersStore::get_tip() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (headers_.empty()) {
        throw std::runtime_error("Headers store is empty");
    }
    return headers_.back();
}

uint32_t HeadersStore::get_tip_height() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (headers_.empty()) {
        return 0;
    }
    return static_cast<uint32_t>(headers_.size() - 1);
}

Hash256 HeadersStore::get_tip_hash() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (headers_.empty()) {
        return Hash256{};
    }
    return headers_.back().hash();
}

bool HeadersStore::has_header(const Hash256& hash) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hash_index_.find(hash) != hash_index_.end();
}

std::vector<BlockHeader> HeadersStore::get_recent_headers(
    std::size_t count
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BlockHeader> result;
    
    if (headers_.empty()) {
        return result;
    }
    
    std::size_t start = headers_.size() > count ? headers_.size() - count : 0;
    result.reserve(headers_.size() - start);
    
    for (std::size_t i = start; i < headers_.size(); ++i) {
        result.push_back(headers_[i]);
    }
    
    return result;
}

std::vector<BlockHeader> HeadersStore::get_headers_range(
    uint32_t start_height,
    uint32_t end_height
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BlockHeader> result;
    
    if (start_height >= headers_.size()) {
        return result;
    }
    
    uint32_t actual_end = std::min(end_height + 1, 
                                    static_cast<uint32_t>(headers_.size()));
    result.reserve(actual_end - start_height);
    
    for (uint32_t i = start_height; i < actual_end; ++i) {
        result.push_back(headers_[i]);
    }
    
    return result;
}

std::size_t HeadersStore::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return headers_.size();
}

void HeadersStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    headers_.clear();
    hash_index_.clear();
    
    // Восстанавливаем genesis
    headers_.push_back(genesis_);
    hash_index_[genesis_.hash()] = 0;
}

} // namespace quaxis::core::sync
