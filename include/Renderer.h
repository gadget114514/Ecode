#pragma once

#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#include <vector>

class Renderer {
public:
  Renderer();
  ~Renderer();

  bool Initialize(HWND hwnd);
  void Resize(UINT width, UINT height);
  void Render(const std::string &text, size_t caretPos = 0,
              size_t selectionStart = 0, size_t selectionEnd = 0,
              size_t firstLineNumber = 1, float scrollX = 0.0f,
              const std::vector<size_t> *physicalLineNumbers = nullptr,
              size_t totalLinesEstimate = 0);

  size_t GetPositionFromPoint(const std::string &text, float x, float y);
  bool HitTestGutter(const std::string &text, float x, float y,
                     size_t &lineIndex);
  float GetTextWidth(const std::string &text);
  float GetLineHeight() const;
  void SetTopOffset(float offset) { m_topOffset = offset; }

  // Font controls
  void SetFont(const std::wstring &familyName, float fontSize);
  void ZoomIn();
  void ZoomOut();
  void ZoomReset();

  void SetCaretVisible(bool visible) { m_caretVisible = visible; }
  void SetShowLineNumbers(bool show) { m_showLineNumbers = show; }
  void SetShowPhysicalLineNumbers(bool show) {
    m_showPhysicalLineNumbers = show;
  }
  void SetWordWrap(bool wrap) { m_wordWrap = wrap; }
  void SetWrapWidth(float width) { m_wrapWidth = width; }

  std::wstring GetFontFamily() const { return m_fontFamily; }
  float GetFontSize() const { return m_fontSize; }
  bool GetShowLineNumbers() const { return m_showLineNumbers; }
  bool GetShowPhysicalLineNumbers() const { return m_showPhysicalLineNumbers; }
  bool GetWordWrap() const { return m_wordWrap; }
  float GetWrapWidth() const { return m_wrapWidth; }

private:
  void UpdateFontFormat();
  bool m_caretVisible = true;
  bool m_showLineNumbers = true;
  bool m_showPhysicalLineNumbers = true;
  bool m_wordWrap = false;
  float m_wrapWidth = 0.0f; // 0 means wrap to window width
  float m_topOffset = 0.0f;
  HWND m_hwnd;
  ComPtr<ID2D1Factory> m_d2dFactory;
  ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
  ComPtr<IDWriteFactory> m_dwriteFactory;
  ComPtr<IDWriteTextFormat> m_textFormat;
  ComPtr<ID2D1SolidColorBrush> m_brush;

  std::wstring m_fontFamily;
  float m_fontSize;

  bool CreateDeviceResources();
  void DiscardDeviceResources();
};
