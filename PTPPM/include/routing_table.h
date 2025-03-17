#pragma once

#include <array>
#include <vector>
#include <memory>
#include <chrono>
#include "node_id.h"
#include "kbucket.h"

constexpr size_t ID_BITS = NODE_ID_SIZE * 8;

class RoutingTable {
public:
    explicit RoutingTable(const NodeID& self_id);

    bool update(const NodeID& id, const std::string& address, uint16_t port);

    bool remove(const NodeID& id);

    std::vector<Contact> findClosestContacts(const NodeID& target, size_t count) const;

    std::vector<Contact> getAllContacts() const;

    std::optional<Contact> findContact(const NodeID& id) const;

    size_t size() const;

    const NodeID& getSelfId() const { return self_id_; }

private:
    NodeID self_id_;
    std::array<KBucket, ID_BITS> buckets_;

    KBucket& getBucket(const NodeID& id);
    const KBucket& getBucket(const NodeID& id) const;
};