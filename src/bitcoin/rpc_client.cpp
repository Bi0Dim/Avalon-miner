/**
 * @file rpc_client.cpp
 * @brief Реализация HTTP клиента для Bitcoin Core RPC
 * 
 * Использует libcurl для HTTP POST запросов.
 * JSON-RPC 1.0 протокол.
 */

#include "rpc_client.hpp"
#include "../core/byte_order.hpp"

#include <curl/curl.h>
#include <format>
#include <sstream>

namespace quaxis::bitcoin {

// =============================================================================
// Реализация (PIMPL)
// =============================================================================

struct RpcClient::Impl {
    std::string url;
    std::string auth;  // Base64 encoded "user:password"
    CURL* curl = nullptr;
    
    Impl(const BitcoinConfig& config) {
        url = config.get_rpc_url();
        
        // Base64 кодирование credentials
        std::string credentials = config.rpc_user + ":" + config.rpc_password;
        auth = base64_encode(credentials);
        
        curl = curl_easy_init();
        if (curl) {
            // Установка базовых опций
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        }
    }
    
    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
    
    /**
     * @brief Base64 кодирование
     */
    static std::string base64_encode(const std::string& input) {
        static const char* chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string result;
        result.reserve(((input.size() + 2) / 3) * 4);
        
        std::size_t i = 0;
        while (i < input.size()) {
            uint32_t a = static_cast<uint8_t>(input[i++]);
            uint32_t b = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
            uint32_t c = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
            
            uint32_t triple = (a << 16) | (b << 8) | c;
            
            result.push_back(chars[(triple >> 18) & 0x3F]);
            result.push_back(chars[(triple >> 12) & 0x3F]);
            result.push_back(chars[(triple >> 6) & 0x3F]);
            result.push_back(chars[triple & 0x3F]);
        }
        
        // Padding
        auto mod = input.size() % 3;
        if (mod == 1) {
            result[result.size() - 2] = '=';
            result[result.size() - 1] = '=';
        } else if (mod == 2) {
            result[result.size() - 1] = '=';
        }
        
        return result;
    }
    
    /**
     * @brief Callback для записи ответа
     */
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
        size_t total_size = size * nmemb;
        output->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
    
    /**
     * @brief Выполнить RPC запрос
     */
    Result<std::string> call(std::string_view method, std::string_view params = "[]") {
        if (!curl) {
            return Err<std::string>(ErrorCode::RpcConnectionFailed, "CURL не инициализирован");
        }
        
        // Формируем JSON-RPC запрос
        std::string request = std::format(
            R"({{"jsonrpc":"1.0","id":"quaxis","method":"{}","params":{}}})",
            method, params
        );
        
        // Заголовки
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string auth_header = std::format("Authorization: Basic {}", auth);
        headers = curl_slist_append(headers, auth_header.c_str());
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.size()));
        
        // Буфер для ответа
        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // Выполняем запрос
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            return Err<std::string>(
                ErrorCode::RpcConnectionFailed,
                std::format("CURL ошибка: {}", curl_easy_strerror(res))
            );
        }
        
        // Проверяем HTTP код
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 401) {
            return Err<std::string>(ErrorCode::RpcAuthFailed, "Ошибка авторизации RPC");
        }
        
        if (http_code != 200) {
            return Err<std::string>(
                ErrorCode::RpcInternalError,
                std::format("HTTP ошибка: {}", http_code)
            );
        }
        
        return response;
    }
    
    /**
     * @brief Простой парсер JSON для извлечения значения по ключу
     * 
     * Минималистичный парсер, не использует внешние библиотеки.
     * Подходит для простых случаев RPC ответов.
     */
    static std::string extract_string(const std::string& json, std::string_view key) {
        std::string search = std::format("\"{}\":", key);
        auto pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos += search.size();
        
        // Пропускаем пробелы
        while (pos < json.size() && std::isspace(json[pos])) ++pos;
        
        if (pos >= json.size()) return "";
        
        if (json[pos] == '"') {
            // Строковое значение
            ++pos;
            auto end = json.find('"', pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        } else if (json[pos] == '{' || json[pos] == '[') {
            // Объект или массив - ищем соответствующую закрывающую скобку
            char open = json[pos];
            char close = (open == '{') ? '}' : ']';
            int depth = 1;
            auto start = pos;
            ++pos;
            while (pos < json.size() && depth > 0) {
                if (json[pos] == open) ++depth;
                else if (json[pos] == close) --depth;
                else if (json[pos] == '"') {
                    // Пропускаем строки
                    ++pos;
                    while (pos < json.size() && json[pos] != '"') {
                        if (json[pos] == '\\') ++pos;
                        ++pos;
                    }
                }
                ++pos;
            }
            return json.substr(start, pos - start);
        } else {
            // Число, bool или null
            auto end = json.find_first_of(",}]", pos);
            if (end == std::string::npos) end = json.size();
            auto result = json.substr(pos, end - pos);
            // Убираем trailing whitespace
            while (!result.empty() && std::isspace(result.back())) {
                result.pop_back();
            }
            return result;
        }
    }
    
    static int64_t extract_int(const std::string& json, std::string_view key) {
        auto str = extract_string(json, key);
        if (str.empty()) return 0;
        try {
            return std::stoll(str);
        } catch (...) {
            return 0;
        }
    }
    
    static double extract_double(const std::string& json, std::string_view key) {
        auto str = extract_string(json, key);
        if (str.empty()) return 0.0;
        try {
            return std::stod(str);
        } catch (...) {
            return 0.0;
        }
    }
    
    static bool extract_bool(const std::string& json, std::string_view key) {
        auto str = extract_string(json, key);
        return str == "true";
    }
};

