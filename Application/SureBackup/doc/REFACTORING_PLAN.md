# SureBackup CMake Migration & Refactoring Status

## Current State (2025-12-31)

### ✅ Successfully Completed:
1. **CMakeLists.txt created** - Project properly configured for CMake
2. **Compilation succeeds** - All C++ source files compile without errors
3. **UTF-8 encoding** - Japanese localization strings work correctly
4. **BackupMode enum migration** - Replaced deprecated `syncMode` bool with proper enum
5. **Localization strings** - Added all missing StrId entries for new UI elements
6. **Configuration system** - Fixed variable declarations and criteria field parsing

### ⚠️ Remaining Linker Errors (2 unresolved externals):
1. **`BackupUtils::NeedsUpdate`** - Function is declared in header but missing implementation in BackupUtils.cpp
2. **`ParallelBackupStrategy::SafeLog`** - Function is declared but missing implementation

### 📋 Next Steps:

#### Phase 1: Fix Linker Errors (Immediate)
- [ ] Implement `BackupUtils::NeedsUpdate` in BackupUtils.cpp
- [ ] Implement `ParallelBackupStrategy::SafeLog` in ParallelBackupStrategy.cpp  
- [ ] Verify successful build and executable creation

#### Phase 2: Code Refactoring (Main Goal)
The user wants to "arrange too big functions to make them understand able". Focus on:

##### 2.1 Refactor `WndProc` (Currently ~500 lines)
Break down the massive window procedure into smaller helper functions:
- **`HandleMenuCommands(HWND, WPARAM)`** - Process menu items
- **`HandleTreeViewSelection(LPNMHDR)`** - Handle tree view selection changes
- **`HandleTreeViewRightClick(LPNMHDR)`** - Context menu logic
- **`HandleTreeViewExpansion(LPNMHDR)`** - Folder expansion/collapse
- **`ResizeMainWindow(HWND, int width, int height)`** - Window resizing logic
- **`InitializeMainWindow(HWND)`** - Window creation/initialization

##### 2.2 Refactor `UnitEditDlgProc` (Currently ~200 lines)
Break down the unit edit dialog procedure:
- **`InitializeUnitEditDialog(HWND, BackupUnit*)`** - Dialog initialization
- **`SaveUnitFromDialog(HWND, BackupUnit*)`** - Save dialog values to unit
- **`BrowseFolderForUnit(HWND, bool isSource)`** - Folder browsing logic

##### 2.3 Extract Helper Functions
- **`CreateMenuBar(HWND)`** - Menu creation logic
- **`CreateTreeView(HWND, int id, int x, int y, int w, int h)`** - TreeView factory
- **`CreateWorkerControls(HWND)`** - Worker UI creation
- **`UpdateTreeViewIcons(HTREEITEM, bool expanded)`** - Icon updates

##### 2.4 Organize Into Separate Files (Optional but Recommended)
- `ui_helpers.cpp/h` - UI creation and layout helpers
- `tree_view_handlers.cpp/h` - TreeView event handling
- `dialog_procedures.cpp/h` - Dialog procedures
- `menu_handlers.cpp/h` - Menu command handlers

#### Phase 3: Add Professional Testing (User's Earlier Request)
Once code is refactored and understandable:
- [ ] Set up Google Test framework
- [ ] Unit tests for Copy/Sync/Verify engines
- [ ] Integration tests for BackupSet/BackupUnit manipulation
- [ ] Mock file system for safe testing
- [ ] Edge case and error handling tests

### 🐛 Known Warnings to Address:
- `IDC_RAD_MODE_VERIFY` macro redefined (conflicting definitions)
- Unused header includes (`atomic`, `filesystem`, `memory`)
- Cast warnings for HMENU conversions
- `hProgressBar` variable shadowing
- ODR violation for `Localization::s_strings` (defined in header)

### 📝 Code Quality Improvements Identified:
1. `std::filesystem` errors in includes (need proper namespace)
2. Duplicate `nu.target = L"D:\\";` assignment (line 977-978)
3. Large switch statements could use jump tables
4. Magic numbers should be named constants
5. Some functions exceed 100 lines (maintainability threshold)

---

## Recommendation:
**Prioritize Phase 1 (fix linker) → Phase 2 (refactor) → Phase 3 (test)**

The refactoring in Phase 2 will make the codebase significantly more maintainable and understandable, which aligns with your request to "arrange too big functions to make them understandable."
