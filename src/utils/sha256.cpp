#include "utils/sha256.h"

namespace p2p {

    uint32_t SHA256::rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    uint32_t SHA256::ch(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (~x & z);
    }

    uint32_t SHA256::maj(uint32_t x, uint32_t y, uint32_t z) {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    uint32_t SHA256::sigma0(uint32_t x) {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }

    uint32_t SHA256::sigma1(uint32_t x) {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }

    uint32_t SHA256::gamma0(uint32_t x) {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }

    uint32_t SHA256::gamma1(uint32_t x) {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    SHA256::SHA256() {
        reset();
    }

    void SHA256::reset() {
        m_state = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };
        m_buffer.clear();
        m_bit_length = 0;
    }

    void SHA256::transform() {
        std::array<uint32_t, 64> m;
        
        for (int i = 0, j = 0; i < 16; ++i, j += 4) {
            m[i] = (static_cast<uint32_t>(m_buffer[j]) << 24) |
                   (static_cast<uint32_t>(m_buffer[j + 1]) << 16) |
                   (static_cast<uint32_t>(m_buffer[j + 2]) << 8) |
                   (static_cast<uint32_t>(m_buffer[j + 3]));
        }
        
        for (int i = 16; i < 64; ++i) {
            m[i] = gamma1(m[i - 2]) + m[i - 7] + gamma0(m[i - 15]) + m[i - 16];
        }
        
        uint32_t a = m_state[0];
        uint32_t b = m_state[1];
        uint32_t c = m_state[2];
        uint32_t d = m_state[3];
        uint32_t e = m_state[4];
        uint32_t f = m_state[5];
        uint32_t g = m_state[6];
        uint32_t h = m_state[7];
        
        for (int i = 0; i < 64; ++i) {
            uint32_t temp1 = h + sigma1(e) + ch(e, f, g) + K[i] + m[i];
            uint32_t temp2 = sigma0(a) + maj(a, b, c);
            
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    void SHA256::update(const uint8_t* data, size_t length) {
        for (size_t i = 0; i < length; ++i) {
            m_buffer.push_back(data[i]);
            if (m_buffer.size() == 64) {
                transform();
                m_bit_length += 512;
                m_buffer.clear();
            }
        }
    }

    void SHA256::update(const std::string& data) {
        update(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
    }

    std::string SHA256::finalize() {
        m_bit_length += m_buffer.size() * 8;
        m_buffer.push_back(0x80);
        while (m_buffer.size() < 56) {
            m_buffer.push_back(0);
        }
        for (int i = 7; i >= 0; --i) {
            m_buffer.push_back(static_cast<uint8_t>((m_bit_length >> (i * 8)) & 0xff));
        }
        transform();
        std::stringstream ss;
        for (uint32_t v : m_state) {
            ss << std::hex << std::setw(8) << std::setfill('0') << v;
        }
        reset();
        return ss.str();
    }

    std::string SHA256::hashFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        SHA256 hasher;
        constexpr size_t buffer_size = 8192;
        std::vector<uint8_t> buffer(buffer_size);
        
        while (file.good()) {
            file.read(reinterpret_cast<char*>(buffer.data()), buffer_size);
            hasher.update(buffer.data(), file.gcount());
        }
        
        return hasher.finalize();
    }

} // namespace p2p