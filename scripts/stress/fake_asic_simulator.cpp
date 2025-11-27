/**
 * @file fake_asic_simulator.cpp
 * @brief Stress test tool: simulates multiple ASIC connections
 * 
 * Compile:
 *   g++ -std=c++23 -O2 -pthread fake_asic_simulator.cpp -o fake_asic_simulator
 * 
 * Usage:
 *   ./fake_asic_simulator [options]
 * 
 * Options:
 *   -h, --help           Show help
 *   -n, --num-clients    Number of simulated ASIC clients (default: 100)
 *   -H, --host           Server host (default: 127.0.0.1)
 *   -p, --port           Server port (default: 3333)
 *   -m, --metrics-port   HTTP metrics port to check (default: 9090)
 *   -d, --duration       Test duration in seconds (default: 60)
 *   -r, --ramp-up        Ramp-up time in seconds (default: 5)
 * 
 * This tool:
 * 1. Gradually connects N clients to the mining server
 * 2. Simulates job reception and nonce submission
 * 3. Periodically checks /metrics endpoint responsiveness
 * 4. Reports latency statistics
 */

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

// =============================================================================
// Configuration
// =============================================================================

struct SimulatorConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 3333;
    uint16_t metrics_port = 9090;
    int num_clients = 100;
    int duration_seconds = 60;
    int ramp_up_seconds = 5;
};

// =============================================================================
// Statistics
// =============================================================================

struct Statistics {
    std::atomic<uint64_t> connections_attempted{0};
    std::atomic<uint64_t> connections_successful{0};
    std::atomic<uint64_t> connections_failed{0};
    std::atomic<uint64_t> jobs_received{0};
    std::atomic<uint64_t> nonces_submitted{0};
    std::atomic<uint64_t> metrics_requests{0};
    std::atomic<uint64_t> metrics_latency_sum_us{0};
    std::atomic<uint64_t> metrics_latency_max_us{0};
    std::atomic<uint64_t> active_connections{0};
    
    void print() const {
        std::cout << "\n=== Stress Test Statistics ===\n";
        std::cout << "Connections attempted: " << connections_attempted.load() << "\n";
        std::cout << "Connections successful: " << connections_successful.load() << "\n";
        std::cout << "Connections failed: " << connections_failed.load() << "\n";
        std::cout << "Active connections: " << active_connections.load() << "\n";
        std::cout << "Jobs received: " << jobs_received.load() << "\n";
        std::cout << "Nonces submitted: " << nonces_submitted.load() << "\n";
        std::cout << "Metrics requests: " << metrics_requests.load() << "\n";
        
        uint64_t reqs = metrics_requests.load();
        if (reqs > 0) {
            double avg_latency = static_cast<double>(metrics_latency_sum_us.load()) / static_cast<double>(reqs);
            std::cout << "Metrics avg latency: " << avg_latency / 1000.0 << " ms\n";
            std::cout << "Metrics max latency: " << static_cast<double>(metrics_latency_max_us.load()) / 1000.0 << " ms\n";
        }
        std::cout << "==============================\n";
    }
};

// Global stats
Statistics g_stats;
std::atomic<bool> g_running{true};

// =============================================================================
// Fake ASIC Client
// =============================================================================

class FakeAsicClient {
public:
    FakeAsicClient(int id, const SimulatorConfig& config)
        : id_(id)
        , config_(config)
        , socket_fd_(-1)
    {}
    
    ~FakeAsicClient() {
        disconnect();
    }
    
