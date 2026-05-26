# Architecture & Strategy Design: SureBackup

## Overview
SureBackup follows a **Strategy Pattern** to decouple the *Backup Engine* from specific file operation logic. The architecture is built for Windows-native performance, ensuring high fidelity for NTFS-specific features while providing a rich, multi-pane user interface.

## 1. Engine Architecture

### Strategy Pattern
The `BackupEngine` (Context) maintains an `IBackupStrategy`. This allows the application to support different backup methodologies (e.g., standard mirror vs. multi-threaded) through a unified interface.

### StandardBackupStrategy (The Core Engine)
The primary implementation mimics **Robocopy /MIR** behavior with professional-grade verification:
*   **Robust File Operations**: Uses native Win32 APIs (`CopyFileExW`, `CopyFileW`) to preserve **Alternate Data Streams (ADS)** and file attributes that standard C++ `std::filesystem` might overlook.
*   **Symbolic Link Preservation**: Implements specific logic to recreate symbolic links (files/dirs) as links on the target rather than copying their contents.
*   **Metadata Integrity**: Beyond content, the engine strictly maintains high-fidelity audit trails by preserving **File Creation/Modification times** and **Directory Timestamps**. This is achieved through a late-pass `SetFileTime` application after successful synchronization, ensuring the target mirrors the source's temporal identity perfectly.
*   **Mode-Based Execution**:
    *   **Append (Copy)**: Updates only newer or sized-changed files.
    *   **Mirror (Sync)**: Performs a two-pass operation: first updating files, then removing extraneous items in the target (SyncDelete).
*   **Dry Run Implementation**: "Dry Run" is a first-class mode within the strategy. It executes the exact same traversal and comparison logic as a real run but bypasses the physical write/delete calls, ensuring 100% simulation accuracy.

A dedicated binary comparison engine verifies data integrity using **16KB block-buffered reads**. This size is optimized for NTFS cluster alignment, providing exhaustive byte-by-byte verification without sacrificing performance.

### ParallelBackupStrategy (High-Performance Engine)
A multithreaded engine designed for speed:
*   **Producer-Consumer Model**: Uses a main thread to crawl directories and populate a thread-safe `std::deque` work queue.
*   **Worker Pool**: Spawns multiple worker threads (defaulting to 4) that consume copy tasks in parallel.
*   **Synchronization**: Uses `std::mutex` and `std::condition_variable` to coordinate tasks.
*   **Per-Worker Control**: Supports individual cancellation of worker threads, providing granular control over long-running jobs.

### ComparingBackupStrategy (Verification Engine)
A dedicated auditing engine that focuses exclusively on data integrity:
*   **Parallel Audit**: Uses a multi-threaded traversal model (similar to Parallel Engine) to calculate hashes or compare metadata at high speed.
*   **Zero-Write Guarantee**: Inherently read-only; designed to audit the target without ever modifying source or destination data.
*   **Flexible Auditing**: Automatically pivots between metadata-only checks (size/timestamp) and heavy binary validation based on task criteria.
*   **Mismatch Logging**: Provides detailed logs for "Missing Dir", "Missing File", and "Content Mismatch", allowing for thorough post-backup verification.

### Specialized Engines (Experimental)
*   **Block Clone (Delta)**: Designed for large files with small changes (e.g., VM disks). Identifies changed blocks and copies only the delta. (Currently in early implementation).
*   **VSS (Shadow Copy)**: Leverages Windows Volume Shadow Copy Service to back up locked or open files. (Requires Administrator privileges; fallback to Standard Engine implemented).

## 2. Advanced Features

### Intelligent "Uncopy" & Comparison Criteria
To optimize performance, the engine supports user-selectable comparison criteria to determine if a file is "Different":
*   **Size**: Fast check (default).
*   **Time**: Timestamp check (checks if Source is newer).
*   **Content**: Full binary comparison (slow but accurate).
Users can mix and match these criteria. If a file is deemed identical, the engine performs an "Uncopy" operation—simply updating progress counters (files/bytes processed) without performing any I/O, providing instant feedback for synced directories.

