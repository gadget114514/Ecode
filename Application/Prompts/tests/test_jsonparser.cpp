#include "../src/JsonParser.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestNull() {
    auto v = JsonValue::parse("null");
    VERIFY(v.type() == JsonValue::Null, "null type should be Null");
    VERIFY(v.serialize() == "null", "null serialize should be 'null'");
    std::cout << "Test Passed: Null" << std::endl;
}

void TestBool() {
    auto t = JsonValue::parse("true");
    VERIFY(t.type() == JsonValue::Bool, "true type should be Bool");
    VERIFY(t.boolean() == true, "true value should be true");
    VERIFY(t.serialize() == "true", "true serialize");

    auto f = JsonValue::parse("false");
    VERIFY(f.type() == JsonValue::Bool, "false type should be Bool");
    VERIFY(f.boolean() == false, "false value should be false");
    VERIFY(f.serialize() == "false", "false serialize");
    std::cout << "Test Passed: Boolean" << std::endl;
}

void TestNumbers() {
    auto i = JsonValue::parse("42");
    VERIFY(i.type() == JsonValue::Number, "integer type should be Number");
    VERIFY(i.number() == 42.0, "integer value should be 42");

    auto neg = JsonValue::parse("-17");
    VERIFY(neg.number() == -17.0, "negative integer");

    auto flt = JsonValue::parse("3.14");
    VERIFY(flt.number() == 3.14, "float value");

    auto sci = JsonValue::parse("1.5e10");
    VERIFY(sci.number() == 1.5e10, "scientific notation");

    auto zero = JsonValue::parse("0");
    VERIFY(zero.number() == 0.0, "zero");
    std::cout << "Test Passed: Numbers" << std::endl;
}

void TestStrings() {
    auto s = JsonValue::parse("\"hello\"");
    VERIFY(s.type() == JsonValue::String, "string type should be String");
    VERIFY(s.string() == "hello", "string value");

    auto empty = JsonValue::parse("\"\"");
    VERIFY(empty.string() == "", "empty string");

    auto esc = JsonValue::parse("\"hello\\nworld\"");
    VERIFY(esc.string() == "hello\nworld", "escaped newline");

    auto esc2 = JsonValue::parse("\"tab\\there\"");
    VERIFY(esc2.string() == "tab\there", "escaped tab");

    auto esc3 = JsonValue::parse("\"quo\\\"te\"");
    VERIFY(esc3.string() == "quo\"te", "escaped quote");

    auto esc4 = JsonValue::parse("\"back\\\\slash\"");
    VERIFY(esc4.string() == "back\\slash", "escaped backslash");

    std::cout << "Test Passed: Strings" << std::endl;
}

void TestUnicodeEscape() {
    auto u = JsonValue::parse("\"\\u0048\\u0065\\u006C\\u006C\\u006F\"");
    VERIFY(u.string() == "Hello", "unicode escape sequence");
    std::cout << "Test Passed: Unicode Escape" << std::endl;
}

void TestArrays() {
    auto arr = JsonValue::parse("[1,2,3]");
    VERIFY(arr.type() == JsonValue::Array, "array type");
    VERIFY(arr.array().size() == 3, "array size 3");
    VERIFY(arr[0].number() == 1.0, "arr[0]");
    VERIFY(arr[1].number() == 2.0, "arr[1]");
    VERIFY(arr[2].number() == 3.0, "arr[2]");

    auto empty = JsonValue::parse("[]");
    VERIFY(empty.array().empty(), "empty array");

    auto nested = JsonValue::parse("[[1,2],[3,4]]");
    VERIFY(nested.array().size() == 2, "nested array size");
    VERIFY(nested[0][0].number() == 1.0, "nested[0][0]");
    VERIFY(nested[1][1].number() == 4.0, "nested[1][1]");

    auto mixed = JsonValue::parse("[1,\"two\",true,null]");
    VERIFY(mixed.array().size() == 4, "mixed array size");
    VERIFY(mixed[1].string() == "two", "mixed[1]");
    VERIFY(mixed[2].boolean() == true, "mixed[2]");
    VERIFY(mixed[3].type() == JsonValue::Null, "mixed[3]");
    std::cout << "Test Passed: Arrays" << std::endl;
}

