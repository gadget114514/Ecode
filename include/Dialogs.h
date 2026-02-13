#pragma once

#include <string>
#include <windows.h>


class Dialogs {
public:
  static std::wstring OpenFileDialog(HWND hwnd);
  static std::wstring SaveFileDialog(HWND hwnd);
  static void ShowAboutDialog(HWND hwnd);
  static void ShowSettingsDialog(HWND hwnd);
  static void ShowFindReplaceDialog(HWND hwnd, bool replaceMode);
  static void ShowJumpToLineDialog(HWND hwnd);
  static void ShowMacroGalleryDialog(HWND hwnd);
};
