#include "EditorWindow.h"
#include "../resources/resource.h"
#include "FileUtils.h"
#include <cctype>
#include <commctrl.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <shlwapi.h>
#include <shobjidl.h>
#include <sstream>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <yaml-cpp/yaml.h>

using json = nlohmann::json;

// -- Child App Embedding Helpers --

struct ChildAppFindInfo {
  DWORD processId;
  HWND foundHwnd;
};

static BOOL CALLBACK FindChildWindowProc(HWND hwnd, LPARAM lParam) {
  ChildAppFindInfo *info = (ChildAppFindInfo *)lParam;
  DWORD pid;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == info->processId && IsWindowVisible(hwnd) &&
      GetWindow(hwnd, GW_OWNER) == NULL) {
    info->foundHwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

struct ChildAppLaunchParams {
  HWND parentHwnd;
  int tabIndex;
  std::wstring exePath;
  std::wstring workingDir;
  HANDLE hProcess;
  DWORD processId;
};

static DWORD WINAPI ChildAppLaunchThread(LPVOID lpParam) {
  ChildAppLaunchParams *params = (ChildAppLaunchParams *)lpParam;

  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi;

  if (!CreateProcessW(params->exePath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL,
                      params->workingDir.c_str(), &si, &pi)) {
    delete params;
    return 1;
  }

  CloseHandle(pi.hThread);
  params->hProcess = pi.hProcess;
  params->processId = pi.dwProcessId;

  // Poll for the child's top-level window
  ChildAppFindInfo findInfo = {pi.dwProcessId, NULL};
  for (int retry = 0; retry < 300 && !findInfo.foundHwnd; retry++) {
    EnumWindows(FindChildWindowProc, (LPARAM)&findInfo);
    if (!findInfo.foundHwnd)
      Sleep(100);
  }

  if (findInfo.foundHwnd) {
    PostMessage(params->parentHwnd, WM_EMBED_APP, (WPARAM)findInfo.foundHwnd,
                (LPARAM)params);
  } else {
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    delete params;
  }

  return 0;
}

// -- Helpers --

// Tree Helpers
static std::wstring StringToWide(const std::string &str) {
  if (str.empty())
    return L"";
  int wLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  std::vector<wchar_t> wOut(wLen + 1);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wOut.data(), wLen);
  wOut[wLen] = 0;
  return std::wstring(wOut.data());
}

static std::string WideToString(const std::wstring &wstr) {
  if (wstr.empty())
    return "";
  int len =
      WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
  std::vector<char> buf(len + 1);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, buf.data(), len, NULL,
                      NULL);
  buf[len] = 0;
  return std::string(buf.data());
}

struct TreeItemData {
  std::string path;    // JSON Pointer path
  bool isArrayElement; // true if it's an array element like [0]
};

static HTREEITEM AddYamlToTree(HWND hTree, HTREEITEM hParent,
                               const std::string &key, const YAML::Node &node,
                               const std::string &path,
                               bool isArrayElem = false) {
  std::wstring wKey = StringToWide(key);

  int line = (int)node.Mark().line;

  std::wstring lineStr =
      (key == "ROOT") ? L"" : (L" (Ln " + std::to_wstring(line) + L")");

  std::wstring text = wKey + lineStr;

  if (node.IsScalar()) {
    text += L": " + StringToWide(node.as<std::string>());
  } else if (node.IsSequence()) {
    text += L" (Sequence)";
  } else if (node.IsMap()) {
    text += L" (Map)";
  }

  TVINSERTSTRUCTW tvis = {0};
  tvis.hParent = hParent;
  tvis.hInsertAfter = TVI_LAST;
  tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
  tvis.item.pszText = (LPWSTR)text.c_str();
  tvis.item.lParam = (LPARAM) new TreeItemData{path, isArrayElem};
  HTREEITEM hItem =
      (HTREEITEM)SendMessage(hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);

  if (node.IsMap()) {
    for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
      std::string k;
      try {
        k = it->first.as<std::string>();
      } catch (...) {
        k = "???";
      }
      // Escape JSON pointer key
      std::string escapedKey = k;
      // Simple escape: replace ~ with ~0 and / with ~1
      size_t pos = 0;
      while ((pos = escapedKey.find("~", pos)) != std::string::npos) {
        escapedKey.replace(pos, 1, "~0");
        pos += 2;
      }
      pos = 0;
      while ((pos = escapedKey.find("/", pos)) != std::string::npos) {
        escapedKey.replace(pos, 1, "~1");
        pos += 2;
      }
      std::string subPath = (path == "/" ? "" : path) + "/" + escapedKey;
      AddYamlToTree(hTree, hItem, k, it->second, subPath, false);
    }
  } else if (node.IsSequence()) {
    for (size_t i = 0; i < node.size(); i++) {
      std::string subPath = (path == "/" ? "" : path) + "/" + std::to_string(i);
      AddYamlToTree(hTree, hItem, "[" + std::to_string(i) + "]", node[i],
                    subPath, true);
    }
  }
  return hItem;
}

// Convert YAML::Node to json
static json YamlToJson(const YAML::Node &node) {
  if (node.IsScalar()) {
    std::string s = node.as<std::string>();
    if (s == "true")
      return true;
    if (s == "false")
      return false;
    if (s == "null" || s == "~")
      return nullptr;
    try {
      if (s.find('.') != std::string::npos ||
          s.find('e') != std::string::npos || s.find('E') != std::string::npos)
        return std::stod(s);
      return std::stoll(s);
    } catch (...) {
    }
    return s;
  }
  if (node.IsSequence()) {
    json j = json::array();
    for (size_t i = 0; i < node.size(); i++)
      j.push_back(YamlToJson(node[i]));
    return j;
  }
  if (node.IsMap()) {
    json j = json::object();
    for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
      std::string key;
      try {
        key = it->first.as<std::string>();
      } catch (...) {
        key = "???";
      }
      j[key] = YamlToJson(it->second);
    }
    return j;
  }
  return nullptr;
}

