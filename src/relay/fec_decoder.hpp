/**
 * @file fec_decoder.hpp
 * @brief Forward Error Correction (FEC) декодер для FIBRE протокола
 * 
 * Реализует декодирование Reed-Solomon-подобных кодов для восстановления
 * данных при потере UDP пакетов. FIBRE использует FEC для достижения
 * надёжной передачи данных поверх UDP.
 * 
 * Принцип работы:
 * - Блок разбивается на N data chunks
 * - Генерируются M FEC (parity) chunks
 * - Для восстановления нужны любые N из (N+M) чанков
 * - Типичное соотношение: N=100, M=50 (можно потерять до 33% пакетов)
 * 
 * @note Текущая реализация использует простой XOR-based FEC.
 *       Для полноценной реализации рекомендуется библиотека cm256.
 */

#pragma once

#include "../core/types.hpp"

#include <cstdint>
#include <vector>
#include <array>
#include <optional>
#include <span>
#include <memory>
#include <expected>

namespace quaxis::relay {

// =============================================================================
// Константы FEC
// =============================================================================

/// @brief Максимальный размер чанка данных
inline constexpr std::size_t MAX_CHUNK_SIZE = 1400;

/// @brief Максимальное количество data chunks
inline constexpr std::size_t MAX_DATA_CHUNKS = 256;

/// @brief Максимальное количество FEC chunks
inline constexpr std::size_t MAX_FEC_CHUNKS = 128;

// =============================================================================
// Структуры данных
// =============================================================================

/**
 * @brief Чанк данных для FEC кодирования/декодирования
 */
struct FecChunk {
    /// @brief ID чанка (0..N-1 для data, N..N+M-1 для FEC)
    uint16_t chunk_id{0};
    
    /// @brief Это FEC (parity) чанк?
    bool is_fec{false};
    
    /// @brief Данные чанка
    std::vector<uint8_t> data;
    
    /**
     * @brief Размер данных чанка
     */
    [[nodiscard]] std::size_t size() const noexcept { return data.size(); }
    
    /**
     * @brief Чанк пустой?
     */
    [[nodiscard]] bool empty() const noexcept { return data.empty(); }
};

/**
 * @brief Результат FEC декодирования
 */
struct FecDecodeResult {
    /// @brief Восстановленные данные
    std::vector<uint8_t> data;
    
    /// @brief Количество использованных data chunks
    std::size_t data_chunks_used{0};
    
    /// @brief Количество использованных FEC chunks
    std::size_t fec_chunks_used{0};
    
    /// @brief Количество восстановленных чанков через FEC
    std::size_t chunks_recovered{0};
};

/**
 * @brief Параметры FEC кодирования
 */
struct FecParams {
    /// @brief Количество data chunks
    uint16_t data_chunk_count{100};
    
    /// @brief Количество FEC chunks
    uint16_t fec_chunk_count{50};
    
    /// @brief Размер одного чанка
    uint16_t chunk_size{1400};
    
    /**
     * @brief Общее количество чанков
     */
    [[nodiscard]] uint16_t total_chunks() const noexcept {
        return static_cast<uint16_t>(data_chunk_count + fec_chunk_count);
    }
    
    /**
     * @brief Процент избыточности
     */
    [[nodiscard]] double overhead() const noexcept {
        if (data_chunk_count == 0) return 0.0;
        return static_cast<double>(fec_chunk_count) / static_cast<double>(data_chunk_count);
    }
};

// =============================================================================
// Класс FEC декодера
// =============================================================================

/**
 * @brief FEC декодер для восстановления данных
 * 
 * Собирает чанки и пытается восстановить оригинальные данные,
 * используя FEC коды при необходимости.
 * 
 * Thread-safety: класс НЕ является потокобезопасным.
 * Внешняя синхронизация требуется при использовании из нескольких потоков.
 */
class FecDecoder {
public:
    /**
     * @brief Создать декодер с заданными параметрами
     * 
     * @param params Параметры FEC (количество чанков, размер)
     */
    explicit FecDecoder(const FecParams& params);
    
