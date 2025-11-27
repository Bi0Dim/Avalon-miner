/**
 * @file reward_dispatcher.cpp
 * @brief Реализация диспетчера отправки блоков
 */

#include "reward_dispatcher.hpp"
#include "../bitcoin/block.hpp"

#include <mutex>

namespace quaxis::merged {

// =============================================================================
// RewardDispatcher::Impl
// =============================================================================

struct RewardDispatcher::Impl {
    ChainManager& chain_manager;
    
    BlockDispatchedCallback dispatch_callback;
    
    std::unordered_map<std::string, uint32_t> dispatch_stats;
    mutable std::mutex stats_mutex;
    
    explicit Impl(ChainManager& cm) : chain_manager(cm) {}
};

// =============================================================================
// RewardDispatcher Implementation
// =============================================================================

RewardDispatcher::RewardDispatcher(ChainManager& chain_manager)
    : impl_(std::make_unique<Impl>(chain_manager)) {}

RewardDispatcher::~RewardDispatcher() = default;

std::vector<DispatchResult> RewardDispatcher::dispatch_block(
    const bitcoin::BlockHeader& header,
    const Bytes& coinbase_tx,
    [[maybe_unused]] uint32_t nonce,
    const MergedJob& merged_job
) {
    std::vector<DispatchResult> results;
    
    // Сериализуем заголовок
    auto header_bytes = header.serialize();
    
    // Строим coinbase branch (для пустого блока merkle root = txid coinbase)
    MerkleBranch coinbase_branch;
    coinbase_branch.index = 0;
    // Для блока только с coinbase branch пустой
    
    // Получаем список chains, которым подходит этот блок
    auto matching_chains = impl_->chain_manager.check_aux_chains(
        header_bytes,
        coinbase_tx,
        coinbase_branch
    );
    
    // Отправляем в каждую подходящую chain
    auto submit_results = impl_->chain_manager.submit_to_matching_chains(
        header_bytes,
        coinbase_tx,
        coinbase_branch
    );
    
    // Формируем результаты
    for (const auto& [chain_name, success] : submit_results) {
        DispatchResult result;
        result.chain_name = chain_name;
        result.success = success;
        
        // Находим шаблон для этой chain
        for (const auto& [name, tmpl] : merged_job.aux_templates) {
            if (name == chain_name) {
                result.height = tmpl.height;
                result.block_hash = tmpl.block_hash;
                break;
            }
        }
        
        if (!success) {
            result.error_message = "Ошибка отправки блока";
        }
        
        results.push_back(result);
        
        // Обновляем статистику
        if (success) {
            std::lock_guard<std::mutex> lock(impl_->stats_mutex);
            impl_->dispatch_stats[chain_name]++;
        }
        
        // Вызываем callback
        if (impl_->dispatch_callback) {
            impl_->dispatch_callback(result);
        }
    }
    
    return results;
}

std::vector<std::string> RewardDispatcher::check_all_chains(
    const bitcoin::BlockHeader& header
) const {
    auto header_bytes = header.serialize();
    
    MerkleBranch empty_branch;
    empty_branch.index = 0;
    
    return impl_->chain_manager.check_aux_chains(
        header_bytes,
        Bytes{}, // Пустая coinbase для проверки
        empty_branch
    );
}

void RewardDispatcher::set_dispatch_callback(BlockDispatchedCallback callback) {
    impl_->dispatch_callback = std::move(callback);
}

std::unordered_map<std::string, uint32_t> RewardDispatcher::get_dispatch_stats() const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    return impl_->dispatch_stats;
}

} // namespace quaxis::merged
