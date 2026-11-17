using System.Text.Json;
using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Detection;

/// <summary>
/// Where the Linux launchers keep their data (docs/vr-port/INSTALLER.md, "Cross-platform"). Quake has no native Linux
/// build: every store installs the Windows game (Steam through Proton, GOG and Epic through Heroic, Lutris through Wine),
/// so the files are the same as on Windows, only found elsewhere.
/// </summary>
public static class LinuxStores
{
    /// <summary>Steam's roots, in the order Steam itself prefers: ~/.steam/steam (a link to the real one),
    /// ~/.local/share/Steam (native), the Flatpak's, the Snap's.</summary>
    public static IEnumerable<string> SteamRoots(ISystemProbe probe)
    {
        if (probe.GetFolder(KnownFolder.UserProfile) is not { } home)
        {
            yield break;
        }
        yield return Path.Combine(home, ".steam", "steam");
        yield return Path.Combine(home, ".steam", "root");
        yield return Path.Combine(probe.GetFolder(KnownFolder.LocalAppData) ?? Path.Combine(home, ".local", "share"), "Steam");
        yield return Path.Combine(home, ".var", "app", "com.valvesoftware.Steam", ".local", "share", "Steam");
        yield return Path.Combine(home, ".var", "app", "com.valvesoftware.Steam", "data", "Steam");
        yield return Path.Combine(home, "snap", "steam", "common", ".local", "share", "Steam");
    }

    /// <summary>Heroic's config folders: the native package's and the Flatpak's.</summary>
    public static IEnumerable<string> HeroicConfigs(ISystemProbe probe)
    {
        if (probe.GetFolder(KnownFolder.ConfigHome) is { } config)
        {
            yield return Path.Combine(config, "heroic");
        }
        if (probe.GetFolder(KnownFolder.UserProfile) is { } home)
        {
            yield return Path.Combine(home, ".var", "app", "com.heroicgameslauncher.hgl", "config", "heroic");
        }
    }

    /// <summary>
    /// GOG games installed by Heroic: <c>&lt;heroic&gt;/gog_store/installed.json</c>,
    /// <c>{"installed": [{"appName": "1435828198", "install_path": "/home/x/Games/Heroic/Quake", ...}]}</c>.
    /// </summary>
    public static string? HeroicGog(ISystemProbe probe, string gameId)
    {
        foreach (var heroic in HeroicConfigs(probe))
        {
            var file = Path.Combine(heroic, "gog_store", "installed.json");
            foreach (var game in ReadArray(file, "installed"))
            {
                if (Str(game, "appName") == gameId && Str(game, "install_path") is { } p && Directory.Exists(p))
                {
                    return p;
                }
            }
        }
        return null;
    }

    /// <summary>
    /// Epic games installed by Heroic (its own Legendary) or by Legendary itself: <c>installed.json</c>, an object
    /// keyed by app name, <c>{"Quake": {"title": "Quake", "install_path": "/home/x/Games/Heroic/Quake", ...}}</c>.
    /// </summary>
    public static IReadOnlyList<EpicLocator.EpicGame> LegendaryQuakeGames(ISystemProbe probe)
    {
        var files = new List<string>();
        files.AddRange(HeroicConfigs(probe).Select(h => Path.Combine(h, "legendaryConfig", "legendary", "installed.json")));
        if (probe.GetFolder(KnownFolder.ConfigHome) is { } config)
        {
            files.Add(Path.Combine(config, "legendary", "installed.json"));
        }
        var games = new List<EpicLocator.EpicGame>();
        foreach (var file in files)
        {
            try
            {
                if (!File.Exists(file))
                {
                    continue;
                }
                using var doc = JsonDocument.Parse(File.ReadAllText(file));
                if (doc.RootElement.ValueKind != JsonValueKind.Object)
                {
                    continue;
                }
                foreach (var app in doc.RootElement.EnumerateObject())
                {
                    if (app.Value.ValueKind == JsonValueKind.Object && Str(app.Value, "title") is { } title &&
                        title.Contains("quake", StringComparison.OrdinalIgnoreCase) &&
                        Str(app.Value, "install_path") is { } p && Directory.Exists(p) && !games.Any(g => PathUtil.SamePath(g.InstallLocation, p)))
                    {
                        games.Add(new EpicLocator.EpicGame(title, p));
                    }
                }
            }
            catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
            {
                // A broken file is skipped, as the Epic manifests are.
            }
        }
        return games;
    }

