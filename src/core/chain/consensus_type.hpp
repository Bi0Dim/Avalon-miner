/**
 * @file consensus_type.hpp
 * @brief Перечисление типов консенсуса для AuxPoW монет
 * 
 * Определяет различные типы консенсуса, используемые в auxiliary chains
 * для merged mining. Каждый тип определяет специфику валидации блоков.
 */

#pragma once

#include <string_view>
#include <cstdint>

namespace quaxis::core {

/**
 * @brief Типы консенсуса для AuxPoW монет
 * 
 * Каждый тип определяет особенности:
 * - Валидации proof-of-work
 * - Дополнительных механизмов защиты
 * - Распределения наград
 */
enum class ConsensusType : uint8_t {
    /// @brief Чистый AuxPoW без дополнительных механизмов
    /// Используется: Namecoin, Huntercoin, Unobtanium, Terracoin, Myriad
    PURE_AUXPOW = 0,
    
    /// @brief AuxPoW с ChainLock механизмом (мгновенная финализация)
    /// Используется: Syscoin
    AUXPOW_CHAINLOCK = 1,
    
    /// @brief Гибридный AuxPoW + Proof-of-Stake
    /// Используется: Emercoin
    AUXPOW_HYBRID_POS = 2,
    
    /// @brief Гибридный AuxPoW + Bonded Proof-of-Stake (35% наград майнерам)
    /// Используется: Elastos
    AUXPOW_HYBRID_BPOS = 3,
    
    /// @brief AuxPoW с DECOR+ протоколом (конкурентные блоки)
    /// Используется: RSK (Rootstock)
    AUXPOW_DECOR = 4,
    
    /// @brief AuxPoW с DAG-структурой (направленный ациклический граф)
    /// Используется: Hathor
    AUXPOW_DAG = 5,
};

/**
 * @brief Преобразование типа консенсуса в строку
 * 
 * @param type Тип консенсуса
 * @return std::string_view Название типа
 */
[[nodiscard]] constexpr std::string_view to_string(ConsensusType type) noexcept {
    switch (type) {
        case ConsensusType::PURE_AUXPOW:
            return "PURE_AUXPOW";
        case ConsensusType::AUXPOW_CHAINLOCK:
            return "AUXPOW_CHAINLOCK";
        case ConsensusType::AUXPOW_HYBRID_POS:
            return "AUXPOW_HYBRID_POS";
        case ConsensusType::AUXPOW_HYBRID_BPOS:
            return "AUXPOW_HYBRID_BPOS";
        case ConsensusType::AUXPOW_DECOR:
            return "AUXPOW_DECOR";
        case ConsensusType::AUXPOW_DAG:
            return "AUXPOW_DAG";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Проверить, поддерживает ли консенсус стандартный AuxPoW
 * 
 * @param type Тип консенсуса
 * @return true если поддерживает стандартный AuxPoW
 */
[[nodiscard]] constexpr bool supports_standard_auxpow(ConsensusType type) noexcept {
    switch (type) {
        case ConsensusType::PURE_AUXPOW:
        case ConsensusType::AUXPOW_CHAINLOCK:
        case ConsensusType::AUXPOW_HYBRID_POS:
        case ConsensusType::AUXPOW_HYBRID_BPOS:
            return true;
        case ConsensusType::AUXPOW_DECOR:
        case ConsensusType::AUXPOW_DAG:
            return false;
        default:
            return false;
    }
}

/**
 * @brief Проверить, требует ли консенсус специальной обработки наград
 * 
 * @param type Тип консенсуса
 * @return true если требуется специальная обработка наград
 */
[[nodiscard]] constexpr bool has_reward_splitting(ConsensusType type) noexcept {
    return type == ConsensusType::AUXPOW_HYBRID_BPOS;
}

} // namespace quaxis::core
