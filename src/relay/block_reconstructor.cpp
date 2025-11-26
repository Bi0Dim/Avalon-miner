/**
 * @file block_reconstructor.cpp
 * @brief Реализация реконструктора блока из FIBRE чанков
 */

#include "block_reconstructor.hpp"
#include "../bitcoin/block.hpp"
#include "../crypto/sha256.hpp"

#include <algorithm>
#include <cstring>

namespace quaxis::relay {

// =============================================================================
// Реализация BlockReconstructor
// =============================================================================

struct BlockReconstructor::Impl {
    /// @brief Хеш блока
    Hash256 block_hash_;
    
    /// @brief Высота блока
    uint32_t height_{0};
    
    /// @brief FEC декодер
    FecDecoder fec_decoder_;
    
    /// @brief Таймаут в миллисекундах
    uint32_t timeout_ms_{5000};
    
    /// @brief Текущее состояние
    ReconstructionState state_{ReconstructionState::Waiting};
    
    /// @brief Статистика
    ReconstructionStats stats_;
    
    /// @brief Полученный header
    std::optional<bitcoin::BlockHeader> header_;
    
    /// @brief Полные данные блока
    std::optional<std::vector<uint8_t>> block_data_;
    
    /// @brief Callback для header
    HeaderCallback header_callback_;
    
    /// @brief Callback для блока
    BlockCallback block_callback_;
    
    /// @brief Callback для таймаута
    TimeoutCallback timeout_callback_;
    
    /// @brief Мьютекс для thread-safety
    mutable std::mutex mutex_;
    
    /**
     * @brief Конструктор
     */
    Impl(const Hash256& hash, uint32_t height, const FecParams& params, uint32_t timeout)
        : block_hash_(hash)
        , height_(height)
        , fec_decoder_(params)
        , timeout_ms_(timeout)
    {
        stats_.start_time = std::chrono::steady_clock::now();
    }
    
    /**
     * @brief Попытаться извлечь header из первых чанков
     */
    void try_extract_header() {
        if (header_) {
            return;  // Уже извлечён
        }
        
        // Нужно минимум 80 байт для header
        constexpr std::size_t HEADER_SIZE = 80;
        
        auto first_bytes = fec_decoder_.get_first_n_bytes(HEADER_SIZE);
        if (!first_bytes) {
            return;  // Недостаточно данных
        }
        
        // Десериализуем header
        auto header_result = bitcoin::BlockHeader::deserialize(*first_bytes);
        if (!header_result) {
            return;  // Ошибка десериализации
        }
        
        header_ = *header_result;
        stats_.header_time = std::chrono::steady_clock::now();
        state_ = ReconstructionState::HeaderReceived;
        
        // Вызываем callback
        if (header_callback_) {
            header_callback_(*header_, height_, block_hash_);
        }
    }
    
    /**
     * @brief Попытаться декодировать полный блок
     */
    bool try_decode_block() {
        if (!fec_decoder_.can_decode()) {
            return false;
        }
        
        auto decode_result = fec_decoder_.decode();
        if (!decode_result) {
            state_ = ReconstructionState::Failed;
            return false;
        }
        
        block_data_ = std::move(decode_result->data);
        stats_.chunks_recovered = decode_result->chunks_recovered;
        stats_.complete_time = std::chrono::steady_clock::now();
        state_ = ReconstructionState::Complete;
        
        // Извлекаем header если ещё не был извлечён
        if (!header_ && block_data_->size() >= 80) {
            auto header_result = bitcoin::BlockHeader::deserialize(
                ByteSpan{block_data_->data(), 80}
            );
            if (header_result) {
                header_ = *header_result;
                if (stats_.header_time.time_since_epoch().count() == 0) {
                    stats_.header_time = stats_.complete_time;
                }
            }
        }
        
        // Вызываем callback
        if (block_callback_) {
            block_callback_(*block_data_, height_, block_hash_);
        }
        
        return true;
    }
};

BlockReconstructor::BlockReconstructor(
    const Hash256& block_hash,
    uint32_t height,
    const FecParams& fec_params,
    uint32_t timeout_ms
)
    : impl_(std::make_unique<Impl>(block_hash, height, fec_params, timeout_ms))
{
}

BlockReconstructor::~BlockReconstructor() = default;

BlockReconstructor::BlockReconstructor(BlockReconstructor&&) noexcept = default;
BlockReconstructor& BlockReconstructor::operator=(BlockReconstructor&&) noexcept = default;

bool BlockReconstructor::on_packet(const FibrePacket& packet) {
    // Проверяем что это наш блок
    if (packet.header.block_hash != impl_->block_hash_) {
        return false;
    }
    
    return on_chunk(
        packet.header.chunk_id,
        packet.header.is_fec(),
        packet.payload
    );
}

bool BlockReconstructor::on_chunk(uint16_t chunk_id, bool is_fec, ByteSpan data) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    // Проверяем состояние
    if (impl_->state_ == ReconstructionState::Complete ||
        impl_->state_ == ReconstructionState::Timeout ||
        impl_->state_ == ReconstructionState::Failed) {
        return false;
    }
    
