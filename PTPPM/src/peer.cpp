#include "peer.h"
#include <iostream>
#include <future>
#include <boost/bind.hpp>
#include "dht.h"
#include <spdlog/spdlog.h>

void Peer::dispatchTask(std::function<void()> task) {
    boost::asio::post(io_context_, task);
}

Peer::Peer() : running_(false), max_connections_(200), dht_enabled_(false) {
    work_guard_ = std::make_shared<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(io_context_));
}

Peer::~Peer() {
    stopServer();
}

bool Peer::startServer(uint16_t port, size_t max_connections) {
    if (running_) {
        spdlog::error("Server is already running");
        return false;
    }

    if (port == 0) {
        spdlog::error("Invalid port (0)");
        return false;
    }

    if (max_connections == 0 || max_connections > 1000) {
        spdlog::error("Invalid max_connections value");
        return false;
    }

    max_connections_ = max_connections;

    try {
        if (!io_thread_.joinable()) {
            io_thread_ = std::thread([this]() {
                try {
                    io_context_.run();
                }
                catch (const std::exception& e) {
                    spdlog::error("IO service error: {}", e.what());
                }
            });
        }

        std::promise<std::shared_ptr<tcp::acceptor>> acceptor_promise;
        auto acceptor_future = acceptor_promise.get_future();

        dispatchTask([this, port, &acceptor_promise]() {
            try {
                auto acceptor = std::shared_ptr<tcp::acceptor>(
                    new tcp::acceptor(io_context_, tcp::endpoint(tcp::v4(), port)));
                acceptor_promise.set_value(acceptor);
            }
            catch (const std::exception& e) {
                acceptor_promise.set_exception(std::current_exception());
            }
        });

        acceptor_ = acceptor_future.get();
        running_ = true;

        spdlog::info("Server started on port {}", port);
        spdlog::info("Maximum connections: {}", max_connections_);

        dispatchTask([this]() {
            acceptLoop();
        });

        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to start server: {}", e.what());
        return false;
    }
}

void Peer::stopServer() {
    if (!running_) return;

    running_ = false;

    if (dht_enabled_ && dht_) {
        dht_->stop();
        dht_.reset();
        dht_enabled_ = false;
    }

    if (acceptor_ && acceptor_->is_open()) {
        dispatchTask([this]() {
            boost::system::error_code ec;
            acceptor_->close(ec);
        });
    }

    work_guard_.reset();

    if (io_thread_.joinable()) {
        io_thread_.join();
    }

    io_context_.stop();

    spdlog::info("Server stopped");
}

bool Peer::connectTo(const std::string& host, uint16_t port) {
    if (host.empty() || port == 0) {
        spdlog::error("Invalid host or port");
        return false;
    }

    if (!running_) {
        work_guard_ = std::make_shared<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            boost::asio::make_work_guard(io_context_));
        
        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            }
            catch (const std::exception& e) {
                spdlog::error("IO service error: {}", e.what());
            }
        });
        
        running_ = true;
    }

    try {
        auto connection = std::make_shared<Connection>(io_context_);
        
        auto connect_promise = std::make_shared<std::promise<boost::system::error_code>>();
        auto connect_future = connect_promise->get_future();

        dispatchTask([this, host, port, connection, connect_promise]() {
            tcp::resolver resolver(io_context_);
            try {
                auto endpoints = resolver.resolve(host, std::to_string(port));
                
                boost::asio::async_connect(connection->socket(), endpoints,
                    [connection, connect_promise](const boost::system::error_code& ec, const tcp::endpoint&) {
                        connect_promise->set_value(ec);
                    });
            }
            catch (const std::exception& e) {
                connect_promise->set_exception(std::current_exception());
            }
        });

        boost::system::error_code ec = connect_future.get();
        if (ec) {
            throw boost::system::system_error(ec);
        }

        spdlog::info("Connected to {}", host + ":" + std::to_string(port));

        session_.addConnection(connection);
        
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Connection failed: {}", e.what());
        return false;
    }
}

void Peer::sendMessage(size_t peer_id, const std::string& message) {
    if (message.empty()) {
        spdlog::error("Empty message");
        return;
    }
    
    auto connection = session_.getConnection(peer_id);
    
    if (connection && connection->isConnected()) {
        try {
            connection->send(Message(MessageType::DATA, message));
        } catch (const std::exception& e) {
            spdlog::error("Failed to send message: {}", e.what());
        }
    }
    else {
        spdlog::error("No connection with ID: {}", peer_id);
    }
}

