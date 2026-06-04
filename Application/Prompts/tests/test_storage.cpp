#include "../src/Storage.h"
#include "../src/JsonParser.h"
#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#define VERIFY(cond, msg) \
  if (!(cond)) { std::cerr << "FAIL at line " << __LINE__ << ": " << msg << std::endl; exit(1); }

static std::wstring GetTempDir() {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"prompts_test_" + std::to_wstring(GetCurrentProcessId());
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
        bool ok = storage.Init(path);
        if (!ok) { std::cerr << "FAIL: Storage::Init failed for " << std::string(path.begin(), path.end()) << std::endl; exit(1); }
    }

    ~TempStorage() {
        CleanupDir(path);
    }
};

void TestInit() {
    TempStorage ts;
    VERIFY(ts.storage.GetBasePath() == ts.path, "base path should match");
    std::cout << "Test Passed: Init" << std::endl;
}

void TestSessionRoundTrip() {
    TempStorage ts;

    SessionData session;
    TabData tab1, tab2;
    tab1.name = "General";
    tab1.file = "general.json";
    tab2.name = "Code";
    tab2.file = "code.json";
    session.tabs.push_back(tab1);
    session.tabs.push_back(tab2);

    ts.storage.SaveSession(session);

    auto loaded = ts.storage.LoadSession();
    VERIFY(loaded.tabs.size() == 2, "session should have 2 tabs");
    VERIFY(loaded.tabs[0].name == "General", "first tab name");
    VERIFY(loaded.tabs[0].file == "general.json", "first tab file (relative)");
    VERIFY(loaded.tabs[1].name == "Code", "second tab name");
    std::cout << "Test Passed: Session Save/Load" << std::endl;
}

void TestSessionDefaultCreation() {
    TempStorage ts;
    auto session = ts.storage.LoadSession();
    VERIFY(session.tabs.size() == 1, "default session should have 1 tab");
    VERIFY(session.tabs[0].name == "General", "default tab name should be General");
    VERIFY(session.tabs[0].file == "general.json", "default tab file");
    std::cout << "Test Passed: Session Default Creation" << std::endl;
}

void TestTabDataRoundTrip() {
    TempStorage ts;

    Node root;
    root.title = "Um9vdA==";
    root.content = "SGVsbG8gV29ybGQ=";
    root.mimetype = "text/plain";

    Node child;
    child.title = "Q2hpbGQ=";
    child.content = "Q2hpbGQgY29udGVudA==";
    child.mimetype = "text/html";
    root.children.push_back(child);

    ts.storage.SaveTabData(L"test.json", root);

    auto loaded = ts.storage.LoadTabData(L"test.json");
    VERIFY(loaded.title == "Um9vdA==", "root title should match");
    VERIFY(loaded.content == "SGVsbG8gV29ybGQ=", "root content should match");
    VERIFY(loaded.mimetype == "text/plain", "root mimetype should match");
    VERIFY(loaded.children.size() == 1, "root should have 1 child");
    VERIFY(loaded.children[0].title == "Q2hpbGQ=", "child title should match");
    VERIFY(loaded.children[0].mimetype == "text/html", "child mimetype should match");
    std::cout << "Test Passed: Tab Data Save/Load" << std::endl;
}

void TestBlobSaveLoad() {
    TempStorage ts;

    std::string data = "binary image data here";
    std::wstring name = ts.storage.SaveBlob(data, L".png");
    VERIFY(!name.empty(), "blob name should not be empty");
    VERIFY(name.find(L".png") != std::string::npos, "blob name should have png extension");

    std::string loaded = ts.storage.LoadBlob(name);
    VERIFY(loaded == data, "blob content should match");
    std::cout << "Test Passed: Blob Save/Load" << std::endl;
}

void TestBlobRemove() {
    TempStorage ts;

    std::string data = "test blob";
    std::wstring name = ts.storage.SaveBlob(data, L".txt");
    VERIFY(!ts.storage.LoadBlob(name).empty(), "blob should exist after save");

    ts.storage.RemoveBlob(name);
    VERIFY(ts.storage.LoadBlob(name).empty(), "blob should be empty after remove");
    std::cout << "Test Passed: Blob Remove" << std::endl;
}

