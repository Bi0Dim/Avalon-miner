/**
 * @file extranonce_manager.cpp
 * @brief Implementation of per-ASIC extranonce management
 */

#include "extranonce_manager.hpp"

namespace quaxis::mining {

ExtrannonceManager::ExtrannonceManager(uint64_t start_value)
    : next_extranonce_(start_value) {}

uint64_t ExtrannonceManager::assign_extranonce(uint32_t connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get current value and increment atomically
    uint64_t extranonce = next_extranonce_.fetch_add(1, std::memory_order_relaxed);
    
    // Associate with connection
    connection_extranonces_[connection_id] = extranonce;
    
    return extranonce;
}

void ExtrannonceManager::release_extranonce(uint32_t connection_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_extranonces_.erase(connection_id);
}

std::optional<uint64_t> ExtrannonceManager::get_extranonce(uint32_t connection_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connection_extranonces_.find(connection_id);
    if (it != connection_extranonces_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ExtrannonceManager::has_extranonce(uint32_t connection_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_extranonces_.contains(connection_id);
}

std::size_t ExtrannonceManager::active_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_extranonces_.size();
}

uint64_t ExtrannonceManager::peek_next_extranonce() const noexcept {
    return next_extranonce_.load(std::memory_order_relaxed);
}

std::vector<uint32_t> ExtrannonceManager::get_active_connections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<uint32_t> connections;
    connections.reserve(connection_extranonces_.size());
    
    for (const auto& [id, _] : connection_extranonces_) {
        connections.push_back(id);
    }
    
    return connections;
}

} // namespace quaxis::mining
