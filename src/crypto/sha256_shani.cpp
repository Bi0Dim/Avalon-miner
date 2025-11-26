/**
 * @file sha256_shani.cpp
 * @brief SHA256 реализация с использованием Intel SHA-NI инструкций
 * 
 * Использует аппаратные SHA256 инструкции, доступные на:
 * - Intel: Ice Lake и новее
 * - AMD: Zen и новее
 * 
 * Основные intrinsics:
 * - _mm_sha256rnds2_epu32: Два раунда SHA256
 * - _mm_sha256msg1_epu32: Message schedule часть 1
 * - _mm_sha256msg2_epu32: Message schedule часть 2
 * 
 * Производительность: ~3-5x быстрее программной реализации
 * Латентность: ~50-100 наносекунд на 64-байтный блок
 * 
 * @note Этот файл компилируется с флагами -msha -msse4.1
 */

#include "sha256.hpp"
#include "../core/constants.hpp"

#ifdef QUAXIS_HAS_SHANI

#include <immintrin.h>
#include <cstring>

namespace quaxis::crypto::shani {

// =============================================================================
// Константы SHA256 для SIMD
// =============================================================================

/**
 * @brief Константы раундов SHA256, упакованные для SIMD
 * 
 * Константы организованы попарно для использования с _mm_sha256rnds2_epu32.
 */
alignas(16) static const uint32_t K256[64] = {
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

/// @brief Маска для byte shuffle (конвертация big-endian)
alignas(16) static const uint8_t BSWAP_MASK[16] = {
    3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12
};

// =============================================================================
// SHA256 Transform с SHA-NI
// =============================================================================

/**
 * @brief Функция сжатия SHA256 с использованием SHA-NI
 * 
 * Алгоритм:
 * 1. Загружаем состояние в SSE регистры
 * 2. Загружаем сообщение и конвертируем в big-endian
 * 3. Выполняем 64 раунда с помощью _mm_sha256rnds2_epu32
 * 4. Добавляем результат к состоянию
 * 
 * @param state Состояние хеша (8 x 32-bit слов)
 * @param block Указатель на 64 байта данных
 */
void sha256_transform(Sha256State& state, const uint8_t* block) noexcept {
    // Маска для byte swap
    const __m128i bswap_mask = _mm_load_si128(reinterpret_cast<const __m128i*>(BSWAP_MASK));
    
    // === Загрузка начального состояния ===
    // Состояние SHA256: A B C D E F G H
    // Загружаем в два 128-битных регистра:
    // STATE0 = A B C D (позже будет E F G H после shuffle)
    // STATE1 = E F G H (позже будет A B C D после shuffle)
    
    // Загружаем состояние
    __m128i tmp = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state.data()));
    __m128i state1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(state.data() + 4));
    
    // Сохраняем начальное состояние для финального сложения
    const __m128i abef_init = tmp;
    const __m128i cdgh_init = state1;
    
    // Переупорядочиваем состояние для SHA-NI
    // tmp = C D A B
    tmp = _mm_shuffle_epi32(tmp, 0xB1);        // 10 11 00 01 = 0xB1
    // state1 = G H E F
    state1 = _mm_shuffle_epi32(state1, 0xB1);
    // STATE0 = A B E F
    __m128i state0 = _mm_alignr_epi8(tmp, state1, 8);
    // STATE1 = C D G H
    state1 = _mm_blend_epi16(state1, tmp, 0xF0);
    
    // === Загрузка сообщения ===
    __m128i msg0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block));
    __m128i msg1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 16));
    __m128i msg2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 32));
    __m128i msg3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(block + 48));
    
    // Конвертируем в big-endian
    msg0 = _mm_shuffle_epi8(msg0, bswap_mask);
    msg1 = _mm_shuffle_epi8(msg1, bswap_mask);
    msg2 = _mm_shuffle_epi8(msg2, bswap_mask);
    msg3 = _mm_shuffle_epi8(msg3, bswap_mask);
    
    // === Раунды 0-3 ===
    __m128i msg_tmp;
    msg_tmp = _mm_add_epi32(msg0, _mm_load_si128(reinterpret_cast<const __m128i*>(K256)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);
    
    // === Раунды 4-7 ===
    msg_tmp = _mm_add_epi32(msg1, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 4)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);
    
    // === Раунды 8-11 ===
    msg_tmp = _mm_add_epi32(msg2, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 8)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);
    
    // === Раунды 12-15 ===
    msg_tmp = _mm_add_epi32(msg3, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 12)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    __m128i tmp_msg = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp_msg);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);
    
    // === Раунды 16-19 ===
    msg_tmp = _mm_add_epi32(msg0, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 16)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp_msg);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);
    
    // === Раунды 20-23 ===
    msg_tmp = _mm_add_epi32(msg1, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 20)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp_msg);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);
    
    // === Раунды 24-27 ===
    msg_tmp = _mm_add_epi32(msg2, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 24)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp_msg);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);
    
    // === Раунды 28-31 ===
    msg_tmp = _mm_add_epi32(msg3, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 28)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp_msg);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);
    
    // === Раунды 32-35 ===
    msg_tmp = _mm_add_epi32(msg0, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 32)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp_msg);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg0 = _mm_sha256msg1_epu32(msg0, msg1);
    
    // === Раунды 36-39 ===
    msg_tmp = _mm_add_epi32(msg1, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 36)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp_msg);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg1 = _mm_sha256msg1_epu32(msg1, msg2);
    
    // === Раунды 40-43 ===
    msg_tmp = _mm_add_epi32(msg2, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 40)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp_msg);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg2 = _mm_sha256msg1_epu32(msg2, msg3);
    
    // === Раунды 44-47 ===
    msg_tmp = _mm_add_epi32(msg3, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 44)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg3, msg2, 4);
    msg0 = _mm_add_epi32(msg0, tmp_msg);
    msg0 = _mm_sha256msg2_epu32(msg0, msg3);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    msg3 = _mm_sha256msg1_epu32(msg3, msg0);
    
    // === Раунды 48-51 ===
    msg_tmp = _mm_add_epi32(msg0, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 48)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg0, msg3, 4);
    msg1 = _mm_add_epi32(msg1, tmp_msg);
    msg1 = _mm_sha256msg2_epu32(msg1, msg0);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    
    // === Раунды 52-55 ===
    msg_tmp = _mm_add_epi32(msg1, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 52)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg1, msg0, 4);
    msg2 = _mm_add_epi32(msg2, tmp_msg);
    msg2 = _mm_sha256msg2_epu32(msg2, msg1);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    
    // === Раунды 56-59 ===
    msg_tmp = _mm_add_epi32(msg2, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 56)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    tmp_msg = _mm_alignr_epi8(msg2, msg1, 4);
    msg3 = _mm_add_epi32(msg3, tmp_msg);
    msg3 = _mm_sha256msg2_epu32(msg3, msg2);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    
    // === Раунды 60-63 (последние) ===
    msg_tmp = _mm_add_epi32(msg3, _mm_load_si128(reinterpret_cast<const __m128i*>(K256 + 60)));
    state1 = _mm_sha256rnds2_epu32(state1, state0, msg_tmp);
    msg_tmp = _mm_shuffle_epi32(msg_tmp, 0x0E);
    state0 = _mm_sha256rnds2_epu32(state0, state1, msg_tmp);
    
    // === Добавляем начальное состояние ===
    // Переупорядочиваем обратно
    tmp = _mm_shuffle_epi32(state0, 0x1B);        // 00 01 10 11 = 0x1B
    state1 = _mm_shuffle_epi32(state1, 0xB1);     // 10 11 00 01 = 0xB1
    state0 = _mm_blend_epi16(tmp, state1, 0xF0);
    state1 = _mm_alignr_epi8(state1, tmp, 8);
    
    // Добавляем начальное состояние
    state0 = _mm_add_epi32(state0, abef_init);
    state1 = _mm_add_epi32(state1, cdgh_init);
    
    // === Сохраняем результат ===
    _mm_storeu_si128(reinterpret_cast<__m128i*>(state.data()), state0);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(state.data() + 4), state1);
}

} // namespace quaxis::crypto::shani

#endif // QUAXIS_HAS_SHANI
