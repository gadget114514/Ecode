@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl /std:c++17 /EHsc /utf-8 /DUNICODE /D_UNICODE /Iinclude tests/test_cursor_save.cpp src/TerminalBuffer.cpp src/TerminalEmulator.cpp user32.lib gdi32.lib shell32.lib ole32.lib d2d1.lib dwrite.lib /Fe:test_cursor_save.exe
if %ERRORLEVEL% EQU 0 (
    test_cursor_save.exe
)
