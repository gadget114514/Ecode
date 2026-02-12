#include "../include/Renderer.h"
#include "../include/Buffer.h"
#include "../include/Editor.h"
#include "../include/Localization.h"
#include <algorithm>
#include <vector>

extern std::unique_ptr<Editor> g_editor;

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

Renderer::Renderer()
    : m_hwnd(NULL), m_fontSize(12.0f), m_fontFamily(L"Consolas") {}

Renderer::~Renderer() {}

bool Renderer::Initialize(HWND hwnd) {
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

bool Renderer::CreateDeviceResources() {
  if (m_renderTarget)
    return true;

  RECT rc;
  GetClientRect(m_hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

  HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(m_hwnd, size), &m_renderTarget);

  return SUCCEEDED(hr);
}

void Renderer::Resize(UINT width, UINT height) {
  if (m_renderTarget) {
    m_renderTarget->Resize(D2D1::SizeU(width, height));
  }
}

void Renderer::Render(const std::string &text, size_t caretPos,
                      size_t selectionStart, size_t selectionEnd,
                      size_t firstLineNumber, float scrollX,
                      const std::vector<size_t> *physicalLineNumbers,
                      size_t totalLinesEstimate) {
  if (!CreateDeviceResources())
    return;

  m_renderTarget->BeginDraw();
  m_renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

  if (!m_brush) {
    m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                          &m_brush);
  }

  // Convert UTF-8 to UTF-16 for DirectWrite
  int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
  std::vector<wchar_t> wtext(len);
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), len);

  // Find character index for caretPos (which is byte index)
  // Simple way: convert prefix to get char count
  int charIndex = 0;
  if (caretPos > 0) {
    charIndex = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                    static_cast<int>(caretPos), NULL, 0);
  }

  D2D1_SIZE_F size = m_renderTarget->GetSize();

  // Calculate dynamic gutter width
  float gutterWidth = 0.0f;
  if (m_showLineNumbers) {
    size_t maxNum = (std::max)(totalLinesEstimate, firstLineNumber);
    int digits = (maxNum > 0) ? (int)std::to_string(maxNum).length() : 1;
    gutterWidth = (digits * 8.0f) + 15.0f; // Approx 8px per digit + padding
  }

  // Draw Gutter
  if (m_showLineNumbers) {
    m_renderTarget->DrawLine(D2D1::Point2F(gutterWidth, m_topOffset),
                             D2D1::Point2F(gutterWidth, size.height),
                             m_brush.Get(), 1.0f);
  }

  ComPtr<IDWriteTextLayout> textLayout;
  float layoutWidth = 100000.0f;
  if (m_wordWrap) {
    if (m_wrapWidth > 0)
      layoutWidth = m_wrapWidth;
    else
      layoutWidth = (std::max)(10.0f, size.width - gutterWidth - 10.0f);
  }

  std::wstring locale = Localization::Instance().GetLocaleName();
  HRESULT hr = m_dwriteFactory->CreateTextLayout(
      wtext.data(), static_cast<UINT32>(wtext.size() - 1), m_textFormat.Get(),
      layoutWidth, size.height, &textLayout);

  if (SUCCEEDED(hr)) {
    textLayout->SetLocaleName(locale.c_str(), {0, (UINT32)wtext.size()});
  }

  if (SUCCEEDED(hr)) {
    float lineHeight = GetLineHeight();
    float yOffset =
        -(static_cast<float>(firstLineNumber - 1) * lineHeight) + m_topOffset;
    float xOffset = gutterWidth + 5 - scrollX;

    // Selection Highlighting
    if (selectionStart != selectionEnd) {
      int selStartChar = MultiByteToWideChar(
          CP_UTF8, 0, text.c_str(), static_cast<int>(selectionStart), NULL, 0);
      int selEndChar = MultiByteToWideChar(
          CP_UTF8, 0, text.c_str(), static_cast<int>(selectionEnd), NULL, 0);

      UINT32 actualHitTestCount = 0;
      textLayout->HitTestTextRange(selStartChar, selEndChar - selStartChar, 0,
                                   0, NULL, 0, &actualHitTestCount);

      if (actualHitTestCount > 0) {
        std::vector<DWRITE_HIT_TEST_METRICS> hitTestMetrics(actualHitTestCount);
        textLayout->HitTestTextRange(selStartChar, selEndChar - selStartChar, 0,
                                     0, hitTestMetrics.data(),
                                     actualHitTestCount, &actualHitTestCount);

        ComPtr<ID2D1SolidColorBrush> selBrush;
        m_renderTarget->CreateSolidColorBrush(
            D2D1::ColorF(D2D1::ColorF::LightBlue, 0.5f), &selBrush);

        for (const auto &m : hitTestMetrics) {
          m_renderTarget->FillRectangle(D2D1::RectF(m.left + xOffset,
                                                    m.top + yOffset,
                                                    m.left + m.width + xOffset,
                                                    m.top + m.height + yOffset),
                                        selBrush.Get());
        }
      }
    }

    m_renderTarget->DrawTextLayout(D2D1::Point2F(xOffset, yOffset),
                                   textLayout.Get(), m_brush.Get());

    // Draw Line Numbers
    if (m_showLineNumbers) {
      UINT32 lineCount = 0;
      textLayout->GetLineMetrics(NULL, 0, &lineCount);
      if (lineCount > 0) {
        std::vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);
        textLayout->GetLineMetrics(lineMetrics.data(), lineCount, &lineCount);

        float currentY = yOffset;
        for (UINT32 i = 0; i < lineCount; ++i) {
          if (currentY + lineMetrics[i].height > 0 && currentY < size.height) {
            size_t displayNum = 0;
            if (m_showPhysicalLineNumbers && physicalLineNumbers) {
              if (i < physicalLineNumbers->size()) {
                displayNum = (*physicalLineNumbers)[i] + 1;
              } else {
                displayNum = 0; // Or some indicator
              }
            } else {
              displayNum = i + firstLineNumber;
            }

            if (displayNum > 0 || !m_showPhysicalLineNumbers) {
              std::wstring lineNum = std::to_wstring(displayNum);
              D2D1_RECT_F lineRect =
                  D2D1::RectF(0, currentY, gutterWidth - 5,
                              currentY + lineMetrics[i].height);
              if (lineRect.top >= m_topOffset) {
                m_renderTarget->DrawText(
                    lineNum.c_str(), static_cast<UINT32>(lineNum.length()),
                    m_textFormat.Get(), lineRect, m_brush.Get());

                // Draw Fold/Unfold indicator
                // For now, let's assume any line can be a fold point for
                // testing Real logic might check if line is folded in
                // activeBuffer
                Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
                if (buf) {
                  size_t physicalLine =
                      buf->GetPhysicalLine(i + firstLineNumber - 1);
                  bool isFolded = buf->IsLineFolded(physicalLine);

                  // Draw small box for indicator
                  float boxSize = 8.0f;
                  float boxX = gutterWidth - 12.0f;
                  float boxY =
                      currentY + (lineMetrics[i].height - boxSize) / 2.0f;
                  D2D1_RECT_F boxRect =
                      D2D1::RectF(boxX, boxY, boxX + boxSize, boxY + boxSize);
                  m_renderTarget->DrawRectangle(boxRect, m_brush.Get(), 1.0f);

                  // Horizontal line (-)
                  m_renderTarget->DrawLine(
                      D2D1::Point2F(boxX + 2, boxY + boxSize / 2),
                      D2D1::Point2F(boxX + boxSize - 2, boxY + boxSize / 2),
                      m_brush.Get(), 1.0f);

                  if (isFolded) {
                    // Vertical line to make it (+)
                    m_renderTarget->DrawLine(
                        D2D1::Point2F(boxX + boxSize / 2, boxY + 2),
                        D2D1::Point2F(boxX + boxSize / 2, boxY + boxSize - 2),
                        m_brush.Get(), 1.0f);
                  }
                }
              }
            }
          }
          currentY += lineMetrics[i].height;
        }
      }
    }

    // Draw Caret
    if (m_caretVisible) {
      DWRITE_HIT_TEST_METRICS metrics;
      float caretX, caretY;
      hr = textLayout->HitTestTextPosition(charIndex, FALSE, &caretX, &caretY,
                                           &metrics);
      if (SUCCEEDED(hr)) {
        m_renderTarget->DrawLine(
            D2D1::Point2F(caretX + xOffset, caretY + yOffset),
            D2D1::Point2F(caretX + xOffset, caretY + metrics.height + yOffset),
            m_brush.Get(), 2.0f);
      }
    }
  }

  m_renderTarget->EndDraw();
}

