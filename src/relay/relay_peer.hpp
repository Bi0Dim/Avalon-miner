/**
 * @file relay_peer.hpp
 * @brief Управление одним FIBRE пиром
 * 
 * Отвечает за:
 * - UDP подключение к FIBRE пиру
 * - Heartbeat / keepalive
 * - Статистика (latency, packet loss)
 * - Автоматическое переподключение
 * 
 * FIBRE пиры - это серверы, распространяющие новые блоки
 * через UDP с использованием FEC для надёжности.
 */

#pragma once

#include "../core/types.hpp"
#include "udp_socket.hpp"
#include "fibre_protocol.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <atomic>

namespace quaxis::relay {

// =============================================================================
// Состояние пира
// =============================================================================

/**
 * @brief Состояние подключения к пиру
 */
enum class PeerState {
    /// @brief Отключен
    Disconnected,
    
    /// @brief Подключение в процессе
    Connecting,
    
    /// @brief Подключен и активен
    Connected,
    
    /// @brief Подключен, но неактивен (нет данных)
    Stale,
    
    /// @brief Ошибка подключения
    Error
};

/**
 * @brief Статистика пира
 */
struct PeerStats {
    /// @brief Всего получено пакетов
    uint64_t packets_received{0};
    
    /// @brief Всего получено байт
    uint64_t bytes_received{0};
    
    /// @brief Количество полученных блоков
    uint32_t blocks_received{0};
    
    /// @brief Количество keepalive
    uint32_t keepalives_sent{0};
    
    /// @brief Количество keepalive ответов
    uint32_t keepalives_received{0};
    
    /// @brief Средняя задержка (мс)
    double avg_latency_ms{0.0};
    
    /// @brief Минимальная задержка (мс)
    double min_latency_ms{0.0};
    
    /// @brief Максимальная задержка (мс)
    double max_latency_ms{0.0};
    
    /// @brief Потеря пакетов (0.0 - 1.0)
    double packet_loss{0.0};
    
    /// @brief Время последнего пакета
    std::chrono::steady_clock::time_point last_packet_time;
    
    /// @brief Время подключения
    std::chrono::steady_clock::time_point connected_at;
    
    /// @brief Время работы в секундах
    [[nodiscard]] double uptime_seconds() const {
        if (connected_at.time_since_epoch().count() == 0) {
            return 0.0;
        }
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - connected_at
        ).count();
    }
};

// =============================================================================
// Callback типы
// =============================================================================

/**
 * @brief Callback при получении FIBRE пакета
 */
using PeerPacketCallback = std::function<void(const FibrePacket& packet)>;

/**
 * @brief Callback при изменении состояния пира
 */
using PeerStateCallback = std::function<void(PeerState old_state, PeerState new_state)>;

// =============================================================================
// Конфигурация пира
// =============================================================================

/**
 * @brief Конфигурация FIBRE пира
 */
struct RelayPeerConfig {
    /// @brief Хост (IP или hostname)
    std::string host;
    
    /// @brief Порт
    uint16_t port{8336};
    
    /// @brief Доверенный пир
    bool trusted{false};
    
    /// @brief Интервал keepalive (мс)
    uint32_t keepalive_interval_ms{10000};
    
    /// @brief Таймаут неактивности (мс)
    uint32_t stale_timeout_ms{30000};
    
    /// @brief Таймаут переподключения (мс)
    uint32_t reconnect_timeout_ms{5000};
    
    /// @brief Включить автопереподключение
    bool auto_reconnect{true};
};

// =============================================================================
// Класс RelayPeer
// =============================================================================

/**
 * @brief Управление одним FIBRE пиром
 * 
 * Thread-safety: методы thread-safe.
 */
class RelayPeer {
public:
    /**
     * @brief Создать пир
     * 
     * @param config Конфигурация пира
     */
    explicit RelayPeer(const RelayPeerConfig& config);
    
    /**
     * @brief Деструктор - отключает пир
     */
    ~RelayPeer();
    
    // Запрещаем копирование
    RelayPeer(const RelayPeer&) = delete;
    RelayPeer& operator=(const RelayPeer&) = delete;
    
    // Разрешаем перемещение
    RelayPeer(RelayPeer&&) noexcept;
    RelayPeer& operator=(RelayPeer&&) noexcept;
    
    // =========================================================================
    // Управление подключением
    // =========================================================================
    
    /**
     * @brief Подключиться к пиру
     * 
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> connect();
    
    /**
     * @brief Отключиться от пира
     */
    void disconnect();
    
    /**
     * @brief Переподключиться
     * 
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> reconnect();
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для пакетов
     */
    void set_packet_callback(PeerPacketCallback callback);
    
    /**
     * @brief Установить callback для изменения состояния
     */
    void set_state_callback(PeerStateCallback callback);
    
    // =========================================================================
    // Операции
    // =========================================================================
    
    /**
     * @brief Обработать входящие пакеты
     * 
     * Должен вызываться периодически из event loop.
     * 
     * @param max_packets Максимальное количество пакетов
     * @return Количество обработанных пакетов
     */
    std::size_t poll(std::size_t max_packets = 100);
    
    /**
     * @brief Отправить keepalive
     * 
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> send_keepalive();
    
    /**
     * @brief Обновить состояние (проверить таймауты)
     * 
     * Должен вызываться периодически.
     */
    void update();
    
    // =========================================================================
    // Информация
    // =========================================================================
    
    /**
     * @brief Текущее состояние
     */
    [[nodiscard]] PeerState state() const noexcept;
    
    /**
     * @brief Хост пира
     */
    [[nodiscard]] const std::string& host() const noexcept;
    
    /**
     * @brief Порт пира
     */
    [[nodiscard]] uint16_t port() const noexcept;
    
    /**
     * @brief Доверенный пир?
     */
    [[nodiscard]] bool is_trusted() const noexcept;
    
    /**
     * @brief Пир подключен?
     */
    [[nodiscard]] bool is_connected() const noexcept;
    
    /**
     * @brief Конфигурация пира
     */
    [[nodiscard]] const RelayPeerConfig& config() const noexcept;
    
    /**
     * @brief Статистика пира
     */
    [[nodiscard]] PeerStats stats() const;
    
    /**
     * @brief Адрес пира как строка
     */
    [[nodiscard]] std::string address_string() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::relay
