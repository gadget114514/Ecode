#pragma once
#include <windows.h>
#include <unknwn.h>
#include <string>
#include <functional>

// Opaque pointer to avoid WebView2 header dependency
struct IWebView2Interface;

class Bridge {
public:
    Bridge();
    ~Bridge();
    
    // Store raw IUnknown pointer (cast from ICoreWebView2* at runtime)
    void SetWebView(IUnknown *webview);
    void SetHandler(std::function<void(const std::string &type, const std::string &payload)> handler);
    void PostToJS(const std::string &type, const std::string &payloadJson);
    void PostToJS(const std::string &typeAndPayload);
    void SendInit(const std::string &language);
    
    IUnknown *GetWebView() const { return webview_; }
    
private:
    IUnknown *webview_{nullptr};
    std::function<void(const std::string&, const std::string&)> handler_;
    DWORD webviewCookie_ = 0;
};
