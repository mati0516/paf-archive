# Parallel Archive Format (PAF)

PAF は GPU 並列 SHA-256 ハッシュと高速 NVMe I/O を中心に設計した、非圧縮の高性能アーカイブフォーマットです。  
ファイルコレクション全体のハッシュ計算を GPU でバッチ処理し、Windows では Microsoft DirectStorage による NVMe 直接読み込みを利用することで I/O ボトルネックを解消します。

## 主な特徴

- **GPU 並列 SHA-256**: CUDA (NVIDIA) または Vulkan コンピュートシェーダー (AMD/Intel/Linux/Android) でバッチハッシュ。GPU 未搭載時は CPU に自動フォールバック。
- **実行時 GPU 検出**: コンパイル時フラグ不要。`libpaf.dll` / `libpaf.so` が起動時に `paf_cuda.dll`・`libvulkan.so.1`・`dstorage.dll` をプローブし、最良のバックエンドを選択。
- **Microsoft DirectStorage v1.2.2** (Windows): NVMe→ホストメモリ転送、および `DSTORAGE_REQUEST_DESTINATION_BUFFER` を使った真の NVMe→GPU DMA (CPU 関与なし)。
- **O(N) デルタエンジン**: インデックス内の SHA-256 ハッシュのみで比較 — ファイルデータの再読み込み不要。
- **バイナリパッチ PAF**: `paf_create_patch` が rsync スタイルのブロックデルタで UPDATED ファイルのみ含むコンパクトなパッチアーカイブを生成。`paf_patch_apply_atomic` が SHA-256 検証＋アトミックリネームで適用。
- **インデックス専用アーカイブ**: データブロックなし・メタデータのみの `.pafi` ファイルでリモートデルタ計算が可能。
- **マルチプラットフォーム**: Windows (GPU + DirectStorage)、Linux (Vulkan GPU)、Android ARM64。

## パフォーマンス (100,000 ファイル、RTX 2080)

| 方式 | 時間 | スループット | 備考 |
|:---|:---|:---|:---|
| **PAF GPU (CUDA + DirectStorage)** | **0.69s** | **143,904 files/sec** | RTX 2080 |
| PAF CPU | 1.12s | 89,285 files/sec | CPU SHA-256 フォールバック |
| TAR | 1.04s | 96,153 files/sec | Windows 標準 |
| ZIP (Deflate) | 10.45s | 9,569 files/sec | Windows 標準 |

> GPU アクセラレーション使用時、PAF は ZIP より約 15 倍高速。

## GPU 優先順位

```
CUDA (paf_cuda.dll)  →  Vulkan (libvulkan + paf_sha256.spv)  →  CPU SHA-256
```

初期化はすべて遅延実行 — 明示的な初期化呼び出し不要。  
Vulkan パスは `paf_sha256.spv`（`libpaf/src/paf_sha256.comp` からコンパイル）をカレントディレクトリまたは `/usr/lib/paf/` に必要とします。

## アーカイブフォーマット

```
[Header 32B] [Data Block] [Index Block N×64B] [Path Buffer]
```

| セクション | サイズ | 内容 |
|:---|:---|:---|
| Header | 32 B | マジック `PAF1`、バージョン、フラグ、ファイル数、インデックス/パスオフセット |
| Data Block | 可変 | 生ファイルデータの連続領域（`PAF_FLAG_INDEX_ONLY` 時は省略） |
| Index Block | N × 64 B | ファイルごとのメタデータ（下記参照） |
| Path Buffer | 可変 | UTF-8 ファイルパス（NUL なし、長さは index で管理） |

### Index Entry フィールド (64 bytes、固定長)

| オフセット | サイズ | フィールド | 説明 |
|:---|:---|:---|:---|
| 0 | 8 B | `path_buffer_offset` | Path Buffer 内バイトオフセット |
| 8 | 4 B | `path_length` | パスのバイト長 |
| 12 | 4 B | `flags` | `PAF_ENTRY_*` ビットマスク |
| 16 | 8 B | `data_offset` | Data Block 内バイトオフセット |
| 24 | 8 B | `data_size` | データのバイトサイズ |
| 32 | 32 B | `hash` | **SHA-256** — 唯一の整合性チェックサム |

> `PAF_ENTRY_BINARY_DELTA` の場合、`hash` はデルタ適用後のファイル（= 新ファイル）の SHA-256 です。

### ヘッダーフラグ (`paf_header_t.flags`)

| フラグ | 値 | 意味 |
|:---|:---|:---|
| `PAF_FLAG_INDEX_ONLY` | `0x02` | Data Block なし。インデックス専用スナップショット |

### エントリフラグ (`paf_index_entry_t.flags`)

| フラグ | 値 | 意味 |
|:---|:---|:---|
| `PAF_ENTRY_DELETED` | `0x04` | 削除済みファイル。`data_size = 0`、データなし |
| `PAF_ENTRY_BINARY_DELTA` | `0x08` | データブロックは PAFD バイナリデルタ |

### PAFD バイナリデルタフォーマット

```
[magic "PAFD" 4B] [命令数 4B] [旧ファイルサイズ 8B]
N × { [type 1B] [offset 8B] [size 4B] [data if LITERAL] }
```

| type | 意味 |
|:---|:---|
| `0` COPY | 旧ファイルの `offset` から `size` バイトをコピー |
| `1` LITERAL | 後続の `size` バイトをそのまま出力 |

