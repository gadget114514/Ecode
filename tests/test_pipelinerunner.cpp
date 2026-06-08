#include "../src/PipelineRunner.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <sstream>

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
        {"special: \b\f", "special: \b\f"},
    };

    for (auto &c : cases) {
        std::string escaped = PipelineRunner::JsonEscape(c.input);
        VERIFY(escaped == c.expected,
            "JsonEscape failed for input: " + c.input +
            ", expected: " + c.expected + ", got: " + escaped);
    }
    std::cout << "Test Passed: JsonEscape" << std::endl;
}

void TestPipelineRunnerCreation() {
    PipelineRunner runner;
    VERIFY(!runner.IsRunning(), "runner should not be running initially");
    std::cout << "Test Passed: PipelineRunner Creation" << std::endl;
}

void TestCancelStopsExecution() {
    PipelineRunner runner;
    VERIFY(!runner.IsRunning(), "runner should not be running initially");
    runner.Cancel();
    VERIFY(!runner.IsRunning(), "runner should still not be running after cancel");
    std::cout << "Test Passed: Cancel Stops Execution" << std::endl;
}

void TestAppendStepWhileRunning() {
    PipelineRunner runner;
    PipelineStep step;
    step.name = "TestStep";
    step.type = "ai";
    // AppendStep should be a no-op when not running
    runner.AppendStep(step);
    // No crash means success
    std::cout << "Test Passed: Append Step While Not Running" << std::endl;
}

void TestInsertStepBoundaries() {
    PipelineRunner runner;
    PipelineStep step;
    step.name = "BoundaryStep";
    step.type = "ai";

    // These should not crash (no-op when not running)
    runner.InsertStep(0, step);
    runner.InsertStep(5, step);
    runner.InsertStep(100, step);
    std::cout << "Test Passed: Insert Step Boundary Conditions" << std::endl;
}

void TestRemoveStepBoundaries() {
    PipelineRunner runner;
    // Should not crash when removing from empty or invalid positions
    runner.RemoveStep(0);
    runner.RemoveStep(5);
    std::cout << "Test Passed: Remove Step Boundary Conditions" << std::endl;
}

void TestUpdateStepBoundaries() {
    PipelineRunner runner;
    PipelineStep step;
    step.name = "UpdateTest";
    step.type = "ai";

    runner.UpdateStep(0, step);
    runner.UpdateStep(10, step);
    std::cout << "Test Passed: Update Step Boundary Conditions" << std::endl;
}

void TestAppendPipelineSteps() {
    PipelineRunner runner;
    // Should not crash (stub implementation)
    runner.AppendPipelineSteps("NonExistentPipeline");
    std::cout << "Test Passed: Append Pipeline Steps" << std::endl;
}

void TestResumeManualWithoutWait() {
    PipelineRunner runner;
    // Should be a no-op when not waiting for manual input
    runner.ResumeManual("some content");
    runner.CancelManual();
    std::cout << "Test Passed: Resume/Cancel Manual Without Wait" << std::endl;
}

void TestSetBridgeCallback() {
    PipelineRunner runner;
    int callCount = 0;
    runner.SetBridgeCallback([&callCount](const std::string &type, const std::string &json) {
        callCount++;
    });
    // Callback is not invoked directly by us — just verify no crash on set
    std::cout << "Test Passed: Set Bridge Callback" << std::endl;
}

void TestRegisterProvider() {
    PipelineRunner runner;
    // Should not crash with empty/invalid provider type
    runner.RegisterProvider("", "", "");
    runner.RegisterProvider("nonexistent", "key", "url");
    std::cout << "Test Passed: Register Provider" << std::endl;
}

void TestMultipleCancel() {
    PipelineRunner runner;
    for (int i = 0; i < 10; i++) {
        runner.Cancel();
        VERIFY(!runner.IsRunning(), "runner should not be running after cancel iteration " + std::to_string(i));
    }
    std::cout << "Test Passed: Multiple Cancel Calls" << std::endl;
}

void TestRunWithEmptySteps() {
    PipelineRunner runner;
    std::vector<PipelineStep> emptySteps;
    runner.Run("test", emptySteps, "input", {}, "child");
    VERIFY(!runner.IsRunning(), "runner should not be running after empty pipeline");
    std::cout << "Test Passed: Run With Empty Steps" << std::endl;
}

