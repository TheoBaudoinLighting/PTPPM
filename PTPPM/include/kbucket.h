#pragma once

#include <list>
#include <mutex>
#include <chrono>
#include <vector>
#include <memory>
#include <optional>
#include "node_id.h"

struct Contact {
    NodeID id;
    std::string address;
    uint16_t port;
    std::chrono::system_clock::time_point last_seen;

    Contact() 
        : id(), address(), port(0), last_seen(std::chrono::system_clock::now()) {
    }

    Contact(const NodeID& id, const std::string& address, uint16_t port)
        : id(id), address(address), port(port), last_seen(std::chrono::system_clock::now()) {
    }

    void updateLastSeen() {
        last_seen = std::chrono::system_clock::now();
    }

    bool isStale(const std::chrono::seconds& timeout) const {
        auto now = std::chrono::system_clock::now();
        return (now - last_seen) > timeout;
    }
};

class KBucket {
public:
    static constexpr size_t K = 20;

    KBucket();

    bool update(const Contact& contact);
    bool remove(const NodeID& id);
    std::optional<Contact> getLeastRecentlySeen() const;
    std::vector<Contact> getContacts() const;
    bool isFull() const;
    size_t size() const;
    bool contains(const NodeID& id) const;

private:
    mutable std::mutex mutex_;
    std::list<Contact> contacts_; 
};