// Subclass procedure for the Edit control
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam, UINT_PTR uIdSubclass,
                                  DWORD_PTR dwRefData) {
  EditorWindow *pThis = (EditorWindow *)dwRefData;
  switch (uMsg) {
  case WM_VSCROLL:
  case WM_MOUSEWHEEL:
    break;
  case WM_KEYDOWN: {
    if (GetKeyState(VK_CONTROL) & 0x8000) {
      if (wParam == 'O') {
        SendMessage(pThis->Window(), WM_COMMAND, JE_IDM_FILE_OPEN, 0);
        return 0;
      } else if (wParam == 'S') {
        SendMessage(pThis->Window(), WM_COMMAND, JE_IDM_FILE_SAVE, 0);
        return 0;
      }
    }
    LRESULT lRes = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    pThis->UpdateLineNumbers(hWnd);
    return lRes;
  }
  case WM_KEYUP: {
    LRESULT lRes = DefSubclassProc(hWnd, uMsg, wParam, lParam);
    pThis->UpdateLineNumbers(hWnd);
    return lRes;
  }
  }
  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

EditorWindow::EditorWindow()
    : m_hwnd(NULL), m_hTabCtrl(NULL), m_hTreeView(NULL), m_activePageIndex(-1) {}

void EditorWindow::UpdateMenus() {
  HMENU hOldMenu = GetMenu(m_hwnd);
  HMENU hMenu = CreateMenu();

  // File Menu
  HMENU hFileMenu = CreatePopupMenu();
  AppendMenu(hFileMenu, MF_STRING, JE_IDM_FILE_NEW,
             m_localization.GetLocalizedString("New").c_str());
  AppendMenu(hFileMenu, MF_STRING, JE_IDM_FILE_OPEN,
             m_localization.GetLocalizedString("Open").c_str());
  AppendMenu(hFileMenu, MF_STRING, JE_IDM_FILE_SAVE,
             m_localization.GetLocalizedString("Save").c_str());
  AppendMenu(hFileMenu, MF_STRING, JE_IDM_FILE_SAVEAS,
             m_localization.GetLocalizedString("SaveAs").c_str());
  AppendMenu(hFileMenu, MF_STRING, JE_IDM_FILE_CLOSE_TAB,
             m_localization.GetLocalizedString("CloseTab").c_str());
  AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
  AppendMenu(hFileMenu, MF_STRING, JE_IDM_FILE_EXIT,
             m_localization.GetLocalizedString("Exit").c_str());
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu,
             m_localization.GetLocalizedString("File").c_str());

  // Format Menu
  HMENU hFormatMenu = CreatePopupMenu();
  AppendMenu(hFormatMenu, MF_STRING, JE_IDM_FORMAT_JSON,
             m_localization.GetLocalizedString("FormatJSON").c_str());
  AppendMenu(hFormatMenu, MF_STRING, JE_IDM_FORMAT_YAML,
             m_localization.GetLocalizedString("FormatYAML").c_str());
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFormatMenu,
             m_localization.GetLocalizedString("Format").c_str());

  // View Menu
  HMENU hViewMenu = CreatePopupMenu();
  AppendMenu(hViewMenu, MF_STRING, JE_IDM_VIEW_REFRESH_TREE,
             m_localization.GetLocalizedString("RefreshTree").c_str());
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hViewMenu,
             m_localization.GetLocalizedString("View").c_str());

  // Eol Menu
  HMENU hEolMenu = CreatePopupMenu();
  AppendMenu(hEolMenu, MF_STRING, JE_IDM_EOL_CRLF, L"CRLF (Windows)");
  AppendMenu(hEolMenu, MF_STRING, JE_IDM_EOL_LF, L"LF (Unix)");
  AppendMenu(hEolMenu, MF_STRING, JE_IDM_EOL_CR, L"CR (Mac Legacy)");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEolMenu,
             m_localization.GetLocalizedString("LineEndings").c_str());

  // Language Menu
  HMENU hLangMenu = CreatePopupMenu();
  AppendMenu(hLangMenu, MF_STRING, JE_IDM_LANG_EN,
             m_localization.GetLocalizedString("English").c_str());
  AppendMenu(hLangMenu, MF_STRING, JE_IDM_LANG_JP,
             m_localization.GetLocalizedString("Japanese").c_str());
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hLangMenu,
             m_localization.GetLocalizedString("Language").c_str());

  // Set Check Mark for current language
  CheckMenuItem(hLangMenu, JE_IDM_LANG_EN,
                m_localization.GetCurrentLanguage() == "en" ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hLangMenu, JE_IDM_LANG_JP,
                m_localization.GetCurrentLanguage() == "jp" ? MF_CHECKED : MF_UNCHECKED);

  // Tools Menu (from settings.json)
  if (!m_embeddedApps.empty()) {
    HMENU hToolsMenu = CreatePopupMenu();
    for (size_t i = 0; i < m_embeddedApps.size(); i++) {
      AppendMenu(hToolsMenu, MF_STRING,
                 JE_IDM_EMBEDDED_APP_BASE + (UINT)i,
                 m_embeddedApps[i].name.c_str());
    }
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hToolsMenu, L"&Tools");
  }

  SetMenu(m_hwnd, hMenu);
  if (hOldMenu)
    DestroyMenu(hOldMenu);
}

EditorWindow::~EditorWindow() { SaveSettings(); }

