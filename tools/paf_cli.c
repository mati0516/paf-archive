/*
 * paf_cli.c -- Cross-platform CLI for the PAF archive format
 *
 * Usage:
 *   paf list   <archive.paf>
 *   paf info   <archive.paf>
 *   paf extract <archive.paf> [outdir]
 *   paf create  <output.paf> <dir>
 *   paf verify  <archive.paf>
 *   paf delta   <old.paf> <new.paf>
 *   paf help
 *
 * Build (Linux / macOS):
 *   gcc -O2 -Ilibpaf/include libpaf/src/*.c tools/paf_cli.c -lpthread -o bin/paf
 *
 * Build (Windows, MSVC):
 *   cl /O2 /Ilibpaf\include tools\paf_cli.c lib\libpaf.lib /Fe:bin\paf.exe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "libpaf.h"
#include "paf.h"
#include "paf_extractor.h"
#include "paf_delta.h"
#include "sha256.h"

#ifdef _WIN32
#  include <windows.h>
#  define PATH_SEP '\\'
#else
#  include <limits.h>
#  define PATH_SEP '/'
#  ifndef PATH_MAX
#    define PATH_MAX 4096
#  endif
#endif

/* ── Portable path helpers ────────────────────────────────────────────────── */

/* Copy at most dst_size-1 bytes; always NUL-terminates. */
static void safe_strncpy(char* dst, const char* src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

/* Return pointer to the last path component (after the last / or \). */
static const char* basename_of(const char* path) {
    const char* sl = strrchr(path, '/');
    const char* bs = strrchr(path, '\\');
    const char* sep = (sl > bs) ? sl : bs;
    return sep ? sep + 1 : path;
}

/* Strip the file extension from path (in-place). */
static void strip_ext(char* path) {
    char* dot = strrchr(path, '.');
    char* sep = strrchr(path, '/');
#ifdef _WIN32
    char* bsep = strrchr(path, '\\');
    if (bsep > sep) sep = bsep;
#endif
    if (dot && dot > sep) *dot = '\0';
}

/* ── Number formatting helper ─────────────────────────────────────────────── */

/* Format a uint64_t with thousands separators into buf. */
static void fmt_size(char* buf, size_t buf_size, uint64_t n) {
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)n);
    int out_pos = 0;
    for (int i = 0; i < len && out_pos < (int)buf_size - 1; i++) {
        int remaining = len - i;
        if (remaining > 1 && remaining % 3 == 1 && i > 0) {
            buf[out_pos++] = ',';
        }
        buf[out_pos++] = tmp[i];
    }
    buf[out_pos] = '\0';
}

/* ── Hash hex formatting ──────────────────────────────────────────────────── */

/* Write the first `bytes` bytes of hash as lowercase hex into dst.
   dst must hold at least 2*bytes+1 characters. */
static void hex_prefix(char* dst, const uint8_t* hash, size_t bytes) {
    static const char hx[] = "0123456789abcdef";
    for (size_t i = 0; i < bytes; i++) {
        dst[2 * i]     = hx[hash[i] >> 4];
        dst[2 * i + 1] = hx[hash[i] & 0xf];
    }
    dst[2 * bytes] = '\0';
}

/* ── cmd_list ─────────────────────────────────────────────────────────────── */

static int cmd_list(const char* paf_path) {
    PafList list;
    memset(&list, 0, sizeof(list));

    if (paf_list_binary(paf_path, &list) != 0) {
        fprintf(stderr, "Error: cannot open archive: %s\n", paf_path);
        return 1;
    }

    uint64_t total = 0;
    for (uint32_t i = 0; i < list.count; i++) {
        total += list.entries[i].size;
    }

    char total_str[32];
    fmt_size(total_str, sizeof(total_str), total);
    printf("Files: %u  (total %s bytes)\n", list.count, total_str);

    for (uint32_t i = 0; i < list.count; i++) {
        PafEntry* e = &list.entries[i];
        char size_str[32];
        char hash_hex[17]; /* 8 bytes = 16 hex chars + NUL */
        fmt_size(size_str, sizeof(size_str), e->size);
        hex_prefix(hash_hex, e->hash, 8);
        printf("  %-40s  %10s B  %s...\n", e->path, size_str, hash_hex);
    }

    free_paf_list(&list);
    return 0;
}