// =============================================================================
// RpcClient
// =============================================================================

RpcClient::RpcClient(const BitcoinConfig& config)
    : impl_(std::make_unique<Impl>(config))
{
}

RpcClient::~RpcClient() = default;

RpcClient::RpcClient(RpcClient&&) noexcept = default;
RpcClient& RpcClient::operator=(RpcClient&&) noexcept = default;

Result<BlockchainInfo> RpcClient::get_blockchain_info() {
    auto response = impl_->call("getblockchaininfo");
    if (!response) {
        return Err<BlockchainInfo>(response.error().code, response.error().message);
    }
    
    // Извлекаем result
    auto result = Impl::extract_string(*response, "result");
    if (result.empty()) {
        auto error = Impl::extract_string(*response, "error");
        return Err<BlockchainInfo>(ErrorCode::RpcInternalError, error);
    }
    
    BlockchainInfo info;
    info.chain = Impl::extract_string(result, "chain");
    info.blocks = static_cast<uint32_t>(Impl::extract_int(result, "blocks"));
    info.headers = static_cast<uint32_t>(Impl::extract_int(result, "headers"));
    info.best_blockhash = Impl::extract_string(result, "bestblockhash");
    info.difficulty = Impl::extract_double(result, "difficulty");
    info.median_time = static_cast<uint64_t>(Impl::extract_int(result, "mediantime"));
    info.initial_block_download = Impl::extract_bool(result, "initialblockdownload");
    
    return info;
}

Result<Hash256> RpcClient::get_best_block_hash() {
    auto response = impl_->call("getbestblockhash");
    if (!response) {
        return Err<Hash256>(response.error().code, response.error().message);
    }
    
    auto result = Impl::extract_string(*response, "result");
    if (result.empty() || result.size() != 64) {
        return Err<Hash256>(ErrorCode::RpcParseError, "Неверный формат хеша блока");
    }
    
    // Конвертируем hex в Hash256
    Hash256 hash{};
    for (std::size_t i = 0; i < 32; ++i) {
        std::string byte_str = result.substr(i * 2, 2);
        // Хеш отображается в reversed формате
        hash[31 - i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
    }
    
    return hash;
}

Result<BlockTemplateData> RpcClient::get_block_template() {
    // Параметры для getblocktemplate
    std::string params = R"([{"rules":["segwit"]}])";
    
    auto response = impl_->call("getblocktemplate", params);
    if (!response) {
        return Err<BlockTemplateData>(response.error().code, response.error().message);
    }
    
    auto result = Impl::extract_string(*response, "result");
    if (result.empty()) {
        auto error = Impl::extract_string(*response, "error");
        return Err<BlockTemplateData>(ErrorCode::RpcInternalError, error);
    }
    
    BlockTemplateData data;
    data.version = static_cast<uint32_t>(Impl::extract_int(result, "version"));
    data.curtime = static_cast<uint32_t>(Impl::extract_int(result, "curtime"));
    data.height = static_cast<uint32_t>(Impl::extract_int(result, "height"));
    data.coinbase_value = Impl::extract_int(result, "coinbasevalue");
    data.target = Impl::extract_string(result, "target");
    data.mintime = static_cast<uint64_t>(Impl::extract_int(result, "mintime"));
    
    // bits в hex формате
    auto bits_hex = Impl::extract_string(result, "bits");
    if (bits_hex.size() == 8) {
        data.bits = static_cast<uint32_t>(std::stoul(bits_hex, nullptr, 16));
    }
    
    // previousblockhash
    auto prev_hash_hex = Impl::extract_string(result, "previousblockhash");
    if (prev_hash_hex.size() == 64) {
        for (std::size_t i = 0; i < 32; ++i) {
            std::string byte_str = prev_hash_hex.substr(i * 2, 2);
            data.prev_blockhash[31 - i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        }
    }
    
    return data;
}

Result<void> RpcClient::submit_block(std::string_view block_hex) {
    std::string params = std::format("[\"{}\"]", block_hex);
    
    auto response = impl_->call("submitblock", params);
    if (!response) {
        return Err<void>(response.error().code, response.error().message);
    }
    
    // Проверяем результат
    auto result = Impl::extract_string(*response, "result");
    
    // null означает успех
    if (result == "null" || result.empty()) {
        return {};
    }
    
    // Иначе это ошибка
    return Err<void>(ErrorCode::MiningBlockRejected, result);
}

Result<void> RpcClient::ping() {
    auto response = impl_->call("getnetworkinfo");
    if (!response) {
        return Err<void>(response.error().code, response.error().message);
    }
    return {};
}

} // namespace quaxis::bitcoin
