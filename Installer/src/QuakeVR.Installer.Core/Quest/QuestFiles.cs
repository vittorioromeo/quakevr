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

    /// <summary>The music formats the Quest build plays: WAV and Ogg Vorbis (Android/android.cmake: stb_vorbis), so the
    /// stores' Ogg soundtracks go too; MP3/FLAC stay on the PC (no codec there). Add a format here when the Quest build
    /// gets its codec.</summary>
    public static readonly string[] MusicFormats = [".wav", ".ogg"];
    /// <summary>A relit copy's files (vr_relight.cpp: the map, its .lit and .lux, and the .relight note naming its source).</summary>
    static readonly string[] RelitExtensions = [".bsp", ".lit", ".lux", ".relight"];

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
            // (a path relative to the app's files/ folder, '/' between parts: the headset's storage refuses ':' and '\',
            // where the copy would fail)
            if (devicePath.StartsWith('/') || devicePath.IndexOfAny([':', '\\']) >= 0)
            {
                throw new InstallException($"Not a path on the Quest: {devicePath} (from {source}).");
            }
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
        // (where the detection finds it: the rerelease's first, as the PC game plays it; the headset reads music/trackNN.<ext>
        // through its search path, so id1/music; only the formats the Quest build decodes)
        if (QuakeDetector.Describe(QuakeStore.Manual, "", quakeDir).MusicDir is { } music)
        {
            foreach (var track in Directory.EnumerateFiles(music).Order(StringComparer.OrdinalIgnoreCase))
            {
                if (MusicFormats.Contains(Path.GetExtension(track), StringComparer.OrdinalIgnoreCase))
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
            // Dimension of the Past, of the Machine, Dawn of the Machine: their text is in the rerelease's language table
            // (its id1/pak0.pak), which the PC game borrows from the store's rerelease in place. The headset has no store:
            // with the original's id1 above, it said "dopa language data incomplete" and refused them (questmods,
            // 2026-11-24). The rerelease's id1 paks go to <files>/rerelease/id1, a root the engine reads only for its
            // language tables and music (vr_gamedir.cpp ownedRoots, VR_LoadOwnedLocalization), never mounted.
            var rereleaseId1 = Path.Combine(e.Root, "id1");
            if (e.Folder is "dopa" or "mg1" or "mg3" && Directory.Exists(rereleaseId1) &&
                !string.Equals(Path.GetFullPath(rereleaseId1).TrimEnd('\\'), Path.GetFullPath(id1).TrimEnd('\\'), StringComparison.OrdinalIgnoreCase))
            {
                foreach (var pak in Directory.EnumerateFiles(rereleaseId1, "pak*.pak").Order(StringComparer.OrdinalIgnoreCase))
                {
                    Add("rerelease/id1/" + Path.GetFileName(pak).ToLowerInvariant(), pak, "language");
                }
            }
        }

        // The official campaigns' own soundtracks (questfovmusic, 2026-11-27: Scourge of Armagon played Quake's tracks on the
        // Quest). The PC game reads <folder>/music/trackNN.<ext> where the search path has the folder, else in place from the
        // owned rerelease (vr_music.cpp findInFolder); the original's expansion folders have no music, the rerelease's do.
        // On the headset they go to <folder>/music: found through the search path, or the base dir as an owned root.
        foreach (var folder in CampaignMusicFolders.Where(folders.Contains))
        {
            if (CampaignMusicDir(folder, MusicRoots(quakeDir, installDir, storeRoots)) is not { } dir)
            {
                continue;
            }
            foreach (var track in Directory.EnumerateFiles(dir).Order(StringComparer.OrdinalIgnoreCase))
            {
                if (MusicFormats.Contains(Path.GetExtension(track), StringComparer.OrdinalIgnoreCase))
                {
                    Add($"{folder}/music/" + Path.GetFileName(track).ToLowerInvariant(), track, "music");
                }
            }
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
            foreach (var (devicePath, source, what) in ModCopyFiles(copy))
            {
                Add(devicePath, source, what);
            }
        }

        AddRelit(installDir, files);
        AddHulls(installDir, files);
        if (DataStamp(record.Version) is { } stamp)
        {
            files[DataVersionPath] = new QuestFile(DataVersionPath, stamp, new FileInfo(stamp).Length, "version stamp");
        }
        return [.. files.Values.OrderBy(f => f.DevicePath, StringComparer.Ordinal)];
    }

    /// <summary>The official campaigns with soundtracks of their own (vr_music.cpp campaignFolders, Quake's id1 above).</summary>
    static readonly string[] CampaignMusicFolders = ["hipnotic", "rogue", "dopa", "mg1", "mg3"];

    /// <summary>Where a campaign's music may be on this PC, in the PC game's order: the player's Quake and the install (the
    /// search path's folders), then the rereleases (the owned roots the PC game reads in place).</summary>
    static IEnumerable<string> MusicRoots(string quakeDir, string installDir, IReadOnlyList<string> storeRoots)
    {
        yield return quakeDir;
        yield return installDir;
        foreach (var root in storeRoots.Prepend(quakeDir))
        {
            if (Directory.Exists(root) && QuakeDetector.Describe(QuakeStore.Manual, "", root).RereleaseRoot is { } rerelease)
            {
                yield return rerelease;
            }
        }
    }

    /// <summary>The first <c>&lt;root&gt;/&lt;folder&gt;/music</c> with a track the Quest build plays, or null.</summary>
    static string? CampaignMusicDir(string folder, IEnumerable<string> roots)
    {
        foreach (var root in roots)
        {
            var dir = Path.Combine(root, folder, "music");
            try
            {
                if (Directory.Exists(dir) && Directory.EnumerateFiles(dir).Any(f => MusicFormats.Contains(Path.GetExtension(f), StringComparer.OrdinalIgnoreCase)))
                {
                    return dir;
                }
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
            }
        }
        return null;
    }

    /// <summary>The game data's version stamp on the headset: the install's version, which the game on the Quest compares
    /// with its own (vr_android.cpp VR_AndroidDataCheck): an app of another version than its data (a newer APK sideloaded,
    /// an older install's files) says "run Setup's Quest step again" instead of a hub that fails to load.</summary>
    public const string DataVersionPath = "quakevr/data_version.txt";

    /// <summary>The stamp's text (the install's version: "1.2.0-beta5", a development package's "1.2.0-beta5-dev (...)";
    /// the game compares its first word, "-dev" dropped) in a file of its own (the temporary folder, one per version), or
    /// null without a version ("unverified": a package without a manifest).</summary>
    static string? DataStamp(string version)
    {
        version = version.Trim();
        if (version.Length == 0 || !char.IsAsciiDigit(version[0]))
        {
            return null;
        }
        var dir = Path.Combine(Path.GetTempPath(), "QuakeVR-Setup");
        Directory.CreateDirectory(dir);
        var name = string.Concat(version.Select(c => char.IsAsciiLetterOrDigit(c) || c is '.' or '-' ? c : '_'));
        var path = Path.Combine(dir, $"data_version-{name}.txt");
        if (!File.Exists(path) || File.ReadAllText(path) != version + "\n")
        {
            File.WriteAllText(path, version + "\n");
        }
        return path;
    }

    /// <summary>A mod copy's files for the headset (device path, PC source, its folder's name), the player's own left out.
    /// <see cref="ModCopy.Folder"/> is the copy's whole PC path (e.g. ...\Quake\dwellv2p2): on the headset it is that
    /// folder's name, lower case as id1's (the real-headset test sent "C:\Program Files (x86)\...\dwellv2p2/gfx/...").</summary>
    public static IEnumerable<(string DevicePath, string Source, string What)> ModCopyFiles(ModCopy copy)
    {
        var dir = Path.Combine(copy.Root, copy.Folder);
        var name = Path.GetFileName(dir.TrimEnd('\\', '/')).ToLowerInvariant();
        foreach (var file in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories).Order(StringComparer.OrdinalIgnoreCase))
        {
            var rel = PathUtil.ToRelative(dir, file).Replace('\\', '/');
            if (!PlayerFile(rel) && !PcOnly(name + "/" + rel))
            {
                yield return ($"{name}/{rel}", file, name);
            }
        }
    }

    /// <summary>
    /// The maps the PC game relit (the preparation run, Graphics > Relighting): the Quest cannot relight (no light tool on
    /// Android), and it loads the same copies (VR_ModelFile: <c>relit_custom/&lt;game&gt;/maps/&lt;map&gt;.bsp</c> through
    /// the search path, so <c>quakevr/relit_custom/</c> on the headset too; used while its .relight names the map's bytes,
    /// which are the same paks). Only the copies with their note, and only for the games the headset gets, under the game
    /// folder's name as the headset has it (Android's names are case-sensitive: "ID1" relit on the PC is "id1" there).
    /// </summary>
    static void AddRelit(string installDir, Dictionary<string, QuestFile> files)
    {
        var games = files.Keys.Select(p => p[..p.IndexOf('/')]).Distinct(StringComparer.OrdinalIgnoreCase)
            .ToDictionary(g => g, StringComparer.OrdinalIgnoreCase);
        foreach (var (game, mapsDir) in FirstStartRelight.RelitGames(installDir))
        {
            if (!games.TryGetValue(game, out var deviceGame))
            {
                continue;
            }
            foreach (var note in Directory.EnumerateFiles(mapsDir, "*.relight", SearchOption.AllDirectories).Order(StringComparer.OrdinalIgnoreCase))
            {
                foreach (var ext in RelitExtensions)
                {
                    var source = Path.ChangeExtension(note, ext);
                    var rel = PathUtil.ToRelative(mapsDir, source).Replace('\\', '/');
                    var devicePath = $"{FirstStartRelight.RelitFolder}/{deviceGame}/maps/{rel}";
                    if (!files.ContainsKey(devicePath) && File.Exists(source))
                    {
                        files[devicePath] = new QuestFile(devicePath, source, new FileInfo(source).Length, "relit");
                    }
                }
            }
        }
    }

    /// <summary>The headset's disk cache, under its files folder: the engine's <c>&lt;base&gt;/cache</c> (vr_files.cpp
    /// baseCacheDir; the Quest's base is the files folder, its vr_cache_dir empty).</summary>
    public const string DeviceCacheFolder = "cache";

    /// <summary>
    /// The collision the preparation run made for Quake's and the mission packs' maps (its hulls' step, listed in
    /// <see cref="GamePreparation.HullListPath"/>): the hull trees and brushes, the same bytes on the PC and the Quest
    /// (keyed by the map's world and the trees' version, never the exe: prebuiltcache), into the headset's own disk cache
    /// (<c>cache/hulls/&lt;build&gt;/</c>), where its game reads them as files it made itself (a build of other trees
    /// prunes that folder; a file amiss is made again). Only the listed files that are still there.
    /// </summary>
    static void AddHulls(string installDir, Dictionary<string, QuestFile> files)
    {
        var list = GamePreparation.HullListPath(installDir);
        if (!File.Exists(list))
        {
            return;
        }
        foreach (var line in File.ReadLines(list))
        {
            var parts = line.Split('\t', 2);
            if (parts.Length != 2)
            {
                continue;
            }
            var rel = parts[0].Trim().Replace('\\', '/');
            var source = parts[1].Trim();
            // (a cache path the list could only have from the game: hulls/<build>/<file>, nothing above it)
            var segments = rel.Split('/');
            if (segments.Length != 3 || segments[0] != "hulls" || segments.Any(s => s.Length == 0 || s is "." or ".." || s.IndexOfAny([':', '\\']) >= 0))
            {
                continue;
            }
            var devicePath = $"{DeviceCacheFolder}/{rel}";
            if (!files.ContainsKey(devicePath) && File.Exists(source))
            {
                files[devicePath] = new QuestFile(devicePath, source, new FileInfo(source).Length, "collision");
            }
        }
    }
}
