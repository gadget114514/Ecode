#pragma once

#include <string>
#include <windows.h>

class Dialogs {
public:
  static std::wstring OpenFileDialog(HWND hwnd);
  static std::wstring SaveFileDialog(HWND hwnd);
  static void ShowAboutDialog(HWND hwnd);
  static void ShowSettingsDialog(HWND hwnd);
  static void ShowAISettingsDialog(HWND hwnd);
  static void ShowFindReplaceDialog(HWND hwnd, bool replaceMode);
  static void ShowFindInFilesDialog(HWND hwnd);
  static void ShowFindFileDialog(HWND hwnd);
  static void ShowDiredPairsDialog(HWND hwnd);
  static void ShowJumpToLineDialog(HWND hwnd);
  static void ShowMacroGalleryDialog(HWND hwnd);
  static void ShowThemeManagerDialog(HWND hwnd);
  static std::wstring BrowseForFolder(HWND hwnd);

  enum class ConfirmationResult { Save, Discard, Cancel };
  static ConfirmationResult
  ShowSaveConfirmationDialog(HWND hwnd, const std::wstring &filename);

  enum class FileModifiedAction { Reload, Keep, OpenInNewBuffer };
  static FileModifiedAction
  ShowFileModifiedDialog(HWND hwnd, const std::wstring &filename, bool isDirty);
};
