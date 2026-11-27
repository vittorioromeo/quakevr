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

/// <summary>Packages on disk: what one holds, and the one shipped beside the installer (an offline download).</summary>
public static class LocalPackages
{
    /// <summary>A package's manifest, or why the path is not a package (no manifest.json, unreadable).</summary>
    public static (PackageManifest? Manifest, string? Error) Inspect(string path, string product = "Quake VR: UNLEASHED")
    {
        var name = Path.GetFileName(path.TrimEnd('\\', '/'));
        try
        {
            using var source = PackageSource.FromPath(path);
            return source.ReadManifest() is { } m ? (m, null) : (null, $"{name} is not a {product} package: it has no manifest.json.");
        }
        catch (Exception e) when (e is IOException or InvalidDataException or UnauthorizedAccessException or System.Text.Json.JsonException or NotSupportedException)
        {
            return (null, $"{name} could not be read as a {product} package ({e.Message}).");
        }
    }

    /// <summary>The first package in <paramref name="dir"/> that has a manifest: QuakeVR.zip, a QuakeVR folder, then
    /// any other QuakeVR*.zip (texture packs excluded). Null when there is none.</summary>
    public static string? FindBeside(string dir)
    {
        var candidates = new List<string> { Path.Combine(dir, "QuakeVR.zip"), Path.Combine(dir, "QuakeVR") };
        try
        {
            candidates.AddRange(Directory.EnumerateFiles(dir, "QuakeVR*.zip").Order(StringComparer.OrdinalIgnoreCase));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            // Only the fixed names then.
        }
        return candidates.Distinct(StringComparer.OrdinalIgnoreCase)
            .Where(p => !Path.GetFileName(p).Contains("textures", StringComparison.OrdinalIgnoreCase))
            .FirstOrDefault(p => (File.Exists(p) || Directory.Exists(p)) && Inspect(p).Manifest is not null);
    }

    /// <summary>The package beside the installer (<see cref="FindBeside(string)"/>) and how its version stands against
    /// the installer's own release (<paramref name="ownVersion"/>, <see cref="InstallerBuild.Version"/>). Null when
    /// there is none.</summary>
    public static BesidePackage? FindBeside(string dir, string ownVersion)
    {
        if (FindBeside(dir) is not { } path)
        {
            return null;
        }
        var version = Inspect(path).Manifest?.Version;
        return new BesidePackage(path, version, ownVersion, Judge(ownVersion, version));
    }

    /// <summary>A package's version against the installer's own: by number and pre-release suffix alone (the
    /// installer knows no commit). An installer without a readable version (a bare development build) cannot tell:
    /// Same (used, as before).</summary>
    public static BesideOrder Judge(string ownVersion, string? packageVersion)
    {
        var own = ReleaseVersion.Parse(ownVersion);
        var package = ReleaseVersion.Parse(packageVersion);
        if (own.Number is null)
        {
            return BesideOrder.Same;
        }
        if (package.Number is null)
        {
            return BesideOrder.Unknown;
        }
        return ReleaseVersion.Compare(own.Short, package.Short) switch
        {
            VersionOrder.Same => BesideOrder.Same,
            VersionOrder.Newer => BesideOrder.Newer,
            VersionOrder.Older => BesideOrder.Older,
            _ => BesideOrder.Unknown,
        };
    }
}

/// <summary>How a package found beside the installer stands against the installer's own release.</summary>
public enum BesideOrder
{
    /// <summary>The installer's own version: used, as always.</summary>
    Same,
    /// <summary>Newer than the installer: used (saying so).</summary>
    Newer,
    /// <summary>Older (an old download left in the same folder): not used; the installer's release is downloaded.</summary>
    Older,
    /// <summary>No readable version: not used, as an older one.</summary>
    Unknown,
}

/// <summary>What to do with a package beside the installer.</summary>
public enum BesideAction
{
    /// <summary>Install from it.</summary>
    Use,
    /// <summary>Leave it; download the release instead.</summary>
    Download,
    /// <summary>No release online: ask whether to install it anyway (naming both versions).</summary>
    Ask,
    /// <summary>Leave it, and nothing else is there to install (offline, and the player said no).</summary>
    Skip,
}

/// <summary>A package found beside the installer (an offline download, or an old one left in Downloads: the 2026-11-27
/// report, a 1.2.0-beta5 installer that repaired a 1.2.0-beta2 install from a QuakeVR.zip of beta2 beside it). Used
/// at once only when it is the installer's own version or newer; an older or unreadable one only when no release can be
/// downloaded and the player agrees.</summary>
public sealed record BesidePackage(string Path, string? Version, string OwnVersion, BesideOrder Order)
{
    public string Name => System.IO.Path.GetFileName(Path.TrimEnd('\\', '/'));
    public string VersionShort => ReleaseVersion.Parse(Version).Short;
    public string OwnShort => ReleaseVersion.Parse(OwnVersion).Short;

    /// <summary>Used without asking: the installer's own version, or a newer one.</summary>
    public bool UsedAtOnce => Order is BesideOrder.Same or BesideOrder.Newer;

    /// <summary>What to do: <paramref name="online"/>, a release can be downloaded; <paramref name="agreed"/>, the
    /// player's answer to <see cref="AskText"/> (null: not asked yet).</summary>
    public BesideAction Decide(bool online, bool? agreed = null) =>
        UsedAtOnce ? BesideAction.Use :
        online ? BesideAction.Download :
        agreed switch { true => BesideAction.Use, false => BesideAction.Skip, null => BesideAction.Ask };

    string Found => Order == BesideOrder.Unknown
        ? $"Found {Name} beside the installer, but its version can't be read (this installer is {OwnShort})"
        : $"Found {Name} {VersionShort} beside the installer, {(Order == BesideOrder.Newer ? "newer" : "older")} than this installer's {OwnShort}";

    /// <summary>The Install page's details line for it (null for the installer's own version: nothing to say).
    /// <paramref name="downloading"/>: the release's version downloaded instead.</summary>
    public string? Note(BesideAction action, string? downloading = null) => (Order, action) switch
    {
        (BesideOrder.Same, _) => null,
        (_, BesideAction.Use) when Order == BesideOrder.Newer => $"{Found}: used.",
        (_, BesideAction.Use) => $"{Found}: used, as you chose (the online release couldn't be reached).",
        (_, BesideAction.Download) => $"{Found}: not used, downloading {(downloading is { } d ? ReleaseVersion.Parse(d).Short : "this installer's release")} instead.",
        _ => $"{Found}: not used.",
    };

    /// <summary>The question when no release can be downloaded: both versions named, and what an older one means.</summary>
    public string AskText => Order == BesideOrder.Unknown
        ? $"The online release couldn't be reached. The only package found is {Name} beside the installer, and its version can't be read " +
          $"(this installer is {OwnShort}).\n\nInstall from {Name} anyway?\n\nNo: nothing is installed from it. Connect to the internet and " +
          "press Try again, or pick another package."
        : $"The online release couldn't be reached. The only package found is {Name} beside the installer: it is {VersionShort}, older than " +
          $"this installer's {OwnShort}.\n\nInstall the older {VersionShort} from it anyway? Players on {OwnShort} can't join its games, nor it theirs." +
          $"\n\nNo: nothing is installed from it. Connect to the internet and press Try again, or pick another package.";
}
