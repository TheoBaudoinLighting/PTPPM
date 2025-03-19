#pragma once
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <functional>

class INetworkServer;
class INetworkClient;

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    bool startServer(unsigned short port, std::atomic<bool>& running);
    void stopServer();
    bool isServerRunning() const;
    std::vector<std::string> getServerLogs();
    void setServerConnectionCallback(const std::function<void(const std::string&)>& callback);
    void setServerMessageCallback(const std::function<void(const std::string&, const std::string&)>& callback);
    
    bool connectClient(const std::string& serverIp, unsigned short serverPort);
    void disconnectClient();
    bool sendClientMessage(const std::string& message);
    bool isClientConnected() const;
    std::vector<std::string> getClientMessages();
    void setClientMessageCallback(const std::function<void(const std::string&)>& callback);
    void setClientConnectionStatusCallback(const std::function<void(bool, const std::string&)>& callback);
    
private:
    std::unique_ptr<INetworkServer> server_;
    std::unique_ptr<INetworkClient> client_;
};