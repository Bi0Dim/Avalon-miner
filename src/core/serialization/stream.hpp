/**
 * @file stream.hpp
 * @brief Потоки чтения/записи для сериализации
 * 
 * Предоставляет классы для чтения и записи бинарных данных
 * в формате Bitcoin (little-endian).
 */

#pragma once

#include "../types.hpp"

#include <span>
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

namespace quaxis::core::serialization {

/**
 * @brief Исключение при ошибке чтения/записи
 */
class StreamError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Поток для чтения бинарных данных
 */
class ReadStream {
public:
    /**
     * @brief Создать поток из span данных
     */
    explicit ReadStream(ByteSpan data) noexcept
        : data_(data), pos_(0) {}
    
    /**
     * @brief Создать поток из вектора
     */
    explicit ReadStream(const Bytes& data) noexcept
        : data_(data), pos_(0) {}
    
    // =========================================================================
    // Чтение примитивов
    // =========================================================================
    
    /**
     * @brief Прочитать 1 байт
     */
    [[nodiscard]] uint8_t read_u8();
    
    /**
     * @brief Прочитать 2 байта (little-endian)
     */
    [[nodiscard]] uint16_t read_u16_le();
    
    /**
     * @brief Прочитать 4 байта (little-endian)
     */
    [[nodiscard]] uint32_t read_u32_le();
    
    /**
     * @brief Прочитать 8 байт (little-endian)
     */
    [[nodiscard]] uint64_t read_u64_le();
    
    /**
     * @brief Прочитать знаковое 4-байтное число
     */
    [[nodiscard]] int32_t read_i32_le();
    
    /**
     * @brief Прочитать знаковое 8-байтное число
     */
    [[nodiscard]] int64_t read_i64_le();
    
    /**
     * @brief Прочитать VarInt (Bitcoin формат)
     */
    [[nodiscard]] uint64_t read_varint();
    
    /**
     * @brief Прочитать массив байт фиксированной длины
     */
    [[nodiscard]] Bytes read_bytes(std::size_t count);
    
    /**
     * @brief Прочитать Hash256
     */
    [[nodiscard]] Hash256 read_hash256();
    
    /**
     * @brief Прочитать строку с префиксом длины (VarInt)
     */
    [[nodiscard]] std::string read_string();
    
    // =========================================================================
    // Состояние
    // =========================================================================
    
    /**
     * @brief Оставшееся количество байт
     */
    [[nodiscard]] std::size_t remaining() const noexcept;
    
    /**
     * @brief Текущая позиция
     */
    [[nodiscard]] std::size_t position() const noexcept;
    
    /**
     * @brief Проверить, достигнут ли конец
     */
    [[nodiscard]] bool eof() const noexcept;
    
    /**
     * @brief Пропустить N байт
     */
    void skip(std::size_t count);
    
private:
    void ensure_available(std::size_t count);
    
    ByteSpan data_;
    std::size_t pos_;
};

/**
 * @brief Поток для записи бинарных данных
 */
class WriteStream {
public:
    /**
     * @brief Создать пустой поток
     */
    WriteStream() = default;
    
    /**
     * @brief Создать поток с предварительно выделенной памятью
     */
    explicit WriteStream(std::size_t reserve_size);
    
    // =========================================================================
    // Запись примитивов
    // =========================================================================
    
    /**
     * @brief Записать 1 байт
     */
    void write_u8(uint8_t value);
    
    /**
     * @brief Записать 2 байта (little-endian)
     */
    void write_u16_le(uint16_t value);
    
    /**
     * @brief Записать 4 байта (little-endian)
     */
    void write_u32_le(uint32_t value);
    
    /**
     * @brief Записать 8 байт (little-endian)
     */
    void write_u64_le(uint64_t value);
    
    /**
     * @brief Записать знаковое 4-байтное число
     */
    void write_i32_le(int32_t value);
    
    /**
     * @brief Записать знаковое 8-байтное число
     */
    void write_i64_le(int64_t value);
    
    /**
     * @brief Записать VarInt (Bitcoin формат)
     */
    void write_varint(uint64_t value);
    
    /**
     * @brief Записать массив байт
     */
    void write_bytes(ByteSpan data);
    
    /**
     * @brief Записать Hash256
     */
    void write_hash256(const Hash256& hash);
    
    /**
     * @brief Записать строку с префиксом длины (VarInt)
     */
    void write_string(std::string_view str);
    
    // =========================================================================
    // Результат
    // =========================================================================
    
    /**
     * @brief Получить записанные данные
     */
    [[nodiscard]] const Bytes& data() const noexcept;
    
    /**
     * @brief Получить записанные данные (перемещение)
     */
    [[nodiscard]] Bytes take_data() noexcept;
    
    /**
     * @brief Текущий размер
     */
    [[nodiscard]] std::size_t size() const noexcept;
    
    /**
     * @brief Очистить поток
     */
    void clear();
    
private:
    Bytes data_;
};

// =============================================================================
// VarInt утилиты
// =============================================================================

/**
 * @brief Размер VarInt для данного значения
 */
[[nodiscard]] inline std::size_t varint_size(uint64_t value) noexcept {
    if (value < 0xFD) return 1;
    if (value <= 0xFFFF) return 3;
    if (value <= 0xFFFFFFFF) return 5;
    return 9;
}

} // namespace quaxis::core::serialization