/* ── cmd_info ─────────────────────────────────────────────────────────────── */

static int cmd_info(const char* paf_path) {
    paf_header_t hdr;
    if (paf_extractor_peek_header(paf_path, &hdr) != 0) {
        fprintf(stderr, "Error: cannot read archive header: %s\n", paf_path);
        return 1;
    }

    /* Collect total size from the list */
    PafList list;
    memset(&list, 0, sizeof(list));
    uint64_t total = 0;
    if (paf_list_binary(paf_path, &list) == 0) {
        for (uint32_t i = 0; i < list.count; i++) {
            total += list.entries[i].size;
        }
        free_paf_list(&list);
    }

    char magic[5];
    memcpy(magic, hdr.magic, 4);
    magic[4] = '\0';

    char total_str[32];
    fmt_size(total_str, sizeof(total_str), total);

    printf("Archive : %s\n", paf_path);
    printf("Magic   : %s\n", magic);
    printf("Version : %u\n", hdr.version);
    printf("Files   : %u\n", hdr.file_count);
    printf("Size    : %s bytes (uncompressed)\n", total_str);
    printf("Flags   : 0x%08X", hdr.flags);
    if (hdr.flags & PAF_FLAG_INDEX_ONLY)
        printf("  [INDEX_ONLY]");
    if (hdr.flags & PAF_EXTRACT_SMART_OVERWRITE)
        printf("  [SMART_OVERWRITE]");
    printf("\n");
    printf("Index @ : 0x%llX\n", (unsigned long long)hdr.index_offset);
    printf("Paths @ : 0x%llX\n", (unsigned long long)hdr.path_offset);

    return 0;
}

/* ── cmd_extract ──────────────────────────────────────────────────────────── */

static int cmd_extract(const char* paf_path, const char* out_dir_arg) {
    char out_dir[PATH_MAX];

    if (out_dir_arg) {
        safe_strncpy(out_dir, out_dir_arg, sizeof(out_dir));
    } else {
        /* Default: ./<archivename_without_ext>/ */
        const char* base = basename_of(paf_path);
        safe_strncpy(out_dir, base, sizeof(out_dir));
        strip_ext(out_dir);
        if (out_dir[0] == '\0') {
            safe_strncpy(out_dir, "paf_out", sizeof(out_dir));
        }
    }

    printf("Extracting to: %s\n", out_dir);

    int rc = paf_extract_binary(paf_path, out_dir, 1 /* overwrite */);
    if (rc != 0) {
        fprintf(stderr, "Error: extraction failed (code %d)\n", rc);
        return 1;
    }

    printf("Done.\n");
    return 0;
}

/* ── cmd_create ───────────────────────────────────────────────────────────── */

static int cmd_create(const char* out_paf, const char* input_dir) {
    const char* inputs[1];
    inputs[0] = input_dir;

    printf("Creating: %s  (from %s)\n", out_paf, input_dir);

    int rc = paf_create_binary(out_paf, inputs, 1, NULL, 1);
    if (rc != 0) {
        fprintf(stderr, "Error: create failed (code %d)\n", rc);
        return 1;
    }

    printf("Done.\n");
    return 0;
}

/* ── cmd_verify ───────────────────────────────────────────────────────────── */

