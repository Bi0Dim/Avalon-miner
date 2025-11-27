/**
 * @file bitcoin_bridge.cpp
 * @brief Реализация моста к Bitcoin Core
 */

#include "bitcoin_bridge.hpp"
#include "../monitoring/alerter.hpp"
#include "../monitoring/metrics.hpp"

#include <iostream>

namespace quaxis::bridge {

// =============================================================================
// Реализация
// =============================================================================

struct BitcoinBridge::Impl {
    BridgeConfig config;
    
    // Состояние
    std::atomic<bool> running{false};
    
    // SHM подписчик
    std::unique_ptr<bitcoin::ShmSubscriber> shm_subscriber;
    
    // Fallback менеджер
    std::unique_ptr<fallback::FallbackManager> fallback_manager;
    
    // Текущий шаблон
    std::optional<BlockTemplate> current_template;
    mutable std::mutex template_mutex;
    
    // Callbacks
    NewTemplateCallback template_callback;
    SourceChangeCallback source_change_callback;
    
    explicit Impl(const BridgeConfig& cfg) : config(cfg) {
        // Создаём fallback менеджер
        fallback_manager = std::make_unique<fallback::FallbackManager>(config.fallback);
        
        // Создаём SHM подписчик если включён
        if (config.shm.enabled) {
            shm_subscriber = std::make_unique<bitcoin::ShmSubscriber>(config.shm);
        }
    }
    
    void on_shm_block(
        const bitcoin::BlockHeader& header,
        uint32_t height,
        int64_t coinbase_value,
        bool is_speculative
    ) {
        BlockTemplate tmpl;
        tmpl.header = header;
        tmpl.height = height;
        tmpl.bits = header.bits;
        tmpl.coinbase_value = coinbase_value;
        tmpl.prev_block_hash = header.prev_block;
        tmpl.merkle_root = header.merkle_root;
        tmpl.received_at = std::chrono::steady_clock::now();
        tmpl.source = fallback::FallbackMode::PrimarySHM;
        tmpl.is_speculative = is_speculative;
        
        // Обновляем шаблон
        {
            std::lock_guard<std::mutex> lock(template_mutex);
            current_template = tmpl;
        }
        
        // Сигнализируем о получении
        if (fallback_manager) {
            fallback_manager->signal_job_received();
        }
        
        // Обновляем метрики
        monitoring::Metrics::instance().set_bitcoin_connected(true);
        monitoring::Metrics::instance().set_block_height(height);
        
        // Вызываем callback
        if (template_callback) {
            template_callback(tmpl);
        }
    }
    
    void on_stratum_job(const fallback::StratumJob& job) {
        BlockTemplate tmpl;
        tmpl.job_id = job.job_id;
        tmpl.coinbase1 = job.coinbase1;
        tmpl.coinbase2 = job.coinbase2;
        tmpl.received_at = std::chrono::steady_clock::now();
        tmpl.source = fallback::FallbackMode::FallbackStratum;
        tmpl.is_speculative = false;
        
        // Получаем extranonce из клиента
        auto* client = fallback_manager->get_stratum_client();
        if (client) {
            tmpl.extranonce1 = client->get_extranonce1();
            tmpl.extranonce2_size = client->get_extranonce2_size();
        }
        
        // Парсим hex данные из job
        // version
        if (job.version.size() >= 8) {
            tmpl.header.version = static_cast<int32_t>(
                std::stoul(job.version, nullptr, 16)
            );
        }
        
        // nbits
        if (job.nbits.size() >= 8) {
            tmpl.bits = static_cast<uint32_t>(
                std::stoul(job.nbits, nullptr, 16)
            );
            tmpl.header.bits = tmpl.bits;
        }
        
        // ntime
        if (job.ntime.size() >= 8) {
            tmpl.header.timestamp = static_cast<uint32_t>(
                std::stoul(job.ntime, nullptr, 16)
            );
        }
        
        // Обновляем шаблон
        {
            std::lock_guard<std::mutex> lock(template_mutex);
            current_template = tmpl;
        }
        
        // Вызываем callback
        if (template_callback) {
            template_callback(tmpl);
        }
    }
    
