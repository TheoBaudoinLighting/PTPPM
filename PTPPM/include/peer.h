#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>
#include "session.h"

using boost::asio::ip::tcp;

class DHT;

class Peer {
public:
    Peer();
    ~Peer();

    bool startServer(uint16_t port, size_t max_connections = 200);
    void stopServer();

    bool connectTo(const std::string& host, uint16_t port);
    void sendMessage(size_t peer_id, const std::string& message);
    void broadcastMessage(const std::string& message);
    size_t connectionCount() const;

    bool isRunning() const;
    
    bool enableDHT();
    bool bootstrapDHT(const std::string& host, uint16_t port);
    bool bootstrapDHT(const std::vector<std::pair<std::string, uint16_t>>& nodes);
    bool storeDHT(const std::string& key, const std::string& value);
    std::string retrieveDHT(const std::string& key);
    std::string getDHTStats() const;
    
    std::shared_ptr<Connection> getConnectionByAddress(const std::string& address, uint16_t port) const;
    
    boost::asio::io_context& getIoContext() { return io_context_; }

private:
    void acceptLoop();
    void dispatchTask(std::function<void()> task);

    boost::asio::io_context io_context_;
    std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::shared_ptr<tcp::acceptor> acceptor_;
    Session session_;

    std::thread io_thread_;
    std::atomic<bool> running_;

    size_t max_connections_;
    
    std::unique_ptr<DHT> dht_;
    bool dht_enabled_;
};