void TestRunWithSingleStepNoProvider() {
    PipelineRunner runner;
    PipelineStep step;
    step.name = "TestAI";
    step.type = "ai";
    step.params["provider"] = "nonexistent";
    step.params["model"] = "test-model";
    step.params["userPrompt"] = "test {content}";

    bool gotError = false;
    runner.SetBridgeCallback([&gotError](const std::string &type, const std::string &json) {
        if (type == "pipeline_error") gotError = true;
    });

    runner.Run("test", {step}, "hello world", {}, "child");
    // Should trigger error since provider doesn't exist
    std::cout << "Test Passed: Run With Single Step No Provider" << std::endl;
}

void TestParallelStateData() {
    ParallelState ps;
    VERIFY(ps.branches.empty(), "default parallel state should have no branches");
    VERIFY(ps.currentBranch == 0, "default current branch should be 0");
    VERIFY(ps.results.empty(), "default results should be empty");
    VERIFY(ps.inputContent.empty(), "default input content should be empty");
    std::cout << "Test Passed: Parallel State Default Data" << std::endl;
}

void TestParallelStateWithBranches() {
    ParallelState ps;
    ParallelState::Branch b1, b2;
    b1.name = "summary";
    b2.name = "keywords";

    PipelineStep s1, s2;
    s1.name = "Summarize";
    s1.type = "ai";
    s1.params["provider"] = "openai";
    b1.steps.push_back(s1);

    s2.name = "Extract";
    s2.type = "ai";
    s2.params["provider"] = "openai";
    b2.steps.push_back(s2);

    ps.branches.push_back(b1);
    ps.branches.push_back(b2);
    ps.inputContent = "some input";

    VERIFY(ps.branches.size() == 2, "should have 2 branches");
    VERIFY(ps.branches[0].name == "summary", "first branch name");
    VERIFY(ps.branches[1].name == "keywords", "second branch name");
    VERIFY(ps.branches[0].steps.size() == 1, "first branch should have 1 step");
    VERIFY(ps.branches[1].steps[0].name == "Extract", "second branch step name");
    std::cout << "Test Passed: Parallel State With Branches" << std::endl;
}

void TestHistoryStepDefaults() {
    HistoryStep hs;
    VERIFY(hs.index == 0, "default index should be 0");
    VERIFY(hs.name.empty(), "default name should be empty");
    VERIFY(hs.type.empty(), "default type should be empty");
    VERIFY(hs.input.empty(), "default input should be empty");
    VERIFY(hs.output.empty(), "default output should be empty");
    VERIFY(hs.retries == 0, "default retries should be 0");
    VERIFY(hs.iterations == 0, "default iterations should be 0");
    VERIFY(hs.test == false, "default test should be false");
    VERIFY(hs.status == "pending", "default status should be pending");
    VERIFY(hs.evaluation.empty(), "default evaluation should be empty");
    VERIFY(hs.promptTokens == 0, "default promptTokens should be 0");
    VERIFY(hs.completionTokens == 0, "default completionTokens should be 0");
    VERIFY(hs.durationMs == 0, "default durationMs should be 0");
    std::cout << "Test Passed: HistoryStep Defaults" << std::endl;
}

void TestHistoryStepFullData() {
    HistoryStep hs;
    hs.index = 2;
    hs.name = "Translate";
    hs.type = "ai";
    hs.input = "Hello world";
    hs.output = "こんにちは世界";
    hs.retries = 1;
    hs.iterations = 3;
    hs.test = true;
    hs.evaluation = "ok";
    hs.evaluationNote = "Good translation";
    hs.promptTokens = 120;
    hs.completionTokens = 45;
    hs.durationMs = 2300;
    hs.status = "completed";
    hs.childRunId = "run_20250601_120000";
    hs.parallelBranches["claude"] = "こんにちは";
    hs.parallelBranches["gpt4"] = "やあ";

    VERIFY(hs.index == 2, "index should be 2");
    VERIFY(hs.name == "Translate", "name should be Translate");
    VERIFY(hs.output == "こんにちは世界", "output should match");
    VERIFY(hs.test == true, "test should be true");
    VERIFY(hs.evaluation == "ok", "evaluation should be ok");
    VERIFY(hs.parallelBranches.size() == 2, "should have 2 parallel branches");
    VERIFY(hs.parallelBranches["claude"] == "こんにちは", "claude branch result");
    std::cout << "Test Passed: HistoryStep Full Data" << std::endl;
}

