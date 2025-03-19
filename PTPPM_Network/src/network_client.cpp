#include "network_client.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>
#include <iomanip>

NetworkClient::NetworkClient()
    : networkManager_(std::make_shared<wrap_boost::NetworkManager>()),
      connectionId_(0),
      connected_(false),
      currentPort_(0) {
    
    networkManager_->setEventHandler(this);
}

NetworkClient::~NetworkClient() {
    disconnect();
}

bool NetworkClient::connect(const std::string& serverIp, unsigned short serverPort) {
    if (connected_) {
        disconnect();
    }
    
    spdlog::info("Tentative de connexion à {}:{}", serverIp, serverPort);
    addReceivedMessage("Tentative de connexion à " + serverIp + ":" + std::to_string(serverPort));
    
    connectionId_ = networkManager_->connect(serverIp, serverPort);
    
    if (connectionId_ > 0) {
        currentServer_ = serverIp;
        currentPort_ = serverPort;
        return true;
    }
    
    addReceivedMessage("Échec de la connexion à " + serverIp + ":" + std::to_string(serverPort));
    if (connectionStatusCallback_) {
        connectionStatusCallback_(false, "Échec de la connexion");
    }
    return false;
}

void NetworkClient::disconnect() {
    if (!connected_ && connectionId_ == 0) {
        return;
    }
    
    if (networkManager_) {
        if (connectionId_ > 0) {
            networkManager_->disconnect(connectionId_);
            connectionId_ = 0;
        }
    }
    
    if (connected_) {
        connected_ = false;
        addReceivedMessage("Déconnecté du serveur");
        if (connectionStatusCallback_) {
            connectionStatusCallback_(false, "Déconnecté du serveur");
        }
    }
}

bool NetworkClient::sendMessage(const std::string& message) {
    if (!connected_ || connectionId_ == 0) {
        spdlog::error("Tentative d'envoi de message sans être connecté");
        return false;
    }
    
    std::vector<uint8_t> data(message.begin(), message.end());
    wrap_boost::NetworkMessage netMsg(data);
    
    bool result = networkManager_->send(connectionId_, netMsg);
    
    if (result) {
        addReceivedMessage("Envoyé: " + message);
        spdlog::debug("Message envoyé: {}", message);
    } else {
        addReceivedMessage("Erreur d'envoi: " + message);
        spdlog::error("Erreur lors de l'envoi du message: {}", message);
    }
    
    return result;
}

bool NetworkClient::isConnected() const {
    return connected_;
}

std::vector<std::string> NetworkClient::getReceivedMessages() {
    std::lock_guard<std::mutex> lock(messagesMutex_);
    return std::vector<std::string>(receivedMessages_.begin(), receivedMessages_.end());
}

void NetworkClient::setMessageCallback(const std::function<void(const std::string&)>& callback) {
    messageCallback_ = callback;
}

void NetworkClient::setConnectionStatusCallback(const std::function<void(bool, const std::string&)>& callback) {
    connectionStatusCallback_ = callback;
}

void NetworkClient::onConnect(uint64_t connectionId, const std::string& endpoint) {
    if (connectionId == connectionId_) {
        connected_ = true;
        addReceivedMessage("Connecté au serveur " + endpoint);
        if (connectionStatusCallback_) {
            connectionStatusCallback_(true, "Connecté au serveur " + endpoint);
        }
        spdlog::info("Connecté au serveur: {}", endpoint);
    }
}

void NetworkClient::onDisconnect(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& reason) {
    if (connectionId == connectionId_) {
        connected_ = false;
        connectionId_ = 0;
        
        std::string message = "Déconnecté du serveur";
        if (reason.isError()) {
            message += ": " + reason.getMessage();
        }
        
        addReceivedMessage(message);
        if (connectionStatusCallback_) {
            connectionStatusCallback_(false, message);
        }
        spdlog::info(message);
    }
}

void NetworkClient::onMessage(uint64_t connectionId, const wrap_boost::NetworkMessage& message) {
    if (connectionId == connectionId_) {
        const std::vector<uint8_t>& data = message.getData();
        std::string receivedData(data.begin(), data.end());
        
        addReceivedMessage("Reçu: " + receivedData);
        spdlog::debug("Message reçu: {}", receivedData);
        
        if (messageCallback_) {
            try {
                messageCallback_(receivedData);
            } catch (const std::exception& e) {
                spdlog::error("Exception dans le callback de message: {}", e.what());
            }
        }
    }
}

void NetworkClient::onError(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& error) {
    if (connectionId == connectionId_ || connectionId == 0) {
        std::string errorMessage = "Erreur de connexion: " + error.getMessage();
        addReceivedMessage(errorMessage);
        spdlog::error(errorMessage);
        
        if (connectionStatusCallback_) {
            connectionStatusCallback_(false, errorMessage);
        }
    }
}

void NetworkClient::addReceivedMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(messagesMutex_);
    
    auto now = std::chrono::system_clock::now();
    auto nowTimeT = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    std::tm tmBuf;
    
#if defined(_WIN32)
    localtime_s(&tmBuf, &nowTimeT);
#else
    localtime_r(&nowTimeT, &tmBuf);
#endif
    
    ss << "[" << std::put_time(&tmBuf, "%H:%M:%S") << "] " << message;
    receivedMessages_.push_front(ss.str());
    
    if (receivedMessages_.size() > maxMessages_) {
        receivedMessages_.pop_back();
    }
}