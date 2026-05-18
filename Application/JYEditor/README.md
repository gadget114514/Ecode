# JYEditor

A native Win32 JSON and YAML editor with tabbed interface.

## Features

- **Multi-tab editing**: Open and edit multiple files simultaneously.
- **JSON & YAML Support**: 
  - Syntax highlighting (basic text for now, but specialized formatting).
  - **Auto-Formatting/Validation**: Format JSON and YAML content via the "Format" menu.
- **Encoding Support**: Full UTF-8 read/write support.
- **Line Endings**: View and change line endings (CRLF, LF, CR).
- **Persistence**: Remembers open files and settings across sessions.
- **Native Performance**: Built with C++ and Win32 API.

## Build Instructions

### Prerequisites
- CMake 3.15+
- Visual Studio 2019/2022
- vcpkg

### Build Steps

1. Clone the repository.
2. Initialize vcpkg and install dependencies:
   ```bash
   vcpkg install
   ```
3. Configure CMake:
   ```bash
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg.cmake] -DVCPKG_TARGET_TRIPLET=x64-windows
   ```
4. Build:
   ```bash
   cmake --build build --config Release
   ```

## Usage

- **File Menu**: New, Open, Save, Save As, Close Tab, Exit.
- **Format Menu**: Format current tab as JSON or YAML.
- **Line Endings**: Toggle between Windows (CRLF), Unix (LF), and Mac Legacy (CR) modes.
