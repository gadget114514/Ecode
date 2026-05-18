#include "MainWindow.h"
#include "ConfigManager.h"
#include "Resource.h"
#include "Localization.h"
#include <tchar.h>
#include <string>
#include <algorithm>
#include <thread>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

MainWindow::MainWindow() : m_hwnd(NULL)
{
    InitEmbeddedApps();
    AddTab(L""); // Default empty tab
    LoadRecentFiles();
}

MainWindow::~MainWindow()
{
}

MainWindow::DocumentTab& MainWindow::GetActiveTab() {
    if (m_tabs.empty()) {
        static DocumentTab empty;
        return empty; 
    }
    if (m_activeTabIndex >= m_tabs.size()) m_activeTabIndex = 0;
    return m_tabs[m_activeTabIndex];
}

const MainWindow::DocumentTab& MainWindow::GetActiveTab() const {
    if (m_tabs.empty()) {
        static DocumentTab empty;
        return empty;
    }
    auto index = m_activeTabIndex;
    if (index >= m_tabs.size()) index = 0;
    return m_tabs[index];
}

void MainWindow::AddTab(const std::wstring& filePath, bool setActive) {
    DocumentTab tab;
    tab.filePath = filePath;
    tab.title = filePath.empty() ? L"Untitled" : filePath.substr(filePath.find_last_of(L"\\/") + 1);
    
    // Initialize default column width in state?
    // tab.state.SetDefaultColWidth(m_defaultColWidth); // If needed
    
    m_tabs.push_back(tab); // removed std::move for simpler copy, struct is copyable
    
    if (setActive) {
        SetActiveTab(m_tabs.size() - 1);
    }
    if (m_hwnd) InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::CloseTab(size_t index) {
    if (index >= m_tabs.size()) return;

    // Cleanup embedded app if present
    auto& tab = m_tabs[index];
    if (tab.IsEmbedded()) {
        // Destroy child window -> terminates child process
        if (tab.childHwnd) {
            DestroyWindow(tab.childHwnd);
            tab.childHwnd = NULL;
        }
        if (tab.placeholderHwnd) {
            DestroyWindow(tab.placeholderHwnd);
            tab.placeholderHwnd = NULL;
        }
        if (tab.hChildProcess) {
            WaitForSingleObject(tab.hChildProcess, 1000);
            CloseHandle(tab.hChildProcess);
            tab.hChildProcess = NULL;
        }
    }
    
    m_tabs.erase(m_tabs.begin() + index);
    
    if (m_tabs.empty()) {
        AddTab(L""); // Ensure always one tab
    }
    
    if (m_activeTabIndex >= m_tabs.size()) {
        m_activeTabIndex = m_tabs.size() - 1;
    }
    
    if (m_hwnd) {
        SetActiveTab(m_activeTabIndex); // Update UI
    }
}

void MainWindow::SetActiveTab(size_t index) {
    if (index < m_tabs.size()) {
        m_activeTabIndex = index;
        if (m_hwnd) {
            UpdateScrollBars();
            // UpdateFormulaBar(); // TODO: Add this method to update UI from Model
            SetWindowText(m_hwnd, GetActiveTab().filePath.empty() ? L"CSV Editor - Untitled" : (L"CSV Editor - " + GetActiveTab().filePath).c_str());
            InvalidateRect(m_hwnd, NULL, FALSE);
        }
    }
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = MainWindow::WindowProc;
    wcex.hInstance      = hInstance;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName  = _T("CSVEditorWindow");

    RegisterClassEx(&wcex);

    // Load Config
    m_rowHeight = CsvConfigManager::Instance().GetFloat(L"Layout", L"RowHeight", 20.0f);
    // m_defaultColWidth handled in GetColumnWidth or state init
    m_maxCellLines = CsvConfigManager::Instance().GetInt(L"Layout", L"MaxCellLines", 1);

    m_hwnd = CreateWindow(
        _T("CSVEditorWindow"),
        _T("CSV Editor"),
        WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1024, 768,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hwnd) return false;
    
    // Formula Bar
    // Label "A1"
    m_hPosLabel = CreateWindow(
        _T("STATIC"), _T(""),
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 
        0, 0, 40, (int)m_formulaBarHeight,
        m_hwnd, NULL, hInstance, NULL
    );
    SendMessage(m_hPosLabel, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // Edit Field
    m_hFormulaEdit = CreateWindow(
        _T("EDIT"),
        NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        40, 0, 100, (int)m_formulaBarHeight, // x starts after label
        m_hwnd,
        (HMENU)ID_FORMULA_EDIT,
        hInstance,
        NULL
    );
    SendMessage(m_hFormulaEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // Open Button
    m_hOpenBtn = CreateWindow(
        _T("BUTTON"), _T("📂 Open..."),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 70, (int)m_formulaBarHeight,
        m_hwnd, (HMENU)IDM_FILE_OPEN, hInstance, NULL
    );
    SendMessage(m_hOpenBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // Save Button
    m_hSaveBtn = CreateWindow(
        _T("BUTTON"), _T("💾 Save"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 70, (int)m_formulaBarHeight,
        m_hwnd, (HMENU)IDM_FILE_SAVE, hInstance, NULL
    );
    SendMessage(m_hSaveBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // Progress Bar
    m_hProgressBar = CreateWindowEx(
        0, PROGRESS_CLASS, NULL,
        WS_CHILD | PBS_SMOOTH, // Not visible initially
        0, 0, 100, 20,
        m_hwnd, NULL, hInstance, NULL
    );
    SendMessage(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    InitializeMenu(m_hwnd);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    
    if (FAILED(m_dxResources.CreateDeviceIndependentResources())) {
        return false;
    }
    
    m_hCursorArrow = LoadCursor(NULL, IDC_ARROW);
    m_hCursorWE = LoadCursor(NULL, IDC_SIZEWE);

    // Trigger initial layout calculation now that all controls are created
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    OnResize(rc.right - rc.left, rc.bottom - rc.top);

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    return true;
}

bool MainWindow::OpenFile(const std::wstring& filePath)
{
    // Reuse current tab if empty/untitled, otherwise add new
    bool reuse = false;
    DocumentTab& current = GetActiveTab();
    if (current.filePath.empty() && current.document.GetRowCount() <= 0) { // Check <= 0 just safely, usually 0
        reuse = true;
    }
    
    if (!reuse) {
        AddTab(L"", true);
    }
    
    DocumentTab& tab = GetActiveTab();
    if (tab.document.Load(filePath)) {
        tab.filePath = filePath;
        tab.title = filePath.empty() ? L"Untitled" : filePath.substr(filePath.find_last_of(L"\\/") + 1);
        tab.state.SetScrollRow(0);
        
        // m_currentFilePath = filePath; // Deprecated
        SetWindowText(m_hwnd, (L"CSV Editor - " + filePath).c_str());
        UpdateScrollBars();
        InvalidateRect(m_hwnd, NULL, FALSE);

        // Track recent file
        AddToRecentFiles(filePath);

        return true;
    } else {
        if (!reuse) {
            CloseTab(m_activeTabIndex); // Close the failed new tab
        }
    }
    return false;
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* pThis = NULL;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
    pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    } // No change here, just context

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_SIZE:
        OnResize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            OnPaint(hwnd);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_COMMAND:
        OnCommand(hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
        return 0;

    case WM_EMBED_APP:
        OnEmbedApp((HWND)wParam, (size_t)lParam);
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(LOWORD(lParam), HIWORD(lParam), (UINT)wParam);
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp(LOWORD(lParam), HIWORD(lParam), (UINT)wParam);
        return 0;

    case WM_CONTEXTMENU:
        OnContextMenu((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_LBUTTONDBLCLK:
        OnLButtonDblClk(LOWORD(lParam), HIWORD(lParam), (UINT)wParam);
        return 0;
    
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wParam == 'Z') {
                SendMessage(hwnd, WM_COMMAND, IDM_EDIT_UNDO, 0);
            } else if (wParam == 'Y') {
                SendMessage(hwnd, WM_COMMAND, IDM_EDIT_REDO, 0);
            } else if (wParam == 'S') { // Save
                SendMessage(hwnd, WM_COMMAND, IDM_FILE_SAVE, 0);
            } else if (wParam == 'O') { // Open
                SendMessage(hwnd, WM_COMMAND, IDM_FILE_OPEN, 0);
            } else if (wParam == 'F') { // Find
                m_showSearch = true;
                UpdateSearchBar();
            } else if (wParam == 'H') { // Replace
                OnSearchReplace();
            }
        } else {
             // Navigation
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000);
            size_t r = 0, c = 0;
            DocumentTab& tab = GetActiveTab();
            auto sels = tab.state.GetSelections();
            if(!sels.empty()) { r = sels[0].start.row; c = sels[0].start.col; }
            
            bool changed = false;
            if (wParam == VK_UP) {
                if(r > 0) { r--; changed = true; }
            } else if (wParam == VK_DOWN) {
                if(r + 1 < tab.document.GetRowCount()) { r++; changed = true; }
            } else if (wParam == VK_LEFT) {
                if(c > 0) { c--; changed = true; }
            } else if (wParam == VK_RIGHT) {
                c++; changed = true; 
            }
            
            if (changed) {
                if (shift) tab.state.DragTo(r, c);
                else tab.state.SelectCell(r, c, false);
                
                UpdateFormulaBar();
                InvalidateRect(hwnd, NULL, FALSE);
                UpdateScrollBars();
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(LOWORD(lParam), HIWORD(lParam), (UINT)wParam);
        return 0;

    case WM_VSCROLL:
        OnVScroll(wParam);
        return 0;

    case WM_HSCROLL:
        OnHScroll(wParam);
        return 0;

    case WM_MOUSEWHEEL:
        OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        return 0; 

    case WM_ERASEBKGND:
        return 1; // Prevent flickering

    case WM_DESTROY:
        CsvConfigManager::Instance().SetFloat(L"Layout", L"RowHeight", m_rowHeight);
        CsvConfigManager::Instance().SetFloat(L"Layout", L"DefaultColWidth", GetActiveTab().state.GetDefaultColumnWidth());
        CsvConfigManager::Instance().SetInt(L"Layout", L"MaxCellLines", m_maxCellLines);
        CsvConfigManager::Instance().Save(); // Implicit in Set, but good for clarity if we had batching
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MainWindow::InitializeMenu(HWND hwnd)
{
    // Destroy old menu if exists
    HMENU hOldMenu = GetMenu(hwnd);
    if (hOldMenu) DestroyMenu(hOldMenu);

    HMENU hMenu = CreateMenu();
    
    // File Menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_OPEN, CsvLocalization::GetString(CsvStringId::Menu_File_Open));
    
    // Recent Files Submenu
    HMENU hRecentMenu = CreatePopupMenu();
    if (m_recentFiles.empty()) {
        AppendMenu(hRecentMenu, MF_STRING | MF_GRAYED, 0, L"(empty)");
    } else {
        for (int i = 0; i < (int)m_recentFiles.size() && i < MAX_RECENT_FILES; ++i) {
            std::wstring label = std::to_wstring(i + 1) + L". " + m_recentFiles[i];
            // Show only filename part to keep menu short
            size_t sep = m_recentFiles[i].find_last_of(L"\\/");
            std::wstring shortName = (sep != std::wstring::npos) ? m_recentFiles[i].substr(sep + 1) : m_recentFiles[i];
            label = std::to_wstring(i + 1) + L".  " + shortName;
            AppendMenu(hRecentMenu, MF_STRING, IDM_FILE_RECENT_1 + i, label.c_str());
        }
        AppendMenu(hRecentMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hRecentMenu, MF_STRING, IDM_FILE_RECENT_CLEAR,
            CsvLocalization::GetString(CsvStringId::Menu_File_RecentFiles_Clear));
    }
    AppendMenu(hFileMenu, MF_POPUP, (UINT_PTR)hRecentMenu,
        CsvLocalization::GetString(CsvStringId::Menu_File_RecentFiles));

    AppendMenu(hFileMenu, MF_STRING | MF_GRAYED, IDM_FILE_REOPEN, CsvLocalization::GetString(CsvStringId::Menu_File_Reopen));
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_IMPORT, CsvLocalization::GetString(CsvStringId::Menu_File_Import));
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_SAVE, CsvLocalization::GetString(CsvStringId::Menu_File_Save));
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, CsvLocalization::GetString(CsvStringId::Menu_File_SaveAs));
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXPORT_HTML, CsvLocalization::GetString(CsvStringId::Menu_File_Export_HTML));
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXPORT_MD, CsvLocalization::GetString(CsvStringId::Menu_File_Export_MD));
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXIT, CsvLocalization::GetString(CsvStringId::Menu_File_Exit));
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, CsvLocalization::GetString(CsvStringId::Menu_File));

    // Edit Menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_UNDO, CsvLocalization::GetString(CsvStringId::Menu_Edit_Undo));
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_REDO, CsvLocalization::GetString(CsvStringId::Menu_Edit_Redo));
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_CUT, CsvLocalization::GetString(CsvStringId::Menu_Edit_Cut));
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_COPY, CsvLocalization::GetString(CsvStringId::Menu_Edit_Copy));
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_PASTE, CsvLocalization::GetString(CsvStringId::Menu_Edit_Paste));
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_DELETE, CsvLocalization::GetString(CsvStringId::Menu_Edit_Delete));
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_INSERT_ROW, CsvLocalization::GetString(CsvStringId::Menu_Edit_InsertRow));
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_INSERT_COL, CsvLocalization::GetString(CsvStringId::Menu_Edit_InsertCol));
    AppendMenu(hEditMenu, MF_STRING, IDM_EDIT_CLEAR, CsvLocalization::GetString(CsvStringId::Menu_Edit_Clear));

    // Sort Submenu
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, NULL);
    HMENU hSortMenu = CreatePopupMenu();
    AppendMenu(hSortMenu, MF_STRING, IDM_EDIT_SORT_ROWS_ASC,  CsvLocalization::GetString(CsvStringId::Menu_Edit_SortRowsAsc));
    AppendMenu(hSortMenu, MF_STRING, IDM_EDIT_SORT_ROWS_DESC, CsvLocalization::GetString(CsvStringId::Menu_Edit_SortRowsDesc));
    AppendMenu(hSortMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSortMenu, MF_STRING, IDM_EDIT_SORT_COLS_ASC,  CsvLocalization::GetString(CsvStringId::Menu_Edit_SortColsAsc));
    AppendMenu(hSortMenu, MF_STRING, IDM_EDIT_SORT_COLS_DESC, CsvLocalization::GetString(CsvStringId::Menu_Edit_SortColsDesc));
    AppendMenu(hEditMenu, MF_POPUP, (UINT_PTR)hSortMenu, CsvLocalization::GetString(CsvStringId::Menu_Edit_Sort));

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, CsvLocalization::GetString(CsvStringId::Menu_Edit));

    // Search Menu
    HMENU hSearchMenu = CreatePopupMenu();
    AppendMenu(hSearchMenu, MF_STRING, ID_SEARCH_CLOSE, CsvLocalization::GetString(CsvStringId::Menu_Search_Find));
    AppendMenu(hSearchMenu, MF_STRING, ID_SEARCH_NEXT, CsvLocalization::GetString(CsvStringId::Menu_Search_FindNext));
    AppendMenu(hSearchMenu, MF_STRING, ID_SEARCH_PREV, CsvLocalization::GetString(CsvStringId::Menu_Search_FindPrev));
    AppendMenu(hSearchMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hSearchMenu, MF_STRING, ID_SEARCH_REPLACE, CsvLocalization::GetString(CsvStringId::Menu_Search_Replace));
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSearchMenu, CsvLocalization::GetString(CsvStringId::Menu_Search));

    // Config Menu
    HMENU hConfigMenu = CreatePopupMenu();
    
    // Delimiter Submenu
    HMENU hDelimMenu = CreatePopupMenu();
    AppendMenu(hDelimMenu, MF_STRING, IDM_CONFIG_DELIM_COMMA, CsvLocalization::GetString(CsvStringId::Menu_Config_Delim_Comma));
    AppendMenu(hDelimMenu, MF_STRING, IDM_CONFIG_DELIM_TAB, CsvLocalization::GetString(CsvStringId::Menu_Config_Delim_Tab));
    AppendMenu(hDelimMenu, MF_STRING, IDM_CONFIG_DELIM_SEMI, CsvLocalization::GetString(CsvStringId::Menu_Config_Delim_Semi));
    AppendMenu(hDelimMenu, MF_STRING, IDM_CONFIG_DELIM_PIPE, CsvLocalization::GetString(CsvStringId::Menu_Config_Delim_Pipe));
    AppendMenu(hConfigMenu, MF_POPUP, (UINT_PTR)hDelimMenu, CsvLocalization::GetString(CsvStringId::Menu_Config_Delimiter));

    // Encoding Submenu
    HMENU hEncMenu = CreatePopupMenu();
    AppendMenu(hEncMenu, MF_STRING, IDM_CONFIG_ENC_UTF8, CsvLocalization::GetString(CsvStringId::Menu_Config_Enc_UTF8));
    AppendMenu(hEncMenu, MF_STRING, IDM_CONFIG_ENC_ANSI, CsvLocalization::GetString(CsvStringId::Menu_Config_Enc_ANSI));
    AppendMenu(hConfigMenu, MF_POPUP, (UINT_PTR)hEncMenu, CsvLocalization::GetString(CsvStringId::Menu_Config_Encoding));

    // Newline Submenu
    HMENU hNlMenu = CreatePopupMenu();
    AppendMenu(hNlMenu, MF_STRING, IDM_CONFIG_NL_CRLF, CsvLocalization::GetString(CsvStringId::Menu_Config_NL_CRLF));
    AppendMenu(hNlMenu, MF_STRING, IDM_CONFIG_NL_LF, CsvLocalization::GetString(CsvStringId::Menu_Config_NL_LF));
    AppendMenu(hNlMenu, MF_STRING, IDM_CONFIG_NL_LF, CsvLocalization::GetString(CsvStringId::Menu_Config_NL_LF));
    AppendMenu(hConfigMenu, MF_POPUP, (UINT_PTR)hNlMenu, CsvLocalization::GetString(CsvStringId::Menu_Config_Newline));
    
    // Font
    AppendMenu(hConfigMenu, MF_STRING, IDM_CONFIG_FONT, CsvLocalization::GetString(CsvStringId::Menu_Config_Font));
    AppendMenu(hConfigMenu, MF_STRING | (m_maxCellLines > 1 ? MF_CHECKED : 0), IDM_CONFIG_FOLDING, CsvLocalization::GetString(CsvStringId::Menu_Config_Folding));

    // Language Submenu
    HMENU hLangMenu = CreatePopupMenu();
    AppendMenu(hLangMenu, MF_STRING, IDM_CONFIG_LANG_ENGLISH, CsvLocalization::GetString(CsvStringId::Menu_Config_Lang_English));
    AppendMenu(hLangMenu, MF_STRING, IDM_CONFIG_LANG_JAPANESE, CsvLocalization::GetString(CsvStringId::Menu_Config_Lang_Japanese));
    AppendMenu(hConfigMenu, MF_POPUP, (UINT_PTR)hLangMenu, CsvLocalization::GetString(CsvStringId::Menu_Config_Language));

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hConfigMenu, CsvLocalization::GetString(CsvStringId::Menu_Config));

    // Tools Menu (dynamic from registered embedded apps)
    HMENU hToolsMenu = CreatePopupMenu();
    for (size_t i = 0; i < m_embeddedApps.size(); ++i) {
        UINT menuId = (UINT)(IDM_TOOLS_EMBED_BASE + i);
        AppendMenu(hToolsMenu, MF_STRING, menuId, m_embeddedApps[i].name.c_str());
    }
    if (m_embeddedApps.empty()) {
        AppendMenu(hToolsMenu, MF_STRING | MF_GRAYED, IDM_TOOLS_EMBED_BASE, L"(no apps registered)");
    }
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hToolsMenu,
        CsvLocalization::GetString(CsvStringId::Menu_Tools));

    // Help Menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenu(hHelpMenu, MF_STRING, IDM_HELP_View, CsvLocalization::GetString(CsvStringId::Menu_Help_View));
    AppendMenu(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, CsvLocalization::GetString(CsvStringId::Menu_Help_About));
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, CsvLocalization::GetString(CsvStringId::Menu_Help));

    SetMenu(hwnd, hMenu);
    DrawMenuBar(hwnd); // Force redraw
}

