#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#endif
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <windows.h>
#include <windowsx.h>

#define _PRSHT_H_
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Strategies/Types.h"
// fs is now defined in Types.h

#include "BackupEngine.h"
#include "Configuration.h"
#include "Localization.h"
#include "Strategies/ComparingBackupStrategy.h"
#include "Strategies/IBackupStrategy.h"
#include "Strategies/ParallelBackupStrategy.h"
#include "Strategies/StandardBackupStrategy.h"
#include "resources/resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "mpr.lib")
#include <winnetwk.h>

#pragma comment(lib, "ole32.lib")

#ifndef LVM_INSERTITEMW
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#endif
#ifndef LVM_GETITEMCOUNT
#define LVM_GETITEMCOUNT (LVM_FIRST + 4)
#endif
#ifndef LVM_DELETEALLITEMS
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#endif

HINSTANCE hInst;
HWND hTreeView, hLogEdit, hStatusBar, hSrcTreeView, hDstTreeView;
HWND hSrcPathLabel, hDstPathLabel, hStopButton, hLogMaxButton, hProgressLabel,
    hProgressBar;
HWND hMainWindow, hToolTip = NULL;
HWND hMainStrategyCombo, hMainModeCombo;
BackupEngine *g_currentEngine = nullptr;
bool g_logMaximized = false;
HIMAGELIST g_hImageList = NULL;

// Image List Indices
#define IDX_DRIVE 0
#define IDX_FOLDER_CLOSED 1
#define IDX_FOLDER_OPEN 2
#define IDX_SET 3
#define IDX_UNIT 4

// Worker control globals
#define ID_WORKER_BASE_BTN 3000
#define ID_WORKER_BASE_PROG 3100
#define ID_WORKER_BASE_LABEL 3200
const int MAX_WORKERS = 4;
HWND hWorkerProgs[MAX_WORKERS];
HWND hWorkerBtns[MAX_WORKERS];
HWND hWorkerLabels[MAX_WORKERS];

std::vector<BackupSet> g_backupSets;
int g_selectedSetIndex = -1;
int g_selectedUnitIndex = -1;

#define ID_TREE_VIEW 107
#define ID_SRC_TREE_VIEW 108
#define ID_DST_TREE_VIEW 109
#define ID_SRC_PATH_LABEL 110
#define ID_DST_PATH_LABEL 111
#define ID_LOG_EDIT 103
#define ID_STATUS_BAR 106
#define ID_STRATEGY_COMBO 105
#define ID_PROGRESS_LABEL 112
#define ID_PROGRESS_BAR 113
#define ID_MAIN_MODE_COMBO 114
#define ID_MAIN_STRATEGY_COMBO 115

#define IDM_LOG_HISTORY 1010
#define ID_LIST_HISTORY 1011
#define ID_BTN_HISTORY_OPEN 1012

// Menu IDs
#define IDM_FILE_EXIT 203
#define IDM_SET_CREATE 301
#define IDM_SET_OPEN 302
#define IDM_SET_DELETE 304
#define IDM_SET_UPDATE 305
#define IDM_UNIT_CREATE 401
#define IDM_UNIT_OPEN 402
#define IDM_UNIT_UPDATE 403
#define IDM_UNIT_DELETE 404
#define IDM_SET_DUPLICATE 306
#define IDM_UNIT_DUPLICATE 405

#define IDM_BACKUP_RUN 701
#define IDM_BACKUP_PREVIEW 702
#define IDM_BACKUP_VERIFY 705
#define IDM_BACKUP_STOP 703
#define IDM_LOG_MAXIMIZE 704
#define IDM_HELP_ABOUT 801
#define IDM_SET_AS_SOURCE 810
#define IDM_SET_AS_TARGET 811

// Dialogue IDs
#define IDC_EDIT_SET_NAME 501
#define IDC_EDIT_SET_DESC 506
#define IDC_BTN_SET_SAVE 502
#define IDC_DLG_LIST_UNITS 503
#define IDC_BTN_UNIT_UP 504
#define IDC_BTN_UNIT_DOWN 505
#define IDC_RAD_SCHED_MANUAL 507
#define IDC_RAD_SCHED_DAILY 508
#define IDC_RAD_SCHED_WEEKLY 509
#define IDC_COMBO_WEEKDAY 510
#define IDC_EDIT_SCHED_HOUR 511
#define IDC_EDIT_SCHED_MIN 512

#define IDC_EDIT_UNIT_NAME 601
#define IDC_EDIT_UNIT_SRC 602
#define IDC_EDIT_UNIT_DST 603
#define IDC_BTN_UNIT_SAVE 604
#define IDC_BTN_BROWSE_SRC 605
#define IDC_BTN_BROWSE_DST 606
#define IDC_RAD_SYNC 607
#define IDC_RAD_COPY 608
#define IDC_RAD_VERIFY_ON 609
#define IDC_RAD_MODE_VERIFY 618
#define IDC_RAD_POLICY_CONT 611
#define IDC_RAD_POLICY_SUSP 612
#define IDC_CHK_CRITERIA_SIZE 615
#define IDC_CHK_CRITERIA_DATA 617
#define IDC_CB_ENGINE 621
#define IDC_BTN_NET_CONN 622

class WindowLogger : public IBackupLogger {
public:
  WindowLogger(HWND hEditIn, HWND hProgressIn, HWND hProgressBarIn,
               HWND *workerProgsIn, HWND *workerBtnsIn, HWND *workerLabelsIn)
      : m_hEdit(hEditIn), m_hProgress(hProgressIn),
        m_hProgressBar(hProgressBarIn) {
    for (int i = 0; i < MAX_WORKERS; ++i) {
      m_workerProgs[i] = workerProgsIn[i];
      m_workerBtns[i] = workerBtnsIn[i];
      m_workerLabels[i] = workerLabelsIn[i];
    }

    std::wstring cfgPath = ConfigManager::GetConfigPath();
    fs::path p(cfgPath);
    m_logFile.open(p.parent_path() / L"last_run.txt",
                   std::ios::out | std::ios::trunc);
  }

  bool HasErrors() const { return m_hasErrors; }
  void ClearErrorFlag() { m_hasErrors = false; }

  ~WindowLogger() {
    if (m_logFile.is_open())
      m_logFile.close();
  }

