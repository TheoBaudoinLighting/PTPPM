#include "boost/wrap_boost_network.h"
#include <iostream>

namespace wrap_boost {

NetworkErrorInfo::NetworkErrorInfo() 
    : code_(NetworkError::NONE), message_("") {}

NetworkErrorInfo::NetworkErrorInfo(NetworkError code, const std::string& message)
    : code_(code), message_(message) {}

NetworkError NetworkErrorInfo::getCode() const {
    return code_;
}

const std::string& NetworkErrorInfo::getMessage() const {
    return message_;
}

bool NetworkErrorInfo::isError() const {
    return code_ != NetworkError::NONE;
}

NetworkMessage::NetworkMessage() : data_() {}

NetworkMessage::NetworkMessage(const std::vector<uint8_t>& data) : data_(data) {}

NetworkMessage::NetworkMessage(std::vector<uint8_t>&& data) : data_(std::move(data)) {}

const std::vector<uint8_t>& NetworkMessage::getData() const {
    return data_;
}

std::vector<uint8_t>& NetworkMessage::getData() {
    return data_;
}

size_t NetworkMessage::size() const {
    return data_.size();
}

bool NetworkMessage::empty() const {
    return data_.empty();
}

NetworkConnection::Pointer NetworkConnection::create(boost::asio::io_context& ioContext, 
                                                   uint64_t id, 
                                                   INetworkEventHandler* handler) {
    return Pointer(new NetworkConnection(ioContext, id, handler));
}

NetworkConnection::NetworkConnection(boost::asio::io_context& ioContext, 
                                   uint64_t id, 
                                   INetworkEventHandler* handler)
    : socket_(ioContext), 
      id_(id), 
      handler_(handler), 
      connected_(false),
      remoteEndpoint_(""),
      readBuffer_(MAX_BUFFER_SIZE) {}

uint64_t NetworkConnection::getId() const {
    return id_;
}

std::string NetworkConnection::getRemoteEndpoint() const {
    return remoteEndpoint_;
}

bool NetworkConnection::isConnected() const {
    return connected_;
}

void NetworkConnection::send(const NetworkMessage& message) {
    if (!connected_) {
        return;
    }
    
    bool writeInProgress = false;
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        writeInProgress = !writeQueue_.empty();
        writeQueue_.push(message);
    }
    
    if (!writeInProgress) {
        startWrite();
    }
}

void NetworkConnection::send(NetworkMessage&& message) {
    if (!connected_) {
        return;
    }
    
    bool writeInProgress = false;
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        writeInProgress = !writeQueue_.empty();
        writeQueue_.push(std::move(message));
    }
    
    if (!writeInProgress) {
        startWrite();
    }
}

void NetworkConnection::close() {
    if (!connected_) {
        return;
    }
    
    try {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        
        connected_ = false;
        
        if (handler_) {
            handler_->onDisconnect(id_, NetworkErrorInfo());
        }
    }
    catch (const std::exception& e) {
        if (handler_) {
            handler_->onError(id_, NetworkErrorInfo(NetworkError::CONNECTION_CLOSED, e.what()));
        }
    }
}

boost::asio::ip::tcp::socket& NetworkConnection::getSocket() {
    return socket_;
}

void NetworkConnection::start() {
    try {
        auto endpoint = socket_.remote_endpoint();
        remoteEndpoint_ = endpoint.address().to_string() + ":" + 
                          std::to_string(endpoint.port());
        connected_ = true;
        
        if (handler_) {
            handler_->onConnect(id_, remoteEndpoint_);
        }
        
        startRead();
    }
    catch (const std::exception& e) {
        if (handler_) {
            handler_->onError(id_, NetworkErrorInfo(NetworkError::CONNECTION_FAILED, e.what()));
        }
        close();
    }
}

void NetworkConnection::startRead() {
    if (!connected_) {
        return;
    }
    
    socket_.async_read_some(
        boost::asio::buffer(readBuffer_),
        [this](const boost::system::error_code& error, size_t bytesTransferred) {
            handleRead(error, bytesTransferred);
        }
    );
}

