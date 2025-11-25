/**
 * @file config.cpp
 * @brief Реализация загрузки конфигурации
 * 
 * Использует библиотеку toml++ для парсинга TOML файлов.
 */

#include "config.hpp"

#include <toml++/toml.hpp>
#include <format>
#include <fstream>

namespace quaxis {

// =============================================================================
// BitcoinConfig
// =============================================================================

std::string BitcoinConfig::get_rpc_url() const {
    return std::format("http://{}:{}/", rpc_host, rpc_port);
}

// =============================================================================
// Config - Загрузка из файла
// =============================================================================

Result<Config> Config::load(const std::filesystem::path& path) {
    // Проверяем существование файла
    if (!std::filesystem::exists(path)) {
        return Err<Config>(
            ErrorCode::ConfigNotFound,
            std::format("Файл конфигурации не найден: {}", path.string())
        );
    }
    
    try {
        // Парсим TOML файл
        auto table = toml::parse_file(path.string());
        
        Config config;
        
        // === Секция [server] ===
        if (auto server = table["server"].as_table()) {
            if (auto val = (*server)["bind_address"].value<std::string>()) {
                config.server.bind_address = *val;
            }
            if (auto val = (*server)["port"].value<int64_t>()) {
                config.server.port = static_cast<uint16_t>(*val);
            }
            if (auto val = (*server)["max_connections"].value<int64_t>()) {
                config.server.max_connections = static_cast<std::size_t>(*val);
            }
        }
        
        // === Секция [bitcoin] ===
        if (auto bitcoin = table["bitcoin"].as_table()) {
            if (auto val = (*bitcoin)["rpc_host"].value<std::string>()) {
                config.bitcoin.rpc_host = *val;
            }
            if (auto val = (*bitcoin)["rpc_port"].value<int64_t>()) {
                config.bitcoin.rpc_port = static_cast<uint16_t>(*val);
            }
            if (auto val = (*bitcoin)["rpc_user"].value<std::string>()) {
                config.bitcoin.rpc_user = *val;
            }
            if (auto val = (*bitcoin)["rpc_password"].value<std::string>()) {
                config.bitcoin.rpc_password = *val;
            }
            if (auto val = (*bitcoin)["payout_address"].value<std::string>()) {
                config.bitcoin.payout_address = *val;
            }
        }
        
        // === Секция [mining] ===
        if (auto mining = table["mining"].as_table()) {
            if (auto val = (*mining)["coinbase_tag"].value<std::string>()) {
                config.mining.coinbase_tag = *val;
            }
            if (auto val = (*mining)["extranonce_size"].value<int64_t>()) {
                config.mining.extranonce_size = static_cast<std::size_t>(*val);
            }
            if (auto val = (*mining)["job_queue_size"].value<int64_t>()) {
                config.mining.job_queue_size = static_cast<std::size_t>(*val);
            }
            if (auto val = (*mining)["use_spy_mining"].value<bool>()) {
                config.mining.use_spy_mining = *val;
            }
            if (auto val = (*mining)["use_mtp_timestamp"].value<bool>()) {
                config.mining.use_mtp_timestamp = *val;
            }
            if (auto val = (*mining)["empty_blocks_only"].value<bool>()) {
                config.mining.empty_blocks_only = *val;
            }
        }
        
        // === Секция [shm] ===
        if (auto shm = table["shm"].as_table()) {
            if (auto val = (*shm)["enabled"].value<bool>()) {
                config.shm.enabled = *val;
            }
            if (auto val = (*shm)["path"].value<std::string>()) {
                config.shm.path = *val;
            }
            if (auto val = (*shm)["spin_wait"].value<bool>()) {
                config.shm.spin_wait = *val;
            }
        }
        
        // === Секция [monitoring] ===
        if (auto monitoring = table["monitoring"].as_table()) {
            if (auto val = (*monitoring)["stats_interval"].value<int64_t>()) {
                config.monitoring.stats_interval = static_cast<uint32_t>(*val);
            }
            if (auto val = (*monitoring)["log_level"].value<std::string>()) {
                config.monitoring.log_level = *val;
            }
            if (auto val = (*monitoring)["log_file"].value<std::string>()) {
                config.monitoring.log_file = *val;
            }
        }
        
        return config;
        
    } catch (const toml::parse_error& e) {
        return Err<Config>(
            ErrorCode::ConfigParseError,
            std::format("Ошибка парсинга TOML: {}", e.what())
        );
    }
}

Result<Config> Config::load_with_search(
    const std::optional<std::filesystem::path>& path
) {
    // Список путей для поиска
    std::vector<std::filesystem::path> search_paths;
    
    if (path.has_value()) {
        search_paths.push_back(path.value());
    }
    
    // Стандартные пути
    search_paths.push_back("quaxis.toml");
    search_paths.push_back("/etc/quaxis/quaxis.toml");
    
    // Домашняя директория пользователя
    if (const char* home = std::getenv("HOME")) {
        search_paths.push_back(
            std::filesystem::path(home) / ".config" / "quaxis" / "quaxis.toml"
        );
    }
    
    // Ищем первый существующий файл
    for (const auto& search_path : search_paths) {
        if (std::filesystem::exists(search_path)) {
            return load(search_path);
        }
    }
    
    return Err<Config>(
        ErrorCode::ConfigNotFound,
        "Файл конфигурации не найден в стандартных путях"
    );
}

// =============================================================================
// Config - Валидация
// =============================================================================

Result<void> Config::validate() const {
    // Проверка обязательных полей
    if (bitcoin.rpc_password.empty()) {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "RPC пароль не указан (bitcoin.rpc_password)"
        );
    }
    
    if (bitcoin.payout_address.empty()) {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Адрес для выплаты не указан (bitcoin.payout_address)"
        );
    }
    
    // Проверка формата адреса (должен начинаться с bc1q для P2WPKH)
    if (!bitcoin.payout_address.starts_with("bc1q") &&
        !bitcoin.payout_address.starts_with("tb1q") &&  // testnet
        !bitcoin.payout_address.starts_with("bcrt1q")) { // regtest
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Адрес должен быть в формате P2WPKH (bc1q...)"
        );
    }
    
    // Проверка размера extranonce
    if (mining.extranonce_size < 1 || mining.extranonce_size > 8) {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Размер extranonce должен быть от 1 до 8 байт"
        );
    }
    
    // Проверка размера тега coinbase
    if (mining.coinbase_tag.size() > 20) {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Тег coinbase слишком длинный (максимум 20 символов)"
        );
    }
    
    // Проверка порта
    if (server.port == 0) {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Порт сервера не может быть 0"
        );
    }
    
    return {};
}

} // namespace quaxis
