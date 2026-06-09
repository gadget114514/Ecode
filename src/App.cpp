#include "App.h"
#include "resource.h"
#include "JsonParser.h"
#include "Base64.h"
#include "AIProvider.h"
#include <shlobj.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <richedit.h>
#include <ole2.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <WebView2.h>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Microsoft::WRL;

#define WM_APP_WEBVIEW2_READY (WM_APP + 1)
#define WM_RELOAD_SESSION (WM_APP + 2)

App::App()
    : versionMgr_(storage_)
    , optimizer_(storage_)
{
    runner_.SetBridgeCallback([this](const std::string &type, const std::string &json) {
        if (type == "pipeline_completed") {
            storage_.SaveHistory(json);
        }
        bridge_.PostToJS("log", "{\"message\":\"🏃 PipelineRunner Callback: type=" + type + "\"}");
        bridge_.PostToJS(type, json);
    });
    bridge_.SetHandler([this](const std::string &type, const std::string &payload) {
        HandleBridgeMessage(type, payload);
    });
}

App::~App() {}

std::wstring App::GetAppDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::wstring(path) + L"\\Ecode\\Prompts";
    }
    return L".\\prompts_data";
}

bool App::Init(HINSTANCE hInstance, bool embedded) {
    hInst_ = hInstance;
    embedded_ = embedded;
    
    appDataPath_ = GetAppDataPath();
    if (!storage_.Init(appDataPath_)) return false;
    
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PromptsAppClass";
    if (!RegisterClassExW(&wc)) return false;
    
    DWORD style = embedded ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    DWORD exStyle = embedded ? WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW : 0;
    
    hwnd_ = CreateWindowExW(exStyle, L"PromptsAppClass", L"Prompts",
                            style, CW_USEDEFAULT, CW_USEDEFAULT,
                            1000, 700, nullptr, nullptr, hInstance, this);
    if (!hwnd_) return false;
    
    return true;
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App *app = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
        app = (App *)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
        app->hwnd_ = hwnd;
    } else {
        app = (App *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if (app) return app->HandleMessage(msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT App::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        OnCreate();
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    case WM_SIZE:
        OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd_);
        return 0;
    case WM_COMMAND:
        OnCommand(LOWORD(wParam));
        return 0;
    case WM_APP_WEBVIEW2_READY:
        if (wParam == 0) ShowFallbackUI();
        return 0;
    case WM_RELOAD_SESSION:
        SendFullInit();
        return 0;
    default:
        return DefWindowProc(hwnd_, msg, wParam, lParam);
    }
}

void App::OnCreate() {
    if (!embedded_) CreateMenuBar();
}

void App::CreateMenuBar() {
    HMENU hMenu = LoadMenuW(hInst_, MAKEINTRESOURCEW(IDR_MENU_MAIN));
    if (hMenu) SetMenu(hwnd_, hMenu);
}

void App::OnCommand(int id) {
    switch (id) {
    case ID_FILE_NEW:
        bridge_.PostToJS("menu_command", "{\"action\":\"new_tab\"}");
        break;
    case ID_FILE_OPEN:
        OpenFileDialog();
        break;
    case ID_FILE_SAVE:
        bridge_.PostToJS("menu_command", "{\"action\":\"save\"}");
        break;
    case ID_FILE_SAVE_AS:
        bridge_.PostToJS("menu_command", "{\"action\":\"save_as\"}");
        break;
    case ID_FILE_IMPORT_ZIP:
        bridge_.PostToJS("menu_command", "{\"action\":\"import_zip\"}");
        break;
    case ID_FILE_EXPORT_NODE:
        bridge_.PostToJS("menu_command", "{\"action\":\"export_node\"}");
        break;
    case ID_FILE_EXIT:
        DestroyWindow(hwnd_);
        break;
    case ID_PIPELINE_RUN:
        bridge_.PostToJS("menu_command", "{\"action\":\"run_pipeline\"}");
        break;
    case ID_PIPELINE_MANAGER:
        bridge_.PostToJS("menu_command", "{\"action\":\"pipeline_manager\"}");
        break;
    case ID_PIPELINE_HISTORY:
        bridge_.PostToJS("menu_command", "{\"action\":\"pipeline_history\"}");
        break;
    case ID_PIPELINE_CANCEL:
        runner_.Cancel();
        break;
    case ID_PROVIDERS_CONFIGURE:
        bridge_.PostToJS("menu_command", "{\"action\":\"config\"}");
        break;
    case ID_PROVIDERS_TEST:
        bridge_.PostToJS("menu_command", "{\"action\":\"test_connection\"}");
        break;
    case ID_RECIPE_MANAGER:
        bridge_.PostToJS("menu_command", "{\"action\":\"recipe_manager\"}");
        break;
    case ID_RECIPE_CONFIGURE:
        bridge_.PostToJS("menu_command", "{\"action\":\"config\"}");
        break;
    case ID_VIEW_TREE:
    case ID_VIEW_LIST:
    case ID_VIEW_EDITOR:
    case ID_VIEW_MESSAGES: {
        std::string pane = id == ID_VIEW_TREE ? "tree" :
                           id == ID_VIEW_LIST ? "list" :
                           id == ID_VIEW_EDITOR ? "editor" : "messages";
        bridge_.PostToJS("menu_command", "{\"action\":\"toggle_pane\",\"pane\":\"" + pane + "\"}");
        break;
    }
    case ID_VIEW_FULLSCREEN:
        if (IsZoomed(hwnd_))
            ShowWindow(hwnd_, SW_RESTORE);
        else
            ShowWindow(hwnd_, SW_MAXIMIZE);
        break;
    case ID_HELP_WIZARD:
        bridge_.PostToJS("menu_command", "{\"action\":\"welcome_wizard\"}");
        break;
    case ID_HELP_SETUP_WIZARD:
        bridge_.PostToJS("menu_command", "{\"action\":\"setup_wizard\"}");
        break;
    case ID_HELP_DOCS:
        ShellExecuteW(hwnd_, L"open", L"https://github.com/gadget114514/Ecode", nullptr, nullptr, SW_SHOW);
        break;
    case ID_HELP_ABOUT:
        bridge_.PostToJS("menu_command", "{\"action\":\"about\"}");
        break;
    default:
        // Recent files (ID_FILE_RECENT_BASE + index)
        if (id >= ID_FILE_RECENT_BASE && id < ID_FILE_RECENT_BASE + MAX_RECENT_FILES) {
            OpenRecentFile(id - ID_FILE_RECENT_BASE);
        }
        break;
    }
}

void App::OpenFileDialog() {
    wchar_t path[32768] = {};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = 32768;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        std::wstring wpath(path);
        AddRecentFile(wpath);
        std::string pathA;
        int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        pathA.resize(n);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &pathA[0], n, nullptr, nullptr);
        if (!pathA.empty() && pathA.back() == '\0') pathA.pop_back();
        bridge_.PostToJS("open_file_result", "{\"path\":\"" + PipelineRunner::JsonEscape(pathA) + "\"}");
    }
}

void App::SaveFileDialog() {
    wchar_t path[32768] = {};
    OPENFILENAMEW ofn = {sizeof(ofn)};
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = 32768;
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
    if (GetSaveFileNameW(&ofn)) {
        std::wstring wpath(path);
        AddRecentFile(wpath);
        std::string pathA;
        int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        pathA.resize(n);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &pathA[0], n, nullptr, nullptr);
        if (!pathA.empty() && pathA.back() == '\0') pathA.pop_back();
        bridge_.PostToJS("save_as_result", "{\"path\":\"" + PipelineRunner::JsonEscape(pathA) + "\"}");
    }
}

void App::OpenRecentFile(int index) {
    if (index < 0 || index >= (int)recentFiles_.size()) return;
    std::wstring path = recentFiles_[index];
    // Move to front
    recentFiles_.erase(recentFiles_.begin() + index);
    recentFiles_.insert(recentFiles_.begin(), path);
    std::string pathA;
    int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    pathA.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &pathA[0], n, nullptr, nullptr);
    if (!pathA.empty() && pathA.back() == '\0') pathA.pop_back();
    bridge_.PostToJS("open_file_result", "{\"path\":\"" + PipelineRunner::JsonEscape(pathA) + "\"}");
    UpdateRecentFilesMenu();
}

