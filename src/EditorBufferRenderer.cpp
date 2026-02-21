#include "../include/EditorBufferRenderer.h"
#include "../include/Buffer.h"
#include "../include/Editor.h"
#include "../include/Localization.h"
#include <algorithm>
#include <vector>

extern Editor *g_editor;

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

EditorBufferRenderer::EditorBufferRenderer()
    : m_hwnd(NULL), m_fontSize(12.0f), m_fontFamily(L"Consolas"),
      m_fontWeight(DWRITE_FONT_WEIGHT_NORMAL), m_enableLigatures(true) {}

EditorBufferRenderer::~EditorBufferRenderer() {}

bool EditorBufferRenderer::Initialize(HWND hwnd) {
  m_hwnd = hwnd;
  HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                 m_d2dFactory.GetAddressOf());
  if (FAILED(hr))
    return false;

  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           &m_dwriteFactory);
  if (FAILED(hr))
    return false;

  hr = m_dwriteFactory->CreateTextFormat(
      m_fontFamily.c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, m_fontSize,
      L"", // locale
      &m_textFormat);

  return SUCCEEDED(hr);
}

void EditorBufferRenderer::SetTheme(const Theme &theme) {
  m_theme = theme;
  DiscardDeviceResources();
  InvalidateRect(m_hwnd, NULL, FALSE);
}

bool EditorBufferRenderer::CreateDeviceResources() {
  if (m_renderTarget)
    return true;

  RECT rc;
  GetClientRect(m_hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

  HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(m_hwnd, size), &m_renderTarget);

  if (SUCCEEDED(hr)) {
    m_renderTarget->CreateSolidColorBrush(m_theme.foreground, &m_brush);
    m_renderTarget->CreateSolidColorBrush(m_theme.background, &m_bgBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.caret, &m_caretBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.selection, &m_selBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.lineNumbers, &m_lnBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.keyword, &m_keywordBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.string, &m_stringBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.number, &m_numberBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.comment, &m_commentBrush);
    m_renderTarget->CreateSolidColorBrush(m_theme.function, &m_functionBrush);
  }

  return SUCCEEDED(hr);
}

void EditorBufferRenderer::DiscardDeviceResources() {
  m_renderTarget.Reset();
  m_brush.Reset();
  m_bgBrush.Reset();
  m_caretBrush.Reset();
  m_selBrush.Reset();
  m_lnBrush.Reset();
  m_keywordBrush.Reset();
  m_stringBrush.Reset();
  m_numberBrush.Reset();
  m_commentBrush.Reset();
  m_functionBrush.Reset();
}

void EditorBufferRenderer::Resize(UINT width, UINT height) {
  if (m_renderTarget) {
    m_renderTarget->Resize(D2D1::SizeU(width, height));
  }
}

size_t EditorBufferRenderer::GetPositionFromPoint(const std::string &text,
                                                  float x, float y,
                                                  size_t totalLinesInFile) {
  // Convert UTF-8 to UTF-16
  int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
  std::vector<wchar_t> wtext(len);
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), len);

  float gutterWidth = 0.0f;
  if (m_showLineNumbers) {
    int digits = (int)std::to_string(totalLinesInFile).length();
    gutterWidth = (digits * 8.0f) + 15.0f;
  }

  D2D1_SIZE_F size = {10000, 10000};
  if (m_renderTarget)
    size = m_renderTarget->GetSize();

  float layoutWidth = 100000.0f;
  if (m_wordWrap) {
    if (m_wrapWidth > 0)
      layoutWidth = m_wrapWidth;
    else
      layoutWidth = (std::max)(10.0f, size.width - gutterWidth - 10.0f);
  }

  std::wstring locale = Localization::Instance().GetLocaleName();
  ComPtr<IDWriteTextLayout> textLayout;
  HRESULT hr = m_dwriteFactory->CreateTextLayout(
      wtext.data(), static_cast<UINT32>(wtext.size() - 1), m_textFormat.Get(),
      layoutWidth, 10000.0f, &textLayout);

  if (FAILED(hr))
    return 0;

  textLayout->SetLocaleName(locale.c_str(), {0, (UINT32)wtext.size()});

  BOOL isTrailingHit;
  BOOL isInside;
  DWRITE_HIT_TEST_METRICS metrics;
  float scrollX = 0.0f;
  if (!m_wordWrap) {
    Buffer *buf = g_editor->GetActiveBuffer();
    if (buf)
      scrollX = buf->GetScrollX();
  }
  float adjustedX = (std::max)(0.0f, x - gutterWidth - 5.0f + scrollX);
  float adjustedY = y - val_TopPadding;
  textLayout->HitTestPoint(adjustedX, adjustedY, &isTrailingHit, &isInside,
                           &metrics);

  int charIndex = metrics.textPosition + (isTrailingHit ? 1 : 0);

  // Convert char back to byte index
  int byteIndex = WideCharToMultiByte(CP_UTF8, 0, wtext.data(), charIndex, NULL,
                                      0, NULL, NULL);
  return static_cast<size_t>(byteIndex);
}