4 KB ブロック単位で FNV-1a 64-bit ハッシュによるマッチングを行い、マッチしたブロックを COPY 命令に、差分を LITERAL 命令に変換します。

## デルタアップデート

PAF の各インデックスエントリには SHA-256 ハッシュが埋め込まれているため、**ファイルデータを再読み込みせずに O(N) で差分を検出**できます。  
ゲームアセットのアップデート配信やディレクトリ同期に使用できます。

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

### ワークフロー B — バイナリパッチ PAF（配布向け）

```
旧ディレクトリ ─────────────────────────────────────────┐
新ディレクトリ ─── paf_create_patch ──▶ patch.paf      │
                   ADDED   : 新ファイル全データ           │
                   UPDATED : PAFD バイナリデルタ          │
                   DELETED : マーカーのみ (data_size=0)  │
                                          │               ▼
                            paf_patch_apply_atomic(patch.paf, dst_dir)
                            旧ファイル + デルタ → .paf_stage
                            → SHA-256 検証 → atomic rename
```

### 差分ステータス

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

// 変更ファイルのみコピー
paf_patch_apply_from_dir("/game/v2", &delta, "/game/installed", NULL, NULL);
paf_delta_free(&delta);

// ── ワークフロー B: バイナリパッチ PAF ────────────────────────────────
// 配布サーバー側: コンパクトなパッチ PAF を生成
paf_create_patch("/game/v1", "/game/v2", "patch_v1_v2.paf", NULL, NULL);

// クライアント側: SHA-256 検証 + アトミックリネームで安全に適用
paf_patch_apply_atomic("patch_v1_v2.paf", "/game/installed", NULL, NULL);
```

`paf_delta_optimize_io` を呼ぶと差分エントリをオフセット順にソートし、シーケンシャルアクセスで I/O 効率を最大化できます。

## ディレクトリ構成

```
libpaf/
  include/              公開 C ヘッダー
    paf.h               フォーマット定義 (paf_header_t, paf_index_entry_t, フラグ)
    libpaf.h            主要 API (create/extract/patch)
    paf_delta.h         デルタ計算・パッチ適用 API
    paf_extractor.h     ランダムアクセス抽出 + GPU バッチ抽出
    paf_gpu_loader.h    実行時 GPU 検出 (CUDA / Vulkan / DirectStorage)
    paf_gpu.h           GPU 直接ロード (DirectStorage / D3D12)
    paf_vulkan.h        Vulkan コンピュートパイプライン API
  src/                  コアライブラリ (C)
    paf_sha256.comp     GLSL SHA-256 コンピュートシェーダー → paf_sha256.spv
    paf_vulkan.c        Vulkan コンピュートパイプライン (vulkan.h 依存なし)
    paf_binary_delta.c  rsync スタイルブロックデルタエンジン (PAFD フォーマット)
    win/                Windows 専用: CUDA カーネル、DirectStorage、D3D12 直接ロード
test/                   テストスイート (test_paf.c)
wasm/                   Emscripten バインディング
.github/workflows/      CI: Windows DLL、Linux .so、Android ARM64
```

## ビルド

### Linux / CI (CPU のみ)
```sh
gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -o libpaf.so
```

### Windows — CPU のみ (CI 互換)
```bat
cl /O2 /LD /Fe:libpaf.dll "-DLIBPAF_EXPORTS" "-DPAF_CI_BUILD" /Ilibpaf/include /Ilibpaf/src/win libpaf/src/*.c
```

### Windows — フル GPU (CUDA 13.2 + MSVC + glslangValidator)
```powershell
.\build_paf_gpu.ps1
```
`paf_sha256.comp` → `paf_sha256.spv` (glslangValidator が PATH にある場合)、`paf_cuda_kernels.cu` を nvcc でビルド、`paf_io_directstorage.cpp` + `paf_io_d3d12_direct.cpp` を cl でビルド、`d3d12.lib` とリンクして `libpaf.dll` を生成します。

### Android ARM64
```powershell
.\build_multi_platform.ps1
```

## 公開 API

| ヘッダー | 主なエクスポート |
|:---|:---|
| `libpaf/include/libpaf.h` | `paf_create_binary`, `paf_create_index_only`, `paf_extract_binary`, `paf_create_patch` |
| `libpaf/include/paf_delta.h` | `paf_delta_calculate`, `paf_delta_free`, `paf_delta_optimize_io`, `paf_patch_apply`, `paf_patch_apply_from_dir`, `paf_patch_apply_atomic` |
| `libpaf/include/paf_extractor.h` | `paf_extractor_open`, `paf_extractor_close`, `paf_extractor_get_file`, `paf_extractor_gpu_run` |
| `libpaf/include/paf_gpu_loader.h` | `paf_cuda_is_available`, `paf_vulkan_is_available`, `paf_dstorage_is_available` |
| `libpaf/include/paf_gpu.h` | `paf_gpu_direct_load`, `paf_gpu_direct_load_d3d12`, `paf_gpu_search_files` |
| `libpaf/include/paf_vulkan.h` | `paf_vulkan_init`, `paf_vulkan_hash_flat`, `paf_vulkan_cleanup` |

## デプロイ

タグ (`v*`) をプッシュすると GitHub Actions が Windows DLL、Linux .so、Android ARM64 `.so` をビルドし、[Releases](../../releases) ページに公開します。

---
Developed by Antigravity AI & Team.
