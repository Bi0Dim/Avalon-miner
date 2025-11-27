/**
 * @file main.cpp
 * @brief Точка входа Quaxis Solo Miner
 * 
 * Quaxis Solo Miner - высокооптимизированный соло-майнер Bitcoin
 * для ASIC Avalon 1126 Pro.
 * 
 * Основные компоненты:
 * 1. RPC Client - связь с Bitcoin Core
 * 2. SHM Subscriber - получение новых блоков через Shared Memory
 * 3. Job Manager - генерация заданий для ASIC
 * 4. TCP Server - связь с ASIC устройствами
 * 5. Share Validator - проверка найденных nonce
 * 6. Stats Collector - мониторинг и статистика
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
#include "bitcoin/rpc_client.hpp"
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
Высокооптимизированный соло-майнер Bitcoin для ASIC Avalon 1126 Pro

ИСПОЛЬЗОВАНИЕ:
    quaxis-miner [ОПЦИИ]

ОПЦИИ:
    -c, --config PATH    Путь к файлу конфигурации (quaxis.toml)
    -h, --help           Показать эту справку
    -v, --version        Показать версию программы
    --test-config        Проверить конфигурацию и выйти
    --test-rpc           Проверить подключение к Bitcoin Core

ПРИМЕРЫ:
    quaxis-miner -c /etc/quaxis/quaxis.toml
    quaxis-miner --test-rpc

ДОКУМЕНТАЦИЯ:
    https://github.com/quaxis/solo-miner

)";
}

/**
 * @brief Вывести версию
 */
