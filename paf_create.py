
import sys
import os
import struct

def collect_files(paths):
    """指定されたファイル・フォルダからファイル一覧を取得（.pafは除外）"""
    collected = []
    for path in paths:
        if os.path.isfile(path):
            if path.lower().endswith('.paf'):
                print(f"⛔ Skipping .paf file itself: {path}")
                continue
            collected.append((path, os.path.basename(path)))
        elif os.path.isdir(path):
            for root, _, files in os.walk(path):
                for name in files:
                    if name.lower().endswith('.paf'):
                        print(f"⛔ Skipping .paf file: {name}")
                        continue
                    full_path = os.path.join(root, name)
                    rel_path = os.path.relpath(full_path, start=path)
                    archive_path = os.path.join(os.path.basename(path), rel_path)
                    collected.append((full_path, archive_path))
        else:
            print(f"⚠️ Skipping invalid path: {path}")
    return collected

def main():
    if len(sys.argv) < 3:
        print("Usage: python paf_create.py output.paf file_or_folder1 [file_or_folder2 ...]")
        return

    out_path = sys.argv[1]
    if not out_path.lower().endswith(".paf"):
        out_path += ".paf"

    input_paths = sys.argv[2:]
    files = collect_files(input_paths)

    index_entries = []
    data_blocks = b''
    current_offset = 0

    for full_path, archive_path in files:
        with open(full_path, 'rb') as f:
            data = f.read()

        filename = archive_path.replace("\\", "/").encode('utf-8')
        filename_len = len(filename)
        file_size = len(data)

        index_entry = struct.pack(
            f'<H{filename_len}sII',
            filename_len,
            filename,
            file_size,
            current_offset
        )
        index_entries.append(index_entry)

        data_blocks += data
        current_offset += file_size

    index_offset = 32 + len(data_blocks)

    header = b'PAF\x01'
    header += struct.pack('<I', len(index_entries))
    header += struct.pack('<I', index_offset)
    header += b'\x00' * 20

    with open(out_path, 'wb') as out:
        out.write(header)
        out.write(data_blocks)
        for entry in index_entries:
            out.write(entry)

    print(f"✅ Created {out_path} with {len(index_entries)} file(s).")

if __name__ == "__main__":
    main()