void NetworkConnection::handleRead(const boost::system::error_code& error, size_t bytesTransferred) {
    if (!error) {
        if (bytesTransferred > 0 && handler_) {
            NetworkMessage message(std::vector<uint8_t>(
                readBuffer_.begin(), 
                readBuffer_.begin() + bytesTransferred
            ));
            
            handler_->onMessage(id_, message);
        }
        
        startRead();
    }
    else {
        if (handler_) {
            handler_->onError(id_, NetworkErrorInfo(NetworkError::READ_ERROR, error.message()));
        }
        close();
    }
}

void NetworkConnection::startWrite() {
    if (!connected_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(writeMutex_);
    
    if (writeQueue_.empty()) {
        return;
    }
    
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(writeQueue_.front().getData()),
        [this](const boost::system::error_code& error, size_t bytesTransferred) {
            handleWrite(error, bytesTransferred);
        }
    );
}

void NetworkConnection::handleWrite(const boost::system::error_code& error, size_t /*bytesTransferred*/) {
    if (!error) {
        std::lock_guard<std::mutex> lock(writeMutex_);
        writeQueue_.pop();
        
        if (!writeQueue_.empty()) {
            startWrite();
        }
    }
    else {
        if (handler_) {
            handler_->onError(id_, NetworkErrorInfo(NetworkError::WRITE_ERROR, error.message()));
        }
        close();
    }
}

NetworkManager::NetworkManager()
    : work_(std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
          ioContext_.get_executor())),
      acceptor_(ioContext_),
      listening_(false),
      eventHandler_(nullptr),
      nextConnectionId_(1) {
    
    for (size_t i = 0; i < THREAD_POOL_SIZE; ++i) {
        threadPool_.create_thread([this]() {
            try {
                ioContext_.run();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in io_context thread: " << e.what() << std::endl;
            }
        });
    }
}

NetworkManager::~NetworkManager() {
    disconnectAll();
    stopListening();
    
    work_.reset();
    ioContext_.stop();
    
    threadPool_.join_all();
}

void NetworkManager::setEventHandler(INetworkEventHandler* handler) {
    eventHandler_ = handler;
}

bool NetworkManager::startListening(uint16_t port) {
    if (listening_) {
        return false;
    }
    
    try {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        startAccept();
        listening_ = true;
        return true;
    }
    catch (const std::exception& e) {
        NetworkErrorInfo error(NetworkError::BIND_ERROR, e.what());
        
        if (eventHandler_) {
            eventHandler_->onError(0, error);
        }
        
        return false;
    }
}

bool NetworkManager::stopListening() {
    if (!listening_) {
        return false;
    }
    
    try {
        acceptor_.close();
        listening_ = false;
        return true;
    }
    catch (const std::exception& e) {
        NetworkErrorInfo error(NetworkError::LISTEN_ERROR, e.what());
        
        if (eventHandler_) {
            eventHandler_->onError(0, error);
        }
        
        return false;
    }
}

uint64_t NetworkManager::connect(const std::string& host, uint16_t port) {
    try {
        boost::asio::ip::tcp::resolver resolver(ioContext_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        uint64_t connectionId = getNextConnectionId();
        
        auto connection = NetworkConnection::create(ioContext_, connectionId, this);
        
        boost::asio::async_connect(
            connection->getSocket(), 
            endpoints,
            [this, connection](const boost::system::error_code& error, 
                              const boost::asio::ip::tcp::endpoint& /*endpoint*/) {
                if (!error) {
                    std::lock_guard<std::mutex> lock(connectionsMutex_);
                    connections_[connection->getId()] = connection;
                    connection->start();
                }
                else {
                    NetworkErrorInfo errorInfo(NetworkError::CONNECTION_FAILED, error.message());
                    
                    if (eventHandler_) {
                        eventHandler_->onError(connection->getId(), errorInfo);
                    }
                }
            }
        );
        
        return connectionId;
    }
    catch (const std::exception& e) {
        NetworkErrorInfo error(NetworkError::RESOLVE_ERROR, e.what());
        
        if (eventHandler_) {
            eventHandler_->onError(0, error);
        }
        
        return 0;
    }
}

bool NetworkManager::disconnect(uint64_t connectionId) {
    NetworkConnection::Pointer connection;
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        auto it = connections_.find(connectionId);
        
        if (it == connections_.end()) {
            return false;
        }
        
        connection = it->second;
    }
    
    if (connection) {
        connection->close();
        return true;
    }
    
    return false;
}

