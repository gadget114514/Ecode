#include "../include/Dialogs.h"
#include "../include/Editor.h"
#include "../include/EditorBufferRenderer.h"
#include "../include/Localization.h"
#include "../include/ScriptEngine.h"
#include "../include/SettingsManager.h"
#include "../include/resource.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

#include "Globals.inl"
#include <objbase.h>
#include <vector>

// Redundant externs removed, they are in Globals.inl

// Use fully qualified calls or ensure C++17

INT_PTR CALLBACK JumpToLineDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG:
    return (INT_PTR)TRUE;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      UINT lineNum = GetDlgItemInt(hDlg, IDC_LINE_NUMBER, NULL, FALSE);
      if (g_editor) {
        Buffer *buf = g_editor->GetActiveBuffer();
        if (buf && lineNum > 0 && lineNum <= buf->GetTotalLines()) {
          size_t offset = buf->GetLineOffset(lineNum - 1);
          buf->SetCaretPos(offset);
          buf->SetSelectionAnchor(offset);
        }
      }
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowJumpToLineDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_JUMP_TO_LINE), hwnd,
             JumpToLineDlgProc);
  InvalidateRect(hwnd, NULL, FALSE);
}

INT_PTR CALLBACK MacroGalleryDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  static std::vector<std::wstring> macroPaths;

  switch (message) {
  case WM_INITDIALOG: {
    macroPaths.clear();
    HWND hList = GetDlgItem(hDlg, IDC_MACRO_LIST);
    wchar_t appData[MAX_PATH];
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) {
      std::wstring macroDir = std::wstring(appData) + L"\\Ecode\\macros";
      if (fs::exists(macroDir)) {
        for (const auto &entry : fs::directory_iterator(macroDir)) {
          if (entry.path().extension() == L".js") {
            macroPaths.push_back(entry.path().wstring());
            SendMessage(hList, LB_ADDSTRING, 0,
                        (LPARAM)entry.path().filename().c_str());
          }
        }
      }
    }
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_RUN_MACRO ||
        (LOWORD(wParam) == IDC_MACRO_LIST && HIWORD(wParam) == LBN_DBLCLK)) {
      HWND hList = GetDlgItem(hDlg, IDC_MACRO_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR && sel < (int)macroPaths.size()) {
        if (g_scriptEngine) {
          g_scriptEngine->RunFile(macroPaths[sel]);
        }
      }
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_CANCEL_MACRO ||
               LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowMacroGalleryDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_MACRO_GALLERY), hwnd,
             MacroGalleryDlgProc);
  InvalidateRect(hwnd, NULL, FALSE);
}

std::wstring Dialogs::OpenFileDialog(HWND hwnd) {
  wchar_t fileName[MAX_PATH] = L"";
  OPENFILENAMEW ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = fileName;
  ofn.nMaxFile = MAX_PATH;

  // Build filter buffer for Windows common dialog (embedded \0 separators)
  std::wstring defExt = SettingsManager::Instance().GetDefaultExtension();
  wchar_t filterBuf[512] = {0};
  if (defExt.empty()) {
    wcscpy_s(filterBuf, 512, L"Text Files (*.txt)");
    size_t pos = wcslen(filterBuf) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"*.txt");
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"All Files (*.*)");
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"*.*");
    pos += wcslen(filterBuf + pos) + 1;
    filterBuf[pos] = L'\0';
    ofn.lpstrDefExt = L"txt";
  } else {
    // Parse semicolon-separated extensions: "*.txt;*.md;*.json"
    std::vector<std::wstring> exts;
    size_t start = 0, end;
    std::wstring extStr = defExt;
    // Strip leading *. if present
    while ((end = extStr.find(L';', start)) != std::wstring::npos) {
      std::wstring e = extStr.substr(start, end - start);
      if (!e.empty()) exts.push_back(e);
      start = end + 1;
    }
    if (start < extStr.size()) exts.push_back(extStr.substr(start));

    // Build: "Supported Files (*.ext1;*.ext2)\0*.ext1;*.ext2\0All Files (*.*)\0*.*\0"
    std::wstring display = L"Supported Files (";
    std::wstring pattern;
    for (size_t i = 0; i < exts.size(); ++i) {
      std::wstring e = exts[i];
      if (e.find(L'.') == 0) e = L"*" + e;
      else if (e.find(L'*') != 0) e = L"*." + e;
      if (i > 0) { display += L"; "; pattern += L";"; }
      display += e; pattern += e;
    }
    display += L")";

    wcscpy_s(filterBuf, 512, display.c_str());
    size_t pos = wcslen(filterBuf) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, pattern.c_str());
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"All Files (*.*)");
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"*.*");
    pos += wcslen(filterBuf + pos) + 1;
    filterBuf[pos] = L'\0';

    // Use first extension as default
    static std::wstring s_firstExt;
    s_firstExt = exts.empty() ? L"txt" : exts[0];
    if (s_firstExt.find(L'.') == 0) s_firstExt = s_firstExt.substr(1);
    else if (s_firstExt.find(L"*.") == 0) s_firstExt = s_firstExt.substr(2);
    ofn.lpstrDefExt = s_firstExt.c_str();
  }
  ofn.lpstrFilter = filterBuf;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameW(&ofn)) {
    return fileName;
  } else {
    DWORD err = CommDlgExtendedError();
    if (err != 0) {
      DebugLog("OpenFileDialog failed with extended error: " +
                   std::to_string(err),
               LOG_ERROR);
    }
  }
  return L"";
}

