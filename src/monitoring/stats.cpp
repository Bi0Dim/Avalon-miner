/**
 * @file stats.cpp
 * @brief Реализация сборщика статистики
 */

#include "stats.hpp"

#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>
#include <format>
#include <cmath>

namespace quaxis::monitoring {

// =============================================================================
// Форматирование
// =============================================================================

std::string format_hashrate(double hashrate) {
    const char* suffixes[] = {"H/s", "KH/s", "MH/s", "GH/s", "TH/s", "PH/s", "EH/s"};
    int suffix_idx = 0;
    
    while (hashrate >= 1000.0 && suffix_idx < 6) {
        hashrate /= 1000.0;
        ++suffix_idx;
    }
    
    return std::format("{:.2f} {}", hashrate, suffixes[suffix_idx]);
}

std::string format_duration(std::chrono::seconds seconds) {
    auto count = seconds.count();
    
    if (count < 60) {
        return std::format("{}s", count);
    }
    
    auto minutes = count / 60;
    auto secs = count % 60;
    
    if (minutes < 60) {
        return std::format("{}m {}s", minutes, secs);
    }
    
    auto hours = minutes / 60;
    minutes %= 60;
    
    if (hours < 24) {
        return std::format("{}h {}m", hours, minutes);
    }
    
    auto days = hours / 24;
    hours %= 24;
    
    return std::format("{}d {}h {}m", days, hours, minutes);
}

// =============================================================================
// StatsCollector реализация
// =============================================================================

struct StatsCollector::Impl {
    MonitoringConfig config;
    
    mutable std::mutex mutex;
    MiningStats stats;
    
    std::atomic<bool> running{false};
    std::thread output_thread;
    
    // Для расчёта среднего хешрейта
    std::vector<double> hashrate_samples;
    static constexpr std::size_t MAX_SAMPLES = 60;  // Последние 60 секунд
    
    explicit Impl(const MonitoringConfig& cfg) : config(cfg) {
        stats.start_time = std::chrono::steady_clock::now();
    }
    
    ~Impl() {
        stop_output();
    }
    
    void stop_output() {
        running.store(false, std::memory_order_relaxed);
        if (output_thread.joinable()) {
            output_thread.join();
        }
    }
    
    void output_loop() {
        while (running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(config.stats_interval));
            
            if (!running.load(std::memory_order_relaxed)) break;
            
            // Выводим статистику
            std::cout << format_stats_internal() << std::endl;
        }
    }
    
    std::string format_stats_internal() const {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - stats.start_time
        );
        
        std::string result;
        result += "╔══════════════════════════════════════════════════════════════╗\n";
        result += "║               QUAXIS SOLO MINER STATISTICS                   ║\n";
        result += "╠══════════════════════════════════════════════════════════════╣\n";
        
        result += std::format("║ Uptime: {:>52} ║\n", format_duration(uptime));
        result += std::format("║ Block Height: {:>47} ║\n", stats.current_height);
        result += std::format("║ Difficulty: {:>49.2f} ║\n", stats.current_difficulty);
        
        result += "╠══════════════════════════════════════════════════════════════╣\n";
        
        result += std::format("║ Hashrate (current): {:>41} ║\n", 
                             format_hashrate(stats.current_hashrate));
        result += std::format("║ Hashrate (average): {:>41} ║\n", 
                             format_hashrate(stats.average_hashrate));
        result += std::format("║ Hashrate (peak):    {:>41} ║\n", 
                             format_hashrate(stats.peak_hashrate));
        
        result += "╠══════════════════════════════════════════════════════════════╣\n";
        
        result += std::format("║ Shares Total:   {:>45} ║\n", stats.shares_total);
        result += std::format("║ Shares Valid:   {:>45} ║\n", stats.shares_valid);
        result += std::format("║ Shares Stale:   {:>45} ║\n", stats.shares_stale);
        
        result += "╠══════════════════════════════════════════════════════════════╣\n";
        
        result += std::format("║ Blocks Found:    {:>44} ║\n", stats.blocks_found);
        result += std::format("║ Blocks Accepted: {:>44} ║\n", stats.blocks_accepted);
        result += std::format("║ Blocks Rejected: {:>44} ║\n", stats.blocks_rejected);
        
        result += "╠══════════════════════════════════════════════════════════════╣\n";
        
        result += std::format("║ ASIC Connected: {:>45} ║\n", stats.asic_connected);
        result += std::format("║ Jobs Sent:      {:>45} ║\n", stats.jobs_sent);
        
        result += "╚══════════════════════════════════════════════════════════════╝";
        
        return result;
    }
    
    void update_average_hashrate(double new_sample) {
        hashrate_samples.push_back(new_sample);
        
        if (hashrate_samples.size() > MAX_SAMPLES) {
            hashrate_samples.erase(hashrate_samples.begin());
        }
        
        if (!hashrate_samples.empty()) {
            double sum = 0.0;
            for (double sample : hashrate_samples) {
                sum += sample;
            }
            stats.average_hashrate = sum / static_cast<double>(hashrate_samples.size());
        }
    }
};

// =============================================================================
// StatsCollector
// =============================================================================

StatsCollector::StatsCollector(const MonitoringConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

StatsCollector::~StatsCollector() = default;

void StatsCollector::record_share(bool valid, bool stale, bool duplicate) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->stats.shares_total++;
    
    if (valid) {
        impl_->stats.shares_valid++;
    }
    if (stale) {
        impl_->stats.shares_stale++;
    }
    if (duplicate) {
        impl_->stats.shares_duplicate++;
    }
}

void StatsCollector::record_block(bool accepted) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->stats.blocks_found++;
    
    if (accepted) {
        impl_->stats.blocks_accepted++;
    } else {
        impl_->stats.blocks_rejected++;
    }
}

void StatsCollector::update_hashrate(double hashrate) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    impl_->stats.current_hashrate = hashrate;
    
    if (hashrate > impl_->stats.peak_hashrate) {
        impl_->stats.peak_hashrate = hashrate;
    }
    
    impl_->update_average_hashrate(hashrate);
}

void StatsCollector::update_block_info(uint32_t height, double difficulty) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stats.current_height = height;
    impl_->stats.current_difficulty = difficulty;
}

void StatsCollector::update_connection_count(std::size_t count) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stats.asic_connected = count;
}

void StatsCollector::record_job_sent() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stats.jobs_sent++;
}

MiningStats StatsCollector::get_stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    auto stats = impl_->stats;
    auto now = std::chrono::steady_clock::now();
    stats.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats.start_time
    );
    
    return stats;
}

std::string StatsCollector::format_stats() const {
    return impl_->format_stats_internal();
}

std::string StatsCollector::format_summary() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - impl_->stats.start_time
    );
    
    return std::format(
        "[{}] Height: {} | Hashrate: {} | Shares: {} | Blocks: {} | ASICs: {}",
        format_duration(uptime),
        impl_->stats.current_height,
        format_hashrate(impl_->stats.current_hashrate),
        impl_->stats.shares_valid,
        impl_->stats.blocks_found,
        impl_->stats.asic_connected
    );
}

void StatsCollector::start_periodic_output() {
    if (impl_->running.load(std::memory_order_relaxed)) {
        return;
    }
    
    impl_->running.store(true, std::memory_order_relaxed);
    impl_->output_thread = std::thread([this] {
        impl_->output_loop();
    });
}

void StatsCollector::stop_periodic_output() {
    impl_->stop_output();
}

} // namespace quaxis::monitoring
