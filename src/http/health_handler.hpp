/**
 * @file health_handler.hpp
 * @brief HTTP обработчик для /health endpoint
 * 
 * Возвращает информацию о здоровье системы в формате JSON.
 */

#pragma once

#include "http_server.hpp"
#include "../fallback/fallback_manager.hpp"

#include <chrono>
#include <functional>
#include <sstream>

namespace quaxis::http {

// =============================================================================
// Данные для Health Check
// =============================================================================

/**
 * @brief Провайдер данных для health check
 */
struct HealthData {
    /// @brief Время запуска системы
    std::chrono::steady_clock::time_point start_time;
    
    /// @brief Текущий режим работы
    fallback::FallbackMode mode{fallback::FallbackMode::PrimarySHM};
    
    /// @brief Подключён ли к Bitcoin Core
    bool bitcoin_core_connected{false};
    
    /// @brief Количество подключённых ASIC
    uint32_t asic_connections{0};
    
    /// @brief Возраст последнего задания (ms)
    uint64_t last_job_age_ms{0};
    
    /// @brief Здорова ли система
    bool is_healthy{true};
    
    /// @brief Сообщение о статусе
    std::string status_message{"healthy"};
};

/**
 * @brief Функция получения данных для health check
 */
using HealthDataProvider = std::function<HealthData()>;

// =============================================================================
// Health Handler
// =============================================================================

/**
 * @brief Создать обработчик /health endpoint
 * 
 * @param provider Функция получения данных
 * @return HttpHandler Обработчик
 */
inline HttpHandler create_health_handler(HealthDataProvider provider) {
    return [provider = std::move(provider)](const HttpRequest& /*request*/) -> HttpResponse {
        HealthData data = provider();
        
        // Вычисляем uptime
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - data.start_time
        );
        
        // Формируем JSON
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"" << data.status_message << "\",\n";
        json << "  \"uptime_seconds\": " << uptime.count() << ",\n";
        json << "  \"mode\": \"" << fallback::to_string(data.mode) << "\",\n";
        json << "  \"bitcoin_core\": \"" 
             << (data.bitcoin_core_connected ? "connected" : "disconnected") << "\",\n";
        json << "  \"asic_connections\": " << data.asic_connections << ",\n";
        json << "  \"last_job_age_ms\": " << data.last_job_age_ms << "\n";
        json << "}";
        
        HttpResponse response;
        response.headers["Content-Type"] = "application/json";
        response.body = json.str();
        
        if (data.is_healthy) {
            response.status = HttpStatus::OK;
        } else {
            response.status = HttpStatus::ServiceUnavailable;
        }
        
        return response;
    };
}

/**
 * @brief Создать простой обработчик /health (всегда OK)
 */
inline HttpHandler create_simple_health_handler() {
    static auto start_time = std::chrono::steady_clock::now();
    
    return [](const HttpRequest& /*request*/) -> HttpResponse {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time
        );
        
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"healthy\",\n";
        json << "  \"uptime_seconds\": " << uptime.count() << "\n";
        json << "}";
        
        return HttpResponse::json(json.str());
    };
}

} // namespace quaxis::http
