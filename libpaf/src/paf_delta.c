#define LIBPAF_EXPORTS
#include "paf_delta.h"
#include "libpaf.h"
#include <stdlib.h>
#include <string.h>

/**
 * 簡易ハッシュテーブル用エントリ
 */
typedef struct {
    const char* path;
    uint32_t index_in_new;
    int used;
} hash_node_t;

/**
 * シンプルなDJB2ハッシュ関数
 */
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

int paf_delta_calculate(const char* old_paf_path, const char* new_paf_path, paf_delta_t* out_delta) {
    if (!old_paf_path || !new_paf_path || !out_delta) return -1;

    // 1. 両方のPAFからINDEXリストを取得
    // ※実体ファイルは開くが、スキャン（GetFiles）はせず、ヘッダー/INDEXのみ読む
    PafList old_list, new_list;
    if (paf_list_binary(old_paf_path, &old_list) != 0) return -1;
    if (paf_list_binary(new_paf_path, &new_list) != 0) return -1;

    // 2. ハッシュマップの初期化 (New PAF用)
    uint32_t map_size = new_list.count * 2; // 負荷率0.5
    hash_node_t* map = (hash_node_t*)calloc(map_size, sizeof(hash_node_t));
    
    for (uint32_t i = 0; i < new_list.count; i++) {
        uint32_t h = hash_string(new_list.entries[i].path) % map_size;
        while (map[h].path != NULL) h = (h + 1) % map_size; // 線形探索
        map[h].path = new_list.entries[i].path;
        map[h].index_in_new = i;
        map[h].used = 0;
    }

    // 3. 差分抽出 (最大サイズで一旦確保)
    paf_delta_entry_t* results = (paf_delta_entry_t*)malloc(sizeof(paf_delta_entry_t) * (old_list.count + new_list.count));
    uint32_t delta_count = 0;

    // A: 旧エントリを走査して 更新(UPDATED) または 削除(DELETED) を判定
    for (uint32_t i = 0; i < old_list.count; i++) {
        const char* old_path = old_list.entries[i].path;
        uint32_t h = hash_string(old_path) % map_size;
        int found = 0;
        
        while (map[h].path != NULL) {
            if (strcmp(map[h].path, old_path) == 0) {
                found = 1;
                map[h].used = 1; // 照合済み
                // ハッシュ比較 (SHA-256)
                if (memcmp(old_list.entries[i].hash, new_list.entries[map[h].index_in_new].hash, 32) != 0) {
                    // UPDATED
                    paf_delta_entry_t* d = &results[delta_count++];
                    strcpy(d->path, old_path);
                    d->status = PAF_DELTA_UPDATED;
                    d->new_offset = new_list.entries[map[h].index_in_new].offset;
                    d->data_size = new_list.entries[map[h].index_in_new].size;
                    memcpy(d->hash, new_list.entries[map[h].index_in_new].hash, 32);
                }
                break;
            }
            h = (h + 1) % map_size;
        }

        if (!found) {
            // DELETED
            paf_delta_entry_t* d = &results[delta_count++];
            strcpy(d->path, old_path);
            d->status = PAF_DELTA_DELETED;
            d->new_offset = 0;
            d->data_size = 0;
        }
    }

    // B: マップに残った(usedでない)エントリは 追加(ADDED)
    for (uint32_t i = 0; i < map_size; i++) {
        if (map[i].path != NULL && !map[i].used) {
            uint32_t idx = map[i].index_in_new;
            paf_delta_entry_t* d = &results[delta_count++];
            strcpy(d->path, new_list.entries[idx].path);
            d->status = PAF_DELTA_ADDED;
            d->new_offset = new_list.entries[idx].offset;
            d->data_size = new_list.entries[idx].size;
            memcpy(d->hash, new_list.entries[idx].crc32, 32);
        }
    }

    out_delta->entries = results;
    out_delta->count = delta_count;

    free(map);
    free_paf_list(&old_list);
    free_paf_list(&new_list);
    return 0;
}

void paf_delta_free(paf_delta_t* delta) {
    if (delta && delta->entries) {
        free(delta->entries);
        delta->entries = NULL;
        delta->count = 0;
    }
}

static int compare_delta_offsets(const void* a, const void* b) {
    const paf_delta_entry_t* ea = (const paf_delta_entry_t*)a;
    const paf_delta_entry_t* eb = (const paf_delta_entry_t*)b;
    if (ea->new_offset < eb->new_offset) return -1;
    if (ea->new_offset > eb->new_offset) return 1;
    return 0;
}

void paf_delta_optimize_io(paf_delta_t* delta) {
    if (!delta || !delta->entries || delta->count == 0) return;
    // オフセット順にソート (DELETEDはオフセット0なので先頭に来る)
    qsort(delta->entries, delta->count, sizeof(paf_delta_entry_t), compare_delta_offsets);
}
