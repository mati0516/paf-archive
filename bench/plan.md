# ベンチマーク実施計画 (PAF vs TAR vs ZIP) - 詳細版

20万ファイルを用いた、アーカイブ「作成」および「展開（抽出）」のパフォーマンス比較。

## 1. ディレクトリ構造
成果物はすべて `/bench` 配下に作成。検証データの重複作成は行わない。

- `/bench/data`: 検証用ソースデータ（20万ファイル、計約1.6GB）
- `/bench/archives`: 作成されたアーカイブ（.paf, .tar, .zip）
- `/bench/extracted`: 展開テスト用ディレクトリ
- `/bench/results`: 計測データ (results.json)

## 2. 検証データ
- `/bench/data` が存在し、ファイル数が200,000個以上であれば再生成しない。
- 生成スクリプト: `bench/generate_data.py`

## 3. 計測シナリオ（逐次実行）
リソース競合を避けるため、各計測の合間に十分な待機時間を設ける。

### A. アーカイブ作成 (Creation)
1. **TAR**: `tar -cf`
2. **ZIP**: `tar -a -cf` (ZIP形式)
3. **PAF (CPU)**: CUDA/Vulkanを無効化した並列ハッシュ作成。
4. **PAF (GPU)**: CUDA/Vulkanを利用した並列ハッシュ作成。

### B. アーカイブ展開 (Extraction)
1. **TAR**: `tar -xf`
2. **ZIP**: `tar -xf`
3. **PAF (CPUのみ)**: 標準I/O + CPUハッシュ検証。
4. **PAF (GPUのみ)**: 標準I/O + CUDAハッシュ検証。
5. **PAF (GPU + DirectStorage)**: DirectStorage I/O + CUDAハッシュ検証。

## 4. 実施手順
1. 環境準備 (`libpaf.dll` のビルド、`/bench` フォルダ作成)。
2. データ生成。
3. `bench/bench_paf.c` のコンパイル。
   - `libpaf` の各モードを明示的に指定して呼び出すためのツール。
4. 逐次計測の実行。
5. 結果の分析とレポート生成。