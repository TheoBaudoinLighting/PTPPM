#pragma once
#include <string>
#include <vector>
#include <functional>
#include <atomic>

class IUserInterface {
public:
    virtual ~IUserInterface() = default;
    
    virtual bool initialize() = 0;
    
    virtual void run(std::atomic<bool>& running) = 0;
    
    virtual void cleanup() = 0;
    
    virtual void setServerStartCallback(const std::function<bool(unsigned short)>& callback) = 0;
    
    virtual void setServerStopCallback(const std::function<void()>& callback) = 0;
    
    virtual void setServerStatusCallback(const std::function<bool()>& callback) = 0;
    
    virtual void setServerLogsCallback(const std::function<std::vector<std::string>()>& callback) = 0;
    
    virtual void setClientConnectCallback(const std::function<bool(const std::string&, unsigned short)>& callback) = 0;
    
    virtual void setClientDisconnectCallback(const std::function<void()>& callback) = 0;
    
    virtual void setClientSendCallback(const std::function<bool(const std::string&)>& callback) = 0;
    
    virtual void setClientStatusCallback(const std::function<bool()>& callback) = 0;
    
    virtual void setClientMessagesCallback(const std::function<std::vector<std::string>()>& callback) = 0;
};