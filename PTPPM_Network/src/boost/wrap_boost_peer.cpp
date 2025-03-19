#include "boost/wrap_boost_peer.h"
#include <iostream>
#include <random>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>

namespace wrap_boost {

enum class PeerDiscoveryMessageType : uint8_t {
    PEER_REQUEST = 1,
    PEER_RESPONSE = 2,
    PEER_ANNOUNCE = 3,
    PEER_KEEPALIVE = 4
};

struct PeerDiscoveryMessage {
    PeerDiscoveryMessageType type;
    uint64_t senderId;
    std::string senderEndpoint;
    uint16_t senderPort;
    std::vector<std::pair<std::string, uint16_t>> peersList;
    
    PeerDiscoveryMessage() 
        : type(PeerDiscoveryMessageType::PEER_REQUEST), 
          senderId(0), 
          senderPort(0) {}
    
    PeerDiscoveryMessage(PeerDiscoveryMessageType msgType, uint64_t id, 
                        const std::string& endpoint, uint16_t port)
        : type(msgType), 
          senderId(id), 
          senderEndpoint(endpoint), 
          senderPort(port) {}
    
    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/) {
        ar & type;
        ar & senderId;
        ar & senderEndpoint;
        ar & senderPort;
        ar & peersList;
    }
};

const std::chrono::seconds PeerManager::DISCOVERY_INTERVAL(30);
const std::chrono::seconds PeerManager::DEFAULT_ANNOUNCEMENT_INTERVAL(60);
const std::chrono::seconds PeerManager::DISCOVERY_CACHE_TTL(300);

PeerInfo::PeerInfo()
    : peerId_(0),
      address_(""),
      port_(0),
      connectionId_(0),
      state_(PeerState::DISCONNECTED),
      lastSeen_(std::chrono::system_clock::now()) {}

PeerInfo::PeerInfo(uint64_t peerId, const std::string& address, uint16_t port)
    : peerId_(peerId),
      address_(address),
      port_(port),
      connectionId_(0),
      state_(PeerState::DISCONNECTED),
      lastSeen_(std::chrono::system_clock::now()) {}

uint64_t PeerInfo::getPeerId() const {
    return peerId_;
}

std::string PeerInfo::getAddress() const {
    return address_;
}

uint16_t PeerInfo::getPort() const {
    return port_;
}

std::string PeerInfo::getEndpoint() const {
    return address_ + ":" + std::to_string(port_);
}

uint64_t PeerInfo::getConnectionId() const {
    return connectionId_;
}

void PeerInfo::setConnectionId(uint64_t connectionId) {
    connectionId_ = connectionId;
}

PeerState PeerInfo::getState() const {
    return state_;
}

void PeerInfo::setState(PeerState state) {
    state_ = state;
}

std::chrono::system_clock::time_point PeerInfo::getLastSeen() const {
    return lastSeen_;
}

void PeerInfo::updateLastSeen() {
    lastSeen_ = std::chrono::system_clock::now();
}

bool PeerInfo::isConnected() const {
    return state_ == PeerState::CONNECTED || 
           state_ == PeerState::HANDSHAKING || 
           state_ == PeerState::ACTIVE;
}

PeerManager::PeerManager()
    : networkManager_(nullptr),
      ownNetworkManager_(false),
      eventHandler_(nullptr),
      nextPeerId_(1),
      listenPort_(0),
      running_(false),
      peerDiscoveryEnabled_(false),
      taskScheduler_(nullptr),
      discoveryRunning_(false),
      discoveryTaskId_(0),
      announcementInterval_(DEFAULT_ANNOUNCEMENT_INTERVAL) {}

PeerManager::~PeerManager() {
    stop();
}

void PeerManager::setEventHandler(IPeerEventHandler* handler) {
    eventHandler_ = handler;
}

void PeerManager::setNetworkManager(std::shared_ptr<NetworkManager> networkManager) {
    if (running_) {
        return;
    }
    
    networkManager_ = networkManager;
    ownNetworkManager_ = false;
}

bool PeerManager::start(uint16_t listenPort) {
    if (running_) {
        return false;
    }
    
    listenPort_ = listenPort;
    
    if (!initializeNetworkManager()) {
        return false;
    }
    
    if (peerDiscoveryEnabled_) {
        startPeerDiscoveryProtocol();
    }
    
    running_ = true;
    return true;
}

