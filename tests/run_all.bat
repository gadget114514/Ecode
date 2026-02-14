@echo off
echo Running PieceTable Tests...
bin\Debug\test_piecetable.exe
if %ERRORLEVEL% NEQ 0 (
    echo PieceTable Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Editor Core Tests...
bin\Debug\test_editor_core.exe
if %ERRORLEVEL% NEQ 0 (
    echo Editor Core Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running File IO Tests...
bin\Debug\test_file_io.exe
if %ERRORLEVEL% NEQ 0 (
    echo File IO Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Visual Wrap Tests...
bin\Debug\test_visual_wrap.exe
if %ERRORLEVEL% NEQ 0 (
    echo Visual Wrap Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running API JS Tests...
bin\Debug\ecode.exe -headless -e "load('tests/api_test.js'); Editor.setStatusText('API JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo API JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Undo/Redo JS Tests...
bin\Debug\ecode.exe -headless -e "load('tests/undoredo_test.js'); Editor.setStatusText('Undo/Redo JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Undo/Redo JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Undo Limit JS Tests...
bin\Debug\ecode.exe -headless -e "load('tests/undostatelimit_test.js'); Editor.setStatusText('Undo Limit JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Undo Limit JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Total Lines JS Tests...
bin\Debug\ecode.exe -headless -e "load('tests/total_lines_test.js'); Editor.setStatusText('Total Lines JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Total Lines JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Multiple Operations JS Tests...
bin\Debug\ecode.exe -headless -e "load('tests/multiple_ops_test.js'); Editor.setStatusText('Multiple Ops JS Tests Completed')"
if %ERRORLEVEL% NEQ 0 (
    echo Multiple Ops JS Tests FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running PieceTable Stress Test...
bin\Debug\test_piecetable_stress.exe
if %ERRORLEVEL% NEQ 0 (
    echo PieceTable Stress Test FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Undo/Redo Stress Test...
bin\Debug\test_undoredo_stress.exe
if %ERRORLEVEL% NEQ 0 (
    echo Undo/Redo Stress Test FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo Running Search/Replace Stress Test...
bin\Debug\test_search_replace.exe
if %ERRORLEVEL% NEQ 0 (
    echo Search/Replace Stress Test FAILED
    exit /b %ERRORLEVEL%
)

echo.
echo === ALL TESTS PASSED ===
