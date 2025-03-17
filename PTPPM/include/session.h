#pragma once

#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <functional>
#include "connection.h"

class Session {
public:
    using DHTMessageHandler = std::function<void(const Message&, std::shared_ptr<Connection>)>;

    Session();

    void addConnection(std::shared_ptr<Connection> connection);
    void removeConnection(std::shared_ptr<Connection> connection);
    size_t connectionCount() const;

    void broadcast(const Message& message, std::shared_ptr<Connection> except = nullptr);

    std::shared_ptr<Connection> getConnection(size_t id) const;
    std::shared_ptr<Connection> getConnectionByAddress(const std::string& address, uint16_t port) const;

    void handleMessage(const Message& message, std::shared_ptr<Connection> sender);
    
    void setDHTMessageHandler(DHTMessageHandler handler);

private:
    void handleHandshake(const Message& message, std::shared_ptr<Connection> sender);
    void handleData(const Message& message, std::shared_ptr<Connection> sender);
    void handlePing(const Message& message, std::shared_ptr<Connection> sender);
    void handleDisconnect(const Message& message, std::shared_ptr<Connection> sender);
    void handleDHTMessage(const Message& message, std::shared_ptr<Connection> sender);

    std::unordered_map<size_t, std::shared_ptr<Connection>> connections_;
    mutable std::mutex connections_mutex_;
    std::atomic<size_t> next_id_;

    std::unordered_map<Connection*, size_t> connection_ids_;
    
    DHTMessageHandler dht_message_handler_;
};