std::wstring Dialogs::SaveFileDialog(HWND hwnd) {
  wchar_t fileName[MAX_PATH] = L"";
  OPENFILENAMEW ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFile = fileName;
  ofn.nMaxFile = MAX_PATH;

  std::wstring defExt = SettingsManager::Instance().GetDefaultExtension();
  wchar_t filterBuf[512] = {0};
  if (defExt.empty()) {
    wcscpy_s(filterBuf, 512, L"Text Files (*.txt)");
    size_t pos = wcslen(filterBuf) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"*.txt");
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"All Files (*.*)");
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"*.*");
    pos += wcslen(filterBuf + pos) + 1;
    filterBuf[pos] = L'\0';
    ofn.lpstrDefExt = L"txt";
  } else {
    // Parse semicolon-separated extensions
    std::vector<std::wstring> exts;
    size_t start = 0, end;
    std::wstring extStr = defExt;
    while ((end = extStr.find(L';', start)) != std::wstring::npos) {
      std::wstring e = extStr.substr(start, end - start);
      if (!e.empty()) exts.push_back(e);
      start = end + 1;
    }
    if (start < extStr.size()) exts.push_back(extStr.substr(start));

    std::wstring display = L"Supported Files (";
    std::wstring pattern;
    for (size_t i = 0; i < exts.size(); ++i) {
      std::wstring e = exts[i];
      if (e.find(L'.') == 0) e = L"*" + e;
      else if (e.find(L'*') != 0) e = L"*." + e;
      if (i > 0) { display += L"; "; pattern += L";"; }
      display += e; pattern += e;
    }
    display += L")";

    wcscpy_s(filterBuf, 512, display.c_str());
    size_t pos = wcslen(filterBuf) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, pattern.c_str());
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"All Files (*.*)");
    pos += wcslen(filterBuf + pos) + 1;
    wcscpy_s(filterBuf + pos, 512 - pos, L"*.*");
    pos += wcslen(filterBuf + pos) + 1;
    filterBuf[pos] = L'\0';

    static std::wstring s_firstExt;
    s_firstExt = exts.empty() ? L"txt" : exts[0];
    if (s_firstExt.find(L'.') == 0) s_firstExt = s_firstExt.substr(1);
    else if (s_firstExt.find(L"*.") == 0) s_firstExt = s_firstExt.substr(2);
    ofn.lpstrDefExt = s_firstExt.c_str();
  }
  ofn.lpstrFilter = filterBuf;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_OVERWRITEPROMPT;

  if (GetSaveFileNameW(&ofn)) {
    return fileName;
  } else {
    DWORD err = CommDlgExtendedError();
    if (err != 0) {
      DebugLog("SaveFileDialog failed with extended error: " +
                   std::to_string(err),
               LOG_ERROR);
    }
  }
  return L"";
}

void Dialogs::ShowAboutDialog(HWND hwnd) {
  std::wstring title = L10N("title");
  std::wstring message =
      title + L"\nVersion 1.0\nBuilt with DirectWrite and Duktape JS.";
  MessageBoxW(hwnd, message.c_str(), L"About Ecode",
              MB_OK | MB_ICONINFORMATION);
}

static HWND g_hDlgSettingsGeneral = NULL;

