/**
 * @file auxpow.hpp
 * @brief Структура Auxiliary Proof of Work для core модуля
 * 
 * Определяет универсальную структуру AuxPoW, используемую всеми
 * merged mining монетами.
 */

#pragma once

#include "../types.hpp"
#include "block_header.hpp"
#include "merkle.hpp"

#include <array>
#include <optional>

namespace quaxis::core {

/// @brief Магические байты AuxPoW в coinbase
inline constexpr std::array<uint8_t, 4> AUXPOW_MAGIC = {0xfa, 0xbe, 0x6d, 0x6d};

/// @brief Максимальная глубина Merkle дерева AuxPoW
inline constexpr std::size_t MAX_AUXPOW_MERKLE_DEPTH = 20;

/**
 * @brief Commitment для AuxPoW в coinbase
 * 
 * Формат в coinbase:
 * - magic (4 байта): 0xfabe6d6d
 * - aux_merkle_root (32 байта): корень Merkle дерева aux chains
 * - tree_size (4 байта): размер дерева
 * - merkle_nonce (4 байта): nonce для slot ID
 */
struct AuxPowCommitment {
    /// @brief Корень Merkle дерева aux chain хешей
    Hash256 aux_merkle_root{};
    
    /// @brief Размер Merkle дерева (степень двойки)
    uint32_t tree_size{1};
    
    /// @brief Nonce для вычисления slot ID
    uint32_t merkle_nonce{0};
    
    /**
     * @brief Сериализовать commitment для coinbase
     * 
     * @return std::array<uint8_t, 44> Сериализованный commitment
     */
    [[nodiscard]] std::array<uint8_t, 44> serialize() const noexcept;
    
    /**
     * @brief Найти commitment в coinbase данных
     * 
     * @param coinbase_data Данные coinbase транзакции
     * @return std::optional<AuxPowCommitment> Commitment или nullopt
     */
    [[nodiscard]] static std::optional<AuxPowCommitment> find_in_coinbase(
        ByteSpan coinbase_data
    ) noexcept;
};

/**
 * @brief Auxiliary Proof of Work
 * 
 * Содержит доказательство того, что aux chain блок был включён
 * в родительский Bitcoin блок.
 * 
 * Структура:
 * 1. coinbase_tx - coinbase транзакция родительского блока
 * 2. coinbase_branch - Merkle branch от coinbase до merkle root
 * 3. aux_branch - Merkle branch от aux hash до aux merkle root
 * 4. parent_header - заголовок родительского блока
 */
struct AuxPow {
    /// @brief Coinbase транзакция родительского блока
    Bytes coinbase_tx;
    
    /// @brief Хеш coinbase транзакции
    Hash256 coinbase_hash{};
    
    /// @brief Merkle branch от coinbase до merkle root родительского блока
    MerkleBranch coinbase_branch;
    
    /// @brief Merkle branch от aux chain hash до aux merkle root (в coinbase)
    MerkleBranch aux_branch;
    
    /// @brief Заголовок родительского блока (80 байт)
    BlockHeader parent_header;
    
    /**
     * @brief Проверить валидность AuxPoW для данного aux chain hash
     * 
     * Проверяет:
     * 1. coinbase_branch ведёт от coinbase_hash к merkle_root в parent_header
     * 2. aux_branch ведёт от aux_hash к aux_merkle_root в coinbase
     * 3. parent_header удовлетворяет своему target
     * 
     * @param aux_hash Хеш блока auxiliary chain
     * @return true если AuxPoW валиден
     */
    [[nodiscard]] bool verify(const Hash256& aux_hash) const noexcept;
    
    /**
     * @brief Проверить только proof-of-work родительского блока
     * 
     * @return true если hash(parent_header) <= target
     */
    [[nodiscard]] bool verify_pow() const noexcept;
    
    /**
     * @brief Проверить, соответствует ли AuxPoW заданному target
     * 
     * @param target_bits Compact target aux chain
     * @return true если hash(parent_header) <= target
     */
    [[nodiscard]] bool meets_target(uint32_t target_bits) const noexcept;
    
    /**
     * @brief Получить хеш родительского заголовка
     * 
     * @return Hash256 SHA256d хеш заголовка
     */
    [[nodiscard]] Hash256 get_parent_hash() const noexcept;
    
    /**
     * @brief Получить chain ID из parent_header
     * 
     * @return uint32_t Chain ID
     */
    [[nodiscard]] uint32_t get_chain_id() const noexcept;
    
    /**
     * @brief Сериализовать AuxPoW
     * 
     * @return Bytes Сериализованные данные
     */
    [[nodiscard]] Bytes serialize() const;
    
    /**
     * @brief Десериализовать AuxPoW
     * 
     * @param data Сериализованные данные
     * @return std::optional<AuxPow> AuxPoW или nullopt при ошибке
     */
    [[nodiscard]] static std::optional<AuxPow> deserialize(ByteSpan data);
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Вычислить slot ID для chain в Merkle дереве
 * 
 * slot_id = (chain_id * merkle_nonce) % tree_size
 * 
 * @param chain_id Идентификатор chain
 * @param nonce Merkle nonce
 * @param tree_size Размер дерева
 * @return uint32_t Slot ID
 */
[[nodiscard]] uint32_t compute_slot_id(
    uint32_t chain_id,
    uint32_t nonce,
    uint32_t tree_size
) noexcept;

/**
 * @brief Вычислить slot ID из genesis hash
 * 
 * @param genesis_hash Хеш genesis блока chain
 * @param nonce Merkle nonce
 * @param tree_size Размер дерева
 * @return uint32_t Slot ID
 */
[[nodiscard]] uint32_t compute_slot_id(
    const Hash256& genesis_hash,
    uint32_t nonce,
    uint32_t tree_size
) noexcept;

/**
 * @brief Создать commitment для списка aux chains
 * 
 * @param aux_hashes Хеши блоков aux chains
 * @param chain_ids Идентификаторы chains
 * @return AuxPowCommitment Commitment для coinbase
 */
[[nodiscard]] AuxPowCommitment create_commitment(
    const std::vector<Hash256>& aux_hashes,
    const std::vector<uint32_t>& chain_ids
) noexcept;

/**
 * @brief Проверить, меньше ли хеш target
 * 
 * @param hash Хеш для проверки
 * @param target_bits Compact target
 * @return true если hash <= target
 */
[[nodiscard]] bool meets_target(
    const Hash256& hash,
    uint32_t target_bits
) noexcept;

} // namespace quaxis::core
