/**
 * @file test_relay.cpp
 * @brief Тесты модуля UDP Relay
 * 
 * Проверяет корректность:
 * - FEC декодирования
 * - Парсинга FIBRE протокола
 * - Реконструкции блоков
 */

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include <cstring>

#include "relay/fec_decoder.hpp"
#include "relay/fibre_protocol.hpp"
#include "relay/block_reconstructor.hpp"

namespace quaxis::tests {

// =============================================================================
// Тесты FEC декодера
// =============================================================================

/**
 * @brief Класс тестов для FEC декодера
 */
class FecDecoderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаём декодер с тестовыми параметрами
        relay::FecParams params;
        params.data_chunk_count = 4;
        params.fec_chunk_count = 2;
        params.chunk_size = 100;
        decoder_ = std::make_unique<relay::FecDecoder>(params);
    }
    
    std::unique_ptr<relay::FecDecoder> decoder_;
};

/**
 * @brief Тест: пустой декодер
 */
TEST_F(FecDecoderTest, EmptyDecoder) {
    EXPECT_FALSE(decoder_->can_decode());
    EXPECT_FALSE(decoder_->has_all_data_chunks());
    EXPECT_EQ(decoder_->received_data_chunks(), 0u);
    EXPECT_EQ(decoder_->received_fec_chunks(), 0u);
}

/**
 * @brief Тест: добавление data чанка
 */
TEST_F(FecDecoderTest, AddDataChunk) {
    std::vector<uint8_t> data(100, 0xAB);
    
    EXPECT_TRUE(decoder_->add_chunk(0, false, data));
    EXPECT_EQ(decoder_->received_data_chunks(), 1u);
    EXPECT_FALSE(decoder_->can_decode());
}

/**
 * @brief Тест: отклонение дубликата
 */
TEST_F(FecDecoderTest, RejectDuplicate) {
    std::vector<uint8_t> data(100, 0xAB);
    
    EXPECT_TRUE(decoder_->add_chunk(0, false, data));
    EXPECT_FALSE(decoder_->add_chunk(0, false, data));  // Дубликат
    EXPECT_EQ(decoder_->received_data_chunks(), 1u);
}

/**
 * @brief Тест: декодирование без FEC
 */
TEST_F(FecDecoderTest, DecodeWithoutFec) {
    // Добавляем все data чанки
    for (uint16_t i = 0; i < 4; ++i) {
        std::vector<uint8_t> data(100, static_cast<uint8_t>(i));
        EXPECT_TRUE(decoder_->add_chunk(i, false, data));
    }
    
    EXPECT_TRUE(decoder_->has_all_data_chunks());
    EXPECT_TRUE(decoder_->can_decode());
    
    auto result = decoder_->decode();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->data.size(), 400u);  // 4 чанка * 100 байт
    EXPECT_EQ(result->chunks_recovered, 0u);
}

/**
 * @brief Тест: получение первых N байт
 */
TEST_F(FecDecoderTest, GetFirstNBytes) {
    // Добавляем первый чанк
    std::vector<uint8_t> data(100, 0xCD);
    EXPECT_TRUE(decoder_->add_chunk(0, false, data));
    
    auto first = decoder_->get_first_n_bytes(50);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->size(), 50u);
    EXPECT_EQ((*first)[0], 0xCD);
}

/**
 * @brief Тест: сброс декодера
 */
TEST_F(FecDecoderTest, Reset) {
    std::vector<uint8_t> data(100, 0xAB);
    decoder_->add_chunk(0, false, data);
    
    EXPECT_EQ(decoder_->received_data_chunks(), 1u);
    
    decoder_->reset();
    
    EXPECT_EQ(decoder_->received_data_chunks(), 0u);
    EXPECT_FALSE(decoder_->can_decode());
}

// =============================================================================
// Тесты FIBRE протокола
// =============================================================================

/**
 * @brief Класс тестов для FIBRE протокола
 */
class FibreProtocolTest : public ::testing::Test {
protected:
    relay::FibreParser parser_;
};

/**
 * @brief Тест: проверка magic number
 */
