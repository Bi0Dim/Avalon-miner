/**
 * @file sha256.cpp
 * @brief Реализация SHA256 диспетчера
 * 
 * Выбирает оптимальную реализацию SHA256 в зависимости от
 * поддержки аппаратных инструкций CPU.
 */

#include "sha256.hpp"
#include "../core/byte_order.hpp"

#include <cstring>

#ifdef __x86_64__
#include <cpuid.h>
#endif

namespace quaxis::crypto {

// =============================================================================
// Внешние функции реализаций
// =============================================================================

// Generic реализация (всегда доступна)
namespace generic {
    void sha256_transform(Sha256State& state, const uint8_t* block) noexcept;
}

// SHA-NI реализация (только если поддерживается)
#ifdef QUAXIS_HAS_SHANI
namespace shani {
    void sha256_transform(Sha256State& state, const uint8_t* block) noexcept;
}
#endif

// =============================================================================
// Детекция SHA-NI
// =============================================================================

namespace {

/**
 * @brief Проверка поддержки SHA-NI через CPUID
 * 
 * SHA-NI поддерживается на:
 * - Intel: Ice Lake и новее
 * - AMD: Zen и новее
 */
bool detect_sha_ni() noexcept {
#if defined(__x86_64__) && defined(QUAXIS_HAS_SHANI)
    unsigned int eax, ebx, ecx, edx;
    
    // Проверяем максимальный уровень CPUID
    if (__get_cpuid(0, &eax, &ebx, &ecx, &edx) == 0) {
        return false;
    }
    
    // SHA-NI: EAX=7, ECX=0, проверяем бит 29 в EBX
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) == 0) {
        return false;
    }
    
    return (ebx & (1 << 29)) != 0;
#else
    return false;
#endif
}

/// @brief Кешированный результат детекции SHA-NI
const bool g_has_sha_ni = detect_sha_ni();

} // anonymous namespace

// =============================================================================
// Публичные функции выбора реализации
// =============================================================================

bool has_sha_ni_support() noexcept {
    return g_has_sha_ni;
}

Sha256Implementation get_sha256_implementation() noexcept {
    return g_has_sha_ni ? Sha256Implementation::ShaNi : Sha256Implementation::Generic;
}

std::string_view get_implementation_name() noexcept {
    return g_has_sha_ni ? "sha-ni" : "generic";
}

// =============================================================================
// SHA256 Transform (диспетчер)
// =============================================================================

void sha256_transform(Sha256State& state, const uint8_t* block) noexcept {
#ifdef QUAXIS_HAS_SHANI
    if (g_has_sha_ni) {
        shani::sha256_transform(state, block);
        return;
    }
#endif
    generic::sha256_transform(state, block);
}

// =============================================================================
// Midstate функции
// =============================================================================

Sha256State compute_midstate(const uint8_t* data) noexcept {
    // Начинаем с начального состояния SHA256
    Sha256State state = constants::SHA256_INIT;
    
    // Применяем transform к первым 64 байтам
    sha256_transform(state, data);
    
    return state;
}

Sha256Midstate state_to_bytes(const Sha256State& state) noexcept {
    Sha256Midstate result;
    
    // Конвертируем 8 x uint32_t в 32 байта (little-endian)
    for (std::size_t i = 0; i < 8; ++i) {
        write_le32(result.data() + i * 4, state[i]);
    }
    
    return result;
}

Sha256State bytes_to_state(const Sha256Midstate& bytes) noexcept {
    Sha256State state;
    
    // Конвертируем 32 байта в 8 x uint32_t
    for (std::size_t i = 0; i < 8; ++i) {
        state[i] = read_le32(bytes.data() + i * 4);
    }
    
    return state;
}

// =============================================================================
// Полные SHA256 функции
// =============================================================================

Hash256 sha256(ByteSpan data) noexcept {
    // Начальное состояние
    Sha256State state = constants::SHA256_INIT;
    
    const auto len = data.size();
    const uint8_t* ptr = data.data();
    
    // Обрабатываем полные 64-байтные блоки
    std::size_t blocks = len / 64;
    for (std::size_t i = 0; i < blocks; ++i) {
        sha256_transform(state, ptr);
        ptr += 64;
    }
    
    // Остаток + padding
    std::size_t remaining = len % 64;
    std::array<uint8_t, 128> buffer{}; // Максимум 2 блока для padding
    
    // Копируем остаток
    std::memcpy(buffer.data(), ptr, remaining);
    
    // Добавляем 0x80 (1 бит + 7 нулевых битов)
    buffer[remaining] = 0x80;
    
    // Длина сообщения в битах (big-endian)
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    
    if (remaining >= 56) {
        // Нужен дополнительный блок
        sha256_transform(state, buffer.data());
        std::memset(buffer.data(), 0, 64);
        write_be32(buffer.data() + 60, static_cast<uint32_t>(bit_len));
        write_be32(buffer.data() + 56, static_cast<uint32_t>(bit_len >> 32));
    } else {
        // Помещается в один блок
        write_be32(buffer.data() + 60, static_cast<uint32_t>(bit_len));
        write_be32(buffer.data() + 56, static_cast<uint32_t>(bit_len >> 32));
    }
    
    sha256_transform(state, buffer.data());
    
    // Конвертируем состояние в хеш (big-endian)
    Hash256 result;
    for (std::size_t i = 0; i < 8; ++i) {
        write_be32(result.data() + i * 4, state[i]);
    }
    
    return result;
}

Hash256 sha256d(ByteSpan data) noexcept {
    // Первый SHA256
    Hash256 first = sha256(data);
    
    // Второй SHA256
    return sha256(ByteSpan(first.data(), first.size()));
}

// =============================================================================
// Специализированные функции для майнинга
// =============================================================================

Hash256 hash_header_with_midstate(
    const Sha256State& midstate,
    std::span<const uint8_t, 16> header_tail
) noexcept {
    // Состояние начинается с midstate
    Sha256State state = midstate;
    
    // Создаём второй 64-байтный блок:
    // [0-15]  : последние 16 байт заголовка
    // [16]    : 0x80 (padding)
    // [17-55] : нули
    // [56-63] : длина = 80 * 8 = 640 бит = 0x0000000000000280 (big-endian)
    std::array<uint8_t, 64> block{};
    std::memcpy(block.data(), header_tail.data(), 16);
    block[16] = 0x80;
    // Длина 80 байт = 640 бит = 0x280
    block[62] = 0x02;
    block[63] = 0x80;
    
    // Первый transform (завершение первого SHA256)
    sha256_transform(state, block.data());
    
    // Конвертируем в hash (big-endian)
    Hash256 first_hash;
    for (std::size_t i = 0; i < 8; ++i) {
        write_be32(first_hash.data() + i * 4, state[i]);
    }
    
    // Второй SHA256 (double hash)
    return sha256(ByteSpan(first_hash.data(), 32));
}

bool check_hash_target(const Hash256& hash, const Hash256& target) noexcept {
    // Сравниваем с конца (старшие байты)
    // hash должен быть <= target
    for (int i = 31; i >= 0; --i) {
        if (hash[static_cast<std::size_t>(i)] < target[static_cast<std::size_t>(i)]) {
            return true;  // hash < target, валидно
        }
        if (hash[static_cast<std::size_t>(i)] > target[static_cast<std::size_t>(i)]) {
            return false; // hash > target, невалидно
        }
    }
    return true; // hash == target, валидно
}

} // namespace quaxis::crypto