void MainWindow::OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id) {
    case ID_FORMULA_EDIT:
        if (codeNotify == EN_CHANGE) {
            if (m_isUpdatingFormula) break;
            
            // Sync Edit -> Cell
            int len = GetWindowTextLength(m_hFormulaEdit);
            std::vector<wchar_t> buffer(len + 1);
            GetWindowText(m_hFormulaEdit, buffer.data(), len + 1);
            std::wstring text = buffer.data();
            
            DocumentTab& tab = GetActiveTab();
            auto selections = tab.state.GetSelections();
            if (!selections.empty()) {
                size_t r = selections[0].start.row;
                size_t c = selections[0].start.col;
                tab.document.UpdateCell(r, c, text);
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }
        break;
        
    case IDM_FILE_OPEN:
        OnFileOpen();
        break;
    case IDM_FILE_IMPORT:
        OnFileImport();
        break;
    case IDM_FILE_EXPORT_HTML:
        OnFileExport(CsvDocument::ExportFormat::HTML);
        break;
    case IDM_FILE_EXPORT_MD:
        OnFileExport(CsvDocument::ExportFormat::Markdown);
        break;
    case IDM_FILE_EXIT:
        OnExit();
        break;
    case IDM_EDIT_COPY:
        OnEditCopy();
        break;
    case IDM_EDIT_PASTE:
        OnEditPaste();
        break;
    case IDM_EDIT_CUT:
        OnEditCut();
        break;
    case IDM_EDIT_INSERT_ROW:
        OnEditInsertRow();
        break;
    case IDM_EDIT_INSERT_COL:
        OnEditInsertColumn();
        break;
        
    case IDM_EDIT_INSERT_ROW_UP:
        {
            WaitCursor wait;
            size_t row = 0;
            DocumentTab& tab = GetActiveTab();
            const auto& selections = tab.state.GetSelections();
            if (!selections.empty()) row = selections[0].start.row;
            tab.document.InsertRow(row, {});
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateScrollBars();
        }
        break;
    case IDM_EDIT_INSERT_ROW_DOWN:
        {
            WaitCursor wait;
            size_t row = 0;
            DocumentTab& tab = GetActiveTab();
            const auto& selections = tab.state.GetSelections();
            if (!selections.empty()) row = selections[0].start.row + 1;
            else row = tab.document.GetRowCount();
            tab.document.InsertRow(row, {});
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateScrollBars();
        }
        break;
    case IDM_EDIT_INSERT_COL_LEFT:
        {
            WaitCursor wait;
            size_t col = 0;
            DocumentTab& tab = GetActiveTab();
            const auto& selections = tab.state.GetSelections();
            if (!selections.empty()) col = selections[0].start.col;
            tab.document.InsertColumn(col, L"");
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateScrollBars();
        }
        break;
    case IDM_EDIT_INSERT_COL_RIGHT:
        {
            WaitCursor wait;
            size_t col = 0;
            DocumentTab& tab = GetActiveTab();
            const auto& selections = tab.state.GetSelections();
            if (!selections.empty()) col = selections[0].start.col + 1;
            else col = 1; 
            if (selections.empty()) col = 0; 
            
            tab.document.InsertColumn(col, L"");
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateScrollBars();
        }
        break;
        
    case IDM_EDIT_UNDO:
        {
            DocumentTab& tab = GetActiveTab();
            if (tab.document.CanUndo()) {
                WaitCursor wait;
                tab.document.Undo();
                UpdateScrollBars();
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }
        break;
    case IDM_EDIT_REDO:
        {
            DocumentTab& tab = GetActiveTab();
            if (tab.document.CanRedo()) {
                WaitCursor wait;
                tab.document.Redo();
                UpdateScrollBars();
                InvalidateRect(m_hwnd, NULL, FALSE);
            }
        }
        break;
        
    case IDM_CONFIG_FONT:
        OnConfigFont();
        break;
        
    case IDM_CONFIG_FOLDING:
        m_maxCellLines = (m_maxCellLines > 1) ? 1 : 5; // Toggle between 1 implementation and 5 (wrapped)
        InitializeMenu(m_hwnd); // Update checkmark
        InvalidateRect(m_hwnd, NULL, FALSE);
        break;

    case ID_SEARCH_CLOSE:
        m_showSearch = false;
        m_showReplace = false;
        UpdateSearchBar();
        break;
    case ID_SEARCH_NEXT:
        OnSearchNext();
        break;
    case ID_SEARCH_PREV:
        OnSearchPrev();
        break;

    case ID_SEARCH_REPLACE:
        OnSearchReplace();
        break;
    case ID_REPLACE_BTN:
        OnReplace();
        break;
    case ID_REPLACE_ALL_BTN:
        OnReplaceAll();
        break;
    case IDM_CONFIG_DELIM_COMMA: { DocumentTab& tab = GetActiveTab(); tab.document.SetDelimiter(L','); tab.document.RebuildRowIndex(); InvalidateRect(m_hwnd, NULL, FALSE); } break;
    case IDM_CONFIG_DELIM_TAB:   { DocumentTab& tab = GetActiveTab(); tab.document.SetDelimiter(L'\t'); tab.document.RebuildRowIndex(); InvalidateRect(m_hwnd, NULL, FALSE); } break;
    case IDM_CONFIG_DELIM_SEMI:  { DocumentTab& tab = GetActiveTab(); tab.document.SetDelimiter(L';'); tab.document.RebuildRowIndex(); InvalidateRect(m_hwnd, NULL, FALSE); } break;
    case IDM_CONFIG_DELIM_PIPE:  { DocumentTab& tab = GetActiveTab(); tab.document.SetDelimiter(L'|'); tab.document.RebuildRowIndex(); InvalidateRect(m_hwnd, NULL, FALSE); } break;

    case IDM_CONFIG_ENC_UTF8:    GetActiveTab().document.SetEncoding(CsvFileEncoding::UTF8); break;
    
    case IDM_CONFIG_NL_CRLF:     /* m_document.SetNewline(L"\r\n"); */ break;
    case IDM_CONFIG_NL_LF:       /* m_document.SetNewline(L"\n"); */ break;

    case IDM_CONFIG_LANG_ENGLISH:
        CsvLocalization::SetLanguage(CsvLanguage::English);
        InitializeMenu(m_hwnd);
        break;
    case IDM_CONFIG_LANG_JAPANESE:
        CsvLocalization::SetLanguage(CsvLanguage::Japanese);
        InitializeMenu(m_hwnd);
        break;

    case IDM_EDIT_DELETE:
        OnEditDelete();
        break;
    case IDM_EDIT_CLEAR:
        OnEditClear();
        break;
    case IDM_FILE_SAVEAS:
        OnFileSaveAs();
        break;
    case IDM_HELP_ABOUT:
        MessageBox(hwnd, CsvLocalization::GetString(CsvStringId::Msg_AppTitle), CsvLocalization::GetString(CsvStringId::Msg_About), MB_OK);
        break;
    case IDM_EDIT_SORT_ROWS_ASC:
        OnSortRowsAsc();
        break;
    case IDM_EDIT_SORT_ROWS_DESC:
        OnSortRowsDesc();
        break;
    case IDM_EDIT_SORT_COLS_ASC:
        OnSortColsAsc();
        break;
    case IDM_EDIT_SORT_COLS_DESC:
        OnSortColsDesc();
        break;
    case IDM_FILE_RECENT_CLEAR:
        m_recentFiles.clear();
        SaveRecentFiles();
        InitializeMenu(m_hwnd);
        break;
    default:
        // Handle dynamic Recent File items
        if (id >= IDM_FILE_RECENT_1 && id < IDM_FILE_RECENT_1 + MAX_RECENT_FILES) {
            int idx = id - IDM_FILE_RECENT_1;
            OnFileOpenRecent(idx);
        }
        // Handle dynamic Embedded App items
        else if (id >= IDM_TOOLS_EMBED_BASE && id < IDM_TOOLS_EMBED_BASE + (int)m_embeddedApps.size()) {
            OnToolsChildApp(id - IDM_TOOLS_EMBED_BASE);
        }
        break;
    // Add other handlers...
    }
}

void MainWindow::OnFileOpen()
{
    OPENFILENAME ofn;
    TCHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    // Construct filter: "CSV Files\0*.csv\0TSV Files\0*.tsv\0All Files\0*.*\0"
    static std::vector<TCHAR> filter; 
    filter.clear();
    auto append = [&](const TCHAR* s) { while(*s) filter.push_back(*s++); filter.push_back('\0'); };
    append(CsvLocalization::GetString(CsvStringId::File_Filter_CSV)); append(_T("*.csv"));
    append(CsvLocalization::GetString(CsvStringId::File_Filter_TSV)); append(_T("*.tsv"));
    append(CsvLocalization::GetString(CsvStringId::File_Filter_All)); append(_T("*.*"));
    filter.push_back('\0');

    ofn.lpstrFilter = filter.data();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        WaitCursor wait;
        if (OpenFile(szFile)) {
            // Update window title or status
            // SetWindowText(m_hwnd, szFile);
        } else {
            MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_OpenFailed), CsvLocalization::GetString(CsvStringId::Msg_Error), MB_ICONERROR);
        }
    }
}

