/**
 * @file chain_registry.hpp
 * @brief Реестр всех поддерживаемых AuxPoW блокчейнов
 * 
 * Централизованное хранилище параметров всех монет.
 * Позволяет получить параметры по имени или тикеру.
 */

#pragma once

#include "chain_params.hpp"

#include <string_view>
#include <optional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace quaxis::core {

/**
 * @brief Реестр параметров всех поддерживаемых блокчейнов
 * 
 * Синглтон, содержащий параметры всех AuxPoW монет.
 * Загружает параметры при первом обращении.
 */
class ChainRegistry {
public:
    /**
     * @brief Получить единственный экземпляр реестра
     * 
     * @return ChainRegistry& Ссылка на реестр
     */
    [[nodiscard]] static ChainRegistry& instance();
    
    // Запрещаем копирование и перемещение
    ChainRegistry(const ChainRegistry&) = delete;
    ChainRegistry& operator=(const ChainRegistry&) = delete;
    ChainRegistry(ChainRegistry&&) = delete;
    ChainRegistry& operator=(ChainRegistry&&) = delete;
    
    // =========================================================================
    // Доступ к параметрам
    // =========================================================================
    
    /**
     * @brief Получить параметры chain по имени
     * 
     * @param name Имя chain (например, "namecoin", "syscoin")
     * @return const ChainParams* Указатель на параметры или nullptr
     */
    [[nodiscard]] const ChainParams* get_by_name(std::string_view name) const;
    
    /**
     * @brief Получить параметры chain по тикеру
     * 
     * @param ticker Тикер (например, "NMC", "SYS")
     * @return const ChainParams* Указатель на параметры или nullptr
     */
    [[nodiscard]] const ChainParams* get_by_ticker(std::string_view ticker) const;
    
    /**
     * @brief Получить параметры chain по chain_id
     * 
     * @param chain_id Идентификатор chain
     * @return const ChainParams* Указатель на параметры или nullptr
     */
    [[nodiscard]] const ChainParams* get_by_chain_id(uint32_t chain_id) const;
    
    /**
     * @brief Проверить существование chain по имени
     * 
     * @param name Имя chain
     * @return true если chain существует
     */
    [[nodiscard]] bool has_chain(std::string_view name) const;
    
    // =========================================================================
    // Перечисление
    // =========================================================================
    
    /**
     * @brief Получить список всех зарегистрированных chains
     * 
     * @return std::vector<std::string_view> Список имён
     */
    [[nodiscard]] std::vector<std::string_view> get_all_names() const;
    
    /**
     * @brief Получить количество зарегистрированных chains
     * 
     * @return std::size_t Количество chains
     */
    [[nodiscard]] std::size_t count() const noexcept;
    
    /**
     * @brief Выполнить действие для каждой chain
     * 
     * @param callback Функция, вызываемая для каждой chain
     */
    void for_each(std::function<void(const ChainParams&)> callback) const;
    
    /**
     * @brief Получить chains по типу консенсуса
     * 
     * @param type Тип консенсуса
     * @return std::vector<const ChainParams*> Список chains
     */
    [[nodiscard]] std::vector<const ChainParams*> get_by_consensus_type(
        ConsensusType type
    ) const;
    
    // =========================================================================
    // Регистрация
    // =========================================================================
    
    /**
     * @brief Зарегистрировать новую chain
     * 
     * @param params Параметры chain
     * @return true если успешно зарегистрирована
     */
    bool register_chain(ChainParams params);
    
private:
    ChainRegistry();
    ~ChainRegistry() = default;
    
    /// @brief Инициализировать встроенные chains
    void init_builtin_chains();
    
    /// @brief Хранилище параметров chains
    std::vector<ChainParams> chains_;
    
    /// @brief Индекс по имени
    std::unordered_map<std::string, std::size_t> name_index_;
    
    /// @brief Индекс по тикеру
    std::unordered_map<std::string, std::size_t> ticker_index_;
    
    /// @brief Индекс по chain_id
    std::unordered_map<uint32_t, std::size_t> chain_id_index_;
};

// =============================================================================
// Удобные функции доступа
// =============================================================================

/**
 * @brief Получить параметры Bitcoin
 */
[[nodiscard]] const ChainParams& bitcoin_params();

/**
 * @brief Получить параметры Namecoin
 */
[[nodiscard]] const ChainParams& namecoin_params();

/**
 * @brief Получить параметры Syscoin
 */
[[nodiscard]] const ChainParams& syscoin_params();

/**
 * @brief Получить параметры Elastos
 */
[[nodiscard]] const ChainParams& elastos_params();

/**
 * @brief Получить параметры Emercoin
 */
[[nodiscard]] const ChainParams& emercoin_params();

/**
 * @brief Получить параметры RSK
 */
[[nodiscard]] const ChainParams& rsk_params();

/**
 * @brief Получить параметры Hathor
 */
[[nodiscard]] const ChainParams& hathor_params();

/**
 * @brief Получить параметры VCash
 */
[[nodiscard]] const ChainParams& vcash_params();

/**
 * @brief Получить параметры Fractal Bitcoin
 */
[[nodiscard]] const ChainParams& fractal_params();

/**
 * @brief Получить параметры Myriad
 */
[[nodiscard]] const ChainParams& myriad_params();

/**
 * @brief Получить параметры Huntercoin
 */
[[nodiscard]] const ChainParams& huntercoin_params();

/**
 * @brief Получить параметры Unobtanium
 */
[[nodiscard]] const ChainParams& unobtanium_params();

/**
 * @brief Получить параметры Terracoin
 */
[[nodiscard]] const ChainParams& terracoin_params();

} // namespace quaxis::core
