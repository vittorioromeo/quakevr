using System.IO.Compression;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// Quoth 2.2, the installer's optional component (docs/vr-port/QUOTH.md): Kell, Necros and Preach's free mod, whose maps
/// Quake VR plays by Arcane Dimensions' rules (its own progs never run). Its licence lets its zip be passed on only
/// unmodified, so Quake VR never hosts it: Quaddicted's <c>quoth2pt2full_2.zip</c> (<see cref="BuiltInComponents.Quoth"/>)
/// is downloaded, checked and unpacked as it is into <c>&lt;install&gt;\quoth</c> by the shared third-party path
/// (<see cref="ModComponent"/>, the one Arcane Dimensions and Dwell use). It needs Arcane Dimensions (<see cref="NeedsAdText"/>).
/// A 2.2 copy already in an owned Quake root is used where it is; another version is reported, not used.
/// </summary>
public static class Quoth
{
    /// <summary>Whether the component is ticked at the start, when Arcane Dimensions is chosen (without AD it is off).</summary>
    public const bool OnByDefault = true;

    public const string Folder = "quoth";
    public const string Title = "Quoth";
    public const string Credit = "free, by Kell, Necros and Preach: its own maps and its monsters in hundreds of custom maps (experimental)";
    /// <summary>2.2's six files unpacked (three paks, quoth.cfg, launch.bat, the readme).</summary>
    public const long InstalledSize = 141_233_329;
    public const string HomeUrl = "https://www.quaddicted.com/reviews/quoth2pt2full_2.html";
    public const string ReadmeFile = "quoth2_2.txt";
    /// <summary>2.2's pak2.pak SHA-256: what makes a folder "Quoth 2.2" (the engine's check, <c>VR_QUOTH_PIN_SHA</c>).</summary>
    public const string Pak2Sha256 = "4a62df0587cb93ae080807614a5428f1bf26c83770c6fd4e7168e9ef7a4c3e77";

    /// <summary>Why the box is off and greyed while Arcane Dimensions is not chosen (not ticked, not in the install, no
    /// usable copy found): Quoth is played only by AD's rules.</summary>
    public const string NeedsAdText = "Needs Arcane Dimensions: Quake VR plays Quoth's maps by its rules. Tick Arcane Dimensions to add Quoth.";

    /// <summary>The zip's six files sit at its root: all of them go under <c>quoth\</c> as they are.</summary>
    public static readonly ModSpec Spec = new(Components.Quoth, Folder, Title, "pak2.pak", n => n,
        r => (r.QuothFile, r.QuothSha256),
        (r, file, sha) => { r.QuothFile = file; r.QuothSha256 = sha; },
        (r, on) => r.Choices.Quoth = on,
        "qvr-setup quoth");

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

    /// <summary>A folder's state by its pak2.pak's SHA-256 and its other two paks (<paramref name="pak2Sha256"/>: tests pass their own).</summary>
    public static ModCopy? Inspect(string root, string folder, string? pak2Sha256 = null)
    {
        var pak2 = Path.Combine(folder, "pak2.pak");
        var others = new[] { "pak0.pak", "pak1.pak" }.Count(p => File.Exists(Path.Combine(folder, p)));
        var where = PathUtil.TryNormalize(folder) ?? folder;
        var r = PathUtil.TryNormalize(root) ?? root;
        if (!File.Exists(pak2))
        {
            return others > 0 ? new ModCopy(r, where, ModCopyState.Incomplete, "no pak2.pak") : null; // none of its data: a lower root's copy counts
        }
        if (!string.Equals(PackageManifest.HashFile(pak2), pak2Sha256 ?? Pak2Sha256, StringComparison.OrdinalIgnoreCase))
        {
            return new ModCopy(r, where, ModCopyState.OtherVersion, "not 2.2 (its pak2.pak is another version's)");
        }
        return others == 2
            ? new ModCopy(r, where, ModCopyState.Ready, "2.2")
            : new ModCopy(r, where, ModCopyState.Incomplete, "pak0.pak or pak1.pak missing");
    }

    /// <summary>The copy the game would use: the highest-priority root with a <c>quoth</c> folder holding any of Quoth's
    /// paks; then its state. Null: none.</summary>
    public static ModCopy? FindExisting(IReadOnlyList<string> ownedRoots, string? pak2Sha256 = null)
    {
        for (var r = ownedRoots.Count - 1; r >= 0; --r)
        {
            var folder = Path.Combine(ownedRoots[r], Folder);
            if (Directory.Exists(folder) && Inspect(ownedRoots[r], folder, pak2Sha256) is { } copy)
            {
                return copy;
            }
        }
        return null;
    }

    /// <summary>The line the Options page and the console show for a found copy.</summary>
    public static string Describe(ModCopy copy) => copy.State switch
    {
        ModCopyState.Ready => $"Found in {copy.Folder}: will be used (nothing downloaded).",
        ModCopyState.OtherVersion => $"Found in {copy.Folder}, but it isn't 2.2, the only version Quake VR plays: tick to download 2.2 into Quake VR's folder (your copy is left as it is).",
        _ => $"Found in {copy.Folder}, {copy.Detail}: tick to download a complete copy into Quake VR's folder (yours is left as it is).",
    };
}
