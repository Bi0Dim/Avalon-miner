/**
 * @file http_server.cpp
 * @brief Реализация простого HTTP сервера
 */

#include "http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <mutex>

namespace quaxis::http {

// =============================================================================
// HttpRequest
// =============================================================================

std::string HttpRequest::get_header(const std::string& name) const {
    // Case-insensitive поиск
    std::string lower_name;
    lower_name.reserve(name.size());
    for (char c : name) {
        lower_name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    
    for (const auto& [key, value] : headers) {
        std::string lower_key;
        lower_key.reserve(key.size());
        for (char c : key) {
            lower_key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        
        if (lower_key == lower_name) {
            return value;
        }
    }
    
    return "";
}

// =============================================================================
// HttpResponse
// =============================================================================

HttpResponse HttpResponse::json(const std::string& json_body) {
    HttpResponse response;
    response.status = HttpStatus::OK;
    response.headers["Content-Type"] = "application/json";
    response.body = json_body;
    return response;
}

HttpResponse HttpResponse::text(const std::string& text_body) {
    HttpResponse response;
    response.status = HttpStatus::OK;
    response.headers["Content-Type"] = "text/plain; charset=utf-8";
    response.body = text_body;
    return response;
}

HttpResponse HttpResponse::error(HttpStatus status, const std::string& message) {
    HttpResponse response;
    response.status = status;
    response.headers["Content-Type"] = "application/json";
    response.body = "{\"error\":\"" + message + "\"}";
    return response;
}

std::string HttpResponse::serialize() const {
    std::ostringstream ss;
    
    // Status line
    ss << "HTTP/1.1 " << static_cast<int>(status) << " " << get_status_text(status) << "\r\n";
    
    // Headers
    for (const auto& [key, value] : headers) {
        ss << key << ": " << value << "\r\n";
    }
    
    // Content-Length
    ss << "Content-Length: " << body.size() << "\r\n";
    
    // Connection
    ss << "Connection: close\r\n";
    
    // End of headers
    ss << "\r\n";
    
    // Body
    ss << body;
    
    return ss.str();
}

// =============================================================================
// HttpServer Implementation
// =============================================================================

struct HttpServer::Impl {
    HttpServerConfig config;
    
    // Сокет
    int server_fd{-1};
    
    // Состояние
    std::atomic<bool> running{false};
    
    // Статистика
    std::atomic<uint64_t> requests_count{0};
    std::atomic<uint64_t> errors_count{0};
    
    // Маршруты
    struct Route {
        HttpMethod method;
        std::string path;
        HttpHandler handler;
    };
    std::vector<Route> routes;
    std::mutex routes_mutex;
    
    // Поток обработки
    std::thread server_thread;
    
    explicit Impl(const HttpServerConfig& cfg) : config(cfg) {}
    
    ~Impl() {
        close_socket();
    }
    
    void close_socket() {
        if (server_fd >= 0) {
            ::close(server_fd);
            server_fd = -1;
        }
    }
    
    bool create_socket() {
        server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            return false;
        }
        
        // Разрешить переиспользование адреса
        int opt = 1;
        ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        return true;
    }
    
    bool bind_socket() {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config.port);
        
        if (config.bind_address == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (::inet_pton(AF_INET, config.bind_address.c_str(), &addr.sin_addr) <= 0) {
                return false;
            }
        }
        
        if (::bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        
        return true;
    }
    
    bool listen_socket() {
        return ::listen(server_fd, static_cast<int>(config.max_connections)) == 0;
    }
    
    void server_loop() {
        struct pollfd pfd{};
        pfd.fd = server_fd;
        pfd.events = POLLIN;
        
        while (running) {
            int ret = ::poll(&pfd, 1, 100);  // 100ms timeout
            
            if (ret < 0) {
                if (errno == EINTR) continue;
                break;
            }
            
            if (ret == 0) continue;
            
            if (pfd.revents & POLLIN) {
                struct sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                
                int client_fd = ::accept(server_fd, 
                    reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
                
                if (client_fd >= 0) {
                    handle_client(client_fd);
                    ::close(client_fd);
                }
            }
        }
    }
    
    void handle_client(int client_fd) {
        // Читаем запрос
        char buffer[4096];
        ssize_t n = ::recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (n <= 0) {
            errors_count++;
            return;
        }
        
        buffer[n] = '\0';
        
        // Парсим запрос
        HttpRequest request = parse_request(buffer);
        
        // Находим обработчик
        HttpResponse response = route_request(request);
        
        // Отправляем ответ
        std::string response_str = response.serialize();
        ::send(client_fd, response_str.c_str(), response_str.size(), 0);
        
        requests_count++;
    }
    
    HttpRequest parse_request(const char* data) {
        HttpRequest request;
        std::istringstream stream(data);
        std::string line;
        
        // Request line
        if (std::getline(stream, line)) {
            // Удаляем \r если есть
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            std::istringstream request_line(line);
            std::string method, path, version;
            request_line >> method >> path >> version;
            
            // Метод
            if (method == "GET") request.method = HttpMethod::GET;
            else if (method == "POST") request.method = HttpMethod::POST;
            else if (method == "PUT") request.method = HttpMethod::PUT;
            else if (method == "DELETE") request.method = HttpMethod::DELETE;
            else if (method == "HEAD") request.method = HttpMethod::HEAD;
            else if (method == "OPTIONS") request.method = HttpMethod::OPTIONS;
            else request.method = HttpMethod::UNKNOWN;
            
            // Путь и query
            auto query_pos = path.find('?');
            if (query_pos != std::string::npos) {
                request.query = path.substr(query_pos + 1);
                request.path = path.substr(0, query_pos);
            } else {
                request.path = path;
            }
        }
        
        // Headers
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            if (line.empty()) break;
            
            auto colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Trim whitespace
                while (!value.empty() && value.front() == ' ') {
                    value.erase(0, 1);
                }
                
                request.headers[name] = value;
            }
        }
        
        // Body (остаток)
        std::string body;
        while (std::getline(stream, line)) {
            body += line + "\n";
        }
        if (!body.empty() && body.back() == '\n') {
            body.pop_back();
        }
        request.body = body;
        
        return request;
    }
    
    HttpResponse route_request(const HttpRequest& request) {
        std::lock_guard<std::mutex> lock(routes_mutex);
        
        for (const auto& route : routes) {
            if (route.path == request.path) {
                if (route.method == HttpMethod::UNKNOWN || route.method == request.method) {
                    try {
                        return route.handler(request);
                    } catch (const std::exception& e) {
                        errors_count++;
                        return HttpResponse::error(HttpStatus::InternalServerError, e.what());
                    }
                }
            }
        }
        
        return HttpResponse::error(HttpStatus::NotFound, "Not found");
    }
};

// =============================================================================
// Публичный API
// =============================================================================

HttpServer::HttpServer(const HttpServerConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::route(const std::string& path, HttpHandler handler) {
    route(HttpMethod::UNKNOWN, path, std::move(handler));
}

void HttpServer::route(HttpMethod method, const std::string& path, HttpHandler handler) {
    std::lock_guard<std::mutex> lock(impl_->routes_mutex);
    impl_->routes.push_back({method, path, std::move(handler)});
}

Result<void> HttpServer::start() {
    if (impl_->running) {
        return {};
    }
    
    if (!impl_->config.enabled) {
        return {};
    }
    
    if (!impl_->create_socket()) {
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, 
            "Не удалось создать сокет"});
    }
    
    if (!impl_->bind_socket()) {
        impl_->close_socket();
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, 
            "Не удалось привязать сокет к " + impl_->config.bind_address + 
            ":" + std::to_string(impl_->config.port)});
    }
    
    if (!impl_->listen_socket()) {
        impl_->close_socket();
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, 
            "Не удалось начать прослушивание"});
    }
    
    impl_->running = true;
    impl_->server_thread = std::thread([this]() {
        impl_->server_loop();
    });
    
    return {};
}

void HttpServer::stop() {
    impl_->running = false;
    impl_->close_socket();
    
    if (impl_->server_thread.joinable()) {
        impl_->server_thread.join();
    }
}

bool HttpServer::is_running() const noexcept {
    return impl_->running;
}

uint16_t HttpServer::get_port() const noexcept {
    return impl_->config.port;
}

uint64_t HttpServer::get_requests_count() const noexcept {
    return impl_->requests_count;
}

uint64_t HttpServer::get_errors_count() const noexcept {
    return impl_->errors_count;
}

} // namespace quaxis::http
