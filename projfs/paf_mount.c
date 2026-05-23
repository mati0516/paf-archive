/*
 * paf_mount.c  --  PAF ProjFS Virtual Mount Provider
 *
 * Mounts a .paf archive as a virtual directory using Windows Projected File
 * System (ProjFS, available on Windows 10 1809+ / Windows Server 2019+).
 * No kernel driver is required; the provider runs as a normal user-mode
 * process that holds the mount alive until Enter is pressed.
 *
 * Usage:
 *   paf_mount.exe <archive.paf> <mount_dir>
 *
 * Build (from repo root, Developer Command Prompt):
 *   projfs\build.bat
 *
 * The .paf archive is opened read-only; files are streamed on demand when
 * an application first opens them through the virtual directory.
 *
 * ProjFS documentation:
 *   https://docs.microsoft.com/en-us/windows/win32/projfs/projected-file-system
 */

#define _WIN32_WINNT 0x0A00   /* Windows 10 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <projectedfslib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "libpaf.h"
#include "paf_extractor.h"

/* ─────────────────────────────────────────────────────────────────────────────
   Virtual-tree node
   ───────────────────────────────────────────────────────────────────────────── */

#define VNODE_NAME_MAX 256

typedef struct VNode {
    WCHAR           name[VNODE_NAME_MAX]; /* final path component */
    int             is_dir;
    int             paf_index;   /* index in PafList (-1 for directories) */
    uint32_t        file_size;   /* 0 for directories */
    struct VNode**  children;
    int             child_count;
    int             child_cap;
} VNode;

/* ─────────────────────────────────────────────────────────────────────────────
   Enumeration session context
   ───────────────────────────────────────────────────────────────────────────── */

typedef struct EnumSession {
    GUID    id;          /* copy of the enumerationId passed by ProjFS */
    VNode*  dir;         /* the directory being enumerated              */
    int     cursor;      /* next child index to emit                    */
    WCHAR   filter[256]; /* wildcard filter (* or a specific name)      */
} EnumSession;

/* Up to 64 simultaneous enum sessions (one per concurrent directory open) */
#define MAX_SESSIONS 64
static EnumSession  g_sessions[MAX_SESSIONS];
static CRITICAL_SECTION g_cs_sessions;

/* ─────────────────────────────────────────────────────────────────────────────
   Provider state
   ───────────────────────────────────────────────────────────────────────────── */

typedef struct ProviderState {
    PafList          list;        /* file list loaded at startup          */
    paf_extractor_t  ext;         /* open extractor for reading file data */
    VNode*           root;        /* root of virtual directory tree       */
    char             paf_path[MAX_PATH * 3]; /* UTF-8 path to .paf file  */
    CRITICAL_SECTION cs_ext;     /* serialise paf_extractor_get_file()   */
    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT ctx; /* ProjFS context           */
} ProviderState;

static ProviderState g_state;

/* ─────────────────────────────────────────────────────────────────────────────
   UTF-8 <-> Wide helpers
   ───────────────────────────────────────────────────────────────────────────── */

static void Utf8ToWide(const char* src, WCHAR* dst, int dst_cch) {
    MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_cch);
}

static void WideToUtf8(const WCHAR* src, char* dst, int dst_cb) {
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_cb, NULL, NULL);
}

/* ─────────────────────────────────────────────────────────────────────────────
   Virtual-tree construction
   ───────────────────────────────────────────────────────────────────────────── */

static VNode* VNode_New(const WCHAR* name, int is_dir, int paf_index, uint32_t size) {
    VNode* n = (VNode*)calloc(1, sizeof(VNode));
    if (!n) return NULL;
    wcsncpy_s(n->name, VNODE_NAME_MAX, name, _TRUNCATE);
    n->is_dir    = is_dir;
    n->paf_index = paf_index;
    n->file_size = size;
    return n;
}

static void VNode_Free(VNode* n) {
    if (!n) return;
    for (int i = 0; i < n->child_count; i++)
        VNode_Free(n->children[i]);
    free(n->children);
    free(n);
}

/* Find an immediate child by name (case-insensitive) */
static VNode* VNode_FindChild(VNode* parent, const WCHAR* name) {
    for (int i = 0; i < parent->child_count; i++) {
        if (_wcsicmp(parent->children[i]->name, name) == 0)
            return parent->children[i];
    }
    return NULL;
}

