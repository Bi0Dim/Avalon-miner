/**
 * @file template_cache.cpp
 * @brief Реализация кеша шаблонов блоков
 */

#include "template_cache.hpp"

#include <cstring>

namespace quaxis::mining {

struct TemplateCache::Impl {
    MiningConfig config;
    bitcoin::CoinbaseBuilder coinbase_builder;
    
    mutable std::mutex mutex;
    
    std::optional<bitcoin::BlockTemplate> current_template;
    std::optional<bitcoin::BlockTemplate> precomputed_template;
    
    uint64_t current_extranonce = 0;
    
    Impl(const MiningConfig& cfg, bitcoin::CoinbaseBuilder builder)
        : config(cfg)
        , coinbase_builder(std::move(builder))
    {}
    
    bitcoin::BlockTemplate create_template(
        const Hash256& prev_hash,
        uint32_t height,
        uint32_t bits,
        uint32_t timestamp,
        int64_t coinbase_value
    ) {
        bitcoin::BlockTemplate tmpl;
        
        tmpl.height = height;
        tmpl.coinbase_value = coinbase_value;
        
        // Заголовок блока
        tmpl.header.version = constants::BLOCK_VERSION;
        tmpl.header.prev_block = prev_hash;
        tmpl.header.timestamp = timestamp;
        tmpl.header.bits = bits;
        tmpl.header.nonce = 0;
        
        // Создаём coinbase транзакцию
        auto [coinbase, coinbase_midstate] = coinbase_builder.build_with_midstate(
            height,
            coinbase_value,
            current_extranonce
        );
        
        tmpl.coinbase_tx = std::move(coinbase);
        tmpl.coinbase_midstate = coinbase_midstate;
        
        // Вычисляем merkle root (для пустого блока = coinbase txid)
        Hash256 coinbase_txid = bitcoin::compute_txid(tmpl.coinbase_tx);
        tmpl.header.merkle_root = coinbase_txid;
        
        // Вычисляем midstate заголовка
        tmpl.header_midstate = tmpl.header.compute_midstate();
        
        // Вычисляем target
        tmpl.target = bitcoin::bits_to_target(bits);
        
        return tmpl;
    }
};

TemplateCache::TemplateCache(
    const MiningConfig& config,
    bitcoin::CoinbaseBuilder coinbase_builder
) : impl_(std::make_unique<Impl>(config, std::move(coinbase_builder)))
{
}

TemplateCache::~TemplateCache() = default;

bitcoin::BlockTemplate& TemplateCache::update_template(
    const Hash256& prev_hash,
    uint32_t height,
    uint32_t bits,
    uint32_t timestamp,
    int64_t coinbase_value
) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    // Инкрементируем extranonce
    impl_->current_extranonce++;
    
    // Создаём новый шаблон
    impl_->current_template = impl_->create_template(
        prev_hash, height, bits, timestamp, coinbase_value
    );
    
    return *impl_->current_template;
}

std::optional<bitcoin::BlockTemplate> TemplateCache::get_current() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->current_template;
}

std::optional<bitcoin::BlockTemplate> TemplateCache::get_precomputed() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->precomputed_template;
}

void TemplateCache::precompute_next(
    uint32_t estimated_next_height,
    uint32_t estimated_bits
) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->current_template) {
        return;
    }
    
    // Предполагаемый prev_hash - это хеш текущего блока (который мы ещё не нашли)
    // Это используется для spy mining - мы получим реальный хеш позже
    Hash256 estimated_prev_hash{};  // Будет обновлён при получении реального блока
    
    uint32_t estimated_timestamp = impl_->current_template->header.timestamp + 600; // ~10 минут
    
    impl_->precomputed_template = impl_->create_template(
        estimated_prev_hash,
        estimated_next_height,
        estimated_bits,
        estimated_timestamp,
        impl_->current_template->coinbase_value  // Награда та же
    );
}

bool TemplateCache::activate_precomputed() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->precomputed_template) {
        return false;
    }
    
    impl_->current_template = std::move(impl_->precomputed_template);
    impl_->precomputed_template = std::nullopt;
    
    return true;
}

void TemplateCache::clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->current_template = std::nullopt;
    impl_->precomputed_template = std::nullopt;
}

uint32_t TemplateCache::current_height() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current_template) {
        return impl_->current_template->height;
    }
    return 0;
}

} // namespace quaxis::mining