size_t Renderer::GetPositionFromPoint(const std::string &text, float x,
                                      float y) {
  // Convert UTF-8 to UTF-16
  int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
  std::vector<wchar_t> wtext(len);
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wtext.data(), len);

  float gutterWidth = 0.0f;
  if (m_showLineNumbers) {
    size_t lineCount = std::count(text.begin(), text.end(), '\n') + 1;
    int digits = (int)std::to_string(lineCount).length();
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
  float adjustedY = y - m_topOffset;
  textLayout->HitTestPoint(adjustedX, adjustedY, &isTrailingHit, &isInside,
                           &metrics);

  int charIndex = metrics.textPosition + (isTrailingHit ? 1 : 0);

  // Convert char back to byte index
  int byteIndex = WideCharToMultiByte(CP_UTF8, 0, wtext.data(), charIndex, NULL,
                                      0, NULL, NULL);
  return static_cast<size_t>(byteIndex);
}

bool Renderer::HitTestGutter(const std::string &text, float x, float y,
                             size_t &lineIndex) {
  float gutterWidth = 0.0f;
  if (!m_showLineNumbers)
    return false;

  size_t totalLines = std::count(text.begin(), text.end(), '\n') + 1;
  int digits = (int)std::to_string(totalLines).length();
  gutterWidth = (digits * 8.0f) + 15.0f;

  if (x >= 0 && x <= gutterWidth) {
    float adjustedY = y - m_topOffset;
    if (adjustedY < 0)
      return false;

    float lineHeight = GetLineHeight();
    lineIndex = static_cast<size_t>(adjustedY / lineHeight);
    return true;
  }
  return false;
}

float Renderer::GetLineHeight() const {
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

float Renderer::GetTextWidth(const std::string &text) {
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

void Renderer::SetFont(const std::wstring &familyName, float fontSize) {
  m_fontFamily = familyName;
  m_fontSize = fontSize;
  UpdateFontFormat();
}

void Renderer::UpdateFontFormat() {
  m_textFormat.Reset();
  m_dwriteFactory->CreateTextFormat(
      m_fontFamily.c_str(), NULL, DWRITE_FONT_WEIGHT_NORMAL,
      DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, m_fontSize,
      L"en-us", &m_textFormat);
}

void Renderer::ZoomIn() {
  m_fontSize += 2.0f;
  UpdateFontFormat();
}

void Renderer::ZoomOut() {
  if (m_fontSize > 4.0f) {
    m_fontSize -= 2.0f;
    UpdateFontFormat();
  }
}

void Renderer::ZoomReset() {
  m_fontSize = 12.0f;
  UpdateFontFormat();
}

// Internal helper to create the HWND render target
// We need the HWND from the Window
// I'll add a SetHwnd or Modify Initialize