TEST_F(FibreProtocolTest, CheckMagic) {
    // Правильный magic
    std::vector<uint8_t> valid = {0xF1, 0xB3, 0xE0, 0x01, 0x00};
    EXPECT_TRUE(relay::FibreParser::check_magic(valid));
    
    // Неправильный magic
    std::vector<uint8_t> invalid = {0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_FALSE(relay::FibreParser::check_magic(invalid));
    
    // Слишком короткий
    std::vector<uint8_t> too_short = {0xF1, 0xB3};
    EXPECT_FALSE(relay::FibreParser::check_magic(too_short));
}

/**
 * @brief Тест: создание keepalive пакета
 */
TEST_F(FibreProtocolTest, CreateKeepalive) {
    auto keepalive = relay::FibreParser::create_keepalive();
    
    EXPECT_GE(keepalive.size(), relay::FIBRE_HEADER_SIZE);
    EXPECT_TRUE(relay::FibreParser::check_magic(keepalive));
    
    // Парсим заголовок
    auto header_result = parser_.parse_header(keepalive);
    ASSERT_TRUE(header_result.has_value());
    EXPECT_TRUE(header_result->is_keepalive());
}

/**
 * @brief Тест: парсинг валидного пакета
 */
TEST_F(FibreProtocolTest, ParseValidPacket) {
    // Создаём тестовый пакет
    std::vector<uint8_t> packet(relay::FIBRE_HEADER_SIZE + 100);
    
    // Magic
    packet[0] = 0xF1;
    packet[1] = 0xB3;
    packet[2] = 0xE0;
    packet[3] = 0x01;
    
    // Version
    packet[4] = 1;
    
    // Flags
    packet[5] = 0;
    
    // Chunk ID (big-endian)
    packet[6] = 0;
    packet[7] = 5;
    
    // Block height (big-endian)
    packet[8] = 0;
    packet[9] = 0;
    packet[10] = 0x0C;
    packet[11] = 0x8F;  // 3215
    
    // Block hash (32 bytes)
    std::memset(&packet[12], 0xAB, 32);
    
    // Total chunks (big-endian)
    packet[44] = 0;
    packet[45] = 150;
    
    // Data chunks (big-endian)
    packet[46] = 0;
    packet[47] = 100;
    
    // Payload size (big-endian)
    packet[48] = 0;
    packet[49] = 100;
    
    // Payload
    std::memset(&packet[50], 0xCD, 100);
    
    auto result = parser_.parse(packet);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(result->header.version, 1);
    EXPECT_EQ(result->header.chunk_id, 5);
    EXPECT_EQ(result->header.block_height, 3215u);
    EXPECT_EQ(result->header.total_chunks, 150);
    EXPECT_EQ(result->header.data_chunks, 100);
    EXPECT_EQ(result->header.payload_size, 100);
    EXPECT_EQ(result->payload.size(), 100u);
}

/**
 * @brief Тест: отклонение некорректного magic
 */
TEST_F(FibreProtocolTest, RejectInvalidMagic) {
    std::vector<uint8_t> packet(relay::FIBRE_HEADER_SIZE);
    packet[0] = 0x00;  // Wrong magic
    
    auto result = parser_.parse_header(packet);
    EXPECT_FALSE(result.has_value());
}

/**
 * @brief Тест: отклонение слишком короткого пакета
 */
TEST_F(FibreProtocolTest, RejectTooShort) {
    std::vector<uint8_t> packet(10);  // Too short
    
    auto result = parser_.parse_header(packet);
    EXPECT_FALSE(result.has_value());
}

/**
 * @brief Тест: сериализация и десериализация
 */
TEST_F(FibreProtocolTest, SerializeDeserialize) {
    relay::FibrePacket original;
    original.header.magic = relay::FIBRE_MAGIC;
    original.header.version = 1;
    original.header.flags = 0;
    original.header.chunk_id = 42;
    original.header.block_height = 123456;
    original.header.total_chunks = 100;
    original.header.data_chunks = 80;
    original.header.payload_size = 50;
    original.payload.resize(50, 0xEF);
    
    // Сериализуем
    auto serialized = parser_.serialize(original);
    
    // Десериализуем
    auto result = parser_.parse(serialized);
    ASSERT_TRUE(result.has_value());
    
    EXPECT_EQ(result->header.magic, original.header.magic);
    EXPECT_EQ(result->header.version, original.header.version);
    EXPECT_EQ(result->header.chunk_id, original.header.chunk_id);
    EXPECT_EQ(result->header.block_height, original.header.block_height);
    EXPECT_EQ(result->header.total_chunks, original.header.total_chunks);
    EXPECT_EQ(result->header.data_chunks, original.header.data_chunks);
    EXPECT_EQ(result->payload, original.payload);
}

// =============================================================================
// Тесты вспомогательных функций
// =============================================================================

/**
 * @brief Тест: XOR байтов
 */
TEST(XorBytesTest, BasicXor) {
    std::vector<uint8_t> dst = {0x00, 0xFF, 0xAA, 0x55};
    std::vector<uint8_t> src = {0xFF, 0x00, 0x55, 0xAA};
    
    relay::xor_bytes(dst, src);
    
    EXPECT_EQ(dst[0], 0xFF);
    EXPECT_EQ(dst[1], 0xFF);
    EXPECT_EQ(dst[2], 0xFF);
    EXPECT_EQ(dst[3], 0xFF);
}

/**
 * @brief Тест: флаги FIBRE
 */
TEST(FibreFlagsTest, HasFlag) {
    uint8_t flags = static_cast<uint8_t>(relay::FibreFlags::FecChunk) |
                    static_cast<uint8_t>(relay::FibreFlags::LastChunk);
    
    EXPECT_TRUE(relay::has_flag(flags, relay::FibreFlags::FecChunk));
    EXPECT_TRUE(relay::has_flag(flags, relay::FibreFlags::LastChunk));
    EXPECT_FALSE(relay::has_flag(flags, relay::FibreFlags::Keepalive));
    EXPECT_FALSE(relay::has_flag(flags, relay::FibreFlags::Ack));
}

/**
 * @brief Тест: преобразование флагов в строку
 */
TEST(FibreFlagsTest, FlagsToString) {
    EXPECT_EQ(relay::flags_to_string(0), "None");
    
    uint8_t flags = static_cast<uint8_t>(relay::FibreFlags::FecChunk);
    EXPECT_NE(relay::flags_to_string(flags).find("FEC"), std::string::npos);
}

} // namespace quaxis::tests