bool EditorWindow::Create(PCWSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle,
                          int x, int y, int nWidth, int nHeight,
                          HWND hWndParent, HMENU hMenu) {
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.lpfnWndProc = EditorWindow::WindowProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = L"JYEditorClass";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hIcon = (HICON)LoadImage(wc.hInstance, MAKEINTRESOURCE(JE_IDI_APP_ICON),
                              IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
  wc.hIconSm = (HICON)LoadImage(wc.hInstance, MAKEINTRESOURCE(JE_IDI_APP_ICON),
                                IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                GetSystemMetrics(SM_CYSMICON), LR_SHARED);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  RegisterClassEx(&wc);

  m_hwnd = CreateWindowEx(dwExStyle, L"JYEditorClass", lpWindowName, dwStyle, x,
                          y, nWidth, nHeight, hWndParent, hMenu,
                          GetModuleHandle(NULL), this);

  return (m_hwnd ? true : false);
}

LRESULT CALLBACK EditorWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                          LPARAM lParam) {
  EditorWindow *pThis = NULL;

  if (uMsg == WM_NCCREATE) {
    CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
    pThis = (EditorWindow *)pCreate->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    pThis->m_hwnd = hwnd;
  } else {
    pThis = (EditorWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (pThis) {
    return pThis->HandleMessage(uMsg, wParam, lParam);
  } else {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

LRESULT EditorWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE:
    OnCreate();
    return 0;
  case WM_SIZE:
    OnSize(LOWORD(lParam), HIWORD(lParam));
    return 0;
  case WM_COMMAND:
    OnCommand(LOWORD(wParam), HIWORD(wParam));
    return 0;
  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->idFrom == JE_IDC_TAB_CONTROL && pnm->code == TCN_SELCHANGE) {
      int index = TabCtrl_GetCurSel(m_hTabCtrl);
      SwitchTab(index);
    } else if (pnm->idFrom == JE_IDC_TREE_VIEW) {
      if (pnm->code == TVN_ENDLABELEDITW || pnm->code == TVN_ENDLABELEDITA) {
        LPNMTVDISPINFO ptvdi = (LPNMTVDISPINFO)lParam;
        if (ptvdi->item.pszText) {
          // Get the path from lParam
          TVITEMW item = {0};
          item.hItem = ptvdi->item.hItem;
          item.mask = TVIF_PARAM;
          if (SendMessage(m_hTreeView, TVM_GETITEMW, 0, (LPARAM)&item)) {
            TreeItemData *pData = (TreeItemData *)item.lParam;
            if (pData) {
              std::string newText = WideToString(ptvdi->item.pszText);
              // Basic editing: If label is "key: value", try to update value
              // If it's just "ROOT" or non-primitive, we might ignore for now
              size_t colonPos = newText.find(": ");
              if (colonPos != std::string::npos) {
                std::string newValStr = newText.substr(colonPos + 2);
                try {
                  // Attempt to parse new value as JSON
                  json newVal = json::parse(newValStr);
                  Document &doc = m_documents[m_activePageIndex];
                  doc.jsonData[json::json_pointer(pData->path)] = newVal;
                  UpdateTextFromModel();
                } catch (...) {
                  // Fallback: update as string
                  Document &doc = m_documents[m_activePageIndex];
                  doc.jsonData[json::json_pointer(pData->path)] = newValStr;
                  UpdateTextFromModel();
                }
              } else if (!pData->isArrayElement && pData->path != "/" &&
                         pData->path != "" && !newText.empty() &&
                         newText != "ROOT") {
                // Key rename
                size_t lastSlash = pData->path.find_last_of('/');
                if (lastSlash != std::string::npos) {
                  std::string parentPath = pData->path.substr(0, lastSlash);
                  std::string oldKeyEscaped = pData->path.substr(lastSlash + 1);

                  // Unescape JSON Pointer key (~1 -> /, ~0 -> ~)
                  std::string oldKey = oldKeyEscaped;
                  size_t p = 0;
                  while ((p = oldKey.find("~1", p)) != std::string::npos) {
                    oldKey.replace(p, 2, "/");
                    p++;
                  }
                  p = 0;
                  while ((p = oldKey.find("~0", p)) != std::string::npos) {
                    oldKey.replace(p, 2, "~");
                    p++;
                  }

                  Document &doc = m_documents[m_activePageIndex];
                  try {
                    json &parent =
                        (parentPath.empty())
                            ? doc.jsonData
                            : doc.jsonData[json::json_pointer(parentPath)];
                    if (parent.is_object() && parent.contains(oldKey)) {
                      auto val = parent[oldKey];
                      parent.erase(oldKey);
                      parent[newText] = val;
                      UpdateTextFromModel();
                    }
                  } catch (...) {
                  }
                }
              }
            }
          }
          return TRUE; // Accept the change
        }
        return FALSE;
      } else if (pnm->code == TVN_DELETEITEMA || pnm->code == TVN_DELETEITEMW) {
        LPNMTREEVIEW pnmv = (LPNMTREEVIEW)lParam;
        if (pnmv->itemOld.lParam) {
          delete (TreeItemData *)pnmv->itemOld.lParam;
        }
      }
    }
  }
    return 0;
  case WM_DESTROY:
    OnDestroy();
    return 0;
  case WM_EMBED_APP: {
    ChildAppLaunchParams *params = (ChildAppLaunchParams *)lParam;
    HWND childHwnd = (HWND)wParam;
    int tabIndex = params->tabIndex;

    if (tabIndex >= 0 && tabIndex < (int)m_documents.size() &&
        m_documents[tabIndex].isChildApp) {
      Document &doc = m_documents[tabIndex];
      doc.hChildProcess = params->hProcess;
      doc.childProcessId = params->processId;

      if (doc.hPlaceholder) {
        DestroyWindow(doc.hPlaceholder);
        doc.hPlaceholder = NULL;
      }

      LONG style = GetWindowLong(childHwnd, GWL_STYLE);
      style &=
          ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
            WS_MAXIMIZEBOX | WS_SYSMENU);
      style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
      SetWindowLong(childHwnd, GWL_STYLE, style);
      SetMenu(childHwnd, NULL);

      SetParent(childHwnd, m_hwnd);

      SetWindowPos(childHwnd, NULL, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                       SWP_FRAMECHANGED);

      doc.hChildApp = childHwnd;

      if (m_activePageIndex == tabIndex) {
        ShowWindow(childHwnd, SW_SHOW);
        SetFocus(childHwnd);
        ResizeTabControl();
      } else {
        ShowWindow(childHwnd, SW_HIDE);
      }
    } else {
      CloseHandle(params->hProcess);
    }

    delete params;
    return 0;
  }
  }
  return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void EditorWindow::OnCreate() {
  // Check for doc folder
  CreateDirectory(L"doc", NULL);

  // Create Open and Save buttons
  m_hOpenBtn = CreateWindow(
      L"BUTTON", L"📂 Open File",
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      5, 3, 110, 24,
      m_hwnd, (HMENU)JE_IDM_FILE_OPEN, GetModuleHandle(NULL), NULL
  );
  SendMessage(m_hOpenBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

  m_hSaveBtn = CreateWindow(
      L"BUTTON", L"💾 Save",
      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
      120, 3, 110, 24,
      m_hwnd, (HMENU)JE_IDM_FILE_SAVE, GetModuleHandle(NULL), NULL
  );
  SendMessage(m_hSaveBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

  // Create Tree View
  m_hTreeView = CreateWindowEx(
      0, WC_TREEVIEW, L"",
      WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS |
          TVS_LINESATROOT | TVS_EDITLABELS,
      0, 0, 0, 0, m_hwnd, (HMENU)JE_IDC_TREE_VIEW, GetModuleHandle(NULL), NULL);

  // Create Tab Control
  m_hTabCtrl = CreateWindow(
      WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, 0, 0, 0,
      m_hwnd, (HMENU)JE_IDC_TAB_CONTROL, GetModuleHandle(NULL), NULL);

  SendMessage(m_hTabCtrl, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
              0);

  LoadSettings();

  UpdateMenus();
  if (m_documents.empty()) {
    CreateNewTab(L"", L"");
  }
}

void EditorWindow::CreateNewTab(const std::wstring &path,
                                const std::wstring &content) {
  Document doc;
  doc.filePath = path;
  doc.fileName = GetFileNameFromPath(path);
  doc.eolMode = 0; // Default CRLF
  doc.isDirty = false;

  // Create Line Number Control (Static)
  doc.hLineNum =
      CreateWindowEx(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0,
                     0, 0, m_hwnd, NULL, GetModuleHandle(NULL), NULL);

  // Create Edit Control
  HFONT hFont =
      CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                 DEFAULT_PITCH | FF_MODERN, L"Consolas");

  // Set font for line numbers too
  SendMessage(doc.hLineNum, WM_SETFONT, (WPARAM)hFont, TRUE);

  doc.hEdit = CreateWindowEx(
      0, L"EDIT", content.c_str(),
      WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
          ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN,
      0, 0, 0, 0, m_hwnd, NULL, GetModuleHandle(NULL), NULL);

  SendMessage(doc.hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
  SendMessage(doc.hEdit, EM_LIMITTEXT, 0, 0);

  // Subclass Edit Control
  SetWindowSubclass(doc.hEdit, EditSubclassProc, 0, (DWORD_PTR)this);

  m_documents.push_back(doc);
  int newIndex = (int)m_documents.size() - 1;

  TCITEM tie;
  tie.mask = TCIF_TEXT;
  tie.pszText = (LPWSTR)doc.fileName.c_str();
  TabCtrl_InsertItem(m_hTabCtrl, newIndex, &tie);

  SwitchTab(newIndex);
}

void EditorWindow::UpdateLineNumbers(HWND hEdit) {
  // Find which document this edit belongs to
  Document *pDoc = nullptr;
  for (auto &doc : m_documents) {
    if (doc.hEdit == hEdit) {
      pDoc = &doc;
      break;
    }
  }
  if (!pDoc || !pDoc->hLineNum)
    return;

  int firstLine = (int)SendMessage(hEdit, EM_GETFIRSTVISIBLELINE, 0, 0);

  // Get client rect to know how many lines fit
  RECT rc;
  GetClientRect(hEdit, &rc);

  // Calculate line height
  HDC hdc = GetDC(hEdit);
  TEXTMETRIC tm;
  HFONT hFont = (HFONT)SendMessage(hEdit, WM_GETFONT, 0, 0);
  SelectObject(hdc, hFont);
  GetTextMetrics(hdc, &tm);
  ReleaseDC(hEdit, hdc);
  int lineHeight = tm.tmHeight;

  int linesVisible = rc.bottom / lineHeight;

  // Also get total lines to avoid printing past EOF
  int totalLines = (int)SendMessage(hEdit, EM_GETLINECOUNT, 0, 0);

  std::wstring numText;
  for (int i = 0; i <= linesVisible + 1; i++) {
    int lineIdx = firstLine + i;
    if (lineIdx >= totalLines)
      break;
    numText += std::to_wstring(lineIdx + 1) + L"\r\n";
  }

  SetWindowText(pDoc->hLineNum, numText.c_str());
}

void EditorWindow::SwitchTab(int index) {
  if (index < 0 || index >= m_documents.size())
    return;

  if (m_activePageIndex != -1) {
    Document &old = m_documents[m_activePageIndex];
    if (old.isChildApp) {
      if (old.hChildApp)
        ShowWindow(old.hChildApp, SW_HIDE);
      if (old.hPlaceholder)
        ShowWindow(old.hPlaceholder, SW_HIDE);
    } else {
      ShowWindow(old.hEdit, SW_HIDE);
      ShowWindow(old.hLineNum, SW_HIDE);
    }
  }

  m_activePageIndex = index;
  TabCtrl_SetCurSel(m_hTabCtrl, index);

  Document &doc = m_documents[index];
  if (doc.isChildApp) {
    if (doc.hChildApp) {
      ShowWindow(doc.hChildApp, SW_SHOW);
      SetFocus(doc.hChildApp);
    } else if (doc.hPlaceholder) {
      ShowWindow(doc.hPlaceholder, SW_SHOW);
    }
  } else {
    ShowWindow(doc.hEdit, SW_SHOW);
    ShowWindow(doc.hLineNum, SW_SHOW);
    SetFocus(doc.hEdit);
  }

  ResizeTabControl();
  UpdateTitle();
  if (!doc.isChildApp) {
    UpdateTreeFromText();
    UpdateLineNumbers(doc.hEdit);
  }

  // Update Menu State
  HMENU hMenu = GetMenu(m_hwnd);
  if (!doc.isChildApp) {
    int currentEol = doc.eolMode;
    CheckMenuItem(hMenu, JE_IDM_EOL_CRLF,
                  currentEol == 0 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, JE_IDM_EOL_LF,
                  currentEol == 1 ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hMenu, JE_IDM_EOL_CR,
                  currentEol == 2 ? MF_CHECKED : MF_UNCHECKED);
  }
}

void EditorWindow::OnSize(int width, int height) {
  int treeWidth = 250;
  if (width < treeWidth)
    treeWidth = width / 2;

  if (m_hOpenBtn) {
    MoveWindow(m_hOpenBtn, 5, 3, 110, 24, TRUE);
  }
  if (m_hSaveBtn) {
    MoveWindow(m_hSaveBtn, 120, 3, 110, 24, TRUE);
  }

  if (m_hTreeView) {
    MoveWindow(m_hTreeView, 0, 30, treeWidth, height - 30, TRUE);
  }
  if (m_hTabCtrl) {
    MoveWindow(m_hTabCtrl, treeWidth, 0, width - treeWidth, height, TRUE);
    ResizeTabControl();
  }
}

void EditorWindow::ResizeTabControl() {
  if (m_activePageIndex == -1 || !m_hTabCtrl)
    return;

  // Get Tab Control position
  RECT rcTab;
  GetWindowRect(m_hTabCtrl, &rcTab);
  POINT pt = {rcTab.left, rcTab.top};
  ScreenToClient(m_hwnd, &pt);

  RECT rcDisplay;
  GetClientRect(m_hTabCtrl, &rcDisplay);
  TabCtrl_AdjustRect(m_hTabCtrl, FALSE, &rcDisplay);

  int x = pt.x + rcDisplay.left;
  int y = pt.y + rcDisplay.top;
  int w = rcDisplay.right - rcDisplay.left;
  int h = rcDisplay.bottom - rcDisplay.top;

  // Layout: [LineNum (40px)] [Edit (Rest)]
  Document &doc = m_documents[m_activePageIndex];

  if (doc.isChildApp) {
    HWND hTarget = doc.hChildApp ? doc.hChildApp : doc.hPlaceholder;
    if (hTarget) {
      MoveWindow(hTarget, x, y, w, h, TRUE);
    }
    return;
  }

  int lineNumWidth = 40;

  if (doc.hLineNum) {
    MoveWindow(doc.hLineNum, x, y, lineNumWidth, h, TRUE);
    InvalidateRect(doc.hLineNum, NULL, TRUE);
  }

  MoveWindow(doc.hEdit, x + lineNumWidth, y, w - lineNumWidth, h, TRUE);
}

void EditorWindow::OnCommand(int id, int code) {
  switch (id) {
  case JE_IDM_FILE_NEW:
    CreateNewTab();
    break;
  case JE_IDM_FILE_EXIT:
    DestroyWindow(m_hwnd);
    break;
  case JE_IDM_FILE_OPEN:
    OpenFile();
    break;
  case JE_IDM_FILE_SAVE:
    SaveFile();
    break;
  case JE_IDM_FILE_SAVEAS:
    SaveFileAs();
    break;
  case JE_IDM_FILE_CLOSE_TAB:
    CloseCurrentTab();
    break;
  case JE_IDM_FORMAT_JSON:
    FormatJson();
    break;
  case JE_IDM_FORMAT_YAML:
    FormatYaml();
    break;
  case JE_IDM_VIEW_REFRESH_TREE:
    UpdateTreeFromText();
    break;
  case JE_IDM_EOL_CRLF:
    if (m_activePageIndex >= 0)
      m_documents[m_activePageIndex].eolMode = 0;
    // Force redraw menu check
    SwitchTab(m_activePageIndex);
    break;
  case JE_IDM_EOL_LF:
    if (m_activePageIndex >= 0)
      m_documents[m_activePageIndex].eolMode = 1;
    SwitchTab(m_activePageIndex);
    break;
  case JE_IDM_EOL_CR:
    if (m_activePageIndex >= 0)
      m_documents[m_activePageIndex].eolMode = 2;
    SwitchTab(m_activePageIndex);
    break;
  case JE_IDM_LANG_EN:
    m_localization.SetLanguage("en");
    UpdateMenus();
    UpdateTitle();
    break;
  case JE_IDM_LANG_JP:
    m_localization.SetLanguage("jp");
    UpdateMenus();
    UpdateTitle();
    break;
  default:
    if (id >= JE_IDM_EMBEDDED_APP_BASE) {
      UINT appIdx = id - JE_IDM_EMBEDDED_APP_BASE;
      if (appIdx < m_embeddedApps.size()) {
        const EmbeddedAppConfig &cfg = m_embeddedApps[appIdx];
        Document doc;
        doc.filePath = L"";
        doc.fileName = cfg.name;
        doc.eolMode = 0;
        doc.isDirty = false;
        doc.isChildApp = true;
        doc.hChildApp = NULL;
        doc.hPlaceholder = NULL;
        doc.hChildProcess = NULL;
        doc.childProcessId = 0;
        doc.hEdit = NULL;
        doc.hLineNum = NULL;
        doc.format = Document::FMT_TEXT;

        doc.hPlaceholder = CreateWindowEx(
            0, L"STATIC", L"Starting...",
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE, 0, 0, 0, 0,
            m_hwnd, NULL, GetModuleHandle(NULL), NULL);
        HFONT hPlaceholderFont =
            CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Segoe UI");
        SendMessage(doc.hPlaceholder, WM_SETFONT, (WPARAM)hPlaceholderFont,
                    TRUE);

        m_documents.push_back(doc);
        int newIndex = (int)m_documents.size() - 1;

        TCITEM tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)doc.fileName.c_str();
        TabCtrl_InsertItem(m_hTabCtrl, newIndex, &tie);

        SwitchTab(newIndex);
        LaunchChildApp(newIndex, cfg);
      }
    }
    break;
  }
}

void EditorWindow::LaunchChildApp(int tabIndex,
                                  const EmbeddedAppConfig &config) {
  wchar_t modulePath[MAX_PATH];
  GetModuleFileNameW(NULL, modulePath, MAX_PATH);
  std::filesystem::path parentPath(modulePath);
  std::wstring exePath =
      (parentPath.parent_path() / config.exeName).wstring();
  std::wstring workDir = parentPath.parent_path().wstring();

  if (!std::filesystem::exists(exePath)) {
    MessageBoxW(m_hwnd,
                (L"Executable not found:\n" + exePath).c_str(),
                L"Launch Error", MB_OK | MB_ICONERROR);
    return;
  }

  ChildAppLaunchParams *params = new ChildAppLaunchParams;
  params->parentHwnd = m_hwnd;
  params->tabIndex = tabIndex;
  params->exePath = exePath;
  params->workingDir = workDir;
  params->hProcess = NULL;
  params->processId = 0;

  HANDLE hThread =
      CreateThread(NULL, 0, ChildAppLaunchThread, params, 0, NULL);
  if (!hThread) {
    delete params;
  } else {
    CloseHandle(hThread);
  }
}

void EditorWindow::OnDestroy() {
  // Clean up any child app processes
  for (auto &doc : m_documents) {
    if (doc.isChildApp && doc.hChildApp) {
      DestroyWindow(doc.hChildApp);
      doc.hChildApp = NULL;
    }
    if (doc.isChildApp && doc.hChildProcess) {
      CloseHandle(doc.hChildProcess);
      doc.hChildProcess = NULL;
    }
    if (doc.isChildApp && doc.hPlaceholder) {
      DestroyWindow(doc.hPlaceholder);
      doc.hPlaceholder = NULL;
    }
  }
  SaveSettings();
  PostQuitMessage(0);
}

void EditorWindow::CloseCurrentTab() {
  if (m_activePageIndex == -1)
    return;

  Document &closing = m_documents[m_activePageIndex];
  if (closing.isChildApp) {
    if (closing.hChildApp) {
      DestroyWindow(closing.hChildApp);
      closing.hChildApp = NULL;
    }
    if (closing.hPlaceholder) {
      DestroyWindow(closing.hPlaceholder);
      closing.hPlaceholder = NULL;
    }
    if (closing.hChildProcess) {
      CloseHandle(closing.hChildProcess);
      closing.hChildProcess = NULL;
    }
  } else {
    DestroyWindow(closing.hEdit);
  }
  TabCtrl_DeleteItem(m_hTabCtrl, m_activePageIndex);
  m_documents.erase(m_documents.begin() + m_activePageIndex);

  if (m_documents.empty()) {
    m_activePageIndex = -1;
    CreateNewTab(); // Always keep one
  } else {
    int newIndex = m_activePageIndex;
    if (newIndex >= m_documents.size())
      newIndex = (int)m_documents.size() - 1;
    m_activePageIndex = -1; // Force switch
    SwitchTab(newIndex);
  }
}

void EditorWindow::OpenFile() {
  IFileOpenDialog *pFileOpen;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                IID_IFileOpenDialog,
                                reinterpret_cast<void **>(&pFileOpen));
  if (SUCCEEDED(hr)) {
    hr = pFileOpen->Show(m_hwnd);
    if (SUCCEEDED(hr)) {
      IShellItem *pItem;
      hr = pFileOpen->GetResult(&pItem);
      if (SUCCEEDED(hr)) {
        PWSTR pszFilePath;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        if (SUCCEEDED(hr)) {
          std::wstring wpath = pszFilePath;
          // Check if already open
          for (size_t i = 0; i < m_documents.size(); i++) {
            if (m_documents[i].filePath == wpath) {
              SwitchTab((int)i);
              CoTaskMemFree(pszFilePath);
              pItem->Release();
              pFileOpen->Release();
              return;
            }
          }

          std::wstring content = JSONEditorFileUtils::ReadFileUtf8(wpath);
          CreateNewTab(wpath, content);

          CoTaskMemFree(pszFilePath);
        }
        pItem->Release();
      }
    }
    pFileOpen->Release();
  }
}

