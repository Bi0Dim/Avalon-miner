/**
 * @file metrics.hpp
 * @brief Prometheus метрики для мониторинга
 * 
 * Централизованный сбор метрик с потокобезопасным доступом.
 * Поддерживает counters, gauges и histograms.
 */

#pragma once

#include "../core/types.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace quaxis::monitoring {

// =============================================================================
// Histogram Buckets
// =============================================================================

/**
 * @brief Bucket для histogram
 */
struct HistogramBucket {
    double le;           ///< Upper bound (less than or equal)
    std::atomic<uint64_t> count{0};
    
    explicit HistogramBucket(double upper_bound) : le(upper_bound) {}
    
    // Конструктор копирования для atomic
    HistogramBucket(const HistogramBucket& other) 
        : le(other.le), count(other.count.load()) {}
    
    // Оператор присваивания для atomic
    HistogramBucket& operator=(const HistogramBucket& other) {
        if (this != &other) {
            le = other.le;
            count.store(other.count.load());
        }
        return *this;
    }
};

/**
 * @brief Histogram метрика
 */
struct Histogram {
    std::vector<HistogramBucket> buckets;
    std::atomic<double> sum{0.0};
    std::atomic<uint64_t> count{0};
    
    // Дефолтный конструктор
    Histogram() = default;
    
    // Конструктор копирования
    Histogram(const Histogram& other) 
        : buckets(other.buckets)
        , sum(other.sum.load())
        , count(other.count.load()) {}
    
    // Конструктор перемещения
    Histogram(Histogram&& other) noexcept
        : buckets(std::move(other.buckets))
        , sum(other.sum.load())
        , count(other.count.load()) {}
    
    // Оператор присваивания
    Histogram& operator=(const Histogram& other) {
        if (this != &other) {
            buckets = other.buckets;
            sum.store(other.sum.load());
            count.store(other.count.load());
        }
        return *this;
    }
    
    // Оператор перемещающего присваивания
    Histogram& operator=(Histogram&& other) noexcept {
        if (this != &other) {
            buckets = std::move(other.buckets);
            sum.store(other.sum.load());
            count.store(other.count.load());
        }
        return *this;
    }
    
    /**
     * @brief Создать histogram с дефолтными buckets для латентности
     */
    static Histogram create_latency_histogram() {
        Histogram h;
        h.buckets.emplace_back(1.0);    // 1 ms
        h.buckets.emplace_back(5.0);    // 5 ms
        h.buckets.emplace_back(10.0);   // 10 ms
        h.buckets.emplace_back(25.0);   // 25 ms
        h.buckets.emplace_back(50.0);   // 50 ms
        h.buckets.emplace_back(100.0);  // 100 ms
        h.buckets.emplace_back(250.0);  // 250 ms
        h.buckets.emplace_back(500.0);  // 500 ms
        h.buckets.emplace_back(1000.0); // 1000 ms
        h.buckets.emplace_back(std::numeric_limits<double>::infinity()); // +Inf
        return h;
    }
    
    /**
     * @brief Записать наблюдение
     */
    void observe(double value) {
        for (auto& bucket : buckets) {
            if (value <= bucket.le) {
                bucket.count++;
            }
        }
        
        // Атомарное обновление sum (приближённое)
        double expected = sum.load();
        while (!sum.compare_exchange_weak(expected, expected + value)) {
            // Retry
        }
        
        count++;
    }
};

// =============================================================================
// Metrics Singleton
// =============================================================================

/**
 * @brief Централизованный сборщик метрик
 * 
 * Singleton для потокобезопасного сбора метрик со всех компонентов.
 */
class Metrics {
public:
    /**
     * @brief Получить единственный экземпляр
     */
    static Metrics& instance();
    
    // Запрещаем копирование
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;
    
    // =========================================================================
    // Время запуска
    // =========================================================================
    
    /**
     * @brief Записать время запуска
     */
    void set_start_time();
    
    /**
     * @brief Получить uptime в секундах
     */
    [[nodiscard]] uint64_t get_uptime_seconds() const;
    
    // =========================================================================
    // Counters (только увеличение)
    // =========================================================================
    
    /**
     * @brief Увеличить счётчик отправленных заданий
     */
    void inc_jobs_sent();
    
