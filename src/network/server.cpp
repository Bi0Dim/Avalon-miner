/**
 * @file server.cpp
 * @brief Реализация TCP сервера для ASIC
 */

#include "server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>

#include <thread>
#include <mutex>
#include <atomic>
#include <list>
#include <format>

namespace quaxis::network {

struct Server::Impl {
    ServerConfig config;
    mining::JobManager& job_manager;
    
    int listen_fd = -1;
    std::atomic<bool> running{false};
    
    std::thread accept_thread;
    std::thread cleanup_thread;
    
    mutable std::mutex connections_mutex;
    std::list<std::unique_ptr<AsicConnection>> connections;
    
    AsicConnectedCallback connected_callback;
    AsicDisconnectedCallback disconnected_callback;
    
    mutable std::mutex stats_mutex;
    ServerStats stats;
    
    Impl(const ServerConfig& cfg, mining::JobManager& jm)
        : config(cfg)
        , job_manager(jm)
    {}
    
    ~Impl() {
        stop();
    }
    
    Result<void> start() {
        // Создаём сокет
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) {
            return Err<void>(
                ErrorCode::NetworkConnectionFailed,
                std::format("Не удалось создать сокет: {}", strerror(errno))
            );
        }
        
        // Опции сокета
        int opt = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // Bind
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config.port);
        
        if (config.bind_address == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, config.bind_address.c_str(), &addr.sin_addr);
        }
        
        if (bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(listen_fd);
            listen_fd = -1;
            return Err<void>(
                ErrorCode::NetworkConnectionFailed,
                std::format("Не удалось привязать сокет к {}:{}: {}",
                           config.bind_address, config.port, strerror(errno))
            );
        }
        
        // Listen
        if (listen(listen_fd, 16) < 0) {
            close(listen_fd);
            listen_fd = -1;
            return Err<void>(
                ErrorCode::NetworkConnectionFailed,
                std::format("Не удалось начать прослушивание: {}", strerror(errno))
            );
        }
        
        // Non-blocking
        int flags = fcntl(listen_fd, F_GETFL, 0);
        fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);
        
        // Запускаем потоки
        running.store(true, std::memory_order_relaxed);
        accept_thread = std::thread([this] { accept_loop(); });
        cleanup_thread = std::thread([this] { cleanup_loop(); });
        
        return {};
    }
    
    void stop() {
        running.store(false, std::memory_order_relaxed);
        
        if (listen_fd >= 0) {
            close(listen_fd);
            listen_fd = -1;
        }
        
        if (accept_thread.joinable()) {
            accept_thread.join();
        }
        if (cleanup_thread.joinable()) {
            cleanup_thread.join();
        }
        
        // Закрываем все соединения
        std::lock_guard<std::mutex> lock(connections_mutex);
        for (auto& conn : connections) {
            conn->stop();
        }
        connections.clear();
    }
    
    void accept_loop() {
        while (running.load(std::memory_order_relaxed)) {
            struct pollfd pfd;
            pfd.fd = listen_fd;
            pfd.events = POLLIN;
            
            int ret = poll(&pfd, 1, 100);
            
            if (ret < 0) {
                if (errno == EINTR) continue;
                break;
            }
            
            if (ret == 0) continue;
            
            if (pfd.revents & POLLIN) {
                accept_connection();
            }
        }
    }
    
    void accept_connection() {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(listen_fd, 
                               reinterpret_cast<struct sockaddr*>(&client_addr),
                               &addr_len);
        
        if (client_fd < 0) {
            return;
        }
        
        // Проверяем лимит подключений
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            if (connections.size() >= config.max_connections) {
                close(client_fd);
                return;
            }
        }
        
        // Адрес клиента
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
        std::string remote_addr = std::format("{}:{}", 
                                              addr_str, 
                                              ntohs(client_addr.sin_port));
        
        // Создаём соединение
        auto conn = std::make_unique<AsicConnection>(client_fd, remote_addr);
        
        // Устанавливаем callbacks
        std::string addr_copy = remote_addr;
        
        conn->set_share_callback([this](const mining::Share& share) {
            on_share_received(share);
        });
        
        conn->set_disconnected_callback([this, addr_copy]() {
            on_disconnected(addr_copy);
        });
        
        conn->start();
        
        // Добавляем в список
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            connections.push_back(std::move(conn));
        }
        
        // Обновляем статистику
        {
            std::lock_guard<std::mutex> lock(stats_mutex);
            stats.total_connections++;
            stats.active_connections = connections.size();
        }
        
        // Callback
        if (connected_callback) {
            connected_callback(remote_addr);
        }
        
        // Отправляем текущее задание новому ASIC
        if (auto job = job_manager.get_next_job()) {
            std::lock_guard<std::mutex> lock(connections_mutex);
            if (!connections.empty()) {
                connections.back()->send_job(*job);
            }
        }
    }
    
    void cleanup_loop() {
        while (running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            std::lock_guard<std::mutex> lock(connections_mutex);
            
            // Удаляем отключённые соединения
            connections.remove_if([](const auto& conn) {
                return !conn->is_connected();
            });
            
            // Обновляем статистику
            {
                std::lock_guard<std::mutex> slock(stats_mutex);
                stats.active_connections = connections.size();
                
                // Суммируем хешрейт
                uint32_t total_hashrate = 0;
                for (const auto& conn : connections) {
                    total_hashrate += conn->stats().last_hashrate;
                }
                stats.total_hashrate = total_hashrate;
            }
        }
    }
    
    void on_share_received(const mining::Share& /*share*/) {
        std::lock_guard<std::mutex> lock(stats_mutex);
        stats.total_shares++;
        
        // Валидация share будет в отдельном компоненте
    }
    
    void on_disconnected(const std::string& addr) {
        if (disconnected_callback) {
            disconnected_callback(addr);
        }
    }
    
    void broadcast(const std::function<void(AsicConnection&)>& action) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        for (auto& conn : connections) {
            if (conn->is_connected()) {
                action(*conn);
            }
        }
    }
};

