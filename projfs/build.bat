@echo off
setlocal
REM projfs\build.bat -- Build paf_mount.exe (ProjFS virtual mount provider)
REM
REM Usage (from repository root, or from projfs\ folder):
REM   projfs\build.bat
REM
REM Requires:
REM   - MSVC (cl.exe) in PATH (run from a VS Developer Command Prompt)
REM   - Windows SDK with ProjectedFSLib.lib  (Windows 10 SDK 17763+)
REM   - lib\libpaf.lib already built (see build_gpu.bat or build_paf_gpu.ps1)
REM
REM Output: bin\paf_mount.exe

REM ── Move to repo root regardless of where the script was called from ────────
cd /d "%~dp0.."

echo === PAF ProjFS Mount Build ===
echo.

REM ── Check for MSVC ──────────────────────────────────────────────────────────
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe not found.
    echo        Run this script from a Visual Studio Developer Command Prompt.
    echo        e.g. "x64 Native Tools Command Prompt for VS 2022"
    exit /b 1
)

REM ── Check for libpaf.lib ────────────────────────────────────────────────────
if not exist lib\libpaf.lib (
    echo ERROR: lib\libpaf.lib not found.
    echo        Build the core library first:
    echo          build_gpu.bat          ^(CPU-only^)
    echo          .\build_paf_gpu.ps1    ^(full GPU^)
    exit /b 1
)

REM ── Locate ProjectedFSLib.lib from the Windows SDK ──────────────────────────
REM The environment variable WindowsSdkDir is set by vcvars*.bat.
REM We search the two most common SDK versions; override PROJFS_LIB if needed.
set "PROJFS_LIB="

if not "%WindowsSdkDir%"=="" (
    for /d %%V in ("%WindowsSdkDir%Lib\*") do (
        if exist "%%V\um\x64\ProjectedFSLib.lib" (
            set "PROJFS_LIB=%%V\um\x64\ProjectedFSLib.lib"
        )
    )
)

REM Fallback: check whether the compiler can find it without an explicit path
if "%PROJFS_LIB%"=="" (
    echo WARNING: Could not locate ProjectedFSLib.lib automatically.
    echo          Relying on the linker search path.
    set "PROJFS_LIB=ProjectedFSLib.lib"
)

echo Using: %PROJFS_LIB%
echo.

REM ── Create output directory ──────────────────────────────────────────────────
if not exist bin mkdir bin

REM ── Compile ──────────────────────────────────────────────────────────────────
echo Building paf_mount.exe ...
cl /O2 /W3 /nologo ^
    /Ilibpaf\include ^
    projfs\paf_mount.c ^
    lib\libpaf.lib ^
    "%PROJFS_LIB%" ^
    ole32.lib ^
    /Fe:bin\paf_mount.exe

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build FAILED.
    exit /b 1
)

echo.
echo Build succeeded: bin\paf_mount.exe
echo.
echo Usage:
echo   projfs\mount.bat  archive.paf [mount_dir]
echo   projfs\umount.bat mount_dir
endlocal
