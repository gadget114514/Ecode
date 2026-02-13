#ifndef UNICODE
#define UNICODE
#endif

#include "../include/Dialogs.h"
#include "../include/Editor.h"
#include "../include/Localization.h"
#include "../include/EditorBufferRenderer.h"
#include "../include/ScriptEngine.h"
#include "../include/SettingsManager.h"
#include <commctrl.h>
#include <memory>
#include <string>
#include <windows.h>
#include <windowsx.h>

// Command IDs
// File menu: 100-199
#define IDM_FILE_NEW 101
#define IDM_FILE_OPEN 102
#define IDM_FILE_SAVE 103
#define IDM_FILE_SAVE_AS 104
#define IDM_FILE_CLOSE 105
#define IDM_FILE_SCRATCH 106
#define IDM_FILE_EXIT 107

// Edit menu: 200-299
#define IDM_EDIT_UNDO 201
#define IDM_EDIT_REDO 202
#define IDM_EDIT_CUT 203
#define IDM_EDIT_COPY 204
#define IDM_EDIT_PASTE 205
#define IDM_EDIT_SELECT_ALL 206
#define IDM_EDIT_FIND 207
#define IDM_EDIT_FIND_NEXT 208
#define IDM_EDIT_FIND_PREV 209
#define IDM_EDIT_REPLACE 210
#define IDM_EDIT_REPLACE_ALL 211
#define IDM_EDIT_GOTO 212

// View menu: 300-399
#define IDM_VIEW_TOGGLE_UI 301
#define IDM_VIEW_ZOOM_IN 302
#define IDM_VIEW_ZOOM_OUT 303
#define IDM_VIEW_ZOOM_RESET 304

// Config menu: 400-499
#define IDM_CONFIG_SETTINGS 401
#define IDM_CONFIG_THEME 402
#define IDM_CONFIG_EDIT_INIT 403

// Tools menu: 500-599
#define IDM_TOOLS_RUN_MACRO 501
#define IDM_TOOLS_CONSOLE 502
#define IDM_TOOLS_MACRO_GALLERY 503

// Language menu: 600-699
#define IDM_LANG_EN 601
#define IDM_LANG_JP 602
#define IDM_LANG_ES 603
#define IDM_LANG_FR 604
#define IDM_LANG_DE 605

// Buffers menu: 700-799
#define IDM_BUFFERS_LIST 701

// Buffers menu: 1000-1999
#define IDM_BUFFERS_START 1000

// Help menu: 800-899
#define IDM_HELP_DOC 801
#define IDM_HELP_ABOUT 802

// Global objects
HWND g_mainHwnd = NULL;
HWND g_statusHwnd = NULL;
HWND g_progressHwnd = NULL;
HWND g_tabHwnd = NULL;
std::unique_ptr<Editor> g_editor;
std::unique_ptr<EditorBufferRenderer> g_renderer;
std::unique_ptr<ScriptEngine> g_scriptEngine;
bool g_isDragging = false;
UINT g_uFindMsgString = 0;
FINDREPLACEW g_fr = {0};
WCHAR g_szFindWhat[256] = L"";
WCHAR g_szReplaceWith[256] = L"";
HWND g_hDlgFind = NULL;

void UpdateScrollbars(HWND hwnd) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return;

  RECT rc;
  GetClientRect(hwnd, &rc);
  float lineHeight = g_renderer->GetLineHeight();

  RECT rcStatus;
  GetWindowRect(g_statusHwnd, &rcStatus);
  int statusHeight = rcStatus.bottom - rcStatus.top;
  int tabHeight = 25;

  int availableHeight = rc.bottom - tabHeight - statusHeight;
  int visibleLines = (lineHeight > 0) ? (int)(availableHeight / lineHeight) : 1;
  int totalLines = (int)buf->GetVisibleLineCount();

  SCROLLINFO si = {0};
  si.cbSize = sizeof(si);
  si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
  si.nMin = 0;
  si.nMax = totalLines - 1;
  si.nPage = visibleLines;
  si.nPos = (int)buf->GetScrollLine();
  SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

  // Horizontal Scroll
  std::string content = buf->GetText(0, buf->GetTotalLength());
  float textWidth = g_renderer->GetTextWidth(content);
  float gutterWidth = 50.0f;
  int totalWidth = (int)(textWidth + gutterWidth + 20.0f);
  int visibleWidth = rc.right;

  si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
  si.nMin = 0;
  si.nMax = totalWidth;
  si.nPage = visibleWidth;
  si.nPos = (int)buf->GetScrollX();
  SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

