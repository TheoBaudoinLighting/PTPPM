#pragma once

#include <array>
#include <string>
#include <vector>
#include <random>
#include <cstdint>
#include <functional>

constexpr size_t NODE_ID_SIZE = 20;

class NodeID {
public:
    using IDType = std::array<uint8_t, NODE_ID_SIZE>;

    NodeID();
    explicit NodeID(const IDType& id);
    explicit NodeID(const std::string& hex_string);
    explicit NodeID(const std::vector<uint8_t>& bytes);

    static NodeID random();
    static NodeID atDistance(const NodeID& from, int distance);

    const IDType& raw() const { return id_; }
    std::string toHex() const;
    std::vector<uint8_t> toBytes() const;

    NodeID distanceXor(const NodeID& other) const;
    int bucketIndex(const NodeID& other) const;

    bool operator==(const NodeID& other) const;
    bool operator!=(const NodeID& other) const;
    bool operator<(const NodeID& other) const;

private:
    IDType id_;
};

namespace std {
    template<>
    struct hash<NodeID> {
        size_t operator()(const NodeID& id) const {
            const auto& raw = id.raw();
            size_t result = 0;
            for (size_t i = 0; i < sizeof(size_t) && i < NODE_ID_SIZE; ++i) {
                result = (result << 8) | raw[i];
            }
            return result;
        }
    };
}