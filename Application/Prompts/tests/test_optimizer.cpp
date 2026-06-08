// Tests for PipelineVersionManager and PipelineOptimizer
#include "../src/PipelineVersionManager.h"
#include "../src/PipelineOptimizer.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

// ==================== PipelineVersionManager Tests ====================

void TestSanitizeName() {
    VERIFY(PipelineVersionManager::SanitizeName("Hello") == "Hello", "simple name");
    VERIFY(PipelineVersionManager::SanitizeName("Hello World") == "Hello_World", "space replaced");
    VERIFY(PipelineVersionManager::SanitizeName("a/b/c") == "a_b_c", "slash replaced");
    VERIFY(PipelineVersionManager::SanitizeName("test-123") == "test-123", "hyphen kept");
    VERIFY(PipelineVersionManager::SanitizeName("") == "", "empty string");
    VERIFY(PipelineVersionManager::SanitizeName("a.b.c") == "a_b_c", "dots replaced");
    VERIFY(PipelineVersionManager::SanitizeName("  spaces  ") == "__spaces__", "leading/trailing spaces");
    std::cout << "Test Passed: SanitizeName" << std::endl;
}

void TestSerializeDeserializeCursor() {
    {
        VersionCursor cursor;
        cursor.pipelineName = "TestPipe";
        cursor.currentVersion = 3;
        cursor.headVersion = 5;

        PipelineVersion v1, v2, v3;
        v1.version = 1;
        v1.timestamp = "2025-01-01T00:00:00Z";
        v1.label = "Initial";
        v1.sessionId = "sess_001";

        v2.version = 2;
        v2.timestamp = "2025-01-02T00:00:00Z";
        v2.label = "Improved prompts";
        v2.sessionId = "sess_002";

        OptEditProposal p;
        p.op = "replace";
        p.stepName = "Translate";
        p.field = "userPrompt";
        p.oldValue = "old prompt";
        p.newValue = "new prompt";
        p.rationale = "Better results";
        v2.approvedProposals.push_back(p);

        v3.version = 3;
        v3.timestamp = "2025-01-03T00:00:00Z";
        v3.label = "Agent review";
        v3.sessionId = "sess_003";

        cursor.entries.push_back(v1);
        cursor.entries.push_back(v2);
        cursor.entries.push_back(v3);

        std::string json = PipelineVersionManager::SerializeCursor(cursor);
        VERIFY(!json.empty(), "serialized cursor should not be empty");
        VERIFY(json.find("TestPipe") != std::string::npos, "json should contain pipeline name");
        VERIFY(json.find("3") != std::string::npos, "json should contain version 3");
        VERIFY(json.find("Translate") != std::string::npos, "json should contain step name");

        VersionCursor deserialized = PipelineVersionManager::DeserializeCursor(json, "TestPipe");
        VERIFY(deserialized.pipelineName == "TestPipe", "deserialized pipeline name");
        VERIFY(deserialized.currentVersion == 3, "deserialized current version");
        VERIFY(deserialized.headVersion == 5, "deserialized head version");
        VERIFY(deserialized.entries.size() == 3, "deserialized entries count");
        VERIFY(deserialized.entries[0].version == 1, "first entry version");
        VERIFY(deserialized.entries[1].version == 2, "second entry version");
        VERIFY(deserialized.entries[2].version == 3, "third entry version");
        VERIFY(deserialized.entries[1].approvedProposals.size() == 1, "should have 1 approved proposal");
        VERIFY(deserialized.entries[1].approvedProposals[0].stepName == "Translate", "proposal step name");
        VERIFY(deserialized.entries[1].approvedProposals[0].op == "replace", "proposal op");
    }
    {
        // Empty cursor
        VersionCursor emptyCursor;
        std::string json = PipelineVersionManager::SerializeCursor(emptyCursor);
        VersionCursor deserialized = PipelineVersionManager::DeserializeCursor(json, "EmptyPipe");
        VERIFY(deserialized.pipelineName == "EmptyPipe", "empty deserialized name");
        VERIFY(deserialized.currentVersion == 0, "empty deserialized current version");
        VERIFY(deserialized.entries.empty(), "empty deserialized entries");
    }
    {
        // Empty JSON string
        VersionCursor deserialized = PipelineVersionManager::DeserializeCursor("", "NoData");
        VERIFY(deserialized.currentVersion == 0, "no data: current version should be 0");
        VERIFY(deserialized.entries.empty(), "no data: entries should be empty");
    }
    std::cout << "Test Passed: Serialize/Deserialize Cursor" << std::endl;
}

