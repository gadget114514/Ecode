#include "../src/Base64.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestEncodeDecode() {
    std::vector<std::string> cases = {
        "",
        "f",
        "fo",
        "foo",
        "foob",
        "fooba",
        "foobar",
        "Hello, World!",
        "\x00\x01\x02\xFF\xFE",
        "The quick brown fox jumps over the lazy dog.",
        std::string(1000, 'A'),
    };
    for (auto &input : cases) {
        std::string encoded = Base64::encode(input);
        VERIFY(!encoded.empty() || input.empty(), "encode should not fail for input len=" + std::to_string(input.size()));

        std::string decoded = Base64::decode(encoded);
        VERIFY(decoded == input, "round-trip failed for input len=" + std::to_string(input.size()));

        VERIFY(Base64::isValid(encoded), "encoded string should be valid base64 for input len=" + std::to_string(input.size()));
    }
    std::cout << "Test Passed: Encode/Decode Round-Trip" << std::endl;
}

void TestEncodeDecodeBinary() {
    unsigned char binary[] = {0x00, 0x01, 0x02, 0x7F, 0x80, 0xFF, 0xAB, 0xCD};
    std::string encoded = Base64::encode(binary, sizeof(binary));
    VERIFY(!encoded.empty(), "encode binary should produce output");

    std::string decoded = Base64::decode(encoded);
    VERIFY(decoded.size() == sizeof(binary), "decoded binary size mismatch");
    VERIFY(memcmp(decoded.data(), binary, sizeof(binary)) == 0, "decoded binary content mismatch");
    std::cout << "Test Passed: Binary Encode/Decode" << std::endl;
}

void TestIsValid() {
    VERIFY(Base64::isValid(""), "empty string is valid");
    VERIFY(Base64::isValid("SGVsbG8="), "standard base64 should be valid");
    VERIFY(Base64::isValid("SGVsbG8"), "base64 without padding should be valid");
    VERIFY(Base64::isValid("SGVsbG8+"), "base64 with + should be valid");
    VERIFY(Base64::isValid("SGVsbG8/"), "base64 with / should be valid");

    VERIFY(!Base64::isValid("!!!invalid!!!"), "string with invalid chars should not be valid");
    VERIFY(!Base64::isValid("SGVsb G8="), "base64 with space should not be valid");
    std::cout << "Test Passed: isValid" << std::endl;
}

void TestKnownVectors() {
    VERIFY(Base64::encode("") == "", "empty encode");
    VERIFY(Base64::encode("f") == "Zg==", "single char encode");
    VERIFY(Base64::encode("fo") == "Zm8=", "two char encode");
    VERIFY(Base64::encode("foo") == "Zm9v", "three char encode");
    VERIFY(Base64::encode("foob") == "Zm9vYg==", "four char encode");
    VERIFY(Base64::encode("fooba") == "Zm9vYmE=", "five char encode");
    VERIFY(Base64::encode("foobar") == "Zm9vYmFy", "six char encode");

    VERIFY(Base64::decode("") == "", "empty decode");
    VERIFY(Base64::decode("Zg==") == "f", "single char decode");
    VERIFY(Base64::decode("Zm8=") == "fo", "two char decode");
    VERIFY(Base64::decode("Zm9v") == "foo", "three char decode");
    std::cout << "Test Passed: Known Vectors" << std::endl;
}

void TestLargeData() {
    std::string large(100000, 'X');
    std::string encoded = Base64::encode(large);
    std::string decoded = Base64::decode(encoded);
    VERIFY(decoded == large, "large data round-trip failed");
    std::cout << "Test Passed: Large Data (100KB)" << std::endl;
}

void TestManyBase64Cases() {
    // 1. Generate inputs of various lengths from 0 to 120 and test round-trip
    // This provides 121 test cases.
    for (int len = 0; len <= 120; ++len) {
        std::string input;
        input.reserve(len);
        for (int i = 0; i < len; ++i) {
            input.push_back(static_cast<char>((i * 31 + 17) % 256));
        }
        std::string encoded = Base64::encode(input);
        std::string decoded = Base64::decode(encoded);
        VERIFY(decoded == input, "round-trip failed for generated len=" + std::to_string(len));
        VERIFY(Base64::isValid(encoded), "isValid failed for generated len=" + std::to_string(len));
    }

    // 2. Dynamic invalid base64 character tests (invalid symbols placed at different positions)
    // This provides around 30 additional test cases.
    std::string validBase64 = "SGVsbG8gV29ybGQ="; // "Hello World"
    for (size_t i = 0; i < validBase64.size(); ++i) {
        char original = validBase64[i];
        if (original == '=') continue;
        
        // replace with invalid character
        validBase64[i] = '@';
        VERIFY(!Base64::isValid(validBase64), "isValid should fail with invalid char '@' at pos " + std::to_string(i));
        
        validBase64[i] = ' ';
        VERIFY(!Base64::isValid(validBase64), "isValid should fail with invalid char ' ' at pos " + std::to_string(i));

        validBase64[i] = original; // restore
    }
    std::cout << "Test Passed: Many Base64 Cases (150+ dynamically generated scenarios)" << std::endl;
}

int main() {
    try {
        TestKnownVectors();
        TestEncodeDecode();
        TestEncodeDecodeBinary();
        TestIsValid();
        TestLargeData();
        TestManyBase64Cases();
        std::cout << "=== ALL BASE64 TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