void App::AddRecentFile(const std::wstring &path) {
    for (auto it = recentFiles_.begin(); it != recentFiles_.end(); ++it) {
        if (*it == path) { recentFiles_.erase(it); break; }
    }
    recentFiles_.insert(recentFiles_.begin(), path);
    if ((int)recentFiles_.size() > MAX_RECENT_FILES)
        recentFiles_.resize(MAX_RECENT_FILES);
    UpdateRecentFilesMenu();
    storage_.SaveRecentFiles(recentFiles_);
}

void App::UpdateRecentFilesMenu() {
    if (embedded_) return;
    HMENU hMenu = GetMenu(hwnd_);
    if (!hMenu) return;
    HMENU hFileMenu = GetSubMenu(hMenu, 0);
    if (!hFileMenu) return;
    // Remove old recent items (search for separator before recent files)
    int menuCount = GetMenuItemCount(hFileMenu);
    int sepPos = -1;
    for (int i = 0; i < menuCount; i++) {
        wchar_t buf[256] = {};
        MENUITEMINFOW mii = {sizeof(mii), MIIM_STRING};
        mii.dwTypeData = buf;
        mii.cch = 256;
        if (GetMenuItemInfoW(hFileMenu, i, TRUE, &mii)) {
            if (buf[0] == 0 && i > 0) sepPos = i; // already has separator
        }
    }
    // Remove old recent items
    if (sepPos > 0) {
        while (GetMenuItemCount(hFileMenu) > sepPos + 1)
            RemoveMenu(hFileMenu, GetMenuItemCount(hFileMenu) - 1, MF_BYPOSITION);
    }
    if (recentFiles_.empty()) return;
    // Add separator if needed
    if (sepPos <= 0) {
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
        sepPos = GetMenuItemCount(hFileMenu) - 1;
    }
    // Add recent files
    int count = 0;
    for (auto &f : recentFiles_) {
        if (count >= 5) break; // Show max 5 in menu
        std::wstring label = L"&" + std::to_wstring(count + 1) + L" " + f;
        AppendMenuW(hFileMenu, MF_STRING, ID_FILE_RECENT_BASE + count, label.c_str());
        count++;
    }
    DrawMenuBar(hwnd_);
}

void App::OnDestroy() {
    runner_.Cancel();
    if (webview_) webview_->Release();
    if (webviewController_) webviewController_->Release();
    PostQuitMessage(0);
}

void App::OnSize(int width, int height) {
    if (webviewController_) {
        RECT bounds = {0, 0, width, height};
        webviewController_->put_Bounds(bounds);
    }
}

void App::InitWebView2() {
    // Use concrete COM objects for WebView2 callbacks
    // Type names from WebView2 SDK header:
    // ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
    // ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
    
    struct EnvHandler : RuntimeClass<RuntimeClassFlags<ClassicCom>, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler> {
        App *app;
        EnvHandler(App *a) : app(a) {}
        HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment *env) override {
            if (FAILED(result) || !env) {
                PostMessage(app->hwnd_, WM_APP_WEBVIEW2_READY, 0, 0); return S_OK;
            }
            
            struct CtrlHandler : RuntimeClass<RuntimeClassFlags<ClassicCom>, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler> {
                App *app;
                CtrlHandler(App *a) : app(a) {}
                HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller *controller) override {
                    if (FAILED(result) || !controller) {
                        PostMessage(app->hwnd_, WM_APP_WEBVIEW2_READY, 0, 0); return S_OK;
                    }
                    
                    app->webviewController_ = controller;
                    controller->AddRef();
                    controller->get_CoreWebView2(&app->webview_);
                    
                    if (app->webview_) {
                        RECT rc; GetClientRect(app->hwnd_, &rc);
                        controller->put_Bounds(rc);
                        app->bridge_.SetWebView((IUnknown*)app->webview_);

                        // Set HTTP log callback (logs request/response to JS message log)
                        {
                            App *appPtr = app;
                            SetHttpLogCallback([appPtr](const std::string &html) {
                                appPtr->bridge_.PostToJS("log", "{\"message\":\"" + PipelineRunner::JsonEscape(html) + "\"}");
                            });
                        }

                        // Use virtual host mapping instead of file://
                        wchar_t exePath[MAX_PATH];
                        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                        wchar_t *p = wcsrchr(exePath, L'\\');
                        if (p) { p[1] = 0; wcscat_s(exePath, L"frontend"); }

                        // Register web message handler (JS → C++)
                        struct MsgHandler : RuntimeClass<RuntimeClassFlags<ClassicCom>, ICoreWebView2WebMessageReceivedEventHandler> {
                            App *app;
                            MsgHandler(App *a) : app(a) {}
                            HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) override {
                                LPWSTR raw = nullptr;
                                args->get_WebMessageAsJson(&raw);
                                if (raw) {
                                    int len = WideCharToMultiByte(CP_UTF8, 0, raw, -1, nullptr, 0, nullptr, nullptr);
                                    std::string s(len, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, raw, -1, &s[0], len, nullptr, nullptr);
                                    CoTaskMemFree(raw);

                                    auto val = JsonValue::parse(s);
                                    std::string type = val["type"].string();
                                    app->bridge_.PostToJS("log", "{\"message\":\"📥 WebMessage Received: type=" + type + "\"}");
                                    if (type == "init_complete") {
                                        app->bridge_.PostToJS("log", "{\"message\":\"[TRACE] init_complete received from JS, calling SendFullInit\"}");
                                        app->SendFullInit();
                                    } else {
                                        // payload may be an object or string — serialize to JSON string
                                        std::string payload;
                                        if (val.has("payload")) {
                                            auto &pv = val["payload"];
                                            payload = pv.serialize();
                                            // unwrap outer quotes if it was a plain string
                                            if (payload.size() >= 2 && payload.front() == '"' && payload.back() == '"')
                                                payload = JsonValue::parse(payload).string();
                                        }
                                        app->HandleBridgeMessage(type, payload);
                                    }
                                }
                                return S_OK;
                            }
                        };

                        app->webview_->add_WebMessageReceived(Make<MsgHandler>(app).Get(), nullptr);

                        // Handle permissions (allow microphone access for voice input/Web Speech API)
                        struct PermissionHandler : RuntimeClass<RuntimeClassFlags<ClassicCom>, ICoreWebView2PermissionRequestedEventHandler> {
                            App *app;
                            PermissionHandler(App *a) : app(a) {}
                            HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2PermissionRequestedEventArgs *args) override {
                                COREWEBVIEW2_PERMISSION_KIND kind;
                                if (SUCCEEDED(args->get_PermissionKind(&kind))) {
                                    if (kind == COREWEBVIEW2_PERMISSION_KIND_MICROPHONE) {
                                        args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
                                    }
                                }
                                return S_OK;
                            }
                        };
                        app->webview_->add_PermissionRequested(Make<PermissionHandler>(app).Get(), nullptr);

                        // Notify JS when navigation is completed so frontend knows the host is ready
                        struct NavCompletedHandler : RuntimeClass<RuntimeClassFlags<ClassicCom>, ICoreWebView2NavigationCompletedEventHandler> {
                            App *app;
                            NavCompletedHandler(App *a) : app(a) {}
                            HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) override {
                                BOOL isSuccess = FALSE;
                                COREWEBVIEW2_WEB_ERROR_STATUS errStatus = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                                if (args) {
                                    args->get_IsSuccess(&isSuccess);
                                    args->get_WebErrorStatus(&errStatus);
                                }
                                app->bridge_.PostToJS("log", "{\"message\":\"[TRACE] NavigationCompleted: success=" + std::to_string(isSuccess) + " errorStatus=" + std::to_string((int)errStatus) + "\"}");
                                if (isSuccess) {
                                    app->bridge_.PostToJS("ready", "{}");
                                } else {
                                    app->bridge_.PostToJS("log", "{\"message\":\"⚠ Navigation FAILED — init flow will not complete\"}");
                                }
                                return S_OK;
                            }
                        };
                        app->webview_->add_NavigationCompleted(Make<NavCompletedHandler>(app).Get(), nullptr);

                        // Set up virtual host mapping
                        ICoreWebView2_3 *wv3 = nullptr;
                        HRESULT hrQI = app->webview_->QueryInterface(
                            __uuidof(ICoreWebView2_3), (void**)&wv3);
                        if (SUCCEEDED(hrQI) && wv3) {
                            wv3->SetVirtualHostNameToFolderMapping(
                                L"prompts.app", exePath,
                                COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                            wv3->Navigate(L"https://prompts.app/index.html");
                            wv3->Release();
                        } else {
                            wchar_t fallbackPath[MAX_PATH];
                            GetModuleFileNameW(nullptr, fallbackPath, MAX_PATH);
                            wchar_t *fp = wcsrchr(fallbackPath, L'\\');
                            if (fp) { fp[1] = 0; wcscat_s(fallbackPath, L"frontend\\index.html"); }
                            app->webview_->Navigate(fallbackPath);
                        }
                    }
                    PostMessage(app->hwnd_, WM_APP_WEBVIEW2_READY, 1, 0);
                    return S_OK;
                }
            };
            
            env->CreateCoreWebView2Controller(app->hwnd_, Make<CtrlHandler>(app).Get());
            return S_OK;
        }
    };
    
    auto handler = Make<EnvHandler>(this);
    if (!handler) { ShowFallbackUI(); return; }
    
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, handler.Get());
    if (FAILED(hr)) {
        hr = CreateCoreWebView2Environment(handler.Get());
        if (FAILED(hr)) ShowFallbackUI();
    }
}

