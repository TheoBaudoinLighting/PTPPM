#pragma once

#include <string>
#include <functional>
#include <asio.hpp>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <atomic>

namespace p2p {

    class MessageService {
    public:
        MessageService(asio::io_context& io_context, uint16_t port);
        ~MessageService();

        void connect(const std::string& ip, uint16_t port);
        void disconnect(const std::string& peer_id);
        bool isConnected(const std::string& peer_id) const;

        void sendMessage(const std::string& peer_id, const std::string& message);
        void sendFile(const std::string& peer_id, const std::string& filepath);

        using MessageCallback = std::function<void(const std::string& peer_id, const std::string& message)>;
        using FileCallback = std::function<void(const std::string& peer_id, const std::string& filepath)>;

        void setMessageCallback(MessageCallback callback);
        void setFileCallback(FileCallback callback);

    private:
        asio::io_context& m_io_context;
        asio::ip::tcp::acceptor m_acceptor;

        struct Connection {
            asio::ip::tcp::socket socket;
            std::string peer_id;
            std::array<char, 1024> receive_buffer;
            std::queue<std::string> send_queue;
            std::atomic<bool> sending;

            Connection(asio::ip::tcp::socket&& s) : socket(std::move(s)), sending(false) {}
        };

        std::unordered_map<std::string, std::shared_ptr<Connection>> m_connections;
        mutable std::mutex m_connections_mutex;

        MessageCallback m_message_callback;
        FileCallback m_file_callback;

        uint16_t m_port;

        void startAccept();
        void handleAccept(std::shared_ptr<Connection> connection, const asio::error_code& error);
        void startReceive(std::shared_ptr<Connection> connection);
        void handleReceive(std::shared_ptr<Connection> connection, const asio::error_code& error, std::size_t bytes_transferred);
        void sendNextMessage(std::shared_ptr<Connection> connection);
        void handleSend(std::shared_ptr<Connection> connection, const asio::error_code& error, std::size_t bytes_transferred);
        void processMessage(const std::string& peer_id, const std::string& message);
        std::string getPeerIdFromSocket(const asio::ip::tcp::socket& socket) const;
    };

} // namespace p2p