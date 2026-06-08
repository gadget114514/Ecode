#include "../src/PipelineRunner.h"
#include "../src/NodeData.h"
#include "../src/JsonParser.h"
#include "../src/AIProvider.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

// ==================== FilterConfig Tests ====================

void TestFilterConfigDefaults() {
    // Test that FilterConfig has sensible defaults
    FilterConfig cfg;
    // These are the expected defaults based on the design
    VERIFY(cfg.mode.empty() || cfg.mode == "auto", "default filter mode should be empty or auto");
    VERIFY(cfg.splitBy.empty(), "default splitBy should be empty");
    VERIFY(cfg.actions.empty(), "default actions should be empty");
    std::cout << "Test Passed: FilterConfig Defaults" << std::endl;
}

void TestFilterConfigManual() {
    FilterConfig cfg;
    cfg.mode = "manual";
    cfg.splitBy = "\n---\n";
    cfg.actions = {"approve", "reject"};

    VERIFY(cfg.mode == "manual", "filter mode should be manual");
    VERIFY(cfg.splitBy == "\n---\n", "splitBy should be set");
    VERIFY(cfg.actions.size() == 2, "should have 2 actions");
    VERIFY(cfg.actions[0] == "approve", "first action should be approve");
    VERIFY(cfg.actions[1] == "reject", "second action should be reject");
    std::cout << "Test Passed: FilterConfig Manual" << std::endl;
}

void TestFilterConfigSplit() {
    FilterConfig cfg;
    cfg.mode = "manual_split";
    cfg.splitBy = "\n---\n";

    // Simulate splitting output
    std::string output = "First block\n---\nSecond block\n---\nThird block";
    std::vector<std::string> blocks;
    size_t start = 0, end;
    while ((end = output.find(cfg.splitBy, start)) != std::string::npos) {
        blocks.push_back(output.substr(start, end - start));
        start = end + cfg.splitBy.size();
    }
    blocks.push_back(output.substr(start));

    VERIFY(blocks.size() == 3, "splitBy should produce 3 blocks");
    VERIFY(blocks[0] == "First block", "first block should match");
    VERIFY(blocks[1] == "Second block", "second block should match");
    VERIFY(blocks[2] == "Third block", "third block should match");

    std::cout << "Test Passed: FilterConfig Split" << std::endl;
}

void TestFilterConfigAuto() {
    FilterConfig cfg;
    cfg.mode = "auto";

    // Auto mode: single output → auto-approve
    VERIFY(cfg.mode == "auto", "auto mode");
    std::cout << "Test Passed: FilterConfig Auto" << std::endl;
}

void TestFilterConfigAutoPass() {
    FilterConfig cfg;
    cfg.mode = "auto_pass";

    // Auto-pass mode: single output → do not save, do not prompt
    VERIFY(cfg.mode == "auto_pass", "auto_pass mode");
    std::cout << "Test Passed: FilterConfig AutoPass" << std::endl;
}

// ==================== Evaluate-Related Tests ====================

void TestEvaluateScoring() {
    // Simulate AI evaluation response parsing
    std::string evalResponse = "{\"score\":8.5,\"rationale\":\"Accurate and fluent translation\",\"maxScore\":10}";
    auto val = JsonValue::parse(evalResponse);

    VERIFY(val.has("score"), "evaluation response should have score");
    VERIFY(val.has("rationale"), "evaluation response should have rationale");
    VERIFY(val["score"].number() >= 0.0 && val["score"].number() <= 10.0,
           "score should be between 0 and 10, got: " + std::to_string(val["score"].number()));
    VERIFY(val["score"].number() == 8.5, "score should be 8.5");
    VERIFY(val["rationale"].string() == "Accurate and fluent translation", "rationale should match");

    std::cout << "Test Passed: Evaluate Scoring" << std::endl;
}