void App::ShowFallbackUI() {
    RECT rc; GetClientRect(hwnd_, &rc);
    HWND hEdit = CreateWindowExW(0, L"EDIT", 
        L"Prompts Application - WebView2 mode\r\n"
        L"WebView2 Runtime not found.\r\n"
        L"Please install WebView2 Runtime to enable the full HTML UI.\r\n\r\n"
        L"The application is running in console fallback mode.\r\n"
        L"Frontend files are located in: frontend/",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        0, 0, rc.right, rc.bottom, hwnd_, nullptr, hInst_, nullptr);
    if (hEdit) {
        SendMessage(hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
}

void App::SendFullInit() {
    bridge_.PostToJS("log", "{\"message\":\"[TRACE] SendFullInit: loading session...\"}");

    // Load session
    auto session = storage_.LoadSession();

    // If no tabs, create default
    if (session.tabs.empty()) {
        TabData tab;
        tab.name = "General";
        tab.file = "general.json";
        storage_.EnsureDirectory(storage_.DataPath(L""));
        Node emptyRoot;
        emptyRoot.mimetype = "text/plain";
        storage_.SaveTabData(L"general.json", emptyRoot);
        session.tabs.push_back(tab);
        storage_.SaveSession(session);
    }

    // Build tabs JSON and nodes map
    std::string tabsJson = "[";
    std::string nodesJson = "{";
    bool firstTab = true;
    for (auto &tab : session.tabs) {
        if (!firstTab) { tabsJson += ","; nodesJson += ","; }
        firstTab = false;
        tabsJson += "{\"name\":\"" + PipelineRunner::JsonEscape(tab.name) + "\""
                  + ",\"file\":\"" + PipelineRunner::JsonEscape(tab.file) + "\"}";
        std::wstring wfile(tab.file.begin(), tab.file.end());
        Node root = storage_.LoadTabData(wfile);
        nodesJson += "\"" + PipelineRunner::JsonEscape(tab.file) + "\":"
                   + Storage::SerializeNode(root);
    }
    tabsJson += "]";
    nodesJson += "}";

    // Load pipelines
    auto pipelines = storage_.LoadPipelines();
    std::string pipelinesJson = Storage::SerializePipelines(pipelines);
    // Strip outer {"pipelines":} wrapper, keep array only
    auto pv = JsonValue::parse(pipelinesJson);
    if (pv.has("pipelines")) {
        pipelinesJson = pv["pipelines"].serialize();
    } else {
        pipelinesJson = "[]";
    }

    // Load and register API providers (resume saved keys)
    {
        bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::SendFullInit: Loading providers to register...\"}");
        auto providers = storage_.LoadProviders();
        bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::SendFullInit: Loaded " + std::to_string(providers.size()) + " providers\"}");
        for (auto &kv : providers) {
            bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::SendFullInit: Registering " + kv.first + " with key='" + kv.second.apiKey + "' url='" + kv.second.baseUrl + "'\"}");
            runner_.RegisterProvider(kv.first, kv.second.apiKey, kv.second.baseUrl);
        }
    }

    // Recent files
    recentFiles_ = storage_.LoadRecentFiles();
    UpdateRecentFilesMenu();
    std::string recentJson = "[";
    bool firstRF = true;
    for (auto &rf : recentFiles_) {
        if (!firstRF) recentJson += ",";
        firstRF = false;
        std::string rfA;
        int n = WideCharToMultiByte(CP_UTF8, 0, rf.c_str(), -1, nullptr, 0, nullptr, nullptr);
        rfA.resize(n);
        WideCharToMultiByte(CP_UTF8, 0, rf.c_str(), -1, &rfA[0], n, nullptr, nullptr);
        if (!rfA.empty() && rfA.back() == '\0') rfA.pop_back();
        recentJson += "\"" + PipelineRunner::JsonEscape(rfA) + "\"";
    }
    recentJson += "]";

    // Run blob GC at startup — collect all referenced blob paths from data files
    std::vector<std::wstring> referencedBlobs;
    auto collectRefs = [&](const Node &n, auto &self) -> void {
        for (auto &a : n.attachments) {
            if (!a.file.empty()) {
                referencedBlobs.push_back(std::wstring(a.file.begin(), a.file.end()));
            }
        }
        for (auto &c : n.children) self(c, self);
    };
    for (auto &tab : session.tabs) {
        std::wstring wfile(tab.file.begin(), tab.file.end());
        Node root = storage_.LoadTabData(wfile);
        collectRefs(root, collectRefs);
    }
    storage_.GarbageCollectBlobs(referencedBlobs);

    // Build providers JSON
    std::string providersJson = "{";
    {
        bool firstProv = true;
        bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::SendFullInit: Loading providers for JS payload...\"}");
        auto providers = storage_.LoadProviders();
        for (auto &kv : providers) {
            if (!firstProv) providersJson += ",";
            firstProv = false;
            std::map<std::string, JsonValue> cfg;
            cfg["apiKey"]  = JsonValue::fromString(kv.second.apiKey);
            cfg["baseUrl"] = JsonValue::fromString(kv.second.baseUrl);
            std::vector<JsonValue> models;
            for (auto &m : kv.second.models)
                models.push_back(JsonValue::fromString(m));
            cfg["models"] = JsonValue::fromArray(models);
            providersJson += "\"" + PipelineRunner::JsonEscape(kv.first) + "\":"
                           + JsonValue::fromObject(cfg).serialize(false);
            bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::SendFullInit: Adding to JS payload -> " + kv.first + " key='" + kv.second.apiKey + "'\"}");
        }
    }
    providersJson += "}";

    // Load general config and chest list
    auto generalCfg = storage_.LoadGeneralConfig();
    auto chestNames = storage_.ListNamedChests();
    std::string chestListJson = "[";
    for (size_t i = 0; i < chestNames.size(); i++) {
        if (i > 0) chestListJson += ",";
        chestListJson += "\"" + PipelineRunner::JsonEscape(chestNames[i]) + "\"";
    }
    chestListJson += "]";
    std::string configJson = "{\"historyRetention\":" + std::to_string(generalCfg.historyRetention)
                           + ",\"chestList\":" + chestListJson
                           + ",\"defaultProvider\":\"" + PipelineRunner::JsonEscape(generalCfg.defaultProvider) + "\""
                           + ",\"defaultModel\":\"" + PipelineRunner::JsonEscape(generalCfg.defaultModel) + "\"}";

    // Load recipes
    auto recipes = storage_.LoadRecipes();
    std::string recipesJson = "[";
    for (size_t i = 0; i < recipes.size(); i++) {
        if (i > 0) recipesJson += ",";
        std::map<std::string, JsonValue> obj;
        obj["type"] = JsonValue::fromString(recipes[i].type);
        obj["name"] = JsonValue::fromString(recipes[i].name);
        obj["provider"] = JsonValue::fromString(recipes[i].provider);
        obj["model"] = JsonValue::fromString(recipes[i].model);
        obj["temperature"] = JsonValue::fromDouble(recipes[i].temperature);
        obj["systemPrompt"] = JsonValue::fromString(recipes[i].systemPrompt);
        obj["command"] = JsonValue::fromString(recipes[i].command);
        recipesJson += JsonValue::fromObject(obj).serialize(false);
    }
    recipesJson += "]";

    bridge_.PostToJS("log", "{\"message\":\"[TRACE] SendFullInit: posting init message...\"}");
    bridge_.PostToJS("init",
        "{\"language\":\"" + localization_.GetCurrentLanguage() + "\""
        ",\"embedded\":"  + (embedded_ ? "true" : "false") +
        ",\"tabs\":"      + tabsJson +
        ",\"nodes\":"     + nodesJson +
        ",\"pipelines\":" + pipelinesJson +
        ",\"providers\":" + providersJson +
        ",\"recentFiles\":" + recentJson +
        ",\"config\":"    + configJson +
        ",\"recipes\":"   + recipesJson + "}");
    bridge_.PostToJS("log", "{\"message\":\"[TRACE] SendFullInit: init posted\"}");
}

