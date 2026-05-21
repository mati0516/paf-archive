/*
 * paf_viewer.c  --  PAF Viewer (standalone executable)
 *
 * Usage: paf_viewer.exe <archive.paf>
 *
 * Win32 window with ListView showing:
 *   - File path
 *   - Size
 *   - SHA-256 (truncated, full value on hover)
 *   - Verification status (after clicking "Verify")
 *
 * Buttons: "ここに展開"  "フォルダを選んで展開"  "整合性を検証"
 *
 * Build (standalone, no MFC):
 *   cl /O2 /Ilibpaf\include shell\paf_viewer.c lib\libpaf.lib
 *      shell32.lib comctl32.lib shlwapi.lib
 *      /Fe:bin\paf_viewer.exe
 */

#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

#include "libpaf.h"
#include "paf_extractor.h"
#include "sha256.h"

/* ─── Controls ─── */
#define IDC_LIST        101
#define IDC_BTN_HERE    201
#define IDC_BTN_TO      202
#define IDC_BTN_VERIFY  203
#define IDC_STATUS      301

/* ─── Columns ─── */
#define COL_PATH    0
#define COL_SIZE    1
#define COL_HASH    2
#define COL_STATUS  3

/* ─── Globals ─── */
static HWND    g_hwndList   = NULL;
static HWND    g_hwndStatus = NULL;
static WCHAR   g_pafPath[MAX_PATH] = {0};
static PafList g_list;
static BOOL    g_listLoaded = FALSE;

/* ─── Helpers ─── */

static void WtoA(const WCHAR* src, char* dst, int dstLen) {
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dstLen, NULL, NULL);
}

static void SetStatus(const WCHAR* msg) {
    SetWindowTextW(g_hwndStatus, msg);
}

/* Convert 32-byte hash to hex WCHAR string */
static void HashToW(const uint8_t* h, WCHAR* buf, int cch) {
    for (int i = 0; i < 32 && (i * 2 + 2) < cch; i++)
        _snwprintf_s(buf + i*2, cch - i*2, _TRUNCATE, L"%02x", h[i]);
}

/* Load the archive file list into the ListView */
static void LoadList(void) {
    if (g_listLoaded) { free_paf_list(&g_list); g_listLoaded = FALSE; }
    ListView_DeleteAllItems(g_hwndList);

    char pathA[MAX_PATH * 3];
    WtoA(g_pafPath, pathA, sizeof pathA);

    if (paf_list_binary(pathA, &g_list) != 0) {
        SetStatus(L"アーカイブを開けませんでした。");
        return;
    }
    g_listLoaded = TRUE;

    for (UINT i = 0; i < g_list.count; i++) {
        WCHAR pathW[1024];
        MultiByteToWideChar(CP_UTF8, 0, g_list.entries[i].path, -1, pathW, 1024);

        LVITEMW lvi = {0};
        lvi.mask     = LVIF_TEXT;
        lvi.iItem    = (int)i;
        lvi.pszText  = pathW;
        ListView_InsertItem(g_hwndList, &lvi);

        /* Size */
        WCHAR sz[32];
        _snwprintf_s(sz, 32, _TRUNCATE, L"%u", g_list.entries[i].size);
        ListView_SetItemText(g_hwndList, (int)i, COL_SIZE, sz);

        /* SHA-256: first 16 hex chars + "…" */
        const uint8_t* h = g_list.entries[i].hash;
        WCHAR hash[80];
        _snwprintf_s(hash, 80, _TRUNCATE,
            L"%02x%02x%02x%02x%02x%02x%02x%02x…",
            h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7]);
        ListView_SetItemText(g_hwndList, (int)i, COL_HASH, hash);

        /* Status: blank until verified */
        ListView_SetItemText(g_hwndList, (int)i, COL_STATUS, L"—");
    }

    WCHAR status[128];
    _snwprintf_s(status, 128, _TRUNCATE, L"%u 件のファイル", g_list.count);
    SetStatus(status);
}

