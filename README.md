# Parallel Archive Format (PAF)

PAF は、**GPU 加速** と **Microsoft DirectStorage** をフル活用した、次世代の超高速非圧縮アーカイブフォーマットです。
既存の ZIP や TAR を圧倒するスループットを実現し、大量のファイルを瞬時にパッケージングすることに特化しています。

## 🚀 主な特徴

- **GPU 加速ハッシュ計算**: NVIDIA CUDA を使用し、SHA-256 ハッシュを数万ファイル単位で並列計算。
- **Microsoft DirectStorage v1.2.2**: Windows 環境において、NVMe ストレージから直接 GPU/メモリへデータを転送し、I/O ボトルネックを解消。
- **圧倒的なパフォーマンス**: 10万ファイル超のアーカイブ作成において **140,000 files/sec** 以上のスループットを達成。
- **マルチプラットフォーム**: 
  - **Windows**: GPU 加速 & DirectStorage フル対応
  - **Linux (WSL)**: 高速 C コアによる安定動作
  - **Android (ARM64)**: モバイル環境での高速アーカイブ
- **ゼロコピー・アーキテクチャ**: 内部バッファの最適化により、メモリコピーを最小限に抑制。

## 📁 プロジェクト構造

- `libpaf/include`: 公開ヘッダーファイル
- `libpaf/src`: 共通コアロジック (C)
- `libpaf/src/win`: Windows 固有の高速化コード (CUDA, DirectStorage)
- `test`: ベンチマークおよびテスト用ソース
- `bench`: ベンチマーク結果および実行データ (Git 除外対象)
- `.github/workflows`: 自動ビルド & リリース環境 (Windows, Linux, Android)

## 🛠️ ビルド方法

### Windows (GPU 加速版)
CUDA Toolkit 13.2+ と Visual Studio が必要です。
```powershell
.\build_paf_gpu.ps1
```

### マルチプラットフォーム (Android / Linux)
Android NDK および WSL (Ubuntu) が必要です。
```powershell
.\build_multi_platform.ps1
```

## 📈 ベンチマーク

`bench/` フォルダ内の `benchmark_win.exe` を実行することで、あなたの環境での PAF の実力を計測できます。

## 📦 デプロイ

GitHub にタグ（例：`v1.0.0`）を Push すると、GitHub Actions が自動的に各プラットフォーム向けのライブラリをビルドし、[Releases](../../releases) ページに公開します。

---
Developed by Antigravity AI & Team.
