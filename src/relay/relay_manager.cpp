/**
 * @file relay_manager.cpp
 * @brief Реализация менеджера FIBRE relay источников
 */

#include "relay_manager.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>

namespace quaxis::relay {

// =============================================================================
// Реализация RelayManager
// =============================================================================

struct RelayManager::Impl {
    /// @brief Конфигурация
    RelayConfig config_;
    
    /// @brief Пиры
    std::vector<std::unique_ptr<RelayPeer>> peers_;
    
    /// @brief Активные реконструкторы блоков (по хешу блока)
    std::map<Hash256, std::unique_ptr<BlockReconstructor>> reconstructors_;
    
    /// @brief Хеши уже полученных блоков (для дедупликации)
    std::set<Hash256> received_blocks_;
    
    /// @brief Callback для header
    RelayHeaderCallback header_callback_;
    
    /// @brief Callback для блока
    RelayBlockCallback block_callback_;
    
    /// @brief Рабочий поток
    std::thread worker_thread_;
    
    /// @brief Флаг работы
    std::atomic<bool> running_{false};
    
    /// @brief Статистика
    RelayManagerStats stats_;
    
    /// @brief Время запуска
    std::chrono::steady_clock::time_point start_time_;
    
    /// @brief Последняя высота блока
    std::atomic<uint32_t> last_block_height_{0};
    
    /// @brief Мьютекс
    mutable std::mutex mutex_;
    
    /**
     * @brief Конструктор
     */
    explicit Impl(const RelayConfig& config)
        : config_(config)
    {}
    
    /**
     * @brief Обработать пакет от пира
     */
    void on_packet(const FibrePacket& packet) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Ищем или создаём реконструктор
        auto it = reconstructors_.find(packet.header.block_hash);
        
        if (it == reconstructors_.end()) {
            // Новый блок
            
            // Проверяем, не получили ли мы уже этот блок
            if (received_blocks_.count(packet.header.block_hash) > 0) {
                ++stats_.duplicate_blocks;
                return;
            }
            
            // Создаём реконструктор
            FecParams fec_params;
            fec_params.data_chunk_count = packet.header.data_chunks;
            fec_params.fec_chunk_count = packet.header.fec_chunks();
            
            auto reconstructor = std::make_unique<BlockReconstructor>(
                packet.header.block_hash,
                packet.header.block_height,
                fec_params,
                config_.reconstruction_timeout
            );
            
            // Устанавливаем callbacks
            reconstructor->set_header_callback(
                [this](const bitcoin::BlockHeader& header, uint32_t height, const Hash256&) {
                    on_header_received(header, height);
                }
            );
            
            reconstructor->set_block_callback(
                [this](const std::vector<uint8_t>& data, uint32_t height, const Hash256& hash) {
                    on_block_received(data, height, hash);
                }
            );
            
            reconstructor->set_timeout_callback(
                [this](uint32_t height, const Hash256& hash, std::size_t, std::size_t) {
                    on_reconstruction_timeout(height, hash);
                }
            );
            
            it = reconstructors_.emplace(
                packet.header.block_hash,
                std::move(reconstructor)
            ).first;
        }
        
        // Передаём пакет реконструктору
        it->second->on_packet(packet);
    }
    
    /**
     * @brief Header получен
     */
    void on_header_received(const bitcoin::BlockHeader& header, uint32_t height) {
        last_block_height_.store(height);
        
        // Обновляем статистику latency
        // (упрощённо - можно улучшить)
        
        if (header_callback_) {
            header_callback_(header, BlockSource::UdpRelay);
        }
    }
    
    /**
     * @brief Блок полностью получен
     */
    void on_block_received(
        const std::vector<uint8_t>& data,
        uint32_t height,
        const Hash256& hash
    ) {
        // Помечаем блок как полученный
        received_blocks_.insert(hash);
        ++stats_.blocks_received;
        
        // Вызываем callback
        if (block_callback_) {
            block_callback_(data, height, BlockSource::UdpRelay);
        }
        
        // Удаляем реконструктор
        reconstructors_.erase(hash);
    }
    
    /**
     * @brief Таймаут реконструкции
     */
    void on_reconstruction_timeout(uint32_t /* height */, const Hash256& hash) {
        ++stats_.reconstruction_timeouts;
        reconstructors_.erase(hash);
    }
    
