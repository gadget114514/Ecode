@echo off
echo Running PieceTable Tests...
bin\test_piecetable.exe
if %ERRORLEVEL% NEQ 0 (
    echo PieceTable Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Editor Core Tests...
bin\test_editor_core.exe
if %ERRORLEVEL% NEQ 0 (
    echo Editor Core Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running API JS Tests...
bin\ecode.exe -e "load('tests/api_test.js'); Editor.setStatusText('API JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo API JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo === ALL TESTS PASSED ===
