#include "dynamic_buffer.h"
#include <algorithm>
#include <cstring>

DynamicBuffer::DynamicBuffer(size_t initialCapacity) {
    buffer_.reserve(initialCapacity);
}

DynamicBuffer::DynamicBuffer(const std::vector<uint8_t>& data)
    : buffer_(data), writePos_(data.size()) {
}

DynamicBuffer::DynamicBuffer(std::vector<uint8_t>&& data)
    : buffer_(std::move(data)), writePos_(buffer_.size()) {
}

DynamicBuffer::DynamicBuffer(const void* data, size_t length) {
    if (data && length > 0) {
        buffer_.resize(length);
        std::memcpy(buffer_.data(), data, length);
        writePos_ = length;
    }
}

DynamicBuffer::DynamicBuffer(const std::string& str)
    : DynamicBuffer(str.data(), str.size()) {
}

size_t DynamicBuffer::size() const {
    return writePos_ - readPos_;
}

size_t DynamicBuffer::capacity() const {
    return buffer_.capacity();
}

bool DynamicBuffer::empty() const {
    return size() == 0;
}

const uint8_t* DynamicBuffer::data() const {
    return buffer_.data() + readPos_;
}

uint8_t* DynamicBuffer::data() {
    return buffer_.data() + readPos_;
}

void DynamicBuffer::resize(size_t newSize) {
    ensureCapacity(newSize - size());
    writePos_ = readPos_ + newSize;
}

void DynamicBuffer::reserve(size_t capacity) {
    if (capacity > buffer_.capacity()) {
        compact();
        buffer_.reserve(capacity);
    }
}

void DynamicBuffer::clear() {
    readPos_ = 0;
    writePos_ = 0;
    buffer_.clear();
}

void DynamicBuffer::append(const void* data, size_t length) {
    if (!data || length == 0) return;
    
    ensureCapacity(length);
    
    if (writePos_ + length > buffer_.size()) {
        buffer_.resize(writePos_ + length);
    }
    
    std::memcpy(buffer_.data() + writePos_, data, length);
    writePos_ += length;
}

void DynamicBuffer::append(const std::vector<uint8_t>& data) {
    append(data.data(), data.size());
}

void DynamicBuffer::append(const DynamicBuffer& other) {
    append(other.data(), other.size());
}

void DynamicBuffer::append(const std::string& str) {
    append(str.data(), str.size());
}

void DynamicBuffer::consume(size_t length) {
    if (length >= size()) {
        readPos_ = 0;
        writePos_ = 0;
    } else {
        readPos_ += length;
        
        if (readPos_ > buffer_.size() / 2) {
            compact();
        }
    }
}

std::vector<uint8_t> DynamicBuffer::toVector() const {
    return std::vector<uint8_t>(buffer_.begin() + readPos_, buffer_.begin() + writePos_);
}

std::string DynamicBuffer::toString() const {
    return std::string(reinterpret_cast<const char*>(data()), size());
}

void DynamicBuffer::compact() {
    if (readPos_ == 0) return;
    
    if (readPos_ < writePos_) {
        std::memmove(buffer_.data(), buffer_.data() + readPos_, size());
    }
    
    writePos_ -= readPos_;
    readPos_ = 0;
}

void DynamicBuffer::ensureCapacity(size_t additionalBytes) {
    if (writePos_ + additionalBytes > buffer_.capacity()) {
        compact();
        
        if (writePos_ + additionalBytes > buffer_.capacity()) {
            size_t newCapacity = std::max(buffer_.capacity() * 2, writePos_ + additionalBytes);
            buffer_.reserve(newCapacity);
        }
    }
}

DynamicBufferPool& DynamicBufferPool::getInstance() {
    static DynamicBufferPool instance;
    return instance;
}

std::shared_ptr<DynamicBuffer> DynamicBufferPool::acquire(size_t initialCapacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!pool_.empty()) {
        auto buffer = pool_.back();
        pool_.pop_back();
        
        buffer->clear();
        buffer->reserve(initialCapacity);
        return buffer;
    }
    
    return std::make_shared<DynamicBuffer>(initialCapacity);
}

void DynamicBufferPool::release(std::shared_ptr<DynamicBuffer>& buffer) {
    if (!buffer) return;
    
    buffer->clear();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pool_.size() < MAX_POOL_SIZE) {
        pool_.push_back(buffer);
    }
    
    buffer.reset();
}

size_t DynamicBufferPool::getPoolSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
}