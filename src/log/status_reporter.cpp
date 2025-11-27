/**
 * @file status_reporter.cpp
 * @brief Реализация компактного терминального вывода статуса
 */

#include "status_reporter.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <format>

namespace quaxis::log {

// =============================================================================
// Реализация
// =============================================================================

struct StatusReporter::Impl {
    StatusReporterConfig config;
    
    // Состояние
    std::atomic<bool> running{false};
    std::thread worker_thread;
    
    // Провайдер данных
    StatusDataProvider data_provider;
    
    // Кольцевой буфер событий
    std::deque<Event> events;
    mutable std::mutex events_mutex;
    
    // Количество строк последнего вывода (для перерисовки)
    size_t last_output_lines{0};
    
    explicit Impl(const StatusReporterConfig& cfg) : config(cfg) {}
    
    ~Impl() {
        stop();
    }
    
    void start() {
        if (running.exchange(true)) {
            return;
        }
        
        worker_thread = std::thread([this] {
            worker_loop();
        });
    }
    
    void stop() {
        if (!running.exchange(false)) {
            return;
        }
        
        if (worker_thread.joinable()) {
            worker_thread.join();
        }
    }
    
    void worker_loop() {
        while (running) {
            render_and_print();
            
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config.refresh_interval_ms)
            );
        }
    }
    
    void render_and_print() {
        if (!data_provider) {
            return;
        }
        
        StatusData data = data_provider();
        
        // Очистить предыдущий вывод (переместить курсор вверх и очистить строки)
        for (size_t i = 0; i < last_output_lines; ++i) {
            std::cout << ansi::MOVE_UP << ansi::CLEAR_LINE;
        }
        
        // Сформировать новый вывод
        std::string output;
        if (config.color) {
            output = render_status_ansi(data);
        } else {
            output = render_status_plain(data);
        }
        
        // Посчитать количество строк
        last_output_lines = 1;
        for (char c : output) {
            if (c == '\n') {
                ++last_output_lines;
            }
        }
        
        std::cout << output << std::flush;
    }
    
    std::string render_status_plain(const StatusData& data) const {
        std::ostringstream oss;
        
        // Строка 1: Uptime и Fallback
        oss << "Uptime: " << format_uptime(data.uptime);
        oss << " | Fallback: " << (data.fallback_active ? "ON" : "OFF");
        oss << "\n";
        
        // Строка 2: Hashrate и ASIC
        if (config.show_hashrate) {
            oss << "Hashrate: " << std::fixed << std::setprecision(2) 
                << data.hashrate_ths << " TH/s";
            oss << " | ASICs: " << data.asic_connections;
            oss << "\n";
        }
        
        // Строка 3: BTC info
        oss << "BTC Height: " << data.btc_height;
        oss << " | Tip Age: " << data.tip_age_ms << " ms";
        oss << " | Jobs: " << data.job_queue_depth;
        oss << " | Templates: " << data.prepared_templates;
        oss << "\n";
        
        // Строка 4: Chains
        oss << "Chains: ";
        if (data.active_chains.empty()) {
            oss << "none";
        } else {
            for (size_t i = 0; i < data.active_chains.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << data.active_chains[i];
            }
        }
        oss << "\n";
        
        // Строка 5: Found blocks (если показывать)
        if (config.show_chain_block_counts && !data.found_blocks.empty()) {
            oss << "Found Blocks: ";
            bool first = true;
            for (const auto& [chain, count] : data.found_blocks) {
                if (count > 0) {
                    if (!first) oss << ", ";
                    oss << chain << ":" << count;
                    first = false;
                }
            }
            if (first) {
                oss << "none";
            }
            oss << "\n";
        }
        
        // Строка 6: Adaptive spin
        oss << "Adaptive Spin: " << (data.adaptive_spin_active ? "ON" : "OFF");
        if (data.adaptive_spin_active) {
            oss << " | SHM CPU: " << std::fixed << std::setprecision(1) 
                << data.shm_cpu_usage_percent << "%";
        }
        oss << "\n";
        
        return oss.str();
    }
    
    std::string render_status_ansi(const StatusData& data) const {
        std::ostringstream oss;
        
        // Строка 1: Uptime и Fallback
        oss << ansi::BOLD << ansi::CYAN << "Uptime: " << ansi::RESET 
            << format_uptime(data.uptime);
        oss << " | ";
        oss << ansi::BOLD << "Fallback: " << ansi::RESET;
        if (data.fallback_active) {
            oss << ansi::RED << "ON" << ansi::RESET;
        } else {
            oss << ansi::GREEN << "OFF" << ansi::RESET;
        }
        oss << "\n";
        
        // Строка 2: Hashrate и ASIC
        if (config.show_hashrate) {
            oss << ansi::BOLD << ansi::YELLOW << "Hashrate: " << ansi::RESET 
                << std::fixed << std::setprecision(2) << data.hashrate_ths << " TH/s";
            oss << " | " << ansi::BOLD << "ASICs: " << ansi::RESET 
                << data.asic_connections;
            oss << "\n";
        }
        
        // Строка 3: BTC info
        oss << ansi::BOLD << ansi::BLUE << "BTC Height: " << ansi::RESET 
            << data.btc_height;
        oss << " | Tip Age: " << data.tip_age_ms << " ms";
        oss << " | Jobs: " << data.job_queue_depth;
        oss << " | Templates: " << data.prepared_templates;
        oss << "\n";
        
        // Строка 4: Chains
        oss << ansi::BOLD << "Chains: " << ansi::RESET;
        if (data.active_chains.empty()) {
            oss << ansi::RED << "none" << ansi::RESET;
        } else {
            for (size_t i = 0; i < data.active_chains.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << ansi::GREEN << data.active_chains[i] << ansi::RESET;
            }
        }
        oss << "\n";
        
        // Строка 5: Found blocks (если показывать)
        if (config.show_chain_block_counts) {
            oss << ansi::BOLD << ansi::MAGENTA << "Found Blocks: " << ansi::RESET;
            bool any_found = false;
            for (const auto& [chain, count] : data.found_blocks) {
                if (count > 0) {
                    if (any_found) oss << ", ";
                    if (config.highlight_found_blocks) {
                        oss << ansi::BOLD << ansi::GREEN;
                    }
                    oss << chain << ":" << count;
                    if (config.highlight_found_blocks) {
                        oss << ansi::RESET;
                    }
                    any_found = true;
                }
            }
            if (!any_found) {
                oss << "none";
            }
            oss << "\n";
        }
        
        // Строка 6: Adaptive spin
        oss << ansi::BOLD << "Adaptive Spin: " << ansi::RESET;
        if (data.adaptive_spin_active) {
            oss << ansi::GREEN << "ON" << ansi::RESET;
            oss << " | SHM CPU: " << std::fixed << std::setprecision(1) 
                << data.shm_cpu_usage_percent << "%";
        } else {
            oss << "OFF";
        }
        oss << "\n";
        
        return oss.str();
    }
    
    std::string render_event(const Event& event, bool with_color) const {
        std::ostringstream oss;
        
        // Timestamp
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - event.timestamp
        ).count();
        
        oss << "[" << std::setw(6) << age << "s ago] ";
        
        // Event type with color
        if (with_color) {
            switch (event.type) {
                case EventType::NewBlock:
                case EventType::AuxBlockFound:
                case EventType::BtcBlockFound:
                case EventType::SubmitOk:
                case EventType::FallbackExit:
                    oss << ansi::GREEN;
                    break;
                case EventType::Error:
                case EventType::SubmitFail:
                case EventType::FallbackEnter:
                    oss << ansi::RED;
                    break;
                default:
                    oss << ansi::YELLOW;
            }
        }
        
        oss << event_type_to_string(event.type);
        
        if (with_color) {
            oss << ansi::RESET;
        }
        
        // Message
        if (!event.message.empty()) {
            oss << ": " << event.message;
        }
        
        // Data
        if (!event.data.empty()) {
            oss << " [" << event.data << "]";
        }
        
        return oss.str();
    }
    
    static std::string format_uptime(std::chrono::seconds uptime) {
        auto total_seconds = uptime.count();
        auto days = total_seconds / 86400;
        auto hours = (total_seconds % 86400) / 3600;
        auto minutes = (total_seconds % 3600) / 60;
        auto seconds = total_seconds % 60;
        
        std::ostringstream oss;
        if (days > 0) {
            oss << days << "d ";
        }
        oss << std::setfill('0') << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds;
        
        return oss.str();
    }
};