void PeerManager::stop() {
    if (!running_) {
        return;
    }
    
    if (peerDiscoveryEnabled_) {
        stopPeerDiscoveryProtocol();
    }
    
    std::vector<uint64_t> peerIds;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        
        for (const auto& pair : peers_) {
            peerIds.push_back(pair.first);
        }
    }
    
    for (auto peerId : peerIds) {
        disconnectPeer(peerId);
    }
    
    if (networkManager_) {
        networkManager_->disconnectAll();
        networkManager_->stopListening();
        
        if (ownNetworkManager_) {
            networkManager_.reset();
            ownNetworkManager_ = false;
        }
    }
    
    running_ = false;
}

bool PeerManager::isRunning() const {
    return running_;
}

uint64_t PeerManager::addPeer(const std::string& address, uint16_t port) {
    uint64_t peerId = generatePeerId();
    PeerInfo peerInfo(peerId, address, port);
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        peers_[peerId] = peerInfo;
    }
    
    if (eventHandler_) {
        eventHandler_->onPeerDiscovered(peerInfo);
    }
    
    return peerId;
}

bool PeerManager::removePeer(uint64_t peerId) {
    PeerInfo peerInfo;
    bool found = false;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it != peers_.end()) {
            peerInfo = it->second;
            found = true;
            
            if (peerInfo.isConnected() && networkManager_) {
                networkManager_->disconnect(peerInfo.getConnectionId());
            }
            
            peers_.erase(it);
        }
    }
    
    if (found && eventHandler_) {
        eventHandler_->onPeerDisconnected(peerInfo, "Peer removed");
    }
    
    return found;
}

bool PeerManager::connectToPeer(uint64_t peerId) {
    if (!running_ || !networkManager_) {
        return false;
    }
    
    PeerInfo peerInfo;
    bool found = false;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it != peers_.end()) {
            peerInfo = it->second;
            found = true;
            
            if (peerInfo.isConnected()) {
                return true;
            }
            
            it->second.setState(PeerState::CONNECTING);
        }
    }
    
    if (!found) {
        return false;
    }
    
    if (eventHandler_) {
        eventHandler_->onPeerStateChanged(peerInfo, PeerState::DISCONNECTED, PeerState::CONNECTING);
    }
    
    uint64_t connectionId = networkManager_->connect(peerInfo.getAddress(), peerInfo.getPort());
    
    if (connectionId > 0) {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it != peers_.end()) {
            it->second.setConnectionId(connectionId);
        }
        
        return true;
    }
    
    setPeerState(peerId, PeerState::DISCONNECTED);
    return false;
}

bool PeerManager::disconnectPeer(uint64_t peerId) {
    if (!running_ || !networkManager_) {
        return false;
    }
    
    uint64_t connectionId = 0;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it == peers_.end() || !it->second.isConnected()) {
            return false;
        }
        
        PeerState oldState = it->second.getState();
        it->second.setState(PeerState::DISCONNECTING);
        
        connectionId = it->second.getConnectionId();
        
        if (eventHandler_) {
            eventHandler_->onPeerStateChanged(it->second, oldState, PeerState::DISCONNECTING);
        }
    }
    
    if (connectionId > 0) {
        return networkManager_->disconnect(connectionId);
    }
    
    return false;
}

bool PeerManager::sendToPeer(uint64_t peerId, const NetworkMessage& message) {
    if (!running_ || !networkManager_) {
        return false;
    }
    
    uint64_t connectionId = 0;
    bool isConnected = false;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it != peers_.end() && it->second.isConnected()) {
            connectionId = it->second.getConnectionId();
            isConnected = true;
        }
    }
    
    if (isConnected && connectionId > 0) {
        return networkManager_->send(connectionId, message);
    }
    
    return false;
}

bool PeerManager::broadcast(const NetworkMessage& message, uint64_t excludePeerId) {
    if (!running_ || !networkManager_) {
        return false;
    }
    
    std::vector<uint64_t> connectionIds;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        
        for (const auto& pair : peers_) {
            if (pair.first != excludePeerId && 
                pair.second.isConnected() && 
                pair.second.getConnectionId() > 0) {
                connectionIds.push_back(pair.second.getConnectionId());
            }
        }
    }
    
    bool result = false;
    
    for (uint64_t connectionId : connectionIds) {
        if (networkManager_->send(connectionId, message)) {
            result = true;
        }
    }
    
    return result;
}

PeerInfo PeerManager::getPeerInfo(uint64_t peerId) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    auto it = peers_.find(peerId);
    
    if (it != peers_.end()) {
        return it->second;
    }
    
    return PeerInfo();
}