void TestObjects() {
    auto obj = JsonValue::parse("{\"key\":\"value\",\"num\":42}");
    VERIFY(obj.type() == JsonValue::Object, "object type");
    VERIFY(obj.has("key"), "has key");
    VERIFY(obj.has("num"), "has num");
    VERIFY(obj["key"].string() == "value", "obj.key");
    VERIFY(obj["num"].number() == 42.0, "obj.num");

    auto empty = JsonValue::parse("{}");
    VERIFY(empty.type() == JsonValue::Object, "empty object");
    VERIFY(empty.object().empty(), "empty object has no keys");

    auto nested = JsonValue::parse("{\"outer\":{\"inner\":123}}");
    VERIFY(nested["outer"]["inner"].number() == 123.0, "nested object access");
    std::cout << "Test Passed: Objects" << std::endl;
}

void TestRoundTrip() {
    std::vector<std::string> cases = {
        "null",
        "true",
        "false",
        "42",
        "-3.14",
        "\"hello\"",
        "\"\\n\\t\\r\"",
        "[1,2,3]",
        "{\"a\":1,\"b\":2}",
        "{\"nested\":{\"array\":[1,2,3],\"obj\":{}}}",
        "[{\"x\":1},{\"x\":2}]",
    };
    for (auto &input : cases) {
        auto parsed = JsonValue::parse(input);
        std::string output = parsed.serialize();
        auto reparsed = JsonValue::parse(output);
        std::string reoutput = reparsed.serialize();
        VERIFY(output == reoutput,
            "round-trip serialize mismatch for input=" + input + " got=" + output + " reout=" + reoutput);
    }
    std::cout << "Test Passed: Round-Trip Serialize" << std::endl;
}

void TestPrettyPrint() {
    auto v = JsonValue::parse("{\"a\":1,\"b\":[2,3]}");
    std::string pretty = v.serialize(true);
    VERIFY(!pretty.empty(), "pretty print should produce output");
    VERIFY(pretty.find('\n') != std::string::npos, "pretty print should contain newlines");
    VERIFY(pretty.find(' ') != std::string::npos, "pretty print should contain spaces");

    auto reparsed = JsonValue::parse(pretty);
    VERIFY(reparsed["a"].number() == 1.0, "pretty print round-trip a");
    VERIFY(reparsed["b"][0].number() == 2.0, "pretty print round-trip b[0]");
    std::cout << "Test Passed: Pretty Print" << std::endl;
}

void TestEdgeCases() {
    auto ws = JsonValue::parse("  {  \"key\"  :  \"value\"  }  ");
    VERIFY(ws["key"].string() == "value", "whitespace handling");

    auto unicode = JsonValue::parse("\"\\u00E9\"");
    VERIFY(unicode.string() == "\xC3\xA9", "unicode e-acute");

    auto special = JsonValue::parse("\"\\/\"");
    VERIFY(special.string() == "/", "escaped slash");
    std::cout << "Test Passed: Edge Cases" << std::endl;
}

void TestLargeJson() {
    std::map<std::string, JsonValue> obj;
    for (int i = 0; i < 100; i++) {
        obj["key" + std::to_string(i)] = JsonValue::fromDouble(i);
    }
    auto v = JsonValue::fromObject(obj);
    std::string json = v.serialize();
    auto reparsed = JsonValue::parse(json);
    VERIFY(reparsed["key99"].number() == 99.0, "large object round-trip");
    std::cout << "Test Passed: Large JSON (100 keys)" << std::endl;
}

