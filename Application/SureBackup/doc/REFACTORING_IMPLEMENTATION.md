# Main.cpp Refactoring Implementation Plan

## Objective
Break down large functions into smaller, understandable pieces while **preserving 100% of functionality**.

## Current Status
- ✅ Build successful
- ✅ All features working
- Target: Make code more maintainable without breaking anything

---

## Phase 1: Extract Helper Functions from WndProc (Lines 1140-1630)

### 1.1 Command Handler Extraction
**Safe to extract** - These are self-contained blocks:

```cpp
// Menu Commands
void HandleCommand_FileExit(HWND hWnd);
void HandleCommand_LogMaximize(HWND hWnd);
void HandleCommand_BackupRun(bool dryRun);
void HandleCommand_BackupStop();
void HandleCommand_SetCreate(HWND hWnd);
void HandleCommand_SetUpdate(HWND hWnd);
void HandleCommand_SetDelete(HWND hWnd);
void HandleCommand_SetDuplicate();
void HandleCommand_UnitCreate(HWND hWnd);
void HandleCommand_UnitUpdate(HWND hWnd);
void HandleCommand_UnitDelete();
void HandleCommand_UnitDuplicate();
void HandleCommand_SetAsSource(HWND hWnd);
void HandleCommand_SetAsTarget(HWND hWnd);
void HandleCommand_WorkerStop(int workerIndex);
```

### 1.2 TreeView Event Handlers
**Safe to extract** - Well-defined event handling:

```cpp  
void HandleTreeViewSelection(LPNMHDR pnmhdr);
void HandleTreeViewRightClick(HWND hTreeView, LPNMHDR pnmhdr);
void HandleTreeViewExpansion(HWND hTreeView, LPNMTREEVIEWW pnmtv);
```

### 1.3 Window Layout Handlers
**Safe to extract** - Pure UI layout:

```cpp
void ResizeMainWindow_Maximized(int width, int height);
void ResizeMainWindow_Normal(int width, int height);
```

---

## Phase 2: Refactor UnitEditDlgProc (Lines 548-746)

### 2.1 Dialog Initialization
```cpp
void InitializeUnitEditDialog(HWND hWnd, BackupUnit* pUnit);
```

### 2.2 Data Extraction
```cpp
bool SaveUnitFromDialog(HWND hWnd, BackupUnit* pUnit);
```

### 2.3 Event Handlers
```cpp
void HandleUnitDialog_BrowseSource(HWND hWnd, BackupUnit* pUnit);
void HandleUnitDialog_BrowseTarget(HWND hWnd, BackupUnit* pUnit);
void HandleUnitDialog_NetworkMap(HWND hWnd);
```

---

## Phase 3: Refactor SetEditDlgProc (Lines 748-909)

Similar pattern to UnitEditDlgProc

---

## Implementation Strategy

### Rule 1: One Function at a Time
- Extract ONE helper function
- Build and test
- Verify no functionality lost
- Commit the change
- Move to next function

### Rule 2: Preserve Exact Behavior
- Copy exact code, don't "improve" yet
- Maintain all variable access patterns
- Keep error handling identical
- Preserve all side effects

### Rule 3: Clear Naming
- Function names describe WHAT they do
- Parameters make dependencies explicit
- Return values indicate success/failure

### Rule 4: No Premature Optimization  
- Don't change algorithms
- Don't change data structures
- Focus ONLY on organization

---

## Testing Checklist (After Each Extraction)

- [ ] Project builds without errors
- [ ] Project builds without warnings
- [ ] Application launches
- [ ] Main window displays correctly
- [ ] TreeView populates
- [ ] Menu commands work
- [ ] Dialogs open and save correctly
- [ ] Backup operations function

---

## Starting Point

**First extraction:** `HandleCommand_FileExit()`
- Simplest possible extraction
- Single line of code
- No dependencies
- Can't break anything

**Progress from there:**
- Work from simple → complex
- Build confidence with easy extractions
- Tackle harder ones with proven pattern

---

## Success Criteria

✅ All existing functionality preserved  
✅ Code is more readable  
✅ Functions have single responsibilities  
✅ Easier to understand flow  
✅ No new bugs introduced

Let's begin!
