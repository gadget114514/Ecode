// =============================================================================
// TreeViewHelpers.inl
// Implementation of the project explorer tree view.
// Included by main.cpp
// =============================================================================

#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

#if defined(__has_include) && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__has_include) && __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

// Defined in Globals.inl
extern HWND g_treeHwnd;

struct TreeItemData {
    std::wstring path;
    bool isDirectory;
};

// Simple vector to manage memory for TreeItemData
static std::vector<TreeItemData*> g_treeItemDataPtrs;

static void ClearTreeItemData() {
    for (auto ptr : g_treeItemDataPtrs) {
        delete ptr;
    }
    g_treeItemDataPtrs.clear();
}

static HTREEITEM InsertTreeItem(HWND hwndTree, HTREEITEM hParent, const std::wstring& text, const std::wstring& fullPath, bool isDirectory) {
    TVITEMW tvi = {0};
    tvi.mask = TVIF_TEXT | TVIF_PARAM;
    tvi.pszText = (LPWSTR)text.c_str();
    
    TreeItemData* data = new TreeItemData{fullPath, isDirectory};
    g_treeItemDataPtrs.push_back(data);
    tvi.lParam = (LPARAM)data;

    TVINSERTSTRUCTW tvins = {0};
    tvins.item = tvi;
    tvins.hInsertAfter = TVI_LAST;
    tvins.hParent = hParent;

    return TreeView_InsertItem(hwndTree, &tvins);
}

static void PopulateTreeViewRecursive(HWND hwndTree, HTREEITEM hParent, const fs::path& directoryPath) {
    std::vector<fs::path> directories;
    std::vector<fs::path> files;

    try {
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            // Basic filtering (ignore hidden or system files)
            std::wstring filename = entry.path().filename().wstring();
            if (filename.empty() || filename[0] == L'.') continue;

            if (entry.is_directory()) {
                directories.push_back(entry.path());
            } else {
                files.push_back(entry.path());
            }
        }
    } catch (...) {
        return; // Ignore access denied etc.
    }

    // Sort alphabetically
    auto pathCompare = [](const fs::path& a, const fs::path& b) {
        return a.filename().wstring() < b.filename().wstring();
    };
    std::sort(directories.begin(), directories.end(), pathCompare);
    std::sort(files.begin(), files.end(), pathCompare);

    // Insert directories first
    for (const auto& dir : directories) {
        HTREEITEM hItem = InsertTreeItem(hwndTree, hParent, dir.filename().wstring(), dir.wstring(), true);
        PopulateTreeViewRecursive(hwndTree, hItem, dir);
    }
    
    // Insert files
    for (const auto& file : files) {
        InsertTreeItem(hwndTree, hParent, file.filename().wstring(), file.wstring(), false);
    }
}

static void RefreshTreeView(HWND hwndTree, const std::wstring& rootPath) {
    if (!hwndTree) return;
    
    TreeView_DeleteAllItems(hwndTree);
    ClearTreeItemData();

    if (rootPath.empty()) return;
    fs::path root(rootPath);
    HTREEITEM hRoot = InsertTreeItem(hwndTree, TVI_ROOT, root.filename().wstring(), root.wstring(), true);
    PopulateTreeViewRecursive(hwndTree, hRoot, root);
    TreeView_Expand(hwndTree, hRoot, TVE_EXPAND);
}
