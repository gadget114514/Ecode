# Software Requirements Specification: SureBackup

## 1. Introduction
SureBackup is a native Win32 application designed for robust folder copying and synchronization. It allows users to define, organize, and execute backup tasks with support for advanced NTFS features and high-fidelity file verification.

## 2. Terminology
*   **Backup Unit**: The fundamental unit of operation, consisting of a specific Source Folder and a Target Folder, along with processing rules (Copy vs Sync, Verification).
*   **Backup Set**: A grouped collection of Backup Units. This allows for batch processing of multiple folder pairs.

## 3. Functional Requirements

### 3.1. File Operations
*   **Recursive Processing**: Operations are performed recursively on directory structures.
*   **Modes**:
    *   **Copy Mode (Append)**: Copies new or updated files from Source to Target.
    *   **Sync Mode (Mirror)**: Perfectly mirrors the Source to the Target, including deleting orphaned files in the target.
    *   **Verify Only (Preview/Simulation)**: Performs a read-only pass to validate data integrity without modifying the target file system. List updates in real-time but no IO occurs.
*   **Performance Engines**:
    *   **Standard Engine**: Reliable sequential processing for maximum compatibility.
    *   **Parallel Engine**: High-performance multithreaded engine for large-scale synchronization.
    *   **Verification Engine**: Deep-dive comparison engine using a producer-consumer model for bit-perfect integrity checks.
    *   **Block Clone Engine (Experimental)**: Uses fallback to Standard Engine with logging. Planned for future delta-sync support.
    *   **VSS Engine (Experimental)**: Uses fallback to Standard Engine; requires administrative privileges for Volume Shadow Copy service.
    *   **NTFS Alternate Data Streams (ADS)**: Native support for preserving ADS during copy operations.
    *   **Symbolic Links**: Correctly handles and recreates symbolic links (files and directories) instead of copying the linked content.
*   **High-Fidelity Verification**: 
    *   **Byte-by-Byte Comparison**: Optional post-copy verification using 16KB blocks to ensure bit-perfect data integrity.
    *   **Metadata Preservation**: Strictly preserves **File Modification** times and **Creation Dates**, as well as **Directory Timestamps**, ensuring the target filesystem is an exact mirror of the source identity.
*   **Enhanced Preview/Simulation**:
    *   Simulate operations without modifying the file system.
    *   **Verbose Feedback**: Logs directory scans, identical files, and planned actions (Copy, Link, Delete).
    *   **Full Path Logging**: Displays complete source and target paths for every action.
*   **Operational Control**:
    *   **User Interruption**: Stop any running backup or simulation immediately via the "STOP" button.
    *   **Error Policy**: Configurable behavior per Backup Unit when a file error occurs:
        *   **Continue**: Skip the error and move to the next file.
        *   **Suspend**: Halt the specific task for manual intervention.
*   **System Integrity**:
    *   **Sleep Prevention**: Automatically prevents the computer from entering sleep or suspend mode while a backup is active.

### 3.2. Data Management
*   **Storage**: 
    *   Backup Sets are persisted as configuration files in `Documents\SureBackup`.
*   **Management Utilities**:
    *   **Duplication**: Ability to duplicate entire Sets or individual Units to quickly create templates.
    *   **Organization**: Rename, reorder, and delete units within sets.
*   **Path Configuration**:
    *   Configure paths via manual input, folder browse dialogs, or direct selection from the integrated directory explorers.
*   **Scheduling**:
    *   **Automated Runs**: Configure Backup Sets to run on a **Daily** or **Weekly** schedule.
    *   **Time-Specific**: Precision scheduling down to the hour and minute.
    *   **Persistence**: Tracks last-run timestamps to ensure reliability and prevent redundant executions.

## 4. User Interface

### 4.1. Main Window Layout
*   **Management Tree (Left)**: Hierarchical view of Backup Sets and Units.
*   **Discovery Explorers (Middle/Right)**: Integrated file system trees for Source and Target drives.
    *   **Automatic Syncing**: Selecting a Backup Unit automatically navigates the explorers to the corresponding folders.
    *   **Path Indicators**: Full-path labels above explorers with interactive **Tooltips** for truncated paths.
    *   **Contextual Configuration**: Right-click folders in explorers to "Set as Source" or "Set as Target".
*   **Execution Log (Bottom)**: Real-time, scrollable log pane providing detailed operational feedback.
    *   **Auto-Clear**: The log is automatically cleared at the start of each new operation.
    *   **Maximize Feature**: Toggleable "Maximize Log" mode that expands the log to full-window height for deep review.
    *   **Real-time Progression**: Dedicated indicator showing the exact file path currently being processed.
    *   **Parallel Progress Bars**: Individual progress tracking for worker threads during multi-threaded operations.
    *   **Fidelity Status Logging**: Explicitly logs every action with a clear outcome:
        *   **SUCCESS**: Successful copy or verification.
        *   **FAIL**: Detail of the specific IO error.
        *   **DELETED**: Confirmation of orphan removal during Sync.
    *   **History Logs**: Automatically saves session logs as `.txt` files named with the task title and completion timestamp in `Documents\SureBackup`.

### 4.2. Menus & Navigation
*   **Menu Bar**:
    *   **File**: Exit.
    *   **Set**: Create new sets.
    *   **Run**: Execute backups or run simulations.
    *   **Help**: "About" dialog detailing application version and capabilities.
*   **Context Menus**:
    *   **Management Tree**: Run, Preview, Verify, Edit (Set/Unit), Duplicate, Delete.
    *   **Directory Trees**: Set as Source/Target (context-aware: disabled when a Set is selected).

### 4.3. Dialogs
*   **Unit Editor**: Professional dialog for defining unit name, paths, mode (Copy/Sync/Verify).
    *   **Engine Selection**: Dropdown list to select between Standard, Parallel, Block Clone (Experimental), VSS (Experimental), and Comparison strategies.
    *   **Verification Criteria**: Checkboxes to define what constitutes a "change" (Size, Timestamp, Content).
*   **Set Editor**: Manage the order and names of units within a set via modal dialogue.
*   **Result Notifications**: Post-execution message boxes indicating session summary (Success or Warning with Error count).

## 5. Technical Constraints
*   **Platform**: Native Windows (Win32 API / C++17).
*   **Reliability**: Utilizes native `CopyFileExW` and `SetFileTime` for high-fidelity attribute preservation.
*   **Performance**: 
    *   **Worker Pool**: Producer-consumer model for IO-parallelized operations.
    *   **Lazy Loading**: Deferred loading of directory trees for efficiency.
*   **Localization**: Fully localized core interface supporting English, Japanese, French, German, and Spanish.

## 6. Final Project Summary
*   **Native Precision**: Built entirely with the Win32 API for maximum performance and minimal footprint. Supports advanced NTFS features like ADS and Symlinks.
*   **Parallel Power**: Multithreaded engine with thread-safe, real-time aggregate progress reporting and worker-level dashboard.
*   **Intuitive UI**: A refactored, split-pane layout with quick-edit combos for Mode and Strategy, plus a comprehensive log view.
*   **Globalized**: Fully localized core interface supporting English, Japanese, French, German, and Spanish.
*   **Reliable Scheduling**: Built-in daily/weekly scheduler with persistent configuration and last-run tracking.
