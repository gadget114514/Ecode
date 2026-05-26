# Bug Fix: Dialog Management and Folder Validation

## 1. Dialog Focus & Creation Fixed
**Issue**: Backup Set and Unit edit dialogs occasionally failed to open or caused focus issues.
**Root Cause**: 
- `RegisterClassEx` was called every time a dialog was opened without checking if the class was already registered.
- `EndDialog` was used in a context where `CreateWindowEx` was used for custom modal behavior, which is incorrect for non-standard dialog templates.
**Fix**:
- Implemented `static bool s_classRegistered` guard to ensure `RegisterClassExW` only runs once.
- Switched to `DestroyWindow(hWnd)` for all dialog closing actions (Save, Close, Cancel).
- Added `if (!hDlg) return;` safety checks.

## 2. Same Folder Copy Prevention
**Issue**: Users could accidentally set the same folder for source and destination, leading to potential data loops or errors.
**Fix**:
- Added validation in `UnitEditDlgProc`'s save handler.
- **Normalization**: Automatically trims trailing slashes and backslashes.
- **Comparison**: Performs case-insensitive comparison of source and target paths.
- **Feedback**: Shows a localized error message (`Err_SamePath`) if paths match.
- **Localization**: Added the following strings:
  - 🇬🇧 English: "Source and target folders cannot be the same."
  - 🇯🇵 Japanese: "ソースとターゲットに同じフォルダを指定することはできません。"
  - 🇪🇸 Spanish: "La carpeta de origen y destino no pueden ser la misma."
  - 🇫🇷 French: "Le dossier source et le dossier cible ne peuvent pas être les mêmes."
  - 🇩🇪 German: "Quell- und Zielordner dürfen nicht identisch sein."

---
*Date: 2025-12-31*
