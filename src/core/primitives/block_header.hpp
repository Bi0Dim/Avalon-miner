/**
 * @file block_header.hpp
 * @brief 80-байтный заголовок блока Bitcoin
 * 
 * Определяет структуру заголовка блока, используемую во всех
 * SHA-256 блокчейнах, совместимых с Bitcoin.
 */

#pragma once

#include "../types.hpp"
#include "uint256.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace quaxis::core {

/// @brief Размер заголовка блока в байтах
inline constexpr std::size_t BLOCK_HEADER_SIZE = 80;

/**
 * @brief Заголовок блока Bitcoin (80 байт)
 * 
 * Структура:
 * - version:     4 байта (int32_t, little-endian)
 * - prev_hash:   32 байта (uint256, little-endian)
 * - merkle_root: 32 байта (uint256, little-endian)
 * - timestamp:   4 байта (uint32_t, little-endian)
 * - bits:        4 байта (uint32_t, little-endian) - compact target
 * - nonce:       4 байта (uint32_t, little-endian)
 */
struct BlockHeader {
    /// @brief Версия блока
    int32_t version{0};
    
    /// @brief Хеш предыдущего блока
    Hash256 prev_hash{};
    
    /// @brief Корень Merkle дерева транзакций
    Hash256 merkle_root{};
    
    /// @brief Временная метка (Unix timestamp)
    uint32_t timestamp{0};
    
    /// @brief Compact target (nBits)
    uint32_t bits{0};
    
    /// @brief Nonce для proof-of-work
    uint32_t nonce{0};
    
    // =========================================================================
    // Сериализация
    // =========================================================================
    
    /**
     * @brief Сериализовать заголовок в 80 байт
     * 
     * @return std::array<uint8_t, 80> Сериализованный заголовок
     */
    [[nodiscard]] std::array<uint8_t, BLOCK_HEADER_SIZE> serialize() const noexcept;
    
    /**
     * @brief Десериализовать заголовок из 80 байт
     * 
     * @param data Указатель на данные (минимум 80 байт)
     * @return BlockHeader Десериализованный заголовок
     */
    [[nodiscard]] static BlockHeader deserialize(const uint8_t* data) noexcept;
    
    /**
     * @brief Десериализовать заголовок из span
     * 
     * @param data Span данных (должен быть минимум 80 байт)
     * @return BlockHeader Десериализованный заголовок
     */
    [[nodiscard]] static BlockHeader deserialize(
        std::span<const uint8_t> data
    ) noexcept;
    
    // =========================================================================
    // Хеширование
    // =========================================================================
    
    /**
     * @brief Вычислить хеш заголовка (double SHA256)
     * 
     * @return Hash256 Хеш блока
     */
    [[nodiscard]] Hash256 hash() const noexcept;
    
    /**
     * @brief Вычислить хеш как uint256
     * 
     * @return uint256 Хеш блока
     */
    [[nodiscard]] uint256 hash_uint256() const noexcept;
    
    // =========================================================================
    // Target и сложность
    // =========================================================================
    
    /**
     * @brief Получить target из compact bits
     * 
     * @return uint256 256-битный target
     */
    [[nodiscard]] uint256 get_target() const noexcept;
    
    /**
     * @brief Проверить, соответствует ли хеш target
     * 
     * @return true если hash <= target
     */
    [[nodiscard]] bool check_pow() const noexcept;
    
    /**
     * @brief Вычислить сложность (относительно Bitcoin genesis)
     * 
     * @return double Сложность
     */
    [[nodiscard]] double get_difficulty() const noexcept;
    
    // =========================================================================
    // AuxPoW
    // =========================================================================
    
    /**
     * @brief Проверить, является ли блок AuxPoW
     * 
     * @return true если блок содержит AuxPoW флаг в версии
     */
    [[nodiscard]] bool is_auxpow() const noexcept;
    
    /**
     * @brief Получить chain ID из версии (для AuxPoW блоков)
     * 
     * @return uint32_t Chain ID или 0
     */
    [[nodiscard]] uint32_t get_chain_id() const noexcept;
    
    // =========================================================================
    // Оптимизации для майнинга
    // =========================================================================
    
    /**
     * @brief Получить первые 64 байта заголовка (для midstate)
     * 
     * @return std::span<const uint8_t, 64> Первые 64 байта
     */
    [[nodiscard]] std::array<uint8_t, 64> get_first_chunk() const noexcept;
    
    /**
     * @brief Получить последние 16 байт заголовка (хвост)
     * 
     * Хвост содержит: последние 4 байта merkle_root + timestamp + bits + nonce
     * 
     * @return std::array<uint8_t, 16> Хвост заголовка
     */
    [[nodiscard]] std::array<uint8_t, 16> get_tail() const noexcept;
};

/**
 * @brief Преобразовать compact bits в 256-битный target
 * 
 * @param bits Compact representation
 * @return uint256 256-битный target
 */
[[nodiscard]] uint256 bits_to_target(uint32_t bits) noexcept;

/**
 * @brief Преобразовать 256-битный target в compact bits
 * 
 * @param target 256-битный target
 * @return uint32_t Compact representation
 */
[[nodiscard]] uint32_t target_to_bits(const uint256& target) noexcept;

/**
 * @brief Вычислить сложность из compact bits
 * 
 * @param bits Compact target
 * @return double Сложность
 */
[[nodiscard]] double bits_to_difficulty(uint32_t bits) noexcept;

} // namespace quaxis::core