void TestSerializeDeserializeSnapshot() {
    {
        Pipeline p;
        p.name = "TestPipeline";
        p.mode = "basic";
        p.outputMode = "child";
        p.outputNaming = "{pipeline_name}_{timestamp}";
        p.multiMedia = "attachments";
        p.retryCount = 3;
        p.retryDelayMs = 2000;

        PipelineStep s1;
        s1.name = "Translate";
        s1.type = "ai";
        s1.params["provider"] = "openai";
        s1.params["model"] = "gpt-4.1";
        s1.params["systemPrompt"] = "You are a translator";
        s1.params["userPrompt"] = "Translate: {content}";
        p.steps.push_back(s1);

        PipelineStep s2;
        s2.name = "Review";
        s2.type = "ai";
        s2.params["provider"] = "anthropic";
        s2.params["model"] = "claude-sonnet-4-6";
        s2.params["systemPrompt"] = "Review translations";
        s2.params["userPrompt"] = "Review: {result}";
        p.steps.push_back(s2);

        std::string json = PipelineVersionManager::SerializeSnapshot(p);
        VERIFY(!json.empty(), "serialized snapshot should not be empty");
        VERIFY(json.find("TestPipeline") != std::string::npos, "json should contain pipeline name");
        VERIFY(json.find("Translate") != std::string::npos, "json should contain step name");
        VERIFY(json.find("Review") != std::string::npos, "json should contain Review step");

        Pipeline deserialized = PipelineVersionManager::DeserializeSnapshot(json);
        VERIFY(deserialized.name == "TestPipeline", "deserialized name");
        VERIFY(deserialized.mode == "basic", "deserialized mode");
        VERIFY(deserialized.outputMode == "child", "deserialized outputMode");
        VERIFY(deserialized.retryCount == 3, "deserialized retryCount");
        VERIFY(deserialized.retryDelayMs == 2000, "deserialized retryDelayMs");
        VERIFY(deserialized.steps.size() == 2, "deserialized steps count");
        VERIFY(deserialized.steps[0].name == "Translate", "first step name");
        VERIFY(deserialized.steps[0].params["provider"] == "openai", "first step provider");
        VERIFY(deserialized.steps[1].name == "Review", "second step name");
        VERIFY(deserialized.steps[1].params["provider"] == "anthropic", "second step provider");
    }
    {
        // Empty snapshot
        std::string json = PipelineVersionManager::SerializeSnapshot(Pipeline{});
        Pipeline deserialized = PipelineVersionManager::DeserializeSnapshot(json);
        VERIFY(deserialized.name.empty(), "empty snapshot name");
        VERIFY(deserialized.steps.empty(), "empty snapshot steps");
    }
    {
        // Empty JSON string
        Pipeline deserialized = PipelineVersionManager::DeserializeSnapshot("");
        VERIFY(deserialized.name.empty(), "no data: name should be empty");
    }
    std::cout << "Test Passed: Serialize/Deserialize Snapshot" << std::endl;
}

// ==================== PipelineOptimizer Tests ====================

void TestTruncateUtf8() {
    VERIFY(PipelineOptimizer::TruncateUtf8("hello", 10) == "hello", "short string");
    VERIFY(PipelineOptimizer::TruncateUtf8("hello", 3) == "hel...", "truncated");
    VERIFY(PipelineOptimizer::TruncateUtf8("", 10) == "", "empty string");
    VERIFY(PipelineOptimizer::TruncateUtf8("hello", 5) == "hello", "exact fit");
    VERIFY(PipelineOptimizer::TruncateUtf8("a", 0).size() == 3, "zero max returns just ellipsis");
    std::cout << "Test Passed: TruncateUtf8" << std::endl;
}

void TestSanitizeNameOptimizer() {
    // Access through the public static method (same as PipelineVersionManager::SanitizeName)
    // We test via PipelineVersionManager::SanitizeName which both classes use
    VERIFY(PipelineVersionManager::SanitizeName("Translate → Review") == "Translate___Review", "arrow in name");
    VERIFY(PipelineVersionManager::SanitizeName("My Pipeline v2.0") == "My_Pipeline_v2_0", "complex name");
    std::cout << "Test Passed: SanitizeName (Optimizer)" << std::endl;
}

