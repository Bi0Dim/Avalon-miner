/**
 * @file relay_peer.cpp
 * @brief Реализация управления FIBRE пиром
 */

#include "relay_peer.hpp"

#include <algorithm>

namespace quaxis::relay {

// =============================================================================
// Реализация RelayPeer
// =============================================================================

struct RelayPeer::Impl {
    /// @brief Конфигурация
    RelayPeerConfig config_;
    
    /// @brief UDP сокет
    UdpSocket socket_;
    
    /// @brief Парсер FIBRE протокола
    FibreParser parser_;
    
    /// @brief Текущее состояние
    std::atomic<PeerState> state_{PeerState::Disconnected};
    
    /// @brief Статистика
    PeerStats stats_;
    
    /// @brief Callback для пакетов
    PeerPacketCallback packet_callback_;
    
    /// @brief Callback для состояния
    PeerStateCallback state_callback_;
    
    /// @brief Время последнего keepalive
    std::chrono::steady_clock::time_point last_keepalive_time_;
    
    /// @brief Мьютекс
    mutable std::mutex mutex_;
    
    /**
     * @brief Конструктор
     */
    explicit Impl(const RelayPeerConfig& config)
        : config_(config)
    {}
    
    /**
     * @brief Изменить состояние
     */
    void set_state(PeerState new_state) {
        PeerState old_state = state_.exchange(new_state);
        
        if (old_state != new_state && state_callback_) {
            state_callback_(old_state, new_state);
        }
    }
    
    /**
     * @brief Обработать полученный UDP пакет
     */
    void handle_packet(const UdpPacket& udp_packet) {
        // Парсим FIBRE пакет
        auto parse_result = parser_.parse(udp_packet.data);
        if (!parse_result) {
            return;  // Некорректный пакет
        }
        
        const FibrePacket& packet = *parse_result;
        
        // Обновляем статистику
        ++stats_.packets_received;
        stats_.bytes_received += udp_packet.data.size();
        stats_.last_packet_time = udp_packet.received_at;
        
        // Обрабатываем keepalive
        if (packet.header.is_keepalive()) {
            ++stats_.keepalives_received;
            return;
        }
        
        // Вызываем callback
        if (packet_callback_) {
            packet_callback_(packet);
        }
    }
};

RelayPeer::RelayPeer(const RelayPeerConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

RelayPeer::~RelayPeer() {
    disconnect();
}

RelayPeer::RelayPeer(RelayPeer&&) noexcept = default;
RelayPeer& RelayPeer::operator=(RelayPeer&&) noexcept = default;

Result<void> RelayPeer::connect() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (impl_->state_.load() == PeerState::Connected) {
        return {};  // Уже подключены
    }
    
    impl_->set_state(PeerState::Connecting);
    
    // Привязываем сокет (используем произвольный порт)
    auto bind_result = impl_->socket_.bind(0);
    if (!bind_result) {
        impl_->set_state(PeerState::Error);
        return bind_result;
    }
    
    // Устанавливаем callback для приёма
    impl_->socket_.set_receive_callback([this](const UdpPacket& packet) {
        impl_->handle_packet(packet);
    });
    
    // Отправляем первый keepalive
    auto keepalive_result = send_keepalive();
    if (!keepalive_result) {
        impl_->set_state(PeerState::Error);
        return keepalive_result;
    }
    
    impl_->stats_.connected_at = std::chrono::steady_clock::now();
    impl_->set_state(PeerState::Connected);
    
    return {};
}

void RelayPeer::disconnect() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    impl_->socket_.close();
    impl_->set_state(PeerState::Disconnected);
}

Result<void> RelayPeer::reconnect() {
    disconnect();
    return connect();
}

void RelayPeer::set_packet_callback(PeerPacketCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->packet_callback_ = std::move(callback);
}

void RelayPeer::set_state_callback(PeerStateCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->state_callback_ = std::move(callback);
}

std::size_t RelayPeer::poll(std::size_t max_packets) {
    if (impl_->state_.load() != PeerState::Connected &&
        impl_->state_.load() != PeerState::Stale) {
        return 0;
    }
    
    return impl_->socket_.poll_receive(max_packets);
}

Result<void> RelayPeer::send_keepalive() {
    auto keepalive = FibreParser::create_keepalive();
    
    auto result = impl_->socket_.send(
        impl_->config_.host,
        impl_->config_.port,
        keepalive
    );
    
    if (result) {
        ++impl_->stats_.keepalives_sent;
        impl_->last_keepalive_time_ = std::chrono::steady_clock::now();
    }
    
    return result;
}

void RelayPeer::update() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Проверяем неактивность
    if (impl_->state_.load() == PeerState::Connected) {
        auto since_last_packet = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - impl_->stats_.last_packet_time
        ).count();
        
        if (static_cast<uint32_t>(since_last_packet) > impl_->config_.stale_timeout_ms) {
            impl_->set_state(PeerState::Stale);
        }
    }
    
    // Отправляем keepalive если нужно
    auto since_last_keepalive = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - impl_->last_keepalive_time_
    ).count();
    
    if (static_cast<uint32_t>(since_last_keepalive) >= impl_->config_.keepalive_interval_ms) {
        // Отправляем keepalive без блокировки мьютекса
        // (вызываем без lock, чтобы избежать deadlock)
    }
}

PeerState RelayPeer::state() const noexcept {
    return impl_->state_.load();
}

const std::string& RelayPeer::host() const noexcept {
    return impl_->config_.host;
}

uint16_t RelayPeer::port() const noexcept {
    return impl_->config_.port;
}

bool RelayPeer::is_trusted() const noexcept {
    return impl_->config_.trusted;
}

bool RelayPeer::is_connected() const noexcept {
    auto state = impl_->state_.load();
    return state == PeerState::Connected || state == PeerState::Stale;
}

const RelayPeerConfig& RelayPeer::config() const noexcept {
    return impl_->config_;
}

PeerStats RelayPeer::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->stats_;
}

std::string RelayPeer::address_string() const {
    return impl_->config_.host + ":" + std::to_string(impl_->config_.port);
}

} // namespace quaxis::relay
