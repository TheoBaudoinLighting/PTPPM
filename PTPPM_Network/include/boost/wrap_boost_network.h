#ifndef WRAP_BOOST_NETWORK_H
#define WRAP_BOOST_NETWORK_H

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include "dynamic_buffer.h"

namespace wrap_boost {

enum class NetworkError {
    NONE,
    CONNECTION_FAILED,
    CONNECTION_CLOSED,
    READ_ERROR,
    WRITE_ERROR,
    BIND_ERROR,
    LISTEN_ERROR,
    ACCEPT_ERROR,
    RESOLVE_ERROR,
    TIMEOUT
};

class NetworkErrorInfo {
public:
    NetworkErrorInfo();
    NetworkErrorInfo(NetworkError code, const std::string& message);
    
    NetworkError getCode() const;
    const std::string& getMessage() const;
    bool isError() const;
    
private:
    NetworkError code_;
    std::string message_;
};

class NetworkMessage {
public:
    NetworkMessage();
    NetworkMessage(const std::vector<uint8_t>& data);
    NetworkMessage(std::vector<uint8_t>&& data);
    NetworkMessage(const DynamicBuffer& buffer);
    NetworkMessage(std::shared_ptr<DynamicBuffer> buffer);
    NetworkMessage(const void* data, size_t length);
    NetworkMessage(const std::string& data);
    
    std::shared_ptr<DynamicBuffer> getBuffer() const;
    
    const uint8_t* data() const;
    
    std::vector<uint8_t> getData() const;
    
    size_t size() const;
    bool empty() const;
    
    void append(const void* data, size_t length);
    void append(const std::vector<uint8_t>& data);
    void append(const std::string& data);
    void append(const NetworkMessage& other);
    
    std::vector<uint8_t> toVector() const;
    std::string toString() const;

private:
    std::shared_ptr<DynamicBuffer> buffer_;
};

class INetworkEventHandler {
public:
    virtual ~INetworkEventHandler() = default;
    
    virtual void onConnect(uint64_t connectionId, const std::string& endpoint) = 0;
    virtual void onDisconnect(uint64_t connectionId, const NetworkErrorInfo& reason) = 0;
    virtual void onMessage(uint64_t connectionId, const NetworkMessage& message) = 0;
    virtual void onError(uint64_t connectionId, const NetworkErrorInfo& error) = 0;
};

class INetworkConnection {
public:
    virtual ~INetworkConnection() = default;
    
    virtual uint64_t getId() const = 0;
    virtual std::string getRemoteEndpoint() const = 0;
    virtual bool isConnected() const = 0;
    
    virtual void send(const NetworkMessage& message) = 0;
    virtual void send(NetworkMessage&& message) = 0;
    virtual void close() = 0;
};

class NetworkConnection : public INetworkConnection {
public:
    using Pointer = std::shared_ptr<NetworkConnection>;
    
    static Pointer create(boost::asio::io_context& ioContext, uint64_t id, 
                          INetworkEventHandler* handler);
    
    virtual uint64_t getId() const override;
    virtual std::string getRemoteEndpoint() const override;
    virtual bool isConnected() const override;
    virtual void send(const NetworkMessage& message) override;
    virtual void send(NetworkMessage&& message) override;
    virtual void close() override;
    
    boost::asio::ip::tcp::socket& getSocket();
    void start();
    
private:
    NetworkConnection(boost::asio::io_context& ioContext, uint64_t id, 
                     INetworkEventHandler* handler);
    
    void startRead();
    void handleRead(const boost::system::error_code& error, size_t bytesTransferred);
    void startWrite();
    void handleWrite(const boost::system::error_code& error, size_t bytesTransferred);
    
    boost::asio::ip::tcp::socket socket_;
    uint64_t id_;
    INetworkEventHandler* handler_;
    std::atomic<bool> connected_;
    std::string remoteEndpoint_;
    
    std::shared_ptr<DynamicBuffer> readBuffer_;
    std::queue<NetworkMessage> writeQueue_;
    std::mutex writeMutex_;
    
    static const size_t INITIAL_BUFFER_SIZE = 65536;
};

class NetworkManager : public INetworkEventHandler {
public:
    NetworkManager();
    ~NetworkManager();
    
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    
    void setEventHandler(INetworkEventHandler* handler);
    
    bool startListening(uint16_t port);
    bool stopListening();
    uint64_t connect(const std::string& host, uint16_t port);
    bool disconnect(uint64_t connectionId);
    void disconnectAll();
    
    bool send(uint64_t connectionId, const NetworkMessage& message);
    bool broadcast(const NetworkMessage& message);
    
    bool isListening() const;
    size_t getConnectionCount() const;
    std::vector<uint64_t> getConnectionIds() const;
    
    virtual void onConnect(uint64_t connectionId, const std::string& endpoint) override;
    virtual void onDisconnect(uint64_t connectionId, const NetworkErrorInfo& reason) override;
    virtual void onMessage(uint64_t connectionId, const NetworkMessage& message) override;
    virtual void onError(uint64_t connectionId, const NetworkErrorInfo& error) override;
    
private:
    void startAccept();
    void handleAccept(NetworkConnection::Pointer connection,
                     const boost::system::error_code& error);
    uint64_t getNextConnectionId();
    
    boost::asio::io_context ioContext_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_;
    boost::thread_group threadPool_;
    
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> listening_;
    
    std::map<uint64_t, NetworkConnection::Pointer> connections_;
    mutable std::mutex connectionsMutex_;
    
    INetworkEventHandler* eventHandler_;
    std::atomic<uint64_t> nextConnectionId_;
    
    static const size_t THREAD_POOL_SIZE = 4;
};

} 

#endif 