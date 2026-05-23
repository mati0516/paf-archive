// Paf.cs — C# P/Invoke bindings for libpaf (PAF archive format).
//
// Targets .NET 6+. Loads libpaf.dll on Windows or libpaf.so on Linux/macOS.
//
// Build with:
//   dotnet build
//
// Usage:
//   using PafArchive;
//   var entries = Paf.List("archive.paf");
//   Paf.Extract("archive.paf", "./output");
//   Paf.Create("archive.paf", "./mydir");
//   var delta = Paf.Delta("old.paf", "new.paf");

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace PafArchive
{
    // -------------------------------------------------------------------------
    // Exceptions
    // -------------------------------------------------------------------------

    /// <summary>Thrown when a libpaf function returns a non-zero error code.</summary>
    public class PafException : Exception
    {
        /// <summary>The raw error code returned by the native function.</summary>
        public int Code { get; }

        public PafException(string message, int code = 0) : base(message)
        {
            Code = code;
        }
    }

    // -------------------------------------------------------------------------
    // Public data types
    // -------------------------------------------------------------------------

    /// <summary>A single file entry read from a PAF archive.</summary>
    public sealed class PafEntry
    {
        /// <summary>UTF-8 file path stored in the archive.</summary>
        public string Path { get; }

        /// <summary>File size in bytes.</summary>
        public uint Size { get; }

        /// <summary>Byte offset of the file data within the PAF data block.</summary>
        public uint Offset { get; }

        /// <summary>Raw 32-byte SHA-256 hash.</summary>
        public byte[] Hash { get; }

        /// <summary>SHA-256 hash as a 64-character lowercase hex string.</summary>
        public string HashHex => Convert.ToHexString(Hash).ToLowerInvariant();

        internal PafEntry(string path, uint size, uint offset, byte[] hash)
        {
            Path   = path;
            Size   = size;
            Offset = offset;
            Hash   = hash;
        }

        public override string ToString() =>
            $"PafEntry {{ Path={Path}, Size={Size}, Hash={HashHex[..12]}... }}";
    }

    /// <summary>Possible change statuses returned by <see cref="Paf.Delta"/>.</summary>
    public enum DeltaStatus
    {
        Added   = 0,
        Updated = 1,
        Deleted = 2,
    }

    /// <summary>A single delta entry produced by <see cref="Paf.Delta"/>.</summary>
    public sealed class DeltaEntry
    {
        /// <summary>File path relative to the archive root.</summary>
        public string Path { get; }

        /// <summary>Whether the file was added, updated, or deleted.</summary>
        public DeltaStatus Status { get; }

        /// <summary>Byte offset of the file data in the new PAF (Added/Updated only).</summary>
        public ulong NewOffset { get; }

        /// <summary>File data size in bytes (Added/Updated only).</summary>
        public ulong DataSize { get; }

        /// <summary>Raw 32-byte SHA-256 hash.</summary>
        public byte[] Hash { get; }

        /// <summary>SHA-256 hash as a 64-character lowercase hex string.</summary>
        public string HashHex => Convert.ToHexString(Hash).ToLowerInvariant();

        internal DeltaEntry(string path, DeltaStatus status, ulong newOffset, ulong dataSize, byte[] hash)
        {
            Path      = path;
            Status    = status;
            NewOffset = newOffset;
            DataSize  = dataSize;
            Hash      = hash;
        }

        public override string ToString() => $"DeltaEntry {{ Status={Status}, Path={Path} }}";
    }

    // -------------------------------------------------------------------------
    // Native interop layer (private)
    // -------------------------------------------------------------------------

    internal static class NativeMethods
    {
        // Resolve the native library name at runtime so that the same binary
        // works on both Windows and Linux without recompilation.
        internal const string LibName =
#if _WINDOWS
            "libpaf.dll";
#else
            "libpaf";   // maps to libpaf.so on Linux, libpaf.dylib on macOS
#endif

        // PafEntry (C layout, 1064 bytes — no padding between fields)
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        internal unsafe struct CEntry
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1024)]
            public string Path;
            public uint Size;
            public uint Offset;
            public fixed byte Hash[32];
        }

        // PafList
        [StructLayout(LayoutKind.Sequential)]
        internal struct CList
        {
            public IntPtr Entries;   // CEntry*
            public uint   Count;
        }

        // paf_delta_entry_t
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        internal unsafe struct CDeltaEntry
        {
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1024)]
            public string Path;
            public int    Status;    // paf_delta_status_t (int-sized enum)
            public ulong  NewOffset;
            public ulong  DataSize;
            public fixed byte Hash[32];
        }

        // paf_delta_t
        [StructLayout(LayoutKind.Sequential)]
        internal struct CDelta
        {
            public IntPtr Entries;   // CDeltaEntry*
            public uint   Count;
        }

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int paf_list_binary(string pafPath, ref CList outList);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void free_paf_list(ref CList list);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int paf_extract_binary(string pafPath, string outputDir, int overwrite);

        // paf_create_binary takes const char** — we pass a flat array of UTF-8 pointers.
        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int paf_create_binary(
            string  outPafPath,
            IntPtr  inputPaths,   // const char**
            int     pathCount,
            string? ignoreFilePath,
            int     recursiveIgnore);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        internal static extern int paf_delta_calculate(string oldPaf, string newPaf, ref CDelta outDelta);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        internal static extern void paf_delta_free(ref CDelta delta);
    }

    // -------------------------------------------------------------------------
    // Public API
    // -------------------------------------------------------------------------

    /// <summary>
    /// Static facade for the libpaf native library.
    /// All methods are thread-safe with respect to independent archives.
    /// </summary>
    public static class Paf
    {
        // Configure the DLL search path once via NativeLibrary on .NET 6+.
        // The runtime also searches next to the executable and the PATH/LD_LIBRARY_PATH.

        /// <summary>
        /// Lists all file entries stored in the given PAF archive.
        /// </summary>
        /// <param name="pafPath">Path to the <c>.paf</c> file.</param>
        /// <returns>Ordered list of <see cref="PafEntry"/> objects.</returns>
        /// <exception cref="PafException">Thrown if the native call fails.</exception>
        public static unsafe List<PafEntry> List(string pafPath)
        {
            var cList = new NativeMethods.CList();
            int rc = NativeMethods.paf_list_binary(pafPath, ref cList);
            if (rc != 0)
                throw new PafException($"paf_list_binary failed (code {rc})", rc);

            var result = new List<PafEntry>((int)cList.Count);
            try
            {
                int entrySize = Marshal.SizeOf<NativeMethods.CEntry>();
                for (uint i = 0; i < cList.Count; i++)
                {
                    IntPtr ptr = cList.Entries + (int)(i * (uint)entrySize);
                    var ce = Marshal.PtrToStructure<NativeMethods.CEntry>(ptr);

                    // Copy the fixed-size hash array
                    var hash = new byte[32];
                    NativeMethods.CEntry* pCe = (NativeMethods.CEntry*)ptr;
                    for (int h = 0; h < 32; h++)
                        hash[h] = pCe->Hash[h];

                    result.Add(new PafEntry(ce.Path ?? string.Empty, ce.Size, ce.Offset, hash));
                }
            }
            finally
            {
                NativeMethods.free_paf_list(ref cList);
            }

            return result;
        }

        /// <summary>
        /// Extracts all files from the PAF archive into the given directory.
        /// </summary>
        /// <param name="pafPath">Path to the <c>.paf</c> file.</param>
        /// <param name="outputDir">Destination directory (created if absent).</param>
        /// <param name="overwrite">
        ///   When <see langword="true"/> (default), existing files are overwritten.
        /// </param>
        /// <exception cref="PafException">Thrown if the native call fails.</exception>
        public static void Extract(string pafPath, string outputDir, bool overwrite = true)
        {
            int rc = NativeMethods.paf_extract_binary(pafPath, outputDir, overwrite ? 1 : 0);
            if (rc != 0)
                throw new PafException($"paf_extract_binary failed (code {rc})", rc);
        }

        /// <summary>
        /// Creates a PAF archive from one or more files or directories.
        /// </summary>
        /// <param name="outPafPath">Destination <c>.paf</c> path.</param>
        /// <param name="inputPaths">One or more source files or directories.</param>
        /// <param name="ignoreFilePath">
        ///   Optional path to a <c>.pafignore</c> file; pass <see langword="null"/> to skip.
        /// </param>
        /// <param name="recursiveIgnore">
        ///   Whether the ignore file is applied recursively (default: <see langword="true"/>).
        /// </param>
        /// <exception cref="ArgumentException">Thrown when no input paths are supplied.</exception>
        /// <exception cref="PafException">Thrown if the native call fails.</exception>
        public static void Create(
            string   outPafPath,
            string[] inputPaths,
            string?  ignoreFilePath  = null,
            bool     recursiveIgnore = true)
        {
            if (inputPaths == null || inputPaths.Length == 0)
                throw new ArgumentException("At least one input path must be provided.", nameof(inputPaths));

            // Build a native char** (array of UTF-8 C-string pointers)
            var ptrs = new IntPtr[inputPaths.Length];
            try
            {
                for (int i = 0; i < inputPaths.Length; i++)
                    ptrs[i] = Marshal.StringToHGlobalAnsi(inputPaths[i]);

                // Pin the pointer array so we can pass it as const char**
                var handle = GCHandle.Alloc(ptrs, GCHandleType.Pinned);
                try
                {
                    int rc = NativeMethods.paf_create_binary(
                        outPafPath,
                        handle.AddrOfPinnedObject(),
                        inputPaths.Length,
                        ignoreFilePath,
                        recursiveIgnore ? 1 : 0);

                    if (rc != 0)
                        throw new PafException($"paf_create_binary failed (code {rc})", rc);
                }
                finally
                {
                    handle.Free();
                }
            }
            finally
            {
                foreach (IntPtr p in ptrs)
                    if (p != IntPtr.Zero) Marshal.FreeHGlobal(p);
            }
        }

        /// <summary>
        /// Convenience overload: create an archive from a single path.
        /// </summary>
        public static void Create(string outPafPath, string inputPath,
                                   string? ignoreFilePath = null, bool recursiveIgnore = true)
            => Create(outPafPath, new[] { inputPath }, ignoreFilePath, recursiveIgnore);

        /// <summary>
        /// Calculates the delta between two PAF archives.
        /// </summary>
        /// <param name="oldPaf">Path to the older <c>.paf</c> file.</param>
        /// <param name="newPaf">Path to the newer <c>.paf</c> file.</param>
        /// <returns>
        ///   A list of <see cref="DeltaEntry"/> objects, each indicating whether a file
        ///   was <see cref="DeltaStatus.Added"/>, <see cref="DeltaStatus.Updated"/>, or
        ///   <see cref="DeltaStatus.Deleted"/>.
        /// </returns>
        /// <exception cref="PafException">Thrown if the native call fails.</exception>
        public static unsafe List<DeltaEntry> Delta(string oldPaf, string newPaf)
        {
            var cDelta = new NativeMethods.CDelta();
            int rc = NativeMethods.paf_delta_calculate(oldPaf, newPaf, ref cDelta);
            if (rc != 0)
                throw new PafException($"paf_delta_calculate failed (code {rc})", rc);

            var result = new List<DeltaEntry>((int)cDelta.Count);
            try
            {
                int entrySize = Marshal.SizeOf<NativeMethods.CDeltaEntry>();
                for (uint i = 0; i < cDelta.Count; i++)
                {
                    IntPtr ptr = cDelta.Entries + (int)(i * (uint)entrySize);
                    var ce = Marshal.PtrToStructure<NativeMethods.CDeltaEntry>(ptr);

                    var hash = new byte[32];
                    NativeMethods.CDeltaEntry* pCe = (NativeMethods.CDeltaEntry*)ptr;
                    for (int h = 0; h < 32; h++)
                        hash[h] = pCe->Hash[h];

                    var status = ce.Status switch
                    {
                        0 => DeltaStatus.Added,
                        1 => DeltaStatus.Updated,
                        2 => DeltaStatus.Deleted,
                        _ => throw new PafException($"Unknown delta status {ce.Status}"),
                    };

                    result.Add(new DeltaEntry(
                        ce.Path ?? string.Empty,
                        status,
                        ce.NewOffset,
                        ce.DataSize,
                        hash));
                }
            }
            finally
            {
                NativeMethods.paf_delta_free(ref cDelta);
            }

            return result;
        }
    }
}
