#define LIBPAF_EXPORTS
#include "paf_delta.h"
#include "paf_binary_delta.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(p) _mkdir(p)
#define UNLINK(p) _unlink(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#define UNLINK(p) unlink(p)
#endif

static void make_parent_dirs(const char* filepath) {
    char buf[1024];
    strncpy(buf, filepath, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char* p = buf + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            MKDIR(buf);
            *p = '/';
        }
    }
}

static int copy_file_raw(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return -1;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    uint8_t buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    }
    fclose(in);
    fclose(out);
    return ok ? 0 : -1;
}

int paf_patch_apply_from_dir(const char* new_src_dir, const paf_delta_t* delta,
                               const char* dst_dir, paf_progress_fn progress, void* user_data) {
    if (!new_src_dir || !delta || !dst_dir) return -1;

    int errors = 0;
    for (uint32_t i = 0; i < delta->count; i++) {
        const paf_delta_entry_t* e = &delta->entries[i];

        char fullpath[1024];
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", dst_dir, e->path) >= (int)sizeof(fullpath)) {
            fprintf(stderr, "paf_patch_apply_from_dir: path too long: %s\n", e->path);
            errors++;
        } else if (e->status == PAF_DELTA_DELETED) {
            if (UNLINK(fullpath) != 0) {
                fprintf(stderr, "paf_patch_apply_from_dir: failed to delete: %s\n", fullpath);
                errors++;
            }
        } else {
            make_parent_dirs(fullpath);
            char srcpath[1024];
            if (snprintf(srcpath, sizeof(srcpath), "%s/%s", new_src_dir, e->path) >= (int)sizeof(srcpath) ||
                copy_file_raw(srcpath, fullpath) != 0) {
                fprintf(stderr, "paf_patch_apply_from_dir: failed to copy: %s\n", e->path);
                errors++;
            }
        }

        if (progress) progress(i + 1, delta->count, e->path, user_data);
    }

    return errors > 0 ? -errors : 0;
}

/* Compute SHA-256 of a file into out[32]. Returns 0 on success. */
static int hash_file(const char* path, uint8_t* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    sha256_context_t ctx;
    sha256_init(&ctx);
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        sha256_update(&ctx, buf, n);
    fclose(f);
    sha256_final(&ctx, out);
    return 0;
}

int paf_patch_apply(const char* new_paf_path, const paf_delta_t* delta,
                    const char* dst_dir, paf_progress_fn progress, void* user_data) {
    if (!new_paf_path || !delta || !dst_dir) return -1;

    int errors = 0;
    for (uint32_t i = 0; i < delta->count; i++) {
        const paf_delta_entry_t* e = &delta->entries[i];

        char fullpath[1024];
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", dst_dir, e->path) >= (int)sizeof(fullpath)) {
            fprintf(stderr, "paf_patch_apply: path too long, skipping: %s\n", e->path);
            errors++;
        } else if (e->status == PAF_DELTA_DELETED) {
            if (UNLINK(fullpath) != 0) {
                fprintf(stderr, "paf_patch_apply: failed to delete: %s\n", fullpath);
                errors++;
            }
        } else {
            make_parent_dirs(fullpath);
            if (paf_extract_file(new_paf_path, e->path, fullpath) != 0) {
                fprintf(stderr, "paf_patch_apply: failed to extract: %s\n", e->path);
                errors++;
            }
        }

        if (progress) progress(i + 1, delta->count, e->path, user_data);
    }

    return errors > 0 ? -errors : 0;
}

/* ── path safety check ──────────────────────────────────────────────────── */
static int paf_is_safe_path(const char* path) {
    if (!path || path[0] == '\0' || path[0] == '/' || path[0] == '\\') return 0;
    if (strstr(path, "..")) return 0;
    return 1;
}