  void Log(const std::wstring &message) override {
    std::wstring msg = message + L"\r\n";
    SendMessageW(m_hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(m_hEdit, EM_REPLACESEL, 0, (LPARAM)msg.c_str());
    if (m_logFile.is_open()) {
      m_logFile << message << std::endl;
      m_logFile.flush();
    }
  }

  void OnFileAction(const std::wstring &action,
                    const std::wstring &path) override {
    std::wstring msg = L"[" + action + L"] " + path + L"\r\n";
    SendMessageW(m_hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(m_hEdit, EM_REPLACESEL, 0, (LPARAM)msg.c_str());
    if (m_logFile.is_open()) {
      m_logFile << L"[" << action << L"] " << path << std::endl;
      m_logFile.flush();
    }
    if (action.find(L"Error") != std::wstring::npos ||
        action.find(L"Fail") != std::wstring::npos) {
      m_hasErrors = true;
    }
  }

  void FinalizeLog(const std::wstring &title) {
    if (m_logFile.is_open())
      m_logFile.close();

    std::wstring cfgPath = ConfigManager::GetConfigPath();
    fs::path p(cfgPath);
    fs::path baseDir = p.parent_path();
    fs::path srcFile = baseDir / L"last_run.txt";

    // Sanitize title for filename
    std::wstring safeTitle = title;
    const std::wstring invalidChars = L"\\/:*?\"<>|";
    for (auto &c : safeTitle) {
      if (invalidChars.find(c) != std::wstring::npos) {
        c = L'_';
      }
    }

    // Prepare timestamp
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm timeinfo;
    localtime_s(&timeinfo, &in_time_t);

    std::wstringstream ss;
    ss << std::put_time(&timeinfo, L"%Y%m%d_%H%M%S");

    std::wstring finalName =
        safeTitle + L"_" + ss.str() + (m_hasErrors ? L"_ERR" : L"") + L".txt";
    fs::path dstFile = baseDir / finalName;

    try {
      if (fs::exists(srcFile)) {
        fs::copy_file(srcFile, dstFile, fs::copy_options::overwrite_existing);
      }
    } catch (...) {
    }

    // Reopen last_run.txt for next run session
    m_logFile.open(srcFile, std::ios::out | std::ios::trunc);
    // Clear flag ONLY after we successfully copy the log to a timestamped file,
    // though usually RunBackup will clear it at start.
  }

  void OnWorkerProgress(int workerId, const std::wstring &file,
                        int percent) override {
    if (workerId >= 0 && workerId < MAX_WORKERS) {
      // Only update controls if they exist (they're now removed from UI)
      if (m_workerLabels[workerId]) {
        std::wstringstream ss;
        ss << L"Worker " << (workerId + 1) << L": " << file;
        SetWindowTextW(m_workerLabels[workerId], ss.str().c_str());
      }

      if (m_workerProgs[workerId] && percent >= 0) {
        SendMessageW(m_workerProgs[workerId], PBM_SETPOS, (WPARAM)percent, 0);
      }
    }
  }

  void OnProgress(const std::wstring &path) override {
    std::wstring txt = L"Processing: " + path;
    SetWindowTextW(m_hProgress, txt.c_str());
  }

  void OnProgressDetailed(const TaskProgress &progress) override {
    if (m_hProgressBar) {
      if (progress.totalBytes > 0) {
        int pct = (int)((progress.processedBytes * 100) / progress.totalBytes);
        SendMessageW(m_hProgressBar, PBM_SETPOS, (WPARAM)pct, 0);
      } else {
        SendMessageW(m_hProgressBar, PBM_SETPOS, 0, 0);
      }
    }

    // Format a nice status string
    std::wstringstream ss;
    ss << L"Processing: " << progress.currentFile << L" | Files: "
       << progress.processedFiles << L"/" << progress.totalFiles << L" | MB: "
       << (progress.processedBytes / 1024 / 1024) << L"/"
       << (progress.totalBytes / 1024 / 1024);

    SetWindowTextW(m_hProgress, ss.str().c_str());
  }

private:
  HWND m_hEdit;
  HWND m_hProgress;
  HWND m_hProgressBar;
  HWND m_workerProgs[MAX_WORKERS];
  HWND m_workerBtns[MAX_WORKERS];
  HWND m_workerLabels[MAX_WORKERS];
  std::wofstream m_logFile;
  bool m_hasErrors = false;
};

WindowLogger *g_logger = nullptr;

void AddToolTip(HWND hParent, HWND hControl, const wchar_t *text) {
  if (!hToolTip) {
    hToolTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
                               WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT, hParent, NULL, hInst, NULL);
    SetWindowPos(hToolTip, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  TOOLINFOW ti = {0};
  ti.cbSize = sizeof(ti);
  ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  ti.hwnd = hParent;
  ti.uId = (UINT_PTR)hControl;
  ti.lpszText = (LPWSTR)text;
  if (!SendMessageW(hToolTip, TTM_GETTOOLINFOW, 0, (LPARAM)&ti)) {
    SendMessageW(hToolTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
  } else {
    SendMessageW(hToolTip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
  }
}

void InitImageList() {
  g_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 5, 1);

  // 0: Drive icon
  SHFILEINFOW sfi = {0};
  SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON);
  ImageList_AddIcon(g_hImageList, sfi.hIcon);
  DestroyIcon(sfi.hIcon);

  // 1: Folder Closed - Use Shell32 icon 3 (classic closed yellow folder)
  HICON hClosedFolder =
      (HICON)LoadImageW(GetModuleHandleW(L"shell32.dll"), MAKEINTRESOURCEW(4),
                        IMAGE_ICON, 16, 16, 0);
  if (hClosedFolder) {
    ImageList_AddIcon(g_hImageList, hClosedFolder);
    DestroyIcon(hClosedFolder);
  } else {
    // Fallback to SHGetFileInfo
    SHGetFileInfoW(L"dummy", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
    ImageList_AddIcon(g_hImageList, sfi.hIcon);
    DestroyIcon(sfi.hIcon);
  }

  // 2: Folder Open - Use Shell32 icon 5 (classic open yellow folder)
  HICON hOpenFolder =
      (HICON)LoadImageW(GetModuleHandleW(L"shell32.dll"), MAKEINTRESOURCEW(5),
                        IMAGE_ICON, 16, 16, 0);
  if (hOpenFolder) {
    ImageList_AddIcon(g_hImageList, hOpenFolder);
    DestroyIcon(hOpenFolder);
  } else {
    // Fallback to SHGetFileInfo with OPENICON flag
    SHGetFileInfoW(L"dummy", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
                   SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON |
                       SHGFI_OPENICON);
    ImageList_AddIcon(g_hImageList, sfi.hIcon);
    DestroyIcon(sfi.hIcon);
  }

  // 3: Set icon (application icon)
  HICON hIconSet =
      LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON));
  if (!hIconSet)
    hIconSet = LoadIcon(NULL, IDI_APPLICATION);
  ImageList_AddIcon(g_hImageList, hIconSet);

  // 4: Unit icon (generic file)
  SHGetFileInfoW(L"dummy.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
                 SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_SMALLICON);
  ImageList_AddIcon(g_hImageList, sfi.hIcon);
  DestroyIcon(sfi.hIcon);
}

void PopulateDriveTree(HWND hwnd) {
  TreeView_DeleteAllItems(hwnd);
  wchar_t drives[256];
  if (GetLogicalDriveStringsW(256, drives)) {
    wchar_t *p = drives;
    while (*p) {
      TVINSERTSTRUCTW tvis = {0};
      tvis.hParent = NULL;
      tvis.hInsertAfter = TVI_LAST;
      tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE |
                       TVIF_SELECTEDIMAGE;
      tvis.item.pszText = p;
      tvis.item.cChildren = 1; // Assume it has subfolders
      tvis.item.iImage = IDX_DRIVE;
      tvis.item.iSelectedImage = IDX_DRIVE;
      std::wstring *pathObj = new std::wstring(p);
      tvis.item.lParam = (LPARAM)pathObj;
      SendMessageW(hwnd, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
      p += wcslen(p) + 1;
    }
  }
}

void ExpandTreeToPath(HWND hwnd, const std::wstring &targetPath) {
  if (targetPath.empty())
    return;
  fs::path p(targetPath);
  std::vector<std::wstring> pathParts;
  for (auto const &part : p) {
    if (part == L"\\")
      continue;
    std::wstring s = part.wstring();
    if (!s.empty() && s.back() == L'\\')
      s.pop_back();
    if (!s.empty())
      pathParts.push_back(s);
  }

  HTREEITEM hParent = NULL;
  for (size_t i = 0; i < pathParts.size(); ++i) {
    HTREEITEM hChild = TreeView_GetChild(hwnd, hParent);
    bool found = false;
    while (hChild) {
      TVITEMW tvi = {0};
      wchar_t buf[MAX_PATH];
      tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_HANDLE;
      tvi.hItem = hChild;
      tvi.pszText = buf;
      tvi.cchTextMax = MAX_PATH;
      TreeView_GetItem(hwnd, &tvi);

      std::wstring itemText = buf;
      if (itemText.back() == L'\\')
        itemText.pop_back();

      if (_wcsicmp(itemText.c_str(), pathParts[i].c_str()) == 0) {
        TreeView_Expand(hwnd, hChild, TVE_EXPAND);
        TreeView_SelectItem(hwnd, hChild);
        TreeView_EnsureVisible(hwnd, hChild);

        std::wstring currentLabel =
            (hwnd == hSrcTreeView ? L"Source Path: " : L"Target Path: ") +
            targetPath;
        SetWindowTextW(hwnd == hSrcTreeView ? hSrcPathLabel : hDstPathLabel,
                       currentLabel.c_str());
        AddToolTip(hMainWindow,
                   hwnd == hSrcTreeView ? hSrcPathLabel : hDstPathLabel,
                   targetPath.c_str());

        hParent = hChild;
        found = true;
        break;
      }
      hChild = TreeView_GetNextSibling(hwnd, hChild);
    }
    if (!found)
      break;
  }
}

void PopulateDirBranch(HWND hwnd, HTREEITEM hItem) {
  TVITEMW tvi = {0};
  tvi.mask = TVIF_PARAM | TVIF_HANDLE;
  tvi.hItem = hItem;
  TreeView_GetItem(hwnd, &tvi);
  std::wstring *pParentPath = (std::wstring *)tvi.lParam;
  if (!pParentPath)
    return;

  HTREEITEM hExisting = TreeView_GetChild(hwnd, hItem);
  if (hExisting) {
    TVITEMW firstChild = {0};
    firstChild.mask = TVIF_PARAM | TVIF_HANDLE;
    firstChild.hItem = hExisting;
    TreeView_GetItem(hwnd, &firstChild);
    if (firstChild.lParam != 0)
      return;
    TreeView_DeleteItem(hwnd, hExisting);
  }

  try {
    for (const auto &entry : fs::directory_iterator(*pParentPath)) {
      if (fs::is_directory(entry.status())) {
        TVINSERTSTRUCTW tvis = {0};
        tvis.hParent = hItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE |
                         TVIF_SELECTEDIMAGE;
        std::wstring name = entry.path().filename().wstring();
        tvis.item.pszText = (LPWSTR)name.c_str();
        tvis.item.cChildren = 1;
        tvis.item.iImage = IDX_FOLDER_CLOSED;
        tvis.item.iSelectedImage = IDX_FOLDER_CLOSED; // Will change on expand
        tvis.item.lParam = (LPARAM) new std::wstring(entry.path().wstring());
        SendMessageW(hwnd, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
      }
    }
  } catch (...) {
  }
}

void UpdateStatusText() {
  if (g_selectedSetIndex == -1) {
    SendMessageW(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
  } else if (g_selectedUnitIndex == -1) {
    std::wstring txt = L"Set: " + g_backupSets[g_selectedSetIndex].name;
    SendMessageW(hStatusBar, SB_SETTEXT, 0, (LPARAM)txt.c_str());
  } else {
    std::wstring txt =
        L"Unit: " + g_backupSets[g_selectedSetIndex].name + L" > " +
        g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex].name;
    SendMessageW(hStatusBar, SB_SETTEXT, 0, (LPARAM)txt.c_str());
  }
}

void RefreshTreeView() {
  TreeView_DeleteAllItems(hTreeView);
  HTREEITEM hSet = NULL;
  for (int i = 0; i < (int)g_backupSets.size(); ++i) {
    std::wstring setDisplayName = g_backupSets[i].name;
    if (!g_backupSets[i].description.empty()) {
      setDisplayName += L" (" + g_backupSets[i].description + L")";
    }
    TVINSERTSTRUCTW tvis = {0};
    tvis.hParent = NULL;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvis.item.pszText = (LPWSTR)setDisplayName.c_str();
    tvis.item.iImage = IDX_SET;
    tvis.item.iSelectedImage = IDX_SET;
    tvis.item.lParam = (LPARAM)((i << 16) | 0xFFFF);
    hSet =
        (HTREEITEM)SendMessageW(hTreeView, TVM_INSERTITEMW, 0, (LPARAM)&tvis);

    for (int j = 0; j < (int)g_backupSets[i].units.size(); ++j) {
      const auto &u = g_backupSets[i].units[j];
      std::wstring summary = u.name + L" [" +
                             (u.mode == BackupMode::Sync     ? L"SYNC"
                              : u.mode == BackupMode::Verify ? L"VERIFY"
                                                             : L"COPY") +
                             (u.verify ? L"+V" : L"") + L"] " + u.source +
                             L" -> " + u.target;

      TVINSERTSTRUCTW tvisChild = {0};
      tvisChild.hParent = hSet;
      tvisChild.hInsertAfter = TVI_LAST;
      tvisChild.item.mask =
          TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
      tvisChild.item.pszText = (LPWSTR)summary.c_str();
      tvisChild.item.iImage = IDX_UNIT;
      tvisChild.item.iSelectedImage = IDX_UNIT;
      tvisChild.item.lParam = (LPARAM)((i << 16) | j);
      SendMessageW(hTreeView, TVM_INSERTITEMW, 0, (LPARAM)&tvisChild);
    }
    if (hSet)
      TreeView_Expand(hTreeView, hSet, TVE_EXPAND);
  }
  UpdateStatusText();
}

bool IsPathOverlap(const std::wstring &p1, const std::wstring &p2) {
  if (p1.empty() || p2.empty())
    return false;
  try {
    fs::path path1 = fs::absolute(p1);
    fs::path path2 = fs::absolute(p2);
    std::wstring s1 = path1.wstring();
    std::wstring s2 = path2.wstring();

    if (_wcsicmp(s1.c_str(), s2.c_str()) == 0)
      return true;

    // Add trailing backslash for containment check
    if (s1.back() != L'\\' && s1.back() != L'/')
      s1 += L'\\';
    if (s2.back() != L'\\' && s2.back() != L'/')
      s2 += L'\\';

    if (_wcsnicmp(s1.c_str(), s2.c_str(), s1.length()) == 0)
      return true; // p2 is inside p1
    if (_wcsnicmp(s1.c_str(), s2.c_str(), s2.length()) == 0)
      return true; // p1 is inside p2
  } catch (...) {
  }
  return false;
}

bool BrowseForFolder(HWND hwnd, std::wstring &outPath) {
  IFileOpenDialog *pfd;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                 IID_IFileOpenDialog, (void **)&pfd))) {
    DWORD dwOptions;
    if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
      pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
    }
    if (SUCCEEDED(pfd->Show(hwnd))) {
      IShellItem *psi;
      if (SUCCEEDED(pfd->GetResult(&psi))) {
        PWSTR pszPath;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
          outPath = pszPath;
          CoTaskMemFree(pszPath);
          psi->Release();
          pfd->Release();
          return true;
        }
        psi->Release();
      }
    }
    pfd->Release();
  }
  return false;
}

