
# 📦 Parallel Archive Format (PAF)

A simple, fast, and parallel-friendly file archiving format designed for maximum I/O performance and minimal friction.

## 🚀 Features

- ✅ No compression (raw file copy)
- ✅ Fast creation using parallel file reads
- ✅ Instant listing and extraction
- ✅ Retains full folder hierarchy
- ✅ UTF-8 filename support (including Japanese)
- ✅ Simple format (header, data, index)
- ✅ MIT Licensed and cross-platform friendly

## 📦 Usage (CLI)

### Create a PAF file:

```bash
python paf_create.py output.paf file1.txt folder2 file3.bin
```

### List contents of a PAF file:

```bash
python paf_ls.py archive.paf
```

### Extract files from a PAF archive:

```bash
python paf_extract.py archive.paf output_folder
```

## 📄 Format Structure

- 32-byte header
- Concatenated file data
- File index (file count, names, sizes, offsets)

## 🛠 Tools

- `paf_create.py` – create `.paf` archives
- `paf_ls.py` – list contents of a `.paf`
- `paf_extract.py` – extract files from `.paf`

## 🧪 Status

All core tools are functional and tested.  
Supports nested folders, large files, Japanese paths.

## 📜 License

MIT License
