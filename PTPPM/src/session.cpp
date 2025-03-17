#include "session.h"
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>

Session::Session() : next_id_(0) {}

void Session::addConnection(std::shared_ptr<Connection> connection) {
    if (!connection || !connection->isConnected()) {
        return;
    }
    
    size_t id;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        id = next_id_++;
        connections_[id] = connection;
        connection_ids_[connection.get()] = id;
    }

    spdlog::info("New connection from {}", connection->getRemoteAddress());
    spdlog::info("Connection ID: {}", id);

    connection->start(
        [this](const Message& msg, std::shared_ptr<Connection> conn) {
            if (conn && conn->isConnected()) {
                boost::asio::post(conn->getIoContext(), [this, msg, conn]() {
                    this->handleMessage(msg, conn);
                });
            }
        },
        [this](std::shared_ptr<Connection> conn) {
            if (conn) {
                boost::asio::post(conn->getIoContext(), [this, conn]() {
                    this->removeConnection(conn);
                });
            }
        }
    );

    try {
        std::string welcome_msg = "Welcome! Your connection ID is " + std::to_string(id);
        connection->send(Message(MessageType::HANDSHAKE, welcome_msg));

        std::string join_msg = "Peer " + std::to_string(id) + " joined from " +
            connection->getRemoteAddress();
        broadcast(Message(MessageType::DATA, join_msg), connection);
    } catch (const std::exception& e) {
        spdlog::error("Error during connection setup: {}", e.what());
    }
}

void Session::removeConnection(std::shared_ptr<Connection> connection) {
    if (!connection) {
        return;
    }
    
    size_t id;
    bool found = false;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connection_ids_.find(connection.get());
        if (it != connection_ids_.end()) {
            id = it->second;
            connections_.erase(id);
            connection_ids_.erase(it);
            found = true;
        }
    }

    if (found) {
        spdlog::info("Connection closed: {}", connection->getRemoteAddress());
        spdlog::info("Connection ID: {}", id);

        try {
            std::string leave_msg = "Peer " + std::to_string(id) + " left";
            broadcast(Message(MessageType::DATA, leave_msg));
        } catch (const std::exception& e) {
            spdlog::error("Error sending leave message: {}", e.what());
        }
    }
}

size_t Session::connectionCount() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return connections_.size();
}

void Session::broadcast(const Message& message, std::shared_ptr<Connection> except) {
    std::vector<std::shared_ptr<Connection>> connections_to_send;
    
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& [id, conn] : connections_) {
            if (conn && conn->isConnected() && (!except || conn != except)) {
                connections_to_send.push_back(conn);
            }
        }
    }
    
    for (auto& conn : connections_to_send) {
        try {
            conn->send(message);
        } catch (const std::exception& e) {
            spdlog::info("Broadcast error to {}: {}", conn->getRemoteAddress(), e.what());
        }
    }
}

std::shared_ptr<Connection> Session::getConnection(size_t id) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = connections_.find(id);
    if (it != connections_.end() && it->second && it->second->isConnected()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Connection> Session::getConnectionByAddress(const std::string& address, uint16_t port) const {
    if (address.empty() || port == 0) {
        return nullptr;
    }
    
    std::string full_address = address + ":" + std::to_string(port);
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    for (const auto& [id, conn] : connections_) {
        if (conn && conn->isConnected() && conn->getRemoteAddress() == full_address) {
            return conn;
        }
    }
    
    return nullptr;
}

void Session::handleMessage(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) {
        return;
    }
    
    try {
        switch (message.type()) {
        case MessageType::HANDSHAKE:
            handleHandshake(message, sender);
            break;
        case MessageType::DATA:
            handleData(message, sender);
            break;
        case MessageType::PING:
            handlePing(message, sender);
            break;
        case MessageType::DISCONNECT:
            handleDisconnect(message, sender);
            break;
        case MessageType::DHT_FIND_NODE:
        case MessageType::DHT_FIND_NODE_REPLY:
        case MessageType::DHT_FIND_VALUE:
        case MessageType::DHT_FIND_VALUE_REPLY:
        case MessageType::DHT_STORE:
        case MessageType::DHT_STORE_REPLY:
            handleDHTMessage(message, sender);
            break;
        default:
            spdlog::error("Unknown message type: {}", static_cast<int>(message.type()));
            break;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling message: {}", e.what());
    }
}

void Session::setDHTMessageHandler(DHTMessageHandler handler) {
    dht_message_handler_ = std::move(handler);
}

void Session::handleHandshake(const Message& message, std::shared_ptr<Connection> sender) {
    if (message.data().empty()) {
        return;
    }
    
    std::string data(message.data().begin(), message.data().end());
    spdlog::info("Handshake from {}", sender->getRemoteAddress());
}

void Session::handleData(const Message& message, std::shared_ptr<Connection> sender) {
    if (message.data().empty()) {
        return;
    }
    
    std::string data(message.data().begin(), message.data().end());

    size_t sender_id = 0;
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connection_ids_.find(sender.get());
        if (it != connection_ids_.end()) {
            sender_id = it->second;
        }
    }

    spdlog::info("Data from peer {}: {}", sender_id, data);

    try {
        std::string forwarded_msg = "Peer " + std::to_string(sender_id) + " says: " + data;
        broadcast(Message(MessageType::DATA, forwarded_msg), sender);
    } catch (const std::exception& e) {
        spdlog::error("Error forwarding message: {}", e.what());
    }
}

void Session::handlePing(const Message& message, std::shared_ptr<Connection> sender) {
    try {
        sender->send(Message(MessageType::PONG, message.data()));
    } catch (const std::exception& e) {
        spdlog::error("Error sending pong: {}", e.what());
    }
}

void Session::handleDisconnect(const Message& message, std::shared_ptr<Connection> sender) {
    std::string data;
    if (!message.data().empty()) {
        data = std::string(message.data().begin(), message.data().end());
        spdlog::info("Disconnect message from {}", sender->getRemoteAddress());
    }

    boost::asio::post(sender->getIoContext(), [sender]() {
        sender->disconnect();
    });
}

void Session::handleDHTMessage(const Message& message, std::shared_ptr<Connection> sender) {
    if (dht_message_handler_) {
        boost::asio::post(sender->getIoContext(), [this, message, sender]() {
            try {
                dht_message_handler_(message, sender);
            } catch (const std::exception& e) {
                spdlog::error("Error in DHT message handler: {}", e.what());
            }
        });
    } else {
        spdlog::error("DHT message received but no handler is set");
    }
}