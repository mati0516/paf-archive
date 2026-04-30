# Parallel Archive Format (PAF)

PAF is a high-performance, non-compressed archive format designed around GPU-parallel SHA-256 hashing and fast NVMe I/O.  
It eliminates I/O bottlenecks for large file collections by batching hash computation on the GPU and (on Windows) using Microsoft DirectStorage for direct NVMe reads.

## Key Features

- **GPU-Parallel SHA-256**: Batch hashing via CUDA (NVIDIA) or Vulkan compute (AMD/Intel/Linux/Android). Automatically falls back to CPU when no GPU is present.
- **Runtime GPU Detection**: No compile-time flags. `libpaf.dll` / `libpaf.so` probes `paf_cuda.dll`, `libvulkan.so.1`, and `dstorage.dll` at startup and selects the best available backend.
- **Microsoft DirectStorage v1.2.2** (Windows): NVMe→host transfers and true NVMe→GPU DMA via `DSTORAGE_REQUEST_DESTINATION_BUFFER` with no CPU involvement.
- **O(N) Delta Engine**: Compares archives using only the embedded SHA-256 hashes — no file data re-read required.
- **Binary Patch PAF**: `paf_create_patch` produces a compact patch archive using rsync-style block deltas for UPDATED files. `paf_patch_apply_atomic` applies it with SHA-256 verification and atomic rename.
- **Index-Only Archives**: Lightweight `.pafi` files (metadata only, no data block) for remote delta calculation without shipping full archives.
- **Multi-Platform**: Windows (GPU + DirectStorage), Linux (Vulkan GPU), Android ARM64.

## Performance

> Benchmarks coming soon.

## GPU Priority Order

```
CUDA (paf_cuda.dll)  →  Vulkan (libvulkan + paf_sha256.spv)  →  CPU SHA-256
```

All detection is lazy — no explicit initialisation call is required. The Vulkan path requires `paf_sha256.spv` (compiled from `libpaf/src/paf_sha256.comp`) in the working directory or `/usr/lib/paf/`.

## Archive Format

```
[Header 32B] [Data Block] [Index Block N×64B] [Path Buffer]
```

| Section | Size | Description |
|:---|:---|:---|
| Header | 32 B | Magic `PAF1`, version, flags, file count, index/path offsets |
| Data Block | Variable | Raw file data, contiguous (absent when `PAF_FLAG_INDEX_ONLY`) |
| Index Block | N × 64 B | Per-file metadata (see below) |
| Path Buffer | Variable | UTF-8 file paths (no NUL terminator; length stored in index) |

### Index Entry Fields (64 bytes, fixed-size for GPU coalesced access)

| Offset | Size | Field | Description |
|:---|:---|:---|:---|
| 0 | 8 B | `path_buffer_offset` | Byte offset within Path Buffer |
| 8 | 4 B | `path_length` | Byte length of UTF-8 path |
| 12 | 4 B | `flags` | `PAF_ENTRY_*` bitmask |
| 16 | 8 B | `data_offset` | Byte offset within Data Block |
| 24 | 8 B | `data_size` | Size of data block entry in bytes |
| 32 | 32 B | `hash` | **SHA-256** — sole integrity checksum |

> For `PAF_ENTRY_BINARY_DELTA` entries, `hash` is the SHA-256 of the file *after* applying the delta (i.e. the new file).

### Header Flags (`paf_header_t.flags`)

| Flag | Value | Meaning |
|:---|:---|:---|
| `PAF_FLAG_INDEX_ONLY` | `0x02` | Data Block absent — index-only snapshot |

### Per-Entry Flags (`paf_index_entry_t.flags`)

| Flag | Value | Meaning |
|:---|:---|:---|
| `PAF_ENTRY_DELETED` | `0x04` | File deleted; `data_size = 0`, no data |
| `PAF_ENTRY_BINARY_DELTA` | `0x08` | Data block is a PAFD binary delta |

### PAFD Binary Delta Format

```
[magic "PAFD" 4B] [instr_count 4B] [old_file_size 8B]
N × { [type 1B] [offset 8B] [size 4B] [data if LITERAL] }
```

| type | Meaning |
|:---|:---|
| `0` COPY | Copy `size` bytes from `offset` in the old file |
| `1` LITERAL | Emit the following `size` bytes verbatim |

Matching is performed on 4 KB aligned blocks using FNV-1a 64-bit hashing with an open-addressing hash table.

## Delta Updates

Every index entry embeds a SHA-256 hash, enabling **O(N) delta detection without re-reading any file data**.  
Suitable for game asset update delivery and directory synchronisation (rsync equivalent).

### Workflow A — Directory Diff Copy

