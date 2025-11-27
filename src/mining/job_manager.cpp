/**
 * @file job_manager.cpp
 * @brief Реализация менеджера заданий
 * 
 * IMPORTANT: Uses ExtrannonceManager for per-ASIC connection extranonce
 * management to prevent duplicate work across connections.
 */

#include "job_manager.hpp"
#include "extranonce_manager.hpp"
#include "../core/byte_order.hpp"

#include <algorithm>

namespace quaxis::mining {

// =============================================================================
// Job сериализация
// =============================================================================

std::array<uint8_t, constants::JOB_MESSAGE_SIZE> Job::serialize() const noexcept {
    std::array<uint8_t, constants::JOB_MESSAGE_SIZE> data{};
    uint8_t* ptr = data.data();
    
    // midstate (32 байта)
    auto midstate_bytes = crypto::state_to_bytes(midstate);
    std::memcpy(ptr, midstate_bytes.data(), 32);
    ptr += 32;
    
    // timestamp (4 байта, little-endian)
    write_le32(ptr, timestamp);
    ptr += 4;
    
    // bits (4 байта, little-endian)
    write_le32(ptr, bits);
    ptr += 4;
    
    // nonce (4 байта, little-endian)
    write_le32(ptr, nonce);
    ptr += 4;
    
    // job_id (4 байта, little-endian)
    write_le32(ptr, job_id);
    
    return data;
}

Result<Job> Job::deserialize(ByteSpan data) {
    if (data.size() < constants::JOB_MESSAGE_SIZE) {
        return Err<Job>(ErrorCode::MiningInvalidJob, "Недостаточно данных для задания");
    }
    
    Job job;
    const uint8_t* ptr = data.data();
    
    // midstate
    crypto::Sha256Midstate midstate_bytes;
    std::memcpy(midstate_bytes.data(), ptr, 32);
    job.midstate = crypto::bytes_to_state(midstate_bytes);
    ptr += 32;
    
    // timestamp
    job.timestamp = read_le32(ptr);
    ptr += 4;
    
    // bits
    job.bits = read_le32(ptr);
    ptr += 4;
    
    // nonce
    job.nonce = read_le32(ptr);
    ptr += 4;
    
    // job_id
    job.job_id = read_le32(ptr);
    
    job.created_at = std::chrono::steady_clock::now();
    
    return job;
}

bool Job::is_stale(uint32_t max_age) const noexcept {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - created_at);
    return age.count() > static_cast<int64_t>(max_age);
}

// =============================================================================
// Share сериализация
// =============================================================================

std::array<uint8_t, constants::SHARE_MESSAGE_SIZE> Share::serialize() const noexcept {
    std::array<uint8_t, constants::SHARE_MESSAGE_SIZE> data{};
    
    write_le32(data.data(), job_id);
    write_le32(data.data() + 4, nonce);
    
    return data;
}

Result<Share> Share::deserialize(ByteSpan data) {
    if (data.size() < constants::SHARE_MESSAGE_SIZE) {
        return Err<Share>(ErrorCode::MiningInvalidNonce, "Недостаточно данных для share");
    }
    
    Share share;
    share.job_id = read_le32(data.data());
    share.nonce = read_le32(data.data() + 4);
    
    return share;
}

// =============================================================================
// JobManager реализация
// =============================================================================

struct JobManager::Impl {
    MiningConfig config;
    bitcoin::CoinbaseBuilder coinbase_builder;
    
    NewJobCallback new_job_callback;
    BlockFoundCallback block_found_callback;
    
    mutable std::mutex mutex;
    
    // Per-connection extranonce management
    ExtrannonceManager extranonce_manager;
    
    // Текущий шаблон блока
    std::optional<bitcoin::BlockTemplate> current_template;
    bool is_speculative = false;
    
    // Активные задания (job_id -> Job)
    std::unordered_map<uint32_t, Job> jobs;
    
    // Счётчики
    uint32_t next_job_id = 1;
    
    Impl(const MiningConfig& cfg, bitcoin::CoinbaseBuilder builder)
        : config(cfg)
        , coinbase_builder(std::move(builder))
        , extranonce_manager(1)  // Start extranonces from 1
    {}
    
    void clear_jobs() {
        jobs.clear();
    }
    
    Job create_job() {
        if (!current_template) {
            return {};
        }
        
        Job job;
        job.job_id = next_job_id++;
        job.midstate = current_template->header_midstate;
        job.timestamp = current_template->header.timestamp;
        job.bits = current_template->header.bits;
        job.nonce = 0;
        job.height = current_template->height;
        job.target = current_template->target;
        job.is_speculative = is_speculative;
        job.created_at = std::chrono::steady_clock::now();
        
        // Сохраняем задание
        jobs[job.job_id] = job;
        
        // Ограничиваем количество заданий
        if (jobs.size() > config.job_queue_size) {
            // Удаляем самое старое задание
            auto oldest = std::min_element(jobs.begin(), jobs.end(),
                [](const auto& a, const auto& b) {
                    return a.second.job_id < b.second.job_id;
                });
            if (oldest != jobs.end()) {
                jobs.erase(oldest);
            }
        }
        
        return job;
    }
    