    /// <summary>
    /// Lutris: its game configs (<c>~/.local/share/lutris/games/*.yml</c>; before Lutris 0.5.13
    /// <c>~/.config/lutris/games</c>), named <c>&lt;slug&gt;-&lt;id&gt;.yml</c>. A "quake" one's <c>exe:</c> (or
    /// <c>working_dir:</c>) points into the game; the Quake folder is the nearest one up from it with an id1 or a
    /// rerelease folder. (The game list itself is SQLite, pga.db: not read, it would need a package.)
    /// </summary>
    public static IReadOnlyList<string> LutrisQuakeDirs(ISystemProbe probe)
    {
        var dirs = new List<string>();
        var configs = new List<string>();
        if (probe.GetFolder(KnownFolder.LocalAppData) is { } data)
        {
            configs.Add(Path.Combine(data, "lutris", "games"));
        }
        if (probe.GetFolder(KnownFolder.ConfigHome) is { } config)
        {
            configs.Add(Path.Combine(config, "lutris", "games"));
        }
        foreach (var folder in configs.Where(Directory.Exists))
        {
            IEnumerable<string> files;
            try
            {
                files = Directory.EnumerateFiles(folder, "*.yml").Where(f => Path.GetFileName(f).Contains("quake", StringComparison.OrdinalIgnoreCase)).ToList();
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                continue;
            }
            foreach (var file in files)
            {
                foreach (var path in YamlPaths(file))
                {
                    if (FindQuakeUp(path) is { } q && !dirs.Any(d => PathUtil.SamePath(d, q)))
                    {
                        dirs.Add(q);
                    }
                }
            }
        }
        return dirs;
    }

    /// <summary>The <c>exe:</c> and <c>working_dir:</c> values of a Lutris game config (plain lines; no YAML library).</summary>
    static IEnumerable<string> YamlPaths(string file)
    {
        string[] lines;
        try
        {
            lines = File.ReadAllLines(file);
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            yield break;
        }
        foreach (var raw in lines)
        {
            var line = raw.Trim();
            foreach (var key in new[] { "exe:", "working_dir:", "game_path:" })
            {
                if (line.StartsWith(key, StringComparison.Ordinal))
                {
                    var value = line[key.Length..].Trim().Trim('"', '\'');
                    if (Path.IsPathRooted(value))
                    {
                        yield return value;
                    }
                }
            }
        }
    }

    /// <summary>The path itself or the nearest folder above it (four levels at most) holding id1 or rerelease.</summary>
    public static string? FindQuakeUp(string path)
    {
        var dir = Directory.Exists(path) ? path : Path.GetDirectoryName(path);
        for (var i = 0; dir is not null && i < 5; ++i, dir = Path.GetDirectoryName(dir))
        {
            if (Directory.Exists(Path.Combine(dir, "id1")) || Directory.Exists(Path.Combine(dir, "rerelease")))
            {
                return dir;
            }
        }
        return null;
    }

    static IEnumerable<JsonElement> ReadArray(string file, string property)
    {
        if (!File.Exists(file))
        {
            return [];
        }
        try
        {
            using var doc = JsonDocument.Parse(File.ReadAllText(file));
            return doc.RootElement.ValueKind == JsonValueKind.Object && doc.RootElement.TryGetProperty(property, out var a) && a.ValueKind == JsonValueKind.Array
                ? [.. a.EnumerateArray().Select(e => e.Clone())]
                : [];
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            return [];
        }
    }

    static string? Str(JsonElement e, string name) =>
        e.TryGetProperty(name, out var v) && v.ValueKind == JsonValueKind.String && v.GetString() is { Length: > 0 } s ? s : null;
}
