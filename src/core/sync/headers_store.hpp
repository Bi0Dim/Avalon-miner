/**
 * @file headers_store.hpp
 * @brief Хранилище заголовков блоков
 * 
 * Предоставляет in-memory хранение и доступ к заголовкам блоков
 * для синхронизации и валидации.
 */

#pragma once

#include "../primitives/block_header.hpp"
#include "../chain/chain_params.hpp"

#include <vector>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <functional>

// Hash для Hash256 - должен быть определён до использования
namespace std {
template<>
struct hash<quaxis::Hash256> {
    std::size_t operator()(const quaxis::Hash256& h) const noexcept {
        // Используем первые 8 байт как hash
        std::size_t result = 0;
        for (std::size_t i = 0; i < sizeof(std::size_t) && i < h.size(); ++i) {
            result |= static_cast<std::size_t>(h[i]) << (i * 8);
        }
        return result;
    }
};
}

namespace quaxis::core::sync {

/**
 * @brief Хранилище заголовков блоков
 * 
 * Хранит заголовки в памяти с индексацией по хешу и высоте.
 */
class HeadersStore {
public:
    /**
     * @brief Создать хранилище для chain
     * 
     * @param params Параметры chain
     */
    explicit HeadersStore(const ChainParams& params);
    
    /**
     * @brief Добавить заголовок
     * 
     * @param header Заголовок блока
     * @param height Высота блока
     * @return true если успешно добавлен
     */
    bool add_header(const BlockHeader& header, uint32_t height);
    
    /**
     * @brief Получить заголовок по хешу
     * 
     * @param hash Хеш блока
     * @return std::optional<BlockHeader> Заголовок или nullopt
     */
    [[nodiscard]] std::optional<BlockHeader> get_by_hash(
        const Hash256& hash
    ) const;
    
    /**
     * @brief Получить заголовок по высоте
     * 
     * @param height Высота блока
     * @return std::optional<BlockHeader> Заголовок или nullopt
     */
    [[nodiscard]] std::optional<BlockHeader> get_by_height(
        uint32_t height
    ) const;
    
    /**
     * @brief Получить высоту блока по хешу
     * 
     * @param hash Хеш блока
     * @return std::optional<uint32_t> Высота или nullopt
     */
    [[nodiscard]] std::optional<uint32_t> get_height(
        const Hash256& hash
    ) const;
    
    /**
     * @brief Получить заголовок вершины цепи
     * 
     * @return const BlockHeader& Заголовок tip
     */
    [[nodiscard]] const BlockHeader& get_tip() const;
    
    /**
     * @brief Получить высоту вершины цепи
     * 
     * @return uint32_t Высота tip
     */
    [[nodiscard]] uint32_t get_tip_height() const noexcept;
    
    /**
     * @brief Получить хеш вершины цепи
     * 
     * @return Hash256 Хеш tip
     */
    [[nodiscard]] Hash256 get_tip_hash() const;
    
    /**
     * @brief Проверить, есть ли заголовок
     * 
     * @param hash Хеш блока
     * @return true если заголовок существует
     */
    [[nodiscard]] bool has_header(const Hash256& hash) const;
    
    /**
     * @brief Получить последние N заголовков
     * 
     * @param count Количество заголовков
     * @return std::vector<BlockHeader> Заголовки (от старых к новым)
     */
    [[nodiscard]] std::vector<BlockHeader> get_recent_headers(
        std::size_t count
    ) const;
    
    /**
     * @brief Получить заголовки в диапазоне высот
     * 
     * @param start_height Начальная высота
     * @param end_height Конечная высота (включительно)
     * @return std::vector<BlockHeader> Заголовки
     */
    [[nodiscard]] std::vector<BlockHeader> get_headers_range(
        uint32_t start_height,
        uint32_t end_height
    ) const;
    
    /**
     * @brief Количество заголовков в хранилище
     */
    [[nodiscard]] std::size_t size() const noexcept;
    
    /**
     * @brief Очистить хранилище
     */
    void clear();
    
private:
    const ChainParams& params_;
    
    mutable std::mutex mutex_;
    
    // Заголовки по высоте
    std::vector<BlockHeader> headers_;
    
    // Индекс хеш -> высота
    std::unordered_map<Hash256, uint32_t> hash_index_;
    
    // Genesis заголовок
    BlockHeader genesis_;
};

} // namespace quaxis::core::sync