void MainWindow::OnFileSave()
{
    // Use current tab path
    DocumentTab& tab = GetActiveTab();
    if (!tab.filePath.empty()) {
        WaitCursor wait;
        if (tab.document.Save(tab.filePath)) {
            MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_SaveSuccess), CsvLocalization::GetString(CsvStringId::Msg_Info), MB_OK);
        } else {
            MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_SaveFailed), CsvLocalization::GetString(CsvStringId::Msg_Error), MB_ICONERROR);
        }
    } else {
        OnFileSaveAs();
    }
}

void MainWindow::OnFileSaveAs()
{
    OPENFILENAME ofn;
    TCHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    // Construct filter: "CSV Files\0*.csv\0TSV Files\0*.tsv\0All Files\0*.*\0"
    static std::vector<TCHAR> filter; 
    filter.clear();
    auto append = [&](const TCHAR* s) { while(*s) filter.push_back(*s++); filter.push_back('\0'); };
    append(CsvLocalization::GetString(CsvStringId::File_Filter_CSV)); append(_T("*.csv"));
    append(CsvLocalization::GetString(CsvStringId::File_Filter_TSV)); append(_T("*.tsv"));
    append(CsvLocalization::GetString(CsvStringId::File_Filter_All)); append(_T("*.*"));
    filter.push_back('\0');

    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    
    // Default to 'doc' folder if current path is empty
    TCHAR curDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, curDir);
    std::wstring docDir = std::wstring(curDir) + L"\\doc";
    
    if (m_currentFilePath.empty() && GetFileAttributes(docDir.c_str()) != INVALID_FILE_ATTRIBUTES) {
         ofn.lpstrInitialDir = docDir.c_str();
    } else {
         ofn.lpstrInitialDir = NULL;
    }

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn) == TRUE) {
        WaitCursor wait;
        DocumentTab& tab = GetActiveTab();
        if (tab.document.Save(szFile)) {
            tab.filePath = szFile;
            tab.title = tab.filePath.empty() ? L"Untitled" : tab.filePath.substr(tab.filePath.find_last_of(L"\\/") + 1);
            // Update title
            SetWindowText(m_hwnd, (L"CSV Editor - " + tab.filePath).c_str());
        } else {
            MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_SaveFailed), CsvLocalization::GetString(CsvStringId::Msg_Error), MB_ICONERROR);
        }
    }
}

void MainWindow::OnFileExport(CsvDocument::ExportFormat format)
{
    OPENFILENAME ofn;
    TCHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    if (format == CsvDocument::ExportFormat::HTML) {
        ofn.lpstrFilter = _T("HTML Files\0*.html;*.htm\0All Files\0*.*\0");
        ofn.lpstrDefExt = _T("html");
    } else {
        ofn.lpstrFilter = _T("Markdown Files\0*.md;*.markdown\0All Files\0*.*\0");
        ofn.lpstrDefExt = _T("md");
    }

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn) == TRUE) {
        // TCHAR to wstring
        std::wstring path = szFile;
        // Check if we need to call Localization for success/fail messages?
        // Using existing SaveSuccess/Fail msg is probably fine or generic.
        WaitCursor wait;
        if (GetActiveTab().document.Export(path, format)) {
             MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_SaveSuccess), CsvLocalization::GetString(CsvStringId::Msg_Info), MB_OK);
        } else {
             MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_SaveFailed), CsvLocalization::GetString(CsvStringId::Msg_Error), MB_ICONERROR);
        }
    }
}

void MainWindow::OnFileImport()
{
    OPENFILENAME ofn;
    TCHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    
    // Construct filter: "CSV Files\0*.csv\0TSV Files\0*.tsv\0All Files\0*.*\0"
    static std::vector<TCHAR> filter; 
    filter.clear();
    auto append = [&](const TCHAR* s) { while(*s) filter.push_back(*s++); filter.push_back('\0'); };
    append(CsvLocalization::GetString(CsvStringId::File_Filter_CSV)); append(_T("*.csv"));
    append(CsvLocalization::GetString(CsvStringId::File_Filter_TSV)); append(_T("*.tsv"));
    append(CsvLocalization::GetString(CsvStringId::File_Filter_All)); append(_T("*.*"));
    filter.push_back('\0');

    ofn.lpstrFilter = filter.data();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        WaitCursor wait;
        if (GetActiveTab().document.Import(szFile)) {
             // Invalidate entire window to refresh grid
             InvalidateRect(m_hwnd, NULL, TRUE);
             UpdateScrollBars(); 
             MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_Info), CsvLocalization::GetString(CsvStringId::Msg_Info), MB_OK); // "Info" / "Imported" (reuse info string for now)
        } else {
             MessageBox(m_hwnd, CsvLocalization::GetString(CsvStringId::Msg_OpenFailed), CsvLocalization::GetString(CsvStringId::Msg_Error), MB_ICONERROR);
        }
    }
}

// ─── Recent Files ───────────────────────────────────────────────────────────

void MainWindow::LoadRecentFiles()
{
    m_recentFiles.clear();
    for (int i = 1; i <= MAX_RECENT_FILES; ++i) {
        std::wstring key = L"File" + std::to_wstring(i);
        std::wstring path = CsvConfigManager::Instance().GetString(L"RecentFiles", key, L"");
        if (!path.empty()) {
            m_recentFiles.push_back(path);
        }
    }
}

