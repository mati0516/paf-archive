# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Linux / CI (CPU only)
```sh
gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -o libpaf.so
```
This is the canonical quick-check build. All `.c` files in `libpaf/src/` are compiled together; the Windows-specific `.cpp` and `.cu` files are excluded automatically.

### Windows — CPU only (matches CI)
```bat
cl /O2 /LD /Fe:libpaf.dll "-DLIBPAF_EXPORTS" "-DPAF_CI_BUILD" /Ilibpaf/include /Ilibpaf/src/win libpaf/src/*.c
```

### Windows — full GPU (CUDA 13.2 + MSVC required)
```powershell
.\build_paf_gpu.ps1
```
Compiles `paf_cuda_kernels.cu` via `nvcc`, then `paf_io_directstorage.cpp` via `cl`, and links everything into `libpaf.dll`. Also builds and runs `bench/benchmark_win.exe`.

### Android ARM64 + Linux cross-compile
```powershell
.\build_multi_platform.ps1
```

### WASM
Built separately via Emscripten from `wasm/paf_wasm.c`. Pre-built outputs live in `wasm/`.

## Architecture

### PAF Binary Format
Defined in `libpaf/include/paf.h`:
```
[Header 32B] [Data Block] [Index Block N×64B] [Path Buffer]
```
- Index-only variant (`PAF_FLAG_INDEX_ONLY = 0x02`): Data Block is omitted; `data_offset` fields in all index entries are meaningless (zero).
- Every index entry carries a SHA-256 hash — this is how `paf_delta_calculate` achieves O(N) diff without reading file data.

### Core Library (`libpaf/src/`)

| File | Responsibility |
|------|---------------|
| `paf_generator.c` | Streaming write pipeline; batches up to 4096 files / 128 MB into a flat buffer for GPU/CPU SHA-256 |
| `libpaf_core.c` | `paf_create_binary`, `paf_create_index_only`, `paf_extract_binary`; file collection with `.pafignore` support |
| `paf_delta.c` | O(N) delta via DJB2 hash table; compares two PAF indexes by SHA-256 |
| `libpaf_patch.c` | `paf_patch_apply` (from PAF), `paf_patch_apply_from_dir` (from directory) |
| `paf_extractor.c` | Random-access file extraction with smart-overwrite (SHA-256 skip) |
| `paf_extractor_gpu.c` | Batch GPU extraction: Phase 1 I/O → Phase 2 SHA-256 → Phase 3 write |
| `paf_gpu_loader.c` | Runtime detection of `paf_cuda.dll` and `dstorage.dll` via `LoadLibraryA` |

### Runtime GPU Switching
`paf_gpu_loader.c` replaces all compile-time `#if PAF_USE_CUDA` guards:
- **CUDA**: loads `paf_cuda.dll` at startup, resolves `paf_cuda_init` + `paf_cuda_hash_flat`.
- **Vulkan**: calls `paf_vulkan_init()` (from `paf_vulkan.c`), which dynamically loads `libvulkan.so.1` / `vulkan-1.dll` and reads `paf_sha256.spv` (compiled from `paf_sha256.comp`).
- **DirectStorage**: loads `dstorage.dll`, checks `DStorageGetFactory`.
- Priority order in hash pipeline: CUDA → Vulkan → CPU SHA-256.
- `paf_generator.c` and `paf_extractor_gpu.c` follow this priority via `paf_cuda_is_available()` / `paf_vulkan_is_available()` + global function pointers.
- Initialisation is lazy — no explicit `paf_gpu_init()` call is required.

### Vulkan Compute Path (`libpaf/src/paf_vulkan.c` + `paf_sha256.comp`)
- `paf_vulkan.c` contains ~350 lines of minimal Vulkan type definitions (no `vulkan.h` dependency) and a full compute pipeline.
- `paf_sha256.comp` is a GLSL compute shader (SHA-256) compiled to SPIR-V (`paf_sha256.spv`) via `glslangValidator`. The `.spv` file is loaded at runtime — `libpaf` does **not** embed it at compile time.
- `build_paf_gpu.ps1` compiles the shader if `glslangValidator` is on `PATH`; otherwise an existing `.spv` is expected.

### Windows GPU Layer (`libpaf/src/win/`)

| File | Role |
|------|------|
| `paf_cuda_kernels.cu` | CUDA SHA-256 kernel; exports `paf_cuda_init`, `paf_cuda_hash_flat` (host↔host), `paf_cuda_sha256_batch` (device↔device) |
| `paf_io_directstorage.cpp` | `paf_io_directstorage_load` — single-file NVMe→memory read via DirectStorage; `DStorageGetFactory` resolved via `GetProcAddress` |
| `paf_io_d3d12_direct.cpp` | `paf_gpu_direct_load_d3d12` — true NVMe→GPU DMA with `DSTORAGE_REQUEST_DESTINATION_BUFFER`; caller supplies `ID3D12Device*` and `ID3D12Resource*` (as `void*`) |
| `paf_patch_dstorage.cpp` | `PatchEngine::ApplyDeltaToBuffer` — batches ADDED/UPDATED delta entries into a flat buffer using DirectStorage; `DStorageGetFactory` also resolved via `GetProcAddress` |

All DirectStorage files load `dstorage.dll` at runtime — `dstorage.lib` is **not** a link-time dependency.

### Public API Surface
- `libpaf/include/libpaf.h` — create/extract/list/exists operations
- `libpaf/include/paf_delta.h` — delta calculation and patch application
- `libpaf/include/paf_extractor.h` — streaming extractor + `paf_extractor_gpu_run`
- `libpaf/include/paf_gpu_loader.h` — `paf_cuda_is_available()`, `paf_dstorage_is_available()`, `paf_vulkan_is_available()`, `g_paf_cuda_hash_flat`, `g_paf_vulkan_hash_flat`
- `libpaf/include/paf_gpu.h` — `paf_gpu_direct_load`, `paf_gpu_direct_load_d3d12`, `paf_gpu_search_files`
- `libpaf/include/paf_vulkan.h` — `paf_vulkan_init`, `paf_vulkan_hash_flat`, `paf_vulkan_cleanup`

### WASM Bindings
`wasm/paf_wasm.c` wraps `paf_list_binary`, `paf_extract_binary`, etc. as `EMSCRIPTEN_KEEPALIVE` exports. Results are returned to JavaScript via `window.paf_on_*` callbacks using `MAIN_THREAD_EM_ASM`.

