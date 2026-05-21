/*
 * paf_shell.c  --  PAF Windows Shell Extension
 *
 * Implements:
 *   IContextMenu + IShellExtInit  (right-click menu: Extract Here / Extract To...)
 *   IPreviewHandler + IInitializeWithFile  (preview pane: file list + SHA-256)
 *
 * Build:
 *   cl /O2 /LD /Ilibpaf\include shell\paf_shell.c lib\libpaf.lib
 *      shell32.lib ole32.lib uuid.lib shlwapi.lib comctl32.lib
 *      /Fe:bin\paf_shell.dll /link /DEF:shell\paf_shell.def
 *
 * Install:
 *   shell\install.bat  (requires admin)
 */

#define _WIN32_WINNT  0x0600
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define INITGUID

#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <objbase.h>
#include <olectl.h>
#include <commctrl.h>
#include <stdio.h>

#include "libpaf.h"
#include "paf_extractor.h"

/* ─── GUIDs ──────────────────────────────────────────────────────────────── */

/* {7F3A8B2C-4D5E-4F6A-8B7C-9D0E1F2A3B4C}  context menu */
static const GUID CLSID_PafContextMenu = {
    0x7f3a8b2cUL, 0x4d5e, 0x4f6a,
    { 0x8b, 0x7c, 0x9d, 0x0e, 0x1f, 0x2a, 0x3b, 0x4c }
};
/* {8A4B9C3D-5E6F-4A7B-9C8D-0E1F2A3B4C5D}  preview handler */
static const GUID CLSID_PafPreviewHandler = {
    0x8a4b9c3dUL, 0x5e6f, 0x4a7b,
    { 0x9c, 0x8d, 0x0e, 0x1f, 0x2a, 0x3b, 0x4c, 0x5d }
};

/* Global DLL state */
static volatile LONG g_refCount = 0;
static HMODULE       g_hModule  = NULL;

/* ─── Utilities ──────────────────────────────────────────────────────────── */

/* Wide string → UTF-8 (for libpaf, which uses UTF-8 paths) */
static void WtoA(const WCHAR* src, char* dst, int dstLen) {
    WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dstLen, NULL, NULL);
}

/* Write a registry string value, creating the key if needed */
static BOOL RegSetSzW(HKEY hRoot, const WCHAR* sub, const WCHAR* name, const WCHAR* val) {
    HKEY hk;
    if (RegCreateKeyExW(hRoot, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &hk, NULL))
        return FALSE;
    DWORD cb = (DWORD)((wcslen(val) + 1) * sizeof(WCHAR));
    BOOL ok = !RegSetValueExW(hk, name, 0, REG_SZ, (const BYTE*)val, cb);
    RegCloseKey(hk);
    return ok;
}

/* Delete a registry key and all sub-keys */
static void RegDeleteTreeSafe(HKEY hRoot, const WCHAR* sub) {
    /* SHDeleteKey is in shlwapi.lib */
    SHDeleteKeyW(hRoot, sub);
}

/* GUID → registry string  "{xxxxxxxx-xxxx-...}" */
static void GuidToStr(const GUID* g, WCHAR* buf, int cch) {
    StringFromGUID2(g, buf, cch);
}

/* ─────────────────────────────────────────────────────────────────────────
   PART 1 — IContextMenu + IShellExtInit
   ───────────────────────────────────────────────────────────────────────── */

#define CMD_EXTRACT_HERE 0
#define CMD_EXTRACT_TO   1
#define CMD_COUNT        2

/* Two embedded interfaces + our private data.
   IContextMenu must be at offset 0 so (PafCtxMenu*)pCM works directly.
   IShellExtInit is recovered via CONTAINING_RECORD. */
typedef struct {
    IContextMenu  cm;               /* offset 0  — IContextMenu* == &obj */
    IShellExtInit sei;              /* offset 8  — recovered below       */
    LONG          cRef;
    WCHAR         path[MAX_PATH];
} PafCtxMenu;

#define SEI_THIS(p) CONTAINING_RECORD((p), PafCtxMenu, sei)

