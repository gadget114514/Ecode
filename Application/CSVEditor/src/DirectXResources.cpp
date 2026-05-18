#include "DirectXResources.h"

DirectXResources::DirectXResources()
{
}

DirectXResources::~DirectXResources()
{
    DiscardDeviceResources();
}

HRESULT DirectXResources::CreateDeviceIndependentResources()
{
    HRESULT hr = S_OK;

    if (!m_pD2DFactory) {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    }

    if (SUCCEEDED(hr) && !m_pDWriteFactory) {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
        );
    }

    if (SUCCEEDED(hr) && !m_pTextFormat) {
        hr = m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI",
            NULL,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,
            L"en-us",
            &m_pTextFormat
        );
    }

    return hr;
}

HRESULT DirectXResources::CreateDeviceResources(HWND hwnd)
{
    HRESULT hr = S_OK;

    if (!m_pRenderTarget) {
        RECT rc;
        GetClientRect(hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
        );

        hr = m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &m_pRenderTarget
        );
    }

    return hr;
}

void DirectXResources::DiscardDeviceResources()
{
    m_pRenderTarget.Release();
}

HRESULT DirectXResources::UpdateTextFormat(const std::wstring& fontName, float fontSize)
{
    m_pTextFormat.Release();
    
    if (!m_pDWriteFactory) return E_FAIL;
    
    return m_pDWriteFactory->CreateTextFormat(
        fontName.c_str(),
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-us",
        &m_pTextFormat
    );
}
