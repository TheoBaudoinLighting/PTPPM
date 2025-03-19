#include "network_manager.h"
#include "inetwork_server.h"
#include "inetwork_client.h"
#include "network_server.h"
#include "network_client.h"
#include <spdlog/spdlog.h>

NetworkManager::NetworkManager() {
    server_ = std::make_unique<NetworkServer>(0);
    client_ = std::make_unique<NetworkClient>();
    
    spdlog::debug("NetworkManager: Initialisation des composants réseau");
}

NetworkManager::~NetworkManager() {
    stopServer();
    disconnectClient();
    
    spdlog::debug("NetworkManager: Destruction des composants réseau");
}

bool NetworkManager::startServer(unsigned short port, std::atomic<bool>& running) {
    try {
        stopServer();
        server_ = std::make_unique<NetworkServer>(port);
        
        spdlog::info("NetworkManager: Démarrage du serveur sur le port {}", port);
        return server_->start(running);
    }
    catch (const std::exception& e) {
        spdlog::error("NetworkManager: Erreur lors du démarrage du serveur: {}", e.what());
        return false;
    }
}

void NetworkManager::stopServer() {
    if (server_ && server_->isRunning()) {
        spdlog::info("NetworkManager: Arrêt du serveur");
        server_->stop();
    }
}

bool NetworkManager::isServerRunning() const {
    return server_ && server_->isRunning();
}

std::vector<std::string> NetworkManager::getServerLogs() {
    if (!server_) {
        return {};
    }
    
    return server_->getConnectionLogs();
}

void NetworkManager::setServerConnectionCallback(const std::function<void(const std::string&)>& callback) {
    if (server_) {
        server_->setConnectionCallback(callback);
    }
}

void NetworkManager::setServerMessageCallback(const std::function<void(const std::string&, const std::string&)>& callback) {
    if (server_) {
        server_->setMessageCallback(callback);
    }
}

bool NetworkManager::connectClient(const std::string& serverIp, unsigned short serverPort) {
    if (!client_) {
        return false;
    }
    
    spdlog::info("NetworkManager: Tentative de connexion client à {}:{}", serverIp, serverPort);
    return client_->connect(serverIp, serverPort);
}

void NetworkManager::disconnectClient() {
    if (client_ && client_->isConnected()) {
        spdlog::info("NetworkManager: Déconnexion du client");
        client_->disconnect();
    }
}

bool NetworkManager::sendClientMessage(const std::string& message) {
    if (!client_ || !client_->isConnected()) {
        spdlog::warn("NetworkManager: Tentative d'envoi de message sans connexion active");
        return false;
    }
    
    spdlog::debug("NetworkManager: Envoi du message: {}", message);
    return client_->sendMessage(message);
}

bool NetworkManager::isClientConnected() const {
    return client_ && client_->isConnected();
}

std::vector<std::string> NetworkManager::getClientMessages() {
    if (!client_) {
        return {};
    }
    
    return client_->getReceivedMessages();
}

void NetworkManager::setClientMessageCallback(const std::function<void(const std::string&)>& callback) {
    if (client_) {
        client_->setMessageCallback(callback);
    }
}

void NetworkManager::setClientConnectionStatusCallback(const std::function<void(bool, const std::string&)>& callback) {
    if (client_) {
        client_->setConnectionStatusCallback(callback);
    }
}