void NetworkManager::disconnectAll() {
    std::vector<NetworkConnection::Pointer> connectionsToClose;
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        
        for (const auto& pair : connections_) {
            connectionsToClose.push_back(pair.second);
        }
    }
    
    for (auto& connection : connectionsToClose) {
        connection->close();
    }
}

bool NetworkManager::send(uint64_t connectionId, const NetworkMessage& message) {
    NetworkConnection::Pointer connection;
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        auto it = connections_.find(connectionId);
        
        if (it == connections_.end()) {
            return false;
        }
        
        connection = it->second;
    }
    
    if (connection && connection->isConnected()) {
        connection->send(message);
        return true;
    }
    
    return false;
}

bool NetworkManager::broadcast(const NetworkMessage& message) {
    std::vector<NetworkConnection::Pointer> connectionsToSend;
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        
        for (const auto& pair : connections_) {
            if (pair.second && pair.second->isConnected()) {
                connectionsToSend.push_back(pair.second);
            }
        }
    }
    
    for (auto& connection : connectionsToSend) {
        connection->send(message);
    }
    
    return !connectionsToSend.empty();
}

bool NetworkManager::isListening() const {
    return listening_;
}

size_t NetworkManager::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

std::vector<uint64_t> NetworkManager::getConnectionIds() const {
    std::vector<uint64_t> ids;
    
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        
        for (const auto& pair : connections_) {
            ids.push_back(pair.first);
        }
    }
    
    return ids;
}

void NetworkManager::onConnect(uint64_t connectionId, const std::string& endpoint) {
    if (eventHandler_) {
        eventHandler_->onConnect(connectionId, endpoint);
    }
}

void NetworkManager::onDisconnect(uint64_t connectionId, const NetworkErrorInfo& reason) {
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(connectionId);
    }
    
    if (eventHandler_) {
        eventHandler_->onDisconnect(connectionId, reason);
    }
}

void NetworkManager::onMessage(uint64_t connectionId, const NetworkMessage& message) {
    if (eventHandler_) {
        eventHandler_->onMessage(connectionId, message);
    }
}

void NetworkManager::onError(uint64_t connectionId, const NetworkErrorInfo& error) {
    if (eventHandler_) {
        eventHandler_->onError(connectionId, error);
    }
}

void NetworkManager::startAccept() {
    if (!listening_) {
        return;
    }
    
    uint64_t connectionId = getNextConnectionId();
    auto connection = NetworkConnection::create(ioContext_, connectionId, this);
    
    acceptor_.async_accept(
        connection->getSocket(),
        [this, connection](const boost::system::error_code& error) {
            handleAccept(connection, error);
        }
    );
}

void NetworkManager::handleAccept(NetworkConnection::Pointer connection,
                                 const boost::system::error_code& error) {
    if (!error) {
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_[connection->getId()] = connection;
        }
        
        connection->start();
    }
    else {
        NetworkErrorInfo errorInfo(NetworkError::ACCEPT_ERROR, error.message());
        
        if (eventHandler_) {
            eventHandler_->onError(0, errorInfo);
        }
    }
    
    startAccept();
}

uint64_t NetworkManager::getNextConnectionId() {
    return nextConnectionId_++;
}

}