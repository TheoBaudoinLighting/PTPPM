#pragma once

#include <string>
#include <functional>
#include <asio.hpp>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include "network/peer_manager.h" 

namespace p2p {

    class DiscoveryService {
    public:
        DiscoveryService(asio::io_context& io_context, uint16_t port);
        ~DiscoveryService();

        void start();
        void stop();

        using DiscoveryCallback = std::function<void(const PeerInfo& peer)>;
        using LostCallback = std::function<void(const std::string& peer_id)>;

        void setDiscoveryCallback(DiscoveryCallback callback);
        void setLostCallback(LostCallback callback);

    private:
        asio::io_context& m_io_context;
        asio::ip::udp::socket m_socket;
        asio::ip::udp::endpoint m_multicast_endpoint;
        asio::steady_timer m_timer;
        std::unordered_map<std::string, PeerInfo> m_discovered_peers;
        std::mutex m_peers_mutex;
        DiscoveryCallback m_discovery_callback;
        LostCallback m_lost_callback;
        uint16_t m_port;
        std::atomic<bool> m_running;
        std::array<char, 1024> m_receive_buffer;

        void startReceive();
        void handleReceive(const asio::error_code& error, std::size_t bytes_transferred);
        void broadcastPresence();
        void checkPeerTimeouts();
        void scheduleBroadcast();
        void scheduleTimeoutCheck();
    };

} // namespace p2p