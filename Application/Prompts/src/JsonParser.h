#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

class JsonValue {
public:
    enum Type { Null, Bool, Number, String, Array, Object };
    
    JsonValue() : type_(Null), num_(0), bool_(false) {}
    
    static JsonValue parse(const std::string &json);
    static JsonValue parse(const char *start, const char *end);
    
    Type type() const { return type_; }
    double number() const { return num_; }
    bool boolean() const { return bool_; }
    const std::string &string() const { return str_; }
    const std::vector<JsonValue> &array() const { return arr_; }
    const std::map<std::string, JsonValue> &object() const { return obj_; }
    
    bool has(const std::string &key) const;
    const JsonValue &get(const std::string &key) const;
    const JsonValue &operator[](size_t i) const;
    const JsonValue &operator[](const std::string &key) const;
    
    std::string serialize(bool pretty = false, int indent = 0) const;
    
    static JsonValue null() { return JsonValue(); }
    static JsonValue fromString(const std::string &s);
    static JsonValue fromDouble(double d);
    static JsonValue fromBool(bool b);
    static JsonValue fromArray(const std::vector<JsonValue> &a);
    static JsonValue fromObject(const std::map<std::string, JsonValue> &o);

private:
    Type type_;
    double num_;
    bool bool_;
    std::string str_;
    std::vector<JsonValue> arr_;
    std::map<std::string, JsonValue> obj_;
    
    struct Parser {
        const char *p;
        const char *end;
        Parser(const char *s, const char *e) : p(s), end(e) {}
        void skipWS();
        JsonValue parseValue();
        JsonValue parseString();
        JsonValue parseNumber();
        JsonValue parseArray();
        JsonValue parseObject();
        std::string parseRawString();
    };
};
