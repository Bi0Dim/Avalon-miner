/**
 * @file chain_params.hpp
 * @brief Универсальная структура параметров блокчейна
 * 
 * Содержит все необходимые параметры для работы с AuxPoW монетами.
 * Позволяет добавлять новую монету созданием одного файла с параметрами.
 */

#pragma once

#include "consensus_type.hpp"
#include "../types.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdint>
#include <optional>

namespace quaxis::core {

/**
 * @brief Параметры AuxPoW для монеты
 * 
 * Определяет специфику merged mining для конкретной chain.
 */
struct AuxPowParams {
    /// @brief Идентификатор chain для slot ID в Merkle tree
    uint32_t chain_id{0};
    
    /// @brief Магические байты AuxPoW (обычно 0xfabe6d6d = "mm")
    std::array<uint8_t, 4> magic_bytes{0xfa, 0xbe, 0x6d, 0x6d};
    
    /// @brief Высота активации AuxPoW (0 = с генезиса)
    uint32_t start_height{0};
    
    /// @brief Версия блока с AuxPoW (обычно имеет специальный флаг)
    uint32_t version_flag{0x00620102};  // Стандартный AuxPoW флаг
    
    /**
     * @brief Проверить, активен ли AuxPoW на данной высоте
     * 
     * @param height Высота блока
     * @return true если AuxPoW активен
     */
    [[nodiscard]] constexpr bool is_active(uint32_t height) const noexcept {
        return height >= start_height;
    }
};

/**
 * @brief Параметры сети
 * 
 * Определяет сетевые константы для подключения к ноде.
 */
struct NetworkParams {
    /// @brief Магические байты сети (4 байта)
    std::array<uint8_t, 4> magic{};
    
    /// @brief Порт P2P по умолчанию
    uint16_t default_port{0};
    
    /// @brief Порт RPC по умолчанию
    uint16_t rpc_port{0};
    
    /// @brief DNS сиды для bootstrap
    std::vector<std::string> dns_seeds;
};

/**
 * @brief Параметры сложности
 * 
 * Определяет алгоритм пересчёта сложности.
 */
struct DifficultyParams {
    /// @brief Целевое время блока (секунды)
    uint32_t target_spacing{600};
    
    /// @brief Интервал пересчёта сложности (в блоках)
    uint32_t adjustment_interval{2016};
    
    /// @brief Минимальная сложность (nBits формат)
    uint32_t pow_limit_bits{0x1d00ffff};
    
    /// @brief Разрешить пересчёт сложности на каждом блоке
    bool allow_min_difficulty{false};
    
    /// @brief Время для срабатывания минимальной сложности (секунды)
    uint32_t min_difficulty_time{0};
};

/**
 * @brief Параметры наград
 * 
 * Определяет экономику монеты.
 */
struct RewardParams {
    /// @brief Начальная награда за блок (в минимальных единицах)
    int64_t initial_reward{0};
    
    /// @brief Интервал halving (0 = нет halving)
    uint32_t halving_interval{0};
    
    /// @brief Доля награды для майнера (1.0 = 100%, 0.35 = 35%)
    double miner_share{1.0};
    
    /// @brief Время созревания coinbase (в блоках)
    uint32_t coinbase_maturity{100};
};

/**
 * @brief Универсальная структура параметров блокчейна
 * 
 * Содержит все параметры, необходимые для:
 * - P2P подключения
 * - Валидации блоков
 * - Merged mining
 * - Расчёта наград
 */
struct ChainParams {
    // =========================================================================
    // Идентификация
    // =========================================================================
    
    /// @brief Полное название монеты
    std::string name;
    
    /// @brief Тикер (символ валюты)
    std::string ticker;
    
    /// @brief Хеш genesis блока
    Hash256 genesis_hash{};
    
    // =========================================================================
    // Консенсус
    // =========================================================================
    
    /// @brief Тип консенсуса
    ConsensusType consensus_type{ConsensusType::PURE_AUXPOW};
    
    /// @brief Параметры AuxPoW
    AuxPowParams auxpow;
    
    /// @brief Параметры сложности
    DifficultyParams difficulty;
    
    /// @brief Параметры наград
    RewardParams rewards;
    
    // =========================================================================
    // Сеть
    // =========================================================================
    
    /// @brief Параметры mainnet
    NetworkParams mainnet;
    
    /// @brief Параметры testnet (опционально)
    std::optional<NetworkParams> testnet;
    
    // =========================================================================
    // Вспомогательные методы
    // =========================================================================
    
    /**
     * @brief Получить chain_id для Merkle tree
     * 
     * @return uint32_t Chain ID
     */
    [[nodiscard]] uint32_t get_chain_id() const noexcept {
        return auxpow.chain_id;
    }
    
    /**
     * @brief Проверить, активен ли AuxPoW на данной высоте
     * 
     * @param height Высота блока
     * @return true если AuxPoW активен
     */
    [[nodiscard]] bool is_auxpow_active(uint32_t height) const noexcept {
        return auxpow.is_active(height);
    }
    
    /**
     * @brief Получить целевое время блока
     * 
     * @return uint32_t Время в секундах
     */
    [[nodiscard]] uint32_t get_target_spacing() const noexcept {
        return difficulty.target_spacing;
    }
    
    /**
     * @brief Получить долю награды майнера
     * 
     * @return double Доля от 0.0 до 1.0
     */
    [[nodiscard]] double get_miner_reward_share() const noexcept {
        return rewards.miner_share;
    }
};

} // namespace quaxis::core