static int cmd_verify(const char* paf_path) {
    paf_extractor_t ext;
    memset(&ext, 0, sizeof(ext));

    if (paf_extractor_open(&ext, paf_path) != 0) {
        fprintf(stderr, "Error: cannot open archive: %s\n", paf_path);
        return 1;
    }

    uint32_t n = ext.header.file_count;
    printf("Verifying %u files...\n", n);

    int errors = 0;
    for (uint32_t i = 0; i < n; i++) {
        char path[1024];
        uint8_t* data = NULL;
        uint64_t size = 0;

        int rc = paf_extractor_get_file(&ext, i, path, sizeof(path), &data, &size);
        if (rc != 0) {
            /* path may not be filled on error; use index as fallback */
            fprintf(stderr, "  error  [%u] (read error %d)\n", i, rc);
            errors++;
            continue;
        }

        /* data == NULL means the extractor detected an identical existing file
           (smart-overwrite). In that case we cannot re-verify, so skip. */
        if (!data) {
            printf("  -  %s  (skipped, up-to-date)\n", path);
            continue;
        }

        /* Compute SHA-256 of extracted data and compare against stored hash */
        sha256_context_t ctx;
        uint8_t computed[32];
        sha256_init(&ctx);
        sha256_update(&ctx, data, (size_t)size);
        sha256_final(&ctx, computed);
        free(data);

        /* Retrieve the stored hash directly from the index entry */
        const uint8_t* stored = ext.entries[i].hash;

        if (memcmp(computed, stored, 32) == 0) {
            printf("  ✓  %s\n", path);
        } else {
            printf("  ✗  %s  (HASH MISMATCH)\n", path);
            errors++;
        }
    }

    paf_extractor_close(&ext);

    if (errors == 0) {
        printf("All %u files OK.\n", n);
        return 0;
    } else {
        printf("%d file(s) failed verification.\n", errors);
        return 1;
    }
}

/* ── cmd_delta ────────────────────────────────────────────────────────────── */

static int cmd_delta(const char* old_paf, const char* new_paf) {
    paf_delta_t delta;
    memset(&delta, 0, sizeof(delta));

    int rc = paf_delta_calculate(old_paf, new_paf, &delta);
    if (rc != 0) {
        fprintf(stderr, "Error: delta calculation failed (code %d)\n", rc);
        return 1;
    }

    uint32_t added = 0, updated = 0, deleted = 0;

    for (uint32_t i = 0; i < delta.count; i++) {
        paf_delta_entry_t* e = &delta.entries[i];
        switch (e->status) {
            case PAF_DELTA_ADDED:
                printf("  ADDED    %s\n", e->path);
                added++;
                break;
            case PAF_DELTA_UPDATED:
                printf("  UPDATED  %s\n", e->path);
                updated++;
                break;
            case PAF_DELTA_DELETED:
                printf("  DELETED  %s\n", e->path);
                deleted++;
                break;
        }
    }

    paf_delta_free(&delta);

    printf("%u added, %u updated, %u deleted\n", added, updated, deleted);
    return 0;
}

/* ── cmd_help ─────────────────────────────────────────────────────────────── */

static void cmd_help(const char* prog) {
    printf("Usage: %s <command> [args...]\n\n", prog);
    printf("Commands:\n");
    printf("  list   <archive.paf>              List files (path, size, SHA-256 prefix)\n");
    printf("  info   <archive.paf>              Show archive metadata\n");
    printf("  extract <archive.paf> [outdir]    Extract all files (default: ./<name>/)\n");
    printf("  create  <output.paf> <dir>        Create archive from directory\n");
    printf("  verify  <archive.paf>             Verify file SHA-256 hashes\n");
    printf("  delta   <old.paf> <new.paf>       Show diff between two archives\n");
    printf("  help                              Show this help\n");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    const char* prog = (argc > 0) ? basename_of(argv[0]) : "paf";

    if (argc < 2) {
        cmd_help(prog);
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "list") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: %s list <archive.paf>\n", prog); return 1; }
        return cmd_list(argv[2]);

    } else if (strcmp(cmd, "info") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: %s info <archive.paf>\n", prog); return 1; }
        return cmd_info(argv[2]);

    } else if (strcmp(cmd, "extract") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: %s extract <archive.paf> [outdir]\n", prog); return 1; }
        return cmd_extract(argv[2], argc >= 4 ? argv[3] : NULL);

    } else if (strcmp(cmd, "create") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: %s create <output.paf> <dir>\n", prog); return 1; }
        return cmd_create(argv[2], argv[3]);

    } else if (strcmp(cmd, "verify") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: %s verify <archive.paf>\n", prog); return 1; }
        return cmd_verify(argv[2]);

    } else if (strcmp(cmd, "delta") == 0) {
        if (argc < 4) { fprintf(stderr, "Usage: %s delta <old.paf> <new.paf>\n", prog); return 1; }
        return cmd_delta(argv[2], argv[3]);

    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        cmd_help(prog);
        return 0;

    } else {
        fprintf(stderr, "Unknown command: %s\n\n", cmd);
        cmd_help(prog);
        return 1;
    }
}
