"""
paf.py — Python ctypes bindings for libpaf (PAF archive format).

Supports Python 3.8+.
Searches for libpaf.so (Linux) or libpaf.dll (Windows) relative to this
file, then falls back to the working directory and system library paths.
"""

from __future__ import annotations

import ctypes
import os
import sys
from ctypes import c_char, c_char_p, c_int, c_uint8, c_uint32, c_uint64, c_void_p, POINTER
from pathlib import Path
from typing import Iterator, List, Optional

# ---------------------------------------------------------------------------
# Library loading
# ---------------------------------------------------------------------------

def _find_library() -> ctypes.CDLL:
    """Locate and load libpaf shared library."""
    lib_name = "libpaf.dll" if sys.platform == "win32" else "libpaf.so"

    # Search order: next to this file → cwd → system
    candidates = [
        Path(__file__).parent / lib_name,
        Path(__file__).parent.parent.parent / lib_name,  # repo root
        Path.cwd() / lib_name,
        Path(lib_name),
    ]

    for path in candidates:
        if path.exists():
            return ctypes.CDLL(str(path))

    # Fall back to ctypes.util / system loader
    try:
        return ctypes.CDLL(lib_name)
    except OSError:
        raise PafError(
            f"Cannot find {lib_name}. Build the library first and place it "
            "next to paf.py or in the working directory."
        )


# ---------------------------------------------------------------------------
# Exceptions
# ---------------------------------------------------------------------------

class PafError(Exception):
    """Raised when a libpaf function returns a non-zero error code."""

    def __init__(self, message: str, code: int = 0):
        super().__init__(message)
        self.code = code


# ---------------------------------------------------------------------------
# C structure definitions
# ---------------------------------------------------------------------------

class _CEntry(ctypes.Structure):
    """Maps to PafEntry in libpaf.h (1064 bytes)."""
    _fields_ = [
        ("path",   c_char * 1024),
        ("size",   c_uint32),
        ("offset", c_uint32),
        ("hash",   c_uint8 * 32),
    ]


class _CList(ctypes.Structure):
    """Maps to PafList in libpaf.h."""
    _fields_ = [
        ("entries", POINTER(_CEntry)),
        ("count",   c_uint32),
    ]


# paf_delta_status_t enum values
_DELTA_ADDED   = 0
_DELTA_UPDATED = 1
_DELTA_DELETED = 2

_DELTA_STATUS_NAMES = {
    _DELTA_ADDED:   "ADDED",
    _DELTA_UPDATED: "UPDATED",
    _DELTA_DELETED: "DELETED",
}


class _CDeltaEntry(ctypes.Structure):
    """Maps to paf_delta_entry_t in paf_delta.h."""
    _fields_ = [
        ("path",       c_char * 1024),
        ("status",     c_int),         # paf_delta_status_t (int-sized enum)
        ("new_offset", c_uint64),
        ("data_size",  c_uint64),
        ("hash",       c_uint8 * 32),
    ]


class _CDelta(ctypes.Structure):
    """Maps to paf_delta_t in paf_delta.h."""
    _fields_ = [
        ("entries", POINTER(_CDeltaEntry)),
        ("count",   c_uint32),
    ]


# ---------------------------------------------------------------------------
# Public Python-side data classes
# ---------------------------------------------------------------------------

class PafEntry:
    """A single file entry read from a PAF archive."""

    __slots__ = ("path", "size", "offset", "_hash_bytes")

    def __init__(self, path: str, size: int, offset: int, hash_bytes: bytes):
        self.path: str = path
        self.size: int = size
        self.offset: int = offset
        self._hash_bytes: bytes = hash_bytes

    @property
    def hash_hex(self) -> str:
        """SHA-256 hash as a 64-character lowercase hex string."""
        return self._hash_bytes.hex()

    def __repr__(self) -> str:
        return f"PafEntry(path={self.path!r}, size={self.size}, hash={self.hash_hex[:12]}...)"


class DeltaEntry:
    """A single delta entry produced by :func:`delta`."""

    __slots__ = ("path", "status", "new_offset", "data_size", "_hash_bytes")

    def __init__(
        self,
        path: str,
        status: str,
        new_offset: int,
        data_size: int,
        hash_bytes: bytes,
    ):
        self.path: str = path
        self.status: str = status          # "ADDED", "UPDATED", or "DELETED"
        self.new_offset: int = new_offset
        self.data_size: int = data_size
        self._hash_bytes: bytes = hash_bytes

    @property
    def hash_hex(self) -> str:
        """SHA-256 hash as a 64-character lowercase hex string."""
        return self._hash_bytes.hex()

    def __repr__(self) -> str:
        return f"DeltaEntry(status={self.status!r}, path={self.path!r})"


# ---------------------------------------------------------------------------
# Internal: lazy-loaded library and prototype setup
# ---------------------------------------------------------------------------

_lib: Optional[ctypes.CDLL] = None


def _get_lib() -> ctypes.CDLL:
    global _lib
    if _lib is None:
        _lib = _find_library()
        _setup_prototypes(_lib)
    return _lib