void TestParseProposals() {
    {
        // Valid JSON array
        std::string response = R"([
            {"op":"replace","stepName":"Translate","field":"userPrompt","oldValue":"old","newValue":"new","rationale":"Better output"}
        ])";
        auto proposals = PipelineOptimizer::ParseProposals(response);
        VERIFY(proposals.size() == 1, "should parse 1 proposal");
        VERIFY(proposals[0].op == "replace", "op should be replace");
        VERIFY(proposals[0].stepName == "Translate", "step name should be Translate");
        VERIFY(proposals[0].field == "userPrompt", "field should be userPrompt");
        VERIFY(proposals[0].rationale == "Better output", "rationale should match");
    }
    {
        // Multiple proposals
        std::string response = R"([
            {"op":"replace","stepName":"Step1","field":"systemPrompt","oldValue":"a","newValue":"b","rationale":"r1"},
            {"op":"add","stepName":"Step2","field":"userPrompt","oldValue":"","newValue":"Added text","rationale":"r2"},
            {"op":"delete","stepName":"Step3","field":"systemPrompt","oldValue":"Remove this","newValue":"","rationale":"r3"}
        ])";
        auto proposals = PipelineOptimizer::ParseProposals(response);
        VERIFY(proposals.size() == 3, "should parse 3 proposals");
        VERIFY(proposals[0].op == "replace", "first op");
        VERIFY(proposals[1].op == "add", "second op");
        VERIFY(proposals[2].op == "delete", "third op");
    }
    {
        // Empty response
        auto proposals = PipelineOptimizer::ParseProposals("");
        VERIFY(proposals.empty(), "empty response should return empty proposals");
    }
    {
        // Response with extra text before/after JSON
        std::string response = "Here are my suggestions:\n[\n{\"op\":\"replace\",\"stepName\":\"Test\",\"field\":\"userPrompt\",\"oldValue\":\"x\",\"newValue\":\"y\",\"rationale\":\"z\"}\n]\nLet me know if helpful.";
        auto proposals = PipelineOptimizer::ParseProposals(response);
        VERIFY(proposals.size() == 1, "should extract proposal from markdown text");
        VERIFY(proposals[0].stepName == "Test", "step name should be Test");
    }
    {
        // Malformed JSON (no array)
        auto proposals = PipelineOptimizer::ParseProposals("not an array at all");
        VERIFY(proposals.empty(), "non-JSON response should return empty");
    }
    {
        // Empty array
        auto proposals = PipelineOptimizer::ParseProposals("[]");
        VERIFY(proposals.empty() || proposals.size() == 0, "empty array should return empty");
    }
    std::cout << "Test Passed: ParseProposals" << std::endl;
}

void TestSerializeProposals() {
    std::vector<OptEditProposal> proposals;
    OptEditProposal p1;
    p1.op = "replace";
    p1.stepName = "Translate";
    p1.field = "userPrompt";
    p1.oldValue = "old prompt";
    p1.newValue = "new prompt";
    p1.rationale = "Better output";
    proposals.push_back(p1);

    OptEditProposal p2;
    p2.op = "add";
    p2.stepName = "Review";
    p2.field = "systemPrompt";
    p2.oldValue = "";
    p2.newValue = "Be thorough";
    p2.rationale = "Catch errors";
    proposals.push_back(p2);

    std::string json = PipelineOptimizer::SerializeProposals(proposals);
    VERIFY(!json.empty(), "serialized proposals should not be empty");
    VERIFY(json.find("Translate") != std::string::npos, "json should contain Translate");
    VERIFY(json.find("Review") != std::string::npos, "json should contain Review");

    // Round-trip through ParseProposals
    auto reparsed = PipelineOptimizer::ParseProposals(json);
    VERIFY(reparsed.size() == 2, "round-trip should preserve 2 proposals");
    VERIFY(reparsed[0].stepName == "Translate", "round-trip first step name");
    VERIFY(reparsed[1].stepName == "Review", "round-trip second step name");
    std::cout << "Test Passed: SerializeProposals" << std::endl;
}

void TestSerializeSession() {
    OptSession session;
    session.pipelineName = "Translate → Review";
    session.sessionId = "opt_20250601_120000";

    std::string json = PipelineOptimizer::SerializeSession(session);
    VERIFY(!json.empty(), "serialized session should not be empty");
    VERIFY(json.find("Translate") != std::string::npos, "json should contain Translate");
    VERIFY(json.find("opt_") != std::string::npos, "json should contain session prefix");
    std::cout << "Test Passed: SerializeSession" << std::endl;
}