void EditorWindow::SaveFile() {
  if (m_activePageIndex == -1)
    return;
  Document &doc = m_documents[m_activePageIndex];

  if (doc.filePath.empty()) {
    SaveFileAs();
    return;
  }

  int len = GetWindowTextLength(doc.hEdit);
  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(doc.hEdit, buffer.data(), len + 1);

  std::wstring text = buffer.data();
  if (JSONEditorFileUtils::WriteFileUtf8(doc.filePath, text,
                               (JSONEditorFileUtils::EolMode)doc.eolMode)) {
    doc.isDirty = false;
    UpdateTitle();
  }
}

void EditorWindow::SaveFileAs() {
  if (m_activePageIndex == -1)
    return;
  Document &doc = m_documents[m_activePageIndex];

  IFileSaveDialog *pFileSave;
  HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
                                IID_IFileSaveDialog,
                                reinterpret_cast<void **>(&pFileSave));
  if (SUCCEEDED(hr)) {
    // Set default folder to 'doc'
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, currentDir);
    std::wstring docPath = std::wstring(currentDir) + L"\\doc";

    IShellItem *pFolder;
    SHCreateItemFromParsingName(docPath.c_str(), NULL, IID_PPV_ARGS(&pFolder));
    if (pFolder) {
      pFileSave->SetFolder(pFolder);
      pFolder->Release();
    }

    hr = pFileSave->Show(m_hwnd);
    if (SUCCEEDED(hr)) {
      IShellItem *pItem;
      hr = pFileSave->GetResult(&pItem);
      if (SUCCEEDED(hr)) {
        PWSTR pszFilePath;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        if (SUCCEEDED(hr)) {
          doc.filePath = pszFilePath;
          doc.fileName = GetFileNameFromPath(doc.filePath);

          // Update Tab Text
          TCITEM tie;
          tie.mask = TCIF_TEXT;
          tie.pszText = (LPWSTR)doc.fileName.c_str();
          TabCtrl_SetItem(m_hTabCtrl, m_activePageIndex, &tie);

          SaveFile(); // Now save
          CoTaskMemFree(pszFilePath);
        }
        pItem->Release();
      }
    }
    pFileSave->Release();
  }
}

