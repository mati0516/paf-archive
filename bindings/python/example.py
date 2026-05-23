"""
example.py — Usage examples for the paf Python bindings.

Run from the repo root after building libpaf.so:

    gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -lpthread -o libpaf.so
    python bindings/python/example.py
"""

import os
import sys
import tempfile

# Allow running from any directory: add the bindings/python folder to sys.path
sys.path.insert(0, os.path.dirname(__file__))

import paf


# ---------------------------------------------------------------------------
# Helper: create a small scratch directory for the demo
# ---------------------------------------------------------------------------

def _make_demo_dir(tmp: str) -> str:
    src = os.path.join(tmp, "demo_src")
    os.makedirs(src, exist_ok=True)
    with open(os.path.join(src, "hello.txt"), "w") as f:
        f.write("Hello, PAF world!\n")
    with open(os.path.join(src, "data.bin"), "wb") as f:
        f.write(bytes(range(256)))
    sub = os.path.join(src, "subdir")
    os.makedirs(sub, exist_ok=True)
    with open(os.path.join(sub, "nested.txt"), "w") as f:
        f.write("Nested file content.\n")
    return src


# ---------------------------------------------------------------------------
# Demo 1 — paf.create / paf.list / paf.extract
# ---------------------------------------------------------------------------

def demo_create_list_extract(tmp: str) -> None:
    print("=== Demo 1: create / list / extract ===")

    src_dir  = _make_demo_dir(tmp)
    paf_path = os.path.join(tmp, "archive.paf")
    out_dir  = os.path.join(tmp, "extracted")

    # Create archive
    print(f"Creating {paf_path} from {src_dir} ...")
    paf.create(paf_path, src_dir)
    print("  Created successfully.")

    # List contents
    print("\nContents:")
    entries = paf.list(paf_path)
    for e in entries:
        print(f"  {e.path:50s}  size={e.size:8d}  sha256={e.hash_hex[:16]}...")

    # Extract
    print(f"\nExtracting to {out_dir} ...")
    paf.extract(paf_path, out_dir)
    print("  Extracted successfully.")
    print()


# ---------------------------------------------------------------------------
# Demo 2 — paf.delta
# ---------------------------------------------------------------------------

def demo_delta(tmp: str) -> None:
    print("=== Demo 2: delta ===")

    # Create v1 archive
    src_v1  = _make_demo_dir(tmp)
    paf_v1  = os.path.join(tmp, "v1.paf")
    paf.create(paf_v1, src_v1)

    # Modify a file and add a new one to create v2
    src_v2 = os.path.join(tmp, "demo_v2")
    os.makedirs(src_v2, exist_ok=True)
    with open(os.path.join(src_v2, "hello.txt"), "w") as f:
        f.write("Hello, PAF world! (updated)\n")
    with open(os.path.join(src_v2, "new_file.txt"), "w") as f:
        f.write("Brand-new file.\n")
    # data.bin is intentionally absent → DELETED

    paf_v2 = os.path.join(tmp, "v2.paf")
    paf.create(paf_v2, src_v2)

    # Compute delta
    print(f"Delta between {paf_v1} and {paf_v2}:")
    changes = paf.delta(paf_v1, paf_v2)
    if not changes:
        print("  (no changes)")
    for d in changes:
        print(f"  [{d.status:7s}]  {d.path}")
    print()


# ---------------------------------------------------------------------------
# Demo 3 — error handling
# ---------------------------------------------------------------------------

def demo_error_handling() -> None:
    print("=== Demo 3: error handling ===")
    try:
        paf.list("/nonexistent/path/archive.paf")
    except paf.PafError as exc:
        print(f"  Caught PafError (code={exc.code}): {exc}")
    print()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    with tempfile.TemporaryDirectory(prefix="paf_demo_") as tmp:
        demo_create_list_extract(tmp)
        demo_delta(tmp)

    demo_error_handling()
    print("All demos completed.")


if __name__ == "__main__":
    main()