/* ── Helper: run extraction then open destination in Explorer ── */
static void RunExtract(const WCHAR* pafW, const WCHAR* outW) {
    char pafA[MAX_PATH * 3], outA[MAX_PATH * 3];
    WtoA(pafW, pafA, sizeof pafA);
    WtoA(outW, outA, sizeof outA);
    CreateDirectoryW(outW, NULL);
    paf_extract_binary(pafA, outA, 1 /*overwrite*/);
    ShellExecuteW(NULL, L"explore", outW, NULL, NULL, SW_SHOWNORMAL);
}

/* ── IContextMenu ── */
static HRESULT STDMETHODCALLTYPE
CM_QueryInterface(IContextMenu* pCM, REFIID riid, void** ppv) {
    PafCtxMenu* p = (PafCtxMenu*)pCM;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IContextMenu)) {
        *ppv = &p->cm;
    } else if (IsEqualIID(riid, &IID_IShellExtInit)) {
        *ppv = &p->sei;
    } else { *ppv = NULL; return E_NOINTERFACE; }
    ((IUnknown*)*ppv)->lpVtbl->AddRef((IUnknown*)*ppv);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE CM_AddRef(IContextMenu* pCM) {
    return InterlockedIncrement(&((PafCtxMenu*)pCM)->cRef);
}
static ULONG STDMETHODCALLTYPE CM_Release(IContextMenu* pCM) {
    PafCtxMenu* p = (PafCtxMenu*)pCM;
    ULONG n = InterlockedDecrement(&p->cRef);
    if (!n) { LocalFree(p); InterlockedDecrement(&g_refCount); }
    return n;
}
static HRESULT STDMETHODCALLTYPE
CM_QueryContextMenu(IContextMenu* pCM, HMENU hMenu, UINT idx,
                    UINT idFirst, UINT idLast, UINT uFlags)
{
    (void)idLast;
    if (uFlags & CMF_DEFAULTONLY) return S_OK;
    InsertMenuW(hMenu, idx,   MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, idx+1, MF_BYPOSITION | MF_STRING,
                idFirst + CMD_EXTRACT_HERE, L"PAF: &ここに展開 (Extract Here)");
    InsertMenuW(hMenu, idx+2, MF_BYPOSITION | MF_STRING,
                idFirst + CMD_EXTRACT_TO,   L"PAF: フォルダに&展開... (Extract To...)");
    return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, CMD_COUNT + 1);
}
static HRESULT STDMETHODCALLTYPE
CM_InvokeCommand(IContextMenu* pCM, LPCMINVOKECOMMANDINFO pici) {
    PafCtxMenu* p = (PafCtxMenu*)pCM;
    if (HIWORD(pici->lpVerb)) return E_INVALIDARG;
    UINT cmd = LOWORD((UINT_PTR)pici->lpVerb);

    if (cmd == CMD_EXTRACT_HERE) {
        WCHAR dir[MAX_PATH];
        wcsncpy_s(dir, MAX_PATH, p->path, _TRUNCATE);
        PathRemoveFileSpecW(dir);
        RunExtract(p->path, dir);
        return S_OK;
    }
    if (cmd == CMD_EXTRACT_TO) {
        /* Default destination: same dir + archive stem */
        WCHAR stem[MAX_PATH];
        wcsncpy_s(stem, MAX_PATH, p->path, _TRUNCATE);
        PathRemoveExtensionW(stem);

        WCHAR outDir[MAX_PATH] = {0};
        BROWSEINFOW bi = {0};
        bi.hwndOwner = pici->hwnd;
        bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        bi.lpszTitle = L"展開先フォルダを選んでください";
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (!pidl) return S_OK;
        SHGetPathFromIDListW(pidl, outDir);
        CoTaskMemFree(pidl);
        PathAppendW(outDir, PathFindFileNameW(stem));
        RunExtract(p->path, outDir);
        return S_OK;
    }
    return E_INVALIDARG;
}
static HRESULT STDMETHODCALLTYPE
CM_GetCommandString(IContextMenu* pCM, UINT_PTR idCmd, UINT uType,
                    UINT* pRes, LPSTR pszName, UINT cch)
{
    (void)pCM; (void)pRes;
    if (idCmd == CMD_EXTRACT_HERE) {
        if (uType & GCS_HELPTEXTW)
            wcsncpy_s((WCHAR*)pszName, cch, L"PAFアーカイブをここに展開します", _TRUNCATE);
        else if (uType & GCS_HELPTEXTA)
            strncpy_s(pszName, cch, "PAF: Extract Here", _TRUNCATE);
        return S_OK;
    }
    return E_INVALIDARG;
}

