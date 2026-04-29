$ndk_path = "C:\Program Files (x86)\Android\AndroidNDK\android-ndk-r27c"
$ndk_clang = "$ndk_path\toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android34-clang.cmd"

Write-Host "--- Multi-Platform Build ---" -ForegroundColor Cyan

# 1. Build for Android (ARM64)
Write-Host "Building for Android (ARM64)..." -ForegroundColor Yellow
if (-not (Test-Path "build_android")) { New-Item -ItemType Directory "build_android" }
& $ndk_clang -O3 -shared -fPIC -Ilibpaf/include `
    libpaf/src/*.c -o build_android/libpaf.so

if ($LASTEXITCODE -eq 0) {
    Write-Host "[Success] Android libpaf.so created in build_android/" -ForegroundColor Green
}

# 2. Build for Linux (via WSL)
Write-Host "Building for Linux (via WSL Ubuntu)..." -ForegroundColor Yellow
if (-not (Test-Path "build_linux")) { New-Item -ItemType Directory "build_linux" }
wsl -e bash -c "gcc -O3 -shared -fPIC -Ilibpaf/include \
    libpaf/src/*.c -o build_linux/libpaf.so"

if ($LASTEXITCODE -eq 0) {
    Write-Host "[Success] Linux libpaf.so created in build_linux/" -ForegroundColor Green
}
