#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "FatReader.h"
#include "Localization.h"
#include "MFTReader.h"
#include "exFatReader.h"
#include "resource.h"
#include <atomic>
#include <commctrl.h>
#include <commdlg.h>
#include <fstream>
#include <sstream>

#if 0
#include <iomanip>
#endif
#include <algorithm>
#include <process.h>
#include <regex>
#include <set>
#include <shlobj.h>
#include <string>
#include <vector>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// Globals
HINSTANCE hInstBuffer;
MFTReader mftReader;
FatReader fatReader;
exFatReader exFatReaderObj;
std::vector<FileResult> searchResults;
std::atomic<bool> isSearching(false);
HWND hList = NULL;
HWND hTargetList = NULL;
int currentLang = 0; // 0=English, 1=Japanese
int currentCodePage = CP_OEMCP;
std::vector<std::wstring> searchTargets;
SearchOptions g_options;

// Sorting Globals
int sortColumn = -1;
bool sortAscending = true;

// Subclassing Globals for Startup Fix
WNDPROC oldEditProc = NULL;
WNDPROC oldListProc = NULL;
bool isStartupGuarded = true;

// Guarded Procedures to ignore disruptive messages during startup
LRESULT CALLBACK GuardedEditProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam) {
  if (isStartupGuarded) {
    if (uMsg == WM_KILLFOCUS)
      return 0; // Ignore focus loss during guard
  }
  return CallWindowProc(oldEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK GuardedListProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam) {
  if (isStartupGuarded) {
    // Ignore all mouse messages and focus/capture during guard to prevent
    // "dragging mode"
    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
      return 0;
    if (uMsg == WM_SETFOCUS || uMsg == WM_KILLFOCUS)
      return 0;
    if (uMsg == WM_CAPTURECHANGED || uMsg == WM_CANCELMODE)
      return 0;
    if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP || uMsg == WM_MOUSEMOVE)
      return 0;
  }
  return CallWindowProc(oldListProc, hWnd, uMsg, wParam, lParam);
}

// Prototypes
void SaveConfig(HWND hDlg);
void LoadConfig(HWND hDlg);
void ResizeLayout(HWND hDlg, int cx, int cy);
int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
std::wstring FormatSize(uint64_t size);
INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                               LPARAM lParam);

// Strings removed - using Localization class

std::wstring FormatDate(uint64_t fileTime) {
  FILETIME ft;
  ft.dwLowDateTime = (DWORD)fileTime;
  ft.dwHighDateTime = (DWORD)(fileTime >> 32);

  SYSTEMTIME stUTC, stLocal;
  FileTimeToSystemTime(&ft, &stUTC);
  SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

  wchar_t buf[64];
  swprintf_s(buf, L"%04d/%02d/%02d %02d:%02d:%02d", stLocal.wYear,
             stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute,
             stLocal.wSecond);
  return buf;
}

std::wstring FormatSize(uint64_t size) {
  if (size < 1024)
    return std::to_wstring(size) + L" B";
  if (size < 1024 * 1024)
    return std::to_wstring(size / 1024) + L" KB";
  if (size < 1024 * 1024 * 1024)
    return std::to_wstring(size / (1024 * 1024)) + L" MB";
  return std::to_wstring(size / (1024 * 1024 * 1024)) + L" GB";
}


void AdjustButtonToText(HWND hBtn) {
  if (!hBtn)
    return;
  wchar_t buf[256];
  GetWindowTextW(hBtn, buf, 256);
  HDC hdc = GetDC(hBtn);
  HFONT hFont = (HFONT)SendMessage(hBtn, WM_GETFONT, 0, 0);
  HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

  SIZE sz;
  GetTextExtentPoint32W(hdc, buf, (int)wcslen(buf), &sz);

  SelectObject(hdc, hOldFont);
  ReleaseDC(hBtn, hdc);

  // Padding: 25px (covers button padding or checkbox icon)
  int width = sz.cx + 25;
  if (width < 60)
    width = 60; // Minimum width

  RECT rc;
  GetWindowRect(hBtn, &rc);
  SetWindowPos(hBtn, NULL, 0, 0, width, rc.bottom - rc.top,
               SWP_NOMOVE | SWP_NOZORDER);
}