Node App::NodeFromJson(const JsonValue &val) {
    Node node;
    if (val.has("title"))   node.title   = val["title"].string();
    if (val.has("content")) node.content = val["content"].string();
    if (val.has("mimetype")) node.mimetype = val["mimetype"].string();
    if (val.has("pipelineMeta")) node.pipelineMeta = val["pipelineMeta"].string();
    if (val.has("attachments")) {
        for (auto &a : val["attachments"].array()) {
            Attachment att;
            if (a.has("id"))      att.id       = a["id"].string();
            if (a.has("mimetype")) att.mimetype = a["mimetype"].string();
            if (a.has("inline"))  att.inlineData = a["inline"].boolean();
            if (a.has("content")) att.content  = a["content"].string();
            if (a.has("file"))    att.file     = a["file"].string();
            if (a.has("size"))    att.size     = (size_t)a["size"].number();
            node.attachments.push_back(att);
        }
    }
    if (val.has("children")) {
        for (auto &c : val["children"].array())
            node.children.push_back(NodeFromJson(c));
    }
    return node;
}

void App::HandleBridgeMessage(const std::string &type, const std::string &payload) {
    (void)payload;
    if (type == "get_file_tree") {
        std::string json = storage_.GetFileTreeJson();
        bridge_.PostToJS("file_tree_result", "{\"tree\":" + json + "}");
    } else if (type == "load_file_data") {
        auto val = JsonValue::parse(payload);
        if (val.has("path")) {
            std::string path = val["path"].string();
            std::wstring wpath(path.begin(), path.end());
            std::wstring resolved = storage_.DataPath(wpath);
            wchar_t fullPath[32768] = {};
            if (GetFullPathNameW(resolved.c_str(), 32768, fullPath, nullptr)) {
                std::wstring expected = storage_.GetBasePath() + L"\\data\\";
                if (std::wstring(fullPath).substr(0, expected.size()) == expected) {
                    Node root = storage_.LoadTabData(wpath);
                    std::string nodeJson = Storage::SerializeNode(root);
                    std::string escPath = PipelineRunner::JsonEscape(path);
                    bridge_.PostToJS("file_data_result", "{\"path\":\"" + escPath + "\",\"root\":" + nodeJson + "}");
                }
            }
        }
    } else if (type == "save_node") {
        auto val = JsonValue::parse(payload);
        if (val.has("tabFile") && val.has("root")) {
            std::string tabFile = val["tabFile"].string();
            std::wstring wTabFile(tabFile.begin(), tabFile.end());
            // Validate path stays within data directory (prevent path traversal)
            std::wstring resolved = storage_.DataPath(wTabFile);
            wchar_t fullPath[32768] = {};
            if (!GetFullPathNameW(resolved.c_str(), 32768, fullPath, nullptr)) return;
            std::wstring expected = storage_.GetBasePath() + L"\\data\\";
            if (std::wstring(fullPath).substr(0, expected.size()) != expected) return;
            Node root = NodeFromJson(val["root"]);
            storage_.SaveTabData(wTabFile, root);
        }
    } else if (type == "set_language") {
        auto val = JsonValue::parse(payload);
        if (val.has("language")) {
            localization_.SetLanguage(val["language"].string());
        }
    } else if (type == "save_session") {
        auto val = JsonValue::parse(payload);
        if (val.has("tabs")) {
            SessionData session;
            for (auto &t : val["tabs"].array()) {
                TabData tab;
                if (t.has("name")) tab.name = t["name"].string();
                if (t.has("file")) tab.file = t["file"].string();
                session.tabs.push_back(tab);
            }
            storage_.SaveSession(session);
        }
    } else if (type == "run_pipeline") {
        auto val = JsonValue::parse(payload);
        if (val.has("pipelineName")) {
            std::string inputContent = val.has("content") ? val["content"].string() : "";
            inputNodeId_ = val.has("nodeId") ? val["nodeId"].string() : "";
            inputTabFile_ = val.has("tabFile") ? val["tabFile"].string() : "";
            auto pipelines = storage_.LoadPipelines();
            for (auto &p : pipelines) {
                if (p.name == val["pipelineName"].string()) {
                    // Pre-process steps: resolve wizard names to wizardData JSON
                    for (auto &step : p.steps) {
                        if (step.type == "wizard" && step.params.count("wizard") && !step.params.count("wizardData")) {
                            std::string wizName = step.params["wizard"];
                            std::string wizJson = storage_.LoadWizardData(wizName);
                            if (!wizJson.empty()) {
                                step.params["wizardData"] = wizJson;
                            }
                        }
                    }
                    runner_.Run(p.name, p.steps, inputContent, {}, p.outputMode);
                    break;
                }
            }
        }
    } else if (type == "run_prompt_process") {
        auto val = JsonValue::parse(payload);
        if (val.has("content")) {
            std::string content = val["content"].string();
            std::string prompt = val.has("userPrompt") ? val["userPrompt"].string() : "{content}";
            std::string provider = val.has("provider") ? val["provider"].string() : "openai";
            std::string model = val.has("model") ? val["model"].string() : "gpt-4.1";
            std::string systemPrompt = val.has("systemPrompt") ? val["systemPrompt"].string() : "";
            double temp = val.has("temperature") ? val["temperature"].number() : 0.7;

            inputNodeId_ = val.has("nodeId") ? val["nodeId"].string() : "";
            inputTabFile_ = val.has("tabFile") ? val["tabFile"].string() : "";

            PipelineStep step;
            step.name = "Process Prompt";
            step.type = "ai";
            step.params["provider"] = provider;
            step.params["model"] = model;
            step.params["systemPrompt"] = systemPrompt;
            step.params["userPrompt"] = prompt;
            step.params["temperature"] = std::to_string(temp);

            std::vector<PipelineStep> steps = { step };
            runner_.Run("Process Prompt", steps, content, {}, "child");
        }
    } else if (type == "cancel_pipeline") {
        runner_.Cancel();
    } else if (type == "wizard_step_resume") {
        auto val = JsonValue::parse(payload);
        std::string valuesJson = val.has("values") ? val["values"].serialize() : "{}";
        runner_.ResumeWizard(valuesJson);
    } else if (type == "manual_step_resume") {
        auto val = JsonValue::parse(payload);
        std::string content = val.has("content") ? val["content"].string() : "";
        runner_.ResumeManual(content);
    } else if (type == "manual_step_cancel") {
        runner_.CancelManual();
    } else if (type == "save_pipeline") {
        HandleSavePipeline(payload);
    } else if (type == "delete_pipeline") {
        HandleDeletePipeline(payload);
    } else if (type == "open_file") {
        OpenFileDialog();
    } else if (type == "save_file_as") {
        SaveFileDialog();
    } else if (type == "get_providers") {
        auto providers = storage_.LoadProviders();
        bridge_.PostToJS("log", "{\"message\":\"get_providers: " + std::to_string(providers.size()) + " providers loaded\"}");
        std::map<std::string, JsonValue> obj;
        for (auto &kv : providers) {
            std::map<std::string, JsonValue> cfg;
            cfg["apiKey"]  = JsonValue::fromString(kv.second.apiKey);
            cfg["baseUrl"] = JsonValue::fromString(kv.second.baseUrl);
            std::vector<JsonValue> models;
            for (auto &m : kv.second.models)
                models.push_back(JsonValue::fromString(m));
            cfg["models"] = JsonValue::fromArray(models);
            obj[kv.first] = JsonValue::fromObject(cfg);
        }
        std::string json = JsonValue::fromObject(obj).serialize(false);
        bridge_.PostToJS("log", "{\"message\":\"providers json: " + PipelineRunner::JsonEscape(json) + "\"}");
        bridge_.PostToJS("providers_result", json);
    } else if (type == "history_list") {
        HandleHistoryList();
    } else if (type == "history_detail") {
        HandleHistoryDetail(payload);
    } else if (type == "evaluate_node") {
        HandleEvaluateNode(payload);
    } else if (type == "evaluate_history_step") {
        HandleEvaluateHistoryStep(payload);
    } else if (type == "evaluate_history_run") {
        HandleEvaluateHistoryRun(payload);
    } else if (type == "optimize_pipeline") {
        HandleOptimizePipeline(payload);
    } else if (type == "optimize_apply") {
        HandleOptimizeApply(payload);
    } else if (type == "optimize_undo") {
        HandleOptimizeUndo(payload);
    } else if (type == "optimize_redo") {
        HandleOptimizeRedo(payload);
    } else if (type == "optimize_checkout") {
        HandleOptimizeCheckout(payload);
    } else if (type == "optimize_reapply") {
        HandleOptimizeReapply(payload);
    } else if (type == "optimize_version_list") {
        HandleOptimizeVersionList(payload);
    } else if (type == "save_providers") {
        bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::HandleBridgeMessage: save_providers called, payload=" + PipelineRunner::JsonEscape(payload) + "\"}");
        auto val = JsonValue::parse(payload);
        if (val.type() != JsonValue::Object) {
            bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::HandleBridgeMessage: ERROR: save_providers payload is not an object\"}");
            return;
        }
        std::map<std::string, ProviderConfig> providers;
        int savedCount = 0;
        for (auto &kv : val.object()) {
            ProviderConfig cfg;
            if (kv.second.has("apiKey"))  cfg.apiKey  = kv.second["apiKey"].string();
            if (kv.second.has("baseUrl")) cfg.baseUrl = kv.second["baseUrl"].string();
            if (kv.second.has("models")) {
                for (auto &m : kv.second["models"].array())
                    cfg.models.push_back(m.string());
            }
            providers[kv.first] = cfg;
            runner_.RegisterProvider(kv.first, cfg.apiKey, cfg.baseUrl);
            bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::HandleBridgeMessage: Registering " + kv.first + " with key='" + cfg.apiKey + "' url='" + cfg.baseUrl + "'\"}");
            if (!cfg.apiKey.empty()) savedCount++;
        }
        // Resolve save path for logging
        std::wstring savePath = appDataPath_ + L"\\providers.json";
        std::string pathA;
        {
            int pathLen = WideCharToMultiByte(CP_UTF8, 0, savePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            pathA.resize(pathLen);
            WideCharToMultiByte(CP_UTF8, 0, savePath.c_str(), -1, &pathA[0], pathLen, nullptr, nullptr);
            if (!pathA.empty() && pathA.back() == '\0') pathA.pop_back();
        }
        if (providers.empty()) {
            bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::HandleBridgeMessage: 📂 No providers to save (not written)\"}");
        } else {
            bool saved = storage_.SaveProviders(providers);
            if (saved) {
                bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::HandleBridgeMessage: ✅ Saved " + std::to_string(savedCount) + " provider(s) → " + PipelineRunner::JsonEscape(pathA) + "\"}");
                HWND hwndEcode = FindWindowW(L"EcodeWindowClass", nullptr);
                if (hwndEcode) PostMessageW(hwndEcode, WM_APP + 3, 0, 0);
            } else {
                bridge_.PostToJS("log", "{\"message\":\"[TRACE] App::HandleBridgeMessage: ❌ FAILED to write " + PipelineRunner::JsonEscape(pathA) + "\"}");
            }
        }
    } else if (type == "test_provider_connection") {
        auto val = JsonValue::parse(payload);
        std::string provider = val.has("provider") ? val["provider"].string() : "";
        std::string apiKey   = val.has("apiKey")   ? val["apiKey"].string()   : "";
        std::string baseUrl  = val.has("baseUrl")  ? val["baseUrl"].string()  : "";
        bridge_.PostToJS("log", "{\"message\":\"Testing " + provider + "...\"}");
        // Run on background thread to avoid blocking UI
        std::thread([this, provider, apiKey, baseUrl]() {
            std::string result;
            std::string providerName = provider;
            if (!providerName.empty()) {
                auto *p = AIProvider::Create(providerName, apiKey, baseUrl);
                if (p) {
                    result = p->TestConnection();
                    delete p;
                } else {
                    result = "Unknown provider: " + providerName;
                }
            } else {
                result = "No provider specified";
            }
            bridge_.PostToJS("log", "{\"message\":\"Test result for " + providerName + ": " +
                (result.empty() ? "OK" : "FAILED") + "\"}");
            std::string resJson = "{\"provider\":\"" + PipelineRunner::JsonEscape(providerName) + "\"";
            if (result.empty()) {
                resJson += ",\"success\":true,\"message\":\"Connection OK\"";
            } else {
                resJson += ",\"success\":false,\"message\":\"" + PipelineRunner::JsonEscape(result) + "\"";
            }
            resJson += "}";
            bridge_.PostToJS("test_connection_result", resJson);
        }).detach();
    } else if (type == "step_filter_resume") {
        runner_.ResumeFilter(payload);
    } else if (type == "fetch_models") {
        auto val = JsonValue::parse(payload);
        std::string provider = val.has("provider") ? val["provider"].string() : "";
        if (!provider.empty()) {
            // Look up provider config from saved providers
            auto providers = storage_.LoadProviders();
            std::string apiKey, baseUrl;
            if (providers.count(provider)) {
                apiKey = providers[provider].apiKey;
                baseUrl = providers[provider].baseUrl;
            }
            auto *p = AIProvider::Create(provider, apiKey, baseUrl);
            std::string modelsJson = "[]";
            if (p) {
                auto models = p->ListModels();
                std::string arr = "[";
                for (size_t i = 0; i < models.size(); i++) {
                    if (i > 0) arr += ",";
                    arr += "\"" + PipelineRunner::JsonEscape(models[i]) + "\"";
                }
                arr += "]";
                modelsJson = arr;
                delete p;
            }
            bridge_.PostToJS("model_list", "{\"provider\":\"" + PipelineRunner::JsonEscape(provider) + "\",\"models\":" + modelsJson + "}");
        }
    } else if (type == "send_to_chest") {
        auto val = JsonValue::parse(payload);
        if (val.has("chestName") && val.has("content")) {
            storage_.SaveToNamedChest(val["chestName"].string(), val["content"].string());
            bridge_.PostToJS("log", "{\"message\":\"📦 Saved to chest: " + val["chestName"].string() + "\"}");
        }
    } else if (type == "select_input_source") {
        auto val = JsonValue::parse(payload);
        if (val.has("source")) {
            std::string source = val["source"].string();
            if (source == "chest" && val.has("chestName")) {
                std::string chestContent = storage_.LoadFromNamedChest(val["chestName"].string());
                runner_.SetExternalInput(chestContent);
            } else if (source == "manual" && val.has("content")) {
                runner_.SetExternalInput(val["content"].string());
            } else if (source == "checkpoint") {
                std::string cp = storage_.LoadCheckpointOutput(runner_.GetRunId(), 0);
                if (!cp.empty()) runner_.SetExternalInput(cp);
            } else {
                runner_.SetExternalInput("");
            }
        }
    } else if (type == "select_project") {
        auto val = JsonValue::parse(payload);
        if (val.has("projectName")) {
            std::string projectName = val["projectName"].string();
            std::wstring wName(projectName.begin(), projectName.end());
            std::wstring newPath = appDataPath_ + L"\\projects\\" + wName;
            if (storage_.Init(newPath)) {
                auto pipelines = storage_.LoadPipelines();
                std::string pipelinesJson = storage_.SerializePipelines(pipelines);
                bridge_.PostToJS("project_changed",
                    "{\"projectName\":\"" + projectName + "\"" +
                    ",\"pipelines\":" + pipelinesJson + "}");
            }
        }
    } else if (type == "create_project") {
        auto val = JsonValue::parse(payload);
        if (val.has("projectName")) {
            std::string projectName = val["projectName"].string();
            std::wstring wName(projectName.begin(), projectName.end());
            std::wstring projPath = appDataPath_ + L"\\projects\\" + wName;
            CreateDirectoryW((projPath + L"\\data").c_str(), nullptr);
            CreateDirectoryW((projPath + L"\\blobs").c_str(), nullptr);
            CreateDirectoryW((projPath + L"\\history").c_str(), nullptr);
            if (storage_.Init(projPath)) {
                bridge_.PostToJS("log", "{\"message\":\"📁 Created project: " + projectName + "\"}");
                bridge_.PostToJS("project_changed",
                    "{\"projectName\":\"" + projectName + "\",\"tabs\":[],\"pipelines\":[]}");
            }
        }
    } else if (type == "save_run_state") {
        auto val = JsonValue::parse(payload);
        if (val.has("currentStep")) {
            std::string runId = runner_.GetRunId();
            if (!runId.empty()) {
                storage_.SaveRunState(runId, payload);
            }
        }
    } else if (type == "resume_run") {
        auto val = JsonValue::parse(payload);
        if (val.has("runId") && val.has("action")) {
            std::string action = val["action"].string();
            std::string runId = val["runId"].string();
            if (action == "continue") {
                bridge_.PostToJS("log", "{\"message\":\"▶ Resuming run: " + runId + "\"}");
            } else if (action == "keep") {
                storage_.CloseRun(runId);
            } else if (action == "discard") {
                storage_.DiscardRun(runId);
            }
        }
    } else if (type == "set_history_retention") {
        auto val = JsonValue::parse(payload);
        if (val.has("maxRuns")) {
            storage_.SetMaxHistoryRuns((int)val["maxRuns"].number());
            Storage::GeneralConfig cfg;
            cfg.historyRetention = storage_.GetMaxHistoryRuns();
            storage_.SaveGeneralConfig(cfg);
            HWND hwndEcode = FindWindowW(L"EcodeWindowClass", nullptr);
            if (hwndEcode) PostMessageW(hwndEcode, WM_APP + 3, 0, 0);
        }
    } else if (type == "save_config") {
        auto val = JsonValue::parse(payload);
        Storage::GeneralConfig cfg;
        if (val.has("historyRetention"))
            cfg.historyRetention = (int)val["historyRetention"].number();
        if (val.has("defaultProvider"))
            cfg.defaultProvider = val["defaultProvider"].string();
        if (val.has("defaultModel"))
            cfg.defaultModel = val["defaultModel"].string();
        storage_.SaveGeneralConfig(cfg);
        HWND hwndEcode = FindWindowW(L"EcodeWindowClass", nullptr);
        if (hwndEcode) PostMessageW(hwndEcode, WM_APP + 3, 0, 0);
    } else if (type == "save_recipes") {
        bridge_.PostToJS("log", "{\"message\":\"[TRACE] save_recipes called, payload length=" + std::to_string(payload.size()) + "\"}");
        auto val = JsonValue::parse(payload);
        std::vector<Storage::Recipe> recipes;
        if (val.type() == JsonValue::Array) {
            for (auto &item : val.array()) {
                Storage::Recipe r;
                if (item.has("type")) r.type = item["type"].string();
                if (item.has("name")) r.name = item["name"].string();
                if (item.has("provider")) r.provider = item["provider"].string();
                if (item.has("model")) r.model = item["model"].string();
                if (item.has("temperature")) r.temperature = item["temperature"].number();
                if (item.has("systemPrompt")) r.systemPrompt = item["systemPrompt"].string();
                if (item.has("command")) r.command = item["command"].string();
                if (!r.name.empty()) recipes.push_back(r);
            }
        }
        storage_.SaveRecipes(recipes);
        HWND hwndEcode = FindWindowW(L"EcodeWindowClass", nullptr);
        if (hwndEcode) PostMessageW(hwndEcode, WM_APP + 3, 0, 0);
    }
}

