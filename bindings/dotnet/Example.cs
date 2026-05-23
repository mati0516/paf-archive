// Example.cs — Usage examples for the PafArchive C# bindings.
//
// Build and run from the bindings/dotnet directory:
//
//   dotnet build
//   dotnet run
//
// The native libpaf.so (Linux) or libpaf.dll (Windows) must be in the same
// directory as the built executable, or on the system library path.
// Build libpaf on Linux with:
//   gcc -O2 -shared -fPIC -Ilibpaf/include libpaf/src/*.c -lpthread -o libpaf.so

using System;
using System.IO;
using PafArchive;

namespace PafArchive
{
    public static class Example
    {
        // ---------------------------------------------------------------------
        // Helper: populate a small scratch directory for demos
        // ---------------------------------------------------------------------

        private static string MakeDemoDir(string root, string name)
        {
            string dir = Path.Combine(root, name);
            Directory.CreateDirectory(dir);

            File.WriteAllText(Path.Combine(dir, "hello.txt"), "Hello, PAF world!\n");
            File.WriteAllBytes(Path.Combine(dir, "data.bin"),
                System.Linq.Enumerable.Range(0, 256).Select(i => (byte)i).ToArray());

            string sub = Path.Combine(dir, "subdir");
            Directory.CreateDirectory(sub);
            File.WriteAllText(Path.Combine(sub, "nested.txt"), "Nested file content.\n");

            return dir;
        }

        // ---------------------------------------------------------------------
        // Demo 1 — Create / List / Extract
        // ---------------------------------------------------------------------

        private static void DemoCreateListExtract(string tmp)
        {
            Console.WriteLine("=== Demo 1: Create / List / Extract ===");

            string srcDir  = MakeDemoDir(tmp, "demo_src");
            string pafPath = Path.Combine(tmp, "archive.paf");
            string outDir  = Path.Combine(tmp, "extracted");

            // Create archive
            Console.WriteLine($"Creating {pafPath} from {srcDir} ...");
            Paf.Create(pafPath, srcDir);
            Console.WriteLine("  Created successfully.");

            // List contents
            Console.WriteLine("\nContents:");
            var entries = Paf.List(pafPath);
            foreach (var e in entries)
            {
                Console.WriteLine($"  {e.Path,-50}  size={e.Size,8}  sha256={e.HashHex[..16]}...");
            }

            // Extract
            Console.WriteLine($"\nExtracting to {outDir} ...");
            Paf.Extract(pafPath, outDir);
            Console.WriteLine("  Extracted successfully.");
            Console.WriteLine();
        }

        // ---------------------------------------------------------------------
        // Demo 2 — Delta
        // ---------------------------------------------------------------------

        private static void DemoDelta(string tmp)
        {
            Console.WriteLine("=== Demo 2: Delta ===");

            // v1 archive
            string srcV1 = MakeDemoDir(tmp, "demo_v1");
            string pafV1 = Path.Combine(tmp, "v1.paf");
            Paf.Create(pafV1, srcV1);

            // v2: modify one file, add another, drop data.bin → DELETED
            string srcV2 = Path.Combine(tmp, "demo_v2");
            Directory.CreateDirectory(srcV2);
            File.WriteAllText(Path.Combine(srcV2, "hello.txt"), "Hello, PAF world! (updated)\n");
            File.WriteAllText(Path.Combine(srcV2, "new_file.txt"), "Brand-new file.\n");

            string pafV2 = Path.Combine(tmp, "v2.paf");
            Paf.Create(pafV2, srcV2);

            // Compute and display delta
            Console.WriteLine($"Delta between {pafV1} and {pafV2}:");
            var changes = Paf.Delta(pafV1, pafV2);
            if (changes.Count == 0)
                Console.WriteLine("  (no changes)");
            foreach (var d in changes)
                Console.WriteLine($"  [{d.Status,-7}]  {d.Path}");

            Console.WriteLine();
        }

        // ---------------------------------------------------------------------
        // Demo 3 — Error handling
        // ---------------------------------------------------------------------

        private static void DemoErrorHandling()
        {
            Console.WriteLine("=== Demo 3: Error Handling ===");
            try
            {
                Paf.List("/nonexistent/path/archive.paf");
            }
            catch (PafException ex)
            {
                Console.WriteLine($"  Caught PafException (code={ex.Code}): {ex.Message}");
            }
            Console.WriteLine();
        }

        // ---------------------------------------------------------------------
        // Entry point
        // ---------------------------------------------------------------------

        public static void Main(string[] args)
        {
            string tmp = Path.Combine(Path.GetTempPath(), $"paf_demo_{Path.GetRandomFileName()}");
            Directory.CreateDirectory(tmp);
            try
            {
                DemoCreateListExtract(tmp);
                DemoDelta(tmp);
            }
            finally
            {
                Directory.Delete(tmp, recursive: true);
            }

            DemoErrorHandling();
            Console.WriteLine("All demos completed.");
        }
    }
}
