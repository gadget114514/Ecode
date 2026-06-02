#include "JsonParser.h"
#include <algorithm>
#include <sstream>
#include <cstdlib>

void JsonValue::Parser::skipWS() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
}

std::string JsonValue::Parser::parseRawString() {
    if (p >= end || *p != '"') return "";
    ++p; // skip opening quote
    std::string result;
    while (p < end && *p != '"') {
        if (*p == '\\') {
            ++p;
            if (p >= end) break;
            switch (*p) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    // simple 4-hex-digit unicode
                    char hex[5] = {0};
                    for (int i = 0; i < 4; i++) {
                        if (p + 1 < end) { ++p; hex[i] = *p; }
                    }
                    unsigned short cp = (unsigned short)strtoul(hex, nullptr, 16);
                    if (cp < 0x80) result += (char)cp;
                    else if (cp < 0x800) { result += (char)(0xC0 | (cp >> 6)); result += (char)(0x80 | (cp & 0x3F)); }
                    else { result += (char)(0xE0 | (cp >> 12)); result += (char)(0x80 | ((cp >> 6) & 0x3F)); result += (char)(0x80 | (cp & 0x3F)); }
                    break;
                }
                default: result += *p; break;
            }
        } else {
            result += *p;
        }
        ++p;
    }
    if (p < end) ++p; // skip closing quote
    return result;
}

JsonValue JsonValue::Parser::parseString() {
    return JsonValue::fromString(parseRawString());
}

JsonValue JsonValue::Parser::parseNumber() {
    const char *start = p;
    if (*p == '-') ++p;
    while (p < end && *p >= '0' && *p <= '9') ++p;
    if (p < end && *p == '.') {
        ++p;
        while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    if (p < end && (*p == 'e' || *p == 'E')) {
        ++p;
        if (p < end && (*p == '+' || *p == '-')) ++p;
        while (p < end && *p >= '0' && *p <= '9') ++p;
    }
    double val = strtod(start, nullptr);
    return JsonValue::fromDouble(val);
}

JsonValue JsonValue::Parser::parseArray() {
    std::vector<JsonValue> arr;
    ++p; // skip '['
    skipWS();
    if (p < end && *p != ']') {
        arr.push_back(parseValue());
        skipWS();
        while (p < end && *p == ',') {
            ++p; skipWS();
            arr.push_back(parseValue());
            skipWS();
        }
    }
    if (p < end) ++p; // skip ']'
    return JsonValue::fromArray(arr);
}

JsonValue JsonValue::Parser::parseObject() {
    std::map<std::string, JsonValue> obj;
    ++p; // skip '{'
    skipWS();
    if (p < end && *p != '}') {
        if (*p == '"') {
            std::string key = parseRawString();
            skipWS();
            if (p < end && *p == ':') { ++p; skipWS(); }
            obj[key] = parseValue();
            skipWS();
        }
        while (p < end && *p == ',') {
            ++p; skipWS();
            if (p < end && *p == '"') {
                std::string key = parseRawString();
                skipWS();
                if (p < end && *p == ':') { ++p; skipWS(); }
                obj[key] = parseValue();
                skipWS();
            }
        }
    }
    if (p < end) ++p; // skip '}'
    return JsonValue::fromObject(obj);
}

JsonValue JsonValue::Parser::parseValue() {
    skipWS();
    if (p >= end) return JsonValue();
    switch (*p) {
        case '"': return parseString();
        case '{': return parseObject();
        case '[': return parseArray();
        case 't': p += 4; return JsonValue::fromBool(true);
        case 'f': p += 5; return JsonValue::fromBool(false);
        case 'n': p += 4; return JsonValue();
        default: return parseNumber();
    }
}

JsonValue JsonValue::parse(const std::string &json) {
    return parse(json.data(), json.data() + json.size());
}

JsonValue JsonValue::parse(const char *start, const char *end) {
    Parser parser(start, end);
    return parser.parseValue();
}

bool JsonValue::has(const std::string &key) const {
    return obj_.find(key) != obj_.end();
}

const JsonValue &JsonValue::get(const std::string &key) const {
    static JsonValue null;
    auto it = obj_.find(key);
    return it != obj_.end() ? it->second : null;
}

const JsonValue &JsonValue::operator[](size_t i) const {
    static JsonValue null;
    return i < arr_.size() ? arr_[i] : null;
}

const JsonValue &JsonValue::operator[](const std::string &key) const {
    return get(key);
}

std::string JsonValue::serialize(bool pretty, int indent) const {
    std::string result;
    std::string ind(indent, ' ');
    std::string ni = pretty ? std::string(indent + 2, ' ') : "";
    if (pretty && indent > 0) result = "\n" + ind;
    
    switch (type_) {
        case Null: result += "null"; break;
        case Bool: result += bool_ ? "true" : "false"; break;
        case Number: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", num_);
            result += buf;
            break;
        }
        case String: {
            result += '"';
            for (char c : str_) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b"; break;
                    case '\f': result += "\\f"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += c;
                }
            }
            result += '"';
            break;
        }
        case Array: {
            result += '[';
            for (size_t i = 0; i < arr_.size(); i++) {
                if (i > 0) result += ',';
                result += arr_[i].serialize(pretty, indent + 2);
            }
            result += ']';
            break;
        }
        case Object: {
            result += '{';
            bool first = true;
            for (auto &kv : obj_) {
                if (!first) result += ',';
                first = false;
                if (pretty) result += '\n' + ni;
                result += '"' + kv.first + "\":" + (pretty ? " " : "") + kv.second.serialize(pretty, indent + 2);
            }
            if (pretty && !obj_.empty()) result += '\n' + ind;
            result += '}';
            break;
        }
    }
    return result;
}

JsonValue JsonValue::fromString(const std::string &s) {
    JsonValue val;
    val.type_ = String;
    val.str_ = s;
    return val;
}

JsonValue JsonValue::fromDouble(double d) {
    JsonValue val;
    val.type_ = Number;
    val.num_ = d;
    return val;
}

JsonValue JsonValue::fromBool(bool b) {
    JsonValue val;
    val.type_ = Bool;
    val.bool_ = b;
    return val;
}

JsonValue JsonValue::fromArray(const std::vector<JsonValue> &a) {
    JsonValue val;
    val.type_ = Array;
    val.arr_ = a;
    return val;
}

JsonValue JsonValue::fromObject(const std::map<std::string, JsonValue> &o) {
    JsonValue val;
    val.type_ = Object;
    val.obj_ = o;
    return val;
}
