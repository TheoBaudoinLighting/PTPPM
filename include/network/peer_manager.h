#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <asio.hpp>
#include <thread>
#include <mutex>
#include <atomic>

namespace p2p {
    class DiscoveryService;
    class MessageService;
}

#include "data/user.h"

namespace p2p {

    enum class PeerStatus {
        OFFLINE,
        ONLINE,
        BUSY,
        AWAY
    };

    struct PeerInfo {
        std::string id;
        std::string name;
        std::string ip_address;
        uint16_t port;
        PeerStatus status;
        std::string version;
        time_t last_seen;
    };

    class PeerManager {
    public:
        PeerManager();
        ~PeerManager();

        bool initialize(uint16_t port, const std::string& username);
        void shutdown();

        bool connect(const std::string& ip, uint16_t port);
        void disconnect(const std::string& peer_id);
        bool isConnected(const std::string& peer_id) const;

        void startDiscovery();
        void stopDiscovery();
        const std::vector<PeerInfo>& getPeers() const;
        const PeerInfo* getPeerById(const std::string& peer_id) const;

        void setStatus(PeerStatus status);
        PeerStatus getStatus() const;

        bool sendMessage(const std::string& peer_id, const std::string& message);
        bool broadcastMessage(const std::string& message);

        bool sendFile(const std::string& peer_id, const std::string& filepath);

        using MessageCallback = std::function<void(const std::string& peer_id, const std::string& message)>;
        using PeerStatusCallback = std::function<void(const std::string& peer_id, PeerStatus status)>;
        using FileReceivedCallback = std::function<void(const std::string& peer_id, const std::string& filepath)>;

        void setMessageCallback(MessageCallback callback);
        void setPeerStatusCallback(PeerStatusCallback callback);
        void setFileReceivedCallback(FileReceivedCallback callback);

    private:
        uint16_t m_port;
        std::string m_username;
        std::string m_peer_id;

        std::unique_ptr<DiscoveryService> m_discovery_service;
        std::unique_ptr<MessageService> m_message_service;

        std::vector<PeerInfo> m_peers;
        mutable std::mutex m_peers_mutex;

        PeerStatus m_status;

        MessageCallback m_message_callback;
        PeerStatusCallback m_peer_status_callback;
        FileReceivedCallback m_file_received_callback;

        asio::io_context m_io_context;
        std::unique_ptr<std::thread> m_io_thread;
        std::atomic<bool> m_running;

        void handlePeerDiscovered(const PeerInfo& peer);
        void handlePeerLost(const std::string& peer_id);
        void handleIncomingMessage(const std::string& peer_id, const std::string& message);
        void handleIncomingFile(const std::string& peer_id, const std::string& filepath);

        void runIoService();
        void generatePeerId();
    };

} // namespace p2p