// ============================================================================
// Dialog Helper Functions
// ============================================================================

static HFONT CreateDialogFont() {
  return CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void InitializeUnitEditDlg(HWND hWnd, BackupUnit *pUnit, HFONT hFont) {
  auto CreateCtrl = [&](const wchar_t *className, const wchar_t *text,
                        DWORD style, int x, int y, int w, int h, int id) {
    HWND hCtrl =
        CreateWindowW(className, text, style | WS_CHILD | WS_VISIBLE, x, y, w,
                      h, hWnd, (HMENU)(UINT_PTR)id, hInst, NULL);
    if (hCtrl)
      SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
    return hCtrl;
  };

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_UnitName), 0, 20, 10, 200,
             20, 0);
  CreateCtrl(L"EDIT", pUnit->name.c_str(), WS_BORDER | ES_AUTOHSCROLL, 20, 30,
             440, 25, IDC_EDIT_UNIT_NAME);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Source), 0, 20, 65, 200,
             20, 0);
  CreateCtrl(L"EDIT", pUnit->source.c_str(), WS_BORDER | ES_AUTOHSCROLL, 20, 85,
             340, 25, IDC_EDIT_UNIT_SRC);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Browse), BS_PUSHBUTTON,
             370, 85, 90, 25, IDC_BTN_BROWSE_SRC);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Target), 0, 20, 120, 200,
             20, 0);
  CreateCtrl(L"EDIT", pUnit->target.c_str(), WS_BORDER | ES_AUTOHSCROLL, 20,
             140, 340, 25, IDC_EDIT_UNIT_DST);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Browse), BS_PUSHBUTTON,
             370, 140, 90, 25, IDC_BTN_BROWSE_DST);

  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_MapNetwork), BS_PUSHBUTTON,
             20, 175, 140, 25, IDC_BTN_NET_CONN);

  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Mode_Copy),
             BS_AUTORADIOBUTTON | WS_GROUP, 20, 210, 150, 25, IDC_RAD_COPY);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Mode_Sync),
             BS_AUTORADIOBUTTON, 180, 210, 150, 25, IDC_RAD_SYNC);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Mode_VerifyOnly),
             BS_AUTORADIOBUTTON, 340, 210, 180, 25, IDC_RAD_MODE_VERIFY);

  if (pUnit->mode == BackupMode::Sync)
    CheckDlgButton(hWnd, IDC_RAD_SYNC, BST_CHECKED);
  else if (pUnit->mode == BackupMode::Verify)
    CheckDlgButton(hWnd, IDC_RAD_MODE_VERIFY, BST_CHECKED);
  else
    CheckDlgButton(hWnd, IDC_RAD_COPY, BST_CHECKED);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Verification), 0, 20, 240,
             200, 20, 0);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Verify_Full),
             BS_AUTORADIOBUTTON | WS_GROUP, 20, 260, 200, 25,
             IDC_RAD_VERIFY_ON);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Verify_Criteria),
             BS_AUTOCHECKBOX, 230, 260, 250, 25, IDC_CHK_CRITERIA_SIZE);

  if (pUnit->verify)
    CheckDlgButton(hWnd, IDC_RAD_VERIFY_ON, BST_CHECKED);
  if (pUnit->criteriaSize)
    CheckDlgButton(hWnd, IDC_CHK_CRITERIA_SIZE, BST_CHECKED);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_ErrorPolicy), 0, 20, 295,
             200, 20, 0);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Policy_Cont),
             BS_AUTORADIOBUTTON | WS_GROUP, 20, 315, 200, 25,
             IDC_RAD_POLICY_CONT);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Policy_Susp),
             BS_AUTORADIOBUTTON, 230, 315, 200, 25, IDC_RAD_POLICY_SUSP);

  if (pUnit->errorPolicy == ErrorPolicy::Suspend)
    CheckDlgButton(hWnd, IDC_RAD_POLICY_SUSP, BST_CHECKED);
  else
    CheckDlgButton(hWnd, IDC_RAD_POLICY_CONT, BST_CHECKED);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Engine), 0, 20, 345, 200,
             20, 0);
  HWND hEngineCombo =
      CreateCtrl(L"COMBOBOX", NULL, CBS_DROPDOWNLIST | WS_VSCROLL, 20, 365, 250,
                 200, IDC_CB_ENGINE);

  const StrId engines[] = {StrId::Eng_Std, StrId::Eng_Par, StrId::Eng_Blk,
                           StrId::Eng_Vss, StrId::Eng_Cmp};
  for (auto id : engines) {
    SendMessageW(hEngineCombo, CB_ADDSTRING, 0, (LPARAM)Localization::Get(id));
  }

  int selIdx = 0;
  if (pUnit->comparisonMode)
    selIdx = 4;
  else if (pUnit->shadowCopyMode)
    selIdx = 3;
  else if (pUnit->blockCloneMode)
    selIdx = 2;
  else if (pUnit->parallelMode)
    selIdx = 1;
  SendMessageW(hEngineCombo, CB_SETCURSEL, selIdx, 0);

  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Btn_Save), BS_PUSHBUTTON,
             300, 445, 240, 30, IDC_BTN_UNIT_SAVE);
}

static void SaveUnitEditDlgData(HWND hWnd, BackupUnit *pUnit) {
  wchar_t buf[MAX_PATH];
  wchar_t srcBuf[MAX_PATH], dstBuf[MAX_PATH];
  GetDlgItemTextW(hWnd, IDC_EDIT_UNIT_SRC, srcBuf, MAX_PATH);
  GetDlgItemTextW(hWnd, IDC_EDIT_UNIT_DST, dstBuf, MAX_PATH);

  if (IsPathOverlap(srcBuf, dstBuf)) {
    MessageBoxW(hWnd, Localization::Get(StrId::Err_PathOverlap),
                Localization::Get(StrId::Main_Error), MB_OK | MB_ICONERROR);
    return;
  }

  GetDlgItemTextW(hWnd, IDC_EDIT_UNIT_NAME, buf, MAX_PATH);
  pUnit->name = buf;
  pUnit->source = srcBuf;
  pUnit->target = dstBuf;

  if (IsDlgButtonChecked(hWnd, IDC_RAD_SYNC) == BST_CHECKED)
    pUnit->mode = BackupMode::Sync;
  else if (IsDlgButtonChecked(hWnd, IDC_RAD_MODE_VERIFY) == BST_CHECKED)
    pUnit->mode = BackupMode::Verify;
  else
    pUnit->mode = BackupMode::Copy;

  pUnit->verify = IsDlgButtonChecked(hWnd, IDC_RAD_VERIFY_ON) == BST_CHECKED;
  pUnit->criteriaSize =
      IsDlgButtonChecked(hWnd, IDC_CHK_CRITERIA_SIZE) == BST_CHECKED;
  pUnit->errorPolicy =
      IsDlgButtonChecked(hWnd, IDC_RAD_POLICY_SUSP) == BST_CHECKED
          ? ErrorPolicy::Suspend
          : ErrorPolicy::Continue;

  int selEng =
      (int)SendMessageW(GetDlgItem(hWnd, IDC_CB_ENGINE), CB_GETCURSEL, 0, 0);
  pUnit->parallelMode = (selEng == 1);
  pUnit->blockCloneMode = (selEng == 2);
  pUnit->shadowCopyMode = (selEng == 3);
  pUnit->comparisonMode = (selEng == 4);

  DestroyWindow(hWnd);
}

