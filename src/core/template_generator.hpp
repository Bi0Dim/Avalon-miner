/**
 * @file template_generator.hpp
 * @brief Генератор шаблонов блоков
 * 
 * Генерирует шаблоны блоков внутри приложения без использования
 * внешнего Bitcoin Core RPC. Поддерживает пустые блоки (только coinbase).
 */

#pragma once

#include "types.hpp"
#include "primitives/block_header.hpp"
#include "mtp_calculator.hpp"

#include <memory>
#include <optional>
#include <string>

namespace quaxis::core {

/**
 * @brief Шаблон блока для майнинга
 */
struct BlockTemplate {
    /// @brief Заголовок блока
    BlockHeader header;
    
    /// @brief Высота блока
    uint32_t height{0};
    
    /// @brief Target в compact формате (nBits)
    uint32_t bits{0};
    
    /// @brief Награда за блок (satoshi)
    int64_t coinbase_value{0};
    
    /// @brief Это speculative (spy mining)?
    bool is_speculative{false};
    
    /// @brief Midstate первых 64 байт coinbase
    std::array<uint8_t, 32> coinbase_midstate{};
    
    /// @brief Midstate первых 64 байт заголовка
    std::array<uint8_t, 32> header_midstate{};
};

/**
 * @brief Конфигурация генератора шаблонов
 */
struct TemplateGeneratorConfig {
    /// @brief Тег в coinbase (максимум 20 байт)
    std::string coinbase_tag = "quaxis";
    
    /// @brief Адрес для выплаты награды
    std::string payout_address;
    
    /// @brief Использовать MTP+1 для timestamp
    bool use_mtp_timestamp = true;
    
    /// @brief Генерировать только пустые блоки (без транзакций)
    bool empty_blocks_only = true;
    
    /// @brief Размер extranonce в байтах
    std::size_t extranonce_size = 6;
};

/**
 * @brief Генератор шаблонов блоков
 * 
 * Создаёт шаблоны блоков на основе информации от headers sync,
 * без обращения к внешнему Bitcoin Core RPC.
 */
class TemplateGenerator {
public:
    /**
     * @brief Создать генератор с конфигурацией
     * 
     * @param config Конфигурация
     */
    explicit TemplateGenerator(const TemplateGeneratorConfig& config);
    
    ~TemplateGenerator();
    
    // Запрещаем копирование
    TemplateGenerator(const TemplateGenerator&) = delete;
    TemplateGenerator& operator=(const TemplateGenerator&) = delete;
    
    /**
     * @brief Обновить информацию о предыдущем блоке
     * 
     * @param prev_hash Хеш предыдущего блока
     * @param height Высота нового блока (prev_height + 1)
     * @param bits Target в compact формате
     * @param coinbase_value Награда за блок
     */
    void update_chain_tip(
        const Hash256& prev_hash,
        uint32_t height,
        uint32_t bits,
        int64_t coinbase_value
    );
    
    /**
     * @brief Получить MTP калькулятор для обновления timestamps
     * 
     * @return MtpCalculator& Ссылка на калькулятор
     */
    [[nodiscard]] MtpCalculator& get_mtp_calculator();
    
    /**
     * @brief Генерировать новый шаблон блока
     * 
     * @param extranonce Значение extranonce для coinbase
     * @return std::optional<BlockTemplate> Шаблон или nullopt при ошибке
     */
    [[nodiscard]] std::optional<BlockTemplate> generate_template(uint64_t extranonce);
    
    /**
     * @brief Генерировать speculative шаблон (для spy mining)
     * 
     * @param prev_hash Хеш нового блока (будет prev_hash для нашего)
     * @param extranonce Значение extranonce
     * @return std::optional<BlockTemplate> Шаблон или nullopt
     */
    [[nodiscard]] std::optional<BlockTemplate> generate_speculative(
        const Hash256& prev_hash,
        uint64_t extranonce
    );
    
    /**
     * @brief Проверить, готов ли генератор
     * 
     * @return bool true если есть информация о chain tip
     */
    [[nodiscard]] bool is_ready() const;
    
    /**
     * @brief Получить текущую высоту
     */
    [[nodiscard]] uint32_t current_height() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::core
