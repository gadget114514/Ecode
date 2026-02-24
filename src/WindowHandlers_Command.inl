// =============================================================================
// WindowHandlers_Command.inl
// Handle WM_COMMAND (menu items, buttons, etc)
// Included by main.cpp
// =============================================================================

#include "../include/Dialogs.h"
#include "../include/StringHelpers.h"
static LRESULT HandleCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
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
      SettingsManager::Instance().AddRecentFile(path);
      UpdateMenu(hwnd);
    }
    break;
  }
  case IDM_FILE_SAVE: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      if (buf->GetPath().empty()) {
        std::wstring path = Dialogs::SaveFileDialog(hwnd);
        if (!path.empty()) {
          if (buf->SaveFile(path)) {
            g_editor->LogMessage("Saved file: " + StringHelpers::Utf16ToUtf8(path));
          }
        }
      } else {
        if (buf->SaveFile(buf->GetPath())) {
          g_editor->LogMessage("Saved file: " + StringHelpers::Utf16ToUtf8(buf->GetPath()));
        }
      }
    }
    break;
  }
  case IDM_FILE_SAVE_AS: {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf) {
      std::wstring path = Dialogs::SaveFileDialog(hwnd);
      if (!path.empty()) {
        if (buf->SaveFile(path)) {
          g_editor->LogMessage("Saved file: " + StringHelpers::Utf16ToUtf8(path));
        }
        SettingsManager::Instance().AddRecentFile(path);
        UpdateMenu(hwnd);
      }
    }
    break;
  }
  case IDM_HELP_DOC: {
      char buffer[MAX_PATH];
      GetModuleFileNameA(NULL, buffer, MAX_PATH);
      std::string exePath(buffer);
      std::string dir = exePath.substr(0, exePath.find_last_of("\\/"));
      
      std::string filename = "documentation.md";
      Language lang = Localization::Instance().GetCurrentLanguage();
      if (lang == Language::Japanese) filename = "documentation_jp.md";
      else if (lang == Language::Spanish) filename = "documentation_es.md";
      else if (lang == Language::French) filename = "documentation_fr.md";
      else if (lang == Language::German) filename = "documentation_de.md";
      
      std::string docPath = dir + "/doc/" + filename;
      
      std::ifstream f(docPath);
      if (!f.good()) {
          // Try dev path
          std::string devPath = "d:/ws/Ecode/doc/" + filename;
          std::ifstream f2(devPath);
          if (f2.good()) {
              docPath = devPath;
          } else {
              DebugLog("HandleCommand - Documentation file not found: " + filename, LOG_ERROR);
              MessageBoxA(hwnd, ("Could not find documentation file: " + filename).c_str(), "Error", MB_ICONERROR);
          }
      }
      
      g_editor->OpenFile(StringToWString(docPath));
      break;
  }

  case IDM_HELP_ABOUT:
    Dialogs::ShowAboutDialog(hwnd);
    break;

  case IDM_HELP_KEYBINDINGS: {
      char buffer[MAX_PATH];
      GetModuleFileNameA(NULL, buffer, MAX_PATH);
      std::string exePath(buffer);
      std::string dir = exePath.substr(0, exePath.find_last_of("\\/"));
      
      std::string filename = "keybindings.md";
      Language lang = Localization::Instance().GetCurrentLanguage();
      if (lang == Language::Japanese) filename = "keybindings_jp.md";
      else if (lang == Language::Spanish) filename = "keybindings_es.md";
      else if (lang == Language::French) filename = "keybindings_fr.md";
      else if (lang == Language::German) filename = "keybindings_de.md";
      
      std::string docPath = dir + "/doc/" + filename;
      
      std::ifstream f(docPath);
      if (!f.good()) {
          // Try dev path
          std::string devPath = "d:/ws/Ecode/doc/" + filename;
          std::ifstream f2(devPath);
          if (f2.good()) {
              docPath = devPath;
          } else {
              DebugLog("HandleCommand - Keybindings file not found: " + filename, LOG_ERROR);
              MessageBoxA(hwnd, ("Could not find keybindings file: " + filename).c_str(), "Error", MB_ICONERROR);
          }
      }
      
      g_editor->OpenFile(StringToWString(docPath));
      break;
  }

  case IDM_FILE_CLOSE:
    if (g_editor->GetActiveBuffer()) {
      if (!PromptSaveBuffer(hwnd, g_editor->GetActiveBuffer()))
        break;
      g_editor->CloseBuffer(g_editor->GetActiveBufferIndex());
      UpdateMenu(hwnd);
    }
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
  case IDM_EDIT_TAG_JUMP:
    g_editor->TagJump();
    break;
  case IDM_EDIT_FIND: {
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = hwnd;
    g_fr.lpstrFindWhat = g_szFindWhat;
    g_fr.wFindWhatLen = 256;
    g_hDlgFind = FindTextW(&g_fr);
    break;
  }
  case IDM_EDIT_GOTO:
    Dialogs::ShowJumpToLineDialog(hwnd);
    break;
  case IDM_EDIT_FIND_IN_FILES:
    Dialogs::ShowFindInFilesDialog(hwnd);
    break;
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
  case IDM_VIEW_TOGGLE_UI:
    MessageBox(hwnd, L"Toggle UI - Under Construction", L"Info", MB_OK);
    break;
  case IDM_VIEW_TOGGLE_TREE:
    g_treeVisible = !g_treeVisible;
    ShowWindow(g_treeHwnd, g_treeVisible ? SW_SHOW : SW_HIDE);
    SendMessage(hwnd, WM_SIZE, 0, 0); // Trigger a resize to allocate/reclaim dock space
    InvalidateRect(hwnd, NULL, FALSE);
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
  case IDM_CONFIG_AI_SETTINGS:
    Dialogs::ShowAISettingsDialog(hwnd);
    break;
  case IDM_SHELL_ENC_UTF8:
    SettingsManager::Instance().SetShellEncoding(0);
    SettingsManager::Instance().Save();
    UpdateMenu(hwnd);
    break;
  case IDM_SHELL_ENC_SJIS:
    SettingsManager::Instance().SetShellEncoding(1);
    SettingsManager::Instance().Save();
    UpdateMenu(hwnd);
    break;
  case IDM_TOOLS_MACRO_GALLERY:
    Dialogs::ShowMacroGalleryDialog(hwnd);
    break;
  case IDM_HELP_MESSAGES: {
    Buffer *msgBuf = g_editor->GetBufferByName(L"*Messages*");
    if (msgBuf) {
      const auto &buffers = g_editor->GetBuffers();
      for (size_t i = 0; i < buffers.size(); ++i) {
        if (buffers[i].get() == msgBuf) {
          g_editor->SwitchToBuffer(i);
          UpdateMenu(hwnd);
          break;
        }
      }
    }
    break;
  }
  case IDM_TOOLS_CONSOLE: {
    // Open Scratch Buffer (Duktape Console)
    g_editor->NewFile();
    if (g_editor->GetActiveBuffer()) {
        g_editor->GetActiveBuffer()->SetScratch(true);
        if (g_editor->GetActiveBuffer()->GetTotalLength() == 0) {
            g_editor->GetActiveBuffer()->Insert(0, "// Duktape Console (Scratch Buffer)\n// Press Ctrl+Enter to evaluate JS\n");
        }
    }
    UpdateMenu(hwnd);
    break;
  }
  case IDM_TOOLS_OPEN_SHELL: {
    g_editor->OpenShell(L"cmd.exe");
    UpdateMenu(hwnd);
    break;
  }
  case IDM_TOOLS_AI_CHAT: {
    g_editor->LogMessage("--- Starting AI Session ---");
    // Ensure we attempt to load the script first, if not already loaded by ecodeinit.js
    g_scriptEngine->Evaluate("if(typeof startAISession === 'undefined') Editor.loadScript('scripts/ai_session.js');");
    
    std::string result = g_scriptEngine->Evaluate("if(typeof startAISession !== 'undefined') startAISession(); else Editor.logMessage('AI session macro not found. Define startAISession in your JS.');");
    UpdateMenu(hwnd);
    break;
  }
  case IDM_CONFIG_EDIT_INIT: {
      char buffer[MAX_PATH];
      if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, buffer))) {
          std::string path = std::string(buffer) + "\\Ecode\\ecodeinit.js";
          std::ifstream f(path);
          if (!f.good()) {
              // Create if not exists
              std::ofstream out(path);
              out << "// ecodeinit.js\n// Initial startup script\nEditor.logMessage(\"Loading ecodeinit.js...\");\n\n// Load Emacs bindings by default\ntry {\n    Editor.loadScript(\"scripts/emacs.js\");\n} catch (e) {\n    Editor.logMessage(\"Failed to load emacs.js: \" + e);\n}\n";
              out.close();
          }
          g_editor->OpenFile(StringToWString(path));
      }
      break;
  }

  default:
    if (LOWORD(wParam) >= IDM_BUFFERS_START &&
        LOWORD(wParam) < IDM_BUFFERS_START + 100) {
      g_editor->SwitchToBuffer(LOWORD(wParam) - IDM_BUFFERS_START);
      UpdateMenu(hwnd);
    } else if (LOWORD(wParam) >= IDM_RECENT_START &&
               LOWORD(wParam) < IDM_RECENT_START + 10) {
      size_t index = LOWORD(wParam) - IDM_RECENT_START;
      const auto &recent = SettingsManager::Instance().GetRecentFiles();
      if (index < recent.size()) {
        g_editor->OpenFile(recent[index]);
        SettingsManager::Instance().AddRecentFile(recent[index]);
        UpdateMenu(hwnd);
      }
    }
    break;
  }
  InvalidateRect(hwnd, NULL, FALSE);
  return 0;
}
