/**
 * @file pool_config.hpp
 * @brief Конфигурация пулов для fallback
 * 
 * Структуры для настройки основного и резервных источников
 * получения заданий майнинга.
 */

#pragma once

#include <string>
#include <chrono>
#include <vector>

namespace quaxis::fallback {

// =============================================================================
// Конфигурация Stratum пула
// =============================================================================

/**
 * @brief Конфигурация Stratum пула
 */
struct StratumPoolConfig {
    /// @brief URL пула (например, "stratum+tcp://solo.ckpool.org:3333")
    std::string url;
    
    /// @brief Хост
    std::string host;
    
    /// @brief Порт
    uint16_t port{3333};
    
    /// @brief Пользователь (обычно Bitcoin адрес)
    std::string user;
    
    /// @brief Пароль (обычно "x")
    std::string password{"x"};
    
    /// @brief Включён ли пул
    bool enabled{true};
    
    /// @brief Приоритет (меньше = выше приоритет)
    uint32_t priority{100};
    
    /**
     * @brief Парсинг URL в host:port
     * 
     * @param url URL вида "stratum+tcp://host:port"
     * @return true если парсинг успешен
     */
    bool parse_url(const std::string& url);
};

// =============================================================================
// Конфигурация ZMQ fallback
// =============================================================================

/**
 * @brief Конфигурация ZMQ fallback
 */
struct ZmqFallbackConfig {
    /// @brief Включён ли ZMQ fallback
    bool enabled{true};
    
    /// @brief Endpoint ZMQ (например, "tcp://127.0.0.1:28332")
    std::string endpoint{"tcp://127.0.0.1:28332"};
};

// =============================================================================
// Таймауты и интервалы
// =============================================================================

/**
 * @brief Конфигурация таймаутов
 */
struct TimeoutConfig {
    /// @brief Интервал проверки здоровья primary источника
    std::chrono::milliseconds primary_health_check{1000};
    
    /// @brief Таймаут primary источника до переключения
    std::chrono::milliseconds primary_timeout{5000};
    
    /// @brief Задержка перед повторным подключением
    std::chrono::milliseconds reconnect_delay{1000};
    
    /// @brief Таймаут подключения к Stratum
    std::chrono::milliseconds stratum_connect_timeout{5000};
    
    /// @brief Интервал keepalive для Stratum
    std::chrono::milliseconds stratum_keepalive{30000};
};

// =============================================================================
// Полная конфигурация Fallback
// =============================================================================

/**
 * @brief Полная конфигурация системы fallback
 */
struct FallbackConfig {
    /// @brief Включена ли система fallback
    bool enabled{true};
    
    /// @brief Конфигурация ZMQ fallback
    ZmqFallbackConfig zmq;
    
    /// @brief Конфигурация Stratum пулов (резервные)
    std::vector<StratumPoolConfig> stratum_pools;
    
    /// @brief Таймауты
    TimeoutConfig timeouts;
    
    /**
     * @brief Получить активный Stratum пул с наивысшим приоритетом
     * 
     * @return Указатель на конфигурацию пула или nullptr
     */
    [[nodiscard]] const StratumPoolConfig* get_active_pool() const;
};

} // namespace quaxis::fallback