static void InitializeSetEditDlg(HWND hWnd, BackupSet *pSet, HFONT hFont) {
  auto CreateCtrl = [&](const wchar_t *className, const wchar_t *text,
                        DWORD style, int x, int y, int w, int h, int id) {
    HWND hCtrl =
        CreateWindowW(className, text, style | WS_CHILD | WS_VISIBLE, x, y, w,
                      h, hWnd, (HMENU)(UINT_PTR)id, hInst, NULL);
    if (hCtrl)
      SendMessageW(hCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);
    return hCtrl;
  };

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_SetName), 0, 20, 10, 200,
             20, 0);
  CreateCtrl(L"EDIT", pSet->name.c_str(), WS_BORDER | ES_AUTOHSCROLL, 20, 30,
             440, 25, IDC_EDIT_SET_NAME);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Description), 0, 20, 60,
             200, 20, 0);
  CreateCtrl(L"EDIT", pSet->description.c_str(), WS_BORDER | ES_AUTOHSCROLL, 20,
             80, 440, 25, IDC_EDIT_SET_DESC);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_UnitsInSet), 0, 20, 110,
             200, 20, 0);
  HWND hList = CreateCtrl(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                          20, 130, 340, 120, IDC_DLG_LIST_UNITS);

  for (const auto &u : pSet->units) {
    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)u.name.c_str());
  }

  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Btn_Up), BS_PUSHBUTTON,
             370, 130, 90, 25, IDC_BTN_UNIT_UP);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Btn_Down), BS_PUSHBUTTON,
             370, 160, 90, 25, IDC_BTN_UNIT_DOWN);

  // Schedule UI
  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_SchedFrq), 0, 20, 260, 200,
             20, 0);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Sched_Manual),
             BS_AUTORADIOBUTTON | WS_GROUP, 20, 280, 100, 25,
             IDC_RAD_SCHED_MANUAL);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Sched_Daily),
             BS_AUTORADIOBUTTON, 125, 280, 80, 25, IDC_RAD_SCHED_DAILY);
  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Sched_Weekly),
             BS_AUTORADIOBUTTON, 215, 280, 100, 25, IDC_RAD_SCHED_WEEKLY);

  if (pSet->scheduleFreq == ScheduleFrequency::Manual)
    CheckDlgButton(hWnd, IDC_RAD_SCHED_MANUAL, BST_CHECKED);
  else if (pSet->scheduleFreq == ScheduleFrequency::Daily)
    CheckDlgButton(hWnd, IDC_RAD_SCHED_DAILY, BST_CHECKED);
  else
    CheckDlgButton(hWnd, IDC_RAD_SCHED_WEEKLY, BST_CHECKED);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Time_Title), 0, 320, 260,
             150, 20, 0);
  wchar_t hBuf[5], mBuf[5];
  swprintf(hBuf, 5, L"%02d", pSet->scheduleHour);
  swprintf(mBuf, 5, L"%02d", pSet->scheduleMinute);
  CreateCtrl(L"EDIT", hBuf, WS_BORDER | ES_NUMBER, 320, 280, 30, 25,
             IDC_EDIT_SCHED_HOUR);
  CreateCtrl(L"STATIC", L":", 0, 355, 280, 10, 25, 0);
  CreateCtrl(L"EDIT", mBuf, WS_BORDER | ES_NUMBER, 365, 280, 30, 25,
             IDC_EDIT_SCHED_MIN);

  CreateCtrl(L"STATIC", Localization::Get(StrId::Dlg_Day_Title), 0, 20, 315,
             200, 20, 0);
  HWND hCombo = CreateCtrl(L"COMBOBOX", L"", CBS_DROPDOWNLIST, 20, 335, 150,
                           200, IDC_COMBO_WEEKDAY);

  const StrId days[] = {StrId::Day_Sun, StrId::Day_Mon, StrId::Day_Tue,
                        StrId::Day_Wed, StrId::Day_Thu, StrId::Day_Fri,
                        StrId::Day_Sat};
  for (int i = 0; i < 7; ++i)
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)Localization::Get(days[i]));
  SendMessageW(hCombo, CB_SETCURSEL, pSet->scheduleDayOfWeek, 0);

  CreateCtrl(L"BUTTON", Localization::Get(StrId::Dlg_Btn_SaveSet),
             BS_PUSHBUTTON, 300, 335, 240, 30, IDC_BTN_SET_SAVE);
}

static void SaveSetEditDlgData(HWND hWnd, BackupSet *pSet) {
  wchar_t buf[MAX_PATH];
  GetDlgItemTextW(hWnd, IDC_EDIT_SET_NAME, buf, MAX_PATH);
  pSet->name = buf;
  GetDlgItemTextW(hWnd, IDC_EDIT_SET_DESC, buf, MAX_PATH);
  pSet->description = buf;

  if (IsDlgButtonChecked(hWnd, IDC_RAD_SCHED_MANUAL))
    pSet->scheduleFreq = ScheduleFrequency::Manual;
  else if (IsDlgButtonChecked(hWnd, IDC_RAD_SCHED_DAILY))
    pSet->scheduleFreq = ScheduleFrequency::Daily;
  else
    pSet->scheduleFreq = ScheduleFrequency::Weekly;

  GetDlgItemTextW(hWnd, IDC_EDIT_SCHED_HOUR, buf, MAX_PATH);
  pSet->scheduleHour = _wtoi(buf);
  GetDlgItemTextW(hWnd, IDC_EDIT_SCHED_MIN, buf, MAX_PATH);
  pSet->scheduleMinute = _wtoi(buf);
  pSet->scheduleDayOfWeek = (int)SendMessageW(
      GetDlgItem(hWnd, IDC_COMBO_WEEKDAY), CB_GETCURSEL, 0, 0);

  DestroyWindow(hWnd);
}

static void HandleSetUnitMovement(HWND hWnd, BackupSet *pSet, bool moveUp) {
  int idx =
      (int)SendDlgItemMessage(hWnd, IDC_DLG_LIST_UNITS, LB_GETCURSEL, 0, 0);
  if (moveUp) {
    if (idx > 0) {
      std::swap(pSet->units[idx], pSet->units[idx - 1]);
      idx--;
    } else
      return;
  } else {
    if (idx >= 0 && idx < (int)pSet->units.size() - 1) {
      std::swap(pSet->units[idx], pSet->units[idx + 1]);
      idx++;
    } else
      return;
  }

  HWND hList = GetDlgItem(hWnd, IDC_DLG_LIST_UNITS);
  SendMessage(hList, LB_RESETCONTENT, 0, 0);
  for (const auto &u : pSet->units)
    SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)u.name.c_str());
  SendMessage(hList, LB_SETCURSEL, idx, 0);
}
LRESULT CALLBACK UnitEditDlgProc(HWND hWnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  static BackupUnit *pUnit = nullptr;
  static HFONT hFont = NULL;

  switch (message) {
  case WM_CREATE:
    pUnit = (BackupUnit *)((LPCREATESTRUCT)lParam)->lpCreateParams;
    hFont = CreateDialogFont();
    InitializeUnitEditDlg(hWnd, pUnit, hFont);
    break;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_BTN_BROWSE_SRC: {
      std::wstring path;
      if (BrowseForFolder(hWnd, path))
        SetDlgItemTextW(hWnd, IDC_EDIT_UNIT_SRC, path.c_str());
    } break;
    case IDC_BTN_BROWSE_DST: {
      std::wstring path;
      if (BrowseForFolder(hWnd, path))
        SetDlgItemTextW(hWnd, IDC_EDIT_UNIT_DST, path.c_str());
    } break;
    case IDC_BTN_NET_CONN:
      WNetConnectionDialog(hWnd, RESOURCETYPE_DISK);
      break;
    case IDC_BTN_UNIT_SAVE:
      SaveUnitEditDlgData(hWnd, pUnit);
      break;
    }
    break;

  case WM_CLOSE:
    DestroyWindow(hWnd);
    break;

  case WM_DESTROY:
    if (hFont)
      DeleteObject(hFont);
    break;

  default:
    return DefWindowProcW(hWnd, message, wParam, lParam);
  }
  return 0;
}

struct HistoryEntry {
  std::wstring fileName;
  std::wstring taskName;
  std::wstring dateTime;
  bool hasError = false;
};

LRESULT CALLBACK LogHistoryDlgProc(HWND hWnd, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
  HFONT hFont = (HFONT)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  std::vector<HistoryEntry> *pEntries =
      (std::vector<HistoryEntry> *)GetPropW(hWnd, L"Entries");

  switch (message) {
  case WM_CREATE: {
    hFont =
        CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)hFont);

    HWND hList = CreateWindowExW(
        0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER, 10, 10,
        580, 400, hWnd, (HMENU)ID_LIST_HISTORY, hInst, NULL);

    SendMessageW(hList, WM_SETFONT, (WPARAM)hFont, TRUE);
    ListView_SetExtendedListViewStyle(hList,
                                      LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.cx = 200;
    lvc.pszText = (LPWSTR)Localization::Get(StrId::Dlg_History_Date);
    ListView_InsertColumn(hList, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.cx = 360;
    lvc.pszText = (LPWSTR)Localization::Get(StrId::Dlg_History_Task);
    ListView_InsertColumn(hList, 1, &lvc);

    HWND hBtnOpen =
        CreateWindowW(L"BUTTON", Localization::Get(StrId::Dlg_History_Open),
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 450, 420, 140, 30,
                      hWnd, (HMENU)ID_BTN_HISTORY_OPEN, hInst, NULL);
    SendMessageW(hBtnOpen, WM_SETFONT, (WPARAM)hFont, TRUE);

    pEntries = new std::vector<HistoryEntry>();
    SetPropW(hWnd, L"Entries", (HANDLE)pEntries);

    std::wstring cfgPath = ConfigManager::GetConfigPath();
    fs::path logDir = fs::path(cfgPath).parent_path();

    try {
      if (fs::exists(logDir)) {
        for (const auto &entry : fs::directory_iterator(logDir)) {
          if (fs::is_regular_file(entry.status()) &&
              entry.path().extension() == ".txt") {
            std::wstring fname = entry.path().filename().wstring();
            if (fname == L"config.txt" || fname == L"last_run.txt")
              continue;

            bool isErr = (fname.find(L"_ERR.txt") != std::wstring::npos);
            std::wstring cleanName = fname;
            if (isErr) {
              size_t pos = cleanName.find(L"_ERR.txt");
              cleanName.replace(pos, 8, L".txt");
            }

            size_t lastUnder = cleanName.find_last_of(L'_');
            if (lastUnder != std::wstring::npos && lastUnder > 0) {
              std::wstring task = cleanName.substr(0, lastUnder);
              std::wstring tsPart = cleanName.substr(lastUnder + 1);
              // Expected format: YYYYMMDD_HHMMSS.txt (15+ chars excluding .txt)
              if (tsPart.length() >= 15) {
                std::wstring datePart = tsPart.substr(0, 8);
                std::wstring timePart = tsPart.substr(9, 6);
                if (datePart.length() == 8 && timePart.length() == 6) {
                  std::wstring formattedDate =
                      datePart.substr(0, 4) + L"/" + datePart.substr(4, 2) +
                      L"/" + datePart.substr(6, 2) + L" " +
                      timePart.substr(0, 2) + L":" + timePart.substr(2, 2) +
                      L":" + timePart.substr(4, 2);

                  HistoryEntry he;
                  he.fileName = entry.path().wstring();
                  he.taskName = task + (isErr ? L" [ERROR]" : L"");
                  he.dateTime = formattedDate;
                  he.hasError = isErr;
                  pEntries->push_back(he);
                }
              }
            }
          }
        }
      }
    } catch (...) {
    }

    std::sort(pEntries->begin(), pEntries->end(),
              [](const HistoryEntry &a, const HistoryEntry &b) {
                return a.dateTime > b.dateTime;
              });

    for (int i = 0; i < (int)pEntries->size(); ++i) {
      LVITEMW lvi = {0};
      lvi.mask = LVIF_TEXT | LVIF_PARAM;
      lvi.iItem = i;
      lvi.iSubItem = 0;
      lvi.pszText = (LPWSTR)(*pEntries)[i].dateTime.c_str();
      lvi.lParam = i;
      ListView_InsertItem(hList, &lvi);
      ListView_SetItemText(hList, i, 1,
                           (LPWSTR)(*pEntries)[i].taskName.c_str());
    }
    break;
  }
  case WM_COMMAND:
    if (LOWORD(wParam) == ID_BTN_HISTORY_OPEN) {
      HWND hList = GetDlgItem(hWnd, ID_LIST_HISTORY);
      int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
      if (sel != -1 && pEntries) {
        ShellExecuteW(NULL, L"open", (*pEntries)[sel].fileName.c_str(), NULL,
                      NULL, SW_SHOWNORMAL);
      }
    }
    break;
  case WM_NOTIFY: {
    NMHDR *nm = (NMHDR *)lParam;
    if (nm->idFrom == ID_LIST_HISTORY && nm->code == NM_DBLCLK) {
      HWND hList = GetDlgItem(hWnd, ID_LIST_HISTORY);
      int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
      if (sel != -1 && pEntries) {
        ShellExecuteW(NULL, L"open", (*pEntries)[sel].fileName.c_str(), NULL,
                      NULL, SW_SHOWNORMAL);
      }
    }
    break;
  }
  case WM_CLOSE:
    DestroyWindow(hWnd);
    break;
  case WM_DESTROY: {
    hFont = (HFONT)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (hFont)
      DeleteObject(hFont);
    pEntries = (std::vector<HistoryEntry> *)GetPropW(hWnd, L"Entries");
    if (pEntries)
      delete pEntries;
    RemovePropW(hWnd, L"Entries");
    break;
  }
  default:
    return DefWindowProcW(hWnd, message, wParam, lParam);
  }
  return 0;
}

