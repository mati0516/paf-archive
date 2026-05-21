@echo off
setlocal
cd /d "%~dp0.."

echo === PAF Shell Extension Build ===
echo.

REM Check for MSVC
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe not found. Run this from a Visual Studio Developer Command Prompt.
    exit /b 1
)

REM Build paf_shell.dll
echo [1/2] Building paf_shell.dll ...
cl /O2 /LD /nologo ^
   /Ilibpaf\include ^
   shell\paf_shell.c ^
   lib\libpaf.lib ^
   shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib ^
   /Fe:bin\paf_shell.dll ^
   /link /DEF:shell\paf_shell.def /NOEXP
if errorlevel 1 ( echo FAILED. & exit /b 1 )

REM Build paf_viewer.exe
echo [2/2] Building paf_viewer.exe ...
cl /O2 /nologo ^
   /Ilibpaf\include ^
   shell\paf_viewer.c ^
   lib\libpaf.lib ^
   shell32.lib comctl32.lib shlwapi.lib ole32.lib ^
   /Fe:bin\paf_viewer.exe ^
   /link /SUBSYSTEM:WINDOWS
if errorlevel 1 ( echo FAILED. & exit /b 1 )

echo.
echo Build succeeded.
echo   bin\paf_shell.dll
echo   bin\paf_viewer.exe
echo.
echo Run shell\install.bat (as Administrator) to register.
