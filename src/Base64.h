#pragma once
#include <string>
#include <vector>

class Base64 {
public:
    static std::string encode(const std::string &data);
    static std::string decode(const std::string &encoded);
    static std::string encode(const unsigned char *data, size_t len);
    static bool isValid(const std::string &s);
};
