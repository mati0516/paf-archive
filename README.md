# 📦 Parallel Archive Format (PAF) - Beta (Pre-v1.0)

A next-generation, high-speed, non-compressed archive format optimized for **GPU acceleration**, **Parallel I/O**, and **100k+ file datasets**.
*Current Status: Beta. Version 1.0 will be released upon implementation of GPU acceleration or CPU multi-threading (Goal: 1 million files in seconds).*

## 🚀 Key Features

- ✅ **GPU-Ready Layout**: Fixed-length index structure (64 bytes) designed for massive parallel access.
- ✅ **Physical Limit Performance**: Uses "Separate & Merge" strategy to eliminate disk seeks during archive creation.
- ✅ **Zero-Copy Indexing**: Instantly parse 100k+ file structures by loading the index directly into VRAM/RAM.
- ✅ **Asynchronous I/O Support**: Architecture designed for `io_uring` and `DirectStorage`.
- ✅ **SHA-256 Hash**: High-security integrity verification for every file (ideal for large datasets).
- ✅ **Mobile Optimized**: Support for Android (Vulkan/Posix) and iPhone (Metal/Darwin) environments.
- ✅ **Cross-Platform**: Core C library buildable on Windows (MSYS2/MinGW), Linux, Android, and iOS.
- ✅ **UTF-8 Support**: Full support for multibyte filenames and complex hierarchies.

## ⚙️ Usage (C Library)

### Archive Creation (Generation Phase)

```c
#include "paf_generator.h"

paf_generator_t gen;
paf_generator_init(&gen);

// Add files
paf_generator_add_file(&gen, "hello.txt", (uint8_t*)"Hello World", 11);
paf_generator_add_file(&gen, "data/huge.bin", buffer, 1024*1024);

// Finalize (Binary Merge)
paf_generator_finalize(&gen, "output.paf");
paf_generator_cleanup(&gen);
```

### Archive Extraction (Extraction Phase)

```c
#include "paf_extractor.h"

paf_extractor_t ext;
paf_extractor_open(&ext, "output.paf");

char path[1024];
uint8_t* data;
uint64_t size;

// Random access by index
paf_extractor_get_file(&ext, 0, path, &data, &size);
printf("Extracted: %s (%lu bytes)\n", path, size);

paf_extractor_close(&ext);
```

## 📄 Format Layout (PAF v1)

Designed for maximum parsing efficiency.

```
+----------------------+
| Header (32 bytes)    |
+----------------------+
| Data Block           | (Raw file contents)
+----------------------+
| Index Block          | (Fixed-length 40-byte entries)
+----------------------+
| Path Buffer          | (Aggregated path strings)
+----------------------+
```

### Header (32 bytes)

| Offset | Type    | Description                  |
|--------|---------|------------------------------|
| 0–3    | char[4] | Magic: `'PAF1'`              |
| 4–7    | uint32  | Version: `1`                 |
| 8–11   | uint32  | Flags                        |
| 12–15  | uint32  | File count                   |
| 16–23  | uint64  | Offset to Index Block        |
| 24–31  | uint64  | Offset to Path Buffer        |

### Index Entry (64 bytes - Fixed Size)

| Offset | Type    | Description                      |
|--------|---------|----------------------------------|
| 0–7    | uint64  | Path Buffer Offset               |
| 8–11   | uint32  | Path Length                      |
| 12–15  | uint32  | Flags                            |
| 16–23  | uint64  | Data Offset (relative to block)  |
| 24–31  | uint64  | Data Size                        |
| 32–63  | uint8[32]| SHA-256 Hash                    |

## 🧪 Development

### Building via WSL (Ubuntu/gcc)
```bash
# Build test
wsl gcc -O2 -Wall -Ilibpaf libpaf/*.c test/test_paf_create.c -o test_paf
wsl ./test_paf
```

---

## 📜 License
MIT License
