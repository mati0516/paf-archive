#define LIBPAF_EXPORTS
#include "paf_delta.h"
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