/* ── IShellExtInit ── */
static HRESULT STDMETHODCALLTYPE
SEI_QueryInterface(IShellExtInit* p, REFIID riid, void** ppv) {
    return CM_QueryInterface((IContextMenu*)SEI_THIS(p), riid, ppv);
}
static ULONG STDMETHODCALLTYPE SEI_AddRef(IShellExtInit* p) {
    return CM_AddRef((IContextMenu*)SEI_THIS(p));
}
static ULONG STDMETHODCALLTYPE SEI_Release(IShellExtInit* p) {
    return CM_Release((IContextMenu*)SEI_THIS(p));
}
static HRESULT STDMETHODCALLTYPE
SEI_Initialize(IShellExtInit* pSEI, LPCITEMIDLIST pidl,
               IDataObject* pDataObj, HKEY hkProgID)
{
    (void)pidl; (void)hkProgID;
    PafCtxMenu* p = SEI_THIS(pSEI);
    if (!pDataObj) return E_INVALIDARG;
    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm = {0};
    HRESULT hr = IDataObject_GetData(pDataObj, &fe, &sm);
    if (FAILED(hr)) return hr;
    HDROP hd = (HDROP)GlobalLock(sm.hGlobal);
    if (hd) { DragQueryFileW(hd, 0, p->path, MAX_PATH); GlobalUnlock(sm.hGlobal); }
    ReleaseStgMedium(&sm);
    return S_OK;
}

/* vtables */
static const IContextMenuVtbl  g_vtbl_CM = {
    CM_QueryInterface, CM_AddRef, CM_Release,
    CM_QueryContextMenu, CM_InvokeCommand, CM_GetCommandString
};
static const IShellExtInitVtbl g_vtbl_SEI = {
    SEI_QueryInterface, SEI_AddRef, SEI_Release, SEI_Initialize
};

static PafCtxMenu* PafCtxMenu_New(void) {
    PafCtxMenu* p = (PafCtxMenu*)LocalAlloc(LPTR, sizeof *p);
    if (!p) return NULL;
    p->cm.lpVtbl  = (IContextMenuVtbl*)&g_vtbl_CM;
    p->sei.lpVtbl = (IShellExtInitVtbl*)&g_vtbl_SEI;
    p->cRef = 1;
    InterlockedIncrement(&g_refCount);
    return p;
}

/* ─────────────────────────────────────────────────────────────────────────
   PART 2 — IPreviewHandler + IInitializeWithFile
   ───────────────────────────────────────────────────────────────────────── */

#define IDC_LIST_FILES  101
#define COL_PATH   0
#define COL_SIZE   1
#define COL_HASH   2

typedef struct {
    IPreviewHandler       ph;    /* offset 0 */
    IInitializeWithFile   iwf;
    LONG   cRef;
    WCHAR  path[MAX_PATH];
    HWND   hwndParent;
    HWND   hwndList;
    RECT   rcParent;
} PafPreview;

#define IWF_THIS(p) CONTAINING_RECORD((p), PafPreview, iwf)

/* ── IInitializeWithFile ── */
static HRESULT STDMETHODCALLTYPE
PH_QI(IPreviewHandler* p, REFIID riid, void** ppv);
static ULONG STDMETHODCALLTYPE PH_AddRef(IPreviewHandler* p);
static ULONG STDMETHODCALLTYPE PH_Release(IPreviewHandler* p);

