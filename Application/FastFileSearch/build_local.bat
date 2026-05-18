@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

if not exist build_local mkdir build_local
cd build_local

cmake .. -G "NMake Makefiles"
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

nmake
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo Build Successful!