### Enhanced Progress Reporting
*   **Pre-scan Phase**: Strategies perform a recursive pre-scan to count total files and bytes before execution.
*   **Granular Feedback**: `RobustCopy` utilizes callback functions to report real-time byte transfer progress for large files.
*   **Worker Dashboard**: The UI displays status, current file, and percentage progress bars for every individual worker thread during parallel backups.
The `BackupEngine` utilizes `std::atomic<bool>` flags to support **User-Initiated Interruption**. The execution thread frequently polls this flag, allowing the "STOP" button to halt operations gracefully. Additionally, Backup Units support configurable **Error Policies** (Continue vs. Suspend), giving users control over how the engine reacts to file-level permissions or locking errors.

### Power & Scheduling Engine
- **Sleep Prevention**: The engine uses the Windows `SetThreadExecutionState` API during active backup threads to prevent the system from entering sleep or suspend mode, ensuring task completion.
- **Background Scheduler**: Implemented via a `WM_TIMER` loop that monitors Backup Set configurations for **Daily** or **Weekly** schedules, triggering automated background runs based on the last-run persistence.

## 2. Integrated Explorer Architecture

The UI is designed around a **Three-Pane Discovery Model**:
1.  **Management Tree (Left)**: Owns the configuration data (Backup Sets and Backup Units).
2.  **Source Explorer (Middle)**: A dynamic view of the local file system for source selection.
3.  **Target Explorer (Right)**: A dynamic view of the local file system for destination selection.

### Selection & Expansion Sync
The architecture implements a "Selection Listener" pattern. Selecting a Backup Unit in the Management Tree triggers an `ExpandTreeToPath` event across the explorers. This involves:
*   **Recursive Node Search**: Traversing tree handles to find path parts.
*   **On-Demand Population (Lazy Loading)**: Directory contents are only enumerated (`std::filesystem::directory_iterator`) when a node is expanded (`TVN_ITEMEXPANDING`), keeping the UI responsive even when browsing large disks.

### Interactive Tooltips
To handle deep Windows paths in a compact UI, we use **Truncation + Tooltips**:
*   Labels use `SS_ENDELLIPSIS` for visual cleanup.
*   The `TTM_ADDTOOL` system is integrated into the Tree Selection process, mapping the raw, unabbreviated `std::wstring` path to the static label's hover state in real-time.

## 3. Data Persistence & Messaging

### Configuration Manager
`ConfigManager` handles serialization of `BackupSet` and `BackupUnit` structures.
*   **Storage**: Persists to `%USERPROFILE%\Documents\SureBackup\config.txt`.
*   **Format**: A robust, line-based format that tracks naming, paths, modes, and verification flags.

### Logging & Progression System
The `IBackupLogger` interface abstracts the reporting mechanism to support multiple output channels:
*   **WindowLogger**: Implements thread-safe logging to the Win32 Edit control. It also manages a **Real-time Progress Indicator** that displays the current file path being processed.
*   **Persistent Archiving**: Every session is automatically archived as a `.txt` file in `Documents\SureBackup`, uniquely named with the task title and a completion timestamp (`YYYYMMDD_HHMMSS`).
*   **Verbosity**: Strategies are responsible for logging detailed "Identical", "Scanning", and "Action" messages to provide full transparency during both Preview and Execution.
*   **Session Health Tracking**: `WindowLogger` maintains an internal `m_hasErrors` state. By monitoring log entries for critical failure keywords, the system can determine the overall success of a complex multi-unit run.
*   **Result Dispatching**: Upon task completion, the logger signals the UI thread which evaluates the session health and presents a localized **Success** or **Warning** status message to the user, ensuring file-level errors are never overlooked in long-running jobs.

## 4. UI Interaction Design

