#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <string>
#include <functional>
#include "Bridge.h"
#include "Storage.h"
#include "PipelineRunner.h"
#include "PromptsLocalization.h"
#include "NodeData.h"
#include "JsonParser.h"

#include <WebView2.h>
// ICoreWebView2 and ICoreWebView2Controller are defined in WebView2.h

class App {
public:
    App();
    ~App();
    
    bool Init(HINSTANCE hInstance, bool embedded);
    int Run();
    
    // Window message handling
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    HWND Hwnd() const { return hwnd_; }
    Storage &GetStorage() { return storage_; }
    PipelineRunner &GetRunner() { return runner_; }
    Bridge &GetBridge() { return bridge_; }
    PromptsLocalization &GetLocalization() { return localization_; }
    
    bool IsEmbedded() const { return embedded_; }

private:
    HWND hwnd_{nullptr};
    HWND hRichEdit_{nullptr};
    HINSTANCE hInst_{nullptr};
    bool embedded_ = false;
    
    Storage storage_;
    Bridge bridge_;
    PipelineRunner runner_;
    PromptsLocalization localization_;
    
    std::wstring appDataPath_;
    
    // WebView2
    void InitWebView2();
    void ShowFallbackUI();
    void InitRichEdit();
    ICoreWebView2 *webview_{nullptr};
    ICoreWebView2Controller *webviewController_{nullptr};
    
    // Message handlers
    void OnCreate();
    void OnDestroy();
    void OnSize(int width, int height);
    
    // Bridge handlers (JS → C++)
    void HandleBridgeMessage(const std::string &type, const std::string &payload);
    
    // Helpers
    std::wstring GetAppDataPath();
    static Node NodeFromJson(const JsonValue &val);
    void SendFullInit();

    // Pipeline run context
    std::string inputNodeId_;
    std::string inputTabFile_;
    
    // Window class
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Menu
    void CreateMenuBar();
    void OnCommand(int id);
    void OpenRecentFile(int index);
    void OpenFileDialog();
    void SaveFileDialog();
    
    // Bridge: pipeline management
    void HandleSavePipeline(const std::string &payload);
    void HandleDeletePipeline(const std::string &payload);

    // Bridge: execution history
    void HandleHistoryList();
    void HandleHistoryDetail(const std::string &payload);
    
    // Recent files
    void AddRecentFile(const std::wstring &path);
    void UpdateRecentFilesMenu();
    
    // RichEdit
    void ShowRichEdit(bool show, const std::string &rtfContent = "");
    std::string GetRichEditContent();
    void PositionRichEdit(int x, int y, int w, int h);
    void InitRichEditToolbar(HWND parent);
    HWND hFormatToolbar_{nullptr};
    
    // Recent files
    std::vector<std::wstring> recentFiles_;
    static const int MAX_RECENT_FILES = 10;
};
