# PAF — Platform Archive Format

PAF is an open, **uncompressed** container format with per-file SHA-256 integrity built into the index.  
Designed for environments where AntiVirus transparency, DLP inspectability, and tamper detection matter more than compression ratio.

## The problem with zip

Compressed archives create friction in enterprise and internal distribution:

- AntiVirus scanners cannot fully inspect compressed or encrypted contents
- DLP tools cannot classify what they cannot read
- Users must extract before using — and many don't know how
- Integrity requires a separate manifest file; there is no per-file hash in the format itself

PAF eliminates these problems by keeping data uncompressed and embedding SHA-256 in the index.

## How PAF is different

| Property | zip (deflate) | zip (store) | PAF |
|:---|:---:|:---:|:---:|
| AntiVirus can scan file data directly | partial | ✓ | ✓ |
| AntiVirus can hash-check without reading data | ✗ | ✗ | ✓ |
| DLP can inspect all contents | partial | ✓ | ✓ |
| Per-file SHA-256 in the format | ✗ | ✗ | ✓ |
| Random access to a single file | ✓ | ✓ | ✓ |
| O(N) delta without re-reading files | ✗ | ✗ | ✓ |

## Key properties

### AntiVirus Transparency
File data is stored uncompressed — AntiVirus engines can read it directly from the archive without decompression.  
More importantly, every file's SHA-256 is stored in the index. A hash-aware AntiVirus engine can check all N entries against its malware database by reading only the index block, never touching the data block at all.

### Built-in Integrity
SHA-256 for every file is recorded at creation time and lives inside the archive. Recipients and automated pipelines can verify the full contents with no separate manifest, no side-channel signature file.

### O(N) Delta Detection
Because hashes are in the index, `paf_delta_calculate` compares two archives in O(N) time without re-reading any file data. Only the changed files need to move.

### SDK-first Design
PAF is a format specification and a reference C library (`libpaf`). File viewers, shell extensions, AntiVirus plugins, deployment tools, and update clients are meant to be built on top. The format is simple, documented, and stable.

## Building on PAF

`libpaf` is a plain C shared library with no mandatory runtime dependencies:

```sh
# Linux / CI
gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -lpthread -o libpaf.so
```

Link against it, include the headers, and read or write PAF archives from any language that has a C FFI. Prebuilt Windows x64 binaries are in `bin/`.

Integrations that would make PAF useful as a platform:

- **Windows Shell Extension** — browse PAF contents in Explorer, launch files directly
- **AntiVirus Plugin** — index-level hash scan before data is touched
- **Deployment Agent** — apply delta updates with `paf_patch_apply_atomic`
- **WASM Viewer** — already in `wasm/`, runs in browser

## Delta Updates

Every index entry embeds a SHA-256 hash, enabling O(N) delta detection without re-reading any file data.

### Workflow A — Directory Diff

```
old dir ── paf_create_index_only ──▶ old.pafi
new dir ── paf_create_index_only ──▶ new.pafi
                                      │
                                      ▼
                     paf_delta_calculate(old.pafi, new.pafi)
                                      │
                 ┌────────────────────┤
                 │  ADDED / UPDATED / DELETED entry list
                 ▼
   paf_patch_apply_from_dir(new_dir, delta, dst_dir)
```

### Workflow B — Binary Patch PAF

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
// Workflow A: directory diff copy
paf_create_index_only("old.pafi", (const char*[]){"/app/v1"}, 1, NULL);
paf_create_index_only("new.pafi", (const char*[]){"/app/v2"}, 1, NULL);

paf_delta_t delta;
paf_delta_calculate("old.pafi", "new.pafi", &delta);
printf("%u change(s)\n", delta.count);

paf_patch_apply_from_dir("/app/v2", &delta, "/app/installed", NULL, NULL);
paf_delta_free(&delta);

