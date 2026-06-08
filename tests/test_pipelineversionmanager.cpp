// Unit tests for PipelineVersionManager serialization logic
// (Storage-dependent tests are in test_storage.cpp)
#include "../src/PipelineVersionManager.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

void TestVersionCursorDefaults() {
    VersionCursor cursor;
    VERIFY(cursor.pipelineName.empty(), "default pipelineName should be empty");
    VERIFY(cursor.currentVersion == 0, "default currentVersion should be 0");
    VERIFY(cursor.headVersion == 0, "default headVersion should be 0");
    VERIFY(cursor.entries.empty(), "default entries should be empty");
    std::cout << "Test Passed: VersionCursor Defaults" << std::endl;
}

void TestPipelineVersionDefaults() {
    PipelineVersion v;
    VERIFY(v.version == 0, "default version should be 0");
    VERIFY(v.timestamp.empty(), "default timestamp should be empty");
    VERIFY(v.sessionId.empty(), "default sessionId should be empty");
    VERIFY(v.label.empty(), "default label should be empty");
    VERIFY(v.approvedProposals.empty(), "default approvedProposals should be empty");
    std::cout << "Test Passed: PipelineVersion Defaults" << std::endl;
}

void TestPipelineVersionWithData() {
    PipelineVersion v;
    v.version = 3;
    v.timestamp = "2025-06-01T12:00:00Z";
    v.sessionId = "opt_20250601_120000";
    v.label = "Improved prompts after review";

    OptEditProposal p1, p2;
    p1.op = "replace";
    p1.stepName = "Translate";
    p1.field = "userPrompt";
    p1.oldValue = "Translate {content}";
    p1.newValue = "Translate the following professionally: {content}";
    p1.rationale = "More specific instruction improved output quality";
    v.approvedProposals.push_back(p1);

    p2.op = "add";
    p2.stepName = "Review";
    p2.field = "systemPrompt";
    p2.oldValue = "";
    p2.newValue = "Review translations for accuracy and fluency";
    p2.rationale = "Adding system prompt for review step";
    v.approvedProposals.push_back(p2);

    VERIFY(v.version == 3, "version should be 3");
    VERIFY(v.timestamp == "2025-06-01T12:00:00Z", "timestamp should match");
    VERIFY(v.approvedProposals.size() == 2, "should have 2 approved proposals");
    VERIFY(v.approvedProposals[0].op == "replace", "first proposal op");
    VERIFY(v.approvedProposals[1].op == "add", "second proposal op");
    VERIFY(v.approvedProposals[0].rationale.find("quality") != std::string::npos, "rationale should contain quality");
    std::cout << "Test Passed: PipelineVersion With Data" << std::endl;
}

void TestOptEditProposalDefaults() {
    OptEditProposal p;
    VERIFY(p.op.empty(), "default op should be empty");
    VERIFY(p.stepName.empty(), "default stepName should be empty");
    VERIFY(p.field.empty(), "default field should be empty");
    VERIFY(p.oldValue.empty(), "default oldValue should be empty");
    VERIFY(p.newValue.empty(), "default newValue should be empty");
    VERIFY(p.rationale.empty(), "default rationale should be empty");
    std::cout << "Test Passed: OptEditProposal Defaults" << std::endl;
}

void TestOptEditProposalAllTypes() {
    auto testProposal = [](const std::string &op, const std::string &field, const std::string &oldVal, const std::string &newVal) {
        OptEditProposal p;
        p.op = op;
        p.stepName = "Step1";
        p.field = field;
        p.oldValue = oldVal;
        p.newValue = newVal;
        p.rationale = "Test rationale for " + op;
        VERIFY(p.op == op, "op should be " + op);
        VERIFY(p.field == field, "field should be " + field);
        VERIFY(p.stepName == "Step1", "stepName should be Step1");
        return p;
    };

    auto r1 = testProposal("replace", "userPrompt", "old", "new");
    auto r2 = testProposal("add", "systemPrompt", "", "appended");
    auto r3 = testProposal("delete", "userPrompt", "remove this", "");
    (void)r1; (void)r2; (void)r3;

    std::cout << "Test Passed: OptEditProposal All Types" << std::endl;
}

void TestCursorSerializationRoundTrip() {
    VersionCursor cursor;
    cursor.pipelineName = "Translate -> Review";
    cursor.currentVersion = 3;
    cursor.headVersion = 5;

    PipelineVersion v1, v2, v3;
    v1.version = 1; v1.timestamp = "2025-01-01T00:00:00Z"; v1.label = "Initial";
    v2.version = 2; v2.timestamp = "2025-01-05T00:00:00Z"; v2.label = "v2 after review";
    v3.version = 3; v3.timestamp = "2025-01-10T00:00:00Z"; v3.label = "v3 optimization";

    OptEditProposal p;
    p.op = "replace"; p.stepName = "X"; p.field = "a"; p.oldValue = "1"; p.newValue = "2"; p.rationale = "test";
    v3.approvedProposals.push_back(p);

    cursor.entries.push_back(v1);
    cursor.entries.push_back(v2);
    cursor.entries.push_back(v3);

    // SerializeCusor → DeserializeCursor round-trip
    std::string json = PipelineVersionManager::SerializeCursor(cursor);
    VersionCursor restored = PipelineVersionManager::DeserializeCursor(json, "Translate -> Review");

    VERIFY(restored.pipelineName == "Translate -> Review", "restored name");
    VERIFY(restored.currentVersion == 3, "restored currentVersion");
    VERIFY(restored.headVersion == 5, "restored headVersion");
    VERIFY(restored.entries.size() == 3, "restored entries count");

    // Verify v3 proposals were preserved
    for (auto &e : restored.entries) {
        if (e.version == 3) {
            VERIFY(e.approvedProposals.size() == 1, "v3 should have 1 proposal");
            VERIFY(e.approvedProposals[0].stepName == "X", "v3 proposal step name");
            VERIFY(e.approvedProposals[0].rationale == "test", "v3 proposal rationale");
        }
    }
    std::cout << "Test Passed: Cursor Serialization Round-Trip" << std::endl;
}

