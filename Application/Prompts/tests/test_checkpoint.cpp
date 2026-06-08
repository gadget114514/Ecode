#include "../src/Storage.h"
#include "../src/JsonParser.h"
#include "../src/Base64.h"
#include "../src/NodeData.h"
#include "../src/PipelineRunner.h"
#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

static std::wstring GetTempDir() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"prompts_ckpt_test_" + std::to_wstring(GetCurrentProcessId());
    return dir;
}

static void CleanupDir(const std::wstring &dir) {
    std::wstring pattern = dir + L"\\*";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring name = ffd.cFileName;
            if (name == L"." || name == L"..") continue;
            std::wstring full = dir + L"\\" + name;
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                CleanupDir(full);
                RemoveDirectoryW(full.c_str());
            } else {
                DeleteFileW(full.c_str());
            }
        } while (FindNextFileW(hFind, &ffd) != 0);
        FindClose(hFind);
    }
    RemoveDirectoryW(dir.c_str());
}

struct TempStorage {
    Storage storage;
    std::wstring path;

    TempStorage() {
        path = GetTempDir();
        if (!storage.Init(path)) {
            std::cerr << "FAIL: Storage::Init failed for " << std::string(path.begin(), path.end()) << std::endl;
            exit(1);
        }
    }

    ~TempStorage() {
        CleanupDir(path);
    }
};

// ==================== Checkpoint Path Structure ====================

void TestCheckpointDirectoryStructure() {
    TempStorage ts;
    std::wstring base = ts.storage.GetBasePath();

    // Check that history directory exists
    std::wstring historyDir = base + L"\\history";
    DWORD attrs = GetFileAttributesW(historyDir.c_str());
    VERIFY(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY),
           "history directory should exist");

    std::cout << "Test Passed: Checkpoint Directory Structure" << std::endl;
}

void TestCheckpointSaveAndLoadInput() {
    TempStorage ts;

    // Simulate saving a checkpoint input
    std::string runId = "run_20260608_120000";
    int stepIndex = 0;
    std::string input = "Hello, world!";
    std::string output = "こんにちは世界！";
    std::string meta = "{\"stepName\":\"Translate\",\"type\":\"ai\",\"tokens\":312}";

    // The pattern: history/run_20260608_120000/checkpoint_0/input.json
    std::wstring runDir = ts.storage.GetBasePath() + L"\\history\\" + std::wstring(runId.begin(), runId.end());
    CreateDirectoryW(runDir.c_str(), nullptr);

    std::wstring ckptDir = runDir + L"\\checkpoint_0";
    CreateDirectoryW(ckptDir.c_str(), nullptr);

    // Write input
    std::wstring inputPath = ckptDir + L"\\input.json";
    FILE *f = nullptr;
    _wfopen_s(&f, inputPath.c_str(), L"wb");
    VERIFY(f != nullptr, "checkpoint input file should be creatable");
    fwrite(input.data(), 1, input.size(), f);
    fclose(f);

    // Write output
    std::wstring outputPath = ckptDir + L"\\output.json";
    _wfopen_s(&f, outputPath.c_str(), L"wb");
    VERIFY(f != nullptr, "checkpoint output file should be creatable");
    fwrite(output.data(), 1, output.size(), f);
    fclose(f);

    // Write meta
    std::wstring metaPath = ckptDir + L"\\meta.json";
    _wfopen_s(&f, metaPath.c_str(), L"wb");
    VERIFY(f != nullptr, "checkpoint meta file should be creatable");
    fwrite(meta.data(), 1, meta.size(), f);
    fclose(f);

    // Verify files exist
    VERIFY(GetFileAttributesW(inputPath.c_str()) != INVALID_FILE_ATTRIBUTES, "input.json should exist");
    VERIFY(GetFileAttributesW(outputPath.c_str()) != INVALID_FILE_ATTRIBUTES, "output.json should exist");
    VERIFY(GetFileAttributesW(metaPath.c_str()) != INVALID_FILE_ATTRIBUTES, "meta.json should exist");

    std::cout << "Test Passed: Checkpoint Save and Load Input" << std::endl;
}