void TestBuildOptimizerPrompt() {
    Pipeline p;
    p.name = "TestPipe";
    PipelineStep s;
    s.name = "Generate";
    s.type = "ai";
    s.params["systemPrompt"] = "You are a writer";
    s.params["userPrompt"] = "Write about {content}";
    p.steps.push_back(s);

    HistoryRecord okRec;
    okRec.id = "run_001";
    okRec.startedAt = "2025-01-01T00:00:00Z";
    HistoryStep okStep;
    okStep.name = "Generate";
    okStep.type = "ai";
    okStep.input = "cats";
    okStep.output = "Cats are fluffy";
    okStep.evaluation = "ok";
    okRec.steps.push_back(okStep);

    HistoryRecord rejRec;
    rejRec.id = "run_002";
    rejRec.startedAt = "2025-01-02T00:00:00Z";
    HistoryStep rejStep;
    rejStep.name = "Generate";
    rejStep.type = "ai";
    rejStep.input = "dogs";
    rejStep.output = "Dogs";
    rejStep.evaluation = "rejected";
    rejStep.evaluationNote = "Too short";
    rejRec.steps.push_back(rejStep);

    std::vector<std::string> pinned = {"This content is pinned and must not change"};
    std::vector<OptEditProposal> rejectedBuffer;
    OptEditProposal rb;
    rb.op = "replace";
    rb.stepName = "Generate";
    rb.field = "systemPrompt";
    rb.rationale = "Previous failed attempt";
    rejectedBuffer.push_back(rb);

    std::string prompt = PipelineOptimizer::BuildOptimizerPrompt(
        p, {okRec}, {rejRec}, pinned, rejectedBuffer, 3);

    VERIFY(!prompt.empty(), "prompt should not be empty");
    VERIFY(prompt.find("TestPipe") != std::string::npos, "prompt should contain pipeline name");
    VERIFY(prompt.find("Generate") != std::string::npos, "prompt should contain step name");
    VERIFY(prompt.find("You are a writer") != std::string::npos, "prompt should contain system prompt");
    VERIFY(prompt.find("cats") != std::string::npos, "prompt should contain OK sample input");
    VERIFY(prompt.find("fluffy") != std::string::npos, "prompt should contain OK sample output");
    VERIFY(prompt.find("dogs") != std::string::npos, "prompt should contain rejected sample input");
    VERIFY(prompt.find("pinned") != std::string::npos, "prompt should mention pinned content");
    VERIFY(prompt.find("Previous failed attempt") != std::string::npos, "prompt should contain rejected buffer");
    VERIFY(prompt.find("3") != std::string::npos, "prompt should contain maxEdits");
    std::cout << "Test Passed: BuildOptimizerPrompt" << std::endl;
}

void TestApplyApprovals() {
    Pipeline p;
    p.name = "TestPipe";
    PipelineStep s;
    s.name = "Translate";
    s.type = "ai";
    s.params["provider"] = "openai";
    s.params["userPrompt"] = "Old prompt";
    p.steps.push_back(s);

    OptSession session;
    session.pipelineName = "TestPipe";
    session.sessionId = "test_session";

    OptEditProposal replaceProp;
    replaceProp.op = "replace";
    replaceProp.stepName = "Translate";
    replaceProp.field = "userPrompt";
    replaceProp.oldValue = "Old prompt";
    replaceProp.newValue = "New prompt";

    OptEditProposal addProp;
    addProp.op = "add";
    addProp.stepName = "Translate";
    addProp.field = "systemPrompt";
    addProp.oldValue = "";
    addProp.newValue = "You are helpful";

    session.proposals.push_back(replaceProp);
    session.proposals.push_back(addProp);

    Pipeline result = PipelineOptimizer::ApplyApprovals(p, {0, 1}, {}, session);
    VERIFY(result.steps[0].params["userPrompt"] == "New prompt", "replace should update prompt");
    VERIFY(result.steps[0].params["systemPrompt"] == "You are helpful", "add should append");
    VERIFY(session.rejectedBuffer.empty(), "no rejected proposals should accumulate");
    std::cout << "Test Passed: ApplyApprovals (Replace+Add)" << std::endl;
}

void TestApplyApprovalsDelete() {
    Pipeline p;
    p.name = "TestPipe";
    PipelineStep s;
    s.name = "Step1";
    s.type = "ai";
    s.params["userPrompt"] = "To delete";
    s.params["systemPrompt"] = "Keep this";
    p.steps.push_back(s);

    OptSession session;
    session.pipelineName = "TestPipe";
    session.sessionId = "test_session2";

    OptEditProposal delProp;
    delProp.op = "delete";
    delProp.stepName = "Step1";
    delProp.field = "userPrompt";
    delProp.oldValue = "To delete";
    delProp.newValue = "";

    session.proposals.push_back(delProp);

    Pipeline result = PipelineOptimizer::ApplyApprovals(p, {0}, {}, session);
    VERIFY(result.steps[0].params["userPrompt"] == "", "delete should clear the field");
    VERIFY(result.steps[0].params["systemPrompt"] == "Keep this", "other fields should be untouched");
    std::cout << "Test Passed: ApplyApprovals (Delete)" << std::endl;
}

