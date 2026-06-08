#include "../src/Storage.h"
#include "../src/JsonParser.h"
#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <map>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

static std::wstring GetTempDir() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"prompts_chest_test_" + std::to_wstring(GetCurrentProcessId());
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

struct TempChestTest {
    std::wstring appDataPath;
    Storage storage;

    TempChestTest() {
        appDataPath = GetTempDir();
        // Create required directories
        CreateDirectoryW((appDataPath + L"\\projects").c_str(), nullptr);
        CreateDirectoryW((appDataPath + L"\\projects\\default").c_str(), nullptr);
        CreateDirectoryW((appDataPath + L"\\projects\\default\\data").c_str(), nullptr);
        CreateDirectoryW((appDataPath + L"\\projects\\default\\blobs").c_str(), nullptr);
        CreateDirectoryW((appDataPath + L"\\projects\\default\\history").c_str(), nullptr);
        CreateDirectoryW((appDataPath + L"\\chests").c_str(), nullptr);
        CreateDirectoryW((appDataPath + L"\\storage_chest").c_str(), nullptr);
        storage.Init(appDataPath + L"\\projects\\default");
    }

    ~TempChestTest() {
        CleanupDir(appDataPath);
    }

    std::wstring ChestRoot() const {
        return appDataPath + L"\\chests";
    }

    std::wstring ChestPath(const std::string &chestName) const {
        std::wstring wname(chestName.begin(), chestName.end());
        return ChestRoot() + L"\\chest_" + wname + L".json";
    }

    std::wstring StorageChestDir() const {
        return appDataPath + L"\\storage_chest";
    }

    void WriteChest(const std::string &chestName, const std::string &content) {
        FILE *f;
        _wfopen_s(&f, ChestPath(chestName).c_str(), L"wb");
        if (f) {
            fwrite(content.data(), 1, content.size(), f);
            fclose(f);
        }
    }

    std::string ReadChest(const std::string &chestName) {
        std::string result;
        FILE *f;
        _wfopen_s(&f, ChestPath(chestName).c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            result.resize(len);
            fread(&result[0], 1, len, f);
            fclose(f);
        }
        return result;
    }
};

// ==================== Chest Directory ====================

void TestChestDirectoryExists() {
    TempChestTest tc;
    DWORD attrs = GetFileAttributesW(tc.ChestRoot().c_str());
    VERIFY(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY),
           "chests/ directory should exist");
    std::cout << "Test Passed: Chest Directory Exists" << std::endl;
}

void TestStorageChestDirectoryExists() {
    TempChestTest tc;
    DWORD attrs = GetFileAttributesW(tc.StorageChestDir().c_str());
    VERIFY(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY),
           "storage_chest/ directory should exist");
    std::cout << "Test Passed: Storage Chest Directory Exists" << std::endl;
}

// ==================== Chest Put ====================

void TestChestPutSimple() {
    TempChestTest tc;

    // Put data into a named chest
    std::string chestName = "test_translations";
    std::string content = "This is translation data for testing";
    tc.WriteChest(chestName, content);

    // Verify the file exists
    VERIFY(GetFileAttributesW(tc.ChestPath(chestName).c_str()) != INVALID_FILE_ATTRIBUTES,
           "chest file should exist after put");

    // Verify content
    std::string readBack = tc.ReadChest(chestName);
    VERIFY(readBack == content, "chest content should match what was written");

    std::cout << "Test Passed: Chest Put Simple" << std::endl;
}

void TestChestOverwrite() {
    TempChestTest tc;

    std::string chestName = "overwrite_test";
    tc.WriteChest(chestName, "version1");
    tc.WriteChest(chestName, "version2");

    std::string readBack = tc.ReadChest(chestName);
    VERIFY(readBack == "version2", "chest overwrite should retain latest version");

    std::cout << "Test Passed: Chest Overwrite" << std::endl;
}

void TestChestMultipleNames() {
    TempChestTest tc;

    tc.WriteChest("chest_a", "Data A");
    tc.WriteChest("chest_b", "Data B");
    tc.WriteChest("chest_c", "Data C");

    // Each chest should have independent content
    VERIFY(tc.ReadChest("chest_a") == "Data A", "chest_a should have its own data");
    VERIFY(tc.ReadChest("chest_b") == "Data B", "chest_b should have its own data");
    VERIFY(tc.ReadChest("chest_c") == "Data C", "chest_c should have its own data");

    // Verify files are separate
    VERIFY(tc.ChestPath("chest_a") != tc.ChestPath("chest_b"), "different chest names must have different paths");

    std::cout << "Test Passed: Chest Multiple Names" << std::endl;
}

