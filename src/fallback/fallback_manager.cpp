/**
 * @file fallback_manager.cpp
 * @brief Реализация менеджера автоматического переключения
 */

#include "fallback_manager.hpp"

#include <iostream>

namespace quaxis::fallback {

// =============================================================================
// Реализация Fallback Manager
// =============================================================================

struct FallbackManager::Impl {
    // Конфигурация
    FallbackConfig config;
    
    // Состояние
    std::atomic<FallbackMode> mode{FallbackMode::PrimarySHM};
    std::atomic<bool> running{false};
    
    // Здоровье источников
    SourceHealth shm_health;
    SourceHealth zmq_health;
    SourceHealth stratum_health;
    
    // Статистика
    FallbackStats stats;
    std::chrono::steady_clock::time_point fallback_started;
    
    // Stratum клиент
    std::unique_ptr<StratumClient> stratum_client;
    
    // Health check функции
    ShmHealthCheck shm_check;
    ZmqHealthCheck zmq_check;
    
    // Callback при смене режима
    ModeChangeCallback mode_change_callback;
    
    // Поток мониторинга
    std::thread monitor_thread;
    
    // Мьютекс
    mutable std::mutex mutex;
    
    explicit Impl(const FallbackConfig& cfg) : config(cfg) {
        // Инициализация здоровья
        auto now = std::chrono::steady_clock::now();
        shm_health.last_check = now;
        zmq_health.last_check = now;
        stratum_health.last_check = now;
        
        // Создание Stratum клиента если настроен
        if (!config.stratum_pools.empty()) {
            const auto* pool = config.get_active_pool();
            if (pool) {
                stratum_client = std::make_unique<StratumClient>(*pool);
            }
        }
    }
    
    void monitor_loop() {
        while (running) {
            check_health();
            
            // Спим интервал проверки
            std::this_thread::sleep_for(config.timeouts.primary_health_check);
        }
    }
    
    void check_health() {
        auto now = std::chrono::steady_clock::now();
        
        // Проверка SHM
        {
            std::lock_guard<std::mutex> lock(mutex);
            shm_health.last_check = now;
            shm_health.total_checks++;
            
            bool available = shm_check ? shm_check() : false;
            
            if (available) {
                shm_health.available = true;
                shm_health.last_success = now;
                shm_health.consecutive_failures = 0;
                shm_health.successful_checks++;
            } else {
                shm_health.consecutive_failures++;
                
                // Если превышен таймаут, считаем недоступным
                auto since_last_success = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - shm_health.last_success
                );
                if (since_last_success > config.timeouts.primary_timeout) {
                    shm_health.available = false;
                }
            }
        }
        
        // Проверка ZMQ
        if (config.zmq.enabled) {
            std::lock_guard<std::mutex> lock(mutex);
            zmq_health.last_check = now;
            zmq_health.total_checks++;
            
            bool available = zmq_check ? zmq_check() : false;
            
            if (available) {
                zmq_health.available = true;
                zmq_health.last_success = now;
                zmq_health.consecutive_failures = 0;
                zmq_health.successful_checks++;
            } else {
                zmq_health.consecutive_failures++;
                zmq_health.available = false;
            }
        }
        
        // Проверка Stratum
        if (stratum_client) {
            std::lock_guard<std::mutex> lock(mutex);
            stratum_health.last_check = now;
            stratum_health.total_checks++;
            
            if (stratum_client->is_connected()) {
                stratum_health.available = true;
                stratum_health.last_success = now;
                stratum_health.consecutive_failures = 0;
                stratum_health.successful_checks++;
            } else {
                stratum_health.consecutive_failures++;
                stratum_health.available = false;
            }
        }
        
        // Логика переключения
        auto current = mode.load();
        
        if (current == FallbackMode::PrimarySHM) {
            // Если primary недоступен, переключаемся
            if (!shm_health.available) {
                switch_to_best_fallback();
            }
        } else {
            // Если мы в fallback, пытаемся вернуться к primary
            if (shm_health.available) {
                restore_primary();
            } else if (current == FallbackMode::FallbackZMQ && !zmq_health.available) {
                // ZMQ тоже умер, переключаемся на Stratum
                switch_to_stratum();
            }
        }
    }
    
    void switch_to_best_fallback() {
        auto old_mode = mode.load();
        
        // Пробуем ZMQ первым
        if (config.zmq.enabled && zmq_health.available) {
            mode = FallbackMode::FallbackZMQ;
            stats.zmq_switches++;
        } else if (stratum_client) {
            // Пробуем подключиться к Stratum
            if (!stratum_client->is_connected()) {
                auto result = stratum_client->connect();
                if (result) {
                    stratum_health.available = true;
                    stratum_health.last_success = std::chrono::steady_clock::now();
                }
            }
            
            if (stratum_client->is_connected()) {
                mode = FallbackMode::FallbackStratum;
                stats.stratum_switches++;
            }
        }
        
        auto new_mode = mode.load();
        if (new_mode != old_mode) {
            fallback_started = std::chrono::steady_clock::now();
            
            if (mode_change_callback) {
                mode_change_callback(old_mode, new_mode);
            }
            
            std::cerr << "[FallbackManager] Переключение: " 
                      << to_string(old_mode) << " -> " << to_string(new_mode) << std::endl;
        }
    }
    
