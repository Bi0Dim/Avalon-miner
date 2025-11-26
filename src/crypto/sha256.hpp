/**
 * @file sha256.hpp
 * @brief SHA256 интерфейс с поддержкой SHA-NI
 * 
 * Предоставляет высокопроизводительную реализацию SHA256 с:
 * - Автоматическим выбором оптимальной реализации (SHA-NI или generic)
 * - Поддержкой midstate для оптимизации майнинга
 * - Double SHA256 (SHA256d) для Bitcoin
 * 
 * Оптимизации для майнинга:
 * 1. SHA-NI инструкции дают ~3-5x ускорение на современных CPU
 * 2. Midstate позволяет вычислять только вторую половину хеша при изменении nonce
 * 3. Предвычисление coinbase midstate экономит время при смене extranonce
 * 
 * @note Латентность SHA256 с SHA-NI: ~50-100 наносекунд на 64 байта
 */

#pragma once

#include "../core/types.hpp"
#include "../core/constants.hpp"

#include <array>
#include <span>
#include <cstdint>

namespace quaxis::crypto {

// =============================================================================
// Типы данных
// =============================================================================

/**
 * @brief SHA256 состояние (8 x 32-bit слов)
 * 
 * Это промежуточное состояние после обработки блока данных.
 * Также называется "midstate" в контексте майнинга.
 */
using Sha256State = std::array<uint32_t, 8>;

/**
 * @brief SHA256 midstate в байтовом представлении (32 байта)
 * 
 * Используется для передачи на ASIC в бинарном протоколе.
 */
using Sha256Midstate = std::array<uint8_t, constants::SHA256_MIDSTATE_SIZE>;

// =============================================================================
// Основные функции SHA256
// =============================================================================

/**
 * @brief Вычислить SHA256 хеш данных произвольной длины
 * 
 * Полная реализация SHA256 согласно FIPS 180-4.
 * 
 * @param data Входные данные для хеширования
 * @return Hash256 32-байтный хеш
 * 
 * @note Автоматически выбирает оптимальную реализацию (SHA-NI или generic)
 */
[[nodiscard]] Hash256 sha256(ByteSpan data) noexcept;

/**
 * @brief Вычислить SHA256d (двойной SHA256) хеш
 * 
 * SHA256d = SHA256(SHA256(data))
 * 
 * Используется в Bitcoin для:
 * - Block hash
 * - Transaction ID (txid)
 * - Merkle tree
 * 
 * @param data Входные данные для хеширования
 * @return Hash256 32-байтный хеш
 */
[[nodiscard]] Hash256 sha256d(ByteSpan data) noexcept;

// =============================================================================
// Функции для работы с midstate
// =============================================================================

/**
 * @brief Вычислить SHA256 transform для одного 64-байтного блока
 * 
 * Это внутренняя функция сжатия SHA256. Обрабатывает ровно 64 байта
 * и обновляет состояние хеша.
 * 
 * @param state Текущее состояние хеша (будет модифицировано)
 * @param block Указатель на 64 байта данных
 * 
 * @warning block должен содержать ровно 64 байта!
 */
void sha256_transform(Sha256State& state, const uint8_t* block) noexcept;

/**
 * @brief Вычислить midstate для первых 64 байт данных
 * 
 * Midstate - это состояние SHA256 после обработки первого 64-байтного блока.
 * Используется для оптимизации майнинга: когда меняется только nonce
 * (последние 4 байта 80-байтного заголовка), первые 64 байта остаются
 * неизменными, и их midstate можно использовать повторно.
 * 
 * @param data Указатель на минимум 64 байта данных
 * @return Sha256State Midstate после обработки первых 64 байт
 * 
 * @note Для заголовка блока: midstate вычисляется один раз при новом блоке,
 *       затем ASIC использует его для всех nonce.
 */
[[nodiscard]] Sha256State compute_midstate(const uint8_t* data) noexcept;

/**
 * @brief Преобразовать Sha256State в байтовый массив (Midstate)
 * 
 * Конвертирует 8 x 32-bit слов в 32-байтный массив в little-endian формате.
 * 
 * @param state Состояние SHA256
 * @return Sha256Midstate Байтовое представление
 */
[[nodiscard]] Sha256Midstate state_to_bytes(const Sha256State& state) noexcept;

/**
 * @brief Преобразовать байтовый массив в Sha256State
 * 
 * Конвертирует 32-байтный массив в 8 x 32-bit слов.
 * 
 * @param bytes Байтовое представление midstate
 * @return Sha256State Состояние SHA256
 */
[[nodiscard]] Sha256State bytes_to_state(const Sha256Midstate& bytes) noexcept;

// =============================================================================
// Специализированные функции для майнинга
// =============================================================================

/**
 * @brief Вычислить хеш заголовка блока с использованием midstate
 * 
 * Оптимизированная функция для майнинга:
 * 1. Берёт готовый midstate (уже вычисленный для первых 64 байт)
 * 2. Дополняет оставшиеся 16 байт + padding
 * 3. Выполняет один SHA256 transform
 * 4. Выполняет второй SHA256 (для double hash)
 * 
 * @param midstate Состояние после первых 64 байт заголовка
 * @param header_tail Последние 16 байт заголовка (merkle[28:32] + time + bits + nonce)
 * @return Hash256 Block hash (double SHA256)
 * 
 * @note Эта функция экономит ~50% вычислений на каждом nonce
 */
[[nodiscard]] Hash256 hash_header_with_midstate(
    const Sha256State& midstate,
    std::span<const uint8_t, 16> header_tail
) noexcept;

/**
 * @brief Проверить, меньше ли хеш заданного target
 * 
 * Быстрая проверка валидности найденного nonce.
 * 
 * @param hash Хеш для проверки
 * @param target Target в 32-байтном формате
 * @return true если hash <= target (блок валиден)
 */
[[nodiscard]] bool check_hash_target(
    const Hash256& hash,
    const Hash256& target
) noexcept;

// =============================================================================
// Выбор реализации
// =============================================================================

/**
 * @brief Перечисление доступных реализаций SHA256
 */
enum class Sha256Implementation {
    Generic,    ///< Программная реализация
    ShaNi       ///< Аппаратная реализация (Intel SHA-NI)
};

/**
 * @brief Получить текущую используемую реализацию
 * 
 * @return Sha256Implementation Тип реализации
 */
[[nodiscard]] Sha256Implementation get_sha256_implementation() noexcept;

/**
 * @brief Проверить поддержку SHA-NI на текущем CPU
 * 
 * @return true если SHA-NI поддерживается
 */
[[nodiscard]] bool has_sha_ni_support() noexcept;

/**
 * @brief Получить строковое название реализации
 * 
 * @return std::string_view Название ("generic" или "sha-ni")
 */
[[nodiscard]] std::string_view get_implementation_name() noexcept;

} // namespace quaxis::crypto
