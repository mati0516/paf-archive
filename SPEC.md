# PAF Format Specification

Version 2 · Revision 2026-05-22

This document defines the PAF binary format at the byte level.  
It is intended for implementors of AntiVirus engines, DLP tools, archive managers, and file system drivers that wish to natively support PAF archives.

---

## 1. Design Goals

| Goal | How PAF achieves it |
|:---|:---|
| AntiVirus transparency | File data is stored **uncompressed**. Every engine that can read raw bytes can scan file contents directly. |
| Hash-first scanning | Every file's SHA-256 is in the **index block**, which appears after the data block. An engine can seek to the index and check all hashes against its malware database **without reading any file data**. |
| Tamper detection | SHA-256 covers file contents. Modifying any byte in the data block invalidates the stored hash. |
| Random access | Fixed-size index entries (128 bytes each) allow O(1) seek to any file's metadata or data. |
| Delta efficiency | Because hashes are embedded, two PAF archives can be diffed in O(N) without re-reading file data. |

---

## 2. File Layout

```
Offset 0
┌─────────────────────────────────┐
│  Header          32 bytes       │
├─────────────────────────────────┤
│  Data Block      variable       │  ← absent when PAF_FLAG_INDEX_ONLY
├─────────────────────────────────┤
│  Index Block     N × 128 bytes  │  ← N = header.file_count
├─────────────────────────────────┤
│  Path Buffer     variable       │
└─────────────────────────────────┘
```

All multi-byte integers are **little-endian**.  
All offsets are **absolute byte offsets** from the start of the file.

---

## 3. Header (32 bytes, offset 0)

```
Offset  Size  Type      Field           Description
──────  ────  ────────  ──────────────  ──────────────────────────────────
0       4     char[4]   magic           Always "PAF1" (0x50 0x41 0x46 0x31)
4       4     uint32    version         Always 2
8       4     uint32    flags           Header-level flags (see §3.1)
12      4     uint32    file_count      Number of index entries (N)
16      8     uint64    index_offset    Absolute offset of Index Block
24      8     uint64    path_offset     Absolute offset of Path Buffer
```

### 3.1 Header Flags

| Bit | Name | Value | Meaning |
|:---|:---|:---|:---|
| 0 | `PAF_EXTRACT_SMART_OVERWRITE` | `0x01` | Hint: skip identical files during extraction |
| 1 | `PAF_FLAG_INDEX_ONLY` | `0x02` | Data Block absent; `data_offset` fields are meaningless |

Implementors **must** ignore unknown flag bits.

---

## 4. Data Block

When `PAF_FLAG_INDEX_ONLY` is **not** set, the Data Block immediately follows the Header (offset 32).  
It is a flat concatenation of all file payloads with no inter-file padding or alignment.  
The position and length of each file's payload are given by the corresponding index entry.

When `PAF_FLAG_INDEX_ONLY` **is** set, the Data Block is absent and `index_offset` equals 32.

---

## 5. Index Block (N × 128 bytes)

The Index Block starts at `header.index_offset`.  
Each entry is exactly **128 bytes**, laid out as follows:

```
Offset  Size  Type      Field                 Description
──────  ────  ────────  ──────────────────    ──────────────────────────────────────
0       8     uint64    path_buffer_offset    Byte offset within Path Buffer
8       4     uint32    path_length           Byte length of the UTF-8 path (no NUL)
12      4     uint32    flags                 Per-entry flags (see §5.1)
16      8     uint64    data_offset           Absolute offset of this file's data
24      8     uint64    data_size             Size of this file's data in bytes
32      32    uint8[32] hash                  SHA-256 of the file contents (see §5.2)
64      32    uint8[32] hash_reserved         Zero-filled; reserved for SHA-512/BLAKE3
96      32    uint8[32] reserved              Zero-filled; reserved for future use
```

### 5.1 Per-Entry Flags

| Bit | Name | Value | Meaning |
|:---|:---|:---|:---|
| 2 | `PAF_ENTRY_DELETED` | `0x04` | File was deleted; `data_size` is 0; no data in the Data Block |
| 3 | `PAF_ENTRY_BINARY_DELTA` | `0x08` | `data_offset`/`data_size` point to a PAFD delta (see §7), not raw file data |

Implementors **must** ignore unknown flag bits.

### 5.2 Hash Field

`hash[0..31]` is the **SHA-256** of the file's raw uncompressed contents.

- For regular entries: SHA-256 of the bytes at `[data_offset, data_offset + data_size)`.
- For `PAF_ENTRY_BINARY_DELTA` entries: SHA-256 of the file **after** applying the delta (i.e. the new/target file).
- For `PAF_ENTRY_DELETED` entries: all 32 bytes are zero.

`hash[32..63]` is zero-filled in version 2 and reserved for a future algorithm (SHA-512 or BLAKE3).

#### AntiVirus hash-based scanning

An AntiVirus engine that maintains a hash database can detect known malware **without reading the Data Block**:

1. Seek to `header.index_offset`.
2. For each of the `N` entries, read the 128-byte entry.
3. Check `entry.hash[0..31]` against the malware hash database.
4. If a match is found, the file at path `path_buffer[path_buffer_offset .. path_buffer_offset + path_length]` is flagged.

Total I/O: 32 bytes (header) + N × 128 bytes (index). The Data Block is never read.

---

## 6. Path Buffer

The Path Buffer starts at `header.path_offset` and extends to end of file.  
It is a flat byte array of **UTF-8 encoded file paths with no NUL terminators**.

