using System.IO.Compression;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// Dwell, the installer's optional component (docs/vr-port/DWELL.md, section 7): Fairweather and the Dwell team's free
/// two-episode, 24-map Copper-based mod (<c>dwellv2p2.zip</c>, <see cref="BuiltInComponents.Dwell"/>), downloaded from
/// Quaddicted, checked, and unpacked as it is into <c>&lt;install&gt;\dwell</c> by the shared third-party path
/// (<see cref="ModComponent"/>, the one Arcane Dimensions uses). Its soundtrack is in the zip. A copy of v2.2 already in
/// an owned Quake root is used where it is (nothing downloaded); older versions are reported, not used.
/// </summary>
public static class Dwell
{
    /// <summary>
    /// Whether the component is ticked at the start: on since the build in which Dwell's maps play by Copper's rules
    /// (P1b, docs/vr-port/DWELL.md sections 7 and 9.2). The component is recommended either way.
    /// </summary>
    public const bool OnByDefault = true;

    public const string Folder = "dwell";
    /// <summary>The folder name Quaddicted's and the zip's own layout use: also accepted in an owned root.</summary>
    public const string AltFolder = "dwellv2p2";
    public const string Title = "Dwell";
    public const string Credit = "free, by Fairweather and the Dwell team: two episodes, 24 maps, soundtrack included";
    /// <summary>v2.2's files unpacked (447 files, the READMEs, the pak, progs and 30 music tracks).</summary>
    public const long InstalledSize = 799_533_222;
    public const string HomeUrl = "https://www.quaddicted.com/reviews/dwellv2p2.html";
    public const string ReadmeFile = "README_dwellv2p2.txt";
    /// <summary>v2.2's progs.dat SHA-256: what makes a folder "Dwell v2.2" (the engine's check, when it plays it).</summary>
    public const string ProgsSha256 = "fe7d21d4bdfd1a5e6672d1606efd774cb730d7f5b68941d82175596f22e719fd";

    /// <summary>The zip's top folder, which becomes <see cref="Folder"/>; its two READMEs sit beside it.</summary>
    const string ZipFolder = "dwellv2p2/";

    /// <summary>Where a zip entry goes under <c>dwell\</c>: the folder's files as they are, the READMEs inside it, and the
    /// one capitalised track lower-cased (<c>music/Track107.ogg</c>: a case-sensitive file system, Linux or the Quest,
    /// would not find it for the engine's lower-case lookup; a rename on install, not a modification of the content).</summary>
    public static string? Map(string entry)
    {
        var rel = entry.StartsWith(ZipFolder, StringComparison.OrdinalIgnoreCase) ? entry[ZipFolder.Length..] : entry;
        return rel.Equals("music/Track107.ogg", StringComparison.Ordinal) ? "music/track107.ogg" : rel;
    }

    public static readonly ModSpec Spec = new(Components.Dwell, Folder, Title, "progs.dat", Map,
        r => (r.DwellFile, r.DwellSha256),
        (r, file, sha) => { r.DwellFile = file; r.DwellSha256 = sha; },
        (r, on) => r.Choices.Dwell = on,
        "qvr-setup dwell");

    public static List<(string Relative, ZipArchiveEntry Entry)> Entries(ZipArchive zip) => ModComponent.Entries(Spec, zip);

    public static (ModAction Action, string Text) Plan(string installDir, InstallRecord installed, FeedFile? target, bool verify) =>
        ModComponent.Plan(Spec, installDir, installed, target, verify);

    public static InstallRecord Install(string installDir, string zipPath, FeedFile expected, IProgress<InstallProgress>? progress, CancellationToken ct) =>
        ModComponent.Install(Spec, installDir, zipPath, expected, progress, ct);

    public static Task<ModOutcome> AddAsync(HttpClient http, string installDir, FeedFile component, string downloadsDir, string? localZip,
        IProgress<InstallProgress>? progress, IProgress<DownloadProgress>? download, CancellationToken ct, int attemptsPerMirror = 2) =>
        ModComponent.AddAsync(Spec, http, installDir, component, downloadsDir, localZip, progress, download, ct, attemptsPerMirror);

    /// <summary>The owned roots an existing copy is looked for in (as Arcane Dimensions').</summary>
    public static IReadOnlyList<string> SearchRoots(IReadOnlyList<string> storeRoots, string quakeDir) =>
        ArcaneDimensions.SearchRoots(storeRoots, quakeDir);

    /// <summary>A folder's state by its progs.dat's SHA-256 and pak0.pak (<paramref name="progsSha256"/>: tests pass their own).</summary>
    public static ModCopy? Inspect(string root, string folder, string? progsSha256 = null)
    {
        var progs = Path.Combine(folder, "progs.dat");
        var hasPak = File.Exists(Path.Combine(folder, "pak0.pak"));
        var where = PathUtil.TryNormalize(folder) ?? folder;
        var r = PathUtil.TryNormalize(root) ?? root;
        if (!File.Exists(progs))
        {
            return hasPak || Directory.Exists(Path.Combine(folder, "maps"))
                ? new ModCopy(r, where, ModCopyState.Incomplete, "no progs.dat")
                : null; // none of its data: a lower root's copy counts
        }
        if (!string.Equals(PackageManifest.HashFile(progs), progsSha256 ?? ProgsSha256, StringComparison.OrdinalIgnoreCase))
        {
            return new ModCopy(r, where, ModCopyState.OtherVersion, "not v2.2 (its progs.dat is another version's)");
        }
        return hasPak
            ? new ModCopy(r, where, ModCopyState.Ready, "v2.2")
            : new ModCopy(r, where, ModCopyState.Incomplete, "no pak0.pak");
    }

    /// <summary>The copy the game would use: the highest-priority root with a <c>dwell</c> (else <c>dwellv2p2</c>) folder
    /// holding any of Dwell's data; then its state. Null: none.</summary>
    public static ModCopy? FindExisting(IReadOnlyList<string> ownedRoots, string? progsSha256 = null)
    {
        for (var r = ownedRoots.Count - 1; r >= 0; --r)
        {
            foreach (var name in new[] { Folder, AltFolder })
            {
                var folder = Path.Combine(ownedRoots[r], name);
                if (Directory.Exists(folder) && Inspect(ownedRoots[r], folder, progsSha256) is { } copy)
                {
                    return copy;
                }
            }
        }
        return null;
    }

    /// <summary>The line the Options page and the console show for a found copy.</summary>
    public static string Describe(ModCopy copy) => copy.State switch
    {
        ModCopyState.Ready => $"Found in {copy.Folder}: will be used (nothing downloaded).",
        ModCopyState.OtherVersion => $"Found in {copy.Folder}, but it isn't v2.2, the only version Quake VR plays: tick to download v2.2 into Quake VR's folder (your copy is left as it is).",
        _ => $"Found in {copy.Folder}, {copy.Detail}: tick to download a complete copy into Quake VR's folder (yours is left as it is).",
    };
}
