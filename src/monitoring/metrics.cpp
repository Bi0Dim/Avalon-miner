/**
 * @file metrics.cpp
 * @brief Реализация Prometheus метрик
 */

#include "metrics.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>

namespace quaxis::monitoring {

// =============================================================================
// Singleton
// =============================================================================

Metrics& Metrics::instance() {
    static Metrics instance;
    return instance;
}

Metrics::Metrics() 
    : start_time_(std::chrono::steady_clock::now())
    , latency_histogram_(Histogram::create_latency_histogram())
    , job_age_histogram_(Histogram::create_latency_histogram()) {}

// =============================================================================
// Время
// =============================================================================

void Metrics::set_start_time() {
    start_time_ = std::chrono::steady_clock::now();
}

uint64_t Metrics::get_uptime_seconds() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count()
    );
}

// =============================================================================
// Counters
// =============================================================================

void Metrics::inc_jobs_sent() {
    jobs_sent_++;
}

void Metrics::inc_shares_found() {
    shares_found_++;
}

void Metrics::inc_blocks_found() {
    blocks_found_++;
}

void Metrics::inc_merged_blocks_found(const std::string& chain) {
    std::lock_guard<std::mutex> lock(merged_blocks_mutex_);
    merged_blocks_[chain]++;
}

void Metrics::inc_errors() {
    errors_++;
}

void Metrics::inc_fallback_switches() {
    fallback_switches_++;
}

// =============================================================================
// Gauges
// =============================================================================

void Metrics::set_hashrate(double ths) {
    hashrate_ = ths;
}

void Metrics::set_mode(int mode) {
    mode_ = mode;
}

void Metrics::set_bitcoin_connected(bool connected) {
    bitcoin_connected_ = connected;
}

void Metrics::set_asic_connections(int count) {
    asic_connections_ = count;
}

void Metrics::set_merged_chains_active(int count) {
    merged_chains_active_ = count;
}

void Metrics::set_difficulty(double diff) {
    difficulty_ = diff;
}

void Metrics::set_block_height(uint32_t height) {
    block_height_ = height;
}

// =============================================================================
// Histograms
// =============================================================================

void Metrics::observe_latency(double ms) {
    latency_histogram_.observe(ms);
}

void Metrics::observe_job_age(double ms) {
    job_age_histogram_.observe(ms);
}

// =============================================================================
// Getters
// =============================================================================

uint64_t Metrics::get_jobs_sent() const {
    return jobs_sent_;
}

uint64_t Metrics::get_shares_found() const {
    return shares_found_;
}

uint64_t Metrics::get_blocks_found() const {
    return blocks_found_;
}

double Metrics::get_hashrate() const {
    return hashrate_;
}

int Metrics::get_mode() const {
    return mode_;
}

bool Metrics::is_bitcoin_connected() const {
    return bitcoin_connected_;
}

int Metrics::get_asic_connections() const {
    return asic_connections_;
}

int Metrics::get_merged_chains_active() const {
    return merged_chains_active_;
}

// =============================================================================
// Экспорт
// =============================================================================

