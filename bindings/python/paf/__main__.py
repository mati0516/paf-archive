# tools/python/paf/__main__.py

from .core import create_paf, extract_paf, convert_to_iso
import sys

if __name__ == "__main__":
    # 例: python -m paf create out.paf dir1 dir2
    cmd = sys.argv[1]
    if cmd == "create":
        out = sys.argv[2]
        paths = sys.argv[3:]
        create_paf(out, paths)
    elif cmd == "extract":
        paf = sys.argv[2]
        dest = sys.argv[3]
        extract_paf(paf, dest)
    elif cmd == "to_iso":
        paf = sys.argv[2]
        iso = sys.argv[3]
        convert_to_iso(paf, iso)
