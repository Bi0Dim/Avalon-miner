/**
 * @file udp_socket.cpp
 * @brief Реализация асинхронного UDP сокета
 * 
 * Использует POSIX sockets для кроссплатформенной совместимости.
 * Поддерживает non-blocking операции через select/poll.
 */

#include "udp_socket.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

// POSIX сокеты
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace quaxis::relay {

// =============================================================================
// Утилиты
// =============================================================================

Result<std::string> resolve_hostname(const std::string& hostname) {
    // Сначала проверяем, может это уже IP адрес
    struct in_addr addr4{};
    if (inet_pton(AF_INET, hostname.c_str(), &addr4) == 1) {
        return hostname;
    }
    
    struct in6_addr addr6{};
    if (inet_pton(AF_INET6, hostname.c_str(), &addr6) == 1) {
        return hostname;
    }
    
    // Резолвим hostname
    struct addrinfo hints{};
    hints.ai_family = AF_INET;  // Предпочитаем IPv4
    hints.ai_socktype = SOCK_DGRAM;
    
    struct addrinfo* result = nullptr;
    int ret = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    
    if (ret != 0 || result == nullptr) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Не удалось разрезолвить hostname: " + hostname
        });
    }
    
    // Получаем первый адрес
    char ip_str[INET_ADDRSTRLEN];
    auto* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
    
    std::string ip(ip_str);
    freeaddrinfo(result);
    
    return ip;
}

bool is_valid_ip(const std::string& address) {
    struct in_addr addr4{};
    if (inet_pton(AF_INET, address.c_str(), &addr4) == 1) {
        return true;
    }
    
    struct in6_addr addr6{};
    if (inet_pton(AF_INET6, address.c_str(), &addr6) == 1) {
        return true;
    }
    
    return false;
}

// =============================================================================
// Реализация UdpSocket
// =============================================================================

struct UdpSocket::Impl {
    /// @brief Файловый дескриптор сокета
    int fd{-1};
    
    /// @brief Локальный порт
    uint16_t local_port_{0};
    
    /// @brief Callback приёма
    UdpReceiveCallback receive_callback;
    
    /// @brief Callback ошибок
    UdpErrorCallback error_callback;
    
    /// @brief Статистика
    UdpStats stats_;
    
    /// @brief Буфер приёма
    std::vector<uint8_t> recv_buffer;
    
    /**
     * @brief Конструктор
     */
    Impl() : recv_buffer(UDP_RECV_BUFFER_SIZE) {}
    
    /**
     * @brief Деструктор
     */
    ~Impl() {
        close_socket();
    }
    
    /**
     * @brief Закрыть сокет
     */
    void close_socket() {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
        local_port_ = 0;
    }
    
    /**
     * @brief Создать сокет
     */
    Result<void> create_socket() {
        if (fd >= 0) {
            return {};  // Уже создан
        }
        
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            return std::unexpected(Error{
                ErrorCode::NetworkConnectionFailed,
                "Не удалось создать UDP сокет: " + std::string(strerror(errno))
            });
        }
        
        // Устанавливаем non-blocking режим
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close_socket();
            return std::unexpected(Error{
                ErrorCode::NetworkConnectionFailed,
                "Не удалось установить non-blocking режим"
            });
        }
        
        return {};
    }
    
    /**
     * @brief Отправить ошибку через callback
     */
    void report_error(const std::string& msg) {
        if (error_callback) {
            error_callback(msg);
        }
    }
};

UdpSocket::UdpSocket()
    : impl_(std::make_unique<Impl>())
{
}

UdpSocket::~UdpSocket() = default;

UdpSocket::UdpSocket(UdpSocket&&) noexcept = default;
UdpSocket& UdpSocket::operator=(UdpSocket&&) noexcept = default;

Result<void> UdpSocket::bind(uint16_t port, const std::string& bind_address) {
    // Создаём сокет если нужно
    auto create_result = impl_->create_socket();
    if (!create_result) {
        return create_result;
    }
    
    // Настраиваем адрес
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (bind_address.empty() || bind_address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
            return std::unexpected(Error{
                ErrorCode::ConfigInvalidValue,
                "Некорректный адрес привязки: " + bind_address
            });
        }
    }
    
    // Привязываем
    if (::bind(impl_->fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Не удалось привязать сокет: " + std::string(strerror(errno))
        });
    }
    
    // Получаем назначенный порт
    socklen_t addr_len = sizeof(addr);
    if (getsockname(impl_->fd, reinterpret_cast<struct sockaddr*>(&addr), &addr_len) == 0) {
        impl_->local_port_ = ntohs(addr.sin_port);
    } else {
        impl_->local_port_ = port;
    }
    
    return {};
}

void UdpSocket::close() {
    impl_->close_socket();
}

bool UdpSocket::is_open() const noexcept {
    return impl_->fd >= 0;
}

uint16_t UdpSocket::local_port() const noexcept {
    return impl_->local_port_;
}

