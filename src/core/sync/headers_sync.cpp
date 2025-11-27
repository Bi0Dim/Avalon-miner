/**
 * @file headers_sync.cpp
 * @brief Реализация синхронизатора заголовков
 */

#include "headers_sync.hpp"
#include "../validation/pow_validator.hpp"

namespace quaxis::core::sync {

HeadersSync::HeadersSync(const ChainParams& params)
    : params_(params)
    , store_(std::make_unique<HeadersStore>(params)) {}

HeadersSync::~HeadersSync() {
    stop();
}

void HeadersSync::start() {
    status_.store(SyncStatus::Syncing, std::memory_order_release);
}

void HeadersSync::stop() {
    status_.store(SyncStatus::Stopped, std::memory_order_release);
}

SyncStatus HeadersSync::status() const noexcept {
    return status_.load(std::memory_order_acquire);
}

bool HeadersSync::is_synchronized() const noexcept {
    return status() == SyncStatus::Synchronized;
}

BlockHeader HeadersSync::get_tip() const {
    return store_->get_tip();
}

uint32_t HeadersSync::get_tip_height() const noexcept {
    return store_->get_tip_height();
}

Hash256 HeadersSync::get_tip_hash() const {
    return store_->get_tip_hash();
}

uint256 HeadersSync::get_current_target() const {
    auto tip = store_->get_tip();
    return bits_to_target(tip.bits);
}

uint32_t HeadersSync::get_current_bits() const noexcept {
    try {
        auto tip = store_->get_tip();
        return tip.bits;
    } catch (...) {
        return params_.difficulty.pow_limit_bits;
    }
}

double HeadersSync::get_difficulty() const noexcept {
    try {
        auto tip = store_->get_tip();
        return tip.get_difficulty();
    } catch (...) {
        return 1.0;
    }
}

std::optional<BlockHeader> HeadersSync::get_header(uint32_t height) const {
    return store_->get_by_height(height);
}

void HeadersSync::on_new_block(NewBlockCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    new_block_callback_ = std::move(callback);
}

bool HeadersSync::process_headers(const std::vector<BlockHeader>& headers) {
    validation::PowValidator validator(params_);
    
    uint32_t current_height = store_->get_tip_height();
    
    for (const auto& header : headers) {
        // Проверяем PoW
        if (!validator.validate_pow(header)) {
            return false;
        }
        
        // Добавляем заголовок
        uint32_t new_height = current_height + 1;
        if (!store_->add_header(header, new_height)) {
            return false;
        }
        
        current_height = new_height;
        
        // Вызываем callback
        {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (new_block_callback_) {
                new_block_callback_(header, new_height);
            }
        }
    }
    
    return true;
}

std::vector<Hash256> HeadersSync::get_block_locator() const {
    std::vector<Hash256> locator;
    
    uint32_t height = store_->get_tip_height();
    int step = 1;
    
    while (height > 0) {
        auto header = store_->get_by_height(height);
        if (header) {
            locator.push_back(header->hash());
        }
        
        if (height < static_cast<uint32_t>(step)) {
            break;
        }
        height -= static_cast<uint32_t>(step);
        
        // Увеличиваем шаг экспоненциально после первых 10
        if (locator.size() > 10) {
            step *= 2;
        }
    }
    
    // Добавляем genesis
    auto genesis = store_->get_by_height(0);
    if (genesis) {
        locator.push_back(genesis->hash());
    }
    
    return locator;
}

} // namespace quaxis::core::sync
