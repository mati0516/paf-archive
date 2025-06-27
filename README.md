# PAF - Parallel Archive Format

**PAF (Parallel Archive Format)** is a super-fast, non-compressed archive format  
designed for modern workflows and parallel processing environments.  
It bundles multiple files into a single `.paf` file without compression,  
enabling instant access and ultra-fast write/read speed.

---

## ✨ Features

- 🗂️ Single-file archive with no compression
- 🚀 Super fast read/write performance
- 📦 Perfect for bundling files from parallel jobs
- 🔍 Files are accessible without extraction
- 🔧 Simple binary format, easy to parse and extend
- 🔒 Safe to scan (antivirus-friendly, no hidden containers)

---

## 📦 Usage (CLI)

### Create a PAF file:

```bash
python paf_create.py output.paf file1.txt file2.txt file3.bin
```

### Coming Soon:
- `paf_ls.py` – List files inside a `.paf`
- `paf_extract.py` – Extract specific files

---

## 📄 Specification

See [`paf_spec.md`](paf_spec.md) for full format details.

---

## 📂 Why PAF?

Traditional archives like ZIP or TAR are designed for compression and slow media.  
PAF is designed for **speed and simplicity**, and lets you:

- Avoid annoying extraction steps
- Handle files directly without temp folders
- Transfer or store job output as a single, clean binary file

---

## 🧪 Status

**In active development** – public release planned soon.  
Contributions and feedback welcome!

---

## 📝 License

This project is licensed under the MIT License.
