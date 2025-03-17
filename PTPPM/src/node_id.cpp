#include "node_id.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <cctype>
#include <spdlog/spdlog.h>

NodeID::NodeID() {
    std::fill(id_.begin(), id_.end(), 0);
}

NodeID::NodeID(const IDType& id) : id_(id) {
}

NodeID::NodeID(const std::string& hex_string) {
    if (hex_string.length() != NODE_ID_SIZE * 2) {
        spdlog::error("Invalid hex string length");
        throw std::invalid_argument("Invalid hex string length");
    }

    for (const char& c : hex_string) {
        if (!std::isxdigit(c)) {
            spdlog::error("Invalid hex character in string");
            throw std::invalid_argument("Invalid hex character in string");
        }
    }

    for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
        if (i * 2 + 1 >= hex_string.length()) {
            spdlog::error("Hex string too short");
            throw std::invalid_argument("Hex string too short");
        }
        
        std::string byte = hex_string.substr(i * 2, 2);
        
        try {
            id_[i] = static_cast<uint8_t>(std::stoi(byte, nullptr, 16));
        } catch (const std::exception& e) {
            spdlog::error("Invalid hex value: {}", e.what());
            throw std::invalid_argument("Invalid hex value");
        }
    }
}

NodeID::NodeID(const std::vector<uint8_t>& bytes) {
    if (bytes.size() != NODE_ID_SIZE) {
        spdlog::error("Invalid byte vector size");
        throw std::invalid_argument("Invalid byte vector size");
    }
    std::copy(bytes.begin(), bytes.end(), id_.begin());
}

NodeID NodeID::random() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<unsigned int> dist(0, 255);

    IDType id;
    for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
        id[i] = static_cast<uint8_t>(dist(gen));
    }

    return NodeID(id);
}

NodeID NodeID::atDistance(const NodeID& from, int distance) {
    if (distance < 0 || distance >= static_cast<int>(NODE_ID_SIZE * 8)) {
        spdlog::error("Distance out of bounds");
        throw std::invalid_argument("Distance out of bounds");
    }

    IDType result = from.raw();

    int byte_pos = distance / 8;
    int bit_pos = 7 - (distance % 8);

    result[byte_pos] ^= (1 << bit_pos);

    return NodeID(result);
}

std::string NodeID::toHex() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (const auto& byte : id_) {
        ss << std::setw(2) << static_cast<int>(byte);
    }

    return ss.str();
}

std::vector<uint8_t> NodeID::toBytes() const {
    return std::vector<uint8_t>(id_.begin(), id_.end());
}

NodeID NodeID::distanceXor(const NodeID& other) const {
    IDType result;
    for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
        result[i] = id_[i] ^ other.id_[i];
    }
    return NodeID(result);
}

int NodeID::bucketIndex(const NodeID& other) const {
    auto xor_result = distanceXor(other);
    const auto& raw = xor_result.raw();

    for (size_t i = 0; i < NODE_ID_SIZE; ++i) {
        if (raw[i] == 0) continue;

        uint8_t val = raw[i];
        for (int j = 7; j >= 0; --j) {
            if ((val & (1 << j)) != 0) {
                return static_cast<int>(i * 8 + (7 - j));
            }
        }
    }

    return -1;
}

bool NodeID::operator==(const NodeID& other) const {
    return std::equal(id_.begin(), id_.end(), other.id_.begin());
}

bool NodeID::operator!=(const NodeID& other) const {
    return !(*this == other);
}

bool NodeID::operator<(const NodeID& other) const {
    return std::lexicographical_compare(id_.begin(), id_.end(), other.id_.begin(), other.id_.end());
}