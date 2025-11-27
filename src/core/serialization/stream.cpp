/**
 * @file stream.cpp
 * @brief Реализация потоков сериализации
 */

#include "stream.hpp"

#include <cstring>

namespace quaxis::core::serialization {

// =============================================================================
// ReadStream
// =============================================================================

void ReadStream::ensure_available(std::size_t count) {
    if (pos_ + count > data_.size()) {
        throw StreamError("Unexpected end of stream");
    }
}

uint8_t ReadStream::read_u8() {
    ensure_available(1);
    return data_[pos_++];
}

uint16_t ReadStream::read_u16_le() {
    ensure_available(2);
    uint16_t result = static_cast<uint16_t>(data_[pos_]) |
                      (static_cast<uint16_t>(data_[pos_ + 1]) << 8);
    pos_ += 2;
    return result;
}

uint32_t ReadStream::read_u32_le() {
    ensure_available(4);
    uint32_t result = static_cast<uint32_t>(data_[pos_]) |
                      (static_cast<uint32_t>(data_[pos_ + 1]) << 8) |
                      (static_cast<uint32_t>(data_[pos_ + 2]) << 16) |
                      (static_cast<uint32_t>(data_[pos_ + 3]) << 24);
    pos_ += 4;
    return result;
}

uint64_t ReadStream::read_u64_le() {
    ensure_available(8);
    uint64_t result = static_cast<uint64_t>(data_[pos_]) |
                      (static_cast<uint64_t>(data_[pos_ + 1]) << 8) |
                      (static_cast<uint64_t>(data_[pos_ + 2]) << 16) |
                      (static_cast<uint64_t>(data_[pos_ + 3]) << 24) |
                      (static_cast<uint64_t>(data_[pos_ + 4]) << 32) |
                      (static_cast<uint64_t>(data_[pos_ + 5]) << 40) |
                      (static_cast<uint64_t>(data_[pos_ + 6]) << 48) |
                      (static_cast<uint64_t>(data_[pos_ + 7]) << 56);
    pos_ += 8;
    return result;
}

int32_t ReadStream::read_i32_le() {
    return static_cast<int32_t>(read_u32_le());
}

int64_t ReadStream::read_i64_le() {
    return static_cast<int64_t>(read_u64_le());
}

uint64_t ReadStream::read_varint() {
    uint8_t first = read_u8();
    if (first < 0xFD) {
        return first;
    } else if (first == 0xFD) {
        return read_u16_le();
    } else if (first == 0xFE) {
        return read_u32_le();
    } else {
        return read_u64_le();
    }
}

Bytes ReadStream::read_bytes(std::size_t count) {
    ensure_available(count);
    Bytes result(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                 data_.begin() + static_cast<std::ptrdiff_t>(pos_ + count));
    pos_ += count;
    return result;
}

Hash256 ReadStream::read_hash256() {
    ensure_available(32);
    Hash256 result;
    std::memcpy(result.data(), data_.data() + pos_, 32);
    pos_ += 32;
    return result;
}

std::string ReadStream::read_string() {
    uint64_t len = read_varint();
    if (len > 10'000'000) {
        throw StreamError("String too long");
    }
    auto bytes = read_bytes(static_cast<std::size_t>(len));
    return std::string(bytes.begin(), bytes.end());
}

std::size_t ReadStream::remaining() const noexcept {
    return data_.size() - pos_;
}

std::size_t ReadStream::position() const noexcept {
    return pos_;
}

bool ReadStream::eof() const noexcept {
    return pos_ >= data_.size();
}

void ReadStream::skip(std::size_t count) {
    ensure_available(count);
    pos_ += count;
}

// =============================================================================
// WriteStream
// =============================================================================

WriteStream::WriteStream(std::size_t reserve_size) {
    data_.reserve(reserve_size);
}

void WriteStream::write_u8(uint8_t value) {
    data_.push_back(value);
}

void WriteStream::write_u16_le(uint16_t value) {
    data_.push_back(static_cast<uint8_t>(value));
    data_.push_back(static_cast<uint8_t>(value >> 8));
}

void WriteStream::write_u32_le(uint32_t value) {
    data_.push_back(static_cast<uint8_t>(value));
    data_.push_back(static_cast<uint8_t>(value >> 8));
    data_.push_back(static_cast<uint8_t>(value >> 16));
    data_.push_back(static_cast<uint8_t>(value >> 24));
}

void WriteStream::write_u64_le(uint64_t value) {
    data_.push_back(static_cast<uint8_t>(value));
    data_.push_back(static_cast<uint8_t>(value >> 8));
    data_.push_back(static_cast<uint8_t>(value >> 16));
    data_.push_back(static_cast<uint8_t>(value >> 24));
    data_.push_back(static_cast<uint8_t>(value >> 32));
    data_.push_back(static_cast<uint8_t>(value >> 40));
    data_.push_back(static_cast<uint8_t>(value >> 48));
    data_.push_back(static_cast<uint8_t>(value >> 56));
}

void WriteStream::write_i32_le(int32_t value) {
    write_u32_le(static_cast<uint32_t>(value));
}

void WriteStream::write_i64_le(int64_t value) {
    write_u64_le(static_cast<uint64_t>(value));
}

void WriteStream::write_varint(uint64_t value) {
    if (value < 0xFD) {
        write_u8(static_cast<uint8_t>(value));
    } else if (value <= 0xFFFF) {
        write_u8(0xFD);
        write_u16_le(static_cast<uint16_t>(value));
    } else if (value <= 0xFFFFFFFF) {
        write_u8(0xFE);
        write_u32_le(static_cast<uint32_t>(value));
    } else {
        write_u8(0xFF);
        write_u64_le(value);
    }
}

void WriteStream::write_bytes(ByteSpan data) {
    data_.insert(data_.end(), data.begin(), data.end());
}

void WriteStream::write_hash256(const Hash256& hash) {
    data_.insert(data_.end(), hash.begin(), hash.end());
}

void WriteStream::write_string(std::string_view str) {
    write_varint(str.size());
    data_.insert(data_.end(), str.begin(), str.end());
}

const Bytes& WriteStream::data() const noexcept {
    return data_;
}

Bytes WriteStream::take_data() noexcept {
    return std::move(data_);
}

std::size_t WriteStream::size() const noexcept {
    return data_.size();
}

void WriteStream::clear() {
    data_.clear();
}

} // namespace quaxis::core::serialization