void EditorWindow::UpdateTitle() {
  if (m_activePageIndex == -1) {
    SetWindowText(m_hwnd, L"JYEditor");
    return;
  }

  std::wstring title = L"JYEditor - " + m_documents[m_activePageIndex].fileName;
  SetWindowText(m_hwnd, title.c_str());
}

std::wstring EditorWindow::GetFileNameFromPath(const std::wstring &path) {
  if (path.empty())
    return m_localization.GetLocalizedString("Untitled");
  std::filesystem::path p(path);
  return p.filename().wstring();
}

void EditorWindow::SaveSettings() {
  json j;
  j["window"]["width"] = 0; // Placeholder

  // Save open files
  std::vector<std::string> files;
  for (const auto &doc : m_documents) {
    if (!doc.filePath.empty()) {
      // Setup simple utf8 conversion
      int len = WideCharToMultiByte(CP_UTF8, 0, doc.filePath.c_str(), -1, NULL,
                                    0, NULL, NULL);
      if (len > 0) {
        std::vector<char> buf(len);
        WideCharToMultiByte(CP_UTF8, 0, doc.filePath.c_str(), -1, buf.data(),
                            len, NULL, NULL);
        files.push_back(std::string(buf.data()));
      }
    }
  }
  j["files"] = files;
  j["language"] = m_localization.GetCurrentLanguage();

  // Save embedded app configurations
  json appsJson = json::array();
  for (const auto &app : m_embeddedApps) {
    appsJson.push_back({{"name", WideToString(app.name)},
                        {"exe", WideToString(app.exeName)}});
  }
  j["embedded_apps"] = appsJson;

  std::ofstream o("settings.json");
  o << j << std::endl;
}

