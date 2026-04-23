#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libpaf/libpaf.h"

// JSから直接呼び出すためのラッパー関数

EMSCRIPTEN_KEEPALIVE
int wasm_paf_list(const char* archive_path) {
    PafList list;
    if (paf_list_binary(archive_path, &list) == 0) {
        // JS側に結果を渡すためにコンソール出力（後でJS側でインターセプト、あるいはJSONで返す等）
        // ここでは簡単な例として EM_ASM を使ってJSのコールバックを呼ぶか、文字列を返す手法を取る
        // より簡単には、標準出力に出したものをJSでキャプチャするか、JSON文字列を構築してポインタを返す
        
        // JSON形式の文字列を構築する（簡易版）
        char* json_out = malloc(1024 * 1024); // 1MBバッファ
        strcpy(json_out, "[");
        for (uint32_t i = 0; i < list.count; ++i) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\"path\":\"%s\", \"size\":%u}%s", 
                     list.entries[i].path, list.entries[i].size, 
                     (i == list.count - 1) ? "" : ",");
            strcat(json_out, entry);
        }
        strcat(json_out, "]");
        
        // EmscriptenのJavaScript環境に変数を渡す
        MAIN_THREAD_EM_ASM({
            if (typeof window.paf_on_list === 'function') {
                window.paf_on_list(UTF8ToString($0));
            }
        }, json_out);
        
        free(json_out);
        free_paf_list(&list);
        return 0;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int wasm_paf_extract_all(const char* archive_path, const char* out_dir) {
    // 仮想FS上に展開する
    return paf_extract_binary(archive_path, out_dir, 1); // overwrite=1 (FS上は常に上書き許容)
}

// 複数ファイルからPAFアーカイブを作成する関数
// paths_joined: "|"（パイプ）で区切られたファイルパスの文字列
EMSCRIPTEN_KEEPALIVE
int wasm_paf_create(const char* out_archive, const char* paths_joined) {
    if (!paths_joined || strlen(paths_joined) == 0) return -1;
    
    // 入力文字列をコピーして strtok で分割できるようにする
    char* paths_copy = strdup(paths_joined);
    const char* paths[1024]; // 最大1024ファイルとする
    int count = 0;
    
    char* token = strtok(paths_copy, "|");
    while (token != NULL && count < 1024) {
        paths[count++] = token;
        token = strtok(NULL, "|");
    }
    
    // PAF作成 (recursive_ignore=1)
    int result = paf_create_binary(out_archive, paths, count, ".pafignore", 1);
    
    free(paths_copy);
    return result;
}