```
old dir ── paf_create_index_only ──▶ old.pafi  (MB-range, no data)
new dir ── paf_create_index_only ──▶ new.pafi
                                      │
                                      ▼
                     paf_delta_calculate(old.pafi, new.pafi)
                                      │
                 ┌────────────────────┤
                 │  ADDED / UPDATED / DELETED entry list
                 ▼
   paf_patch_apply_from_dir(new_dir, delta, dst_dir)
   → copies changed files only, removes deleted files
```

### Workflow B — Binary Patch PAF (distribution)

```
old dir ────────────────────────────────────────────┐
new dir ── paf_create_patch ──▶ patch.paf           │
           ADDED   : full file data                  │
           UPDATED : PAFD binary delta               │
           DELETED : marker only (data_size = 0)    │
                                    │                ▼
                    paf_patch_apply_atomic(patch.paf, dst_dir)
                    old file + delta → .paf_stage
                    → SHA-256 verify → atomic rename
```

### Delta Status Values

| Status | Meaning |
|:---|:---|
| `PAF_DELTA_ADDED` | File present in new version only |
| `PAF_DELTA_UPDATED` | SHA-256 changed between versions |
| `PAF_DELTA_DELETED` | File present in old version only |

### Code Examples

```c
// ── Workflow A: directory diff copy ─────────────────────────────────────
paf_create_index_only("old.pafi", (const char*[]){"/game/v1"}, 1, NULL);
paf_create_index_only("new.pafi", (const char*[]){"/game/v2"}, 1, NULL);

paf_delta_t delta;
paf_delta_calculate("old.pafi", "new.pafi", &delta);
printf("%u change(s)\n", delta.count);

paf_patch_apply_from_dir("/game/v2", &delta, "/game/installed", NULL, NULL);
paf_delta_free(&delta);

// ── Workflow B: binary patch PAF ────────────────────────────────────────
// Server side: generate compact patch archive
paf_create_patch("/game/v1", "/game/v2", "patch_v1_v2.paf", NULL, NULL);

// Client side: apply with SHA-256 verification + atomic rename
paf_patch_apply_atomic("patch_v1_v2.paf", "/game/installed", NULL, NULL);
```

Call `paf_delta_optimize_io` to sort delta entries by offset, maximising sequential I/O throughput.

## Directory Structure

```
libpaf/
  include/              Public C headers
    paf.h               Format definitions (paf_header_t, paf_index_entry_t, flags)
    libpaf.h            Primary API (create / extract / patch)
    paf_delta.h         Delta calculation and patch application
    paf_extractor.h     Random-access extraction + GPU batch extraction
    paf_gpu_loader.h    Runtime GPU detection (CUDA / Vulkan / DirectStorage)
    paf_gpu.h           GPU direct load (DirectStorage / D3D12)
    paf_vulkan.h        Vulkan compute pipeline API
  src/                  Core library (C)
    paf_sha256.comp     GLSL SHA-256 compute shader → paf_sha256.spv
    paf_vulkan.c        Vulkan compute pipeline (no vulkan.h dependency)
    paf_binary_delta.c  rsync-style block delta engine (PAFD format)
    win/                Windows-only: CUDA kernel, DirectStorage, D3D12 direct load
test/                   Test suite (test_paf.c)
wasm/                   Emscripten bindings
.github/workflows/      CI: Windows DLL, Linux .so, Android ARM64
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
| `libpaf/include/libpaf.h` | `paf_create_binary`, `paf_create_index_only`, `paf_extract_binary`, `paf_create_patch` |
| `libpaf/include/paf_delta.h` | `paf_delta_calculate`, `paf_delta_free`, `paf_delta_optimize_io`, `paf_patch_apply`, `paf_patch_apply_from_dir`, `paf_patch_apply_atomic` |
| `libpaf/include/paf_extractor.h` | `paf_extractor_open`, `paf_extractor_close`, `paf_extractor_get_file`, `paf_extractor_gpu_run` |
| `libpaf/include/paf_gpu_loader.h` | `paf_cuda_is_available`, `paf_vulkan_is_available`, `paf_dstorage_is_available` |
| `libpaf/include/paf_gpu.h` | `paf_gpu_direct_load`, `paf_gpu_direct_load_d3d12`, `paf_gpu_search_files` |
| `libpaf/include/paf_vulkan.h` | `paf_vulkan_init`, `paf_vulkan_hash_flat`, `paf_vulkan_cleanup` |

## Deployment

Pushing a tag (`v*`) triggers GitHub Actions to build Windows DLL, Linux .so, and Android ARM64 `.so`, then publishes them to the [Releases](../../releases) page.

---
Developed by Antigravity AI & Team.