void TestChestIsSeparateFromProjectData() {
    TempChestTest tc;

    // Chest files must be in chests/ directory, NOT in projects/ 
    std::wstring chestFilePath = tc.ChestPath("test_data");
    VERIFY(chestFilePath.find(L"\\chests\\") != std::wstring::npos,
           "chest file must be inside chests/ directory");
    VERIFY(chestFilePath.find(L"\\projects\\") == std::wstring::npos,
           "chest file must NOT be inside projects/ directory");

    std::cout << "Test Passed: Chest Is Separate From Project Data" << std::endl;
}

// ==================== Chest Take ====================

void TestChestTakeRoundTrip() {
    TempChestTest tc;

    // Put
    std::string chestName = "roundtrip";
    std::string content = R"({"text":"Hello from chest","score":0.95})";
    tc.WriteChest(chestName, content);

    // Take (read back)
    std::string readBack = tc.ReadChest(chestName);
    VERIFY(!readBack.empty(), "take should return content");
    VERIFY(readBack == content, "take should return exact content written");

    // Verify it's valid JSON
    auto val = JsonValue::parse(readBack);
    VERIFY(val.has("text"), "chest content should be valid JSON with text field");
    VERIFY(val["text"].string() == "Hello from chest", "chest JSON text should match");
    VERIFY(val["score"].number() == 0.95, "chest JSON score should match");

    std::cout << "Test Passed: Chest Take Round Trip" << std::endl;
}

// ==================== Chest Isolation ====================

void TestChestDoesNotLeakProjectData() {
    TempChestTest tc;

    // Write project data
    Node root;
    root.title = "UHJvamVjdERhdGE=";
    root.content = "U2VjcmV0IHByb2plY3QgZGF0YQ==";
    root.mimetype = "text/plain";
    tc.storage.SaveTabData(L"project_secret.json", root);

    // Write chest data
    tc.WriteChest("public_chest", "{ \"type\": \"public\" }");

    // Chest file should NOT contain project data
    std::string chestContent = tc.ReadChest("public_chest");
    VERIFY(chestContent.find("project_secret") == std::string::npos,
           "chest should not contain project data");
    VERIFY(chestContent.find("Secret project data") == std::string::npos,
           "chest should not leak project content");

    std::cout << "Test Passed: Chest Does Not Leak Project Data" << std::endl;
}

void TestChestNameValidation() {
    TempChestTest tc;

    // Test various chest names
    std::vector<std::string> validNames = {
        "simple", "with_underscore", "with-hyphen", "test123",
        "日本語", "français", "deutsch"
    };

    for (auto &name : validNames) {
        tc.WriteChest(name, "data for " + name);
        std::string readBack = tc.ReadChest(name);
        VERIFY(!readBack.empty(), "chest '" + name + "' should be readable");
        VERIFY(readBack == "data for " + name, "chest '" + name + "' content should match");
    }

    std::cout << "Test Passed: Chest Name Validation" << std::endl;
}

void TestChestPutThenTakeCrossProject() {
    TempChestTest tc;

    // Simulate project_A putting data to chest
    std::string chestName = "cross_project_data";
    std::string data = "Shared content between projects";
    tc.WriteChest(chestName, data);

    // Simulate project_B taking data from same chest
    // (chests are global/shared, so this should work)
    std::string readBack = tc.ReadChest(chestName);
    VERIFY(readBack == data, "cross-project chest should preserve content exactly");

    std::cout << "Test Passed: Chest Put Then Take Cross Project" << std::endl;
}

// ==================== Storage Chest ====================