To retrieve the path for index entry `i`:

```
path_bytes = file[path_offset + entry[i].path_buffer_offset
                  .. path_offset + entry[i].path_buffer_offset + entry[i].path_length]
path_string = UTF-8 decode(path_bytes)
```

Path format rules:
- Forward slashes (`/`) are used as directory separators.
- Paths are **relative** — they never start with `/` or a drive letter.
- `..` components are forbidden. Implementations **must** reject archives that contain them.
- Maximum path length: 4095 bytes encoded in UTF-8.

---

## 7. PAFD Binary Delta Format

When `PAF_ENTRY_BINARY_DELTA` is set, the data at `[data_offset, data_offset + data_size)` is a **PAFD** instruction stream, not raw file data.

```
Offset  Size  Type      Field          Description
──────  ────  ────────  ─────────────  ──────────────────────────────
0       4     char[4]   magic          "PAFD" (0x50 0x41 0x46 0x44)
4       4     uint32    instr_count    Number of instructions
8       8     uint64    old_file_size  Expected size of the source file

8 + instr_count × variable   instruction stream
```

Each instruction:

```
Offset  Size  Type      Field    Description
──────  ────  ────────  ───────  ──────────────────────────────────────
0       1     uint8     type     0 = COPY, 1 = LITERAL
1       8     uint64    offset   Source offset (COPY) or unused (LITERAL)
9       4     uint32    size     Byte count
13      size  uint8[]   data     Present only for LITERAL instructions
```

Applying a PAFD delta:

1. Verify source file size equals `old_file_size`.
2. Iterate instructions in order:
   - `COPY`: append `size` bytes from `source_file[offset]`.
   - `LITERAL`: append the following `size` inline bytes.
3. Verify the resulting buffer's SHA-256 matches `entry.hash[0..31]`.

Matching uses 4 KB aligned blocks with FNV-1a 64-bit hashing.

---

## 8. Index-Only Archives (`.pafi`)

When `PAF_FLAG_INDEX_ONLY` is set, the archive contains **no file data** — only the header, index, and path buffer.

Use cases:
- Remote delta detection: ship a `.pafi` snapshot; compare two snapshots with O(N) hash comparison to find changed files.
- Integrity manifests: a `.pafi` file is a signed list of expected SHA-256 hashes for a software release.

A `.pafi` file is structurally identical to `.paf` with `data_size = 0` for all entries and `data_offset` fields set to zero.

---

## 9. Validation Rules

Implementations **must** reject an archive if any of the following are true:

| Check | Condition |
|:---|:---|
| Bad magic | `header.magic != "PAF1"` |
| Unknown version | `header.version != 2` |
| Index overflow | `header.index_offset + header.file_count * 128 > file_size` |
| Path overflow | any `path_buffer_offset + path_length > (file_size - header.path_offset)` |
| Unsafe path | any path contains `..` segments or starts with `/` or a drive letter |
| Data overflow | any `data_offset + data_size > file_size` (when data block is present) |
| Excessive file count | `file_count > 10,000,000` (implementation-defined limit; must be at least 1,000,000) |

---

## 10. MIME Type and File Extensions

| Extension | MIME type | Contents |
|:---|:---|:---|
| `.paf` | `application/x-paf` | Full archive (header + data + index + paths) |
| `.pafi` | `application/x-paf-index` | Index-only snapshot |

---

## 11. Reference Implementation

The reference C library (`libpaf`) is available at:  
https://github.com/mati0516/paf-archive

Key source files for implementors:

| File | Relevance |
|:---|:---|
| `libpaf/include/paf.h` | Header and index entry structs (`paf_header_t`, `paf_index_entry_t`) |
| `libpaf/include/libpaf.h` | High-level API (`paf_list_binary`, `paf_extract_binary`) |
| `libpaf/src/libpaf_list.c` | Index parsing |
| `libpaf/src/paf_extractor.c` | Random-access extraction |
| `libpaf/src/paf_binary_delta.c` | PAFD delta engine |
| `wasm/app.js` | Pure-JavaScript PAF index parser (no native dependency) |

The JavaScript parser in `wasm/app.js` (`parsePafIndex`) is a self-contained reference for languages without C FFI.

---

## 12. AntiVirus and DLP Integration Guide

### 12.1 Content scanning (byte-level)

Because PAF stores file data uncompressed:

1. Read `header.index_offset` and `header.path_offset`.
2. For each index entry with `data_size > 0` and no `PAF_ENTRY_DELETED` flag:
   - Seek to `data_offset`.
   - Read `data_size` bytes.
   - Scan as if the bytes were a standalone file with path `entry.path`.

No decompression is required.

### 12.2 Hash-based scanning (index-only)

See §5.2. Total I/O is proportional to the number of files, not their total size.

### 12.3 DLP classification

PAF contains no encryption and no compression.  
A DLP agent can:
- Enumerate all paths from the Path Buffer to apply filename-based policies.
- Read individual file payloads from the Data Block to apply content-based policies.
- Use the index SHA-256 to identify known sensitive documents by hash.

### 12.4 Integrity verification

To verify that a PAF archive has not been tampered with:

1. Parse the Index Block.
2. For each non-deleted entry, read `data_size` bytes from `data_offset`.
3. Compute SHA-256 of those bytes.
4. Compare with `entry.hash[0..31]`. Any mismatch indicates tampering.

Step 2–4 can be skipped for entries where the hash is already known to the scanner (hash-database hit in §12.2).