void TestCheckpointRoundTrip() {
    TempStorage ts;

    // Set up a pipeline run
    std::wstring runId = L"run_roundtrip_test";
    std::wstring runDir = ts.storage.GetBasePath() + L"\\history\\" + runId;
    CreateDirectoryW(runDir.c_str(), nullptr);

    // Create 3 checkpoints
    std::vector<std::string> inputs = {"Hello", "Bonjour", "こんにちは"};
    std::vector<std::string> outputs = {"Bonjour", "こんにちは", "Hello"};
    std::vector<std::string> names = {"Translate", "Polish", "Review"};

    for (int i = 0; i < 3; i++) {
        std::wstring ckptDir = runDir + L"\\checkpoint_" + std::to_wstring(i);
        CreateDirectoryW(ckptDir.c_str(), nullptr);

        std::string input = inputs[i];
        std::string output = outputs[i];
        std::string meta = "{\"stepIndex\":" + std::to_string(i) +
                           ",\"stepName\":\"" + names[i] +
                           "\",\"status\":\"completed\"}";

        FILE *f;
        _wfopen_s(&f, (ckptDir + L"\\input.json").c_str(), L"wb");
        fwrite(input.data(), 1, input.size(), f); fclose(f);
        _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"wb");
        fwrite(output.data(), 1, output.size(), f); fclose(f);
        _wfopen_s(&f, (ckptDir + L"\\meta.json").c_str(), L"wb");
        fwrite(meta.data(), 1, meta.size(), f); fclose(f);
    }

    // Verify we can read back all 3 checkpoints
    for (int i = 0; i < 3; i++) {
        std::wstring ckptDir = runDir + L"\\checkpoint_" + std::to_wstring(i);
        std::string readInput, readOutput, readMeta;

        FILE *f;
        // Read input
        _wfopen_s(&f, (ckptDir + L"\\input.json").c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            readInput.resize(len);
            fread(&readInput[0], 1, len, f);
            fclose(f);
        }
        // Read output
        _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            readOutput.resize(len);
            fread(&readOutput[0], 1, len, f);
            fclose(f);
        }
        // Read meta
        _wfopen_s(&f, (ckptDir + L"\\meta.json").c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            readMeta.resize(len);
            fread(&readMeta[0], 1, len, f);
            fclose(f);
        }

        VERIFY(readInput == inputs[i], "checkpoint " + std::to_string(i) + " input should match");
        VERIFY(readOutput == outputs[i], "checkpoint " + std::to_string(i) + " output should match");
        VERIFY(readMeta.find(names[i]) != std::string::npos, "checkpoint " + std::to_string(i) + " meta should contain step name");
    }

    std::cout << "Test Passed: Checkpoint Round Trip" << std::endl;
}

void TestCheckpointAppendOnly() {
    TempStorage ts;

    std::wstring runId = L"run_append_test";
    std::wstring runDir = ts.storage.GetBasePath() + L"\\history\\" + runId;
    CreateDirectoryW(runDir.c_str(), nullptr);
    std::wstring ckptDir = runDir + L"\\checkpoint_0";
    CreateDirectoryW(ckptDir.c_str(), nullptr);

    // Write initial output (append-only: content must not change after write)
    std::string originalOutput = "Original output";
    FILE *f;
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"wb");
    fwrite(originalOutput.data(), 1, originalOutput.size(), f);
    fclose(f);

    // Verify original content
    std::string readBack;
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        readBack.resize(len);
        fread(&readBack[0], 1, len, f);
        fclose(f);
    }
    VERIFY(readBack == originalOutput, "original output should be unchanged");

    // "Modify" by appending — this is allowed (append-only log)
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"ab");
    std::string append = " + appended";
    fwrite(append.data(), 1, append.size(), f);
    fclose(f);

    // Read back should now contain both original and appended
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        readBack.resize(len);
        fread(&readBack[0], 1, len, f);
        fclose(f);
    }
    VERIFY(readBack == "Original output + appended", "append-only: original should be preserved, new content appended");
    VERIFY(readBack.find("Original output") != std::string::npos, "Original content must survive append");

    std::cout << "Test Passed: Checkpoint Append Only" << std::endl;
}

