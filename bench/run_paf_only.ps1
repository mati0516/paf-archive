$BENCH_DIR = "h:\paf-archive\bench"
$DATA_DIR = "h:\paf-archive\bench\data"
$EXTRACT_DIR = "h:\paf-archive\bench\extracted"
$ARCHIVE_DIR = "h:\paf-archive\bench\archives"
$RESULTS_FILE = "h:\paf-archive\bench\results\results_paf_new.txt"

if (-not (Test-Path $RESULTS_FILE)) { New-Item -ItemType File $RESULTS_FILE }
"Benchmark Report (PAF Only) - $(Get-Date)" | Out-File $RESULTS_FILE

function Run-Step($name, $cmd) {
    Write-Host "--- Running: $name ---" -ForegroundColor Yellow
    if (Test-Path $EXTRACT_DIR) { 
        cmd /c "rd /s /q `"$EXTRACT_DIR`" 2>nul"
    }
    New-Item -ItemType Directory $EXTRACT_DIR | Out-Null
    
    $start = Get-Date
    $output = Invoke-Expression "$cmd 2>&1"
    $end = Get-Date
    $duration = ($end - $start).TotalSeconds
    
    Write-Host $output
    "$($name): $($duration.ToString('F3')) sec | $output" | Out-File $RESULTS_FILE -Append
    Write-Host "Completed $name in $($duration.ToString('F3')) sec" -ForegroundColor Green
    Start-Sleep -Seconds 1
}

# Ensure archives dir exists
if (-not (Test-Path $ARCHIVE_DIR)) { New-Item -ItemType Directory $ARCHIVE_DIR }

Set-Location $BENCH_DIR
$env:PAF_DISABLE_VULKAN = "1"

# --- PAF Benchmarks ---

# Creation
$env:PAF_DISABLE_CUDA = "1"; $env:PAF_DISABLE_DSTORAGE = "1"
Run-Step "PAF Creation (CPU)" ".\bench_paf.exe create data archives\bench_cpu.paf"

$env:PAF_DISABLE_CUDA = "0"; $env:PAF_DISABLE_DSTORAGE = "0"
Run-Step "PAF Creation (GPU)" ".\bench_paf.exe create data archives\bench_gpu.paf"

# Extraction (CPU parallel)
$env:PAF_DISABLE_CUDA = "1"; $env:PAF_DISABLE_DSTORAGE = "1"
Run-Step "PAF Extraction (CPU Parallel)" ".\bench_paf.exe extract archives\bench_cpu.paf extracted"

# Extraction (GPU hash verify)
$env:PAF_DISABLE_CUDA = "0"; $env:PAF_DISABLE_DSTORAGE = "1"
Run-Step "PAF Extraction (GPU Only)" ".\bench_paf.exe extract_gpu archives\bench_gpu.paf extracted"

# Extraction (GPU + DirectStorage)
$env:PAF_DISABLE_CUDA = "0"; $env:PAF_DISABLE_DSTORAGE = "0"
Run-Step "PAF Extraction (GPU + DirectStorage)" ".\bench_paf.exe extract_gpu archives\bench_gpu.paf extracted"

Write-Host "All PAF benchmarks completed." -ForegroundColor Cyan
