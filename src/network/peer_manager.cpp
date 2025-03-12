#include "network/peer_manager.h"
#include "network/discovery_service.h"
#include "network/message_service.h"
#include <spdlog/spdlog.h>
#include <random>
#include <iomanip>
#include <sstream>

namespace p2p {

    PeerManager::PeerManager()
        : m_port(0),
        m_status(PeerStatus::OFFLINE),
        m_running(false) {
    }

    PeerManager::~PeerManager() {
        shutdown();
    }

    bool PeerManager::initialize(uint16_t port, const std::string& username) {
        try {
            m_port = port;
            m_username = username;

            generatePeerId();

            m_discovery_service = std::make_unique<DiscoveryService>(m_io_context, m_port);

            m_message_service = std::make_unique<MessageService>(m_io_context, m_port);

            m_discovery_service->setDiscoveryCallback(
                [this](const PeerInfo& peer) {
                    handlePeerDiscovered(peer);
                }
            );

            m_discovery_service->setLostCallback(
                [this](const std::string& peer_id) {
                    handlePeerLost(peer_id);
                }
            );

            m_message_service->setMessageCallback(
                [this](const std::string& peer_id, const std::string& message) {
                    handleIncomingMessage(peer_id, message);
                }
            );

            m_message_service->setFileCallback(
                [this](const std::string& peer_id, const std::string& filepath) {
                    handleIncomingFile(peer_id, filepath);
                }
            );

            m_running = true;
            m_io_thread = std::make_unique<std::thread>(&PeerManager::runIoService, this);

            setStatus(PeerStatus::ONLINE);

            spdlog::info("Peer manager initialized with ID: {} on port: {}", m_peer_id, m_port);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to initialize peer manager: {}", e.what());
            return false;
        }
    }

    void PeerManager::shutdown() {
        try {
            stopDiscovery();

            setStatus(PeerStatus::OFFLINE);

            m_running = false;
            m_io_context.stop();

            if (m_io_thread && m_io_thread->joinable()) {
                m_io_thread->join();
            }

            m_discovery_service.reset();
            m_message_service.reset();

            spdlog::info("Peer manager shutdown");
        }
        catch (const std::exception& e) {
            spdlog::error("Error during peer manager shutdown: {}", e.what());
        }
    }

    bool PeerManager::connect(const std::string& ip, uint16_t port) {
        try {
            m_message_service->connect(ip, port);

            spdlog::info("Connected to peer at {}:{}", ip, port);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to connect to peer: {}", e.what());
            return false;
        }
    }

    void PeerManager::disconnect(const std::string& peer_id) {
        try {
            m_message_service->disconnect(peer_id);

            spdlog::info("Disconnected from peer: {}", peer_id);
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to disconnect from peer: {}", e.what());
        }
    }

    bool PeerManager::isConnected(const std::string& peer_id) const {
        return m_message_service->isConnected(peer_id);
    }

    void PeerManager::startDiscovery() {
        try {
            m_discovery_service->start();

            spdlog::info("Peer discovery started");
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to start peer discovery: {}", e.what());
        }
    }

    void PeerManager::stopDiscovery() {
        try {
            m_discovery_service->stop();

            spdlog::info("Peer discovery stopped");
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to stop peer discovery: {}", e.what());
        }
    }

    const std::vector<PeerInfo>& PeerManager::getPeers() const {
        std::lock_guard<std::mutex> lock(m_peers_mutex);
        return m_peers;
    }

    const PeerInfo* PeerManager::getPeerById(const std::string& peer_id) const {
        std::lock_guard<std::mutex> lock(m_peers_mutex);

        for (const auto& peer : m_peers) {
            if (peer.id == peer_id) {
                return &peer;
            }
        }

        return nullptr;
    }

    void PeerManager::setStatus(PeerStatus status) {
        m_status = status;
    }

    PeerStatus PeerManager::getStatus() const {
        return m_status;
    }

