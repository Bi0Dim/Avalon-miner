/**
 * @file share_validator.cpp
 * @brief Реализация валидатора шар
 */

#include "share_validator.hpp"
#include "../bitcoin/target.hpp"
#include "../core/byte_order.hpp"

#include <atomic>
#include <set>
#include <mutex>

namespace quaxis::mining {

struct ShareValidator::Impl {
    JobManager& job_manager;
    ValidBlockCallback valid_block_callback;
    
    double partial_difficulty = 1.0;
    
    mutable std::mutex mutex;
    
    // Статистика
    std::atomic<uint64_t> total_shares_count{0};
    std::atomic<uint64_t> blocks_found_count{0};
    std::atomic<uint64_t> stale_shares_count{0};
    std::atomic<uint64_t> duplicate_shares_count{0};
    
    // Дедупликация (job_id << 32 | nonce)
    std::set<uint64_t> seen_shares;
    static constexpr std::size_t MAX_SEEN_SHARES = 100000;
    
    explicit Impl(JobManager& jm) : job_manager(jm) {}
    
    bool check_duplicate(uint32_t job_id, uint32_t nonce) {
        uint64_t key = (static_cast<uint64_t>(job_id) << 32) | nonce;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Ограничиваем размер
        if (seen_shares.size() >= MAX_SEEN_SHARES) {
            // Удаляем старые записи (первые 10%)
            auto it = seen_shares.begin();
            std::advance(it, MAX_SEEN_SHARES / 10);
            seen_shares.erase(seen_shares.begin(), it);
        }
        
        auto [_, inserted] = seen_shares.insert(key);
        return !inserted;  // true если дубликат
    }
};

ShareValidator::ShareValidator(JobManager& job_manager)
    : impl_(std::make_unique<Impl>(job_manager))
{
}

ShareValidator::~ShareValidator() = default;

ValidationResult ShareValidator::validate(const Share& share) {
    ValidationResult result;
    result.job_id = share.job_id;
    result.nonce = share.nonce;
    
    // Инкрементируем счётчик
    impl_->total_shares_count.fetch_add(1, std::memory_order_relaxed);
    
    // Получаем задание
    auto job_opt = impl_->job_manager.get_job(share.job_id);
    if (!job_opt) {
        result.result = ShareResult::InvalidJobId;
        return result;
    }
    
    const Job& job = *job_opt;
    
    // Проверяем на stale
    if (job.is_stale()) {
        impl_->stale_shares_count.fetch_add(1, std::memory_order_relaxed);
        result.result = ShareResult::StaleJob;
        return result;
    }
    
    // Проверяем на дубликат
    if (impl_->check_duplicate(share.job_id, share.nonce)) {
        impl_->duplicate_shares_count.fetch_add(1, std::memory_order_relaxed);
        result.result = ShareResult::DuplicateShare;
        return result;
    }
    
    // Вычисляем хеш
    // Формируем хвост заголовка: последние 4 байта merkle_root + timestamp + bits + nonce
    std::array<uint8_t, 16> header_tail{};
    
    // Для правильного вычисления нам нужен полный header
    // Пока используем упрощённую версию с timestamp + bits + nonce
    write_le32(header_tail.data() + 0, job.timestamp);
    write_le32(header_tail.data() + 4, job.bits);
    write_le32(header_tail.data() + 8, share.nonce);
    write_le32(header_tail.data() + 12, 0);  // Padding
    
    // Вычисляем хеш с использованием midstate
    result.hash = crypto::hash_header_with_midstate(
        job.midstate,
        std::span<const uint8_t, 16>(header_tail)
    );
    
    // Вычисляем сложность найденного хеша
    result.difficulty = bitcoin::target_to_difficulty(result.hash);
    
    // Проверяем соответствие target
    if (!bitcoin::meets_target(result.hash, job.target)) {
        // Проверяем partial difficulty
        double hash_diff = result.difficulty;
        if (hash_diff >= impl_->partial_difficulty) {
            result.result = ShareResult::ValidPartial;
        } else {
            result.result = ShareResult::TargetNotMet;
        }
        return result;
    }
    
    // БЛОК НАЙДЕН!
    result.result = ShareResult::Valid;
    impl_->blocks_found_count.fetch_add(1, std::memory_order_relaxed);
    
    // Вызываем callback
    if (impl_->valid_block_callback) {
        // Формируем заголовок блока
        bitcoin::BlockHeader header;
        header.timestamp = job.timestamp;
        header.bits = job.bits;
        header.nonce = share.nonce;
        // Остальные поля должны быть заполнены из job_manager
        
        impl_->valid_block_callback(result, header);
    }
    
    return result;
}

void ShareValidator::set_valid_block_callback(ValidBlockCallback callback) {
    impl_->valid_block_callback = std::move(callback);
}

void ShareValidator::set_partial_difficulty(double difficulty) {
    impl_->partial_difficulty = difficulty;
}

uint64_t ShareValidator::total_shares() const {
    return impl_->total_shares_count.load(std::memory_order_relaxed);
}

uint64_t ShareValidator::blocks_found() const {
    return impl_->blocks_found_count.load(std::memory_order_relaxed);
}

uint64_t ShareValidator::stale_shares() const {
    return impl_->stale_shares_count.load(std::memory_order_relaxed);
}

uint64_t ShareValidator::duplicate_shares() const {
    return impl_->duplicate_shares_count.load(std::memory_order_relaxed);
}

void ShareValidator::reset_stats() {
    impl_->total_shares_count.store(0, std::memory_order_relaxed);
    impl_->blocks_found_count.store(0, std::memory_order_relaxed);
    impl_->stale_shares_count.store(0, std::memory_order_relaxed);
    impl_->duplicate_shares_count.store(0, std::memory_order_relaxed);
    
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->seen_shares.clear();
}

} // namespace quaxis::mining