void UpdateLanguage(HWND hDlg) {
  SetDlgItemTextW(hDlg, IDC_BTN_SEARCH, Localization::GetString(IDS_SEARCH));
  SetDlgItemTextW(hDlg, IDC_STATIC_FILENAME,
                  Localization::GetString(IDS_FILENAME_LABEL));
  SetDlgItemTextW(hDlg, IDC_BTN_SAVE, Localization::GetString(IDS_BTN_SAVE));
  SetDlgItemTextW(hDlg, IDC_STATIC_TARGETS,
                  Localization::GetString(IDS_LBL_TARGETS));
  SetDlgItemTextW(hDlg, IDC_BTN_ADD, Localization::GetString(IDS_BTN_ADD));
  SetDlgItemTextW(hDlg, IDC_BTN_REMOVE,
                  Localization::GetString(IDS_BTN_REMOVE));
  SetDlgItemTextW(hDlg, IDC_CHECK_NOT, Localization::GetString(IDS_CHK_NOT));

  // Update List Columns
  LVCOLUMNW lvc;
  lvc.mask = LVCF_TEXT;
  lvc.pszText = (LPWSTR)Localization::GetString(IDS_COL_NAME);
  SendMessage(hList, LVM_SETCOLUMNW, 0, (LPARAM)&lvc);
  lvc.pszText = (LPWSTR)Localization::GetString(IDS_COL_PATH);
  SendMessage(hList, LVM_SETCOLUMNW, 1, (LPARAM)&lvc);
  lvc.pszText = (LPWSTR)Localization::GetString(IDS_COL_DATE);
  SendMessage(hList, LVM_SETCOLUMNW, 2, (LPARAM)&lvc);
  lvc.pszText = (LPWSTR)Localization::GetString(IDS_COL_SIZE);
  SendMessage(hList, LVM_SETCOLUMNW, 3, (LPARAM)&lvc);

  SetDlgItemTextW(hDlg, IDC_STATIC_CP,
                  Localization::GetString(IDS_LBL_CODEPAGE));

  // Update Menu
  HMENU hMenu = GetMenu(hDlg);
  if (hMenu) {
    // Top Level
    ModifyMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING | MF_POPUP,
                (UINT_PTR)GetSubMenu(hMenu, 0),
                Localization::GetString(IDS_MENU_CONFIG));
    ModifyMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING | MF_POPUP,
                (UINT_PTR)GetSubMenu(hMenu, 1),
                Localization::GetString(IDS_MENU_LANGUAGE));
    ModifyMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING | MF_POPUP,
                (UINT_PTR)GetSubMenu(hMenu, 2),
                Localization::GetString(IDS_MENU_HELP));

    // Items
    ModifyMenuW(hMenu, ID_CONFIG_OPTIONS, MF_BYCOMMAND | MF_STRING,
                ID_CONFIG_OPTIONS,
                Localization::GetString(IDS_MENU_SEARCH_OPTIONS));
    ModifyMenuW(hMenu, ID_HELP_ABOUT, MF_BYCOMMAND | MF_STRING, ID_HELP_ABOUT,
                Localization::GetString(IDS_MENU_ABOUT));

    DrawMenuBar(hDlg);
  }

  // Adjust sizes based on new text
  AdjustButtonToText(GetDlgItem(hDlg, IDC_BTN_SEARCH));
  AdjustButtonToText(GetDlgItem(hDlg, IDC_BTN_SAVE));
  AdjustButtonToText(GetDlgItem(hDlg, IDC_BTN_ADD));
  AdjustButtonToText(GetDlgItem(hDlg, IDC_BTN_REMOVE));
  AdjustButtonToText(GetDlgItem(hDlg, IDC_CHECK_NOT));

  // Determine Min Width
  int m = 10;
  
  // 1. Top Row: Label + Edit(150) + Not + Search
  RECT rcS, rcNot, rcLbl;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_SEARCH), &rcS);
  GetWindowRect(GetDlgItem(hDlg, IDC_CHECK_NOT), &rcNot);
  
  // Measure Label explicitly (as AdjustButtonToText doesn't run on Label)
  {
      wchar_t buf[256];
      GetDlgItemTextW(hDlg, IDC_STATIC_FILENAME, buf, 256);
      HDC hdc = GetDC(GetDlgItem(hDlg, IDC_STATIC_FILENAME));
      HFONT hFont = (HFONT)SendDlgItemMessage(hDlg, IDC_STATIC_FILENAME, WM_GETFONT, 0, 0);
      HFONT hOld = (HFONT)SelectObject(hdc, hFont);
      SIZE sz;
      GetTextExtentPoint32W(hdc, buf, (int)wcslen(buf), &sz);
      SelectObject(hdc, hOld);
      ReleaseDC(GetDlgItem(hDlg, IDC_STATIC_FILENAME), hdc);
      // Resize Label Control
      SetWindowPos(GetDlgItem(hDlg, IDC_STATIC_FILENAME), NULL, 0, 0, sz.cx + 5, sz.cy, SWP_NOMOVE|SWP_NOZORDER);
      GetWindowRect(GetDlgItem(hDlg, IDC_STATIC_FILENAME), &rcLbl);
  }

  int lblW = rcLbl.right - rcLbl.left;
  int notW = rcNot.right - rcNot.left;
  int searchW = rcS.right - rcS.left;
  
  int minW_Top = m + lblW + m + 150 + m + notW + m + searchW + m;

  // 2. Middle: List(100) + Btns (Add/Rem)
  RECT rcAdd, rcRem, rcTLabel;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_ADD), &rcAdd);
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_REMOVE), &rcRem);
  
  // Measure "Search Candidates" Label
  {
      wchar_t buf[256];
      GetDlgItemTextW(hDlg, IDC_STATIC_TARGETS, buf, 256);
      HDC hdc = GetDC(GetDlgItem(hDlg, IDC_STATIC_TARGETS));
      HFONT hFont = (HFONT)SendDlgItemMessage(hDlg, IDC_STATIC_TARGETS, WM_GETFONT, 0, 0);
      HFONT hOld = (HFONT)SelectObject(hdc, hFont);
      SIZE sz;
      GetTextExtentPoint32W(hdc, buf, (int)wcslen(buf), &sz);
      SelectObject(hdc, hOld);
      ReleaseDC(GetDlgItem(hDlg, IDC_STATIC_TARGETS), hdc);
      SetWindowPos(GetDlgItem(hDlg, IDC_STATIC_TARGETS), NULL, 0, 0, sz.cx + 5, sz.cy, SWP_NOMOVE|SWP_NOZORDER);
      GetWindowRect(GetDlgItem(hDlg, IDC_STATIC_TARGETS), &rcTLabel);
  }

  int addW = rcAdd.right - rcAdd.left;
  int remW = rcRem.right - rcRem.left;
  int tLblW = rcTLabel.right - rcTLabel.left;

  int maxBtnW = (addW > remW) ? addW : remW;
  int minW_Mid = m + 100 + m + maxBtnW + m;
  int minW_Lbl = m + tLblW + m; 

  int reqWidth = minW_Top;
  if (minW_Mid > reqWidth) reqWidth = minW_Mid;
  if (minW_Lbl > reqWidth) reqWidth = minW_Lbl;

  // Current Client Width
  RECT rcClient;
  GetClientRect(hDlg, &rcClient);
  
  if (rcClient.right < reqWidth) {
      // Resize Window
      RECT rcWin;
      GetWindowRect(hDlg, &rcWin);
      int diff = reqWidth - rcClient.right;
      SetWindowPos(hDlg, NULL, 0, 0, (rcWin.right - rcWin.left) + diff, rcWin.bottom - rcWin.top, SWP_NOMOVE | SWP_NOZORDER);
      GetClientRect(hDlg, &rcClient); // Update
  }

  // Refresh Layout
  ResizeLayout(hDlg, rcClient.right, rcClient.bottom);
}

void ScanThread(void *param) {
  HWND hDlg = (HWND)param;

  wchar_t queryBuf[256];
  GetDlgItemTextW(hDlg, IDC_EDIT_QUERY, queryBuf, 256);
  std::wstring query = queryBuf;

  searchResults.clear();

  std::set<wchar_t> drivesToScan;
  for (const auto &target : searchTargets) {
    if (target.length() >= 3 && target[1] == L':') {
      drivesToScan.insert(towupper(target[0]));
    }
  }

  auto callback = [](int percent, int max, void *userData) {
    // Post progress message (WM_USER + 3)
    // wParam = percent
    HWND hDlg = (HWND)userData;
    PostMessage(hDlg, WM_USER + 3, (WPARAM)percent, 0);
  };

  bool anySuccess = false;

  for (wchar_t drive : drivesToScan) {
    wchar_t driveRoot[] = {drive, L':', L'\\', L'\0'};
    wchar_t fsName[MAX_PATH];
    bool fsOk = GetVolumeInformationW(driveRoot, NULL, 0, NULL, NULL, NULL,
                                      fsName, MAX_PATH);

    if (fsOk) {
      bool scanSuccess = false;
      bool isNtfs = (wcscmp(fsName, L"NTFS") == 0);
      bool isFat =
          (wcscmp(fsName, L"FAT") == 0 || wcscmp(fsName, L"FAT32") == 0);
      bool isExFat = (wcscmp(fsName, L"exFAT") == 0);

      // Reset Progress for next drive
      callback(0, 100, hDlg);

      if (isNtfs) {
        if (mftReader.Initialize(drive) && mftReader.Scan(callback, hDlg))
          scanSuccess = true;
      } else if (isFat) {
        if (fatReader.Initialize(drive) &&
            fatReader.Scan(currentCodePage, callback, hDlg))
          scanSuccess = true;
      } else if (isExFat) {
        if (exFatReaderObj.Initialize(drive) &&
            exFatReaderObj.Scan(callback, hDlg))
          scanSuccess = true;
      }

      callback(100, 100, hDlg); // Ensure 100% at end

      if (scanSuccess) {
        std::vector<std::wstring> driveTargets;
        for (const auto &t : searchTargets) {
          if (t.length() >= 3 && towupper(t[0]) == drive) {
            driveTargets.push_back(t);
          }
        }

        for (const auto &targetFolder : driveTargets) {
          std::vector<FileResult> results;
          if (isNtfs)
            results = mftReader.Search(query, targetFolder, g_options, 50000);
          else if (isFat)
            results = fatReader.Search(query, targetFolder, currentCodePage,
                                       g_options, 50000);
          else if (isExFat)
            results =
                exFatReaderObj.Search(query, targetFolder, g_options, 50000);

          searchResults.insert(searchResults.end(), results.begin(),
                               results.end());
          if (searchResults.size() >= 50000)
            break;
        }
        anySuccess = true;
      }
    }
  }

  if (anySuccess) {
    PostMessage(hDlg, WM_USER + 1, 0, 0);
  } else {
    // If we scanned nothing or failed all
    PostMessage(hDlg, WM_USER + 2, 0, 0);
  }

  isSearching = false;
}