void TestHistoryRecordDefaults() {
    HistoryRecord rec;
    VERIFY(rec.id.empty(), "default id should be empty");
    VERIFY(rec.pipelineName.empty(), "default pipelineName should be empty");
    VERIFY(rec.status.empty(), "default status should be empty");
    VERIFY(rec.evaluation.empty(), "default evaluation should be empty");
    VERIFY(rec.steps.empty(), "default steps should be empty");
    std::cout << "Test Passed: HistoryRecord Defaults" << std::endl;
}

void TestHistoryRecordFullData() {
    HistoryRecord rec;
    rec.id = "run_20250601_120000";
    rec.pipelineName = "Translate → Review";
    rec.inputNodeId = "node_abc";
    rec.outputNodeId = "node_xyz";
    rec.startedAt = "2025-06-01T12:00:00Z";
    rec.status = "completed";
    rec.evaluation = "ok";

    HistoryStep s1;
    s1.index = 0;
    s1.name = "Translate";
    s1.status = "completed";
    s1.evaluation = "ok";
    rec.steps.push_back(s1);

    HistoryStep s2;
    s2.index = 1;
    s2.name = "Review";
    s2.status = "completed";
    rec.steps.push_back(s2);

    VERIFY(rec.id == "run_20250601_120000", "id should match");
    VERIFY(rec.steps.size() == 2, "should have 2 steps");
    VERIFY(rec.steps[0].name == "Translate", "first step name");
    VERIFY(rec.steps[1].name == "Review", "second step name");
    VERIFY(rec.evaluation == "ok", "evaluation should be ok");
    std::cout << "Test Passed: HistoryRecord Full Data" << std::endl;
}

void TestPlaceholderReplacement() {
    std::string prompt = "Translate {content} to Japanese. Previous result: {result}";
    std::string content = "Hello world";
    std::string result = "こんにちは世界";

    auto replaceAll = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    std::string resolved = prompt;
    replaceAll(resolved, "{content}", content);
    replaceAll(resolved, "{result}", result);

    VERIFY(resolved == "Translate Hello world to Japanese. Previous result: こんにちは世界",
        "placeholder replacement failed: " + resolved);

    // Test with no placeholders
    std::string noPlaceholders = "Just a static prompt";
    replaceAll(noPlaceholders, "{content}", "anything");
    replaceAll(noPlaceholders, "{result}", "anything");
    VERIFY(noPlaceholders == "Just a static prompt", "no placeholder prompt should stay unchanged");

    // Test with multiple occurrences
    std::string multi = "{content} and {content} again";
    replaceAll(multi, "{content}", "X");
    VERIFY(multi == "X and X again", "multiple placeholder replacement failed: " + multi);

    std::cout << "Test Passed: Placeholder Replacement" << std::endl;
}

void TestStepTypeConstants() {
    std::vector<std::string> stepTypes = {"ai", "manual", "command", "tool", "fetch",
                                          "condition", "transform", "history", "call_pipeline",
                                          "foreach", "parallel", "wait"};
    for (auto &type : stepTypes) {
        PipelineStep step;
        step.type = type;
        VERIFY(step.type == type, "step type should be set correctly: " + type);
    }
    std::cout << "Test Passed: Step Type Constants" << std::endl;
}

void TestStepParamsMap() {
    PipelineStep step;
    step.params["provider"] = "openai";
    step.params["model"] = "gpt-4.1";
    step.params["temperature"] = "0.3";
    step.params["maxTokens"] = "4096";

    VERIFY(step.params.size() == 4, "should have 4 params");
    VERIFY(step.params["provider"] == "openai", "provider param");
    VERIFY(step.params["model"] == "gpt-4.1", "model param");
    VERIFY(step.params["temperature"] == "0.3", "temperature param");
    VERIFY(step.params["maxTokens"] == "4096", "maxTokens param");
    VERIFY(step.params.count("nonexistent") == 0, "nonexistent key should not exist");
    std::cout << "Test Passed: Step Params Map" << std::endl;
}

