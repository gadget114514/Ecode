#include "Base64.h"
#include <cstring>

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b64url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static unsigned char b64rev[256] = {0};

static bool b64rev_init = false;
static void init_b64rev() {
    if (b64rev_init) return;
    b64rev_init = true;
    memset(b64rev, 0xFF, sizeof(b64rev));
    for (int i = 0; i < 64; i++) {
        b64rev[(unsigned char)b64[i]] = (unsigned char)i;
        b64rev[(unsigned char)b64url[i]] = (unsigned char)i;
    }
}

std::string Base64::encode(const std::string &data) {
    return encode((const unsigned char*)data.data(), data.size());
}

std::string Base64::encode(const unsigned char *data, size_t len) {
    std::string result;
    result.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned char b0 = data[i];
        unsigned char b1 = (i + 1 < len) ? data[i + 1] : 0;
        unsigned char b2 = (i + 2 < len) ? data[i + 2] : 0;
        result += b64[b0 >> 2];
        result += b64[((b0 & 3) << 4) | (b1 >> 4)];
        result += (i + 1 < len) ? b64[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        result += (i + 2 < len) ? b64[b2 & 63] : '=';
    }
    return result;
}

bool Base64::isValid(const std::string &s) {
    if (s.empty()) return true;
    for (char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '+' || c == '/' ||
              c == '-' || c == '_' || c == '='))
            return false;
    }
    if (s.length() % 4 == 1) return false;
    return true;
}

std::string Base64::decode(const std::string &encoded) {
    init_b64rev();
    if (encoded.empty()) return {};
    std::string result;
    result.reserve(encoded.length() / 4 * 3);
    unsigned char buf[4];
    for (size_t i = 0; i < encoded.length(); i += 4) {
        int pad = 0;
        for (int j = 0; j < 4; j++) {
            if (i + j >= encoded.length()) { buf[j] = 0; continue; }
            char c = encoded[i + j];
            if (c == '=') { buf[j] = 0; pad++; }
            else { buf[j] = b64rev[(unsigned char)c]; }
        }
        int outBytes = 3 - pad;
        if (outBytes >= 1) result += (char)((buf[0] << 2) | (buf[1] >> 4));
        if (outBytes >= 2) result += (char)(((buf[1] & 15) << 4) | (buf[2] >> 2));
        if (outBytes >= 3) result += (char)(((buf[2] & 3) << 6) | buf[3]);
    }
    return result;
}