void App::HandleSavePipeline(const std::string &payload) {
    auto val = JsonValue::parse(payload);
    auto pipelines = storage_.LoadPipelines();
    
    std::string name = val.has("name") ? val["name"].string() : "";
    if (name.empty()) return;
    
    // Find and update, or add new
    bool found = false;
    for (auto &p : pipelines) {
        if (p.name == name) {
            found = true;
            if (val.has("mode"))       p.mode       = val["mode"].string();
            if (val.has("outputMode")) p.outputMode = val["outputMode"].string();
            if (val.has("outputNaming")) p.outputNaming = val["outputNaming"].string();
            if (val.has("multiMedia")) p.multiMedia = val["multiMedia"].string();
            if (val.has("retryCount")) p.retryCount = (int)val["retryCount"].number();
            if (val.has("retryDelayMs")) p.retryDelayMs = (int)val["retryDelayMs"].number();
            if (val.has("steps")) {
                p.steps.clear();
                for (auto &s : val["steps"].array()) {
                    PipelineStep step;
                    if (s.has("name")) step.name = s["name"].string();
                    if (s.has("type")) step.type = s["type"].string();
                    for (auto &kv : s.object()) {
                        auto &v = kv.second;
                        if (v.type() == JsonValue::String)
                            step.params[kv.first] = v.string();
                        else
                            step.params[kv.first] = v.serialize();
                    }
                    p.steps.push_back(step);
                }
            }
            break;
        }
    }
    if (!found) {
        Pipeline p;
        p.name = name;
        p.mode = val.has("mode") ? val["mode"].string() : "basic";
        p.outputMode = val.has("outputMode") ? val["outputMode"].string() : "child";
        p.outputNaming = val.has("outputNaming") ? val["outputNaming"].string() : "{pipeline_name}_{timestamp}";
        if (val.has("retryCount")) p.retryCount = (int)val["retryCount"].number();
        if (val.has("retryDelayMs")) p.retryDelayMs = (int)val["retryDelayMs"].number();
        if (val.has("multiMedia")) p.multiMedia = val["multiMedia"].string();
        if (val.has("steps")) {
            for (auto &s : val["steps"].array()) {
                PipelineStep step;
                if (s.has("name")) step.name = s["name"].string();
                if (s.has("type")) step.type = s["type"].string();
                for (auto &kv : s.object()) {
                    auto &v = kv.second;
                    if (v.type() == JsonValue::String) step.params[kv.first] = v.string();
                    else step.params[kv.first] = v.serialize();
                }
                p.steps.push_back(step);
            }
        }
        pipelines.push_back(p);
    }
    storage_.SavePipelines(pipelines);
    HWND hwndEcode = FindWindowW(L"EcodeWindowClass", nullptr);
    if (hwndEcode) PostMessageW(hwndEcode, WM_APP + 3, 0, 0);
    bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
}