void print_version() {
    std::cout << "Quaxis Solo Miner v" << VERSION << std::endl;
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
║              SOLO MINER для Avalon 1126 Pro                       ║
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
    bool test_rpc = false;
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
        } else if (arg == "--test-rpc") {
            args.test_rpc = true;
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
    
    if (args.test_config) {
        std::cout << "[INFO] Конфигурация валидна" << std::endl;
        return 0;
    }
    
    // Проверяем подключение к Bitcoin Core
    std::cout << "[INFO] Подключение к Bitcoin Core " 
              << config.bitcoin.rpc_host << ":" << config.bitcoin.rpc_port 
              << "..." << std::endl;
    
    bitcoin::RpcClient rpc_client(config.bitcoin);
    
    auto ping_result = rpc_client.ping();
    if (!ping_result) {
        std::cerr << "[ERROR] Не удалось подключиться к Bitcoin Core: "
                  << ping_result.error().message << std::endl;
        return 1;
    }
    
    // Получаем информацию о блокчейне
    auto blockchain_info = rpc_client.get_blockchain_info();
    if (!blockchain_info) {
        std::cerr << "[ERROR] Не удалось получить информацию о блокчейне: "
                  << blockchain_info.error().message << std::endl;
        return 1;
    }
    
    std::cout << "[INFO] Подключено к Bitcoin Core" << std::endl;
    std::cout << "[INFO] Сеть: " << blockchain_info->chain << std::endl;
    std::cout << "[INFO] Высота блока: " << blockchain_info->blocks << std::endl;
    std::cout << "[INFO] Сложность: " << bitcoin::format_difficulty(blockchain_info->difficulty) 
              << std::endl;
    
    if (blockchain_info->initial_block_download) {
        std::cerr << "[WARNING] Bitcoin Core синхронизируется (IBD), "
                  << "майнинг может быть неэффективным" << std::endl;
    }
    
    if (args.test_rpc) {
        std::cout << "[INFO] Подключение к Bitcoin Core успешно" << std::endl;
        return 0;
    }
    
    // Создаём coinbase builder
    auto coinbase_builder_result = bitcoin::CoinbaseBuilder::from_address(
        config.bitcoin.payout_address,
        config.mining.coinbase_tag
    );
    
    if (!coinbase_builder_result) {
        std::cerr << "[ERROR] Неверный адрес выплаты: "
                  << coinbase_builder_result.error().message << std::endl;
        return 1;
    }
    
    auto coinbase_builder = std::move(*coinbase_builder_result);
    
    std::cout << "[INFO] Адрес выплаты: " << config.bitcoin.payout_address << std::endl;
    std::cout << "[INFO] Тег coinbase: " << config.mining.coinbase_tag << std::endl;
    
    // Создаём менеджер заданий
    mining::JobManager job_manager(config.mining, std::move(coinbase_builder));
    
    // Создаём валидатор shares
    mining::ShareValidator share_validator(job_manager);
    
    // Создаём TCP сервер
    network::Server server(config.server, job_manager);
    
    // Создаём репортер статуса
    log::StatusReporterConfig log_config;
    log_config.refresh_interval_ms = config.logging.refresh_interval_ms;
    log_config.event_history = config.logging.event_history;
    log_config.color = config.logging.color;
    log_config.highlight_found_blocks = config.logging.highlight_found_blocks;
    log_config.show_chain_block_counts = config.logging.show_chain_block_counts;
    log_config.show_hashrate = config.logging.show_hashrate;
    log::StatusReporter status_reporter(log_config);
    
    // Время запуска для uptime
    auto start_time = std::chrono::steady_clock::now();
    uint32_t current_height = 0;
    uint64_t jobs_sent = 0;
    uint64_t blocks_found = 0;
    
    // Провайдер данных для status reporter
    status_reporter.set_data_provider([&]() {
        log::StatusData data;
        auto now = std::chrono::steady_clock::now();
        data.uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        data.fallback_active = false;
        data.hashrate_ths = 0.0; // TODO: get from ASIC
        data.asic_connections = static_cast<uint32_t>(server.connection_count());
        data.btc_height = current_height;
        data.tip_age_ms = 0;
        data.job_queue_depth = static_cast<uint32_t>(jobs_sent);
        data.prepared_templates = 1;
        data.adaptive_spin_active = config.shm.spin_wait;
        return data;
    });
    
    server.set_connected_callback([&status_reporter](const std::string& addr) {
        std::cout << "[INFO] ASIC подключён: " << addr << std::endl;
        status_reporter.add_event(log::EventType::SubmitOk, "ASIC connected", addr);
    });
    
    server.set_disconnected_callback([&status_reporter](const std::string& addr) {
        std::cout << "[INFO] ASIC отключён: " << addr << std::endl;
        status_reporter.add_event(log::EventType::Error, "ASIC disconnected", addr);
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
    
    // Запускаем терминальный вывод статуса
    status_reporter.start();
    
    // Основной цикл
    std::cout << "[INFO] Начинаем майнинг..." << std::endl;
    
    while (g_running.load(std::memory_order_relaxed)) {
        // Получаем новый шаблон блока
        auto template_result = rpc_client.get_block_template();
        
        if (template_result) {
            auto& tmpl = *template_result;
            
            // Создаём BlockTemplate
            bitcoin::BlockTemplate block_template;
            block_template.height = tmpl.height;
            block_template.header.version = tmpl.version;
            block_template.header.prev_block = tmpl.prev_blockhash;
            block_template.header.timestamp = tmpl.curtime;
            block_template.header.bits = tmpl.bits;
            block_template.coinbase_value = tmpl.coinbase_value;
            
            // Обновляем менеджер заданий
            job_manager.on_new_block(block_template, config.mining.use_spy_mining);
            
            // Получаем задание и рассылаем ASIC
            if (auto job = job_manager.get_next_job()) {
                server.broadcast_job(*job);
                ++jobs_sent;
            }
            
            // Обновляем текущую высоту
            current_height = tmpl.height;
            
            // Добавляем событие о новом блоке
            status_reporter.add_event(log::EventType::NewBlock, 
                "Height: " + std::to_string(tmpl.height), "");
        }
        
        // Пауза перед следующей итерацией
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Graceful shutdown
    std::cout << "[INFO] Остановка сервера..." << std::endl;
    
    status_reporter.stop();
    server.stop();
    
    std::cout << "[INFO] Quaxis Solo Miner остановлен" << std::endl;
    
    // Выводим финальную статистику
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    std::cout << "\n=== Итоговая статистика ===" << std::endl;
    std::cout << "Время работы: " << uptime.count() << " секунд" << std::endl;
    std::cout << "Отправлено заданий: " << jobs_sent << std::endl;
    std::cout << "Найдено блоков: " << blocks_found << std::endl;
    
    return 0;
}
