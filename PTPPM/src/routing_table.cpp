#include "routing_table.h"
#include <algorithm>
#include <set>
#include <map>

RoutingTable::RoutingTable(const NodeID& self_id) : self_id_(self_id) {
}

bool RoutingTable::update(const NodeID& id, const std::string& address, uint16_t port) {
    if (id == self_id_) {
        return false;
    }
    
    if (address.empty() || port == 0) {
        return false;
    }

    Contact contact(id, address, port);
    return getBucket(id).update(contact);
}

bool RoutingTable::remove(const NodeID& id) {
    if (id == self_id_) {
        return false;
    }
    
    return getBucket(id).remove(id);
}

std::vector<Contact> RoutingTable::findClosestContacts(const NodeID& target, size_t count) const {
    if (target == self_id_ || count == 0) {
        return {};
    }

    std::map<NodeID, Contact> contacts_by_distance;

    for (const auto& bucket : buckets_) {
        for (const auto& contact : bucket.getContacts()) {
            NodeID distance = contact.id.distanceXor(target);
            contacts_by_distance[distance] = contact;
        }
    }

    std::vector<Contact> result;
    result.reserve(std::min(count, contacts_by_distance.size()));
    
    for (const auto& [distance, contact] : contacts_by_distance) {
        result.push_back(contact);
        if (result.size() >= count) {
            break;
        }
    }

    return result;
}

std::vector<Contact> RoutingTable::getAllContacts() const {
    std::vector<Contact> result;
    size_t total_contacts = 0;
    
    for (const auto& bucket : buckets_) {
        total_contacts += bucket.size();
    }
    
    result.reserve(total_contacts);

    for (const auto& bucket : buckets_) {
        auto contacts = bucket.getContacts();
        result.insert(result.end(), contacts.begin(), contacts.end());
    }

    return result;
}

std::optional<Contact> RoutingTable::findContact(const NodeID& id) const {
    if (id == self_id_) {
        return std::nullopt;
    }
    
    const KBucket& bucket = getBucket(id);

    if (!bucket.contains(id)) {
        return std::nullopt;
    }

    for (const auto& contact : bucket.getContacts()) {
        if (contact.id == id) {
            return contact;
        }
    }

    return std::nullopt;
}

size_t RoutingTable::size() const {
    size_t count = 0;
    for (const auto& bucket : buckets_) {
        count += bucket.size();
    }
    return count;
}

KBucket& RoutingTable::getBucket(const NodeID& id) {
    int index = self_id_.bucketIndex(id);
    if (index < 0 || index >= static_cast<int>(ID_BITS)) {
        index = ID_BITS - 1;
    }
    return buckets_[index];
}

const KBucket& RoutingTable::getBucket(const NodeID& id) const {
    int index = self_id_.bucketIndex(id);
    if (index < 0 || index >= static_cast<int>(ID_BITS)) {
        index = ID_BITS - 1;
    }
    return buckets_[index];
}