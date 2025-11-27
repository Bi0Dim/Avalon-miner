/**
 * @file auxpow.hpp
 * @brief Структуры и функции для Auxiliary Proof of Work (AuxPoW)
 * 
 * AuxPoW позволяет использовать proof-of-work Bitcoin для защиты
 * дополнительных блокчейнов без потери хешрейта.
 * 
 * Структура AuxPoW:
 * - Coinbase транзакция родительского блока (содержит aux commitment)
 * - Merkle branch от coinbase к merkle root родительского блока
 * - Заголовок родительского блока
 * 
 * Merkle Tree для AuxPoW:
 * - Aux chains размещаются в Merkle tree по slot ID
 * - Root этого дерева помещается в coinbase родительского блока
 * - Позволяет доказать включение в родительский блок
 */

#pragma once

#include "../core/types.hpp"
#include "../crypto/sha256.hpp"

#include <vector>
#include <optional>
#include <cstdint>

namespace quaxis::merged {

// =============================================================================
// Константы AuxPoW
// =============================================================================

/// @brief Магические байты AuxPoW commitment в coinbase
constexpr std::array<uint8_t, 4> AUXPOW_MAGIC = {0xfa, 0xbe, 0x6d, 0x6d};

/// @brief Максимальное количество auxiliary chains
constexpr std::size_t MAX_AUX_CHAINS = 8;

/// @brief Максимальная глубина Merkle дерева AuxPoW
constexpr std::size_t MAX_MERKLE_DEPTH = 16;

// =============================================================================
// Merkle Branch
// =============================================================================

/**
 * @brief Merkle branch для доказательства включения транзакции/элемента
 * 
 * Содержит хеши соседних узлов для проверки пути от листа к корню.
 */
struct MerkleBranch {
    /// @brief Хеши соседних узлов на пути к корню
    std::vector<Hash256> hashes;
    
    /// @brief Индекс позиции (битовая маска: 0=лево, 1=право)
    uint32_t index{0};
    
    /**
     * @brief Вычислить корень Merkle дерева
     * 
     * @param leaf_hash Хеш листа (начальный элемент)
     * @return Hash256 Корень Merkle дерева
     */
    [[nodiscard]] Hash256 compute_root(const Hash256& leaf_hash) const noexcept;
    
    /**
     * @brief Проверить корректность branch
     * 
     * @param leaf_hash Хеш листа
     * @param expected_root Ожидаемый корень
     * @return true если branch корректен
     */
    [[nodiscard]] bool verify(
        const Hash256& leaf_hash,
        const Hash256& expected_root
    ) const noexcept;
    
    /**
     * @brief Сериализовать branch в bytes
     * 
     * @return Bytes Сериализованный branch
     */
    [[nodiscard]] Bytes serialize() const;
    
    /**
     * @brief Десериализовать branch из bytes
     * 
     * @param data Сериализованные данные
     * @return Result<MerkleBranch> Branch или ошибка
     */
    [[nodiscard]] static Result<MerkleBranch> deserialize(ByteSpan data);
};

// =============================================================================
// AuxPoW структура
// =============================================================================

/**
 * @brief Auxiliary Proof of Work
 * 
 * Содержит все данные для доказательства, что auxiliary chain блок
 * был включён в родительский Bitcoin блок.
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
    std::array<uint8_t, 80> parent_header{};
    
    /**
     * @brief Проверить валидность AuxPoW для данного aux chain hash
     * 
     * @param aux_hash Хеш блока auxiliary chain
     * @return true если AuxPoW валиден
     */
    [[nodiscard]] bool verify(const Hash256& aux_hash) const noexcept;
    
    /**
     * @brief Получить хеш родительского заголовка
     * 
     * @return Hash256 SHA256d хеш заголовка
     */
    [[nodiscard]] Hash256 get_parent_hash() const noexcept;
    
    /**
     * @brief Сериализовать AuxPoW
     * 
     * @return Bytes Сериализованный AuxPoW
     */
    [[nodiscard]] Bytes serialize() const;
    
    /**
     * @brief Десериализовать AuxPoW
     * 
     * @param data Сериализованные данные
     * @return Result<AuxPow> AuxPoW или ошибка
     */
    [[nodiscard]] static Result<AuxPow> deserialize(ByteSpan data);
};

// =============================================================================
// AuxPoW Commitment
// =============================================================================

/**
 * @brief Commitment для auxiliary chains в coinbase
 * 
 * Формат: AUXPOW_MAGIC || aux_merkle_root || merkle_tree_size || merkle_nonce
 */
struct AuxCommitment {
    /// @brief Merkle root всех aux chain hashes
    Hash256 aux_merkle_root{};
    
    /// @brief Размер Merkle дерева (степень двойки)
    uint32_t tree_size{1};
    
    /// @brief Nonce для вычисления slot ID
    uint32_t merkle_nonce{0};
    
    /**
     * @brief Сериализовать commitment для включения в coinbase
     * 
     * @return std::array<uint8_t, 44> Сериализованный commitment
     */
    [[nodiscard]] std::array<uint8_t, 44> serialize() const noexcept;
    
    /**
     * @brief Найти commitment в coinbase данных
     * 
     * @param coinbase_data Данные coinbase транзакции
     * @return std::optional<AuxCommitment> Commitment или nullopt
     */
    [[nodiscard]] static std::optional<AuxCommitment> find_in_coinbase(
        ByteSpan coinbase_data
    ) noexcept;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Построить Merkle tree из списка хешей
 * 
 * @param leaves Листья дерева (хеши)
 * @return std::vector<Hash256> Все узлы дерева (включая корень)
 */
[[nodiscard]] std::vector<Hash256> build_merkle_tree(
    const std::vector<Hash256>& leaves
) noexcept;

/**
 * @brief Получить Merkle branch для заданного индекса
 * 
 * @param tree Merkle tree (все узлы)
 * @param index Индекс листа
 * @return MerkleBranch Branch для доказательства
 */
[[nodiscard]] MerkleBranch get_merkle_branch(
    const std::vector<Hash256>& tree,
    std::size_t index
) noexcept;

/**
 * @brief Вычислить slot ID для chain
 * 
 * Slot ID определяет позицию chain в AuxPoW Merkle tree.
 * slot_id = chain_id % tree_size
 * 
 * @param chain_id Идентификатор chain (обычно хеш genesis block)
 * @param nonce Merkle nonce
 * @param tree_size Размер дерева
 * @return uint32_t Slot ID
 */
[[nodiscard]] uint32_t compute_slot_id(
    const Hash256& chain_id,
    uint32_t nonce,
    uint32_t tree_size
) noexcept;

/**
 * @brief Создать commitment для списка auxiliary chains
 * 
 * @param aux_hashes Хеши блоков auxiliary chains
 * @param chain_ids Идентификаторы chains
 * @return AuxCommitment Commitment для включения в coinbase
 */
[[nodiscard]] AuxCommitment create_aux_commitment(
    const std::vector<Hash256>& aux_hashes,
    const std::vector<Hash256>& chain_ids
) noexcept;

/**
 * @brief Проверить что хеш меньше target
 * 
 * @param hash Хеш для проверки
 * @param target Target (compact bits формат)
 * @return true если hash < target
 */
[[nodiscard]] bool meets_target(
    const Hash256& hash,
    uint32_t target_bits
) noexcept;

/**
 * @brief Конвертировать target bits в 256-bit target
 * 
 * @param bits Compact target representation
 * @return Hash256 256-bit target
 */
[[nodiscard]] Hash256 bits_to_target(uint32_t bits) noexcept;

} // namespace quaxis::merged
