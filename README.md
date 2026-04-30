# Parallel Archive Format (PAF)

PAF is a high-performance, non-compressed archive format designed around GPU-parallel SHA-256 hashing and fast NVMe I/O.  
It eliminates I/O bottlenecks for large file collections by batching hash computation on the GPU and (on Windows) using Microsoft DirectStorage for direct NVMe reads.

## Key Features

- **GPU-Accelerated Hashing**: Batch SHA-256 via CUDA (NVIDIA) or Vulkan compute (AMD/Intel/Linux/Android). Falls back to CPU automatically when no GPU is present.
- **Runtime GPU Detection**: No compile-time GPU flags. `libpaf.dll` / `libpaf.so` probes `paf_cuda.dll`, `libvulkan.so.1`, and `dstorage.dll` at startup and uses the best available backend.
- **Microsoft DirectStorage v1.2.2** (Windows): NVMe→host and true NVMe→GPU DMA (`DSTORAGE_REQUEST_DESTINATION_BUFFER`) without CPU involvement.
- **O(N) Delta Engine**: Archive comparison via SHA-256 hash table — no file data re-read needed.
- **Index-Only Archives**: Metadata-only `.paf` files for remote delta calculation without shipping full data.
- **DirectStorage Patching**: Batch-copy ADDED/UPDATED delta entries from NVMe using DirectStorage.
- **Multi-Platform**: Windows (GPU + DirectStorage), Linux (GPU via Vulkan), Android ARM64.

## Performance (100,000 files, RTX 2080)

| Method | Time | Throughput | Notes |
|:---|:---|:---|:---|
| **PAF GPU (CUDA + DirectStorage)** | **0.69s** | **143,904 files/sec** | RTX 2080 |
| PAF CPU | 1.12s | 89,285 files/sec | CPU SHA-256 fallback |
| TAR | 1.04s | 96,153 files/sec | Windows standard |
| ZIP (Deflate) | 10.45s | 9,569 files/sec | Windows standard |

> PAF is ~15× faster than ZIP when GPU acceleration is available.

## GPU Priority Order

```
CUDA (paf_cuda.dll)  →  Vulkan (libvulkan + paf_sha256.spv)  →  CPU SHA-256
```

All detection is lazy — no explicit initialisation call is required. The Vulkan path requires `paf_sha256.spv` (compiled from `libpaf/src/paf_sha256.comp`) in the working directory or `/usr/lib/paf/`.

## PAF Binary Format (v2)

```
[Header 32B] [Data Block] [Index Block N×64B] [Path Buffer]
```

| Section | Size | Description |
|:---|:---|:---|
| Header | 32 B | Magic `PAF1`, version, flags, file count, index/path offsets |
| Data Block | Variable | Raw file data contiguous |
| Index Block | N × 64 B | Per-file: path offset, path length, data offset, data size, **SHA-256 hash** |
| Path Buffer | Variable | UTF-8 file paths |

When `header.flags & PAF_FLAG_INDEX_ONLY (0x02)` is set, the Data Block is absent — used for lightweight delta snapshots.

## Directory Structure

```
libpaf/
  include/          Public C headers
  src/              Core library (C)
    paf_sha256.comp GLSL SHA-256 compute shader → compile to paf_sha256.spv
    paf_vulkan.c    Vulkan compute pipeline (no vulkan.h dependency)
    win/            Windows-only: CUDA kernel, DirectStorage, D3D12 direct load
test/               Test suite (test_paf.c)
wasm/               Emscripten bindings
.github/workflows/  CI: Windows DLL, Linux .so, Android ARM64
```

## Build

### Linux / CI (CPU only)
```sh
gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -o libpaf.so
```

### Windows — CPU only (CI-compatible)
```bat
cl /O2 /LD /Fe:libpaf.dll "-DLIBPAF_EXPORTS" "-DPAF_CI_BUILD" /Ilibpaf/include /Ilibpaf/src/win libpaf/src/*.c
```

### Windows — full GPU (CUDA 13.2 + MSVC + optional glslangValidator)
```powershell
.\build_paf_gpu.ps1
```
Compiles `paf_sha256.comp` → `paf_sha256.spv` (if `glslangValidator` is on PATH), builds `paf_cuda_kernels.cu` via nvcc, `paf_io_directstorage.cpp` + `paf_io_d3d12_direct.cpp` via cl, then links everything into `libpaf.dll` with `d3d12.lib`.

### Android ARM64
```powershell
.\build_multi_platform.ps1
```

## Public API

| Header | Key exports |
|:---|:---|
| `libpaf/include/libpaf.h` | `paf_create_binary`, `paf_create_index_only`, `paf_extract_binary` |
| `libpaf/include/paf_delta.h` | `paf_delta_calculate`, `paf_patch_apply`, `paf_patch_apply_from_dir` |
| `libpaf/include/paf_extractor.h` | `paf_extractor_open/close/get_file`, `paf_extractor_gpu_run` |
| `libpaf/include/paf_gpu_loader.h` | `paf_cuda_is_available`, `paf_vulkan_is_available`, `paf_dstorage_is_available` |
| `libpaf/include/paf_gpu.h` | `paf_gpu_direct_load`, `paf_gpu_direct_load_d3d12`, `paf_gpu_search_files` |
| `libpaf/include/paf_vulkan.h` | `paf_vulkan_init`, `paf_vulkan_hash_flat`, `paf_vulkan_cleanup` |

## Deployment

Pushing a tag (`v*`) triggers GitHub Actions to build Windows DLL, Linux .so, and Android ARM64 `.so`, then publishes them to the [Releases](../../releases) page.

---
Developed by Antigravity AI & Team.