static void OnLogHistory(HWND hWnd) {
  HWND hHistory = CreateWindowExW(
      0, L"LogHistoryClass", Localization::Get(StrId::Dlg_History_Title),
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
      620, 520, hWnd, NULL, hInst, NULL);
  ShowWindow(hHistory, SW_SHOW);
}

LRESULT CALLBACK SetEditDlgProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  static BackupSet *pSet = nullptr;
  static HFONT hFont = NULL;

  switch (message) {
  case WM_CREATE:
    pSet = (BackupSet *)((LPCREATESTRUCT)lParam)->lpCreateParams;
    hFont = CreateDialogFont();
    InitializeSetEditDlg(hWnd, pSet, hFont);
    break;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_BTN_UNIT_UP:
      HandleSetUnitMovement(hWnd, pSet, true);
      break;
    case IDC_BTN_UNIT_DOWN:
      HandleSetUnitMovement(hWnd, pSet, false);
      break;
    case IDC_BTN_SET_SAVE:
      SaveSetEditDlgData(hWnd, pSet);
      break;
    }
    break;

  case WM_CLOSE:
    DestroyWindow(hWnd);
    break;

  case WM_DESTROY:
    if (hFont)
      DeleteObject(hFont);
    break;

  default:
    return DefWindowProcW(hWnd, message, wParam, lParam);
  }
  return 0;
}

enum class RunMode { Backup, Preview, Verify };

void OnSetUpdate(HWND hWnd) {
  if (g_selectedSetIndex < 0)
    return;

  HWND hDlg =
      CreateWindowExW(WS_EX_DLGMODALFRAME, L"SetEditDlgClass",
                      Localization::Get(StrId::Ctx_EditSet),
                      WS_VISIBLE | WS_SYSMENU | WS_CAPTION, 200, 200, 500, 420,
                      hWnd, NULL, hInst, &g_backupSets[g_selectedSetIndex]);

  if (!hDlg) {
    MessageBoxW(hWnd, L"Error: Could not open Set Edit Dialogue.",
                Localization::Get(StrId::Main_Error), MB_OK | MB_ICONERROR);
    return;
  }

  EnableWindow(hWnd, FALSE);
  MSG msg;
  while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
    if (!IsDialogMessage(hDlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  EnableWindow(hWnd, TRUE);
  SetForegroundWindow(hWnd);
  ConfigManager::Save(g_backupSets);
  RefreshTreeView();
}

void OnUnitUpdate(HWND hWnd) {
  if (g_selectedSetIndex < 0 || g_selectedUnitIndex < 0)
    return;

  HWND hDlg = CreateWindowExW(
      WS_EX_DLGMODALFRAME, L"UnitEditDlgClass",
      Localization::Get(StrId::Ctx_EditUnit),
      WS_VISIBLE | WS_SYSMENU | WS_CAPTION, 200, 200, 560, 540, hWnd, NULL,
      hInst, &g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex]);

  if (!hDlg) {
    MessageBoxW(hWnd, L"Error: Could not open Unit Edit Dialogue.",
                Localization::Get(StrId::Main_Error), MB_OK | MB_ICONERROR);
    return;
  }

  EnableWindow(hWnd, FALSE);
  MSG msg;
  while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  EnableWindow(hWnd, TRUE);
  SetForegroundWindow(hWnd);
  ConfigManager::Save(g_backupSets);
  RefreshTreeView();
}

void OnSetCreate(HWND /*hWnd*/) {
  BackupSet ns;
  ns.name = Localization::Get(StrId::Dlg_NewSet_Default);
  ns.description = Localization::Get(StrId::Dlg_NewSet_Desc_Default);
  g_backupSets.push_back(ns);
  ConfigManager::Save(g_backupSets);
  RefreshTreeView();
}
void OnUnitCreate(HWND hWnd) {
  if (g_selectedSetIndex >= 0) {
    BackupUnit nu;
    nu.name = Localization::Get(StrId::Dlg_NewUnit_Default);
    nu.source = L"C:\\";
    nu.target = L"D:\\";
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"UnitCreateDlgClass",
                                Localization::Get(StrId::Ctx_AddUnit),
                                WS_VISIBLE | WS_SYSMENU | WS_CAPTION, 200, 200,
                                560, 540, hWnd, NULL, hInst, &nu);
    if (!hDlg)
      return;

    EnableWindow(hWnd, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
      if (!IsDialogMessage(hDlg, &msg)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    EnableWindow(hWnd, TRUE);
    SetForegroundWindow(hWnd);
    UpdateStatusText();

    if (hDlg) {
      g_backupSets[g_selectedSetIndex].units.push_back(nu);
      ConfigManager::Save(g_backupSets);
      RefreshTreeView();
    }
  } else {
    MessageBoxW(hWnd, Localization::Get(StrId::Msg_SelectFirst),
                Localization::Get(StrId::Main_Error), MB_OK | MB_ICONWARNING);
  }
}

// ============================================================================
// Layout and UI Infrastructure
// ============================================================================

static void InitializeMainUI(HWND hWnd) {
  // 1. Menus
  HMENU hMenu = CreateMenu();
  HMENU hFile = CreateMenu();
  AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT,
              Localization::Get(StrId::Menu_Exit));
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile,
              Localization::Get(StrId::Menu_File));

  HMENU hSet = CreateMenu();
  AppendMenuW(hSet, MF_STRING, IDM_SET_CREATE,
              Localization::Get(StrId::Menu_NewSet));
  AppendMenuW(hSet, MF_STRING, IDM_SET_UPDATE,
              Localization::Get(StrId::Ctx_EditSet));
  AppendMenuW(hSet, MF_STRING, IDM_SET_DUPLICATE,
              Localization::Get(StrId::Ctx_Duplicate));
  AppendMenuW(hSet, MF_STRING, IDM_SET_DELETE,
              Localization::Get(StrId::Ctx_Delete));
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSet,
              Localization::Get(StrId::Menu_Set));

  HMENU hUnit = CreateMenu();
  AppendMenuW(hUnit, MF_STRING, IDM_UNIT_CREATE,
              Localization::Get(StrId::Ctx_AddUnit));
  AppendMenuW(hUnit, MF_STRING, IDM_UNIT_UPDATE,
              Localization::Get(StrId::Ctx_EditUnit));
  AppendMenuW(hUnit, MF_STRING, IDM_UNIT_DUPLICATE,
              Localization::Get(StrId::Ctx_Duplicate));
  AppendMenuW(hUnit, MF_STRING, IDM_UNIT_DELETE,
              Localization::Get(StrId::Ctx_Delete));
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hUnit,
              Localization::Get(StrId::Menu_Unit));

  HMENU hBackup = CreateMenu();
  AppendMenuW(hBackup, MF_STRING, IDM_BACKUP_RUN,
              Localization::Get(StrId::Menu_Run));
  AppendMenuW(hBackup, MF_STRING, IDM_BACKUP_PREVIEW,
              Localization::Get(StrId::Menu_Preview));
  AppendMenuW(hBackup, MF_STRING, IDM_BACKUP_VERIFY,
              Localization::Get(StrId::Menu_Verify));
  AppendMenuW(hBackup, MF_SEPARATOR, 0, NULL);
  AppendMenuW(hBackup, MF_STRING, IDM_BACKUP_STOP,
              Localization::Get(StrId::Main_Stop));
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBackup,
              Localization::Get(StrId::Menu_Backup));

  HMENU hLog = CreateMenu();
  AppendMenuW(hLog, MF_STRING, IDM_LOG_HISTORY,
              Localization::Get(StrId::Menu_LogHistory));
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLog,
              Localization::Get(StrId::Menu_Log));

  HMENU hHelp = CreateMenu();
  AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT,
              Localization::Get(StrId::Menu_About));
  AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp,
              Localization::Get(StrId::Menu_Help));

  SetMenu(hWnd, hMenu);

  // 2. Status Bar
  hStatusBar =
      CreateWindowExW(0, STATUSCLASSNAMEW, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0,
                      0, hWnd, (HMENU)ID_STATUS_BAR, hInst, NULL);

  // 3. Tree Views
  InitImageList();
  hTreeView =
      CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                      WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT |
                          TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
                      0, 0, 0, 0, hWnd, (HMENU)ID_TREE_VIEW, hInst, NULL);
  TreeView_SetImageList(hTreeView, g_hImageList, TVSIL_NORMAL);

  hSrcTreeView = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
      WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
      0, 0, 0, 0, hWnd, (HMENU)ID_SRC_TREE_VIEW, hInst, NULL);
  TreeView_SetImageList(hSrcTreeView, g_hImageList, TVSIL_NORMAL);

  hDstTreeView = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
      WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
      0, 0, 0, 0, hWnd, (HMENU)ID_DST_TREE_VIEW, hInst, NULL);
  TreeView_SetImageList(hDstTreeView, g_hImageList, TVSIL_NORMAL);

  // 4. Labels
  hSrcPathLabel = CreateWindowW(L"STATIC", L"Source Path: ",
                                WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 0,
                                0, hWnd, (HMENU)ID_SRC_PATH_LABEL, hInst, NULL);
  hDstPathLabel = CreateWindowW(L"STATIC", L"Target Path: ",
                                WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS, 0, 0, 0,
                                0, hWnd, (HMENU)ID_DST_PATH_LABEL, hInst, NULL);
  hProgressLabel =
      CreateWindowW(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_ENDELLIPSIS,
                    0, 0, 0, 0, hWnd, (HMENU)ID_PROGRESS_LABEL, hInst, NULL);

  // 5. Buttons
  hStopButton = CreateWindowW(L"BUTTON", Localization::Get(StrId::Main_Stop),
                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                              hWnd, (HMENU)IDM_BACKUP_STOP, hInst, NULL);
  EnableWindow(hStopButton, FALSE);

  hLogMaxButton =
      CreateWindowW(L"BUTTON", Localization::Get(StrId::Main_MaxLog),
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd,
                    (HMENU)IDM_LOG_MAXIMIZE, hInst, NULL);

  // 6. Progress Bars
  hProgressBar =
      CreateWindowExW(0, PROGRESS_CLASSW, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0,
                      0, hWnd, (HMENU)ID_PROGRESS_BAR, hInst, NULL);
  SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

  // 7. Main Window Property Selectors (Quick Edit)
  hMainModeCombo =
      CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                    0, 0, 0, 0, hWnd, (HMENU)ID_MAIN_MODE_COMBO, hInst, NULL);
  SendMessageW(hMainModeCombo, CB_ADDSTRING, 0,
               (LPARAM)Localization::Get(StrId::Dlg_Mode_Copy));
  SendMessageW(hMainModeCombo, CB_ADDSTRING, 0,
               (LPARAM)Localization::Get(StrId::Dlg_Mode_Sync));
  SendMessageW(hMainModeCombo, CB_ADDSTRING, 0,
               (LPARAM)Localization::Get(StrId::Dlg_Mode_VerifyOnly));

  hMainStrategyCombo = CreateWindowW(
      L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 0, 0, 0, 0,
      hWnd, (HMENU)ID_MAIN_STRATEGY_COMBO, hInst, NULL);
  const StrId engines[] = {StrId::Eng_Std, StrId::Eng_Par, StrId::Eng_Blk,
                           StrId::Eng_Vss, StrId::Eng_Cmp};
  for (auto id : engines) {
    SendMessageW(hMainStrategyCombo, CB_ADDSTRING, 0,
                 (LPARAM)Localization::Get(id));
  }

  // 8. Log Area
  hLogEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                 ES_AUTOVSCROLL | ES_READONLY,
                             0, 0, 0, 0, hWnd, (HMENU)ID_LOG_EDIT, hInst, NULL);
  SendMessageW(hLogEdit, EM_SETLIMITTEXT, 0, 0);

  // 9. Worker Controls
  for (int i = 0; i < MAX_WORKERS; ++i) {
    hWorkerLabels[i] =
        CreateWindowW(L"STATIC", L"", WS_CHILD, 0, 0, 0, 0, hWnd,
                      (HMENU)(UINT_PTR)(ID_WORKER_BASE_LABEL + i), hInst, NULL);
    hWorkerProgs[i] = CreateWindowExW(
        0, PROGRESS_CLASSW, NULL, WS_CHILD, 0, 0, 0, 0, hWnd,
        (HMENU)(UINT_PTR)(ID_WORKER_BASE_PROG + i), hInst, NULL);
    hWorkerBtns[i] = CreateWindowW(
        L"BUTTON", L"X", WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, hWnd,
        (HMENU)(UINT_PTR)(ID_WORKER_BASE_BTN + i), hInst, NULL);
    SendMessage(hWorkerProgs[i], PBM_SETRANGE, 0, MAKELPARAM(0, 100));
  }

  // 10. Logger and Initial Data
  g_logger = new WindowLogger(hLogEdit, hProgressLabel, hProgressBar,
                              hWorkerProgs, hWorkerBtns, hWorkerLabels);

  g_backupSets = ConfigManager::Load();
  PopulateDriveTree(hSrcTreeView);
  PopulateDriveTree(hDstTreeView);
  RefreshTreeView();

  SetTimer(hWnd, 2001, 15000, NULL); // Schedule check every 15s
}

