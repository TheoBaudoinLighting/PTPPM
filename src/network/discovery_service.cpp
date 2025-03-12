#include "network/discovery_service.h"
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <asio.hpp>

namespace p2p {

    DiscoveryService::DiscoveryService(asio::io_context& io_context, uint16_t port)
        : m_io_context(io_context),
        m_socket(io_context),
        m_timer(io_context),
        m_port(port),
        m_running(false) {
    }

    DiscoveryService::~DiscoveryService() {
        stop();
    }

    void DiscoveryService::start() {
        try {
            if (m_running) {
                return;
            }

            m_socket.open(asio::ip::udp::v4());
            m_socket.set_option(asio::ip::udp::socket::reuse_address(true));
            m_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), m_port));

            m_multicast_endpoint = asio::ip::udp::endpoint(
                asio::ip::address::from_string("239.255.42.99"), m_port
            );

            m_socket.set_option(asio::ip::multicast::join_group(
                asio::ip::address::from_string("239.255.42.99")
            ));

            m_running = true;
            startReceive();
            scheduleBroadcast();
            scheduleTimeoutCheck();

            spdlog::info("Discovery service started on port {}", m_port);
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to start discovery service: {}", e.what());
        }
    }

    void DiscoveryService::stop() {
        try {
            if (!m_running) {
                return;
            }

            m_socket.close();
            m_timer.cancel();
            m_running = false;

            spdlog::info("Discovery service stopped");
        }
        catch (const std::exception& e) {
            spdlog::error("Error stopping discovery service: {}", e.what());
        }
    }

    void DiscoveryService::setDiscoveryCallback(DiscoveryCallback callback) {
        m_discovery_callback = callback;
    }

    void DiscoveryService::setLostCallback(LostCallback callback) {
        m_lost_callback = callback;
    }

    void DiscoveryService::startReceive() {
        if (!m_running) {
            return;
        }

        m_socket.async_receive_from(
            asio::buffer(m_receive_buffer),
            m_multicast_endpoint,
            [this](const asio::error_code& error, std::size_t bytes_transferred) {
                handleReceive(error, bytes_transferred);
            }
        );
    }

    void DiscoveryService::handleReceive(const asio::error_code& error, std::size_t bytes_transferred) {
        if (!m_running) {
            return;
        }

        if (!error) {
            try {
                std::string data(m_receive_buffer.data(), bytes_transferred);
                auto json = nlohmann::json::parse(data);

                PeerInfo peer;
                peer.id = json["id"];
                peer.name = json["name"];
                peer.ip_address = json["ip"];
                peer.port = json["port"];
                peer.status = static_cast<PeerStatus>(json["status"].get<int>());
                peer.version = json["version"];
                peer.last_seen = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

                {
                    std::lock_guard<std::mutex> lock(m_peers_mutex);
                    m_discovered_peers[peer.id] = peer;
                }

                if (m_discovery_callback) {
                    m_discovery_callback(peer);
                }
            }
            catch (const std::exception& e) {
                spdlog::error("Error parsing discovery message: {}", e.what());
            }
        }
        else {
            spdlog::error("Error receiving discovery message: {}", error.message());
        }

        startReceive();
    }

    void DiscoveryService::broadcastPresence() {
        try {
            nlohmann::json json;
            json["id"] = "our-peer-id";
            json["name"] = "Our Name";
            json["ip"] = "127.0.0.1";
            json["port"] = m_port;
            json["status"] = 1;
            json["version"] = "1.0.0";

            std::string message = json.dump();

            m_socket.async_send_to(
                asio::buffer(message),
                m_multicast_endpoint,
                [](const asio::error_code& error, std::size_t /*bytes_transferred*/) {
                    if (error) {
                        spdlog::error("Error broadcasting presence: {}", error.message());
                    }
                }
            );
        }
        catch (const std::exception& e) {
            spdlog::error("Error preparing broadcast message: {}", e.what());
        }

        scheduleBroadcast();
    }

    void DiscoveryService::checkPeerTimeouts() {
        try {
            auto now_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::vector<std::string> timed_out_peers;

            {
                std::lock_guard<std::mutex> lock(m_peers_mutex);

                for (auto it = m_discovered_peers.begin(); it != m_discovered_peers.end();) {
                    auto elapsed = now_time_t - it->second.last_seen;

                    if (elapsed > 60) {
                        timed_out_peers.push_back(it->first);
                        it = m_discovered_peers.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }

            if (m_lost_callback) {
                for (const auto& peer_id : timed_out_peers) {
                    m_lost_callback(peer_id);
                }
            }
        }
        catch (const std::exception& e) {
            spdlog::error("Error checking peer timeouts: {}", e.what());
        }

        scheduleTimeoutCheck();
    }

    void DiscoveryService::scheduleBroadcast() {
        if (!m_running) {
            return;
        }

        m_timer.expires_after(std::chrono::seconds(30));
        m_timer.async_wait(
            [this](const asio::error_code& error) {
                if (!error && m_running) {
                    broadcastPresence();
                }
            }
        );
    }

    void DiscoveryService::scheduleTimeoutCheck() {
        if (!m_running) {
            return;
        }

        m_timer.expires_after(std::chrono::seconds(15));
        m_timer.async_wait(
            [this](const asio::error_code& error) {
                if (!error && m_running) {
                    checkPeerTimeouts();
                }
            }
        );
    }

} // namespace p2p