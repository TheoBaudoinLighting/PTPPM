#pragma once
#include <string>
#include <vector>
#include <functional>

class INetworkClient {
public:
    virtual ~INetworkClient() = default;
    
    virtual bool connect(const std::string& serverIp, unsigned short serverPort) = 0;
    virtual void disconnect() = 0;
    virtual bool sendMessage(const std::string& message) = 0;
    virtual bool isConnected() const = 0;
    virtual std::vector<std::string> getReceivedMessages() = 0;
    virtual void setMessageCallback(const std::function<void(const std::string&)>& callback) = 0;
    virtual void setConnectionStatusCallback(const std::function<void(bool, const std::string&)>& callback) = 0;
};