static HRESULT STDMETHODCALLTYPE
IWF_QI(IInitializeWithFile* p, REFIID riid, void** ppv) {
    return PH_QI((IPreviewHandler*)IWF_THIS(p), riid, ppv);
}
static ULONG STDMETHODCALLTYPE IWF_AddRef(IInitializeWithFile* p) {
    return PH_AddRef((IPreviewHandler*)IWF_THIS(p));
}
static ULONG STDMETHODCALLTYPE IWF_Release(IInitializeWithFile* p) {
    return PH_Release((IPreviewHandler*)IWF_THIS(p));
}
static HRESULT STDMETHODCALLTYPE
IWF_Initialize(IInitializeWithFile* pIWF, LPCWSTR pszFile, DWORD grfMode) {
    (void)grfMode;
    PafPreview* p = IWF_THIS(pIWF);
    wcsncpy_s(p->path, MAX_PATH, pszFile, _TRUNCATE);
    return S_OK;
}

/* ── Build preview ListView ── */
static void BuildPreviewList(PafPreview* p) {
    char pathA[MAX_PATH * 3];
    WtoA(p->path, pathA, sizeof pathA);

    PafList list;
    if (paf_list_binary(pathA, &list) != 0) return;

    ListView_DeleteAllItems(p->hwndList);

    for (UINT i = 0; i < list.count; i++) {
        /* Widen UTF-8 path */
        WCHAR pathW[1024];
        MultiByteToWideChar(CP_UTF8, 0, list.entries[i].path, -1, pathW, 1024);

        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = (int)i;
        lvi.iSubItem = COL_PATH;
        lvi.pszText  = pathW;
        ListView_InsertItem(p->hwndList, &lvi);

        /* Size column */
        WCHAR sz[32];
        _snwprintf_s(sz, 32, _TRUNCATE, L"%u", list.entries[i].size);
        ListView_SetItemText(p->hwndList, (int)i, COL_SIZE, sz);

        /* SHA-256 column: first 8 hex chars + "…" */
        const uint8_t* h = list.entries[i].hash;
        WCHAR hash[64];
        _snwprintf_s(hash, 64, _TRUNCATE,
            L"%02x%02x%02x%02x…", h[0], h[1], h[2], h[3]);
        ListView_SetItemText(p->hwndList, (int)i, COL_HASH, hash);
    }
    free_paf_list(&list);
}