void PopulateList() {
  ListView_DeleteAllItems(hList);
  SendMessage(hList, WM_SETREDRAW, FALSE, 0);

  LVITEM lvI;
  lvI.mask = LVIF_TEXT | LVIF_STATE;
  lvI.stateMask = 0;
  lvI.state = 0;

  for (size_t i = 0; i < searchResults.size(); ++i) {
    memset(&lvI, 0, sizeof(lvI));
    lvI.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
    lvI.state = 0;
    lvI.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    lvI.iItem = (int)i;
    lvI.iSubItem = 0;
    lvI.lParam = (LPARAM)i;
    lvI.pszText = (LPWSTR)searchResults[i].Name.c_str();
    int idx = ListView_InsertItem(hList, &lvI);

    ListView_SetItemText(hList, idx, 1,
                         (LPWSTR)searchResults[i].FullPath.c_str());

    std::wstring dateStr = FormatDate(searchResults[i].LastWriteTime);
    ListView_SetItemText(hList, idx, 2, (LPWSTR)dateStr.c_str());

    std::wstring sizeStr = FormatSize(searchResults[i].Size);
    ListView_SetItemText(hList, idx, 3, (LPWSTR)sizeStr.c_str());
  }

  SendMessage(hList, WM_SETREDRAW, TRUE, 0);

  // 1. Force focus away immediately
  SetFocus(GetDlgItem(GetParent(hList), IDC_EDIT_QUERY));

  // 2. Clear state strictly
  ListView_SetItemState(hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
  ListView_SetSelectionMark(hList, -1);

  // 3. Force Repaint
  InvalidateRect(hList, NULL, TRUE);
  UpdateWindow(hList);
}

void SaveResults(HWND hDlg) {
  OPENFILENAMEW ofn;
  wchar_t fileName[MAX_PATH] = L"scan_result.txt";

  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hDlg;
  ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
  ofn.lpstrFile = fileName;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags =
      OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
  ofn.lpstrDefExt = L"txt";

  if (GetSaveFileNameW(&ofn)) {
    std::wofstream outfile(fileName);
    outfile << L"Name\tPath\tDate\tSize\n"; // Header
    for (const auto &res : searchResults) {
      outfile << res.Name << L"\t" << res.FullPath << L"\t"
              << FormatDate(res.LastWriteTime) << L"\t" << res.Size << L"\n";
    }
  }
}

void AddFolder(HWND hDlg) {
  BROWSEINFOW bi = {0};
  bi.hwndOwner = hDlg;
  bi.lpszTitle = L"Select Folder to Search";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

  LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
  if (pidl != 0) {
    wchar_t path[MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) {
      // Check duplicates
      bool found = false;
      for (const auto &t : searchTargets) {
        if (t == path)
          found = true;
      }
      if (!found) {
        searchTargets.push_back(path);
        SendMessage(hTargetList, LB_ADDSTRING, 0, (LPARAM)path);
      }
    }
    CoTaskMemFree(pidl);
  }
}

void RemoveFolder(HWND hDlg) {
  int idx = (int)SendMessage(hTargetList, LB_GETCURSEL, 0, 0);
  if (idx != LB_ERR) {
    wchar_t buf[MAX_PATH];
    SendMessage(hTargetList, LB_GETTEXT, idx, (LPARAM)buf);
    SendMessage(hTargetList, LB_DELETESTRING, idx, 0);

    // Remove from vector
    for (auto it = searchTargets.begin(); it != searchTargets.end(); ++it) {
      if (*it == buf) {
        searchTargets.erase(it);
        break;
      }
    }
  }
}

std::wstring GetConfigPath(bool createDir) {
  wchar_t path[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                                 path))) {
    std::wstring dir = std::wstring(path) + L"\\FastFileSearch";
    if (createDir) {
      CreateDirectoryW(dir.c_str(), NULL);
    }
    return dir + L"\\FastFileSearch.ini";
  }
  // Fallback to local
  return L"FastFileSearch.ini";
}

void SaveConfig(HWND hDlg) {
  std::wstring iniPath = GetConfigPath(true);

  // Save Window Size
  RECT rc;
  if (GetWindowRect(hDlg, &rc)) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    WritePrivateProfileStringW(L"Settings", L"Width",
                               std::to_wstring(w).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"Height",
                               std::to_wstring(h).c_str(), iniPath.c_str());
  }

  // Save Language
  WritePrivateProfileStringW(L"Settings", L"Language",
                             std::to_wstring(currentLang).c_str(),
                             iniPath.c_str());

  // Save Code Page
  WritePrivateProfileStringW(L"Settings", L"CodePage",
                             std::to_wstring(currentCodePage).c_str(),
                             iniPath.c_str());

  // Save Search Options
  WritePrivateProfileStringW(L"SearchOptions", L"Mode",
                             std::to_wstring((int)g_options.mode).c_str(),
                             iniPath.c_str());
  WritePrivateProfileStringW(
      L"SearchOptions", L"IgnoreCase",
      std::to_wstring(g_options.ignoreCase ? 1 : 0).c_str(), iniPath.c_str());
  WritePrivateProfileStringW(L"SearchOptions", L"MinSize",
                             std::to_wstring(g_options.minSize).c_str(),
                             iniPath.c_str());
  WritePrivateProfileStringW(L"SearchOptions", L"MaxSize",
                             std::to_wstring(g_options.maxSize).c_str(),
                             iniPath.c_str());
  WritePrivateProfileStringW(L"SearchOptions", L"MinDate",
                             std::to_wstring(g_options.minDate).c_str(),
                             iniPath.c_str());
  WritePrivateProfileStringW(L"SearchOptions", L"MaxDate",
                             std::to_wstring(g_options.maxDate).c_str(),
                             iniPath.c_str());
  WritePrivateProfileStringW(
      L"SearchOptions", L"IncludeFiles",
      std::to_wstring(g_options.includeFiles ? 1 : 0).c_str(), iniPath.c_str());
  WritePrivateProfileStringW(
      L"SearchOptions", L"IncludeFolders",
      std::to_wstring(g_options.includeFolders ? 1 : 0).c_str(),
      iniPath.c_str());
  WritePrivateProfileStringW(L"SearchOptions", L"ExtensionFilter",
                             g_options.extensionFilter.c_str(),
                             iniPath.c_str());
  WritePrivateProfileStringW(
      L"SearchOptions", L"MatchFullPath",
      std::to_wstring(g_options.matchFullPath ? 1 : 0).c_str(),
      iniPath.c_str());
  WritePrivateProfileStringW(L"SearchOptions", L"ExcludePattern",
                             g_options.excludePattern.c_str(), iniPath.c_str());
  WritePrivateProfileStringW(
      L"SearchOptions", L"InvertMatch",
      std::to_wstring(g_options.invertMatch ? 1 : 0).c_str(), iniPath.c_str());

  // Clear old Targets
  WritePrivateProfileStringW(L"Targets", NULL, NULL, iniPath.c_str());

  // Save Count
  WritePrivateProfileStringW(L"Targets", L"Count",
                             std::to_wstring(searchTargets.size()).c_str(),
                             iniPath.c_str());

  // Save Items
  for (size_t i = 0; i < searchTargets.size(); ++i) {
    std::wstring key = L"Target" + std::to_wstring(i);
    WritePrivateProfileStringW(L"Targets", key.c_str(),
                               searchTargets[i].c_str(), iniPath.c_str());
  }
}

