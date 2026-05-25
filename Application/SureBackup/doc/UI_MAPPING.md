# UI Functional Mapping: SureBackup

This document provides a direct mapping between the User Interface elements (Menus, Buttons, Context Menus) and the internal logic/functions in `src/main.cpp`.

## 1. Top Menu Bar

| Menu Path | ID | Function Called | Description |
| :--- | :--- | :--- | :--- |
| **File** > Exit | `IDM_FILE_EXIT` | `HandleCommand_FileExit` | Prompts for confirmation and terminates the application. |
| **Set** > New Backup Set | `IDM_SET_CREATE` | `OnSetCreate` | Creates a default set and adds it to the configuration. |
| **Run** > Run Backup Set | `IDM_BACKUP_RUN` | `RunBackup(RunMode::Backup)` | Starts the processing of the selected set or unit. |
| **Run** > Backup Preview | `IDM_BACKUP_PREVIEW` | `RunBackup(RunMode::Preview)` | Executes a "Dry Run" without file modifications. |
| **Run** > Verify | `IDM_BACKUP_VERIFY` | `RunBackup(RunMode::Verify)` | Runs the verification engine to audit existing backups. |
| **Help** > About | `IDM_HELP_ABOUT` | (Inline MessageBox) | Displays version info and core terminology. |

## 2. Main Window Buttons (Toolbar area)

| Button Label | ID | Function Called | Description |
| :--- | :--- | :--- | :--- |
| **STOP ALL** | `IDM_BACKUP_STOP` | `HandleCommand_BackupStop` | Signals the active engine to abort immediately. |
| **MAXIMIZE LOG** | `IDM_LOG_MAXIMIZE` | `HandleCommand_LogMaximize` | Toggles between 3-pane view and full-log view. |
| **Add Set** | `IDM_SET_CREATE` | `OnSetCreate` | Quick access to create a new set. |
| **Add Unit** | `IDM_UNIT_CREATE` | `OnUnitCreate` | Opens the creation dialogue for a new backup unit. |
| **Backup Mode Combo** | `ID_MAIN_MODE_COMBO` | `HandleCommand_MainModeChange` | Quick-select Mode (Copy/Sync/Verify) for selected Unit. |
| **Strategy Engine Combo**| `ID_MAIN_STRATEGY_COMBO`| `HandleCommand_MainStrategyChange`| Quick-select Engine (Std/Par/Blk/Vss/Cmp) for selected Unit. |

## 3. Management Tree (Left Pane) Context Menu

| Context Item | ID | Function Called | Applicability |
| :--- | :--- | :--- | :--- |
| **Run** | `IDM_BACKUP_RUN` | `RunBackup(RunMode::Backup)` | Selected Set or Unit. |
| **Preview** | `IDM_BACKUP_PREVIEW` | `RunBackup(RunMode::Preview)` | Selected Set or Unit. |
| **Verify** | `IDM_BACKUP_VERIFY` | `RunBackup(RunMode::Verify)` | Selected Set or Unit. |
| **Edit Backup Set** | `IDM_SET_UPDATE` | `OnSetUpdate` | Right-clicked Set node. |
| **Edit Backup Unit** | `IDM_UNIT_UPDATE` | `OnUnitUpdate` | Right-clicked Unit node. |
| **Add Unit** | `IDM_UNIT_CREATE` | `OnUnitCreate` | Inside a Set node. |
| **Duplicate** | `IDM_SET_DUPLICATE` / `IDM_UNIT_DUPLICATE` | `HandleCommand_SetDuplicate` / `HandleCommand_UnitDuplicate` | Selected item. |
| **Delete** | `IDM_SET_DELETE` / `IDM_UNIT_DELETE` | `HandleCommand_SetDelete` / `HandleCommand_UnitDelete` | Selected item. |

## 4. Directory Explorers (Center/Right Panes) Context Menu

| Context Item | ID | Function Called | Description |
| :--- | :--- | :--- | :--- |
| **Set as Source** | `IDM_SET_AS_SOURCE` | `HandleCommand_SetAsSource` | Assigns right-clicked folder in Middle pane to selected Unit. |
| **Set as Target** | `IDM_SET_AS_TARGET` | `HandleCommand_SetAsTarget` | Assigns right-clicked folder in Right pane to selected Unit. |

## 5. Parallel Worker Dashboard (Maximized View)

| Component | ID | Function Called | Description |
| :--- | :--- | :--- | :--- |
| **Worker Stop (X)** | `ID_WORKER_BASE_BTN + i` | `HandleCommand_WorkerStop` | Cancels a specific thread in the Parallel engine. |

## 6. Background Engine

| Trigger | Logical Link | Function Called | Description |
| :--- | :--- | :--- | :--- |
| **Timer (1 min)** | `WM_TIMER` (2001) | `CheckSchedules` | Evaluates if any Set is due for a daily/weekly run. |
