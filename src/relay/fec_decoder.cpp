/**
 * @file fec_decoder.cpp
 * @brief Реализация Forward Error Correction (FEC) декодера
 * 
 * Реализует простой XOR-based FEC для восстановления данных при потере пакетов.
 * Для полноценной реализации Reed-Solomon рекомендуется использовать cm256.
 */

#include "fec_decoder.hpp"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <stdexcept>

namespace quaxis::relay {

// =============================================================================
// Вспомогательные функции
// =============================================================================

void xor_bytes(MutableByteSpan dst, ByteSpan src) noexcept {
    const std::size_t len = std::min(dst.size(), src.size());
    for (std::size_t i = 0; i < len; ++i) {
        dst[i] ^= src[i];
    }
}

std::vector<uint8_t> xor_chunks(
    const std::vector<ByteSpan>& chunks,
    std::size_t chunk_size
) {
    std::vector<uint8_t> result(chunk_size, 0);
    
    for (const auto& chunk : chunks) {
        xor_bytes(result, chunk);
    }
    
    return result;
}

// =============================================================================
// Реализация FecDecoder
// =============================================================================

struct FecDecoder::Impl {
    /// @brief Параметры FEC
    FecParams params;
    
    /// @brief Полученные data чанки (индексированы по chunk_id)
    std::vector<std::optional<std::vector<uint8_t>>> data_chunks;
    
    /// @brief Полученные FEC чанки (индексированы по chunk_id - data_chunk_count)
    std::vector<std::optional<std::vector<uint8_t>>> fec_chunks;
    
    /// @brief Битовая маска полученных data чанков
    std::bitset<MAX_DATA_CHUNKS> data_received;
    
    /// @brief Битовая маска полученных FEC чанков
    std::bitset<MAX_FEC_CHUNKS> fec_received;
    
    /// @brief Счётчик полученных data чанков
    std::size_t data_count{0};
    
    /// @brief Счётчик полученных FEC чанков
    std::size_t fec_count{0};
    
    /**
     * @brief Инициализация с параметрами
     */
    explicit Impl(const FecParams& p) 
        : params(p)
        , data_chunks(p.data_chunk_count)
        , fec_chunks(p.fec_chunk_count)
    {
        data_received.reset();
        fec_received.reset();
    }
    
    /**
     * @brief Сброс состояния
     */
    void reset() {
        for (auto& chunk : data_chunks) {
            chunk.reset();
        }
        for (auto& chunk : fec_chunks) {
            chunk.reset();
        }
        data_received.reset();
        fec_received.reset();
        data_count = 0;
        fec_count = 0;
    }
    
