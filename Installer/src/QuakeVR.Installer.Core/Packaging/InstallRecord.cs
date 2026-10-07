using System.Text.Json;

namespace QuakeVR.Installer.Core.Packaging;

public static class Components
{
    public const string Core = "core";
    public const string HdTextures = "hdtextures";
    /// <summary>VisPatch's data (see-through water for the relight), with the relight component.</summary>
    public const string VisPatch = "vispatch";
}

public sealed class InstalledFile
{
    public string Path { get; set; } = "";
    public long Size { get; set; }
    public string Sha256 { get; set; } = "";
    public string Component { get; set; } = Components.Core;
}

public sealed class InstallChoices
{
    public bool HdTextures { get; set; }
    public bool RelightOnFirstRun { get; set; }
    public bool VisPatch { get; set; }
    public bool DesktopShortcut { get; set; }
    public bool StartMenuShortcuts { get; set; }
    public bool FlatShortcut { get; set; }
    public bool LogShortcut { get; set; }
}

/// <summary>
/// <c>install.json</c> in the Quake VR folder: what this installer put there and where it pointed the shortcuts. The
/// uninstaller and updates remove only what it lists; everything else in the folder (config, saves, screenshots,
/// relit maps, Map Library downloads) is the player's.
/// </summary>
public sealed class InstallRecord
{
    public const string FileName = "install.json";

    public int Schema { get; set; } = 1;
    public string Product { get; set; } = "Quake VR: Unleashed";
    public string Version { get; set; } = "";
    public DateTimeOffset InstalledAt { get; set; }
    public DateTimeOffset? UpdatedAt { get; set; }
    /// <summary>The first -basedir (the folder with id1\pak0.pak). Never written to.</summary>
    public string QuakeDir { get; set; } = "";
    public string? QuakeStore { get; set; }
    public string? PackageSource { get; set; }
    public InstallChoices Choices { get; set; } = new();
    public List<InstalledFile> Files { get; set; } = [];
    /// <summary>Folders the installer created, relative (removed on uninstall only when empty).</summary>
    public List<string> Directories { get; set; } = [];
    /// <summary>Absolute paths of the shortcuts it made.</summary>
    public List<string> Shortcuts { get; set; } = [];
    /// <summary>The player asked for the relight at the first start at the last install or update (the game starts it
    /// from the marker, <see cref="FirstStartRelight"/>: whether it still has to is <see cref="FirstStartRelight.Pending"/>).</summary>
    public bool RelightPending { get; set; }

    public static InstallRecord? Load(string installDir)
    {
        var path = Path.Combine(installDir, FileName);
        if (!File.Exists(path))
        {
            return null;
        }
        var r = JsonSerializer.Deserialize<InstallRecord>(File.ReadAllText(path), PackageManifest.Json)
                ?? throw new InvalidDataException($"empty {path}");
        if (r.Schema != 1)
        {
            throw new InvalidDataException($"{path}: unsupported schema {r.Schema}");
        }
        return r;
    }

    /// <summary>Written to a temporary file, then moved over the old one: a crash never leaves half a record.</summary>
    public void Save(string installDir)
    {
        var path = Path.Combine(installDir, FileName);
        var tmp = path + ".tmp";
        File.WriteAllText(tmp, JsonSerializer.Serialize(this, PackageManifest.Json));
        File.Move(tmp, path, overwrite: true);
    }
}
