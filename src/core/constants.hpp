/**
 * @file constants.hpp
 * @brief Константы протокола Bitcoin и Quaxis
 * 
 * Содержит все магические числа, размеры структур и константы,
 * используемые в Bitcoin протоколе и Quaxis майнере.
 * 
 * @note Все константы определены как constexpr для compile-time вычислений.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace quaxis::constants {

// =============================================================================
// Размеры структур Bitcoin
// =============================================================================

/// @brief Размер SHA256 хеша в байтах
inline constexpr std::size_t SHA256_SIZE = 32;

/// @brief Размер SHA256 midstate в байтах
inline constexpr std::size_t SHA256_MIDSTATE_SIZE = 32;

/// @brief Размер блока SHA256 (один transform) в байтах
inline constexpr std::size_t SHA256_BLOCK_SIZE = 64;

/// @brief Размер заголовка блока Bitcoin в байтах
inline constexpr std::size_t BLOCK_HEADER_SIZE = 80;

/// @brief Размер coinbase транзакции (минимальный, только payout) в байтах
inline constexpr std::size_t COINBASE_SIZE = 110;

/// @brief Размер хвоста заголовка (merkle + time + bits + nonce) в байтах
inline constexpr std::size_t HEADER_TAIL_SIZE = 12;

/// @brief Размер RIPEMD160 хеша (для P2PKH/P2WPKH) в байтах
inline constexpr std::size_t RIPEMD160_SIZE = 20;

// =============================================================================
// Константы Quaxis протокола
// =============================================================================

/// @brief Размер тега "quaxis" в coinbase scriptsig
inline constexpr std::size_t COINBASE_TAG_SIZE = 6;

/// @brief Тег "quaxis" в ASCII
inline constexpr std::array<uint8_t, 6> COINBASE_TAG = {'q', 'u', 'a', 'x', 'i', 's'};

/// @brief Размер extranonce в байтах
inline constexpr std::size_t EXTRANONCE_SIZE = 6;

/// @brief Максимальное значение extranonce (2^48 - 1)
inline constexpr uint64_t EXTRANONCE_MAX = (1ULL << 48) - 1;

/// @brief Размер job_id в байтах
inline constexpr std::size_t JOB_ID_SIZE = 4;

// =============================================================================
// Размеры сообщений Quaxis бинарного протокола
// =============================================================================

/// @brief Размер задания для ASIC: midstate[32] + header_tail[12] + job_id[4]
inline constexpr std::size_t JOB_MESSAGE_SIZE = SHA256_MIDSTATE_SIZE + HEADER_TAIL_SIZE + JOB_ID_SIZE;
static_assert(JOB_MESSAGE_SIZE == 48, "Размер задания должен быть 48 байт");

/// @brief Размер ответа от ASIC: job_id[4] + nonce[4]
inline constexpr std::size_t SHARE_MESSAGE_SIZE = JOB_ID_SIZE + 4;
static_assert(SHARE_MESSAGE_SIZE == 8, "Размер ответа должен быть 8 байт");

// =============================================================================
// Константы Bitcoin
// =============================================================================

/// @brief Версия блока (по умолчанию 0x20000000 = version bits)
inline constexpr uint32_t BLOCK_VERSION = 0x20000000;

/// @brief Версия транзакции
inline constexpr uint32_t TX_VERSION = 1;

/// @brief Sequence для coinbase input
inline constexpr uint32_t COINBASE_SEQUENCE = 0xFFFFFFFF;

/// @brief Locktime для coinbase транзакции
inline constexpr uint32_t COINBASE_LOCKTIME = 0;

/// @brief Количество подтверждений для созревания coinbase
inline constexpr uint32_t COINBASE_MATURITY = 100;

/// @brief Награда за блок в сатоши (на момент halving 2024)
/// 3.125 BTC = 312500000 satoshi
inline constexpr int64_t BLOCK_REWARD_SATOSHI = 312'500'000;

/// @brief Интервал halving в блоках
inline constexpr uint32_t HALVING_INTERVAL = 210'000;

/// @brief Максимальный размер блока в байтах (legacy)
inline constexpr std::size_t MAX_BLOCK_SIZE = 1'000'000;

/// @brief Максимальный вес блока
inline constexpr std::size_t MAX_BLOCK_WEIGHT = 4'000'000;

// =============================================================================
// Константы Shared Memory
// =============================================================================

/// @brief Путь к shared memory по умолчанию
inline constexpr const char* DEFAULT_SHM_PATH = "/quaxis_block";

/// @brief Размер shared memory структуры (с выравниванием)
inline constexpr std::size_t SHM_BLOCK_SIZE = 256;

// =============================================================================
// Константы сети
// =============================================================================

/// @brief Порт сервера по умолчанию
inline constexpr uint16_t DEFAULT_SERVER_PORT = 3333;

/// @brief Максимальное количество подключений
inline constexpr std::size_t DEFAULT_MAX_CONNECTIONS = 10;

/// @brief Размер очереди заданий по умолчанию
inline constexpr std::size_t DEFAULT_JOB_QUEUE_SIZE = 100;

/// @brief Таймаут подключения в миллисекундах
inline constexpr uint32_t CONNECTION_TIMEOUT_MS = 5000;

/// @brief Интервал heartbeat в секундах
inline constexpr uint32_t HEARTBEAT_INTERVAL_SEC = 30;

// =============================================================================
// Константы Bitcoin Core RPC
// =============================================================================

/// @brief Порт RPC Bitcoin Core (mainnet)
inline constexpr uint16_t BITCOIN_RPC_PORT_MAINNET = 8332;

/// @brief Порт RPC Bitcoin Core (testnet)
inline constexpr uint16_t BITCOIN_RPC_PORT_TESTNET = 18332;

/// @brief Порт RPC Bitcoin Core (regtest)
inline constexpr uint16_t BITCOIN_RPC_PORT_REGTEST = 18443;

// =============================================================================
// Начальные значения SHA256 (H0-H7)
// =============================================================================

/// @brief Начальные значения хеша SHA256 (FIPS 180-4)
inline constexpr std::array<uint32_t, 8> SHA256_INIT = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/// @brief Константы раунда SHA256 (первые 32 бита дробной части кубических корней простых чисел)
inline constexpr std::array<uint32_t, 64> SHA256_K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

} // namespace quaxis::constants
