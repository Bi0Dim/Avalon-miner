/**
 * @file block_reconstructor.hpp
 * @brief Реконструкция блока из FIBRE чанков
 * 
 * Отвечает за:
 * - Сбор чанков от FIBRE пиров
 * - Использование FEC для восстановления потерянных чанков
 * - Раннее извлечение block header (для Spy Mining)
 * - Уведомление о готовности блока
 * 
 * Ключевая оптимизация: извлечение block header из ПЕРВЫХ чанков,
 * не дожидаясь полного блока. Block header всегда в начале (первые 80 байт),
 * что позволяет начать Spy Mining максимально рано.
 */

#pragma once

#include "../core/types.hpp"
#include "../bitcoin/block.hpp"
#include "fec_decoder.hpp"
#include "fibre_protocol.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace quaxis::relay {

// =============================================================================
// Callback типы
// =============================================================================

/**
 * @brief Callback при получении block header
 * 
 * Вызывается как можно раньше, когда получено достаточно данных
 * для извлечения 80-байтного заголовка блока.
 * 
 * @param header Заголовок блока
 * @param height Высота блока
 * @param block_hash Хеш блока
 */
using HeaderCallback = std::function<void(
    const bitcoin::BlockHeader& header,
    uint32_t height,
    const Hash256& block_hash
)>;

/**
 * @brief Callback при полной реконструкции блока
 * 
 * @param data Полные данные блока
 * @param height Высота блока
 * @param block_hash Хеш блока
 */
using BlockCallback = std::function<void(
    const std::vector<uint8_t>& data,
    uint32_t height,
    const Hash256& block_hash
)>;

/**
 * @brief Callback при таймауте реконструкции
 * 
 * @param height Высота блока
 * @param block_hash Хеш блока
 * @param chunks_received Количество полученных чанков
 * @param chunks_total Общее количество чанков
 */
using TimeoutCallback = std::function<void(
    uint32_t height,
    const Hash256& block_hash,
    std::size_t chunks_received,
    std::size_t chunks_total
)>;

// =============================================================================
// Статус реконструкции
// =============================================================================

/**
 * @brief Состояние реконструкции блока
 */
enum class ReconstructionState {
    /// @brief Ожидание чанков
    Waiting,
    
    /// @brief Header получен, ожидание остальных данных
    HeaderReceived,
    
    /// @brief Блок полностью реконструирован
    Complete,
    
    /// @brief Таймаут реконструкции
    Timeout,
    
    /// @brief Ошибка (невозможно реконструировать)
    Failed
};

/**
 * @brief Статистика реконструкции
 */
struct ReconstructionStats {
    /// @brief Время начала реконструкции
    std::chrono::steady_clock::time_point start_time;
    
    /// @brief Время получения header
    std::chrono::steady_clock::time_point header_time;
    
    /// @brief Время завершения
    std::chrono::steady_clock::time_point complete_time;
    
    /// @brief Количество полученных data чанков
    std::size_t data_chunks_received{0};
    
    /// @brief Количество полученных FEC чанков
    std::size_t fec_chunks_received{0};
    
    /// @brief Количество восстановленных чанков
    std::size_t chunks_recovered{0};
    
    /// @brief Количество дубликатов
    std::size_t duplicates{0};
    
    /**
     * @brief Время до получения header (мс)
     */
    [[nodiscard]] double header_latency_ms() const {
        if (header_time.time_since_epoch().count() == 0) return -1.0;
        return std::chrono::duration<double, std::milli>(header_time - start_time).count();
    }
    
    /**
     * @brief Время полной реконструкции (мс)
     */
    [[nodiscard]] double total_latency_ms() const {
        if (complete_time.time_since_epoch().count() == 0) return -1.0;
        return std::chrono::duration<double, std::milli>(complete_time - start_time).count();
    }
};

// =============================================================================
// Класс реконструктора блока
// =============================================================================

/**
 * @brief Реконструктор блока из FIBRE чанков
 * 
 * Собирает чанки блока от различных источников и реконструирует блок.
 * Поддерживает FEC для восстановления потерянных чанков.
 * 
 * Thread-safety: методы thread-safe благодаря внутренней синхронизации.
 */
class BlockReconstructor {
public:
    /**
     * @brief Создать реконструктор
     * 
     * @param block_hash Хеш блока для реконструкции
     * @param height Высота блока
     * @param fec_params Параметры FEC
     * @param timeout_ms Таймаут реконструкции в миллисекундах
     */
    BlockReconstructor(
        const Hash256& block_hash,
        uint32_t height,
        const FecParams& fec_params,
        uint32_t timeout_ms = 5000
    );
    
    /**
     * @brief Деструктор
     */
    ~BlockReconstructor();
    
    // Запрещаем копирование
    BlockReconstructor(const BlockReconstructor&) = delete;
    BlockReconstructor& operator=(const BlockReconstructor&) = delete;
    
    // Разрешаем перемещение
    BlockReconstructor(BlockReconstructor&&) noexcept;
    BlockReconstructor& operator=(BlockReconstructor&&) noexcept;
    
    // =========================================================================
    // Получение чанков
    // =========================================================================
    
    /**
     * @brief Обработать полученный FIBRE пакет
     * 
     * @param packet FIBRE пакет
     * @return true если пакет был обработан
     */
    bool on_packet(const FibrePacket& packet);
    
    /**
     * @brief Обработать чанк напрямую
     * 
     * @param chunk_id ID чанка
     * @param is_fec Это FEC чанк?
     * @param data Данные чанка
     * @return true если чанк был обработан
     */
    bool on_chunk(uint16_t chunk_id, bool is_fec, ByteSpan data);
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    /**
     * @brief Установить callback для получения header
     * 
     * @param callback Функция-обработчик
     */
    void set_header_callback(HeaderCallback callback);
    
    /**
     * @brief Установить callback для полного блока
     * 
     * @param callback Функция-обработчик
     */
    void set_block_callback(BlockCallback callback);
    
    /**
     * @brief Установить callback для таймаута
     * 
     * @param callback Функция-обработчик
     */
    void set_timeout_callback(TimeoutCallback callback);
    
    // =========================================================================
    // Статус
    // =========================================================================
    
    /**
     * @brief Текущее состояние реконструкции
     */
    [[nodiscard]] ReconstructionState state() const;
    
    /**
     * @brief Header уже получен?
     */
    [[nodiscard]] bool has_header() const;
    
    /**
     * @brief Блок полностью реконструирован?
     */
    [[nodiscard]] bool is_complete() const;
    
    /**
     * @brief Таймаут истёк?
     */
    [[nodiscard]] bool is_timed_out() const;
    
    /**
     * @brief Можно ли попытаться декодировать?
     */
    [[nodiscard]] bool can_try_decode() const;
    
    // =========================================================================
    // Информация
    // =========================================================================
    
    /**
     * @brief Хеш блока
     */
    [[nodiscard]] const Hash256& block_hash() const noexcept;
    
    /**
     * @brief Высота блока
     */
    [[nodiscard]] uint32_t height() const noexcept;
    
    /**
     * @brief Статистика реконструкции
     */
    [[nodiscard]] ReconstructionStats stats() const;
    
    /**
     * @brief Проверить таймаут и вызвать callback если нужно
     * 
     * Должен вызываться периодически из event loop.
     */
    void check_timeout();
    
    /**
     * @brief Попытаться завершить реконструкцию
     * 
     * Вызывает FEC декодирование если достаточно чанков.
     * 
     * @return true если блок успешно реконструирован
     */
    bool try_complete();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::relay
