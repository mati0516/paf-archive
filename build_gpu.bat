@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 2>nul
if errorlevel 1 (
    echo VsDevCmd failed, trying VS2022...
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 2>nul
)
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2
set PATH=%CUDA_PATH%\bin;%PATH%
set INCLUDE=%INCLUDE%;%CUDA_PATH%\include;H:\paf-archive\libpaf\include;H:\paf-archive\libpaf\src\win
set LIB=%LIB%;%CUDA_PATH%\lib\x64;H:\paf-archive\libpaf\src\win

cd /d H:\paf-archive

echo --- Step 1: SPIR-V ---
where glslangValidator >nul 2>&1 && (glslangValidator -V libpaf\src\paf_sha256.comp -o paf_sha256.spv) || echo skipped

echo --- Step 2: CUDA kernels ---
"%CUDA_PATH%\bin\nvcc.exe" -O3 -c libpaf\src\win\paf_cuda_kernels.cu -o paf_cuda_kernels.obj
if errorlevel 1 goto :fail

echo --- Step 3: DirectStorage ---
cl /O2 /c /DLIBPAF_EXPORTS /Ilibpaf\include /Ilibpaf\src\win libpaf\src\win\paf_io_directstorage.cpp /Fo:paf_io_directstorage.obj
if errorlevel 1 goto :fail
cl /O2 /c /DLIBPAF_EXPORTS /Ilibpaf\include /Ilibpaf\src\win libpaf\src\win\paf_io_d3d12_direct.cpp /Fo:paf_io_d3d12_direct.obj
if errorlevel 1 goto :fail
cl /O2 /c /DLIBPAF_EXPORTS /Ilibpaf\include /Ilibpaf\src\win libpaf\src\win\paf_patch_dstorage.cpp /Fo:paf_patch_dstorage.obj
if errorlevel 1 goto :fail

echo --- Step 4: libpaf.dll ---
cl /O2 /LD /DLIBPAF_EXPORTS /DPAF_USE_CUDA /Ilibpaf\include /Ilibpaf\src\win ^
   libpaf\src\*.c ^
   paf_cuda_kernels.obj paf_io_directstorage.obj paf_io_d3d12_direct.obj paf_patch_dstorage.obj ^
   cudart.lib dxgi.lib dxguid.lib d3d12.lib advapi32.lib ^
   /Fe:libpaf.dll
if errorlevel 1 goto :fail

echo --- Step 5: benchmark ---
if not exist bench mkdir bench
cl /O2 /Ilibpaf\include test\benchmark_win.c libpaf.lib /Fe:bench\benchmark_win.exe
if errorlevel 1 goto :fail
echo [SUCCESS]
goto :eof
:fail
echo [FAILED]
exit /b 1

