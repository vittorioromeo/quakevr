using System.Text.Json;
using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Detection;

/// <summary>Steam: the client's folder from the registry, every library from libraryfolders.vdf, and an app's folder
/// from its appmanifest_&lt;id&gt;.acf (as the engine's Steam_FindGame, Quake/steam.c).</summary>
public sealed class SteamLocator(ISystemProbe probe)
{
    public const string QuakeAppId = "2310";
    public const string SteamVrAppId = "250820";

    public string? FindSteamRoot()
    {
        string?[] candidates =
        [
            probe.GetRegistryString(@"HKCU\Software\Valve\Steam", "SteamPath"),
            probe.GetRegistryString(@"HKLM\SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
            probe.GetRegistryString(@"HKLM\SOFTWARE\Valve\Steam", "InstallPath"),
            probe.GetFolder(KnownFolder.ProgramFilesX86) is { } pf ? Path.Combine(pf, "Steam") : null,
        ];
        foreach (var c in candidates)
        {
            if (c is not null && PathUtil.TryNormalize(c) is { } p && Directory.Exists(Path.Combine(p, "steamapps")))
            {
                return PathUtil.RealCase(p);
            }
        }
        return null;
    }

    /// <summary>The Steam folder itself, then every other library it lists (existing folders only).</summary>
    public IReadOnlyList<string> FindLibraries()
    {
        var root = FindSteamRoot();
        if (root is null)
        {
            return [];
        }
        var libraries = new List<string> { root };
        foreach (var vdfPath in new[] { Path.Combine(root, "steamapps", "libraryfolders.vdf"), Path.Combine(root, "config", "libraryfolders.vdf") })
        {
            if (!File.Exists(vdfPath))
            {
                continue;
            }
            VdfObject vdf;
            try
            {
                vdf = Vdf.Parse(File.ReadAllText(vdfPath));
            }
            catch (IOException)
            {
                continue;
            }
            var folders = vdf.GetObject("libraryfolders") ?? vdf.GetObject("LibraryFolders");
            if (folders is null)
            {
                continue;
            }
            foreach (var (key, value) in folders.Entries)
            {
                // Current format: "0" { "path" "..." }. Older: "1" "D:\\SteamLibrary".
                var path = value switch
                {
                    VdfObject o => o.GetString("path"),
                    string s when key.All(char.IsDigit) => s,
                    _ => null,
                };
                if (path is not null && PathUtil.TryNormalize(path) is { } p && Directory.Exists(p) &&
                    !libraries.Any(l => PathUtil.SamePath(l, p)))
                {
                    libraries.Add(p);
                }
            }
        }
        return libraries;
    }

    /// <summary>The install folder of an app, or null when no library has it.</summary>
    public string? FindApp(string appId)
    {
        foreach (var library in FindLibraries())
        {
            var manifest = Path.Combine(library, "steamapps", $"appmanifest_{appId}.acf");
            if (!File.Exists(manifest))
            {
                continue;
            }
            try
            {
                var installDir = Vdf.Parse(File.ReadAllText(manifest)).GetObject("AppState")?.GetString("installdir");
                if (string.IsNullOrWhiteSpace(installDir))
                {
                    continue;
                }
                var dir = Path.Combine(library, "steamapps", "common", installDir);
                if (Directory.Exists(dir))
                {
                    return dir;
                }
            }
            catch (IOException)
            {
            }
        }
        return null;
    }
}

/// <summary>GOG Galaxy's registry entries (the engine's Sys_GetGOGQuakeDir / Sys_GetGOGQuakeEnhancedDir).</summary>
public sealed class GogLocator(ISystemProbe probe)
{
    public const string OriginalId = "1435828198";
    public const string EnhancedId = "1739637082";

    public string? Find(string gameId)
    {
        foreach (var key in new[] { $@"HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\{gameId}", $@"HKLM\SOFTWARE\GOG.com\Games\{gameId}" })
        {
            if (probe.GetRegistryString(key, "path") is { } p && PathUtil.TryNormalize(p) is { } dir && Directory.Exists(dir))
            {
                return dir;
            }
        }
        return null;
    }
}

/// <summary>The Epic Games Launcher's install manifests (<c>%ProgramData%\Epic\EpicGamesLauncher\Data\Manifests\*.item</c>,
/// JSON): every title whose DisplayName says Quake, with its InstallLocation (the engine's addEpicRoots).</summary>
public sealed class EpicLocator(ISystemProbe probe, string? manifestsDir = null)
{
    public sealed record EpicGame(string DisplayName, string InstallLocation);

    public string? ManifestsDir =>
        manifestsDir ?? (probe.GetFolder(KnownFolder.ProgramData) is { } pd
            ? Path.Combine(pd, "Epic", "EpicGamesLauncher", "Data", "Manifests") : null);

    public IReadOnlyList<EpicGame> FindQuakeGames()
    {
        var dir = ManifestsDir;
        if (dir is null || !Directory.Exists(dir))
        {
            return [];
        }
        var games = new List<EpicGame>();
        foreach (var file in Directory.EnumerateFiles(dir, "*.item"))
        {
            try
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(file));
                var root = doc.RootElement;
                var name = root.TryGetProperty("DisplayName", out var n) ? n.GetString() : null;
                var location = root.TryGetProperty("InstallLocation", out var l) ? l.GetString() : null;
                if (name is not null && location is not null && name.Contains("quake", StringComparison.OrdinalIgnoreCase) &&
                    PathUtil.TryNormalize(location) is { } loc && Directory.Exists(loc))
                {
                    games.Add(new EpicGame(name, loc));
                }
            }
            catch (Exception e) when (e is JsonException or IOException or InvalidOperationException)
            {
                // A broken manifest is skipped, as the engine does.
            }
        }
        return games;
    }
}