    void on_mode_change(fallback::FallbackMode old_mode, fallback::FallbackMode new_mode) {
        // Обновляем метрики
        monitoring::Metrics::instance().set_mode(fallback::to_prometheus_value(new_mode));
        monitoring::Metrics::instance().inc_fallback_switches();
        
        // Алертинг
        if (new_mode == fallback::FallbackMode::PrimarySHM) {
            monitoring::Alerter::instance().alert_primary_restored();
        } else {
            monitoring::Alerter::instance().alert_fallback_activated(
                fallback::to_string(new_mode)
            );
        }
        
        // Вызываем callback
        if (source_change_callback) {
            source_change_callback(old_mode, new_mode);
        }
    }
};

// =============================================================================
// Публичный API
// =============================================================================

BitcoinBridge::BitcoinBridge(const BridgeConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

BitcoinBridge::~BitcoinBridge() {
    stop();
}

Result<void> BitcoinBridge::start() {
    if (impl_->running) {
        return {};
    }
    
    // Настраиваем SHM callback
    if (impl_->shm_subscriber) {
        impl_->shm_subscriber->set_callback(
            [this](const bitcoin::BlockHeader& header, uint32_t height, 
                   int64_t coinbase_value, bool is_speculative) {
                impl_->on_shm_block(header, height, coinbase_value, is_speculative);
            }
        );
        
        auto result = impl_->shm_subscriber->start();
        if (!result) {
            // SHM не удалось запустить - это нормально, используем fallback
            std::cerr << "[BitcoinBridge] SHM недоступен: " 
                      << result.error().message << std::endl;
        }
    }
    
    // Настраиваем health check для fallback
    if (impl_->fallback_manager) {
        impl_->fallback_manager->set_shm_health_check([this]() {
            if (!impl_->shm_subscriber) return false;
            return impl_->shm_subscriber->is_running();
        });
        
        impl_->fallback_manager->set_mode_change_callback(
            [this](fallback::FallbackMode old_mode, fallback::FallbackMode new_mode) {
                impl_->on_mode_change(old_mode, new_mode);
            }
        );
        
        // Настраиваем Stratum callback
        auto* client = impl_->fallback_manager->get_stratum_client();
        if (client) {
            client->set_job_callback([this](const fallback::StratumJob& job) {
                impl_->on_stratum_job(job);
            });
            
            client->set_disconnect_callback([this](const std::string& reason) {
                monitoring::Alerter::instance().alert_stratum_error("pool", reason);
            });
        }
        
        impl_->fallback_manager->start();
    }
    
    impl_->running = true;
    
    return {};
}

void BitcoinBridge::stop() {
    impl_->running = false;
    
    if (impl_->shm_subscriber) {
        impl_->shm_subscriber->stop();
    }
    
    if (impl_->fallback_manager) {
        impl_->fallback_manager->stop();
    }
}

bool BitcoinBridge::is_running() const noexcept {
    return impl_->running;
}

std::optional<BlockTemplate> BitcoinBridge::get_template() const {
    std::lock_guard<std::mutex> lock(impl_->template_mutex);
    return impl_->current_template;
}

fallback::FallbackMode BitcoinBridge::current_source() const noexcept {
    if (impl_->fallback_manager) {
        return impl_->fallback_manager->current_mode();
    }
    return fallback::FallbackMode::PrimarySHM;
}

bool BitcoinBridge::is_bitcoin_connected() const {
    auto mode = current_source();
    
    switch (mode) {
        case fallback::FallbackMode::PrimarySHM:
            return impl_->shm_subscriber && impl_->shm_subscriber->is_running();
            
        case fallback::FallbackMode::FallbackZMQ:
            // ZMQ health check
            return true;  // TODO: реальная проверка
            
        case fallback::FallbackMode::FallbackStratum:
            return impl_->fallback_manager && 
                   impl_->fallback_manager->is_stratum_connected();
    }
    
    return false;
}

uint64_t BitcoinBridge::get_current_job_age_ms() const {
    std::lock_guard<std::mutex> lock(impl_->template_mutex);
    
    if (!impl_->current_template) {
        return 0;
    }
    
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - impl_->current_template->received_at
        ).count()
    );
}

Result<bool> BitcoinBridge::submit_share(
    const std::string& job_id,
    const std::string& extranonce2,
    const std::string& ntime,
    const std::string& nonce
) {
    auto mode = current_source();
    
    if (mode == fallback::FallbackMode::FallbackStratum) {
        auto* client = impl_->fallback_manager->get_stratum_client();
        if (!client) {
            return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, 
                "Stratum клиент не инициализирован"});
        }
        
        auto result = client->submit(job_id, extranonce2, ntime, nonce);
        if (!result) {
            return std::unexpected(result.error());
        }
        
        return result->accepted;
    }
    
    // Для SHM/ZMQ - блок отправляется напрямую через RPC
    // Здесь возвращаем успех, так как submit обрабатывается отдельно
    return true;
}

void BitcoinBridge::set_template_callback(NewTemplateCallback callback) {
    impl_->template_callback = std::move(callback);
}

void BitcoinBridge::set_source_change_callback(SourceChangeCallback callback) {
    impl_->source_change_callback = std::move(callback);
}

fallback::FallbackManager& BitcoinBridge::get_fallback_manager() {
    return *impl_->fallback_manager;
}

const fallback::FallbackManager& BitcoinBridge::get_fallback_manager() const {
    return *impl_->fallback_manager;
}

} // namespace quaxis::bridge