    void switch_to_stratum() {
        if (!stratum_client) return;
        
        auto old_mode = mode.load();
        
        if (!stratum_client->is_connected()) {
            auto result = stratum_client->connect();
            if (!result) {
                std::cerr << "[FallbackManager] Не удалось подключиться к Stratum: " 
                          << result.error().message << std::endl;
                return;
            }
        }
        
        mode = FallbackMode::FallbackStratum;
        stats.stratum_switches++;
        
        if (mode_change_callback && old_mode != FallbackMode::FallbackStratum) {
            mode_change_callback(old_mode, FallbackMode::FallbackStratum);
        }
    }
    
    void restore_primary() {
        auto old_mode = mode.load();
        
        if (old_mode == FallbackMode::PrimarySHM) return;
        
        mode = FallbackMode::PrimarySHM;
        stats.primary_restorations++;
        
        // Обновляем статистику времени в fallback
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - fallback_started
        );
        stats.fallback_duration_seconds += static_cast<uint64_t>(duration.count());
        
        // Отключаем Stratum если был подключён
        if (stratum_client && stratum_client->is_connected()) {
            stratum_client->disconnect();
        }
        
        if (mode_change_callback) {
            mode_change_callback(old_mode, FallbackMode::PrimarySHM);
        }
        
        std::cerr << "[FallbackManager] Восстановление primary SHM" << std::endl;
    }
};

// =============================================================================
// Публичный API
// =============================================================================

FallbackManager::FallbackManager(const FallbackConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

FallbackManager::~FallbackManager() {
    stop();
}

void FallbackManager::start() {
    if (impl_->running) return;
    
    impl_->running = true;
    
    // Инициализируем здоровье SHM как доступное
    impl_->shm_health.available = true;
    impl_->shm_health.last_success = std::chrono::steady_clock::now();
    
    impl_->monitor_thread = std::thread([this]() {
        impl_->monitor_loop();
    });
}

void FallbackManager::stop() {
    impl_->running = false;
    
    if (impl_->monitor_thread.joinable()) {
        impl_->monitor_thread.join();
    }
    
    if (impl_->stratum_client) {
        impl_->stratum_client->disconnect();
    }
}

bool FallbackManager::is_running() const noexcept {
    return impl_->running;
}

void FallbackManager::set_shm_health_check(ShmHealthCheck check) {
    impl_->shm_check = std::move(check);
}

void FallbackManager::set_zmq_health_check(ZmqHealthCheck check) {
    impl_->zmq_check = std::move(check);
}

void FallbackManager::check_primary_health() {
    impl_->check_health();
}

void FallbackManager::signal_job_received() {
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    switch (impl_->mode.load()) {
        case FallbackMode::PrimarySHM:
            impl_->shm_health.last_job_received = now;
            impl_->shm_health.last_success = now;
            impl_->shm_health.consecutive_failures = 0;
            impl_->shm_health.available = true;
            break;
            
        case FallbackMode::FallbackZMQ:
            impl_->zmq_health.last_job_received = now;
            impl_->zmq_health.last_success = now;
            break;
            
        case FallbackMode::FallbackStratum:
            impl_->stratum_health.last_job_received = now;
            impl_->stratum_health.last_success = now;
            break;
    }
}

void FallbackManager::switch_to_fallback() {
    impl_->switch_to_best_fallback();
}

void FallbackManager::try_restore_primary() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    if (impl_->shm_health.available) {
        impl_->restore_primary();
    }
}

void FallbackManager::set_mode(FallbackMode mode) {
    auto old_mode = impl_->mode.exchange(mode);
    
    if (old_mode != mode && impl_->mode_change_callback) {
        impl_->mode_change_callback(old_mode, mode);
    }
}

FallbackMode FallbackManager::current_mode() const noexcept {
    return impl_->mode;
}

SourceHealth FallbackManager::get_shm_health() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->shm_health;
}

SourceHealth FallbackManager::get_zmq_health() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->zmq_health;
}

SourceHealth FallbackManager::get_stratum_health() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stratum_health;
}

FallbackStats FallbackManager::get_stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

bool FallbackManager::is_stratum_connected() const {
    if (!impl_->stratum_client) return false;
    return impl_->stratum_client->is_connected();
}

StratumClient* FallbackManager::get_stratum_client() {
    return impl_->stratum_client.get();
}

std::optional<StratumJob> FallbackManager::get_stratum_job() const {
    if (!impl_->stratum_client) return std::nullopt;
    return impl_->stratum_client->get_current_job();
}

void FallbackManager::set_mode_change_callback(ModeChangeCallback callback) {
    impl_->mode_change_callback = std::move(callback);
}

} // namespace quaxis::fallback
