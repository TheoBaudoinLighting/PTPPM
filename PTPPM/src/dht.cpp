#include "dht.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>
#include <map>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;
using namespace std::chrono_literals;

DHT::DHT(boost::asio::io_context& io_context, Peer& peer)
    : io_context_(io_context),
    peer_(peer),
    node_id_(NodeID::random()),
    routing_table_(node_id_),
    maintenance_timer_(io_context) {
}

void DHT::start() {
    spdlog::info("DHT started with node ID: {}", node_id_.toHex());
    startMaintenanceTimer();
}

void DHT::stop() {
    maintenance_timer_.cancel();
}

bool DHT::store(const std::string& key, const std::vector<uint8_t>& value,
    const std::chrono::seconds& ttl) {
    
    if (key.empty() || value.empty()) {
        spdlog::error("Empty key or value not allowed");
        return false;
    }
    
    if (value.size() > Message::MAX_BODY_SIZE / 2) {
        spdlog::error("Value too large to be stored in DHT");
        return false;
    }
    
    NodeID key_id;
    try {
        key_id = keyToNodeId(key);
    } catch (const std::exception& e) {
        spdlog::error("Failed to convert key to NodeID: {}", e.what());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        storage_[key_id] = DHTEntry(value, ttl);
    }

    auto closest_nodes = findNode(key_id);

    if (closest_nodes.empty()) {
        spdlog::info("No close nodes found to replicate the value.");
        return true;
    }

    size_t successful_stores = 1;

    try {
        json store_data;
        store_data["key"] = key_id.toHex();
        store_data["value"] = value;
        store_data["ttl"] = ttl.count();

        std::string serialized = store_data.dump();
        std::vector<uint8_t> message_data(serialized.begin(), serialized.end());

        for (const auto& contact : closest_nodes) {
            if (successful_stores >= REPLICATION_FACTOR) {
                break;
            }

            auto connection = peer_.getConnectionByAddress(contact.address, contact.port);

            if (connection && connection->isConnected()) {
                sendDHTMessage(connection, DHTMessageType::STORE, message_data);
                successful_stores++;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during store replication: {}", e.what());
    }

    return successful_stores >= 1;
}

std::future<std::optional<std::vector<uint8_t>>> DHT::retrieve(const std::string& key) {
    if (key.empty()) {
        std::promise<std::optional<std::vector<uint8_t>>> promise;
        promise.set_value(std::nullopt);
        return promise.get_future();
    }
    
    NodeID key_id;
    try {
        key_id = keyToNodeId(key);
    } catch (const std::exception& e) {
        spdlog::error("Failed to convert key to NodeID: {}", e.what());
        std::promise<std::optional<std::vector<uint8_t>>> promise;
        promise.set_value(std::nullopt);
        return promise.get_future();
    }

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        auto it = storage_.find(key_id);
        if (it != storage_.end() && !it->second.isExpired()) {
            std::promise<std::optional<std::vector<uint8_t>>> promise;
            promise.set_value(it->second.value);
            return promise.get_future();
        }
    }

    std::promise<std::optional<std::vector<uint8_t>>> promise;
    auto future = promise.get_future();

    boost::asio::post(io_context_, [this, key_id, promise = std::move(promise)]() mutable {
        try {
            findValue(key_id, promise);
        } catch (const std::exception& e) {
            spdlog::error("Error during retrieve: {}", e.what());
            promise.set_value(std::nullopt);
        }
    });

    return future;
}

bool DHT::remove(const std::string& key) {
    if (key.empty()) {
        return false;
    }
    
    NodeID key_id;
    try {
        key_id = keyToNodeId(key);
    } catch (const std::exception& e) {
        spdlog::error("Failed to convert key to NodeID: {}", e.what());
        return false;
    }

    std::lock_guard<std::mutex> lock(storage_mutex_);
    return storage_.erase(key_id) > 0;
}

bool DHT::bootstrap(const std::string& host, uint16_t port) {
    if (host.empty() || port == 0) {
        spdlog::error("Invalid bootstrap host or port");
        return false;
    }
    
    if (!peer_.connectTo(host, port)) {
        spdlog::error("Could not connect to bootstrap node {}", host + ":" + std::to_string(port));
        return false;
    }

    spdlog::info("Connected to bootstrap node {}", host + ":" + std::to_string(port));

    NodeID bootstrap_id = NodeID::random();
    routing_table_.update(bootstrap_id, host, port);

    findNode(node_id_);

    return true;
}

bool DHT::joinNetwork(const std::vector<std::pair<std::string, uint16_t>>& bootstrap_nodes) {
    if (bootstrap_nodes.empty()) {
        spdlog::error("No bootstrap nodes provided");
        return false;
    }
    
    bool success = false;

    for (const auto& [host, port] : bootstrap_nodes) {
        if (bootstrap(host, port)) {
            success = true;
        }
    }

    return success;
}

NodeID DHT::getNodeId() const {
    return node_id_;
}

void DHT::setStoreCallback(std::function<void(const std::string&, const std::vector<uint8_t>&)> callback) {
    store_callback_ = std::move(callback);
}

void DHT::handleDHTMessage(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) {
        return;
    }
    
    try {
        DHTMessageType dht_type = static_cast<DHTMessageType>(message.type());

        switch (dht_type) {
        case DHTMessageType::FIND_NODE:
            handleFindNode(message, sender);
            break;
        case DHTMessageType::FIND_NODE_REPLY:
            handleFindNodeReply(message, sender);
            break;
        case DHTMessageType::FIND_VALUE:
            handleFindValue(message, sender);
            break;
        case DHTMessageType::FIND_VALUE_REPLY:
            handleFindValueReply(message, sender);
            break;
        case DHTMessageType::STORE:
            handleStore(message, sender);
            break;
        case DHTMessageType::STORE_REPLY:
            handleStoreReply(message, sender);
            break;
        default:
            spdlog::error("Unknown DHT message type: {}", static_cast<int>(dht_type));
            break;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error handling DHT message: {}", e.what());
    }
}

std::string DHT::getStats() const {
    std::stringstream ss;

    ss << "DHT Stats:" << std::endl;
    ss << "  Node ID: " << node_id_.toHex() << std::endl;
    ss << "  Contacts in routing table: " << routing_table_.size() << std::endl;

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        size_t expired = 0;
        for (const auto& [id, entry] : storage_) {
            if (entry.isExpired()) {
                expired++;
            }
        }
        ss << "  Locally stored entries: " << storage_.size()
            << " (including " << expired << " expired)" << std::endl;
    }

    return ss.str();
}

