namespace QuakeVR.Installer.Core.Detection;

public enum ExpansionState
{
    NotFound,
    Ready,
    /// <summary>Some of its data is there but not all, or a file is damaged: the player should verify the game's files.</summary>
    Incomplete,
    /// <summary>Installed and intact, but the game cannot play it yet (mg1, mg3: decision 9).</summary>
    DetectedNotSupported,
}

public sealed record ExpansionInfo(string Folder, string Title, ExpansionState State, string? Root, string Detail);

/// <summary>
/// The official expansions' status, by the engine's rules (Quake/vr/vr_gamedir.cpp): Scourge of Armagon and
/// Dissolution of Eternity are looked for in the base dirs (every one the shortcuts pass); Dimension of the Past and
/// the MachineGames episodes in the owned roots (<c>ownedRoots</c>: Steam's rerelease, GOG's enhanced edition, Epic's,
/// then each base dir's <c>..\rerelease</c>, <c>rerelease</c> and the base itself), highest priority first, a root
/// whose folder holds none of the pack's data skipped. The engine stays the authority (<c>vr_pack_status</c>,
/// <c>vr_campaign_status</c>); this only reports.
/// </summary>
public static class ExpansionDetector
{
    public sealed record Campaign(string Folder, string Title, bool NativeReady, bool InBaseDirs);

    /// <summary>The engine's <c>campaigns[]</c> table, less id1.</summary>
    public static readonly IReadOnlyList<Campaign> Campaigns =
    [
        new("hipnotic", "Scourge of Armagon", true, true),
        new("rogue", "Dissolution of Eternity", true, true),
        new("dopa", "Dimension of the Past", true, false),
        new("mg1", "Dimension of the Machine", false, false),
        new("mg3", "Dawn of the Machine", false, false),
    ];

    /// <summary>The store roots of <c>ownedRoots</c>, lowest priority first.</summary>
    public static IReadOnlyList<string> StoreRoots(QuakeDetector detector)
    {
        var roots = new List<string>();
        if (detector.Steam.FindApp(SteamLocator.QuakeAppId) is { } steam)
        {
            roots.Add(Path.Combine(steam, "rerelease"));
        }
        if (detector.Gog.Find(GogLocator.EnhancedId) is { } gog)
        {
            roots.Add(gog);
        }
        foreach (var epic in detector.Epic.FindQuakeGames())
        {
            var rerelease = Path.Combine(epic.InstallLocation, "rerelease");
            if (File.Exists(Path.Combine(rerelease, "id1", "pak0.pak")))
            {
                roots.Add(rerelease);
            }
            else if (File.Exists(Path.Combine(epic.InstallLocation, "id1", "pak0.pak")))
            {
                roots.Add(epic.InstallLocation);
            }
        }
        return roots;
    }

    /// <summary>The engine's <c>ownedRoots()</c> for these base dirs, lowest priority first.</summary>
    public static IReadOnlyList<string> OwnedRoots(IReadOnlyList<string> storeRoots, IReadOnlyList<string> baseDirs)
    {
        var roots = new List<string>(storeRoots);
        foreach (var b in baseDirs)
        {
            roots.Add(Path.GetFullPath(Path.Combine(b, "..", "rerelease")));
            roots.Add(Path.Combine(b, "rerelease"));
            roots.Add(b);
        }
        return roots;
    }

    /// <param name="baseDirs">The -basedirs in order: the Quake folder, then the Quake VR folder.</param>
    /// <param name="storeRoots">From <see cref="StoreRoots"/>.</param>
    public static IReadOnlyList<ExpansionInfo> Detect(IReadOnlyList<string> baseDirs, IReadOnlyList<string> storeRoots)
    {
        var owned = OwnedRoots(storeRoots, baseDirs);
        var result = new List<ExpansionInfo>();
        foreach (var c in Campaigns)
        {
            var resources = PackLists.Resources(c.Folder);
            PackResult status;
            string? root = null;
            if (c.InBaseDirs)
            {
                status = PackInspector.Inspect(c.Folder, resources, baseDirs);
                // Where its data is (for the report): the first base dir with a pak or one of its files.
                root = baseDirs.FirstOrDefault(b => File.Exists(Path.Combine(b, c.Folder, "pak0.pak")) ||
                                                    resources.Any(f => File.Exists(Path.Combine(b, c.Folder, f))));
            }
            else
            {
                status = new PackResult(PackStatus.Missing, "not installed");
                for (var r = owned.Count - 1; r >= 0; --r)
                {
                    if (!Directory.Exists(Path.Combine(owned[r], c.Folder)))
                    {
                        continue;
                    }
                    status = PackInspector.Inspect(c.Folder, resources, [owned[r]]);
                    if (status.Status == PackStatus.Missing)
                    {
                        continue; // none of its data (a texture pack's folder): a lower root's copy counts
                    }
                    root = owned[r];
                    break; // a damaged higher-priority copy is reported, never masked by another release
                }
            }
            var state = status.Status switch
            {
                PackStatus.Ready => c.NativeReady ? ExpansionState.Ready : ExpansionState.DetectedNotSupported,
                PackStatus.Incomplete => ExpansionState.Incomplete,
                _ => ExpansionState.NotFound,
            };
            var detail = state switch
            {
                ExpansionState.Ready => c.Folder == "dopa" ? "ready (single player)" : "ready",
                ExpansionState.DetectedNotSupported => "detected, not yet supported (native support in progress)",
                ExpansionState.Incomplete => status.Detail + ": verify the game's files in your store",
                _ => "not installed",
            };
            result.Add(new ExpansionInfo(c.Folder, c.Title, state, root is null ? null : PathUtil.TryNormalize(root), detail));
        }
        return result;
    }
}
