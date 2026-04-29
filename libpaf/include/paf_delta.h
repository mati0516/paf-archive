#ifndef PAF_DELTA_H
#define PAF_DELTA_H

#include "paf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 差分ステータス
 */
typedef enum {
    PAF_DELTA_ADDED,   // 新規追加
    PAF_DELTA_UPDATED, // 内容更新
    PAF_DELTA_DELETED  // 削除済み
} paf_delta_status_t;

/**
 * 差分エントリ: パッチ適用に必要なメタデータ
 */
typedef struct {
    char path[1024];           // ターゲットパス
    paf_delta_status_t status; // ステータス
    uint64_t new_offset;       // 新しいPAF内でのデータ位置 (Added/Updated時)
    uint64_t data_size;        // データサイズ
    uint8_t hash[32];          // SHA-256
} paf_delta_entry_t;

/**
 * Delta Index: 突合結果の全体集合
 */
typedef struct {
    paf_delta_entry_t* entries;
    uint32_t count;
} paf_delta_t;

/**
 * 2つのPAFインデックスを比較し、差分を抽出する
 * 
 * @param old_paf_path 旧PAFのパス
 * @param new_paf_path 新PAFのパス
 * @param out_delta 抽出結果（呼び出し側で解放が必要）
 * @return 0: 成功, その他: 失敗
 */
PAF_API int paf_delta_calculate(const char* old_paf_path, const char* new_paf_path, paf_delta_t* out_delta);

/**
 * 差分リソースを解放する
 */
PAF_API void paf_delta_free(paf_delta_t* delta);

/**
 * 差分をオフセット順にソートしてI/O効率を最適化する
 */
PAF_API void paf_delta_optimize_io(paf_delta_t* delta);

#ifdef __cplusplus
}
#endif

#endif // PAF_DELTA_H
