/**
 * @file status_reporter.cpp
 * @brief Реализация терминального репортёра статуса
 */

#include "status_reporter.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace quaxis::log {

// =============================================================================
// ANSI коды цветов
// =============================================================================

namespace ansi {
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM = "\033[2m";
    
    // Цвета текста
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* WHITE = "\033[37m";
    
    // Управление курсором
    constexpr const char* CLEAR_SCREEN = "\033[2J";
    constexpr const char* HOME = "\033[H";
    constexpr const char* CLEAR_LINE = "\033[2K";
    constexpr const char* SAVE_CURSOR = "\033[s";
    constexpr const char* RESTORE_CURSOR = "\033[u";
}

// =============================================================================
// Реализация
// =============================================================================

struct StatusReporter::Impl {
    LoggingConfig config;
    
    // Состояние
    std::atomic<bool> running{false};
    std::thread render_thread;
    std::chrono::steady_clock::time_point start_time;
    
    // Данные
    BitcoinStats bitcoin_stats;
    AsicStats asic_stats;
    ShmStats shm_stats;
    fallback::FallbackMode fallback_mode = fallback::FallbackMode::PrimarySHM;
    std::vector<std::string> active_chains;
    std::unordered_map<std::string, uint64_t> block_counts;
    
    // События
    std::deque<EventRecord> events;
    mutable std::mutex events_mutex;
    
    // Защита данных
    mutable std::mutex data_mutex;
    
    explicit Impl(const LoggingConfig& cfg) 
        : config(cfg)
        , start_time(std::chrono::steady_clock::now()) {}
    
