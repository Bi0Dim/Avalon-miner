/**
 * @file stratum_client.cpp
 * @brief Реализация Stratum v1 клиента
 */

#include "stratum_client.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <queue>

namespace quaxis::fallback {

// =============================================================================
// Реализация Stratum Client
// =============================================================================

struct StratumClient::Impl {
    // Конфигурация
    StratumPoolConfig config;
    
    // Состояние
    std::atomic<StratumState> state{StratumState::Disconnected};
    std::atomic<bool> running{false};
    
    // Сокет
    int socket_fd{-1};
    
    // Данные подписки
    std::optional<SubscribeResult> subscribe_result;
    
    // Текущее задание
    std::optional<StratumJob> current_job;
    
    // Сложность
    std::atomic<double> difficulty{1.0};
    
    // Счётчик ID запросов
    std::atomic<uint64_t> request_id{1};
    
    // Мьютекс для потокобезопасности
    mutable std::mutex mutex;
    
    // Callbacks
    JobCallback job_callback;
    DifficultyCallback difficulty_callback;
    DisconnectCallback disconnect_callback;
    
    // Фоновый поток для чтения
    std::thread read_thread;
    
    // Буфер чтения
    std::string read_buffer;
    
    // Очередь ожидающих ответов
    struct PendingRequest {
        uint64_t id;
        std::string method;
        std::chrono::steady_clock::time_point sent_at;
    };
    std::queue<PendingRequest> pending_requests;
    
    explicit Impl(const StratumPoolConfig& cfg) : config(cfg) {}
    
    ~Impl() {
        close_socket();
    }
    
    void close_socket() {
        if (socket_fd >= 0) {
            ::close(socket_fd);
            socket_fd = -1;
        }
    }
    
    bool create_socket() {
        socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            return false;
        }
        
        // Установить TCP_NODELAY для минимальной латентности
        int flag = 1;
        ::setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        return true;
    }
    
    bool connect_to_pool() {
        struct hostent* host = ::gethostbyname(config.host.c_str());
        if (!host) {
            return false;
        }
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config.port);
        std::memcpy(&addr.sin_addr, host->h_addr_list[0], static_cast<size_t>(host->h_length));
        
        if (::connect(socket_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            return false;
        }
        
        return true;
    }
    
    bool send_json(const std::string& json) {
        std::string line = json + "\n";
        ssize_t sent = ::send(socket_fd, line.c_str(), line.size(), 0);
        return sent == static_cast<ssize_t>(line.size());
    }
    
    std::string build_subscribe() {
        uint64_t id = request_id++;
        std::ostringstream ss;
        ss << R"({"id":)" << id << R"(,"method":"mining.subscribe","params":["quaxis/1.0"]})";
        
        std::lock_guard<std::mutex> lock(mutex);
        pending_requests.push({id, "mining.subscribe", std::chrono::steady_clock::now()});
        
        return ss.str();
    }
    
