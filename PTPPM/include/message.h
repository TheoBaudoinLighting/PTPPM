#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/asio.hpp>

enum class MessageType : uint8_t {
    HANDSHAKE = 0,
    DATA = 1,
    PING = 2,
    PONG = 3,
    DISCONNECT = 4,

    DHT_FIND_NODE = 10,
    DHT_FIND_NODE_REPLY = 11,
    DHT_FIND_VALUE = 12,
    DHT_FIND_VALUE_REPLY = 13,
    DHT_STORE = 14,
    DHT_STORE_REPLY = 15
};

class Message {
public:
    static constexpr size_t HEADER_SIZE = 5;
    static constexpr size_t MAX_BODY_SIZE = 1024 * 1024;

    Message();
    Message(MessageType type, const std::string& data);
    Message(MessageType type, const std::vector<uint8_t>& data);

    MessageType type() const;
    std::vector<uint8_t> data() const;

    std::vector<uint8_t> serialize() const;
    static Message deserialize(const std::vector<uint8_t>& buffer);

    static MessageType readType(const std::vector<uint8_t>& header_buffer);
    static uint32_t readSize(const std::vector<uint8_t>& header_buffer);

private:
    MessageType type_;
    std::vector<uint8_t> data_;
};