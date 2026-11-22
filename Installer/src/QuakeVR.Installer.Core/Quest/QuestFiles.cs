using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.Core.Quest;

/// <summary>One file for the headset: its path under the app's files folder ('/'-separated) and where it is on this PC.</summary>
public sealed record QuestFile(string DevicePath, string Source, long Size, string What);

/// <summary>
/// What the Quest needs, read from what the PC target already has (Quest is another target of the same install): the
/// game's own files from a Quake VR install's record (the PC install, or the Quest-only staging folder the same install
/// steps fill), less the PC's programs; and the player's Quake data where the PC game reads it in place: id1's paks and
/// music, the official expansions the detection finds, and Arcane Dimensions, Dwell or Quoth copies already on this PC.
/// Android's file names are case-sensitive: Quake's own files go lower case, as the engine asks for them.
/// </summary>
public static class QuestFiles
{
    /// <summary>The app (Android/app's manifest) and its files folder: the game's base dir on the headset.</summary>
    public const string Package = "com.vittorioromeo.quakevr";
    public const string DeviceRoot = "/sdcard/Android/data/" + Package + "/files";

    /// <summary>The PC's programs and tools, never needed on the headset (the engine is the APK's).</summary>
    static readonly string[] PcOnlyPrefixes = ["setup/", "quakevr/tools/", "quakevr/xr_layers/"];
    static readonly string[] PcOnlyExtensions = [".exe", ".dll", ".pdb", ".bat", ".cmd", ".ps1", ".lnk"];
    /// <summary>The player's own files in a mod folder found on this PC (other engines' saves and configs): left behind.</summary>
    static readonly string[] PlayerExtensions = [".sav", ".dem", ".log"];

    static bool PlayerFile(string rel) => PlayerExtensions.Contains(Path.GetExtension(rel), StringComparer.OrdinalIgnoreCase) ||
                                          Path.GetFileName(rel).Equals("config.cfg", StringComparison.OrdinalIgnoreCase);

    public static bool PcOnly(string rel)
    {
        rel = rel.Replace('\\', '/');
        return !rel.Contains('/') || PcOnlyPrefixes.Any(p => rel.StartsWith(p, StringComparison.OrdinalIgnoreCase)) ||
               PcOnlyExtensions.Contains(Path.GetExtension(rel), StringComparer.OrdinalIgnoreCase);
    }

    /// <param name="installDir">A Quake VR install (its install record lists the game's files).</param>
    /// <param name="quakeDir">The player's Quake (the folder with id1).</param>
    /// <param name="storeRoots">The stores' Quake folders (<see cref="ExpansionDetector.StoreRoots"/>; tests pass none).</param>
    public static List<QuestFile> Plan(string installDir, string quakeDir, IReadOnlyList<string> storeRoots)
    {
        var record = InstallRecord.Load(installDir)
                     ?? throw new InstallException($"No Quake VR install in {installDir} (no {InstallRecord.FileName}): install it first.");
        var files = new Dictionary<string, QuestFile>(StringComparer.OrdinalIgnoreCase);
        void Add(string devicePath, string source, string what)
        {
            if (!files.ContainsKey(devicePath) && File.Exists(source))
            {
                files[devicePath] = new QuestFile(devicePath, source, new FileInfo(source).Length, what);
            }
        }

        foreach (var f in record.Files.Where(f => f.Component != Components.Setup))
        {
            var rel = f.Path.Replace('\\', '/');
            if (!PcOnly(rel))
            {
                Add(rel, PathUtil.SafeCombine(installDir, rel), f.Component);
            }
        }
        // The game folders the install has its own data for (HD textures and VisPatch data only add to the player's).
        var folders = files.Values.Where(f => f.What is not (Components.HdTextures or Components.VisPatch))
            .Select(f => f.DevicePath[..f.DevicePath.IndexOf('/')]).ToHashSet(StringComparer.OrdinalIgnoreCase);

        // id1: the paks (pak0 + pak1, or the rerelease's one), and the music the PC game reads in place.
        var id1 = Path.Combine(quakeDir, "id1");
        if (Directory.Exists(id1))
        {
            foreach (var pak in Directory.EnumerateFiles(id1, "pak*.pak").Order(StringComparer.OrdinalIgnoreCase))
            {
                Add("id1/" + Path.GetFileName(pak).ToLowerInvariant(), pak, "id1");
            }
        }
        if (QuakeData.FindMusic(quakeDir) is { } music)
        {
            foreach (var track in Directory.EnumerateFiles(music).Order(StringComparer.OrdinalIgnoreCase))
            {
                if (Path.GetExtension(track).ToLowerInvariant() is ".ogg" or ".mp3" or ".flac" or ".wav" or ".opus")
                {
                    Add("id1/music/" + Path.GetFileName(track).ToLowerInvariant(), track, "music");
                }
            }
        }

        // The official expansions the PC game plays, from where the detection finds them.
        foreach (var e in ExpansionDetector.Detect([quakeDir, installDir], storeRoots))
        {
            if (e.State != ExpansionState.Ready || e.Root is null || folders.Contains(e.Folder))
            {
                continue;
            }
            var dir = Path.Combine(e.Root, e.Folder);
            foreach (var pak in Directory.EnumerateFiles(dir, "pak*.pak").Order(StringComparer.OrdinalIgnoreCase))
            {
                Add($"{e.Folder}/{Path.GetFileName(pak).ToLowerInvariant()}", pak, e.Folder);
            }
            foreach (var r in PackLists.Resources(e.Folder))
            {
                Add($"{e.Folder}/{r.ToLowerInvariant()}", Path.Combine(dir, r.Replace('/', '\\')), e.Folder);
            }
            folders.Add(e.Folder);
        }

        // Arcane Dimensions, Dwell, Quoth: a copy already on this PC that the PC game plays where it is (when Setup did
        // not install its own into the install folder above).
        var adRoots = ArcaneDimensions.SearchRoots(storeRoots, quakeDir);
        foreach (var copy in new[]
                 {
                     folders.Contains(ArcaneDimensions.Folder) ? null : ArcaneDimensions.FindExisting(adRoots),
                     folders.Contains(Dwell.Folder) || folders.Contains(Dwell.AltFolder) ? null : Dwell.FindExisting(Dwell.SearchRoots(storeRoots, quakeDir)),
                     folders.Contains(Quoth.Folder) ? null : Quoth.FindExisting(Quoth.SearchRoots(storeRoots, quakeDir)),
                 })
        {
            if (copy is not { Usable: true })
            {
                continue;
            }
            var dir = Path.Combine(copy.Root, copy.Folder);
            foreach (var file in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories).Order(StringComparer.OrdinalIgnoreCase))
            {
                var rel = PathUtil.ToRelative(dir, file);
                if (!PlayerFile(rel) && !PcOnly(copy.Folder + "/" + rel))
                {
                    Add($"{copy.Folder}/{rel}", file, copy.Folder);
                }
            }
        }
        return [.. files.Values.OrderBy(f => f.DevicePath, StringComparer.Ordinal)];
    }
}