/* ── IPreviewHandler ── */
static HRESULT STDMETHODCALLTYPE PH_QI(IPreviewHandler* p, REFIID riid, void** ppv) {
    PafPreview* pp = (PafPreview*)p;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPreviewHandler)) {
        *ppv = &pp->ph;
    } else if (IsEqualIID(riid, &IID_IInitializeWithFile)) {
        *ppv = &pp->iwf;
    } else { *ppv = NULL; return E_NOINTERFACE; }
    ((IUnknown*)*ppv)->lpVtbl->AddRef((IUnknown*)*ppv);
    return S_OK;
}
static ULONG STDMETHODCALLTYPE PH_AddRef(IPreviewHandler* p) {
    return InterlockedIncrement(&((PafPreview*)p)->cRef);
}
static ULONG STDMETHODCALLTYPE PH_Release(IPreviewHandler* p) {
    PafPreview* pp = (PafPreview*)p;
    ULONG n = InterlockedDecrement(&pp->cRef);
    if (!n) { LocalFree(pp); InterlockedDecrement(&g_refCount); }
    return n;
}
static HRESULT STDMETHODCALLTYPE
PH_SetWindow(IPreviewHandler* p, HWND hwnd, const RECT* prc) {
    PafPreview* pp = (PafPreview*)p;
    pp->hwndParent = hwnd;
    if (prc) pp->rcParent = *prc;
    if (pp->hwndList)
        SetWindowPos(pp->hwndList, NULL,
            prc->left, prc->top,
            prc->right - prc->left, prc->bottom - prc->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE
PH_SetRect(IPreviewHandler* p, const RECT* prc) {
    return PH_SetWindow(p, ((PafPreview*)p)->hwndParent, prc);
}
static HRESULT STDMETHODCALLTYPE PH_DoPreview(IPreviewHandler* p) {
    PafPreview* pp = (PafPreview*)p;
    if (!pp->hwndParent) return E_FAIL;

    /* Create ListView */
    RECT rc = pp->rcParent;
    pp->hwndList = CreateWindowExW(0, WC_LISTVIEWW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
        pp->hwndParent, (HMENU)(UINT_PTR)IDC_LIST_FILES, g_hModule, NULL);
    if (!pp->hwndList) return E_FAIL;

    ListView_SetExtendedListViewStyle(pp->hwndList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    /* Columns */
    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.pszText = L"ファイルパス"; col.cx = 340;
    ListView_InsertColumn(pp->hwndList, COL_PATH, &col);
    col.pszText = L"サイズ (B)";   col.cx = 90;
    ListView_InsertColumn(pp->hwndList, COL_SIZE, &col);
    col.pszText = L"SHA-256";       col.cx = 100;
    ListView_InsertColumn(pp->hwndList, COL_HASH, &col);

    BuildPreviewList(pp);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE PH_Unload(IPreviewHandler* p) {
    PafPreview* pp = (PafPreview*)p;
    if (pp->hwndList) { DestroyWindow(pp->hwndList); pp->hwndList = NULL; }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE PH_SetFocus(IPreviewHandler* p) {
    PafPreview* pp = (PafPreview*)p;
    if (pp->hwndList) SetFocus(pp->hwndList);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE PH_QueryFocus(IPreviewHandler* p, HWND* phwnd) {
    *phwnd = GetFocus(); return S_OK;
}
static HRESULT STDMETHODCALLTYPE
PH_TranslateAccelerator(IPreviewHandler* p, MSG* pmsg) { return S_FALSE; }

/* vtables */
static const IInitializeWithFileVtbl g_vtbl_IWF = {
    IWF_QI, IWF_AddRef, IWF_Release, IWF_Initialize
};
static const IPreviewHandlerVtbl g_vtbl_PH = {
    PH_QI, PH_AddRef, PH_Release,
    PH_SetWindow, PH_SetRect, PH_DoPreview, PH_Unload,
    PH_SetFocus, PH_QueryFocus, PH_TranslateAccelerator
};

static PafPreview* PafPreview_New(void) {
    PafPreview* p = (PafPreview*)LocalAlloc(LPTR, sizeof *p);
    if (!p) return NULL;
    p->ph.lpVtbl  = (IPreviewHandlerVtbl*)&g_vtbl_PH;
    p->iwf.lpVtbl = (IInitializeWithFileVtbl*)&g_vtbl_IWF;
    p->cRef = 1;
    InterlockedIncrement(&g_refCount);
    return p;
}

/* ─────────────────────────────────────────────────────────────────────────
   PART 3 — Class factories
   ───────────────────────────────────────────────────────────────────────── */

typedef struct {
    IClassFactory cf;
    LONG          cRef;
    const GUID*   clsid;
} PafCF;

static HRESULT STDMETHODCALLTYPE
CF_QI(IClassFactory* p, REFIID riid, void** ppv) {
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory)) {
        *ppv = p;
        ((IUnknown*)p)->lpVtbl->AddRef((IUnknown*)p);
        return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE CF_AddRef(IClassFactory* p) {
    return InterlockedIncrement(&((PafCF*)p)->cRef);
}
static ULONG STDMETHODCALLTYPE CF_Release(IClassFactory* p) {
    PafCF* cf = (PafCF*)p;
    ULONG n = InterlockedDecrement(&cf->cRef);
    if (!n) LocalFree(cf);
    return n;
}
static HRESULT STDMETHODCALLTYPE
CF_CreateInstance(IClassFactory* p, IUnknown* pUnkOuter, REFIID riid, void** ppv) {
    PafCF* cf = (PafCF*)p;
    *ppv = NULL;
    if (pUnkOuter) return CLASS_E_NOAGGREGATION;

    if (IsEqualGUID(cf->clsid, &CLSID_PafContextMenu)) {
        PafCtxMenu* obj = PafCtxMenu_New();
        if (!obj) return E_OUTOFMEMORY;
        HRESULT hr = CM_QueryInterface(&obj->cm, riid, ppv);
        CM_Release(&obj->cm);
        return hr;
    }
    if (IsEqualGUID(cf->clsid, &CLSID_PafPreviewHandler)) {
        PafPreview* obj = PafPreview_New();
        if (!obj) return E_OUTOFMEMORY;
        HRESULT hr = PH_QI(&obj->ph, riid, ppv);
        PH_Release(&obj->ph);
        return hr;
    }
    return CLASS_E_CLASSNOTAVAILABLE;
}
static HRESULT STDMETHODCALLTYPE CF_LockServer(IClassFactory* p, BOOL fLock) {
    if (fLock) InterlockedIncrement(&g_refCount);
    else        InterlockedDecrement(&g_refCount);
    return S_OK;
}
static const IClassFactoryVtbl g_vtbl_CF = {
    CF_QI, CF_AddRef, CF_Release, CF_CreateInstance, CF_LockServer
};

/* ─────────────────────────────────────────────────────────────────────────
   PART 4 — DLL entry points
   ───────────────────────────────────────────────────────────────────────── */

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hInst;
        DisableThreadLibraryCalls(hInst);
        InitCommonControls();
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    *ppv = NULL;
    if (!IsEqualGUID(rclsid, &CLSID_PafContextMenu) &&
        !IsEqualGUID(rclsid, &CLSID_PafPreviewHandler))
        return CLASS_E_CLASSNOTAVAILABLE;

    PafCF* cf = (PafCF*)LocalAlloc(LPTR, sizeof *cf);
    if (!cf) return E_OUTOFMEMORY;
    cf->cf.lpVtbl = (IClassFactoryVtbl*)&g_vtbl_CF;
    cf->cRef  = 1;
    cf->clsid = IsEqualGUID(rclsid, &CLSID_PafContextMenu)
                    ? &CLSID_PafContextMenu : &CLSID_PafPreviewHandler;

    HRESULT hr = CF_QI(&cf->cf, riid, ppv);
    CF_Release(&cf->cf);
    return hr;
}

STDAPI DllCanUnloadNow(void) {
    return g_refCount == 0 ? S_OK : S_FALSE;
}

/* ─── Registry helpers for DllRegisterServer / DllUnregisterServer ─── */

static void BuildClsidPath(WCHAR* buf, int cch, const GUID* g, const WCHAR* sub) {
    WCHAR guid[64];
    GuidToStr(g, guid, 64);
    _snwprintf_s(buf, cch, _TRUNCATE, L"CLSID\\%s%s%s", guid, sub ? L"\\" : L"", sub ? sub : L"");
}

STDAPI DllRegisterServer(void) {
    WCHAR dllPath[MAX_PATH];
    GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);
    WCHAR guid_cm[64], guid_ph[64], key[256];
    GuidToStr(&CLSID_PafContextMenu,    guid_cm, 64);
    GuidToStr(&CLSID_PafPreviewHandler, guid_ph, 64);

    /* ── Context menu CLSID ── */
    BuildClsidPath(key, 256, &CLSID_PafContextMenu, NULL);
    RegSetSzW(HKEY_CLASSES_ROOT, key, NULL, L"PAF Context Menu Handler");
    BuildClsidPath(key, 256, &CLSID_PafContextMenu, L"InprocServer32");
    RegSetSzW(HKEY_CLASSES_ROOT, key, NULL, dllPath);
    RegSetSzW(HKEY_CLASSES_ROOT, key, L"ThreadingModel", L"Apartment");

    /* ── Preview handler CLSID ── */
    BuildClsidPath(key, 256, &CLSID_PafPreviewHandler, NULL);
    RegSetSzW(HKEY_CLASSES_ROOT, key, NULL, L"PAF Preview Handler");
    BuildClsidPath(key, 256, &CLSID_PafPreviewHandler, L"InprocServer32");
    RegSetSzW(HKEY_CLASSES_ROOT, key, NULL, dllPath);
    RegSetSzW(HKEY_CLASSES_ROOT, key, L"ThreadingModel", L"Apartment");

    /* ── .paf file association ── */
    RegSetSzW(HKEY_CLASSES_ROOT, L".paf",                         NULL, L"PAF.Archive");
    RegSetSzW(HKEY_CLASSES_ROOT, L"PAF.Archive",                  NULL, L"PAF Archive");
    RegSetSzW(HKEY_CLASSES_ROOT, L"PAF.Archive\\DefaultIcon",     NULL, L"shell32.dll,1");

    /* Viewer: paf_viewer.exe lives next to paf_shell.dll */
    WCHAR viewerPath[MAX_PATH + 8];
    wcsncpy_s(viewerPath, MAX_PATH + 8, dllPath, _TRUNCATE);
    PathRemoveFileSpecW(viewerPath);
    PathAppendW(viewerPath, L"paf_viewer.exe");
    WCHAR openCmd[MAX_PATH + 16];
    _snwprintf_s(openCmd, MAX_PATH + 16, _TRUNCATE, L"\"%s\" \"%%1\"", viewerPath);
    RegSetSzW(HKEY_CLASSES_ROOT, L"PAF.Archive\\shell\\open\\command", NULL, openCmd);

    /* ── Shell extension handlers on PAF.Archive ── */
    _snwprintf_s(key, 256, _TRUNCATE,
        L"PAF.Archive\\shellex\\ContextMenuHandlers\\PAFShellExt");
    RegSetSzW(HKEY_CLASSES_ROOT, key, NULL, guid_cm);

    _snwprintf_s(key, 256, _TRUNCATE,
        L"PAF.Archive\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}");
    RegSetSzW(HKEY_CLASSES_ROOT, key, NULL, guid_ph);

    /* ── Approve extensions (required for Explorer) ── */
    const WCHAR* approvedPath =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
    RegSetSzW(HKEY_LOCAL_MACHINE, approvedPath, guid_cm, L"PAF Context Menu Handler");
    RegSetSzW(HKEY_LOCAL_MACHINE, approvedPath, guid_ph, L"PAF Preview Handler");

    /* ── Register preview handler ── */
    const WCHAR* phPath =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers";
    RegSetSzW(HKEY_LOCAL_MACHINE, phPath, guid_ph, L"PAF Preview Handler");

    /* Notify shell */
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}

STDAPI DllUnregisterServer(void) {
    WCHAR guid_cm[64], guid_ph[64], key[256];
    GuidToStr(&CLSID_PafContextMenu,    guid_cm, 64);
    GuidToStr(&CLSID_PafPreviewHandler, guid_ph, 64);

    /* Remove CLSIDs */
    BuildClsidPath(key, 256, &CLSID_PafContextMenu, NULL);
    RegDeleteTreeSafe(HKEY_CLASSES_ROOT, key);
    BuildClsidPath(key, 256, &CLSID_PafPreviewHandler, NULL);
    RegDeleteTreeSafe(HKEY_CLASSES_ROOT, key);

    /* Remove file association */
    RegDeleteTreeSafe(HKEY_CLASSES_ROOT, L".paf");
    RegDeleteTreeSafe(HKEY_CLASSES_ROOT, L"PAF.Archive");

    /* Remove approvals */
    const WCHAR* approvedPath =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
    HKEY hk;
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, approvedPath, 0, KEY_WRITE, &hk)) {
        RegDeleteValueW(hk, guid_cm);
        RegDeleteValueW(hk, guid_ph);
        RegCloseKey(hk);
    }
    const WCHAR* phPath =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers";
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, phPath, 0, KEY_WRITE, &hk)) {
        RegDeleteValueW(hk, guid_ph);
        RegCloseKey(hk);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return S_OK;
}
