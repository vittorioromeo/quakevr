using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.SelfTest;

/// <summary>Made-up game data and packages: the smallest files the engine's checks accept. No id data.</summary>
static class Fixtures
{
    public static byte[] Mdl()
    {
        var b = new byte[84 + 4 + 1 + 12 + 16 + 4 + 24 + 4];
        "IDPO"u8.CopyTo(b);
        Int(b, 4, 6);
        Int(b, 48, 1); // skins
        Int(b, 52, 1); // width
        Int(b, 56, 1); // height
        Int(b, 60, 1); // vertices
        Int(b, 64, 1); // triangles
        Int(b, 68, 1); // frames
        return b;
    }

    public static byte[] Wav()
    {
        var b = new byte[46];
        "RIFF"u8.CopyTo(b);
        Int(b, 4, b.Length - 8);
        "WAVE"u8.CopyTo(b.AsSpan(8));
        "fmt "u8.CopyTo(b.AsSpan(12));
        Int(b, 16, 16);
        "data"u8.CopyTo(b.AsSpan(36));
        Int(b, 40, 2);
        return b;
    }

    public static byte[] Spr()
    {
        var b = new byte[36 + 4 + 16 + 1];
        "IDSP"u8.CopyTo(b);
        Int(b, 4, 1);
        Int(b, 24, 1); // frames
        Int(b, 40 + 8, 1); // width
        Int(b, 40 + 12, 1); // height
        return b;
    }

    public static byte[] Bsp()
    {
        var b = new byte[124 + 64];
        Int(b, 0, 29);
        Int(b, 116, 124); // models lump
        Int(b, 120, 64);
        return b;
    }

    public static byte[] For(string name) => Path.GetExtension(name) switch
    {
        ".mdl" => Mdl(),
        ".wav" => Wav(),
        ".spr" => Spr(),
        _ => Bsp(),
    };

    static void Int(byte[] b, int at, int v) => BinaryPrimitives.WriteInt32LittleEndian(b.AsSpan(at), v);

    /// <summary>A pack whose pak0 holds every required file (or all but <paramref name="leaveOut"/>).</summary>
    public static void MakePack(string root, string game, string? leaveOut = null)
    {
        var dir = Directory.CreateDirectory(Path.Combine(root, game)).FullName;
        var files = PackLists.Resources(game).Where(r => r != leaveOut).Select(r => (r, For(r))).ToList();
        PakFile.Write(Path.Combine(dir, "pak0.pak"), files);
    }

    /// <summary>An original Quake (pak0 + pak1, tiny) at <paramref name="root"/>.</summary>
    public static void MakeOriginal(string root)
    {
        var id1 = Directory.CreateDirectory(Path.Combine(root, "id1")).FullName;
        PakFile.Write(Path.Combine(id1, "pak0.pak"), [("maps/start.bsp", Bsp())]);
        PakFile.Write(Path.Combine(id1, "pak1.pak"), [("maps/e1m1.bsp", Bsp())]);
    }

    /// <summary>A rerelease layout at <paramref name="root"/> (QuakeEX.kpf beside id1, one pak0).</summary>
    public static void MakeRerelease(string root)
    {
        var id1 = Directory.CreateDirectory(Path.Combine(root, "id1")).FullName;
        PakFile.Write(Path.Combine(id1, "pak0.pak"), [("maps/start.bsp", Bsp())]);
        File.WriteAllText(Path.Combine(root, "QuakeEX.kpf"), "kex");
    }

    /// <summary>A small package like package-quakevr.ps1's: engine stand-ins, the game folder, the launcher.</summary>
    public static string MakePackage(string dir, string version, Dictionary<string, string>? overrideFiles = null, bool manifest = true)
    {
        Directory.CreateDirectory(dir);
        var files = new Dictionary<string, string>
        {
            ["ironwail.exe"] = "fake engine " + version,
            ["ironwail.pak"] = "fake pak",
            ["ironwail.pdb"] = "fake symbols",
            ["SDL2.dll"] = "fake sdl",
            ["QuakeVR.bat"] = "@echo off",
            ["quakevr/progs.dat"] = "fake progs " + version,
            ["quakevr/default.cfg"] = "exec vr_defaults.cfg",
            ["quakevr/vr_defaults.cfg"] = "vr_default vr_relight_strength 1.2",
            ["quakevr/maps/vrhub.bsp"] = "hub",
            ["quakevr/tools/ericw-tools/light.exe"] = "fake light",
            ["quakevr/tools/ericw-tools/gpl_v3.txt"] = "GPL-3",
        };
        foreach (var (k, v) in overrideFiles ?? [])
        {
            if (v.Length == 0)
            {
                files.Remove(k);
            }
            else
            {
                files[k] = v;
            }
        }
        foreach (var (rel, content) in files)
        {
            var path = Path.Combine(dir, rel.Replace('/', '\\'));
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path, content);
        }
        if (manifest)
        {
            File.WriteAllText(Path.Combine(dir, PackageManifest.FileName), PackageManifest.Create(dir, version).ToJson());
        }
        return dir;
    }

    public static string Zip(string folder, string zipPath, string? topFolder = null)
    {
        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (var f in Directory.EnumerateFiles(folder, "*", SearchOption.AllDirectories))
        {
            var rel = Path.GetRelativePath(folder, f).Replace('\\', '/');
            zip.CreateEntryFromFile(f, topFolder is null ? rel : topFolder + "/" + rel);
        }
        return zipPath;
    }

    public static string TextureZip(string zipPath)
    {
        using var zip = ZipFile.Open(zipPath, ZipArchiveMode.Create);
        foreach (var name in new[] { "id1/textures/wall1.png", "id1/textures/e1m1/floor.png", "hipnotic/textures/h_wall.png",
                     "rogue/textures/r_wall.png", "quakevr/textures_quetoo/x_norm.png", "README.txt" })
        {
            using var s = zip.CreateEntry(name).Open();
            s.Write(Encoding.UTF8.GetBytes("png:" + name));
        }
        return zipPath;
    }
}
