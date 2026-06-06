#include "Bridge.h"
#include <unknwn.h>
#include <string>
#include <windows.h>
#include <WebView2.h>

Bridge::Bridge() {}
Bridge::~Bridge() {
    if (webview_) webview_->Release();
}

void Bridge::SetWebView(IUnknown *webview) {
    if (webview_) webview_->Release();
    webview_ = webview;
    if (webview_) webview_->AddRef();
}

void Bridge::SetHandler(std::function<void(const std::string&, const std::string&)> handler) {
    handler_ = handler;
}

void Bridge::PostToJS(const std::string &type, const std::string &payloadJson) {
    PostToJS("{\"type\":\"" + type + "\",\"payload\":" + payloadJson + "}");
}

void Bridge::PostToJS(const std::string &typeAndPayload) {
    if (!webview_) return;
    
    ICoreWebView2* webview = (ICoreWebView2*)webview_;
    
    // UTF-8 → UTF-16 (handles Japanese and all non-ASCII characters)
    int wlen = MultiByteToWideChar(CP_UTF8, 0, typeAndPayload.c_str(), -1, nullptr, 0);
    std::wstring json(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, typeAndPayload.c_str(), -1, &json[0], wlen);
    webview->PostWebMessageAsJson(json.c_str());
}

void Bridge::SendInit(const std::string &language) {
    PostToJS("init", "{\"language\":\"" + language + "\"}");
}
