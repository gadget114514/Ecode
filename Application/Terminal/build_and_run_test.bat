@echo off
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /Iinclude tests/test_cursor_save.cpp src/TerminalBuffer.cpp src/TerminalEmulator.cpp user32.lib gdi32.lib shell32.lib ole32.lib d2d1.lib dwrite.lib /Fe:test_cursor_save.exe
if %ERRORLEVEL% EQU 0 (
    test_cursor_save.exe
)
echo.
cl /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /Iinclude tests/test_cursor_move.cpp src/TerminalBuffer.cpp src/TerminalEmulator.cpp user32.lib gdi32.lib shell32.lib ole32.lib d2d1.lib dwrite.lib /Fe:test_cursor_move.exe
if %ERRORLEVEL% EQU 0 (
    test_cursor_move.exe
)
echo.
cl /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /Iinclude tests/test_buffer_boundary.cpp src/TerminalBuffer.cpp src/TerminalEmulator.cpp user32.lib gdi32.lib shell32.lib ole32.lib d2d1.lib dwrite.lib /Fe:test_buffer_boundary.exe
if %ERRORLEVEL% EQU 0 (
    test_buffer_boundary.exe
)
echo.
cl /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /Iinclude tests/test_esc_recovery.cpp src/TerminalBuffer.cpp src/TerminalEmulator.cpp user32.lib gdi32.lib shell32.lib ole32.lib d2d1.lib dwrite.lib /Fe:test_esc_recovery.exe
if %ERRORLEVEL% EQU 0 (
    test_esc_recovery.exe
)
echo.
cl /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /Iinclude tests/test_backspace_wide.cpp src/TerminalBuffer.cpp src/TerminalEmulator.cpp user32.lib gdi32.lib shell32.lib ole32.lib d2d1.lib dwrite.lib /Fe:test_backspace_wide.exe
if %ERRORLEVEL% EQU 0 (
    test_backspace_wide.exe
)

