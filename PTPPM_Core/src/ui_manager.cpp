#include "ui_manager.h"
#include "iuser_interface.h"
#include "user_interface.h"
#include "network_manager.h"
#include <spdlog/spdlog.h>
#include <functional>

UIManager::UIManager(NetworkManager& networkManager)
    : networkManager_(networkManager), runningPtr_(nullptr) {
    spdlog::debug("UIManager: Initialisation");
}

UIManager::~UIManager() {
    cleanup();
    spdlog::debug("UIManager: Destruction");
}

bool UIManager::initialize(int windowWidth, int windowHeight, const std::string& windowTitle) {
    try {
        ui_ = std::make_unique<UserInterface>(windowWidth, windowHeight, windowTitle);
        
        if (!ui_->initialize()) {
            spdlog::error("UIManager: Échec de l'initialisation de l'interface utilisateur");
            return false;
        }
        
        ui_->setServerStartCallback(std::bind(&UIManager::onServerStart, this, std::placeholders::_1));
        ui_->setServerStopCallback(std::bind(&UIManager::onServerStop, this));
        ui_->setServerStatusCallback(std::bind(&UIManager::onServerStatus, this));
        ui_->setServerLogsCallback(std::bind(&UIManager::onServerLogs, this));
        
        ui_->setClientConnectCallback(std::bind(&UIManager::onClientConnect, this, 
                                               std::placeholders::_1, std::placeholders::_2));
        ui_->setClientDisconnectCallback(std::bind(&UIManager::onClientDisconnect, this));
        ui_->setClientSendCallback(std::bind(&UIManager::onClientSend, this, std::placeholders::_1));
        ui_->setClientStatusCallback(std::bind(&UIManager::onClientStatus, this));
        ui_->setClientMessagesCallback(std::bind(&UIManager::onClientMessages, this));
        
        spdlog::info("UIManager: Interface utilisateur initialisée avec succès");
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("UIManager: Erreur lors de l'initialisation de l'interface: {}", e.what());
        return false;
    }
}

void UIManager::run(std::atomic<bool>& running) {
    if (!ui_) {
        spdlog::error("UIManager: Tentative d'exécution de l'interface non initialisée");
        return;
    }
    
    runningPtr_ = &running;
    spdlog::info("UIManager: Démarrage de l'interface utilisateur");
    ui_->run(running);
}

void UIManager::cleanup() {
    if (ui_) {
        spdlog::info("UIManager: Nettoyage de l'interface utilisateur");
        ui_->cleanup();
        ui_.reset();
    }
}

bool UIManager::onServerStart(unsigned short port) {
    spdlog::info("UIManager: Demande de démarrage du serveur sur le port {}", port);
    
    if (!runningPtr_) {
        spdlog::error("UIManager: Pointeur running non initialisé");
        return false;
    }
    
    return networkManager_.startServer(port, *runningPtr_);
}

void UIManager::onServerStop() {
    spdlog::info("UIManager: Demande d'arrêt du serveur");
    networkManager_.stopServer();
}

bool UIManager::onServerStatus() {
    return networkManager_.isServerRunning();
}

std::vector<std::string> UIManager::onServerLogs() {
    return networkManager_.getServerLogs();
}

bool UIManager::onClientConnect(const std::string& serverIp, unsigned short serverPort) {
    spdlog::info("UIManager: Demande de connexion client à {}:{}", serverIp, serverPort);
    return networkManager_.connectClient(serverIp, serverPort);
}

void UIManager::onClientDisconnect() {
    spdlog::info("UIManager: Demande de déconnexion client");
    networkManager_.disconnectClient();
}

bool UIManager::onClientSend(const std::string& message) {
    spdlog::debug("UIManager: Demande d'envoi de message: {}", message);
    return networkManager_.sendClientMessage(message);
}

bool UIManager::onClientStatus() {
    return networkManager_.isClientConnected();
}

std::vector<std::string> UIManager::onClientMessages() {
    return networkManager_.getClientMessages();
}