void TestProvidersRoundTrip() {
    TempStorage ts;

    std::map<std::string, ProviderConfig> providers;
    ProviderConfig openai;
    openai.apiKey = "sk-abc123";
    openai.baseUrl = "https://api.openai.com/v1";
    openai.models = {"gpt-4", "gpt-3.5-turbo"};
    providers["openai"] = openai;

    ProviderConfig ollama;
    ollama.baseUrl = "http://localhost:11434";
    ollama.models = {"llama3", "mistral"};
    providers["ollama"] = ollama;

    ts.storage.SaveProviders(providers);

    auto loaded = ts.storage.LoadProviders();
    VERIFY(loaded.size() == 2, "should have 2 providers");
    VERIFY(loaded["openai"].apiKey == "sk-abc123", "openai api key");
    VERIFY(loaded["openai"].baseUrl == "https://api.openai.com/v1", "openai base url");
    VERIFY(loaded["openai"].models.size() == 2, "openai models");
    VERIFY(loaded["ollama"].baseUrl == "http://localhost:11434", "ollama base url");
    VERIFY(loaded["ollama"].models[0] == "llama3", "ollama first model");
    std::cout << "Test Passed: Providers Save/Load" << std::endl;
}

void TestPipelinesRoundTrip() {
    TempStorage ts;

    std::vector<Pipeline> pipelines;
    Pipeline pipe;
    pipe.name = "Translate";
    pipe.mode = "basic";
    pipe.outputMode = "child";

    PipelineStep step;
    step.name = "Translate";
    step.type = "ai";
    step.params["provider"] = "openai";
    step.params["model"] = "gpt-4.1";
    pipe.steps.push_back(step);
    pipelines.push_back(pipe);

    ts.storage.SavePipelines(pipelines);

    auto loaded = ts.storage.LoadPipelines();
    VERIFY(loaded.size() == 1, "should have 1 pipeline");
    VERIFY(loaded[0].name == "Translate", "pipeline name should match");
    VERIFY(loaded[0].steps.size() == 1, "pipeline should have 1 step");
    VERIFY(loaded[0].steps[0].name == "Translate", "step name");
    std::cout << "Test Passed: Pipelines Save/Load" << std::endl;
}

void TestGetTabFiles() {
    TempStorage ts;

    Node root;
    root.title = "VGVzdA==";
    root.mimetype = "text/plain";
    ts.storage.SaveTabData(L"tab1.json", root);
    ts.storage.SaveTabData(L"tab2.json", root);

    auto files = ts.storage.GetTabFiles();
    VERIFY(files.size() >= 2, "should find at least 2 tab files");
    std::cout << "Test Passed: Get Tab Files" << std::endl;
}

void TestPipelineMetaRoundTrip() {
    TempStorage ts;

    Node root;
    root.title = "Um9vdA==";
    root.mimetype = "text/plain";
    root.pipelineMeta = "{\"pipelineName\":\"TestPipe\",\"executedAt\":\"2026-06-04T12:00:00Z\",\"steps\":[]}";

    ts.storage.SaveTabData(L"meta_test.json", root);

    auto loaded = ts.storage.LoadTabData(L"meta_test.json");
    VERIFY(loaded.title == "Um9vdA==", "title match");
    VERIFY(loaded.pipelineMeta == root.pipelineMeta, "pipelineMeta should be serialized and deserialized correctly");
    std::cout << "Test Passed: Pipeline Meta Round-Trip" << std::endl;
}

int main() {
    try {
        TestInit();
        TestSessionDefaultCreation();
        TestSessionRoundTrip();
        TestTabDataRoundTrip();
        TestBlobSaveLoad();
        TestBlobRemove();
        TestProvidersRoundTrip();
        TestPipelinesRoundTrip();
        TestGetTabFiles();
        TestPipelineMetaRoundTrip();
        std::cout << "=== ALL STORAGE TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
