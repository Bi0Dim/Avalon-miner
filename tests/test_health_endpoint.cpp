/**
 * @file test_health_endpoint.cpp
 * @brief Тесты для HTTP сервера и health/metrics endpoints
 */

#include <gtest/gtest.h>

#include "http/http_server.hpp"
#include "http/health_handler.hpp"
#include "http/metrics_handler.hpp"

namespace quaxis::tests {

// =============================================================================
// Тесты HTTP Response
// =============================================================================

class HttpResponseTest : public ::testing::Test {};

/**
 * @brief Тест: создание JSON ответа
 */
TEST_F(HttpResponseTest, JsonResponse) {
    auto response = http::HttpResponse::json("{\"status\":\"ok\"}");
    
    EXPECT_EQ(response.status, http::HttpStatus::OK);
    EXPECT_EQ(response.headers["Content-Type"], "application/json");
    EXPECT_EQ(response.body, "{\"status\":\"ok\"}");
}

/**
 * @brief Тест: создание text ответа
 */
TEST_F(HttpResponseTest, TextResponse) {
    auto response = http::HttpResponse::text("Hello World");
    
    EXPECT_EQ(response.status, http::HttpStatus::OK);
    EXPECT_EQ(response.headers["Content-Type"], "text/plain; charset=utf-8");
    EXPECT_EQ(response.body, "Hello World");
}

/**
 * @brief Тест: создание error ответа
 */
TEST_F(HttpResponseTest, ErrorResponse) {
    auto response = http::HttpResponse::error(http::HttpStatus::NotFound, "Not found");
    
    EXPECT_EQ(response.status, http::HttpStatus::NotFound);
    EXPECT_EQ(response.headers["Content-Type"], "application/json");
    EXPECT_NE(response.body.find("error"), std::string::npos);
}

/**
 * @brief Тест: сериализация ответа
 */
TEST_F(HttpResponseTest, Serialize) {
    auto response = http::HttpResponse::json("{\"test\":true}");
    std::string serialized = response.serialize();
    
    // Проверяем наличие HTTP строки статуса
    EXPECT_NE(serialized.find("HTTP/1.1 200 OK"), std::string::npos);
    
    // Проверяем Content-Length
    EXPECT_NE(serialized.find("Content-Length:"), std::string::npos);
    
    // Проверяем Content-Type
    EXPECT_NE(serialized.find("Content-Type: application/json"), std::string::npos);
    
    // Проверяем тело
    EXPECT_NE(serialized.find("{\"test\":true}"), std::string::npos);
}

// =============================================================================
// Тесты HTTP Status
// =============================================================================

TEST(HttpStatusTest, GetStatusText) {
    EXPECT_EQ(http::get_status_text(http::HttpStatus::OK), "OK");
    EXPECT_EQ(http::get_status_text(http::HttpStatus::NotFound), "Not Found");
    EXPECT_EQ(http::get_status_text(http::HttpStatus::ServiceUnavailable), "Service Unavailable");
    EXPECT_EQ(http::get_status_text(http::HttpStatus::InternalServerError), "Internal Server Error");
}

// =============================================================================
// Тесты Health Handler
// =============================================================================

class HealthHandlerTest : public ::testing::Test {};

/**
 * @brief Тест: простой health handler
 */
TEST_F(HealthHandlerTest, SimpleHealthHandler) {
    auto handler = http::create_simple_health_handler();
    
    http::HttpRequest request;
    request.method = http::HttpMethod::GET;
    request.path = "/health";
    
    auto response = handler(request);
    
    EXPECT_EQ(response.status, http::HttpStatus::OK);
    EXPECT_NE(response.body.find("healthy"), std::string::npos);
    EXPECT_NE(response.body.find("uptime_seconds"), std::string::npos);
}

/**
 * @brief Тест: health handler с провайдером данных
 */
TEST_F(HealthHandlerTest, HealthHandlerWithProvider) {
    auto handler = http::create_health_handler([]() {
        http::HealthData data;
        data.start_time = std::chrono::steady_clock::now();
        data.mode = fallback::FallbackMode::PrimarySHM;
        data.bitcoin_core_connected = true;
        data.asic_connections = 3;
        data.last_job_age_ms = 150;
        data.is_healthy = true;
        data.status_message = "healthy";
        return data;
    });
    
    http::HttpRequest request;
    request.method = http::HttpMethod::GET;
    request.path = "/health";
    
    auto response = handler(request);
    
    EXPECT_EQ(response.status, http::HttpStatus::OK);
    EXPECT_NE(response.body.find("\"status\": \"healthy\""), std::string::npos);
    EXPECT_NE(response.body.find("\"mode\": \"primary_shm\""), std::string::npos);
    EXPECT_NE(response.body.find("\"bitcoin_core\": \"connected\""), std::string::npos);
    EXPECT_NE(response.body.find("\"asic_connections\": 3"), std::string::npos);
}

/**
 * @brief Тест: unhealthy status
 */
TEST_F(HealthHandlerTest, UnhealthyStatus) {
    auto handler = http::create_health_handler([]() {
        http::HealthData data;
        data.start_time = std::chrono::steady_clock::now();
        data.is_healthy = false;
        data.status_message = "bitcoin disconnected";
        return data;
    });
    
    http::HttpRequest request;
    auto response = handler(request);
    
    EXPECT_EQ(response.status, http::HttpStatus::ServiceUnavailable);
    EXPECT_NE(response.body.find("bitcoin disconnected"), std::string::npos);
}

// =============================================================================
// Тесты Metrics Handler
// =============================================================================

class MetricsHandlerTest : public ::testing::Test {};

/**
 * @brief Тест: metrics handler
 */
TEST_F(MetricsHandlerTest, MetricsHandler) {
    auto handler = http::create_metrics_handler([]() {
        http::MetricsData data;
        data.hashrate_ths = 90.5;
        data.jobs_sent = 12345;
        data.shares_found = 42;
        data.blocks_found = 1;
        data.uptime_seconds = 86400;
        data.mode = 0;
        data.bitcoin_core_connected = true;
        data.asic_connections = 3;
        data.merged_chains_active = 11;
        return data;
    });
    
    http::HttpRequest request;
    request.method = http::HttpMethod::GET;
    request.path = "/metrics";
    
    auto response = handler(request);
    
    EXPECT_EQ(response.status, http::HttpStatus::OK);
    EXPECT_NE(response.headers["Content-Type"].find("text/plain"), std::string::npos);
    
    // Проверяем наличие метрик
    EXPECT_NE(response.body.find("quaxis_hashrate_ths 90.5"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_jobs_sent_total 12345"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_shares_found_total 42"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_blocks_found_total 1"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_uptime_seconds 86400"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_mode 0"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_bitcoin_core_connected 1"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_asic_connections 3"), std::string::npos);
    EXPECT_NE(response.body.find("quaxis_merged_chains_active 11"), std::string::npos);
    
    // Проверяем формат Prometheus
    EXPECT_NE(response.body.find("# HELP"), std::string::npos);
    EXPECT_NE(response.body.find("# TYPE"), std::string::npos);
}

/**
 * @brief Тест: latency histogram в metrics
 */
TEST_F(MetricsHandlerTest, LatencyHistogram) {
    auto handler = http::create_metrics_handler([]() {
        http::MetricsData data;
        data.latency_bucket_1ms = 100;
        data.latency_bucket_5ms = 500;
        data.latency_bucket_10ms = 900;
        data.latency_bucket_inf = 1000;
        return data;
    });
    
    http::HttpRequest request;
    auto response = handler(request);
    
    EXPECT_NE(response.body.find("quaxis_latency_ms_bucket"), std::string::npos);
    EXPECT_NE(response.body.find("le=\"1\""), std::string::npos);
    EXPECT_NE(response.body.find("le=\"5\""), std::string::npos);
    EXPECT_NE(response.body.find("le=\"10\""), std::string::npos);
    EXPECT_NE(response.body.find("le=\"+Inf\""), std::string::npos);
}

// =============================================================================
// Тесты HTTP Server Config
// =============================================================================

TEST(HttpServerConfigTest, DefaultValues) {
    http::HttpServerConfig config;
    
    EXPECT_EQ(config.bind_address, "0.0.0.0");
    EXPECT_EQ(config.port, 9090);
    EXPECT_EQ(config.max_connections, 100);
    EXPECT_TRUE(config.enabled);
}

} // namespace quaxis::tests
