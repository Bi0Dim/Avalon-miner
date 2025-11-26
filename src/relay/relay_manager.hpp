/**
 * @file relay_manager.hpp
 * @brief Менеджер FIBRE relay источников
 * 
 * Центральный компонент UDP relay подсистемы:
 * - Управляет списком FIBRE пиров
 * - Выбирает лучший источник по latency
 * - Дедупликация блоков
 * - Интеграция с Shared Memory для уведомления сервера
 * 
 * Архитектура:
 * ```
 * RelayManager
 *     ├── RelayPeer 1 (fibre.asia.bitcoinfibre.org)
 *     ├── RelayPeer 2 (fibre.eu.bitcoinfibre.org)
 *     └── RelayPeer 3 (fibre.us.bitcoinfibre.org)
 *           │
 *           ▼
 *     BlockReconstructor (для каждого блока)
 *           │
 *           ▼
 *     HeaderCallback / BlockCallback
 * ```
 */

#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "../bitcoin/block.hpp"
#include "relay_peer.hpp"
#include "block_reconstructor.hpp"

#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>

namespace quaxis::relay {

// =============================================================================
// Конфигурация
// =============================================================================

/**
 * @brief Источник блока
 */
enum class BlockSource {
    /// @brief Получен через UDP relay
    UdpRelay,
    
    /// @brief Получен через Bitcoin P2P
    BitcoinP2P,
    
    /// @brief Получен через Shared Memory
    SharedMemory
};

// =============================================================================
// Callback типы
// =============================================================================

/**
 * @brief Callback при получении block header
 * 
 * @param header Заголовок блока
 * @param source Источник блока
 */
using RelayHeaderCallback = std::function<void(
    const bitcoin::BlockHeader& header,
    BlockSource source
)>;

/**
 * @brief Callback при получении полного блока
 * 
 * @param data Данные блока
 * @param height Высота блока
 * @param source Источник блока
 */
using RelayBlockCallback = std::function<void(
    const std::vector<uint8_t>& data,
    uint32_t height,
    BlockSource source
)>;

// =============================================================================
// Статистика менеджера
// =============================================================================

/**
 * @brief Статистика RelayManager
 */
struct RelayManagerStats {
    /// @brief Количество активных пиров
    std::size_t active_peers{0};
    
    /// @brief Всего подключенных пиров
    std::size_t connected_peers{0};
    
    /// @brief Всего получено блоков
    uint64_t blocks_received{0};
    
    /// @brief Количество дубликатов блоков
    uint64_t duplicate_blocks{0};
    
    /// @brief Среднее время получения header (мс)
    double avg_header_latency_ms{0.0};
    
    /// @brief Среднее время полной реконструкции (мс)
    double avg_reconstruction_latency_ms{0.0};
    
    /// @brief Количество таймаутов реконструкции
    uint64_t reconstruction_timeouts{0};
    
    /// @brief Время работы (секунды)
    double uptime_seconds{0.0};
};

// =============================================================================
// Класс RelayManager
// =============================================================================

/**
 * @brief Менеджер FIBRE relay источников
 * 
 * Работает в отдельном потоке, принимает блоки от FIBRE пиров
 * и уведомляет сервер через callbacks.
 * 
 * Thread-safety: публичные методы thread-safe.
 */
class RelayManager {
public:
    /**
     * @brief Создать менеджер relay
     * 
     * @param config Конфигурация relay
     */
    explicit RelayManager(const RelayConfig& config);
    
    /**
     * @brief Деструктор - останавливает менеджер
     */
    ~RelayManager();
    
    // Запрещаем копирование
    RelayManager(const RelayManager&) = delete;
    RelayManager& operator=(const RelayManager&) = delete;
    
    // =========================================================================
    // Управление
    // =========================================================================
    
    /**
     * @brief Запустить менеджер
     * 
     * Запускает рабочий поток и подключается ко всем пирам.
     * 
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> start();
    
    /**
     * @brief Остановить менеджер
     * 
     * Останавливает рабочий поток и отключается от всех пиров.
     */
    void stop();
    
    /**
     * @brief Менеджер запущен?
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для получения header
     * 
     * Вызывается как можно раньше, когда header извлечён.
     * 
     * @param callback Функция обработки
     */
    void set_header_callback(RelayHeaderCallback callback);
    
    /**
     * @brief Установить callback для полного блока
     * 
     * @param callback Функция обработки
     */
    void set_block_callback(RelayBlockCallback callback);
    
    // =========================================================================
    // Управление пирами
    // =========================================================================
    
    /**
     * @brief Добавить пир
     * 
     * @param config Конфигурация пира
     * @return Успех или ошибка
     */
    [[nodiscard]] Result<void> add_peer(const RelayPeerConfig& config);
    
    /**
     * @brief Удалить пир
     * 
     * @param host Хост пира
     * @param port Порт пира
     */
    void remove_peer(const std::string& host, uint16_t port);
    
    /**
     * @brief Получить количество пиров
     */
    [[nodiscard]] std::size_t peer_count() const;
    
    /**
     * @brief Получить количество подключенных пиров
     */
    [[nodiscard]] std::size_t connected_peer_count() const;
    
    // =========================================================================
    // Информация
    // =========================================================================
    
    /**
     * @brief Получить статистику
     */
    [[nodiscard]] RelayManagerStats stats() const;
    
    /**
     * @brief Конфигурация
     */
    [[nodiscard]] const RelayConfig& config() const noexcept;
    
    /**
     * @brief Последняя высота блока
     */
    [[nodiscard]] uint32_t last_block_height() const noexcept;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Вспомогательные функции
// =============================================================================

/**
 * @brief Преобразовать источник блока в строку
 */
[[nodiscard]] constexpr std::string_view to_string(BlockSource source) noexcept {
    switch (source) {
        case BlockSource::UdpRelay: return "UDP Relay";
        case BlockSource::BitcoinP2P: return "Bitcoin P2P";
        case BlockSource::SharedMemory: return "Shared Memory";
        default: return "Unknown";
    }
}

} // namespace quaxis::relay
