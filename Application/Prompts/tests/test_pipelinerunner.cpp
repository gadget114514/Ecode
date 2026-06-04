#include "../src/PipelineRunner.h"
#include <cassert>
#include <iostream>
#include <string>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestJsonEscape() {
    struct TestCase {
        std::string input;
        std::string expected;
    };
    TestCase cases[] = {
        {"", ""},
        {"hello", "hello"},
        {"hello \"world\"", "hello \\\"world\\\""},
        {"hello\\world", "hello\\\\world"},
        {"line1\nline2", "line1\\nline2"},
        {"line1\r\nline2", "line1\\r\\nline2"},
        {"col1\tcol2", "col1\\tcol2"},
    };

    for (auto &c : cases) {
        std::string escaped = PipelineRunner::JsonEscape(c.input);
        VERIFY(escaped == c.expected, "JsonEscape failed for input: " + c.input + ", expected: " + c.expected + ", got: " + escaped);
    }
    std::cout << "Test Passed: JsonEscape" << std::endl;
}

void TestPipelineRunnerCreation() {
    PipelineRunner runner;
    VERIFY(!runner.IsRunning(), "runner should not be running initially");
    std::cout << "Test Passed: PipelineRunner Creation" << std::endl;
}

int main() {
    try {
        TestJsonEscape();
        TestPipelineRunnerCreation();
        std::cout << "=== ALL PIPELINERUNNER TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
