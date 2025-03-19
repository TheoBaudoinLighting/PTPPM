#include "network_buffer_utils.h"
#include <stdexcept>
#include <cstring>
#include <boost/endian/conversion.hpp>

namespace NetworkBufferUtils {

void writeUInt8(DynamicBuffer& buffer, uint8_t value) {
    buffer.append(&value, sizeof(value));
}

void writeUInt16(DynamicBuffer& buffer, uint16_t value, bool networkByteOrder) {
    if (networkByteOrder) {
        value = htons(value);
    }
    buffer.append(&value, sizeof(value));
}

void writeUInt32(DynamicBuffer& buffer, uint32_t value, bool networkByteOrder) {
    if (networkByteOrder) {
        value = htonl(value);
    }
    buffer.append(&value, sizeof(value));
}

void writeUInt64(DynamicBuffer& buffer, uint64_t value, bool networkByteOrder) {
    if (networkByteOrder) {
        value = htonll(value);
    }
    buffer.append(&value, sizeof(value));
}

void writeFloat(DynamicBuffer& buffer, float value, bool networkByteOrder) {
    if (networkByteOrder) {
        uint32_t temp;
        std::memcpy(&temp, &value, sizeof(value));
        temp = htonl(temp);
        buffer.append(&temp, sizeof(temp));
    } else {
        buffer.append(&value, sizeof(value));
    }
}

void writeDouble(DynamicBuffer& buffer, double value, bool networkByteOrder) {
    if (networkByteOrder) {
        uint64_t temp;
        std::memcpy(&temp, &value, sizeof(value));
        temp = htonll(temp);
        buffer.append(&temp, sizeof(temp));
    } else {
        buffer.append(&value, sizeof(value));
    }
}

void writeString(DynamicBuffer& buffer, const std::string& value) {
    writeUInt32(buffer, static_cast<uint32_t>(value.size()));
    if (!value.empty()) {
        buffer.append(value.data(), value.size());
    }
}

void writeBytes(DynamicBuffer& buffer, const void* data, size_t length) {
    if (data && length > 0) {
        buffer.append(data, length);
    }
}

uint8_t readUInt8(const DynamicBuffer& buffer, size_t& offset) {
    ensureCanRead(buffer, offset, sizeof(uint8_t));
    uint8_t value = *(buffer.data() + offset);
    offset += sizeof(uint8_t);
    return value;
}

uint16_t readUInt16(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder) {
    ensureCanRead(buffer, offset, sizeof(uint16_t));
    uint16_t value;
    std::memcpy(&value, buffer.data() + offset, sizeof(value));
    offset += sizeof(uint16_t);
    
    if (networkByteOrder) {
        value = ntohs(value);
    }
    return value;
}

uint32_t readUInt32(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder) {
    ensureCanRead(buffer, offset, sizeof(uint32_t));
    uint32_t value;
    std::memcpy(&value, buffer.data() + offset, sizeof(value));
    offset += sizeof(uint32_t);
    
    if (networkByteOrder) {
        value = ntohl(value);
    }
    return value;
}

uint64_t readUInt64(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder) {
    ensureCanRead(buffer, offset, sizeof(uint64_t));
    uint64_t value;
    std::memcpy(&value, buffer.data() + offset, sizeof(value));
    offset += sizeof(uint64_t);
    
    if (networkByteOrder) {
        value = ntohll(value);
    }
    return value;
}

float readFloat(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder) {
    ensureCanRead(buffer, offset, sizeof(float));
    
    if (networkByteOrder) {
        uint32_t temp;
        std::memcpy(&temp, buffer.data() + offset, sizeof(temp));
        temp = ntohl(temp);
        offset += sizeof(float);
        
        float value;
        std::memcpy(&value, &temp, sizeof(value));
        return value;
    } else {
        float value;
        std::memcpy(&value, buffer.data() + offset, sizeof(value));
        offset += sizeof(float);
        return value;
    }
}

double readDouble(const DynamicBuffer& buffer, size_t& offset, bool networkByteOrder) {
    ensureCanRead(buffer, offset, sizeof(double));
    
    if (networkByteOrder) {
        uint64_t temp;
        std::memcpy(&temp, buffer.data() + offset, sizeof(temp));
        temp = ntohll(temp);
        offset += sizeof(double);
        
        double value;
        std::memcpy(&value, &temp, sizeof(value));
        return value;
    } else {
        double value;
        std::memcpy(&value, buffer.data() + offset, sizeof(value));
        offset += sizeof(double);
        return value;
    }
}

std::string readString(const DynamicBuffer& buffer, size_t& offset) {
    uint32_t length = readUInt32(buffer, offset);
    
    if (length == 0) {
        return "";
    }
    
    ensureCanRead(buffer, offset, length);
    std::string result(reinterpret_cast<const char*>(buffer.data() + offset), length);
    offset += length;
    return result;
}

std::vector<uint8_t> readBytes(const DynamicBuffer& buffer, size_t& offset, size_t length) {
    if (length == 0) {
        return {};
    }
    
    ensureCanRead(buffer, offset, length);
    std::vector<uint8_t> result(buffer.data() + offset, buffer.data() + offset + length);
    offset += length;
    return result;
}

uint16_t htons(uint16_t value) {
    return boost::endian::native_to_big(value);
}

uint32_t htonl(uint32_t value) {
    return boost::endian::native_to_big(value);
}

uint64_t htonll(uint64_t value) {
    return boost::endian::native_to_big(value);
}

uint16_t ntohs(uint16_t value) {
    return boost::endian::big_to_native(value);
}

uint32_t ntohl(uint32_t value) {
    return boost::endian::big_to_native(value);
}

uint64_t ntohll(uint64_t value) {
    return boost::endian::big_to_native(value);
}

void writeVarInt(DynamicBuffer& buffer, uint64_t value) {
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        
        if (value != 0) {
            byte |= 0x80;
        }
        
        writeUInt8(buffer, byte);
    } while (value != 0);
}

uint64_t readVarInt(const DynamicBuffer& buffer, size_t& offset) {
    uint64_t result = 0;
    int shift = 0;
    uint8_t byte;
    
    do {
        byte = readUInt8(buffer, offset);
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        
        if (shift > 63) {
            throw std::runtime_error("VarInt trop long");
        }
    } while (byte & 0x80);
    
    return result;
}

bool canRead(const DynamicBuffer& buffer, size_t offset, size_t bytesToRead) {
    return offset + bytesToRead <= buffer.size();
}

void ensureCanRead(const DynamicBuffer& buffer, size_t offset, size_t bytesToRead) {
    if (!canRead(buffer, offset, bytesToRead)) {
        throw std::out_of_range("Tentative de lecture au-delà de la fin du buffer");
    }
}

}