void App::HandleDeletePipeline(const std::string &payload) {
    auto val = JsonValue::parse(payload);
    std::string name = val.has("name") ? val["name"].string() : "";
    if (name.empty()) return;
    auto pipelines = storage_.LoadPipelines();
    auto it = std::remove_if(pipelines.begin(), pipelines.end(),
        [&](const Pipeline &p) { return p.name == name; });
    if (it != pipelines.end()) {
        pipelines.erase(it, pipelines.end());
        storage_.SavePipelines(pipelines);
        HWND hwndEcode = FindWindowW(L"EcodeWindowClass", nullptr);
        if (hwndEcode) PostMessageW(hwndEcode, WM_APP + 3, 0, 0);
        bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
    }
}

void App::HandleHistoryList() {
    auto files = storage_.ListHistory();
    std::string json = "[";
    bool first = true;
    int limit = 100;
    for (auto &file : files) {
        if (limit-- <= 0) break;
        auto record = storage_.LoadHistoryRecord(file);
        if (record.empty()) continue;
        auto val = JsonValue::parse(record);
        if (!val.has("pipelineName")) continue;
        if (!first) json += ",";
        first = false;
        std::string id         = val.has("id")           ? val["id"].string()           : "";
        std::string name       = val.has("pipelineName")  ? val["pipelineName"].string()  : "";
        std::string at         = val.has("startedAt")     ? val["startedAt"].string()     :
                                  (val.has("executedAt")  ? val["executedAt"].string()    : "");
        std::string status     = val.has("status")        ? val["status"].string()        : "completed";
        std::string evaluation = val.has("evaluation")    ? val["evaluation"].string()    : "";
        int stepCount = val.has("steps") ? (int)val["steps"].array().size() : 0;
        json += "{\"id\":\"" + PipelineRunner::JsonEscape(id) + "\""
              + ",\"pipelineName\":\"" + PipelineRunner::JsonEscape(name) + "\""
              + ",\"startedAt\":\"" + PipelineRunner::JsonEscape(at) + "\""
              + ",\"status\":\"" + PipelineRunner::JsonEscape(status) + "\""
              + ",\"evaluation\":\"" + PipelineRunner::JsonEscape(evaluation) + "\""
              + ",\"stepCount\":" + std::to_string(stepCount) + "}";
    }
    json += "]";
    bridge_.PostToJS("history_list_result", "{\"items\":" + json + "}");
}