NodeID DHT::keyToNodeId(const std::string& key) const {
    if (key.empty()) {
        spdlog::error("Empty key not allowed");
        throw std::invalid_argument("Empty key not allowed");
    }
    
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(key.c_str()), key.length(), hash);

    NodeID::IDType id_array;
    std::copy(hash, hash + NODE_ID_SIZE, id_array.begin());

    return NodeID(id_array);
}

std::vector<Contact> DHT::findNode(const NodeID& target) {
    auto closest = routing_table_.findClosestContacts(target, K);

    if (closest.empty()) {
        return {};
    }

    std::set<NodeID> queried_nodes;

    std::map<NodeID, Contact> closest_nodes;
    for (const auto& contact : closest) {
        NodeID distance = contact.id.distanceXor(target);
        closest_nodes[distance] = contact;
    }

    std::vector<NodeID> active_nodes;

    try {
        json find_data;
        find_data["target"] = target.toHex();
        find_data["sender"] = node_id_.toHex();
        std::string serialized = find_data.dump();
        std::vector<uint8_t> message_data(serialized.begin(), serialized.end());

        size_t alpha_count = std::min(closest.size(), ALPHA);
        for (size_t i = 0; i < alpha_count; ++i) {
            const auto& contact = closest[i];

            auto connection = peer_.getConnectionByAddress(contact.address, contact.port);
            if (connection && connection->isConnected()) {
                sendDHTMessage(connection, DHTMessageType::FIND_NODE, message_data);
                queried_nodes.insert(contact.id);
                active_nodes.push_back(contact.id);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during findNode: {}", e.what());
        return {};
    }

    auto deadline = std::chrono::system_clock::now() + 5s;

    while (!active_nodes.empty() && std::chrono::system_clock::now() < deadline) {
        std::this_thread::sleep_for(100ms);

        std::vector<Contact> new_closest = routing_table_.findClosestContacts(target, K);

        bool found_new = false;
        for (const auto& contact : new_closest) {
            if (queried_nodes.find(contact.id) == queried_nodes.end()) {
                found_new = true;

                auto connection = peer_.getConnectionByAddress(contact.address, contact.port);
                if (connection && connection->isConnected()) {
                    try {
                        json find_data;
                        find_data["target"] = target.toHex();
                        find_data["sender"] = node_id_.toHex();
                        std::string serialized = find_data.dump();
                        std::vector<uint8_t> message_data(serialized.begin(), serialized.end());
                        
                        sendDHTMessage(connection, DHTMessageType::FIND_NODE, message_data);
                        queried_nodes.insert(contact.id);
                        active_nodes.push_back(contact.id);
                    } catch (const std::exception& e) {
                        spdlog::error("Error sending findNode message: {}", e.what());
                    }
                }
            }
        }

        if (!found_new) {
            break;
        }
    }

    std::vector<Contact> result;
    for (const auto& [distance, contact] : closest_nodes) {
        result.push_back(contact);
        if (result.size() >= K) {
            break;
        }
    }

    return result;
}

void DHT::findValue(const NodeID& key, std::promise<std::optional<std::vector<uint8_t>>>& promise) {
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        auto it = storage_.find(key);
        if (it != storage_.end() && !it->second.isExpired()) {
            promise.set_value(it->second.value);
            return;
        }
    }

    auto closest = routing_table_.findClosestContacts(key, K);

    if (closest.empty()) {
        promise.set_value(std::nullopt);
        return;
    }

    std::set<NodeID> queried_nodes;

    std::map<NodeID, Contact> closest_nodes;
    for (const auto& contact : closest) {
        NodeID distance = contact.id.distanceXor(key);
        closest_nodes[distance] = contact;
    }

    try {
        json find_data;
        find_data["key"] = key.toHex();
        find_data["sender"] = node_id_.toHex();
        std::string serialized = find_data.dump();
        std::vector<uint8_t> message_data(serialized.begin(), serialized.end());

        size_t alpha_count = std::min(closest.size(), ALPHA);
        for (size_t i = 0; i < alpha_count; ++i) {
            const auto& contact = closest[i];

            auto connection = peer_.getConnectionByAddress(contact.address, contact.port);
            if (connection && connection->isConnected()) {
                sendDHTMessage(connection, DHTMessageType::FIND_VALUE, message_data);
                queried_nodes.insert(contact.id);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error during findValue: {}", e.what());
        promise.set_value(std::nullopt);
        return;
    }

    auto deadline = std::chrono::system_clock::now() + 5s;

    while (std::chrono::system_clock::now() < deadline) {
        std::this_thread::sleep_for(100ms);

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            auto it = storage_.find(key);
            if (it != storage_.end() && !it->second.isExpired()) {
                promise.set_value(it->second.value);
                return;
            }
        }

        std::vector<Contact> new_closest = routing_table_.findClosestContacts(key, K);

        bool found_new = false;
        for (const auto& contact : new_closest) {
            if (queried_nodes.find(contact.id) == queried_nodes.end()) {
                found_new = true;

                auto connection = peer_.getConnectionByAddress(contact.address, contact.port);
                if (connection && connection->isConnected()) {
                    try {
                        json find_data;
                        find_data["key"] = key.toHex();
                        find_data["sender"] = node_id_.toHex();
                        std::string serialized = find_data.dump();
                        std::vector<uint8_t> message_data(serialized.begin(), serialized.end());
                        
                        sendDHTMessage(connection, DHTMessageType::FIND_VALUE, message_data);
                        queried_nodes.insert(contact.id);
                    } catch (const std::exception& e) {
                        spdlog::error("Error sending findValue message: {}", e.what());
                    }
                }
            }
        }

        if (!found_new) {
            break;
        }
    }

    promise.set_value(std::nullopt);
}

void DHT::handleFindNode(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) return;
    
    try {
        std::string data_str(message.data().begin(), message.data().end());
        auto j = json::parse(data_str);

        NodeID target(j["target"].get<std::string>());
        NodeID sender_id(j["sender"].get<std::string>());

        routing_table_.update(sender_id, sender->getRemoteAddress(),
            sender->getRemotePort());

        auto closest = routing_table_.findClosestContacts(target, K);

        auto serialized_contacts = serializeContacts(closest);

        sendDHTMessage(sender, DHTMessageType::FIND_NODE_REPLY, serialized_contacts);
    }
    catch (const std::exception& e) {
        spdlog::error("Error processing FIND_NODE: {}", e.what());
    }
}

void DHT::handleFindNodeReply(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) return;
    
    try {
        auto contacts = deserializeContacts(message.data());

        for (const auto& contact : contacts) {
            if (contact.id != node_id_) { 
                routing_table_.update(contact.id, contact.address, contact.port);
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error processing FIND_NODE_REPLY: {}", e.what());
    }
}

void DHT::handleFindValue(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) return;
    
    try {
        std::string data_str(message.data().begin(), message.data().end());
        auto j = json::parse(data_str);

        NodeID key(j["key"].get<std::string>());
        NodeID sender_id(j["sender"].get<std::string>());

        routing_table_.update(sender_id, sender->getRemoteAddress(),
            sender->getRemotePort());

        std::vector<uint8_t> reply_data;

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            auto it = storage_.find(key);
            if (it != storage_.end() && !it->second.isExpired()) {
                json reply;
                reply["found"] = true;
                reply["value"] = it->second.value;
                std::string serialized = reply.dump();
                reply_data.assign(serialized.begin(), serialized.end());

                sendDHTMessage(sender, DHTMessageType::FIND_VALUE_REPLY, reply_data);
                return;
            }
        }

        auto closest = routing_table_.findClosestContacts(key, K);

        json reply;
        reply["found"] = false;
        auto& contacts_array = reply["contacts"] = json::array();

        for (const auto& contact : closest) {
            if (contact.id != sender_id) { 
                json contact_json;
                contact_json["id"] = contact.id.toHex();
                contact_json["address"] = contact.address;
                contact_json["port"] = contact.port;
                contacts_array.push_back(contact_json);
            }
        }

        std::string serialized = reply.dump();
        reply_data.assign(serialized.begin(), serialized.end());

        sendDHTMessage(sender, DHTMessageType::FIND_VALUE_REPLY, reply_data);
    }
    catch (const std::exception& e) {
        spdlog::error("Error processing FIND_VALUE: {}", e.what());
    }
}

