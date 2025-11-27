/**
 * @file base_chain.cpp
 * @brief Реализация базового класса auxiliary chain
 */

#include "base_chain.hpp"

#include <algorithm>

namespace quaxis::merged {

// =============================================================================
// BaseChain Implementation
// =============================================================================

BaseChain::BaseChain(const ChainConfig& config)
    : config_(config)
    , enabled_(config.enabled) {
    
    // Примечание: name_cache_, ticker_cache_ и chain_id_cache_
    // инициализируются в конструкторах наследников через init()
    
    // Инициализируем информацию со значениями по умолчанию
    info_.status = ChainStatus::Disconnected;
}

void BaseChain::init_chain_info() {
    // Вызывается из конструкторов наследников после полной инициализации
    name_cache_ = get_chain_name();
    ticker_cache_ = get_chain_ticker();
    chain_id_cache_ = get_chain_id();
    
    info_.name = name_cache_;
    info_.ticker = ticker_cache_;
}

BaseChain::~BaseChain() {
    disconnect();
}

// =============================================================================
// Информация о chain
// =============================================================================

std::string_view BaseChain::name() const noexcept {
    return name_cache_;
}

std::string_view BaseChain::ticker() const noexcept {
    return ticker_cache_;
}

const Hash256& BaseChain::chain_id() const noexcept {
    return chain_id_cache_;
}

uint32_t BaseChain::priority() const noexcept {
    return config_.priority;
}

ChainInfo BaseChain::get_info() const noexcept {
    std::lock_guard<std::mutex> lock(info_mutex_);
    info_.status = status_.load();
    return info_;
}

// =============================================================================
// Статус и подключение
// =============================================================================

ChainStatus BaseChain::status() const noexcept {
    return status_.load();
}

Result<void> BaseChain::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ == ChainStatus::Ready || status_ == ChainStatus::Connecting) {
        return {};
    }
    
    status_ = ChainStatus::Connecting;
    
    try {
        rpc_client_ = std::make_unique<AuxRpcClient>(
            config_.rpc_url,
            config_.rpc_user,
            config_.rpc_password,
            config_.rpc_timeout
        );
        
        // Проверяем соединение
        auto result = rpc_client_->ping();
        if (!result) {
            status_ = ChainStatus::Error;
            return std::unexpected(result.error());
        }
        
        status_ = ChainStatus::Ready;
        
        // Обновляем информацию
        {
            std::lock_guard<std::mutex> info_lock(info_mutex_);
            info_.status = ChainStatus::Ready;
            info_.last_update = std::chrono::steady_clock::now();
        }
        
        return {};
        
    } catch (const std::exception& e) {
        status_ = ChainStatus::Error;
        return std::unexpected(Error{ErrorCode::RpcConnectionFailed, e.what()});
    }
}

void BaseChain::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    rpc_client_.reset();
    status_ = ChainStatus::Disconnected;
    
    std::lock_guard<std::mutex> info_lock(info_mutex_);
    info_.status = ChainStatus::Disconnected;
}

bool BaseChain::is_connected() const noexcept {
    return status_ == ChainStatus::Ready;
}

// =============================================================================
// Майнинг
// =============================================================================

Result<AuxBlockTemplate> BaseChain::get_block_template() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!rpc_client_) {
        return std::unexpected(Error{ErrorCode::RpcConnectionFailed,
            "Нет подключения к " + name_cache_});
    }
    
    // Вызываем createauxblock RPC
    auto method = get_create_aux_block_method();
    auto result = rpc_client_->call(method, "[]");
    
    if (!result) {
        return std::unexpected(result.error());
    }
    
    // Парсим ответ
    return parse_aux_block_response(*result);
}

Result<void> BaseChain::submit_block(
    const AuxPow& auxpow,
    const AuxBlockTemplate& block_template
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!rpc_client_) {
        return std::unexpected(Error{ErrorCode::RpcConnectionFailed,
            "Нет подключения к " + name_cache_});
    }
    
    // Сериализуем AuxPoW
    auto auxpow_data = auxpow.serialize();
    
    // Преобразуем в hex
    std::string auxpow_hex;
    auxpow_hex.reserve(auxpow_data.size() * 2);
    static constexpr char hex_chars[] = "0123456789abcdef";
    for (uint8_t byte : auxpow_data) {
        auxpow_hex.push_back(hex_chars[byte >> 4]);
        auxpow_hex.push_back(hex_chars[byte & 0x0F]);
    }
    
    // Преобразуем block_hash в hex
    std::string hash_hex;
    hash_hex.reserve(64);
    for (int i = 31; i >= 0; --i) {
        hash_hex.push_back(hex_chars[block_template.block_hash[static_cast<std::size_t>(i)] >> 4]);
        hash_hex.push_back(hex_chars[block_template.block_hash[static_cast<std::size_t>(i)] & 0x0F]);
    }
    
    // Вызываем submitauxblock RPC
    auto method = get_submit_aux_block_method();
    std::string params = "[\"" + hash_hex + "\", \"" + auxpow_hex + "\"]";
    
    auto result = rpc_client_->call(method, params);
    
    if (!result) {
        return std::unexpected(result.error());
    }
    
    // Обновляем статистику
    {
        std::lock_guard<std::mutex> info_lock(info_mutex_);
        info_.last_update = std::chrono::steady_clock::now();
    }
    
    return {};
}

