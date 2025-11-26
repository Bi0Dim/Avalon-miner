/**
 * @file stats.hpp
 * @brief Статистика майнинга и мониторинг
 * 
 * Собирает и выводит статистику:
 * - Хешрейт (текущий, средний, пиковый)
 * - Найденные блоки и shares
 * - Состояние соединений
 * - Время безотказной работы
 */

#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../mining/job_manager.hpp"
#include "../network/server.hpp"

#include <memory>
#include <chrono>
#include <string>

namespace quaxis::monitoring {

// =============================================================================
// Статистика майнинга
// =============================================================================

/**
 * @brief Общая статистика майнинга
 */
struct MiningStats {
    // Хешрейт
    double current_hashrate = 0.0;    ///< Текущий хешрейт (H/s)
    double average_hashrate = 0.0;    ///< Средний хешрейт (H/s)
    double peak_hashrate = 0.0;       ///< Пиковый хешрейт (H/s)
    
    // Shares
    uint64_t shares_total = 0;        ///< Всего shares
    uint64_t shares_valid = 0;        ///< Валидных shares
    uint64_t shares_stale = 0;        ///< Устаревших shares
    uint64_t shares_duplicate = 0;    ///< Дубликатов
    
    // Блоки
    uint64_t blocks_found = 0;        ///< Найденных блоков
    uint64_t blocks_accepted = 0;     ///< Принятых сетью
    uint64_t blocks_rejected = 0;     ///< Отклонённых сетью
    
    // Соединения
    std::size_t asic_connected = 0;   ///< Подключённых ASIC
    uint64_t jobs_sent = 0;           ///< Отправленных заданий
    
    // Время
    std::chrono::steady_clock::time_point start_time;
    std::chrono::seconds uptime{0};
    
    // Текущий блок
    uint32_t current_height = 0;
    double current_difficulty = 0.0;
};

// =============================================================================
// Stats Collector
// =============================================================================

/**
 * @brief Сборщик и агрегатор статистики
 */
class StatsCollector {
public:
    /**
     * @brief Создать сборщик статистики
     * 
     * @param config Конфигурация мониторинга
     */
    explicit StatsCollector(const MonitoringConfig& config);
    
    ~StatsCollector();
    
    // Запрещаем копирование
    StatsCollector(const StatsCollector&) = delete;
    StatsCollector& operator=(const StatsCollector&) = delete;
    
    // =========================================================================
    // Обновление статистики
    // =========================================================================
    
    /**
     * @brief Зарегистрировать новый share
     * 
     * @param valid true если share валиден
     * @param stale true если share устарел
     * @param duplicate true если это дубликат
     */
    void record_share(bool valid, bool stale = false, bool duplicate = false);
    
    /**
     * @brief Зарегистрировать найденный блок
     * 
     * @param accepted true если блок принят сетью
     */
    void record_block(bool accepted);
    
    /**
     * @brief Обновить хешрейт
     * 
     * @param hashrate Новое значение хешрейта (H/s)
     */
    void update_hashrate(double hashrate);
    
    /**
     * @brief Обновить информацию о блоке
     * 
     * @param height Высота блока
     * @param difficulty Сложность
     */
    void update_block_info(uint32_t height, double difficulty);
    
    /**
     * @brief Обновить количество соединений
     * 
     * @param count Количество подключённых ASIC
     */
    void update_connection_count(std::size_t count);
    
    /**
     * @brief Зарегистрировать отправку задания
     */
    void record_job_sent();
    
    // =========================================================================
    // Получение статистики
    // =========================================================================
    
    /**
     * @brief Получить текущую статистику
     */
    [[nodiscard]] MiningStats get_stats() const;
    
    /**
     * @brief Получить форматированную статистику для вывода
     */
    [[nodiscard]] std::string format_stats() const;
    
    /**
     * @brief Получить краткую строку статистики
     */
    [[nodiscard]] std::string format_summary() const;
    
    // =========================================================================
    // Периодический вывод
    // =========================================================================
    
    /**
     * @brief Запустить периодический вывод статистики
     */
    void start_periodic_output();
    
    /**
     * @brief Остановить периодический вывод
     */
    void stop_periodic_output();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Форматирование
// =============================================================================

/**
 * @brief Форматировать хешрейт для отображения
 * 
 * @param hashrate Хешрейт в H/s
 * @return std::string Форматированная строка (например, "100.5 TH/s")
 */
[[nodiscard]] std::string format_hashrate(double hashrate);

/**
 * @brief Форматировать время для отображения
 * 
 * @param seconds Время в секундах
 * @return std::string Форматированная строка (например, "1d 2h 30m")
 */
[[nodiscard]] std::string format_duration(std::chrono::seconds seconds);

} // namespace quaxis::monitoring
