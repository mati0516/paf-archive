@echo off
setlocal
cd /d "%~dp0.."

REM Require Administrator
net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: 管理者権限で実行してください。
    echo Right-click this file and choose "Run as administrator".
    pause & exit /b 1
)

set DLL=%~dp0..\bin\paf_shell.dll
set VIEWER=%~dp0..\bin\paf_viewer.exe
set LIBPAF=%~dp0..\bin\libpaf.dll

if not exist "%DLL%" (
    echo ERROR: bin\paf_shell.dll が見つかりません。先に shell\build.bat を実行してください。
    pause & exit /b 1
)

echo === PAF Shell Extension インストール ===
echo.

REM Copy DLLs to System32 so regsvr32 can find them without PATH
copy /Y "%DLL%"    "%SystemRoot%\System32\paf_shell.dll"    >nul
copy /Y "%LIBPAF%" "%SystemRoot%\System32\libpaf.dll"       >nul
copy /Y "%VIEWER%" "%SystemRoot%\System32\paf_viewer.exe"   >nul

REM Also copy DirectStorage if present (needed by libpaf on some builds)
if exist "%~dp0..\bin\dstorage.dll" (
    copy /Y "%~dp0..\bin\dstorage.dll"     "%SystemRoot%\System32\" >nul
    copy /Y "%~dp0..\bin\dstoragecore.dll" "%SystemRoot%\System32\" >nul
)

REM Register the shell extension
regsvr32 /s "%SystemRoot%\System32\paf_shell.dll"
if errorlevel 1 ( echo DLL の登録に失敗しました。 & pause & exit /b 1 )

echo ✅ PAF Shell Extension を登録しました。
echo    .paf ファイルを右クリックして確認してください。
echo.
pause