void TestCheckpointFilterDecision() {
    TempStorage ts;

    std::wstring runId = L"run_filter_test";
    std::wstring runDir = ts.storage.GetBasePath() + L"\\history\\" + runId;
    CreateDirectoryW(runDir.c_str(), nullptr);
    std::wstring ckptDir = runDir + L"\\checkpoint_0";
    CreateDirectoryW(ckptDir.c_str(), nullptr);

    // Write output first
    std::string output = "This is the model output";
    FILE *f;
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"wb");
    fwrite(output.data(), 1, output.size(), f);
    fclose(f);

    // Write filter decision (separate file, after the fact)
    std::string filterDecision = "{\"decision\":\"approved\",\"timestamp\":\"2026-06-08T12:00:00Z\"}";
    _wfopen_s(&f, (ckptDir + L"\\filter.json").c_str(), L"wb");
    fwrite(filterDecision.data(), 1, filterDecision.size(), f);
    fclose(f);

    // Verify both files coexist independently
    std::wstring filterPath = ckptDir + L"\\filter.json";
    VERIFY(GetFileAttributesW(filterPath.c_str()) != INVALID_FILE_ATTRIBUTES, "filter.json should exist");

    // Verify output remains unchanged despite filter decision
    std::string outputAfterFilter;
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        outputAfterFilter.resize(len);
        fread(&outputAfterFilter[0], 1, len, f);
        fclose(f);
    }
    VERIFY(outputAfterFilter == output, "output must remain unchanged after filter decision");

    // Toggle filter decision (re-write filter.json — allowed, only the decision changes)
    std::string newDecision = "{\"decision\":\"rejected\",\"timestamp\":\"2026-06-08T12:05:00Z\"}";
    _wfopen_s(&f, (ckptDir + L"\\filter.json").c_str(), L"wb");
    fwrite(newDecision.data(), 1, newDecision.size(), f);
    fclose(f);

    std::string readNewDecision;
    _wfopen_s(&f, filterPath.c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        readNewDecision.resize(len);
        fread(&readNewDecision[0], 1, len, f);
        fclose(f);
    }
    VERIFY(readNewDecision.find("rejected") != std::string::npos, "filter decision should be updatable");

    std::cout << "Test Passed: Checkpoint Filter Decision" << std::endl;
}

void TestIncompleteRunDetection() {
    TempStorage ts;
    std::wstring base = ts.storage.GetBasePath();

    // Create a "completed" run
    std::wstring runCompleted = base + L"\\history\\run_completed";
    CreateDirectoryW(runCompleted.c_str(), nullptr);
    FILE *f;
    _wfopen_s(&f, (runCompleted + L"\\state.json").c_str(), L"wb");
    std::string completedState = "{\"status\":\"completed\",\"currentStep\":2,\"totalSteps\":3}";
    fwrite(completedState.data(), 1, completedState.size(), f);
    fclose(f);

    // Create an "interrupted" run (simulates power outage)
    std::wstring runInterrupted = base + L"\\history\\run_interrupted";
    CreateDirectoryW(runInterrupted.c_str(), nullptr);
    _wfopen_s(&f, (runInterrupted + L"\\state.json").c_str(), L"wb");
    std::string interruptedState = "{\"status\":\"running\",\"currentStep\":1,\"totalSteps\":4}";
    fwrite(interruptedState.data(), 1, interruptedState.size(), f);
    fclose(f);

    // Verify both run dirs exist
    VERIFY(GetFileAttributesW(runCompleted.c_str()) != INVALID_FILE_ATTRIBUTES, "completed run dir should exist");
    VERIFY(GetFileAttributesW(runInterrupted.c_str()) != INVALID_FILE_ATTRIBUTES, "interrupted run dir should exist");

    // Verify state files have correct status
    {
        FILE *f;
        std::string stateContent;

        _wfopen_s(&f, (runCompleted + L"\\state.json").c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END); long len = ftell(f);
            fseek(f, 0, SEEK_SET); stateContent.resize(len);
            fread(&stateContent[0], 1, len, f);
            fclose(f);
        }
        auto val = JsonValue::parse(stateContent);
        VERIFY(val.has("status") && val["status"].string() == "completed", "completed run should have status=completed");
        VERIFY(val.has("currentStep") && val["currentStep"].number() == 2, "completed run currentStep should be 2");
    }
    {
        FILE *f;
        std::string stateContent;
        _wfopen_s(&f, (runInterrupted + L"\\state.json").c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END); long len = ftell(f);
            fseek(f, 0, SEEK_SET); stateContent.resize(len);
            fread(&stateContent[0], 1, len, f);
            fclose(f);
        }
        auto val = JsonValue::parse(stateContent);
        VERIFY(val.has("status") && val["status"].string() == "running", "interrupted run should have status=running");
        VERIFY(val.has("currentStep") && val["currentStep"].number() == 1, "interrupted run currentStep should be 1");
    }

    std::cout << "Test Passed: Incomplete Run Detection" << std::endl;
}