    std::string build_authorize() {
        uint64_t id = request_id++;
        std::ostringstream ss;
        ss << R"({"id":)" << id << R"(,"method":"mining.authorize","params":[")" 
           << config.user << R"(",")" << config.password << R"("]})";
        
        std::lock_guard<std::mutex> lock(mutex);
        pending_requests.push({id, "mining.authorize", std::chrono::steady_clock::now()});
        
        return ss.str();
    }
    
    std::string build_submit(const std::string& job_id, 
                             const std::string& extranonce2,
                             const std::string& ntime,
                             const std::string& nonce) {
        uint64_t id = request_id++;
        std::ostringstream ss;
        ss << R"({"id":)" << id << R"(,"method":"mining.submit","params":[")" 
           << config.user << R"(",")" << job_id << R"(",")" 
           << extranonce2 << R"(",")" << ntime << R"(",")" << nonce << R"("]})";
        
        std::lock_guard<std::mutex> lock(mutex);
        pending_requests.push({id, "mining.submit", std::chrono::steady_clock::now()});
        
        return ss.str();
    }
    
    void process_line(const std::string& line) {
        // Простой парсинг JSON ответов
        // В реальном коде стоит использовать nlohmann/json
        
        if (line.find("\"method\"") != std::string::npos) {
            // Это уведомление от пула
            if (line.find("mining.notify") != std::string::npos) {
                process_notify(line);
            } else if (line.find("mining.set_difficulty") != std::string::npos) {
                process_set_difficulty(line);
            }
        } else if (line.find("\"result\"") != std::string::npos) {
            // Это ответ на наш запрос
            process_response(line);
        }
    }
    
    void process_notify(const std::string& line) {
        // Парсинг mining.notify
        // Формат: {"id":null,"method":"mining.notify","params":[...]}
        
        StratumJob job;
        job.received_at = std::chrono::steady_clock::now();
        
        // Извлечение параметров (упрощённый парсинг)
        auto params_start = line.find("\"params\":");
        if (params_start == std::string::npos) return;
        
        auto arr_start = line.find('[', params_start);
        if (arr_start == std::string::npos) return;
        
        // Парсим массив параметров
        std::vector<std::string> params;
        auto pos = arr_start + 1;
        
        while (pos < line.size()) {
            // Пропуск пробелов
            while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
            
            if (line[pos] == ']') break;
            if (line[pos] == ',') { pos++; continue; }
            
            if (line[pos] == '"') {
                // Строковый параметр
                auto end = line.find('"', pos + 1);
                if (end != std::string::npos) {
                    params.push_back(line.substr(pos + 1, end - pos - 1));
                    pos = end + 1;
                } else {
                    break;
                }
            } else if (line[pos] == '[') {
                // Массив (merkle_branch)
                auto end = line.find(']', pos);
                if (end != std::string::npos) {
                    // Парсим внутренний массив
                    std::string arr = line.substr(pos + 1, end - pos - 1);
                    // Здесь нужен дополнительный парсинг merkle_branch
                    pos = end + 1;
                } else {
                    break;
                }
            } else if (line[pos] == 't' || line[pos] == 'f') {
                // boolean
                if (line.substr(pos, 4) == "true") {
                    params.emplace_back("true");
                    pos += 4;
                } else if (line.substr(pos, 5) == "false") {
                    params.emplace_back("false");
                    pos += 5;
                }
            } else {
                pos++;
            }
        }
        
        // Заполнение структуры job
        if (params.size() >= 8) {
            job.job_id = params[0];
            job.prevhash = params[1];
            job.coinbase1 = params[2];
            job.coinbase2 = params[3];
            // params[4] - merkle_branch (массив)
            job.version = params[5];
            job.nbits = params[6];
            job.ntime = params[7];
            if (params.size() > 8) {
                job.clean_jobs = (params[8] == "true");
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            current_job = job;
        }
        
        if (job_callback) {
            job_callback(job);
        }
    }
    
    void process_set_difficulty(const std::string& line) {
        // Парсинг mining.set_difficulty
        auto params_start = line.find("\"params\":");
        if (params_start == std::string::npos) return;
        
        auto num_start = line.find('[', params_start);
        if (num_start == std::string::npos) return;
        
        auto num_end = line.find(']', num_start);
        if (num_end == std::string::npos) return;
        
        std::string num_str = line.substr(num_start + 1, num_end - num_start - 1);
        
        try {
            double diff = std::stod(num_str);
            difficulty.store(diff);
            
            if (difficulty_callback) {
                difficulty_callback(diff);
            }
        } catch (...) {
            // Игнорируем ошибки парсинга
        }
    }
    
    void process_response(const std::string& line) {
        // Проверяем, есть ли ожидающий запрос
        std::lock_guard<std::mutex> lock(mutex);
        
        if (pending_requests.empty()) return;
        
        auto req = pending_requests.front();
        pending_requests.pop();
        
        bool success = line.find("\"result\":true") != std::string::npos ||
                       line.find("\"result\":[") != std::string::npos;
        
        if (req.method == "mining.subscribe" && success) {
            // Извлекаем extranonce1 и extranonce2_size
            SubscribeResult result;
            
            // Упрощённый парсинг
            auto result_start = line.find("\"result\":");
            if (result_start != std::string::npos) {
                // Ищем extranonce1 (второй элемент)
                auto first_arr_end = line.find("]", result_start);
                if (first_arr_end != std::string::npos) {
                    auto quote1 = line.find('"', first_arr_end);
                    auto quote2 = line.find('"', quote1 + 1);
                    if (quote1 != std::string::npos && quote2 != std::string::npos) {
                        result.extranonce1 = line.substr(quote1 + 1, quote2 - quote1 - 1);
                    }
                    
                    // Ищем extranonce2_size (число после extranonce1)
                    auto comma = line.find(',', quote2);
                    if (comma != std::string::npos) {
                        auto num_start = comma + 1;
                        while (num_start < line.size() && !std::isdigit(line[num_start])) {
                            num_start++;
                        }
                        auto num_end = num_start;
                        while (num_end < line.size() && std::isdigit(line[num_end])) {
                            num_end++;
                        }
                        if (num_end > num_start) {
                            result.extranonce2_size = static_cast<uint32_t>(
                                std::stoul(line.substr(num_start, num_end - num_start))
                            );
                        }
                    }
                }
                
                subscribe_result = result;
                state = StratumState::Authorizing;
            }
        } else if (req.method == "mining.authorize") {
            if (success) {
                state = StratumState::Connected;
            } else {
                state = StratumState::Error;
            }
        }
    }
    
    void read_loop() {
        char buffer[4096];
        
        while (running && socket_fd >= 0) {
            ssize_t n = ::recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
            
            if (n <= 0) {
                if (running) {
                    state = StratumState::Disconnected;
                    if (disconnect_callback) {
                        disconnect_callback("Connection lost");
                    }
                }
                break;
            }
            
            buffer[n] = '\0';
            read_buffer += buffer;
            
            // Обрабатываем все полные строки
            size_t pos;
            while ((pos = read_buffer.find('\n')) != std::string::npos) {
                std::string line = read_buffer.substr(0, pos);
                read_buffer.erase(0, pos + 1);
                
                if (!line.empty()) {
                    process_line(line);
                }
            }
        }
    }
};