*   **Duplication Logic**: Implements deep copying of data structures followed by a GUI refresh.
*   **Context-Aware Menus**: Context menus dynamically enable/disable items (like "Set as Source") based on the current selection state of the Management Tree (e.g., disabling folder assignment when a Set is selected instead of a Unit).
*   **Engine Selection**: The "Unit Edit" dialog features a **Dropdown Strategy Selector**, allowing users to easily choose between Standard, Multithreaded, Block Clone, and VSS engines.
*   **Memory Management**: Tree nodes use `lParam` to store `std::wstring` pointers for full paths, with a `TVN_DELETEITEM` handler ensuring zero memory leaks during navigation.

### Smart Worker Progress Visibility

The UI implements **intelligent visibility management** for worker progress indicators, optimizing screen real estate based on the current view mode:

#### Maximized Log Mode (Detailed Monitoring)
When the user activates "MAXIMIZE LOG", the interface reveals full multi-thread monitoring capabilities:
*   **4 Worker Progress Bars**: One compact row per worker thread (18px height, 4px spacing = 22px total per row)
*   **Real-time Status**: Each row displays `[Worker Label] [Progress Bar] [Stop Button]`
*   **Granular Control**: Individual "X" buttons allow cancellation of specific worker threads
*   **Positioned Above Log**: Workers are displayed between the main progress bar and the log text area
*   **Space Allocation**: 88px total height (4 workers × 22px) with log text positioned below

**Layout (Maximized):**
```
Main Progress Bar: [████████░░] 80%
├─ Worker 1: file.txt     [███░░░░] [X]  (18px)
├─ Worker 2: photo.jpg    [█████░░] [X]  (18px)
├─ Worker 3: document.pdf [██░░░░░] [X]  (18px)
└─ Worker 4: Idle         [       ] [X]  (18px)
───────────────────────────────────────
Log Text Area (remaining vertical space)
```

#### Normal Size Mode (Space Optimized)
When the window displays the standard three-pane view (Backup Sets | Source | Target), worker controls are completely hidden:
*   **Hidden Workers**: All 4 worker progress bars hidden via `ShowWindow(SW_HIDE)`
*   **Maximum Log Space**: Log text area expands to use all available vertical space
*   **Internal Tracking**: Progress tracking continues normally; only the visual display is suppressed
*   **Clean Interface**: No clutter or visual complexity in compact view mode

**Layout (Normal):**
```
┌─────────┬──────────┬──────────┐
│ Backup  │  Source  │  Target  │
│ Sets/   │   Tree   │   Tree   │
│ Units   │   View   │   View   │
└─────────┴──────────┴──────────┘
Main Progress: [████████░] 75%
─────────────────────────────────
Log Text Area
(Full available space - workers hidden)
```

#### Implementation Details
*   **Control Dimensions**: Label (250px), Progress (dynamic: W-355px), Button (55px "X")
*   **Automatic Toggle**: Mode switching handled in `WM_SIZE` message handler
*   **Position Calculation (Maximized)**: `workerY = 95 + (workerIndex × 22); logStartY = 95 + (4 × 22) + 5`
*   **No Null Checks Required**: Controls always exist but visibility toggled via `ShowWindow()`
*   **WindowLogger Integration**: Receives `OnWorkerProgress()` calls in both modes; updates controls only when visible

## 5. Built-in Localization System

SureBackup features a zero-dependency localization engine that provides native language support without external files:

*   **Language Auto-Detection**: Uses `GetUserDefaultUILanguage()` to match the system language at startup.
*   **Supported Languages**: English (Default), Japanese, Spanish, French, and German.
*   **Compile-Time Safety**: Centralized `StrId` enum manages all UI strings, ensuring consistency across languages.
*   **Wide-String Support**: Built entirely on `std::wstring` for high-fidelity Unicode display across all Windows localizations.

This system ensures a professional, localized experience for international users while maintaining the performance and simplicity of a single-binary application.

## 6. Detailed UI-to-Function Mapping

For a direct mapping of menu IDs and button controls to their respective C++ handlers, please refer to:
[UI Functional Mapping](./UI_MAPPING.md)
