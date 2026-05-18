# Software Architecture

## Overview
JYEditor is a lightweight, native Windows (Win32 API) text editor specialized for JSON and YAML formats. It features a dual-pane interface with a raw text editor and a hierarchical tree view helper.

## Core Components

### 1. Application Entry (`main.cpp`)
- Initializes the Windows application.
- Creates the main `EditorWindow`.
- Runs the main message loop.

### 2. Main Window (`EditorWindow` class)
This is the core controller of the application.
- **Responsibility**: Manages the main window lifecycle, message handling (WndProc), and UI layout.
- **UI Structure**:
  - **Tree View (`m_hTreeView`)**: Displays the hierarchical structure of key-value pairs.
  - **Tab Control (`m_hTabCtrl`)**: Manages multiple open documents.
  - **Tab Handling**: Each tab maps to a `Document` struct.
- **Features**:
  - File I/O (Read/Write with UTF-8 support).
  - Data Parsing (JSON via `nlohmann/json`, YAML via `yaml-cpp`).
  - formatting/Pretty-printing.

### 3. Data Model (`Document` struct)
Each open file is represented by a `Document` structure:
- `HWND hEdit`: Handle to the source code edit control (Win32 Edit Control).
- `filePath`: Absolute path to the file.
- `jsonData`: Internal `nlohmann::json` object representing the parsed data.
- `format`: Enum indicating if the file is Text, JSON, or YAML.

### 4. File Utilities (`FileUtils` class)
- Helper static methods for handling file reading and writing.
- Handles text encoding conversions (WideChar <-> MultiByte/UTF-8).
- Detects Line Endings (CRLF, LF, CR).

## Data Flow
1. **Loading**: File -> `FileUtils::ReadFileUtf8` -> Edit Control Text.
2. **Parsing**: Edit Control Text -> `nlohmann::json` or `YAML::Node` -> `EditorWindow::UpdateTree`.
3. **Editing**: User edits text -> Parsing triggers on request -> Tree updates.
4. **Saving**: Edit Control Text -> `FileUtils::WriteFileUtf8` -> Disk.

## External Dependencies
- **nlohmann-json**: For parsing and manipulating JSON data.
- **yaml-cpp**: For parsing and generating YAML data.

## Build System
- **CMake**: Manages build configuration.
- **vcpkg**: Packet manager for dependencies (json, yaml-cpp).
