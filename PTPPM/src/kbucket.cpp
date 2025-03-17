#include "kbucket.h"
#include <algorithm>

KBucket::KBucket() = default;

bool KBucket::update(const Contact& contact) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(contacts_.begin(), contacts_.end(),
        [&](const Contact& c) { return c.id == contact.id; });

    if (it != contacts_.end()) {
        Contact updated_contact = *it;
        updated_contact.updateLastSeen();
        updated_contact.address = contact.address;
        updated_contact.port = contact.port;

        contacts_.erase(it);
        contacts_.push_back(updated_contact);
        return true;
    }
    else if (contacts_.size() < K) {
        contacts_.push_back(contact);
        return true;
    }

    return false;
}

bool KBucket::remove(const NodeID& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(contacts_.begin(), contacts_.end(),
        [&](const Contact& c) { return c.id == id; });

    if (it != contacts_.end()) {
        contacts_.erase(it);
        return true;
    }

    return false;
}

std::optional<Contact> KBucket::getLeastRecentlySeen() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (contacts_.empty()) {
        return std::nullopt;
    }

    return contacts_.front();
}

std::vector<Contact> KBucket::getContacts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<Contact>(contacts_.begin(), contacts_.end());
}

bool KBucket::isFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return contacts_.size() >= K;
}

size_t KBucket::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return contacts_.size();
}

bool KBucket::contains(const NodeID& id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    return std::any_of(contacts_.begin(), contacts_.end(),
        [&](const Contact& c) { return c.id == id; });
}