void TestApplyApprovalsRejectedBuffer() {
    Pipeline p;
    p.name = "TestPipe";
    PipelineStep s;
    s.name = "Step1";
    s.type = "ai";
    s.params["userPrompt"] = "Prompt";
    p.steps.push_back(s);

    OptSession session;
    session.pipelineName = "TestPipe";
    session.sessionId = "test_session3";

    OptEditProposal rejectedProp;
    rejectedProp.op = "replace";
    rejectedProp.stepName = "Step1";
    rejectedProp.field = "userPrompt";
    rejectedProp.oldValue = "Prompt";
    rejectedProp.newValue = "Changed";
    rejectedProp.rationale = "Test rejected";

    session.proposals.push_back(rejectedProp);

    Pipeline result = PipelineOptimizer::ApplyApprovals(p, {}, {0}, session);
    VERIFY(result.steps[0].params["userPrompt"] == "Prompt", "rejected proposal should not change");
    VERIFY(session.rejectedBuffer.size() == 1, "rejected buffer should have 1 entry");
    VERIFY(session.rejectedBuffer[0].rationale == "Test rejected", "rejected rationale should match");
    std::cout << "Test Passed: ApplyApprovals (Rejected Buffer)" << std::endl;
}

void TestApplyApprovalsPartialSelect() {
    Pipeline p;
    p.name = "TestPipe";
    PipelineStep s;
    s.name = "MultiStep";
    s.type = "ai";
    s.params["userPrompt"] = "Original";
    p.steps.push_back(s);

    OptSession session;
    session.pipelineName = "TestPipe";
    session.sessionId = "test_session4";

    OptEditProposal p1, p2, p3;
    p1.op = "replace"; p1.stepName = "MultiStep"; p1.field = "userPrompt"; p1.oldValue = "Original"; p1.newValue = "NewValue1";
    p2.op = "add"; p2.stepName = "MultiStep"; p2.field = "systemPrompt"; p2.oldValue = ""; p2.newValue = "System instruction";
    p3.op = "delete"; p3.stepName = "MultiStep"; p3.field = "userPrompt"; p3.oldValue = "Original"; p3.newValue = "";

    session.proposals.push_back(p1);
    session.proposals.push_back(p2);
    session.proposals.push_back(p3);

    // Approve only p1 and p3, reject p2
    Pipeline result = PipelineOptimizer::ApplyApprovals(p, {0, 2}, {1}, session);
    VERIFY(result.steps[0].params["userPrompt"] == "", "p3 approved: should delete userPrompt");
    VERIFY(result.steps[0].params["systemPrompt"] != "System instruction", "p2 rejected: should not be added");
    VERIFY(session.rejectedBuffer.size() == 1, "1 rejected entry");
    VERIFY(session.rejectedBuffer[0].stepName == "MultiStep", "rejected entry step name");
    std::cout << "Test Passed: ApplyApprovals (Partial Select)" << std::endl;
}

void TestApplyApprovalsInvalidIndex() {
    Pipeline p;
    p.name = "TestPipe";
    PipelineStep s;
    s.name = "Step1";
    s.type = "ai";
    p.steps.push_back(s);

    OptSession session;
    session.pipelineName = "TestPipe";
    session.sessionId = "test_session5";
    session.proposals.push_back({});

    // Out of bounds indices should not crash
    Pipeline result = PipelineOptimizer::ApplyApprovals(p, {5, -1, 100}, {}, session);
    VERIFY(result.name == "TestPipe", "invalid indices should not crash");
    std::cout << "Test Passed: ApplyApprovals (Invalid Index)" << std::endl;
}

int main() {
    try {
        // PipelineVersionManager tests
        TestSanitizeName();
        TestSerializeDeserializeCursor();
        TestSerializeDeserializeSnapshot();

        // PipelineOptimizer tests
        TestTruncateUtf8();
        TestSanitizeNameOptimizer();
        TestParseProposals();
        TestSerializeProposals();
        TestSerializeSession();
        TestBuildOptimizerPrompt();
        TestApplyApprovals();
        TestApplyApprovalsDelete();
        TestApplyApprovalsRejectedBuffer();
        TestApplyApprovalsPartialSelect();
        TestApplyApprovalsInvalidIndex();

        std::cout << "=== ALL OPTIMIZER & VERSION MANAGER TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