    // Добавляем чанк
    bool added = impl_->fec_decoder_.add_chunk(chunk_id, is_fec, data);
    
    if (added) {
        if (is_fec) {
            ++impl_->stats_.fec_chunks_received;
        } else {
            ++impl_->stats_.data_chunks_received;
        }
        
        // Пытаемся извлечь header как можно раньше
        impl_->try_extract_header();
        
        // Пытаемся декодировать полный блок
        impl_->try_decode_block();
    } else {
        ++impl_->stats_.duplicates;
    }
    
    return added;
}

void BlockReconstructor::set_header_callback(HeaderCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->header_callback_ = std::move(callback);
}

void BlockReconstructor::set_block_callback(BlockCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->block_callback_ = std::move(callback);
}

void BlockReconstructor::set_timeout_callback(TimeoutCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->timeout_callback_ = std::move(callback);
}

ReconstructionState BlockReconstructor::state() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->state_;
}

bool BlockReconstructor::has_header() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->header_.has_value();
}

bool BlockReconstructor::is_complete() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->state_ == ReconstructionState::Complete;
}

bool BlockReconstructor::is_timed_out() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (impl_->state_ == ReconstructionState::Timeout) {
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - impl_->stats_.start_time
    ).count();
    
    return static_cast<uint32_t>(elapsed) >= impl_->timeout_ms_;
}

bool BlockReconstructor::can_try_decode() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->fec_decoder_.can_decode();
}

const Hash256& BlockReconstructor::block_hash() const noexcept {
    return impl_->block_hash_;
}

uint32_t BlockReconstructor::height() const noexcept {
    return impl_->height_;
}

ReconstructionStats BlockReconstructor::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    ReconstructionStats stats = impl_->stats_;
    stats.data_chunks_received = impl_->fec_decoder_.received_data_chunks();
    stats.fec_chunks_received = impl_->fec_decoder_.received_fec_chunks();
    
    return stats;
}

void BlockReconstructor::check_timeout() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (impl_->state_ == ReconstructionState::Complete ||
        impl_->state_ == ReconstructionState::Timeout ||
        impl_->state_ == ReconstructionState::Failed) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - impl_->stats_.start_time
    ).count();
    
    if (static_cast<uint32_t>(elapsed) >= impl_->timeout_ms_) {
        impl_->state_ = ReconstructionState::Timeout;
        
        if (impl_->timeout_callback_) {
            impl_->timeout_callback_(
                impl_->height_,
                impl_->block_hash_,
                impl_->fec_decoder_.received_total_chunks(),
                impl_->fec_decoder_.params().total_chunks()
            );
        }
    }
}

bool BlockReconstructor::try_complete() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    if (impl_->state_ == ReconstructionState::Complete) {
        return true;
    }
    
    if (impl_->state_ == ReconstructionState::Timeout ||
        impl_->state_ == ReconstructionState::Failed) {
        return false;
    }
    
    return impl_->try_decode_block();
}

} // namespace quaxis::relay
