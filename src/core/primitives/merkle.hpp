/**
 * @file merkle.hpp
 * @brief Merkle tree функции для Bitcoin
 * 
 * Предоставляет функции для построения и проверки Merkle деревьев,
 * используемых в Bitcoin для транзакций и AuxPoW.
 */

#pragma once

#include "../types.hpp"
#include "uint256.hpp"

#include <vector>
#include <span>

namespace quaxis::core {

/**
 * @brief Merkle branch для доказательства включения
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
     * @brief Сериализовать branch
     * 
     * @return Bytes Сериализованные данные
     */
    [[nodiscard]] Bytes serialize() const;
    
    /**
     * @brief Десериализовать branch
     * 
     * @param data Сериализованные данные
     * @return MerkleBranch Десериализованный branch
     */
    [[nodiscard]] static MerkleBranch deserialize(ByteSpan data);
};

/**
 * @brief Merkle tree
 * 
 * Полное бинарное дерево хешей.
 */
class MerkleTree {
public:
    /**
     * @brief Построить Merkle tree из списка хешей
     * 
     * @param leaves Листья дерева (хеши транзакций или элементов)
     */
    explicit MerkleTree(std::vector<Hash256> leaves);
    
    /**
     * @brief Получить корень дерева
     * 
     * @return const Hash256& Merkle root
     */
    [[nodiscard]] const Hash256& root() const noexcept;
    
    /**
     * @brief Получить branch для элемента по индексу
     * 
     * @param index Индекс листа
     * @return MerkleBranch Branch для доказательства
     */
    [[nodiscard]] MerkleBranch get_branch(std::size_t index) const;
    
    /**
     * @brief Получить количество листьев
     */
    [[nodiscard]] std::size_t leaf_count() const noexcept;
    
    /**
     * @brief Получить глубину дерева
     */
    [[nodiscard]] std::size_t depth() const noexcept;
    
    /**
     * @brief Получить все узлы дерева
     */
    [[nodiscard]] const std::vector<Hash256>& nodes() const noexcept;
    
private:
    std::vector<Hash256> nodes_;
    std::size_t leaf_count_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Вычислить Merkle root из списка хешей
 * 
 * @param leaves Листья дерева
 * @return Hash256 Корень дерева
 */
[[nodiscard]] Hash256 compute_merkle_root(std::vector<Hash256> leaves) noexcept;

/**
 * @brief Объединить два хеша для Merkle дерева
 * 
 * @param left Левый хеш
 * @param right Правый хеш
 * @return Hash256 SHA256d(left || right)
 */
[[nodiscard]] Hash256 merkle_hash(
    const Hash256& left,
    const Hash256& right
) noexcept;

/**
 * @brief Вычислить Merkle root для coinbase и транзакций
 * 
 * Bitcoin использует специальное правило: при нечётном количестве
 * элементов последний дублируется.
 * 
 * @param txids Transaction IDs
 * @return Hash256 Merkle root
 */
[[nodiscard]] Hash256 compute_witness_merkle_root(
    const std::vector<Hash256>& wtxids
) noexcept;

} // namespace quaxis::core
