#include "network_server.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>
#include <iomanip>

NetworkServer::NetworkServer(unsigned short port)
    : networkManager_(std::make_shared<wrap_boost::NetworkManager>()),
      port_(port),
      isRunning_(false) {
    
    networkManager_->setEventHandler(this);
}

NetworkServer::~NetworkServer() {
    stop();
}

bool NetworkServer::start(std::atomic<bool>& running) {
    try {
        if (isRunning_) {
            spdlog::warn("Le serveur est déjà en cours d'exécution");
            return false;
        }
        
        if (networkManager_->startListening(port_)) {
            isRunning_ = true;
            addLog("Serveur démarré sur le port " + std::to_string(port_));
            spdlog::info("Serveur démarré sur le port {}", port_);
            return true;
        }
        
        spdlog::error("Échec du démarrage du serveur sur le port {}", port_);
        return false;
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors du démarrage du serveur: {}", e.what());
        return false;
    }
}

void NetworkServer::stop() {
    if (!isRunning_) {
        return;
    }
    
    try {
        networkManager_->stopListening();
        networkManager_->disconnectAll();
        
        isRunning_ = false;
        addLog("Serveur arrêté");
        spdlog::info("Serveur arrêté");
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors de l'arrêt du serveur: {}", e.what());
    }
}

bool NetworkServer::isRunning() const {
    return isRunning_;
}

unsigned short NetworkServer::getPort() const {
    return port_;
}

std::vector<std::string> NetworkServer::getConnectionLogs() {
    std::lock_guard<std::mutex> lock(logsMutex_);
    return std::vector<std::string>(connectionLogs_.begin(), connectionLogs_.end());
}

void NetworkServer::setConnectionCallback(const std::function<void(const std::string&)>& callback) {
    connectionCallback_ = callback;
}

void NetworkServer::setMessageCallback(const std::function<void(const std::string&, const std::string&)>& callback) {
    messageCallback_ = callback;
}

void NetworkServer::onConnect(uint64_t connectionId, const std::string& endpoint) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clientEndpoints_[connectionId] = endpoint;
    }
    
    addLog("Nouvelle connexion de " + endpoint);
    spdlog::info("Nouvelle connexion: {} (ID: {})", endpoint, connectionId);
    
    if (connectionCallback_) {
        try {
            connectionCallback_(endpoint);
        }
        catch (const std::exception& e) {
            spdlog::error("Exception dans le callback de connexion: {}", e.what());
        }
    }
}

void NetworkServer::onDisconnect(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& reason) {
    std::string endpoint;
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clientEndpoints_.find(connectionId);
        if (it != clientEndpoints_.end()) {
            endpoint = it->second;
            clientEndpoints_.erase(it);
        }
    }
    
    if (!endpoint.empty()) {
        std::string message = "Déconnexion de " + endpoint;
        if (reason.isError()) {
            message += ": " + reason.getMessage();
        }
        
        addLog(message);
        spdlog::info(message);
    }
}

void NetworkServer::onMessage(uint64_t connectionId, const wrap_boost::NetworkMessage& message) {
    std::string endpoint;
    
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clientEndpoints_.find(connectionId);
        if (it != clientEndpoints_.end()) {
            endpoint = it->second;
        }
    }
    
    if (!endpoint.empty()) {
        std::string receivedData = message.toString();
        
        addLog("Message de " + endpoint + ": " + receivedData);
        spdlog::debug("Message reçu de {}: {}", endpoint, receivedData);
        
        std::string echoMessage = "Echo: " + receivedData;
        
        wrap_boost::NetworkMessage echoNetMsg(echoMessage);
        networkManager_->send(connectionId, echoNetMsg);
        
        if (messageCallback_) {
            try {
                messageCallback_(endpoint, receivedData);
            }
            catch (const std::exception& e) {
                spdlog::error("Exception dans le callback de message: {}", e.what());
            }
        }
    }
}

void NetworkServer::onError(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& error) {
    std::string endpoint = "inconnu";
    
    if (connectionId > 0) {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clientEndpoints_.find(connectionId);
        if (it != clientEndpoints_.end()) {
            endpoint = it->second;
        }
    }
    
    std::string errorMessage = "Erreur pour " + endpoint + ": " + error.getMessage();
    addLog(errorMessage);
    spdlog::error(errorMessage);
}

void NetworkServer::addLog(const std::string& message) {
    try {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::tm tm;
#if defined(_WIN32)
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        
        std::stringstream logEntry;
        logEntry << std::put_time(&tm, "[%H:%M:%S] ") << message;
        
        std::lock_guard<std::mutex> lock(logsMutex_);
        connectionLogs_.push_back(logEntry.str());
        if (connectionLogs_.size() > maxLogSize_) {
            connectionLogs_.pop_front();
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Erreur lors de l'ajout d'un log: {}", e.what());
    }
}