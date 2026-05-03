#include "paf_binary_delta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BDELTA_MAGIC    0x44464150u  /* "PAFD" little-endian */
#define BLOCK_SIZE      4096u
#define HT_CAP          (1u << 20)  /* 1M slots; handles files up to ~4 GB */
#define BDELTA_COPY     0
#define BDELTA_LITERAL  1
#define MAX_LIT_CHUNK   65535u      /* max bytes per LITERAL instruction */

/* ── grow buffer ─────────────────────────────────────────────────────────── */

typedef struct { uint8_t* data; size_t len; size_t cap; } buf_t;

static int buf_push(buf_t* b, const void* d, size_t n) {
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 65536;
        while (nc < b->len + n) nc *= 2;
        uint8_t* p = realloc(b->data, nc);
        if (!p) return -1;
        b->data = p; b->cap = nc;
    }
    memcpy(b->data + b->len, d, n);
    b->len += n;
    return 0;
}

/* ── FNV-1a 64-bit ──────────────────────────────────────────────────────── */

static uint64_t fnv1a_64(const uint8_t* data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++)
        h = (h ^ (uint64_t)data[i]) * 0x100000001b3ULL;
    return h;
}

/* ── open-addressing hash table (hash → block offset in old file) ─────── */

typedef struct { uint64_t hash; uint64_t offset; int used; } ht_slot_t;

static ht_slot_t* ht_alloc(void) {
    return (ht_slot_t*)calloc(HT_CAP, sizeof(ht_slot_t));
}

static void ht_insert(ht_slot_t* ht, uint64_t hash, uint64_t offset) {
    uint32_t idx = (uint32_t)(hash & (HT_CAP - 1));
    while (ht[idx].used && ht[idx].hash != hash)
        idx = (idx + 1) & (HT_CAP - 1);
    ht[idx].hash   = hash;
    ht[idx].offset = offset;
    ht[idx].used   = 1;
}

static int ht_find(const ht_slot_t* ht, uint64_t hash, uint64_t* out_off) {
    uint32_t idx = (uint32_t)(hash & (HT_CAP - 1));
    while (ht[idx].used) {
        if (ht[idx].hash == hash) { *out_off = ht[idx].offset; return 1; }
        idx = (idx + 1) & (HT_CAP - 1);
    }
    return 0;
}

/* ── instruction emitters ──────────────────────────────────────────────── */

static int emit_copy(buf_t* out, uint64_t old_off, uint32_t sz) {
    uint8_t type = BDELTA_COPY;
    return buf_push(out, &type, 1)
        || buf_push(out, &old_off, 8)
        || buf_push(out, &sz, 4);
}

static int emit_literal(buf_t* out, const uint8_t* data, size_t total) {
    while (total > 0) {
        uint32_t chunk = total > MAX_LIT_CHUNK ? MAX_LIT_CHUNK : (uint32_t)total;
        uint8_t  type  = BDELTA_LITERAL;
        uint64_t off   = 0;
        if (buf_push(out, &type, 1)
         || buf_push(out, &off,  8)
         || buf_push(out, &chunk, 4)
         || buf_push(out, data, chunk)) return -1;
        data  += chunk;
        total -= chunk;
    }
    return 0;
}

/* ── public: create delta ──────────────────────────────────────────────── */

