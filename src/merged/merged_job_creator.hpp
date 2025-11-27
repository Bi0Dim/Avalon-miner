/**
 * @file merged_job_creator.hpp
 * @brief Создание заданий для merged mining
 * 
 * Отвечает за создание заданий майнинга с включением
 * AuxPoW commitment для auxiliary chains.
 */

#pragma once

#include "chain_manager.hpp"
#include "auxpow.hpp"
#include "../bitcoin/coinbase.hpp"
#include "../bitcoin/block.hpp"

#include <memory>
#include <optional>

namespace quaxis::merged {

/**
 * @brief Задание с поддержкой merged mining
 */
struct MergedJob {
    /// @brief Оригинальный Bitcoin job
    bitcoin::BlockTemplate bitcoin_template;
    
    /// @brief AuxPoW commitment (если есть активные aux chains)
    std::optional<AuxCommitment> aux_commitment;
    
    /// @brief Coinbase транзакция с AuxPoW commitment
    Bytes coinbase_tx;
    
    /// @brief Шаблоны auxiliary chains
    std::vector<std::pair<std::string, AuxBlockTemplate>> aux_templates;
    
    /// @brief ID задания
    uint32_t job_id{0};
    
    /// @brief Extranonce
    uint64_t extranonce{0};
    
    /**
     * @brief Есть ли активные auxiliary chains
     */
    [[nodiscard]] bool has_aux_chains() const noexcept {
        return aux_commitment.has_value();
    }
};

/**
 * @brief Создатель заданий для merged mining
 * 
 * Модифицирует coinbase транзакцию Bitcoin для включения
 * AuxPoW commitment, позволяя одновременный майнинг
 * Bitcoin и auxiliary chains.
 */
class MergedJobCreator {
public:
    /**
     * @brief Создать job creator с chain manager
     * 
     * @param chain_manager Менеджер auxiliary chains
     * @param coinbase_builder Bitcoin coinbase builder
     */
    explicit MergedJobCreator(
        ChainManager& chain_manager,
        bitcoin::CoinbaseBuilder coinbase_builder
    );
    
    ~MergedJobCreator();
    
    // Запрещаем копирование
    MergedJobCreator(const MergedJobCreator&) = delete;
    MergedJobCreator& operator=(const MergedJobCreator&) = delete;
    
    /**
     * @brief Создать merged job из Bitcoin шаблона
     * 
     * @param bitcoin_template Шаблон Bitcoin блока
     * @param job_id ID задания
     * @param extranonce Значение extranonce
     * @return MergedJob Задание с AuxPoW commitment
     */
    [[nodiscard]] MergedJob create_job(
        const bitcoin::BlockTemplate& bitcoin_template,
        uint32_t job_id,
        uint64_t extranonce
    );
    
    /**
     * @brief Построить coinbase с AuxPoW commitment
     * 
     * @param height Высота блока
     * @param value Награда в satoshi
     * @param extranonce Extranonce
     * @param aux_commitment AuxPoW commitment
     * @return Bytes Coinbase транзакция
     */
    [[nodiscard]] Bytes build_coinbase_with_aux(
        uint32_t height,
        int64_t value,
        uint64_t extranonce,
        const std::optional<AuxCommitment>& aux_commitment
    ) const;
    
    /**
     * @brief Получить текущий AuxPoW commitment
     */
    [[nodiscard]] std::optional<AuxCommitment> get_current_aux_commitment() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::merged