    /**
     * @brief Деструктор
     */
    ~FecDecoder();
    
    // Запрещаем копирование
    FecDecoder(const FecDecoder&) = delete;
    FecDecoder& operator=(const FecDecoder&) = delete;
    
    // Разрешаем перемещение
    FecDecoder(FecDecoder&&) noexcept;
    FecDecoder& operator=(FecDecoder&&) noexcept;
    
    // =========================================================================
    // Добавление чанков
    // =========================================================================
    
    /**
     * @brief Добавить полученный чанк
     * 
     * @param chunk_id ID чанка
     * @param is_fec Это FEC чанк?
     * @param data Данные чанка
     * @return true если чанк успешно добавлен
     */
    bool add_chunk(uint16_t chunk_id, bool is_fec, ByteSpan data);
    
    /**
     * @brief Добавить чанк из структуры
     * 
     * @param chunk Структура чанка
     * @return true если чанк успешно добавлен
     */
    bool add_chunk(const FecChunk& chunk);
    
    // =========================================================================
    // Проверка статуса
    // =========================================================================
    
    /**
     * @brief Можно ли декодировать данные?
     * 
     * Возвращает true если получено достаточно чанков для восстановления.
     * 
     * @return true если декодирование возможно
     */
    [[nodiscard]] bool can_decode() const noexcept;
    
    /**
     * @brief Все ли data чанки получены?
     * 
     * @return true если все data чанки получены (FEC не нужен)
     */
    [[nodiscard]] bool has_all_data_chunks() const noexcept;
    
    /**
     * @brief Количество полученных data чанков
     */
    [[nodiscard]] std::size_t received_data_chunks() const noexcept;
    
    /**
     * @brief Количество полученных FEC чанков
     */
    [[nodiscard]] std::size_t received_fec_chunks() const noexcept;
    
    /**
     * @brief Общее количество полученных чанков
     */
    [[nodiscard]] std::size_t received_total_chunks() const noexcept;
    
    /**
     * @brief Параметры FEC
     */
    [[nodiscard]] const FecParams& params() const noexcept;
    
    // =========================================================================
    // Декодирование
    // =========================================================================
    
    /**
     * @brief Попытаться декодировать данные
     * 
     * @return Результат декодирования или ошибка
     */
    [[nodiscard]] Result<FecDecodeResult> decode();
    
    /**
     * @brief Получить первые N байт данных (если доступны)
     * 
     * Позволяет получить начало данных без полного декодирования.
     * Полезно для раннего извлечения block header.
     * 
     * @param n Количество байт
     * @return Данные или nullopt если недоступны
     */
    [[nodiscard]] std::optional<std::vector<uint8_t>> get_first_n_bytes(std::size_t n) const;
    
    // =========================================================================
    // Управление состоянием
    // =========================================================================
    
    /**
     * @brief Сбросить декодер для нового блока
     */
    void reset();
    
    /**
     * @brief Сбросить с новыми параметрами
     * 
     * @param params Новые параметры FEC
     */
    void reset(const FecParams& params);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief XOR двух массивов байт
 * 
 * Простейшая операция для FEC восстановления.
 * 
 * @param dst Массив-приёмник (будет модифицирован)
 * @param src Массив-источник
 */
void xor_bytes(MutableByteSpan dst, ByteSpan src) noexcept;

/**
 * @brief Вычислить XOR нескольких чанков
 * 
 * @param chunks Список чанков
 * @param chunk_size Размер каждого чанка
 * @return Результат XOR всех чанков
 */
[[nodiscard]] std::vector<uint8_t> xor_chunks(
    const std::vector<ByteSpan>& chunks,
    std::size_t chunk_size
);

} // namespace quaxis::relay
