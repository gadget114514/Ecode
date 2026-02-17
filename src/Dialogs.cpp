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

enum LogLevel { LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARN = 2, LOG_ERROR = 3 };
void DebugLog(const std::string &msg, LogLevel level = LOG_INFO);
std::string GetWin32ErrorString(DWORD errorCode);
#include <objbase.h>
#include <vector>

#if defined(__has_include) && __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif defined(__has_include) && __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
// Fallback or error
#error "No filesystem support found"
#endif

extern std::unique_ptr<EditorBufferRenderer> g_renderer;
extern std::unique_ptr<Editor> g_editor;
extern std::unique_ptr<ScriptEngine> g_scriptEngine;
extern int g_currentLogLevel;

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
        DebugLog("OpenFileDialog failed with extended error: " + std::to_string(err), LOG_ERROR);
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
        DebugLog("SaveFileDialog failed with extended error: " + std::to_string(err), LOG_ERROR);
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

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
  case WM_INITDIALOG: {
    if (g_renderer) {
      SetDlgItemTextW(hDlg, IDC_FONT_FAMILY,
                      g_renderer->GetFontFamily().c_str());
      SetDlgItemInt(hDlg, IDC_FONT_SIZE,
                    static_cast<UINT>(g_renderer->GetFontSize()), FALSE);
      CheckDlgButton(hDlg, IDC_SHOW_LINE_NUMBERS,
                     g_renderer->GetShowLineNumbers() ? BST_CHECKED
                                                      : BST_UNCHECKED);
      CheckDlgButton(hDlg, IDC_WORD_WRAP,
                     g_renderer->GetWordWrap() ? BST_CHECKED : BST_UNCHECKED);
      SetDlgItemInt(hDlg, IDC_WRAP_WIDTH,
                    static_cast<UINT>(g_renderer->GetWrapWidth()), FALSE);
      SetDlgItemInt(hDlg, IDC_FONT_WEIGHT,
                    static_cast<UINT>(g_renderer->GetFontWeight()), FALSE);
      CheckDlgButton(hDlg, IDC_ENABLE_LIGATURES,
                     g_renderer->GetEnableLigatures() ? BST_CHECKED
                                                      : BST_UNCHECKED);

      HWND hCaret = GetDlgItem(hDlg, IDC_CARET_STYLE);
      SendMessage(hCaret, CB_ADDSTRING, 0, (LPARAM)L"Line");
      SendMessage(hCaret, CB_ADDSTRING, 0, (LPARAM)L"Block");
      SendMessage(hCaret, CB_ADDSTRING, 0, (LPARAM)L"Underline");
      SendMessage(hCaret, CB_SETCURSEL,
                  (static_cast<int>(g_renderer->GetCaretStyle())), 0);

      CheckDlgButton(hDlg, IDC_CARET_BLINKING,
                     SettingsManager::Instance().IsCaretBlinking() ? BST_CHECKED
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
      BOOL showLineNums = IsDlgButtonChecked(hDlg, IDC_SHOW_LINE_NUMBERS);
      BOOL showPhysical = IsDlgButtonChecked(hDlg, IDC_SHOW_PHYSICAL_LINE_NUMS);
      BOOL wordWrap = IsDlgButtonChecked(hDlg, IDC_WORD_WRAP);
      UINT wrapWidth = GetDlgItemInt(hDlg, IDC_WRAP_WIDTH, NULL, FALSE);
      UINT fontWeight = GetDlgItemInt(hDlg, IDC_FONT_WEIGHT, NULL, FALSE);
      BOOL enableLigatures = IsDlgButtonChecked(hDlg, IDC_ENABLE_LIGATURES);
      int lang =
          (int)SendMessage(GetDlgItem(hDlg, IDC_LANGUAGE), CB_GETCURSEL, 0, 0);

      if (g_renderer) {
        g_renderer->SetFont(fontFamily, (float)fontSize,
                            static_cast<DWRITE_FONT_WEIGHT>(fontWeight));
        g_renderer->SetEnableLigatures(enableLigatures == BST_CHECKED);
        g_renderer->SetShowLineNumbers(showLineNums == BST_CHECKED);
        // g_renderer->SetShowPhysicalLineNumbers(showPhysical == BST_CHECKED);
        g_renderer->SetWordWrap(wordWrap == BST_CHECKED);
        g_renderer->SetWrapWidth((float)wrapWidth);

        int caretStyle =
            (int)SendDlgItemMessage(hDlg, IDC_CARET_STYLE, CB_GETCURSEL, 0, 0);
        g_renderer->SetCaretStyle((EditorBufferRenderer::CaretStyle)caretStyle);
      }
      BOOL caretBlinking = IsDlgButtonChecked(hDlg, IDC_CARET_BLINKING);
      if (g_renderer) {
        g_renderer->SetCaretBlinking(caretBlinking == BST_CHECKED);
      }

      Localization::Instance().SetLanguage((Language)lang);

      int logLevel =
          (int)SendMessage(GetDlgItem(hDlg, IDC_LOG_LEVEL), CB_GETCURSEL, 0, 0);
      g_currentLogLevel = logLevel;
      SettingsManager::Instance().SetLogLevel(logLevel);
      g_currentLogLevel = logLevel;
      SettingsManager::Instance().SetLogLevel(logLevel);
      SettingsManager::Instance().SetCaretBlinking(caretBlinking == BST_CHECKED);
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
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&pfd));
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
          DebugLog("BrowseForFolder - GetDisplayName failed: hr=" + std::to_string(hr), LOG_ERROR);
        }
        psi->Release();
      } else {
        DebugLog("BrowseForFolder - GetResult failed: hr=" + std::to_string(hr), LOG_ERROR);
      }
    } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
      DebugLog("BrowseForFolder - pfd->Show failed: hr=" + std::to_string(hr), LOG_ERROR);
    }
    pfd->Release();
  } else {
    DebugLog("BrowseForFolder - CoCreateInstance failed: hr=" + std::to_string(hr), LOG_ERROR);
  }
  return folderPath;
}
