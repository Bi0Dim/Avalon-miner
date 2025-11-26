/**
 * @file template_cache.hpp
 * @brief Кеш предвычисленных шаблонов блоков
 * 
 * Оптимизация: предвычисляем шаблон блока N+1 пока майним блок N.
 * Это позволяет мгновенно переключиться на новый блок при появлении.
 */

#pragma once

#include "../bitcoin/block.hpp"
#include "../bitcoin/coinbase.hpp"
#include "../core/config.hpp"

#include <memory>
#include <optional>
#include <mutex>

namespace quaxis::mining {

/**
 * @brief Кеш предвычисленных шаблонов блоков
 * 
 * Хранит:
 * - Текущий активный шаблон
 * - Предвычисленный шаблон следующего блока
 * - Историю последних N блоков (для orphan detection)
 */
class TemplateCache {
public:
    /**
     * @brief Создать кеш
     * 
     * @param config Конфигурация майнинга
     * @param coinbase_builder Построитель coinbase
     */
    explicit TemplateCache(
        const MiningConfig& config,
        bitcoin::CoinbaseBuilder coinbase_builder
    );
    
    ~TemplateCache();
    
    // Запрещаем копирование
    TemplateCache(const TemplateCache&) = delete;
    TemplateCache& operator=(const TemplateCache&) = delete;
    
    /**
     * @brief Обновить шаблон для нового блока
     * 
     * @param prev_hash Хеш предыдущего блока
     * @param height Высота нового блока
     * @param bits Compact target
     * @param timestamp Timestamp блока
     * @param coinbase_value Награда за блок
     * @return bitcoin::BlockTemplate& Ссылка на обновлённый шаблон
     */
    bitcoin::BlockTemplate& update_template(
        const Hash256& prev_hash,
        uint32_t height,
        uint32_t bits,
        uint32_t timestamp,
        int64_t coinbase_value
    );
    
    /**
     * @brief Получить текущий активный шаблон
     * 
     * @return std::optional<bitcoin::BlockTemplate> Шаблон или nullopt
     */
    [[nodiscard]] std::optional<bitcoin::BlockTemplate> get_current() const;
    
    /**
     * @brief Получить предвычисленный шаблон
     * 
     * @return std::optional<bitcoin::BlockTemplate> Шаблон или nullopt
     */
    [[nodiscard]] std::optional<bitcoin::BlockTemplate> get_precomputed() const;
    
    /**
     * @brief Предвычислить шаблон следующего блока
     * 
     * Вызывается в фоне для подготовки к следующему блоку.
     * 
     * @param estimated_next_height Предполагаемая высота
     * @param estimated_bits Предполагаемый bits (обычно тот же)
     */
    void precompute_next(
        uint32_t estimated_next_height,
        uint32_t estimated_bits
    );
    
    /**
     * @brief Активировать предвычисленный шаблон
     * 
     * Переключает предвычисленный шаблон в активный.
     * 
     * @return true если успешно активирован
     */
    bool activate_precomputed();
    
    /**
     * @brief Очистить кеш
     */
    void clear();
    
    /**
     * @brief Получить текущую высоту блока
     */
    [[nodiscard]] uint32_t current_height() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::mining