void App::HandleHistoryDetail(const std::string &payload) {
    auto val = JsonValue::parse(payload);
    std::string id = val.has("id") ? val["id"].string() : "";
    if (id.empty()) { bridge_.PostToJS("history_detail_result", "{}"); return; }
    std::wstring wid(id.begin(), id.end());
    auto record = storage_.LoadHistoryRecord(L"run_" + wid + L".json");
    if (record.empty()) { bridge_.PostToJS("history_detail_result", "{}"); return; }
    bridge_.PostToJS("history_detail_result", record);
}

// --- Evaluation handlers ---

void App::HandleEvaluateNode(const std::string &payload) {
    auto val = JsonValue::parse(payload);
    std::string nodeId     = val.has("nodeId")    ? val["nodeId"].string()    : "";
    std::string tabFile    = val.has("tabFile")   ? val["tabFile"].string()   : "";
    std::string evaluation = val.has("evaluation")? val["evaluation"].string(): "";
    std::string note       = val.has("note")      ? val["note"].string()      : "";
    if (nodeId.empty() || tabFile.empty()) return;

    // Load tab, find node by id (nodes don't have explicit id; use title+position heuristic).
    // For now, tab root and children are identified by nodeId as stringified index path "0.1.2"
    // (frontend sends path as nodeId). We update the node at that path.
    std::wstring wtab(tabFile.begin(), tabFile.end());
    Node root = storage_.LoadTabData(storage_.DataPath(wtab));

    // Walk path
    std::function<Node*(Node*, const std::string&)> findNode =
        [&](Node *n, const std::string &path) -> Node* {
            if (path.empty()) return n;
            size_t dot = path.find('.');
            int idx = std::stoi(path.substr(0, dot == std::string::npos ? path.size() : dot));
            if (idx < 0 || idx >= (int)n->children.size()) return nullptr;
            std::string rest = dot == std::string::npos ? "" : path.substr(dot + 1);
            return findNode(&n->children[idx], rest);
        };

    Node *target = findNode(&root, nodeId);
    if (!target) {
        bridge_.PostToJS("evaluation_saved", "{\"error\":\"node not found\"}");
        return;
    }

    // Get current ISO timestamp
    time_t t = time(nullptr);
    struct tm tm_info;
    gmtime_s(&tm_info, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_info);

    target->evaluation     = evaluation;
    target->evaluatedAt    = buf;
    target->evaluationNote = note;
    storage_.SaveTabData(storage_.DataPath(wtab), root);

    std::map<std::string, JsonValue> resp;
    resp["targetType"]  = JsonValue::fromString("node");
    resp["id"]          = JsonValue::fromString(nodeId);
    resp["evaluation"]  = JsonValue::fromString(evaluation);
    bridge_.PostToJS("evaluation_saved", JsonValue::fromObject(resp).serialize());
}

void App::HandleEvaluateHistoryStep(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string runId      = val.has("runId")      ? val["runId"].string()      : "";
    int stepIdx            = val.has("stepIndex")  ? (int)val["stepIndex"].number() : -1;
    std::string evaluation = val.has("evaluation") ? val["evaluation"].string() : "";
    std::string note       = val.has("note")       ? val["note"].string()       : "";
    if (runId.empty() || stepIdx < 0) return;

    std::wstring wid(runId.begin(), runId.end());
    std::wstring filename = L"run_" + wid + L".json";
    std::string json = storage_.LoadHistoryRecord(filename);
    if (json.empty()) return;

    auto rec = JsonValue::parse(json);
    // Rebuild object with modified step
    std::map<std::string, JsonValue> obj;
    for (auto &kv : rec.object()) obj[kv.first] = kv.second;
    if (rec.has("steps")) {
        std::vector<JsonValue> steps;
        int i = 0;
        for (auto &s : rec["steps"].array()) {
            if (i == stepIdx) {
                std::map<std::string, JsonValue> sobj;
                for (auto &kv : s.object()) sobj[kv.first] = kv.second;
                sobj["evaluation"]     = JsonValue::fromString(evaluation);
                sobj["evaluationNote"] = JsonValue::fromString(note);
                steps.push_back(JsonValue::fromObject(sobj));
            } else {
                steps.push_back(s);
            }
            i++;
        }
        obj["steps"] = JsonValue::fromArray(steps);
    }
    storage_.SaveHistory(JsonValue::fromObject(obj).serialize(true));

    std::map<std::string, JsonValue> resp;
    resp["targetType"] = JsonValue::fromString("step");
    resp["id"]         = JsonValue::fromString(runId + "." + std::to_string(stepIdx));
    resp["evaluation"] = JsonValue::fromString(evaluation);
    bridge_.PostToJS("evaluation_saved", JsonValue::fromObject(resp).serialize());
}

void App::HandleEvaluateHistoryRun(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string runId      = val.has("runId")      ? val["runId"].string()      : "";
    std::string evaluation = val.has("evaluation") ? val["evaluation"].string() : "";
    if (runId.empty()) return;

    std::wstring wid(runId.begin(), runId.end());
    storage_.UpdateHistoryEvaluation(L"run_" + wid + L".json", evaluation);

    std::map<std::string, JsonValue> resp;
    resp["targetType"] = JsonValue::fromString("run");
    resp["id"]         = JsonValue::fromString(runId);
    resp["evaluation"] = JsonValue::fromString(evaluation);
    bridge_.PostToJS("evaluation_saved", JsonValue::fromObject(resp).serialize());
}

// --- Optimizer handlers ---

static std::string VersionCursorToJson(const VersionCursor &cursor) {
    std::map<std::string, JsonValue> obj;
    obj["pipelineName"]  = JsonValue::fromString(cursor.pipelineName);
    obj["currentVersion"]= JsonValue::fromDouble(cursor.currentVersion);
    obj["headVersion"]   = JsonValue::fromDouble(cursor.headVersion);
    std::vector<JsonValue> entries;
    for (auto &e : cursor.entries) {
        std::map<std::string, JsonValue> ej;
        ej["version"]   = JsonValue::fromDouble(e.version);
        ej["timestamp"] = JsonValue::fromString(e.timestamp);
        ej["label"]     = JsonValue::fromString(e.label);
        ej["sessionId"] = JsonValue::fromString(e.sessionId);
        entries.push_back(JsonValue::fromObject(ej));
    }
    obj["entries"] = JsonValue::fromArray(entries);
    return JsonValue::fromObject(obj).serialize();
}

static void PostVersionChanged(Bridge &bridge, const VersionCursor &cursor) {
    std::map<std::string, JsonValue> obj;
    obj["pipelineName"]  = JsonValue::fromString(cursor.pipelineName);
    obj["version"]       = JsonValue::fromDouble(cursor.currentVersion);
    obj["canUndo"]       = JsonValue::fromBool(cursor.currentVersion > 1);
    obj["canRedo"]       = JsonValue::fromBool(cursor.currentVersion < cursor.headVersion);
    bridge.PostToJS("optimize_version_changed", JsonValue::fromObject(obj).serialize());
}