// =============================================================================
// Server
// =============================================================================

Server::Server(const ServerConfig& config, mining::JobManager& job_manager)
    : impl_(std::make_unique<Impl>(config, job_manager))
{
}

Server::~Server() = default;

Result<void> Server::start() {
    return impl_->start();
}

void Server::stop() {
    impl_->stop();
}

bool Server::is_running() const noexcept {
    return impl_->running.load(std::memory_order_relaxed);
}

void Server::broadcast_job(const mining::Job& job) {
    impl_->broadcast([&job](AsicConnection& conn) {
        conn.send_job(job);
    });
    
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    impl_->stats.total_jobs_sent++;
}

void Server::broadcast_stop() {
    impl_->broadcast([](AsicConnection& conn) {
        conn.send_stop();
    });
}

void Server::broadcast_target(const Hash256& target) {
    impl_->broadcast([&target](AsicConnection& conn) {
        conn.send_target(target);
    });
}

void Server::set_connected_callback(AsicConnectedCallback callback) {
    impl_->connected_callback = std::move(callback);
}

void Server::set_disconnected_callback(AsicDisconnectedCallback callback) {
    impl_->disconnected_callback = std::move(callback);
}

ServerStats Server::stats() const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    return impl_->stats;
}

std::size_t Server::connection_count() const {
    std::lock_guard<std::mutex> lock(impl_->connections_mutex);
    return impl_->connections.size();
}

std::vector<std::string> Server::connected_addresses() const {
    std::lock_guard<std::mutex> lock(impl_->connections_mutex);
    
    std::vector<std::string> addresses;
    addresses.reserve(impl_->connections.size());
    
    for (const auto& conn : impl_->connections) {
        addresses.push_back(conn->remote_address());
    }
    
    return addresses;
}

} // namespace quaxis::network