void DHT::handleFindValueReply(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) return;
    
    try {
        std::string data_str(message.data().begin(), message.data().end());
        auto j = json::parse(data_str);

        if (j["found"].get<bool>()) {
            std::vector<uint8_t> value = j["value"].get<std::vector<uint8_t>>();
            
            if (!value.empty()) {
                std::lock_guard<std::mutex> lock(storage_mutex_);
                
                NodeID temp_key = NodeID::random();
                storage_[temp_key] = DHTEntry(value);
            }
        }
        else {
            auto& contacts_array = j["contacts"];

            for (const auto& contact_json : contacts_array) {
                try {
                    NodeID id(contact_json["id"].get<std::string>());
                    std::string address = contact_json["address"].get<std::string>();
                    uint16_t port = contact_json["port"].get<uint16_t>();

                    if (id != node_id_) { 
                        routing_table_.update(id, address, port);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error processing contact in FIND_VALUE_REPLY: {}", e.what());
                }
            }
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error processing FIND_VALUE_REPLY: {}", e.what());
    }
}

void DHT::handleStore(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) return;
    
    try {
        std::string data_str(message.data().begin(), message.data().end());
        auto j = json::parse(data_str);

        NodeID key(j["key"].get<std::string>());
        std::vector<uint8_t> value = j["value"].get<std::vector<uint8_t>>();
        std::chrono::seconds ttl(j["ttl"].get<int64_t>());

        if (value.size() > Message::MAX_BODY_SIZE / 2) {
            throw std::runtime_error("Value too large to store");
        }

        {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            storage_[key] = DHTEntry(value, ttl);
        }

        if (store_callback_) {
            std::string key_str = key.toHex();
            store_callback_(key_str, value);
        }

        json reply;
        reply["success"] = true;
        std::string serialized = reply.dump();
        std::vector<uint8_t> reply_data(serialized.begin(), serialized.end());

        sendDHTMessage(sender, DHTMessageType::STORE_REPLY, reply_data);
    }
    catch (const std::exception& e) {
        spdlog::error("Error processing STORE: {}", e.what());

        json reply;
        reply["success"] = false;
        reply["error"] = e.what();
        std::string serialized = reply.dump();
        std::vector<uint8_t> reply_data(serialized.begin(), serialized.end());

        sendDHTMessage(sender, DHTMessageType::STORE_REPLY, reply_data);
    }
}

