/**
 * @file main.cpp
 * @brief Точка входа Quaxis Solo Miner
 * 
 * Universal AuxPoW Core - автономный соло-майнер Bitcoin
 * для ASIC Avalon 1126 Pro.
 * 
 * Основные компоненты:
 * 1. Headers Sync - синхронизация заголовков через P2P/FIBRE
 * 2. Template Generator - внутренняя генерация шаблонов блоков
 * 3. Job Manager - генерация заданий для ASIC
 * 4. TCP Server - связь с ASIC устройствами
 * 5. Share Validator - проверка найденных nonce
 * 6. Status Reporter - терминальный вывод статуса
 * 
 * Использование:
 *   quaxis-miner [options]
 * 
 * Опции:
 *   -c, --config PATH    Путь к файлу конфигурации
 *   -h, --help           Показать справку
 *   -v, --version        Показать версию
 */

#include "core/types.hpp"
#include "core/config.hpp"
#include "core/constants.hpp"
#include "crypto/sha256.hpp"
#include "bitcoin/shm_subscriber.hpp"
#include "bitcoin/coinbase.hpp"
#include "bitcoin/target.hpp"
#include "mining/job_manager.hpp"
#include "mining/share_validator.hpp"
#include "network/server.hpp"
#include "log/status_reporter.hpp"

#include <iostream>
#include <format>
#include <csignal>
#include <atomic>
#include <thread>

namespace {

/// @brief Версия программы
constexpr std::string_view VERSION = "1.0.0";

/// @brief Флаг для graceful shutdown
std::atomic<bool> g_running{true};

/**
 * @brief Обработчик сигналов
 */
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\n[INFO] Получен сигнал завершения, останавливаем..." << std::endl;
        g_running.store(false, std::memory_order_relaxed);
    }
}

/**
 * @brief Вывести справку
 */
void print_help() {
    std::cout << R"(
Quaxis Solo Miner v)" << VERSION << R"(
Universal AuxPoW Core - автономный соло-майнер Bitcoin для ASIC Avalon 1126 Pro

ИСПОЛЬЗОВАНИЕ:
    quaxis-miner [ОПЦИИ]

ОПЦИИ:
    -c, --config PATH    Путь к файлу конфигурации (quaxis.toml)
    -h, --help           Показать эту справку
    -v, --version        Показать версию программы
    --test-config        Проверить конфигурацию и выйти

ПРИМЕРЫ:
    quaxis-miner -c /etc/quaxis/quaxis.toml
    quaxis-miner --test-config

ДОКУМЕНТАЦИЯ:
    https://github.com/quaxis/solo-miner

)";
}

/**
 * @brief Вывести версию
 */
void print_version() {
    std::cout << "Quaxis Solo Miner v" << VERSION << std::endl;
    std::cout << "Universal AuxPoW Core" << std::endl;
    std::cout << "SHA256: " << quaxis::crypto::get_implementation_name() << std::endl;
}

/**
 * @brief Вывести баннер при запуске
 */
void print_banner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════════╗
║                                                                   ║
║   ██████╗ ██╗   ██╗ █████╗ ██╗  ██╗██╗███████╗                   ║
║  ██╔═══██╗██║   ██║██╔══██╗╚██╗██╔╝██║██╔════╝                   ║
║  ██║   ██║██║   ██║███████║ ╚███╔╝ ██║███████╗                   ║
║  ██║▄▄ ██║██║   ██║██╔══██║ ██╔██╗ ██║╚════██║                   ║
║  ╚██████╔╝╚██████╔╝██║  ██║██╔╝ ██╗██║███████║                   ║
║   ╚══▀▀═╝  ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝╚══════╝                   ║
║                                                                   ║
║          Universal AuxPoW Core for Avalon 1126 Pro                ║
║                        v)" << VERSION << R"(                                  ║
║                                                                   ║
╚═══════════════════════════════════════════════════════════════════╝
)";
}

/**
 * @brief Парсинг аргументов командной строки
 */
struct Args {
    std::optional<std::string> config_path;
    bool show_help = false;
    bool show_version = false;
    bool test_config = false;
};

Args parse_args(int argc, char* argv[]) {
    Args args;
    
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.show_help = true;
        } else if (arg == "-v" || arg == "--version") {
            args.show_version = true;
        } else if (arg == "--test-config") {
            args.test_config = true;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            args.config_path = argv[++i];
        }
    }
    
    return args;
}

} // anonymous namespace

/**
 * @brief Главная функция
 */
