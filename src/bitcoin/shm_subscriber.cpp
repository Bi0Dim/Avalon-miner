/**
 * @file shm_subscriber.cpp
 * @brief Реализация подписчика на Shared Memory
 * 
 * Использует POSIX shared memory (shm_open, mmap) для
 * минимальной латентности при получении новых блоков.
 */

#include "shm_subscriber.hpp"
#include "../core/byte_order.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <format>
#include <thread>

namespace quaxis::bitcoin {

// =============================================================================
// Реализация (PIMPL)
// =============================================================================

struct ShmSubscriber::Impl {
    ShmConfig config;
    NewBlockCallback callback;
    
    int shm_fd = -1;
    QuaxisSharedBlock* shm_block = nullptr;
    
    std::thread worker_thread;
    std::atomic<bool> running{false};
    std::atomic<uint64_t> last_sequence{0};
    
    BlockHeader last_block;
    std::mutex last_block_mutex;
    
    explicit Impl(const ShmConfig& cfg) : config(cfg) {}
    
    ~Impl() {
        stop();
        cleanup();
    }
    
    void cleanup() {
        if (shm_block && shm_block != MAP_FAILED) {
            munmap(shm_block, sizeof(QuaxisSharedBlock));
            shm_block = nullptr;
        }
        if (shm_fd >= 0) {
            close(shm_fd);
            shm_fd = -1;
        }
    }
    
    Result<void> open_shm() {
        // Открываем shared memory
        shm_fd = shm_open(config.path.c_str(), O_RDONLY, 0);
        if (shm_fd < 0) {
            return Err<void>(
                ErrorCode::ShmOpenFailed,
                std::format("Не удалось открыть shared memory '{}': {}", 
                           config.path, strerror(errno))
            );
        }
        
        // Маппим в память
        void* ptr = mmap(nullptr, sizeof(QuaxisSharedBlock), 
                        PROT_READ, MAP_SHARED, shm_fd, 0);
        if (ptr == MAP_FAILED) {
            close(shm_fd);
            shm_fd = -1;
            return Err<void>(
                ErrorCode::ShmMapFailed,
                std::format("Не удалось замапить shared memory: {}", strerror(errno))
            );
        }
        
        shm_block = static_cast<QuaxisSharedBlock*>(ptr);
        return {};
    }
    
    void worker_loop() {
        while (running.load(std::memory_order_relaxed)) {
            // Читаем sequence
            uint64_t seq = shm_block->sequence.load(std::memory_order_acquire);
            
            if (seq != last_sequence.load(std::memory_order_relaxed)) {
                // Новый блок!
                process_new_block(seq);
            } else if (!config.spin_wait) {
                // Poll режим - ждём
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            } else {
                // Spin-wait - пауза для CPU
                std::this_thread::yield();
            }
        }
    }
    
    void process_new_block(uint64_t seq) {
        // Читаем данные блока
        auto state = static_cast<ShmBlockState>(
            shm_block->state.load(std::memory_order_acquire)
        );
        
        // Проверяем валидность состояния
        if (state != ShmBlockState::Speculative && state != ShmBlockState::Confirmed) {
            return;
        }
        
        // Десериализуем заголовок
        std::array<uint8_t, 80> header_raw;
        std::memcpy(header_raw.data(), shm_block->header_raw, 80);
        
        auto header_result = BlockHeader::deserialize(
            ByteSpan(header_raw.data(), header_raw.size())
        );
        
        if (!header_result) {
            return;  // Ошибка десериализации
        }
        
        BlockHeader header = *header_result;
        uint32_t height = shm_block->height;
        int64_t coinbase_value = shm_block->coinbase_value;
        bool is_speculative = (state == ShmBlockState::Speculative);
        
        // Обновляем sequence
        last_sequence.store(seq, std::memory_order_relaxed);
        
        // Сохраняем последний блок
        {
            std::lock_guard<std::mutex> lock(last_block_mutex);
            last_block = header;
        }
        
        // Вызываем callback
        if (callback) {
            callback(header, height, coinbase_value, is_speculative);
        }
    }
    
    void stop() {
        running.store(false, std::memory_order_relaxed);
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
};

// =============================================================================
// ShmSubscriber
// =============================================================================

ShmSubscriber::ShmSubscriber(const ShmConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

ShmSubscriber::~ShmSubscriber() = default;

void ShmSubscriber::set_callback(NewBlockCallback callback) {
    impl_->callback = std::move(callback);
}

Result<void> ShmSubscriber::start() {
    if (impl_->running.load()) {
        return {};  // Уже запущен
    }
    
    // Открываем shared memory
    auto result = impl_->open_shm();
    if (!result) {
        return result;
    }
    
    // Запускаем worker thread
    impl_->running.store(true, std::memory_order_relaxed);
    impl_->worker_thread = std::thread([this] {
        impl_->worker_loop();
    });
    
    return {};
}

void ShmSubscriber::stop() {
    impl_->stop();
}

bool ShmSubscriber::is_running() const noexcept {
    return impl_->running.load(std::memory_order_relaxed);
}

uint64_t ShmSubscriber::get_sequence() const noexcept {
    return impl_->last_sequence.load(std::memory_order_relaxed);
}

std::optional<BlockHeader> ShmSubscriber::get_last_block() const {
    std::lock_guard<std::mutex> lock(impl_->last_block_mutex);
    if (impl_->last_block.timestamp == 0) {
        return std::nullopt;
    }
    return impl_->last_block;
}

// =============================================================================
// Фабричные функции
// =============================================================================

Result<void> create_shm_segment(std::string_view path) {
    // Создаём shared memory сегмент
    int fd = shm_open(path.data(), O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        return Err<void>(
            ErrorCode::ShmOpenFailed,
            std::format("Не удалось создать shared memory '{}': {}", 
                       path, strerror(errno))
        );
    }
    
    // Устанавливаем размер
    if (ftruncate(fd, sizeof(QuaxisSharedBlock)) < 0) {
        close(fd);
        shm_unlink(path.data());
        return Err<void>(
            ErrorCode::ShmOpenFailed,
            std::format("Не удалось установить размер shared memory: {}", strerror(errno))
        );
    }
    
    // Маппим и инициализируем
    void* ptr = mmap(nullptr, sizeof(QuaxisSharedBlock), 
                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        shm_unlink(path.data());
        return Err<void>(
            ErrorCode::ShmMapFailed,
            std::format("Не удалось замапить shared memory: {}", strerror(errno))
        );
    }
    
    // Инициализируем нулями
    std::memset(ptr, 0, sizeof(QuaxisSharedBlock));
    
    // Закрываем
    munmap(ptr, sizeof(QuaxisSharedBlock));
    close(fd);
    
    return {};
}

Result<void> remove_shm_segment(std::string_view path) {
    if (shm_unlink(path.data()) < 0 && errno != ENOENT) {
        return Err<void>(
            ErrorCode::ShmOpenFailed,
            std::format("Не удалось удалить shared memory '{}': {}", 
                       path, strerror(errno))
        );
    }
    return {};
}

} // namespace quaxis::bitcoin
