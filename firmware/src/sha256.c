/**
 * @file sha256.c
 * @brief Реализация SHA256 для микроконтроллера
 */

#include "sha256.h"
#include <string.h>

/* Начальные значения хеша SHA256 */
static const uint32_t SHA256_INIT[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

/* Константы раунда SHA256 */
static const uint32_t K[64] = {
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

/* Макросы SHA256 */
#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define sigma1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/* Вспомогательные функции */
static uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           ((uint32_t)p[3]);
}

static void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static uint32_t read_le32(const uint8_t* p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* SHA256 Transform */
void sha256_transform(uint32_t* state, const uint8_t* block) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    int i;
    
    /* Подготовка message schedule */
    for (i = 0; i < 16; i++) {
        W[i] = read_be32(block + i * 4);
    }
    
    for (i = 16; i < 64; i++) {
        W[i] = sigma1(W[i-2]) + W[i-7] + sigma0(W[i-15]) + W[i-16];
    }
    
    /* Инициализация рабочих переменных */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];
    
    /* 64 раунда */
    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + W[i];
        uint32_t t2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    /* Добавляем к состоянию */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256_init(sha256_ctx_t* ctx) {
    memcpy(ctx->state, SHA256_INIT, sizeof(SHA256_INIT));
    ctx->count = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void sha256_init_midstate(sha256_ctx_t* ctx, const uint8_t* midstate, uint64_t count) {
    /* Загружаем midstate в little-endian формате */
    for (int i = 0; i < 8; i++) {
        ctx->state[i] = read_le32(midstate + i * 4);
    }
    ctx->count = count;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void sha256_update(sha256_ctx_t* ctx, const uint8_t* data, size_t len) {
    size_t buffer_fill = ctx->count % 64;
    ctx->count += len;
    
    /* Если есть данные в буфере, заполняем его */
    if (buffer_fill > 0) {
        size_t to_copy = 64 - buffer_fill;
        if (to_copy > len) {
            to_copy = len;
        }
        memcpy(ctx->buffer + buffer_fill, data, to_copy);
        data += to_copy;
        len -= to_copy;
        buffer_fill += to_copy;
        
        if (buffer_fill == 64) {
            sha256_transform(ctx->state, ctx->buffer);
            buffer_fill = 0;
        }
    }
    
    /* Обрабатываем полные блоки */
    while (len >= 64) {
        sha256_transform(ctx->state, data);
        data += 64;
        len -= 64;
    }
    
    /* Сохраняем остаток */
    if (len > 0) {
        memcpy(ctx->buffer, data, len);
    }
}

void sha256_final(sha256_ctx_t* ctx, uint8_t* hash) {
    size_t buffer_fill = ctx->count % 64;
    uint64_t bit_count = ctx->count * 8;
    
    /* Padding */
    ctx->buffer[buffer_fill++] = 0x80;
    
    if (buffer_fill > 56) {
        memset(ctx->buffer + buffer_fill, 0, 64 - buffer_fill);
        sha256_transform(ctx->state, ctx->buffer);
        buffer_fill = 0;
    }
    
    memset(ctx->buffer + buffer_fill, 0, 56 - buffer_fill);
    
    /* Длина в битах (big-endian) */
    ctx->buffer[56] = (uint8_t)(bit_count >> 56);
    ctx->buffer[57] = (uint8_t)(bit_count >> 48);
    ctx->buffer[58] = (uint8_t)(bit_count >> 40);
    ctx->buffer[59] = (uint8_t)(bit_count >> 32);
    ctx->buffer[60] = (uint8_t)(bit_count >> 24);
    ctx->buffer[61] = (uint8_t)(bit_count >> 16);
    ctx->buffer[62] = (uint8_t)(bit_count >> 8);
    ctx->buffer[63] = (uint8_t)(bit_count);
    
    sha256_transform(ctx->state, ctx->buffer);
    
    /* Записываем результат в big-endian */
    for (int i = 0; i < 8; i++) {
        write_be32(hash + i * 4, ctx->state[i]);
    }
}

void sha256(const uint8_t* data, size_t len, uint8_t* hash) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}

void sha256d(const uint8_t* data, size_t len, uint8_t* hash) {
    uint8_t first_hash[32];
    sha256(data, len, first_hash);
    sha256(first_hash, 32, hash);
}

void sha256_mining_hash(const uint8_t* midstate, const uint8_t* tail, uint8_t* hash) {
    sha256_ctx_t ctx;
    
    /* Инициализируем с midstate (64 байта уже обработано) */
    sha256_init_midstate(&ctx, midstate, 64);
    
    /* Добавляем хвост (16 байт) */
    sha256_update(&ctx, tail, 16);
    
    /* Завершаем первый хеш */
    uint8_t first_hash[32];
    sha256_final(&ctx, first_hash);
    
    /* Второй хеш */
    sha256(first_hash, 32, hash);
}

int sha256_check_target(const uint8_t* hash, const uint8_t* target) {
    /* Сравниваем от старших байт к младшим */
    for (int i = 31; i >= 0; i--) {
        if (hash[i] < target[i]) {
            return 1;  /* hash < target, OK */
        }
        if (hash[i] > target[i]) {
            return 0;  /* hash > target, не подходит */
        }
    }
    return 1;  /* hash == target, OK */
}