// =============================================================================
// Публичный API
// =============================================================================

StratumClient::StratumClient(const StratumPoolConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

StratumClient::~StratumClient() {
    disconnect();
}

Result<void> StratumClient::connect() {
    if (impl_->state == StratumState::Connected) {
        return {};
    }
    
    impl_->state = StratumState::Connecting;
    
    if (!impl_->create_socket()) {
        impl_->state = StratumState::Error;
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, 
            "Не удалось создать сокет"});
    }
    
    if (!impl_->connect_to_pool()) {
        impl_->close_socket();
        impl_->state = StratumState::Error;
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed, 
            "Не удалось подключиться к пулу " + impl_->config.host});
    }
    
    impl_->running = true;
    impl_->state = StratumState::Subscribing;
    
    // Запуск потока чтения
    impl_->read_thread = std::thread([this]() {
        impl_->read_loop();
    });
    
    // Отправка mining.subscribe
    if (!impl_->send_json(impl_->build_subscribe())) {
        disconnect();
        return std::unexpected(Error{ErrorCode::NetworkSendFailed, 
            "Не удалось отправить subscribe"});
    }
    
    // Ждём ответа на subscribe (с таймаутом)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (impl_->state == StratumState::Subscribing && 
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    if (impl_->state != StratumState::Authorizing) {
        disconnect();
        return std::unexpected(Error{ErrorCode::NetworkTimeout, 
            "Таймаут ожидания subscribe"});
    }
    
    // Отправка mining.authorize
    if (!impl_->send_json(impl_->build_authorize())) {
        disconnect();
        return std::unexpected(Error{ErrorCode::NetworkSendFailed, 
            "Не удалось отправить authorize"});
    }
    
    // Ждём ответа на authorize
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (impl_->state == StratumState::Authorizing && 
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    if (impl_->state != StratumState::Connected) {
        disconnect();
        return std::unexpected(Error{ErrorCode::RpcAuthFailed, 
            "Авторизация на пуле не удалась"});
    }
    
    return {};
}

void StratumClient::disconnect() {
    impl_->running = false;
    impl_->close_socket();
    
    if (impl_->read_thread.joinable()) {
        impl_->read_thread.join();
    }
    
    impl_->state = StratumState::Disconnected;
}

bool StratumClient::is_connected() const noexcept {
    return impl_->state == StratumState::Connected;
}

StratumState StratumClient::get_state() const noexcept {
    return impl_->state;
}

Result<SubmitResult> StratumClient::submit(
    const std::string& job_id,
    const std::string& extranonce2,
    const std::string& ntime,
    const std::string& nonce
) {
    if (!is_connected()) {
        return std::unexpected(Error{ErrorCode::NetworkConnectionFailed,
            "Не подключён к пулу"});
    }
    
    std::string json = impl_->build_submit(job_id, extranonce2, ntime, nonce);
    
    if (!impl_->send_json(json)) {
        return std::unexpected(Error{ErrorCode::NetworkSendFailed,
            "Не удалось отправить submit"});
    }
    
    // Возвращаем успех - реальный результат придёт асинхронно
    SubmitResult result;
    result.accepted = true;
    return result;
}

void StratumClient::set_job_callback(JobCallback callback) {
    impl_->job_callback = std::move(callback);
}

void StratumClient::set_difficulty_callback(DifficultyCallback callback) {
    impl_->difficulty_callback = std::move(callback);
}

void StratumClient::set_disconnect_callback(DisconnectCallback callback) {
    impl_->disconnect_callback = std::move(callback);
}

std::optional<SubscribeResult> StratumClient::get_subscribe_result() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->subscribe_result;
}

std::optional<StratumJob> StratumClient::get_current_job() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->current_job;
}

double StratumClient::get_difficulty() const noexcept {
    return impl_->difficulty;
}

std::string StratumClient::get_extranonce1() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->subscribe_result) {
        return impl_->subscribe_result->extranonce1;
    }
    return "";
}

uint32_t StratumClient::get_extranonce2_size() const noexcept {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->subscribe_result) {
        return impl_->subscribe_result->extranonce2_size;
    }
    return 4;
}

} // namespace quaxis::fallback
