/**
 * @file headers_sync.hpp
 * @brief Универсальный синхронизатор заголовков
 * 
 * Предоставляет синхронизацию заголовков для любой AuxPoW монеты.
 */

#pragma once

#include "../chain/chain_params.hpp"
#include "../primitives/block_header.hpp"
#include "headers_store.hpp"

#include <functional>
#include <memory>
#include <atomic>

namespace quaxis::core::sync {

/**
 * @brief Callback при получении нового блока
 */
using NewBlockCallback = std::function<void(const BlockHeader&, uint32_t height)>;

/**
 * @brief Статус синхронизации
 */
enum class SyncStatus {
    Stopped,      ///< Синхронизация остановлена
    Connecting,   ///< Подключение к peer
    Syncing,      ///< Идёт синхронизация
    Synchronized  ///< Полностью синхронизирован
};

/**
 * @brief Преобразование статуса в строку
 */
[[nodiscard]] constexpr std::string_view to_string(SyncStatus status) noexcept {
    switch (status) {
        case SyncStatus::Stopped: return "Stopped";
        case SyncStatus::Connecting: return "Connecting";
        case SyncStatus::Syncing: return "Syncing";
        case SyncStatus::Synchronized: return "Synchronized";
        default: return "Unknown";
    }
}

/**
 * @brief Универсальный синхронизатор заголовков
 * 
 * Работает с ЛЮБОЙ монетой, поддерживающей стандартный Bitcoin протокол
 * сообщений headers. Особенности консенсуса определяются ChainParams.
 */
class HeadersSync {
public:
    /**
     * @brief Создать синхронизатор для chain
     * 
     * @param params Параметры chain
     */
    explicit HeadersSync(const ChainParams& params);
    
    ~HeadersSync();
    
    // Запрещаем копирование
    HeadersSync(const HeadersSync&) = delete;
    HeadersSync& operator=(const HeadersSync&) = delete;
    
    // =========================================================================
    // Управление
    // =========================================================================
    
    /**
     * @brief Запустить синхронизацию
     */
    void start();
    
    /**
     * @brief Остановить синхронизацию
     */
    void stop();
    
    /**
     * @brief Получить текущий статус
     */
    [[nodiscard]] SyncStatus status() const noexcept;
    
    /**
     * @brief Проверить, синхронизирован ли
     */
    [[nodiscard]] bool is_synchronized() const noexcept;
    
    // =========================================================================
    // Данные
    // =========================================================================
    
    /**
     * @brief Получить текущий tip
     * 
     * @return BlockHeader Заголовок вершины цепи
     */
    [[nodiscard]] BlockHeader get_tip() const;
    
    /**
     * @brief Получить высоту tip
     * 
     * @return uint32_t Высота
     */
    [[nodiscard]] uint32_t get_tip_height() const noexcept;
    
    /**
     * @brief Получить хеш tip
     * 
     * @return Hash256 Хеш
     */
    [[nodiscard]] Hash256 get_tip_hash() const;
    
    /**
     * @brief Получить текущий target
     * 
     * @return uint256 256-битный target
     */
    [[nodiscard]] uint256 get_current_target() const;
    
    /**
     * @brief Получить текущий compact target
     * 
     * @return uint32_t nBits
     */
    [[nodiscard]] uint32_t get_current_bits() const noexcept;
    
    /**
     * @brief Получить текущую сложность
     * 
     * @return double Сложность
     */
    [[nodiscard]] double get_difficulty() const noexcept;
    
    /**
     * @brief Получить заголовок по высоте
     * 
     * @param height Высота
     * @return std::optional<BlockHeader> Заголовок или nullopt
     */
    [[nodiscard]] std::optional<BlockHeader> get_header(uint32_t height) const;
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для новых блоков
     * 
     * @param callback Функция, вызываемая при получении нового блока
     */
    void on_new_block(NewBlockCallback callback);
    
    // =========================================================================
    // Обработка сообщений (для P2P модуля)
    // =========================================================================
    
    /**
     * @brief Обработать полученные заголовки
     * 
     * @param headers Список заголовков
     * @return bool true если все заголовки валидны и добавлены
     */
    bool process_headers(const std::vector<BlockHeader>& headers);
    
    /**
     * @brief Получить locator для запроса заголовков
     * 
     * @return std::vector<Hash256> Block locator hashes
     */
    [[nodiscard]] std::vector<Hash256> get_block_locator() const;
    
private:
    const ChainParams& params_;
    
    std::unique_ptr<HeadersStore> store_;
    
    std::atomic<SyncStatus> status_{SyncStatus::Stopped};
    
    NewBlockCallback new_block_callback_;
    
    mutable std::mutex callback_mutex_;
};

} // namespace quaxis::core::sync