    /**
     * @brief Сброс с новыми параметрами
     */
    void reset(const FecParams& p) {
        params = p;
        data_chunks.clear();
        data_chunks.resize(p.data_chunk_count);
        fec_chunks.clear();
        fec_chunks.resize(p.fec_chunk_count);
        data_received.reset();
        fec_received.reset();
        data_count = 0;
        fec_count = 0;
    }
};

FecDecoder::FecDecoder(const FecParams& params)
    : impl_(std::make_unique<Impl>(params))
{
}

FecDecoder::~FecDecoder() = default;

FecDecoder::FecDecoder(FecDecoder&&) noexcept = default;
FecDecoder& FecDecoder::operator=(FecDecoder&&) noexcept = default;

bool FecDecoder::add_chunk(uint16_t chunk_id, bool is_fec, ByteSpan data) {
    if (data.empty() || data.size() > MAX_CHUNK_SIZE) {
        return false;
    }
    
    if (is_fec) {
        // FEC чанк
        const uint16_t fec_idx = chunk_id;
        if (fec_idx >= impl_->params.fec_chunk_count) {
            return false;
        }
        
        if (impl_->fec_received.test(fec_idx)) {
            return false;  // Дубликат
        }
        
        impl_->fec_chunks[fec_idx] = std::vector<uint8_t>(data.begin(), data.end());
        impl_->fec_received.set(fec_idx);
        ++impl_->fec_count;
    } else {
        // Data чанк
        if (chunk_id >= impl_->params.data_chunk_count) {
            return false;
        }
        
        if (impl_->data_received.test(chunk_id)) {
            return false;  // Дубликат
        }
        
        impl_->data_chunks[chunk_id] = std::vector<uint8_t>(data.begin(), data.end());
        impl_->data_received.set(chunk_id);
        ++impl_->data_count;
    }
    
    return true;
}

bool FecDecoder::add_chunk(const FecChunk& chunk) {
    return add_chunk(chunk.chunk_id, chunk.is_fec, chunk.data);
}

bool FecDecoder::can_decode() const noexcept {
    // Для простого XOR FEC: нужны все data чанки или data + fec >= total
    // Для полноценного Reed-Solomon: нужны любые data_chunk_count чанков
    
    // Простая проверка: достаточно ли чанков в сумме
    const std::size_t total = impl_->data_count + impl_->fec_count;
    return total >= impl_->params.data_chunk_count;
}

bool FecDecoder::has_all_data_chunks() const noexcept {
    return impl_->data_count >= impl_->params.data_chunk_count;
}

std::size_t FecDecoder::received_data_chunks() const noexcept {
    return impl_->data_count;
}

std::size_t FecDecoder::received_fec_chunks() const noexcept {
    return impl_->fec_count;
}

std::size_t FecDecoder::received_total_chunks() const noexcept {
    return impl_->data_count + impl_->fec_count;
}

const FecParams& FecDecoder::params() const noexcept {
    return impl_->params;
}

Result<FecDecodeResult> FecDecoder::decode() {
    FecDecodeResult result;
    
    if (has_all_data_chunks()) {
        // Все data чанки есть - просто собираем
        result.data.reserve(
            static_cast<std::size_t>(impl_->params.data_chunk_count) * 
            static_cast<std::size_t>(impl_->params.chunk_size)
        );
        
        for (uint16_t i = 0; i < impl_->params.data_chunk_count; ++i) {
            if (impl_->data_chunks[i]) {
                result.data.insert(
                    result.data.end(),
                    impl_->data_chunks[i]->begin(),
                    impl_->data_chunks[i]->end()
                );
            }
        }
        
        result.data_chunks_used = impl_->data_count;
        result.fec_chunks_used = 0;
        result.chunks_recovered = 0;
        
        return result;
    }
    
    if (!can_decode()) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Недостаточно чанков для декодирования FEC"
        });
    }
    
    // Нужно использовать FEC для восстановления
    // Простая XOR реализация: восстанавливаем один потерянный чанк
    // Для полноценной реализации нужна библиотека cm256 или wirehair
    
    // Находим отсутствующие data чанки
    std::vector<uint16_t> missing_chunks;
    for (uint16_t i = 0; i < impl_->params.data_chunk_count; ++i) {
        if (!impl_->data_received.test(i)) {
            missing_chunks.push_back(i);
        }
    }
    
    // Простая XOR реализация работает только для одного потерянного чанка
    if (missing_chunks.size() > 1 && impl_->fec_count < missing_chunks.size()) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Слишком много потерянных чанков для простого XOR FEC"
        });
    }
    
    // Для каждого отсутствующего чанка пытаемся восстановить через XOR
    // Примечание: это упрощённая реализация, настоящий Reed-Solomon сложнее
    for (uint16_t missing_idx : missing_chunks) {
        if (impl_->fec_count == 0) {
            break;
        }
        
        // Находим FEC чанк, который покрывает этот data чанк
        // В простой схеме XOR: FEC[i] = XOR всех data чанков
        // Тогда: missing = XOR(FEC, все остальные data чанки)
        
        // Находим первый доступный FEC чанк
        std::size_t fec_idx = 0;
        while (fec_idx < impl_->params.fec_chunk_count && 
               !impl_->fec_received.test(fec_idx)) {
            ++fec_idx;
        }
        
        if (fec_idx >= impl_->params.fec_chunk_count) {
            break;  // Нет доступных FEC чанков
        }
        
        // Копируем FEC чанк как начальное значение
        std::vector<uint8_t> recovered = *impl_->fec_chunks[fec_idx];
        
        // XOR со всеми имеющимися data чанками
        for (uint16_t i = 0; i < impl_->params.data_chunk_count; ++i) {
            if (i != missing_idx && impl_->data_received.test(i)) {
                xor_bytes(recovered, *impl_->data_chunks[i]);
            }
        }
        
        // Сохраняем восстановленный чанк
        impl_->data_chunks[missing_idx] = std::move(recovered);
        impl_->data_received.set(missing_idx);
        ++impl_->data_count;
        ++result.chunks_recovered;
        
        // Помечаем FEC как использованный
        impl_->fec_received.reset(fec_idx);
        impl_->fec_chunks[fec_idx].reset();
        --impl_->fec_count;
    }
    
    // Проверяем, все ли чанки восстановлены
    if (!has_all_data_chunks()) {
        return std::unexpected(Error{
            ErrorCode::CryptoInvalidLength,
            "Не удалось восстановить все чанки"
        });
    }
    
    // Собираем данные
    result.data.reserve(
        static_cast<std::size_t>(impl_->params.data_chunk_count) * 
        static_cast<std::size_t>(impl_->params.chunk_size)
    );
    
    for (uint16_t i = 0; i < impl_->params.data_chunk_count; ++i) {
        if (impl_->data_chunks[i]) {
            result.data.insert(
                result.data.end(),
                impl_->data_chunks[i]->begin(),
                impl_->data_chunks[i]->end()
            );
        }
    }
    
    result.data_chunks_used = impl_->data_count;
    result.fec_chunks_used = result.chunks_recovered;
    
    return result;
}

std::optional<std::vector<uint8_t>> FecDecoder::get_first_n_bytes(std::size_t n) const {
    if (n == 0) {
        return std::vector<uint8_t>{};
    }
    
    std::vector<uint8_t> result;
    result.reserve(n);
    
    // Собираем данные из первых чанков
    for (uint16_t i = 0; i < impl_->params.data_chunk_count && result.size() < n; ++i) {
        if (!impl_->data_received.test(i)) {
            // Нет последовательных чанков - не можем гарантировать начало
            if (result.empty()) {
                return std::nullopt;
            }
            break;
        }
        
        const auto& chunk = *impl_->data_chunks[i];
        const std::size_t need = n - result.size();
        const std::size_t take = std::min(need, chunk.size());
        
        result.insert(result.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(take));
    }
    
    if (result.size() < n) {
        return std::nullopt;
    }
    
    return result;
}

void FecDecoder::reset() {
    impl_->reset();
}

void FecDecoder::reset(const FecParams& params) {
    impl_->reset(params);
}

} // namespace quaxis::relay