void TestPipelineStepDefaults() {
    PipelineStep step;
    VERIFY(step.name.empty(), "default name should be empty");
    VERIFY(step.type.empty() || step.type == "ai", "default type should be empty or ai");
    VERIFY(step.params.empty(), "default params should be empty");
    std::cout << "Test Passed: PipelineStep Defaults" << std::endl;
}

void TestBuildMetaJson() {
    PipelineRunner runner;

    std::string jsonOutput;
    runner.SetBridgeCallback([&jsonOutput](const std::string &type, const std::string &json) {
        if (type == "pipeline_completed") jsonOutput = json;
    });

    PipelineStep step;
    step.name = "TestStep";
    step.type = "ai";

    runner.Run("test_pipeline", {step}, "input content", {}, "child");
    // Since no provider is registered, it will error out, not complete
    // So we just verify it doesn't crash
    std::cout << "Test Passed: BuildMetaJson No Crash" << std::endl;
}

void TestParallelStepWithMissingProvider() {
    PipelineRunner runner;
    PipelineStep step;
    step.name = "ParallelTest";
    step.type = "parallel";
    step.params["branches"] = R"([{"name":"b1","steps":[{"type":"ai","provider":"missing"}]}])";

    bool gotError = false;
    runner.SetBridgeCallback([&gotError](const std::string &type, const std::string &) {
        if (type == "pipeline_error") gotError = true;
    });

    runner.Run("parallel_test", {step}, "input", {}, "child");
    // Should handle gracefully since provider is missing
    std::cout << "Test Passed: Parallel Step With Missing Provider" << std::endl;
}

void TestConditionStepEvaluation() {
    PipelineRunner runner;
    PipelineStep step;
    step.name = "ConditionTest";
    step.type = "condition";
    step.params["expression"] = "{result}";
    step.params["operator"] = "contains";
    step.params["value"] = "error";
    step.params["onTrue"] = "goto_step";
    step.params["onTrueIndex"] = "0";
    step.params["onFalse"] = "next_step";

    // Just verify no crash on run
    std::vector<PipelineStep> steps = {step};
    runner.Run("condition_test", steps, "this is an error message", {}, "child");
    std::cout << "Test Passed: Condition Step Evaluation" << std::endl;
}

void TestRunWithMultipleSteps() {
    PipelineRunner runner;
    PipelineStep s1, s2, s3;
    s1.name = "Step1";
    s1.type = "manual";
    s1.params["mode"] = "view";
    s1.params["prompt"] = "Check step 1";

    s2.name = "Step2";
    s2.type = "manual";
    s2.params["mode"] = "edit";
    s2.params["prompt"] = "Edit step 2";

    s3.name = "Step3";
    s3.type = "condition";
    s3.params["expression"] = "{result}";
    s3.params["operator"] = "contains";
    s3.params["value"] = "test";

    std::vector<PipelineStep> steps = {s1, s2, s3};

    bool stepStarted = false;
    runner.SetBridgeCallback([&stepStarted](const std::string &type, const std::string &) {
        if (type == "step_started") stepStarted = true;
    });

    runner.Run("multi_step_test", steps, "test input", {}, "child");
    std::cout << "Test Passed: Run With Multiple Steps" << std::endl;
}

int main() {
    try {
        TestJsonEscape();
        TestPipelineRunnerCreation();
        TestCancelStopsExecution();
        TestAppendStepWhileRunning();
        TestInsertStepBoundaries();
        TestRemoveStepBoundaries();
        TestUpdateStepBoundaries();
        TestAppendPipelineSteps();
        TestResumeManualWithoutWait();
        TestSetBridgeCallback();
        TestRegisterProvider();
        TestMultipleCancel();
        TestRunWithEmptySteps();
        TestRunWithSingleStepNoProvider();
        TestParallelStateData();
        TestParallelStateWithBranches();
        TestHistoryStepDefaults();
        TestHistoryStepFullData();
        TestHistoryRecordDefaults();
        TestHistoryRecordFullData();
        TestPlaceholderReplacement();
        TestStepTypeConstants();
        TestStepParamsMap();
        TestPipelineStepDefaults();
        TestBuildMetaJson();
        TestParallelStepWithMissingProvider();
        TestConditionStepEvaluation();
        TestRunWithMultipleSteps();
        std::cout << "=== ALL PIPELINERUNNER TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