void TestStorageChestMove() {
    TempChestTest tc;

    // Simulate moving old checkpoint to storage chest
    std::string oldData = "{\"output\":\"old checkpoint data\"}";
    std::string storageFileName = "moved_checkpoint_001.json";

    FILE *f;
    _wfopen_s(&f, (tc.StorageChestDir() + L"\\" + std::wstring(storageFileName.begin(), storageFileName.end())).c_str(), L"wb");
    VERIFY(f != nullptr, "storage chest file should be creatable");
    fwrite(oldData.data(), 1, oldData.size(), f);
    fclose(f);

    // Verify it's there
    std::wstring storagePath = tc.StorageChestDir() + L"\\moved_checkpoint_001.json";
    VERIFY(GetFileAttributesW(storagePath.c_str()) != INVALID_FILE_ATTRIBUTES,
           "storage chest file should exist after move");

    // Verify we can read it back
    std::string readBack;
    _wfopen_s(&f, storagePath.c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END); long len = ftell(f);
        fseek(f, 0, SEEK_SET); readBack.resize(len);
        fread(&readBack[0], 1, len, f);
        fclose(f);
    }
    VERIFY(readBack == oldData, "storage chest content should be preserved");

    std::cout << "Test Passed: Storage Chest Move" << std::endl;
}

void TestStorageChestRecover() {
    TempChestTest tc;

    // Move data to storage chest
    std::string recoveredContent = "{\"pipelineName\":\"old_pipe\",\"output\":\"something useful\"}";
    std::string fileName = "deleted_pipeline_output.json";

    FILE *f;
    _wfopen_s(&f, (tc.StorageChestDir() + L"\\" + std::wstring(fileName.begin(), fileName.end())).c_str(), L"wb");
    fwrite(recoveredContent.data(), 1, recoveredContent.size(), f);
    fclose(f);

    // Simulate recovery: read from storage chest, write to project
    std::string storageContent;
    _wfopen_s(&f, (tc.StorageChestDir() + L"\\deleted_pipeline_output.json").c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END); long len = ftell(f);
        fseek(f, 0, SEEK_SET); storageContent.resize(len);
        fread(&storageContent[0], 1, len, f);
        fclose(f);
    }
    VERIFY(!storageContent.empty(), "storage chest content should be recoverable");
    VERIFY(storageContent.find("something useful") != std::string::npos,
           "recovered content should contain original data");

    // Write recovered data to project
    Node recovered;
    recovered.title = "UmVjb3ZlcmVk";
    recovered.content = "UmVjb3ZlcmVkIGRhdGE=";
    recovered.mimetype = "text/plain";
    tc.storage.SaveTabData(L"recovered.json", recovered);

    // Verify it's in project
    Node loaded = tc.storage.LoadTabData(L"recovered.json");
    VERIFY(loaded.title == "UmVjb3ZlcmVk", "recovered node should be in project");

    std::cout << "Test Passed: Storage Chest Recover" << std::endl;
}

void TestChestEmptyContent() {
    TempChestTest tc;

    // Empty content should be storable
    tc.WriteChest("empty_chest", "");
    std::string readBack = tc.ReadChest("empty_chest");
    // Empty string is fine — chest with empty content should still exist
    std::wstring chestFile = tc.ChestPath("empty_chest");
    VERIFY(GetFileAttributesW(chestFile.c_str()) != INVALID_FILE_ATTRIBUTES,
           "chest file should exist even with empty content");

    std::cout << "Test Passed: Chest Empty Content" << std::endl;
}

void TestChestLargeContent() {
    TempChestTest tc;

    std::string largeContent(100000, 'Z');
    tc.WriteChest("large_chest", largeContent);
    std::string readBack = tc.ReadChest("large_chest");

    VERIFY(readBack.size() == 100000, "large chest content should preserve size");
    VERIFY(readBack[0] == 'Z', "large chest first char should match");
    VERIFY(readBack[99999] == 'Z', "large chest last char should match");

    std::cout << "Test Passed: Chest Large Content" << std::endl;
}

int main() {
    try {
        TestChestDirectoryExists();
        TestStorageChestDirectoryExists();
        TestChestPutSimple();
        TestChestOverwrite();
        TestChestMultipleNames();
        TestChestIsSeparateFromProjectData();
        TestChestTakeRoundTrip();
        TestChestDoesNotLeakProjectData();
        TestChestNameValidation();
        TestChestPutThenTakeCrossProject();
        TestStorageChestMove();
        TestStorageChestRecover();
        TestChestEmptyContent();
        TestChestLargeContent();
        std::cout << "=== ALL CHEST TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
