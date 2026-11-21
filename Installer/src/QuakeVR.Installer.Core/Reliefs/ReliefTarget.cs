using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.Core.Reliefs;

/// <summary>
/// "Make reliefs only": the reliefs step on a folder that already plays Quake VR, nothing else installed or changed
/// (only <c>&lt;folder&gt;\reliefs</c> is written). Such a folder is Setup's own install (install.json) or a Quake folder
/// with a <c>quakevr</c> folder (the engine beside id1, as a developer's or a hand install runs it), with Dwell
/// (<c>dwell</c>, <c>dwellv2p2</c>) and/or Quoth (<c>quoth</c>) in it. The engine reads <c>&lt;last basedir&gt;\reliefs</c>,
/// so the reliefs go into that folder. Shared by the window's "Make reliefs only" and <c>qvr-setup reliefs</c>.
/// </summary>
public sealed record ReliefTarget(string Dir, IReadOnlyList<string> ModDirs, string? QuakeVrDir, IReadOnlyList<string> PaletteDirs, bool OwnInstall)
{
    /// <summary>Dwell's folder names, the one the game plays first (Dwell.Find's order), then Quoth's.</summary>
    static readonly string[][] ModFolders = [[Dwell.Folder, Dwell.AltFolder], [Quoth.Folder]];

    /// <summary>The folder's Dwell and Quoth (each once: <c>dwell</c> before <c>dwellv2p2</c>), its quakevr and where
    /// Quake's palette is read: the mods, the folder's own id1, then the Quake folder an install record or the caller names.</summary>
    public static ReliefTarget For(string dir, string? quakeDir = null)
    {
        var full = PathUtil.TryNormalize(dir) ?? dir;
        var mods = new List<string>();
        foreach (var names in ModFolders)
        {
            if (names.Select(n => Path.Combine(full, n)).FirstOrDefault(IsGameFolder) is { } found)
            {
                mods.Add(found);
            }
        }
        InstallRecord? record = null;
        try
        {
            record = InstallRecord.Load(full);
        }
        catch (Exception e) when (e is IOException or InvalidDataException or UnauthorizedAccessException or System.Text.Json.JsonException)
        {
            // Not Setup's install (or unreadable): the folder's own id1 and the caller's Quake are still looked at.
        }
        var quakevr = Path.Combine(full, "quakevr");
        var palettes = new List<string>(mods) { Path.Combine(full, "id1") };
        foreach (var q in new[] { record?.QuakeDir, quakeDir })
        {
            if (q is { Length: > 0 } && !palettes.Any(p => PathUtil.SamePath(p, Path.Combine(q, "id1"))))
            {
                palettes.Add(Path.Combine(q, "id1"));
            }
        }
        return new ReliefTarget(full, mods, Directory.Exists(quakevr) ? quakevr : null, palettes, record is not null);
    }

    /// <summary>A mod folder the game can play: progs.dat, pak0.pak or a maps folder in it.</summary>
    static bool IsGameFolder(string dir) =>
        Directory.Exists(dir) && (File.Exists(Path.Combine(dir, "progs.dat")) || File.Exists(Path.Combine(dir, "pak0.pak")) || Directory.Exists(Path.Combine(dir, "maps")));

    /// <summary>Quake VR plays from this folder (Setup's install, or a quakevr folder in it) and has Dwell or Quoth.</summary>
    public bool Offered => (OwnInstall || QuakeVrDir is not null) && ModDirs.Count > 0;

    /// <summary>"Dwell and Quoth", "Dwell" or "Quoth".</summary>
    public string ModsText => string.Join(" and ", ModDirs.Select(m => Path.GetFileName(m).StartsWith("dwell", StringComparison.OrdinalIgnoreCase) ? "Dwell" : "Quoth"));

    /// <summary>The reliefs already in the folder (made by an earlier run, the window's or the console's).</summary>
    public int Existing => Directory.Exists(ReliefMaker.TextureDir(Dir)) ? Directory.EnumerateFiles(ReliefMaker.TextureDir(Dir), "*_norm.png").Count() : 0;

    /// <summary>The folders offered, in the order given (the window: its install, then the Quake folders it found), each once.</summary>
    public static List<ReliefTarget> Find(IEnumerable<(string? Dir, string? QuakeDir)> folders)
    {
        var found = new List<ReliefTarget>();
        foreach (var (dir, quake) in folders)
        {
            if (dir is null || !Directory.Exists(dir) || found.Any(f => PathUtil.SamePath(f.Dir, dir)))
            {
                continue;
            }
            if (For(dir, quake) is { Offered: true } t)
            {
                found.Add(t);
            }
        }
        return found;
    }

    /// <summary>The step on this folder: only the missing reliefs are made (ReliefMaker's rule). Throws
    /// InvalidDataException when Quake's palette is in none of <see cref="PaletteDirs"/>.</summary>
    public ReliefSummary Run(bool forceCpu, IProgress<InstallProgress>? progress, CancellationToken ct, int? limit = null)
    {
        var palette = ReliefMaker.FindPalette(PaletteDirs)
                      ?? throw new InvalidDataException("Quake's palette (id1's gfx/palette.lmp) wasn't found in " + string.Join(", ", PaletteDirs));
        var quakevr = QuakeVrDir ?? Path.Combine(Dir, "quakevr");
        return ReliefMaker.Run(Dir, ModDirs, [quakevr], palette, forceCpu, progress, ct, limit, quakevr);
    }

    /// <summary>The Done page's and the console's line: how many were made and how many already existed.</summary>
    public static string Outcome(ReliefSummary s) =>
        $"{s.Made} made, {s.Kept} already existed{(s.Failed > 0 ? $", {s.Failed} failed" : "")}{(s.Cancelled ? " (cancelled: the next run makes the rest)" : "")}";
}