// =============================================================================
// Публичный API
// =============================================================================

StatusReporter::StatusReporter(const StatusReporterConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

StatusReporter::~StatusReporter() = default;

void StatusReporter::start() {
    impl_->start();
}

void StatusReporter::stop() {
    impl_->stop();
}

bool StatusReporter::is_running() const noexcept {
    return impl_->running;
}

void StatusReporter::set_data_provider(StatusDataProvider provider) {
    impl_->data_provider = std::move(provider);
}

void StatusReporter::add_event(EventType type, std::string_view message, std::string_view data) {
    Event event;
    event.type = type;
    event.timestamp = std::chrono::steady_clock::now();
    event.message = std::string(message);
    event.data = std::string(data);
    
    std::lock_guard<std::mutex> lock(impl_->events_mutex);
    impl_->events.push_back(std::move(event));
    
    // Удаляем старые события если превышен лимит
    while (impl_->events.size() > impl_->config.event_history) {
        impl_->events.pop_front();
    }
}

std::vector<Event> StatusReporter::get_events(size_t count) const {
    std::lock_guard<std::mutex> lock(impl_->events_mutex);
    
    std::vector<Event> result;
    
    if (count == 0 || count >= impl_->events.size()) {
        result.assign(impl_->events.begin(), impl_->events.end());
    } else {
        auto start = impl_->events.end() - static_cast<std::ptrdiff_t>(count);
        result.assign(start, impl_->events.end());
    }
    
    return result;
}

void StatusReporter::clear_events() {
    std::lock_guard<std::mutex> lock(impl_->events_mutex);
    impl_->events.clear();
}

std::string StatusReporter::render_status_plain(const StatusData& data) const {
    return impl_->render_status_plain(data);
}

std::string StatusReporter::render_status_ansi(const StatusData& data) const {
    return impl_->render_status_ansi(data);
}

std::string StatusReporter::render_event(const Event& event, bool with_color) const {
    return impl_->render_event(event, with_color);
}

} // namespace quaxis::log
