#include "Bridge.h"
#include <unknwn.h>
#include <string>

// Minimal ICoreWebView2 vtable layout for PostWebMessageAsJson
// IUnknown: QueryInterface, AddRef, Release (indices 0-2)
// ICoreWebView2: get_CoreWebView2Controller (3), get_Settings (4), Navigate (5),
//   NavigateToString (6), PostWebMessageAsJson (7), PostWebMessageAsString (8), ...
struct ICoreWebView2Vtbl {
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUnknown*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IUnknown*);
    ULONG   (STDMETHODCALLTYPE *Release)(IUnknown*);
    HRESULT (STDMETHODCALLTYPE *get_CoreWebView2Controller)(IUnknown*, void**);
    HRESULT (STDMETHODCALLTYPE *get_Settings)(IUnknown*, void**);
    HRESULT (STDMETHODCALLTYPE *Navigate)(IUnknown*, LPCWSTR);
    HRESULT (STDMETHODCALLTYPE *NavigateToString)(IUnknown*, LPCWSTR);
    HRESULT (STDMETHODCALLTYPE *PostWebMessageAsJson)(IUnknown*, LPCWSTR);
};

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
    
    // WebView2 vtable index 7 = PostWebMessageAsJson
    // Based on ICoreWebView2 interface layout
    auto vtbl = *(void***)webview_;
    if (!vtbl) return;
    
    typedef HRESULT (STDMETHODCALLTYPE *PostMsgFn)(IUnknown*, LPCWSTR);
    auto postFn = (PostMsgFn)vtbl[7];
    if (!postFn) return;
    
    std::wstring json(typeAndPayload.begin(), typeAndPayload.end());
    postFn(webview_, json.c_str());
}

void Bridge::SendInit(const std::string &language) {
    PostToJS("init", "{\"language\":\"" + language + "\"}");
}