void Peer::broadcastMessage(const std::string& message) {
    if (message.empty()) {
        spdlog::error("Empty message");
        return;
    }
    
    dispatchTask([this, message]() {
        try {
            session_.broadcast(Message(MessageType::DATA, message));
        } catch (const std::exception& e) {
            spdlog::error("Broadcast failed: {}", e.what());
        }
    });
}

size_t Peer::connectionCount() const {
    return session_.connectionCount();
}

bool Peer::isRunning() const {
    return running_;
}

bool Peer::enableDHT() {
    if (dht_enabled_) {
        spdlog::error("DHT is already enabled");
        return true;
    }
    
    if (!running_) {
        spdlog::error("Server must be running to enable DHT");
        return false;
    }
    
    try {
        std::promise<bool> promise;
        auto future = promise.get_future();
        
        dispatchTask([this, &promise]() {
            try {
                dht_ = std::unique_ptr<DHT>(new DHT(io_context_, *this));
                dht_->start();
                dht_enabled_ = true;
                
                session_.setDHTMessageHandler([this](const Message& msg, std::shared_ptr<Connection> sender) {
                    if (dht_enabled_ && dht_) {
                        dht_->handleDHTMessage(msg, sender);
                    }
                });
                
                spdlog::info("DHT enabled");
                promise.set_value(true);
            }
            catch (const std::exception& e) {
                spdlog::error("Failed to enable DHT: {}", e.what());
                promise.set_value(false);
            }
        });
        
        return future.get();
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to enable DHT: {}", e.what());
        return false;
    }
}

bool Peer::bootstrapDHT(const std::string& host, uint16_t port) {
    if (!dht_enabled_ || !dht_) {
        spdlog::error("DHT is not enabled");
        return false;
    }
    
    if (host.empty() || port == 0) {
        spdlog::error("Invalid host or port");
        return false;
    }
    
    return dht_->bootstrap(host, port);
}

bool Peer::bootstrapDHT(const std::vector<std::pair<std::string, uint16_t>>& nodes) {
    if (!dht_enabled_ || !dht_) {
        spdlog::error("DHT is not enabled");
        return false;
    }
    
    if (nodes.empty()) {
        spdlog::error("Empty nodes list");
        return false;
    }
    
    return dht_->joinNetwork(nodes);
}

bool Peer::storeDHT(const std::string& key, const std::string& value) {
    if (!dht_enabled_ || !dht_) {
        spdlog::error("DHT is not enabled");
        return false;
    }
    
    if (key.empty()) {
        spdlog::error("Empty key");
        return false;
    }
    
    if (value.empty()) {
        spdlog::error("Empty value");
        return false;
    }
    
    std::vector<uint8_t> data(value.begin(), value.end());
    return dht_->store(key, data);
}

std::string Peer::retrieveDHT(const std::string& key) {
    if (!dht_enabled_ || !dht_) {
        spdlog::error("DHT is not enabled");
        return "";
    }
    
    if (key.empty()) {
        spdlog::error("Empty key");
        return "";
    }
    
    try {
        auto future = dht_->retrieve(key);
        auto result = future.get();
        
        if (result) {
            return std::string(result->begin(), result->end());
        }
    } catch (const std::exception& e) {
        spdlog::error("Error retrieving from DHT: {}", e.what());
    }
    
    return "";
}

std::string Peer::getDHTStats() const {
    if (!dht_enabled_ || !dht_) {
        return "DHT is not enabled";
    }
    
    return dht_->getStats();
}

std::shared_ptr<Connection> Peer::getConnectionByAddress(const std::string& address, uint16_t port) const {
    if (address.empty() || port == 0) {
        return nullptr;
    }
    
    return session_.getConnectionByAddress(address, port);
}

void Peer::acceptLoop() {
    if (!acceptor_ || !acceptor_->is_open() || !running_) return;

    auto connection = std::make_shared<Connection>(io_context_);

    acceptor_->async_accept(
        connection->socket(),
        [this, connection](const boost::system::error_code& error) {
            if (!error) {
                if (session_.connectionCount() < max_connections_) {
                    session_.addConnection(connection);
                }
                else {
                    spdlog::error("Connection limit reached, rejecting connection from {}", connection->getRemoteAddress());
                    connection->disconnect();
                }
            }
            else if (error != boost::asio::error::operation_aborted) {
                spdlog::error("Accept error: {}", error.message());
            }

            if (running_) {
                dispatchTask([this]() {
                    acceptLoop();
                });
            }
        }
    );
}