void TestManyJsonCases() {
    // 1. 50 valid JSON cases and their expected types
    struct ValidCase {
        std::string json;
        JsonValue::Type expectedType;
    };
    std::vector<ValidCase> validCases = {
        {"100", JsonValue::Number},
        {"-100", JsonValue::Number},
        {"100.5", JsonValue::Number},
        {"-100.5", JsonValue::Number},
        {"1e2", JsonValue::Number},
        {"1.5e-2", JsonValue::Number},
        {"0.0001", JsonValue::Number},
        {"-0.0", JsonValue::Number},
        {"true", JsonValue::Bool},
        {"false", JsonValue::Bool},
        {"null", JsonValue::Null},
        {"\"\"", JsonValue::String},
        {"\"a\"", JsonValue::String},
        {"\"abc\"", JsonValue::String},
        {"\"\\\"\"", JsonValue::String},
        {"\"\\\\\"", JsonValue::String},
        {"\"\\/\"", JsonValue::String},
        {"\"\\b\"", JsonValue::String},
        {"\"\\f\"", JsonValue::String},
        {"\"\\n\"", JsonValue::String},
        {"\"\\r\"", JsonValue::String},
        {"\"\\t\"", JsonValue::String},
        {"\"\\u0000\"", JsonValue::String},
        {"\"\\u001f\"", JsonValue::String},
        {"\"\\u1234\"", JsonValue::String},
        {"[]", JsonValue::Array},
        {"[1]", JsonValue::Array},
        {"[1,2]", JsonValue::Array},
        {"[1,2,3]", JsonValue::Array},
        {"[\"a\"]", JsonValue::Array},
        {"[\"a\",\"b\"]", JsonValue::Array},
        {"[true,false,null]", JsonValue::Array},
        {"[[]]", JsonValue::Array},
        {"[[1],[2,3]]", JsonValue::Array},
        {"{}", JsonValue::Object},
        {"{\"a\":1}", JsonValue::Object},
        {"{\"a\":1,\"b\":2}", JsonValue::Object},
        {"{\"a\":true,\"b\":null}", JsonValue::Object},
        {"{\"a\":[]}", JsonValue::Object},
        {"{\"a\":{}}", JsonValue::Object},
        {"{\"a\":{\"b\":{\"c\":1}}}", JsonValue::Object},
        {" \n\t\r 100 \n\t\r ", JsonValue::Number},
        {" \n\t\r [ \n\t\r ] \n\t\r ", JsonValue::Array},
        {" \n\t\r { \n\t\r } \n\t\r ", JsonValue::Object},
        {"{\"spaced key\" : \"value\"}", JsonValue::Object},
        {"[ 1 , 2 , 3 ]", JsonValue::Array},
        {"{\"a\":[1,2,{\"b\":true}]}", JsonValue::Object},
        {"\"escaped: \\\\ \\\" \\/ \\b \\f \\n \\r \\t\"", JsonValue::String},
        {"123456789.987654321", JsonValue::Number},
        {"-123456789.987654321", JsonValue::Number}
    };

    int caseId = 0;
    for (const auto &c : validCases) {
        auto val = JsonValue::parse(c.json);
        VERIFY(val.type() == c.expectedType, "valid json case #" + std::to_string(caseId) + " (" + c.json + ") type mismatch, expected " + std::to_string(c.expectedType) + " got " + std::to_string(val.type()));
        caseId++;
    }

    // 2. 50 invalid JSON cases (they should parse to Null or have invalid structures instead of crashing)
    // Note: since our parser doesn't throw exceptions, we verify it handles invalid input without crashing.
    std::vector<std::string> invalidCases = {
        "{",
        "}",
        "[",
        "]",
        "{\"a\"}",
        "{\"a\":}",
        "{\"a\":1",
        "{\"a\":1,",
        "{\"a\":1,}",
        "{\"a\":1 \"b\":2}",
        "{\"a\"::1}",
        "{a:1}",
        "{'a':1}",
        "{\"a\":'b'}",
        "[1,",
        "[1,]",
        "[1 2]",
        "[,1]",
        "\"unterminated",
        "\"escaped unclosed \\\"",
        "\"invalid escape \\x\"",
        "\"invalid unicode \\u12\"",
        "\"invalid unicode \\u123z\"",
        "tru",
        "fals",
        "nul",
        "1.2.3",
        "1e2e3",
        "1.5e-+2",
        "1.5e+-2",
        "--1",
        "++1",
        "1+",
        "1-",
        "0123", // octal-like is often invalid in standard JSON but let's check it doesn't crash
        "NaN",
        "Infinity",
        "-Infinity",
        "",
        " ",
        "\n",
        "[,]",
        "{\"a\":1,,\"b\":2}",
        "[1,,2]",
        "\"\\u\"",
        "\"\\u0\"",
        "\"\\u00\"",
        "\"\\u000\"",
        "\"\\u000g\"",
        "{\"a\":{\"b\":[1,2"
    };

    caseId = 0;
    for (const auto &json : invalidCases) {
        // Just make sure it doesn't crash.
        auto val = JsonValue::parse(json);
        (void)val;
        caseId++;
    }
    std::cout << "Test Passed: Many JSON Cases (100+ validation test scenarios)" << std::endl;
}

int main() {
    try {
        TestNull();
        TestBool();
        TestNumbers();
        TestStrings();
        TestUnicodeEscape();
        TestArrays();
        TestObjects();
        TestRoundTrip();
        TestPrettyPrint();
        TestEdgeCases();
        TestLargeJson();
        TestManyJsonCases();
        std::cout << "=== ALL JSON PARSER TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
