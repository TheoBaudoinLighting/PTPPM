#include "network/message_service.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace p2p {

    MessageService::MessageService(asio::io_context& io_context, uint16_t port)
        : m_io_context(io_context),
        m_acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
        m_port(port) {
        startAccept();
    }

    MessageService::~MessageService() {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        for (auto& conn : m_connections) {
            try {
                conn.second->socket.close();
            } catch (const std::exception& e) {
                spdlog::error("Error closing connection: {}", e.what());
            }
        }
        m_connections.clear();
    }

    void MessageService::connect(const std::string& ip, uint16_t port) {
        try {
            auto connection = std::make_shared<Connection>(asio::ip::tcp::socket(m_io_context));
            asio::ip::tcp::resolver resolver(m_io_context);
            asio::ip::tcp::resolver::results_type endpoints = 
                resolver.resolve(ip, std::to_string(port));
            asio::connect(connection->socket, endpoints);
            connection->peer_id = getPeerIdFromSocket(connection->socket);
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            m_connections[connection->peer_id] = connection;
            startReceive(connection);
            spdlog::info("Connected to peer at {}:{}", ip, port);
        } catch (const std::exception& e) {
            spdlog::error("Failed to connect to peer: {}", e.what());
            throw;
        }
    }

    void MessageService::disconnect(const std::string& peer_id) {
        try {
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            auto it = m_connections.find(peer_id);
            if (it != m_connections.end()) {
                it->second->socket.close();
                m_connections.erase(it);
                spdlog::info("Disconnected from peer: {}", peer_id);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to disconnect from peer: {}", e.what());
            throw;
        }
    }

    bool MessageService::isConnected(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_connections_mutex);
        auto it = m_connections.find(peer_id);
        return (it != m_connections.end() && it->second->socket.is_open());
    }

    void MessageService::sendMessage(const std::string& peer_id, const std::string& message) {
        try {
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            auto it = m_connections.find(peer_id);
            if (it != m_connections.end()) {
                it->second->send_queue.push(message);
                if (!it->second->sending) {
                    it->second->sending = true;
                    sendNextMessage(it->second);
                }
            } else {
                throw std::runtime_error("Peer not connected");
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to send message: {}", e.what());
            throw;
        }
    }

    void MessageService::sendFile(const std::string& peer_id, const std::string& filepath) {
        try {
            spdlog::info("Sending file {} to peer {}", filepath, peer_id);
            std::string message = "FILE:" + filepath;
            sendMessage(peer_id, message);
        } catch (const std::exception& e) {
            spdlog::error("Failed to send file: {}", e.what());
            throw;
        }
    }

    void MessageService::setMessageCallback(MessageCallback callback) {
        m_message_callback = callback;
    }

    void MessageService::setFileCallback(FileCallback callback) {
        m_file_callback = callback;
    }

    void MessageService::startAccept() {
        auto connection = std::make_shared<Connection>(asio::ip::tcp::socket(m_io_context));
        m_acceptor.async_accept(connection->socket,
            [this, connection](const asio::error_code& error) {
                handleAccept(connection, error);
            }
        );
    }

    void MessageService::handleAccept(std::shared_ptr<Connection> connection, const asio::error_code& error) {
        if (!error) {
            connection->peer_id = getPeerIdFromSocket(connection->socket);
            {
                std::lock_guard<std::mutex> lock(m_connections_mutex);
                m_connections[connection->peer_id] = connection;
            }
            startReceive(connection);
            spdlog::info("Accepted connection from peer: {}", connection->peer_id);
        } else {
            spdlog::error("Error accepting connection: {}", error.message());
        }
        startAccept();
    }

    void MessageService::startReceive(std::shared_ptr<Connection> connection) {
        connection->socket.async_read_some(
            asio::buffer(connection->receive_buffer),
            [this, connection](const asio::error_code& error, std::size_t bytes_transferred) {
                handleReceive(connection, error, bytes_transferred);
            }
        );
    }

    void MessageService::handleReceive(std::shared_ptr<Connection> connection, 
                                      const asio::error_code& error, 
                                      std::size_t bytes_transferred) {
        if (!error) {
            std::string message(connection->receive_buffer.data(), bytes_transferred);
            processMessage(connection->peer_id, message);
            startReceive(connection);
        } else if (error == asio::error::eof || error == asio::error::connection_reset) {
            spdlog::info("Connection closed by peer: {}", connection->peer_id);
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            m_connections.erase(connection->peer_id);
        } else {
            spdlog::error("Error receiving data: {}", error.message());
        }
    }

    void MessageService::sendNextMessage(std::shared_ptr<Connection> connection) {
        if (connection->send_queue.empty()) {
            connection->sending = false;
            return;
        }
        
        std::string message = connection->send_queue.front();
        
        asio::async_write(
            connection->socket,
            asio::buffer(message),
            [this, connection](const asio::error_code& error, std::size_t bytes_transferred) {
                handleSend(connection, error, bytes_transferred);
            }
        );
    }

    void MessageService::handleSend(std::shared_ptr<Connection> connection, 
                                   const asio::error_code& error, 
                                   std::size_t bytes_transferred) {
        if (!error) {
            {
                std::lock_guard<std::mutex> lock(m_connections_mutex);
                if (!connection->send_queue.empty()) {
                    connection->send_queue.pop();
                }
            }
            sendNextMessage(connection);
        } else {
            spdlog::error("Error sending data: {}", error.message());
            try {
                connection->socket.close();
            } catch (...) {}
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            m_connections.erase(connection->peer_id);
        }
    }

    void MessageService::processMessage(const std::string& peer_id, const std::string& message) {
        if (message.substr(0, 5) == "FILE:") {
            std::string filepath = message.substr(5);
            if (m_file_callback) {
                m_file_callback(peer_id, filepath);
            }
        } else {
            if (m_message_callback) {
                m_message_callback(peer_id, message);
            }
        }
    }

    std::string MessageService::getPeerIdFromSocket(const asio::ip::tcp::socket& socket) const {
        try {
            auto endpoint = socket.remote_endpoint();
            return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        } catch (const std::exception& e) {
            spdlog::error("Failed to get peer ID from socket: {}", e.what());
            return "unknown";
        }
    }

} // namespace p2p 