int main(int argc, char* argv[]) {
    using namespace quaxis;
    
    // Парсим аргументы
    auto args = parse_args(argc, argv);
    
    if (args.show_help) {
        print_help();
        return 0;
    }
    
    if (args.show_version) {
        print_version();
        return 0;
    }
    
    // Выводим баннер
    print_banner();
    
    std::cout << "[INFO] SHA256 реализация: " << crypto::get_implementation_name() << std::endl;
    std::cout << "[INFO] Universal AuxPoW Core - автономный режим" << std::endl;
    
    // Загружаем конфигурацию
    std::cout << "[INFO] Загрузка конфигурации..." << std::endl;
    
    auto config_result = args.config_path 
        ? Config::load(*args.config_path)
        : Config::load_with_search();
    
    if (!config_result) {
        std::cerr << "[ERROR] " << config_result.error().message << std::endl;
        return 1;
    }
    
    Config config = *config_result;
    
    // Валидируем конфигурацию
    auto validation = config.validate();
    if (!validation) {
        std::cerr << "[ERROR] Ошибка валидации конфигурации: " 
                  << validation.error().message << std::endl;
        return 1;
    }
    
    std::cout << "[INFO] Конфигурация загружена успешно" << std::endl;
    std::cout << "[INFO] Источник заголовков: " << config.parent_chain.headers_source << std::endl;
    
    if (args.test_config) {
        std::cout << "[INFO] Конфигурация валидна" << std::endl;
        return 0;
    }
    
    // Создаём coinbase builder
    auto coinbase_builder_result = bitcoin::CoinbaseBuilder::from_address(
        config.parent_chain.payout_address,
        config.mining.coinbase_tag
    );
    
    if (!coinbase_builder_result) {
        std::cerr << "[ERROR] Неверный адрес выплаты: "
                  << coinbase_builder_result.error().message << std::endl;
        return 1;
    }
    
    auto coinbase_builder = std::move(*coinbase_builder_result);
    
    std::cout << "[INFO] Адрес выплаты: " << config.parent_chain.payout_address << std::endl;
    std::cout << "[INFO] Тег coinbase: " << config.mining.coinbase_tag << std::endl;
    
    // Создаём менеджер заданий
    mining::JobManager job_manager(config.mining, std::move(coinbase_builder));
    
    // Создаём валидатор shares
    mining::ShareValidator share_validator(job_manager);
    
    // Создаём репортёр статуса
    log::LoggingConfig log_config;
    log_config.refresh_interval_ms = config.logging.refresh_interval_ms;
    log_config.color = config.logging.color;
    log_config.show_hashrate = config.logging.show_hashrate;
    log_config.show_chain_block_counts = config.logging.show_chain_block_counts;
    log_config.event_history = config.logging.event_history;
    log::StatusReporter status_reporter(log_config);
    
    // Создаём TCP сервер
    network::Server server(config.server, job_manager);
    
    server.set_connected_callback([](const std::string& addr) {
        std::cout << "[INFO] ASIC подключён: " << addr << std::endl;
    });
    
    server.set_disconnected_callback([](const std::string& addr) {
        std::cout << "[INFO] ASIC отключён: " << addr << std::endl;
    });
    
    // Устанавливаем обработчики сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Запускаем сервер
    std::cout << "[INFO] Запуск сервера на " 
              << config.server.bind_address << ":" << config.server.port 
              << "..." << std::endl;
    
    auto server_result = server.start();
    if (!server_result) {
        std::cerr << "[ERROR] Не удалось запустить сервер: "
                  << server_result.error().message << std::endl;
        return 1;
    }
    
    std::cout << "[INFO] Сервер запущен" << std::endl;
    
    // Запускаем репортёр статуса
    status_reporter.start();
    
    // Основной цикл - ожидание блоков через SHM или fallback
    std::cout << "[INFO] Ожидание блоков..." << std::endl;
    std::cout << "[INFO] Источник: " << config.parent_chain.headers_source << std::endl;
    
    // Инициализируем SHM подписчик если включён
    std::unique_ptr<bitcoin::ShmSubscriber> shm_subscriber;
    if (config.shm.enabled) {
        shm_subscriber = std::make_unique<bitcoin::ShmSubscriber>(config.shm);
        shm_subscriber->set_callback([&](const bitcoin::BlockHeader& header, 
                                          uint32_t height, 
                                          int64_t coinbase_value,
                                          bool is_speculative) {
            // Создаём BlockTemplate
            bitcoin::BlockTemplate block_template;
            block_template.height = height;
            block_template.header = header;
            block_template.coinbase_value = coinbase_value;
            
            // Обновляем менеджер заданий
            job_manager.on_new_block(block_template, is_speculative);
            
            // Получаем задание и рассылаем ASIC
            if (auto job = job_manager.get_next_job()) {
                server.broadcast_job(*job);
                status_reporter.log_event(log::EventType::NEW_BLOCK, 
                    "Job sent at height " + std::to_string(height));
            }
            
            // Обновляем статистику
            log::BitcoinStats btc_stats;
            btc_stats.height = height;
            btc_stats.connected = true;
            status_reporter.update_bitcoin_stats(btc_stats);
        });
        
        auto shm_result = shm_subscriber->start();
        if (!shm_result) {
            std::cerr << "[WARNING] SHM недоступен: " << shm_result.error().message << std::endl;
            std::cout << "[INFO] Используем fallback режим (Stratum pool)" << std::endl;
        } else {
            std::cout << "[INFO] SHM подключён: " << config.shm.path << std::endl;
        }
    }
    
    while (g_running.load(std::memory_order_relaxed)) {
        // Обновляем статистику ASIC
        log::AsicStats asic_stats;
        asic_stats.connected_count = static_cast<uint32_t>(server.connection_count());
        status_reporter.update_asic_stats(asic_stats);
        
        // Пауза перед следующей итерацией
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Graceful shutdown
    std::cout << "[INFO] Остановка сервера..." << std::endl;
    
    if (shm_subscriber) {
        shm_subscriber->stop();
    }
    status_reporter.stop();
    server.stop();
    
    std::cout << "[INFO] Quaxis Solo Miner остановлен" << std::endl;
    
    return 0;
}