std::optional<UdpPacket> UdpSocket::try_receive() {
    if (impl_->fd < 0) {
        return std::nullopt;
    }
    
    struct sockaddr_in sender_addr{};
    socklen_t sender_len = sizeof(sender_addr);
    
    ssize_t received = recvfrom(
        impl_->fd,
        impl_->recv_buffer.data(),
        impl_->recv_buffer.size(),
        MSG_DONTWAIT,
        reinterpret_cast<struct sockaddr*>(&sender_addr),
        &sender_len
    );
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return std::nullopt;  // Нет данных
        }
        ++impl_->stats_.recv_errors;
        impl_->report_error("Ошибка приёма: " + std::string(strerror(errno)));
        return std::nullopt;
    }
    
    if (received == 0) {
        return std::nullopt;
    }
    
    // Формируем пакет
    UdpPacket packet;
    packet.data.assign(
        impl_->recv_buffer.begin(),
        impl_->recv_buffer.begin() + received
    );
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sender_addr.sin_addr, ip_str, sizeof(ip_str));
    packet.sender.host = ip_str;
    packet.sender.port = ntohs(sender_addr.sin_port);
    packet.received_at = std::chrono::steady_clock::now();
    
    // Обновляем статистику
    ++impl_->stats_.packets_received;
    impl_->stats_.bytes_received += static_cast<uint64_t>(received);
    impl_->stats_.last_packet_time = packet.received_at;
    
    return packet;
}

Result<UdpPacket> UdpSocket::receive(uint32_t timeout_ms) {
    if (impl_->fd < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Сокет не открыт"
        });
    }
    
    // Ждём данных через poll
    struct pollfd pfd{};
    pfd.fd = impl_->fd;
    pfd.events = POLLIN;
    
    int ret = poll(&pfd, 1, static_cast<int>(timeout_ms));
    
    if (ret < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkRecvFailed,
            "Ошибка poll: " + std::string(strerror(errno))
        });
    }
    
    if (ret == 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkTimeout,
            "Таймаут ожидания данных"
        });
    }
    
    // Есть данные
    auto packet = try_receive();
    if (!packet) {
        return std::unexpected(Error{
            ErrorCode::NetworkRecvFailed,
            "Не удалось получить данные"
        });
    }
    
    return *packet;
}

void UdpSocket::set_receive_callback(UdpReceiveCallback callback) {
    impl_->receive_callback = std::move(callback);
}

std::size_t UdpSocket::poll_receive(std::size_t max_packets) {
    std::size_t count = 0;
    
    while (count < max_packets) {
        auto packet = try_receive();
        if (!packet) {
            break;
        }
        
        ++count;
        
        if (impl_->receive_callback) {
            impl_->receive_callback(*packet);
        }
    }
    
    return count;
}

Result<void> UdpSocket::send(const UdpEndpoint& endpoint, ByteSpan data) {
    return send(endpoint.host, endpoint.port, data);
}

Result<void> UdpSocket::send(const std::string& host, uint16_t port, ByteSpan data) {
    if (impl_->fd < 0) {
        // Создаём сокет если нужно
        auto result = impl_->create_socket();
        if (!result) {
            return result;
        }
    }
    
    // Резолвим адрес
    auto ip_result = resolve_hostname(host);
    if (!ip_result) {
        return std::unexpected(ip_result.error());
    }
    
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_result->c_str(), &addr.sin_addr);
    
    ssize_t sent = sendto(
        impl_->fd,
        data.data(),
        data.size(),
        0,
        reinterpret_cast<struct sockaddr*>(&addr),
        sizeof(addr)
    );
    
    if (sent < 0) {
        ++impl_->stats_.send_errors;
        return std::unexpected(Error{
            ErrorCode::NetworkSendFailed,
            "Ошибка отправки: " + std::string(strerror(errno))
        });
    }
    
    ++impl_->stats_.packets_sent;
    impl_->stats_.bytes_sent += static_cast<uint64_t>(sent);
    
    return {};
}

Result<void> UdpSocket::set_recv_buffer_size(std::size_t size) {
    if (impl_->fd < 0) {
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, "Сокет не открыт"});
    }
    
    int buf_size = static_cast<int>(size);
    if (setsockopt(impl_->fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size)) < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Не удалось установить размер буфера приёма"
        });
    }
    
    return {};
}

Result<void> UdpSocket::set_send_buffer_size(std::size_t size) {
    if (impl_->fd < 0) {
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, "Сокет не открыт"});
    }
    
    int buf_size = static_cast<int>(size);
    if (setsockopt(impl_->fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size)) < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Не удалось установить размер буфера отправки"
        });
    }
    
    return {};
}

Result<void> UdpSocket::set_broadcast(bool enable) {
    if (impl_->fd < 0) {
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, "Сокет не открыт"});
    }
    
    int val = enable ? 1 : 0;
    if (setsockopt(impl_->fd, SOL_SOCKET, SO_BROADCAST, &val, sizeof(val)) < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Не удалось установить broadcast"
        });
    }
    
    return {};
}

Result<void> UdpSocket::set_reuse_address(bool enable) {
    if (impl_->fd < 0) {
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, "Сокет не открыт"});
    }
    
    int val = enable ? 1 : 0;
    if (setsockopt(impl_->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        return std::unexpected(Error{
            ErrorCode::NetworkConnectionFailed,
            "Не удалось установить reuse address"
        });
    }
    
    return {};
}

const UdpStats& UdpSocket::stats() const noexcept {
    return impl_->stats_;
}

void UdpSocket::reset_stats() {
    impl_->stats_ = UdpStats{};
}

void UdpSocket::set_error_callback(UdpErrorCallback callback) {
    impl_->error_callback = std::move(callback);
}

} // namespace quaxis::relay
