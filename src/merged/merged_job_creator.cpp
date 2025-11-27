/**
 * @file merged_job_creator.cpp
 * @brief Реализация создателя заданий для merged mining
 */

#include "merged_job_creator.hpp"

namespace quaxis::merged {

// =============================================================================
// MergedJobCreator::Impl
// =============================================================================

struct MergedJobCreator::Impl {
    ChainManager& chain_manager;
    bitcoin::CoinbaseBuilder coinbase_builder;
    
    Impl(ChainManager& cm, bitcoin::CoinbaseBuilder cb)
        : chain_manager(cm)
        , coinbase_builder(std::move(cb)) {}
};

// =============================================================================
// MergedJobCreator Implementation
// =============================================================================

MergedJobCreator::MergedJobCreator(
    ChainManager& chain_manager,
    bitcoin::CoinbaseBuilder coinbase_builder
) : impl_(std::make_unique<Impl>(chain_manager, std::move(coinbase_builder))) {}

MergedJobCreator::~MergedJobCreator() = default;

MergedJob MergedJobCreator::create_job(
    const bitcoin::BlockTemplate& bitcoin_template,
    uint32_t job_id,
    uint64_t extranonce
) {
    MergedJob job;
    job.bitcoin_template = bitcoin_template;
    job.job_id = job_id;
    job.extranonce = extranonce;
    
    // Получаем текущий AuxPoW commitment
    job.aux_commitment = impl_->chain_manager.get_aux_commitment();
    
    // Получаем текущие шаблоны aux chains
    job.aux_templates = impl_->chain_manager.get_active_templates();
    
    // Строим coinbase с AuxPoW commitment
    job.coinbase_tx = build_coinbase_with_aux(
        bitcoin_template.height,
        bitcoin_template.coinbase_value,
        extranonce,
        job.aux_commitment
    );
    
    return job;
}

Bytes MergedJobCreator::build_coinbase_with_aux(
    uint32_t height,
    int64_t value,
    uint64_t extranonce,
    const std::optional<AuxCommitment>& aux_commitment
) const {
    // Базовая coinbase
    Bytes coinbase = impl_->coinbase_builder.build(height, value, extranonce);
    
    if (!aux_commitment) {
        return coinbase;
    }
    
    // Добавляем AuxPoW commitment в coinbase
    // Формат: вставляем перед scriptPubKey в output
    
    auto commitment_data = aux_commitment->serialize();
    
    // Находим позицию для вставки (после scriptsig, перед outputs)
    // В нашей структуре coinbase это примерно позиция 68-70
    // (зависит от размера scriptsig)
    
    // Упрощённый подход: добавляем commitment как OP_RETURN output
    // Это стандартный способ для merged mining
    
    // Увеличиваем output count
    // Находим позицию output_count в coinbase
    // Наша coinbase имеет фиксированную структуру
    
    // Вместо модификации существующей coinbase, создаём новую
    // с дополнительным OP_RETURN output
    
    // Позиция output_count в нашей структуре coinbase - байт 74
    // (version[4] + input_count[1] + prev_hash[32] + prev_index[4] + 
    //  scriptsig_len[1] + scriptsig[26] + sequence[4] + output_count[1])
    
    // Простой подход: добавляем commitment данные в конец scriptsig
    // Это работает для большинства AuxPoW реализаций
    
    Bytes result;
    result.reserve(coinbase.size() + commitment_data.size());
    
    // Копируем coinbase до output section
    // scriptsig_len находится на позиции 41
    if (coinbase.size() < 42) {
        return coinbase; // Coinbase слишком короткая
    }
    
    // Увеличиваем scriptsig_len
    std::size_t original_scriptsig_len = coinbase[41];
    std::size_t new_scriptsig_len = original_scriptsig_len + commitment_data.size();
    
    if (new_scriptsig_len > 100) {
        // Scriptsig слишком длинный для BIP34
        return coinbase;
    }
    
    // Копируем начало coinbase до scriptsig_len
    result.insert(result.end(), coinbase.begin(), coinbase.begin() + 41);
    
    // Новая длина scriptsig
    result.push_back(static_cast<uint8_t>(new_scriptsig_len));
    
    // Копируем оригинальный scriptsig
    std::size_t scriptsig_end = 42 + original_scriptsig_len;
    result.insert(result.end(), coinbase.begin() + 42, 
                  coinbase.begin() + static_cast<std::ptrdiff_t>(scriptsig_end));
    
    // Добавляем commitment
    result.insert(result.end(), commitment_data.begin(), commitment_data.end());
    
    // Копируем остаток coinbase (sequence, outputs, locktime)
    result.insert(result.end(), 
                  coinbase.begin() + static_cast<std::ptrdiff_t>(scriptsig_end),
                  coinbase.end());
    
    return result;
}

std::optional<AuxCommitment> MergedJobCreator::get_current_aux_commitment() const {
    return impl_->chain_manager.get_aux_commitment();
}

} // namespace quaxis::merged
