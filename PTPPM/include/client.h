#pragma once
#include <boost/asio.hpp>
#include <string>
#include <memory>
#include <functional>
#include <deque>
#include <mutex>
#include <atomic>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

class TCPClient {
public:
    TCPClient();
    ~TCPClient();
    bool connect(const std::string& server_ip, unsigned short server_port);
    void disconnect();
    bool send_message(const std::string& message);
    bool is_connected() const;
    std::vector<std::string> get_received_messages();
    void add_received_message(const std::string& message);
private:
    void start_receive();
    void handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred);
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::thread io_thread_;
    std::array<char, 1024> receive_buffer_;
    std::deque<std::string> received_messages_;
    std::mutex messages_mutex_;
    const size_t max_messages_ = 100;
    bool connected_;
    std::string current_server_;
    unsigned short current_port_;
};
