@echo off
setlocal enabledelayedexpansion

:: 0. Setup MSVC
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

:: 1. Variables
set INPUT=input.asm
set OUT_DIR=output

set MY_OUT=%OUT_DIR%\my-out.bin
set MY_DIS=%OUT_DIR%\my-out.dis
set NASM_OUT=%OUT_DIR%\nasm-control.bin
set NASM_DIS=%OUT_DIR%\nasm-control.dis
set PROGRAM=%OUT_DIR%\my-program

:: 2. Create output directory if it doesn't exist
if not exist "%OUT_DIR%" (
    mkdir "%OUT_DIR%"
)

:: 3. Compile encoder.c with MSVC (suppress output, remove .obj)
cl.exe -nologo -Fe"%PROGRAM%.exe" encoder.c >"%OUT_DIR%\compile.log" 2>&1
if errorlevel 1 (
    type "%OUT_DIR%\compile.log"
    del /f /q "%OUT_DIR%\compile.log"
    exit /b 1
)
del /f /q "%OUT_DIR%\compile.log"
if exist encoder.obj del /f /q encoder.obj

:: 4. Assemble with my program
"%PROGRAM%.exe" "%INPUT%" "%MY_OUT%"
if errorlevel 1 (
    exit /b 1
)

:: 5. Assemble with NASM
nasm -f bin "%INPUT%" -o "%NASM_OUT%"
if errorlevel 1 (
    echo [X] NASM failed.
    exit /b 1
)

:: 6. Disassemble both
ndisasm -b 16 "%MY_OUT%" > "%MY_DIS%"
ndisasm -b 16 "%NASM_OUT%" > "%NASM_DIS%"

:: 7. Compare
fc "%NASM_DIS%" "%MY_DIS%" >nul
if errorlevel 1 (
    echo [X] Mismatch found.
    exit /b 1
) else (
    echo Output matches NASM
)
