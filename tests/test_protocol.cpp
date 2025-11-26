/**
 * @file test_protocol.cpp
 * @brief Тесты бинарного протокола связи с ASIC
 * 
 * Проверяет корректность кодирования и декодирования
 * сообщений протокола Quaxis (JOB, SHARE, PING, STATUS).
 */

#include <gtest/gtest.h>
#include <array>
#include <cstring>

#include "network/protocol.hpp"
#include "core/types.hpp"

namespace quaxis::tests {

/**
 * @brief Класс тестов для протокола
 */
class ProtocolTest : public ::testing::Test {
protected:
    network::Protocol protocol;
};

/**
 * @brief Тест: размер сообщения JOB
 * 
 * JOB = 1 байт тип + 48 байт данные = 49 байт
 */
TEST_F(ProtocolTest, JobMessageSize) {
    network::JobMessage job{};
    
    EXPECT_EQ(sizeof(job.midstate), 32);
    EXPECT_EQ(sizeof(job.header_tail), 12);
    EXPECT_EQ(sizeof(job.job_id), 4);
    
    // Общий размер данных: 48 байт
    EXPECT_EQ(sizeof(job.midstate) + sizeof(job.header_tail) + sizeof(job.job_id), 48);
}

/**
 * @brief Тест: размер сообщения SHARE
 * 
 * SHARE = 1 байт тип + 8 байт данные = 9 байт
 */
TEST_F(ProtocolTest, ShareMessageSize) {
    network::ShareMessage share{};
    
    EXPECT_EQ(sizeof(share.job_id), 4);
    EXPECT_EQ(sizeof(share.nonce), 4);
    
    // Общий размер данных: 8 байт
    EXPECT_EQ(sizeof(share.job_id) + sizeof(share.nonce), 8);
}

/**
 * @brief Тест: сериализация JOB
 */
TEST_F(ProtocolTest, JobSerialization) {
    network::JobMessage job{};
    
    // Заполняем midstate
    for (size_t i = 0; i < 32; ++i) {
        job.midstate[i] = static_cast<uint8_t>(i);
    }
    
    // Заполняем header_tail
    for (size_t i = 0; i < 12; ++i) {
        job.header_tail[i] = static_cast<uint8_t>(0x80 + i);
    }
    
    // job_id
    job.job_id = 0x12345678;
    
    auto serialized = protocol.serialize_job(job);
    
    // 1 байт тип + 48 байт данные
    EXPECT_EQ(serialized.size(), 49);
    
    // Проверяем тип сообщения
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(network::MessageType::JOB));
    
    // Проверяем midstate
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(serialized[1 + i], static_cast<uint8_t>(i));
    }
    
    // Проверяем header_tail
    for (size_t i = 0; i < 12; ++i) {
        EXPECT_EQ(serialized[33 + i], static_cast<uint8_t>(0x80 + i));
    }
    
    // Проверяем job_id (little-endian)
    uint32_t job_id = serialized[45] | (serialized[46] << 8) |
                      (serialized[47] << 16) | (serialized[48] << 24);
    EXPECT_EQ(job_id, 0x12345678);
}

/**
 * @brief Тест: десериализация SHARE
 */
TEST_F(ProtocolTest, ShareDeserialization) {
    // Формируем сообщение SHARE
    std::array<uint8_t, 9> data{};
    data[0] = static_cast<uint8_t>(network::MessageType::SHARE);
    
    // job_id = 0xAABBCCDD (little-endian)
    data[1] = 0xDD;
    data[2] = 0xCC;
    data[3] = 0xBB;
    data[4] = 0xAA;
    
    // nonce = 0x11223344 (little-endian)
    data[5] = 0x44;
    data[6] = 0x33;
    data[7] = 0x22;
    data[8] = 0x11;
    
    auto result = protocol.deserialize_share(data.data(), data.size());
    
    ASSERT_TRUE(result.has_value()) << "Десериализация SHARE должна быть успешной";
    
    auto& share = result.value();
    EXPECT_EQ(share.job_id, 0xAABBCCDD);
    EXPECT_EQ(share.nonce, 0x11223344);
}

/**
 * @brief Тест: PING сообщение
 */
TEST_F(ProtocolTest, PingMessage) {
    uint64_t timestamp = 1700000000000ULL; // миллисекунды
    
    auto serialized = protocol.serialize_ping(timestamp);
    
    // 1 байт тип + 8 байт timestamp
    EXPECT_EQ(serialized.size(), 9);
    
    // Проверяем тип
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(network::MessageType::PING));
    
    // Проверяем timestamp (little-endian)
    uint64_t parsed_ts = 0;
    for (size_t i = 0; i < 8; ++i) {
        parsed_ts |= static_cast<uint64_t>(serialized[1 + i]) << (i * 8);
    }
    EXPECT_EQ(parsed_ts, timestamp);
}

