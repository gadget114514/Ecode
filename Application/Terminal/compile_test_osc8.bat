@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc "%~dp0test_osc8.cpp" /Fe:"%~dp0test_osc8.exe"
if %errorlevel% equ 0 (
    echo.
    echo Build successful: %~dp0test_osc8.exe
    echo Run it in the ecode terminal to test OSC 8 hyperlinks.
)
