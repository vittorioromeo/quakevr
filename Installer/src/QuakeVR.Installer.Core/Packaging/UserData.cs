using System.Text.Json;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>The player's files in an install, by what "Install again from scratch" can reset or remove.</summary>
public enum UserDataKind
{
    /// <summary>Configs the game wrote or the player made (<c>ironwail.cfg</c>, <c>autoexec.cfg</c>...), the retro
    /// overrides and the body calibration: "Reset settings".</summary>
    Settings,
    /// <summary>Saved games (<c>*.sav</c>, the autosave folders): "Remove saves and installed maps".</summary>
    Saves,
    /// <summary>The Map Library's installed packages (<c>qvr_addons\</c>) and its list of them
    /// (<c>cache\maps_installed.txt</c>): "Remove saves and installed maps".</summary>
    Maps,
    /// <summary>Everything else of the player's that the game did not derive from its own files: screenshots, voice notes,
    /// recordings, logs, the tips seen and checklist ticks, maps and mods added by hand: "Remove screenshots, voice notes
    /// and your other files".</summary>
    Personal,
    /// <summary>Relit maps (<c>relit*\</c>) and caches (<c>cache\</c>): rebuilt from the game's files (relighting takes
    /// long), never reset or removed by Setup.</summary>
    Other,
}

/// <summary>
/// Program files are exactly what <c>install.json</c> lists (the package's manifest, the HD textures, the VisPatch data,
/// Setup's copy); every other file in the install folder is the player's, and no update, repair or uninstall touches it.
/// This sorts the player's files for "Install again from scratch" and for the plans' counts. Setup's own folders
/// (<c>backups\</c>, <c>setup\</c>) and install.json are neither.
/// </summary>
public static class UserData
{
    /// <summary>Where Setup keeps what it set aside (<see cref="Backup"/>), in the install.</summary>
    public const string BackupsFolder = "backups";

    public static UserDataKind Classify(string relative)
    {
        var p = relative.Replace('\\', '/').ToLowerInvariant();
        var parts = p.Split('/');
        var name = parts[^1];
        if (p.StartsWith("qvr_addons/", StringComparison.Ordinal) || p == "cache/maps_installed.txt")
        {
            return UserDataKind.Maps;
        }
        if (name.EndsWith(".sav", StringComparison.Ordinal) || parts[..^1].Contains("autosave"))
        {
            return UserDataKind.Saves;
        }
        if (parts[0] != "cache" && (name.EndsWith(".cfg", StringComparison.Ordinal) || name == "retro_overrides.txt" ||
                                    (parts.Length >= 3 && parts[1] == "bodycal")))
        {
            return UserDataKind.Settings;
        }
        if (parts[..^1].Any(d => d == "cache" || d.StartsWith("relit", StringComparison.Ordinal)))
        {
            return UserDataKind.Other;
        }
        return UserDataKind.Personal;
    }

    /// <summary>The player's files in <paramref name="installDir"/> (relative, forward slashes) and their kind.</summary>
    public static List<(string Path, UserDataKind Kind)> Find(string installDir, InstallRecord? record)
    {
        var ours = record?.Files.Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase) ?? [];
        var list = new List<(string, UserDataKind)>();
        if (!Directory.Exists(installDir))
        {
            return list;
        }
        foreach (var file in Directory.EnumerateFiles(installDir, "*", SearchOption.AllDirectories))
        {
            var rel = PathUtil.ToRelative(installDir, file);
            var top = rel.Split('/')[0];
            if (ours.Contains(rel) || rel.Equals(InstallRecord.FileName, StringComparison.OrdinalIgnoreCase) ||
                top.Equals(BackupsFolder, StringComparison.OrdinalIgnoreCase) || (top.Equals(SetupCopy.Folder, StringComparison.OrdinalIgnoreCase) && rel.Contains('/')) ||
                rel.EndsWith(".qvrnew", StringComparison.OrdinalIgnoreCase) ||
                rel.Equals("quakevr/" + FirstStartRelight.MarkerName, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }
            list.Add((rel, Classify(rel)));
        }
        list.Sort((a, b) => string.CompareOrdinal(a.Item1, b.Item1));
        return list;
    }

    /// <summary>"12 settings, 3 saves, 0 installed-map files, 30 personal, 10 other" for the plans.</summary>
    public static string Summary(IEnumerable<(string Path, UserDataKind Kind)> files)
    {
        var counts = files.GroupBy(f => f.Kind).ToDictionary(g => g.Key, g => g.Count());
        int C(UserDataKind k) => counts.GetValueOrDefault(k);
        return $"{C(UserDataKind.Settings)} settings, {C(UserDataKind.Saves)} saves, {C(UserDataKind.Maps)} installed-map files, {C(UserDataKind.Personal)} personal, {C(UserDataKind.Other)} other";
    }
}

