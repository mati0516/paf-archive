#ifndef LIBPAF_H
#define LIBPAF_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* path;      // UTF-8パス（NULL終端）
    uint32_t size;   // ファイルサイズ
    uint32_t offset; // データブロック内のオフセット
} PafEntry;

typedef struct {
    PafEntry* entries; // ファイル一覧
    uint32_t count;    // ファイル数
} PafList;

//// API ////

// アーカイブを作成（paths: UTF-8のファイル/フォルダパスの配列）
int paf_create(const char* out_paf_path, const char** paths, int path_count);

// 一覧を取得（呼び出し元で free_paf_list() 必須）
int paf_list(const char* paf_path, PafList* out_list);

// 単一ファイルを抽出（ファイル名はリストで取得できる）
int paf_extract_file(const char* paf_path, const char* file_name, const char* out_path);

// 全ファイルを抽出（出力フォルダに展開）
int paf_extract_all(const char* paf_path, const char* output_dir);

// 後始末
void free_paf_list(PafList* list);

#ifdef __cplusplus
}
#endif

#endif // LIBPAF_H