void DHT::handleStoreReply(const Message& message, std::shared_ptr<Connection> sender) {
    if (!sender || !sender->isConnected()) return;
    
    try {
        std::string data_str(message.data().begin(), message.data().end());
        auto j = json::parse(data_str);

        bool success = j["success"].get<bool>();

        if (!success && j.contains("error")) {
            std::string error = j["error"].get<std::string>();
            spdlog::error("Storage error on {}: {}", sender->getRemoteAddress(), error);
        }
    }
    catch (const std::exception& e) {
        spdlog::error("Error processing STORE_REPLY: {}", e.what());
    }
}

std::vector<uint8_t> DHT::serializeContacts(const std::vector<Contact>& contacts) const {
    try {
        json j = json::array();

        for (const auto& contact : contacts) {
            json contact_json;
            contact_json["id"] = contact.id.toHex();
            contact_json["address"] = contact.address;
            contact_json["port"] = contact.port;
            j.push_back(contact_json);
        }

        std::string serialized = j.dump();
        return std::vector<uint8_t>(serialized.begin(), serialized.end());
    } catch (const std::exception& e) {
        spdlog::error("Error serializing contacts: {}", e.what());
        return {};
    }
}

std::vector<Contact> DHT::deserializeContacts(const std::vector<uint8_t>& data) const {
    std::vector<Contact> contacts;
    
    if (data.empty()) {
        return contacts;
    }
    
    try {
        std::string data_str(data.begin(), data.end());
        auto j = json::parse(data_str);

        for (const auto& contact_json : j) {
            NodeID id(contact_json["id"].get<std::string>());
            std::string address = contact_json["address"].get<std::string>();
            uint16_t port = contact_json["port"].get<uint16_t>();

            contacts.emplace_back(id, address, port);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error deserializing contacts: {}", e.what());
    }

    return contacts;
}

void DHT::startMaintenanceTimer() {
    maintenance_timer_.expires_after(std::chrono::minutes(10));
    maintenance_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            doMaintenance();
            startMaintenanceTimer();
        }
    });
}

void DHT::doMaintenance() {
    std::cout << "Running DHT maintenance..." << std::endl;

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        for (auto it = storage_.begin(); it != storage_.end();) {
            if (it->second.isExpired()) {
                it = storage_.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

void DHT::sendDHTMessage(std::shared_ptr<Connection> connection,
    DHTMessageType type,
    const std::vector<uint8_t>& data) {
    
    if (!connection || !connection->isConnected() || data.size() > Message::MAX_BODY_SIZE) {
        return;
    }
    
    try {
        MessageType msg_type = static_cast<MessageType>(type);
        Message message(msg_type, data);
        connection->send(message);
    } catch (const std::exception& e) {
        spdlog::error("Error sending DHT message: {}", e.what());
    }
}