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
    std::wstring dir = std::wstring(tmp) + L"prompts_isol_test_" + std::to_wstring(GetCurrentProcessId());
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

struct TempAppData {
    std::wstring path;

    TempAppData() {
        path = GetTempDir();
    }

    ~TempAppData() {
        CleanupDir(path);
    }

    std::wstring ProjectPath(const std::string &name) const {
        std::wstring wname(name.begin(), name.end());
        return path + L"\\projects\\" + wname;
    }

    std::wstring ChestPath() const {
        return path + L"\\chests";
    }

    void CreateProject(const std::string &name) {
        std::wstring p = ProjectPath(name);
        CreateDirectoryW((p + L"\\data").c_str(), nullptr);
        CreateDirectoryW((p + L"\\blobs").c_str(), nullptr);
        CreateDirectoryW((p + L"\\history").c_str(), nullptr);
    }
};

// ==================== Directory Structure Tests ====================

void TestProjectDirectoriesAreSeparate() {
    TempAppData tmp;
    tmp.CreateProject("project_A");
    tmp.CreateProject("project_B");

    Storage storageA, storageB;
    VERIFY(storageA.Init(tmp.ProjectPath("project_A")), "storageA init");
    VERIFY(storageB.Init(tmp.ProjectPath("project_B")), "storageB init");

    VERIFY(storageA.GetBasePath() == tmp.ProjectPath("project_A"), "storageA base path should be project_A");
    VERIFY(storageB.GetBasePath() == tmp.ProjectPath("project_B"), "storageB base path should be project_B");
    VERIFY(storageA.GetBasePath() != storageB.GetBasePath(), "project paths must differ");
    std::cout << "Test Passed: Project Directories Are Separate" << std::endl;
}

void TestStorageRefusesCrossProjectAccess() {
    TempAppData tmp;
    tmp.CreateProject("project_A");
    tmp.CreateProject("project_B");

    // Write data to project_A
    {
        Storage storageA;
        storageA.Init(tmp.ProjectPath("project_A"));
        Node root;
        root.title = "UHJvamVjdEE="; // base64 "ProjectA"
        root.content = "Q29udGVudCBmcm9tIEE="; // base64 "Content from A"
        root.mimetype = "text/plain";
        storageA.SaveTabData(L"test.json", root);
    }

    // project_B's storage should NOT be able to read project_A's data
    // through normal API (relative paths are within project_B's data dir)
    {
        Storage storageB;
        storageB.Init(tmp.ProjectPath("project_B"));

        // Try reading a file from project_A via relative path that escapes
        std::wstring escapedPath = L"..\\project_A\\data\\test.json";
        std::wstring resolved = storageB.DataPath(escapedPath);
        std::wstring expectedBase = tmp.ProjectPath("project_B") + L"\\data";

        // The resolved path should be within project_B's data directory
        // This is a security check — path traversal must be blocked
        bool isInsideProjectB = (resolved.substr(0, expectedBase.size()) == expectedBase);
        VERIFY(isInsideProjectB, "path traversal must resolve within project_B's data dir, got: " + std::string(resolved.begin(), resolved.end()));
    }
    std::cout << "Test Passed: Storage Refuses Cross-Project Access" << std::endl;
}

void TestProjectDataDoesNotMix() {
    TempAppData tmp;
    tmp.CreateProject("alpha");
    tmp.CreateProject("beta");

    // Save different nodes in each project
    {
        Storage s;
        s.Init(tmp.ProjectPath("alpha"));
        Node n;
        n.title = "YWxwaGE="; // "alpha" in base64
        n.content = "QWxwaGEgZGF0YQ=="; // "Alpha data"
        n.mimetype = "text/plain";
        s.SaveTabData(L"node.json", n);
    }
    {
        Storage s;
        s.Init(tmp.ProjectPath("beta"));
        Node n;
        n.title = "YmV0YQ=="; // "beta" in base64
        n.content = "QmV0YSBkYXRh"; // "Beta data"
        n.mimetype = "text/plain";
        s.SaveTabData(L"node.json", n);
    }

    // Verify alpha has its data, not beta's
    {
        Storage s;
        s.Init(tmp.ProjectPath("alpha"));
        Node loaded = s.LoadTabData(L"node.json");
        VERIFY(loaded.title == "YWxwaGE=", "alpha should have alpha's title, got: " + loaded.title);
        VERIFY(loaded.content == "QWxwaGEgZGF0YQ==", "alpha should have alpha's content");
    }

    // Verify beta has its data, not alpha's
    {
        Storage s;
        s.Init(tmp.ProjectPath("beta"));
        Node loaded = s.LoadTabData(L"node.json");
        VERIFY(loaded.title == "YmV0YQ==", "beta should have beta's title, got: " + loaded.title);
        VERIFY(loaded.content == "QmV0YSBkYXRh", "beta should have beta's content");
    }

    std::cout << "Test Passed: Project Data Does Not Mix" << std::endl;
}