/* Extract to a directory */
static void DoExtract(const WCHAR* outDirW) {
    char pathA[MAX_PATH * 3], outA[MAX_PATH * 3];
    WtoA(g_pafPath, pathA, sizeof pathA);
    WtoA(outDirW,   outA,  sizeof outA);
    SetStatus(L"展開中...");
    CreateDirectoryW(outDirW, NULL);
    int res = paf_extract_binary(pathA, outA, 1);
    if (res == 0) {
        SetStatus(L"✅ 展開が完了しました。");
        ShellExecuteW(NULL, L"explore", outDirW, NULL, NULL, SW_SHOWNORMAL);
    } else {
        SetStatus(L"❌ 展開に失敗しました。");
    }
}

/* Verify integrity: recompute SHA-256 for each file and compare */
static void DoVerify(void) {
    if (!g_listLoaded) return;
    SetStatus(L"検証中...");

    char pathA[MAX_PATH * 3];
    WtoA(g_pafPath, pathA, sizeof pathA);

    paf_extractor_t ext;
    if (paf_extractor_open(&ext, pathA) != 0) {
        SetStatus(L"❌ アーカイブを開けませんでした。");
        return;
    }

    int ok = 0, fail = 0;
    for (UINT i = 0; i < g_list.count; i++) {
        uint8_t*  data = NULL;
        uint64_t  size = 0;
        char      entryPath[1024];

        int r = paf_extractor_get_file(&ext, i, entryPath, sizeof entryPath, &data, &size);
        if (r != 0 || !data) {
            ListView_SetItemText(g_hwndList, (int)i, COL_STATUS, L"err");
            fail++;
            continue;
        }

        /* Compute SHA-256 */
        uint8_t computed[32];
        sha256_context_t ctx;
        sha256_init(&ctx);
        sha256_update(&ctx, data, (size_t)size);
        sha256_final(&ctx, computed);
        free(data);

        BOOL match = (memcmp(computed, g_list.entries[i].hash, 32) == 0);
        ListView_SetItemText(g_hwndList, (int)i, COL_STATUS,
                             match ? L"✓" : L"✗");
        if (match) ok++; else fail++;
    }
    paf_extractor_close(&ext);

    WCHAR status[128];
    if (fail == 0)
        _snwprintf_s(status, 128, _TRUNCATE,
            L"✅ %d 件すべて検証OK", ok);
    else
        _snwprintf_s(status, 128, _TRUNCATE,
            L"❌ %d 件失敗 / %d 件検証", fail, ok + fail);
    SetStatus(status);
}