std::vector<PeerInfo> PeerManager::getAllPeers() const {
    std::vector<PeerInfo> result;
    
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    for (const auto& pair : peers_) {
        result.push_back(pair.second);
    }
    
    return result;
}

std::vector<PeerInfo> PeerManager::getConnectedPeers() const {
    std::vector<PeerInfo> result;
    
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    for (const auto& pair : peers_) {
        if (pair.second.isConnected()) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

bool PeerManager::hasPeer(uint64_t peerId) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peers_.find(peerId) != peers_.end();
}

size_t PeerManager::getPeerCount() const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    return peers_.size();
}

size_t PeerManager::getConnectedPeerCount() const {
    size_t count = 0;
    
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    for (const auto& pair : peers_) {
        if (pair.second.isConnected()) {
            ++count;
        }
    }
    
    return count;
}

void PeerManager::enablePeerDiscovery(bool enable) {
    peerDiscoveryEnabled_ = enable;
    
    if (running_) {
        if (enable) {
            startPeerDiscoveryProtocol();
        } else {
            stopPeerDiscoveryProtocol();
        }
    }
}

bool PeerManager::isPeerDiscoveryEnabled() const {
    return peerDiscoveryEnabled_;
}

void PeerManager::onConnect(uint64_t connectionId, const std::string& endpoint) {
    uint64_t peerId = findPeerByConnectionId(connectionId);
    
    if (peerId == 0) {
        std::string address;
        uint16_t port = 0;
        
        auto colonPos = endpoint.find_last_of(':');
        if (colonPos != std::string::npos) {
            address = endpoint.substr(0, colonPos);
            try {
                port = static_cast<uint16_t>(std::stoi(endpoint.substr(colonPos + 1)));
            } catch (...) {
                port = 0;
            }
        } else {
            address = endpoint;
        }
        
        peerId = addPeer(address, port);
        
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it != peers_.end()) {
            it->second.setConnectionId(connectionId);
            it->second.setState(PeerState::CONNECTED);
        }
    } else {
        setPeerState(peerId, PeerState::CONNECTED);
    }
    
    PeerInfo peerInfo = getPeerInfo(peerId);
    
    if (eventHandler_ && peerId > 0) {
        eventHandler_->onPeerConnected(peerInfo);
    }
    
    setPeerState(peerId, PeerState::HANDSHAKING);
    
    if (peerDiscoveryEnabled_) {
        sendPeerList(connectionId);
    }
}

void PeerManager::onDisconnect(uint64_t connectionId, const NetworkErrorInfo& reason) {
    uint64_t peerId = findPeerByConnectionId(connectionId);
    
    if (peerId > 0) {
        PeerInfo peerInfo = getPeerInfo(peerId);
        std::string reasonStr = reason.isError() ? reason.getMessage() : "Normal disconnect";
        
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            auto it = peers_.find(peerId);
            
            if (it != peers_.end()) {
                it->second.setState(PeerState::DISCONNECTED);
                it->second.setConnectionId(0);
            }
        }
        
        if (eventHandler_) {
            eventHandler_->onPeerDisconnected(peerInfo, reasonStr);
        }
    }
}

void PeerManager::onMessage(uint64_t connectionId, const NetworkMessage& message) {
    uint64_t peerId = findPeerByConnectionId(connectionId);
    
    if (peerId > 0) {
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            auto it = peers_.find(peerId);
            
            if (it != peers_.end()) {
                it->second.updateLastSeen();
                
                if (it->second.getState() == PeerState::HANDSHAKING) {
                    it->second.setState(PeerState::ACTIVE);
                    
                    if (eventHandler_) {
                        eventHandler_->onPeerStateChanged(it->second, PeerState::HANDSHAKING, PeerState::ACTIVE);
                    }
                }
            }
        }
        
        if (peerDiscoveryEnabled_) {
            handlePeerDiscoveryMessage(connectionId, message);
        }
        
        PeerInfo peerInfo = getPeerInfo(peerId);
        
        if (eventHandler_) {
            eventHandler_->onPeerMessage(peerInfo, message);
        }
    }
}

void PeerManager::onError(uint64_t connectionId, const NetworkErrorInfo& error) {
    if (connectionId > 0) {
        uint64_t peerId = findPeerByConnectionId(connectionId);
        
        if (peerId > 0 && error.isError()) {
            std::cerr << "Network error with peer " << peerId << ": " 
                      << error.getMessage() << std::endl;
        }
    }
}