INT_PTR CALLBACK GeneralSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                        LPARAM lParam);
INT_PTR CALLBACK AiSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam);
void CreateNewTerminal(HWND hwnd, const std::wstring &shell, const std::wstring &label);

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    HWND hTab = GetDlgItem(hDlg, IDC_TAB_SETTINGS);
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"General";
    TabCtrl_InsertItem(hTab, 0, &tie);

    g_hDlgSettingsGeneral = CreateDialogW(
        GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_SETTINGS_GENERAL), hDlg,
        GeneralSettingsDlgProc);

    RECT rcTab;
    GetWindowRect(hTab, &rcTab);
    MapWindowPoints(NULL, hDlg, (LPPOINT)&rcTab, 2);
    TabCtrl_AdjustRect(hTab, FALSE, &rcTab);

    SetWindowPos(g_hDlgSettingsGeneral, HWND_TOP, rcTab.left, rcTab.top,
                 rcTab.right - rcTab.left, rcTab.bottom - rcTab.top,
                 SWP_SHOWWINDOW);

    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      SendMessage(g_hDlgSettingsGeneral, WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

INT_PTR CALLBACK GeneralSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                        LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    if (g_renderer) {
      SetDlgItemTextW(hDlg, IDC_FONT_FAMILY,
                      g_renderer->GetFontFamily().c_str());
      SetDlgItemInt(hDlg, IDC_FONT_SIZE,
                    static_cast<UINT>(g_renderer->GetFontSize()), FALSE);
      SetDlgItemInt(hDlg, IDC_FONT_WEIGHT,
                    static_cast<UINT>(g_renderer->GetFontWeight()), FALSE);
      CheckDlgButton(hDlg, IDC_SHOW_LINE_NUMBERS,
                     g_renderer->GetShowLineNumbers() ? BST_CHECKED
                                                      : BST_UNCHECKED);
      CheckDlgButton(hDlg, IDC_ENABLE_LIGATURES,
                     g_renderer->GetEnableLigatures() ? BST_CHECKED
                                                      : BST_UNCHECKED);
      CheckDlgButton(hDlg, IDC_WORD_WRAP,
                     g_renderer->GetWordWrap() ? BST_CHECKED : BST_UNCHECKED);
      ::SetDlgItemInt(hDlg, IDC_WRAP_WIDTH,
                      static_cast<UINT>(g_renderer->GetWrapWidth()), FALSE);

      HWND hCaret = GetDlgItem(hDlg, IDC_CARET_STYLE);
      ::SendMessage(hCaret, CB_ADDSTRING, 0, (LPARAM)L"Line");
      ::SendMessage(hCaret, CB_ADDSTRING, 0, (LPARAM)L"Block");
      ::SendMessage(hCaret, CB_ADDSTRING, 0, (LPARAM)L"Underline");
      ::SendMessage(hCaret, CB_SETCURSEL,
                    static_cast<WPARAM>(g_renderer->GetCaretStyle()), 0);

      CheckDlgButton(hDlg, IDC_CARET_BLINKING,
                     SettingsManager::Instance().IsCaretBlinking()
                         ? BST_CHECKED
                         : BST_UNCHECKED);
    }

    HWND hLog = GetDlgItem(hDlg, IDC_LOG_LEVEL);
    SendMessage(hLog, CB_ADDSTRING, 0, (LPARAM)L"DEBUG");
    SendMessage(hLog, CB_ADDSTRING, 0, (LPARAM)L"INFO");
    SendMessage(hLog, CB_ADDSTRING, 0, (LPARAM)L"WARN");
    SendMessage(hLog, CB_ADDSTRING, 0, (LPARAM)L"ERROR");
    SendMessage(hLog, CB_SETCURSEL, (WPARAM)g_currentLogLevel, 0);

    HWND hCombo = GetDlgItem(hDlg, IDC_LANGUAGE);
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Japanese");
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Spanish");
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"French");
    SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"German");
    SendMessage(hCombo, CB_SETCURSEL,
                (WPARAM)Localization::Instance().GetCurrentLanguage(), 0);

    SetDlgItemTextW(hDlg, IDC_BASH_PATH,
                    SettingsManager::Instance().GetBashPath().c_str());
    // Hide Show AI checkbox (AI is managed separately)
    ShowWindow(GetDlgItem(hDlg, IDC_SHOW_AI), SW_HIDE);

    SetDlgItemTextW(hDlg, IDC_DEFAULT_EXT,
                    SettingsManager::Instance().GetDefaultExtension().c_str());

    SetDlgItemTextW(hDlg, IDC_PLUGINS_DIR,
                    SettingsManager::Instance().GetPluginsDirectory().c_str());

    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_BASH_BROWSE) {
      std::wstring path = Dialogs::BrowseForFolder(hDlg);
      if (!path.empty()) {
        std::wstring bashPath = path + L"\\usr\\bin\\bash.exe";
        if (GetFileAttributesW(bashPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
          bashPath = path + L"\\bin\\bash.exe";
        }
        if (GetFileAttributesW(bashPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
          SetDlgItemTextW(hDlg, IDC_BASH_PATH, bashPath.c_str());
        } else {
          SetDlgItemTextW(hDlg, IDC_BASH_PATH, path.c_str());
        }
      }
      return (INT_PTR)TRUE;
    }
    if (LOWORD(wParam) == IDC_PLUGINS_BROWSE) {
      std::wstring path = Dialogs::BrowseForFolder(hDlg);
      if (!path.empty()) {
        SetDlgItemTextW(hDlg, IDC_PLUGINS_DIR, path.c_str());
      }
      return (INT_PTR)TRUE;
    }
    if (LOWORD(wParam) == IDOK) {
      wchar_t fontFamily[256];
      GetDlgItemTextW(hDlg, IDC_FONT_FAMILY, fontFamily, 256);
      UINT fontSize = GetDlgItemInt(hDlg, IDC_FONT_SIZE, NULL, FALSE);
      UINT fontWeight = GetDlgItemInt(hDlg, IDC_FONT_WEIGHT, NULL, FALSE);
      BOOL showLineNums = IsDlgButtonChecked(hDlg, IDC_SHOW_LINE_NUMBERS);
      BOOL enableLigatures = IsDlgButtonChecked(hDlg, IDC_ENABLE_LIGATURES);
      BOOL wordWrap = IsDlgButtonChecked(hDlg, IDC_WORD_WRAP);
      UINT wrapWidth = GetDlgItemInt(hDlg, IDC_WRAP_WIDTH, NULL, FALSE);
      BOOL caretBlinking = IsDlgButtonChecked(hDlg, IDC_CARET_BLINKING);
      int caretStyle =
          (int)SendDlgItemMessage(hDlg, IDC_CARET_STYLE, CB_GETCURSEL, 0, 0);
      int lang =
          (int)SendMessage(GetDlgItem(hDlg, IDC_LANGUAGE), CB_GETCURSEL, 0, 0);
      int logLevel =
          (int)SendMessage(GetDlgItem(hDlg, IDC_LOG_LEVEL), CB_GETCURSEL, 0, 0);

      if (g_renderer) {
        g_renderer->SetFont(fontFamily, (float)fontSize,
                            static_cast<DWRITE_FONT_WEIGHT>(fontWeight));
        g_renderer->SetEnableLigatures(enableLigatures == BST_CHECKED);
        g_renderer->SetShowLineNumbers(showLineNums == BST_CHECKED);
        g_renderer->SetWordWrap(wordWrap == BST_CHECKED);
        g_renderer->SetWrapWidth((float)wrapWidth);
        g_renderer->SetCaretStyle((EditorBufferRenderer::CaretStyle)caretStyle);
        g_renderer->SetCaretBlinking(caretBlinking == BST_CHECKED);
      }

      Localization::Instance().SetLanguage((Language)lang);
      g_currentLogLevel = logLevel;
      SettingsManager::Instance().SetLogLevel(logLevel);
      SettingsManager::Instance().SetCaretBlinking(caretBlinking ==
                                                   BST_CHECKED);

      wchar_t bashPath[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_BASH_PATH, bashPath, MAX_PATH);
      SettingsManager::Instance().SetBashPath(bashPath);

      wchar_t defExt[64];
      GetDlgItemTextW(hDlg, IDC_DEFAULT_EXT, defExt, 64);
      SettingsManager::Instance().SetDefaultExtension(defExt);

      wchar_t pluginDir[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_PLUGINS_DIR, pluginDir, MAX_PATH);
      SettingsManager::Instance().SetPluginsDirectory(pluginDir);

      SettingsManager::Instance().Save();
    }
    break;
  }
  return (INT_PTR)FALSE;
}