def _setup_prototypes(lib: ctypes.CDLL) -> None:
    """Configure argtypes/restype for all used functions."""
    lib.paf_list_binary.argtypes = [c_char_p, POINTER(_CList)]
    lib.paf_list_binary.restype  = c_int

    lib.free_paf_list.argtypes = [POINTER(_CList)]
    lib.free_paf_list.restype  = None

    lib.paf_extract_binary.argtypes = [c_char_p, c_char_p, c_int]
    lib.paf_extract_binary.restype  = c_int

    lib.paf_create_binary.argtypes = [c_char_p, POINTER(c_char_p), c_int, c_char_p, c_int]
    lib.paf_create_binary.restype  = c_int

    lib.paf_delta_calculate.argtypes = [c_char_p, c_char_p, POINTER(_CDelta)]
    lib.paf_delta_calculate.restype  = c_int

    lib.paf_delta_free.argtypes = [POINTER(_CDelta)]
    lib.paf_delta_free.restype  = None


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def list(paf_path: str) -> List[PafEntry]:  # noqa: A001  (shadows builtin intentionally)
    """Return all file entries from *paf_path*.

    Parameters
    ----------
    paf_path:
        Path to the ``.paf`` archive.

    Returns
    -------
    list[PafEntry]
        Entries in the order stored in the archive.

    Raises
    ------
    PafError
        If the library returns a non-zero error code.
    """
    lib = _get_lib()
    c_list = _CList()
    rc = lib.paf_list_binary(paf_path.encode(), ctypes.byref(c_list))
    if rc != 0:
        raise PafError(f"paf_list_binary failed (code {rc})", rc)

    result: List[PafEntry] = []
    try:
        for i in range(c_list.count):
            e = c_list.entries[i]
            result.append(PafEntry(
                path=e.path.decode("utf-8", errors="replace"),
                size=e.size,
                offset=e.offset,
                hash_bytes=bytes(e.hash),
            ))
    finally:
        lib.free_paf_list(ctypes.byref(c_list))

    return result


def extract(paf_path: str, output_dir: str, overwrite: bool = True) -> None:
    """Extract all files from *paf_path* into *output_dir*.

    Parameters
    ----------
    paf_path:
        Path to the ``.paf`` archive.
    output_dir:
        Destination directory (created if it does not exist).
    overwrite:
        When ``True`` (default), existing files are overwritten.

    Raises
    ------
    PafError
        If the library returns a non-zero error code.
    """
    lib = _get_lib()
    rc = lib.paf_extract_binary(
        paf_path.encode(),
        output_dir.encode(),
        c_int(1 if overwrite else 0),
    )
    if rc != 0:
        raise PafError(f"paf_extract_binary failed (code {rc})", rc)


def create(
    out_paf_path: str,
    *input_paths: str,
    ignore_file: Optional[str] = None,
    recursive_ignore: bool = True,
) -> None:
    """Create a PAF archive at *out_paf_path* from one or more files/directories.

    Parameters
    ----------
    out_paf_path:
        Destination ``.paf`` path.
    *input_paths:
        One or more source file or directory paths.
    ignore_file:
        Optional path to a ``.pafignore`` file.
    recursive_ignore:
        Whether to apply the ignore file recursively (default: ``True``).

    Raises
    ------
    PafError
        If the library returns a non-zero error code or no paths are given.

    Examples
    --------
    >>> paf.create("archive.paf", "./mydir")
    >>> paf.create("archive.paf", "file1.txt", "file2.txt")
    """
    if not input_paths:
        raise PafError("At least one input path must be provided.")

    lib = _get_lib()

    # Build a C array of char* pointers
    PathArray = c_char_p * len(input_paths)
    c_paths = PathArray(*(p.encode() for p in input_paths))

    c_ignore = ignore_file.encode() if ignore_file else None
    rc = lib.paf_create_binary(
        out_paf_path.encode(),
        c_paths,
        c_int(len(input_paths)),
        c_ignore,
        c_int(1 if recursive_ignore else 0),
    )
    if rc != 0:
        raise PafError(f"paf_create_binary failed (code {rc})", rc)


def delta(old_paf: str, new_paf: str) -> List[DeltaEntry]:
    """Calculate the delta between two PAF archives.

    Parameters
    ----------
    old_paf:
        Path to the older ``.paf`` archive.
    new_paf:
        Path to the newer ``.paf`` archive.

    Returns
    -------
    list[DeltaEntry]
        Each entry has a ``status`` of ``"ADDED"``, ``"UPDATED"``, or
        ``"DELETED"``, plus ``path``, ``hash_hex``, ``new_offset``, and
        ``data_size``.

    Raises
    ------
    PafError
        If the library returns a non-zero error code.
    """
    lib = _get_lib()
    c_delta = _CDelta()
    rc = lib.paf_delta_calculate(
        old_paf.encode(),
        new_paf.encode(),
        ctypes.byref(c_delta),
    )
    if rc != 0:
        raise PafError(f"paf_delta_calculate failed (code {rc})", rc)

    result: List[DeltaEntry] = []
    try:
        for i in range(c_delta.count):
            e = c_delta.entries[i]
            status_str = _DELTA_STATUS_NAMES.get(e.status, f"UNKNOWN({e.status})")
            result.append(DeltaEntry(
                path=e.path.decode("utf-8", errors="replace"),
                status=status_str,
                new_offset=e.new_offset,
                data_size=e.data_size,
                hash_bytes=bytes(e.hash),
            ))
    finally:
        lib.paf_delta_free(ctypes.byref(c_delta))

    return result
