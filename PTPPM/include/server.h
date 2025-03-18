#pragma once
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <mutex>
#include <deque>
#include <array>
#include "config.h"

class TCPConnection : public std::enable_shared_from_this<TCPConnection> {
public:
    using Pointer = std::shared_ptr<TCPConnection>;
    static Pointer create(boost::asio::io_context& io_context);
    boost::asio::ip::tcp::socket& socket();
    void start();
    std::string get_client_info() const;
private:
    explicit TCPConnection(boost::asio::io_context& io_context);
    void handle_read(const boost::system::error_code& error, std::size_t bytes_transferred);
    void handle_write(const boost::system::error_code& error);
    boost::asio::ip::tcp::socket socket_;
    std::string remote_endpoint_;
    std::array<char, 1024> buffer_;
    std::string message_;
};

class TCPServer {
public:
    explicit TCPServer(unsigned short port);
    ~TCPServer();
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;
    void run(std::atomic<bool>& running);
    void stop();
    bool is_running() const { return is_running_; }
    unsigned short get_port() const { return port_; }
    std::vector<std::string> get_connection_logs();
    void add_log(const std::string& message);
private:
    void start_accept();
    void handle_accept(TCPConnection::Pointer new_connection, const boost::system::error_code& error);
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::vector<TCPConnection::Pointer> connections_;
    std::mutex connections_mutex_;
    std::deque<std::string> connection_logs_;
    std::mutex logs_mutex_;
    const size_t max_log_size_ = 100;
    bool is_running_ = false;
    unsigned short port_;
};