/* Append a child, growing the array as needed */
static int VNode_AddChild(VNode* parent, VNode* child) {
    if (parent->child_count >= parent->child_cap) {
        int new_cap = parent->child_cap ? parent->child_cap * 2 : 8;
        VNode** tmp = (VNode**)realloc(parent->children, sizeof(VNode*) * new_cap);
        if (!tmp) return 0;
        parent->children = tmp;
        parent->child_cap = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return 1;
}

/*
 * Insert one PAF entry into the virtual tree.
 * path is a forward-slash-separated UTF-8 string (e.g. "docs/api/index.html").
 */
static void Tree_Insert(VNode* root, const char* utf8_path, int paf_index, uint32_t size) {
    /* Convert full path to wide */
    WCHAR wpath[1024];
    Utf8ToWide(utf8_path, wpath, 1024);

    VNode* cur = root;
    WCHAR* tok_ctx = NULL;
    WCHAR  tmp[1024];
    wcsncpy_s(tmp, 1024, wpath, _TRUNCATE);

    WCHAR* tok = wcstok_s(tmp, L"/", &tok_ctx);
    WCHAR* next;
    while (tok) {
        /* Peek at the rest to decide if this component is a directory */
        next = wcstok_s(NULL, L"/", &tok_ctx);

        if (next == NULL) {
            /* This is the leaf (file) */
            VNode* leaf = VNode_FindChild(cur, tok);
            if (!leaf) {
                leaf = VNode_New(tok, 0, paf_index, size);
                if (leaf) VNode_AddChild(cur, leaf);
            }
        } else {
            /* This is an intermediate directory */
            VNode* dir = VNode_FindChild(cur, tok);
            if (!dir) {
                dir = VNode_New(tok, 1, -1, 0);
                if (dir) VNode_AddChild(cur, dir);
            }
            cur = dir;
            /* Put next back into the loop by re-using it as tok */
            tok = next;
            continue;
        }
        tok = next;
    }
}

/* Locate a node by a relative wide path (e.g. L"docs\\api") */
static VNode* Tree_Lookup(VNode* root, PCWSTR rel_path) {
    if (!rel_path || rel_path[0] == L'\0') return root;

    WCHAR tmp[1024];
    wcsncpy_s(tmp, 1024, rel_path, _TRUNCATE);

    /* Normalise backslashes to forward slashes */
    for (WCHAR* p = tmp; *p; p++) if (*p == L'\\') *p = L'/';

    VNode* cur = root;
    WCHAR* tok_ctx = NULL;
    WCHAR* tok = wcstok_s(tmp, L"/", &tok_ctx);
    while (tok && cur) {
        cur = VNode_FindChild(cur, tok);
        tok = wcstok_s(NULL, L"/", &tok_ctx);
    }
    return cur;
}

/* ─────────────────────────────────────────────────────────────────────────────
   Enumeration-session pool
   ───────────────────────────────────────────────────────────────────────────── */

static EnumSession* Session_Find(const GUID* id) {
    EnterCriticalSection(&g_cs_sessions);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (IsEqualGUID(&g_sessions[i].id, id)) {
            LeaveCriticalSection(&g_cs_sessions);
            return &g_sessions[i];
        }
    }
    LeaveCriticalSection(&g_cs_sessions);
    return NULL;
}

static EnumSession* Session_Create(const GUID* id, VNode* dir) {
    EnterCriticalSection(&g_cs_sessions);
    /* Prefer a truly empty slot (all-zero GUID) */
    static const GUID zero_guid = {0};
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (IsEqualGUID(&g_sessions[i].id, &zero_guid)) {
            g_sessions[i].id     = *id;
            g_sessions[i].dir    = dir;
            g_sessions[i].cursor = 0;
            g_sessions[i].filter[0] = L'*';
            g_sessions[i].filter[1] = L'\0';
            LeaveCriticalSection(&g_cs_sessions);
            return &g_sessions[i];
        }
    }
    LeaveCriticalSection(&g_cs_sessions);
    return NULL; /* pool exhausted */
}

static void Session_Destroy(const GUID* id) {
    EnterCriticalSection(&g_cs_sessions);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (IsEqualGUID(&g_sessions[i].id, id)) {
            memset(&g_sessions[i].id, 0, sizeof(GUID));
            g_sessions[i].dir    = NULL;
            g_sessions[i].cursor = 0;
            break;
        }
    }
    LeaveCriticalSection(&g_cs_sessions);
}

/* ─────────────────────────────────────────────────────────────────────────────
   ProjFS callback helpers
   ───────────────────────────────────────────────────────────────────────────── */

