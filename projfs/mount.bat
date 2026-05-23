@echo off
setlocal
REM projfs\mount.bat -- Mount a .paf archive as a virtual directory via ProjFS
REM
REM Usage:
REM   projfs\mount.bat <archive.paf> [mount_dir]
REM
REM   archive.paf  Path to the PAF archive to mount (required).
REM   mount_dir    Directory to use as the mount point (optional).
REM                Defaults to <archive_basename>_mount in the current folder.
REM
REM The mount stays live until you press Enter in the paf_mount.exe window,
REM or you run projfs\umount.bat <mount_dir>.
REM
REM Requirements:
REM   - bin\paf_mount.exe  (build with projfs\build.bat first)
REM   - Windows 10 version 1809 (build 17763) or later
REM   - ProjFS optional feature enabled; to enable (as Administrator):
REM       Enable-WindowsOptionalFeature -Online -FeatureName Client-ProjFS -NoRestart
REM     or:
REM       dism /online /enable-feature /featurename:Client-ProjFS

REM ── Move to repo root so relative paths work ────────────────────────────────
cd /d "%~dp0.."

REM ── Validate arguments ───────────────────────────────────────────────────────
if "%~1"=="" (
    echo Usage: projfs\mount.bat ^<archive.paf^> [mount_dir]
    echo.
    echo   archive.paf   Path to the .paf archive to mount.
    echo   mount_dir     Mount point directory ^(default: ^<name^>_mount^).
    exit /b 1
)

REM ── Resolve archive path to absolute ────────────────────────────────────────
set "ARCHIVE=%~f1"
if not exist "%ARCHIVE%" (
    echo ERROR: Archive not found: %ARCHIVE%
    exit /b 1
)

REM ── Determine mount directory ────────────────────────────────────────────────
if not "%~2"=="" (
    set "MOUNTDIR=%~2"
) else (
    set "MOUNTDIR=%~n1_mount"
)

REM ── Check that paf_mount.exe exists ─────────────────────────────────────────
if not exist bin\paf_mount.exe (
    echo ERROR: bin\paf_mount.exe not found.
    echo        Run projfs\build.bat to build it first.
    exit /b 1
)

REM ── Create mount directory if it does not exist ──────────────────────────────
if not exist "%MOUNTDIR%" mkdir "%MOUNTDIR%"

REM ── Launch provider ──────────────────────────────────────────────────────────
echo Mounting %ARCHIVE%
echo      at  %MOUNTDIR%
echo.
echo Press Enter in this window (or the paf_mount window) to unmount.
echo.

bin\paf_mount.exe "%ARCHIVE%" "%MOUNTDIR%"

echo.
echo Archive unmounted.
endlocal
