/**
 * @file aux_rpc_client.cpp
 * @brief Реализация RPC клиента для auxiliary chains
 */

#include "aux_rpc_client.hpp"

#include <curl/curl.h>
#include <sstream>
#include <atomic>

namespace quaxis::merged {

// =============================================================================
// CURL callback
// =============================================================================

namespace {

std::size_t write_callback(
    char* ptr,
    std::size_t size,
    std::size_t nmemb,
    void* userdata
) {
    auto* response = static_cast<std::string*>(userdata);
    response->append(ptr, size * nmemb);
    return size * nmemb;
}

// Глобальная инициализация CURL
std::atomic<bool> curl_initialized{false};

void ensure_curl_init() {
    bool expected = false;
    if (curl_initialized.compare_exchange_strong(expected, true)) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
}

} // anonymous namespace

// =============================================================================
// AuxRpcClient::Impl
// =============================================================================

struct AuxRpcClient::Impl {
    std::string url;
    std::string user;
    std::string password;
    uint32_t timeout;
    CURL* curl{nullptr};
    
    Impl(std::string u, std::string usr, std::string pwd, uint32_t t)
        : url(std::move(u))
        , user(std::move(usr))
        , password(std::move(pwd))
        , timeout(t) {
        
        ensure_curl_init();
        curl = curl_easy_init();
    }
    
    ~Impl() {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
    
    Result<std::string> call(std::string_view method, std::string_view params) {
        if (!curl) {
            return std::unexpected(Error{ErrorCode::RpcConnectionFailed,
                "CURL не инициализирован"});
        }
        
        // Формируем JSON-RPC запрос
        std::ostringstream json;
        json << R"({"jsonrpc":"2.0","id":1,"method":")" << method 
             << R"(","params":)" << params << "}";
        std::string request_body = json.str();
        
        std::string response;
        
        // Настраиваем CURL
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 
                         static_cast<long>(request_body.size()));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout));
        
        // Заголовки
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // Basic auth
        if (!user.empty()) {
            std::string auth = user + ":" + password;
            curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
        }
        
        // Выполняем запрос
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            return std::unexpected(Error{ErrorCode::RpcConnectionFailed,
                std::string("CURL ошибка: ") + curl_easy_strerror(res)});
        }
        
        // Проверяем HTTP код
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 401) {
            return std::unexpected(Error{ErrorCode::RpcAuthFailed,
                "Ошибка авторизации RPC"});
        }
        
        if (http_code != 200) {
            return std::unexpected(Error{ErrorCode::RpcInternalError,
                "HTTP ошибка: " + std::to_string(http_code)});
        }
        
        // Проверяем наличие ошибки в JSON ответе
        if (response.find("\"error\"") != std::string::npos &&
            response.find("\"error\":null") == std::string::npos) {
            // Есть ошибка в ответе
            auto error_start = response.find("\"error\"");
            auto error_end = response.find('}', error_start);
            if (error_end != std::string::npos) {
                return std::unexpected(Error{ErrorCode::RpcInternalError,
                    "RPC ошибка: " + response.substr(error_start, error_end - error_start + 1)});
            }
        }
        
        return response;
    }
    
    Result<void> ping() {
        // Используем getblockchaininfo для проверки соединения
        // Большинство Bitcoin-like нод поддерживают этот метод
        auto result = call("getblockchaininfo", "[]");
        if (!result) {
            return std::unexpected(result.error());
        }
        return {};
    }
};

// =============================================================================
// AuxRpcClient Implementation
// =============================================================================

AuxRpcClient::AuxRpcClient(
    std::string url,
    std::string user,
    std::string password,
    uint32_t timeout
) : impl_(std::make_unique<Impl>(
        std::move(url),
        std::move(user),
        std::move(password),
        timeout
    )) {}

AuxRpcClient::~AuxRpcClient() = default;

AuxRpcClient::AuxRpcClient(AuxRpcClient&&) noexcept = default;
AuxRpcClient& AuxRpcClient::operator=(AuxRpcClient&&) noexcept = default;

Result<std::string> AuxRpcClient::call(
    std::string_view method,
    std::string_view params
) {
    return impl_->call(method, params);
}

Result<void> AuxRpcClient::ping() {
    return impl_->ping();
}

const std::string& AuxRpcClient::url() const noexcept {
    return impl_->url;
}

} // namespace quaxis::merged