int paf_patch_apply_atomic(const char* patch_paf, const char* dst_dir,
                            paf_progress_fn progress, void* user_data) {
    if (!patch_paf || !dst_dir) return -1;

    FILE* fp = fopen(patch_paf, "rb");
    if (!fp) return -1;

    paf_header_t hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1 ||
        memcmp(hdr.magic, PAF_MAGIC, 4) != 0) {
        fclose(fp); return -2;
    }

    paf_index_entry_t* idx =
        (paf_index_entry_t*)malloc(sizeof(paf_index_entry_t) * hdr.file_count);
    if (!idx) { fclose(fp); return -1; }

    fseek(fp, (long)hdr.index_offset, SEEK_SET);
    if (fread(idx, sizeof(paf_index_entry_t), hdr.file_count, fp) != hdr.file_count) {
        free(idx); fclose(fp); return -3;
    }

    int errors = 0;

    for (uint32_t i = 0; i < hdr.file_count; i++) {
        char rel_path[1024] = {0};
        uint32_t plen = idx[i].path_length;
        if (plen == 0 || plen >= sizeof(rel_path)) { errors++; continue; }
        fseek(fp, (long)(hdr.path_offset + idx[i].path_buffer_offset), SEEK_SET);
        if (fread(rel_path, 1, plen, fp) != plen) { errors++; continue; }
        rel_path[plen] = '\0';

        if (!paf_is_safe_path(rel_path)) { errors++; continue; }

        char dst_path[1024];
        if (snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, rel_path) >=
            (int)sizeof(dst_path)) { errors++; continue; }

        /* DELETED */
        if (idx[i].flags & PAF_ENTRY_DELETED) {
            if (UNLINK(dst_path) != 0) {
                fprintf(stderr, "paf_patch_apply_atomic: delete failed: %s\n", rel_path);
                errors++;
            }
            if (progress) progress(i + 1, hdr.file_count, rel_path, user_data);
            continue;
        }

        /* Stage path: dst_path + ".paf_stage" */
        char stage_path[1024 + 12];
        snprintf(stage_path, sizeof(stage_path), "%s.paf_stage", dst_path);
        make_parent_dirs(stage_path);

        /* BINARY_DELTA */
        if (idx[i].flags & PAF_ENTRY_BINARY_DELTA) {
            uint64_t dsz = idx[i].data_size;
            uint8_t* dbuf = (uint8_t*)malloc((size_t)dsz);
            if (!dbuf) { errors++; continue; }
            fseek(fp, (long)(sizeof(paf_header_t) + idx[i].data_offset), SEEK_SET);
            if (fread(dbuf, 1, (size_t)dsz, fp) != (size_t)dsz) {
                free(dbuf); errors++; continue;
            }
            int apply_ok = paf_bdelta_apply(dbuf, (size_t)dsz, dst_path, stage_path);
            free(dbuf);
            if (apply_ok != 0) {
                fprintf(stderr, "paf_patch_apply_atomic: delta apply failed: %s\n", rel_path);
                UNLINK(stage_path); errors++; continue;
            }
        } else {
            /* Normal: extract data to stage file */
            fseek(fp, (long)(sizeof(paf_header_t) + idx[i].data_offset), SEEK_SET);
            FILE* sf = fopen(stage_path, "wb");
            if (!sf) { errors++; continue; }
            uint8_t buf[65536];
            uint64_t rem = idx[i].data_size;
            int io_ok = 1;
            while (rem > 0 && io_ok) {
                size_t chunk = rem > sizeof(buf) ? sizeof(buf) : (size_t)rem;
                size_t got = fread(buf, 1, chunk, fp);
                if (got == 0) { io_ok = 0; break; }
                if (fwrite(buf, 1, got, sf) != got) io_ok = 0;
                rem -= (uint64_t)got;
            }
            fclose(sf);
            if (!io_ok) { UNLINK(stage_path); errors++; continue; }
        }

        /* Verify SHA-256 */
        uint8_t computed[32];
        if (hash_file(stage_path, computed) != 0 ||
            memcmp(computed, idx[i].hash, 32) != 0) {
            fprintf(stderr, "paf_patch_apply_atomic: hash mismatch: %s\n", rel_path);
            UNLINK(stage_path); errors++; continue;
        }

        /* Atomic rename into place */
        if (rename(stage_path, dst_path) != 0) {
            fprintf(stderr, "paf_patch_apply_atomic: rename failed: %s\n", rel_path);
            UNLINK(stage_path); errors++;
        }

        if (progress) progress(i + 1, hdr.file_count, rel_path, user_data);
    }

    free(idx);
    fclose(fp);
    return errors > 0 ? -errors : 0;
}
