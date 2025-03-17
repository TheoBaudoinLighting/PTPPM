#pragma once

#include <string>
#include <unordered_map>
#include <set>
#include <map>
#include <mutex>
#include <future>
#include <optional>
#include <functional>
#include <chrono>
#include <boost/asio.hpp>
#include "node_id.h"
#include "routing_table.h"
#include "peer.h"
#include "message.h"

enum class DHTMessageType : uint8_t {
    FIND_NODE = 10,
    FIND_NODE_REPLY = 11,
    FIND_VALUE = 12,
    FIND_VALUE_REPLY = 13,
    STORE = 14,
    STORE_REPLY = 15
};

struct DHTEntry {
    std::vector<uint8_t> value;
    std::chrono::system_clock::time_point expiry;

    DHTEntry() = default;

    DHTEntry(const std::vector<uint8_t>& value,
        const std::chrono::seconds& ttl = std::chrono::seconds(86400))
        : value(value),
        expiry(std::chrono::system_clock::now() + ttl) {
    }

    bool isExpired() const {
        return std::chrono::system_clock::now() >= expiry;
    }
};

class DHT {
public:
    static constexpr size_t ALPHA = 3;
    static constexpr size_t K = KBucket::K;
    static constexpr size_t REPLICATION_FACTOR = 3;

    DHT(boost::asio::io_context& io_context, Peer& peer);

    void start();
    void stop();

    bool store(const std::string& key, const std::vector<uint8_t>& value,
        const std::chrono::seconds& ttl = std::chrono::seconds(86400));

    std::future<std::optional<std::vector<uint8_t>>> retrieve(const std::string& key);

    bool remove(const std::string& key);

    bool bootstrap(const std::string& host, uint16_t port);

    bool joinNetwork(const std::vector<std::pair<std::string, uint16_t>>& bootstrap_nodes);

    NodeID getNodeId() const;

    void setStoreCallback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback);

    void handleDHTMessage(const Message& message, std::shared_ptr<Connection> sender);

    std::string getStats() const;

private:
    boost::asio::io_context& io_context_;
    Peer& peer_;

    NodeID node_id_;

    RoutingTable routing_table_;

    std::unordered_map<NodeID, DHTEntry> storage_;
    mutable std::mutex storage_mutex_;

    std::mutex operations_mutex_;

    std::function<void(const std::string&, const std::vector<uint8_t>&)> store_callback_;

    boost::asio::steady_timer maintenance_timer_;

    NodeID keyToNodeId(const std::string& key) const;

    std::vector<Contact> findNode(const NodeID& target);

    void findValue(const NodeID& key,
        std::promise<std::optional<std::vector<uint8_t>>>& promise);

    void handleFindNode(const Message& message, std::shared_ptr<Connection> sender);
    void handleFindNodeReply(const Message& message, std::shared_ptr<Connection> sender);
    void handleFindValue(const Message& message, std::shared_ptr<Connection> sender);
    void handleFindValueReply(const Message& message, std::shared_ptr<Connection> sender);
    void handleStore(const Message& message, std::shared_ptr<Connection> sender);
    void handleStoreReply(const Message& message, std::shared_ptr<Connection> sender);

    std::vector<uint8_t> serializeContacts(const std::vector<Contact>& contacts) const;
    std::vector<Contact> deserializeContacts(const std::vector<uint8_t>& data) const;

    void startMaintenanceTimer();
    void doMaintenance();

    void sendDHTMessage(std::shared_ptr<Connection> connection,
        DHTMessageType type,
        const std::vector<uint8_t>& data);
};