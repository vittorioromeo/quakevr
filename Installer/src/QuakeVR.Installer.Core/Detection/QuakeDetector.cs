using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Detection;

public enum QuakeStore
{
    Steam,
    Gog,
    Epic,
    Manual,
}

/// <summary>
/// One Quake found on the PC. <see cref="BaseDir"/> is the first <c>-basedir</c> the shortcuts pass (it holds
/// <c>id1\pak0.pak</c>): the original game's folder when there is one (what the relight and the author use), else the
/// rerelease's. The engine finds the rerelease next to it by itself (<c>ownedRoots</c>: <c>&lt;base&gt;\rerelease</c>
/// and <c>&lt;base&gt;\..\rerelease</c>) for Dimension of the Past and the MachineGames episodes.
/// </summary>
public sealed record QuakeInstall(
    QuakeStore Store,
    string Name,
    string InstallDir,
    Id1Info Original,
    Id1Info Rerelease,
    string? RereleaseRoot,
    string? MusicDir,
    string? ExistingQuakeVr)
{
    public bool Playable => Original.Playable || Rerelease.Playable;

    public string? BaseDir => Original.Playable ? InstallDir : Rerelease.Playable ? RereleaseRoot : null;

    public string Summary =>
        Original.Playable && Rerelease.Playable ? "Original + 2021 rerelease" :
        Original.Playable ? "Original" :
        Rerelease.Playable ? "2021 rerelease" :
        Original.Kind != Id1Kind.Missing ? Original.Detail : Rerelease.Kind != Id1Kind.Missing ? Rerelease.Detail : "no Quake data";
}

/// <summary>Finds Quake in Steam, GOG and Epic (both the original and the 2021 rerelease), and checks a folder the
/// player picks by hand. Never writes anything.</summary>
public sealed class QuakeDetector(ISystemProbe probe, string? epicManifestsDir = null)
{
    public SteamLocator Steam { get; } = new(probe);
    public GogLocator Gog { get; } = new(probe);
    public EpicLocator Epic { get; } = new(probe, epicManifestsDir);

    public IReadOnlyList<QuakeInstall> FindAll()
    {
        var found = new List<QuakeInstall>();
        if (Steam.FindApp(SteamLocator.QuakeAppId) is { } steam)
        {
            found.Add(Describe(QuakeStore.Steam, "Steam", steam));
        }
        if (Gog.Find(GogLocator.OriginalId) is { } gogOriginal)
        {
            found.Add(Describe(QuakeStore.Gog, "GOG", gogOriginal));
        }
        if (Gog.Find(GogLocator.EnhancedId) is { } gogEnhanced && !found.Any(q => PathUtil.SamePath(q.InstallDir, gogEnhanced)))
        {
            found.Add(Describe(QuakeStore.Gog, "GOG (enhanced)", gogEnhanced));
        }
        foreach (var epic in Epic.FindQuakeGames())
        {
            if (!found.Any(q => PathUtil.SamePath(q.InstallDir, epic.InstallLocation)))
            {
                found.Add(Describe(QuakeStore.Epic, "Epic Games", epic.InstallLocation));
            }
        }
        return found;
    }

    /// <summary>A folder picked by hand: the Quake folder itself, its <c>id1</c>, or a <c>rerelease</c> folder.</summary>
    public static QuakeInstall DescribeManual(string picked)
    {
        var dir = PathUtil.TryNormalize(picked) ?? picked;
        if (Path.GetFileName(dir).Equals("id1", StringComparison.OrdinalIgnoreCase) && Path.GetDirectoryName(dir) is { } up)
        {
            dir = up;
        }
        // A rerelease folder picked inside an install that also has the original: use the install.
        if (Path.GetFileName(dir).Equals("rerelease", StringComparison.OrdinalIgnoreCase) && Path.GetDirectoryName(dir) is { } parent &&
            QuakeData.Inspect(parent).Playable)
        {
            dir = parent;
        }
        return Describe(QuakeStore.Manual, "Folder", dir);
    }

    public static QuakeInstall Describe(QuakeStore store, string name, string dir)
    {
        var original = QuakeData.Inspect(dir);
        string? rereleaseRoot = Path.Combine(dir, "rerelease");
        Id1Info rerelease;
        if (original.Kind == Id1Kind.Rerelease)
        {
            // GOG's enhanced edition and some Epic installs: the rerelease at the root.
            rerelease = original;
            original = new Id1Info(Id1Kind.Missing, "no original game data");
            rereleaseRoot = dir;
        }
        else
        {
            rerelease = QuakeData.Inspect(rereleaseRoot);
            if (rerelease.Kind == Id1Kind.Missing)
            {
                rereleaseRoot = null;
            }
        }
        var music = (rereleaseRoot is not null ? QuakeData.FindMusic(rereleaseRoot) : null) ?? QuakeData.FindMusic(dir);
        // A zip install of Quake VR (layout A): the engine and the quakevr folder copied into the Quake folder.
        var existing = File.Exists(Path.Combine(dir, "ironwail.exe")) || Directory.Exists(Path.Combine(dir, "quakevr"))
            ? Path.Combine(dir, "quakevr") : null;
        return new QuakeInstall(store, name, dir, original, rerelease, rereleaseRoot, music, existing);
    }
}
