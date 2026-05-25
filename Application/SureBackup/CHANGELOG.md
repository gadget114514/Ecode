# Changelog: SureBackup

All notable changes to this project during the refactoring and enhancement sessions.

## [1.0.0] - 2026-01-01

### Added
- **Parallel Engine**: Implemented `ParallelBackupStrategy` for multithreaded file operations.
- **Worker Dashboard**: Added real-time progress bars and status indicators for individual worker threads.
- **Verification Engine**: Added `ComparingBackupStrategy` for deep-dive byte-by-byte integrity checks.
- **Quick-Edit Controls**: Integrated Backup Mode and Engine Strategy selection directly into the main window toolbar.
- **Modernized Layout**: Re-calculated responsive layout with improved support for large localized fonts.
- **Localization**: Added full support for English, Japanese, French, German, and Spanish with auto-detection.
- **Scheduler**: Added daily and weekly background task scheduling.
- **NTFS Fidelity**: Support for Alternate Data Streams (ADS) and native Symbolic Link recreation.
- **Log Archiving**: Automatic persistence of session logs to `Documents\SureBackup`.

### Fixed
- **UI Clipping**: Fixed button and label clipping by expanding control widths (e.g., Stop button increased to 220px).
- **Dialog Navigation**: Fixed keyboard and radio button behavior in modal dialogs using `IsDialogMessage`.
- **Layout Persistence**: Fixed "Restore Layout" button alignment and log window overlap issues.
- **Warning Cleanup**: Resolved all MSVC compiler warnings (unused parameters, signed/unsigned comparisons).

### Changed
- **Refactored Architecture**: Decoupled UI logic from command handlers. Extracted ~10 helper functions from `WndProc`.
- **Strategy Pattern**: Core logic migrated to a modular strategy-based engine system.
- **Standardized IO**: Aliased `std::filesystem` to `fs` throughout the codebase for cleaner syntax.
- **Improved Explorer Navigation**: Added "Selection Listener" to automatically expand directory trees to matched paths.

## [Early Implementation]
- Initial port from legacy codebase.
- Basic Win32 window and tree-view setup.
- Basic `StandardBackupStrategy` implementation.
