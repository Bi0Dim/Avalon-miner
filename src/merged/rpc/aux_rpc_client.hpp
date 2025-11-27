/**
 * @file aux_rpc_client.hpp
 * @brief RPC клиент для auxiliary chains
 * 
 * Универсальный RPC клиент для взаимодействия с нодами auxiliary chains.
 * Поддерживает JSON-RPC и REST API.
 */

#pragma once

#include "../../core/types.hpp"

#include <string>
#include <memory>

namespace quaxis::merged {

/**
 * @brief RPC клиент для auxiliary chains
 * 
 * Асинхронный HTTP/HTTPS клиент для вызова RPC методов
 * auxiliary chain нод.
 */
class AuxRpcClient {
public:
    /**
     * @brief Создать клиент
     * 
     * @param url URL ноды (http://host:port)
     * @param user Имя пользователя для basic auth
     * @param password Пароль для basic auth
     * @param timeout Таймаут в секундах
     */
    AuxRpcClient(
        std::string url,
        std::string user = "",
        std::string password = "",
        uint32_t timeout = 30
    );
    
    ~AuxRpcClient();
    
    // Запрещаем копирование
    AuxRpcClient(const AuxRpcClient&) = delete;
    AuxRpcClient& operator=(const AuxRpcClient&) = delete;
    
    // Разрешаем перемещение
    AuxRpcClient(AuxRpcClient&&) noexcept;
    AuxRpcClient& operator=(AuxRpcClient&&) noexcept;
    
    /**
     * @brief Вызвать RPC метод
     * 
     * @param method Имя метода
     * @param params JSON параметры
     * @return Result<std::string> JSON ответ или ошибка
     */
    [[nodiscard]] Result<std::string> call(
        std::string_view method,
        std::string_view params = "[]"
    );
    
    /**
     * @brief Проверить соединение
     * 
     * @return Result<void> Успех или ошибка
     */
    [[nodiscard]] Result<void> ping();
    
    /**
     * @brief Получить URL
     */
    [[nodiscard]] const std::string& url() const noexcept;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace quaxis::merged
