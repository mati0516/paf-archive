@echo off
REM build_cli.bat -- Build paf CLI tool on Windows (MSVC)
REM
REM Usage (from repository root):
REM   tools\build_cli.bat
REM
REM Requires:
REM   - MSVC (cl.exe) available in PATH (run from a VS Developer Command Prompt)
REM   - lib\libpaf.lib already built (see build_gpu.bat or build_paf_gpu.ps1)
REM
REM Output: bin\paf.exe

setlocal

if not exist bin mkdir bin

echo Building paf CLI -^> bin\paf.exe

cl /O2 /W3 /nologo ^
    /I"libpaf\include" ^
    "tools\paf_cli.c" "libpaf\src\sha256.c" ^
    "lib\libpaf.lib" ^
    /Fe:"bin\paf.exe"

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b 1
)

echo Build successful: bin\paf.exe
endlocal