void LoadConfig(HWND hDlg) {
  std::wstring iniPath = GetConfigPath(false);

  // Load Window Size
  int w = GetPrivateProfileIntW(L"Settings", L"Width", 0, iniPath.c_str());
  int h = GetPrivateProfileIntW(L"Settings", L"Height", 0, iniPath.c_str());

  int screenW = GetSystemMetrics(SM_CXSCREEN);
  int screenH = GetSystemMetrics(SM_CYSCREEN);

  if (w > 300 && w <= screenW && h > 200 && h <= screenH) {
    SetWindowPos(hDlg, NULL, 0, 0, w, h,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  // Load Language
  currentLang =
      GetPrivateProfileIntW(L"Settings", L"Language", 0, iniPath.c_str());
  if (currentLang < 0 || currentLang >= APP_LANG_COUNT)
    currentLang = 0;
  Localization::SetLanguage((LanguageID)currentLang);

  // Load Code Page
  currentCodePage = GetPrivateProfileIntW(L"Settings", L"CodePage", CP_OEMCP,
                                          iniPath.c_str());

  // Load Search Options
  g_options.mode = (MatchMode)GetPrivateProfileIntW(L"SearchOptions", L"Mode",
                                                    0, iniPath.c_str());
  g_options.ignoreCase = GetPrivateProfileIntW(L"SearchOptions", L"IgnoreCase",
                                               1, iniPath.c_str()) != 0;

  wchar_t valBuf[64];
  GetPrivateProfileStringW(L"SearchOptions", L"MinSize", L"0", valBuf, 64,
                           iniPath.c_str());
  g_options.minSize = _wtoi64(valBuf);
  GetPrivateProfileStringW(L"SearchOptions", L"MaxSize", L"0", valBuf, 64,
                           iniPath.c_str());
  g_options.maxSize = _wtoi64(valBuf);
  GetPrivateProfileStringW(L"SearchOptions", L"MinDate", L"0", valBuf, 64,
                           iniPath.c_str());
  g_options.minDate = _wtoi64(valBuf);
  GetPrivateProfileStringW(L"SearchOptions", L"MaxDate", L"0", valBuf, 64,
                           iniPath.c_str());
  g_options.maxDate = _wtoi64(valBuf);

  g_options.includeFiles =
      GetPrivateProfileIntW(L"SearchOptions", L"IncludeFiles", 1,
                            iniPath.c_str()) != 0;
  g_options.includeFolders =
      GetPrivateProfileIntW(L"SearchOptions", L"IncludeFolders", 1,
                            iniPath.c_str()) != 0;

  wchar_t extBuf[256];
  GetPrivateProfileStringW(L"SearchOptions", L"ExtensionFilter", L"", extBuf,
                           256, iniPath.c_str());
  g_options.extensionFilter = extBuf;

  g_options.matchFullPath =
      GetPrivateProfileIntW(L"SearchOptions", L"MatchFullPath", 0,
                            iniPath.c_str()) != 0;

  wchar_t exBuf[256];
  GetPrivateProfileStringW(L"SearchOptions", L"ExcludePattern", L"", exBuf, 256,
                           iniPath.c_str());
  g_options.excludePattern = exBuf;

  g_options.invertMatch =
      GetPrivateProfileIntW(L"SearchOptions", L"InvertMatch", 0,
                            iniPath.c_str()) != 0;
  CheckDlgButton(hDlg, IDC_CHECK_NOT,
                 g_options.invertMatch ? BST_CHECKED : BST_UNCHECKED);

  // Load Targets
  searchTargets.clear();
  SendMessage(hTargetList, LB_RESETCONTENT, 0, 0);

  int count = GetPrivateProfileIntW(L"Targets", L"Count", 0, iniPath.c_str());
  if (count > 0) {
    wchar_t buf[MAX_PATH];
    for (int i = 0; i < count; ++i) {
      std::wstring key = L"Target" + std::to_wstring(i);
      GetPrivateProfileStringW(L"Targets", key.c_str(), L"", buf, MAX_PATH,
                               iniPath.c_str());
      if (wcslen(buf) > 0) {
        searchTargets.push_back(buf);
        SendMessage(hTargetList, LB_ADDSTRING, 0, (LPARAM)buf);
      }
    }
  }

  SendDlgItemMessage(hDlg, IDC_COMBO_LANG, CB_SETCURSEL, currentLang, 0);
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
  if (lParam1 >= (LPARAM)searchResults.size() ||
      lParam2 >= (LPARAM)searchResults.size())
    return 0;

  const FileResult &a = searchResults[lParam1];
  const FileResult &b = searchResults[lParam2];

  int cmp = 0;
  switch (sortColumn) {
  case 0: // Name
    cmp = lstrcmpiW(a.Name.c_str(), b.Name.c_str());
    break;
  case 1: // Path
    cmp = lstrcmpiW(a.FullPath.c_str(), b.FullPath.c_str());
    break;
  case 2: // Date
    if (a.LastWriteTime < b.LastWriteTime)
      cmp = -1;
    else if (a.LastWriteTime > b.LastWriteTime)
      cmp = 1;
    break;
  case 3: // Size
    if (a.Size < b.Size)
      cmp = -1;
    else if (a.Size > b.Size)
      cmp = 1;
    break;
  }

  return sortAscending ? cmp : -cmp;
}

void ResizeLayout(HWND hDlg, int cx, int cy) {
  if (cx == 0 || cy == 0)
    return;

  // Quick helper to move
  auto Move = [&](int id, int x, int y, int w, int h, bool size) {
    SetWindowPos(GetDlgItem(hDlg, id), NULL, x, y, w, h,
                 SWP_NOZORDER | (size ? 0 : SWP_NOSIZE));
  };

  // Margins (in pixels approx)
  int m = 10;

  // Get Button Sizes
  RECT rcBtnS, rcBtnAdd, rcBtnRem;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_SEARCH), &rcBtnS);
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_ADD), &rcBtnAdd);
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_REMOVE), &rcBtnRem);

  int btnW = rcBtnS.right - rcBtnS.left;
  int addW = rcBtnAdd.right - rcBtnAdd.left;
  int remW = rcBtnRem.right - rcBtnRem.left;

  int maxRightColW = btnW;
  if(addW > maxRightColW) maxRightColW = addW;
  if(remW > maxRightColW) maxRightColW = remW;

  // Right Edge for buttons
  int rightEdge = cx - m;

  // Top Row
  // Buttons align to Right
  int btnX = rightEdge - btnW;

  // Search Button
  RECT rcS;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_SEARCH), &rcS);
  POINT ptS = {rcS.left, rcS.top};
  ScreenToClient(hDlg, &ptS);
  Move(IDC_BTN_SEARCH, btnX, ptS.y, 0, 0, false);

  // Not Checkbox
  RECT rcNot;
  GetWindowRect(GetDlgItem(hDlg, IDC_CHECK_NOT), &rcNot);
  int notW = rcNot.right - rcNot.left;
  int notX = btnX - m - notW;
  Move(IDC_CHECK_NOT, notX, ptS.y + 2, 0, 0, false);

  // Filename Label
  RECT rcLbl;
  GetWindowRect(GetDlgItem(hDlg, IDC_STATIC_FILENAME), &rcLbl);
  int lblW = rcLbl.right - rcLbl.left;
  Move(IDC_STATIC_FILENAME, m, ptS.y + 2, lblW, rcLbl.bottom - rcLbl.top, true);

  // Edit Query
  // Starts after Label + m, ends before Not - m
  int editX = m + lblW + m;
  int editW = notX - m - editX;
  if (editW < 50) editW = 50; // Safety
  
  RECT rcE;
  GetWindowRect(GetDlgItem(hDlg, IDC_EDIT_QUERY), &rcE);
  Move(IDC_EDIT_QUERY, editX, ptS.y, editW, rcE.bottom - rcE.top, true);

  // Target List Buttons
  // Align right edge to rightEdge
  RECT rcAddT;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_ADD), &rcAddT);
  POINT ptAdd = {rcAddT.left, rcAddT.top};
  ScreenToClient(hDlg, &ptAdd);
  // x = rightEdge - addW
  Move(IDC_BTN_ADD, rightEdge - addW, ptAdd.y, 0, 0, false);

  RECT rcRemT;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_REMOVE), &rcRemT);
  POINT ptRem = {rcRemT.left, rcRemT.top};
  ScreenToClient(hDlg, &ptRem);
  // x = rightEdge - remW
  Move(IDC_BTN_REMOVE, rightEdge - remW, ptRem.y, 0, 0, false);

  // Target List
  RECT rcT;
  GetWindowRect(GetDlgItem(hDlg, IDC_LIST_TARGETS), &rcT);
  POINT ptT = {rcT.left, rcT.top};
  ScreenToClient(hDlg, &ptT);
  // Width = rightEdge - maxRightColW - m - m
  Move(IDC_LIST_TARGETS, m, ptT.y, cx - m - maxRightColW - m - m, rcT.bottom - rcT.top, true);

  // Result List
  RECT rcL;
  GetWindowRect(GetDlgItem(hDlg, IDC_LIST_RESULTS), &rcL);
  POINT ptL = {rcL.left, rcL.top};
  ScreenToClient(hDlg, &ptL);
  // Height: cy - m - BottomRow - ptL.y
  int bottomRowH = 40;
  Move(IDC_LIST_RESULTS, m, ptL.y, cx - m - m, cy - bottomRowH - ptL.y, true);

  // Bottom Row
  int bottomY = cy - 35;
  Move(IDC_BTN_SAVE, m, bottomY, 0, 0, false);

  // Status and Progress
  RECT rcSave;
  GetWindowRect(GetDlgItem(hDlg, IDC_BTN_SAVE), &rcSave);
  int statusX = m + (rcSave.right - rcSave.left) + m;
  int availableWidth = cx - statusX - m;

  // Split width: Status gets 60%, Progress gets 40% (approx)
  int progressW = 150;
  if (availableWidth > 300)
    progressW = 200;

  int statusW = availableWidth - progressW - m;
  if (statusW < 50)
    statusW = 50; // Min width

  Move(IDC_STATUS, statusX, bottomY + 5, statusW, 20, true);
  Move(IDC_PROGRESS, statusX + statusW + m, bottomY + 2, progressW, 20, true);
}

