# 📦 Parallel Archive Format (PAF) - Beta (Pre-v1.0)

A next-generation, high-speed, non-compressed archive format optimized for **GPU acceleration**, **Parallel I/O**, and **100k+ file datasets**.
*Current Status: Beta. Version 1.0 will be released upon implementation of GPU acceleration or CPU multi-threading (Goal: 1 million files in seconds).*

**The killer feature of PAF v1.** 
When extracting a 100M+ file archive, PAF doesn't just blindly overwrite everything. It uses its built-in SHA-256 index to:
1. Check if the file already exists.
2. Compare the hash of the local file with the archive.
3. **Skip writing** if they are identical.

This turns a multi-gigabyte extraction into a **near-instant differential sync**, saving both time and SSD lifespan.

## 🚀 Key Features
- ✅ **GPU-Ready Layout**: Fixed-length index structure (64 bytes) designed for massive parallel access.
- ✅ **Physical Limit Performance**: Uses "Separate & Merge" strategy to eliminate disk seeks.
- ✅ **Smart Overwrite**: High-speed differential extraction using SHA-256 verification.
- ✅ **Hash-based Deduplication**: Automatically shares data for identical files, saving storage.
- ✅ **GPU Grep**: Instant parallel search across 100M+ files using VRAM.
- ✅ **Mobile Optimized**: Support for Android (Vulkan/Posix) and iPhone (Metal/Darwin).
- ✅ **Cross-Platform**: Windows (.dll), Linux (.so), macOS (.dylib), and Android (.so).

## 📥 Get the Libraries
PAF is automatically built for all platforms via GitHub Actions.
1. Go to the [Actions tab](https://github.com/mati0516/paf-archive/actions) in this repository.
2. Click on the latest successful build.
3. Download the `libpaf-artifacts` for your platform.

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