std::string Metrics::export_prometheus() const {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    
    // Hashrate
    out << "# HELP quaxis_hashrate_ths Current hashrate in TH/s\n";
    out << "# TYPE quaxis_hashrate_ths gauge\n";
    out << "quaxis_hashrate_ths " << hashrate_.load() << "\n\n";
    
    // Jobs sent
    out << "# HELP quaxis_jobs_sent_total Total jobs sent to ASIC\n";
    out << "# TYPE quaxis_jobs_sent_total counter\n";
    out << "quaxis_jobs_sent_total " << jobs_sent_.load() << "\n\n";
    
    // Shares found
    out << "# HELP quaxis_shares_found_total Total shares found\n";
    out << "# TYPE quaxis_shares_found_total counter\n";
    out << "quaxis_shares_found_total " << shares_found_.load() << "\n\n";
    
    // Blocks found
    out << "# HELP quaxis_blocks_found_total Total blocks found\n";
    out << "# TYPE quaxis_blocks_found_total counter\n";
    out << "quaxis_blocks_found_total " << blocks_found_.load() << "\n\n";
    
    // Errors
    out << "# HELP quaxis_errors_total Total errors\n";
    out << "# TYPE quaxis_errors_total counter\n";
    out << "quaxis_errors_total " << errors_.load() << "\n\n";
    
    // Fallback switches
    out << "# HELP quaxis_fallback_switches_total Total fallback mode switches\n";
    out << "# TYPE quaxis_fallback_switches_total counter\n";
    out << "quaxis_fallback_switches_total " << fallback_switches_.load() << "\n\n";
    
    // Latency histogram
    out << "# HELP quaxis_latency_ms Job latency in milliseconds\n";
    out << "# TYPE quaxis_latency_ms histogram\n";
    for (const auto& bucket : latency_histogram_.buckets) {
        if (std::isinf(bucket.le)) {
            out << "quaxis_latency_ms_bucket{le=\"+Inf\"} " << bucket.count.load() << "\n";
        } else {
            out << "quaxis_latency_ms_bucket{le=\"" << bucket.le << "\"} " 
                << bucket.count.load() << "\n";
        }
    }
    out << "quaxis_latency_ms_sum " << latency_histogram_.sum.load() << "\n";
    out << "quaxis_latency_ms_count " << latency_histogram_.count.load() << "\n\n";
    
    // Uptime
    out << "# HELP quaxis_uptime_seconds Server uptime\n";
    out << "# TYPE quaxis_uptime_seconds counter\n";
    out << "quaxis_uptime_seconds " << get_uptime_seconds() << "\n\n";
    
    // Mode
    out << "# HELP quaxis_mode Current operating mode (0=shm, 1=zmq, 2=stratum)\n";
    out << "# TYPE quaxis_mode gauge\n";
    out << "quaxis_mode " << mode_.load() << "\n\n";
    
    // Bitcoin Core connected
    out << "# HELP quaxis_bitcoin_core_connected Bitcoin Core connection status\n";
    out << "# TYPE quaxis_bitcoin_core_connected gauge\n";
    out << "quaxis_bitcoin_core_connected " << (bitcoin_connected_.load() ? 1 : 0) << "\n\n";
    
    // ASIC connections
    out << "# HELP quaxis_asic_connections Number of connected ASIC devices\n";
    out << "# TYPE quaxis_asic_connections gauge\n";
    out << "quaxis_asic_connections " << asic_connections_.load() << "\n\n";
    
    // Merged chains active
    out << "# HELP quaxis_merged_chains_active Active merged mining chains\n";
    out << "# TYPE quaxis_merged_chains_active gauge\n";
    out << "quaxis_merged_chains_active " << merged_chains_active_.load() << "\n\n";
    
    // Difficulty
    out << "# HELP quaxis_difficulty Current mining difficulty\n";
    out << "# TYPE quaxis_difficulty gauge\n";
    out << "quaxis_difficulty " << difficulty_.load() << "\n\n";
    
    // Block height
    out << "# HELP quaxis_block_height Current block height\n";
    out << "# TYPE quaxis_block_height gauge\n";
    out << "quaxis_block_height " << block_height_.load() << "\n\n";
    
    // Merged blocks per chain
    {
        std::lock_guard<std::mutex> lock(merged_blocks_mutex_);
        if (!merged_blocks_.empty()) {
            out << "# HELP quaxis_merged_blocks_total Total merged mining blocks found per chain\n";
            out << "# TYPE quaxis_merged_blocks_total counter\n";
            for (const auto& [chain, count] : merged_blocks_) {
                out << "quaxis_merged_blocks_total{chain=\"" << chain << "\"} " 
                    << count.load() << "\n";
            }
            out << "\n";
        }
    }
    
    return out.str();
}

void Metrics::reset() {
    jobs_sent_ = 0;
    shares_found_ = 0;
    blocks_found_ = 0;
    errors_ = 0;
    fallback_switches_ = 0;
    hashrate_ = 0.0;
    mode_ = 0;
    bitcoin_connected_ = false;
    asic_connections_ = 0;
    merged_chains_active_ = 0;
    difficulty_ = 0.0;
    block_height_ = 0;
    
    // Reset histograms
    for (auto& bucket : latency_histogram_.buckets) {
        bucket.count = 0;
    }
    latency_histogram_.sum = 0.0;
    latency_histogram_.count = 0;
    
    for (auto& bucket : job_age_histogram_.buckets) {
        bucket.count = 0;
    }
    job_age_histogram_.sum = 0.0;
    job_age_histogram_.count = 0;
    
    // Clear merged blocks
    {
        std::lock_guard<std::mutex> lock(merged_blocks_mutex_);
        merged_blocks_.clear();
    }
    
    // Reset start time
    start_time_ = std::chrono::steady_clock::now();
}

} // namespace quaxis::monitoring
