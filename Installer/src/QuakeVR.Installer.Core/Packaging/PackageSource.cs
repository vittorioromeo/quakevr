using System.IO.Compression;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// A Quake VR package: the folder or zip <c>Windows/package-quakevr.ps1</c> makes (<c>dist\QuakeVR</c>,
/// <c>dist\QuakeVR.zip</c>). A zip whose files all sit in one top folder (e.g. <c>QuakeVR/</c>) is read from inside it.
/// </summary>
public abstract class PackageSource : IDisposable
{
    public abstract string Location { get; }

    /// <summary>Relative paths (forward slashes) of every file in the package.</summary>
    public abstract IReadOnlyCollection<string> Files { get; }

    public abstract Stream Open(string relative);

    public abstract long SizeOf(string relative);

    public PackageManifest? ReadManifest()
    {
        if (!Files.Contains(PackageManifest.FileName, StringComparer.OrdinalIgnoreCase))
        {
            return null;
        }
        using var s = Open(PackageManifest.FileName);
        using var r = new StreamReader(s);
        return PackageManifest.Parse(r.ReadToEnd());
    }

    public static PackageSource FromPath(string path) =>
        Directory.Exists(path) ? new FolderPackage(path) :
        File.Exists(path) ? new ZipPackage(path) :
        throw new FileNotFoundException($"no package at {path}");

    public virtual void Dispose() => GC.SuppressFinalize(this);
}

sealed class FolderPackage : PackageSource
{
    readonly string _root;
    readonly HashSet<string> _files;

    public FolderPackage(string root)
    {
        _root = Path.GetFullPath(root);
        _files = new HashSet<string>(Directory.EnumerateFiles(_root, "*", SearchOption.AllDirectories)
            .Select(f => PathUtil.ToRelative(_root, f)), StringComparer.OrdinalIgnoreCase);
    }

    public override string Location => _root;
    public override IReadOnlyCollection<string> Files => _files;
    public override Stream Open(string relative) =>
        new FileStream(PathUtil.SafeCombine(_root, relative), FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20);
    public override long SizeOf(string relative) => new FileInfo(PathUtil.SafeCombine(_root, relative)).Length;
}

sealed class ZipPackage : PackageSource
{
    readonly string _path;
    readonly ZipArchive _zip;
    readonly Dictionary<string, ZipArchiveEntry> _entries = new(StringComparer.OrdinalIgnoreCase);

    public ZipPackage(string path)
    {
        _path = Path.GetFullPath(path);
        _zip = ZipFile.OpenRead(_path);
        var files = _zip.Entries.Where(e => !e.FullName.EndsWith('/') && !e.FullName.EndsWith('\\')).ToList();
        // One top folder holding everything: read from inside it.
        var prefix = "";
        var tops = files.Select(e => e.FullName.Replace('\\', '/').Split('/')[0]).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        if (tops.Count == 1 && files.All(e => e.FullName.Replace('\\', '/').Contains('/')))
        {
            prefix = tops[0] + "/";
        }
        foreach (var e in files)
        {
            _entries[e.FullName.Replace('\\', '/')[prefix.Length..]] = e;
        }
    }

    public override string Location => _path;
    public override IReadOnlyCollection<string> Files => _entries.Keys;
    public override Stream Open(string relative) => _entries[relative].Open();
    public override long SizeOf(string relative) => _entries[relative].Length;

    public override void Dispose()
    {
        _zip.Dispose();
        base.Dispose();
    }
}
