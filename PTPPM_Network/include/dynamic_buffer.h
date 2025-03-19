#pragma once

#include <vector>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

class DynamicBuffer {
public:
    DynamicBuffer(size_t initialCapacity = 4096);
    DynamicBuffer(const std::vector<uint8_t>& data);
    DynamicBuffer(std::vector<uint8_t>&& data);
    DynamicBuffer(const void* data, size_t length);
    DynamicBuffer(const std::string& str);
    ~DynamicBuffer() = default;

    size_t size() const;
    size_t capacity() const;
    bool empty() const;

    const uint8_t* data() const;
    uint8_t* data();

    void resize(size_t newSize);
    void reserve(size_t capacity);
    void clear();

    void append(const void* data, size_t length);
    void append(const std::vector<uint8_t>& data);
    void append(const DynamicBuffer& other);
    void append(const std::string& str);

    void consume(size_t length);

    std::vector<uint8_t> toVector() const;
    std::string toString() const;

private:
    std::vector<uint8_t> buffer_;
    size_t readPos_ = 0;
    size_t writePos_ = 0;

    void compact();
    void ensureCapacity(size_t additionalBytes);
};

class DynamicBufferPool {
public:
    static DynamicBufferPool& getInstance();

    std::shared_ptr<DynamicBuffer> acquire(size_t initialCapacity = 4096);
    void release(std::shared_ptr<DynamicBuffer>& buffer);
    
    size_t getPoolSize() const;

private:
    DynamicBufferPool() = default;
    ~DynamicBufferPool() = default;
    
    DynamicBufferPool(const DynamicBufferPool&) = delete;
    DynamicBufferPool& operator=(const DynamicBufferPool&) = delete;

    std::vector<std::shared_ptr<DynamicBuffer>> pool_;
    mutable std::mutex mutex_;

    static constexpr size_t MAX_POOL_SIZE = 32;
};