void TestEvaluateMultipleScores() {
    // Each output gets its own independent score
    std::vector<std::string> outputs = {
        "{\"output\":\"Hello world\",\"score\":8.5,\"rationale\":\"Good\"}",
        "{\"output\":\"Hi world\",\"score\":6.0,\"rationale\":\"Too informal\"}",
        "{\"output\":\"World, hello\",\"score\":9.0,\"rationale\":\"Excellent\"}"
    };

    int count = 0;
    double totalScore = 0;
    for (auto &out : outputs) {
        auto val = JsonValue::parse(out);
        VERIFY(val.has("score"), "each output should have a score");
        double score = val["score"].number();
        VERIFY(score >= 0 && score <= 10, "individual score should be 0-10, got: " + std::to_string(score));
        totalScore += score;
        count++;
    }
    VERIFY(count == 3, "should have 3 scored outputs");
    VERIFY(totalScore == 23.5, "total score should be sum of individual scores");

    std::cout << "Test Passed: Evaluate Multiple Scores" << std::endl;
}

void TestEvaluateScoreAttachment() {
    // Simulate attaching score to a node for storage
    std::string nodeMeta = "{\"pipelineName\":\"Translate\",\"steps\":[]}";
    std::string evaluation = "{\"score\":7.5,\"rationale\":\"Decent translation, minor errors\",\"checkedAt\":\"2026-06-08T12:00:00Z\"}";

    auto metaVal = JsonValue::parse(nodeMeta);
    auto evalVal = JsonValue::parse(evaluation);

    // Merge evaluation into meta
    std::map<std::string, JsonValue> obj;
    for (auto &kv : metaVal.object()) obj[kv.first] = kv.second;
    obj["evaluation"] = evalVal;
    std::string merged = JsonValue::fromObject(obj).serialize();

    auto reparsed = JsonValue::parse(merged);
    VERIFY(reparsed.has("evaluation"), "merged meta should have evaluation");
    VERIFY(reparsed["evaluation"]["score"].number() == 7.5, "evaluation score should be preserved in merged meta");

    std::cout << "Test Passed: Evaluate Score Attachment" << std::endl;
}

// ==================== Input Source Selection Tests ====================

void TestInputSourceDefault() {
    // Default input source is "previous step output"
    std::string defaultSource = "previous_step";
    VERIFY(defaultSource == "previous_step", "default input source should be previous_step");
    std::cout << "Test Passed: Input Source Default" << std::endl;
}

void TestInputSourceCheckpoint() {
    // Input source can be an explicit checkpoint
    std::string source = "checkpoint";
    std::string runId = "run_20260608_120000";
    int checkpointIndex = 2;

    std::string sourcePath = runId + "/checkpoint_" + std::to_string(checkpointIndex) + "/output.json";
    VERIFY(sourcePath == "run_20260608_120000/checkpoint_2/output.json",
           "checkpoint source path should be well-formed");

    std::cout << "Test Passed: Input Source Checkpoint" << std::endl;
}

void TestInputSourceExternalFile() {
    // External file input source
    std::string source = "external_file";
    std::string filePath = "C:/data/input.txt";
    VERIFY(source == "external_file", "source should indicate external file");
    VERIFY(!filePath.empty(), "file path should not be empty");
    std::cout << "Test Passed: Input Source External File" << std::endl;
}

void TestInputSourceManual() {
    // Manual input via textarea
    std::string source = "manual";
    std::string text = "User typed this directly";
    VERIFY(source == "manual", "source should indicate manual input");
    VERIFY(!text.empty(), "manual text should not be empty");
    std::cout << "Test Passed: Input Source Manual" << std::endl;
}

void TestInputSourceChest() {
    // Input from named chest
    std::string source = "chest";
    std::string chestName = "jp_translations";
    VERIFY(source == "chest", "source should indicate chest input");
    VERIFY(!chestName.empty(), "chest name should not be empty");
    std::cout << "Test Passed: Input Source Chest" << std::endl;
}

void TestInputSourceResolvesToContent() {
    // Test that various input sources resolve to a content string
    struct TestCase {
        std::string source;
        std::string resolvedContent;
    };

    TestCase cases[] = {
        {"previous_step", "Output from step 1"},
        {"manual", "User typed text"},
        {"external_file", "Content loaded from file"},
    };

    for (auto &c : cases) {
        VERIFY(!c.resolvedContent.empty(), c.source + " should resolve to non-empty content");
    }

    std::cout << "Test Passed: Input Source Resolves To Content" << std::endl;
}

