#include "../include/Buffer.h"
#include "../include/Editor.h"
#include "../include/EditorBufferRenderer.h"
#include "../include/Localization.h"
#include <algorithm>
#include <vector>

extern std::unique_ptr<Editor> g_editor;

void EditorBufferRenderer::DrawEditorLines(
    const std::string &text, size_t caretPos,
    const std::vector<Buffer::SelectionRange> *selectionRanges,
    const std::vector<Buffer::HighlightRange> *highlights,
    size_t firstLineNumber, float scrollX,
    const std::vector<size_t> *physicalLineNumbers, size_t totalLinesEstimate) {
  if (!this->CreateDeviceResources())
    return;

  this->m_renderTarget->BeginDraw();
  this->m_renderTarget->Clear(this->m_theme.background);

  // OPTIMIZATION #6: Cache UTF-8 to UTF-16 conversion
  bool textChanged = false;
  if (!m_isConversionCacheValid || m_lastUtf8Content != text) {
    int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    m_cachedWtext.resize(len);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, m_cachedWtext.data(),
                        len);
    m_lastUtf8Content = text;
    m_isConversionCacheValid = true;
    textChanged = true;
  }

  const std::vector<wchar_t> &wtext = m_cachedWtext;

  // Find character index for caretPos (which is byte index)
  int charIndex = 0;
  if (caretPos > 0) {
    charIndex = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                    static_cast<int>(caretPos), NULL, 0);
  }

  D2D1_SIZE_F size = this->m_renderTarget->GetSize();

  // Calculate dynamic gutter width
  float gutterWidth = 0.0f;
  if (this->m_showLineNumbers) {
    size_t maxNum = (std::max)(totalLinesEstimate, firstLineNumber);
    int digits = (maxNum > 0) ? (int)std::to_string(maxNum).length() : 1;
    gutterWidth = (digits * 8.0f) + 15.0f;
  }

  // Draw Gutter Line
  if (this->m_showLineNumbers) {
    this->m_renderTarget->DrawLine(
        D2D1::Point2F(gutterWidth, this->val_TopPadding),
        D2D1::Point2F(gutterWidth, size.height), this->m_lnBrush.Get(), 1.0f);
  }

  float layoutWidth = 100000.0f;
  if (this->m_wordWrap) {
    if (this->m_wrapWidth > 0)
      layoutWidth = this->m_wrapWidth;
    else
      layoutWidth = (std::max)(10.0f, size.width - gutterWidth - 10.0f);
  }

  // OPTIMIZATION #9: Reuse TextLayout if possible
  bool highlightsChanged = false;
  if (highlights) {
    if (*highlights != m_lastHighlights) {
      highlightsChanged = true;
      m_lastHighlights = *highlights;
    }
  } else if (!m_lastHighlights.empty()) {
    highlightsChanged = true;
    m_lastHighlights.clear();
  }

  if (!m_cachedTextLayout || textChanged || m_lastLayoutWidth != layoutWidth ||
      m_lastLayoutHeight != size.height || highlightsChanged) {
    std::wstring locale = Localization::Instance().GetLocaleName();
    HRESULT hr = this->m_dwriteFactory->CreateTextLayout(
        wtext.data(), static_cast<UINT32>(wtext.size() - 1),
        this->m_textFormat.Get(), layoutWidth, size.height,
        &m_cachedTextLayout);

    if (SUCCEEDED(hr)) {
      m_cachedTextLayout->SetLocaleName(locale.c_str(),
                                        {0, (UINT32)wtext.size()});

      if (this->m_enableLigatures) {
        Microsoft::WRL::ComPtr<IDWriteTypography> typography;
        if (SUCCEEDED(this->m_dwriteFactory->CreateTypography(&typography))) {
          DWRITE_FONT_FEATURE feature = {
              DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1};
          typography->AddFontFeature(feature);
          m_cachedTextLayout->SetTypography(typography.Get(),
                                            {0, (UINT32)wtext.size()});
        }
      }

      // Apply Syntax Highlighting
      if (highlights && !highlights->empty()) {
        for (const auto &hrange : *highlights) {
          int startChar =
              MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                  static_cast<int>(hrange.start), NULL, 0);
          int endChar = MultiByteToWideChar(
              CP_UTF8, 0, text.c_str(),
              static_cast<int>(hrange.start + hrange.length), NULL, 0);

          ID2D1SolidColorBrush *hBrush = nullptr;
          switch (hrange.type) {
          case 1:
            hBrush = this->m_keywordBrush.Get();
            break;
          case 2:
            hBrush = this->m_stringBrush.Get();
            break;
          case 3:
            hBrush = this->m_numberBrush.Get();
            break;
          case 4:
            hBrush = this->m_commentBrush.Get();
            break;
          case 5:
            hBrush = this->m_functionBrush.Get();
            break;
          }
          if (hBrush) {
            m_cachedTextLayout->SetDrawingEffect(
                hBrush, {(UINT32)startChar, (UINT32)(endChar - startChar)});
          }
        }
      }
    }
    m_lastLayoutWidth = layoutWidth;
    m_lastLayoutHeight = size.height;
  }

  IDWriteTextLayout *textLayout = m_cachedTextLayout.Get();
  if (!textLayout)
    return; // Should not happen if CreateDeviceResources worked

  {
    HRESULT hr = S_OK;
    float lineHeight = this->GetLineHeight();
    float yOffset = this->val_TopPadding; // Text is exactly the viewport content, so draw from top
    float xOffset = gutterWidth + 5 - scrollX;

    // Selection Highlighting
    if (selectionRanges && !selectionRanges->empty()) {
      for (const auto &range : *selectionRanges) {
        int selStartChar = MultiByteToWideChar(
            CP_UTF8, 0, text.c_str(), static_cast<int>(range.start), NULL, 0);
        int selEndChar = MultiByteToWideChar(
            CP_UTF8, 0, text.c_str(), static_cast<int>(range.end), NULL, 0);

        UINT32 actualHitTestCount = 0;
        textLayout->HitTestTextRange(selStartChar, selEndChar - selStartChar, 0,
                                     0, NULL, 0, &actualHitTestCount);

        if (actualHitTestCount > 0) {
          std::vector<DWRITE_HIT_TEST_METRICS> hitTestMetrics(
              actualHitTestCount);
          textLayout->HitTestTextRange(selStartChar, selEndChar - selStartChar,
                                       0, 0, hitTestMetrics.data(),
                                       actualHitTestCount, &actualHitTestCount);

          for (const auto &m : hitTestMetrics) {
            this->m_renderTarget->FillRectangle(
                D2D1::RectF(m.left + xOffset, m.top + yOffset,
                            m.left + m.width + xOffset,
                            m.top + m.height + yOffset),
                this->m_selBrush.Get());
          }
        }
      }
    }

    this->m_renderTarget->DrawTextLayout(D2D1::Point2F(xOffset, yOffset),
                                         textLayout, this->m_brush.Get());

    // Draw Line Numbers
    if (this->m_showLineNumbers) {
      UINT32 lineCount = 0;
      textLayout->GetLineMetrics(NULL, 0, &lineCount);
      if (lineCount > 0) {
        std::vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);
        textLayout->GetLineMetrics(lineMetrics.data(), lineCount, &lineCount);

        float currentY = yOffset;
        for (UINT32 i = 0; i < lineCount; ++i) {
          if (currentY + lineMetrics[i].height > 0 && currentY < size.height) {
            size_t displayNum = 0;
            if (physicalLineNumbers && i < physicalLineNumbers->size()) {
              size_t currentLog = (*physicalLineNumbers)[i];
              // Only show number for the first visual line of a logical line
              if (i == 0 ||
                  (i > 0 && (*physicalLineNumbers)[i - 1] != currentLog)) {
                displayNum = currentLog + 1;
              }
            } else {
              // Fallback if no physical mapping (shouldn't happen for editor)
              displayNum = i + firstLineNumber;
            }

            if (displayNum > 0) {
              std::wstring lineNum = std::to_wstring(displayNum);
              D2D1_RECT_F lineRect =
                  D2D1::RectF(0, currentY, gutterWidth - 5,
                              currentY + lineMetrics[i].height);
              float tp = this->val_TopPadding;
              if (lineRect.top >= tp) {
                this->m_renderTarget->DrawText(
                    lineNum.c_str(), static_cast<UINT32>(lineNum.length()),
                    this->m_textFormat.Get(), lineRect, this->m_lnBrush.Get());

                Buffer *buf = g_editor ? g_editor->GetActiveBuffer() : nullptr;
                if (buf) {
                  /*
                  // Removed fold icons per user request
                  size_t physicalLine =
                      buf->GetPhysicalLine(i + firstLineNumber - 1);
                  bool isFolded = buf->IsLineFolded(physicalLine);

                  float boxSize = 8.0f;
                  float boxX = gutterWidth - 12.0f;
                  float boxY =
                      currentY + (lineMetrics[i].height - boxSize) / 2.0f;
                  D2D1_RECT_F boxRect =
                      D2D1::RectF(boxX, boxY, boxX + boxSize, boxY + boxSize);
                  this->m_renderTarget->DrawRectangle(
                      boxRect, this->m_lnBrush.Get(), 1.0f);

                  this->m_renderTarget->DrawLine(
                      D2D1::Point2F(boxX + 2, boxY + boxSize / 2),
                      D2D1::Point2F(boxX + boxSize - 2, boxY + boxSize / 2),
                      this->m_lnBrush.Get(), 1.0f);

                  if (isFolded) {
                    this->m_renderTarget->DrawLine(
                        D2D1::Point2F(boxX + boxSize / 2, boxY + 2),
                        D2D1::Point2F(boxX + boxSize / 2, boxY + boxSize - 2),
                        this->m_lnBrush.Get(), 1.0f);
                  }
                  */
                }
              }
            }
          }
          currentY += lineMetrics[i].height;
        }
      }
    }

    // Draw Caret
    // Draw Caret
    bool cv = m_enableCaretBlinking ? this->m_bIsCaretVisibleVal : true;
    if (cv) {
      DWRITE_HIT_TEST_METRICS metrics;
      float caretX, caretY;
      HRESULT hr = textLayout->HitTestTextPosition(charIndex, FALSE, &caretX,
                                                   &caretY, &metrics);
      if (SUCCEEDED(hr)) {
        float width = metrics.width;
        if (width <= 0)
          width = 8.0f;

        if (m_caretStyle == CaretStyle::Block) {
          D2D1_RECT_F rect = D2D1::RectF(caretX + xOffset, caretY + yOffset,
                                         caretX + xOffset + width,
                                         caretY + metrics.height + yOffset);
          this->m_lastCaretRect = rect;
          m_caretBrush->SetOpacity(0.5f);
          this->m_renderTarget->FillRectangle(rect, this->m_caretBrush.Get());
          m_caretBrush->SetOpacity(1.0f);
        } else if (m_caretStyle == CaretStyle::Underline) {
          float yPos = caretY + metrics.height + yOffset;
          this->m_lastCaretRect = D2D1::RectF(caretX + xOffset, yPos - 2,
                                              caretX + xOffset + width, yPos);
          this->m_renderTarget->DrawLine(
              D2D1::Point2F(caretX + xOffset, yPos),
              D2D1::Point2F(caretX + xOffset + width, yPos),
              this->m_caretBrush.Get(), 2.0f);
        } else { // Line
          this->m_lastCaretRect = D2D1::RectF(
              caretX + xOffset, caretY + yOffset, caretX + xOffset + 2,
              caretY + metrics.height + yOffset);
          this->m_renderTarget->DrawLine(
              D2D1::Point2F(caretX + xOffset, caretY + yOffset),
              D2D1::Point2F(caretX + xOffset,
                            caretY + metrics.height + yOffset),
              this->m_caretBrush.Get(), 2.0f);
        }
      }
    }
  }

  this->m_renderTarget->EndDraw();
}
