
import sys
import os
import struct

def extract_paf(paf_path, output_dir):
    with open(paf_path, 'rb') as f:
        header = f.read(32)
        if header[0:4] != b'PAF\x01':
            print("❌ Not a valid PAF file.")
            return

        file_count = struct.unpack('<I', header[4:8])[0]
        index_offset = struct.unpack('<I', header[8:12])[0]

        f.seek(index_offset)
        index_entries = []
        for _ in range(file_count):
            name_len = struct.unpack('<H', f.read(2))[0]
            name = f.read(name_len).decode('utf-8')
            size = struct.unpack('<I', f.read(4))[0]
            offset = struct.unpack('<I', f.read(4))[0]
            index_entries.append((name, size, offset))

        for name, size, offset in index_entries:
            out_path = os.path.join(output_dir, name)
            os.makedirs(os.path.dirname(out_path), exist_ok=True)
            f.seek(32 + offset)
            with open(out_path, 'wb') as out_file:
                out_file.write(f.read(size))
            print(f"✅ Extracted: {name} ({size} bytes)")

def main():
    if len(sys.argv) != 3:
        print("Usage: python paf_extract.py archive.paf output_folder")
        return

    paf_path = sys.argv[1]
    output_dir = sys.argv[2]
    extract_paf(paf_path, output_dir)

if __name__ == "__main__":
    main()
