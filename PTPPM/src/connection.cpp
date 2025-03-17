#include "connection.h"
#include <iostream>
#include <spdlog/spdlog.h>
Connection::Connection(boost::asio::io_context& io_context)
    : socket_(io_context),
    connected_(false),
    read_buffer_header_(Message::HEADER_SIZE),
    writing_(false) {
}

tcp::socket& Connection::socket() {
    return socket_;
}

std::string Connection::getRemoteAddress() const {
    try {
        return socket_.remote_endpoint().address().to_string() + ":" +
            std::to_string(socket_.remote_endpoint().port());
    }
    catch (const boost::system::system_error&) {
        return "Not connected";
    }
}

uint16_t Connection::getRemotePort() const {
    try {
        return socket_.remote_endpoint().port();
    }
    catch (const boost::system::system_error&) {
        return 0;
    }
}

void Connection::start(MessageHandler on_message, DisconnectHandler on_disconnect) {
    on_message_ = std::move(on_message);
    on_disconnect_ = std::move(on_disconnect);
    connected_ = true;

    doReadHeader();
}

void Connection::send(const Message& message) {
    if (!connected_) return;
    
    auto self = shared_from_this();
    boost::asio::post(getIoContext(), [self, message]() {
        bool write_in_progress;

        {
            std::lock_guard<std::mutex> lock(self->write_mutex_);
            write_in_progress = self->writing_;
            
            try {
                self->write_queue_.push(message.serialize());
            } catch (const std::exception& e) {
                spdlog::error("Serialization error: {}", e.what());
                return;
            }
        }

        if (!write_in_progress) {
            self->doWrite();
        }
    });
}

void Connection::disconnect() {
    auto self = shared_from_this();

    boost::asio::post(getIoContext(), [self]() {
        if (!self->connected_) return;

        boost::system::error_code ec;
        try {
            self->socket_.shutdown(tcp::socket::shutdown_both, ec);
        } catch (const std::exception&) {}
        
        try {
            self->socket_.close(ec);
        } catch (const std::exception&) {}
        
        self->connected_ = false;

        if (self->on_disconnect_) {
            self->on_disconnect_(self);
        }
    });
}

bool Connection::isConnected() const {
    return connected_;
}

void Connection::doReadHeader() {
    if (!connected_) return;
    
    auto self = shared_from_this();

    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_buffer_header_, Message::HEADER_SIZE),
        [self](boost::system::error_code ec, std::size_t bytes) {
            self->handleHeaderRead(ec, bytes);
        }
    );
}

void Connection::doReadBody() {
    if (!connected_) return;
    
    auto self = shared_from_this();

    uint32_t body_size = 0;
    try {
        body_size = Message::readSize(read_buffer_header_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to read message size: {}", e.what());
        disconnect();
        return;
    }

    if (body_size > Message::MAX_BODY_SIZE) {
        spdlog::error("Message body too large: {} bytes", body_size);
        disconnect();
        return;
    }

    try {
        read_buffer_body_.resize(body_size);
    } catch (const std::exception& e) {
        spdlog::error("Failed to resize read buffer: {}", e.what());
        disconnect();
        return;
    }

    boost::asio::async_read(
        socket_,
        boost::asio::buffer(read_buffer_body_, body_size),
        [self](boost::system::error_code ec, std::size_t bytes) {
            self->handleBodyRead(ec, bytes);
        }
    );
}

void Connection::doWrite() {
    if (!connected_) return;
    
    auto self = shared_from_this();

    std::lock_guard<std::mutex> lock(write_mutex_);

    if (write_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto& message_buffer = write_queue_.front();

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(message_buffer),
        [self](boost::system::error_code ec, std::size_t bytes) {
            self->handleWrite(ec, bytes);
        }
    );
}

void Connection::handleHeaderRead(boost::system::error_code ec, std::size_t bytes) {
    if (!connected_) return;
    
    if (!ec && bytes == Message::HEADER_SIZE) {
        doReadBody();
    }
    else {
        spdlog::error("Error reading header: {}", ec.message());
        disconnect();
    }
}

void Connection::handleBodyRead(boost::system::error_code ec, std::size_t bytes) {
    if (!connected_) return;
    
    if (!ec && bytes == read_buffer_body_.size()) {
        try {
            std::vector<uint8_t> full_message;
            full_message.reserve(read_buffer_header_.size() + read_buffer_body_.size());
            full_message.insert(full_message.end(), read_buffer_header_.begin(), read_buffer_header_.end());
            full_message.insert(full_message.end(), read_buffer_body_.begin(), read_buffer_body_.end());

            Message message = Message::deserialize(full_message);

            if (on_message_) {
                on_message_(message, shared_from_this());
            }

            doReadHeader();
        }
        catch (const std::exception& e) {
            spdlog::error("Error processing message: {}", e.what());
            disconnect();
        }
    }
    else {
        spdlog::error("Error reading body: {}", ec.message());
        disconnect();
    }
}

void Connection::handleWrite(boost::system::error_code ec, std::size_t bytes) {
    if (!connected_) return;
    
    if (!ec) {
        std::lock_guard<std::mutex> lock(write_mutex_);
        if (!write_queue_.empty()) {
            write_queue_.pop();
        }

        if (!write_queue_.empty()) {
            doWrite();
        }
        else {
            writing_ = false;
        }
    }
    else {
        spdlog::error("Error writing message: {}", ec.message());
        disconnect();
    }
}