static void ResizeMainLayout(HWND /*hWnd*/, int W, int H) {
  if (W == 0 || H == 0)
    return;

  if (g_logMaximized) {
    // Hide Management and Explorer Views
    ShowWindow(hTreeView, SW_HIDE);
    ShowWindow(hSrcTreeView, SW_HIDE);
    ShowWindow(hDstTreeView, SW_HIDE);
    ShowWindow(hSrcPathLabel, SW_HIDE);
    ShowWindow(hDstPathLabel, SW_HIDE);

    // Position Header
    MoveWindow(hProgressLabel, 10, 10, W - 250, 20, TRUE);
    MoveWindow(hProgressBar, 10, 40, W - 250, 25, TRUE);
    MoveWindow(hStopButton, W - 230, 8, 220, 25, TRUE);
    MoveWindow(hLogMaxButton, W - 230, 40, 220, 25, TRUE);

    // Worker Dashboard
    int dashY = 80;
    for (int i = 0; i < MAX_WORKERS; ++i) {
      ShowWindow(hWorkerLabels[i], SW_SHOW);
      ShowWindow(hWorkerProgs[i], SW_SHOW);
      ShowWindow(hWorkerBtns[i], SW_SHOW);
      MoveWindow(hWorkerLabels[i], 10, dashY + (i * 22), 250, 18, TRUE);
      MoveWindow(hWorkerProgs[i], 265, dashY + (i * 22), W - 335, 18, TRUE);
      MoveWindow(hWorkerBtns[i], W - 65, dashY + (i * 22), 55, 18, TRUE);
    }

    // Log Area (Maximized)
    int logTop = dashY + (MAX_WORKERS * 22) + 10;
    MoveWindow(hLogEdit, 10, logTop, W - 20, H - logTop - 30, TRUE);

  } else {
    // Show Management and Explorer Views
    ShowWindow(hTreeView, SW_SHOW);
    ShowWindow(hSrcTreeView, SW_SHOW);
    ShowWindow(hDstTreeView, SW_SHOW);
    ShowWindow(hSrcPathLabel, SW_SHOW);
    ShowWindow(hDstPathLabel, SW_SHOW);

    // Hide Workers in Normal Mode
    for (int i = 0; i < MAX_WORKERS; ++i) {
      ShowWindow(hWorkerLabels[i], SW_HIDE);
      ShowWindow(hWorkerProgs[i], SW_HIDE);
      ShowWindow(hWorkerBtns[i], SW_HIDE);
    }

    // Normal Three-Pane Layout
    int treeW = W / 3;
    int topH = H - 250;
    MoveWindow(hTreeView, 0, 30, treeW, topH, TRUE);
    MoveWindow(hSrcTreeView, treeW, 30, treeW, topH, TRUE);
    MoveWindow(hDstTreeView, treeW * 2, 30, W - (treeW * 2), topH, TRUE);

    MoveWindow(hSrcPathLabel, treeW + 5, topH + 35, treeW - 10, 20, TRUE);
    MoveWindow(hDstPathLabel, (treeW * 2) + 5, topH + 35, treeW - 10, 20, TRUE);

    // Bottom Controls
    int ctrlY = topH + 50;
    MoveWindow(hProgressLabel, 10, ctrlY, W - 250, 20, TRUE);
    MoveWindow(hProgressBar, 10, ctrlY + 25, W - 250, 25, TRUE);
    MoveWindow(hStopButton, W - 230, ctrlY, 220, 25, TRUE);
    MoveWindow(hLogMaxButton, W - 230, ctrlY + 30, 220, 25, TRUE);

    // Quick Edit Combos and Log Area
    int logTop = ctrlY + 65;
    int comboW = 160;
    int rightLimit = W - 10;

    MoveWindow(hMainModeCombo, rightLimit - comboW, logTop, comboW, 200, TRUE);
    MoveWindow(hMainStrategyCombo, rightLimit - (comboW * 2) - 10, logTop,
               comboW, 200, TRUE);

    // Log Area (Normal)
    int logW = rightLimit - (comboW * 2) - 20;
    MoveWindow(hLogEdit, 0, logTop, logW, H - logTop - 30, TRUE);
  }
}

