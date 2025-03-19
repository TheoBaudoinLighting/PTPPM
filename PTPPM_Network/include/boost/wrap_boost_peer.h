#ifndef WRAP_BOOST_PEER_H
#define WRAP_BOOST_PEER_H

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include "wrap_boost_network.h"
#include "wrap_boost_task.h"
#include <string>
#include <map>
#include <set>
#include <mutex>
#include <chrono>
#include <functional>
#include <memory>

namespace wrap_boost {

struct PeerDiscoveryMessage;

enum class PeerState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    HANDSHAKING,
    ACTIVE,
    DISCONNECTING
};

class PeerInfo {
public:
    PeerInfo();
    PeerInfo(uint64_t peerId, const std::string& address, uint16_t port);
    
    uint64_t getPeerId() const;
    std::string getAddress() const;
    uint16_t getPort() const;
    
    std::string getEndpoint() const;
    uint64_t getConnectionId() const;
    void setConnectionId(uint64_t connectionId);
    
    PeerState getState() const;
    void setState(PeerState state);
    
    std::chrono::system_clock::time_point getLastSeen() const;
    void updateLastSeen();
    
    bool isConnected() const;
    
private:
    uint64_t peerId_;
    std::string address_;
    uint16_t port_;
    uint64_t connectionId_;
    PeerState state_;
    std::chrono::system_clock::time_point lastSeen_;
};

class IPeerEventHandler {
public:
    virtual ~IPeerEventHandler() = default;
    
    virtual void onPeerDiscovered(const PeerInfo& peer) = 0;
    virtual void onPeerConnected(const PeerInfo& peer) = 0;
    virtual void onPeerDisconnected(const PeerInfo& peer, const std::string& reason) = 0;
    virtual void onPeerMessage(const PeerInfo& peer, const NetworkMessage& message) = 0;
    virtual void onPeerStateChanged(const PeerInfo& peer, PeerState oldState, PeerState newState) = 0;
};

class PeerManager : public INetworkEventHandler {
public:
    PeerManager();
    ~PeerManager();
    
    PeerManager(const PeerManager&) = delete;
    PeerManager& operator=(const PeerManager&) = delete;
    
    void setEventHandler(IPeerEventHandler* handler);
    void setNetworkManager(std::shared_ptr<NetworkManager> networkManager);
    
    bool start(uint16_t listenPort);
    void stop();
    bool isRunning() const;
    
    uint64_t addPeer(const std::string& address, uint16_t port);
    bool removePeer(uint64_t peerId);
    bool connectToPeer(uint64_t peerId);
    bool disconnectPeer(uint64_t peerId);
    
    bool sendToPeer(uint64_t peerId, const NetworkMessage& message);
    bool broadcast(const NetworkMessage& message, uint64_t excludePeerId = 0);
    
    PeerInfo getPeerInfo(uint64_t peerId) const;
    std::vector<PeerInfo> getAllPeers() const;
    std::vector<PeerInfo> getConnectedPeers() const;
    bool hasPeer(uint64_t peerId) const;
    size_t getPeerCount() const;
    size_t getConnectedPeerCount() const;
    
    void enablePeerDiscovery(bool enable);
    bool isPeerDiscoveryEnabled() const;
    
    virtual void onConnect(uint64_t connectionId, const std::string& endpoint) override;
    virtual void onDisconnect(uint64_t connectionId, const NetworkErrorInfo& reason) override;
    virtual void onMessage(uint64_t connectionId, const NetworkMessage& message) override;
    virtual void onError(uint64_t connectionId, const NetworkErrorInfo& error) override;
    
private:
    bool initializeNetworkManager();
    uint64_t generatePeerId();
    void setPeerState(uint64_t peerId, PeerState state);
    uint64_t findPeerByConnectionId(uint64_t connectionId) const;
    
    void startPeerDiscoveryProtocol();
    void stopPeerDiscoveryProtocol();
    void handlePeerDiscoveryMessage(uint64_t connectionId, const NetworkMessage& message);
    void sendPeerList(uint64_t connectionId);
    
    void broadcastDiscoveryAnnouncement();
    void cleanupDiscoveryCache();
    void handlePeerRequest(uint64_t connectionId, const PeerDiscoveryMessage& message);
    void handlePeerResponse(const PeerDiscoveryMessage& message);
    void handlePeerAnnouncement(const PeerDiscoveryMessage& message);
    void updatePeerLastSeen(uint64_t peerId);

    std::shared_ptr<NetworkManager> networkManager_;
    bool ownNetworkManager_;
    
    std::map<uint64_t, PeerInfo> peers_;
    mutable std::mutex peersMutex_;
    
    IPeerEventHandler* eventHandler_;
    
    uint64_t nextPeerId_;
    std::mutex peerIdMutex_;
    
    uint16_t listenPort_;
    bool running_;
    bool peerDiscoveryEnabled_;
    
    std::shared_ptr<TaskScheduler> taskScheduler_;
    std::atomic<bool> discoveryRunning_;
    uint64_t discoveryTaskId_;
    std::chrono::seconds announcementInterval_;
    
    std::mutex discoveryMutex_;
    std::map<uint64_t, std::chrono::system_clock::time_point> recentDiscoveryMessages_;
    
    static const size_t MAX_RECONNECT_ATTEMPTS = 5;
    static const std::chrono::seconds DISCOVERY_INTERVAL;
    static const std::chrono::seconds DEFAULT_ANNOUNCEMENT_INTERVAL;
    static const std::chrono::seconds DISCOVERY_CACHE_TTL;
};

} 

#endif 