    void render_loop() {
        while (running) {
            render_status();
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config.refresh_interval_ms)
            );
        }
    }
    
    void render_status() {
        std::string output = render_impl(config.color);
        
        // Перемещаем курсор в начало и выводим
        if (config.color) {
            std::cout << ansi::HOME << output << std::flush;
        } else {
            // В режиме без цветов просто добавляем разделитель
            std::cout << output << std::flush;
        }
    }
    
    std::string render_impl(bool use_color) const {
        std::ostringstream out;
        
        const char* bold = use_color ? ansi::BOLD : "";
        const char* reset = use_color ? ansi::RESET : "";
        const char* green = use_color ? ansi::GREEN : "";
        const char* yellow = use_color ? ansi::YELLOW : "";
        const char* red = use_color ? ansi::RED : "";
        const char* cyan = use_color ? ansi::CYAN : "";
        const char* dim = use_color ? ansi::DIM : "";
        
        std::lock_guard<std::mutex> lock(data_mutex);
        
        // === Заголовок ===
        out << bold << "═══════════════════════════════════════════════════════════════════\n"
            << "                    QUAXIS SOLO MINER v1.0.0\n"
            << "═══════════════════════════════════════════════════════════════════" << reset << "\n\n";
        
        // === Uptime ===
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        auto hours = uptime.count() / 3600;
        auto minutes = (uptime.count() % 3600) / 60;
        auto seconds = uptime.count() % 60;
        
        out << bold << "Uptime: " << reset
            << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds << "\n\n";
        
        // === Bitcoin Status ===
        out << bold << "Bitcoin:" << reset << "\n";
        out << "  Height: " << bitcoin_stats.height;
        if (bitcoin_stats.tip_age_seconds > 0) {
            out << " (tip age: " << bitcoin_stats.tip_age_seconds << "s)";
        }
        out << "\n";
        out << "  Connection: ";
        if (bitcoin_stats.connected) {
            out << green << "CONNECTED" << reset;
        } else {
            out << red << "DISCONNECTED" << reset;
        }
        out << "\n\n";
        
        // === Hashrate ===
        if (config.show_hashrate) {
            out << bold << "Hashrate:" << reset << "\n";
            double hashrate = asic_stats.estimated_hashrate_ths > 0 
                ? asic_stats.estimated_hashrate_ths 
                : config.rated_ths;
            out << "  " << std::fixed << std::setprecision(1) << hashrate << " TH/s";
            if (asic_stats.estimated_hashrate_ths == 0) {
                out << dim << " (rated)" << reset;
            }
            out << "\n\n";
        }
        
        // === ASIC Status ===
        out << bold << "ASIC:" << reset << "\n";
        out << "  Connected: " << asic_stats.connected_count << "\n";
        if (asic_stats.temperature_avg > 0) {
            out << "  Avg Temp: " << std::fixed << std::setprecision(1) 
                << asic_stats.temperature_avg << "°C\n";
        }
        out << "\n";
        
        // === Fallback Status ===
        out << bold << "Source:" << reset << " ";
        switch (fallback_mode) {
            case fallback::FallbackMode::PrimarySHM:
                out << green << "SHM (Primary)" << reset;
                break;
            case fallback::FallbackMode::FallbackZMQ:
                out << yellow << "ZMQ (Fallback)" << reset;
                break;
            case fallback::FallbackMode::FallbackStratum:
                out << yellow << "Stratum (Fallback)" << reset;
                break;
        }
        out << "\n";
        
        // === SHM Status ===
        out << bold << "SHM:" << reset << "\n";
        out << "  Spin Wait: " << (shm_stats.spin_wait_active ? "active" : "polling") << "\n";
        if (shm_stats.adaptive_mode) {
            out << "  Adaptive: enabled\n";
        }
        out << "  CPU Usage: " << std::fixed << std::setprecision(1) 
            << shm_stats.cpu_usage_percent << "%\n\n";
        
        // === Active Merged Chains ===
        out << bold << "Merged Mining Chains:" << reset << "\n";
        if (active_chains.empty()) {
            out << "  " << dim << "(none)" << reset << "\n";
        } else {
            for (const auto& chain : active_chains) {
                out << "  • " << chain;
                if (config.show_chain_block_counts) {
                    auto it = block_counts.find(chain);
                    if (it != block_counts.end() && it->second > 0) {
                        out << " (" << cyan << it->second << " blocks" << reset << ")";
                    }
                }
                out << "\n";
            }
        }
        out << "\n";
        
        // === Recent Events ===
        out << bold << "Recent Events:" << reset << "\n";
        {
            std::lock_guard<std::mutex> events_lock(events_mutex);
            if (events.empty()) {
                out << "  " << dim << "(no events)" << reset << "\n";
            } else {
                // Показываем последние 10 событий
                size_t start = events.size() > 10 ? events.size() - 10 : 0;
                for (size_t i = start; i < events.size(); ++i) {
                    const auto& event = events[i];
                    
                    // Время
                    auto time = std::chrono::system_clock::to_time_t(event.timestamp);
                    auto tm = *std::localtime(&time);
                    out << "  " << std::put_time(&tm, "%H:%M:%S") << " ";
                    
                    // Тип события
                    switch (event.type) {
                        case EventType::NEW_BLOCK:
                            out << cyan << "[" << to_string(event.type) << "]" << reset;
                            break;
                        case EventType::AUX_BLOCK_FOUND:
                        case EventType::BTC_BLOCK_FOUND:
                            if (config.highlight_found_blocks) {
                                out << bold << green << "[" << to_string(event.type) << "]" << reset;
                            } else {
                                out << green << "[" << to_string(event.type) << "]" << reset;
                            }
                            break;
                        case EventType::FALLBACK_ENTER:
                            out << yellow << "[" << to_string(event.type) << "]" << reset;
                            break;
                        case EventType::FALLBACK_EXIT:
                            out << green << "[" << to_string(event.type) << "]" << reset;
                            break;
                        case EventType::SUBMIT_OK:
                            out << green << "[" << to_string(event.type) << "]" << reset;
                            break;
                        case EventType::SUBMIT_FAIL:
                        case EventType::ERROR:
                            out << red << "[" << to_string(event.type) << "]" << reset;
                            break;
                    }
                    
                    // Сообщение
                    out << " " << event.message;
                    if (!event.chain_name.empty()) {
                        out << dim << " (" << event.chain_name << ")" << reset;
                    }
                    out << "\n";
                }
            }
        }
        
        out << "\n" << bold << "───────────────────────────────────────────────────────────────────" << reset << "\n";
        
        return out.str();
    }
};

// =============================================================================
// Публичный API
// =============================================================================