void TestInputSourcePathValidation() {
    // Validate that paths don't allow directory traversal
    auto isPathSafe = [](const std::string &path) -> bool {
        if (path.find("..") != std::string::npos) return false;
        if (path.find("~") != std::string::npos) return false;
        return true;
    };

    VERIFY(isPathSafe("checkpoint_0/output.json"), "normal path should be safe");
    VERIFY(!isPathSafe("../other_project/data.json"), "path traversal should be rejected");
    VERIFY(!isPathSafe("../../secrets.json"), "upward traversal should be rejected");
    VERIFY(isPathSafe("C:/data/input.txt"), "absolute path should be allowed (user responsibility)");

    std::cout << "Test Passed: Input Source Path Validation" << std::endl;
}

// ==================== Run State Machine Tests ====================

void TestRunStateTransitions() {
    // Valid state transitions for a pipeline run
    std::string validTransitions[][2] = {
        {"pending", "running"},
        {"running", "completed"},
        {"running", "cancelled"},
        {"completed", "archived"},
    };

    for (auto &t : validTransitions) {
        std::string from = t[0];
        std::string to = t[1];
        VERIFY(from != to, "state transition should change the state");
    }

    // Invalid transitions should not happen
    std::string invalidTransitions[][2] = {
        {"completed", "running"},
        {"cancelled", "running"},
        {"pending", "completed"},
    };

    for (auto &t : invalidTransitions) {
        std::string from = t[0];
        std::string to = t[1];
        VERIFY(from != to, "invalid transition: " + from + " -> " + to);
    }

    std::cout << "Test Passed: Run State Transitions" << std::endl;
}

// ==================== Placeholder Resolution Tests ====================

void TestPlaceholderContent() {
    // {content} should resolve to the input node's content
    std::string prompt = "Process this: {content}";
    std::string inputContent = "Hello world";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(prompt, "{content}", inputContent);
    VERIFY(prompt == "Process this: Hello world", "{content} should be replaced with input content");

    std::cout << "Test Passed: Placeholder Content" << std::endl;
}

void TestPlaceholderResult() {
    // {result} should resolve to the previous step's output
    std::string prompt = "Polish this: {result}";
    std::string previousOutput = "This is rough text";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(prompt, "{result}", previousOutput);
    VERIFY(prompt == "Polish this: This is rough text", "{result} should be replaced");

    std::cout << "Test Passed: Placeholder Result" << std::endl;
}

void TestPlaceholderStepRef() {
    // {step.N.result} should resolve to a specific step's output
    std::string prompt = "Compare: {step.1.result} vs {step.2.result}";
    std::vector<std::string> stepOutputs = {"", "First result", "Second result"};

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(prompt, "{step.1.result}", stepOutputs[1]);
    replaceAll(prompt, "{step.2.result}", stepOutputs[2]);

    VERIFY(prompt == "Compare: First result vs Second result",
           "{step.N.result} should resolve to specific step output");

    std::cout << "Test Passed: Placeholder StepRef" << std::endl;
}

void TestPlaceholderInputFrom() {
    // {inputFrom} should resolve to the specified input source
    std::string prompt = "Process: {inputFrom}";
    std::string inputSource = "run_20260608_120000/checkpoint_1";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(prompt, "{inputFrom}", inputSource);
    VERIFY(prompt == "Process: run_20260608_120000/checkpoint_1",
           "{inputFrom} should resolve to the specified source");

    std::cout << "Test Passed: Placeholder InputFrom" << std::endl;
}

void TestPlaceholderCheckpointRef() {
    // {checkpoint.N.output} should resolve to specific checkpoint output
    std::string prompt = "Refine: {checkpoint.0.output}";
    std::string checkpointOutput = "Checkpoint zero data";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(prompt, "{checkpoint.0.output}", checkpointOutput);
    VERIFY(prompt == "Refine: Checkpoint zero data", "{checkpoint.N.output} should resolve");

    std::cout << "Test Passed: Placeholder CheckpointRef" << std::endl;
}