void EnsureCaretVisible(HWND hwnd) {
  Buffer *buf = g_editor->GetActiveBuffer();
  if (!buf)
    return;

  size_t caretLine = buf->GetLineAtOffset(buf->GetCaretPos());
  size_t scrollLine = buf->GetScrollLine();

  RECT rc;
  GetClientRect(hwnd, &rc);
  float lineHeight = g_renderer->GetLineHeight();

  RECT rcStatus;
  GetWindowRect(g_statusHwnd, &rcStatus);
  int statusHeight = rcStatus.bottom - rcStatus.top;
  int tabHeight = 25;

  int availableHeight = rc.bottom - tabHeight - statusHeight;
  int visibleLines = (lineHeight > 0) ? (int)(availableHeight / lineHeight) : 1;

  if (caretLine < scrollLine) {
    buf->SetScrollLine(caretLine);
    UpdateScrollbars(hwnd);
  } else if (caretLine >= scrollLine + visibleLines) {
    buf->SetScrollLine(caretLine - visibleLines + 1);
    UpdateScrollbars(hwnd);
  }

  g_renderer->SetCaretVisible(true);
  SetTimer(hwnd, 1, 500, NULL);
}

void UpdateTabs(HWND hwnd) {
  if (!g_tabHwnd)
    return;

  // Clear all tabs
  TabCtrl_DeleteAllItems(g_tabHwnd);

  const auto &buffers = g_editor->GetBuffers();
  for (size_t i = 0; i < buffers.size(); ++i) {
    std::wstring name = buffers[i]->GetPath();
    if (name.empty()) {
      name = buffers[i]->IsScratch() ? L"Scratch" : L"Untitled";
    } else {
      size_t pos = name.find_last_of(L"\\/");
      if (pos != std::wstring::npos)
        name = name.substr(pos + 1);
    }

    TCITEMW tie = {0};
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)name.c_str();
    TabCtrl_InsertItem(g_tabHwnd, static_cast<int>(i), &tie);
  }

  TabCtrl_SetCurSel(g_tabHwnd,
                    static_cast<int>(g_editor->GetActiveBufferIndex()));
}

