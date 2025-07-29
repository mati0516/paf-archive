# 📦 Parallel Archive Format (PAF)

A simple, fast, and transparent archiving format designed for maximum I/O performance and ease of use.

Supports both C and Python implementations.  
Compression-free, instantly mountable, and CLI-friendly.

## 🚀 Features

- ✅ No compression (raw file copy)
- ✅ Fast and parallel-safe creation
- ✅ Instant listing and extraction
- ✅ Full folder hierarchy support
- ✅ UTF-8 filename support (including Japanese)
- ✅ CRC32 verification per file
- ✅ `.pafignore` support (like `.gitignore`) ← WIP
- ✅ C library + Python CLI interface
- ✅ Cross-platform (Windows / Linux / macOS)
- ✅ MIT Licensed

## 📦 Usage (Python CLI)

Install locally:

```bash
pip install -e .
```

Then use:

```bash
paf create archive.paf myfolder/
paf ls archive.paf
paf extract archive.paf output_folder/
paf to-iso archive.paf iso_extract_dir/
```

## ⚙️ Usage (C Library)

```c
#include "libpaf.h"

const char* files[] = {"file1.txt", "dir2"};
paf_create("out.paf", files, 2);

paf_extract_all("out.paf", "output/");
```

To verify individual file:

```c
int ok = paf_verify_file("out.paf", "dir2/hello.txt"); // returns 0 if valid
```

You can also extract to a directory structure:

```c
paf_extract_to_dir("out.paf", "iso_extract/");
```

## 🛠 Tools

- `libpaf/` – C implementation of the PAF format
- `tools/python/paf/` – Python CLI & module (`paf create`, `extract`, `ls`, etc.)
- `setup.py` – pip install support
- `.pafignore` – optional ignore file for archive filtering

## 📄 Format Structure

- 32-byte header
- Concatenated file data
- File index (file count, names, sizes, offsets)

## 📜 License

MIT License

---

## 📑 Format Specification (PAF v1)

### Overview

The PAF (Parallel Archive Format) is a simple binary format designed for high-speed archiving without compression.  
It consists of three main parts: **Header**, **File Data Block**, and **File Index Block**.

---

### Format Layout

```
+----------------------+  Offset 0
| Header (32 bytes)    |
+----------------------+
| File Data Block      |
| (concatenated files) |
+----------------------+
| File Index Block     |
+----------------------+
```

---

### 🧱 Header (32 bytes)

| Bytes | Type     | Description |
|-------|----------|-------------|
| 0–3   | char[4]  | Magic number: `b'PAF'` |
| 4–7   | uint32   | File count |
| 8–11  | uint32   | Offset to index block (from start of file) |
| 12–31 | reserved | Reserved for future use (20 bytes, zero-filled) |

---

### 📦 File Data Block

- Raw binary data of each file, concatenated in order.
- No compression, encryption, or padding.
- Offset and size of each file is tracked in the index block.

---

### 📋 File Index Block

Each file entry in the index block is encoded as:

| Bytes              | Type                  | Description            |
|--------------------|-----------------------|------------------------|
| 0–1                | uint16                | Filename length (in bytes) |
| 2–(2+N-1)          | utf-8 encoded string  | Filename (N bytes, not null-terminated) |
| (2+N)–(5+N)        | uint32                | File size (in bytes)   |
| (6+N)–(9+N)        | uint32                | Offset (from start of data block) |

> ✅ Filenames use UTF-8 and may contain slashes (`/`) for directory structure.

---

### 📘 Notes

- Directory structure is preserved via `/` in filenames (not as actual folders).
- File offsets in the index block are relative to the start of the data block (not the whole file).
- Reserved bytes in the header (bytes 12–31) may be used in future versions.
