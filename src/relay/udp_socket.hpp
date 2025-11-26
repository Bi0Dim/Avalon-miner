/**
 * @file udp_socket.hpp
 * @brief Асинхронный UDP сокет для FIBRE протокола
 * 
 * Обёртка над системными UDP сокетами с поддержкой:
 * - Асинхронного приёма пакетов
 * - Множественных endpoint'ов
 * - Статистики (latency, packet loss)
 * - Non-blocking операций
 * 
 * @note Для асинхронной работы требуется внешний event loop.
 *       Поддерживается как select/poll, так и Boost.Asio.
 */

#pragma once

#include "../core/types.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <chrono>

namespace quaxis::relay {

// =============================================================================
// Константы
// =============================================================================

/// @brief Максимальный размер UDP пакета (MTU)
inline constexpr std::size_t MAX_UDP_PACKET_SIZE = 1500;

/// @brief Размер буфера приёма
inline constexpr std::size_t UDP_RECV_BUFFER_SIZE = 65536;

/// @brief Таймаут по умолчанию для операций
inline constexpr uint32_t DEFAULT_UDP_TIMEOUT_MS = 5000;

// =============================================================================
// Структуры данных
// =============================================================================

/**
 * @brief Адрес UDP endpoint
 */
struct UdpEndpoint {
    /// @brief IP адрес или hostname
    std::string host;
    
    /// @brief Порт
    uint16_t port{0};
    
    /**
     * @brief Создать endpoint
     */
    UdpEndpoint() = default;
    
    /**
     * @brief Создать endpoint с адресом и портом
     */
    UdpEndpoint(std::string h, uint16_t p) 
        : host(std::move(h)), port(p) {}
    
    /**
     * @brief Сравнение endpoint'ов
     */
    [[nodiscard]] bool operator==(const UdpEndpoint& other) const {
        return host == other.host && port == other.port;
    }
    
    /**
     * @brief Строковое представление
     */
    [[nodiscard]] std::string to_string() const {
        return host + ":" + std::to_string(port);
    }
};

/**
 * @brief Принятый UDP пакет
 */
struct UdpPacket {
    /// @brief Данные пакета
    std::vector<uint8_t> data;
    
    /// @brief Адрес отправителя
    UdpEndpoint sender;
    
    /// @brief Время получения
    std::chrono::steady_clock::time_point received_at;
    
    /**
     * @brief Размер данных
     */
    [[nodiscard]] std::size_t size() const noexcept { return data.size(); }
    
    /**
     * @brief Пакет пустой?
     */
    [[nodiscard]] bool empty() const noexcept { return data.empty(); }
};

/**
 * @brief Статистика UDP сокета
 */
struct UdpStats {
    /// @brief Всего принято пакетов
    uint64_t packets_received{0};
    
    /// @brief Всего отправлено пакетов
    uint64_t packets_sent{0};
    
    /// @brief Всего принято байт
    uint64_t bytes_received{0};
    
    /// @brief Всего отправлено байт
    uint64_t bytes_sent{0};
    
    /// @brief Количество ошибок приёма
    uint64_t recv_errors{0};
    
    /// @brief Количество ошибок отправки
    uint64_t send_errors{0};
    
    /// @brief Время последнего пакета
    std::chrono::steady_clock::time_point last_packet_time;
};

// =============================================================================
// Callback типы
// =============================================================================

/**
 * @brief Callback при получении пакета
 * 
 * @param packet Полученный пакет
 */
using UdpReceiveCallback = std::function<void(const UdpPacket& packet)>;

/**
 * @brief Callback при ошибке
 * 
 * @param error_message Описание ошибки
 */
using UdpErrorCallback = std::function<void(const std::string& error_message)>;

// =============================================================================
// Класс UDP сокета
// =============================================================================

/**
 * @brief Асинхронный UDP сокет
 * 
 * Поддерживает:
 * - Привязку к локальному порту
 * - Асинхронный приём пакетов
 * - Отправку на указанные endpoint'ы
 * - Статистику операций
 * 
 * Thread-safety: класс НЕ является потокобезопасным.
 * Все операции должны выполняться из одного потока или с внешней синхронизацией.
 */
class UdpSocket {
public:
    /**
     * @brief Создать UDP сокет
     */
    UdpSocket();
    
