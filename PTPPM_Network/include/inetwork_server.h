#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <functional>

class INetworkServer {
public:
    virtual ~INetworkServer() = default;
    
    virtual bool start(std::atomic<bool>& running) = 0;
    
    virtual void stop() = 0;
    
    virtual bool isRunning() const = 0;
    
    virtual unsigned short getPort() const = 0;
    
    virtual std::vector<std::string> getConnectionLogs() = 0;
    
    virtual void setConnectionCallback(const std::function<void(const std::string&)>& callback) = 0;
    
    virtual void setMessageCallback(const std::function<void(const std::string&, const std::string&)>& callback) = 0;
};