bool PeerManager::initializeNetworkManager() {
    if (!networkManager_) {
        networkManager_ = std::make_shared<NetworkManager>();
        ownNetworkManager_ = true;
    }
    
    networkManager_->setEventHandler(this);
    
    if (listenPort_ > 0) {
        return networkManager_->startListening(listenPort_);
    }
    
    return true;
}

uint64_t PeerManager::generatePeerId() {
    std::lock_guard<std::mutex> lock(peerIdMutex_);
    return nextPeerId_++;
}

void PeerManager::setPeerState(uint64_t peerId, PeerState state) {
    PeerInfo peerInfo;
    PeerState oldState = PeerState::DISCONNECTED;
    bool stateChanged = false;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        auto it = peers_.find(peerId);
        
        if (it != peers_.end()) {
            oldState = it->second.getState();
            
            if (oldState != state) {
                it->second.setState(state);
                peerInfo = it->second;
                stateChanged = true;
            }
        }
    }
    
    if (stateChanged && eventHandler_) {
        eventHandler_->onPeerStateChanged(peerInfo, oldState, state);
    }
}

uint64_t PeerManager::findPeerByConnectionId(uint64_t connectionId) const {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    for (const auto& pair : peers_) {
        if (pair.second.getConnectionId() == connectionId) {
            return pair.first;
        }
    }
    
    return 0;
}