void MainWindow::SaveRecentFiles()
{
    // Clear all slots first
    for (int i = 1; i <= MAX_RECENT_FILES; ++i) {
        std::wstring key = L"File" + std::to_wstring(i);
        CsvConfigManager::Instance().SetString(L"RecentFiles", key, L"");
    }
    // Write current list
    for (int i = 0; i < (int)m_recentFiles.size() && i < MAX_RECENT_FILES; ++i) {
        std::wstring key = L"File" + std::to_wstring(i + 1);
        CsvConfigManager::Instance().SetString(L"RecentFiles", key, m_recentFiles[i]);
    }
}

void MainWindow::AddToRecentFiles(const std::wstring& filePath)
{
    if (filePath.empty()) return;

    // Remove if already exists (move-to-front)
    m_recentFiles.erase(
        std::remove(m_recentFiles.begin(), m_recentFiles.end(), filePath),
        m_recentFiles.end());

    // Insert at front
    m_recentFiles.insert(m_recentFiles.begin(), filePath);

    // Cap at MAX
    if (m_recentFiles.size() > MAX_RECENT_FILES)
        m_recentFiles.resize(MAX_RECENT_FILES);

    SaveRecentFiles();

    // Refresh menu so the new entry appears immediately
    if (m_hwnd) InitializeMenu(m_hwnd);
}

void MainWindow::OnFileOpenRecent(int index)
{
    if (index < 0 || index >= (int)m_recentFiles.size()) return;
    std::wstring path = m_recentFiles[index];

    WaitCursor wait;
    if (!OpenFile(path)) {
        // File might have been deleted/moved
        MessageBox(m_hwnd,
            (L"Could not open: " + path).c_str(),
            CsvLocalization::GetString(CsvStringId::Msg_Error),
            MB_ICONERROR);
        // Remove the stale entry
        m_recentFiles.erase(m_recentFiles.begin() + index);
        SaveRecentFiles();
        InitializeMenu(m_hwnd);
    }
}

// ─── Sort Handlers ──────────────────────────────────────────────────────────

// Helper: determine [startRow, endRow, sortCol] from current selection
static bool GetRowSortRange(const std::vector<CsvSelectionRange>& sels,
                            size_t totalRows, size_t& startRow, size_t& endRow, size_t& sortCol)
{
    if (sels.empty() || totalRows == 0) return false;
    const auto& sel = sels[0];

    if (sel.mode == CsvSelectionMode::Row) {
        startRow = (std::min)(sel.start.row, sel.end.row);
        endRow   = (std::max)(sel.start.row, sel.end.row);
        sortCol  = 0; // Sort by first column by default for row selection
        return true;
    }
    if (sel.mode == CsvSelectionMode::Column) {
        // Sort all rows by the selected column
        startRow = 0;
        endRow   = totalRows > 0 ? totalRows - 1 : 0;
        sortCol  = sel.start.col;
        return true;
    }
    if (sel.mode == CsvSelectionMode::Cell) {
        // Sort all rows by the selected column
        startRow = 0;
        endRow   = totalRows > 0 ? totalRows - 1 : 0;
        sortCol  = sel.start.col;
        return true;
    }
    if (sel.mode == CsvSelectionMode::All) {
        startRow = 0;
        endRow   = totalRows > 0 ? totalRows - 1 : 0;
        sortCol  = 0;
        return true;
    }
    return false;
}

// Helper: determine [startCol, endCol, sortRow] from current selection
static bool GetColSortRange(const std::vector<CsvSelectionRange>& sels,
                            size_t totalCols, size_t& startCol, size_t& endCol, size_t& sortRow)
{
    if (sels.empty() || totalCols == 0) return false;
    const auto& sel = sels[0];

    if (sel.mode == CsvSelectionMode::Column) {
        startCol = (std::min)(sel.start.col, sel.end.col);
        endCol   = (std::max)(sel.start.col, sel.end.col);
        sortRow  = 0;
        return true;
    }
    if (sel.mode == CsvSelectionMode::Row) {
        // Sort all columns by the selected row
        startCol = 0;
        endCol   = totalCols > 0 ? totalCols - 1 : 0;
        sortRow  = sel.start.row;
        return true;
    }
    if (sel.mode == CsvSelectionMode::Cell) {
        startCol = 0;
        endCol   = totalCols > 0 ? totalCols - 1 : 0;
        sortRow  = sel.start.row;
        return true;
    }
    if (sel.mode == CsvSelectionMode::All) {
        startCol = 0;
        endCol   = totalCols > 0 ? totalCols - 1 : 0;
        sortRow  = 0;
        return true;
    }
    return false;
}