void EditorWindow::LoadSettings() {
  if (!std::filesystem::exists("settings.json"))
    return;

  try {
    std::ifstream i("settings.json");
    json j;
    i >> j;

    // Load language
    if (j.contains("language")) {
      m_localization.SetLanguage(j["language"].get<std::string>());
      UpdateMenus();
    }

    // Load files
    if (j.contains("files")) {
      for (const auto &f : j["files"]) {
        std::string s = f.get<std::string>();

        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (len > 0) {
          std::vector<wchar_t> wbuf(len);
          MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wbuf.data(), len);
          std::wstring wpath = wbuf.data();
          if (std::filesystem::exists(wpath)) {
            std::wstring content = JSONEditorFileUtils::ReadFileUtf8(wpath);
            CreateNewTab(wpath, content);
          }
        }
      }
    }

    // Load embedded app configurations
    m_embeddedApps.clear();
    if (j.contains("embedded_apps")) {
      for (const auto &app : j["embedded_apps"]) {
        EmbeddedAppConfig cfg;
        std::string s = app["name"].get<std::string>();
        int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (wlen > 0) {
          std::vector<wchar_t> wbuf(wlen);
          MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wbuf.data(), wlen);
          cfg.name = wbuf.data();
        }
        s = app["exe"].get<std::string>();
        wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (wlen > 0) {
          std::vector<wchar_t> wbuf(wlen);
          MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wbuf.data(), wlen);
          cfg.exeName = wbuf.data();
        }
        m_embeddedApps.push_back(cfg);
      }
    }
  } catch (...) {
  }
}

