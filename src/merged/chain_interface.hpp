/**
 * @file chain_interface.hpp
 * @brief Интерфейс для auxiliary chains в merged mining
 * 
 * Определяет базовый интерфейс для всех auxiliary chains,
 * которые могут быть использованы для merged mining с Bitcoin.
 */

#pragma once

#include "../core/types.hpp"
#include "auxpow.hpp"

#include <string>
#include <optional>
#include <chrono>

namespace quaxis::merged {

// =============================================================================
// Статус chain
// =============================================================================

/**
 * @brief Статус подключения к auxiliary chain
 */
enum class ChainStatus {
    Disconnected,   ///< Нет соединения с RPC
    Connecting,     ///< Процесс подключения
    Syncing,        ///< Синхронизация блокчейна
    Ready,          ///< Готов к майнингу
    Error           ///< Ошибка подключения
};

/**
 * @brief Преобразование статуса в строку
 */
[[nodiscard]] constexpr std::string_view to_string(ChainStatus status) noexcept {
    switch (status) {
        case ChainStatus::Disconnected: return "Disconnected";
        case ChainStatus::Connecting: return "Connecting";
        case ChainStatus::Syncing: return "Syncing";
        case ChainStatus::Ready: return "Ready";
        case ChainStatus::Error: return "Error";
        default: return "Unknown";
    }
}

// =============================================================================
// Информация о chain
// =============================================================================

/**
 * @brief Информация о auxiliary chain
 */
struct ChainInfo {
    /// @brief Название chain
    std::string name;
    
    /// @brief Тикер (символ валюты)
    std::string ticker;
    
    /// @brief Алгоритм хеширования
    std::string algorithm{"SHA-256"};
    
    /// @brief Текущий статус
    ChainStatus status{ChainStatus::Disconnected};
    
    /// @brief Текущая высота блока
    uint32_t height{0};
    
    /// @brief Текущая сложность
    double difficulty{0.0};
    
    /// @brief Награда за блок
    double block_reward{0.0};
    
    /// @brief Время последнего обновления
    std::chrono::steady_clock::time_point last_update{};
};

// =============================================================================
// Шаблон блока auxiliary chain
// =============================================================================

/**
 * @brief Шаблон блока для auxiliary chain
 */
struct AuxBlockTemplate {
    /// @brief Хеш блока для включения в AuxPoW
    Hash256 block_hash{};
    
    /// @brief Chain ID (обычно hash genesis block)
    Hash256 chain_id{};
    
    /// @brief Target в compact формате
    uint32_t target_bits{0};
    
    /// @brief Высота блока
    uint32_t height{0};
    
    /// @brief Дополнительные данные (chain-specific)
    Bytes extra_data;
    
    /// @brief Время создания шаблона
    std::chrono::steady_clock::time_point created_at{
        std::chrono::steady_clock::now()
    };
    
    /**
     * @brief Проверить, устарел ли шаблон
     * 
     * @param max_age Максимальный возраст в секундах
     * @return true если шаблон устарел
     */
    [[nodiscard]] bool is_stale(std::chrono::seconds max_age) const noexcept {
        auto age = std::chrono::steady_clock::now() - created_at;
        return age > max_age;
    }
};

// =============================================================================
// Интерфейс Chain
// =============================================================================

/**
 * @brief Базовый интерфейс для auxiliary chain
 * 
 * Определяет методы для взаимодействия с auxiliary chain:
 * - Получение информации о chain
 * - Получение шаблона блока
 * - Отправка найденного блока
 */
class IChain {
public:
    virtual ~IChain() = default;
    
    // =========================================================================
    // Информация о chain
    // =========================================================================
    
    /**
     * @brief Получить название chain
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    
    /**
     * @brief Получить тикер (символ валюты)
     */
    [[nodiscard]] virtual std::string_view ticker() const noexcept = 0;
    
    /**
     * @brief Получить Chain ID
     * 
     * Chain ID используется для вычисления slot ID в AuxPoW Merkle tree.
     * Обычно это хеш genesis block.
     */
    [[nodiscard]] virtual const Hash256& chain_id() const noexcept = 0;
    
    /**
     * @brief Получить приоритет chain
     * 
     * Более высокий приоритет = более важный chain.
     * Используется для разрешения коллизий slot ID.
     */
    [[nodiscard]] virtual uint32_t priority() const noexcept = 0;
    
    /**
     * @brief Получить полную информацию о chain
     */
    [[nodiscard]] virtual ChainInfo get_info() const noexcept = 0;
    
    // =========================================================================
    // Статус и подключение
    // =========================================================================
    
    /**
     * @brief Получить текущий статус chain
     */
    [[nodiscard]] virtual ChainStatus status() const noexcept = 0;
    
    /**
     * @brief Подключиться к RPC ноде
     * 
     * @return Result<void> Успех или ошибка подключения
     */
    [[nodiscard]] virtual Result<void> connect() = 0;
    
    /**
     * @brief Отключиться от RPC ноды
     */
    virtual void disconnect() = 0;
    
    /**
     * @brief Проверить соединение
     * 
     * @return true если соединение активно
     */
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;
    
    // =========================================================================
    // Майнинг
    // =========================================================================
    
    /**
     * @brief Получить текущий шаблон блока
     * 
     * @return Result<AuxBlockTemplate> Шаблон или ошибка
     */
    [[nodiscard]] virtual Result<AuxBlockTemplate> get_block_template() = 0;
    
    /**
     * @brief Отправить найденный блок
     * 
     * @param auxpow AuxPoW доказательство
     * @param block_template Исходный шаблон блока
     * @return Result<void> Успех или ошибка отправки
     */
    [[nodiscard]] virtual Result<void> submit_block(
        const AuxPow& auxpow,
        const AuxBlockTemplate& block_template
    ) = 0;
    
    /**
     * @brief Проверить, подходит ли данный хеш для этой chain
     * 
     * @param pow_hash Хеш parent block header (Bitcoin)
     * @param current_template Текущий шаблон блока
     * @return true если хеш меньше target
     */
    [[nodiscard]] virtual bool meets_target(
        const Hash256& pow_hash,
        const AuxBlockTemplate& current_template
    ) const noexcept = 0;
    
    // =========================================================================
    // Конфигурация
    // =========================================================================
    
    /**
     * @brief Включить/выключить chain
     * 
     * @param enabled true для включения
     */
    virtual void set_enabled(bool enabled) = 0;
    
    /**
     * @brief Проверить, включен ли chain
     */
    [[nodiscard]] virtual bool is_enabled() const noexcept = 0;
    
    /**
     * @brief Установить приоритет
     * 
     * @param priority Новый приоритет
     */
    virtual void set_priority(uint32_t priority) = 0;
};

} // namespace quaxis::merged
