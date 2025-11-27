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
        
        // === Секция [parent_chain] ===
        if (auto parent_chain = table["parent_chain"].as_table()) {
            if (auto val = (*parent_chain)["headers_source"].value<std::string>()) {
                config.parent_chain.headers_source = *val;
            }
            if (auto val = (*parent_chain)["mtp_refresh_seconds"].value<int64_t>()) {
                config.parent_chain.mtp_refresh_seconds = static_cast<uint32_t>(*val);
            }
            if (auto val = (*parent_chain)["payout_address"].value<std::string>()) {
                config.parent_chain.payout_address = *val;
            }
            
            // Парсим seed_nodes
            if (auto nodes = (*parent_chain)["seed_nodes"].as_array()) {
                config.parent_chain.seed_nodes.clear();
                for (const auto& node : *nodes) {
                    if (auto val = node.value<std::string>()) {
                        config.parent_chain.seed_nodes.push_back(*val);
                    }
                }
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
            if (auto val = (*shm)["adaptive_spin_enabled"].value<bool>()) {
                config.shm.adaptive_spin_enabled = *val;
            }
            if (auto val = (*shm)["spin_phase1_iterations"].value<int64_t>()) {
                config.shm.spin_phase1_iterations = static_cast<uint32_t>(*val);
            }
            if (auto val = (*shm)["spin_phase2_iterations"].value<int64_t>()) {
                config.shm.spin_phase2_iterations = static_cast<uint32_t>(*val);
            }
            if (auto val = (*shm)["sleep_us"].value<int64_t>()) {
                config.shm.sleep_us = static_cast<uint32_t>(*val);
            }
        }
        
        // === Секция [logging] ===
        if (auto logging = table["logging"].as_table()) {
            if (auto val = (*logging)["refresh_interval_ms"].value<int64_t>()) {
                config.logging.refresh_interval_ms = static_cast<uint32_t>(*val);
            }
            if (auto val = (*logging)["level"].value<std::string>()) {
                config.logging.level = *val;
            }
            if (auto val = (*logging)["event_history"].value<int64_t>()) {
                config.logging.event_history = static_cast<std::size_t>(*val);
            }
            if (auto val = (*logging)["color"].value<bool>()) {
                config.logging.color = *val;
            }
            if (auto val = (*logging)["show_hashrate"].value<bool>()) {
                config.logging.show_hashrate = *val;
            }
            if (auto val = (*logging)["highlight_found_blocks"].value<bool>()) {
                config.logging.highlight_found_blocks = *val;
            }
            if (auto val = (*logging)["show_chain_block_counts"].value<bool>()) {
                config.logging.show_chain_block_counts = *val;
            }
        }
        
        // === Секция [relay] ===
        if (auto relay = table["relay"].as_table()) {
            if (auto val = (*relay)["enabled"].value<bool>()) {
                config.relay.enabled = *val;
            }
            if (auto val = (*relay)["local_port"].value<int64_t>()) {
                config.relay.local_port = static_cast<uint16_t>(*val);
            }
            if (auto val = (*relay)["bandwidth_limit"].value<int64_t>()) {
                config.relay.bandwidth_limit = static_cast<uint32_t>(*val);
            }
            if (auto val = (*relay)["reconstruction_timeout"].value<int64_t>()) {
                config.relay.reconstruction_timeout = static_cast<uint32_t>(*val);
            }
            if (auto val = (*relay)["fec_enabled"].value<bool>()) {
                config.relay.fec_enabled = *val;
            }
            if (auto val = (*relay)["fec_overhead"].value<double>()) {
                config.relay.fec_overhead = *val;
            }
            
            // Парсим пиры из [[relay.peers]]
            if (auto peers = (*relay)["peers"].as_array()) {
                for (const auto& peer_node : *peers) {
                    if (auto peer_table = peer_node.as_table()) {
                        RelayPeerConfig peer_config;
                        
                        if (auto host = (*peer_table)["host"].value<std::string>()) {
                            peer_config.host = *host;
                        }
                        if (auto port = (*peer_table)["port"].value<int64_t>()) {
                            peer_config.port = static_cast<uint16_t>(*port);
                        }
                        if (auto trusted = (*peer_table)["trusted"].value<bool>()) {
                            peer_config.trusted = *trusted;
                        }
                        
                        if (!peer_config.host.empty()) {
                            config.relay.peers.push_back(std::move(peer_config));
                        }
                    }
                }
            }
        }
        
        // === Секция [merged_mining] ===
        if (auto merged = table["merged_mining"].as_table()) {
            if (auto val = (*merged)["enabled"].value<bool>()) {
                config.merged_mining.enabled = *val;
            }
            if (auto val = (*merged)["health_check_interval"].value<int64_t>()) {
                config.merged_mining.health_check_interval = static_cast<uint32_t>(*val);
            }
            
            // Парсим chains из [[merged_mining.chains]]
            if (auto chains = (*merged)["chains"].as_array()) {
                for (const auto& chain_node : *chains) {
                    if (auto chain_table = chain_node.as_table()) {
                        MergedChainConfig chain_config;
                        
                        if (auto name = (*chain_table)["name"].value<std::string>()) {
                            chain_config.name = *name;
                        }
                        if (auto enabled = (*chain_table)["enabled"].value<bool>()) {
                            chain_config.enabled = *enabled;
                        }
                        if (auto rpc_url = (*chain_table)["rpc_url"].value<std::string>()) {
                            chain_config.rpc_url = *rpc_url;
                        }
                        if (auto rpc_user = (*chain_table)["rpc_user"].value<std::string>()) {
                            chain_config.rpc_user = *rpc_user;
                        }
                        if (auto rpc_password = (*chain_table)["rpc_password"].value<std::string>()) {
                            chain_config.rpc_password = *rpc_password;
                        }
                        if (auto payout_address = (*chain_table)["payout_address"].value<std::string>()) {
                            chain_config.payout_address = *payout_address;
                        }
                        if (auto priority = (*chain_table)["priority"].value<int64_t>()) {
                            chain_config.priority = static_cast<uint32_t>(*priority);
                        }
                        if (auto rpc_timeout = (*chain_table)["rpc_timeout"].value<int64_t>()) {
                            chain_config.rpc_timeout = static_cast<uint32_t>(*rpc_timeout);
                        }
                        if (auto update_interval = (*chain_table)["update_interval"].value<int64_t>()) {
                            chain_config.update_interval = static_cast<uint32_t>(*update_interval);
                        }
                        
                        if (!chain_config.name.empty()) {
                            config.merged_mining.chains.push_back(std::move(chain_config));
                        }
                    }
                }
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
    // Проверка адреса выплаты
    if (parent_chain.payout_address.empty()) {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Адрес для выплаты не указан (parent_chain.payout_address)"
        );
    }
    
    // Проверка формата адреса (должен начинаться с bc1q для P2WPKH)
    if (!parent_chain.payout_address.starts_with("bc1q") &&
        !parent_chain.payout_address.starts_with("tb1q") &&  // testnet
        !parent_chain.payout_address.starts_with("bcrt1q")) { // regtest
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "Адрес должен быть в формате P2WPKH (bc1q...)"
        );
    }
    
    // Проверка источника заголовков
    if (parent_chain.headers_source != "p2p" &&
        parent_chain.headers_source != "fibre" &&
        parent_chain.headers_source != "trusted") {
        return Err<void>(
            ErrorCode::ConfigInvalidValue,
            "headers_source должен быть 'p2p', 'fibre' или 'trusted'"
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
    
    // Проверка merged mining chains
    if (merged_mining.enabled) {
        for (const auto& chain : merged_mining.chains) {
            if (chain.enabled && chain.payout_address.empty()) {
                return Err<void>(
                    ErrorCode::ConfigInvalidValue,
                    std::format("Не указан payout_address для chain '{}'. "
                               "Без адреса награды будут потеряны!", chain.name)
                );
            }
            if (chain.enabled && chain.rpc_url.empty()) {
                return Err<void>(
                    ErrorCode::ConfigInvalidValue,
                    std::format("Не указан rpc_url для chain '{}'", chain.name)
                );
            }
        }
    }
    
    return {};
}

} // namespace quaxis
