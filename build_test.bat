@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
cl tests/test_esc.c /Fe:test_esc.exe 2>&1