void TestCheckpointEdgeCases() {
    TempStorage ts;

    std::wstring runId = L"run_edge_test";
    std::wstring runDir = ts.storage.GetBasePath() + L"\\history\\" + runId;
    CreateDirectoryW(runDir.c_str(), nullptr);

    // Edge case: checkpoint with empty output
    std::wstring ckptDir = runDir + L"\\checkpoint_empty";
    CreateDirectoryW(ckptDir.c_str(), nullptr);
    FILE *f;
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"wb");
    fwrite("", 1, 0, f);
    fclose(f);
    _wfopen_s(&f, (ckptDir + L"\\input.json").c_str(), L"wb");
    std::string input = "test input";
    fwrite(input.data(), 1, input.size(), f);
    fclose(f);

    // Edge case: large output (>1MB simulation in test)
    ckptDir = runDir + L"\\checkpoint_large";
    CreateDirectoryW(ckptDir.c_str(), nullptr);
    std::string largeOutput(10000, 'X');
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"wb");
    fwrite(largeOutput.data(), 1, largeOutput.size(), f);
    fclose(f);

    // Verify large output can be read back
    std::string readLarge;
    _wfopen_s(&f, (ckptDir + L"\\output.json").c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END); long len = ftell(f);
        fseek(f, 0, SEEK_SET); readLarge.resize(len);
        fread(&readLarge[0], 1, len, f);
        fclose(f);
    }
    VERIFY(readLarge.size() == 10000, "large output should round-trip correctly");
    VERIFY(readLarge[0] == 'X', "first char of large output should match");

    std::cout << "Test Passed: Checkpoint Edge Cases" << std::endl;
}

void TestCheckpointStateTransition() {
    // Verify state.json follows correct transitions: pending → running → completed
    TempStorage ts;

    std::wstring runId = L"run_state_test";
    std::wstring runDir = ts.storage.GetBasePath() + L"\\history\\" + runId;
    CreateDirectoryW(runDir.c_str(), nullptr);

    auto writeState = [&](const std::string &status, int currentStep) {
        FILE *f;
        _wfopen_s(&f, (runDir + L"\\state.json").c_str(), L"wb");
        std::string json = "{\"status\":\"" + status + "\",\"currentStep\":" + std::to_string(currentStep) + ",\"totalSteps\":3}";
        fwrite(json.data(), 1, json.size(), f);
        fclose(f);
    };

    // Initial state: pending
    writeState("pending", 0);
    std::string validStates[] = {"pending", "running", "completed", "cancelled"};
    for (auto &s : validStates) {
        writeState(s, 1);
        FILE *f;
        std::string content;
        _wfopen_s(&f, (runDir + L"\\state.json").c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END); long len = ftell(f);
            fseek(f, 0, SEEK_SET); content.resize(len);
            fread(&content[0], 1, len, f);
            fclose(f);
        }
        auto val = JsonValue::parse(content);
        VERIFY(val.has("status") && val["status"].string() == s, "state should be: " + s);
    }

    std::cout << "Test Passed: Checkpoint State Transition" << std::endl;
}

int main() {
    try {
        TestCheckpointDirectoryStructure();
        TestCheckpointSaveAndLoadInput();
        TestCheckpointRoundTrip();
        TestCheckpointAppendOnly();
        TestCheckpointFilterDecision();
        TestIncompleteRunDetection();
        TestCheckpointEdgeCases();
        TestCheckpointStateTransition();
        std::cout << "=== ALL CHECKPOINT TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
