# tools/python/paf/core.py

import ctypes
import os
import platform

# OSに応じて.so/.dll/.dylibを指定
if platform.system() == "Windows":
    lib = ctypes.CDLL("libpaf.dll")
elif platform.system() == "Darwin":
    lib = ctypes.CDLL("libpaf.dylib")
else:
    lib = ctypes.CDLL("libpaf.so")

# 関数プロトタイプの指定（戻り値と引数型）
lib.paf_create_binary.argtypes = [
    ctypes.c_char_p,                    # out_paf_path
    ctypes.POINTER(ctypes.c_char_p),   # input_paths
    ctypes.c_int,                      # path_count
    ctypes.c_char_p,                   # ignore_file_path
    ctypes.c_int                       # recursive_ignore
]
lib.paf_create_binary.restype = ctypes.c_int

lib.paf_extract_binary.argtypes = [
    ctypes.c_char_p,  # paf_path
    ctypes.c_char_p,  # output_dir
    ctypes.c_int      # overwrite
]
lib.paf_extract_binary.restype = ctypes.c_int

lib.paf_to_iso.argtypes = [
    ctypes.c_char_p,  # paf_path
    ctypes.c_char_p   # out_iso_path
]
lib.paf_to_iso.restype = ctypes.c_int

# Python ラッパー関数
def create_paf(output_path, input_paths, ignore_file=None, recursive=False):
    arr = (ctypes.c_char_p * len(input_paths))()
    arr[:] = [p.encode() for p in input_paths]
    return lib.paf_create_binary(
        output_path.encode(),
        arr,
        len(input_paths),
        ignore_file.encode() if ignore_file else None,
        int(recursive)
    )

def extract_paf(paf_path, output_dir, overwrite=True):
    return lib.paf_extract_binary(
        paf_path.encode(),
        output_dir.encode(),
        int(overwrite)
    )

def convert_to_iso(paf_path, iso_path):
    return lib.paf_to_iso(
        paf_path.encode(),
        iso_path.encode()
    )
