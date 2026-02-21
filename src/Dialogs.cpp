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
  ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
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
  ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
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
static HWND g_hDlgSettingsAI = NULL;

INT_PTR CALLBACK GeneralSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                        LPARAM lParam);
INT_PTR CALLBACK AiSettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                   LPARAM lParam);

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    HWND hTab = GetDlgItem(hDlg, IDC_TAB_SETTINGS);
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"General";
    TabCtrl_InsertItem(hTab, 0, &tie);
    tie.pszText = (LPWSTR)L"AI";
    TabCtrl_InsertItem(hTab, 1, &tie);

    g_hDlgSettingsGeneral = CreateDialogW(
        GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_SETTINGS_GENERAL), hDlg,
        GeneralSettingsDlgProc);
    g_hDlgSettingsAI =
        CreateDialogW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDD_SETTINGS_AI),
                      hDlg, AiSettingsDlgProc);

    RECT rcTab;
    GetWindowRect(hTab, &rcTab);
    MapWindowPoints(NULL, hDlg, (LPPOINT)&rcTab, 2);
    TabCtrl_AdjustRect(hTab, FALSE, &rcTab);

    SetWindowPos(g_hDlgSettingsGeneral, HWND_TOP, rcTab.left, rcTab.top,
                 rcTab.right - rcTab.left, rcTab.bottom - rcTab.top,
                 SWP_SHOWWINDOW);
    SetWindowPos(g_hDlgSettingsAI, HWND_TOP, rcTab.left, rcTab.top,
                 rcTab.right - rcTab.left, rcTab.bottom - rcTab.top,
                 SWP_HIDEWINDOW);

    return (INT_PTR)TRUE;
  }
  case WM_NOTIFY: {
    LPNMHDR pnmh = (LPNMHDR)lParam;
    if (pnmh->idFrom == IDC_TAB_SETTINGS && pnmh->code == TCN_SELCHANGE) {
      int sel = TabCtrl_GetCurSel(pnmh->hwndFrom);
      ShowWindow(g_hDlgSettingsGeneral, sel == 0 ? SW_SHOW : SW_HIDE);
      ShowWindow(g_hDlgSettingsAI, sel == 1 ? SW_SHOW : SW_HIDE);
    }
    break;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      SendMessage(g_hDlgSettingsGeneral, WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
      SendMessage(g_hDlgSettingsAI, WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
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

    return (INT_PTR)TRUE;
  }
  case WM_COMMAND:
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

void Dialogs::ShowFindReplaceDialog(HWND hwnd, bool replaceMode) {
  // Find/Replace dialog placeholder
  std::wstring type = replaceMode ? L"Replace" : L"Find";
  MessageBoxW(hwnd, (type + L" Dialog - Under Construction").c_str(),
              type.c_str(), MB_OK);
}

INT_PTR CALLBACK FindInFilesDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG:
    SetDlgItemTextW(hDlg, IDC_FIND_DIR, L".");
    return (INT_PTR)TRUE;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK) {
      wchar_t pattern[256];
      wchar_t dir[MAX_PATH];
      GetDlgItemTextW(hDlg, IDC_FIND_PATTERN, pattern, 256);
      GetDlgItemTextW(hDlg, IDC_FIND_DIR, dir, MAX_PATH);

      if (g_editor) {
        g_editor->FindInFiles(dir, pattern);
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
      EndDialog(hDlg, IDCANCEL);
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