/**
 * @brief Тест: STATUS сообщение
 */
TEST_F(ProtocolTest, StatusMessage) {
    network::StatusMessage status{};
    status.hashrate = 90000000000000ULL; // 90 TH/s
    status.temp_chip = 750;  // 75.0°C
    status.temp_board = 550; // 55.0°C
    status.fan_speed = 3000; // 3000 RPM
    status.errors = 0;
    
    auto serialized = protocol.serialize_status(status);
    
    // 1 байт тип + 16 байт данные
    EXPECT_EQ(serialized.size(), 17);
    
    // Проверяем тип
    EXPECT_EQ(serialized[0], static_cast<uint8_t>(network::MessageType::STATUS));
}

/**
 * @brief Тест: определение типа сообщения
 */
TEST_F(ProtocolTest, MessageTypeDetection) {
    EXPECT_EQ(protocol.get_message_type(0x01), network::MessageType::JOB);
    EXPECT_EQ(protocol.get_message_type(0x02), network::MessageType::SHARE);
    EXPECT_EQ(protocol.get_message_type(0x03), network::MessageType::PING);
    EXPECT_EQ(protocol.get_message_type(0x04), network::MessageType::STATUS);
    EXPECT_EQ(protocol.get_message_type(0xFF), network::MessageType::UNKNOWN);
}

/**
 * @brief Тест: размер сообщения по типу
 */
TEST_F(ProtocolTest, MessageSize) {
    EXPECT_EQ(protocol.get_message_size(network::MessageType::JOB), 49);
    EXPECT_EQ(protocol.get_message_size(network::MessageType::SHARE), 9);
    EXPECT_EQ(protocol.get_message_size(network::MessageType::PING), 9);
    EXPECT_EQ(protocol.get_message_size(network::MessageType::STATUS), 17);
}

/**
 * @brief Тест: порядок байт (Little-Endian)
 */
TEST_F(ProtocolTest, ByteOrder) {
    uint32_t value = 0x12345678;
    
    std::array<uint8_t, 4> bytes{};
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    
    EXPECT_EQ(bytes[0], 0x78);
    EXPECT_EQ(bytes[1], 0x56);
    EXPECT_EQ(bytes[2], 0x34);
    EXPECT_EQ(bytes[3], 0x12);
}

/**
 * @brief Тест: валидация сообщения
 */
TEST_F(ProtocolTest, MessageValidation) {
    // Валидное JOB сообщение
    std::vector<uint8_t> valid_job(49);
    valid_job[0] = static_cast<uint8_t>(network::MessageType::JOB);
    
    EXPECT_TRUE(protocol.validate_message(valid_job.data(), valid_job.size()));
    
    // Слишком короткое сообщение
    std::vector<uint8_t> short_msg(5);
    short_msg[0] = static_cast<uint8_t>(network::MessageType::JOB);
    
    EXPECT_FALSE(protocol.validate_message(short_msg.data(), short_msg.size()));
    
    // Неизвестный тип
    std::vector<uint8_t> unknown_msg(10);
    unknown_msg[0] = 0xFF;
    
    EXPECT_FALSE(protocol.validate_message(unknown_msg.data(), unknown_msg.size()));
}

/**
 * @brief Тест: round-trip для JOB
 */
TEST_F(ProtocolTest, JobRoundTrip) {
    network::JobMessage original{};
    
    // Заполняем данные
    for (size_t i = 0; i < 32; ++i) {
        original.midstate[i] = static_cast<uint8_t>(rand() % 256);
    }
    for (size_t i = 0; i < 12; ++i) {
        original.header_tail[i] = static_cast<uint8_t>(rand() % 256);
    }
    original.job_id = 0xDEADBEEF;
    
    // Сериализация
    auto serialized = protocol.serialize_job(original);
    
    // Десериализация
    auto result = protocol.deserialize_job(serialized.data(), serialized.size());
    
    ASSERT_TRUE(result.has_value());
    
    auto& parsed = result.value();
    
    // Проверяем, что данные совпадают
    for (size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(parsed.midstate[i], original.midstate[i]);
    }
    for (size_t i = 0; i < 12; ++i) {
        EXPECT_EQ(parsed.header_tail[i], original.header_tail[i]);
    }
    EXPECT_EQ(parsed.job_id, original.job_id);
}

/**
 * @brief Тест: round-trip для SHARE
 */
TEST_F(ProtocolTest, ShareRoundTrip) {
    network::ShareMessage original{};
    original.job_id = 0xCAFEBABE;
    original.nonce = 0xFEEDFACE;
    
    auto serialized = protocol.serialize_share(original);
    auto result = protocol.deserialize_share(serialized.data(), serialized.size());
    
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(result.value().job_id, original.job_id);
    EXPECT_EQ(result.value().nonce, original.nonce);
}

} // namespace quaxis::tests
