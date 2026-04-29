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
- **Delta Engine**: High-speed O(N) archive comparison using SHA-256 for instant update detection.
- **Index-Only Archives (.pafi)**: Metadata-only export for remote sync and delta calculation without full data.
- **DirectStorage Patching (C++)**: Batch copy updated/added files directly from NVMe to memory using DS v1.2.2.

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

## 🛠️ Binary Specification (v2)

The PAF format is designed for maximum throughput and GPU-friendly parallel processing.

1. **Header (32 bytes)**
   - Magic: `PAF1` (4 bytes)
   - Version: `uint32_t` (4 bytes) - Currently `1` (v2 spec)
   - Flags: `uint32_t` (4 bytes)
   - File Count: `uint32_t` (4 bytes)
   - Index Offset: `uint64_t` (8 bytes) - Absolute offset to the Index Block
   - Path Offset: `uint64_t` (8 bytes) - Absolute offset to the Path Buffer

2. **Index Block (Fixed 64 bytes per entry)**
   - Designed for GPU coalesced access and fast delta comparison.
   - Path Buffer Offset: `uint64_t` (8 bytes)
   - Path Length: `uint32_t` (4 bytes)
   - Flags: `uint32_t` (4 bytes)
   - Data Offset: `uint64_t` (8 bytes)
   - Data Size: `uint64_t` (8 bytes)
   - **SHA-256 Hash**: `uint8_t[32]` (32 bytes) - High-precision file integrity and delta matching.

3. **Path Buffer (Variable)**
   - Null-terminated UTF-8 file paths.

4. **Data Block (Variable)**
   - Raw file data stored contiguously.

5. **Index-Only Flag**
   - When `header.flags & 0x02` (PAF_FLAG_INDEX_ONLY) is set, the Data Block is empty.

## 📦 Official Pre-built Binaries (GPU + DirectStorage)

For Windows users who want the maximum performance without setting up the build environment, we provide a pre-built binary in the repository:

- **Path**: `bin/windows-gpu/libpaf-gpu-directstorage-x64.dll`
- **Features**: Includes **CUDA GPU acceleration** and **Microsoft DirectStorage v1.2.2**.
- **Requirement**: NVIDIA GPU (RTX 20 series or newer recommended) and updated drivers.

## 📦 Deployment

Pushing a tag (e.g., `v1.0.0`) to GitHub automatically triggers GitHub Actions to build binaries for all platforms and publish them to the [Releases](../../releases) page.

---
Developed by Antigravity AI & Team.
