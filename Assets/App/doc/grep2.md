# Plan: Find in Files History

## What we want
1. Save the entire search condition (pattern, directory, ext filter, regex, match case, show current, verbose) as a single history entry when "Find" is clicked
2. Select from a list of past conditions in the dialog
3. Persist history across sessions (save/restore from `settings.ini`)

## Files to modify

| File | Change |
|------|--------|
| `include/SettingsManager.h` | Add `FindInFilesCondition` struct, `m_findHistory` vector, getter/add/remove/save methods |
| `src/SettingsManager.cpp` | Implement load/save from `[FindHistory]` INI section, `AddFindHistory()` dedup logic |
| `include/resource.h` | Add `IDC_FIND_HISTORY` resource ID |
| `src/Ecode.rc` | Add a combobox at the top of the dialog + expand dialog size |
| `src/Dialogs.cpp` | Populate history on init, handle selection (fills all fields), save condition on Find |

## Data structure

```cpp
struct FindInFilesCondition {
    std::wstring pattern;
    std::wstring directory;
    std::wstring extFilter;
    bool useRegex = false;
    bool matchCase = false;
    bool showCurrentFile = true;
    bool verbose = false;
};
```

Stored in `SettingsManager::m_findHistory` (max 20 entries, dedup by all 7 fields matching).

## Persistence format

In `settings.ini` under `[FindHistory]`:
```ini
[FindHistory]
Count=3
Entry0_Pattern=foo
Entry0_Dir=C:\project
Entry0_Ext=.cpp;.h
Entry0_Regex=0
Entry0_MatchCase=1
Entry0_ShowCurrent=1
Entry0_Verbose=0
Entry1_...
```

## Dialog UI changes

- Widen the dialog from 250 to 370, height from 145 to 190
- Add a labeled combobox (dropdown) at top: shows saved conditions as `pattern @ dir (ext)`
- Selecting an entry fills all fields (pattern, dir, ext, checkboxes)

## Key implementation details

- **`AddFindHistory()`** — dedup by comparing all 7 fields; insert at front; cap at 20; call `SaveFindHistory()`
- **`Save()`** — also calls `SaveFindHistory()` so it's saved during normal settings save
- **Dialog `WM_INITDIALOG`** — populate combobox
- **Combobox `CBN_SELCHANGE`** — fill all fields from the selected condition
- **`IDOK` handler** — after starting grep, call `AddFindHistory()` with current condition
