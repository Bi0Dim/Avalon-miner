/**
 * @file sha256_generic.cpp
 * @brief Программная реализация SHA256
 * 
 * Стандартная реализация SHA256 без аппаратного ускорения.
 * Используется как fallback когда SHA-NI недоступен.
 * 
 * Алгоритм соответствует FIPS 180-4.
 */

#include "sha256.hpp"
#include "../core/byte_order.hpp"
#include "../core/constants.hpp"

#include <bit>

namespace quaxis::crypto::generic {

// =============================================================================
// Вспомогательные макросы SHA256
// =============================================================================

// Операции SHA256
#define ROTR32(x, n) std::rotr(x, n)
#define SHR(x, n)    ((x) >> (n))

// Функции SHA256 (FIPS 180-4, секция 4.1.2)
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x)    (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define SIGMA1(x)    (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define sigma0(x)    (ROTR32(x, 7) ^ ROTR32(x, 18) ^ SHR(x, 3))
#define sigma1(x)    (ROTR32(x, 17) ^ ROTR32(x, 19) ^ SHR(x, 10))

/**
 * @brief Один раунд SHA256
 * 
 * Выполняет один раунд сжатия SHA256.
 * 
 * @param a-h Рабочие переменные (a будет обновлена, остальные сдвинутся)
 * @param w Слово расписания сообщения
 * @param k Константа раунда
 */
#define SHA256_ROUND(a, b, c, d, e, f, g, h, w, k) \
    do { \
        uint32_t t1 = (h) + SIGMA1(e) + CH(e, f, g) + (k) + (w); \
        uint32_t t2 = SIGMA0(a) + MAJ(a, b, c); \
        (h) = (g); \
        (g) = (f); \
        (f) = (e); \
        (e) = (d) + t1; \
        (d) = (c); \
        (c) = (b); \
        (b) = (a); \
        (a) = t1 + t2; \
    } while (0)

// =============================================================================
// SHA256 Transform
// =============================================================================

/**
 * @brief Функция сжатия SHA256
 * 
 * Обрабатывает один 64-байтный (512-битный) блок сообщения
 * и обновляет состояние хеша.
 * 
 * Алгоритм:
 * 1. Подготовка расписания сообщения W[0..63]
 * 2. Инициализация рабочих переменных a-h
 * 3. 64 раунда сжатия
 * 4. Добавление результата к состоянию
 * 
 * @param state Состояние хеша (8 x 32-bit слов), будет обновлено
 * @param block Указатель на 64 байта данных
 */
void sha256_transform(Sha256State& state, const uint8_t* block) noexcept {
    // Расписание сообщения (message schedule)
    uint32_t W[64];
    
    // === Шаг 1: Подготовка расписания сообщения ===
    // W[0..15] = слова из блока (big-endian)
    for (int i = 0; i < 16; ++i) {
        W[i] = read_be32(block + i * 4);
    }
    
    // W[16..63] = расширение
    for (int i = 16; i < 64; ++i) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }
    
    // === Шаг 2: Инициализация рабочих переменных ===
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    
    // === Шаг 3: 64 раунда сжатия ===
    // Развёрнутые раунды для лучшей производительности
    for (int i = 0; i < 64; i += 8) {
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 0], constants::SHA256_K[static_cast<std::size_t>(i + 0)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 1], constants::SHA256_K[static_cast<std::size_t>(i + 1)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 2], constants::SHA256_K[static_cast<std::size_t>(i + 2)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 3], constants::SHA256_K[static_cast<std::size_t>(i + 3)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 4], constants::SHA256_K[static_cast<std::size_t>(i + 4)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 5], constants::SHA256_K[static_cast<std::size_t>(i + 5)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 6], constants::SHA256_K[static_cast<std::size_t>(i + 6)]);
        SHA256_ROUND(a, b, c, d, e, f, g, h, W[i + 7], constants::SHA256_K[static_cast<std::size_t>(i + 7)]);
    }
    
    // === Шаг 4: Добавление к состоянию ===
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

// Очистка макросов
#undef ROTR32
#undef SHR
#undef CH
#undef MAJ
#undef SIGMA0
#undef SIGMA1
#undef sigma0
#undef sigma1
#undef SHA256_ROUND

} // namespace quaxis::crypto::generic
