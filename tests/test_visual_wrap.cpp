#include "../include/Buffer.h"
#include "../include/Editor.h"
#include "../include/EditorBufferRenderer.h"
#include <d2d1.h>
#include <dwrite.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

// Mocks and requirements for EditorBufferRenderer
std::unique_ptr<Editor> g_editor;

#define VERIFY(cond, msg)                                                      \
  if (!(cond)) {                                                               \
    std::cerr << "VISUAL FAILURE at line " << __LINE__ << ": " << msg          \
              << std::endl;                                                    \
    exit(1);                                                                   \
  }

// We need a real window for HwndRenderTarget in the current
// EditorBufferRenderer implementation, but we can create a hidden one.
LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  return DefWindowProc(hwnd, msg, wp, lp);
}

HWND CreateHiddenWindow() {
  WNDCLASSW wc = {0};
  wc.lpfnWndProc = DummyWndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = L"VisualTestWnd";
  RegisterClassW(&wc);
  return CreateWindowExW(0, wc.lpszClassName, L"Test", 0, 0, 0, 800, 600, NULL,
                         NULL, wc.hInstance, NULL);
}

void TestTextWrapping() {
  g_editor = std::make_unique<Editor>();
  g_editor->NewFile();

  HWND hwnd = CreateHiddenWindow();
  EditorBufferRenderer renderer;
  VERIFY(renderer.Initialize(hwnd), "Failed to initialize renderer");

  // Set font size to something predictable
  renderer.SetFont(L"Consolas", 20.0f);
  float lineHeight = renderer.GetLineHeight();
  VERIFY(lineHeight > 0, "Line height should be positive");

  std::string text = "This is a very long line of text used for testing the "
                     "word wrap functionality in Ecode.";
  // Logical line is 1 line.

  // Case 1: Wrap OFF
  renderer.SetWordWrap(false);
  renderer.SetShowLineNumbers(false); // Remove gutter for easier calc

  // Get position at (0, 0)
  size_t pos0 = renderer.GetPositionFromPoint(text, 0, 0, 1);
  VERIFY(pos0 == 0, "Top-left should be byte 0");

  // Get position at a large X, 0
  size_t posX = renderer.GetPositionFromPoint(text, 500, 0, 1);
  VERIFY(posX > 0, "Large X should hit later in the string");

  // Get position at 0, LineHeight
  size_t posNext = renderer.GetPositionFromPoint(text, 0, lineHeight, 1);
  // Since there's only 1 logical line and wrap is OFF, this should snap to the
  // start of the line (Char 0) because Y is outside (below) the line, so it
  // snaps to the nearest line (Line 0), and X=0 snaps to Char 0.
  VERIFY(posNext == 0,
         "Wrap OFF: (0, LineHeight) should snap to start of single line");

  // Case 2: Wrap ON
  renderer.SetWordWrap(true);
  // Set a narrow wrap width of 100 pixels (Consolas 20px chars are ~11px wide,
  // so ~9 chars per line)
  renderer.SetWrapWidth(100.0f);

  size_t posWrapped = renderer.GetPositionFromPoint(text, 0, lineHeight, 1);
  VERIFY(posWrapped > 0 && posWrapped < text.length(),
         "Wrap ON: (0, LineHeight) should hit in the middle of the string");

  size_t posWrapped2 =
      renderer.GetPositionFromPoint(text, 0, lineHeight * 2, 1);
  VERIFY(posWrapped2 > posWrapped,
         "Wrap ON: Each visual line should progress through the string");

  std::cout << "Test Passed: Visual Text Wrapping Logic" << std::endl;
  DestroyWindow(hwnd);
}

int main() {
  try {
    CoInitialize(NULL);
    TestTextWrapping();
    std::cout << "=== ALL VISUAL TESTS PASSED ===" << std::endl;
    CoUninitialize();
  } catch (const std::exception &e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
