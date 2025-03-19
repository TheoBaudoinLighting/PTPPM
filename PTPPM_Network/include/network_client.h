#pragma once
#include "inetwork_client.h"
#include "boost/wrap_boost_network.h"
#include <string>
#include <vector>
#include <deque>
#include <mutex>

class NetworkClient : public INetworkClient, private wrap_boost::INetworkEventHandler {
public:
    NetworkClient();
    ~NetworkClient() override;
    
    bool connect(const std::string& serverIp, unsigned short serverPort) override;
    void disconnect() override;
    bool sendMessage(const std::string& message) override;
    bool isConnected() const override;
    std::vector<std::string> getReceivedMessages() override;
    void setMessageCallback(const std::function<void(const std::string&)>& callback) override;
    void setConnectionStatusCallback(const std::function<void(bool, const std::string&)>& callback) override;
    
    void onConnect(uint64_t connectionId, const std::string& endpoint) override;
    void onDisconnect(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& reason) override;
    void onMessage(uint64_t connectionId, const wrap_boost::NetworkMessage& message) override;
    void onError(uint64_t connectionId, const wrap_boost::NetworkErrorInfo& error) override;
    
private:
    void addReceivedMessage(const std::string& message);
    
    std::shared_ptr<wrap_boost::NetworkManager> networkManager_;
    uint64_t connectionId_;
    bool connected_;
    std::string currentServer_;
    unsigned short currentPort_;
    
    std::deque<std::string> receivedMessages_;
    std::mutex messagesMutex_;
    const size_t maxMessages_ = 100;
    
    std::function<void(const std::string&)> messageCallback_;
    std::function<void(bool, const std::string&)> connectionStatusCallback_;
};