@echo off
echo Running PieceTable Tests...
bin\Debug\test_piecetable.exe
if %ERRORLEVEL% NEQ 0 (
    echo PieceTable Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Editor Core Tests...
..\bin\Debug\test_editor_core.exe
if %ERRORLEVEL% NEQ 0 (
    echo Editor Core Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running File IO Tests...
..\bin\Debug\test_file_io.exe
if %ERRORLEVEL% NEQ 0 (
    echo File IO Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Visual Wrap Tests...
..\bin\Debug\test_visual_wrap.exe
if %ERRORLEVEL% NEQ 0 (
    echo Visual Wrap Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running API JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/api_test.js'); Editor.setStatusText('API JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo API JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Undo/Redo JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/undoredo_test.js'); Editor.setStatusText('Undo/Redo JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Undo/Redo JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Undo Limit JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/undostatelimit_test.js'); Editor.setStatusText('Undo Limit JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Undo Limit JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Total Lines JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/total_lines_test.js'); Editor.setStatusText('Total Lines JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Total Lines JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Multiple Operations JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/multiple_ops_test.js'); Editor.setStatusText('Multiple Ops JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Multiple Ops JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running PieceTable Stress Test...
..\bin\Debug\test_piecetable_stress.exe
if %ERRORLEVEL% NEQ 0 (
    echo PieceTable Stress Test FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Undo/Redo Stress Test...
..\bin\Debug\test_undoredo_stress.exe
if %ERRORLEVEL% NEQ 0 (
    echo Undo/Redo Stress Test FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Search/Replace Stress Test...
..\bin\Debug\test_search_replace.exe
if %ERRORLEVEL% NEQ 0 (
    echo Search/Replace Stress Test FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Kill Ring JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/killring_test.js'); Editor.setStatusText('Kill Ring JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Kill Ring JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Multi-byte Character JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/multibyte_test.js'); Editor.setStatusText('Multi-byte JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Multi-byte JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Emacs Navigation JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/emacs_navigation_test.js'); Editor.setStatusText('Emacs Navigation JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Emacs Navigation JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Messages Buffer JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "load('tests/messages_buffer_test.js'); Editor.setStatusText('Messages Buffer JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Messages Buffer JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Regression 20260216 JS Tests...
..\bin\Debug\ecode_console.exe -headless -e "Editor.loadScript('regression_20260216.js');"
if %ERRORLEVEL% NEQ 0 (
    echo Regression 20260216 JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo === ALL TESTS PASSED ===
