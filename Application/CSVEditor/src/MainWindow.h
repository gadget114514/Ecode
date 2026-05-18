#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <map>
#include <vector>
#include <string>
#include "DirectXResources.h"
#include "CsvDocument.h"
#include "EditorState.h"
#include "Resource.h"

// Helper for wait cursor
struct WaitCursor {
    HCURSOR hOld;
    WaitCursor() { hOld = SetCursor(LoadCursor(NULL, IDC_WAIT)); }
    ~WaitCursor() { SetCursor(hOld); }
};

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInstance, int nCmdShow);
    bool OpenFile(const std::wstring& filePath);

protected:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnPaint(HWND hwnd);
    void OnResize(UINT width, UINT height);
    void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);

    void InitializeMenu(HWND hwnd);
    void OnFileOpen();
    void OnFileImport();
    void OnFileOpenRecent(int index);
    void OnFileSave();
    void OnFileSaveAs();
    void OnFileExport(CsvDocument::ExportFormat format);
    void OnExit();

    void OnEditDelete();
    void OnEditClear();
    void OnEditCopy();
    void OnEditPaste();
    void OnEditCut();
    void OnEditInsertRow();
    void OnEditInsertColumn();
    void OnSortRowsAsc();
    void OnSortRowsDesc();
    void OnSortColsAsc();
    void OnSortColsDesc();
    
    void OnConfigFont();

    void OnToolsChildApp(int appIndex);
    void OnEmbedApp(HWND childHwnd, size_t tabIndex);
    static DWORD WINAPI SpawnChildAppThread(LPVOID lpParam);

    void OnLButtonDown(int x, int y, UINT keyFlags);
    void OnLButtonUp(int x, int y, UINT keyFlags);
    void OnLButtonDblClk(int x, int y, UINT keyFlags);
    void OnContextMenu(HWND hwnd, int x, int y);
    void OnMouseMove(int x, int y, UINT keyFlags);
    void OnMouseWheel(short delta);
    void OnVScroll(WPARAM wParam);
    void OnHScroll(WPARAM wParam);

private:
    // Helpers to hit test
    bool HitTest(int x, int y, size_t& outRow, size_t& outCol, bool& outIsRowHeader, bool& outIsColHeader);

    // Tab Management
    struct DocumentTab {
        CsvDocument document;
        CsvEditorState state;
        std::wstring filePath; // Empty if new
        std::wstring title;    // Filename or "Untitled"

        // Embedded app support (optional, mutually exclusive with document)
        bool isEmbeddedApp = false;
        HANDLE hChildProcess = NULL;
        DWORD childProcessId = 0;
        HWND childHwnd = NULL;
        HWND placeholderHwnd = NULL;

        bool IsEmbedded() const { return isEmbeddedApp; }
    };
    
    std::vector<DocumentTab> m_tabs;
    size_t m_activeTabIndex = 0;
    
    DocumentTab& GetActiveTab();
    const DocumentTab& GetActiveTab() const;
    void AddTab(const std::wstring& filePath, bool setActive = true);
    void CloseTab(size_t index);
    void SetActiveTab(size_t index);
    
    // Registered embeddable applications
    struct EmbeddedAppInfo {
        std::wstring name;
        std::wstring exePath;
        std::wstring workingDir;
        std::wstring cmdArgs;
    };
    std::vector<EmbeddedAppInfo> m_embeddedApps;
    void InitEmbeddedApps();

    // Embedded app thread parameter
    struct SpawnParams {
        std::wstring appName;
        std::wstring exePath;
        std::wstring workingDir;
        std::wstring cmdArgs;
        size_t tabIndex;
        HWND parentHwnd;
    };

    // UI Layout - Tab Bar
    float m_tabBarHeight = 25.0f;
    void DrawTabBar(HDC hdc, RECT clientRect);
    int HitTestTabBar(int x, int y); // Returns tab index or -1

private:
    void UpdateScrollBars();
    std::wstring m_currentFilePath; // Deprecated, use GetActiveTab().filePath
    HWND m_hwnd;
    DirectXResources m_dxResources;
    // CsvDocument m_document; // Moved to tabs
    // CsvEditorState m_state;    // Moved to tabs

    // Recent Files
    std::vector<std::wstring> m_recentFiles;
    void LoadRecentFiles();
    void SaveRecentFiles();
    void AddToRecentFiles(const std::wstring& filePath);
    void UpdateRecentFilesMenu();

    // UI Layout
    float m_rowHeight = 20.0f;
    // float m_colWidth = 100.0f; // Unused?
    // float m_headerWidth = 50.0f; // Row headers
    float m_headerHeight = 25.0f; // Col headers
    float m_headerWidth = 50.0f; 

    // Editors
    HWND m_hEdit = NULL;
    bool m_isEditing = false;
    size_t m_editingRow = 0;
    size_t m_editingCol = 0;

    void ShowCellEditor(size_t row, size_t col);
    void HideCellEditor(bool save); // false = cancel
    
    // Search UI
    bool m_showSearch = false;
    HWND m_hSearchEdit = NULL;
    HWND m_hNextBtn = NULL;
    HWND m_hPrevBtn = NULL;
    HWND m_hCloseSearchBtn = NULL;
    
    // Formula Bar
    HWND m_hFormulaEdit = NULL;
    HWND m_hPosLabel = NULL; // "A1" Label
    HWND m_hOpenBtn = NULL;
    HWND m_hSaveBtn = NULL;
    float m_formulaBarHeight = 25.0f;
    bool m_isUpdatingFormula = false;
    void UpdateFormulaBar();
    
    // Progress Bar
    HWND m_hProgressBar = NULL;
    float m_progressBarHeight = 20.0f;
    
    void UpdateSearchBar();
    void CreateSearchBar();
    void OnSearchNext();
    void OnSearchPrev();
    
    // Replace UI
    bool m_showReplace = false;
    HWND m_hReplaceEdit = NULL;
    HWND m_hReplaceBtn = NULL;
    HWND m_hReplaceAllBtn = NULL;
    
    void OnSearchReplace(); // Menu handler
    void OnReplace();       // Button handler
    void OnReplaceAll();    // Button handler
    
    // Resizing
    bool m_isResizingCol = false;
    size_t m_resizingColIndex = 0;
    float m_resizeStartX = 0.0f;
    float m_resizeStartWidth = 0.0f;
    
    // std::map<size_t, float> m_colWidths; // Moved to EditorState
    // float m_defaultColWidth = 100.0f;    // Moved to EditorState
    int m_maxCellLines = 1; // Folding value (1 = no wrap)
    
    float GetColumnWidth(size_t col) const;
    float GetColumnX(size_t col) const; // Absolute X position
    size_t GetColumnAtX(float x) const;
    
    // Cursor
    HCURSOR m_hCursorArrow = NULL;
    HCURSOR m_hCursorWE = NULL; // Resize
    
};
