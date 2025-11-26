/**
 * @file share_validator.hpp
 * @brief Валидатор шар от ASIC
 * 
 * Проверяет shares (найденные nonce) от ASIC майнеров:
 * 1. Проверка job_id (задание существует и не устарело)
 * 2. Вычисление хеша с найденным nonce
 * 3. Проверка соответствия target
 * 4. Детекция дубликатов
 */

#pragma once

#include "job.hpp"
#include "job_manager.hpp"
#include "../bitcoin/block.hpp"

#include <memory>
#include <functional>

namespace quaxis::mining {

/**
 * @brief Результат валидации с деталями
 */
struct ValidationResult {
    ShareResult result;
    Hash256 hash{};          ///< Вычисленный хеш (если валидация прошла)
    uint32_t job_id = 0;
    uint32_t nonce = 0;
    double difficulty = 0.0; ///< Сложность найденного хеша
    
    [[nodiscard]] bool is_valid() const noexcept {
        return result == ShareResult::Valid || result == ShareResult::ValidPartial;
    }
    
    [[nodiscard]] bool is_block() const noexcept {
        return result == ShareResult::Valid;
    }
};

/**
 * @brief Callback при нахождении валидного блока
 */
using ValidBlockCallback = std::function<void(
    const ValidationResult& result,
    const bitcoin::BlockHeader& header
)>;

/**
 * @brief Валидатор шар от ASIC
 */
class ShareValidator {
public:
    /**
     * @brief Создать валидатор
     * 
     * @param job_manager Менеджер заданий
     */
    explicit ShareValidator(JobManager& job_manager);
    
    ~ShareValidator();
    
    // Запрещаем копирование
    ShareValidator(const ShareValidator&) = delete;
    ShareValidator& operator=(const ShareValidator&) = delete;
    
    /**
     * @brief Валидировать share
     * 
     * @param share Share от ASIC
     * @return ValidationResult Результат валидации
     */
    [[nodiscard]] ValidationResult validate(const Share& share);
    
    /**
     * @brief Установить callback для валидных блоков
     */
    void set_valid_block_callback(ValidBlockCallback callback);
    
    /**
     * @brief Установить минимальную сложность для partial shares
     * 
     * @param difficulty Минимальная сложность
     */
    void set_partial_difficulty(double difficulty);
    
    /**
     * @brief Получить количество валидированных shares
     */
    [[nodiscard]] uint64_t total_shares() const;
    
    /**
     * @brief Получить количество найденных блоков
     */
    [[nodiscard]] uint64_t blocks_found() const;
    
    /**
     * @brief Получить количество stale shares
     */
    [[nodiscard]] uint64_t stale_shares() const;
    
    /**
     * @brief Получить количество дубликатов
     */
    [[nodiscard]] uint64_t duplicate_shares() const;
    
    /**
     * @brief Сбросить статистику
     */
    void reset_stats();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::mining