    bool PeerManager::sendMessage(const std::string& peer_id, const std::string& message) {
        try {
            m_message_service->sendMessage(peer_id, message);

            spdlog::info("Sent message to peer: {}", peer_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to send message: {}", e.what());
            return false;
        }
    }

    bool PeerManager::broadcastMessage(const std::string& message) {
        try {
            std::lock_guard<std::mutex> lock(m_peers_mutex);

            for (const auto& peer : m_peers) {
                if (m_message_service->isConnected(peer.id)) {
                    m_message_service->sendMessage(peer.id, message);
                }
            }

            spdlog::info("Broadcast message to all connected peers");
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to broadcast message: {}", e.what());
            return false;
        }
    }

    bool PeerManager::sendFile(const std::string& peer_id, const std::string& filepath) {
        try {
            m_message_service->sendFile(peer_id, filepath);

            spdlog::info("Sent file to peer: {}", peer_id);
            return true;
        }
        catch (const std::exception& e) {
            spdlog::error("Failed to send file: {}", e.what());
            return false;
        }
    }

    void PeerManager::setMessageCallback(MessageCallback callback) {
        m_message_callback = callback;
    }

    void PeerManager::setPeerStatusCallback(PeerStatusCallback callback) {
        m_peer_status_callback = callback;
    }

    void PeerManager::setFileReceivedCallback(FileReceivedCallback callback) {
        m_file_received_callback = callback;
    }

    void PeerManager::handlePeerDiscovered(const PeerInfo& peer) {
        try {
            std::lock_guard<std::mutex> lock(m_peers_mutex);

            bool found = false;
            for (auto& existing_peer : m_peers) {
                if (existing_peer.id == peer.id) {
                    existing_peer = peer;
                    found = true;
                    break;
                }
            }

            if (!found) {
                m_peers.push_back(peer);
            }

            spdlog::info("Discovered peer: {} ({})", peer.name, peer.id);
        }
        catch (const std::exception& e) {
            spdlog::error("Error handling peer discovery: {}", e.what());
        }
    }

    void PeerManager::handlePeerLost(const std::string& peer_id) {
        try {
            std::lock_guard<std::mutex> lock(m_peers_mutex);

            m_peers.erase(
                std::remove_if(m_peers.begin(), m_peers.end(),
                    [&peer_id](const PeerInfo& peer) {
                        return peer.id == peer_id;
                    }
                ),
                m_peers.end()
            );

            spdlog::info("Lost peer: {}", peer_id);
        }
        catch (const std::exception& e) {
            spdlog::error("Error handling peer loss: {}", e.what());
        }
    }

    void PeerManager::handleIncomingMessage(const std::string& peer_id, const std::string& message) {
        try {
            if (m_message_callback) {
                m_message_callback(peer_id, message);
            }

            spdlog::info("Received message from peer: {}", peer_id);
        }
        catch (const std::exception& e) {
            spdlog::error("Error handling incoming message: {}", e.what());
        }
    }

    void PeerManager::handleIncomingFile(const std::string& peer_id, const std::string& filepath) {
        try {
            if (m_file_received_callback) {
                m_file_received_callback(peer_id, filepath);
            }

            spdlog::info("Received file from peer: {}", peer_id);
        }
        catch (const std::exception& e) {
            spdlog::error("Error handling incoming file: {}", e.what());
        }
    }

    void PeerManager::runIoService() {
        try {
            asio::io_context::work work(m_io_context);

            m_io_context.run();
        }
        catch (const std::exception& e) {
            spdlog::error("Error in IO service thread: {}", e.what());
        }
    }

    void PeerManager::generatePeerId() {
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        auto value = now_ms.time_since_epoch().count();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 999999);
        int random_value = dis(gen);

        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << value;
        ss << "-";
        ss << std::hex << std::setw(6) << std::setfill('0') << random_value;

        m_peer_id = ss.str();
    }

} // namespace p2p