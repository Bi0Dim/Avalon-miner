/**
 * @file shm_subscriber.hpp
 * @brief Подписчик на Shared Memory для получения новых блоков
 * 
 * Shared Memory обеспечивает минимальную латентность (~100 нс)
 * при получении уведомлений о новых блоках от модифицированного Bitcoin Core.
 * 
 * Структура QuaxisSharedBlock:
 * - sequence: атомарный счётчик для детекции изменений
 * - state: состояние блока (empty, speculative, confirmed, invalid)
 * - header_raw: 80-байтный заголовок блока
 * - height, bits, timestamp, coinbase_value
 * - block_hash: хеш блока
 * 
 * Режимы работы:
 * 1. Spin-wait: минимальная латентность, но высокое использование CPU
 * 2. Poll: периодическая проверка с заданным интервалом
 */

#pragma once

#include "../core/types.hpp"
#include "../core/config.hpp"
#include "block.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace quaxis::bitcoin {

// =============================================================================
// Константы Shared Memory
// =============================================================================

/**
 * @brief Состояние блока в shared memory
 */
enum class ShmBlockState : uint8_t {
    Empty = 0,       ///< Нет данных
    Speculative = 1, ///< Spy mining: блок получен, но не валидирован
    Confirmed = 2,   ///< Блок полностью валидирован
    Invalid = 3      ///< Блок оказался невалидным
};

// =============================================================================
// Структура Shared Memory блока
// =============================================================================

/**
 * @brief Структура данных в shared memory
 * 
 * Выравнивание по 64-байтным cache lines для избежания false sharing.
 */
struct alignas(64) QuaxisSharedBlock {
    /// @brief Атомарный sequence counter для детекции изменений
    alignas(64) std::atomic<uint64_t> sequence;
    
    /// @brief Состояние блока
    alignas(64) std::atomic<uint8_t> state;
    
    /// @brief Заголовок блока (80 байт)
    alignas(64) uint8_t header_raw[80];
    
    /// @brief Высота блока
    uint32_t height;
    
    /// @brief Compact target (bits)
    uint32_t bits;
    
    /// @brief Timestamp блока
    uint32_t timestamp;
    
    /// @brief Padding для выравнивания
    uint32_t reserved;
    
    /// @brief Награда за блок (satoshi)
    int64_t coinbase_value;
    
    /// @brief Хеш блока
    uint8_t block_hash[32];
};

static_assert(sizeof(QuaxisSharedBlock) <= 256, "QuaxisSharedBlock превышает 256 байт");

// =============================================================================
// Callback тип
// =============================================================================

/**
 * @brief Callback при получении нового блока
 * 
 * @param header Десериализованный заголовок блока
 * @param height Высота блока
 * @param coinbase_value Награда за блок
 * @param is_speculative true если это spy mining (блок ещё не валидирован)
 */
using NewBlockCallback = std::function<void(
    const BlockHeader& header,
    uint32_t height,
    int64_t coinbase_value,
    bool is_speculative
)>;

// =============================================================================
// Shared Memory Subscriber
// =============================================================================

/**
 * @brief Подписчик на shared memory для получения новых блоков
 * 
 * Работает в отдельном потоке, мониторит shared memory
 * и вызывает callback при появлении новых блоков.
 */
class ShmSubscriber {
public:
    /**
     * @brief Создать подписчик
     * 
     * @param config Конфигурация shared memory
     */
    explicit ShmSubscriber(const ShmConfig& config);
    
    /**
     * @brief Деструктор - останавливает подписчик
     */
    ~ShmSubscriber();
    
    // Запрещаем копирование
    ShmSubscriber(const ShmSubscriber&) = delete;
    ShmSubscriber& operator=(const ShmSubscriber&) = delete;
    
    /**
     * @brief Установить callback для новых блоков
     * 
     * @param callback Функция обработки новых блоков
     */
    void set_callback(NewBlockCallback callback);
    
    /**
     * @brief Запустить подписчик
     * 
     * @return Result<void> Успех или ошибка открытия shared memory
     */
    [[nodiscard]] Result<void> start();
    
    /**
     * @brief Остановить подписчик
     */
    void stop();
    
    /**
     * @brief Проверить, запущен ли подписчик
     */
    [[nodiscard]] bool is_running() const noexcept;
    
    /**
     * @brief Получить текущий sequence number
     */
    [[nodiscard]] uint64_t get_sequence() const noexcept;
    
    /**
     * @brief Получить последний известный блок (если есть)
     */
    [[nodiscard]] std::optional<BlockHeader> get_last_block() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// =============================================================================
// Фабричные функции
// =============================================================================

/**
 * @brief Создать shared memory сегмент (для Bitcoin Core патча)
 * 
 * @param path Путь к shared memory (/quaxis_block)
 * @return Result<void> Успех или ошибка
 */
[[nodiscard]] Result<void> create_shm_segment(std::string_view path);

/**
 * @brief Удалить shared memory сегмент
 * 
 * @param path Путь к shared memory
 * @return Result<void> Успех или ошибка
 */
[[nodiscard]] Result<void> remove_shm_segment(std::string_view path);

} // namespace quaxis::bitcoin
