@echo off
setlocal
REM projfs\umount.bat -- Clean up a ProjFS mount point directory
REM
REM Usage:
REM   projfs\umount.bat <mount_dir>
REM
REM This script removes placeholder/reparse-point state left by ProjFS after
REM paf_mount.exe has exited (or was killed).  It deletes every file and
REM sub-directory inside mount_dir so that the directory can be reused or
REM removed cleanly.
REM
REM NOTE: If paf_mount.exe is still running, stop it first (press Enter in its
REM       window) before running this script.

REM ── Move to repo root so relative paths work ────────────────────────────────
cd /d "%~dp0.."

REM ── Validate arguments ───────────────────────────────────────────────────────
if "%~1"=="" (
    echo Usage: projfs\umount.bat ^<mount_dir^>
    echo.
    echo   mount_dir   Directory that was used as the ProjFS mount point.
    exit /b 1
)

set "MOUNTDIR=%~1"

if not exist "%MOUNTDIR%" (
    echo INFO: Directory not found, nothing to clean up: %MOUNTDIR%
    exit /b 0
)

REM ── Warn if paf_mount.exe is still running ───────────────────────────────────
tasklist /FI "IMAGENAME eq paf_mount.exe" 2>nul | find /I "paf_mount.exe" >nul
if %ERRORLEVEL% equ 0 (
    echo WARNING: paf_mount.exe appears to be running.
    echo          Press Enter in the paf_mount window first, then re-run this script.
    echo          Proceeding anyway -- some files may be locked.
    echo.
)

REM ── Remove all placeholder files created by ProjFS ───────────────────────────
REM  /S  recurse into subdirectories
REM  /Q  quiet (no prompts)
REM  /F  force-delete read-only files (ProjFS marks placeholders read-only)
echo Cleaning up ProjFS placeholders in: %MOUNTDIR%
rd /S /Q "%MOUNTDIR%" 2>nul

REM If rd failed (e.g. directory is still open), try a targeted attrib + del
if exist "%MOUNTDIR%" (
    echo Forcing attribute reset and retry ...
    attrib -R -H -S "%MOUNTDIR%\*" /S /D >nul 2>&1
    rd /S /Q "%MOUNTDIR%" 2>nul
)

if exist "%MOUNTDIR%" (
    echo WARNING: Could not fully remove %MOUNTDIR%.
    echo          Some files may still be open.  Close all handles and retry.
    exit /b 1
)

echo Done. Mount point removed: %MOUNTDIR%
echo.
echo You can now re-mount the archive:
echo   projfs\mount.bat archive.paf %MOUNTDIR%
endlocal