    /**
     * @brief Деструктор - закрывает сокет
     */
    ~UdpSocket();
    
    // Запрещаем копирование
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    
    // Разрешаем перемещение
    UdpSocket(UdpSocket&&) noexcept;
    UdpSocket& operator=(UdpSocket&&) noexcept;
    
    // =========================================================================
    // Управление сокетом
    // =========================================================================
    
    /**
     * @brief Привязать сокет к локальному порту
     * 
     * @param port Локальный порт (0 = автовыбор)
     * @param bind_address Адрес для привязки ("0.0.0.0" = все интерфейсы)
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> bind(uint16_t port, const std::string& bind_address = "0.0.0.0");
    
    /**
     * @brief Закрыть сокет
     */
    void close();
    
    /**
     * @brief Сокет открыт?
     */
    [[nodiscard]] bool is_open() const noexcept;
    
    /**
     * @brief Получить локальный порт
     */
    [[nodiscard]] uint16_t local_port() const noexcept;
    
    // =========================================================================
    // Приём пакетов
    // =========================================================================
    
    /**
     * @brief Попытаться принять пакет (non-blocking)
     * 
     * @return Пакет или nullopt если нет данных
     */
    [[nodiscard]] std::optional<UdpPacket> try_receive();
    
    /**
     * @brief Принять пакет с таймаутом
     * 
     * @param timeout_ms Таймаут в миллисекундах
     * @return Пакет или ошибка
     */
    [[nodiscard]] Result<UdpPacket> receive(uint32_t timeout_ms = DEFAULT_UDP_TIMEOUT_MS);
    
    /**
     * @brief Установить callback для асинхронного приёма
     * 
     * @param callback Функция обработки пакетов
     */
    void set_receive_callback(UdpReceiveCallback callback);
    
    /**
     * @brief Обработать входящие пакеты (вызывает callbacks)
     * 
     * Должен вызываться периодически из event loop.
     * 
     * @param max_packets Максимальное количество пакетов за вызов
     * @return Количество обработанных пакетов
     */
    std::size_t poll_receive(std::size_t max_packets = 100);
    
    // =========================================================================
    // Отправка пакетов
    // =========================================================================
    
    /**
     * @brief Отправить пакет
     * 
     * @param endpoint Адрес получателя
     * @param data Данные для отправки
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> send(const UdpEndpoint& endpoint, ByteSpan data);
    
    /**
     * @brief Отправить пакет по адресу
     * 
     * @param host Хост получателя
     * @param port Порт получателя
     * @param data Данные для отправки
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> send(const std::string& host, uint16_t port, ByteSpan data);
    
    // =========================================================================
    // Опции сокета
    // =========================================================================
    
    /**
     * @brief Установить размер буфера приёма
     * 
     * @param size Размер буфера в байтах
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> set_recv_buffer_size(std::size_t size);
    
    /**
     * @brief Установить размер буфера отправки
     * 
     * @param size Размер буфера в байтах
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> set_send_buffer_size(std::size_t size);
    
    /**
     * @brief Включить/выключить broadcast
     * 
     * @param enable Включить broadcast
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> set_broadcast(bool enable);
    
    /**
     * @brief Включить/выключить reuse address
     * 
     * @param enable Включить reuse
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> set_reuse_address(bool enable);
    
    // =========================================================================
    // Статистика
    // =========================================================================
    
    /**
     * @brief Получить статистику сокета
     */
    [[nodiscard]] const UdpStats& stats() const noexcept;
    
    /**
     * @brief Сбросить статистику
     */
    void reset_stats();
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback ошибок
     */
    void set_error_callback(UdpErrorCallback callback);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Утилиты
// =============================================================================

/**
 * @brief Разрезолвить hostname в IP адрес
 * 
 * @param hostname Имя хоста или IP адрес
 * @return IP адрес или ошибка
 */
[[nodiscard]] Result<std::string> resolve_hostname(const std::string& hostname);

/**
 * @brief Проверить, является ли строка валидным IP адресом
 * 
 * @param address Адрес для проверки
 * @return true если это валидный IPv4 или IPv6 адрес
 */
[[nodiscard]] bool is_valid_ip(const std::string& address);

} // namespace quaxis::relay
