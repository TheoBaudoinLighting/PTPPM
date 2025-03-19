#pragma once
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

class IUserInterface;
class NetworkManager;

class UIManager {
public:
    explicit UIManager(NetworkManager& networkManager);
    ~UIManager();
    bool initialize(int windowWidth, int windowHeight, const std::string& windowTitle);
    void run(std::atomic<bool>& running);
    void cleanup();
    
private:
    bool onServerStart(unsigned short port);
    void onServerStop();
    bool onServerStatus();
    std::vector<std::string> onServerLogs();
    
    bool onClientConnect(const std::string& serverIp, unsigned short serverPort);
    void onClientDisconnect();
    bool onClientSend(const std::string& message);
    bool onClientStatus();
    std::vector<std::string> onClientMessages();
    
    std::unique_ptr<IUserInterface> ui_;
    NetworkManager& networkManager_;
    std::atomic<bool>* runningPtr_;
};