INT_PTR CALLBACK AiSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    if (g_scriptEngine) {
      // Load servers list
      std::string servers = g_scriptEngine->Evaluate(
          "Object.keys(getAiConfig().servers).join(',')");
      if (!servers.empty() && servers.find("Error") != 0) {
        HWND hCombo = GetDlgItem(hDlg, IDC_AI_SERVER);
        size_t start = 0, end;
        while ((end = servers.find(',', start)) != std::string::npos) {
          std::string s = servers.substr(start, end - start);
          SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)s.c_str());
          start = end + 1;
        }
        SendMessageA(hCombo, CB_ADDSTRING, 0,
                     (LPARAM)servers.substr(start).c_str());

        std::string activeServer =
            g_scriptEngine->Evaluate("getAiConfig().activeServer");
        int idx = (int)SendMessageA(hCombo, CB_FINDSTRINGEXACT, -1,
                                    (LPARAM)activeServer.c_str());
        SendMessage(hCombo, CB_SETCURSEL, idx, 0);

        // Load active server details
        std::string model =
            g_scriptEngine->Evaluate("getActiveServer().model || ''");
        std::string base = g_scriptEngine->Evaluate(
            "getActiveServer().apiBase || getActiveServer().url || ''");
        std::string key =
            g_scriptEngine->Evaluate("getActiveServer().apiKey || ''");
        std::string projectDir =
            g_scriptEngine->Evaluate("getAiConfig().allowedProjectDir || ''");

        SetDlgItemTextA(hDlg, IDC_AI_MODEL, model.c_str());
        SetDlgItemTextA(hDlg, IDC_AI_BASE_URL, base.c_str());
        SetDlgItemTextA(hDlg, IDC_AI_KEY, key.c_str());
        SetDlgItemTextA(hDlg, IDC_AI_PROJECT_DIR, projectDir.c_str());
      }
    }
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_AI_SERVER) {
      char server[64];
      GetDlgItemTextA(hDlg, IDC_AI_SERVER, server, 64);
      g_scriptEngine->Evaluate("switchAiServer('" + std::string(server) + "')");
      // Reload fields
      std::string model =
          g_scriptEngine->Evaluate("getActiveServer().model || ''");
      std::string base = g_scriptEngine->Evaluate(
          "getActiveServer().apiBase || getActiveServer().url || ''");
      std::string key =
          g_scriptEngine->Evaluate("getActiveServer().apiKey || ''");
      SetDlgItemTextA(hDlg, IDC_AI_MODEL, model.c_str());
      SetDlgItemTextA(hDlg, IDC_AI_BASE_URL, base.c_str());
      SetDlgItemTextA(hDlg, IDC_AI_KEY, key.c_str());
    } else if (LOWORD(wParam) == IDC_AI_PROJECT_DIR_BROWSE) {
      std::wstring path = Dialogs::BrowseForFolder(hDlg);
      if (!path.empty()) {
        SetDlgItemTextW(hDlg, IDC_AI_PROJECT_DIR, path.c_str());
      }
    } else if (LOWORD(wParam) == IDOK) {
      char model[128], base[256], key[256], projectDir[MAX_PATH];
      GetDlgItemTextA(hDlg, IDC_AI_MODEL, model, 128);
      GetDlgItemTextA(hDlg, IDC_AI_BASE_URL, base, 256);
      GetDlgItemTextA(hDlg, IDC_AI_KEY, key, 256);
      GetDlgItemTextA(hDlg, IDC_AI_PROJECT_DIR, projectDir, MAX_PATH);

      std::string js = "(function() { var config = getAiConfig(); var s = "
                       "getActiveServer(); "
                       "s.model = '" +
                       std::string(model) + "'; ";
      std::string sBase(base);
      if (sBase.find("/chat/completions") != std::string::npos) {
        js += "s.url = '" + sBase + "'; delete s.apiBase; ";
      } else {
        js += "s.apiBase = '" + sBase + "'; delete s.url; ";
      }
      js += "s.apiKey = '" + std::string(key) + "'; ";

      std::string sProjectDir(projectDir);
      // Escape backslashes for JS string
      size_t pos = 0;
      while ((pos = sProjectDir.find('\\', pos)) != std::string::npos) {
        sProjectDir.replace(pos, 1, "\\\\");
        pos += 2;
      }
      js += "config.allowedProjectDir = '" + sProjectDir + "'; ";

      js += "saveAiConfig(config); })();";
      g_scriptEngine->Evaluate(js);
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowSettingsDialog(HWND hwnd) {
  if (DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_SETTINGS), hwnd,
                 SettingsDlgProc) == IDOK) {
    InvalidateRect(hwnd, NULL, FALSE);
    // Force menu update? We might need a better way to trigger menu update from
    // here.
    SendMessage(hwnd, WM_COMMAND,
                601 + (int)Localization::Instance().GetCurrentLanguage(), 0);
  }
}

INT_PTR CALLBACK AISettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    HWND hVendor = GetDlgItem(hDlg, IDC_AI_VENDOR);
    SendMessage(hVendor, CB_ADDSTRING, 0, (LPARAM)L"Gemini");
    SendMessage(hVendor, CB_ADDSTRING, 0, (LPARAM)L"OpenAI");
    SendMessage(hVendor, CB_ADDSTRING, 0, (LPARAM)L"Anthropic");

    std::wstring currVendor = SettingsManager::Instance().GetAIVendor();
    if (currVendor.empty()) currVendor = L"Gemini";
    
    int selIndex = 0;
    if (currVendor == L"OpenAI") selIndex = 1;
    else if (currVendor == L"Anthropic") selIndex = 2;
    SendMessage(hVendor, CB_SETCURSEL, selIndex, 0);

    HWND hModel = GetDlgItem(hDlg, IDC_AI_MODEL);
    if (currVendor == L"Gemini") {
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gemini-1.5-pro");
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gemini-1.5-flash");
    } else if (currVendor == L"OpenAI") {
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gpt-4o");
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gpt-4-turbo");
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gpt-3.5-turbo");
    } else if (currVendor == L"Anthropic") {
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"claude-3-opus-20240229");
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"claude-3-sonnet-20240229");
      SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"claude-3-haiku-20240307");
    }
    
    std::wstring currModel = SettingsManager::Instance().GetAIModel();
    int modelIndex = (int)SendMessage(hModel, CB_FINDSTRINGEXACT, -1, (LPARAM)currModel.c_str());
    if (modelIndex == CB_ERR) {
      if (currVendor == L"Gemini") currModel = L"gemini-1.5-pro";
      else if (currVendor == L"OpenAI") currModel = L"gpt-4o";
      else if (currVendor == L"Anthropic") currModel = L"claude-3-opus-20240229";
      modelIndex = (int)SendMessage(hModel, CB_FINDSTRINGEXACT, -1, (LPARAM)currModel.c_str());
    }
    SendMessage(hModel, CB_SETCURSEL, (std::max)(0, modelIndex), 0);
    
    std::wstring apiKey = SettingsManager::Instance().GetAIApiKey(currVendor);
    SetDlgItemTextW(hDlg, IDC_AI_API_KEY, apiKey.c_str());

    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_AI_VENDOR) {
      // Handle vendor change
      HWND hVendor = GetDlgItem(hDlg, IDC_AI_VENDOR);
      int selIndex = (int)SendMessage(hVendor, CB_GETCURSEL, 0, 0);
      wchar_t vendorStr[256] = {0};
      SendMessage(hVendor, CB_GETLBTEXT, selIndex, (LPARAM)vendorStr);
      std::wstring vendor(vendorStr);

      HWND hModel = GetDlgItem(hDlg, IDC_AI_MODEL);
      SendMessage(hModel, CB_RESETCONTENT, 0, 0);

      // Save previous key to manager before switching (so we don't lose typed context before IDOK)
      // Actually simpler: just re-pull existing from settings. Changing fields drops typed unsaved changes.
      std::wstring apiKey = SettingsManager::Instance().GetAIApiKey(vendor);
      SetDlgItemTextW(hDlg, IDC_AI_API_KEY, apiKey.c_str());

      if (vendor == L"Gemini") {
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gemini-1.5-pro");
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gemini-1.5-flash");
      } else if (vendor == L"OpenAI") {
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gpt-4o");
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gpt-4-turbo");
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"gpt-3.5-turbo");
      } else if (vendor == L"Anthropic") {
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"claude-3-opus-20240229");
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"claude-3-sonnet-20240229");
        SendMessage(hModel, CB_ADDSTRING, 0, (LPARAM)L"claude-3-haiku-20240307");
      }
      SendMessage(hModel, CB_SETCURSEL, 0, 0);
      
      return (INT_PTR)TRUE;
    }
    
    if (LOWORD(wParam) == IDOK) {
      HWND hVendor = GetDlgItem(hDlg, IDC_AI_VENDOR);
      int vendorIndex = (int)SendMessage(hVendor, CB_GETCURSEL, 0, 0);
      wchar_t vendorStr[256] = {0};
      SendMessage(hVendor, CB_GETLBTEXT, vendorIndex, (LPARAM)vendorStr);
      
      HWND hModel = GetDlgItem(hDlg, IDC_AI_MODEL);
      int modelIndex = (int)SendMessage(hModel, CB_GETCURSEL, 0, 0);
      wchar_t modelStr[256] = {0};
      SendMessage(hModel, CB_GETLBTEXT, modelIndex, (LPARAM)modelStr);
      
      wchar_t keyStr[1024] = {0};
      GetDlgItemTextW(hDlg, IDC_AI_API_KEY, keyStr, 1024);
      
      SettingsManager::Instance().SetAIVendor(vendorStr);
      SettingsManager::Instance().SetAIModel(modelStr);
      SettingsManager::Instance().SetAIApiKey(vendorStr, keyStr);
      SettingsManager::Instance().Save();
      
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowAISettingsDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_AI_SETTINGS), hwnd,
             AISettingsDlgProc);
  InvalidateRect(hwnd, NULL, FALSE);
}

