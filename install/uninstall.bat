@echo off
setlocal

:: ============================================================
::  PAF Extractor -- Uninstaller
::  Requires administrator privileges.
:: ============================================================

echo.
echo  PAF Extractor - Uninstaller
echo  =============================
echo.

:: --- Check admin ---
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo  [!] Administrator privileges required.
    echo      Right-click this file and choose "Run as administrator".
    echo.
    pause
    exit /b 1
)

set "INSTALL_DIR=%ProgramFiles%\PAF"

:: --- Remove context menu and file type ---
echo  Removing .paf file association...
reg delete "HKCR\.paf"       /f >nul 2>&1
reg delete "HKCR\PAFArchive" /f >nul 2>&1
echo  Done.
echo.

:: --- Remove installed files ---
echo  Removing files from %INSTALL_DIR%...
if exist "%INSTALL_DIR%" (
    del /Q "%INSTALL_DIR%\paf_extract.exe"  >nul 2>&1
    del /Q "%INSTALL_DIR%\libpaf.dll"       >nul 2>&1
    del /Q "%INSTALL_DIR%\dstorage.dll"     >nul 2>&1
    del /Q "%INSTALL_DIR%\dstoragecore.dll" >nul 2>&1
    rmdir  "%INSTALL_DIR%"                  >nul 2>&1
    echo  Done.
) else (
    echo  (Nothing to remove -- folder not found)
)
echo.

:: --- Refresh Explorer ---
ie4uinit.exe -show >nul 2>&1

echo  =============================================
echo   PAF Extractor has been uninstalled.
echo  =============================================
echo.
pause
endlocal