LRESULT DrawItemWithHighlight(LPNMLVCUSTOMDRAW lplvcd, HWND hDlg) {
  int iItem = (int)lplvcd->nmcd.dwItemSpec;
  int iSubItem = lplvcd->iSubItem;
  HDC hdc = lplvcd->nmcd.hdc;

  // Get Text
  wchar_t textBuf[1024];
  ListView_GetItemText(lplvcd->nmcd.hdr.hwndFrom, iItem, iSubItem, textBuf,
                       1024);
  std::wstring text = textBuf;

  // Get Query
  wchar_t queryBuf[256];
  GetDlgItemTextW(hDlg, IDC_EDIT_QUERY, queryBuf, 256);
  std::wstring query = queryBuf;

  if (query.empty() || text.empty() || iSubItem != 0) {
    return CDRF_DODEFAULT;
  }

  struct Range {
    size_t start, len;
  };
  std::vector<Range> ranges;

  std::wstring textLower = text;
  std::wstring queryLower = query;
  if (g_options.ignoreCase) {
    for (auto &c : textLower)
      c = towlower(c);
    for (auto &c : queryLower)
      c = towlower(c);
  }

  // Collect Ranges
  if (g_options.mode == MatchMode_Exact) {
    bool match = g_options.ignoreCase
                     ? (lstrcmpiW(text.c_str(), query.c_str()) == 0)
                     : (text == query);
    if (match)
      ranges.push_back({0, text.length()});
  } else if (g_options.mode == MatchMode_SpaceDivided) {
    std::wstringstream ss(queryLower);
    std::wstring token;
    while (ss >> token) {
      size_t pos = textLower.find(token);
      while (pos != std::wstring::npos) {
        ranges.push_back({pos, token.length()});
        pos = textLower.find(token, pos + 1);
      }
    }
  } else if (g_options.mode == MatchMode_RegEx) {
    try {
      std::wregex re(query,
                     (g_options.ignoreCase
                          ? std::regex_constants::icase
                          : (std::regex_constants::syntax_option_type)0) |
                         std::regex_constants::optimize);
      auto it = std::wsregex_iterator(text.begin(), text.end(), re);
      auto end = std::wsregex_iterator();
      for (; it != end; ++it) {
        ranges.push_back({(size_t)it->position(), (size_t)it->length()});
      }
    } catch (...) {
    }
  } else { // Substring
    size_t pos = textLower.find(queryLower);
    while (pos != std::wstring::npos) {
      ranges.push_back({pos, query.length()});
      pos = textLower.find(queryLower, pos + 1);
    }
  }

  if (ranges.empty())
    return CDRF_DODEFAULT;

  // Merge Ranges
  std::sort(ranges.begin(), ranges.end(),
            [](const Range &a, const Range &b) { return a.start < b.start; });
  std::vector<Range> merged;
  for (auto &r : ranges) {
    if (merged.empty() || r.start > merged.back().start + merged.back().len) {
      merged.push_back(r);
    } else {
      size_t end =
          std::max(merged.back().start + merged.back().len, r.start + r.len);
      merged.back().len = end - merged.back().start;
    }
  }

  // Draw
  RECT rc;
  ListView_GetSubItemRect(lplvcd->nmcd.hdr.hwndFrom, iItem, iSubItem,
                          LVIR_LABEL, &rc);

  bool isSelected =
      (ListView_GetItemState(lplvcd->nmcd.hdr.hwndFrom, iItem, LVIS_SELECTED) &
       LVIS_SELECTED) != 0;
  COLORREF clrText = isSelected ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                                : GetSysColor(COLOR_WINDOWTEXT);
  COLORREF clrMatchBk = RGB(0, 255, 255); // Cyan
  COLORREF clrMatchText = RGB(0, 0, 0);   // Black

  HBRUSH hFillBr = isSelected ? GetSysColorBrush(COLOR_HIGHLIGHT)
                              : GetSysColorBrush(COLOR_WINDOW);
  FillRect(hdc, &rc, hFillBr);

  int oldBkMode = SetBkMode(hdc, TRANSPARENT);
  HFONT hFont = (HFONT)SendMessage(lplvcd->nmcd.hdr.hwndFrom, WM_GETFONT, 0, 0);
  HGDIOBJ oldFont = SelectObject(hdc, hFont);

  int currentX = rc.left + 6;
  auto DrawSegment = [&](const std::wstring &seg, bool highlight) {
    if (seg.empty())
      return;
    SIZE sz;
    GetTextExtentPoint32W(hdc, seg.c_str(), (int)seg.length(), &sz);
    RECT segRect = {currentX, rc.top, currentX + sz.cx, rc.bottom};
    if (segRect.left < rc.right) {
      if (segRect.right > rc.right)
        segRect.right = rc.right;
      if (highlight) {
        HBRUSH hBr = CreateSolidBrush(clrMatchBk);
        FillRect(hdc, &segRect, hBr);
        DeleteObject(hBr);
        SetTextColor(hdc, clrMatchText);
      } else {
        SetTextColor(hdc, clrText);
      }
      DrawTextW(hdc, seg.c_str(), (int)seg.length(), &segRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    currentX += sz.cx;
  };

  size_t lastPos = 0;
  for (auto &r : merged) {
    if (r.start > lastPos)
      DrawSegment(text.substr(lastPos, r.start - lastPos), false);
    DrawSegment(text.substr(r.start, r.len), true);
    lastPos = r.start + r.len;
  }
  if (lastPos < text.length())
    DrawSegment(text.substr(lastPos), false);

  SelectObject(hdc, oldFont);
  SetBkMode(hdc, oldBkMode);
  return CDRF_SKIPDEFAULT;
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_SIZE:
    ResizeLayout(hDlg, LOWORD(lParam), HIWORD(lParam));
    break;
  case WM_INITDIALOG: {
    // Stage 1 Fix: Ensure no capture immediately and LOCKDOWN the list view
    ReleaseCapture();

    // Set Icon
    HICON hIconBig =
        (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON),
                         IMAGE_ICON, 32, 32, 0);
    HICON hIconSmall =
        (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON),
                         IMAGE_ICON, 16, 16, 0);
    SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    isStartupGuarded = true; // Start guarded

    hInstBuffer = GetModuleHandle(NULL);
    hList = GetDlgItem(hDlg, IDC_LIST_RESULTS);
    HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_QUERY);

    // Subclass controls to ignore disruptive messages during startup
    oldEditProc = (WNDPROC)SetWindowLongPtr(hEdit, GWLP_WNDPROC,
                                            (LONG_PTR)GuardedEditProc);
    oldListProc = (WNDPROC)SetWindowLongPtr(hList, GWLP_WNDPROC,
                                            (LONG_PTR)GuardedListProc);

    // Set a 1000ms timer to settle and enforce focus
    SetTimer(hDlg, 1, 1000, NULL);

    // Temporarily disable the list to ignore double-click artifacts from launch
    EnableWindow(hList, FALSE);

    ListView_SetExtendedListViewStyle(hList,
                                      LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    hTargetList = GetDlgItem(hDlg, IDC_LIST_TARGETS);

    // Setup Columns
    LVCOLUMN lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    lvc.iSubItem = 0;
    lvc.pszText = (LPWSTR)L"Name";
    lvc.cx = 200;
    lvc.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(hList, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = (LPWSTR)L"Path";
    lvc.cx = 300;
    ListView_InsertColumn(hList, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = (LPWSTR)L"Modified Date";
    lvc.cx = 140;
    ListView_InsertColumn(hList, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.pszText = (LPWSTR)L"Size";
    lvc.cx = 100;
    ListView_InsertColumn(hList, 3, &lvc);

    // Setup Languages removed - now in Menu

    // Check Current Language Menu Item
    HMENU hMenu = GetMenu(hDlg);
    CheckMenuItem(hMenu, ID_LANG_ENGLISH + currentLang,
                  MF_BYCOMMAND | MF_CHECKED);

    // Setup Code Pages
    HWND hCP = GetDlgItem(hDlg, IDC_COMBO_CP);
    SendMessage(hCP, CB_ADDSTRING, 0, (LPARAM)L"Auto (OEM)");
    SendMessage(hCP, CB_SETITEMDATA, 0, (LPARAM)CP_OEMCP);
    SendMessage(hCP, CB_ADDSTRING, 0, (LPARAM)L"Japanese (932)");
    SendMessage(hCP, CB_SETITEMDATA, 1, (LPARAM)932);
    SendMessage(hCP, CB_ADDSTRING, 0, (LPARAM)L"Western (1252)");
    SendMessage(hCP, CB_SETITEMDATA, 2, (LPARAM)1252);
    SendMessage(hCP, CB_ADDSTRING, 0, (LPARAM)L"Simplified Chinese (936)");
    SendMessage(hCP, CB_SETITEMDATA, 3, (LPARAM)936);
    SendMessage(hCP, CB_ADDSTRING, 0, (LPARAM)L"Traditional Chinese (950)");
    SendMessage(hCP, CB_SETITEMDATA, 4, (LPARAM)950);
    SendMessage(hCP, CB_ADDSTRING, 0, (LPARAM)L"UTF-8 (65001)");
    SendMessage(hCP, CB_SETITEMDATA, 5, (LPARAM)CP_UTF8);

    // Select current CP
    SendMessage(hCP, CB_SETCURSEL, 0, 0); // Default to Auto
    for (int i = 0; i < (int)SendMessage(hCP, CB_GETCOUNT, 0, 0); ++i) {
      if ((int)SendMessage(hCP, CB_GETITEMDATA, i, 0) == currentCodePage) {
        SendMessage(hCP, CB_SETCURSEL, i, 0);
        break;
      }
    }

    // Initial Default: Add C:\ drive?
    // Load Config
    LoadConfig(hDlg);

    // Apply Language (including Menu)
    UpdateLanguage(hDlg);

    // Check Configured Language Menu Item
    hMenu = GetMenu(hDlg);
    for (int i = 0; i < APP_LANG_COUNT; i++) {
      CheckMenuItem(hMenu, ID_LANG_ENGLISH + i,
                    MF_BYCOMMAND |
                        (i == currentLang ? MF_CHECKED : MF_UNCHECKED));
    }

    if (searchTargets.empty()) {
      wchar_t defaultPath[] = L"C:\\";
      searchTargets.push_back(defaultPath);
      SendMessage(hTargetList, LB_ADDSTRING, 0, (LPARAM)defaultPath);
    }

    ListView_SetExtendedListViewStyle(hList,
                                      LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    UpdateLanguage(hDlg);

    // FIX: Set focus to the search field and return FALSE as requested.
    // This is the definitive way to set focus in a dialog during
    // initialization.
    SetFocus(GetDlgItem(hDlg, IDC_EDIT_QUERY));
    return FALSE;
  }
  case WM_USER + 3: {
    // Progress Update
    // wParam = percent
    SendDlgItemMessage(hDlg, IDC_PROGRESS, PBM_SETPOS, (WPARAM)wParam, 0);
    return TRUE;
  }
  case WM_TIMER: {
    if (wParam == 1) {
      KillTimer(hDlg, 1);

      // Release the interactive guard
      isStartupGuarded = false;

      // 1. Re-enable the list view first so it can process the cleanup messages
      EnableWindow(hList, TRUE);

      // 2. Clear any stray mouse messages that arrived during
      // startup/initialization
      MSG msg;
      while (PeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE))
        ;

      // 3. Abort all internal modes (drags, marquees, mouse capture)
      ReleaseCapture();
      SendMessage(hDlg, WM_CANCELMODE, 0, 0);
      SendMessage(hList, WM_CANCELMODE, 0, 0);

      // 4. Force selection/focus reset on the list
      ListView_SetItemState(hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
      ListView_SetSelectionMark(hList, -1);

      // 5. Force focus to the query edit box as the absolute final action
      HWND hEdit = GetDlgItem(hDlg, IDC_EDIT_QUERY);
      SetFocus(hEdit);
      SendMessage(hEdit, EM_SETSEL, -1, -1);
    }
    break;
  }
  case WM_COMMAND: {
    int id = LOWORD(wParam);
    if (id == IDC_BTN_SEARCH) {
      if (!isSearching) {
        if (searchTargets.empty()) {
          MessageBoxW(hDlg, L"Please add at least one search candidate.",
                      L"Info", MB_OK);
          break;
        }

        // Update Options
        g_options.invertMatch =
            (IsDlgButtonChecked(hDlg, IDC_CHECK_NOT) == BST_CHECKED);

        isSearching = true;
        SetDlgItemTextW(hDlg, IDC_STATUS,
                        Localization::GetString(IDS_STATUS_BUSY));
        ListView_DeleteAllItems(hList);
        _beginthread(ScanThread, 0, (void *)hDlg);
      }
    } else if (id == IDC_BTN_SAVE) {
      SaveResults(hDlg);
    } else if (id == IDC_BTN_ADD) {
      AddFolder(hDlg);
    } else if (id == IDC_BTN_REMOVE) {
      RemoveFolder(hDlg);
    } else if (id == IDC_COMBO_CP && HIWORD(wParam) == CBN_SELCHANGE) {
      int idx = (int)SendDlgItemMessage(hDlg, IDC_COMBO_CP, CB_GETCURSEL, 0, 0);
      currentCodePage =
          (int)SendDlgItemMessage(hDlg, IDC_COMBO_CP, CB_GETITEMDATA, idx, 0);
    } else if (id == ID_POPUP_COPYPATH) {
    } else if (id >= ID_LANG_ENGLISH && id <= ID_LANG_PORTUGUESE) {
      int newLang = id - ID_LANG_ENGLISH;
      Localization::SetLanguage((LanguageID)newLang);
      currentLang = newLang;
      UpdateLanguage(hDlg);

      // Update Menu Checks
      HMENU hMenu = GetMenu(hDlg);
      for (int i = 0; i < APP_LANG_COUNT; i++) {
        CheckMenuItem(hMenu, ID_LANG_ENGLISH + i,
                      MF_BYCOMMAND |
                          (i == currentLang ? MF_CHECKED : MF_UNCHECKED));
      }
    } else if (id == ID_CONFIG_OPTIONS) {
      if (DialogBox(hInstBuffer, MAKEINTRESOURCE(IDD_CONFIG), hDlg,
                    ConfigDlgProc) == IDOK) {
        // Options updated
      }
      break;
    } else if (id == ID_HELP_ABOUT) {
      MessageBoxW(
          hDlg,
          L"Fast File Search v1.1\n\nHigh-performance File Search Tool.\n\n"
          L"Supports:\n"
          L"- NTFS (Direct MFT Access)\n"
          L"- FAT16/FAT32 (Direct Volume Access)\n"
          L"- exFAT (Direct Volume Access)\n\n"
          L"Scans raw disk structures for maximum speed.",
          L"About", MB_OK | MB_ICONINFORMATION);
    } else if (id == IDCANCEL) {
      SaveConfig(hDlg);
      EndDialog(hDlg, 0);
    }
    break;
  }
  case WM_ACTIVATE: {
    // Focus handled by Timer and Tab Order
    break;
  }
  case WM_NOTIFY: {
    LPNMHDR pnm = (LPNMHDR)lParam;
    if (pnm->idFrom == IDC_LIST_RESULTS) {
      if (pnm->code == NM_CUSTOMDRAW) {
        LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;
        if (lplvcd->nmcd.dwDrawStage == CDDS_PREPAINT) {
          SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
          return TRUE;
        }
        if (lplvcd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
          SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
          return TRUE;
        }
        if (lplvcd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM)) {
          int col = lplvcd->iSubItem;
          if (col == 0 || col == 1) { // Name or Path
            LRESULT ret = DrawItemWithHighlight(lplvcd, hDlg);
            SetWindowLongPtr(hDlg, DWLP_MSGRESULT, ret);
            return TRUE;
          }
          SetWindowLongPtr(hDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
          return TRUE;
        }
      } else if (pnm->code == LVN_COLUMNCLICK) {
        LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
        if (sortColumn == pnmv->iSubItem) {
          sortAscending = !sortAscending;
        } else {
          sortColumn = pnmv->iSubItem;
          sortAscending = true;
        }
        ListView_SortItems(hList, CompareFunc, 0);
      } else if (pnm->code == NM_RCLICK) {
        LPNMITEMACTIVATE pnmitem = (LPNMITEMACTIVATE)pnm;
        if (pnmitem->iItem != -1) {
          HMENU hMenu =
              LoadMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));
          HMENU hSubMenu = GetSubMenu(hMenu, 0);
          POINT pt;
          GetCursorPos(&pt);
          int cmd = TrackPopupMenu(hSubMenu, TPM_RETURNCMD, pt.x, pt.y, 0, hDlg,
                                   NULL);
          if (cmd == ID_POPUP_COPYPATH) {
            wchar_t buf[MAX_PATH];
            ListView_GetItemText(hList, pnmitem->iItem, 1, buf, MAX_PATH);

            if (OpenClipboard(hDlg)) {
              EmptyClipboard();
              size_t len = (wcslen(buf) + 1) * sizeof(wchar_t);
              HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
              memcpy(GlobalLock(hMem), buf, len);
              GlobalUnlock(hMem);
              SetClipboardData(CF_UNICODETEXT, hMem);
              CloseClipboard();
            }
          }
        }
      }
    }
    break;
  }
  case WM_USER + 1: // Search Done
    PopulateList();
    {
      wchar_t buf[100];
      wsprintfW(buf, Localization::GetString(IDS_STATUS_FOUND_FMT),
                searchResults.size());
      SetDlgItemTextW(hDlg, IDC_STATUS, buf);
    }
    break;
  case WM_USER + 2: // Error
  {
    std::wstring err = mftReader.GetLastErrorMessage();
    if (err.empty())
      err = L"Unknown Error or Scan Failed.";

    // Detailed error dialog
    MessageBoxW(hDlg, err.c_str(), L"FastFileSearch Error",
                MB_ICONERROR | MB_OK);

    SetDlgItemTextW(hDlg, IDC_STATUS, L"Failed.");
  }
    isSearching = false;
    break;
  }
  return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine,
                   int nShowCmd) {
  INITCOMMONCONTROLSEX icex;
  icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
  icex.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES;
  InitCommonControlsEx(&icex);

  CoInitialize(NULL); // Init COM for SHBrowseForFolder
  int ret =
      DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG), NULL, DialogProc);
  CoUninitialize();
  return (int)ret;
}

INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam,
                               LPARAM lParam) {
  auto UpdateUIState = [hDlg]() {
    BOOL sizeEnabled = IsDlgButtonChecked(hDlg, IDC_CHKS_SIZE) == BST_CHECKED;
    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MINSIZE), sizeEnabled);
    EnableWindow(GetDlgItem(hDlg, IDC_EDIT_MAXSIZE), sizeEnabled);
    BOOL dateEnabled = IsDlgButtonChecked(hDlg, IDC_CHKS_DATE) == BST_CHECKED;
    EnableWindow(GetDlgItem(hDlg, IDC_DATE_MIN), dateEnabled);
    EnableWindow(GetDlgItem(hDlg, IDC_DATE_MAX), dateEnabled);
  };

  switch (uMsg) {
  case WM_INITDIALOG: {
    // Localize Dialog
    SetWindowTextW(hDlg, Localization::GetString(IDS_CONFIG_TITLE));
    SetDlgItemTextW(hDlg, IDC_GRP_MATCHMODE,
                    Localization::GetString(IDS_GRP_MATCHMODE));

    SetDlgItemTextW(hDlg, IDC_RADIO_SUBSTRING,
                    Localization::GetString(IDS_RAD_SUBSTRING));
    SetDlgItemTextW(hDlg, IDC_RADIO_EXACT,
                    Localization::GetString(IDS_RAD_EXACT));
    SetDlgItemTextW(hDlg, IDC_RADIO_SPACED,
                    Localization::GetString(IDS_RAD_SPACED));
    SetDlgItemTextW(hDlg, IDC_RADIO_REGEX,
                    Localization::GetString(IDS_RAD_REGEX));
    SetDlgItemTextW(hDlg, IDC_CHKS_IGNORECASE,
                    Localization::GetString(IDS_CHK_IGNORECASE));

    SetDlgItemTextW(hDlg, IDC_GRP_SIZE, Localization::GetString(IDS_GRP_SIZE));
    SetDlgItemTextW(hDlg, IDC_CHKS_SIZE, Localization::GetString(IDS_CHK_SIZE));
    SetDlgItemTextW(hDlg, IDC_STATIC_MIN, Localization::GetString(IDS_LBL_MIN));
    SetDlgItemTextW(hDlg, IDC_STATIC_MAX, Localization::GetString(IDS_LBL_MAX));

    SetDlgItemTextW(hDlg, IDC_GRP_DATE, Localization::GetString(IDS_GRP_DATE));
    SetDlgItemTextW(hDlg, IDC_CHKS_DATE, Localization::GetString(IDS_CHK_DATE));
    SetDlgItemTextW(hDlg, IDC_STATIC_FROM,
                    Localization::GetString(IDS_LBL_FROM));
    SetDlgItemTextW(hDlg, IDC_STATIC_TO, Localization::GetString(IDS_LBL_TO));

    SetDlgItemTextW(hDlg, IDC_GRP_INCLUDE,
                    Localization::GetString(IDS_GRP_INCLUDE));
    SetDlgItemTextW(hDlg, IDC_CHKS_FILES,
                    Localization::GetString(IDS_CHK_FILES));
    SetDlgItemTextW(hDlg, IDC_CHKS_FOLDERS,
                    Localization::GetString(IDS_CHK_FOLDERS));

    SetDlgItemTextW(hDlg, IDC_GRP_TYPE, Localization::GetString(IDS_GRP_TYPE));
    SetDlgItemTextW(hDlg, IDC_STATIC_EXT, Localization::GetString(IDS_LBL_EXT));

    SetDlgItemTextW(hDlg, IDC_GRP_ADVANCED,
                    Localization::GetString(IDS_GRP_ADVANCED));
    SetDlgItemTextW(hDlg, IDC_CHKS_FULLPATH,
                    Localization::GetString(IDS_CHK_FULLPATH));
    SetDlgItemTextW(hDlg, IDC_STATIC_EXCLUDE,
                    Localization::GetString(IDS_LBL_EXCLUDE));

    SetDlgItemTextW(hDlg, IDOK, Localization::GetString(IDS_BTN_OK));
    SetDlgItemTextW(hDlg, IDCANCEL, Localization::GetString(IDS_BTN_CANCEL));

    // Mode
    int id = IDC_RADIO_SUBSTRING;
    if (g_options.mode == MatchMode_Exact)
      id = IDC_RADIO_EXACT;
    else if (g_options.mode == MatchMode_SpaceDivided)
      id = IDC_RADIO_SPACED;
    else if (g_options.mode == MatchMode_RegEx)
      id = IDC_RADIO_REGEX;
    CheckRadioButton(hDlg, IDC_RADIO_SUBSTRING, IDC_RADIO_REGEX, id);
    CheckDlgButton(hDlg, IDC_CHKS_IGNORECASE,
                   g_options.ignoreCase ? BST_CHECKED : BST_UNCHECKED);

    // Size
    CheckDlgButton(hDlg, IDC_CHKS_SIZE,
                   (g_options.minSize > 0 || g_options.maxSize > 0)
                       ? BST_CHECKED
                       : BST_UNCHECKED);
    SetDlgItemInt(hDlg, IDC_EDIT_MINSIZE, (UINT)(g_options.minSize / 1024),
                  FALSE);
    SetDlgItemInt(hDlg, IDC_EDIT_MAXSIZE, (UINT)(g_options.maxSize / 1024),
                  FALSE);

    // Date
    CheckDlgButton(hDlg, IDC_CHKS_DATE,
                   (g_options.minDate > 0 || g_options.maxDate > 0)
                       ? BST_CHECKED
                       : BST_UNCHECKED);
    SYSTEMTIME stMin, stMax;
    FILETIME ft;
    if (g_options.minDate > 0) {
      ULARGE_INTEGER uli;
      uli.QuadPart = g_options.minDate;
      ft.dwLowDateTime = uli.LowPart;
      ft.dwHighDateTime = uli.HighPart;
      FileTimeToSystemTime(&ft, &stMin);
    } else
      GetLocalTime(&stMin);
    if (g_options.maxDate > 0) {
      ULARGE_INTEGER uli;
      uli.QuadPart = g_options.maxDate;
      ft.dwLowDateTime = uli.LowPart;
      ft.dwHighDateTime = uli.HighPart;
      FileTimeToSystemTime(&ft, &stMax);
    } else
      GetLocalTime(&stMax);
    DateTime_SetSystemtime(GetDlgItem(hDlg, IDC_DATE_MIN), GDT_VALID, &stMin);
    DateTime_SetSystemtime(GetDlgItem(hDlg, IDC_DATE_MAX), GDT_VALID, &stMax);

    // Include
    CheckDlgButton(hDlg, IDC_CHKS_FILES,
                   g_options.includeFiles ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hDlg, IDC_CHKS_FOLDERS,
                   g_options.includeFolders ? BST_CHECKED : BST_UNCHECKED);

    // Extension
    SetDlgItemTextW(hDlg, IDC_EDIT_EXT, g_options.extensionFilter.c_str());

    // Advanced
    CheckDlgButton(hDlg, IDC_CHKS_FULLPATH,
                   g_options.matchFullPath ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemTextW(hDlg, IDC_EDIT_EXCLUDE, g_options.excludePattern.c_str());

    UpdateUIState();
    return TRUE;
  }
  case WM_COMMAND: {
    int id = LOWORD(wParam);
    if (id == IDOK) {
      // Mode
      if (IsDlgButtonChecked(hDlg, IDC_RADIO_SUBSTRING))
        g_options.mode = MatchMode_Substring;
      else if (IsDlgButtonChecked(hDlg, IDC_RADIO_EXACT))
        g_options.mode = MatchMode_Exact;
      else if (IsDlgButtonChecked(hDlg, IDC_RADIO_SPACED))
        g_options.mode = MatchMode_SpaceDivided;
      else if (IsDlgButtonChecked(hDlg, IDC_RADIO_REGEX))
        g_options.mode = MatchMode_RegEx;
      g_options.ignoreCase =
          (IsDlgButtonChecked(hDlg, IDC_CHKS_IGNORECASE) == BST_CHECKED);

      // Size
      if (IsDlgButtonChecked(hDlg, IDC_CHKS_SIZE) == BST_CHECKED) {
        g_options.minSize =
            (uint64_t)GetDlgItemInt(hDlg, IDC_EDIT_MINSIZE, NULL, FALSE) * 1024;
        g_options.maxSize =
            (uint64_t)GetDlgItemInt(hDlg, IDC_EDIT_MAXSIZE, NULL, FALSE) * 1024;
      } else {
        g_options.minSize = 0;
        g_options.maxSize = 0;
      }

      // Date
      if (IsDlgButtonChecked(hDlg, IDC_CHKS_DATE) == BST_CHECKED) {
        SYSTEMTIME st;
        FILETIME ft;
        ULARGE_INTEGER uli;
        DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_DATE_MIN), &st);
        SystemTimeToFileTime(&st, &ft);
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        g_options.minDate = uli.QuadPart;

        DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_DATE_MAX), &st);
        st.wHour = 23;
        st.wMinute = 59;
        st.wSecond = 59; // End of day
        SystemTimeToFileTime(&st, &ft);
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        g_options.maxDate = uli.QuadPart;
      } else {
        g_options.minDate = 0;
        g_options.maxDate = 0;
      }

      // Include
      g_options.includeFiles =
          (IsDlgButtonChecked(hDlg, IDC_CHKS_FILES) == BST_CHECKED);
      g_options.includeFolders =
          (IsDlgButtonChecked(hDlg, IDC_CHKS_FOLDERS) == BST_CHECKED);

      // Extension
      wchar_t extBuf[256];
      GetDlgItemTextW(hDlg, IDC_EDIT_EXT, extBuf, 256);
      g_options.extensionFilter = extBuf;

      // Advanced
      g_options.matchFullPath =
          (IsDlgButtonChecked(hDlg, IDC_CHKS_FULLPATH) == BST_CHECKED);
      wchar_t exBuf[256];
      GetDlgItemTextW(hDlg, IDC_EDIT_EXCLUDE, exBuf, 256);
      g_options.excludePattern = exBuf;

      EndDialog(hDlg, IDOK);
      return TRUE;
    } else if (id == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return TRUE;
    } else if (id == IDC_CHKS_SIZE || id == IDC_CHKS_DATE) {
      UpdateUIState();
    }
    break;
  }
  }
  return FALSE;
}
