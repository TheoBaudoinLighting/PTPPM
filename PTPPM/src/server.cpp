#include "server.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <boost/bind/bind.hpp>

using namespace boost::placeholders;

TCPConnection::Pointer TCPConnection::create(boost::asio::io_context& io_context) {
    return Pointer(new TCPConnection(io_context));
}

TCPConnection::TCPConnection(boost::asio::io_context& io_context) : socket_(io_context) {
}

boost::asio::ip::tcp::socket& TCPConnection::socket() {
    return socket_;
}

void TCPConnection::start() {
    try {
        remote_endpoint_ = get_client_info();
        spdlog::info("Nouvelle connexion de {}", remote_endpoint_);
        socket_.async_read_some(boost::asio::buffer(buffer_), boost::bind(&TCPConnection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors du démarrage de la connexion: {}", e.what());
    }
}

std::string TCPConnection::get_client_info() const {
    try {
        auto endpoint = socket_.remote_endpoint();
        return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
    }
    catch (const std::exception& e) {
        return "unknown";
    }
}

void TCPConnection::handle_read(const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (!error) {
        std::string received_data(buffer_.data(), bytes_transferred);
        spdlog::debug("Reçu de {}: {}", remote_endpoint_, received_data);
        message_ = "Echo: " + received_data;
        boost::asio::async_write(socket_, boost::asio::buffer(message_), boost::bind(&TCPConnection::handle_write, shared_from_this(), boost::asio::placeholders::error));
    }
    else if (error != boost::asio::error::operation_aborted) {
        if (error == boost::asio::error::eof) {
            spdlog::info("Connexion fermée par {}", remote_endpoint_);
        }
        else {
            spdlog::error("Erreur de lecture de {}: {}", remote_endpoint_, error.message());
        }
    }
}

void TCPConnection::handle_write(const boost::system::error_code& error) {
    if (!error) {
        socket_.async_read_some(boost::asio::buffer(buffer_), boost::bind(&TCPConnection::handle_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
    }
    else if (error != boost::asio::error::operation_aborted) {
        spdlog::error("Erreur d'écriture vers {}: {}", remote_endpoint_, error.message());
    }
}

TCPServer::TCPServer(unsigned short port) : io_context_(), acceptor_(io_context_), is_running_(false), port_(port) {
}

void TCPServer::run(std::atomic<bool>& running) {
    try {
        if (is_running_) {
            spdlog::warn("Le serveur est déjà en cours d'exécution");
            return;
        }
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        spdlog::info("Serveur TCP démarré sur le port {}", port_);
        add_log("Serveur démarré sur le port " + std::to_string(port_));
        is_running_ = true;
        start_accept();
        while (running && is_running_) {
            io_context_.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        stop();
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur dans le serveur TCP: {}", e.what());
        add_log("Erreur: " + std::string(e.what()));
        stop();
    }
}

void TCPServer::stop() {
    try {
        if (!is_running_) {
            return;
        }
        is_running_ = false;
        spdlog::info("Arrêt du serveur TCP");
        add_log("Serveur arrêté");
        boost::system::error_code ec;
        if (acceptor_.is_open()) {
            acceptor_.close(ec);
            if (ec) {
                spdlog::error("Erreur lors de la fermeture de l'acceptor: {}", ec.message());
            }
        }
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (auto& conn : connections_) {
                try {
                    boost::system::error_code close_ec;
                    if (conn && conn->socket().is_open()) {
                        conn->socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, close_ec);
                        conn->socket().close(close_ec);
                    }
                }
                catch (const std::exception& e) {
                    spdlog::error("Erreur lors de la fermeture d'une connexion: {}", e.what());
                }
            }
            connections_.clear();
        }
        try {
            io_context_.stop();
        }
        catch (const std::exception& e) {
            spdlog::error("Erreur lors de l'arrêt du io_context: {}", e.what());
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors de l'arrêt du serveur: {}", e.what());
    }
}

TCPServer::~TCPServer() {
    try {
        stop();
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur dans le destructeur de TCPServer: {}", e.what());
    }
}

void TCPServer::start_accept() {
    TCPConnection::Pointer new_connection = TCPConnection::create(io_context_);
    acceptor_.async_accept(new_connection->socket(), boost::bind(&TCPServer::handle_accept, this, new_connection, boost::asio::placeholders::error));
}

void TCPServer::handle_accept(TCPConnection::Pointer new_connection, const boost::system::error_code& error) {
    if (!error) {
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.push_back(new_connection);
        }
        new_connection->start();
        std::string client_info = new_connection->get_client_info();
        add_log("Nouvelle connexion de " + client_info);
        start_accept();
    }
    else if (error != boost::asio::error::operation_aborted) {
        spdlog::error("Erreur lors de l'acceptation d'une connexion: {}", error.message());
        start_accept();
    }
}

void TCPServer::add_log(const std::string& message) {
    try {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time);
        std::stringstream log_entry;
        log_entry << std::put_time(&tm, "[%H:%M:%S] ") << message;
        std::lock_guard<std::mutex> lock(logs_mutex_);
        connection_logs_.push_back(log_entry.str());
        if (connection_logs_.size() > max_log_size_) {
            connection_logs_.pop_front();
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors de l'ajout d'un log: {}", e.what());
    }
}

std::vector<std::string> TCPServer::get_connection_logs() {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    return std::vector<std::string>(connection_logs_.begin(), connection_logs_.end());
}
