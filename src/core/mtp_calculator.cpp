/**
 * @file mtp_calculator.cpp
 * @brief Реализация вычисления Median Time Past
 */

#include "mtp_calculator.hpp"

#include <algorithm>
#include <chrono>

namespace quaxis::core {

void MtpCalculator::push_timestamp(uint32_t timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    timestamps_[head_] = timestamp;
    head_ = (head_ + 1) % MTP_BLOCK_COUNT;
    
    if (count_ < MTP_BLOCK_COUNT) {
        ++count_;
    }
}

void MtpCalculator::push_header(const BlockHeader& header) {
    push_timestamp(header.timestamp);
}

void MtpCalculator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    timestamps_.fill(0);
    count_ = 0;
    head_ = 0;
}

uint32_t MtpCalculator::get_mtp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (count_ < MTP_BLOCK_COUNT) {
        return 0;
    }
    
    // Копируем timestamps для сортировки
    std::array<uint32_t, MTP_BLOCK_COUNT> sorted = timestamps_;
    std::sort(sorted.begin(), sorted.end());
    
    // Медиана - средний элемент (индекс MTP_BLOCK_COUNT/2 для нечётного количества)
    return sorted[MTP_BLOCK_COUNT / 2];
}

uint32_t MtpCalculator::get_min_timestamp() const {
    uint32_t mtp = get_mtp();
    
    if (mtp == 0) {
        // Недостаточно данных - возвращаем текущее время
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(epoch).count()
        );
    }
    
    return mtp + 1;
}

bool MtpCalculator::has_sufficient_data() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_ >= MTP_BLOCK_COUNT;
}

std::size_t MtpCalculator::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

} // namespace quaxis::core
