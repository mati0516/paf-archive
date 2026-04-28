# 📦 Parallel Archive Format (PAF) v1.0.0 - Official Release

The ultimate high-speed, non-compressed archive format optimized for **GPU acceleration**, **Parallel I/O**, and **100M+ file datasets**.
*Status: Official Release (v1.0.0) with Full GPU & Mobile Support.*

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
- ✅ **Mobile Optimized**: Full support for Android (NDK/arm64) and iOS (Xcode/arm64).
- ✅ **Cross-Platform**: Windows (.dll), Linux (.so), macOS (.dylib), Android (.so), and iOS (.dylib).

## 📥 Get the Libraries
PAF is automatically built for all platforms (Windows, Linux, macOS, Android, iOS) via GitHub Actions.
1. Go to the [Actions tab](https://github.com/mati0516/paf-archive/actions) in this repository.
2. Click on the latest successful build.
3. Download the artifacts for your platform:
   - `libpaf-windows-latest`
   - `libpaf-ubuntu-latest`
   - `libpaf-macos-latest`
   - `libpaf-android`
   - `libpaf-ios`

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

## ⚙️ Multi-Platform Integration Guide

### 💻 Desktop (Windows, Linux, macOS)
Download the shared library (`.dll`, `.so`, or `.dylib`) and include `paf.h`.

**Compile Command:**
```bash
# Example for Linux
gcc main.c -L. -lpaf -Ilibpaf -o my_app
```

### 🤖 Android (NDK)
1. Copy `libpaf_android_arm64.so` to your project's `src/main/jniLibs/arm64-v8a/` directory.
2. In your `CMakeLists.txt`:
```cmake
add_library(libpaf SHARED IMPORTED)
set_target_properties(libpaf PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libpaf.so)
target_link_libraries(my_jni_target libpaf)
```

### 🍎 iOS (Xcode)
1. Download `libpaf_ios.dylib`.
2. Drag and drop the library into your Xcode project.
3. Under **General > Frameworks, Libraries, and Embedded Content**, ensure it is set to "Embed & Sign".
4. Add `libpaf/` to your **Header Search Paths**.

## 🧪 Development & Build
If you want to build the source yourself:
```bash
# Desktop
gcc -O3 -shared -fPIC -Ilibpaf libpaf/*.c -o libpaf.so

# GPU Engine (Requires CUDA)
nvcc -O3 --shared libpaf/paf_cuda_kernels.cu -o libpaf_cuda.so
```

## 📊 Performance Comparison (AI-Calculated Theoretical Values / AIによる理論値)
*Scenario: 100,000 Files (10KB each, Total 1GB) on High-end NVMe (7000MB/s) + Modern GPU.*

| Format | Operation | Est. Time | Bottleneck |
| :--- | :--- | :--- | :--- |
| **ZIP (Deflate)** | Creation | 90 - 150 sec | CPU (Compression Logic) |
| **TAR** | Creation | 15 - 25 sec | Disk IO (Sequential Seek) |
| **PAF v1 (CPU)** | Creation | 3 - 5 sec | Disk IO (Sequential Write) |
| **PAF v1 (GPU+DS)** | **Creation** | **< 0.8 sec** | **None (Physical Limit)** |

### 🚀 Why is PAF so fast?
- **ZIP/TAR**: Processing 100,000 files one by one causes massive OS context switching.
- **PAF v1**: Treats 100,000 files as a single continuous stream. By offloading SHA-256 to **thousands of GPU cores** and using **DirectStorage** to bypass the CPU, PAF hits the physical speed limit of your hardware.

---

## 📜 License
MIT License
