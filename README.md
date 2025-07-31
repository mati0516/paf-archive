# 📦 Parallel Archive Format (PAF)

A high-speed, non-compressed archive format optimized for ease of use, transparency, and cross-platform support.

## 🚀 Key Features

- ✅ No compression (raw file copy)
- ✅ Ultra-fast archive creation and extraction
- ✅ UTF-8 filename support (including Japanese/multibyte)
- ✅ Full folder hierarchy and recursive directory support
- ✅ `.pafignore` auto-detection and explicit specification
- ✅ `--ignore-from` support (uses `.gitignore`-style wildcards via `fnmatch()`)
- ✅ CRC32 checksum per file
- ✅ Path traversal protection (safe extraction)
- ✅ Overwrite protection (root overwrite disabled by default)
- ✅ C library (shared: `.so`, `.dll`, `.dylib`)
- ✅ Python CLI / WASM / GUI planned
- ✅ Fully cross-platform (Windows / Linux / macOS)
- ✅ MIT Licensed

## 📦 Usage (Python CLI)

Install:

```bash
pip install -e .
```

Then run:

```bash
paf create archive.paf myfolder/
paf ls archive.paf
paf extract archive.paf out/
paf extract archive.paf out/ --overwrite
```

⚠️ Note: Only the C library has been fully tested as of now. Python CLI is under redevelopment and temporarily excluded from this version.

## ⚙️ Usage (C Library)

```c
#include "libpaf.h"

const char* paths[] = {"file1.txt", "dir2"};
paf_create_binary("out.paf", paths, 2, ".pafignore", 1); // last arg: recursive-ignore
```

Extract all:

```c
paf_extract_binary("out.paf", "out/", 0); // overwrite = 0
```

Extract single file or folder:

```c
paf_extract_file("out.paf", "dir2/note.txt", "out_single/");
paf_extract_folder("out.paf", "dir2/", "out_dir/");
```

List entries:

```c
PafList list;
if (paf_list_binary("out.paf", &list) == 0) {
    for (uint32_t i = 0; i < list.count; ++i) {
        printf("%s (%u bytes)\n", list.entries[i].path, list.entries[i].size);
    }
    free_paf_list(&list);
}
```

## 🧪 Test All Features

Run from `paf-archive/test/`:

```cmd
test.cmd
```

## 📁 Project Structure

```
paf-archive/
├── libpaf/             # Core C library
│   ├── libpaf_core.c
│   ├── libpaf_list.c
│   ├── libpaf_extract.c
│   ├── libpaf_exists.c
│   ├── libpaf_extra.c
│   └── libpaf.h
├── test/               # C-based functional tests
│   ├── test_all.c
│   ├── test.cmd
│   └── ...
├── dist/               # Built shared libraries
│   ├── windows/libpaf.dll
│   ├── linux/libpaf.so
│   └── macos/libpaf.dylib
├── bindings/           # Multi-language bindings
│   ├── python/
│   ├── go/
│   ├── rust/
│   ├── csharp/
│   └── node/
├── wasm/               # Planned browser viewer (WASM/JS)
│   ├── index.html
│   └── paf.js / paf.wasm
```

## 📄 Format Layout (PAF v1)

```
+----------------------+
| Header (32 bytes)    |
+----------------------+
| File Data Block      |
+----------------------+
| File Index Block     |
+----------------------+
```

### Header (32 bytes)

| Offset | Type    | Description                  |
|--------|---------|------------------------------|
| 0–3    | char[4] | Magic: `'PAF1'`              |
| 4–7    | uint32  | File count                   |
| 8–11   | uint32  | Offset to index block        |
| 12–31  | char[20]| Reserved (zero-filled)       |

### File Index Block

Each entry contains:

- Filename length (2 bytes)
- UTF-8 path (N bytes)
- File size (4 bytes)
- Offset (4 bytes)
- CRC32 (4 bytes)

---

## 📜 License

MIT License