/* Fill in a PRJ_FILE_BASIC_INFO for a VNode */
static void FillBasicInfo(PRJ_FILE_BASIC_INFO* info, const VNode* node) {
    memset(info, 0, sizeof(*info));
    info->IsDirectory = (BOOLEAN)node->is_dir;
    info->FileSize    = (INT64)node->file_size;
    /* Timestamps: use a fixed value (1970-01-01 in FILETIME ticks) for simplicity */
    FILETIME ft = {0xD53E8000UL, 0x019DB1DEUL}; /* 1970-01-01 00:00:00 UTC */
    info->CreationTime.QuadPart  = ((ULARGE_INTEGER*)&ft)->QuadPart;
    info->LastWriteTime          = info->CreationTime;
    info->ChangeTime             = info->CreationTime;
    info->LastAccessTime         = info->CreationTime;
    info->FileAttributes         = node->is_dir
                                   ? FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY
                                   : FILE_ATTRIBUTE_READONLY;
}

/* Fill in a PRJ_PLACEHOLDER_INFO for a VNode.
   PRJ_PLACEHOLDER_INFO is a variable-length struct; for our use
   no extended info is needed so the static portion suffices. */
static void FillPlaceholderInfo(PRJ_PLACEHOLDER_INFO* pi, const VNode* node) {
    memset(pi, 0, sizeof(*pi));
    FillBasicInfo(&pi->FileBasicInfo, node);
    /* EaInformation, SecurityInformation, StreamsInformation all zero */
}

/* ─────────────────────────────────────────────────────────────────────────────
   ProjFS callbacks
   ───────────────────────────────────────────────────────────────────────────── */

static HRESULT CALLBACK
CB_StartDirectoryEnumeration(
    const PRJ_CALLBACK_DATA* callbackData,
    const GUID*              enumerationId)
{
    /* Locate the virtual directory corresponding to the requested path */
    VNode* dir = Tree_Lookup(g_state.root, callbackData->FilePathName);
    if (!dir || !dir->is_dir) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    EnumSession* s = Session_Create(enumerationId, dir);
    if (!s) return HRESULT_FROM_WIN32(ERROR_TOO_MANY_OPEN_FILES);
    return S_OK;
}

static HRESULT CALLBACK
CB_EndDirectoryEnumeration(
    const PRJ_CALLBACK_DATA* callbackData,
    const GUID*              enumerationId)
{
    (void)callbackData;
    Session_Destroy(enumerationId);
    return S_OK;
}

static HRESULT CALLBACK
CB_GetDirectoryEnumeration(
    const PRJ_CALLBACK_DATA* callbackData,
    const GUID*              enumerationId,
    PCWSTR                   searchExpression,
    PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle)
{
    EnumSession* s = Session_Find(enumerationId);
    if (!s) return HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);

    /* If a new search expression was provided, restart from the beginning */
    if (callbackData->Flags & PRJ_CB_DATA_FLAG_ENUM_RESTART_SCAN) {
        s->cursor = 0;
        if (searchExpression)
            wcsncpy_s(s->filter, 256, searchExpression, _TRUNCATE);
        else {
            s->filter[0] = L'*';
            s->filter[1] = L'\0';
        }
    }

    VNode* dir = s->dir;
    BOOL   any = FALSE;

    while (s->cursor < dir->child_count) {
        VNode* child = dir->children[s->cursor];

        /* Apply wildcard filter (ProjFS guarantees the filter is a simple
           pattern that PrjFileNameMatch understands) */
        if (!PrjFileNameMatch(child->name, s->filter)) {
            s->cursor++;
            continue;
        }

        PRJ_FILE_BASIC_INFO info;
        FillBasicInfo(&info, child);

        HRESULT hr = PrjFillDirEntryBuffer(child->name, &info, dirEntryBufferHandle);
        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
            /* Buffer full — return S_OK so ProjFS calls us again */
            if (!any) {
                /* The very first entry doesn't fit; something is seriously wrong */
                return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            return S_OK;
        }
        if (FAILED(hr)) return hr;

        s->cursor++;
        any = TRUE;
    }

    return S_OK; /* enumeration complete */
}

static HRESULT CALLBACK
CB_GetPlaceholderInfo(const PRJ_CALLBACK_DATA* callbackData)
{
    VNode* node = Tree_Lookup(g_state.root, callbackData->FilePathName);
    if (!node) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    PRJ_PLACEHOLDER_INFO pi;
    FillPlaceholderInfo(&pi, node);

    HRESULT hr = PrjWritePlaceholderInfo(
        callbackData->NamespaceVirtualizationContext,
        callbackData->FilePathName,
        &pi,
        sizeof(pi));
    return hr;
}