void Dialogs::ShowFindReplaceDialog(HWND hwnd, bool replaceMode) {
  // Find/Replace dialog placeholder
  std::wstring type = replaceMode ? L"Replace" : L"Find";
  MessageBoxW(hwnd, (type + L" Dialog - Under Construction").c_str(),
              type.c_str(), MB_OK);
}

INT_PTR CALLBACK FindFileDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    SetDlgItemTextW(hDlg, IDC_FILE_SEARCH_DIR, L".");
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_FILE_SEARCH_BROWSE) {
      std::wstring path = Dialogs::BrowseForFolder(hDlg);
      if (!path.empty()) SetDlgItemTextW(hDlg, IDC_FILE_SEARCH_DIR, path.c_str());
      return (INT_PTR)TRUE;
    }
    if (LOWORD(wParam) == IDOK) {
      wchar_t pattern[256], dir[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_FIND_PATTERN, pattern, 256);
      GetDlgItemTextW(hDlg, IDC_FILE_SEARCH_DIR, dir, MAX_PATH);
      if (g_editor && wcslen(pattern) > 0) {
        g_editor->FindFile(pattern, dir);
      }
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

INT_PTR CALLBACK FindInFilesDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    std::wstring findStart = SettingsManager::Instance().GetFindStartDirectory();
    if (findStart.empty()) findStart = L".";
    SetDlgItemTextW(hDlg, IDC_FIND_DIR, findStart.c_str());
    CheckDlgButton(hDlg, IDC_FIND_REGEX, BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_FIND_MATCH_CASE, BST_UNCHECKED);
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      wchar_t pattern[256], dir[MAX_PATH], ext[256];
      GetDlgItemTextW(hDlg, IDC_FIND_PATTERN, pattern, 256);
      GetDlgItemTextW(hDlg, IDC_FIND_DIR, dir, MAX_PATH);
      GetDlgItemTextW(hDlg, IDC_FIND_EXT, ext, 256);
      BOOL useRegex = IsDlgButtonChecked(hDlg, IDC_FIND_REGEX);
      BOOL matchCase = IsDlgButtonChecked(hDlg, IDC_FIND_MATCH_CASE);

      if (g_editor && wcslen(pattern) > 0 && wcslen(dir) > 0) {
        SettingsManager::Instance().SetFindStartDirectory(dir);
        SettingsManager::Instance().Save();

        // Disable controls, start search
        EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_FIND_PATTERN), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_FIND_DIR), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_FIND_EXT), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_FIND_REGEX), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_FIND_MATCH_CASE), FALSE);
        EnableWindow(GetDlgItem(hDlg, IDC_FIND_BROWSE), FALSE);
        SetDlgItemTextW(hDlg, IDCANCEL, L"Stop");

        g_editor->FindInFiles(dir, pattern, ext, useRegex ? true : false, matchCase ? true : false);

        // Nested message loop while search is active
        MSG msg;
        while (g_grepSearchActive) {
          while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (!IsDialogMessage(hDlg, &msg)) {
              TranslateMessage(&msg);
              DispatchMessage(&msg);
            }
          }
          Sleep(10);
        }
      }
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_FIND_BROWSE) {
      std::wstring path = Dialogs::BrowseForFolder(hDlg);
      if (!path.empty()) {
        SetDlgItemTextW(hDlg, IDC_FIND_DIR, path.c_str());
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      if (g_grepSearchActive) {
        g_editor->CancelFindInFiles();
        // Don't call EndDialog here; the nested IDOK loop will exit and call it
      } else {
        EndDialog(hDlg, IDCANCEL);
      }
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowFindInFilesDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_FIND_IN_FILES), hwnd,
             FindInFilesDlgProc);
}

void Dialogs::ShowFindFileDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_FIND_FILE), hwnd,
             FindFileDlgProc);
}

static void RefreshCliList(HWND hDlg) {
  HWND hList = GetDlgItem(hDlg, IDC_CLI_LIST);
  SendMessage(hList, LB_RESETCONTENT, 0, 0);
  const auto &entries = SettingsManager::Instance().GetCliEntries();
  for (size_t i = 0; i < entries.size(); ++i) {
    std::wstring enc = entries[i].encoding ? L"SJIS" : L"UTF8";
    std::wstring text = entries[i].command + L"  [" + enc + L"]  →  " + entries[i].folder;
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
  }
}