void MainWindow::OnSortRowsAsc()
{
    DocumentTab& tab = GetActiveTab();
    size_t startRow = 0, endRow = 0, sortCol = 0;
    if (GetRowSortRange(tab.state.GetSelections(), tab.document.GetRowCount(), startRow, endRow, sortCol)) {
        WaitCursor wait;
        tab.document.SortRowsByColumn(startRow, endRow, sortCol, true);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnSortRowsDesc()
{
    DocumentTab& tab = GetActiveTab();
    size_t startRow = 0, endRow = 0, sortCol = 0;
    if (GetRowSortRange(tab.state.GetSelections(), tab.document.GetRowCount(), startRow, endRow, sortCol)) {
        WaitCursor wait;
        tab.document.SortRowsByColumn(startRow, endRow, sortCol, false);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnSortColsAsc()
{
    DocumentTab& tab = GetActiveTab();
    size_t startCol = 0, endCol = 0, sortRow = 0;
    size_t maxCols = tab.document.GetMaxColumnCount();
    if (GetColSortRange(tab.state.GetSelections(), maxCols, startCol, endCol, sortRow)) {
        WaitCursor wait;
        tab.document.SortColumnsByRow(startCol, endCol, sortRow, true);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnSortColsDesc()
{
    DocumentTab& tab = GetActiveTab();
    size_t startCol = 0, endCol = 0, sortRow = 0;
    size_t maxCols = tab.document.GetMaxColumnCount();
    if (GetColSortRange(tab.state.GetSelections(), maxCols, startCol, endCol, sortRow)) {
        WaitCursor wait;
        tab.document.SortColumnsByRow(startCol, endCol, sortRow, false);
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}


void MainWindow::OnEditDelete()
{
    // Delete selected rows or columns
    DocumentTab& tab = GetActiveTab();
    const auto& selections = tab.state.GetSelections();
    if (selections.empty()) return;

    // Handle multiple selections?
    // For now, take first selection range
    const auto& range = selections[0];
    
    WaitCursor wait;

    if (range.mode == CsvSelectionMode::Row) {
        // Delete rows from range.start.row to range.end.row
        // Delete in reverse order to preserve indices!
        size_t start = (std::min)(range.start.row, range.end.row);
        size_t end = (std::max)(range.start.row, range.end.row);
        
        for (size_t i = end; ; --i) {
            tab.document.DeleteRow(i);
            if (i == start) break;
        }
        tab.state.ClearSelection();
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else if (range.mode == CsvSelectionMode::Column) {
        size_t start = (std::min)(range.start.col, range.end.col);
        size_t end = (std::max)(range.start.col, range.end.col);

        for (size_t i = end; ; --i) {
            tab.document.DeleteColumn(i);
            if (i == start) break;
        }
        tab.state.ClearSelection();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}


void MainWindow::OnEditInsertColumn()
{
    // Insert before current selection
    DocumentTab& tab = GetActiveTab();
    const auto& selections = tab.state.GetSelections();
    size_t col = 0;
    if (!selections.empty()) {
        col = selections[0].start.col;
    }
    
    tab.document.InsertColumn(col, L"");
    
    InvalidateRect(m_hwnd, NULL, FALSE);
    UpdateScrollBars();
}

void MainWindow::OnEditClear()
{
    // Clear content of selected cells
    DocumentTab& tab = GetActiveTab();
    const auto& selections = tab.state.GetSelections();
    if (selections.empty()) return;
    
    const auto& range = selections[0];
    
    size_t startRow = (std::min)(range.start.row, range.end.row);
    size_t endRow = (std::max)(range.start.row, range.end.row);
    size_t startCol = (std::min)(range.start.col, range.end.col);
    size_t endCol = (std::max)(range.start.col, range.end.col);
    
    for (size_t r = startRow; r <= endRow; ++r) {
        for (size_t c = startCol; c <= endCol; ++c) {
            tab.document.UpdateCell(r, c, L"");
        }
    }
    
    UpdateFormulaBar();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnEditCopy()
{
    DocumentTab& tab = GetActiveTab();
    const auto& selections = tab.state.GetSelections();
    if (selections.empty()) return;
    const auto& range = selections[0]; // Multi-selection copy is complex, use primary
    
    // Normalize range
    size_t startRow = (std::min)(range.start.row, range.end.row);
    size_t endRow = (std::max)(range.start.row, range.end.row);
    size_t startCol = (std::min)(range.start.col, range.end.col);
    size_t endCol = (std::max)(range.start.col, range.end.col);

    // If Row selection, handle max col
    if (range.mode == CsvSelectionMode::Row) {
         endCol = 100; // HACK: Should ideally find max cols in rows
    }
    // If Column selection, handle max row
    else if (range.mode == CsvSelectionMode::Column) {
         endRow = tab.document.GetRowCount() > 0 ? tab.document.GetRowCount() - 1 : 0;
    }
    // If All selection
    else if (range.mode == CsvSelectionMode::All) {
         endRow = tab.document.GetRowCount() > 0 ? tab.document.GetRowCount() - 1 : 0;
         endCol = 100; // HACK: Max cols
    }

    std::wstring text = tab.document.GetRangeAsText(startRow, startCol, endRow, endCol);

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(wchar_t));
        if (hGlob) {
            memcpy(GlobalLock(hGlob), text.c_str(), (text.length() + 1) * sizeof(wchar_t));
            GlobalUnlock(hGlob);
            SetClipboardData(CF_UNICODETEXT, hGlob);
        }
        CloseClipboard();
    }
}

void MainWindow::OnEditPaste()
{
    // Basic Paste: Insert text at start of selection
    // Parsing the pasted CSV/TSV data and inserting into table.
    // CsvDocument pastedDoc; // Unused
    
    if (!OpenClipboard(m_hwnd)) return;
    
    // ScopedWaitCursor for potential long parse
    WaitCursor wait;
    
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HGLOBAL hGlob = GetClipboardData(CF_UNICODETEXT);
        if (hGlob) {
            wchar_t* pText = (wchar_t*)GlobalLock(hGlob);
            if (pText) {
                std::wstring pasteData = pText;
                GlobalUnlock(hGlob); // Unlock early as we copied data
                
                DocumentTab& tab = GetActiveTab();
                const auto& selections = tab.state.GetSelections();
                if (!selections.empty()) {
                    size_t r = selections[0].start.row;
                    size_t c = selections[0].start.col;
                    tab.document.PasteCells(r, c, pasteData);
                    InvalidateRect(m_hwnd, NULL, FALSE);
                }
            }
        }
    }
    CloseClipboard();
}

void MainWindow::OnEditCut()
{
    OnEditCopy();
    OnEditClear(); // Or Delete? Excel clears content on cut usually, moving it. Text editors delete.
    // Excel style: "Cut" mode, then move. 
    // Text Editor style: Copy + Delete.
    // Let's go with Clear for cells, Delete for Row mode.
    DocumentTab& tab = GetActiveTab();
    const auto& selections = tab.state.GetSelections();
    if (!selections.empty() && selections[0].mode == CsvSelectionMode::Row) {
         OnEditDelete();
    } else {
         OnEditClear();
    }
}

void MainWindow::OnEditInsertRow()
{
    DocumentTab& tab = GetActiveTab();
    const auto& selections = tab.state.GetSelections();
    size_t insertRow = 0;
    
    if (!selections.empty()) {
        insertRow = selections[0].start.row;
    } else {
        insertRow = tab.document.GetRowCount();
    }
    
    // Insert empty row
    std::vector<std::wstring> emptyValues; // No columns? Or default columns?
    // CsvDocument InsertRow handles empty vector -> just delimiter+newline?
    // Let's insert "" to ensure at least one cell if needed or just newline.
    // Actually InsertRow logic iterates values. If empty, it adds just \n.
    tab.document.InsertRow(insertRow, emptyValues);
    
    InvalidateRect(m_hwnd, NULL, FALSE);
    UpdateScrollBars();
}

void MainWindow::OnExit()
{
    DestroyWindow(m_hwnd);
}

void MainWindow::InitEmbeddedApps()
{
    m_embeddedApps.clear();

    // User can add any number of embeddable apps here.
    // Each entry creates a menu item under Tools and a new tab type.

    m_embeddedApps.push_back({
        L"[ChildApp]",                 // tab title / menu label
        L"ChildApp.exe",              // exe path (relative or absolute)
        L"",                           // working dir (empty = same as parent)
        L""                            // command-line arguments
    });

    // Example: uncomment to add more
    // m_embeddedApps.push_back({ L"Notepad",     L"notepad.exe",     L"", L"" });
    // m_embeddedApps.push_back({ L"Calculator",  L"calc.exe",        L"", L"" });
    // m_embeddedApps.push_back({ L"Paint",       L"mspaint.exe",     L"", L"" });
}

void MainWindow::OnToolsChildApp(int appIndex)
{
    if (appIndex < 0 || appIndex >= (int)m_embeddedApps.size()) return;
    const EmbeddedAppInfo& app = m_embeddedApps[appIndex];

    std::wstring workDir = app.workingDir.empty()
        ? L"."
        : app.workingDir;

    // Create a new tab with embedding enabled
    DocumentTab tab;
    tab.title = app.name;
    tab.isEmbeddedApp = true;
    m_tabs.push_back(tab);
    size_t tabIndex = m_tabs.size() - 1;
    SetActiveTab(tabIndex);

    // Spawn background thread to launch the child process and find its window
    SpawnParams* params = new SpawnParams;
    params->appName = app.name;
    params->exePath = app.exePath;
    params->workingDir = workDir;
    params->cmdArgs = app.cmdArgs;
    params->tabIndex = tabIndex;
    params->parentHwnd = m_hwnd;

    HANDLE hThread = CreateThread(NULL, 0, SpawnChildAppThread, params, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    } else {
        delete params;
        MessageBox(m_hwnd, (app.name + L": failed to launch thread.").c_str(), L"Error", MB_ICONERROR);
    }
}

DWORD WINAPI MainWindow::SpawnChildAppThread(LPVOID lpParam)
{
    SpawnParams* params = static_cast<SpawnParams*>(lpParam);
    std::wstring exePath = params->exePath;
    std::wstring workingDir = params->workingDir;
    size_t tabIndex = params->tabIndex;
    HWND parentHwnd = params->parentHwnd;
    delete params;

    // Launch child process
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    std::wstring cmdLine = L"\"" + exePath + L"\"";
    std::wstring workDir = workingDir;

    BOOL ok = CreateProcessW(
        NULL,
        &cmdLine[0],
        NULL, NULL, FALSE,
        0,
        NULL,
        workDir.empty() ? NULL : workDir.c_str(),
        &si,
        &pi
    );

    if (!ok) {
        PostMessage(parentHwnd, WM_EMBED_APP, (WPARAM)NULL, (LPARAM)tabIndex);
        return 1;
    }

    CloseHandle(pi.hThread);
    DWORD childPid = pi.dwProcessId;

    // Poll for the child's top-level window
    HWND childHwnd = NULL;
    int attempts = 0;
    const int maxAttempts = 200; // ~10 seconds

    struct FindWindowCtx { DWORD pid; HWND* pOut; };
    FindWindowCtx ctx = { childPid, &childHwnd };

    while (childHwnd == NULL && attempts < maxAttempts) {
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* pCtx = reinterpret_cast<FindWindowCtx*>(lParam);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid == pCtx->pid && IsWindowVisible(hwnd)) {
                if (GetWindow(hwnd, GW_OWNER) == NULL) {
                    wchar_t className[256] = {};
                    GetClassNameW(hwnd, className, 256);
                    if (wcscmp(className, L"#32770") != 0 &&
                        wcslen(className) > 0) {
                        *(pCtx->pOut) = hwnd;
                        return FALSE;
                    }
                }
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));

        if (childHwnd == NULL) {
            Sleep(50);
            attempts++;
        }
    }

    if (childHwnd) {
        PostMessage(parentHwnd, WM_EMBED_APP, (WPARAM)childHwnd, (LPARAM)tabIndex);
    } else {
        PostMessage(parentHwnd, WM_EMBED_APP, (WPARAM)NULL, (LPARAM)tabIndex);
    }

    return 0;
}

void MainWindow::OnEmbedApp(HWND childHwnd, size_t tabIndex)
{
    if (tabIndex >= m_tabs.size()) return;
    DocumentTab& tab = m_tabs[tabIndex];

    if (!childHwnd) {
        MessageBox(m_hwnd, L"Failed to launch ChildApp or find its window.", L"[ChildApp] Error", MB_ICONERROR);
        return;
    }

    tab.childHwnd = childHwnd;
    tab.childProcessId = 0;
    GetWindowThreadProcessId(childHwnd, &tab.childProcessId);

    // Open handle for lifetime management
    tab.hChildProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, tab.childProcessId);

    // Reparent and strip borders
    SetParent(childHwnd, m_hwnd);

    LONG style = GetWindowLong(childHwnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
    SetWindowLong(childHwnd, GWL_STYLE, style);

    SetMenu(childHwnd, NULL);

    SetWindowPos(childHwnd, NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Size the child to fill the client area
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int top = (int)(m_tabBarHeight + m_formulaBarHeight);
    MoveWindow(childHwnd, 0, top, rc.right - rc.left, rc.bottom - top, TRUE);

    // Ensure the active tab view is updated
    SetActiveTab(tabIndex);
}

void MainWindow::UpdateScrollBars()
{
    if (!m_hwnd) return;
    
    DocumentTab& tab = GetActiveTab();

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int clientHeight = rc.bottom - rc.top;
    int clientWidth = rc.right - rc.left;

    // Vertical Scrollbar
    size_t rowCount = tab.document.GetRowCount();
    int visibleRows = (int)((clientHeight - m_headerHeight) / m_rowHeight);
    if (visibleRows < 0) visibleRows = 0;

    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (int)rowCount > 0 ? (int)rowCount - 1 : 0;
    si.nPage = visibleRows;
    si.nPos = (int)tab.state.GetScrollRow();
    
    SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);

    // Horizontal Scrollbar
    float totalWidth = GetColumnX(tab.document.GetMaxColumnCount());
    // Ensure at least some width
    if (totalWidth < clientWidth) totalWidth = (float)clientWidth;

    si.nMax = (int)totalWidth;
    si.nPage = clientWidth;
    si.nPos = (int)tab.state.GetScrollX();
    
    SetScrollInfo(m_hwnd, SB_HORZ, &si, TRUE);
}

void MainWindow::OnVScroll(WPARAM wParam)
{
    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hwnd, SB_VERT, &si);

    int newPos = si.nPos;
    int action = LOWORD(wParam);

    switch (action) {
    case SB_TOP: newPos = si.nMin; break;
    case SB_BOTTOM: newPos = si.nMax; break;
    case SB_LINEUP: newPos -= 1; break;
    case SB_LINEDOWN: newPos += 1; break;
    case SB_PAGEUP: newPos -= si.nPage; break;
    case SB_PAGEDOWN: newPos += si.nPage; break;
    case SB_THUMBTRACK: newPos = si.nTrackPos; break;
    }

    if (newPos < si.nMin) newPos = si.nMin;
    if (newPos > (int)(si.nMax - si.nPage + 1)) newPos = (si.nMax - si.nPage + 1);
    if (newPos < 0) newPos = 0;

    GetActiveTab().state.SetScrollRow((size_t)newPos);
    UpdateScrollBars();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnHScroll(WPARAM wParam)
{
    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_ALL;
    GetScrollInfo(m_hwnd, SB_HORZ, &si);

    int newPos = si.nPos;
    int action = LOWORD(wParam);

    switch (action) {
    case SB_LINELEFT: newPos -= 10; break;
    case SB_LINERIGHT: newPos += 10; break;
    case SB_PAGELEFT: newPos -= si.nPage; break;
    case SB_PAGERIGHT: newPos += si.nPage; break;
    case SB_THUMBTRACK: newPos = si.nTrackPos; break;
    }
    
    if (newPos < si.nMin) newPos = si.nMin;
    if (newPos > (int)(si.nMax - si.nPage + 1)) newPos = (si.nMax - si.nPage + 1);
    if (newPos < 0) newPos = 0;

    GetActiveTab().state.SetScrollX((float)newPos);
    UpdateScrollBars();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnMouseWheel(short delta)
{
    int lines = - (delta / WHEEL_DELTA) * 3;
    
    DocumentTab& tab = GetActiveTab();
    int currentRow = (int)tab.state.GetScrollRow();
    int newRow = currentRow + lines;
    if (newRow < 0) newRow = 0;
    
    size_t rowCount = tab.document.GetRowCount();
    if (newRow >= (int)rowCount) newRow = (int)rowCount - 1; 
    
    tab.state.SetScrollRow((size_t)newRow);
    UpdateScrollBars();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

bool MainWindow::HitTest(int x, int y, size_t& outRow, size_t& outCol, bool& outIsRowHeader, bool& outIsColHeader)
{
    float fx = (float)x;
    float fy = (float)y;
    
    // Account for TabBar + FormulaBar
    fy -= (m_tabBarHeight + m_formulaBarHeight);
    
    if (fy < 0) return false; // Clicked in formula/tab or above

    outIsRowHeader = false;
    outIsColHeader = false;
    
    DocumentTab& tab = GetActiveTab();

    // Check Headers
    if (fx < m_headerWidth && fy < m_headerHeight) {
        // Top-left corner (Select all)
        outIsRowHeader = true;
        outIsColHeader = true;
        return true; 
    }

    if (fx < m_headerWidth) {
        outIsRowHeader = true;
        // Calculate row
        float relativeY = fy - m_headerHeight;
        if (relativeY < 0) return false;
        
        outRow = tab.state.GetScrollRow() + (size_t)(relativeY / m_rowHeight);
        return true;
    }

    if (fy < m_headerHeight) {
        outIsColHeader = true;
        // Calculate col
        float relativeX = fx - m_headerWidth + tab.state.GetScrollX(); 
        
        // Find column index based on cumulative width

        outCol = GetColumnAtX(relativeX);
        return true;
    }

    // Grid Area
    float relativeY = fy - m_headerHeight;
    float relativeX = fx - m_headerWidth + tab.state.GetScrollX(); 
    
    outRow = tab.state.GetScrollRow() + (size_t)(relativeY / m_rowHeight);
    outCol = GetColumnAtX(relativeX);
    
    return true;
}

void MainWindow::OnLButtonDown(int x, int y, UINT keyFlags)
{
    // Check Tab Bar Click
    if (y < m_tabBarHeight) {
        int tabIndex = HitTestTabBar(x, y);
        if (tabIndex != -1) {
            if (tabIndex == m_tabs.size()) {
                AddTab(L"", true); // New Tab
            } else {
                SetActiveTab(tabIndex);
            }
        }
        return;
    }

    SetCapture(m_hwnd);
    
    // Check Resize
    float fy = (float)y - (m_formulaBarHeight + m_tabBarHeight);
    bool inHeader = (fy >= 0 && fy < m_headerHeight);
    if (inHeader && x > m_headerWidth) {
        float gridX = (float)x - m_headerWidth;
        float scrollX = GetActiveTab().state.GetScrollX();
        float contentX = gridX + scrollX;
        size_t col = GetColumnAtX(contentX);
        float colRight = GetColumnX(col) + GetColumnWidth(col);
        
        if (abs(contentX - colRight) < 8.0f) {
            m_isResizingCol = true;
            m_resizingColIndex = col;
            m_resizeStartX = (float)x;
            m_resizeStartWidth = GetColumnWidth(col);
            return; // Consume event
        }
    }

    size_t row, col;
    bool isRowHeader, isColHeader;
    
    if (HitTest(x, y, row, col, isRowHeader, isColHeader)) {
        bool ctrl = (keyFlags & MK_CONTROL); // Logical AND for check usually or GetKeyState
        
        DocumentTab& tab = GetActiveTab();

        if (isRowHeader && isColHeader) {
             tab.state.SelectAll();
        } else if (isRowHeader) {
            tab.state.SelectRow(row, ctrl);
        } else if (isColHeader) {
            tab.state.SelectColumn(col, ctrl);
        } else {
            tab.state.SelectCell(row, col, ctrl);
        }
        
        UpdateFormulaBar();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonUp(int x, int y, UINT keyFlags)
{
    if (m_isResizingCol) {
        m_isResizingCol = false;
    }
    ReleaseCapture();
}

void MainWindow::OnMouseMove(int x, int y, UINT keyFlags)
{
    // Adjust y for formula bar
    // Standard Grid Area check for Resize Cursor
    // Only allow resize in header area? Or entire column line? Usually Header + Grid.
    // Let's stick to Header area for triggering, or Header area for visual feedback? 
    // Excel allows resizing by dragging separator in Header. Dragging in grid usually selects.
    // User said "dragging with holding lines dividing cells".
    // "holding lines" -> grabbing lines?
    // Usually standard UI allows resize from Header. 
    // If I allow resize from Grid lines, it conflicts with Selection.
    // I will enable it in Header area (like Excel).
    
    // Check if in Header vertical range
    float fy = (float)y - (m_formulaBarHeight + m_tabBarHeight);
    bool inHeader = (fy >= 0 && fy < m_headerHeight);
    
    if (m_isResizingCol) {
        float delta = (float)x - m_resizeStartX;
        float newW = m_resizeStartWidth + delta;
        if (newW < 20.0f) newW = 20.0f;
        
        GetActiveTab().state.SetColumnWidth(m_resizingColIndex, newW);
        InvalidateRect(m_hwnd, NULL, FALSE);
        UpdateScrollBars(); // Width changed
        return;
    }

    // Check for resize cursor
    if (inHeader && x > m_headerWidth) {
        float gridX = (float)x - m_headerWidth;
        float scrollX = GetActiveTab().state.GetScrollX();
        float contentX = gridX + scrollX;
        
        size_t col = GetColumnAtX(contentX);
        float colX = GetColumnX(col); // start of col
        float colW = GetColumnWidth(col);
        float colRight = colX + colW;
        
        if (abs(contentX - colRight) < 8.0f) { // Tolerance
            SetCursor(m_hCursorWE);
            return; 
        }
    }
    SetCursor(m_hCursorArrow);

    if (keyFlags & MK_LBUTTON) {
        size_t row, col;
        bool isRowHeader, isColHeader;
        
        if (HitTest(x, y, row, col, isRowHeader, isColHeader)) {
            GetActiveTab().state.DragTo(row, col);
            InvalidateRect(m_hwnd, NULL, FALSE);
            UpdateFormulaBar();
        }
    }
}


void MainWindow::OnContextMenu(HWND hwnd, int x, int y)
{
    // If handled by keyboard (x=-1, y=-1), get cursor pos
    if (x == -1 && y == -1) {
        POINT pt;
        GetCursorPos(&pt);
        x = pt.x;
        y = pt.y;
    }

    // Convert to client for HitTest?
    // HitTest expects Client coordinates.
    POINT ptClient = { x, y };
    ScreenToClient(m_hwnd, &ptClient);

    size_t row, col;
    bool isRowHeader, isColHeader;
    
    // Only show if we hit something valid or generally inside the grid/header area
    // HitTest returns true for headers and cells
    if (HitTest(ptClient.x, ptClient.y, row, col, isRowHeader, isColHeader)) {
        
        // If it's a header or cell, we show the menu.
        // We might want to ensure selection is correct? 
        // Standard behavior: Right click selects the item if not already selected.
        // But for multiple selection, right click on *selection* keeps selection. 
        // Right click *outside* selection selects that single item.
        
        bool isSelected = false;
        DocumentTab& tab = GetActiveTab();
        if (isRowHeader) isSelected = tab.state.IsRowSelected(row);
        else if (isColHeader) isSelected = tab.state.IsColumnSelected(col);
        else isSelected = tab.state.IsSelected(row, col);

        if (!isSelected) {
            // Select it (singlular)
            if (isRowHeader) tab.state.SelectRow(row, false);
            else if (isColHeader) tab.state.SelectColumn(col, false);
            else tab.state.SelectCell(row, col, false);
            
            InvalidateRect(m_hwnd, NULL, FALSE);
        }

        HMENU hMenu = CreatePopupMenu();
        AppendMenu(hMenu, MF_STRING, IDM_EDIT_CUT, CsvLocalization::GetString(CsvStringId::Menu_Edit_Cut));
        AppendMenu(hMenu, MF_STRING, IDM_EDIT_COPY, CsvLocalization::GetString(CsvStringId::Menu_Edit_Copy));
        AppendMenu(hMenu, MF_STRING, IDM_EDIT_PASTE, CsvLocalization::GetString(CsvStringId::Menu_Edit_Paste));
        
        // For Delete, context sensitive string? Or just "Delete"
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hMenu, MF_STRING, IDM_EDIT_DELETE, CsvLocalization::GetString(CsvStringId::Menu_Edit_Delete));

        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

        if (isRowHeader || (!isRowHeader && !isColHeader)) {
            AppendMenu(hMenu, MF_STRING, IDM_EDIT_INSERT_ROW_UP, CsvLocalization::GetString(CsvStringId::Menu_Edit_InsertRowUp));
            AppendMenu(hMenu, MF_STRING, IDM_EDIT_INSERT_ROW_DOWN, CsvLocalization::GetString(CsvStringId::Menu_Edit_InsertRowDown));
        }

        if (isColHeader || (!isRowHeader && !isColHeader)) {
            AppendMenu(hMenu, MF_STRING, IDM_EDIT_INSERT_COL_LEFT, CsvLocalization::GetString(CsvStringId::Menu_Edit_InsertColLeft));
            AppendMenu(hMenu, MF_STRING, IDM_EDIT_INSERT_COL_RIGHT, CsvLocalization::GetString(CsvStringId::Menu_Edit_InsertColRight));
        }

        // Sort Submenu
        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
        HMENU hSortCtx = CreatePopupMenu();
        if (isRowHeader || (!isRowHeader && !isColHeader)) {
            AppendMenu(hSortCtx, MF_STRING, IDM_EDIT_SORT_ROWS_ASC,  CsvLocalization::GetString(CsvStringId::Menu_Edit_SortRowsAsc));
            AppendMenu(hSortCtx, MF_STRING, IDM_EDIT_SORT_ROWS_DESC, CsvLocalization::GetString(CsvStringId::Menu_Edit_SortRowsDesc));
        }
        if (isColHeader || (!isRowHeader && !isColHeader)) {
            if (isRowHeader || (!isRowHeader && !isColHeader))
                AppendMenu(hSortCtx, MF_SEPARATOR, 0, NULL);
            AppendMenu(hSortCtx, MF_STRING, IDM_EDIT_SORT_COLS_ASC,  CsvLocalization::GetString(CsvStringId::Menu_Edit_SortColsAsc));
            AppendMenu(hSortCtx, MF_STRING, IDM_EDIT_SORT_COLS_DESC, CsvLocalization::GetString(CsvStringId::Menu_Edit_SortColsDesc));
        }
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSortCtx, CsvLocalization::GetString(CsvStringId::Menu_Edit_Sort));

        TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, m_hwnd, NULL);
        DestroyMenu(hMenu);
    }
}


void MainWindow::OnResize(UINT width, UINT height)
{
    if (m_hPosLabel) {
        MoveWindow(m_hPosLabel, 0, (int)m_tabBarHeight, 40, (int)m_formulaBarHeight, TRUE);
    }
    if (m_hFormulaEdit) {
        MoveWindow(m_hOpenBtn, 40, (int)m_tabBarHeight, 70, (int)m_formulaBarHeight, TRUE);
        if (m_hSaveBtn) {
            MoveWindow(m_hSaveBtn, 110, (int)m_tabBarHeight, 60, (int)m_formulaBarHeight, TRUE);
        }
        MoveWindow(m_hFormulaEdit, 170, (int)m_tabBarHeight, width - 170, (int)m_formulaBarHeight, TRUE);
    }
    
    if (m_hProgressBar) {
        MoveWindow(m_hProgressBar, 0, height - (int)m_progressBarHeight, width, (int)m_progressBarHeight, TRUE);
    }

    // Resize embedded child window in active tab
    for (auto& tab : m_tabs) {
        if (tab.IsEmbedded() && tab.childHwnd) {
            int top = (int)(m_tabBarHeight + m_formulaBarHeight);
            MoveWindow(tab.childHwnd, 0, top, width, height - top, TRUE);
        }
    }

    if (m_dxResources.GetRenderTarget()) {
        // Render target is entire client area.
        m_dxResources.GetRenderTarget()->Resize(D2D1::SizeU(width, height));
    }
    UpdateScrollBars();
}

void MainWindow::OnConfigFont()
{
    CHOOSEFONT cf = {0};
    LOGFONT lf = {0};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;
    
    // Set current? Not tracking current yet.
    
    if (ChooseFont(&cf)) {
        // Point size is in 1/10 pts
        float fontSize = (float)cf.iPointSize / 10.0f; 
        
        // Update Resources
        m_dxResources.UpdateTextFormat(lf.lfFaceName, fontSize);
        
        // Recalc row height based on font?
        // Simple heuristic: font size * 1.5
        // Or measure text.
        // For now, let's just create format. 
        // Row height needs update maybe?
        m_rowHeight = fontSize * 1.8f; 
        if (m_rowHeight < 20.0f) m_rowHeight = 20.0f;
        
        UpdateScrollBars();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

// Subclass procedure for the Edit control to handle Enter/Esc
LRESULT CALLBACK MainWindow::EditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    MainWindow* pThis = (MainWindow*)dwRefData;

    switch (uMsg) {
    case WM_GETDLGCODE:
        // Ensure we receive Enter and Tab
        return DLGC_WANTALLKEYS;
        
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            pThis->HideCellEditor(true); // Save
            return 0;
        }
        else if (wParam == VK_ESCAPE) {
            pThis->HideCellEditor(false); // Cancel
            return 0;
        }
        break;
    
    case WM_KILLFOCUS:
        // If we lose focus, save? Usually yes in Excel.
        // But be careful if lose focus because we are destroying it.
        // Let main window handle it via notification if possible? 
        // Or just trigger save here.
        pThis->HideCellEditor(true);
        break;
        
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, EditSubclassProc, uIdSubclass);
        return DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }
    
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void MainWindow::ShowCellEditor(size_t row, size_t col)
{
    if (m_isEditing) HideCellEditor(true);

    m_editingRow = row;
    m_editingCol = col;
    m_isEditing = true;

    // Calculate Rect
    // Need to account for scroll
    DocumentTab& tab = GetActiveTab();
    size_t scrollRow = tab.state.GetScrollRow();
    
    if (row < scrollRow) return; // Not visible
    
    // Y position needs to account for TabBar + FormulaBar + Header
    float topOffset = m_tabBarHeight + m_formulaBarHeight + m_headerHeight;
    
    float y = topOffset + (row - scrollRow) * m_rowHeight;
    float x = m_headerWidth + GetColumnX(col) - tab.state.GetScrollX(); 
    float w = GetColumnWidth(col);
    
    RECT rc;
    rc.left = (LONG)x;
    rc.top = (LONG)y;
    rc.right = (LONG)(x + w);
    rc.bottom = (LONG)(y + m_rowHeight);
    
    // Get Cell Value
    std::wstring text;
    auto cells = tab.document.GetRowCells(row);
    if (col < cells.size()) text = cells[col];
    
    m_hEdit = CreateWindowEx(
        0, _T("EDIT"), text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_LEFT,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        m_hwnd, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (m_hEdit) {
        SetWindowSubclass(m_hEdit, EditSubclassProc, 0, (DWORD_PTR)this);
        SetFocus(m_hEdit);
        SendMessage(m_hEdit, EM_SETSEL, 0, -1); // Select all text
    }
}

void MainWindow::HideCellEditor(bool save)
{
    if (!m_isEditing || !m_hEdit) return;

    if (save) {
        // Get text
        int len = GetWindowTextLength(m_hEdit);
        std::vector<wchar_t> buffer(len + 1);
        GetWindowText(m_hEdit, buffer.data(), len + 1);
        std::wstring text = buffer.data();
        
        GetActiveTab().document.UpdateCell(m_editingRow, m_editingCol, text);
    }

    m_isEditing = false;
    DestroyWindow(m_hEdit);
    m_hEdit = NULL;
    
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::CreateSearchBar()
{
    if (m_hSearchEdit) return;

    // Create simple controls at top
    int y = (int)m_formulaBarHeight;
    int h = 25;
    
    m_hSearchEdit = CreateWindowEx(0, _T("EDIT"), _T(""), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
        0, y, 200, h, m_hwnd, (HMENU)ID_SEARCH_EDIT, GetModuleHandle(NULL), NULL);
        
    m_hNextBtn = CreateWindowEx(0, _T("BUTTON"), _T("Next"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        205, y, 50, h, m_hwnd, (HMENU)ID_SEARCH_NEXT, GetModuleHandle(NULL), NULL);

    m_hPrevBtn = CreateWindowEx(0, _T("BUTTON"), _T("Prev"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        260, y, 50, h, m_hwnd, (HMENU)ID_SEARCH_PREV, GetModuleHandle(NULL), NULL);

    m_hCloseSearchBtn = CreateWindowEx(0, _T("BUTTON"), _T("X"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        315, y, 25, h, m_hwnd, (HMENU)ID_SEARCH_CLOSE, GetModuleHandle(NULL), NULL);

    // Replace Controls (Row 2)
    int y2 = y + h;
    m_hReplaceEdit = CreateWindowEx(0, _T("EDIT"), _T(""), WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 
        0, y2, 200, h, m_hwnd, (HMENU)ID_REPLACE_EDIT, GetModuleHandle(NULL), NULL);

    m_hReplaceBtn = CreateWindowEx(0, _T("BUTTON"), _T("Replace"), WS_CHILD | BS_PUSHBUTTON,
        205, y2, 60, h, m_hwnd, (HMENU)ID_REPLACE_BTN, GetModuleHandle(NULL), NULL);
        
    m_hReplaceAllBtn = CreateWindowEx(0, _T("BUTTON"), _T("All"), WS_CHILD | BS_PUSHBUTTON,
        270, y2, 40, h, m_hwnd, (HMENU)ID_REPLACE_ALL_BTN, GetModuleHandle(NULL), NULL);
        
    // Subclass Edit for Enter key? (Optional)
}

void MainWindow::UpdateSearchBar()
{
    if (m_showSearch) {
        CreateSearchBar();
        ShowWindow(m_hSearchEdit, SW_SHOW);
        ShowWindow(m_hNextBtn, SW_SHOW);
        ShowWindow(m_hPrevBtn, SW_SHOW);
        ShowWindow(m_hCloseSearchBtn, SW_SHOW);
        
        if (m_showReplace) {
             ShowWindow(m_hReplaceEdit, SW_SHOW);
             ShowWindow(m_hReplaceBtn, SW_SHOW);
             ShowWindow(m_hReplaceAllBtn, SW_SHOW);
        } else {
             ShowWindow(m_hReplaceEdit, SW_HIDE);
             ShowWindow(m_hReplaceBtn, SW_HIDE);
             ShowWindow(m_hReplaceAllBtn, SW_HIDE);
        }

        SetFocus(m_hSearchEdit);
    } else {
        if (m_hSearchEdit) ShowWindow(m_hSearchEdit, SW_HIDE);
        if (m_hNextBtn) ShowWindow(m_hNextBtn, SW_HIDE);
        if (m_hPrevBtn) ShowWindow(m_hPrevBtn, SW_HIDE);
        if (m_hCloseSearchBtn) ShowWindow(m_hCloseSearchBtn, SW_HIDE);
        
        if (m_hReplaceEdit) ShowWindow(m_hReplaceEdit, SW_HIDE);
        if (m_hReplaceBtn) ShowWindow(m_hReplaceBtn, SW_HIDE);
        if (m_hReplaceAllBtn) ShowWindow(m_hReplaceAllBtn, SW_HIDE);
        
        // Restore focus to grid/main
        if (m_hwnd) SetFocus(m_hwnd);
    }
    
    // Adjust layout
    m_headerHeight = 25.0f; // Base
    
    if (m_showSearch) {
        m_headerHeight += 25.0f; // Search row
        if (m_showReplace) m_headerHeight += 25.0f; // Replace row
    }
    
    InvalidateRect(m_hwnd, NULL, TRUE);
}

// Replace Handlers
void MainWindow::OnSearchReplace()
{
    m_showSearch = true;
    m_showReplace = true;
    UpdateSearchBar();
}

void MainWindow::OnReplace()
{
    if (!m_hSearchEdit || !m_hReplaceEdit) return;
    
    int len = GetWindowTextLength(m_hSearchEdit);
    if (len == 0) return;
    std::wstring query; query.resize(len+1);
    GetWindowText(m_hSearchEdit, &query[0], len+1);
    query.resize(len);

    int rLen = GetWindowTextLength(m_hReplaceEdit);
    std::wstring replace; replace.resize(rLen+1);
    GetWindowText(m_hReplaceEdit, &replace[0], rLen+1);
    replace.resize(rLen);

    // Get Selection
    DocumentTab& tab = GetActiveTab();
    auto selections = tab.state.GetSelections();
    size_t r = 0, c = 0;
    if (!selections.empty()) {
        r = selections[0].start.row;
        c = selections[0].start.col;
    }
    
    // Search Options
    CsvDocument::SearchOptions opts;
    opts.forward = true;
    opts.mode = CsvDocument::SearchMode::Contains;
    
    // Try Replace
    bool replaced = false;
    {
        WaitCursor wait;
        replaced = tab.document.Replace(query, replace, r, c, opts);
    }
    
    if (replaced) {
        InvalidateRect(m_hwnd, NULL, FALSE);
        
        // Find Next
        if (tab.document.Search(query, r, c, opts)) {
             tab.state.SelectCell(r, c, false);
             tab.state.SetScrollRow(r > 5 ? r - 5 : 0);
             UpdateScrollBars();
        }
    } else {
        // Maybe try to Find first if not currently on match?
        if (tab.document.Search(query, r, c, opts)) {
             tab.state.SelectCell(r, c, false);
             tab.state.SetScrollRow(r > 5 ? r - 5 : 0);
             UpdateScrollBars();
             // Found it, now user clicks Replace again to replace.
        } else {
             MessageBox(m_hwnd, _T("Not found"), _T("Replace"), MB_OK);
        }
    }
}

void MainWindow::OnReplaceAll()
{
    if (!m_hSearchEdit || !m_hReplaceEdit) return;
    
    int len = GetWindowTextLength(m_hSearchEdit);
    if (len == 0) return;
    std::wstring query; query.resize(len+1);
    GetWindowText(m_hSearchEdit, &query[0], len+1);
    query.resize(len);

    int rLen = GetWindowTextLength(m_hReplaceEdit);
    std::wstring replace; replace.resize(rLen+1);
    GetWindowText(m_hReplaceEdit, &replace[0], rLen+1);
    replace.resize(rLen);

    CsvDocument::SearchOptions opts;
    opts.mode = CsvDocument::SearchMode::Contains;
    
    int count = 0;
    {
        WaitCursor wait;
        count = GetActiveTab().document.ReplaceAll(query, replace, opts);
    }
    
    std::wstring msg = _T("Replaced ") + std::to_wstring(count) + _T(" occurrences.");
    MessageBox(m_hwnd, msg.c_str(), _T("Replace All"), MB_OK);
    
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnSearchNext()
{
    if (!m_hSearchEdit) return;
    int len = GetWindowTextLength(m_hSearchEdit);
    if (len == 0) return;
    std::wstring query;
    query.resize(len+1);
    GetWindowText(m_hSearchEdit, &query[0], len+1);
    query.resize(len);
    
    DocumentTab& tab = GetActiveTab();
    auto selections = tab.state.GetSelections();
    size_t r = 0, c = 0;
    if (!selections.empty()) {
        r = selections[0].start.row;
        c = selections[0].start.col;
    }
    
    CsvDocument::SearchOptions opts;
    opts.forward = true;
    opts.matchCase = false; 
    opts.mode = CsvDocument::SearchMode::Contains;

    bool found = false;
    {
         WaitCursor wait;
         found = tab.document.Search(query, r, c, opts);
    }

    if (found) {
        tab.state.SelectCell(r, c, false);
        // Ensure visible
        tab.state.SetScrollRow(r > 5 ? r - 5 : 0); // Center roughly
        UpdateScrollBars();
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else {
        MessageBox(m_hwnd, _T("Not found"), _T("Search"), MB_OK);
    }
}

void MainWindow::OnSearchPrev()
{
    if (!m_hSearchEdit) return;
    int len = GetWindowTextLength(m_hSearchEdit);
    if (len == 0) return;
    std::wstring query;
    query.resize(len+1);
    GetWindowText(m_hSearchEdit, &query[0], len+1);
    query.resize(len);
    
    DocumentTab& tab = GetActiveTab();
    auto selections = tab.state.GetSelections();
    size_t r = 0, c = 0;
    if (!selections.empty()) {
        r = selections[0].start.row;
        c = selections[0].start.col;
    }
    
    CsvDocument::SearchOptions opts;
    opts.forward = false;
    opts.matchCase = false;
    opts.mode = CsvDocument::SearchMode::Contains;

    bool found = false;
    {
         WaitCursor wait;
         found = tab.document.Search(query, r, c, opts);
    }
    
    if (found) {
        tab.state.SelectCell(r, c, false);
        tab.state.SetScrollRow(r > 5 ? r - 5 : 0);
        UpdateScrollBars();
        InvalidateRect(m_hwnd, NULL, FALSE);
    } else {
         MessageBox(m_hwnd, _T("Not found"), _T("Search"), MB_OK);
    }
}

void MainWindow::OnLButtonDblClk(int x, int y, UINT keyFlags)
{
    size_t row, col;
    bool isRH, isCH;
    if (HitTest(x, y, row, col, isRH, isCH)) {
        if (!isRH && !isCH) {
            ShowCellEditor(row, col);
        }
    }
}

void MainWindow::UpdateFormulaBar()
{
    if (!m_hFormulaEdit) return;
    
    m_isUpdatingFormula = true;
    
    // Get Active Cell
    DocumentTab& tab = GetActiveTab();
    auto selections = tab.state.GetSelections();
    if (selections.empty()) {
        SetWindowText(m_hFormulaEdit, L"");
        if (m_hPosLabel) SetWindowText(m_hPosLabel, L"");
        m_isUpdatingFormula = false;
        return;
    }
    
    // Check mode
    if (selections[0].mode == CsvSelectionMode::Row || selections[0].mode == CsvSelectionMode::Column || selections[0].mode == CsvSelectionMode::All) {
         SetWindowText(m_hFormulaEdit, L"");
         if (m_hPosLabel) SetWindowText(m_hPosLabel, L"");
         m_isUpdatingFormula = false;
         return;
    }
    
    size_t r = selections[0].start.row;
    size_t c = selections[0].start.col;
    
    // Update Label (e.g. A1)
    if (m_hPosLabel) {
        std::wstring label;
        // Col to string: 0->A, ... 26->AA
        size_t tempC = c + 1;
        std::wstring colStr;
        while (tempC > 0) {
            size_t rem = (tempC - 1) % 26;
            colStr += (wchar_t)(L'A' + rem);
            tempC = (tempC - 1) / 26;
        }
        std::reverse(colStr.begin(), colStr.end());
        label = colStr + std::to_wstring(r + 1);
        SetWindowText(m_hPosLabel, label.c_str());
    }
    
    if (r < tab.document.GetRowCount()) {
        auto cells = tab.document.GetRowCells(r);
        if (c < cells.size()) {
            SetWindowText(m_hFormulaEdit, cells[c].c_str());
            m_isUpdatingFormula = false;
            return;
        }
    }
    SetWindowText(m_hFormulaEdit, L"");
    m_isUpdatingFormula = false;
}

void MainWindow::OnPaint(HWND hwnd)
{
    if (GetActiveTab().IsEmbedded())
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC hdc = GetDC(hwnd);
        FillRect(hdc, &rc, (HBRUSH)(COLOR_3DFACE + 1));
        ReleaseDC(hwnd, hdc);
        return;
    }

    HRESULT hr = m_dxResources.CreateDeviceResources(hwnd);
    if (FAILED(hr)) return;

    auto pRT = m_dxResources.GetRenderTarget();
    auto pTF = m_dxResources.GetTextFormat();

    pRT->BeginDraw();
    pRT->Clear(D2D1::ColorF(D2D1::ColorF::LightGray)); // background for tab bar

    CComPtr<ID2D1SolidColorBrush> pBrushBlack;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBrushBlack);
    
    CComPtr<ID2D1SolidColorBrush> pBrushGrid;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray), &pBrushGrid);

    CComPtr<ID2D1SolidColorBrush> pBrushSel;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.6f, 1.0f, 0.3f), &pBrushSel); 
    
    CComPtr<ID2D1SolidColorBrush> pBrushHeader;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::WhiteSmoke), &pBrushHeader);
    
    CComPtr<ID2D1SolidColorBrush> pBrushTabActive;
    pRT->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pBrushTabActive);
    
    CComPtr<ID2D1SolidColorBrush> pBrushTabInactive;
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.9f, 0.9f, 0.9f), &pBrushTabInactive);

    if (pBrushBlack && pBrushGrid && pTF) {
        D2D1_SIZE_F size = pRT->GetSize();
        
        // Draw Tabs
        float tabX = 0;
        float tabW = 120.0f;
        float tabH = m_tabBarHeight; // 25.0f
        
        for (size_t i = 0; i < m_tabs.size(); ++i) {
            D2D1_RECT_F tabRect = D2D1::RectF(tabX, 0, tabX + tabW, tabH);
            
            pRT->FillRectangle(tabRect, (i == m_activeTabIndex) ? pBrushTabActive : pBrushTabInactive);
            pRT->DrawRectangle(tabRect, pBrushGrid);
            
            std::wstring title = m_tabs[i].title;
            D2D1_RECT_F textRect = D2D1::RectF(tabX + 5, 5, tabX + tabW - 5, tabH - 5);
            
            // Allow clipping
            pRT->PushAxisAlignedClip(textRect, D2D1_ANTIALIAS_MODE_ALIASED);
            pRT->DrawText(title.c_str(), (UINT32)title.length(), pTF, textRect, pBrushBlack);
            pRT->PopAxisAlignedClip();
            
            tabX += tabW;
        }
        
        // New Tab Button [+]
        D2D1_RECT_F newTabRect = D2D1::RectF(tabX, 0, tabX + 30, tabH);
        pRT->FillRectangle(newTabRect, pBrushTabInactive);
        pRT->DrawRectangle(newTabRect, pBrushGrid);
        pRT->DrawText(L"+", 1, pTF, D2D1::RectF(tabX + 10, 2, tabX + 25, 20), pBrushBlack);


        // Formula Bar Offset
        float gridY = m_formulaBarHeight + m_tabBarHeight; 
        
        size_t rowCount = GetActiveTab().document.GetRowCount();
        
        size_t visibleRows = (size_t)((size.height - m_headerHeight - gridY) / m_rowHeight) + 2;
        
        float startY = m_headerHeight + gridY;
        
        // Update Scroll Access
        size_t startRow = GetActiveTab().state.GetScrollRow();

        // Draw Row Headers
        // Match valid rows
        bool match = true;
        // For simplicity: Iterate same rows as grid
        
        // Draw Grid and Selection
        auto& activeDoc = GetActiveTab().document;
        const auto& activeState = GetActiveTab().state;
        
        for (size_t i = startRow; i < rowCount && i < startRow + visibleRows; ++i) {
            std::vector<std::wstring> cells = activeDoc.GetRowCells(i);
            float y = startY + (i - startRow) * m_rowHeight;
            
            // Draw Row Header
            D2D1_RECT_F headerRect = D2D1::RectF(0, y, m_headerWidth, y + m_rowHeight);
            pRT->FillRectangle(headerRect, pBrushHeader);
            pRT->DrawRectangle(headerRect, pBrushGrid);
            
            // Row Number
            std::wstring rowNum = std::to_wstring(i + 1);
            pRT->DrawText(rowNum.c_str(), (UINT32)rowNum.length(), pTF, headerRect, pBrushBlack);

            float scrollX = activeState.GetScrollX();
            size_t startCol = GetColumnAtX(scrollX);
            
            float x = m_headerWidth + GetColumnX(startCol) - scrollX;
            
            size_t colCount = cells.size(); 

            for (size_t col = startCol; col < colCount; ++col) {
                const auto& cell = cells[col];
                float colW = GetColumnWidth(col);
                
                D2D1_RECT_F cellRect = D2D1::RectF(x, y, x + colW, y + m_rowHeight);
                
                // Selection
                if (activeState.IsSelected(i, col)) {
                    pRT->FillRectangle(cellRect, pBrushSel);
                }

                // Draw text with Layout for Folding support
                CComPtr<IDWriteTextLayout> pLayout;
                HRESULT hrLayout = m_dxResources.GetWriteFactory()->CreateTextLayout(
                    cell.c_str(),
                    (UINT32)cell.length(),
                    pTF,
                    colW,         // Max Width
                    m_rowHeight,  // Max Height
                    &pLayout
                );

                if (SUCCEEDED(hrLayout)) {
                    if (m_maxCellLines > 1) {
                         pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
                    } else {
                         pLayout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                    }
                    
                    // Clip visuals
                    pRT->PushAxisAlignedClip(cellRect, D2D1_ANTIALIAS_MODE_ALIASED);
                    pRT->DrawTextLayout(D2D1::Point2F(x, y), pLayout, pBrushBlack);
                    pRT->PopAxisAlignedClip();
                }

                // Draw cell border
                pRT->DrawRectangle(cellRect, pBrushGrid);
                
                x += colW;
                if (x > size.width) break;
            }
        }
        
        // Draw Column Headers
        float scrollX = GetActiveTab().state.GetScrollX();
        size_t startCol = GetColumnAtX(scrollX);
        float hx = m_headerWidth + GetColumnX(startCol) - scrollX;
        
        // Estimate max cols to draw
        // We iterate until off-screen
        size_t c = startCol;
        
        while (hx < size.width) {
            float colW = GetColumnWidth(c);
            
            D2D1_RECT_F colRect = D2D1::RectF(hx, gridY, hx + colW, gridY + m_headerHeight);
             pRT->FillRectangle(colRect, pBrushHeader);
             pRT->DrawRectangle(colRect, pBrushGrid);
             
             // A, B, C...
             std::wstring colStr;
             size_t tempC = c + 1;
             while (tempC > 0) {
                 size_t rem = (tempC - 1) % 26;
                 colStr += (wchar_t)(L'A' + rem);
                 tempC = (tempC - 1) / 26;
             }
             std::reverse(colStr.begin(), colStr.end());

             pRT->DrawText(colStr.c_str(), (UINT32)colStr.length(), pTF, colRect, pBrushBlack);
             
             hx += colW;
             c++;
             
             // Safety cap
             if (c > activeDoc.GetMaxColumnCount() + 100) break; 
        }

        // Corner Header
         D2D1_RECT_F cornerRect = D2D1::RectF(0, gridY, m_headerWidth, gridY + m_headerHeight);
         pRT->FillRectangle(cornerRect, pBrushHeader);
         pRT->DrawRectangle(cornerRect, pBrushGrid);

    }

    hr = pRT->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        m_dxResources.DiscardDeviceResources();
    }
}
float MainWindow::GetColumnWidth(size_t col) const
{
    return GetActiveTab().state.GetColumnWidth(col);
}

float MainWindow::GetColumnX(size_t col) const
{
    float x = 0;
    const auto& state = GetActiveTab().state;
    // Optimization: Store cached widths or sum? 
    // For now linear sum is okay for reasonable col counts.
    for (size_t i = 0; i < col; ++i) {
        x += state.GetColumnWidth(i); // This accesses map, acceptable.
    }
    return x;
}

size_t MainWindow::GetColumnAtX(float targetX) const
{
    if (targetX < 0) return 0;
    
    // Scan
    float x = 0;
    size_t c = 0;
    const auto& state = GetActiveTab().state;
    // Safety cap
    size_t maxCols = 1000; // Reasonable cap to prevent inf loops if width 0 (though min is 10)
    
    while (x <= targetX) {
        float w = state.GetColumnWidth(c);
        if (x + w > targetX) return c;
        x += w;
        c++;
        if (c > maxCols) break; 
    }
    return c;
}
int MainWindow::HitTestTabBar(int x, int y)
{
    float fx = (float)x;
    float currentX = 0;
    float tabW = 120.0f; // Must match OnPaint
    
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (fx >= currentX && fx < currentX + tabW) {
            return (int)i;
        }
        currentX += tabW;
    }
    
    // Check New Tab Button
    if (fx >= currentX && fx < currentX + 30.0f) {
        return (int)m_tabs.size(); // Special index for new tab
    }
    
    return -1;
}