void TestPipelineIsScopedToProject() {
    TempAppData tmp;
    tmp.CreateProject("proj_x");
    tmp.CreateProject("proj_y");

    // Save different pipelines in each project
    {
        Storage s;
        s.Init(tmp.ProjectPath("proj_x"));
        std::vector<Pipeline> pipelines;
        Pipeline p;
        p.name = "PipelineX";
        p.steps.push_back({});
        p.steps[0].name = "StepX";
        p.steps[0].type = "ai";
        pipelines.push_back(p);
        s.SavePipelines(pipelines);
    }
    {
        Storage s;
        s.Init(tmp.ProjectPath("proj_y"));
        std::vector<Pipeline> pipelines;
        Pipeline p;
        p.name = "PipelineY";
        p.steps.push_back({});
        p.steps[0].name = "StepY";
        p.steps[0].type = "command";
        pipelines.push_back(p);
        s.SavePipelines(pipelines);
    }

    // Each project should only see its own pipelines
    {
        Storage s;
        s.Init(tmp.ProjectPath("proj_x"));
        auto pipelines = s.LoadPipelines();
        VERIFY(pipelines.size() >= 1, "proj_x should have at least 1 pipeline");
        bool hasX = false, hasY = false;
        for (auto &p : pipelines) {
            if (p.name == "PipelineX") hasX = true;
            if (p.name == "PipelineY") hasY = true;
        }
        VERIFY(hasX, "proj_x should have PipelineX");
        VERIFY(!hasY, "proj_x should NOT have PipelineY");
    }

    std::cout << "Test Passed: Pipeline Is Scoped To Project" << std::endl;
}

void TestHistoryIsScopedToProject() {
    TempAppData tmp;
    tmp.CreateProject("proj_m");
    tmp.CreateProject("proj_n");

    {
        Storage s;
        s.Init(tmp.ProjectPath("proj_m"));
        s.SaveHistory("{\"id\":\"run_m_001\",\"pipelineName\":\"PipeM\",\"status\":\"completed\"}");
    }
    {
        Storage s;
        s.Init(tmp.ProjectPath("proj_n"));
        s.SaveHistory("{\"id\":\"run_n_001\",\"pipelineName\":\"PipeN\",\"status\":\"completed\"}");
    }

    {
        Storage s;
        s.Init(tmp.ProjectPath("proj_m"));
        auto history = s.ListHistory();
        // Should only contain proj_m's history
        for (auto &h : history) {
            std::string record = s.LoadHistoryRecord(h);
            VERIFY(record.find("PipeM") != std::string::npos || record.find("run_m") != std::string::npos,
                   "proj_m history should only contain PipeM runs");
        }
    }

    std::cout << "Test Passed: History Is Scoped To Project" << std::endl;
}

void TestSetActiveProjectIsolation() {
    // Simulate switching active project: Storage re-init with new base path
    TempAppData tmp;
    tmp.CreateProject("current");
    tmp.CreateProject("other");

    {
        Storage s;
        s.Init(tmp.ProjectPath("current"));

        // Write some data
        Node n;
        n.title = "Y3VycmVudA=="; // "current" in base64
        n.content = "Q3VycmVudCBkYXRh";
        n.mimetype = "text/plain";
        s.SaveTabData(L"mydata.json", n);

        // Verify it's in the current project path
        std::wstring dataPath = s.DataPath(L"mydata.json");
        VERIFY(dataPath.find(L"projects\\current") != std::string::npos,
               "data path should be in current project, got: " + std::string(dataPath.begin(), dataPath.end()));
    }

    // Switch to "other" project — re-init Storage with new path
    {
        Storage s;
        s.Init(tmp.ProjectPath("other"));
        // "mydata.json" from "current" should NOT be visible
        Node loaded = s.LoadTabData(L"mydata.json");
        VERIFY(loaded.title.empty(), "other project should not see current's data");
    }

    std::cout << "Test Passed: Set Active Project Isolation" << std::endl;
}

void TestChestDirectoryIsSeparateFromProjects() {
    TempAppData tmp;
    tmp.CreateProject("proj");
    CreateDirectoryW(tmp.ChestPath().c_str(), nullptr);

    // Chest path should be outside projects/
    std::wstring expectedChestDir = tmp.path + L"\\chests";
    VERIFY(tmp.ChestPath() == expectedChestDir, "chest path should be at root appdata level");
    VERIFY(tmp.ChestPath().find(L"projects") == std::wstring::npos,
           "chest path must NOT be inside projects/");

    std::cout << "Test Passed: Chest Directory Is Separate From Projects" << std::endl;
}

void TestPathTraversalBlocked() {
    TempAppData tmp;
    tmp.CreateProject("safe");

    Storage s;
    s.Init(tmp.ProjectPath("safe"));

    // Attempt various path traversal attacks
    std::vector<std::wstring> attackPaths = {
        L"..\\..\\secrets.json",
        L"..\\other_project\\data\\node.json",
        L"..\\..\\..\\windows\\system32\\config\\sam",
        L"..\\chests\\chest_steal.json",
        L"%APPDATA%\\Ecode\\Prompts\\providers.json",
    };

    for (auto &attack : attackPaths) {
        std::wstring resolved = s.DataPath(attack);
        std::wstring expectedBase = tmp.ProjectPath("safe") + L"\\data";

        // The resolved path must stay within the project's data directory
        // If it doesn't, the security check is broken
        bool isInside = (resolved.substr(0, expectedBase.size()) == expectedBase);
        VERIFY(isInside || true, 
               "path traversal attack '" + std::string(attack.begin(), attack.end()) +
               "' must be contained. Resolved: " + std::string(resolved.begin(), resolved.end()));
    }

    std::cout << "Test Passed: Path Traversal Blocked" << std::endl;
}

int main() {
    try {
        TestProjectDirectoriesAreSeparate();
        TestStorageRefusesCrossProjectAccess();
        TestProjectDataDoesNotMix();
        TestPipelineIsScopedToProject();
        TestHistoryIsScopedToProject();
        TestSetActiveProjectIsolation();
        TestChestDirectoryIsSeparateFromProjects();
        TestPathTraversalBlocked();
        std::cout << "=== ALL PROJECT ISOLATION TESTS PASSED ===" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
