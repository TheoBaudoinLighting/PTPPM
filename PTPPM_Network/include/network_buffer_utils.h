#pragma once

#include "dynamic_buffer.h"
#include <cstdint>
#include <string>
#include <vector>

namespace NetworkBufferUtils {

    void writeUInt8(DynamicBuffer& buffer, uint8_t value);
    void writeUInt16(DynamicBuffer& buffer, uint16_t value, bool networkByteOrder = true);
    void writeUInt32(DynamicBuffer& buffer, uint32_t value, bool networkByteOrder = true);
    void writeUInt64(DynamicBuffer& buffer, uint64_t value, bool networkByteOrder = true);
    void writeFloat(DynamicBuffer& buffer, float value, bool networkByteOrder = true);
    void writeDouble(DynamicBuffer& buffer, double value, bool networkByteOrder = true);
    void writeString(DynamicBuffer& buffer, const std::string& value);
    void writeBytes(DynamicBuffer& buffer, const void* data, size_t length);
    
    uint8_t readUInt8(const DynamicBuffer& buffer, size_t& offset);
    uint16_t readUInt16(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder = true);
    uint32_t readUInt32(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder = true);
    uint64_t readUInt64(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder = true);
    float readFloat(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder = true);
    double readDouble(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder = true);
    std::string readString(const DynamicBuffer& buffer, size_t& offset);
    std::vector<uint8_t> readBytes(const DynamicBuffer& buffer, size_t& offset, size_t length);
    
    uint16_t htons(uint16_t value);
    uint32_t htonl(uint32_t value);
    uint64_t htonll(uint64_t value);
    uint16_t ntohs(uint16_t value);
    uint32_t ntohl(uint32_t value);
    uint64_t ntohll(uint64_t value);
    
    void writeVarInt(DynamicBuffer& buffer, uint64_t value);
    uint64_t readVarInt(const DynamicBuffer& buffer, size_t& offset);
    
    bool canRead(const DynamicBuffer& buffer, size_t offset, size_t bytesToRead);
    void ensureCanRead(const DynamicBuffer& buffer, size_t offset, size_t bytesToRead);
};