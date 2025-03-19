#include "boost/wrap_boost_serialization.h"
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
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/beast/core/detail/base64.hpp>

#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace wrap_boost {

std::string SerializedData::toHexString() const {
    std::ostringstream oss;
    
    for (const auto& byte : data_) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    
    return oss.str();
}

std::string SerializedData::toBase64() const {
    using namespace boost::beast::detail;
    
    std::string result;
    result.resize(base64::encoded_size(data_.size()));
    
    base64::encode(result.data(), data_.data(), data_.size());
    
    return result;
}

SerializedData SerializedData::fromBase64(const std::string& base64, SerializationFormat format) {
    using namespace boost::beast::detail;
    
    std::vector<uint8_t> result;
    result.resize(base64::decoded_size(base64.size()));
    
    auto [decodedSize, _] = base64::decode(result.data(), base64.data(), base64.size());
    result.resize(decodedSize);
    
    return SerializedData(std::move(result), format);
}

std::vector<uint8_t> compressData(const std::vector<uint8_t>& data) {
    std::stringstream compressed;
    std::stringstream source(std::string(data.begin(), data.end()));
    
    boost::iostreams::filtering_streambuf<boost::iostreams::input> out;
    out.push(boost::iostreams::gzip_compressor());
    out.push(source);
    boost::iostreams::copy(out, compressed);
    
    std::string str = compressed.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressedData) {
    std::stringstream compressed(std::string(compressedData.begin(), compressedData.end()));
    std::stringstream decompressed;
    
    boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
    in.push(boost::iostreams::gzip_decompressor());
    in.push(compressed);
    boost::iostreams::copy(in, decompressed);
    
    std::string str = decompressed.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

}