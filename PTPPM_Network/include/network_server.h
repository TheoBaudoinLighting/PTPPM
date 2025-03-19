#pragma once
#include "inetwork_server.h"
#include "boost/wrap_boost_network.h"
#include <memory>
#include <mutex>
#include <deque>
#include <atomic>
#include <functional>
#include <map>

class NetworkServer : public INetworkServer, private wrap_boost::INetworkEventHandler {
public:
    explicit NetworkServer(unsigned short port);
    ~NetworkServer() override;
    
    bool start(std::atomic<bool>& running) override;
    void stop() override;
    bool isRunning() const override;
    unsigned short getPort() const override;
    std::vector<std::string> getConnectionLogs() override;
    void setConnectionCallback(const std::function<void(const std::string&)>& callback) override;
    void setMessageCallback(const std::function<void(const std::string&, const std::string&)>& callback) override;
    
    void onConnect(uint64_t connectionId, const std::string& endpoint) override;
    void onDisconnect(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& reason) override;
    void onMessage(uint64_t connectionId, const wrap_boost::NetworkMessage& message) override;
    void onError(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& error) override;
    
private:
    void addLog(const std::string& message);
    
    std::shared_ptr<wrap_boost::NetworkManager> networkManager_;
    unsigned short port_;
    bool isRunning_;
    
    std::deque<std::string> connectionLogs_;
    std::mutex logsMutex_;
    const size_t maxLogSize_ = 100;
    
    std::map<uint64_t, std::string> clientEndpoints_;
    std::mutex clientsMutex_;
    
    std::function<void(const std::string&)> connectionCallback_;
    std::function<void(const std::string&, const std::string&)> messageCallback_;
};