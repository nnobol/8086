@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set OUT_DIR=output
set PROGRAM=%OUT_DIR%\my-program

cl.exe -nologo -Fe"%PROGRAM%.exe" /W4 /WX parser_test.c parser.c tokenizer.c >"%OUT_DIR%\compile.log" 2>&1
if errorlevel 1 (
    type "%OUT_DIR%\compile.log"
    del /f /q "%OUT_DIR%\compile.log"
    if exist parser.obj del /f /q parser.obj
    if exist parser_test.obj del /f /q parser_test.obj
    if exist tokenizer.obj del /f /q tokenizer.obj
    exit /b 1
)
del /f /q "%OUT_DIR%\compile.log"
if exist parser.obj del /f /q parser.obj
if exist parser_test.obj del /f /q parser_test.obj
if exist tokenizer.obj del /f /q tokenizer.obj

"%PROGRAM%.exe"
if errorlevel 1 (
    exit /b 1
)