    void run() {
        while (g_running.load()) {
            if (!connected_) {
                if (!connect()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }
            }
            
            // Simulate receiving job (48 bytes)
            std::array<uint8_t, 48> job{};
            ssize_t received = recv(socket_fd_, job.data(), job.size(), MSG_DONTWAIT);
            
            if (received > 0) {
                g_stats.jobs_received.fetch_add(1, std::memory_order_relaxed);
                
                // Simulate processing and submit fake nonce (8 bytes)
                std::this_thread::sleep_for(std::chrono::milliseconds(rng_() % 100));
                
                std::array<uint8_t, 8> response{};
                // Copy job_id from job
                std::memcpy(response.data(), job.data() + 44, 4);
                // Random nonce
                uint32_t fake_nonce = rng_();
                std::memcpy(response.data() + 4, &fake_nonce, 4);
                
                ssize_t sent = send(socket_fd_, response.data(), response.size(), MSG_NOSIGNAL);
                if (sent > 0) {
                    g_stats.nonces_submitted.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (received == 0) {
                // Connection closed
                disconnect();
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                disconnect();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        disconnect();
    }

private:
    bool connect() {
        g_stats.connections_attempted.fetch_add(1, std::memory_order_relaxed);
        
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            g_stats.connections_failed.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Set TCP_NODELAY
        int flag = 1;
        setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);
        inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
        
        if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            g_stats.connections_failed.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        connected_ = true;
        g_stats.connections_successful.fetch_add(1, std::memory_order_relaxed);
        g_stats.active_connections.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    
    void disconnect() {
        if (connected_) {
            g_stats.active_connections.fetch_sub(1, std::memory_order_relaxed);
            connected_ = false;
        }
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
    int id_;
    SimulatorConfig config_;
    int socket_fd_;
    bool connected_ = false;
    std::mt19937 rng_{std::random_device{}()};
};

// =============================================================================
// Metrics Checker
// =============================================================================

class MetricsChecker {
public:
    explicit MetricsChecker(const SimulatorConfig& config)
        : config_(config)
    {}
    
    void run() {
        while (g_running.load()) {
            check_metrics();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    void check_metrics() {
        auto start = std::chrono::steady_clock::now();
        
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return;
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.metrics_port);
        inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
        
        // Set timeout
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        
        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            close(sock);
            return;
        }
        
        // Send HTTP request
        const char* request = "GET /metrics HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        send(sock, request, strlen(request), 0);
        
        // Read response
        char buffer[4096];
        ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        close(sock);
        
        auto end = std::chrono::steady_clock::now();
        
        if (received > 0) {
            uint64_t latency_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
            );
            
            g_stats.metrics_requests.fetch_add(1, std::memory_order_relaxed);
            g_stats.metrics_latency_sum_us.fetch_add(latency_us, std::memory_order_relaxed);
            
            // Update max
            uint64_t current_max = g_stats.metrics_latency_max_us.load();
            while (latency_us > current_max) {
                if (g_stats.metrics_latency_max_us.compare_exchange_weak(current_max, latency_us)) {
                    break;
                }
            }
        }
    }
    
    SimulatorConfig config_;
};

// =============================================================================
// Progress Reporter
// =============================================================================

void report_progress() {
    int seconds = 0;
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        seconds += 5;
        
        std::cout << "[" << seconds << "s] Active: " << g_stats.active_connections.load()
                  << ", Jobs: " << g_stats.jobs_received.load()
                  << ", Metrics: " << g_stats.metrics_requests.load() << "\n";
    }
}

// =============================================================================
// Parse Arguments
// =============================================================================

void print_help() {
    std::cout << R"(
Fake ASIC Simulator - Stress test for Quaxis Solo Miner

Usage: fake_asic_simulator [options]

Options:
  -h, --help           Show this help
  -n, --num-clients    Number of simulated clients (default: 100)
  -H, --host           Server host (default: 127.0.0.1)
  -p, --port           Mining server port (default: 3333)
  -m, --metrics-port   HTTP metrics port (default: 9090)
  -d, --duration       Test duration in seconds (default: 60)
  -r, --ramp-up        Ramp-up time in seconds (default: 5)

Example:
  ./fake_asic_simulator -n 500 -d 120 --host 192.168.1.100

)";
}

SimulatorConfig parse_args(int argc, char** argv) {
    SimulatorConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_help();
            exit(0);
        } else if ((arg == "-n" || arg == "--num-clients") && i + 1 < argc) {
            config.num_clients = std::stoi(argv[++i]);
        } else if ((arg == "-H" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-m" || arg == "--metrics-port") && i + 1 < argc) {
            config.metrics_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-d" || arg == "--duration") && i + 1 < argc) {
            config.duration_seconds = std::stoi(argv[++i]);
        } else if ((arg == "-r" || arg == "--ramp-up") && i + 1 < argc) {
            config.ramp_up_seconds = std::stoi(argv[++i]);
        }
    }
    
    return config;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char** argv) {
    SimulatorConfig config = parse_args(argc, argv);
    
    std::cout << "=== Fake ASIC Simulator ===\n";
    std::cout << "Target: " << config.host << ":" << config.port << "\n";
    std::cout << "Metrics: " << config.host << ":" << config.metrics_port << "\n";
    std::cout << "Clients: " << config.num_clients << "\n";
    std::cout << "Duration: " << config.duration_seconds << "s\n";
    std::cout << "Ramp-up: " << config.ramp_up_seconds << "s\n";
    std::cout << "===========================\n\n";
    
    // Create client threads
    std::vector<std::unique_ptr<FakeAsicClient>> clients;
    std::vector<std::thread> client_threads;
    
    // Metrics checker
    MetricsChecker metrics_checker(config);
    std::thread metrics_thread([&metrics_checker]() {
        metrics_checker.run();
    });
    
    // Progress reporter
    std::thread progress_thread(report_progress);
    
    // Ramp-up: gradually start clients
    int delay_between_clients_ms = (config.ramp_up_seconds * 1000) / config.num_clients;
    if (delay_between_clients_ms < 1) delay_between_clients_ms = 1;
    
    std::cout << "Starting " << config.num_clients << " clients...\n";
    
    for (int i = 0; i < config.num_clients; ++i) {
        auto client = std::make_unique<FakeAsicClient>(i, config);
        auto* client_ptr = client.get();
        clients.push_back(std::move(client));
        
        client_threads.emplace_back([client_ptr]() {
            client_ptr->run();
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_between_clients_ms));
    }
    
    std::cout << "All clients started. Running for " << config.duration_seconds << " seconds...\n";
    
    // Wait for test duration
    std::this_thread::sleep_for(std::chrono::seconds(config.duration_seconds));
    
    // Stop
    std::cout << "Stopping...\n";
    g_running.store(false);
    
    // Wait for threads
    for (auto& t : client_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    if (metrics_thread.joinable()) {
        metrics_thread.join();
    }
    
    if (progress_thread.joinable()) {
        progress_thread.join();
    }
    
    // Print final stats
    g_stats.print();
    
    return 0;
}
