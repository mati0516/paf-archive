/*
 * paf_extract.exe  --  GPU-accelerated PAF archive extractor
 *
 * Usage:
 *   paf_extract <archive.paf> [output_dir]
 *
 * If output_dir is omitted, files are extracted into a folder named after
 * the archive (without extension) in the same directory as the .paf file.
 *
 * Example:
 *   paf_extract game_data.paf              -> extracts to .\game_data\
 *   paf_extract game_data.paf C:\assets   -> extracts to C:\assets\
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "libpaf.h"
#include "paf_extractor.h"

/* ---- path helpers -------------------------------------------------------- */

/* Resolve the absolute path of `src` into `buf` (size `len`). */
static int abs_path(const char* src, char* buf, size_t len) {
    return GetFullPathNameA(src, (DWORD)len, buf, NULL) != 0;
}

/* Strip the file extension from `path` in place. */
static void strip_ext(char* path) {
    char* dot  = strrchr(path, '.');
    char* bk   = strrchr(path, '\\');
    char* sl   = strrchr(path, '/');
    char* sep  = (bk > sl) ? bk : sl;
    if (dot && dot > sep) *dot = '\0';
}

/* Replace forward slashes with backslashes. */
static void to_backslash(char* p) {
    for (; *p; p++) if (*p == '/') *p = '\\';
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    printf("=== PAF Extractor (GPU-accelerated) ===\n\n");

    if (argc < 2) {
        printf("Usage: paf_extract <archive.paf> [output_dir]\n\n");
        printf("  archive.paf   Path to the .paf archive to extract.\n");
        printf("  output_dir    Destination folder (default: <archive name>\\)\n\n");
        printf("Press any key to exit...\n");
        (void)getchar();
        return 1;
    }

    /* Resolve input path */
    char paf_path[MAX_PATH];
    if (!abs_path(argv[1], paf_path, sizeof(paf_path))) {
        strncpy(paf_path, argv[1], sizeof(paf_path) - 1);
        paf_path[sizeof(paf_path) - 1] = '\0';
    }
    to_backslash(paf_path);

    /* Check the file exists */
    if (GetFileAttributesA(paf_path) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "Error: file not found: %s\n", paf_path);
        return 1;
    }

    /* Determine output directory */
    char out_dir[MAX_PATH];
    if (argc >= 3) {
        if (!abs_path(argv[2], out_dir, sizeof(out_dir))) {
            strncpy(out_dir, argv[2], sizeof(out_dir) - 1);
            out_dir[sizeof(out_dir) - 1] = '\0';
        }
    } else {
        /* Strip extension: "C:\path\game_data.paf" -> "C:\path\game_data" */
        strncpy(out_dir, paf_path, sizeof(out_dir) - 1);
        out_dir[sizeof(out_dir) - 1] = '\0';
        strip_ext(out_dir);
    }
    to_backslash(out_dir);

    printf("Archive : %s\n", paf_path);
    printf("Output  : %s\n\n", out_dir);

    /* Create output directory if needed */
    CreateDirectoryA(out_dir, NULL);

    /* Open extractor */
    paf_extractor_t ext;
    memset(&ext, 0, sizeof(ext));
    if (paf_extractor_open(&ext, paf_path) != 0) {
        fprintf(stderr, "Error: failed to open archive.\n");
        return 1;
    }

    printf("Files   : %u\n\n", ext.header.file_count);

    /* GPU-accelerated extraction (CUDA -> Vulkan -> CPU fallback) */
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    int result = paf_extractor_gpu_run(&ext, paf_path, out_dir);

    QueryPerformanceCounter(&t1);
    double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;

    paf_extractor_close(&ext);

    if (result == 0) {
        printf("\nDone in %.2f sec.\n", elapsed);
    } else {
        fprintf(stderr, "\nExtraction finished with %d error(s) in %.2f sec.\n",
                -result, elapsed);
    }

    /* Keep window open when launched from Explorer (not a terminal) */
    HWND con = GetConsoleWindow();
    if (con) {
        DWORD pid = 0;
        GetWindowThreadProcessId(con, &pid);
        if (pid == GetCurrentProcessId()) {
            printf("\nPress any key to close...\n");
            (void)getchar();
        }
    }

    return result == 0 ? 0 : 1;
}
