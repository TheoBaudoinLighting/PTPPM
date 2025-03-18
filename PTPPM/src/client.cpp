#include "client.h"
#include <spdlog/spdlog.h>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

using namespace boost::placeholders;

TCPClient::TCPClient() : connected_(false), current_port_(0), work_guard_(boost::asio::make_work_guard(io_context_)) {
    io_thread_ = std::thread([this]() {
        try {
            io_context_.run();
        }
        catch (const std::exception& e) {
            spdlog::error("Exception dans le thread client IO: {}", e.what());
        }
    });
}

TCPClient::~TCPClient() {
    disconnect();
    work_guard_.reset();
    io_context_.stop();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

bool TCPClient::connect(const std::string& server_ip, unsigned short server_port) {
    try {
        if (connected_) {
            disconnect();
        }
        spdlog::info("Tentative de connexion à {}:{}", server_ip, server_port);
        add_received_message("Tentative de connexion à " + server_ip + ":" + std::to_string(server_port));
        socket_.reset(new boost::asio::ip::tcp::socket(io_context_));
        boost::asio::ip::tcp::resolver resolver(io_context_);
        boost::asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(server_ip, std::to_string(server_port));
        boost::asio::connect(*socket_, endpoints);
        connected_ = true;
        current_server_ = server_ip;
        current_port_ = server_port;
        start_receive();
        spdlog::info("Connecté avec succès à {}:{}", server_ip, server_port);
        add_received_message("Connecté au serveur " + server_ip + ":" + std::to_string(server_port));
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur de connexion à {}:{}: {}", server_ip, server_port, e.what());
        add_received_message("Erreur de connexion: " + std::string(e.what()));
        connected_ = false;
        return false;
    }
}

void TCPClient::disconnect() {
    if (!connected_) {
        return;
    }
    try {
        spdlog::info("Déconnexion du serveur {}:{}", current_server_, current_port_);
        if (socket_ && socket_->is_open()) {
            boost::system::error_code ec;
            socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            socket_->close(ec);
        }
        connected_ = false;
        add_received_message("Déconnecté du serveur");
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors de la déconnexion: {}", e.what());
    }
}

bool TCPClient::send_message(const std::string& message) {
    if (!connected_ || !socket_ || !socket_->is_open()) {
        spdlog::error("Tentative d'envoi de message sans être connecté");
        return false;
    }
    try {
        spdlog::debug("Envoi du message: {}", message);
        boost::asio::write(*socket_, boost::asio::buffer(message));
        add_received_message("Envoyé: " + message);
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors de l'envoi du message: {}", e.what());
        add_received_message("Erreur d'envoi: " + std::string(e.what()));
        connected_ = false;
        return false;
    }
}

bool TCPClient::is_connected() const {
    return connected_ && socket_ && socket_->is_open();
}

void TCPClient::start_receive() {
    if (!connected_ || !socket_ || !socket_->is_open()) {
        return;
    }
    socket_->async_read_some(boost::asio::buffer(receive_buffer_), boost::bind(&TCPClient::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}

void TCPClient::handle_receive(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error) {
        std::string received_data(receive_buffer_.data(), bytes_transferred);
        spdlog::debug("Reçu: {}", received_data);
        add_received_message("Reçu: " + received_data);
        start_receive();
    }
    else if (error != boost::asio::error::operation_aborted) {
        if (error == boost::asio::error::eof) {
            spdlog::info("Connexion fermée par le serveur");
            add_received_message("Connexion fermée par le serveur");
        }
        else {
            spdlog::error("Erreur de réception: {}", error.message());
            add_received_message("Erreur de réception: " + error.message());
        }
        connected_ = false;
    }
}

std::vector<std::string> TCPClient::get_received_messages() {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    return std::vector<std::string>(received_messages_.begin(), received_messages_.end());
}

void TCPClient::add_received_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(messages_mutex_);
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    std::tm tm_buf;
#if defined(_WIN32)
    localtime_s(&tm_buf, &now_time_t);
#else
    localtime_r(&now_time_t, &tm_buf);
#endif
    ss << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "] " << message;
    received_messages_.push_front(ss.str());
    if (received_messages_.size() > max_messages_) {
        received_messages_.pop_back();
    }
}