static HRESULT CALLBACK
CB_GetFileData(
    const PRJ_CALLBACK_DATA* callbackData,
    UINT64                   byteOffset,
    UINT32                   length)
{
    VNode* node = Tree_Lookup(g_state.root, callbackData->FilePathName);
    if (!node || node->is_dir) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    if (node->paf_index < 0)   return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    /* Allocate a ProjFS-aligned buffer */
    void* buf = PrjAllocateAlignedBuffer(
        callbackData->NamespaceVirtualizationContext, length);
    if (!buf) return E_OUTOFMEMORY;

    /* Extract file data from the PAF archive (serialised) */
    char path_out[1024];
    uint8_t* data  = NULL;
    uint64_t dsize = 0;

    EnterCriticalSection(&g_state.cs_ext);
    int rc = paf_extractor_get_file(
        &g_state.ext,
        (uint32_t)node->paf_index,
        path_out, sizeof(path_out),
        &data, &dsize);
    LeaveCriticalSection(&g_state.cs_ext);

    HRESULT hr = S_OK;

    if (rc != 0) {
        hr = HRESULT_FROM_WIN32(ERROR_READ_FAULT);
        goto done;
    }

    if (!data) {
        /* paf_extractor_get_file returns NULL data for zero-byte files */
        memset(buf, 0, length);
    } else {
        /* Sanity-check the requested range */
        if (byteOffset > dsize) {
            free(data);
            hr = HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
            goto done;
        }

        UINT64 avail = dsize - byteOffset;
        UINT32 copy_len = (UINT32)(avail < length ? avail : length);

        memcpy(buf, data + byteOffset, copy_len);

        /* Zero-pad the tail if the request extends past EOF */
        if (copy_len < length)
            memset((char*)buf + copy_len, 0, length - copy_len);

        free(data);
    }

    hr = PrjWriteFileData(
        callbackData->NamespaceVirtualizationContext,
        &callbackData->DataStreamId,
        buf,
        byteOffset,
        length);

done:
    PrjFreeAlignedBuffer(buf);
    return hr;
}

/* ─────────────────────────────────────────────────────────────────────────────
   ProjFS feature detection
   ───────────────────────────────────────────────────────────────────────────── */

static BOOL ProjFS_IsEnabled(void) {
    /* Try to load ProjectedFSLib at runtime to detect availability */
    HMODULE h = LoadLibraryExW(L"ProjectedFSLib.dll", NULL,
                               LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!h) return FALSE;
    FreeLibrary(h);
    return TRUE;
}

/* ─────────────────────────────────────────────────────────────────────────────
   Entry point
   ───────────────────────────────────────────────────────────────────────────── */

static void PrintUsage(const WCHAR* argv0) {
    wprintf(L"Usage: %s <archive.paf> <mount_dir>\n", argv0);
    wprintf(L"\n");
    wprintf(L"  Mounts <archive.paf> as a read-only virtual directory at <mount_dir>.\n");
    wprintf(L"  Press Enter in this window to unmount.\n");
    wprintf(L"\n");
    wprintf(L"  Requirements:\n");
    wprintf(L"    - Windows 10 version 1809 or later (build 17763+)\n");
    wprintf(L"    - ProjFS optional feature enabled\n");
    wprintf(L"\n");
    wprintf(L"  To enable ProjFS (requires admin):\n");
    wprintf(L"    Enable-WindowsOptionalFeature -Online -FeatureName Client-ProjFS -NoRestart\n");
    wprintf(L"  or:\n");
    wprintf(L"    dism /online /enable-feature /featurename:Client-ProjFS\n");
}