    /**
     * @brief Create a job with a specific extranonce
     * 
     * This updates the template with the given extranonce and creates a job.
     * Used for per-connection job creation.
     * 
     * @param extranonce The extranonce value for this job
     * @return Job The created job
     */
    Job create_job_with_extranonce(uint64_t extranonce) {
        if (!current_template) {
            return {};
        }
        
        // Create a copy of the template and update it with this extranonce
        auto updated_template = *current_template;
        updated_template.update_extranonce(extranonce);
        
        Job job;
        job.job_id = next_job_id++;
        job.midstate = updated_template.header_midstate;
        job.timestamp = updated_template.header.timestamp;
        job.bits = updated_template.header.bits;
        job.nonce = 0;
        job.height = updated_template.height;
        job.target = updated_template.target;
        job.is_speculative = is_speculative;
        job.created_at = std::chrono::steady_clock::now();
        
        // Store the job
        jobs[job.job_id] = job;
        
        // Limit job count
        if (jobs.size() > config.job_queue_size) {
            auto oldest = std::min_element(jobs.begin(), jobs.end(),
                [](const auto& a, const auto& b) {
                    return a.second.job_id < b.second.job_id;
                });
            if (oldest != jobs.end()) {
                jobs.erase(oldest);
            }
        }
        
        return job;
    }
};

JobManager::JobManager(
    const MiningConfig& config,
    bitcoin::CoinbaseBuilder coinbase_builder
) : impl_(std::make_unique<Impl>(config, std::move(coinbase_builder)))
{
}

JobManager::~JobManager() = default;

void JobManager::on_new_block(
    const bitcoin::BlockTemplate& block_template,
    bool is_speculative
) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    // Очищаем старые задания
    impl_->clear_jobs();
    
    // Сохраняем новый шаблон
    impl_->current_template = block_template;
    impl_->is_speculative = is_speculative;
    
    // NOTE: Do NOT increment a global extranonce here!
    // Each connection has its own extranonce managed by ExtrannonceManager.
    // The template will be updated with connection-specific extranonce
    // when get_next_job_for_connection() is called.
    //
    // When a new block arrives:
    // 1. Old jobs are cleared (above)
    // 2. New template is stored
    // 3. Each connection will get a new job with THEIR unique extranonce
    //    via get_next_job_for_connection() when the Server broadcasts jobs
    // 4. Extranonces remain stable per-connection across block changes
}

void JobManager::confirm_speculative_block() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (impl_->current_template) {
        impl_->is_speculative = false;
        impl_->current_template->is_speculative = false;
        
        // Обновляем все задания
        for (auto& [id, job] : impl_->jobs) {
            job.is_speculative = false;
        }
    }
}

void JobManager::invalidate_speculative_block() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (impl_->is_speculative) {
        impl_->clear_jobs();
        impl_->current_template = std::nullopt;
        impl_->is_speculative = false;
    }
}

std::optional<Job> JobManager::get_next_job() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->current_template) {
        return std::nullopt;
    }
    
    Job job = impl_->create_job();
    
    // Вызываем callback
    if (impl_->new_job_callback) {
        impl_->new_job_callback(job);
    }
    
    return job;
}

std::optional<Job> JobManager::get_next_job_for_connection(uint32_t connection_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (!impl_->current_template) {
        return std::nullopt;
    }
    
    // Get this connection's unique extranonce
    auto extranonce = impl_->extranonce_manager.get_extranonce(connection_id);
    if (!extranonce) {
        return std::nullopt;  // Connection not registered
    }
    
    // Create job with THIS connection's extranonce
    Job job = impl_->create_job_with_extranonce(*extranonce);
    
    // Call callback
    if (impl_->new_job_callback) {
        impl_->new_job_callback(job);
    }
    
    return job;
}

std::optional<Job> JobManager::get_job(uint32_t job_id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    auto it = impl_->jobs.find(job_id);
    if (it != impl_->jobs.end()) {
        return it->second;
    }
    return std::nullopt;
}

// =========================================================================
// Connection Management (ExtrannonceManager integration)
// =========================================================================

uint64_t JobManager::register_connection(uint32_t connection_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->extranonce_manager.assign_extranonce(connection_id);
}

void JobManager::unregister_connection(uint32_t connection_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->extranonce_manager.release_extranonce(connection_id);
}

std::optional<uint64_t> JobManager::get_connection_extranonce(uint32_t connection_id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->extranonce_manager.get_extranonce(connection_id);
}

std::size_t JobManager::active_connection_count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->extranonce_manager.active_count();
}

void JobManager::set_new_job_callback(NewJobCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->new_job_callback = std::move(callback);
}

void JobManager::set_block_found_callback(BlockFoundCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->block_found_callback = std::move(callback);
}

std::size_t JobManager::active_job_count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->jobs.size();
}

uint64_t JobManager::current_extranonce() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // Returns the next extranonce that will be assigned
    return impl_->extranonce_manager.peek_next_extranonce();
}

uint32_t JobManager::current_height() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->current_template) {
        return impl_->current_template->height;
    }
    return 0;
}

bool JobManager::has_template() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->current_template.has_value();
}

} // namespace quaxis::mining
