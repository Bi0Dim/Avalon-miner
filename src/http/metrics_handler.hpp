/**
 * @file metrics_handler.hpp
 * @brief HTTP обработчик для /metrics endpoint (Prometheus формат)
 * 
 * Возвращает метрики в формате Prometheus exposition format.
 */

#pragma once

#include "http_server.hpp"

#include <functional>
#include <sstream>

namespace quaxis::http {

// =============================================================================
// Данные для Metrics
// =============================================================================

/**
 * @brief Данные метрик
 */
struct MetricsData {
    /// @brief Хешрейт (TH/s)
    double hashrate_ths{0.0};
    
    /// @brief Отправленных заданий
    uint64_t jobs_sent{0};
    
    /// @brief Найденных shares
    uint64_t shares_found{0};
    
    /// @brief Найденных блоков
    uint64_t blocks_found{0};
    
    /// @brief Латентность заданий (ms)
    double latency_ms{0.0};
    
    /// @brief Uptime (секунды)
    uint64_t uptime_seconds{0};
    
    /// @brief Текущий режим (0=SHM, 1=ZMQ, 2=Stratum)
    int mode{0};
    
    /// @brief Подключён ли к Bitcoin Core
    bool bitcoin_core_connected{false};
    
    /// @brief Количество подключённых ASIC
    uint32_t asic_connections{0};
    
    /// @brief Активных merged mining chains
    uint32_t merged_chains_active{0};
    
    // Histogram buckets для латентности
    uint64_t latency_bucket_1ms{0};
    uint64_t latency_bucket_5ms{0};
    uint64_t latency_bucket_10ms{0};
    uint64_t latency_bucket_inf{0};
};

/**
 * @brief Функция получения данных для metrics
 */
using MetricsDataProvider = std::function<MetricsData()>;

// =============================================================================
// Metrics Handler
// =============================================================================

/**
 * @brief Создать обработчик /metrics endpoint (Prometheus формат)
 * 
 * @param provider Функция получения данных
 * @return HttpHandler Обработчик
 */
inline HttpHandler create_metrics_handler(MetricsDataProvider provider) {
    return [provider = std::move(provider)](const HttpRequest& /*request*/) -> HttpResponse {
        MetricsData data = provider();
        
        std::ostringstream out;
        
        // Hashrate
        out << "# HELP quaxis_hashrate_ths Current hashrate in TH/s\n";
        out << "# TYPE quaxis_hashrate_ths gauge\n";
        out << "quaxis_hashrate_ths " << data.hashrate_ths << "\n\n";
        
        // Jobs sent
        out << "# HELP quaxis_jobs_sent_total Total jobs sent to ASIC\n";
        out << "# TYPE quaxis_jobs_sent_total counter\n";
        out << "quaxis_jobs_sent_total " << data.jobs_sent << "\n\n";
        
        // Shares found
        out << "# HELP quaxis_shares_found_total Total shares found\n";
        out << "# TYPE quaxis_shares_found_total counter\n";
        out << "quaxis_shares_found_total " << data.shares_found << "\n\n";
        
        // Blocks found
        out << "# HELP quaxis_blocks_found_total Total blocks found\n";
        out << "# TYPE quaxis_blocks_found_total counter\n";
        out << "quaxis_blocks_found_total " << data.blocks_found << "\n\n";
        
        // Latency histogram
        out << "# HELP quaxis_latency_ms Job latency in milliseconds\n";
        out << "# TYPE quaxis_latency_ms histogram\n";
        out << "quaxis_latency_ms_bucket{le=\"1\"} " << data.latency_bucket_1ms << "\n";
        out << "quaxis_latency_ms_bucket{le=\"5\"} " << data.latency_bucket_5ms << "\n";
        out << "quaxis_latency_ms_bucket{le=\"10\"} " << data.latency_bucket_10ms << "\n";
        out << "quaxis_latency_ms_bucket{le=\"+Inf\"} " << data.latency_bucket_inf << "\n";
        out << "quaxis_latency_ms_sum " << data.latency_ms << "\n";
        out << "quaxis_latency_ms_count " << data.latency_bucket_inf << "\n\n";
        
        // Uptime
        out << "# HELP quaxis_uptime_seconds Server uptime\n";
        out << "# TYPE quaxis_uptime_seconds counter\n";
        out << "quaxis_uptime_seconds " << data.uptime_seconds << "\n\n";
        
        // Mode
        out << "# HELP quaxis_mode Current operating mode (0=shm, 1=zmq, 2=stratum)\n";
        out << "# TYPE quaxis_mode gauge\n";
        out << "quaxis_mode " << data.mode << "\n\n";
        
        // Bitcoin Core connected
        out << "# HELP quaxis_bitcoin_core_connected Bitcoin Core connection status\n";
        out << "# TYPE quaxis_bitcoin_core_connected gauge\n";
        out << "quaxis_bitcoin_core_connected " << (data.bitcoin_core_connected ? 1 : 0) << "\n\n";
        
        // ASIC connections
        out << "# HELP quaxis_asic_connections Number of connected ASIC devices\n";
        out << "# TYPE quaxis_asic_connections gauge\n";
        out << "quaxis_asic_connections " << data.asic_connections << "\n\n";
        
        // Merged chains active
        out << "# HELP quaxis_merged_chains_active Active merged mining chains\n";
        out << "# TYPE quaxis_merged_chains_active gauge\n";
        out << "quaxis_merged_chains_active " << data.merged_chains_active << "\n";
        
        HttpResponse response;
        response.status = HttpStatus::OK;
        response.headers["Content-Type"] = "text/plain; version=0.0.4; charset=utf-8";
        response.body = out.str();
        
        return response;
    };
}

} // namespace quaxis::http
