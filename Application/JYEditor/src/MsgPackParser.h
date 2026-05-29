#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

using json = nlohmann::json;

class MsgPackParser {
public:
    explicit MsgPackParser(const std::vector<uint8_t>& data)
        : m_data(data), m_pos(0) {}

    json parse() {
        if (m_data.empty())
            return json();
        return decodeValue();
    }

private:
    const std::vector<uint8_t>& m_data;
    size_t m_pos;

    uint8_t readByte() {
        if (m_pos >= m_data.size())
            throw std::runtime_error("msgpack: unexpected end of data");
        return m_data[m_pos++];
    }

    template<typename T>
    T readBigEndian() {
        T val = 0;
        for (size_t i = 0; i < sizeof(T); i++) {
            val = (val << 8) | readByte();
        }
        return val;
    }

    std::vector<uint8_t> readBytes(size_t len) {
        if (m_pos + len > m_data.size())
            throw std::runtime_error("msgpack: unexpected end of data");
        std::vector<uint8_t> buf(m_data.begin() + m_pos, m_data.begin() + m_pos + len);
        m_pos += len;
        return buf;
    }

    static std::string base64Encode(const std::vector<uint8_t>& data) {
        static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        result.reserve((data.size() + 2) / 3 * 4);
        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t b = ((uint32_t)data[i] << 16);
            if (i + 1 < data.size()) b |= ((uint32_t)data[i + 1] << 8);
            if (i + 2 < data.size()) b |= data[i + 2];
            result += table[(b >> 18) & 0x3f];
            result += table[(b >> 12) & 0x3f];
            result += (i + 1 < data.size()) ? table[(b >> 6) & 0x3f] : '=';
            result += (i + 2 < data.size()) ? table[b & 0x3f] : '=';
        }
        return result;
    }

    json decodeValue() {
        if (m_pos >= m_data.size())
            throw std::runtime_error("msgpack: unexpected end of data");

        uint8_t b = m_data[m_pos];

        // positive fixint: 0x00 - 0x7f
        if (b <= 0x7f) {
            m_pos++;
            return json((int64_t)b);
        }

        // fixmap: 0x80 - 0x8f
        if (b >= 0x80 && b <= 0x8f) {
            return decodeFixMap(b & 0x0f);
        }

        // fixarray: 0x90 - 0x9f
        if (b >= 0x90 && b <= 0x9f) {
            return decodeFixArray(b & 0x0f);
        }

        // fixstr: 0xa0 - 0xbf
        if (b >= 0xa0 && b <= 0xbf) {
            return decodeFixStr(b & 0x1f);
        }

        // negative fixint: 0xe0 - 0xff
        if (b >= 0xe0) {
            m_pos++;
            return json((int64_t)(int8_t)b);
        }

        switch (b) {
        case 0xc0: // nil
            m_pos++;
            return json();
        case 0xc2: // false
            m_pos++;
            return json(false);
        case 0xc3: // true
            m_pos++;
            return json(true);
        case 0xc4: // bin 8
            return decodeBin(8);
        case 0xc5: // bin 16
            return decodeBin(16);
        case 0xc6: // bin 32
            return decodeBin(32);
        case 0xca: // float 32
            m_pos++;
            return decodeFloat32();
        case 0xcb: // float 64
            m_pos++;
            return decodeFloat64();
        case 0xcc: // uint 8
            m_pos++;
            return json((int64_t)readByte());
        case 0xcd: // uint 16
            m_pos++;
            return json((int64_t)readBigEndian<uint16_t>());
        case 0xce: // uint 32
            m_pos++;
            return json((int64_t)readBigEndian<uint32_t>());
        case 0xcf: // uint 64
            m_pos++;
            return json((int64_t)readBigEndian<uint64_t>());
        case 0xd0: // int 8
            m_pos++;
            return json((int64_t)(int8_t)readByte());
        case 0xd1: // int 16
            m_pos++;
            return json((int64_t)(int16_t)readBigEndian<uint16_t>());
        case 0xd2: // int 32
            m_pos++;
            return json((int64_t)(int32_t)readBigEndian<uint32_t>());
        case 0xd3: // int 64
            m_pos++;
            return json((int64_t)readBigEndian<uint64_t>());
        case 0xd9: // str 8
            return decodeStr(8);
        case 0xda: // str 16
            return decodeStr(16);
        case 0xdb: // str 32
            return decodeStr(32);
        case 0xdc: // array 16
            return decodeArray(16);
        case 0xdd: // array 32
            return decodeArray(32);
        case 0xde: // map 16
            return decodeMap(16);
        case 0xdf: // map 32
            return decodeMap(32);
        default:
            throw std::runtime_error("msgpack: unknown byte: 0x" +
                std::to_string(b));
        }
    }

    json decodeFixMap(uint8_t n) {
        m_pos++;
        json obj = json::object();
        for (uint8_t i = 0; i < n; i++) {
            json key = decodeValue();
            std::string k = key.is_string() ? key.get<std::string>() : key.dump();
            obj[k] = decodeValue();
        }
        return obj;
    }

    json decodeFixArray(uint8_t n) {
        m_pos++;
        json arr = json::array();
        for (uint8_t i = 0; i < n; i++)
            arr.push_back(decodeValue());
        return arr;
    }

    json decodeFixStr(uint8_t len) {
        m_pos++;
        auto bytes = readBytes(len);
        return json(std::string(bytes.begin(), bytes.end()));
    }

    json decodeStr(uint8_t sizeBytes) {
        m_pos++;
        uint32_t len = 0;
        if (sizeBytes == 8) len = readByte();
        else if (sizeBytes == 16) len = readBigEndian<uint16_t>();
        else len = readBigEndian<uint32_t>();
        auto bytes = readBytes(len);
        return json(std::string(bytes.begin(), bytes.end()));
    }

    json decodeBin(uint8_t sizeBytes) {
        m_pos++;
        uint32_t len = 0;
        if (sizeBytes == 8) len = readByte();
        else if (sizeBytes == 16) len = readBigEndian<uint16_t>();
        else len = readBigEndian<uint32_t>();
        auto bytes = readBytes(len);
        return json({{"$binary", base64Encode(bytes)}});
    }

    json decodeArray(uint8_t sizeBytes) {
        m_pos++;
        uint32_t len = (sizeBytes == 16) ? readBigEndian<uint16_t>() : readBigEndian<uint32_t>();
        json arr = json::array();
        for (uint32_t i = 0; i < len; i++)
            arr.push_back(decodeValue());
        return arr;
    }

    json decodeMap(uint8_t sizeBytes) {
        m_pos++;
        uint32_t len = (sizeBytes == 16) ? readBigEndian<uint16_t>() : readBigEndian<uint32_t>();
        json obj = json::object();
        for (uint32_t i = 0; i < len; i++) {
            json key = decodeValue();
            std::string k = key.is_string() ? key.get<std::string>() : key.dump();
            obj[k] = decodeValue();
        }
        return obj;
    }

    json decodeFloat32() {
        uint32_t bits = readBigEndian<uint32_t>();
        float f;
        memcpy(&f, &bits, sizeof(f));
        return json((double)f);
    }

    json decodeFloat64() {
        uint64_t bits = readBigEndian<uint64_t>();
        double d;
        memcpy(&d, &bits, sizeof(d));
        return json(d);
    }
};
