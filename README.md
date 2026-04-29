# Parallel Archive Format (PAF)

PAF is a next-generation, high-performance, non-compressed archive format that fully leverages **GPU acceleration** and **Microsoft DirectStorage**.
It is designed to eliminate I/O bottlenecks and provide massive throughput for packaging and extracting large numbers of files.

## 🚀 Key Features

- **GPU-Accelerated Hashing**: Parallel SHA-256 calculation for tens of thousands of files using NVIDIA CUDA.
- **Microsoft DirectStorage v1.2.2**: Direct NVMe-to-GPU/Memory data paths on Windows, bypassing CPU bottlenecks.
- **Extreme Performance**: Achieved over **140,000 files/sec** throughput for 100k+ file archives.
- **Multi-Platform Support**:
  - **Windows**: Full support for GPU acceleration & DirectStorage.
  - **Linux (WSL)**: High-performance C core for server environments.
  - **Android (ARM64)**: Optimized for mobile high-speed archiving.
- **Zero-Copy Architecture**: Minimized memory copying via optimized internal flat buffers.

## 📁 Directory Structure

- `libpaf/include`: Public header files.
- `libpaf/src`: Common core logic (C).
- `libpaf/src/win`: Windows-specific acceleration (CUDA, DirectStorage).
- `test`: Benchmark and test source code.
- `bench`: Benchmark results and execution data (Git-ignored).
- `.github/workflows`: Automated build & release pipeline (Windows, Linux, Android).

## 🛠️ Build Instructions

### Windows (GPU-Accelerated)
Requires CUDA Toolkit 13.2+ and Visual Studio (MSVC).
```powershell
.\build_paf_gpu.ps1
```

### Multi-Platform (Android / Linux)
Requires Android NDK and WSL (Ubuntu).
```powershell
.\build_multi_platform.ps1
```

## 📈 Benchmark Results (100,000 files)

Measured on an RTX 2080 GPU with DirectStorage enabled.

| Format / Method | Time (sec) | Throughput (files/sec) | Notes |
| :--- | :--- | :--- | :--- |
| **PAF (GPU Batched)** | **0.69s** | **143,904** | **RTX 2080 + CUDA + DirectStorage** |
| PAF (CPU Core) | 1.12s | 89,285 | Common C Core implementation |
| TAR (Standard) | 1.04s | 96,153 | Windows 10 standard tar |
| ZIP (Deflate) | 10.45s | 9,569 | Windows 10 standard zip |

> [!TIP]
> PAF is approximately **15x faster** than ZIP and significantly faster than TAR when GPU acceleration is utilized.

## 🛠️ Binary Specification (v1)

The PAF format is designed for minimal parsing overhead.

1. **Header (8 bytes)**
   - Magic: `PAF1` (4 bytes)
   - File Count: `uint32_t` (4 bytes)

2. **Index Table (Variable)**
   - Per-file entry:
     - Path Length: `uint16_t` (2 bytes)
     - Path String: `char[Path Length]` (Variable)
     - Data Size: `uint32_t` (4 bytes)
     - Data Offset: `uint32_t` (4 bytes) - Relative to the start of the data block
     - CRC32/Hash: `uint32_t` (4 bytes)

3. **Data Block (Variable)**
   - Raw file data stored contiguously in the order of the index table.

## 📦 Official Pre-built Binaries (GPU + DirectStorage)

For Windows users who want the maximum performance without setting up the build environment, we provide a pre-built binary in the repository:

- **Path**: `bin/windows-gpu/libpaf-gpu-directstorage-x64.dll`
- **Features**: Includes **CUDA GPU acceleration** and **Microsoft DirectStorage v1.2.2**.
- **Requirement**: NVIDIA GPU (RTX 20 series or newer recommended) and updated drivers.

## 📦 Deployment

Pushing a tag (e.g., `v1.0.0`) to GitHub automatically triggers GitHub Actions to build binaries for all platforms and publish them to the [Releases](../../releases) page.

---
Developed by Antigravity AI & Team.
