# Parallel Archive Format (PAF)

PAF is a high-performance, non-compressed archive format designed around GPU-parallel SHA-256 hashing and fast NVMe I/O.  
It eliminates I/O bottlenecks for large file collections by batching hash computation on the GPU and (on Windows) using Microsoft DirectStorage for direct NVMe reads.

## Key Features

- **GPU-Accelerated Hashing**: Batch SHA-256 via CUDA (NVIDIA) or Vulkan compute (AMD/Intel/Linux/Android). Falls back to CPU automatically when no GPU is present.
- **Runtime GPU Detection**: No compile-time GPU flags. `libpaf.dll` / `libpaf.so` probes `paf_cuda.dll`, `libvulkan.so.1`, and `dstorage.dll` at startup and uses the best available backend.
- **Microsoft DirectStorage v1.2.2** (Windows): NVMe→host and true NVMe→GPU DMA (`DSTORAGE_REQUEST_DESTINATION_BUFFER`) without CPU involvement.
- **O(N) Delta Engine**: Archive comparison via SHA-256 hash table — no file data re-read needed.
- **Binary Patch PAF**: `paf_create_patch` produces a compact patch archive (rsync-style block delta for UPDATED files). `paf_patch_apply_atomic` applies it with SHA-256 verification and atomic rename.
- **Index-Only Archives**: Metadata-only `.paf` files for remote delta calculation without shipping full data.
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

## Archive Format

```
[Header 32B] [Data Block] [Index Block N×64B] [Path Buffer]
```

| Section | Size | Description |
|:---|:---|:---|
| Header | 32 B | Magic `PAF1`, version, flags, file count, index/path offsets |
| Data Block | Variable | Raw file data contiguous (absent when `PAF_FLAG_INDEX_ONLY`) |
| Index Block | N × 64 B | Per-file: path offset, path length, data offset, data size, per-entry flags, **SHA-256 hash** |
| Path Buffer | Variable | UTF-8 file paths |

### Header flags

| Flag | Value | Meaning |
|:---|:---|:---|
| `PAF_FLAG_INDEX_ONLY` | `0x02` | Data Block absent — lightweight delta snapshot |

### Per-entry flags (`paf_index_entry_t.flags`)

| Flag | Value | Meaning |
|:---|:---|:---|
| `PAF_ENTRY_DELETED` | `0x04` | File was deleted; no data |
| `PAF_ENTRY_BINARY_DELTA` | `0x08` | Data block is a binary delta (PAFD format) |

## Delta Update (差分アップデート)

PAF の各インデックスエントリには SHA-256 ハッシュが埋め込まれているため、**ファイルデータを再読込せずに O(N) で差分を検出**できます。  
ゲームアセットのアップデート配信やディレクトリ同期 (rsync 相当) に使用できます。

### ワークフロー A — ディレクトリ差分コピー

```
旧ディレクトリ ─── paf_create_index_only ───▶ old.pafi (数MB、データなし)
新ディレクトリ ─── paf_create_index_only ───▶ new.pafi
                                               │
                                               ▼
                              paf_delta_calculate(old.pafi, new.pafi)
                                               │
                        ┌──────────────────────┤
                        │  ADDED / UPDATED / DELETED エントリ一覧
                        ▼
          paf_patch_apply_from_dir(new_dir, delta, dst_dir)
          → 変更ファイルのみコピー、削除ファイルを除去
```

### ワークフロー B — バイナリパッチ PAF（コンパクト配布）

```
旧ディレクトリ ──────────────────────────────────────┐
新ディレクトリ ─── paf_create_patch ───▶ patch.paf  │
                   (ADDED=全データ、                  │
                    UPDATED=バイナリデルタ、          │
                    DELETED=マーカー)                 │
                                          │           │
                                          ▼           ▼
                            paf_patch_apply_atomic(patch.paf, dst_dir)
                            → SHA-256 検証 → .paf_stage → atomic rename
```

### 差分が検出されるケース

| ステータス | 意味 |
|:---|:---|
| `PAF_DELTA_ADDED` | 新バージョンに追加されたファイル |
| `PAF_DELTA_UPDATED` | SHA-256 が変化したファイル |
| `PAF_DELTA_DELETED` | 旧バージョンにあって新バージョンにないファイル |

### コード例

```c
// ── ワークフロー A: ディレクトリ差分コピー ──────────────────────────────
paf_create_index_only("old.pafi", (const char*[]){"/game/v1"}, 1, NULL);
paf_create_index_only("new.pafi", (const char*[]){"/game/v2"}, 1, NULL);

paf_delta_t delta;
paf_delta_calculate("old.pafi", "new.pafi", &delta);
printf("%u 件の変更\n", delta.count);

paf_patch_apply_from_dir("/game/v2", &delta, "/game/installed", NULL, NULL);
paf_delta_free(&delta);

// ── ワークフロー B: バイナリパッチ PAF ────────────────────────────────
// パッチ作成（配布サーバー側）
paf_create_patch("/game/v1", "/game/v2", "patch_v1_v2.paf", NULL, NULL);

// パッチ適用（クライアント側） — SHA-256 検証 + アトミックリネーム
paf_patch_apply_atomic("patch_v1_v2.paf", "/game/installed", NULL, NULL);
```

`paf_delta_optimize_io` を呼ぶと差分エントリをオフセット順にソートし、シーケンシャルアクセスで I/O 効率を最大化できます。

## Directory Structure

```
libpaf/
  include/          Public C headers
  src/              Core library (C)
    paf_sha256.comp GLSL SHA-256 compute shader → compile to paf_sha256.spv
    paf_vulkan.c    Vulkan compute pipeline (no vulkan.h dependency)
    paf_binary_delta.c  rsync-style block delta engine (PAFD format)
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
| `libpaf/include/libpaf.h` | `paf_create_binary`, `paf_create_index_only`, `paf_extract_binary`, `paf_create_patch` |
| `libpaf/include/paf_delta.h` | `paf_delta_calculate`, `paf_patch_apply`, `paf_patch_apply_from_dir`, `paf_patch_apply_atomic` |
| `libpaf/include/paf_extractor.h` | `paf_extractor_open/close/get_file`, `paf_extractor_gpu_run` |
| `libpaf/include/paf_gpu_loader.h` | `paf_cuda_is_available`, `paf_vulkan_is_available`, `paf_dstorage_is_available` |
| `libpaf/include/paf_gpu.h` | `paf_gpu_direct_load`, `paf_gpu_direct_load_d3d12`, `paf_gpu_search_files` |
| `libpaf/include/paf_vulkan.h` | `paf_vulkan_init`, `paf_vulkan_hash_flat`, `paf_vulkan_cleanup` |

## Deployment

Pushing a tag (`v*`) triggers GitHub Actions to build Windows DLL, Linux .so, and Android ARM64 `.so`, then publishes them to the [Releases](../../releases) page.

---
Developed by Antigravity AI & Team.