public sealed class BackupEntry
{
    public string Path { get; set; } = "";
    public long Size { get; set; }
    public string Sha256 { get; set; } = "";
    public string Kind { get; set; } = "";
    /// <summary>Moved: no longer in the install (reset or removed). Copied: a copy of a file Setup then overwrote.</summary>
    public bool Moved { get; set; }
}

/// <summary>
/// A dated folder of the player's files Setup is about to reset, remove or overwrite:
/// <c>&lt;QVR&gt;\backups\&lt;yyyy-MM-dd HHmmss&gt; &lt;reason&gt;\</c>, the files at their paths in the install, and
/// <c>backup.json</c> listing each with its size and SHA-256. Made before anything is changed, and checked: every file is
/// hashed before and after it goes in (a mismatch stops Setup). Setup never deletes a backup; it is the player's.
/// </summary>
public sealed class Backup
{
    public const string ListName = "backup.json";

    public int Schema { get; set; } = 1;
    public string Reason { get; set; } = "";
    public DateTimeOffset CreatedAt { get; set; }
    public string InstallDir { get; set; } = "";
    public string? InstalledVersion { get; set; }
    public List<BackupEntry> Files { get; set; } = [];

    [System.Text.Json.Serialization.JsonIgnore]
    public string Dir { get; private set; } = "";

    /// <summary>A new, empty backup folder (a free name: "-2", "-3"... when that second's is taken).</summary>
    public static Backup Create(string installDir, string reason, string? version, DateTimeOffset now)
    {
        var root = Path.Combine(installDir, UserData.BackupsFolder);
        var name = $"{now:yyyy-MM-dd HHmmss} {reason}";
        var dir = Path.Combine(root, name);
        for (var i = 2; Directory.Exists(dir); ++i)
        {
            dir = Path.Combine(root, $"{name}-{i}");
        }
        Directory.CreateDirectory(dir);
        var b = new Backup { Reason = reason, CreatedAt = now, InstallDir = installDir, InstalledVersion = version, Dir = dir };
        b.Save();
        return b;
    }

    /// <summary>Copies a file of the install into the backup (it stays in the install), checked.</summary>
    public void Copy(string relative, string kind) => Add(relative, kind, move: false);

    /// <summary>Moves a file of the install into the backup (reset or removed), checked.</summary>
    public void Move(string relative, string kind) => Add(relative, kind, move: true);

    void Add(string relative, string kind, bool move)
    {
        var source = PathUtil.SafeCombine(InstallDir, relative);
        var dest = PathUtil.SafeCombine(Dir, relative);
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(dest)!);
        var sha = PackageManifest.HashFile(source);
        var size = new FileInfo(source).Length;
        if (move)
        {
            File.Move(source, dest);
        }
        else
        {
            File.Copy(source, dest);
        }
        if (!string.Equals(PackageManifest.HashFile(dest), sha, StringComparison.OrdinalIgnoreCase))
        {
            throw new InstallException($"The backup of {relative} in {Dir} does not match the original: stopped, nothing else was changed.");
        }
        Files.Add(new BackupEntry { Path = relative, Size = size, Sha256 = sha, Kind = kind, Moved = move });
        Save();
    }

    /// <summary>backup.json, written after every file (an interrupted backup still lists what it holds).</summary>
    public void Save()
    {
        var path = System.IO.Path.Combine(Dir, ListName);
        File.WriteAllText(path + ".tmp", JsonSerializer.Serialize(this, PackageManifest.Json));
        File.Move(path + ".tmp", path, overwrite: true);
    }

    public static Backup Load(string dir)
    {
        var b = JsonSerializer.Deserialize<Backup>(File.ReadAllText(System.IO.Path.Combine(dir, ListName)), PackageManifest.Json)
                ?? throw new InvalidDataException($"empty {ListName} in {dir}");
        b.Dir = dir;
        return b;
    }

    /// <summary>Every file backup.json lists, present with its size and SHA-256 (the problems; none: intact).</summary>
    public static List<string> Verify(string dir)
    {
        var b = Load(dir);
        var problems = new List<string>();
        foreach (var f in b.Files)
        {
            var p = PathUtil.SafeCombine(dir, f.Path);
            if (!File.Exists(p))
            {
                problems.Add($"missing: {f.Path}");
            }
            else if (new FileInfo(p).Length != f.Size || !string.Equals(PackageManifest.HashFile(p), f.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                problems.Add($"changed: {f.Path}");
            }
        }
        return problems;
    }
}