bool BaseChain::meets_target(
    const Hash256& pow_hash,
    const AuxBlockTemplate& current_template
) const noexcept {
    return quaxis::merged::meets_target(pow_hash, current_template.target_bits);
}

// =============================================================================
// Конфигурация
// =============================================================================

void BaseChain::set_enabled(bool enabled) {
    enabled_ = enabled;
}

bool BaseChain::is_enabled() const noexcept {
    return enabled_;
}

void BaseChain::set_priority(uint32_t priority) {
    config_.priority = priority;
}

// =============================================================================
// Protected методы
// =============================================================================

std::string BaseChain::get_create_aux_block_method() const {
    return "createauxblock";
}

std::string BaseChain::get_submit_aux_block_method() const {
    return "submitauxblock";
}

Result<AuxBlockTemplate> BaseChain::parse_aux_block_response(
    const std::string& response
) const {
    // Базовый парсинг JSON ответа
    // Формат: {"result": {"hash": "...", "chainid": "...", "target": "..."}, ...}
    
    AuxBlockTemplate tmpl;
    tmpl.created_at = std::chrono::steady_clock::now();
    
    // Ищем hash
    auto hash_pos = response.find("\"hash\"");
    if (hash_pos != std::string::npos) {
        auto start = response.find('\"', hash_pos + 6) + 1;
        auto end = response.find('\"', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string hash_hex = response.substr(start, end - start);
            
            // Парсим hex в Hash256 (reverse order for Bitcoin)
            if (hash_hex.size() == 64) {
                for (std::size_t i = 0; i < 32; ++i) {
                    std::size_t hex_idx = (31 - i) * 2;
                    uint8_t high = (hash_hex[hex_idx] >= 'a') 
                        ? static_cast<uint8_t>(hash_hex[hex_idx] - 'a' + 10)
                        : static_cast<uint8_t>(hash_hex[hex_idx] - '0');
                    uint8_t low = (hash_hex[hex_idx + 1] >= 'a')
                        ? static_cast<uint8_t>(hash_hex[hex_idx + 1] - 'a' + 10)
                        : static_cast<uint8_t>(hash_hex[hex_idx + 1] - '0');
                    tmpl.block_hash[i] = static_cast<uint8_t>((high << 4) | low);
                }
            }
        }
    }
    
    // Ищем chainid
    auto chainid_pos = response.find("\"chainid\"");
    if (chainid_pos != std::string::npos) {
        auto start = response.find('\"', chainid_pos + 9) + 1;
        auto end = response.find('\"', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string chainid_hex = response.substr(start, end - start);
            
            // Парсим hex в chain_id
            if (chainid_hex.size() == 64) {
                for (std::size_t i = 0; i < 32; ++i) {
                    std::size_t hex_idx = (31 - i) * 2;
                    uint8_t high = (chainid_hex[hex_idx] >= 'a')
                        ? static_cast<uint8_t>(chainid_hex[hex_idx] - 'a' + 10)
                        : static_cast<uint8_t>(chainid_hex[hex_idx] - '0');
                    uint8_t low = (chainid_hex[hex_idx + 1] >= 'a')
                        ? static_cast<uint8_t>(chainid_hex[hex_idx + 1] - 'a' + 10)
                        : static_cast<uint8_t>(chainid_hex[hex_idx + 1] - '0');
                    tmpl.chain_id[i] = static_cast<uint8_t>((high << 4) | low);
                }
            }
        }
    }
    
    // Ищем target (bits)
    auto target_pos = response.find("\"bits\"");
    if (target_pos != std::string::npos) {
        auto start = response.find('\"', target_pos + 6) + 1;
        auto end = response.find('\"', start);
        if (start != std::string::npos && end != std::string::npos) {
            std::string bits_hex = response.substr(start, end - start);
            tmpl.target_bits = static_cast<uint32_t>(std::stoul(bits_hex, nullptr, 16));
        }
    }
    
    // Ищем height
    auto height_pos = response.find("\"height\"");
    if (height_pos != std::string::npos) {
        auto start = response.find(':', height_pos) + 1;
        // Пропускаем пробелы
        while (start < response.size() && 
               (response[start] == ' ' || response[start] == '\t')) {
            ++start;
        }
        auto end = response.find_first_of(",}", start);
        if (end != std::string::npos) {
            std::string height_str = response.substr(start, end - start);
            tmpl.height = static_cast<uint32_t>(std::stoul(height_str));
        }
    }
    
    return tmpl;
}

} // namespace quaxis::merged