int wmain(int argc, WCHAR* argv[]) {
    wprintf(L"PAF ProjFS Mount v1.0\n");
    wprintf(L"---------------------\n");

    if (argc < 3) {
        PrintUsage(argc > 0 ? argv[0] : L"paf_mount.exe");
        return 1;
    }

    /* ── 0. Check ProjFS availability ────────────────────────────────────── */
    if (!ProjFS_IsEnabled()) {
        fwprintf(stderr,
            L"ERROR: Windows Projected File System (ProjFS) is not available.\n"
            L"\n"
            L"To enable it, run (as Administrator):\n"
            L"  Enable-WindowsOptionalFeature -Online -FeatureName Client-ProjFS -NoRestart\n"
            L"or:\n"
            L"  dism /online /enable-feature /featurename:Client-ProjFS\n"
            L"\n"
            L"ProjFS requires Windows 10 version 1809 (build 17763) or later.\n");
        return 1;
    }

    PCWSTR paf_path_w  = argv[1];
    PCWSTR mount_dir_w = argv[2];

    /* ── 1. Convert PAF path to UTF-8 and open archive ───────────────────── */
    WideToUtf8(paf_path_w, g_state.paf_path, sizeof(g_state.paf_path));

    wprintf(L"[1/5] Opening archive: %s\n", paf_path_w);

    if (paf_list_binary(g_state.paf_path, &g_state.list) != 0) {
        fwprintf(stderr, L"ERROR: Failed to read PAF archive: %s\n", paf_path_w);
        return 1;
    }
    wprintf(L"      %u file(s) found.\n", g_state.list.count);

    if (paf_extractor_open(&g_state.ext, g_state.paf_path) != 0) {
        fwprintf(stderr, L"ERROR: Failed to open PAF extractor for: %s\n", paf_path_w);
        free_paf_list(&g_state.list);
        return 1;
    }

    /* ── 2. Build virtual directory tree ─────────────────────────────────── */
    wprintf(L"[2/5] Building virtual directory tree ...\n");

    InitializeCriticalSection(&g_state.cs_ext);
    InitializeCriticalSection(&g_cs_sessions);

    g_state.root = VNode_New(L"", 1, -1, 0);
    if (!g_state.root) {
        fwprintf(stderr, L"ERROR: Out of memory.\n");
        return 1;
    }

    for (uint32_t i = 0; i < g_state.list.count; i++) {
        Tree_Insert(g_state.root,
                    g_state.list.entries[i].path,
                    (int)i,
                    g_state.list.entries[i].size);
    }

    /* ── 3. Create mount directory ───────────────────────────────────────── */
    wprintf(L"[3/5] Preparing mount point: %s\n", mount_dir_w);

    if (!CreateDirectoryW(mount_dir_w, NULL)) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            fwprintf(stderr,
                L"ERROR: Cannot create directory '%s' (error %lu).\n",
                mount_dir_w, err);
            return 1;
        }
    }

    /* ── 4. Mark as virtualization root ──────────────────────────────────── */
    wprintf(L"[4/5] Marking directory as virtualization root ...\n");

    GUID instance_id;
    CoCreateGuid(&instance_id);

    HRESULT hr = PrjMarkDirectoryAsPlaceholder(
        mount_dir_w, NULL, NULL, &instance_id);
    if (FAILED(hr)) {
        fwprintf(stderr,
            L"ERROR: PrjMarkDirectoryAsPlaceholder failed: 0x%08lX\n"
            L"  Make sure ProjFS is enabled and the directory is empty.\n",
            hr);
        return 1;
    }

    /* ── 5. Start virtualizing ───────────────────────────────────────────── */
    wprintf(L"[5/5] Starting ProjFS provider ...\n");

    PRJ_CALLBACKS cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.StartDirectoryEnumerationCallback = CB_StartDirectoryEnumeration;
    cbs.EndDirectoryEnumerationCallback   = CB_EndDirectoryEnumeration;
    cbs.GetDirectoryEnumerationCallback   = CB_GetDirectoryEnumeration;
    cbs.GetPlaceholderInfoCallback        = CB_GetPlaceholderInfo;
    cbs.GetFileDataCallback               = CB_GetFileData;

    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT nvc = NULL;
    hr = PrjStartVirtualizing(mount_dir_w, &cbs, &g_state, NULL, &nvc);
    if (FAILED(hr)) {
        fwprintf(stderr,
            L"ERROR: PrjStartVirtualizing failed: 0x%08lX\n"
            L"\n"
            L"Common causes:\n"
            L"  - ProjFS feature not enabled  (run projfs\\umount.bat if the\n"
            L"    directory is in a broken state, then re-enable ProjFS)\n"
            L"  - Another provider already holds the directory\n",
            hr);
        return 1;
    }
    g_state.ctx = nvc;

    wprintf(L"\n");
    wprintf(L"  Archive : %s\n", paf_path_w);
    wprintf(L"  Mount   : %s\n", mount_dir_w);
    wprintf(L"  Files   : %u\n", g_state.list.count);
    wprintf(L"\n");
    wprintf(L"Mounted successfully.  Press Enter to unmount ...\n");
    wprintf(L"\n");

    /* Block until the user presses Enter */
    getchar();

    /* ── Teardown ─────────────────────────────────────────────────────────── */
    wprintf(L"Unmounting ...\n");
    PrjStopVirtualizing(nvc);
    paf_extractor_close(&g_state.ext);
    free_paf_list(&g_state.list);
    VNode_Free(g_state.root);
    DeleteCriticalSection(&g_state.cs_ext);
    DeleteCriticalSection(&g_cs_sessions);

    wprintf(L"Done.\n");
    return 0;
}