bool EditorBufferRenderer::HitTestGutter(float x, float y,
                                         size_t totalLinesInFile,
                                         size_t &lineIndex) {
  float gutterWidth = 0.0f;
  if (!m_showLineNumbers)
    return false;

  int digits = (int)std::to_string(totalLinesInFile).length();
  gutterWidth = (digits * 8.0f) + 15.0f;

  if (x >= 0 && x <= gutterWidth) {
    float adjustedY = y - val_TopPadding;
    if (adjustedY < 0)
      return false;

    float lineHeight = GetLineHeight();
    lineIndex = static_cast<size_t>(adjustedY / lineHeight);
    return true;
  }
  return false;
}

float EditorBufferRenderer::GetLineHeight() const {
  if (!m_textFormat)
    return 20.0f;

  ComPtr<IDWriteTextLayout> textLayout;
  m_dwriteFactory->CreateTextLayout(L" ", 1, m_textFormat.Get(), 100.0f, 100.0f,
                                    &textLayout);
  if (textLayout) {
    DWRITE_LINE_METRICS metrics;
    UINT32 count;
    textLayout->GetLineMetrics(&metrics, 1, &count);
    return metrics.height;
  }
  return 20.0f;
}

float EditorBufferRenderer::GetTextWidth(const std::string &text) {
  int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
  std::vector<wchar_t> wtext(len);
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), len);

  ComPtr<IDWriteTextLayout> textLayout;
  HRESULT hr = m_dwriteFactory->CreateTextLayout(
      wtext.data(), static_cast<UINT32>(wtext.size() - 1), m_textFormat.Get(),
      100000.0f, 1000.0f, &textLayout);

  if (SUCCEEDED(hr)) {
    DWRITE_TEXT_METRICS metrics;
    textLayout->GetMetrics(&metrics);
    return metrics.widthIncludingTrailingWhitespace;
  }
  return 0.0f;
}

void EditorBufferRenderer::SetFont(const std::wstring &familyName,
                                   float fontSize, DWRITE_FONT_WEIGHT weight) {
  m_fontFamily = familyName;
  m_fontSize = fontSize;
  m_fontWeight = weight;
  UpdateFontFormat();
}

void EditorBufferRenderer::UpdateFontFormat() {
  m_textFormat.Reset();
  m_dwriteFactory->CreateTextFormat(
      m_fontFamily.c_str(), NULL, m_fontWeight, DWRITE_FONT_STYLE_NORMAL,
      DWRITE_FONT_STRETCH_NORMAL, m_fontSize, L"en-us", &m_textFormat);
}

void EditorBufferRenderer::ZoomIn() {
  m_fontSize += 2.0f;
  UpdateFontFormat();
}

void EditorBufferRenderer::ZoomOut() {
  if (m_fontSize > 4.0f) {
    m_fontSize -= 2.0f;
    UpdateFontFormat();
  }
}

void EditorBufferRenderer::ZoomReset() {
  m_fontSize = 12.0f;
  UpdateFontFormat();
}

// Internal helper to create the HWND render target
// We need the HWND from the Window
// I'll add a SetHwnd or Modify Initialize

size_t EditorBufferRenderer::CalculateVisibleLineCount() const {
  if (!m_renderTarget)
    return 50; // Default fallback

  D2D1_SIZE_F size = m_renderTarget->GetSize();
  float lineHeight = GetLineHeight();

  if (lineHeight <= 0.0f)
    return 50;

  // Calculate how many lines fit in the viewport
  float availableHeight = size.height - val_TopPadding;
  size_t visibleLines = static_cast<size_t>(availableHeight / lineHeight) +
                        2; // +2 for partial lines

  return visibleLines;
}

POINT EditorBufferRenderer::GetCaretScreenPoint() const {
  POINT pt = {(long)m_lastCaretRect.left, (long)m_lastCaretRect.bottom};
  ClientToScreen(m_hwnd, &pt);
  return pt;
}
