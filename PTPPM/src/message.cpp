#include "message.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

#include <spdlog/spdlog.h>

Message::Message() : type_(MessageType::DATA) {}

Message::Message(MessageType type, const std::string& data) : type_(type) {
    if (data.size() > MAX_BODY_SIZE) {
        spdlog::error("Message data exceeds maximum allowed size");
        throw std::runtime_error("Message data exceeds maximum allowed size");
    }
    
    data_.resize(data.size());
    std::copy(data.begin(), data.end(), data_.begin());
}

Message::Message(MessageType type, const std::vector<uint8_t>& data) : type_(type) {
    if (data.size() > MAX_BODY_SIZE) {
        spdlog::error("Message data exceeds maximum allowed size");
        throw std::runtime_error("Message data exceeds maximum allowed size");
    }
    
    data_ = data;
}

MessageType Message::type() const {
    return type_;
}

std::vector<uint8_t> Message::data() const {
    return data_;
}

std::vector<uint8_t> Message::serialize() const {
    if (data_.size() > MAX_BODY_SIZE) {
        spdlog::error("Message data exceeds maximum allowed size");
        throw std::runtime_error("Message data exceeds maximum allowed size");
    }
    
    std::vector<uint8_t> result;
    result.reserve(HEADER_SIZE + data_.size());

    result.push_back(static_cast<uint8_t>(type_));

    uint32_t size = static_cast<uint32_t>(data_.size());
    result.push_back(static_cast<uint8_t>(size & 0xFF));
    result.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>((size >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((size >> 24) & 0xFF));

    result.insert(result.end(), data_.begin(), data_.end());

    return result;
}

Message Message::deserialize(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < HEADER_SIZE) {
        spdlog::error("Buffer too small to contain a valid message header");
        throw std::runtime_error("Buffer too small to contain a valid message header");
    }

    try {
        MessageType type = readType(buffer);
        uint32_t size = readSize(buffer);

        if (size > MAX_BODY_SIZE) {
            spdlog::error("Message size exceeds maximum allowed size");
            throw std::runtime_error("Message size exceeds maximum allowed size");
        }

        if (buffer.size() != HEADER_SIZE + size) {
            spdlog::error("Buffer size doesn't match expected message size");
            throw std::runtime_error("Buffer size doesn't match expected message size");
        }

        std::vector<uint8_t> data;
        data.reserve(size);
        data.insert(data.end(), buffer.begin() + HEADER_SIZE, buffer.end());
        
        return Message(type, data);
    } catch (const std::exception& e) {
        spdlog::error("Deserialization error: {}", e.what());
        throw std::runtime_error(std::string("Deserialization error: ") + e.what());
    }
}

MessageType Message::readType(const std::vector<uint8_t>& header_buffer) {
    if (header_buffer.size() < 1) {
        spdlog::error("Header buffer too small to read message type");
        throw std::runtime_error("Header buffer too small to read message type");
    }
    
    uint8_t type_value = header_buffer[0];
    
    if (type_value > static_cast<uint8_t>(MessageType::DHT_STORE_REPLY)) {
        spdlog::error("Unknown message type");
        throw std::runtime_error("Unknown message type");
    }
    
    return static_cast<MessageType>(type_value);
}

uint32_t Message::readSize(const std::vector<uint8_t>& header_buffer) {
    if (header_buffer.size() < HEADER_SIZE) {
        spdlog::error("Header buffer too small to read message size");
        throw std::runtime_error("Header buffer too small to read message size");
    }

    return static_cast<uint32_t>(header_buffer[1]) |
           (static_cast<uint32_t>(header_buffer[2]) << 8) |
           (static_cast<uint32_t>(header_buffer[3]) << 16) |
           (static_cast<uint32_t>(header_buffer[4]) << 24);
}