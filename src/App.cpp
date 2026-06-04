#include "App.h"
#include "resource.h"
#include "JsonParser.h"
#include "Base64.h"
#include <shlobj.h>
#include <commctrl.h>
#include <richedit.h>
#include <ole2.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <WebView2.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

using namespace Microsoft::WRL;

#define WM_APP_WEBVIEW2_READY (WM_APP + 1)

App::App() {
    runner_.SetBridgeCallback([this](const std::string &type, const std::string &json) {
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
    case WM_APP_WEBVIEW2_READY:
        if (wParam == 0) ShowFallbackUI();
        return 0;
    default:
        return DefWindowProc(hwnd_, msg, wParam, lParam);
    }
}

void App::OnCreate() {
    // InitWebView2 is called from App::Run() after ShowWindow
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
                                args->TryGetWebMessageAsString(&raw);
                                if (raw) {
                                    int len = WideCharToMultiByte(CP_UTF8, 0, raw, -1, nullptr, 0, nullptr, nullptr);
                                    std::string s(len, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, raw, -1, &s[0], len, nullptr, nullptr);
                                    CoTaskMemFree(raw);

                                    auto val = JsonValue::parse(s);
                                    std::string type = val["type"].string();
                                    if (type == "init_complete") {
                                        app->bridge_.SendInit(val["language"].string());
                                    } else {
                                        std::string payload = val["payload"].string();
                                        app->HandleBridgeMessage(type, payload.empty() ? "" : payload);
                                    }
                                }
                                return S_OK;
                            }
                        };

                        app->webview_->add_WebMessageReceived(Make<MsgHandler>(app).Get(), nullptr);

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
    if (type == "save_node") {
    } else if (type == "run_pipeline") {
        auto val = JsonValue::parse(payload);
        if (val.has("pipelineName")) {
            std::string inputContent = val.has("content") ? val["content"].string() : "";
            inputNodeId_ = val.has("nodeId") ? val["nodeId"].string() : "";
            inputTabFile_ = val.has("tabFile") ? val["tabFile"].string() : "";
            auto pipelines = storage_.LoadPipelines();
            for (auto &p : pipelines) {
                if (p.name == val["pipelineName"].string()) {
                    runner_.Run(p.name, p.steps, inputContent, {}, p.outputMode);
                    break;
                }
            }
        }
    } else if (type == "save_node") {
        auto val = JsonValue::parse(payload);
        if (val.has("tabFile") && val.has("root")) {
            std::string tabFile = val["tabFile"].string();
            std::wstring wTabFile(tabFile.begin(), tabFile.end());
            Node root = NodeFromJson(val["root"]);
            storage_.SaveTabData(storage_.DataPath(wTabFile), root);
        }
    } else if (type == "cancel_pipeline") {
        runner_.Cancel();
    } else if (type == "manual_step_resume") {
        auto val = JsonValue::parse(payload);
        std::string content = val.has("content") ? val["content"].string() : "";
        runner_.ResumeManual(content);
    } else if (type == "manual_step_cancel") {
        runner_.CancelManual();
    } else if (type == "get_providers") {
        auto providers = storage_.LoadProviders();
        std::string json = "{";
        bool first = true;
        for (auto &kv : providers) {
            if (!first) json += ",";
            first = false;
            json += "\"" + kv.first + "\":{\"apiKey\":\"" + kv.second.apiKey + "\",\"baseUrl\":\"" + kv.second.baseUrl + "\"}";
        }
        json += "}";
        bridge_.PostToJS("providers_result", json);
    } else if (type == "save_providers") {
        auto val = JsonValue::parse(payload);
        std::map<std::string, ProviderConfig> providers;
        for (auto &kv : val.object()) {
            ProviderConfig cfg;
            if (kv.second.has("apiKey"))  cfg.apiKey  = kv.second["apiKey"].string();
            if (kv.second.has("baseUrl")) cfg.baseUrl = kv.second["baseUrl"].string();
            providers[kv.first] = cfg;
            runner_.RegisterProvider(kv.first, cfg.apiKey, cfg.baseUrl);
        }
        storage_.SaveProviders(providers);
    }
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
 
 