static void HandleNotify(HWND hWnd, LPARAM lParam) {
  LPNMHDR nm = (LPNMHDR)lParam;
  if (nm->idFrom == ID_TREE_VIEW) {
    if (nm->code == TVN_SELCHANGEDW) {
      LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
      LPARAM lp = pnmtv->itemNew.lParam;
      int setIdx = (int)((lp >> 16) & 0xFFFF);
      int unitIdx = (int)(lp & 0xFFFF);

      g_selectedSetIndex = setIdx;
      g_selectedUnitIndex = (unitIdx == 0xFFFF) ? -1 : unitIdx;

      UpdateStatusText();

      if (g_selectedSetIndex >= 0 && g_selectedUnitIndex >= 0) {
        BackupUnit &u =
            g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex];
        ExpandTreeToPath(hSrcTreeView, u.source);
        ExpandTreeToPath(hDstTreeView, u.target);

        // Update main window property selectors
        int modeIdx = 0;
        if (u.mode == BackupMode::Sync)
          modeIdx = 1;
        else if (u.mode == BackupMode::Verify)
          modeIdx = 2;
        SendMessageW(hMainModeCombo, CB_SETCURSEL, modeIdx, 0);

        int engIdx = 0;
        if (u.comparisonMode)
          engIdx = 4;
        else if (u.shadowCopyMode)
          engIdx = 3;
        else if (u.blockCloneMode)
          engIdx = 2;
        else if (u.parallelMode)
          engIdx = 1;
        SendMessageW(hMainStrategyCombo, CB_SETCURSEL, engIdx, 0);
      }
    } else if (nm->code == NM_RCLICK) {
      TVHITTESTINFO ht = {0};
      GetCursorPos(&ht.pt);
      ScreenToClient(hTreeView, &ht.pt);
      HTREEITEM hItem = TreeView_HitTest(hTreeView, &ht);
      if (hItem) {
        TreeView_SelectItem(hTreeView, hItem);
        HMENU hMainPopup = CreatePopupMenu();
        if (g_selectedUnitIndex >= 0) {
          AppendMenuW(hMainPopup, MF_STRING, IDM_UNIT_UPDATE,
                      Localization::Get(StrId::Ctx_EditUnit));
          AppendMenuW(hMainPopup, MF_STRING, IDM_UNIT_DUPLICATE,
                      Localization::Get(StrId::Ctx_Duplicate));
          AppendMenuW(hMainPopup, MF_STRING, IDM_UNIT_DELETE,
                      Localization::Get(StrId::Ctx_Delete));
        } else if (g_selectedSetIndex >= 0) {
          AppendMenuW(hMainPopup, MF_STRING, IDM_UNIT_CREATE,
                      Localization::Get(StrId::Ctx_AddUnit));
          AppendMenuW(hMainPopup, MF_SEPARATOR, 0, NULL);
          AppendMenuW(hMainPopup, MF_STRING, IDM_SET_UPDATE,
                      Localization::Get(StrId::Ctx_EditSet));
          AppendMenuW(hMainPopup, MF_STRING, IDM_SET_DUPLICATE,
                      Localization::Get(StrId::Ctx_Duplicate));
          AppendMenuW(hMainPopup, MF_STRING, IDM_SET_DELETE,
                      Localization::Get(StrId::Ctx_Delete));
        }
        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(hMainPopup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hMainPopup);
      }
    }
  } else if (nm->idFrom == ID_SRC_TREE_VIEW || nm->idFrom == ID_DST_TREE_VIEW) {
    HWND hTree = nm->hwndFrom;
    if (nm->code == TVN_ITEMEXPANDINGW) {
      LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
      if (pnmtv->action == TVE_EXPAND) {
        PopulateDirBranch(hTree, pnmtv->itemNew.hItem);
        TVITEMW tvi = {0};
        tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_HANDLE;
        tvi.hItem = pnmtv->itemNew.hItem;
        tvi.iImage = IDX_FOLDER_OPEN;
        tvi.iSelectedImage = IDX_FOLDER_OPEN;
        TreeView_SetItem(hTree, &tvi);
      } else if (pnmtv->action == TVE_COLLAPSE) {
        TVITEMW tvi = {0};
        tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_HANDLE;
        tvi.hItem = pnmtv->itemNew.hItem;
        tvi.iImage = IDX_FOLDER_CLOSED;
        tvi.iSelectedImage = IDX_FOLDER_CLOSED;
        TreeView_SetItem(hTree, &tvi);
      }
    } else if (nm->code == TVN_SELCHANGEDW) {
      LPNMTREEVIEWW pnmtv = (LPNMTREEVIEWW)lParam;
      std::wstring *pPath = (std::wstring *)pnmtv->itemNew.lParam;
      if (pPath) {
        std::wstring labelBase = Localization::Get(
            hTree == hSrcTreeView ? StrId::Main_SrcPath : StrId::Main_DstPath);
        std::wstring currentLabel = labelBase + L": " + *pPath;
        SetWindowTextW(hTree == hSrcTreeView ? hSrcPathLabel : hDstPathLabel,
                       currentLabel.c_str());
        AddToolTip(hWnd, hTree == hSrcTreeView ? hSrcPathLabel : hDstPathLabel,
                   pPath->c_str());
      }
    } else if (nm->code == NM_RCLICK) {
      TVHITTESTINFO ht = {0};
      GetCursorPos(&ht.pt);
      ScreenToClient(nm->hwndFrom, &ht.pt);
      HTREEITEM hItem = TreeView_HitTest(nm->hwndFrom, &ht);
      if (hItem) {
        TreeView_SelectItem(nm->hwndFrom, hItem);
        HMENU hPopup = CreatePopupMenu();
        if (nm->idFrom == ID_SRC_TREE_VIEW) {
          AppendMenuW(hPopup, MF_STRING, IDM_SET_AS_SOURCE,
                      Localization::Get(StrId::Ctx_SetAsSource));
        } else {
          AppendMenuW(hPopup, MF_STRING, IDM_SET_AS_TARGET,
                      Localization::Get(StrId::Ctx_SetAsTarget));
        }
        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(hPopup, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        DestroyMenu(hPopup);
      }
    }
  }
}

void RunBackup(RunMode runMode);

void CheckSchedules() {
  static std::wstring lastCheckSlot = L"";
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  struct tm timeinfo;
  localtime_s(&timeinfo, &in_time_t);

  std::wstringstream ss;
  ss << std::put_time(&timeinfo, L"%Y%m%d%H%M");
  std::wstring currentSlot = ss.str();
  if (currentSlot == lastCheckSlot)
    return;
  lastCheckSlot = currentSlot;

  bool changed = false;
  for (int i = 0; i < (int)g_backupSets.size(); ++i) {
    auto &s = g_backupSets[i];
    if (s.scheduleFreq == ScheduleFrequency::Manual)
      continue;

    bool shouldRun = false;
    if (timeinfo.tm_hour == s.scheduleHour &&
        timeinfo.tm_min == s.scheduleMinute) {
      if (s.scheduleFreq == ScheduleFrequency::Daily) {
        shouldRun = true;
      } else if (s.scheduleFreq == ScheduleFrequency::Weekly &&
                 timeinfo.tm_wday == s.scheduleDayOfWeek) {
        shouldRun = true;
      }
    }

    if (shouldRun && s.lastScheduledRun != currentSlot) {
      s.lastScheduledRun = currentSlot;
      changed = true;
      if (g_logger)
        g_logger->Log(L"--- [SCHEDULED RUN] Starting Set: " + s.name + L" ---");

      // We run it as a background task.
      // Note: RunBackup uses g_selectedSetIndex, so we temporarily set it.
      // This is a bit hacky but consistent with current design.
      int oldIndex = g_selectedSetIndex;
      int oldUnit = g_selectedUnitIndex;
      g_selectedSetIndex = i;
      g_selectedUnitIndex = -1;
      RunBackup(RunMode::Backup);
      g_selectedSetIndex = oldIndex;
      g_selectedUnitIndex = oldUnit;
    }
  }
  if (changed) {
    ConfigManager::Save(g_backupSets);
  }
}

void RunBackup(RunMode runMode) {
  bool dryRun = (runMode == RunMode::Preview);
  if (g_selectedSetIndex < 0) {
    MessageBoxW(hMainWindow, Localization::Get(StrId::Main_Error),
                Localization::Get(StrId::Main_Error), MB_OK | MB_ICONERROR);
    return;
  }

  if (g_currentEngine) {
    MessageBoxW(hMainWindow, Localization::Get(StrId::Err_AlreadyRunning),
                Localization::Get(StrId::Main_Error), MB_OK | MB_ICONERROR);
    return;
  }

  // Capture setup data to avoid race conditions
  std::vector<BackupUnit> unitsToRun;
  std::wstring taskTitle;
  if (g_selectedUnitIndex >= 0) {
    unitsToRun.push_back(
        g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex]);
    taskTitle = unitsToRun[0].name;
  } else {
    unitsToRun = g_backupSets[g_selectedSetIndex].units;
    taskTitle = g_backupSets[g_selectedSetIndex].name;
  }

  EnableWindow(hStopButton, TRUE);
  if (g_logger)
    g_logger->ClearErrorFlag();

  std::thread t([runMode, dryRun, unitsToRun, taskTitle]() {
    // RAII guard for power state
    struct PowerGuard {
      PowerGuard() {
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                                ES_AWAYMODE_REQUIRED);
      }
      ~PowerGuard() { SetThreadExecutionState(ES_CONTINUOUS); }
    } guard;

    BackupEngine engine(g_logger);
    g_currentEngine = &engine;

    for (const auto &u : unitsToRun) {
      if (engine.IsAborted())
        break;

      if (runMode == RunMode::Verify || u.comparisonMode)
        engine.SetStrategy(std::make_unique<ComparingBackupStrategy>());
      else if (u.blockCloneMode) {
        // Future: engine.SetStrategy(std::make_unique<BlockCloneStrategy>());
        // For now, fallback to Standard or Log
        engine.SetStrategy(std::make_unique<StandardBackupStrategy>());
        if (g_logger)
          g_logger->Log(L"NOTE: Block Clone Engine not fully implemented. "
                        L"Using Standard.");
      } else if (u.shadowCopyMode) {
        // Future: engine.SetStrategy(std::make_unique<VssBackupStrategy>());
        engine.SetStrategy(std::make_unique<StandardBackupStrategy>());
        if (g_logger)
          g_logger->Log(L"NOTE: VSS Engine requires Admin privs. Using "
                        L"Standard fallback.");
      } else if (u.parallelMode)
        engine.SetStrategy(std::make_unique<ParallelBackupStrategy>());
      else
        engine.SetStrategy(std::make_unique<StandardBackupStrategy>());

      BackupTask task;
      task.name = u.name;
      task.sourcePath = u.source;
      task.targetPath = u.target;
      task.mode = (runMode == RunMode::Verify) ? BackupMode::Verify : u.mode;
      task.verify = u.verify;
      task.errorPolicy = u.errorPolicy;
      engine.Run(task, dryRun);
    }

    if (g_logger) {
      g_logger->FinalizeLog(taskTitle);
      g_logger->Log(L"--- Task completed: " + taskTitle + L" ---");
    }

    g_currentEngine = nullptr;
    PostMessage(hMainWindow, WM_USER + 100, 0, 0);
  });
  t.detach();
}

// ============================================================================
// Command Handler Helper Functions
// ============================================================================

static void HandleCommand_FileExit(HWND hWnd) {
  if (MessageBoxW(hWnd, Localization::Get(StrId::Msg_ConfirmExit),
                  Localization::Get(StrId::Msg_ConfirmTitle),
                  MB_YESNO | MB_ICONQUESTION) == IDYES) {
    PostQuitMessage(0);
  }
}

static void HandleCommand_LogMaximize(HWND hWnd) {
  g_logMaximized = !g_logMaximized;
  SetWindowTextW(hLogMaxButton,
                 g_logMaximized ? Localization::Get(StrId::Main_RestoreLayout)
                                : Localization::Get(StrId::Main_MaxLog));
  RECT rc;
  GetClientRect(hWnd, &rc);
  PostMessage(hWnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));
}

static void HandleCommand_BackupStop() {
  if (g_currentEngine) {
    if (g_logger)
      g_logger->Log(L"ABORT REQUESTED BY USER...");
    g_currentEngine->Abort();
  }
}

static void HandleCommand_WorkerStop(int workerIndex) {
  if (g_currentEngine) {
    g_currentEngine->CancelWorker(workerIndex);
  }
}

static void HandleCommand_SetDuplicate() {
  if (g_selectedSetIndex != -1) {
    BackupSet copy = g_backupSets[g_selectedSetIndex];
    copy.name += L" - Copy";
    g_backupSets.push_back(copy);
    ConfigManager::Save(g_backupSets);
    RefreshTreeView();
  }
}