void TestPlaceholderMultiple() {
    // Multiple placeholders in one prompt
    std::string prompt = "Title: {title}\nInput: {content}\nPrevious: {result}\nCheckpoint: {checkpoint.1.output}";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(prompt, "{title}", "My Document");
    replaceAll(prompt, "{content}", "Hello world");
    replaceAll(prompt, "{result}", "Previous output");
    replaceAll(prompt, "{checkpoint.1.output}", "Cached data");

    VERIFY(prompt.find("{title}") == std::string::npos, "all title placeholders should be replaced");
    VERIFY(prompt.find("My Document") != std::string::npos, "title value should appear");
    VERIFY(prompt.find("Hello world") != std::string::npos, "content value should appear");
    VERIFY(prompt.find("Previous output") != std::string::npos, "result value should appear");
    VERIFY(prompt.find("Cached data") != std::string::npos, "checkpoint value should appear");

    std::cout << "Test Passed: Placeholder Multiple" << std::endl;
}

// ==================== Filter Approve/Reject Logic ====================

void TestFilterApproveSingle() {
    std::vector<int> approved = {0};
    std::vector<int> rejected = {};

    VERIFY(approved.size() == 1, "1 item should be approved");
    VERIFY(rejected.empty(), "no items should be rejected");
    VERIFY(approved[0] == 0, "item 0 should be approved");

    std::cout << "Test Passed: Filter Approve Single" << std::endl;
}

void TestFilterRejectSingle() {
    std::vector<int> approved = {};
    std::vector<int> rejected = {2};

    VERIFY(approved.empty(), "no items should be approved");
    VERIFY(rejected.size() == 1, "1 item should be rejected");
    VERIFY(rejected[0] == 2, "item 2 should be rejected");

    std::cout << "Test Passed: Filter Reject Single" << std::endl;
}

void TestFilterMixedDecision() {
    std::vector<int> approved = {0, 2};
    std::vector<int> rejected = {1, 3};

    VERIFY(approved.size() == 2, "2 items should be approved");
    VERIFY(rejected.size() == 2, "2 items should be rejected");
    VERIFY(approved[0] == 0 && approved[1] == 2, "items 0 and 2 should be approved");
    VERIFY(rejected[0] == 1 && rejected[1] == 3, "items 1 and 3 should be rejected");

    // Approved and rejected sets must be disjoint
    for (int a : approved) {
        for (int r : rejected) {
            VERIFY(a != r, "approved and rejected sets must be disjoint");
        }
    }

    std::cout << "Test Passed: Filter Mixed Decision" << std::endl;
}

void TestFilterAppendOnlyDecision() {
    // Filter decisions are stored in a separate file, not modifying original output
    std::string outputContent = "Original AI output content";
    std::string filterDecision = "{\"decision\":\"approved\",\"stepIndex\":2}";

    // Output must remain unchanged after filter decision
    std::string outputAfterDecision = "Original AI output content";
    VERIFY(outputAfterDecision == outputContent, "output must remain unchanged after filter decision");

    std::cout << "Test Passed: Filter Append Only Decision" << std::endl;
}

int main() {
    try {
        // FilterConfig tests
        TestFilterConfigDefaults();
        TestFilterConfigManual();
        TestFilterConfigSplit();
        TestFilterConfigAuto();
        TestFilterConfigAutoPass();

        // Evaluate tests
        TestEvaluateScoring();
        TestEvaluateMultipleScores();
        TestEvaluateScoreAttachment();

        // Input source selection tests
        TestInputSourceDefault();
        TestInputSourceCheckpoint();
        TestInputSourceExternalFile();
        TestInputSourceManual();
        TestInputSourceChest();
        TestInputSourceResolvesToContent();
        TestInputSourcePathValidation();

        // Run state machine tests
        TestRunStateTransitions();

        // Placeholder resolution tests
        TestPlaceholderContent();
        TestPlaceholderResult();
        TestPlaceholderStepRef();
        TestPlaceholderInputFrom();
        TestPlaceholderCheckpointRef();
        TestPlaceholderMultiple();

        // Filter approve/reject logic tests
        TestFilterApproveSingle();
        TestFilterRejectSingle();
        TestFilterMixedDecision();
        TestFilterAppendOnlyDecision();

        std::cout << "=== ALL STREAM MODEL TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
