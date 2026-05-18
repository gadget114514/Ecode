#pragma once
#include "JSONEditorLocalization.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <windows.h>

#define WM_EMBED_APP (WM_APP + 1)

struct EmbeddedAppConfig {
  std::wstring name;
  std::wstring exeName;
};

class EditorWindow {
public:
  EditorWindow();
  ~EditorWindow();

  bool Create(PCWSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle = 0,
              int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
              int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT,
              HWND hWndParent = 0, HMENU hMenu = 0);
  HWND Window() const { return m_hwnd; }
  void UpdateLineNumbers(HWND hEdit);

protected:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCreate();
  void OnSize(int width, int height);
  void OnCommand(int id, int code);
  void OnDestroy();

  // File operations
  void NewFile();
  void OpenFile();
  void SaveFile();
  void SaveFileAs();
  void CloseCurrentTab();
  void UpdateTitle();
  void SwitchTab(int index);
  void FormatJson();
  void FormatYaml();

  // Settings & Persistence
  void LoadSettings();
  void SaveSettings();

  void UpdateMenus();

  void LaunchChildApp(int tabIndex, const EmbeddedAppConfig &config);

private:
  struct Document {
    HWND hEdit;
    HWND hLineNum;
    std::wstring filePath; // Empty for new untitled files
    std::wstring fileName; // Display name
    bool isDirty;
    int eolMode; // 0: CRLF, 1: LF, 2: CR

    // Internal Data Structure
    nlohmann::json jsonData;
    enum { FMT_TEXT, FMT_JSON, FMT_YAML } format = FMT_TEXT;

    // Child app embedding
    bool isChildApp;
    HWND hChildApp;
    HWND hPlaceholder;
    HANDLE hChildProcess;
    DWORD childProcessId;
  };

  HWND m_hwnd;
  HWND m_hTabCtrl;
  std::vector<Document> m_documents;
  int m_activePageIndex;

  JSONEditorLocalization m_localization;
  std::vector<EmbeddedAppConfig> m_embeddedApps;

  void CreateNewTab(const std::wstring &path = L"",
                    const std::wstring &content = L"");
  void ResizeTabControl();
  std::wstring GetFileNameFromPath(const std::wstring &path);

  // Tree View & Data Model
  void UpdateTreeFromText();
  void UpdateTextFromModel(bool toYaml = false);
  void SyncModelToTree(); // Uses internal jsonData
  HWND m_hTreeView;
  HWND m_hOpenBtn = NULL;
  HWND m_hSaveBtn = NULL;
};
