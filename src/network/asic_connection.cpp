/**
 * @file asic_connection.cpp
 * @brief Реализация соединения с ASIC
 */

#include "asic_connection.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>

namespace quaxis::network {

struct AsicConnection::Impl {
    int socket_fd = -1;
    std::string remote_addr;
    
    std::atomic<bool> connected{false};
    std::atomic<bool> running{false};
    
    std::thread recv_thread;
    std::thread send_thread;
    
    mutable std::mutex send_mutex;
    std::queue<Bytes> send_queue;
    
    ProtocolParser parser;
    
    ShareReceivedCallback share_callback;
    DisconnectedCallback disconnected_callback;
    StatusReceivedCallback status_callback;
    
    mutable std::mutex stats_mutex;
    ConnectionStats stats;
    
    Impl(int fd, std::string addr) 
        : socket_fd(fd)
        , remote_addr(std::move(addr))
    {
        stats.connected_at = std::chrono::steady_clock::now();
    }
    
    ~Impl() {
        stop();
    }
    
    void stop() {
        running.store(false, std::memory_order_relaxed);
        connected.store(false, std::memory_order_relaxed);
        
        if (socket_fd >= 0) {
            shutdown(socket_fd, SHUT_RDWR);
            close(socket_fd);
            socket_fd = -1;
        }
        
        if (recv_thread.joinable()) {
            recv_thread.join();
        }
        if (send_thread.joinable()) {
            send_thread.join();
        }
    }
    
    void recv_loop() {
        std::array<uint8_t, 1024> buffer;
        
        while (running.load(std::memory_order_relaxed)) {
            // Poll с таймаутом
            struct pollfd pfd;
            pfd.fd = socket_fd;
            pfd.events = POLLIN;
            
            int ret = poll(&pfd, 1, 100);  // 100ms таймаут
            
            if (ret < 0) {
                if (errno == EINTR) continue;
                break;
            }
            
            if (ret == 0) continue;  // Таймаут
            
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                break;  // Ошибка соединения
            }
            
            if (pfd.revents & POLLIN) {
                ssize_t n = recv(socket_fd, buffer.data(), buffer.size(), 0);
                
                if (n <= 0) {
                    break;  // Соединение закрыто или ошибка
                }
                
                // Обновляем статистику
                {
                    std::lock_guard<std::mutex> lock(stats_mutex);
                    stats.bytes_received += static_cast<uint64_t>(n);
                }
                
                // Добавляем данные в парсер
                parser.add_data(ByteSpan(buffer.data(), static_cast<std::size_t>(n)));
                
                // Обрабатываем сообщения
                while (auto msg = parser.try_parse()) {
                    process_message(*msg);
                }
            }
        }
        
        connected.store(false, std::memory_order_relaxed);
        
        if (disconnected_callback) {
            disconnected_callback();
        }
    }
    
    void send_loop() {
        while (running.load(std::memory_order_relaxed)) {
            Bytes data;
            
            {
                std::lock_guard<std::mutex> lock(send_mutex);
                if (!send_queue.empty()) {
                    data = std::move(send_queue.front());
                    send_queue.pop();
                }
            }
            
            if (data.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            // Отправляем данные
            std::size_t sent = 0;
            while (sent < data.size() && running.load(std::memory_order_relaxed)) {
                ssize_t n = send(socket_fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
                
                if (n < 0) {
                    if (errno == EINTR || errno == EAGAIN) continue;
                    break;
                }
                
                sent += static_cast<std::size_t>(n);
            }
            
            // Обновляем статистику
            if (sent > 0) {
                std::lock_guard<std::mutex> lock(stats_mutex);
                stats.bytes_sent += sent;
            }
        }
    }
    
    void process_message(const ParsedMessage& msg) {
        std::visit([this](auto&& m) {
            using T = std::decay_t<decltype(m)>;
            
            if constexpr (std::is_same_v<T, ShareMessage>) {
                {
                    std::lock_guard<std::mutex> lock(stats_mutex);
                    stats.shares_received++;
                    stats.last_share_at = std::chrono::steady_clock::now();
                }
                
                if (share_callback) {
                    share_callback(m.share);
                }
            }
            else if constexpr (std::is_same_v<T, StatusMessage>) {
                {
                    std::lock_guard<std::mutex> lock(stats_mutex);
                    stats.last_hashrate = m.hashrate;
                    stats.last_temperature = m.temperature;
                }
                
                if (status_callback) {
                    status_callback(m);
                }
            }
            else if constexpr (std::is_same_v<T, ErrorMessage>) {
                // Логируем ошибку
            }
        }, msg);
    }
    
    bool enqueue_send(Bytes data) {
        if (!connected.load(std::memory_order_relaxed)) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(send_mutex);
        send_queue.push(std::move(data));
        return true;
    }
};

// =============================================================================
// AsicConnection
// =============================================================================

AsicConnection::AsicConnection(int socket_fd, std::string remote_addr)
    : impl_(std::make_unique<Impl>(socket_fd, std::move(remote_addr)))
{
    // Настраиваем сокет
    int flag = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    
    // Устанавливаем non-blocking
    int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    impl_->connected.store(true, std::memory_order_relaxed);
}

AsicConnection::~AsicConnection() = default;

AsicConnection::AsicConnection(AsicConnection&&) noexcept = default;
AsicConnection& AsicConnection::operator=(AsicConnection&&) noexcept = default;

void AsicConnection::start() {
    impl_->running.store(true, std::memory_order_relaxed);
    impl_->recv_thread = std::thread([this] { impl_->recv_loop(); });
    impl_->send_thread = std::thread([this] { impl_->send_loop(); });
}

void AsicConnection::stop() {
    impl_->stop();
}

bool AsicConnection::is_connected() const noexcept {
    return impl_->connected.load(std::memory_order_relaxed);
}

bool AsicConnection::send_job(const mining::Job& job) {
    auto data = serialize_new_job(job);
    bool result = impl_->enqueue_send(std::move(data));
    
    if (result) {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex);
        impl_->stats.jobs_sent++;
    }
    
    return result;
}

bool AsicConnection::send_stop() {
    return impl_->enqueue_send(serialize_stop());
}

bool AsicConnection::send_heartbeat() {
    return impl_->enqueue_send(serialize_heartbeat());
}

bool AsicConnection::send_target(const Hash256& target) {
    return impl_->enqueue_send(serialize_set_target(target));
}

void AsicConnection::set_share_callback(ShareReceivedCallback callback) {
    impl_->share_callback = std::move(callback);
}

void AsicConnection::set_disconnected_callback(DisconnectedCallback callback) {
    impl_->disconnected_callback = std::move(callback);
}

void AsicConnection::set_status_callback(StatusReceivedCallback callback) {
    impl_->status_callback = std::move(callback);
}

const std::string& AsicConnection::remote_address() const noexcept {
    return impl_->remote_addr;
}

ConnectionStats AsicConnection::stats() const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    return impl_->stats;
}

std::size_t AsicConnection::pending_jobs() const {
    std::lock_guard<std::mutex> lock(impl_->send_mutex);
    return impl_->send_queue.size();
}

} // namespace quaxis::network
