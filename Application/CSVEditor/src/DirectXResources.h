#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <atlbase.h> // For CComPtr
#include <string>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

class DirectXResources {
public:
    DirectXResources();
    ~DirectXResources();

    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources(HWND hwnd);
    void DiscardDeviceResources();
    
    HRESULT UpdateTextFormat(const std::wstring& fontName, float fontSize);

    ID2D1HwndRenderTarget* GetRenderTarget() const { return m_pRenderTarget; }
    IDWriteFactory* GetWriteFactory() const { return m_pDWriteFactory; }
    IDWriteTextFormat* GetTextFormat() const { return m_pTextFormat; }

private:
    CComPtr<ID2D1Factory> m_pD2DFactory;
    CComPtr<IDWriteFactory> m_pDWriteFactory;
    CComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
    CComPtr<IDWriteTextFormat> m_pTextFormat;
};