StatusReporter::StatusReporter(const LoggingConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

StatusReporter::~StatusReporter() {
    stop();
}

void StatusReporter::start() {
    if (impl_->running) {
        return;
    }
    
    impl_->running = true;
    impl_->start_time = std::chrono::steady_clock::now();
    
    // Очищаем экран если используются цвета
    if (impl_->config.color) {
        std::cout << ansi::CLEAR_SCREEN << ansi::HOME << std::flush;
    }
    
    impl_->render_thread = std::thread([this]() {
        impl_->render_loop();
    });
}

void StatusReporter::stop() {
    impl_->running = false;
    
    if (impl_->render_thread.joinable()) {
        impl_->render_thread.join();
    }
}

bool StatusReporter::is_running() const noexcept {
    return impl_->running;
}

void StatusReporter::update_bitcoin_stats(const BitcoinStats& stats) {
    std::lock_guard<std::mutex> lock(impl_->data_mutex);
    impl_->bitcoin_stats = stats;
}

void StatusReporter::update_asic_stats(const AsicStats& stats) {
    std::lock_guard<std::mutex> lock(impl_->data_mutex);
    impl_->asic_stats = stats;
}

void StatusReporter::update_shm_stats(const ShmStats& stats) {
    std::lock_guard<std::mutex> lock(impl_->data_mutex);
    impl_->shm_stats = stats;
}

void StatusReporter::update_fallback_mode(fallback::FallbackMode mode) {
    std::lock_guard<std::mutex> lock(impl_->data_mutex);
    impl_->fallback_mode = mode;
}

void StatusReporter::update_active_chains(const std::vector<std::string>& chains) {
    std::lock_guard<std::mutex> lock(impl_->data_mutex);
    impl_->active_chains = chains;
}

void StatusReporter::update_block_count(const std::string& chain_name, uint64_t count) {
    std::lock_guard<std::mutex> lock(impl_->data_mutex);
    impl_->block_counts[chain_name] = count;
}

void StatusReporter::log_event(EventType type, const std::string& message,
                               const std::string& chain_name) {
    std::lock_guard<std::mutex> lock(impl_->events_mutex);
    
    EventRecord record;
    record.type = type;
    record.timestamp = std::chrono::system_clock::now();
    record.message = message;
    record.chain_name = chain_name;
    
    impl_->events.push_back(std::move(record));
    
    // Ограничиваем размер истории
    while (impl_->events.size() > impl_->config.event_history) {
        impl_->events.pop_front();
    }
}

void StatusReporter::log_new_block(uint32_t height) {
    log_event(EventType::NEW_BLOCK, "New Bitcoin block at height " + std::to_string(height));
}

void StatusReporter::log_aux_block_found(const std::string& chain_name, uint32_t height) {
    log_event(EventType::AUX_BLOCK_FOUND, 
              "Found block at height " + std::to_string(height),
              chain_name);
}

void StatusReporter::log_btc_block_found(uint32_t height) {
    log_event(EventType::BTC_BLOCK_FOUND,
              "FOUND BITCOIN BLOCK at height " + std::to_string(height));
}

void StatusReporter::log_fallback_change(fallback::FallbackMode old_mode,
                                         fallback::FallbackMode new_mode) {
    if (new_mode == fallback::FallbackMode::PrimarySHM) {
        log_event(EventType::FALLBACK_EXIT,
                  std::string("Restored to ") + std::string(fallback::to_string(new_mode)));
    } else {
        log_event(EventType::FALLBACK_ENTER,
                  std::string("Switched from ") + std::string(fallback::to_string(old_mode)) +
                  " to " + std::string(fallback::to_string(new_mode)));
    }
    
    update_fallback_mode(new_mode);
}

void StatusReporter::log_submit(bool success, const std::string& chain_name) {
    if (success) {
        log_event(EventType::SUBMIT_OK, "Share/block submitted", chain_name);
    } else {
        log_event(EventType::SUBMIT_FAIL, "Submit failed", chain_name);
    }
}

void StatusReporter::log_error(const std::string& message) {
    log_event(EventType::ERROR, message);
}

std::string StatusReporter::render_plain() const {
    return impl_->render_impl(false);
}

std::string StatusReporter::render() const {
    return impl_->render_impl(impl_->config.color);
}

} // namespace quaxis::log
