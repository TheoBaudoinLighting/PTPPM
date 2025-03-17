#pragma once

#include <memory>
#include <queue>
#include <functional>
#include <boost/asio.hpp>
#include "message.h"

using boost::asio::ip::tcp;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using MessageHandler = std::function<void(const Message&, std::shared_ptr<Connection>)>;
    using DisconnectHandler = std::function<void(std::shared_ptr<Connection>)>;

    Connection(boost::asio::io_context& io_context);

    tcp::socket& socket();
    std::string getRemoteAddress() const;
    uint16_t getRemotePort() const;

    void start(MessageHandler on_message, DisconnectHandler on_disconnect);
    void send(const Message& message);
    void disconnect();

    bool isConnected() const;
    
    boost::asio::io_context& getIoContext() { return static_cast<boost::asio::io_context&>(socket_.get_executor().context()); }

private:
    void doReadHeader();
    void doReadBody();
    void doWrite();

    void handleHeaderRead(boost::system::error_code ec, std::size_t bytes);
    void handleBodyRead(boost::system::error_code ec, std::size_t bytes);
    void handleWrite(boost::system::error_code ec, std::size_t bytes);

    tcp::socket socket_;
    std::atomic<bool> connected_;

    std::vector<uint8_t> read_buffer_header_;
    std::vector<uint8_t> read_buffer_body_;

    std::queue<std::vector<uint8_t>> write_queue_;
    std::mutex write_mutex_;
    bool writing_;

    MessageHandler on_message_;
    DisconnectHandler on_disconnect_;
};