    /**
     * @brief Увеличить счётчик найденных shares
     */
    void inc_shares_found();
    
    /**
     * @brief Увеличить счётчик найденных блоков
     */
    void inc_blocks_found();
    
    /**
     * @brief Увеличить счётчик найденных merged mining блоков
     */
    void inc_merged_blocks_found(const std::string& chain);
    
    /**
     * @brief Увеличить счётчик ошибок
     */
    void inc_errors();
    
    /**
     * @brief Увеличить счётчик переключений fallback
     */
    void inc_fallback_switches();
    
    // =========================================================================
    // Gauges (произвольное значение)
    // =========================================================================
    
    /**
     * @brief Установить хешрейт (TH/s)
     */
    void set_hashrate(double ths);
    
    /**
     * @brief Установить режим работы (0=SHM, 1=ZMQ, 2=Stratum)
     */
    void set_mode(int mode);
    
    /**
     * @brief Установить статус подключения к Bitcoin Core
     */
    void set_bitcoin_connected(bool connected);
    
    /**
     * @brief Установить количество подключённых ASIC
     */
    void set_asic_connections(int count);
    
    /**
     * @brief Установить количество активных merged mining chains
     */
    void set_merged_chains_active(int count);
    
    /**
     * @brief Установить текущую сложность
     */
    void set_difficulty(double diff);
    
    /**
     * @brief Установить текущую высоту блока
     */
    void set_block_height(uint32_t height);
    
    // =========================================================================
    // Histograms
    // =========================================================================
    
    /**
     * @brief Записать наблюдение латентности (ms)
     */
    void observe_latency(double ms);
    
    /**
     * @brief Записать возраст задания (ms)
     */
    void observe_job_age(double ms);
    
    // =========================================================================
    // Получение значений
    // =========================================================================
    
    /**
     * @brief Получить количество отправленных заданий
     */
    [[nodiscard]] uint64_t get_jobs_sent() const;
    
    /**
     * @brief Получить количество найденных shares
     */
    [[nodiscard]] uint64_t get_shares_found() const;
    
    /**
     * @brief Получить количество найденных блоков
     */
    [[nodiscard]] uint64_t get_blocks_found() const;
    
    /**
     * @brief Получить хешрейт
     */
    [[nodiscard]] double get_hashrate() const;
    
    /**
     * @brief Получить текущий режим
     */
    [[nodiscard]] int get_mode() const;
    
    /**
     * @brief Проверить подключение к Bitcoin Core
     */
    [[nodiscard]] bool is_bitcoin_connected() const;
    
    /**
     * @brief Получить количество подключённых ASIC
     */
    [[nodiscard]] int get_asic_connections() const;
    
    /**
     * @brief Получить количество активных merged chains
     */
    [[nodiscard]] int get_merged_chains_active() const;
    
    // =========================================================================
    // Экспорт
    // =========================================================================
    
    /**
     * @brief Экспортировать метрики в формате Prometheus
     */
    [[nodiscard]] std::string export_prometheus() const;
    
    /**
     * @brief Сбросить все метрики
     */
    void reset();
    
private:
    Metrics();
    ~Metrics() = default;
    
    // Время запуска
    std::chrono::steady_clock::time_point start_time_;
    
    // Counters
    std::atomic<uint64_t> jobs_sent_{0};
    std::atomic<uint64_t> shares_found_{0};
    std::atomic<uint64_t> blocks_found_{0};
    std::atomic<uint64_t> errors_{0};
    std::atomic<uint64_t> fallback_switches_{0};
    
    // Merged blocks per chain
    std::unordered_map<std::string, std::atomic<uint64_t>> merged_blocks_;
    mutable std::mutex merged_blocks_mutex_;
    
    // Gauges
    std::atomic<double> hashrate_{0.0};
    std::atomic<int> mode_{0};
    std::atomic<bool> bitcoin_connected_{false};
    std::atomic<int> asic_connections_{0};
    std::atomic<int> merged_chains_active_{0};
    std::atomic<double> difficulty_{0.0};
    std::atomic<uint32_t> block_height_{0};
    
    // Histograms
    Histogram latency_histogram_;
    Histogram job_age_histogram_;
};

} // namespace quaxis::monitoring
