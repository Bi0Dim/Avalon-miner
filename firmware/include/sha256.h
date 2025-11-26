/**
 * @file sha256.h
 * @brief SHA256 для контроллера ASIC
 * 
 * Облегчённая реализация SHA256 для микроконтроллера.
 * Используется для верификации хешей на стороне контроллера.
 */

#ifndef QUAXIS_SHA256_H
#define QUAXIS_SHA256_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Состояние SHA256 (midstate)
 */
typedef struct {
    uint32_t state[8];      /* Текущее состояние хеша */
    uint64_t count;         /* Количество обработанных байт */
    uint8_t  buffer[64];    /* Буфер для неполного блока */
} sha256_ctx_t;

/**
 * @brief Инициализировать контекст SHA256
 * 
 * @param ctx Указатель на контекст
 */
void sha256_init(sha256_ctx_t* ctx);

/**
 * @brief Инициализировать контекст с готовым midstate
 * 
 * @param ctx Указатель на контекст
 * @param midstate 32 байта midstate
 * @param count Количество уже обработанных байт (обычно 64)
 */
void sha256_init_midstate(sha256_ctx_t* ctx, const uint8_t* midstate, uint64_t count);

/**
 * @brief Добавить данные для хеширования
 * 
 * @param ctx Указатель на контекст
 * @param data Данные для хеширования
 * @param len Длина данных
 */
void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len);

/**
 * @brief Завершить хеширование и получить результат
 * 
 * @param ctx Указатель на контекст
 * @param hash Буфер для результата (32 байта)
 */
void sha256_final(sha256_ctx_t* ctx, uint8_t* hash);

/**
 * @brief Вычислить SHA256 хеш за один вызов
 * 
 * @param data Данные для хеширования
 * @param len Длина данных
 * @param hash Буфер для результата (32 байта)
 */
void sha256(const uint8_t* data, size_t len, uint8_t* hash);

/**
 * @brief Вычислить SHA256d (двойной SHA256)
 * 
 * @param data Данные для хеширования
 * @param len Длина данных
 * @param hash Буфер для результата (32 байта)
 */
void sha256d(const uint8_t* data, size_t len, uint8_t* hash);

/**
 * @brief Выполнить один transform SHA256 (обработка 64-байтного блока)
 * 
 * @param state Текущее состояние (8 x uint32_t)
 * @param block 64 байта данных
 */
void sha256_transform(uint32_t* state, const uint8_t* block);

/**
 * @brief Вычислить хеш блока с использованием midstate
 * 
 * Оптимизированная функция для майнинга:
 * Принимает готовый midstate и вычисляет хеш только второй части.
 * 
 * @param midstate 32 байта midstate (состояние после первых 64 байт)
 * @param tail 16 байт хвоста (merkle[28:32] + time + bits + nonce)
 * @param hash Буфер для результата (32 байта)
 */
void sha256_mining_hash(const uint8_t* midstate, const uint8_t* tail, uint8_t* hash);

/**
 * @brief Сравнить хеш с target
 * 
 * @param hash 32-байтный хеш
 * @param target 32-байтный target
 * @return 1 если hash <= target, 0 иначе
 */
int sha256_check_target(const uint8_t* hash, const uint8_t* target);

#endif /* QUAXIS_SHA256_H */