static void HandleCommand_SetDelete(HWND hWnd) {
  if (g_selectedSetIndex != -1) {
    std::wstring msg = Localization::Get(StrId::Msg_ConfirmDeleteSet);
    msg += L"\n\n(" + g_backupSets[g_selectedSetIndex].name + L")";
    if (MessageBoxW(hWnd, msg.c_str(),
                    Localization::Get(StrId::Msg_ConfirmTitle),
                    MB_YESNO | MB_ICONWARNING) == IDYES) {
      g_backupSets.erase(g_backupSets.begin() + g_selectedSetIndex);
      g_selectedSetIndex = -1;
      ConfigManager::Save(g_backupSets);
      RefreshTreeView();
    }
  }
}

static void HandleCommand_UnitDelete(HWND hWnd) {
  if (g_selectedSetIndex != -1 && g_selectedUnitIndex != -1) {
    std::wstring msg = Localization::Get(StrId::Msg_ConfirmDeleteUnit);
    msg += L"\n\n(" +
           g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex].name +
           L")";
    if (MessageBoxW(hWnd, msg.c_str(),
                    Localization::Get(StrId::Msg_ConfirmTitle),
                    MB_YESNO | MB_ICONWARNING) == IDYES) {
      g_backupSets[g_selectedSetIndex].units.erase(
          g_backupSets[g_selectedSetIndex].units.begin() + g_selectedUnitIndex);
      g_selectedUnitIndex = -1;
      ConfigManager::Save(g_backupSets);
      RefreshTreeView();
    }
  }
}

static void HandleCommand_UnitDuplicate() {
  if (g_selectedSetIndex != -1 && g_selectedUnitIndex != -1) {
    BackupUnit copy =
        g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex];
    copy.name += L" - Copy";
    g_backupSets[g_selectedSetIndex].units.push_back(copy);
    ConfigManager::Save(g_backupSets);
    RefreshTreeView();
  }
}

static void HandleCommand_SetAsSource(HWND /*hWnd*/) {
  if (g_selectedSetIndex != -1 && g_selectedUnitIndex != -1) {
    TVITEMW tvi = {0};
    tvi.mask = TVIF_PARAM | TVIF_HANDLE;
    tvi.hItem = TreeView_GetSelection(hSrcTreeView);
    if (tvi.hItem && TreeView_GetItem(hSrcTreeView, &tvi)) {
      std::wstring *p = (std::wstring *)tvi.lParam;
      if (p) {
        g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex].source = *p;
        ConfigManager::Save(g_backupSets);
        RefreshTreeView();
      }
    }
  }
}

static void HandleCommand_SetAsTarget(HWND /*hWnd*/) {
  if (g_selectedSetIndex != -1 && g_selectedUnitIndex != -1) {
    TVITEMW tvi = {0};
    tvi.mask = TVIF_PARAM | TVIF_HANDLE;
    tvi.hItem = TreeView_GetSelection(hDstTreeView);
    if (tvi.hItem && TreeView_GetItem(hDstTreeView, &tvi)) {
      std::wstring *p = (std::wstring *)tvi.lParam;
      if (p) {
        g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex].target = *p;
        ConfigManager::Save(g_backupSets);
        RefreshTreeView();
      }
    }
  }
}

static void HandleCommand_MainModeChange() {
  if (g_selectedSetIndex < 0 || g_selectedUnitIndex < 0)
    return;
  int sel = (int)SendMessageW(hMainModeCombo, CB_GETCURSEL, 0, 0);
  BackupUnit &u = g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex];
  if (sel == 1)
    u.mode = BackupMode::Sync;
  else if (sel == 2)
    u.mode = BackupMode::Verify;
  else
    u.mode = BackupMode::Copy;

  ConfigManager::Save(g_backupSets);
  RefreshTreeView();
}

static void HandleCommand_MainStrategyChange() {
  if (g_selectedSetIndex < 0 || g_selectedUnitIndex < 0)
    return;
  int sel = (int)SendMessageW(hMainStrategyCombo, CB_GETCURSEL, 0, 0);
  BackupUnit &u = g_backupSets[g_selectedSetIndex].units[g_selectedUnitIndex];

  u.parallelMode = (sel == 1);
  u.blockCloneMode = (sel == 2);
  u.shadowCopyMode = (sel == 3);
  u.comparisonMode = (sel == 4);

  ConfigManager::Save(g_backupSets);
  RefreshTreeView();
}

// ============================================================================
// Main Window Procedure
// ============================================================================

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
  switch (message) {
  case WM_CREATE:
    InitializeMainUI(hWnd);
    break;
  case WM_TIMER:
    if (wParam == 2001)
      CheckSchedules();
    break;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDM_HELP_ABOUT:
      MessageBoxW(
          hWnd,
          L"SureBackup v"
          L"1.1.0"
          L"\n\n"
          L"A professional native Win32 backup and synchronization tool.\n"
          L"Designed for high-fidelity data protection with NTFS "
          L"optimization.\n\n"
          L"Features:\n"
          L"- Multithreaded Parallel Engine\n"
          L"- NTFS Alternate Data Streams (ADS) Support\n"
          L"- Directory & File Symbolic Link Preservation\n"
          L"- Bit-Perfect Byte-by-Byte Verification\n"
          L"- Persistent Execution History\n\n"
          L"Developed in collaboration with Antigravity (AI).",
          Localization::Get(StrId::Menu_About), MB_OK | MB_ICONINFORMATION);
      break;
    case IDM_SET_AS_SOURCE:
      HandleCommand_SetAsSource(hWnd);
      break;
    case IDM_SET_AS_TARGET:
      HandleCommand_SetAsTarget(hWnd);
      break;
    case IDM_SET_CREATE:
      OnSetCreate(hWnd);
      break;
    case IDM_SET_UPDATE:
      OnSetUpdate(hWnd);
      break;
    case IDM_SET_DELETE:
      HandleCommand_SetDelete(hWnd);
      break;
    case IDM_SET_DUPLICATE:
      HandleCommand_SetDuplicate();
      break;
    case IDM_UNIT_CREATE:
      OnUnitCreate(hWnd);
      break;
    case IDM_UNIT_UPDATE:
      OnUnitUpdate(hWnd);
      break;
    case IDM_UNIT_DELETE:
      HandleCommand_UnitDelete(hWnd);
      break;
    case IDM_UNIT_DUPLICATE:
      HandleCommand_UnitDuplicate();
      break;
    case IDM_BACKUP_RUN:
      RunBackup(RunMode::Backup);
      break;
    case IDM_BACKUP_PREVIEW:
      RunBackup(RunMode::Preview);
      break;
    case IDM_BACKUP_VERIFY:
      RunBackup(RunMode::Verify);
      break;
    case IDM_BACKUP_STOP:
      HandleCommand_BackupStop();
      break;
    case ID_WORKER_BASE_BTN:
    case ID_WORKER_BASE_BTN + 1:
    case ID_WORKER_BASE_BTN + 2:
    case ID_WORKER_BASE_BTN + 3: {
      int workerIdx = LOWORD(wParam) - ID_WORKER_BASE_BTN;
      HandleCommand_WorkerStop(workerIdx);
    } break;
    case IDM_LOG_MAXIMIZE:
      HandleCommand_LogMaximize(hWnd);
      break;
    case IDM_LOG_HISTORY:
      OnLogHistory(hWnd);
      break;
    case ID_MAIN_MODE_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE)
        HandleCommand_MainModeChange();
      break;
    case ID_MAIN_STRATEGY_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE)
        HandleCommand_MainStrategyChange();
      break;
    case IDM_FILE_EXIT:
      HandleCommand_FileExit(hWnd);
      break;
    }
    break;
  case WM_NOTIFY:
    HandleNotify(hWnd, lParam);
    break;
  case WM_SIZE:
    ResizeMainLayout(hWnd, LOWORD(lParam), HIWORD(lParam));
    break;
  case WM_DESTROY:
    if (g_logger)
      delete g_logger;
    PostQuitMessage(0);
    break;
  case WM_USER + 100:
    EnableWindow(hStopButton, FALSE);
    if (g_logger) {
      if (g_logger->HasErrors()) {
        MessageBoxW(hWnd, Localization::Get(StrId::Msg_ProcessErrors),
                    Localization::Get(StrId::Msg_StatusTitle),
                    MB_OK | MB_ICONWARNING);
      } else {
        MessageBoxW(hWnd, Localization::Get(StrId::Msg_ProcessSuccess),
                    Localization::Get(StrId::Msg_StatusTitle),
                    MB_OK | MB_ICONINFORMATION);
      }
    }
    break;
  default:
    return DefWindowProcW(hWnd, message, wParam, lParam);
  }
  return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
  hInst = hInstance;

  // Check for --embedded flag (host requests hidden window for embedding)
  {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
      for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--embedded") == 0) {
          nCmdShow = SW_HIDE;
          break;
        }
      }
      LocalFree(argv);
    }
  }

  Localization::Init();
  InitCommonControls();
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_WIN95_CLASSES | ICC_PROGRESS_CLASS;
  InitCommonControlsEx(&icex);
  WNDCLASSEXW wc = {sizeof(wc)};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
  wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON),
                                IMAGE_ICON, 16, 16, 0);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = L"SureBackupClass";
  RegisterClassExW(&wc);

  WNDCLASSEXW wcu = {sizeof(wcu)};
  wcu.lpfnWndProc = UnitEditDlgProc;
  wcu.hInstance = hInstance;
  wcu.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcu.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wcu.lpszClassName = L"UnitEditDlgClass";
  RegisterClassExW(&wcu);

  WNDCLASSEXW wcc = {sizeof(wcc)};
  wcc.lpfnWndProc = UnitEditDlgProc; // Same proc for create
  wcc.hInstance = hInstance;
  wcc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wcc.lpszClassName = L"UnitCreateDlgClass";
  RegisterClassExW(&wcc);

  WNDCLASSEXW wcs = {sizeof(wcs)};
  wcs.lpfnWndProc = SetEditDlgProc;
  wcs.hInstance = hInstance;
  wcs.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcs.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wcs.lpszClassName = L"SetEditDlgClass";
  RegisterClassExW(&wcs);

  WNDCLASSEXW wlh = {sizeof(wlh)};
  wlh.lpfnWndProc = LogHistoryDlgProc;
  wlh.hInstance = hInstance;
  wlh.hCursor = LoadCursor(NULL, IDC_ARROW);
  wlh.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wlh.lpszClassName = L"LogHistoryClass";
  RegisterClassExW(&wlh);

  hMainWindow = CreateWindowExW(
      0, L"SureBackupClass", L"SureBackup v1.1.0",
      WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
      CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900, NULL, NULL, hInstance, NULL);
  ShowWindow(hMainWindow, nCmdShow);
  UpdateWindow(hMainWindow);
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}
