@echo off
REM Build script for libsixel using Visual C++
REM Run from a Visual Studio Developer Command Prompt or run:
REM   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
REM   build.bat

set BUILD_DIR=%~dp0build

cmake -S "%~dp0." -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo.
echo === Build succeeded ===
echo Library: %BUILD_DIR%\Release\libsixel.lib
echo Headers: %~dp0include\sixel.h
