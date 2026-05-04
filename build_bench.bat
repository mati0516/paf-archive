@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 2>nul
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2
set INCLUDE=%INCLUDE%;H:\paf-archive\libpaf\include
set LIB=%LIB%;H:\paf-archive\bench

cd /d H:\paf-archive\bench
cl /O2 /Ilibpaf_include bench_paf.c libpaf.lib /Fe:bench_paf.exe
if errorlevel 1 (
    cl /O2 /I"..\libpaf\include" bench_paf.c libpaf.lib /Fe:bench_paf.exe
)
if errorlevel 1 echo FAILED