uint8_t* paf_bdelta_create(const char* old_path, const char* new_path, size_t* out_size) {
    /* Read old file */
    FILE* f = fopen(old_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); size_t old_sz = (size_t)ftell(f); rewind(f);
    uint8_t* old_buf = old_sz ? (uint8_t*)malloc(old_sz) : NULL;
    if (old_sz) {
        if (!old_buf || fread(old_buf, 1, old_sz, f) != old_sz) {
            free(old_buf); fclose(f); return NULL;
        }
    }
    fclose(f);

    /* Read new file */
    f = fopen(new_path, "rb");
    if (!f) { free(old_buf); return NULL; }
    fseek(f, 0, SEEK_END); size_t new_sz = (size_t)ftell(f); rewind(f);
    uint8_t* new_buf = new_sz ? (uint8_t*)malloc(new_sz) : NULL;
    if (new_sz) {
        if (!new_buf || fread(new_buf, 1, new_sz, f) != new_sz) {
            free(new_buf); free(old_buf); fclose(f); return NULL;
        }
    }
    fclose(f);

    /* Build block hash table from old file (4 KB aligned blocks) */
    ht_slot_t* ht = ht_alloc();
    if (!ht) { free(old_buf); free(new_buf); return NULL; }
    for (size_t off = 0; off + BLOCK_SIZE <= old_sz; off += BLOCK_SIZE) {
        uint64_t h = fnv1a_64(old_buf + off, BLOCK_SIZE);
        ht_insert(ht, h, (uint64_t)off);
    }

    /* Generate instructions */
    buf_t instr  = {0};
    uint32_t cnt = 0;
    size_t pos   = 0;
    size_t lit_start = 0;

    while (pos < new_sz) {
        size_t remaining = new_sz - pos;

        if (remaining >= BLOCK_SIZE) {
            uint64_t h = fnv1a_64(new_buf + pos, BLOCK_SIZE);
            uint64_t old_off = 0;
            if (ht_find(ht, h, &old_off)
                && memcmp(new_buf + pos, old_buf + old_off, BLOCK_SIZE) == 0) {
                /* Flush pending literal */
                if (pos > lit_start) {
                    size_t lit_len = pos - lit_start;
                    emit_literal(&instr, new_buf + lit_start, lit_len);
                    /* Count emitted LITERAL chunks */
                    cnt += (uint32_t)((lit_len + MAX_LIT_CHUNK - 1) / MAX_LIT_CHUNK);
                }
                emit_copy(&instr, old_off, BLOCK_SIZE);
                cnt++;
                pos      += BLOCK_SIZE;
                lit_start = pos;
                continue;
            }
        }
        pos++;
    }
    /* Flush trailing literal */
    if (pos > lit_start) {
        size_t lit_len = pos - lit_start;
        emit_literal(&instr, new_buf + lit_start, lit_len);
        cnt += (uint32_t)((lit_len + MAX_LIT_CHUNK - 1) / MAX_LIT_CHUNK);
    }

    free(ht);
    free(old_buf);
    free(new_buf);

    /* Assemble final buffer: [magic 4B][cnt 4B][old_sz 8B][instructions] */
    buf_t out = {0};
    uint32_t magic    = BDELTA_MAGIC;
    uint64_t old_sz64 = (uint64_t)old_sz;
    if (buf_push(&out, &magic,    4)
     || buf_push(&out, &cnt,      4)
     || buf_push(&out, &old_sz64, 8)
     || buf_push(&out, instr.data, instr.len)) {
        free(instr.data); free(out.data); return NULL;
    }
    free(instr.data);

    *out_size = out.len;
    return out.data;
}

/* ── public: apply delta ───────────────────────────────────────────────── */

int paf_bdelta_apply(const uint8_t* delta, size_t delta_size,
                     const char* old_path, const char* out_path) {
    if (!delta || delta_size < 16) return -1;

    uint32_t magic;
    memcpy(&magic, delta, 4);
    if (magic != BDELTA_MAGIC) return -1;

    uint32_t instr_cnt;
    uint64_t old_file_sz;
    memcpy(&instr_cnt,    delta + 4, 4);
    memcpy(&old_file_sz,  delta + 8, 8);

    /* Read old file */
    uint8_t* old_buf = NULL;
    if (old_file_sz > 0) {
        FILE* f = fopen(old_path, "rb");
        if (!f) return -1;
        old_buf = (uint8_t*)malloc((size_t)old_file_sz);
        if (!old_buf) { fclose(f); return -1; }
        size_t got = fread(old_buf, 1, (size_t)old_file_sz, f);
        fclose(f);
        if (got != (size_t)old_file_sz) { free(old_buf); return -1; }
    }

    FILE* out = fopen(out_path, "wb");
    if (!out) { free(old_buf); return -1; }

    int ok = 1;
    size_t pos = 16;
    for (uint32_t i = 0; i < instr_cnt && ok; i++) {
        if (pos + 13 > delta_size) { ok = 0; break; }
        uint8_t  type;
        uint64_t offset;
        uint32_t sz;
        type = delta[pos++];
        memcpy(&offset, delta + pos, 8); pos += 8;
        memcpy(&sz,     delta + pos, 4); pos += 4;

        if (type == BDELTA_COPY) {
            if (!old_buf || offset + sz > old_file_sz) { ok = 0; break; }
            ok = fwrite(old_buf + (size_t)offset, 1, sz, out) == sz;
        } else { /* LITERAL */
            if (pos + sz > delta_size) { ok = 0; break; }
            ok = fwrite(delta + pos, 1, sz, out) == sz;
            pos += sz;
        }
    }

    fclose(out);
    free(old_buf);
    return ok ? 0 : -1;
}
