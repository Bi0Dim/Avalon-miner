/**
 * @file pool_config.cpp
 * @brief Реализация конфигурации пулов для fallback
 */

#include "pool_config.hpp"

#include <algorithm>
#include <regex>

namespace quaxis::fallback {

bool StratumPoolConfig::parse_url(const std::string& pool_url) {
    // Формат: stratum+tcp://host:port или tcp://host:port
    // Регулярное выражение для парсинга URL
    static const std::regex url_regex(
        R"((?:stratum\+)?(?:tcp|ssl)://([^:]+):(\d+))",
        std::regex::icase
    );
    
    std::smatch match;
    if (std::regex_match(pool_url, match, url_regex)) {
        host = match[1].str();
        port = static_cast<uint16_t>(std::stoi(match[2].str()));
        url = pool_url;
        return true;
    }
    
    return false;
}

const StratumPoolConfig* FallbackConfig::get_active_pool() const {
    const StratumPoolConfig* best = nullptr;
    
    for (const auto& pool : stratum_pools) {
        if (!pool.enabled) {
            continue;
        }
        
        if (!best || pool.priority < best->priority) {
            best = &pool;
        }
    }
    
    return best;
}

} // namespace quaxis::fallback