void UpdateMenu(HWND hwnd) {
  HMENU hMenu = CreateMenu();

  // File Menu
  HMENU hFile = CreatePopupMenu();
  AppendMenu(hFile, MF_STRING, IDM_FILE_NEW, L10N("menu_file_new"));
  AppendMenu(hFile, MF_STRING, IDM_FILE_OPEN, L10N("menu_file_open"));
  AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE, L10N("menu_file_save"));
  AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE_AS, L10N("menu_file_save_as"));
  AppendMenu(hFile, MF_STRING, IDM_FILE_CLOSE, L10N("menu_file_close"));
  AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
  AppendMenu(hFile, MF_STRING, IDM_FILE_SCRATCH, L10N("menu_file_scratch"));
  AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
  AppendMenu(hFile, MF_STRING, IDM_FILE_EXIT, L10N("menu_file_exit"));

  // Edit Menu
  HMENU hEdit = CreatePopupMenu();
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_UNDO, L10N("menu_edit_undo"));
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_REDO, L10N("menu_edit_redo"));
  AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_CUT, L10N("menu_edit_cut"));
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_COPY, L10N("menu_edit_copy"));
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_PASTE, L10N("menu_edit_paste"));
  AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_SELECT_ALL,
             L10N("menu_edit_select_all"));
  AppendMenu(hEdit, MF_SEPARATOR, 0, NULL);
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_FIND, L10N("menu_edit_find"));
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_REPLACE, L10N("menu_edit_replace"));
  AppendMenu(hEdit, MF_STRING, IDM_EDIT_GOTO, L10N("menu_edit_goto"));

  // View Menu
  HMENU hView = CreatePopupMenu();
  AppendMenu(hView, MF_STRING, IDM_VIEW_TOGGLE_UI, L10N("menu_view_toggle_ui"));
  AppendMenu(hView, MF_SEPARATOR, 0, NULL);
  AppendMenu(hView, MF_STRING, IDM_VIEW_ZOOM_IN, L10N("menu_view_zoom_in"));
  AppendMenu(hView, MF_STRING, IDM_VIEW_ZOOM_OUT, L10N("menu_view_zoom_out"));
  AppendMenu(hView, MF_STRING, IDM_VIEW_ZOOM_RESET,
             L10N("menu_view_zoom_reset"));

  // Config Menu
  HMENU hConfig = CreatePopupMenu();
  AppendMenu(hConfig, MF_STRING, IDM_CONFIG_SETTINGS,
             L10N("menu_config_settings"));
  AppendMenu(hConfig, MF_STRING, IDM_CONFIG_THEME, L10N("menu_config_theme"));
  AppendMenu(hConfig, MF_SEPARATOR, 0, NULL);
  AppendMenu(hConfig, MF_STRING, IDM_CONFIG_EDIT_INIT,
             L10N("menu_config_edit_init"));

  // Tools Menu
  HMENU hTools = CreatePopupMenu();
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_RUN_MACRO,
             L10N("menu_tools_run_macro"));
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_CONSOLE, L10N("menu_tools_console"));
  AppendMenu(hTools, MF_STRING, IDM_TOOLS_MACRO_GALLERY,
             L10N("menu_tools_macro_gallery"));

  // Language Menu
  HMENU hLang = CreatePopupMenu();
  AppendMenu(hLang, MF_STRING, IDM_LANG_EN, L10N("menu_language_en"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_JP, L10N("menu_language_jp"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_ES, L10N("menu_language_es"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_FR, L10N("menu_language_fr"));
  AppendMenu(hLang, MF_STRING, IDM_LANG_DE, L10N("menu_language_de"));

  // Buffers Menu (Dynamic)
  HMENU hBuffers = CreatePopupMenu();
  const auto &buffers = g_editor->GetBuffers();
  for (size_t i = 0; i < buffers.size(); ++i) {
    std::wstring name = buffers[i]->GetPath();
    if (name.empty()) {
      name = buffers[i]->IsScratch() ? L"Scratch" : L"Untitled";
    } else {
      // Just filename
      size_t pos = name.find_last_of(L"\\/");
      if (pos != std::wstring::npos)
        name = name.substr(pos + 1);
    }

    UINT flags = MF_STRING;
    if (i == g_editor->GetActiveBufferIndex())
      flags |= MF_CHECKED;
    AppendMenu(hBuffers, flags, IDM_BUFFERS_START + i, name.c_str());
  }

  // Help Menu
  HMENU hHelp = CreatePopupMenu();
  AppendMenu(hHelp, MF_STRING, IDM_HELP_DOC, L10N("menu_help_doc"));
  AppendMenu(hHelp, MF_STRING, IDM_HELP_ABOUT, L10N("menu_help_about"));

  // Main Menu Bar
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, L10N("menu_file"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEdit, L10N("menu_edit"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hView, L10N("menu_view"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hConfig, L10N("menu_config"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTools, L10N("menu_tools"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hLang, L10N("menu_language"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hBuffers, L10N("menu_buffers"));
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelp, L10N("menu_help"));

  SetMenu(hwnd, hMenu);
  SetWindowText(hwnd, L10N("title"));
  UpdateScrollbars(hwnd);
  UpdateTabs(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  if (uMsg == g_uFindMsgString) {
    LPFINDREPLACEW lpfr = (LPFINDREPLACEW)lParam;
    Buffer *buf = g_editor->GetActiveBuffer();
    if (!buf)
      return 0;

    auto WToUTF8 = [](LPCWSTR wstr) -> std::string {
      if (!wstr)
        return "";
      int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
      if (len <= 0)
        return "";
      std::string res(len - 1, 0);
      WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &res[0], len, NULL, NULL);
      return res;
    };

    if (lpfr->Flags & FR_DIALOGTERM) {
      g_hDlgFind = NULL;
    } else if (lpfr->Flags & FR_FINDNEXT) {
      std::string findWhat = WToUTF8(lpfr->lpstrFindWhat);
      bool forward = (lpfr->Flags & FR_DOWN) != 0;
      bool matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
      bool useRegex = false;
      if (findWhat.length() >= 2 && findWhat.front() == '/' &&
          findWhat.back() == '/') {
        findWhat = findWhat.substr(1, findWhat.length() - 2);
        useRegex = true;
      }

      size_t startPos = buf->GetCaretPos();

      if (forward && buf->HasSelection()) {
        size_t s, e;
        buf->GetSelectionRange(s, e);
        startPos = e;
      } else if (!forward && buf->HasSelection()) {
        size_t s, e;
        buf->GetSelectionRange(s, e);
        startPos = s;
      }

      size_t pos = buf->Find(findWhat, startPos, forward, useRegex, matchCase);
      if (pos != std::string::npos) {
        buf->SetSelectionAnchor(pos);
        buf->SetCaretPos(pos + findWhat.length());
        EnsureCaretVisible(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      } else {
        MessageBeep(MB_ICONWARNING);
      }
    } else if (lpfr->Flags & FR_REPLACE) {
      std::string findWhat = WToUTF8(lpfr->lpstrFindWhat);
      std::string replaceWith = WToUTF8(lpfr->lpstrReplaceWith);
      bool matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
      bool useRegex = false;
      if (findWhat.length() >= 2 && findWhat.front() == '/' &&
          findWhat.back() == '/') {
        findWhat = findWhat.substr(1, findWhat.length() - 2);
        useRegex = true;
      }

      size_t s, e;
      buf->GetSelectionRange(s, e);
      std::string selected = buf->GetText(s, e - s);
      if (selected == findWhat) {
        buf->Replace(s, e, replaceWith);
        buf->SetCaretPos(s + replaceWith.length());
        buf->SetSelectionAnchor(s + replaceWith.length());
      }
      SendMessage(hwnd, g_uFindMsgString, 0, (LPARAM)lpfr);
    } else if (lpfr->Flags & FR_REPLACEALL) {
      std::string findWhat = WToUTF8(lpfr->lpstrFindWhat);
      std::string replaceWith = WToUTF8(lpfr->lpstrReplaceWith);
      bool matchCase = (lpfr->Flags & FR_MATCHCASE) != 0;
      bool useRegex = false;
      if (findWhat.length() >= 2 && findWhat.front() == '/' &&
          findWhat.back() == '/') {
        findWhat = findWhat.substr(1, findWhat.length() - 2);
        useRegex = true;
      }

      size_t count = 0;
      size_t pos = 0;
      while ((pos = buf->Find(findWhat, pos, true, useRegex, matchCase)) !=
             std::string::npos) {
        buf->Replace(pos, pos + findWhat.length(), replaceWith);
        pos += replaceWith.length();
        count++;
      }
      if (count > 0) {
        UpdateScrollbars(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    }
    return 0;
  }

  switch (uMsg) {
  case WM_CREATE: {
    g_mainHwnd = hwnd;

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    // Create Tab Control
    g_tabHwnd = CreateWindowEx(0, WC_TABCONTROL, NULL,
                               WS_CHILD | WS_VISIBLE | TCS_TABS, 0, 0, 0, 0,
                               hwnd, (HMENU)2002, GetModuleHandle(NULL), NULL);

    // Create Status Bar
    g_statusHwnd = CreateWindowEx(
        0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0,
        0, 0, hwnd, (HMENU)2000, GetModuleHandle(NULL), NULL);

    // Create Progress Bar as a child of the status bar or just above it
    // For simplicity, we'll put it in one of the parts of the status bar later,
    // but for now let's just create it.
    g_progressHwnd = CreateWindowEx(
        0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 0, 0,
        g_statusHwnd, (HMENU)2001, GetModuleHandle(NULL), NULL);

    int parts[] = {300, 600, -1};
    SendMessage(g_statusHwnd, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessage(g_statusHwnd, SB_SETTEXT, 0, (LPARAM)L"Ready");

    g_renderer = std::make_unique<EditorBufferRenderer>();
    if (!g_renderer->Initialize(hwnd)) {
      return -1;
    }
    g_scriptEngine = std::make_unique<ScriptEngine>();
    g_scriptEngine->Initialize();

    g_editor = std::make_unique<Editor>();
    g_editor->NewFile();
    UpdateMenu(hwnd);
    SetTimer(hwnd, 1, 500, NULL);
    g_uFindMsgString = RegisterWindowMessageW(FINDMSGSTRINGW);
    DragAcceptFiles(hwnd, TRUE);

    // Load initialization script
    wchar_t appData[MAX_PATH];
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
      std::wstring initPath = std::wstring(appData) + L"\\Ecode\\ecodeinit.js";
      g_scriptEngine->RunFile(initPath);
    }

    // Apply managed settings
    auto& settings = SettingsManager::Instance();
    settings.Load();
    
    RECT rc = {0};
    settings.GetWindowRect(rc);
    if (rc.right > rc.left && rc.bottom > rc.top) {
        MoveWindow(hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
    }
    if (settings.IsWindowMaximized()) {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    g_renderer->SetFont(settings.GetFontFamily(), settings.GetFontSize());
    g_renderer->SetWordWrap(settings.IsWordWrap());
    Localization::Instance().SetLanguage(static_cast<Language>(settings.GetLanguage()));
    UpdateMenu(hwnd);

    return 0;
}
  case WM_DESTROY: {
    auto& settings = SettingsManager::Instance();
    WINDOWPLACEMENT wp = {0};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd, &wp)) {
        settings.SetWindowRect(wp.rcNormalPosition);
        settings.SetWindowMaximized(wp.showCmd == SW_SHOWMAXIMIZED);
    }
    settings.SetFontFamily(g_renderer->GetFontFamily());
    settings.SetFontSize(g_renderer->GetFontSize());
    settings.SetLanguage(static_cast<int>(Localization::Instance().GetCurrentLanguage()));
    settings.SetWordWrap(g_renderer->IsWordWrap());
    settings.Save();
    
    PostQuitMessage(0);
    return 0;
  }
  case WM_COMMAND: {
    switch (LOWORD(wParam)) {
    case IDM_FILE_NEW:
      g_editor->NewFile();
      UpdateMenu(hwnd);
      break;
    case IDM_FILE_SCRATCH:
      g_editor->NewFile();
      if (g_editor->GetActiveBuffer()) {
        g_editor->GetActiveBuffer()->SetScratch(true);
        g_editor->GetActiveBuffer()->Insert(
            0, "// Scratch Buffer - Press Ctrl+Enter to execute JS\n");
      }
      UpdateMenu(hwnd);
      break;
    case IDM_LANG_EN:
      Localization::Instance().SetLanguage(Language::English);
      UpdateMenu(hwnd);
      break;
    case IDM_LANG_JP:
      Localization::Instance().SetLanguage(Language::Japanese);
      UpdateMenu(hwnd);
      break;
    case IDM_LANG_ES:
      Localization::Instance().SetLanguage(Language::Spanish);
      UpdateMenu(hwnd);
      break;
    case IDM_LANG_FR:
      Localization::Instance().SetLanguage(Language::French);
      UpdateMenu(hwnd);
      break;
    case IDM_LANG_DE:
      Localization::Instance().SetLanguage(Language::German);
      UpdateMenu(hwnd);
      break;
    case IDM_FILE_EXIT:
      PostQuitMessage(0);
      break;

    case IDM_FILE_OPEN: {
      std::wstring path = Dialogs::OpenFileDialog(hwnd);
      if (!path.empty()) {
        g_editor->OpenFile(path);
      }
      break;
    }
    case IDM_FILE_SAVE: {
      Buffer *buf = g_editor->GetActiveBuffer();
      if (buf) {
        if (buf->GetPath().empty()) {
          std::wstring path = Dialogs::SaveFileDialog(hwnd);
          if (!path.empty())
            buf->SaveFile(path);
        } else {
          buf->SaveFile(buf->GetPath());
        }
      }
      break;
    }
    case IDM_FILE_SAVE_AS: {
      Buffer *buf = g_editor->GetActiveBuffer();
      if (buf) {
        std::wstring path = Dialogs::SaveFileDialog(hwnd);
        if (!path.empty())
          buf->SaveFile(path);
      }
      break;
    }
    case IDM_HELP_ABOUT:
      Dialogs::ShowAboutDialog(hwnd);
      break;

    // Stubs for other menu items
    case IDM_FILE_CLOSE:
      g_editor->CloseBuffer(g_editor->GetActiveBufferIndex());
      UpdateMenu(hwnd);
      break;
    case IDM_EDIT_UNDO:
      g_editor->Undo();
      break;
    case IDM_EDIT_REDO:
      g_editor->Redo();
      break;
    case IDM_EDIT_CUT:
      g_editor->Cut(hwnd);
      break;
    case IDM_EDIT_COPY:
      g_editor->Copy(hwnd);
      break;
    case IDM_EDIT_PASTE:
      g_editor->Paste(hwnd);
      break;
    case IDM_EDIT_SELECT_ALL: {
      Buffer *buf = g_editor->GetActiveBuffer();
      if (buf) {
        buf->SetSelectionAnchor(0);
        buf->SetCaretPos(buf->GetTotalLength());
      }
      break;
    }
    case IDM_EDIT_FIND: {
      ZeroMemory(&g_fr, sizeof(g_fr));
      g_fr.lStructSize = sizeof(g_fr);
      g_fr.hwndOwner = hwnd;
      g_fr.lpstrFindWhat = g_szFindWhat;
      g_fr.wFindWhatLen = 256;
      g_hDlgFind = FindTextW(&g_fr);
      break;
    }
    case IDM_EDIT_REPLACE: {
      ZeroMemory(&g_fr, sizeof(g_fr));
      g_fr.lStructSize = sizeof(g_fr);
      g_fr.hwndOwner = hwnd;
      g_fr.lpstrFindWhat = g_szFindWhat;
      g_fr.wFindWhatLen = 256;
      g_fr.lpstrReplaceWith = g_szReplaceWith;
      g_fr.wReplaceWithLen = 256;
      g_hDlgFind = ReplaceTextW(&g_fr);
      break;
    }
    case IDM_EDIT_GOTO:
      Dialogs::ShowJumpToLineDialog(hwnd);
      break;
    case IDM_VIEW_TOGGLE_UI:
      MessageBox(hwnd, L"Toggle UI - Under Construction", L"Info", MB_OK);
      break;
    case IDM_VIEW_ZOOM_IN:
      g_renderer->ZoomIn();
      UpdateScrollbars(hwnd);
      break;
    case IDM_VIEW_ZOOM_OUT:
      g_renderer->ZoomOut();
      UpdateScrollbars(hwnd);
      break;
    case IDM_VIEW_ZOOM_RESET:
      g_renderer->ZoomReset();
      UpdateScrollbars(hwnd);
      break;
    case IDM_CONFIG_SETTINGS:
      Dialogs::ShowSettingsDialog(hwnd);
      break;
    case IDM_CONFIG_THEME:
    case IDM_CONFIG_EDIT_INIT:
    case IDM_TOOLS_RUN_MACRO:
    case IDM_TOOLS_CONSOLE:
      MessageBox(hwnd, L"Feature not yet implemented", L"Info", MB_OK);
      break;
    case IDM_TOOLS_MACRO_GALLERY:
      Dialogs::ShowMacroGalleryDialog(hwnd);
      break;
    case IDM_HELP_DOC:
      MessageBox(hwnd, L"Feature not yet implemented", L"Info", MB_OK);
      break;

    default:
      if (LOWORD(wParam) >= IDM_BUFFERS_START &&
          LOWORD(wParam) < IDM_BUFFERS_START + 100) {
        g_editor->SwitchToBuffer(LOWORD(wParam) - IDM_BUFFERS_START);
        UpdateMenu(hwnd);
      }
      break;
    }
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_SIZE: {
    UINT width = LOWORD(lParam);
    UINT height = HIWORD(lParam);

    // Resize Tab Control
    MoveWindow(g_tabHwnd, 0, 0, width, 25, TRUE);
    int tabHeight = 25;

    // Resize Status Bar
    SendMessage(g_statusHwnd, WM_SIZE, 0, 0);

    // Reposition Progress Bar within the status bar (part 2)
    RECT rcStatus;
    SendMessage(g_statusHwnd, SB_GETRECT, 2, (LPARAM)&rcStatus);
    MoveWindow(g_progressHwnd, rcStatus.left + 2, rcStatus.top + 2,
               rcStatus.right - rcStatus.left - 4,
               rcStatus.bottom - rcStatus.top - 4, TRUE);

    // Calculate available height for editor
    RECT rcStatusWnd;
    GetWindowRect(g_statusHwnd, &rcStatusWnd);
    int statusHeight = rcStatusWnd.bottom - rcStatusWnd.top;

    g_renderer->SetTopOffset((float)tabHeight);
    g_renderer->Resize(width, height - tabHeight - statusHeight);
    UpdateScrollbars(hwnd);
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    BeginPaint(hwnd, &ps);
    Buffer *activeBuffer = g_editor->GetActiveBuffer();
    if (activeBuffer) {
      std::string content = activeBuffer->GetVisibleText();
      
      auto selectionRanges = activeBuffer->GetSelectionRanges();
      
      size_t visualCaret =
          activeBuffer->LogicalToVisualOffset(activeBuffer->GetCaretPos());

      std::vector<size_t> physicalLineNumbers;
      size_t visibleLineCount = activeBuffer->GetVisibleLineCount();
      physicalLineNumbers.reserve(visibleLineCount);
      for (size_t i = 0; i < visibleLineCount; ++i) {
        physicalLineNumbers.push_back(activeBuffer->GetPhysicalLine(i));
      }

      g_renderer->DrawEditorLines(content, visualCaret, &selectionRanges,
                         activeBuffer->GetScrollLine() + 1,
                         activeBuffer->GetScrollX(), &physicalLineNumbers,
                         activeBuffer->GetTotalLines());
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_CHAR: {
    if (g_scriptEngine->IsKeyboardCaptured()) {
      char c = static_cast<char>(wParam);
      if (g_scriptEngine->HandleKeyEvent(std::string(1, c), true)) {
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
      }
    }
    Buffer *activeBuffer = g_editor->GetActiveBuffer();
    if (activeBuffer) {
      if (wParam >= 32) {
        if (activeBuffer->HasSelection()) {
          activeBuffer->DeleteSelection();
        }
        char c = static_cast<char>(wParam);
        std::string s(1, c);
        activeBuffer->Insert(activeBuffer->GetCaretPos(), s);
        activeBuffer->MoveCaret(1);
        activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        EnsureCaretVisible(hwnd);
        UpdateScrollbars(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      } else if (wParam == VK_BACK) {
        if (activeBuffer->HasSelection()) {
          activeBuffer->DeleteSelection();
        } else {
          size_t pos = activeBuffer->GetCaretPos();
          if (pos > 0) {
            activeBuffer->Delete(pos - 1, 1);
            activeBuffer->MoveCaret(-1);
            activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
            EnsureCaretVisible(hwnd);
            UpdateScrollbars(hwnd);
          }
        }
        InvalidateRect(hwnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_KEYDOWN: {
    std::string chord;
    if (GetKeyState(VK_CONTROL) & 0x8000)
      chord += "Ctrl+";
    if (GetKeyState(VK_SHIFT) & 0x8000)
      chord += "Shift+";
    if (GetKeyState(VK_MENU) & 0x8000)
      chord += "Alt+";
    
    if (GetKeyState(VK_CONTROL) & 0x8000 && wParam == 'G') {
        Dialogs::ShowJumpToLineDialog(hwnd);
        return 0;
    }

    if (wParam >= 'A' && wParam <= 'Z') {
      chord += (char)wParam;
    } else if (wParam >= VK_F1 && wParam <= VK_F12) {
      chord += "F" + std::to_string(wParam - VK_F1 + 1);
    } else if (wParam == VK_RETURN) {
      chord += "Enter";
    } else if (wParam == VK_LEFT) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer && activeBuffer->GetCaretPos() > 0) {
        activeBuffer->MoveCaret(-1);
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_RIGHT) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaret(1);
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_UP) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaretUp();
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_DOWN) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaretDown();
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_HOME) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaretHome();
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_END) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaretEnd();
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_PRIOR) { // PageUp
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaretPageUp(
            20); // TODO: Calculate actual visible lines
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_NEXT) { // PageDown
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        activeBuffer->MoveCaretPageDown(20);
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(activeBuffer->GetCaretPos());
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    } else if (wParam == VK_DELETE) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        if (activeBuffer->HasSelection()) {
          activeBuffer->DeleteSelection();
        } else if (activeBuffer->GetCaretPos() <
                   activeBuffer->GetTotalLength()) {
          activeBuffer->Delete(activeBuffer->GetCaretPos(), 1);
        }
        InvalidateRect(hwnd, NULL, FALSE);
        EnsureCaretVisible(hwnd);
        return 0;
      }
    }

    if (g_scriptEngine->IsKeyboardCaptured()) {
      std::string keyName;
      if (wParam == VK_ESCAPE)
        keyName = "Esc";
      else if (wParam == VK_RETURN)
        keyName = "Enter";
      else if (wParam == VK_BACK)
        keyName = "Backspace";
      else if (wParam == VK_DELETE)
        keyName = "Delete";
      else if (wParam == VK_UP)
        keyName = "Up";
      else if (wParam == VK_DOWN)
        keyName = "Down";
      else if (wParam == VK_LEFT)
        keyName = "Left";
      else if (wParam == VK_RIGHT)
        keyName = "Right";
      else if (wParam == VK_PRIOR)
        keyName = "PageUp";
      else if (wParam == VK_NEXT)
        keyName = "PageDown";
      else if (wParam == VK_HOME)
        keyName = "Home";
      else if (wParam == VK_END)
        keyName = "End";
      else if (wParam == VK_TAB)
        keyName = "Tab";

      if (!keyName.empty() || chord.size() > 0) {
        std::string fullKey = chord;
        if (keyName.empty() && wParam >= 'A' && wParam <= 'Z')
          keyName = (char)wParam;

        if (!keyName.empty()) {
          if (g_scriptEngine->HandleKeyEvent(chord + keyName, false)) {
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
          }
        }
      }
    }

    if (!chord.empty() && g_scriptEngine->HandleBinding(chord)) {
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }

    if (wParam == VK_RETURN && (GetKeyState(VK_CONTROL) & 0x8000)) {
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer && activeBuffer->IsScratch()) {
        std::string code =
            activeBuffer->GetText(0, activeBuffer->GetTotalLength());
        std::string result = g_scriptEngine->Evaluate(code);
        activeBuffer->Insert(activeBuffer->GetTotalLength(),
                             "\n// Result: " + result + "\n");
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    break;
  }
  case WM_LBUTTONDOWN: {
    g_isDragging = true;
    SetCapture(hwnd);
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    Buffer *activeBuffer = g_editor->GetActiveBuffer();
    if (activeBuffer) {
      size_t visualLineIndex;
      std::string visibleText = activeBuffer->GetVisibleText();
      if (g_renderer->HitTestGutter(visibleText, (float)x, (float)y,
                                    visualLineIndex)) {
        size_t physicalLine =
            activeBuffer->GetPhysicalLine(visualLineIndex +
                                          activeBuffer->GetScrollLine());
        
        // Check if hit fold indicator area (approx last 15 pixels of gutter)
        // We'll trust EditorBufferRenderer to give us the line, but we check X here
        // This is a bit of a hack, better to have HitTestGutter return more info
        // but for now...
        float gutterWidth;
        {
            size_t totalLines = activeBuffer->GetTotalLines();
            int digits = (int)std::to_string(totalLines).length();
            gutterWidth = (digits * 8.0f) + 15.0f;
        }

        if (x > gutterWidth - 15.0f) {
            activeBuffer->ToggleFold(physicalLine);
        } else {
            activeBuffer->SelectLine(physicalLine);
        }
      } else {
        size_t visualPos =
            g_renderer->GetPositionFromPoint(visibleText, (float)x, (float)y);
        size_t pos = activeBuffer->VisualToLogicalOffset(visualPos);
        activeBuffer->SetCaretPos(pos);
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
          activeBuffer->SetSelectionAnchor(pos);
        }
        EnsureCaretVisible(hwnd);
      }
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_MOUSEMOVE: {
    if (g_isDragging) {
      int x = LOWORD(lParam);
      int y = HIWORD(lParam);
      Buffer *activeBuffer = g_editor->GetActiveBuffer();
      if (activeBuffer) {
        // Toggle Box selection if Alt is pressed
        if (GetKeyState(VK_MENU) & 0x8000) {
            activeBuffer->SetSelectionMode(SelectionMode::Box);
        } else {
            activeBuffer->SetSelectionMode(SelectionMode::Normal);
        }

        std::string text = activeBuffer->GetVisibleText();
        size_t visualPos =
            g_renderer->GetPositionFromPoint(text, (float)x, (float)y);
        size_t pos = activeBuffer->VisualToLogicalOffset(visualPos);
        activeBuffer->SetCaretPos(pos);
        EnsureCaretVisible(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_LBUTTONUP: {
    if (g_isDragging) {
      g_isDragging = false;
      ReleaseCapture();
    }
    return 0;
  }
  case WM_CONTEXTMENU: {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_UNDO, L10N("menu_edit_undo"));
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_REDO, L10N("menu_edit_redo"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_CUT, L10N("menu_edit_cut"));
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_COPY, L10N("menu_edit_copy"));
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_PASTE, L10N("menu_edit_paste"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, IDM_EDIT_SELECT_ALL,
               L10N("menu_edit_select_all"));

    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    if (x == -1 && y == -1) {
      RECT rc;
      GetWindowRect(hwnd, &rc);
      x = rc.left + 50;
      y = rc.top + 50;
    }
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
    return 0;
  }
  case WM_VSCROLL: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (!buf)
      return 0;

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    int oldPos = si.nPos;
    switch (LOWORD(wParam)) {
    case SB_TOP:
      si.nPos = si.nMin;
      break;
    case SB_BOTTOM:
      si.nPos = si.nMax;
      break;
    case SB_LINEUP:
      si.nPos -= 1;
      break;
    case SB_LINEDOWN:
      si.nPos += 1;
      break;
    case SB_PAGEUP:
      si.nPos -= si.nPage;
      break;
    case SB_PAGEDOWN:
      si.nPos += si.nPage;
      break;
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
    }

    si.fMask = SIF_POS;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    GetScrollInfo(hwnd, SB_VERT, &si);

    if (si.nPos != oldPos) {
      buf->SetScrollLine(si.nPos);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_HSCROLL: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (!buf)
      return 0;

    SCROLLINFO si = {0};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_HORZ, &si);

    int oldPos = si.nPos;
    switch (LOWORD(wParam)) {
    case SB_LEFT:
      si.nPos = si.nMin;
      break;
    case SB_RIGHT:
      si.nPos = si.nMax;
      break;
    case SB_LINELEFT:
      si.nPos -= 10;
      break;
    case SB_LINERIGHT:
      si.nPos += 10;
      break;
    case SB_PAGELEFT:
      si.nPos -= si.nPage;
      break;
    case SB_PAGERIGHT:
      si.nPos += si.nPage;
      break;
    case SB_THUMBTRACK:
      si.nPos = si.nTrackPos;
      break;
    }

    si.fMask = SIF_POS;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
    GetScrollInfo(hwnd, SB_HORZ, &si);

    if (si.nPos != oldPos) {
      buf->SetScrollX((float)si.nPos);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_MOUSEWHEEL: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (!buf)
      return 0;

    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int lines = delta / WHEEL_DELTA * 3; // Scroll 3 lines at a time
    int newScroll = (int)buf->GetScrollLine() - lines;
    if (newScroll < 0)
      newScroll = 0;
    int maxScroll = (int)buf->GetTotalLines() - 1;
    if (newScroll > maxScroll)
      newScroll = maxScroll;

    if (newScroll != (int)buf->GetScrollLine()) {
      buf->SetScrollLine(newScroll);
      UpdateScrollbars(hwnd);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_TIMER: {
    if (wParam == 1) {
      static bool caretVisible = true;
      caretVisible = !caretVisible;
      g_renderer->SetCaretVisible(caretVisible);
      InvalidateRect(hwnd, NULL, FALSE);
    }
    return 0;
  }
  case WM_CLOSE: {
    bool cancelExit = false;
    for (const auto &buf : g_editor->GetBuffers()) {
      if (buf->IsDirty()) {
        std::wstring name =
            buf->GetPath().empty() ? L"Untitled" : buf->GetPath();
        int res = MessageBoxW(hwnd, (L"Save changes to " + name + L"?").c_str(),
                              L"Ecode", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (res == IDCANCEL) {
          cancelExit = true;
          break;
        } else if (res == IDYES) {
          if (buf->GetPath().empty()) {
            std::wstring path = Dialogs::SaveFileDialog(hwnd);
            if (!path.empty())
              buf->SaveFile(path);
            else {
              cancelExit = true;
              break;
            }
          } else {
            buf->SaveFile(buf->GetPath());
          }
        }
      }
    }
    if (!cancelExit)
      DestroyWindow(hwnd);
    return 0;
  }
  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)wParam;
    UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < count; i++) {
      wchar_t path[MAX_PATH];
      DragQueryFile(hDrop, i, path, MAX_PATH);
      g_editor->OpenFile(path);
    }
    DragFinish(hDrop);
    UpdateMenu(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }
  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->hwndFrom == g_tabHwnd && pnm->code == TCN_SELCHANGE) {
      int sel = TabCtrl_GetCurSel(g_tabHwnd);
      if (sel != -1) {
        g_editor->SwitchToBuffer(static_cast<size_t>(sel));
        UpdateMenu(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
      }
    }
    return 0;
  }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
  const wchar_t CLASS_NAME[] = L"EcodeWindowClass";

  WNDCLASS wc = {};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor = LoadCursor(NULL, IDC_IBEAM);

  RegisterClass(&wc);

  HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Ecode",
                             WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

  if (hwnd == NULL)
    return 0;

  // Handle command line arguments
  int argc;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv) {
    for (int i = 1; i < argc; ++i) {
      if (wcscmp(argv[i], L"-e") == 0 && i + 1 < argc) {
        std::wstring code = argv[++i];
        int len = WideCharToMultiByte(CP_UTF8, 0, code.c_str(), -1, NULL, 0,
                                      NULL, NULL);
        if (len > 0) {
          std::string codeA(len - 1, 0);
          WideCharToMultiByte(CP_UTF8, 0, code.c_str(), -1, &codeA[0], len,
                              NULL, NULL);
          g_scriptEngine->Evaluate(codeA);
        }
      } else {
        g_editor->OpenFile(argv[i]);
      }
    }
    LocalFree(argv);
    UpdateMenu(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
  }

  ShowWindow(hwnd, nCmdShow);

  MSG msg = {};
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (g_hDlgFind == NULL || !IsDialogMessage(g_hDlgFind, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  return 0;
}