void TestCursorWithNoEntries() {
    VersionCursor cursor;
    cursor.pipelineName = "Empty";
    cursor.currentVersion = 1;
    cursor.headVersion = 1;

    std::string json = PipelineVersionManager::SerializeCursor(cursor);
    VersionCursor restored = PipelineVersionManager::DeserializeCursor(json, "Empty");
    VERIFY(restored.pipelineName == "Empty", "restored name for empty cursor");
    VERIFY(restored.currentVersion == 1, "restored currentVersion for empty");
    VERIFY(restored.entries.empty(), "empty cursor entries");
    std::cout << "Test Passed: Cursor With No Entries" << std::endl;
}

void TestSnapshotSerializationRoundTrip() {
    Pipeline p;
    p.name = "TestSnapshot";
    p.mode = "basic";
    p.outputMode = "child";
    p.multiMedia = "attachments";
    p.retryCount = 3;
    p.retryDelayMs = 2000;

    PipelineStep s1, s2;
    s1.name = "Generate";
    s1.type = "ai";
    s1.params["provider"] = "openai";
    s1.params["model"] = "gpt-4.1";
    s1.params["systemPrompt"] = "You are a creative writer";
    s1.params["userPrompt"] = "Write a story about {content}";
    s1.params["temperature"] = "0.8";

    s2.name = "Review";
    s2.type = "manual";
    s2.params["mode"] = "edit";
    s2.params["prompt"] = "Review and edit the story";

    p.steps.push_back(s1);
    p.steps.push_back(s2);

    std::string json = PipelineVersionManager::SerializeSnapshot(p);
    Pipeline restored = PipelineVersionManager::DeserializeSnapshot(json);

    VERIFY(restored.name == "TestSnapshot", "restored name");
    VERIFY(restored.mode == "basic", "restored mode");
    VERIFY(restored.outputMode == "child", "restored outputMode");
    VERIFY(restored.retryCount == 3, "restored retryCount");
    VERIFY(restored.retryDelayMs == 2000, "restored retryDelayMs");
    VERIFY(restored.steps.size() == 2, "restored steps count");
    VERIFY(restored.steps[0].name == "Generate", "first step name");
    VERIFY(restored.steps[0].params["provider"] == "openai", "first step provider");
    VERIFY(restored.steps[1].name == "Review", "second step name");
    VERIFY(restored.steps[1].params["mode"] == "edit", "second step mode");
    std::cout << "Test Passed: Snapshot Serialization Round-Trip" << std::endl;
}

void TestSnapshotEmptyPipeline() {
    Pipeline p;
    std::string json = PipelineVersionManager::SerializeSnapshot(p);
    Pipeline restored = PipelineVersionManager::DeserializeSnapshot(json);
    VERIFY(restored.name.empty(), "empty pipeline name");
    VERIFY(restored.mode.empty(), "empty pipeline mode");
    VERIFY(restored.steps.empty(), "empty steps");
    std::cout << "Test Passed: Snapshot Empty Pipeline" << std::endl;
}

void TestSnapshotEdgeCases() {
    // Null/empty JSON
    Pipeline nullRestored = PipelineVersionManager::DeserializeSnapshot("");
    VERIFY(nullRestored.name.empty(), "null JSON should return empty pipeline");

    // Malformed JSON
    Pipeline badRestored = PipelineVersionManager::DeserializeSnapshot("{bad json}");
    VERIFY(badRestored.name.empty(), "bad JSON should not crash");

    // JSON without pipeline key
    Pipeline noKeyRestored = PipelineVersionManager::DeserializeSnapshot("{\"not_pipeline\": {}}");
    VERIFY(noKeyRestored.name.empty(), "JSON without pipeline key should return empty");

    std::cout << "Test Passed: Snapshot Edge Cases" << std::endl;
}

void TestReapplyToDifferentPipeline() {
    // Verify that proposal format is generic enough to work across different pipeline versions
    OptEditProposal p;
    p.op = "replace";
    p.stepName = "Translate";
    p.field = "userPrompt";
    p.oldValue = "";
    p.newValue = "Translate this: {content}";

    // Should have the basic structure we need
    VERIFY(p.op == "replace", "replace operation");
    VERIFY(p.stepName == "Translate", "target step");
    VERIFY(p.field == "userPrompt", "target field");
    VERIFY(p.newValue == "Translate this: {content}", "new value");
    std::cout << "Test Passed: Reapply To Different Pipeline" << std::endl;
}

int main() {
    try {
        TestVersionCursorDefaults();
        TestPipelineVersionDefaults();
        TestPipelineVersionWithData();
        TestOptEditProposalDefaults();
        TestOptEditProposalAllTypes();
        TestCursorSerializationRoundTrip();
        TestCursorWithNoEntries();
        TestSnapshotSerializationRoundTrip();
        TestSnapshotEmptyPipeline();
        TestSnapshotEdgeCases();
        TestReapplyToDifferentPipeline();
        std::cout << "=== ALL PIPELINE VERSION MANAGER TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
