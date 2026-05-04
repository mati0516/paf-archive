@echo off
setlocal EnableDelayedExpansion

:: ============================================================
::  PAF Extractor -- Windows Installer
::  Requires administrator privileges.
::
::  What this does:
::    1. Copies paf_extract.exe + DLLs to %ProgramFiles%\PAF\
::    2. Registers the .paf file extension
::    3. Adds "Extract Here" right-click menu in Explorer
:: ============================================================

echo.
echo  PAF Extractor - Installer
echo  ===========================
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

:: --- Locate bin\ relative to this script ---
set "SCRIPT_DIR=%~dp0"
set "BIN_DIR=%SCRIPT_DIR%..\bin"

:: Resolve to absolute path
pushd "%BIN_DIR%" 2>nul
if %errorlevel% neq 0 (
    echo  [!] bin\ folder not found next to install\.
    echo      Run the installer from inside the PAF release folder.
    pause
    exit /b 1
)
set "BIN_DIR=%CD%"
popd

:: --- Verify required files ---
for %%F in (paf_extract.exe libpaf.dll dstorage.dll dstoragecore.dll) do (
    if not exist "%BIN_DIR%\%%F" (
        echo  [!] Missing file: %BIN_DIR%\%%F
        pause
        exit /b 1
    )
)

:: --- Install directory ---
set "INSTALL_DIR=%ProgramFiles%\PAF"
echo  Install location: %INSTALL_DIR%
echo.

if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"

:: --- Copy files ---
echo  Copying files...
copy /Y "%BIN_DIR%\paf_extract.exe"    "%INSTALL_DIR%\" >nul
copy /Y "%BIN_DIR%\libpaf.dll"         "%INSTALL_DIR%\" >nul
copy /Y "%BIN_DIR%\dstorage.dll"       "%INSTALL_DIR%\" >nul
copy /Y "%BIN_DIR%\dstoragecore.dll"   "%INSTALL_DIR%\" >nul
echo  Done.
echo.

:: --- Register .paf file type ---
echo  Registering .paf file extension...

:: File extension -> ProgID
reg add "HKCR\.paf"               /ve /d "PAFArchive" /f >nul
reg add "HKCR\.paf"               /v "Content Type" /d "application/x-paf" /f >nul

:: ProgID metadata
reg add "HKCR\PAFArchive"         /ve /d "PAF Archive" /f >nul
reg add "HKCR\PAFArchive\DefaultIcon" /ve /d "%INSTALL_DIR%\paf_extract.exe,0" /f >nul

:: "Extract Here" -- extracts to a subfolder next to the .paf file
reg add "HKCR\PAFArchive\shell\extract_here" /ve /d "Extract Here" /f >nul
reg add "HKCR\PAFArchive\shell\extract_here" /v "Icon" /d "%INSTALL_DIR%\paf_extract.exe,0" /f >nul
reg add "HKCR\PAFArchive\shell\extract_here\command" /ve /d "\"%INSTALL_DIR%\paf_extract.exe\" \"%%1\"" /f >nul

:: "Extract to <name>\" -- same as above but makes it explicit in the label
::  The exe already defaults to <archive_name>\ so the behaviour is identical;
::  this entry just shows the target name in the menu dynamically.
reg add "HKCR\PAFArchive\shell\extract_to" /ve /d "Extract to folder..." /f >nul
reg add "HKCR\PAFArchive\shell\extract_to" /v "Icon" /d "%INSTALL_DIR%\paf_extract.exe,0" /f >nul
reg add "HKCR\PAFArchive\shell\extract_to\command" /ve /d "\"%INSTALL_DIR%\paf_extract.exe\" \"%%1\"" /f >nul

echo  Done.
echo.

:: --- Refresh Explorer ---
ie4uinit.exe -show >nul 2>&1

echo  =============================================
echo   Installation complete!
echo.
echo   Right-click any .paf file in Explorer to
echo   see "Extract Here" and "Extract to folder..."
echo  =============================================
echo.
pause
endlocal
