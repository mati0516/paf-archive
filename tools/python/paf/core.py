
import argparse
import struct
import os
import sys
import shutil

PAF_MAGIC = b'PAF0'
HEADER_SIZE = 32

def create_paf(output_file, input_paths):
    files = []
    for path in input_paths:
        if os.path.isdir(path):
            for root, _, filenames in os.walk(path):
                for name in filenames:
                    full_path = os.path.join(root, name)
                    rel_path = os.path.relpath(full_path, start=path)
                    files.append((full_path, rel_path))
        elif os.path.isfile(path):
            name = os.path.basename(path)
            files.append((path, name))

    with open(output_file, 'wb') as out:
        out.write(PAF_MAGIC + b'\x00' * (HEADER_SIZE - 4))
        data_offset = HEADER_SIZE
        index_entries = b""

        for full_path, rel_path in files:
            with open(full_path, 'rb') as f:
                data = f.read()
            out.write(data)
            rel_path_bytes = rel_path.encode('utf-8')
            index_entries += struct.pack('<H', len(rel_path_bytes)) + rel_path_bytes
            index_entries += struct.pack('<II', len(data), data_offset)
            data_offset += len(data)

        index_offset = data_offset
        out.write(index_entries)

        out.seek(0)
        out.write(PAF_MAGIC + struct.pack('<II', len(files), index_offset) + b'\x00' * (HEADER_SIZE - 12))

def extract_paf(paf_file, output_dir):
    with open(paf_file, 'rb') as f:
        header = f.read(HEADER_SIZE)
        if header[:4] != PAF_MAGIC:
            raise ValueError("Invalid PAF file")
        file_count, index_offset = struct.unpack('<II', header[4:12])
        f.seek(HEADER_SIZE)
        data = f.read(index_offset - HEADER_SIZE)
        f.seek(index_offset)
        index_data = f.read()

        index_ptr = 0
        for _ in range(file_count):
            name_len = struct.unpack('<H', index_data[index_ptr:index_ptr+2])[0]
            index_ptr += 2
            name = index_data[index_ptr:index_ptr+name_len].decode('utf-8')
            index_ptr += name_len
            size, offset = struct.unpack('<II', index_data[index_ptr:index_ptr+8])
            index_ptr += 8

            file_data = data[offset - HEADER_SIZE:offset - HEADER_SIZE + size]
            output_path = os.path.join(output_dir, name)
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            with open(output_path, 'wb') as out_file:
                out_file.write(file_data)

def list_paf(paf_file):
    with open(paf_file, 'rb') as f:
        header = f.read(HEADER_SIZE)
        if header[:4] != PAF_MAGIC:
            raise ValueError("Invalid PAF file")
        file_count, index_offset = struct.unpack('<II', header[4:12])
        f.seek(index_offset)
        index_data = f.read()

        index_ptr = 0
        for _ in range(file_count):
            name_len = struct.unpack('<H', index_data[index_ptr:index_ptr+2])[0]
            index_ptr += 2
            name = index_data[index_ptr:index_ptr+name_len].decode('utf-8')
            index_ptr += name_len
            size, offset = struct.unpack('<II', index_data[index_ptr:index_ptr+8])
            index_ptr += 8
            print(f"{name} (offset: {offset}, size: {size})")

def to_iso(paf_file, output_iso_dir):
    temp_dir = "__paf_temp_extract__"
    extract_paf(paf_file, temp_dir)
    if os.path.exists(output_iso_dir):
        shutil.rmtree(output_iso_dir)
    shutil.move(temp_dir, output_iso_dir)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PAF archive tool")
    subparsers = parser.add_subparsers(dest="command")

    parser_create = subparsers.add_parser("create", help="Create a .paf file")
    parser_create.add_argument("output", help="Output PAF file")
    parser_create.add_argument("inputs", nargs="+", help="Input files or folders")

    parser_extract = subparsers.add_parser("extract", help="Extract a .paf file")
    parser_extract.add_argument("paf_file", help="Input PAF file")
    parser_extract.add_argument("output_dir", help="Output directory")

    parser_ls = subparsers.add_parser("ls", help="List contents of a .paf file")
    parser_ls.add_argument("paf_file", help="Input PAF file")

    parser_iso = subparsers.add_parser("to-iso", help="Convert .paf to folder (for ISO tools)")
    parser_iso.add_argument("paf_file", help="Input PAF file")
    parser_iso.add_argument("output_dir", help="Output folder")

    args = parser.parse_args()

    if args.command == "create":
        create_paf(args.output, args.inputs)
    elif args.command == "extract":
        extract_paf(args.paf_file, args.output_dir)
    elif args.command == "ls":
        list_paf(args.paf_file)
    elif args.command == "to-iso":
        to_iso(args.paf_file, args.output_dir)
    else:
        parser.print_help()