INT_PTR CALLBACK CliSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    RefreshCliList(hDlg);
    HWND hEnc = GetDlgItem(hDlg, IDC_CLI_ENCODING);
    SendMessage(hEnc, CB_ADDSTRING, 0, (LPARAM)L"UTF-8");
    SendMessage(hEnc, CB_ADDSTRING, 0, (LPARAM)L"Shift-JIS");
    SendMessage(hEnc, CB_SETCURSEL, 0, 0);
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_CLI_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
      HWND hList = GetDlgItem(hDlg, IDC_CLI_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        const auto &entries = SettingsManager::Instance().GetCliEntries();
        if (sel >= 0 && sel < (int)entries.size()) {
          SetDlgItemTextW(hDlg, IDC_CLI_CMD, entries[sel].command.c_str());
          SetDlgItemTextW(hDlg, IDC_CLI_FOLDER, entries[sel].folder.c_str());
          SendDlgItemMessage(hDlg, IDC_CLI_ENCODING, CB_SETCURSEL, entries[sel].encoding, 0);
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_CLI_BROWSE) {
      std::wstring path = Dialogs::BrowseForFolder(hDlg);
      if (!path.empty()) {
        SetDlgItemTextW(hDlg, IDC_CLI_FOLDER, path.c_str());
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_CLI_ADD) {
      wchar_t cmd[1024], folder[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_CLI_CMD, cmd, 1024);
      GetDlgItemTextW(hDlg, IDC_CLI_FOLDER, folder, MAX_PATH);
      int enc = (int)SendDlgItemMessage(hDlg, IDC_CLI_ENCODING, CB_GETCURSEL, 0, 0);
      if (wcslen(cmd) > 0 && wcslen(folder) > 0) {
        SettingsManager::Instance().AddCliEntry(cmd, folder, enc);
        SettingsManager::Instance().Save();
        RefreshCliList(hDlg);
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_CLI_DUPLICATE) {
      HWND hList = GetDlgItem(hDlg, IDC_CLI_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        const auto &entries = SettingsManager::Instance().GetCliEntries();
        if (sel >= 0 && sel < (int)entries.size()) {
          auto &entry = entries[sel];
          SettingsManager::Instance().AddCliEntry(entry.command, entry.folder, entry.encoding);
          SettingsManager::Instance().Save();
          RefreshCliList(hDlg);
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_CLI_REMOVE) {
      HWND hList = GetDlgItem(hDlg, IDC_CLI_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        SettingsManager::Instance().RemoveCliEntry(sel);
        SettingsManager::Instance().Save();
        RefreshCliList(hDlg);
        SetDlgItemTextW(hDlg, IDC_CLI_CMD, L"");
        SetDlgItemTextW(hDlg, IDC_CLI_FOLDER, L"");
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_CLI_RUN) {
      wchar_t cmd[1024], folder[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_CLI_CMD, cmd, 1024);
      GetDlgItemTextW(hDlg, IDC_CLI_FOLDER, folder, MAX_PATH);
      int enc = (int)SendDlgItemMessage(hDlg, IDC_CLI_ENCODING, CB_GETCURSEL, 0, 0);
      if (wcslen(cmd) == 0 || wcslen(folder) == 0) {
        HWND hList = GetDlgItem(hDlg, IDC_CLI_LIST);
        int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR) {
          const auto &entries = SettingsManager::Instance().GetCliEntries();
          if (sel >= 0 && sel < (int)entries.size()) {
            wcscpy_s(cmd, 1024, entries[sel].command.c_str());
            wcscpy_s(folder, MAX_PATH, entries[sel].folder.c_str());
            enc = entries[sel].encoding;
          }
        }
      }
      // Validate inputs
      if (wcslen(cmd) == 0) {
        MessageBoxW(hDlg, L"Command cannot be empty.", L"Error", MB_ICONERROR);
        return (INT_PTR)TRUE;
      }
      if (wcslen(folder) == 0) {
        MessageBoxW(hDlg, L"Folder cannot be empty.", L"Error", MB_ICONERROR);
        return (INT_PTR)TRUE;
      }
      DWORD folderAttr = GetFileAttributesW(folder);
      if (folderAttr == INVALID_FILE_ATTRIBUTES || !(folderAttr & FILE_ATTRIBUTE_DIRECTORY)) {
        MessageBoxW(hDlg, L"Folder does not exist.", L"Error", MB_ICONERROR);
        return (INT_PTR)TRUE;
      }
      std::wstring bashDir;
      std::wstring bashCmd = SettingsManager::Instance().GetBashCommand(&bashDir);
      if (bashCmd.empty()) {
        MessageBoxW(hDlg, L"Git Bash is not installed. Configure Bash Path in Settings.", L"Error", MB_ICONERROR);
        return (INT_PTR)TRUE;
      }
      HWND parent = GetParent(hDlg);
      SetCurrentDirectoryW(folder);
      std::wstring localePrefix = (enc == 1) ? L"export LANG=ja_JP.UTF-8; " : L"export LANG=en_US.UTF-8; ";
      std::wstring cmdLine = bashCmd + L" -c \"" + localePrefix + cmd + L"; exec bash --login -i\"";
      std::wstring label = std::wstring(folder);
      size_t pos = label.find_last_of(L"\\/");
      if (pos != std::wstring::npos) label = label.substr(pos + 1);
      if (label.empty()) label = L"CLI";
      CreateNewTerminal(parent, cmdLine, label);
      EndDialog(hDlg, IDOK);
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

static void RefreshDpList(HWND hDlg) {
  HWND hList = GetDlgItem(hDlg, IDC_DP_LIST);
  SendMessage(hList, LB_RESETCONTENT, 0, 0);
  const auto &pairs = SettingsManager::Instance().GetDiredPairs();
  for (size_t i = 0; i < pairs.size(); ++i) {
    std::wstring text = pairs[i].label + L"  →  " + pairs[i].leftDir + L" | " + pairs[i].rightDir;
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
  }
}

static void OpenDiredPair(const DiredPair &pair) {
  if (pair.leftDir.empty() && pair.rightDir.empty()) return;
  // Find Dired.exe alongside ecode.exe
  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  std::wstring dir = modulePath;
  size_t pos = dir.find_last_of(L"\\/");
  if (pos != std::wstring::npos) dir = dir.substr(0, pos + 1);
  std::wstring diredPath = dir + L"Dired.exe";
  if (GetFileAttributesW(diredPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    diredPath = L"Dired.exe";

  std::wstring cmd = L"\"" + diredPath + L"\" \"" + pair.leftDir + L"\"";
  if (!pair.rightDir.empty())
    cmd += L" \"" + pair.rightDir + L"\"";

  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi;
  CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

INT_PTR CALLBACK DiredPairsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    RefreshDpList(hDlg);
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_DP_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
      HWND hList = GetDlgItem(hDlg, IDC_DP_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        const auto &pairs = SettingsManager::Instance().GetDiredPairs();
        if (sel >= 0 && sel < (int)pairs.size()) {
          SetDlgItemTextW(hDlg, IDC_DP_LABEL, pairs[sel].label.c_str());
          SetDlgItemTextW(hDlg, IDC_DP_LEFT, pairs[sel].leftDir.c_str());
          SetDlgItemTextW(hDlg, IDC_DP_RIGHT, pairs[sel].rightDir.c_str());
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_DP_LEFT_BROWSE) {
      std::wstring p = Dialogs::BrowseForFolder(hDlg);
      if (!p.empty()) SetDlgItemTextW(hDlg, IDC_DP_LEFT, p.c_str());
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_DP_RIGHT_BROWSE) {
      std::wstring p = Dialogs::BrowseForFolder(hDlg);
      if (!p.empty()) SetDlgItemTextW(hDlg, IDC_DP_RIGHT, p.c_str());
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_DP_ADD) {
      wchar_t label[256], left[MAX_PATH], right[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_DP_LABEL, label, 256);
      GetDlgItemTextW(hDlg, IDC_DP_LEFT, left, MAX_PATH);
      GetDlgItemTextW(hDlg, IDC_DP_RIGHT, right, MAX_PATH);
      if (wcslen(label) > 0 && wcslen(left) > 0) {
        SettingsManager::Instance().AddDiredPair(label, left, right);
        SettingsManager::Instance().Save();
        RefreshDpList(hDlg);
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_DP_DUPLICATE) {
      HWND hList = GetDlgItem(hDlg, IDC_DP_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        const auto &pairs = SettingsManager::Instance().GetDiredPairs();
        if (sel >= 0 && sel < (int)pairs.size()) {
          auto &p = pairs[sel];
          SettingsManager::Instance().AddDiredPair(p.label, p.leftDir, p.rightDir);
          SettingsManager::Instance().Save();
          RefreshDpList(hDlg);
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_DP_REMOVE) {
      HWND hList = GetDlgItem(hDlg, IDC_DP_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        SettingsManager::Instance().RemoveDiredPair(sel);
        SettingsManager::Instance().Save();
        RefreshDpList(hDlg);
        SetDlgItemTextW(hDlg, IDC_DP_LABEL, L"");
        SetDlgItemTextW(hDlg, IDC_DP_LEFT, L"");
        SetDlgItemTextW(hDlg, IDC_DP_RIGHT, L"");
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_DP_OPEN) {
      wchar_t label[256], left[MAX_PATH], right[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_DP_LABEL, label, 256);
      GetDlgItemTextW(hDlg, IDC_DP_LEFT, left, MAX_PATH);
      GetDlgItemTextW(hDlg, IDC_DP_RIGHT, right, MAX_PATH);
      if (wcslen(label) > 0 && wcslen(left) > 0) {
        OpenDiredPair({label, left, right});
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowDiredPairsDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_DIRED_PAIRS), hwnd,
             DiredPairsDlgProc);
}

void ShowCliDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_CLI_SETTINGS), hwnd,
             CliSettingsDlgProc);
}

static void RefreshThemeList(HWND hDlg) {
  HWND hList = GetDlgItem(hDlg, IDC_TH_LIST);
  SendMessage(hList, LB_RESETCONTENT, 0, 0);
  const auto &themes = SettingsManager::Instance().GetThemes();
  for (size_t i = 0; i < themes.size(); ++i) {
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)themes[i].name.c_str());
  }
}

static void ThemeEntryToFields(HWND hDlg, const ThemeEntry &te) {
  SetDlgItemTextW(hDlg, IDC_TH_NAME, te.name.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_BG, te.background.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_FG, te.foreground.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_CARET, te.caret.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_SEL, te.selection.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_LN, te.lineNumbers.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_KW, te.keyword.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_STR, te.string.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_NUM, te.number.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_CMT, te.comment.c_str());
  SetDlgItemTextW(hDlg, IDC_TH_FUNC, te.function.c_str());
}

static ThemeEntry FieldsToThemeEntry(HWND hDlg) {
  ThemeEntry te;
  wchar_t buf[256];
  auto GetHex = [&](int id) -> std::wstring {
    GetDlgItemTextW(hDlg, id, buf, 256);
    return buf;
  };
  te.name = GetHex(IDC_TH_NAME);
  te.background = GetHex(IDC_TH_BG);
  te.foreground = GetHex(IDC_TH_FG);
  te.caret = GetHex(IDC_TH_CARET);
  te.selection = GetHex(IDC_TH_SEL);
  te.lineNumbers = GetHex(IDC_TH_LN);
  te.keyword = GetHex(IDC_TH_KW);
  te.string = GetHex(IDC_TH_STR);
  te.number = GetHex(IDC_TH_NUM);
  te.comment = GetHex(IDC_TH_CMT);
  te.function = GetHex(IDC_TH_FUNC);
  return te;
}

INT_PTR CALLBACK ThemeManagerDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    RefreshThemeList(hDlg);
    // Select first theme
    const auto &themes = SettingsManager::Instance().GetThemes();
    if (!themes.empty()) {
      SendMessage(GetDlgItem(hDlg, IDC_TH_LIST), LB_SETCURSEL, 0, 0);
      ThemeEntryToFields(hDlg, themes[0]);
    }
    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_TH_LIST && HIWORD(wParam) == LBN_SELCHANGE) {
      HWND hList = GetDlgItem(hDlg, IDC_TH_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        const auto &themes = SettingsManager::Instance().GetThemes();
        if (sel >= 0 && sel < (int)themes.size()) {
          ThemeEntryToFields(hDlg, themes[sel]);
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_TH_ADD) {
      ThemeEntry te = FieldsToThemeEntry(hDlg);
      if (te.name.empty()) {
        te.name = L"New Theme";
      }
      SettingsManager::Instance().AddTheme(te);
      SettingsManager::Instance().SaveThemes();
      RefreshThemeList(hDlg);
      // Select the newly added theme
      HWND hList = GetDlgItem(hDlg, IDC_TH_LIST);
      int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
      if (count > 0) {
        SendMessage(hList, LB_SETCURSEL, count - 1, 0);
        const auto &themes = SettingsManager::Instance().GetThemes();
        ThemeEntryToFields(hDlg, themes.back());
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_TH_DUPLICATE) {
      HWND hList = GetDlgItem(hDlg, IDC_TH_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        ThemeEntry te = FieldsToThemeEntry(hDlg);
        if (te.name.empty()) te.name = L"Copy";
        else te.name = te.name + L" (Copy)";
        SettingsManager::Instance().AddTheme(te);
        SettingsManager::Instance().SaveThemes();
        RefreshThemeList(hDlg);
        int count = (int)SendMessage(hList, LB_GETCOUNT, 0, 0);
        if (count > 0) {
          SendMessage(hList, LB_SETCURSEL, count - 1, 0);
          const auto &themes = SettingsManager::Instance().GetThemes();
          ThemeEntryToFields(hDlg, themes.back());
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_TH_REMOVE) {
      HWND hList = GetDlgItem(hDlg, IDC_TH_LIST);
      int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
      if (sel != LB_ERR) {
        SettingsManager::Instance().RemoveTheme(sel);
        SettingsManager::Instance().SaveThemes();
        RefreshThemeList(hDlg);
        const auto &themes = SettingsManager::Instance().GetThemes();
        if (!themes.empty()) {
          SendMessage(hList, LB_SETCURSEL, 0, 0);
          ThemeEntryToFields(hDlg, themes[0]);
        } else {
          SetDlgItemTextW(hDlg, IDC_TH_NAME, L"");
          for (int id = IDC_TH_BG; id <= IDC_TH_FUNC; ++id)
            SetDlgItemTextW(hDlg, id, L"");
        }
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDC_TH_APPLY) {
      ThemeEntry te = FieldsToThemeEntry(hDlg);
      if (!te.name.empty()) {
        // Update the theme entry in settings if it exists
        const auto &themes = SettingsManager::Instance().GetThemes();
        for (size_t i = 0; i < themes.size(); ++i) {
          if (themes[i].name == te.name) {
            // Remove and re-add with updated values
            SettingsManager::Instance().RemoveTheme(i);
            break;
          }
        }
        SettingsManager::Instance().AddTheme(te);
        SettingsManager::Instance().SetActiveThemeName(te.name);
        SettingsManager::Instance().SaveThemes();
        SettingsManager::Instance().Save();
        RefreshThemeList(hDlg);

        // Apply to renderer
        if (g_renderer) {
          Theme theme;
          theme.name = te.name;
          auto HexToColor = [](const std::wstring &hex) -> D2D1_COLOR_F {
            unsigned int r = 0, g = 0, b = 0, a = 255;
            if (hex.length() >= 8) {
              swscanf_s(hex.c_str(), L"%02x%02x%02x%02x", &r, &g, &b, &a);
            } else if (hex.length() >= 6) {
              swscanf_s(hex.c_str(), L"%02x%02x%02x", &r, &g, &b);
            }
            return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
          };
          theme.background = HexToColor(te.background);
          theme.foreground = HexToColor(te.foreground);
          theme.caret = HexToColor(te.caret);
          theme.selection = HexToColor(te.selection);
          theme.lineNumbers = HexToColor(te.lineNumbers);
          theme.keyword = HexToColor(te.keyword);
          theme.string = HexToColor(te.string);
          theme.number = HexToColor(te.number);
          theme.comment = HexToColor(te.comment);
          theme.function = HexToColor(te.function);
          g_renderer->SetTheme(theme);
        }
        // Trigger repaint
        HWND parent = GetParent(hDlg);
        if (parent) InvalidateRect(parent, NULL, FALSE);
      }
      return (INT_PTR)TRUE;
    } else if (LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return (INT_PTR)TRUE;
    }
    break;
  }
  return (INT_PTR)FALSE;
}

void Dialogs::ShowThemeManagerDialog(HWND hwnd) {
  DialogBoxW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_THEME_MANAGER), hwnd,
             ThemeManagerDlgProc);
}

Dialogs::ConfirmationResult
Dialogs::ShowSaveConfirmationDialog(HWND hwnd, const std::wstring &filename) {
  std::wstring name = filename.empty() ? L"Untitled" : filename;
  std::wstring msg = L"Do you want to save changes to " + name + L"?";
  int result =
      MessageBoxW(hwnd, msg.c_str(), L"Ecode", MB_YESNOCANCEL | MB_ICONWARNING);
  if (result == IDYES)
    return ConfirmationResult::Save;
  if (result == IDNO)
    return ConfirmationResult::Discard;
  return ConfirmationResult::Cancel;
}

std::wstring Dialogs::BrowseForFolder(HWND hwnd) {
  std::wstring folderPath;
  IFileOpenDialog *pfd = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
  if (SUCCEEDED(hr)) {
    DWORD dwOptions;
    if (SUCCEEDED(hr = pfd->GetOptions(&dwOptions))) {
      pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
    }
    hr = pfd->Show(hwnd);
    if (SUCCEEDED(hr)) {
      IShellItem *psi = nullptr;
      if (SUCCEEDED(hr = pfd->GetResult(&psi))) {
        PWSTR pszPath = nullptr;
        if (SUCCEEDED(hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
          folderPath = pszPath;
          CoTaskMemFree(pszPath);
        } else {
          DebugLog("BrowseForFolder - GetDisplayName failed: hr=" +
                       std::to_string(hr),
                   LOG_ERROR);
        }
        psi->Release();
      } else {
        DebugLog("BrowseForFolder - GetResult failed: hr=" + std::to_string(hr),
                 LOG_ERROR);
      }
    } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
      DebugLog("BrowseForFolder - pfd->Show failed: hr=" + std::to_string(hr),
               LOG_ERROR);
    }
    pfd->Release();
  } else {
    DebugLog("BrowseForFolder - CoCreateInstance failed: hr=" +
                 std::to_string(hr),
             LOG_ERROR);
  }
  return folderPath;
}