void App::HandleOptimizePipeline(const std::string &payload) {
    auto val        = JsonValue::parse(payload);
    std::string name = val.has("pipelineName") ? val["pipelineName"].string() : "";
    int historyLimit = val.has("historyLimit")  ? (int)val["historyLimit"].number()  : 10;
    int maxEdits     = val.has("maxEditsPerStep")? (int)val["maxEditsPerStep"].number(): 3;
    std::string prov  = val.has("provider")     ? val["provider"].string() : "";
    std::string model = val.has("model")        ? val["model"].string()    : "";
    if (name.empty() || prov.empty() || model.empty()) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Missing pipelineName, provider, or model\"}");
        return;
    }

    auto pipelines = storage_.LoadPipelines();
    Pipeline pipeline;
    bool found = false;
    for (auto &p : pipelines) { if (p.name == name) { pipeline = p; found = true; break; } }
    if (!found) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Pipeline not found: " +
                         PipelineRunner::JsonEscape(name) + "\"}");
        return;
    }

    // Ensure base version exists
    versionMgr_.EnsureBaseVersion(name, pipeline);

    // Look up API credentials
    auto providers = storage_.LoadProviders();
    std::string apiKey, baseUrl;
    if (providers.count(prov)) {
        apiKey  = providers[prov].apiKey;
        baseUrl = providers[prov].baseUrl;
    }

    // Active session for later apply
    activeOptSession_ = std::make_unique<OptSession>();
    activeOptSession_->pipelineName = name;
    activeOptSession_->rejectedBuffer = optimizer_.LoadRejectedBuffer(name);

    // Bridge callback (thread-safe: PostToJS queues a UI-thread message)
    auto &bridge = bridge_;
    auto &opt    = optimizer_;
    auto &sess   = activeOptSession_;
    optimizer_.StartSession(name, pipeline, historyLimit, maxEdits,
                            prov, apiKey, baseUrl, model,
                            [&bridge, &opt, &sess](const std::string &type, const std::string &json) {
                                if (type == "optimize_proposals") {
                                    // Store proposals in active session
                                    auto v = JsonValue::parse(json);
                                    if (v.has("sessionId") && sess)
                                        sess->sessionId = v["sessionId"].string();
                                    if (v.has("proposals") && sess) {
                                        for (auto &p : v["proposals"].array()) {
                                            OptEditProposal prop;
                                            if (p.has("op"))        prop.op        = p["op"].string();
                                            if (p.has("stepName"))  prop.stepName  = p["stepName"].string();
                                            if (p.has("field"))     prop.field     = p["field"].string();
                                            if (p.has("oldValue"))  prop.oldValue  = p["oldValue"].string();
                                            if (p.has("newValue"))  prop.newValue  = p["newValue"].string();
                                            if (p.has("rationale")) prop.rationale = p["rationale"].string();
                                            sess->proposals.push_back(prop);
                                        }
                                    }
                                }
                                bridge.PostToJS(type, json);
                            });
}

void App::HandleOptimizeApply(const std::string &payload) {
    auto val = JsonValue::parse(payload);
    std::string name      = val.has("pipelineName") ? val["pipelineName"].string() : "";
    std::string sessionId = val.has("sessionId")    ? val["sessionId"].string()    : "";
    if (name.empty() || !activeOptSession_ || activeOptSession_->pipelineName != name) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"No active optimization session\"}");
        return;
    }

    std::vector<int> approved, rejected;
    if (val.has("approved"))
        for (auto &v : val["approved"].array()) approved.push_back((int)v.number());
    if (val.has("rejected"))
        for (auto &v : val["rejected"].array()) rejected.push_back((int)v.number());

    auto pipelines = storage_.LoadPipelines();
    Pipeline pipeline;
    bool found = false;
    int pipelineIdx = -1;
    for (int i = 0; i < (int)pipelines.size(); i++) {
        if (pipelines[i].name == name) { pipeline = pipelines[i]; found = true; pipelineIdx = i; break; }
    }
    if (!found) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Pipeline not found\"}");
        return;
    }

    Pipeline updated = PipelineOptimizer::ApplyApprovals(pipeline, approved, rejected, *activeOptSession_);

    // Save rejected buffer
    optimizer_.SaveRejectedBuffer(name, activeOptSession_->rejectedBuffer);

    // Build label
    std::string label = "Optimize (" + std::to_string(approved.size()) + " edits)";
    std::vector<OptEditProposal> approvedProposals;
    for (int idx : approved)
        if (idx >= 0 && idx < (int)activeOptSession_->proposals.size())
            approvedProposals.push_back(activeOptSession_->proposals[idx]);

    int newVersion = versionMgr_.CommitVersion(name, updated, sessionId, label, approvedProposals);

    // Update pipeline in storage
    if (pipelineIdx >= 0) pipelines[pipelineIdx] = updated;
    else pipelines.push_back(updated);
    storage_.SavePipelines(pipelines);

    // Clear active session
    activeOptSession_.reset();

    // Notify frontend
    auto cursor = versionMgr_.GetCursor(name);
    std::map<std::string, JsonValue> resp;
    resp["pipelineName"]  = JsonValue::fromString(name);
    resp["approvedCount"] = JsonValue::fromDouble((int)approved.size());
    resp["rejectedCount"] = JsonValue::fromDouble((int)rejected.size());
    resp["version"]       = JsonValue::fromDouble(newVersion);
    bridge_.PostToJS("optimize_applied", JsonValue::fromObject(resp).serialize());
    bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
    PostVersionChanged(bridge_, cursor);
}

void App::HandleOptimizeUndo(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string name = val.has("pipelineName") ? val["pipelineName"].string() : "";
    if (name.empty()) return;

    auto restored = versionMgr_.Undo(name);
    if (!restored) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Already at earliest version\"}");
        return;
    }

    auto pipelines = storage_.LoadPipelines();
    for (auto &p : pipelines) { if (p.name == name) { p = *restored; break; } }
    storage_.SavePipelines(pipelines);

    auto cursor = versionMgr_.GetCursor(name);
    bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
    PostVersionChanged(bridge_, cursor);
}

void App::HandleOptimizeRedo(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string name = val.has("pipelineName") ? val["pipelineName"].string() : "";
    if (name.empty()) return;

    auto restored = versionMgr_.Redo(name);
    if (!restored) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Already at latest version\"}");
        return;
    }

    auto pipelines = storage_.LoadPipelines();
    for (auto &p : pipelines) { if (p.name == name) { p = *restored; break; } }
    storage_.SavePipelines(pipelines);

    auto cursor = versionMgr_.GetCursor(name);
    bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
    PostVersionChanged(bridge_, cursor);
}

void App::HandleOptimizeCheckout(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string name = val.has("pipelineName") ? val["pipelineName"].string() : "";
    int version      = val.has("version")       ? (int)val["version"].number()  : -1;
    if (name.empty() || version < 1) return;

    auto restored = versionMgr_.CheckoutVersion(name, version);
    if (!restored) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Version not found\"}");
        return;
    }

    auto pipelines = storage_.LoadPipelines();
    for (auto &p : pipelines) { if (p.name == name) { p = *restored; break; } }
    storage_.SavePipelines(pipelines);

    auto cursor = versionMgr_.GetCursor(name);
    bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
    PostVersionChanged(bridge_, cursor);
}

void App::HandleOptimizeReapply(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string name = val.has("pipelineName") ? val["pipelineName"].string() : "";
    int version      = val.has("version")       ? (int)val["version"].number()  : -1;
    if (name.empty() || version < 1) return;

    auto pipelines = storage_.LoadPipelines();
    Pipeline current;
    bool found = false;
    int idx = -1;
    for (int i = 0; i < (int)pipelines.size(); i++) {
        if (pipelines[i].name == name) { current = pipelines[i]; found = true; idx = i; break; }
    }
    if (!found) {
        bridge_.PostToJS("optimize_error", "{\"message\":\"Pipeline not found\"}");
        return;
    }

    Pipeline updated = versionMgr_.ReapplyVersion(name, version, current);
    if (idx >= 0) pipelines[idx] = updated;
    storage_.SavePipelines(pipelines);

    auto cursor = versionMgr_.GetCursor(name);
    std::map<std::string, JsonValue> resp;
    resp["pipelineName"]  = JsonValue::fromString(name);
    resp["approvedCount"] = JsonValue::fromDouble(0);
    resp["rejectedCount"] = JsonValue::fromDouble(0);
    resp["version"]       = JsonValue::fromDouble(cursor.currentVersion);
    bridge_.PostToJS("optimize_applied", JsonValue::fromObject(resp).serialize());
    bridge_.PostToJS("pipeline_list", "{\"pipelines\":" + Storage::SerializePipelines(pipelines) + "}");
    PostVersionChanged(bridge_, cursor);
}

void App::HandleOptimizeVersionList(const std::string &payload) {
    auto val    = JsonValue::parse(payload);
    std::string name = val.has("pipelineName") ? val["pipelineName"].string() : "";
    if (name.empty()) return;
    auto cursor = versionMgr_.GetCursor(name);
    std::map<std::string, JsonValue> resp;
    resp["pipelineName"] = JsonValue::fromString(name);
    resp["cursor"]       = JsonValue::parse(VersionCursorToJson(cursor));
    bridge_.PostToJS("optimize_version_list_result", JsonValue::fromObject(resp).serialize());
}

int App::Run() {
    if (!hwnd_) return 1;
    ShowWindow(hwnd_, embedded_ ? SW_HIDE : SW_SHOW);
    UpdateWindow(hwnd_);
    
    // Initialize WebView2 after window is visible (non-blocking)
    InitWebView2();
    
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
 
 
