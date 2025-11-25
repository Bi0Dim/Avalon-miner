/**
 * @file block.hpp
 * @brief Работа с Bitcoin блоками
 * 
 * Структуры и функции для создания и сериализации блоков.
 * 
 * Структура заголовка блока (80 байт):
 * - version (4 байта): версия блока
 * - prev_block (32 байта): хеш предыдущего блока
 * - merkle_root (32 байта): корень Merkle дерева транзакций
 * - timestamp (4 байта): время создания блока (Unix timestamp)
 * - bits (4 байта): compact target
 * - nonce (4 байта): nonce для PoW
 */

#pragma once

#include "../core/types.hpp"
#include "../core/constants.hpp"
#include "../crypto/sha256.hpp"

#include <array>
#include <cstdint>

namespace quaxis::bitcoin {

// =============================================================================
// Структура заголовка блока
// =============================================================================

/**
 * @brief Заголовок Bitcoin блока
 * 
 * Содержит все поля 80-байтного заголовка блока.
 * Используется для построения и хеширования блоков.
 */
struct BlockHeader {
    /// @brief Версия блока (0x20000000 для version bits)
    uint32_t version = constants::BLOCK_VERSION;
    
    /// @brief Хеш предыдущего блока (little-endian)
    Hash256 prev_block{};
    
    /// @brief Корень Merkle дерева транзакций
    Hash256 merkle_root{};
    
    /// @brief Unix timestamp
    uint32_t timestamp = 0;
    
    /// @brief Compact target (сложность)
    uint32_t bits = 0;
    
    /// @brief Nonce для Proof of Work
    uint32_t nonce = 0;
    
    /**
     * @brief Сериализовать заголовок в 80 байт
     * 
     * @return std::array<uint8_t, 80> Сериализованный заголовок
     */
    [[nodiscard]] std::array<uint8_t, constants::BLOCK_HEADER_SIZE> serialize() const noexcept;
    
    /**
     * @brief Вычислить хеш заголовка (SHA256d)
     * 
     * Block hash = SHA256(SHA256(header))
     * 
     * @return Hash256 Хеш блока
     */
    [[nodiscard]] Hash256 hash() const noexcept;
    
    /**
     * @brief Вычислить midstate для первых 64 байт
     * 
     * Midstate используется для оптимизации майнинга:
     * первые 64 байта заголовка не меняются при изменении nonce.
     * 
     * @return crypto::Sha256State Состояние SHA256 после первых 64 байт
     */
    [[nodiscard]] crypto::Sha256State compute_midstate() const noexcept;
    
    /**
     * @brief Получить последние 16 байт заголовка (хвост)
     * 
     * Хвост содержит: merkle_root[28:32] + timestamp + bits + nonce
     * Это часть, которая меняется при изменении nonce.
     * 
     * @return std::array<uint8_t, 16> Последние 16 байт
     */
    [[nodiscard]] std::array<uint8_t, 16> get_tail() const noexcept;
    
    /**
     * @brief Десериализовать заголовок из 80 байт
     * 
     * @param data 80 байт сериализованного заголовка
     * @return Result<BlockHeader> Заголовок или ошибка
     */
    [[nodiscard]] static Result<BlockHeader> deserialize(ByteSpan data);
};

// =============================================================================
// Шаблон блока для майнинга
// =============================================================================

/**
 * @brief Шаблон блока для майнинга
 * 
 * Содержит все данные, необходимые для создания заданий майнинга:
 * - Заголовок блока
 * - Предвычисленный midstate
 * - Coinbase транзакция
 * - Target
 */
struct BlockTemplate {
    /// @brief Высота блока
    uint32_t height = 0;
    
    /// @brief Заголовок блока
    BlockHeader header;
    
    /// @brief Предвычисленный midstate заголовка
    crypto::Sha256State header_midstate{};
    
    /// @brief Coinbase транзакция (сериализованная)
    Bytes coinbase_tx;
    
    /// @brief Midstate coinbase (первые 64 байта)
    crypto::Sha256State coinbase_midstate{};
    
    /// @brief Target в 256-битном формате
    Hash256 target{};
    
    /// @brief Награда за блок в satoshi
    int64_t coinbase_value = 0;
    
    /// @brief Это speculative (spy mining) шаблон?
    bool is_speculative = false;
    
    /**
     * @brief Обновить midstate после изменения extranonce
     * 
     * Пересчитывает merkle_root и header_midstate.
     * 
     * @param extranonce Новое значение extranonce (6 байт)
     */
    void update_extranonce(uint64_t extranonce) noexcept;
    
    /**
     * @brief Создать задание для ASIC
     * 
     * @param job_id ID задания
     * @return std::array<uint8_t, 48> Задание в бинарном формате
     */
    [[nodiscard]] std::array<uint8_t, constants::JOB_MESSAGE_SIZE> create_job(
        uint32_t job_id
    ) const noexcept;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Вычислить Merkle root для списка транзакций
 * 
 * Для пустого блока (только coinbase):
 * merkle_root = SHA256d(coinbase_txid)
 * 
 * @param txids Список txid транзакций
 * @return Hash256 Merkle root
 */
[[nodiscard]] Hash256 compute_merkle_root(const std::vector<Hash256>& txids) noexcept;

/**
 * @brief Вычислить Merkle root для одной транзакции (coinbase only)
 * 
 * @param coinbase_txid TXID coinbase транзакции
 * @return Hash256 Merkle root
 */
[[nodiscard]] Hash256 compute_merkle_root_single(const Hash256& coinbase_txid) noexcept;

} // namespace quaxis::bitcoin