void EditorWindow::FormatJson() {
  if (m_activePageIndex == -1)
    return;
  HWND hEdit = m_documents[m_activePageIndex].hEdit;

  int len = GetWindowTextLength(hEdit);
  if (len == 0)
    return;

  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(hEdit, buffer.data(), len + 1);

  // Convert to UTF-8 for parsing
  int utf8Len =
      WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
  std::vector<char> utf8Buf(utf8Len + 1); // +1 for safety
  WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, utf8Buf.data(), utf8Len,
                      NULL, NULL);
  utf8Buf[utf8Len] = 0; // Ensure null terminated

  try {
    auto j = json::parse(utf8Buf.data());
    std::string formatted = j.dump(4);

    // Convert back to Wide
    int wLen = MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wOut(wLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, wOut.data(), wLen);
    wOut[wLen] = 0;

    std::wstring wFormatted = JSONEditorFileUtils::NormalizeToCrlf(wOut.data());
    SetWindowText(hEdit, wFormatted.c_str());
    UpdateTreeFromText();
  } catch (json::parse_error &e) {
    std::string err = e.what();
    std::wstring wErr(err.begin(), err.end());
    MessageBox(m_hwnd, wErr.c_str(), L"JSON Parse Error", MB_OK | MB_ICONERROR);
  }
}

