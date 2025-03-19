#ifndef WRAP_BOOST_SERIALISATION_H
#define WRAP_BOOST_SERIALISATION_H

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/base_object.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <type_traits>

namespace wrap_boost {

enum class SerializationFormat {
    BINARY,         
    TEXT,           
    JSON,          
    XML           
};

class SerializationError : public std::runtime_error {
public:
    explicit SerializationError(const std::string& message) 
        : std::runtime_error(message) {}
};

class DeserializationError : public std::runtime_error {
public:
    explicit DeserializationError(const std::string& message) 
        : std::runtime_error(message) {}
};

class ISerializable {
public:
    virtual ~ISerializable() = default;
    
    virtual std::vector<uint8_t> serialize(SerializationFormat format = SerializationFormat::BINARY) const = 0;
    virtual void deserialize(const std::vector<uint8_t>& data, SerializationFormat format = SerializationFormat::BINARY) = 0;
};

class Serializer {
public:
    template<typename T>
    static std::vector<uint8_t> serialize(const T& obj, SerializationFormat format = SerializationFormat::BINARY) {
        try {
            std::ostringstream oss;
            
            switch (format) {
                case SerializationFormat::BINARY:
                {
                    boost::archive::binary_oarchive oa(oss);
                    oa << obj;
                    break;
                }
                case SerializationFormat::TEXT:
                {
                    boost::archive::text_oarchive oa(oss);
                    oa << obj;
                    break;
                }
                case SerializationFormat::JSON:
                case SerializationFormat::XML:
                    throw SerializationError("Format non supporté actuellement");
            }
            
            std::string str = oss.str();
            return std::vector<uint8_t>(str.begin(), str.end());
        }
        catch (const std::exception& e) {
            throw SerializationError(std::string("Erreur de sérialisation: ") + e.what());
        }
    }
    
    template<typename T>
    static void deserialize(const std::vector<uint8_t>& data, T& obj, SerializationFormat format = SerializationFormat::BINARY) {
        if (data.empty()) {
            throw DeserializationError("Données vides");
        }
        
        try {
            std::string str(data.begin(), data.end());
            std::istringstream iss(str);
            
            switch (format) {
                case SerializationFormat::BINARY:
                {
                    boost::archive::binary_iarchive ia(iss);
                    ia >> obj;
                    break;
                }
                case SerializationFormat::TEXT:
                {
                    boost::archive::text_iarchive ia(iss);
                    ia >> obj;
                    break;
                }
                case SerializationFormat::JSON:
                case SerializationFormat::XML:
                    throw DeserializationError("Format non supporté actuellement");
            }
        }
        catch (const std::exception& e) {
            throw DeserializationError(std::string("Erreur de désérialisation: ") + e.what());
        }
    }
    
    template<typename T>
    static T deserialize(const std::vector<uint8_t>& data, SerializationFormat format = SerializationFormat::BINARY) {
        T obj;
        deserialize(data, obj, format);
        return obj;
    }
};

template<typename Derived>
class SerializableMixin : public ISerializable {
public:
    virtual std::vector<uint8_t> serialize(SerializationFormat format = SerializationFormat::BINARY) const override {
        return Serializer::serialize(static_cast<const Derived&>(*this), format);
    }
    
    virtual void deserialize(const std::vector<uint8_t>& data, SerializationFormat format = SerializationFormat::BINARY) override {
        Serializer::deserialize(data, static_cast<Derived&>(*this), format);
    }
};

class SerializationRegistry {
public:
    template<typename Base, typename Derived>
    static void registerType() {
        static_assert(std::is_base_of<Base, Derived>::value, "Derived must inherit from Base");
        boost::serialization::void_cast_register<Derived, Base>();
    }
};

class SerializedData {
public:
    SerializedData() : format_(SerializationFormat::BINARY) {}
    
    SerializedData(const std::vector<uint8_t>& data, SerializationFormat format = SerializationFormat::BINARY)
        : data_(data), format_(format) {}
    
    SerializedData(std::vector<uint8_t>&& data, SerializationFormat format = SerializationFormat::BINARY)
        : data_(std::move(data)), format_(format) {}
    
    const std::vector<uint8_t>& getData() const {
        return data_;
    }
    
    std::vector<uint8_t>& getData() {
        return data_;
    }
    
    SerializationFormat getFormat() const {
        return format_;
    }
    
    void setFormat(SerializationFormat format) {
        format_ = format;
    }
    
    size_t size() const {
        return data_.size();
    }
    
    bool empty() const {
        return data_.empty();
    }
    
    std::string toHexString() const;
    
    std::string toBase64() const;
    static SerializedData fromBase64(const std::string& base64, SerializationFormat format = SerializationFormat::BINARY);
    
    template<typename T>
    void serializeObject(const T& obj) {
        data_ = Serializer::serialize(obj, format_);
    }
    
    template<typename T>
    void deserializeObject(T& obj) const {
        Serializer::deserialize(data_, obj, format_);
    }
    
    template<typename T>
    T deserializeObject() const {
        return Serializer::deserialize<T>(data_, format_);
    }
    
private:
    std::vector<uint8_t> data_;
    SerializationFormat format_;
    
    friend class boost::serialization::access;
    
    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*version*/) {
        ar & data_;
        ar & format_;
    }
};

template<typename IntType>
std::vector<uint8_t> serializeCompactInt(IntType value) {
    static_assert(std::is_integral<IntType>::value, "Type must be an integer");
    
    std::vector<uint8_t> result;
    
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        
        if (value != 0) {
            byte |= 0x80;
        }
        
        result.push_back(byte);
    } while (value != 0);
    
    return result;
}

template<typename IntType>
IntType deserializeCompactInt(const std::vector<uint8_t>& data, size_t& pos) {
    static_assert(std::is_integral<IntType>::value, "Type must be an integer");
    
    IntType result = 0;
    int shift = 0;
    uint8_t byte;
    
    do {
        if (pos >= data.size()) {
            throw DeserializationError("Fin prématurée des données");
        }
        
        byte = data[pos++];
        result |= static_cast<IntType>(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    
    return result;
}

std::vector<uint8_t> compressData(const std::vector<uint8_t>& data);
std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressedData);

} 

#endif 