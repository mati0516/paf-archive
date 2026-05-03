#define LIBPAF_EXPORTS
#include "paf_delta.h"
#include "libpaf.h"
#include <stdlib.h>
#include <string.h>

/**
 * Hash table entry for path matching
 */
typedef struct {
    const char* path;
    uint32_t index_in_new;
    int used;
} hash_node_t;

/**
 * DJB2 hash function
 */
static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

int paf_delta_calculate(const char* old_paf_path, const char* new_paf_path, paf_delta_t* out_delta) {
    if (!old_paf_path || !new_paf_path || !out_delta) return -1;

    PafList old_list, new_list;
    if (paf_list_binary(old_paf_path, &old_list) != 0) return -1;
    if (paf_list_binary(new_paf_path, &new_list) != 0) {
        free_paf_list(&old_list);
        return -1;
    }

    uint32_t map_size = (new_list.count > 0) ? (new_list.count * 4) : 16;
    hash_node_t* map = (hash_node_t*)calloc(map_size, sizeof(hash_node_t));
    if (!map) {
        free_paf_list(&old_list);
        free_paf_list(&new_list);
        return -1;
    }
    
    for (uint32_t i = 0; i < new_list.count; i++) {
        uint32_t h = hash_string(new_list.entries[i].path) % map_size;
        while (map[h].path != NULL) h = (h + 1) % map_size;
        map[h].path = new_list.entries[i].path;
        map[h].index_in_new = i;
        map[h].used = 0;
    }

    paf_delta_entry_t* results = (paf_delta_entry_t*)malloc(sizeof(paf_delta_entry_t) * (old_list.count + new_list.count));
    if (!results) {
        free(map);
        free_paf_list(&old_list);
        free_paf_list(&new_list);
        return -1;
    }
    uint32_t delta_count = 0;

    for (uint32_t i = 0; i < old_list.count; i++) {
        const char* old_path = old_list.entries[i].path;
        uint32_t h = hash_string(old_path) % map_size;
        int found = 0;
        
        while (map[h].path != NULL) {
            if (strcmp(map[h].path, old_path) == 0) {
                found = 1;
                map[h].used = 1;
                if (memcmp(old_list.entries[i].hash, new_list.entries[map[h].index_in_new].hash, 32) != 0) {
                    paf_delta_entry_t* d = &results[delta_count++];
                    strncpy(d->path, old_path, 1023);
                    d->path[1023] = '\0';
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
            paf_delta_entry_t* d = &results[delta_count++];
            strncpy(d->path, old_path, 1023);
            d->path[1023] = '\0';
            d->status = PAF_DELTA_DELETED;
            d->new_offset = 0;
            d->data_size = 0;
        }
    }

    for (uint32_t i = 0; i < map_size; i++) {
        if (map[i].path != NULL && !map[i].used) {
            uint32_t idx = map[i].index_in_new;
            paf_delta_entry_t* d = &results[delta_count++];
            strncpy(d->path, new_list.entries[idx].path, 1023);
            d->path[1023] = '\0';
            d->status = PAF_DELTA_ADDED;
            d->new_offset = new_list.entries[idx].offset;
            d->data_size = new_list.entries[idx].size;
            memcpy(d->hash, new_list.entries[idx].hash, 32);
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
    qsort(delta->entries, delta->count, sizeof(paf_delta_entry_t), compare_delta_offsets);
}