void PeerManager::startPeerDiscoveryProtocol() {
    if (!running_ || !networkManager_ || discoveryRunning_ || !peerDiscoveryEnabled_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(discoveryMutex_);
    
    if (!taskScheduler_) {
        taskScheduler_ = std::make_shared<TaskScheduler>();
        taskScheduler_->start();
    }
    
    discoveryTaskId_ = taskScheduler_->scheduleRecurring(
        [this]() {
            if (running_ && networkManager_ && peerDiscoveryEnabled_) {
                broadcastDiscoveryAnnouncement();
                cleanupDiscoveryCache();
            }
        },
        std::chrono::milliseconds(500),
        std::chrono::duration_cast<std::chrono::milliseconds>(announcementInterval_)
    );
    
    discoveryRunning_ = true;
    std::cout << "Protocole de découverte de pairs démarré." << std::endl;
}

void PeerManager::stopPeerDiscoveryProtocol() {
    if (!discoveryRunning_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(discoveryMutex_);
    
    if (taskScheduler_ && discoveryTaskId_ > 0) {
        taskScheduler_->cancelScheduledTask(discoveryTaskId_);
        discoveryTaskId_ = 0;
    }
    
    discoveryRunning_ = false;
    std::cout << "Protocole de découverte de pairs arrêté." << std::endl;
}

void PeerManager::handlePeerDiscoveryMessage(uint64_t connectionId, const NetworkMessage& message) {
    if (!peerDiscoveryEnabled_ || !running_) {
        return;
    }
    
    try {
        PeerDiscoveryMessage discoveryMsg;
        std::vector<uint8_t> data = message.getData();
        
        try {
            std::string str(data.begin(), data.end());
            std::istringstream iss(str);
            boost::archive::binary_iarchive ia(iss);
            ia >> discoveryMsg;
        } catch (const std::exception& e) {
            std::cerr << "Erreur de désérialisation du message de découverte: " << e.what() << std::endl;
            return;
        }
        
        {
            std::lock_guard<std::mutex> lock(discoveryMutex_);
            
            auto now = std::chrono::system_clock::now();
            auto it = recentDiscoveryMessages_.find(discoveryMsg.senderId);
            
            if (it != recentDiscoveryMessages_.end()) {
                if (now - it->second < DISCOVERY_CACHE_TTL) {
                    return;
                }
            }
            
            recentDiscoveryMessages_[discoveryMsg.senderId] = now;
        }
        
        switch (discoveryMsg.type) {
            case PeerDiscoveryMessageType::PEER_REQUEST:
                handlePeerRequest(connectionId, discoveryMsg);
                break;
                
            case PeerDiscoveryMessageType::PEER_RESPONSE:
                handlePeerResponse(discoveryMsg);
                break;
                
            case PeerDiscoveryMessageType::PEER_ANNOUNCE:
                handlePeerAnnouncement(discoveryMsg);
                break;
                
            case PeerDiscoveryMessageType::PEER_KEEPALIVE:
                updatePeerLastSeen(discoveryMsg.senderId);
                break;
                
            default:
                std::cerr << "Type de message de découverte inconnu: " 
                          << static_cast<int>(discoveryMsg.type) << std::endl;
                break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Erreur lors du traitement du message de découverte: " << e.what() << std::endl;
    }
}

void PeerManager::sendPeerList(uint64_t connectionId) {
    if (!peerDiscoveryEnabled_ || !running_ || !networkManager_) {
        return;
    }
    
    try {
        PeerDiscoveryMessage response(PeerDiscoveryMessageType::PEER_RESPONSE, 
                                     generatePeerId(),
                                     "",
                                     listenPort_);
        
        if (networkManager_->isListening()) {
            response.senderEndpoint = "127.0.0.1";
        }
        
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            
            for (const auto& pair : peers_) {
                if (pair.first != findPeerByConnectionId(connectionId)) {
                    response.peersList.push_back(std::make_pair(
                        pair.second.getAddress(),
                        pair.second.getPort()
                    ));
                }
            }
        }
        
        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << response;
        
        std::string serialized = oss.str();
        std::vector<uint8_t> data(serialized.begin(), serialized.end());
        
        NetworkMessage netMsg(data);
        networkManager_->send(connectionId, netMsg);
        
        std::cout << "Liste de pairs envoyée (" << response.peersList.size() 
                 << " pairs) au pair avec connectionId " << connectionId << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Erreur lors de l'envoi de la liste de pairs: " << e.what() << std::endl;
    }
}

void PeerManager::broadcastDiscoveryAnnouncement() {
    if (!peerDiscoveryEnabled_ || !running_ || !networkManager_) {
        return;
    }
    
    try {
        PeerDiscoveryMessage announcement(PeerDiscoveryMessageType::PEER_ANNOUNCE, 
                                         generatePeerId(),
                                         "",
                                         listenPort_);
        
        if (networkManager_->isListening()) {
            announcement.senderEndpoint = "127.0.0.1";
        }
        
        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << announcement;
        
        std::string serialized = oss.str();
        std::vector<uint8_t> data(serialized.begin(), serialized.end());
        
        NetworkMessage netMsg(data);
        networkManager_->broadcast(netMsg);
        
        std::cout << "Annonce de découverte diffusée à tous les pairs" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Erreur lors de la diffusion de l'annonce de découverte: " << e.what() << std::endl;
    }
}

void PeerManager::cleanupDiscoveryCache() {
    std::lock_guard<std::mutex> lock(discoveryMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto it = recentDiscoveryMessages_.begin();
    
    while (it != recentDiscoveryMessages_.end()) {
        if (now - it->second > DISCOVERY_CACHE_TTL) {
            it = recentDiscoveryMessages_.erase(it);
        } else {
            ++it;
        }
    }
}

void PeerManager::handlePeerRequest(uint64_t connectionId, const PeerDiscoveryMessage& message) {
    sendPeerList(connectionId);
}

void PeerManager::handlePeerResponse(const PeerDiscoveryMessage& message) {
    for (const auto& peerInfo : message.peersList) {
        const std::string& address = peerInfo.first;
        uint16_t port = peerInfo.second;
        
        bool peerExists = false;
        
        {
            std::lock_guard<std::mutex> lock(peersMutex_);
            
            for (const auto& pair : peers_) {
                if (pair.second.getAddress() == address && pair.second.getPort() == port) {
                    peerExists = true;
                    break;
                }
            }
        }
        
        if (!peerExists) {
            uint64_t peerId = addPeer(address, port);
            
            std::cout << "Nouveau pair découvert et ajouté: " << address 
                     << ":" << port << " (Id: " << peerId << ")" << std::endl;
        }
    }
}

void PeerManager::handlePeerAnnouncement(const PeerDiscoveryMessage& message) {
    bool peerExists = false;
    uint64_t existingPeerId = 0;
    
    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        
        for (const auto& pair : peers_) {
            if (pair.second.getAddress() == message.senderEndpoint && 
                pair.second.getPort() == message.senderPort) {
                peerExists = true;
                existingPeerId = pair.first;
                break;
            }
        }
    }
    
    if (!peerExists) {
        uint64_t peerId = addPeer(message.senderEndpoint, message.senderPort);
        
        std::cout << "Pair annoncé découvert et ajouté: " << message.senderEndpoint 
                 << ":" << message.senderPort << " (Id: " << peerId << ")" << std::endl;
    } else {
        updatePeerLastSeen(existingPeerId);
    }
}

void PeerManager::updatePeerLastSeen(uint64_t peerId) {
    std::lock_guard<std::mutex> lock(peersMutex_);
    
    auto it = peers_.find(peerId);
    if (it != peers_.end()) {
        it->second.updateLastSeen();
    }
}

} 