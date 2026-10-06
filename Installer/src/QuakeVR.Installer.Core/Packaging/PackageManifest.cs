using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace QuakeVR.Installer.Core.Packaging;

public sealed class ManifestFile
{
    /// <summary>Relative to the package (and install) root, forward slashes.</summary>
    public string Path { get; set; } = "";
    public long Size { get; set; }
    /// <summary>Lowercase hex.</summary>
    public string Sha256 { get; set; } = "";
}

/// <summary>
/// <c>manifest.json</c> at a package's root (written by <c>Windows/package-quakevr.ps1</c>): the version and every
/// shipped file with its size and SHA-256. The installer copies exactly these files and checks each hash while
/// copying; files in the package but not in the manifest are not installed.
/// </summary>
public sealed class PackageManifest
{
    public const string FileName = "manifest.json";

    public int Schema { get; set; } = 1;
    public string Product { get; set; } = "Quake VR";
    public string Version { get; set; } = "";
    public List<ManifestFile> Files { get; set; } = [];

    [JsonIgnore]
    public long TotalSize => Files.Sum(f => f.Size);

    internal static readonly JsonSerializerOptions Json = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
    };

    public static PackageManifest Parse(string json)
    {
        var m = JsonSerializer.Deserialize<PackageManifest>(json, Json) ?? throw new InvalidDataException("empty manifest");
        m.Validate();
        return m;
    }

    public string ToJson() => JsonSerializer.Serialize(this, Json);

    public void Validate()
    {
        if (Schema != 1)
        {
            throw new InvalidDataException($"unsupported manifest schema {Schema} (this installer reads 1): update the installer");
        }
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var f in Files)
        {
            PathUtil.SafeCombine(@"C:\x", f.Path); // throws on unsafe paths
            if (f.Sha256.Length != 64 || !f.Sha256.All(Uri.IsHexDigit) || f.Size < 0)
            {
                throw new InvalidDataException($"bad manifest entry for {f.Path}");
            }
            if (!seen.Add(f.Path))
            {
                throw new InvalidDataException($"duplicate manifest entry {f.Path}");
            }
        }
    }

    /// <summary>A manifest of every file under <paramref name="folder"/> (less any manifest.json/install.json).</summary>
    public static PackageManifest Create(string folder, string version)
    {
        var m = new PackageManifest { Version = version };
        foreach (var file in Directory.EnumerateFiles(folder, "*", SearchOption.AllDirectories).Order(StringComparer.Ordinal))
        {
            var rel = PathUtil.ToRelative(folder, file);
            if (rel is FileName or InstallRecord.FileName)
            {
                continue;
            }
            using var fs = File.OpenRead(file);
            m.Files.Add(new ManifestFile { Path = rel, Size = fs.Length, Sha256 = Hash(fs) });
        }
        return m;
    }

    public static string Hash(Stream s) => Convert.ToHexStringLower(SHA256.HashData(s));

    public static string HashFile(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20);
        return Hash(fs);
    }
}
