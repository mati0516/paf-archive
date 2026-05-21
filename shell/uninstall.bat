@echo off
setlocal

net session >nul 2>&1
if errorlevel 1 (
    echo ERROR: 管理者権限で実行してください。
    pause & exit /b 1
)

echo === PAF Shell Extension アンインストール ===
echo.

regsvr32 /s /u "%SystemRoot%\System32\paf_shell.dll"

del /f /q "%SystemRoot%\System32\paf_shell.dll"   >nul 2>&1
del /f /q "%SystemRoot%\System32\paf_viewer.exe"  >nul 2>&1

echo ✅ PAF Shell Extension を削除しました。
pause