    /**
     * @brief Рабочий цикл
     */
    void worker_loop() {
        while (running_.load()) {
            // Опрашиваем все пиры
            for (auto& peer : peers_) {
                if (peer->is_connected()) {
                    peer->poll(100);
                }
            }
            
            // Обновляем состояние пиров
            for (auto& peer : peers_) {
                peer->update();
            }
            
            // Проверяем таймауты реконструкторов
            {
                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& [hash, reconstructor] : reconstructors_) {
                    reconstructor->check_timeout();
                }
            }
            
            // Небольшая пауза
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    /**
     * @brief Очистка старых данных
     */
    void cleanup_old_blocks() {
        // Ограничиваем размер кэша полученных блоков
        constexpr std::size_t MAX_RECEIVED_BLOCKS = 1000;
        
        if (received_blocks_.size() > MAX_RECEIVED_BLOCKS) {
            received_blocks_.clear();  // Простая очистка
        }
    }
};

RelayManager::RelayManager(const RelayConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
    // Создаём пиры из конфигурации
    for (const auto& peer_config : config.peers) {
        RelayPeerConfig cfg;
        cfg.host = peer_config.host;
        cfg.port = peer_config.port;
        cfg.trusted = peer_config.trusted;
        
        impl_->peers_.push_back(std::make_unique<RelayPeer>(cfg));
    }
}

RelayManager::~RelayManager() {
    stop();
}

Result<void> RelayManager::start() {
    if (impl_->running_.load()) {
        return {};  // Уже запущен
    }
    
    impl_->start_time_ = std::chrono::steady_clock::now();
    
    // Подключаемся ко всем пирам
    for (auto& peer : impl_->peers_) {
        peer->set_packet_callback([this](const FibrePacket& packet) {
            impl_->on_packet(packet);
        });
        
        auto result = peer->connect();
        if (!result) {
            // Логируем ошибку, но продолжаем с другими пирами
        }
    }
    
    // Запускаем рабочий поток
    impl_->running_.store(true);
    impl_->worker_thread_ = std::thread([this]() {
        impl_->worker_loop();
    });
    
    return {};
}

void RelayManager::stop() {
    if (!impl_->running_.load()) {
        return;
    }
    
    impl_->running_.store(false);
    
    if (impl_->worker_thread_.joinable()) {
        impl_->worker_thread_.join();
    }
    
    // Отключаемся от всех пиров
    for (auto& peer : impl_->peers_) {
        peer->disconnect();
    }
}

bool RelayManager::is_running() const noexcept {
    return impl_->running_.load();
}

void RelayManager::set_header_callback(RelayHeaderCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->header_callback_ = std::move(callback);
}

void RelayManager::set_block_callback(RelayBlockCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    impl_->block_callback_ = std::move(callback);
}

Result<void> RelayManager::add_peer(const RelayPeerConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    // Проверяем, нет ли уже такого пира
    for (const auto& peer : impl_->peers_) {
        if (peer->host() == config.host && peer->port() == config.port) {
            return std::unexpected(Error{
                ErrorCode::ConfigInvalidValue,
                "Пир уже существует: " + config.host + ":" + std::to_string(config.port)
            });
        }
    }
    
    auto peer = std::make_unique<RelayPeer>(config);
    
    peer->set_packet_callback([this](const FibrePacket& packet) {
        impl_->on_packet(packet);
    });
    
    if (impl_->running_.load()) {
        auto result = peer->connect();
        if (!result) {
            return result;
        }
    }
    
    impl_->peers_.push_back(std::move(peer));
    
    return {};
}

void RelayManager::remove_peer(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    auto it = std::remove_if(
        impl_->peers_.begin(),
        impl_->peers_.end(),
        [&](const auto& peer) {
            return peer->host() == host && peer->port() == port;
        }
    );
    
    impl_->peers_.erase(it, impl_->peers_.end());
}

std::size_t RelayManager::peer_count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->peers_.size();
}

std::size_t RelayManager::connected_peer_count() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    return static_cast<std::size_t>(std::count_if(
        impl_->peers_.begin(),
        impl_->peers_.end(),
        [](const auto& peer) { return peer->is_connected(); }
    ));
}

RelayManagerStats RelayManager::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    
    RelayManagerStats stats = impl_->stats_;
    stats.active_peers = impl_->peers_.size();
    stats.connected_peers = static_cast<std::size_t>(std::count_if(
        impl_->peers_.begin(),
        impl_->peers_.end(),
        [](const auto& peer) { return peer->is_connected(); }
    ));
    
    if (impl_->start_time_.time_since_epoch().count() > 0) {
        stats.uptime_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - impl_->start_time_
        ).count();
    }
    
    return stats;
}

const RelayConfig& RelayManager::config() const noexcept {
    return impl_->config_;
}

uint32_t RelayManager::last_block_height() const noexcept {
    return impl_->last_block_height_.load();
}

} // namespace quaxis::relay
