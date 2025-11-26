/**
 * @file protocol.cpp
 * @brief Реализация бинарного протокола
 */

#include "protocol.hpp"
#include "../core/byte_order.hpp"

#include <cstring>

namespace quaxis::network {

// =============================================================================
// NewJobMessage
// =============================================================================

Bytes NewJobMessage::serialize() const {
    Bytes data;
    data.reserve(1 + constants::JOB_MESSAGE_SIZE);
    
    // Команда
    data.push_back(static_cast<uint8_t>(Command::NewJob));
    
    // Задание
    auto job_data = job.serialize();
    data.insert(data.end(), job_data.begin(), job_data.end());
    
    return data;
}

Result<NewJobMessage> NewJobMessage::deserialize(ByteSpan data) {
    if (data.size() < constants::JOB_MESSAGE_SIZE) {
        return Err<NewJobMessage>(ErrorCode::NetworkRecvFailed, "Недостаточно данных для NewJob");
    }
    
    auto job_result = mining::Job::deserialize(data);
    if (!job_result) {
        return Err<NewJobMessage>(job_result.error().code, job_result.error().message);
    }
    
    return NewJobMessage{*job_result};
}

// =============================================================================
// ShareMessage
// =============================================================================

Bytes ShareMessage::serialize() const {
    Bytes data;
    data.reserve(1 + constants::SHARE_MESSAGE_SIZE);
    
    // Ответ
    data.push_back(static_cast<uint8_t>(Response::Share));
    
    // Share
    auto share_data = share.serialize();
    data.insert(data.end(), share_data.begin(), share_data.end());
    
    return data;
}

Result<ShareMessage> ShareMessage::deserialize(ByteSpan data) {
    if (data.size() < constants::SHARE_MESSAGE_SIZE) {
        return Err<ShareMessage>(ErrorCode::NetworkRecvFailed, "Недостаточно данных для Share");
    }
    
    auto share_result = mining::Share::deserialize(data);
    if (!share_result) {
        return Err<ShareMessage>(share_result.error().code, share_result.error().message);
    }
    
    return ShareMessage{*share_result};
}

// =============================================================================
// SetTargetMessage
// =============================================================================

Bytes SetTargetMessage::serialize() const {
    Bytes data;
    data.reserve(1 + 32);
    
    data.push_back(static_cast<uint8_t>(Command::SetTarget));
    data.insert(data.end(), target.begin(), target.end());
    
    return data;
}

Result<SetTargetMessage> SetTargetMessage::deserialize(ByteSpan data) {
    if (data.size() < 32) {
        return Err<SetTargetMessage>(ErrorCode::NetworkRecvFailed, "Недостаточно данных для SetTarget");
    }
    
    SetTargetMessage msg;
    std::memcpy(msg.target.data(), data.data(), 32);
    
    return msg;
}

// =============================================================================
// StatusMessage
// =============================================================================

Bytes StatusMessage::serialize() const {
    Bytes data;
    data.reserve(1 + 8);
    
    data.push_back(static_cast<uint8_t>(Response::Status));
    
    // hashrate (4 байта)
    uint8_t buf[4];
    write_le32(buf, hashrate);
    data.insert(data.end(), buf, buf + 4);
    
    // temperature (1 байт)
    data.push_back(temperature);
    
    // fan_speed (1 байт)
    data.push_back(fan_speed);
    
    // errors (2 байта)
    write_le16(reinterpret_cast<uint8_t*>(buf), errors);
    data.insert(data.end(), buf, buf + 2);
    
    return data;
}

Result<StatusMessage> StatusMessage::deserialize(ByteSpan data) {
    if (data.size() < 8) {
        return Err<StatusMessage>(ErrorCode::NetworkRecvFailed, "Недостаточно данных для Status");
    }
    
    StatusMessage msg;
    msg.hashrate = read_le32(data.data());
    msg.temperature = data[4];
    msg.fan_speed = data[5];
    msg.errors = read_le16(data.data() + 6);
    
    return msg;
}

// =============================================================================
// ErrorMessage
// =============================================================================

Bytes ErrorMessage::serialize() const {
    Bytes data;
    data.reserve(1 + 1 + message.size());
    
    data.push_back(static_cast<uint8_t>(Response::Error));
    data.push_back(error_code);
    data.insert(data.end(), message.begin(), message.end());
    
    return data;
}

Result<ErrorMessage> ErrorMessage::deserialize(ByteSpan data) {
    if (data.size() < 1) {
        return Err<ErrorMessage>(ErrorCode::NetworkRecvFailed, "Недостаточно данных для Error");
    }
    
    ErrorMessage msg;
    msg.error_code = data[0];
    if (data.size() > 1) {
        msg.message = std::string(
            reinterpret_cast<const char*>(data.data() + 1),
            data.size() - 1
        );
    }
    
    return msg;
}

// =============================================================================
// ProtocolParser
// =============================================================================

void ProtocolParser::add_data(ByteSpan data) {
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

std::optional<ParsedMessage> ProtocolParser::try_parse() {
    if (buffer_.empty()) {
        return std::nullopt;
    }
    
    auto response_type = static_cast<Response>(buffer_[0]);
    
    switch (response_type) {
        case Response::Share: {
            if (buffer_.size() < 1 + constants::SHARE_MESSAGE_SIZE) {
                return std::nullopt;
            }
            
            auto result = ShareMessage::deserialize(
                ByteSpan(buffer_.data() + 1, constants::SHARE_MESSAGE_SIZE)
            );
            
            if (result) {
                buffer_.erase(buffer_.begin(), buffer_.begin() + 1 + constants::SHARE_MESSAGE_SIZE);
                return *result;
            }
            break;
        }
        
        case Response::Status: {
            if (buffer_.size() < 1 + 8) {
                return std::nullopt;
            }
            
            auto result = StatusMessage::deserialize(
                ByteSpan(buffer_.data() + 1, 8)
            );
            
            if (result) {
                buffer_.erase(buffer_.begin(), buffer_.begin() + 1 + 8);
                return *result;
            }
            break;
        }
        
        case Response::Heartbeat: {
            buffer_.erase(buffer_.begin());
            // Возвращаем пустой Status как heartbeat
            return StatusMessage{0, 0, 0, 0};
        }
        
        case Response::Error: {
            // Ищем конец сообщения (предполагаем фиксированный размер или null-terminated)
            if (buffer_.size() < 2) {
                return std::nullopt;
            }
            
            // Простой вариант: фиксированный размер 32 байта
            std::size_t msg_len = std::min(buffer_.size(), std::size_t(32));
            
            auto result = ErrorMessage::deserialize(
                ByteSpan(buffer_.data() + 1, msg_len - 1)
            );
            
            if (result) {
                buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(msg_len));
                return *result;
            }
            break;
        }
        
        default:
            // Неизвестный тип - пропускаем байт
            buffer_.erase(buffer_.begin());
            break;
    }
    
    return std::nullopt;
}

std::size_t ProtocolParser::buffered_size() const {
    return buffer_.size();
}

void ProtocolParser::clear() {
    buffer_.clear();
}

// =============================================================================
// Функции сериализации команд
// =============================================================================

Bytes serialize_new_job(const mining::Job& job) {
    NewJobMessage msg{job};
    return msg.serialize();
}

Bytes serialize_stop() {
    return {static_cast<uint8_t>(Command::Stop)};
}

Bytes serialize_heartbeat() {
    return {static_cast<uint8_t>(Command::Heartbeat)};
}

Bytes serialize_set_target(const Hash256& target) {
    SetTargetMessage msg{target};
    return msg.serialize();
}

} // namespace quaxis::network
