$cuda_path = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.2"
$nvcc = "$cuda_path\bin\nvcc.exe"
$ErrorActionPreference = "Stop"

# Setup VS Environment
$vsdevcmd = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
cmd /c " `"$vsdevcmd`" -arch=x64 && set " | Foreach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:\$($Matches[1])" -Value $Matches[2]
    }
}

# Paths
$env:PATH = "$cuda_path\bin;$env:PATH"
$env:INCLUDE = "$env:INCLUDE;$cuda_path\include;$PSScriptRoot\libpaf\include"
$env:LIB     = "$env:LIB;$cuda_path\lib\x64;$PSScriptRoot\libpaf\src\win"

$cl = "cl.exe"

Write-Host "--- Step 1: Compiling Vulkan SPIR-V shader ---" -ForegroundColor Cyan
$glslang = Get-Command glslangValidator -ErrorAction SilentlyContinue
if ($glslang) {
    & glslangValidator -V libpaf\src\paf_sha256.comp -o paf_sha256.spv
    if ($LASTEXITCODE -ne 0) { throw "glslangValidator failed" }
    Write-Host "  paf_sha256.spv compiled." -ForegroundColor DarkGray
} else {
    Write-Host "  glslangValidator not found — skipping SPIR-V compile (paf_sha256.spv must already exist)." -ForegroundColor Yellow
}

Write-Host "--- Step 2: Compiling CUDA Kernels ---" -ForegroundColor Cyan
& $nvcc -O3 -c libpaf\src\win\paf_cuda_kernels.cu -o paf_cuda_kernels.obj
if ($LASTEXITCODE -ne 0) { throw "nvcc failed" }

Write-Host "--- Step 3: Compiling DirectStorage C++ files ---" -ForegroundColor Cyan
& $cl /O2 /c /DLIBPAF_EXPORTS /Ilibpaf\include /Ilibpaf\src\win libpaf\src\win\paf_io_directstorage.cpp /Fo:paf_io_directstorage.obj
if ($LASTEXITCODE -ne 0) { throw "cl failed (Step 3a)" }
& $cl /O2 /c /DLIBPAF_EXPORTS /Ilibpaf\include /Ilibpaf\src\win libpaf\src\win\paf_io_d3d12_direct.cpp /Fo:paf_io_d3d12_direct.obj
if ($LASTEXITCODE -ne 0) { throw "cl failed (Step 3b)" }
& $cl /O2 /c /DLIBPAF_EXPORTS /Ilibpaf\include /Ilibpaf\src\win libpaf\src\win\paf_patch_dstorage.cpp /Fo:paf_patch_dstorage.obj
if ($LASTEXITCODE -ne 0) { throw "cl failed (Step 3c)" }

Write-Host "--- Step 4: Building Final libpaf.dll ---" -ForegroundColor Cyan
& $cl /O2 /LD /DLIBPAF_EXPORTS /DPAF_USE_CUDA /Ilibpaf\include /Ilibpaf\src\win `
    libpaf\src\*.c `
    paf_cuda_kernels.obj paf_io_directstorage.obj paf_io_d3d12_direct.obj paf_patch_dstorage.obj `
    cudart.lib dxgi.lib dxguid.lib d3d12.lib `
    /Fe:libpaf.dll
if ($LASTEXITCODE -ne 0) { throw "cl failed (Step 4)" }

Write-Host "[Success] Full GPU-Accelerated libpaf.dll created." -ForegroundColor Green

Write-Host "--- Step 5: Building Benchmark ---" -ForegroundColor Cyan
if (-not (Test-Path "bench")) { New-Item -ItemType Directory "bench" }
& $cl /O2 /Ilibpaf\include test\benchmark_win.c libpaf.lib /Fe:bench\benchmark_win.exe
if ($LASTEXITCODE -eq 0) {
    Write-Host "[Success] bench\benchmark_win.exe created." -ForegroundColor Green
    Write-Host "--- Step 6: Running High-Performance Benchmark ---" -ForegroundColor Cyan
    Set-Location bench
    .\benchmark_win.exe
    Set-Location ..
}