/* ─── Window procedure ─── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icex = { sizeof icex, ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icex);

        /* ListView */
        g_hwndList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_LIST, NULL, NULL);
        ListView_SetExtendedListViewStyle(g_hwndList,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_INFOTIP);

        LVCOLUMNW col = {0};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        col.fmt  = LVCFMT_LEFT;

        col.pszText = L"ファイルパス";  col.cx = 400;
        ListView_InsertColumn(g_hwndList, COL_PATH, &col);
        col.pszText = L"サイズ (B)";    col.cx = 100;
        ListView_InsertColumn(g_hwndList, COL_SIZE, &col);
        col.pszText = L"SHA-256";        col.cx = 160;
        ListView_InsertColumn(g_hwndList, COL_HASH, &col);
        col.pszText = L"状態";          col.cx = 60;
        ListView_InsertColumn(g_hwndList, COL_STATUS, &col);

        /* Buttons */
        CreateWindowW(L"BUTTON", L"ここに展開",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_HERE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"フォルダを選んで展開...",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_TO, NULL, NULL);
        CreateWindowW(L"BUTTON", L"整合性を検証",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_BTN_VERIFY, NULL, NULL);

        /* Status bar */
        g_hwndStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, hwnd, (HMENU)(UINT_PTR)IDC_STATUS, NULL, NULL);

        LoadList();
        return 0;
    }
    case WM_SIZE: {
        int W = LOWORD(lp), H = HIWORD(lp);
        int btnH = 28, statusH = 20, pad = 6;
        int btnY = H - btnH - statusH - pad * 2;
        int listH = btnY - pad;

        SetWindowPos(g_hwndList, NULL, pad, pad, W - pad*2, listH,
                     SWP_NOZORDER | SWP_NOACTIVATE);

        int bw = (W - pad*4) / 3;
        HWND hBtnHere   = GetDlgItem(hwnd, IDC_BTN_HERE);
        HWND hBtnTo     = GetDlgItem(hwnd, IDC_BTN_TO);
        HWND hBtnVerify = GetDlgItem(hwnd, IDC_BTN_VERIFY);
        SetWindowPos(hBtnHere,   NULL, pad,           btnY, bw, btnH, SWP_NOZORDER);
        SetWindowPos(hBtnTo,     NULL, pad*2 + bw,    btnY, bw, btnH, SWP_NOZORDER);
        SetWindowPos(hBtnVerify, NULL, pad*3 + bw*2,  btnY, bw, btnH, SWP_NOZORDER);
        SetWindowPos(g_hwndStatus, NULL, pad, btnY + btnH + pad, W - pad*2, statusH,
                     SWP_NOZORDER);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_HERE: {
            if (!g_pafPath[0]) break;
            WCHAR dir[MAX_PATH];
            wcsncpy_s(dir, MAX_PATH, g_pafPath, _TRUNCATE);
            PathRemoveFileSpecW(dir);
            DoExtract(dir);
            break;
        }
        case IDC_BTN_TO: {
            if (!g_pafPath[0]) break;
            WCHAR outDir[MAX_PATH] = {0};
            BROWSEINFOW bi = {0};
            bi.hwndOwner = hwnd;
            bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            bi.lpszTitle = L"展開先フォルダを選んでください";
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (!pidl) break;
            SHGetPathFromIDListW(pidl, outDir);
            CoTaskMemFree(pidl);

            /* Append archive stem as subfolder */
            WCHAR stem[MAX_PATH];
            wcsncpy_s(stem, MAX_PATH, g_pafPath, _TRUNCATE);
            PathRemoveExtensionW(stem);
            PathAppendW(outDir, PathFindFileNameW(stem));
            DoExtract(outDir);
            break;
        }
        case IDC_BTN_VERIFY:
            DoVerify();
            break;
        }
        return 0;

    case WM_NOTIFY: {
        LPNMHDR pnm = (LPNMHDR)lp;
        /* Tooltip: show full SHA-256 on hover */
        if (pnm->idFrom == IDC_LIST && pnm->code == LVN_GETINFOTIPW) {
            LPNMLVGETINFOTIPW pTip = (LPNMLVGETINFOTIPW)lp;
            if (g_listLoaded && (UINT)pTip->iItem < g_list.count) {
                WCHAR fullHash[65];
                HashToW(g_list.entries[pTip->iItem].hash, fullHash, 65);
                _snwprintf_s(pTip->pszText, pTip->cchTextMax, _TRUNCATE,
                    L"SHA-256: %s", fullHash);
            }
        }
        return 0;
    }
    case WM_DESTROY:
        if (g_listLoaded) { free_paf_list(&g_list); g_listLoaded = FALSE; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ─── WinMain ─── */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmd, int nShow) {
    SetConsoleOutputCP(CP_UTF8);

    /* argv[1] = path to .paf file */
    int argc;
    LPWSTR* argv = CommandLineToArgvW(lpCmd, &argc);
    if (!argv || argc < 1 || !argv[0][0]) {
        MessageBoxW(NULL,
            L"使い方: paf_viewer.exe <archive.paf>",
            L"PAF Viewer", MB_ICONINFORMATION);
        return 1;
    }
    GetFullPathNameW(argv[0], MAX_PATH, g_pafPath, NULL);
    LocalFree(argv);

    /* Window title = filename */
    WCHAR title[MAX_PATH + 16];
    _snwprintf_s(title, MAX_PATH + 16, _TRUNCATE,
        L"PAF Viewer — %s", PathFindFileNameW(g_pafPath));

    /* Register window class */
    WNDCLASSEXW wc = { sizeof wc };
    wc.hInstance     = hInst;
    wc.lpszClassName = L"PAFViewerWnd";
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"PAFViewerWnd", title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 560,
        NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
