# Ecode Folders & Files

## Settings

| Path | Description |
|------|-------------|
| `%APPDATA%\Ecode\settings.ini` | Main configuration file (window state, font, themes, AI config, etc.) |
| `%APPDATA%\Ecode\ecodeinit.js` | User init script loaded on startup |
| `%APPDATA%\Ecode\recent.ini` | Recently opened files list |
| `%APPDATA%\Ecode\macros\*.js` | User-recorded macros |
| `%APPDATA%\Ecode\*.jsb` | Bytecode cache for scripts |

On Windows, `%APPDATA%` resolves to `C:\Users\<username>\AppData\Roaming`.

## Binaries

| Path | Description |
|------|-------------|
| `bin\Release\ecode.exe` | Main editor (Release build) |
| `bin\Debug\ecode.exe` | Main editor (Debug build) |
| `bin\Release\ecode_console.exe` | Console-based variant |
| `bin\EcodeSetup.exe` | Inno Setup installer |

## Source

| Path | Description |
|------|-------------|
| `src\` | Core editor source code |
| `include\` | Header files |
| `Application\` | Plugin projects (Dired, CSVEditor, FastFileSearch, FastFD, Terminal, JYEditor) |

## Build

| Path | Description |
|------|-------------|
| `build\` | CMake build output for ecode |
| `Application\*\build\` | Per-plugin CMake build output |

## Other

| Path | Description |
|------|-------------|
| `doc\` | Documentation |
| `scripts\` | Built-in ecode scripts |
| `tests\` | Test source files |
| `cmake\` | CMake helper modules |
| `installer\` | Inno Setup script (`ecode.iss`) |
