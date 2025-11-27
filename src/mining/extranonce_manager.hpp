/**
 * @file extranonce_manager.hpp
 * @brief Per-ASIC connection extranonce management
 * 
 * CRITICAL: Each ASIC connection MUST have a unique extranonce to prevent
 * duplicate work. Without this, multiple ASICs would compute the same hashes,
 * wasting hashrate.
 * 
 * This manager:
 * - Assigns a unique extranonce to each new ASIC connection
 * - Tracks active extranonces per connection
 * - Releases extranonces when connections close
 * - Ensures no two connections ever have the same extranonce
 */

#pragma once

#include "../core/types.hpp"

#include <atomic>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <cstdint>

namespace quaxis::mining {

/**
 * @brief Manager for per-connection extranonce values
 * 
 * Thread-safe manager that ensures each ASIC connection gets a unique
 * extranonce value. This is CRITICAL for preventing duplicate work
 * when multiple ASICs are connected.
 * 
 * Extranonce is included in the coinbase transaction scriptsig,
 * which changes the merkle root, which changes the block header hash.
 * Thus, different extranonces guarantee different hash spaces.
 */
class ExtrannonceManager {
public:
    /**
     * @brief Create extranonce manager
     * 
     * @param start_value Initial extranonce value (default: 1)
     */
    explicit ExtrannonceManager(uint64_t start_value = 1);
    
    ~ExtrannonceManager() = default;
    
    // Disable copying
    ExtrannonceManager(const ExtrannonceManager&) = delete;
    ExtrannonceManager& operator=(const ExtrannonceManager&) = delete;
    
    /**
     * @brief Assign a unique extranonce to a new connection
     * 
     * Each call returns a new unique extranonce value.
     * The value is associated with the given connection ID.
     * 
     * @param connection_id Unique identifier for the ASIC connection
     * @return uint64_t The assigned extranonce value
     */
    [[nodiscard]] uint64_t assign_extranonce(uint32_t connection_id);
    
    /**
     * @brief Release extranonce when connection closes
     * 
     * Removes the association between connection and extranonce.
     * Note: The extranonce value is NOT reused to prevent any chance
     * of duplicate work if connections rapidly reconnect.
     * 
     * @param connection_id Connection to release
     */
    void release_extranonce(uint32_t connection_id);
    
    /**
     * @brief Get extranonce for a connection
     * 
     * @param connection_id Connection to look up
     * @return std::optional<uint64_t> Extranonce or nullopt if not found
     */
    [[nodiscard]] std::optional<uint64_t> get_extranonce(uint32_t connection_id) const;
    
    /**
     * @brief Check if a connection has an assigned extranonce
     * 
     * @param connection_id Connection to check
     * @return bool true if connection has an extranonce
     */
    [[nodiscard]] bool has_extranonce(uint32_t connection_id) const;
    
    /**
     * @brief Get total number of active connections
     * 
     * @return std::size_t Number of connections with assigned extranonces
     */
    [[nodiscard]] std::size_t active_count() const;
    
    /**
     * @brief Get the next extranonce that will be assigned
     * 
     * @return uint64_t Next extranonce value
     */
    [[nodiscard]] uint64_t peek_next_extranonce() const noexcept;
    
    /**
     * @brief Get all active connection IDs
     * 
     * @return std::vector<uint32_t> List of connection IDs
     */
    [[nodiscard]] std::vector<uint32_t> get_active_connections() const;

private:
    /// @brief Next extranonce value to assign
    std::atomic<uint64_t> next_extranonce_;
    
    /// @brief Map of connection_id -> extranonce
    std::unordered_map<uint32_t, uint64_t> connection_extranonces_;
    
    /// @brief Mutex for thread-safe access to map
    mutable std::mutex mutex_;
};

} // namespace quaxis::mining
