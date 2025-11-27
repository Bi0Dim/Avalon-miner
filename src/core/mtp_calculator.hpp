/**
 * @file mtp_calculator.hpp
 * @brief Вычисление Median Time Past (MTP)
 * 
 * Вычисляет MTP по последним 11 блокам для определения
 * минимально допустимого timestamp нового блока (MTP + 1).
 */

#pragma once

#include "types.hpp"
#include "primitives/block_header.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>

namespace quaxis::core {

/**
 * @brief Количество блоков для вычисления MTP
 */
constexpr std::size_t MTP_BLOCK_COUNT = 11;

/**
 * @brief Калькулятор Median Time Past
 * 
 * Хранит timestamps последних 11 блоков и вычисляет медиану.
 * Thread-safe.
 */
class MtpCalculator {
public:
    MtpCalculator() = default;
    
    /**
     * @brief Добавить timestamp нового блока
     * 
     * @param timestamp Unix timestamp блока
     */
    void push_timestamp(uint32_t timestamp);
    
    /**
     * @brief Добавить заголовок блока
     * 
     * @param header Заголовок блока
     */
    void push_header(const BlockHeader& header);
    
    /**
     * @brief Сбросить все данные
     */
    void reset();
    
    /**
     * @brief Вычислить MTP
     * 
     * @return uint32_t MTP или 0 если недостаточно данных
     */
    [[nodiscard]] uint32_t get_mtp() const;
    
    /**
     * @brief Получить минимально допустимый timestamp (MTP + 1)
     * 
     * @return uint32_t MTP + 1 или текущее время если недостаточно данных
     */
    [[nodiscard]] uint32_t get_min_timestamp() const;
    
    /**
     * @brief Проверить, достаточно ли данных для вычисления MTP
     * 
     * @return bool true если есть минимум 11 timestamps
     */
    [[nodiscard]] bool has_sufficient_data() const;
    
    /**
     * @brief Получить количество сохранённых timestamps
     */
    [[nodiscard]] std::size_t count() const;

private:
    mutable std::mutex mutex_;
    std::array<uint32_t, MTP_BLOCK_COUNT> timestamps_{};
    std::size_t count_{0};
    std::size_t head_{0};  // Позиция для следующей записи (кольцевой буфер)
};

} // namespace quaxis::core