// Workflow B: binary patch PAF
paf_create_patch("/app/v1", "/app/v2", "patch_v1_v2.paf", NULL, NULL);
paf_patch_apply_atomic("patch_v1_v2.paf", "/app/installed", NULL, NULL);
```

## Archive Format

```
[Header 32B] [Data Block] [Index Block N×128B] [Path Buffer]
```

| Section | Size | Description |
|:---|:---|:---|
| Header | 32 B | Magic `PAF1`, version, flags, file count, index/path offsets |
| Data Block | Variable | Raw uncompressed file data (absent when `PAF_FLAG_INDEX_ONLY`) |
| Index Block | N × 128 B | Per-file metadata including SHA-256 |
| Path Buffer | Variable | UTF-8 file paths |

### Index Entry (128 bytes)

| Offset | Size | Field | Description |
|:---|:---|:---|:---|
| 0 | 8 B | `path_buffer_offset` | Byte offset within Path Buffer |
| 8 | 4 B | `path_length` | Byte length of UTF-8 path |
| 12 | 4 B | `flags` | `PAF_ENTRY_*` bitmask |
| 16 | 8 B | `data_offset` | Byte offset within Data Block |
| 24 | 8 B | `data_size` | Size in bytes |
| 32 | 32 B | `hash` | SHA-256 of file contents |
| 64 | 32 B | reserved | Zero-filled; reserved for SHA-512 / BLAKE3 |
| 96 | 32 B | reserved | Zero-filled |

### Header Flags

| Flag | Value | Meaning |
|:---|:---|:---|
| `PAF_FLAG_INDEX_ONLY` | `0x02` | Data Block absent — hashes only |

### Per-Entry Flags

| Flag | Value | Meaning |
|:---|:---|:---|
| `PAF_ENTRY_DELETED` | `0x04` | File deleted; `data_size = 0` |
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

## Public API

| Header | Key exports |
|:---|:---|
| `libpaf/include/libpaf.h` | `paf_create_binary`, `paf_create_index_only`, `paf_extract_binary`, `paf_create_patch` |
| `libpaf/include/paf_delta.h` | `paf_delta_calculate`, `paf_delta_free`, `paf_delta_optimize_io`, `paf_patch_apply`, `paf_patch_apply_from_dir`, `paf_patch_apply_atomic` |
| `libpaf/include/paf_extractor.h` | `paf_extractor_open`, `paf_extractor_close`, `paf_extractor_get_file`, `paf_extractor_gpu_run` |
| `libpaf/include/paf_gpu_loader.h` | `paf_cuda_is_available`, `paf_vulkan_is_available`, `paf_dstorage_is_available` |

## High-Performance Backend (optional)

For large-scale use (tens of thousands of files), `libpaf` supports GPU-accelerated SHA-256 and NVMe direct I/O. All detection is at runtime — no compile-time flags required.

**GPU priority order:**
```
CUDA (paf_cuda.dll)  →  Vulkan (paf_sha256.spv)  →  CPU SHA-256
```

**DirectStorage** (Windows): NVMe→host and NVMe→GPU DMA transfers with no CPU involvement.

These backends accelerate archive creation and integrity verification; they are not required for reading or integration.

## Prebuilt Binaries

```
bin/
  libpaf.dll          Windows x64, full GPU build (CUDA + DirectStorage + Vulkan)
  dstorage.dll        DirectStorage 1.2.2 runtime
  dstoragecore.dll    DirectStorage 1.2.2 core
  bench_paf.exe       Benchmark

lib/
  libpaf.lib          Import library
```

Link against `lib/libpaf.lib`, include headers from `libpaf/include/`, and place the DLLs next to your application.

## Build

### Linux / CI (CPU only)
```sh
gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -lpthread -o libpaf.so
```

### Windows — CPU only
```bat
cl /O2 /LD /Fe:libpaf.dll "-DLIBPAF_EXPORTS" "-DPAF_CI_BUILD" /Ilibpaf/include /Ilibpaf/src/win libpaf/src/*.c
```

### Windows — full GPU (CUDA 13.2 + MSVC)
```powershell
.\build_paf_gpu.ps1
```

### Android ARM64
```powershell
.\build_multi_platform.ps1
```

## Directory Structure

```
bin/                    Prebuilt Windows x64 binaries
lib/                    Import library (libpaf.lib)
libpaf/
  include/              Public C headers
  src/                  Core library (C)
    paf_sha256.comp     GLSL SHA-256 compute shader
    win/                Windows-only: CUDA, DirectStorage, D3D12
test/                   Test suite
wasm/                   Emscripten bindings (browser viewer)
.github/workflows/      CI: Windows DLL, Linux .so, Android ARM64
```

## Deployment

Pushing a tag (`v*`) triggers GitHub Actions to build Windows DLL, Linux .so, and Android ARM64 `.so`, then publishes them to the [Releases](../../releases) page.

---
Developed by Antigravity AI & Team.