#include <yaml-cpp/yaml.h>

void EditorWindow::FormatYaml() {
  if (m_activePageIndex == -1)
    return;
  HWND hEdit = m_documents[m_activePageIndex].hEdit;

  int len = GetWindowTextLength(hEdit);
  if (len == 0)
    return;

  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(hEdit, buffer.data(), len + 1);

  // Convert to UTF-8
  int utf8Len =
      WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
  std::vector<char> utf8Buf(utf8Len + 1);
  WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, utf8Buf.data(), utf8Len,
                      NULL, NULL);
  utf8Buf[utf8Len] = 0;

  try {
    YAML::Node node = YAML::Load(utf8Buf.data());
    YAML::Emitter out;
    out.SetIndent(2);
    out << node;

    std::string formatted = out.c_str();

    // Convert back to Wide
    int wLen = MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wOut(wLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, wOut.data(), wLen);
    wOut[wLen] = 0;

    std::wstring wFormatted = JSONEditorFileUtils::NormalizeToCrlf(wOut.data());
    SetWindowText(hEdit, wFormatted.c_str());
    UpdateTreeFromText();
  } catch (YAML::Exception &e) {
    std::string err = e.what();
    std::wstring wErr(err.begin(), err.end());
    MessageBox(m_hwnd, wErr.c_str(), L"YAML Parse Error", MB_OK | MB_ICONERROR);
  }
}

void EditorWindow::UpdateTreeFromText() {
  if (!m_hTreeView || m_activePageIndex == -1)
    return;

  Document &doc = m_documents[m_activePageIndex];
  HWND hEdit = doc.hEdit;

  int len = GetWindowTextLength(hEdit);
  if (len == 0) {
    TreeView_DeleteAllItems(m_hTreeView);
    doc.format = Document::FMT_TEXT;
    doc.jsonData = json(); // Clear model
    return;
  }

  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(hEdit, buffer.data(), len + 1);

  // Convert to UTF-8
  int utf8Len =
      WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
  std::vector<char> utf8Buf(utf8Len + 1);
  WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, utf8Buf.data(), utf8Len,
                      NULL, NULL);
  utf8Buf[utf8Len] = 0;

  // Unified Parsing using YAML parser (supports JSON and provides line numbers)
  try {
    std::vector<YAML::Node> nodes = YAML::LoadAll(utf8Buf.data());
    if (!nodes.empty()) {
      // Build JSON model from YAML nodes
      if (nodes.size() == 1) {
        doc.jsonData = YamlToJson(nodes[0]);
      } else {
        doc.jsonData = json::array();
        for (const auto &n : nodes) {
          doc.jsonData.push_back(YamlToJson(n));
        }
      }

      // Detection: Check if it's JSON or YAML
      doc.format = Document::FMT_YAML; // Assume YAML by default if parsed
      std::string s(utf8Buf.data());
      // Trim leading whitespace to find first non-whitespace character
      size_t first_char_idx = s.find_first_not_of(" \t\n\r");
      if (first_char_idx != std::string::npos) {
        char first_char = s[first_char_idx];
        if (first_char == '{' || first_char == '[') {
          // Attempt to parse as JSON to confirm
          try {
            json::parse(s); // If this succeeds, it's likely JSON
            doc.format = Document::FMT_JSON;
          } catch (const json::parse_error &) {
            // It parsed as YAML, but not strict JSON, so keep as YAML
            doc.format = Document::FMT_YAML;
          }
        }
      }

      // Populate Tree
      TreeView_DeleteAllItems(m_hTreeView);
      for (size_t i = 0; i < nodes.size(); i++) {
        std::string rootName = "ROOT";
        if (nodes.size() > 1)
          rootName += " [" + std::to_string(i) + "]";
        HTREEITEM hRoot =
            AddYamlToTree(m_hTreeView, TVI_ROOT, rootName, nodes[i], "/");
        TreeView_Expand(m_hTreeView, hRoot, TVE_EXPAND);
      }
      return;
    }
  } catch (...) {
    // Fall through to text format if YAML parsing fails
  }

  // Fallback
  doc.format = Document::FMT_TEXT;
  doc.jsonData = json(); // Clear model
  TreeView_DeleteAllItems(m_hTreeView);
}

void EditorWindow::SyncModelToTree() {
  UpdateTreeFromText(); // Unified
}

void EditorWindow::UpdateTextFromModel(bool toYaml) {
  if (m_activePageIndex == -1)
    return;
  Document &doc = m_documents[m_activePageIndex];

  std::string formatted;
  if (toYaml || doc.format == Document::FMT_YAML) {
    // Convert json to yaml (basic)
    // For robust json->yaml, we might need to iterate json and build YAML::Node
    // But for now, let's just use json dump if not forcing yaml
    if (toYaml) {
      // Placeholder: Properly implementing JSON -> YAML via yaml-cpp requires
      // recursive build For now, dump JSON as it's valid YAML superset (mostly)
      formatted = doc.jsonData.dump(2);
    } else {
      formatted = doc.jsonData.dump(4);
    }
  } else {
    formatted = doc.jsonData.dump(4);
  }

  // Convert back to Wide
  int wLen = MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, NULL, 0);
  std::vector<wchar_t> wOut(wLen + 1);
  MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, wOut.data(), wLen);
  wOut[wLen] = 0;

  std::wstring wFormatted = JSONEditorFileUtils::NormalizeToCrlf(wOut.data());
  SetWindowText(doc.hEdit, wFormatted.c_str());
}
