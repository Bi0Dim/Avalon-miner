/**
 * @file http_server.hpp
 * @brief Простой HTTP сервер для health и metrics endpoints
 * 
 * Минималистичный HTTP/1.1 сервер для:
 * - /health - проверка здоровья для load balancer
 * - /metrics - метрики в формате Prometheus
 */

#pragma once

#include "../core/types.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

namespace quaxis::http {

// =============================================================================
// HTTP типы
// =============================================================================

/**
 * @brief HTTP методы
 */
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    HEAD,
    OPTIONS,
    UNKNOWN
};

/**
 * @brief HTTP статус коды
 */
enum class HttpStatus {
    OK = 200,
    Created = 201,
    NoContent = 204,
    BadRequest = 400,
    Unauthorized = 401,
    NotFound = 404,
    MethodNotAllowed = 405,
    InternalServerError = 500,
    ServiceUnavailable = 503
};

/**
 * @brief Получить текст статуса
 */
[[nodiscard]] constexpr std::string_view get_status_text(HttpStatus status) noexcept {
    switch (status) {
        case HttpStatus::OK: return "OK";
        case HttpStatus::Created: return "Created";
        case HttpStatus::NoContent: return "No Content";
        case HttpStatus::BadRequest: return "Bad Request";
        case HttpStatus::Unauthorized: return "Unauthorized";
        case HttpStatus::NotFound: return "Not Found";
        case HttpStatus::MethodNotAllowed: return "Method Not Allowed";
        case HttpStatus::InternalServerError: return "Internal Server Error";
        case HttpStatus::ServiceUnavailable: return "Service Unavailable";
        default: return "Unknown";
    }
}

/**
 * @brief HTTP запрос
 */
struct HttpRequest {
    /// @brief Метод
    HttpMethod method{HttpMethod::GET};
    
    /// @brief Путь (например, "/health")
    std::string path;
    
    /// @brief Query string (без ?)
    std::string query;
    
    /// @brief Заголовки
    std::unordered_map<std::string, std::string> headers;
    
    /// @brief Тело запроса
    std::string body;
    
    /// @brief Получить заголовок (case-insensitive)
    [[nodiscard]] std::string get_header(const std::string& name) const;
};

/**
 * @brief HTTP ответ
 */
struct HttpResponse {
    /// @brief Статус
    HttpStatus status{HttpStatus::OK};
    
    /// @brief Заголовки
    std::unordered_map<std::string, std::string> headers;
    
    /// @brief Тело ответа
    std::string body;
    
    /**
     * @brief Создать ответ OK с JSON
     */
    static HttpResponse json(const std::string& json_body);
    
    /**
     * @brief Создать ответ OK с plain text
     */
    static HttpResponse text(const std::string& text_body);
    
    /**
     * @brief Создать ответ с ошибкой
     */
    static HttpResponse error(HttpStatus status, const std::string& message);
    
    /**
     * @brief Сериализовать в HTTP строку
     */
    [[nodiscard]] std::string serialize() const;
};

// =============================================================================
// HTTP Handler
// =============================================================================

/**
 * @brief Тип обработчика HTTP запроса
 */
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

// =============================================================================
// HTTP Server
// =============================================================================

/**
 * @brief Конфигурация HTTP сервера
 */
struct HttpServerConfig {
    /// @brief Адрес для прослушивания
    std::string bind_address{"0.0.0.0"};
    
    /// @brief Порт
    uint16_t port{9090};
    
    /// @brief Максимальное количество соединений
    uint32_t max_connections{100};
    
    /// @brief Таймаут чтения (секунды)
    uint32_t read_timeout{30};
    
    /// @brief Включён ли сервер
    bool enabled{true};
};

/**
 * @brief Простой HTTP сервер
 * 
 * Однопоточный HTTP/1.1 сервер для обработки health и metrics запросов.
 */
class HttpServer {
public:
    /**
     * @brief Создать сервер с конфигурацией
     */
    explicit HttpServer(const HttpServerConfig& config);
    
    ~HttpServer();
    
    // Запрещаем копирование
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    // ==========================================================================
    // Маршрутизация
    // ==========================================================================
    
    /**
     * @brief Зарегистрировать обработчик для пути
     * 
     * @param path Путь (например, "/health")
     * @param handler Функция обработки
     */
    void route(const std::string& path, HttpHandler handler);
    
    /**
     * @brief Зарегистрировать обработчик для метода и пути
     */
    void route(HttpMethod method, const std::string& path, HttpHandler handler);
    
    // ==========================================================================
    // Управление
    // ==========================================================================
    
    /**
     * @brief Запустить сервер
     * 
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> start();
    
    /**
     * @brief Остановить сервер
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли сервер
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    /**
     * @brief Получить порт
     */
    [[nodiscard]] uint16_t get_port() const noexcept;
    
    // ==========================================================================
    // Статистика
    // ==========================================================================
    
    /**
     * @brief Получить количество обработанных запросов
     */
    [[nodiscard]] uint64_t get_requests_count() const noexcept;
    
    /**
     * @brief Получить количество ошибок
     */
    [[nodiscard]] uint64_t get_errors_count() const noexcept;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::http
