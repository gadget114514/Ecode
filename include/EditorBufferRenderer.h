#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include "Buffer.h"
#include <vector>

struct Theme {
  D2D1_COLOR_F background = {1.0f, 1.0f, 1.0f, 1.0f};
  D2D1_COLOR_F foreground = {0.0f, 0.0f, 0.0f, 1.0f};
  D2D1_COLOR_F caret = {0.0f, 0.0f, 0.0f, 1.0f};
  D2D1_COLOR_F selection = {0.68f, 0.85f, 0.90f, 0.5f}; // LightBlue
  D2D1_COLOR_F lineNumbers = {0.5f, 0.5f, 0.5f, 1.0f};

  // Syntax
  D2D1_COLOR_F keyword = {0.0f, 0.0f, 1.0f, 1.0f};
  D2D1_COLOR_F string = {0.64f, 0.16f, 0.16f, 1.0f};
  D2D1_COLOR_F number = {0.0f, 0.5f, 0.0f, 1.0f};
  D2D1_COLOR_F comment = {0.0f, 0.39f, 0.0f, 1.0f};
  D2D1_COLOR_F function = {0.5f, 0.0f, 0.5f, 1.0f};
};

class EditorBufferRenderer {
public:
  EditorBufferRenderer();
  ~EditorBufferRenderer();

  bool Initialize(HWND hwnd);
  void Resize(UINT width, UINT height);
  void DrawEditorLines(
      const std::string &text, size_t caretPos = 0,
      const std::vector<Buffer::SelectionRange> *selectionRanges = nullptr,
      const std::vector<Buffer::HighlightRange> *highlights = nullptr,
      size_t firstLineNumber = 1, float scrollX = 0.0f,
      const std::vector<size_t> *physicalLineNumbers = nullptr,
      size_t totalLinesEstimate = 0);

  size_t GetPositionFromPoint(const std::string &text, float x, float y,
                              size_t totalLinesInFile);
  bool HitTestGutter(float x, float y, size_t totalLinesInFile,
                     size_t &lineIndex);
  float GetTextWidth(const std::string &text);
  float GetLineHeight() const;
  void SetTopOffset(float offset) { val_TopPadding = offset; }

  // Font controls
  void SetFont(const std::wstring &familyName, float fontSize,
               DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL);
  bool SetLigatures(bool enable) {
    bool old = m_enableLigatures;
    m_enableLigatures = enable;
    UpdateFontFormat();
    return old;
  }
  bool SetEnableLigatures(bool enable) { return SetLigatures(enable); } // Alias
  void ZoomIn();
  void ZoomOut();
  void ZoomReset();

  enum class CaretStyle { Line, Block, Underline };

  void SetCaretVisible(bool visible) { m_bIsCaretVisibleVal = visible; }
  void SetCaretStyle(CaretStyle style) { m_caretStyle = style; }
  void SetCaretBlinking(bool blink) { m_enableCaretBlinking = blink; }
  bool SetShowLineNumbers(bool show) {
    bool old = m_showLineNumbers;
    m_showLineNumbers = show;
    return old;
  }
  bool SetWordWrap(bool wrap) {
    bool old = m_wordWrap;
    m_wordWrap = wrap;
    return old;
  }
  bool SetWrapWidth(float width) {
    m_wrapWidth = width;
    return true;
  }
  void SetTheme(const Theme &theme);

  // Viewport rendering support for large files
  size_t CalculateVisibleLineCount() const;

  std::wstring GetFontFamily() const { return m_fontFamily; }
  float GetFontSize() const { return m_fontSize; }
  DWRITE_FONT_WEIGHT GetFontWeight() const { return m_fontWeight; }
  bool GetEnableLigatures() const { return m_enableLigatures; }
  bool GetShowLineNumbers() const { return m_showLineNumbers; }
  bool IsWordWrap() const { return m_wordWrap; }
  bool GetWordWrap() const { return m_wordWrap; } // Alias for IsWordWrap
  float GetWrapWidth() const { return m_wrapWidth; }
  CaretStyle GetCaretStyle() const { return m_caretStyle; }
  bool GetCaretBlinking() const { return m_enableCaretBlinking; }

  D2D1_RECT_F GetLastCaretRect() const { return m_lastCaretRect; }
  POINT GetCaretScreenPoint() const;

private:
  void UpdateFontFormat();
  bool m_bIsCaretVisibleVal = true;
  bool m_enableCaretBlinking = true;
  CaretStyle m_caretStyle = CaretStyle::Line;
  bool m_showLineNumbers = true;
  bool m_wordWrap = false;
  float m_wrapWidth = 0.0f; // 0 means wrap to window width
  float val_TopPadding = 0.0f;
  HWND m_hwnd;
  D2D1_RECT_F m_lastCaretRect = {0, 0, 0, 0};
  // OPTIMIZATION #6: UTF-8 to UTF-16 conversion caching
  mutable std::string m_lastUtf8Content;
  mutable std::vector<wchar_t> m_cachedWtext;
  mutable bool m_isConversionCacheValid = false;

  void InvalidateConversionCache() { m_isConversionCacheValid = false; }
  void InvalidateLayoutCache() { m_cachedTextLayout = nullptr; }

  // OPTIMIZATION #9: DirectWrite TextLayout caching
  mutable ComPtr<IDWriteTextLayout> m_cachedTextLayout;
  mutable float m_lastLayoutWidth = 0.0f;
  mutable float m_lastLayoutHeight = 0.0f;
  mutable std::vector<Buffer::HighlightRange> m_lastHighlights;

  ComPtr<ID2D1Factory> m_d2dFactory;
  ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
  ComPtr<IDWriteFactory> m_dwriteFactory;
  ComPtr<IDWriteTextFormat> m_textFormat;
  Theme m_theme;

  ComPtr<ID2D1SolidColorBrush> m_brush; // Foreground
  ComPtr<ID2D1SolidColorBrush> m_bgBrush;
  ComPtr<ID2D1SolidColorBrush> m_caretBrush;
  ComPtr<ID2D1SolidColorBrush> m_selBrush;
  ComPtr<ID2D1SolidColorBrush> m_lnBrush;
  ComPtr<ID2D1SolidColorBrush> m_keywordBrush;
  ComPtr<ID2D1SolidColorBrush> m_stringBrush;
  ComPtr<ID2D1SolidColorBrush> m_numberBrush;
  ComPtr<ID2D1SolidColorBrush> m_commentBrush;
  ComPtr<ID2D1SolidColorBrush> m_functionBrush;

  std::wstring m_fontFamily;
  float m_fontSize;
  DWRITE_FONT_WEIGHT m_fontWeight;
  bool m_enableLigatures;

  bool CreateDeviceResources();
  void DiscardDeviceResources();
};
