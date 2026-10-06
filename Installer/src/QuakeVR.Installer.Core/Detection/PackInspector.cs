using System.Reflection;
using System.Text.RegularExpressions;

namespace QuakeVR.Installer.Core.Detection;

/// <summary>The engine's pack states (<c>inspectPack</c>): 0 missing, 1 available, 2 incomplete or corrupt.</summary>
public enum PackStatus
{
    Missing = 0,
    Ready = 1,
    Incomplete = 2,
}

public sealed record PackResult(PackStatus Status, string Detail);

/// <summary>The required files of each official pack, compiled into this assembly from the engine's own lists
/// (Quake/vr/vr_pack_*.inc): the installer and the game check the same names.</summary>
public static partial class PackLists
{
    static readonly Dictionary<string, IReadOnlyList<string>> Cache = new(StringComparer.OrdinalIgnoreCase);
    static readonly Lock CacheLock = new();

    public static IReadOnlyList<string> Resources(string game)
    {
        lock (CacheLock)
        {
            if (Cache.TryGetValue(game, out var list))
            {
                return list;
            }
            using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream($"QuakeVR.Packs.vr_pack_{game.ToLowerInvariant()}.inc")
                ?? throw new InvalidOperationException($"no resource list for {game}");
            using var reader = new StreamReader(stream);
            var names = new List<string>();
            while (reader.ReadLine() is { } line)
            {
                var m = Entry().Match(line);
                if (m.Success && !line.TrimStart().StartsWith("//", StringComparison.Ordinal))
                {
                    names.Add(m.Groups[1].Value);
                }
            }
            Cache[game] = names;
            return names;
        }
    }

    [GeneratedRegex("^\\s*\"([^\"]+)\"")]
    private static partial Regex Entry();
}

/// <summary>
/// Whether a pack's required files are all present and intact in the packs (pak0.pak, pak1.pak, ...) or loose files of
/// <c>&lt;root&gt;\&lt;game&gt;</c> over the given roots: a port of the engine's <c>inspectPack</c>. A folder with none
/// of the pack's own data (only an extracted texture pack's <c>textures\</c>) reads as missing, not incomplete.
/// </summary>
public static class PackInspector
{
    public static PackResult Inspect(string game, IReadOnlyList<string> resources, IEnumerable<string> roots)
    {
        var found = new bool[resources.Count];
        var gameData = false;
        foreach (var root in roots)
        {
            var folder = Path.Combine(root, game);
            for (var pak = 0; ; ++pak)
            {
                var path = Path.Combine(folder, $"pak{pak}.pak");
                if (!File.Exists(path))
                {
                    break;
                }
                gameData = true;
                try
                {
                    using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 16);
                    var entries = PakFile.TryReadDirectory(file);
                    if (entries is null)
                    {
                        return new(PackStatus.Incomplete, $"corrupt archive {path}");
                    }
                    foreach (var entry in entries)
                    {
                        for (var i = 0; i < resources.Count; ++i)
                        {
                            if (!string.Equals(entry.Name, resources[i], StringComparison.Ordinal))
                            {
                                continue;
                            }
                            var bytes = new byte[entry.Length];
                            file.Position = entry.Offset;
                            if (file.ReadAtLeast(bytes, entry.Length, throwOnEndOfStream: false) != entry.Length ||
                                !ResourceValidator.IsValid(bytes, entry.Name))
                            {
                                return new(PackStatus.Incomplete, $"invalid or truncated {entry.Name} in {path}");
                            }
                            found[i] = true;
                        }
                    }
                }
                catch (IOException e)
                {
                    return new(PackStatus.Incomplete, $"cannot read {path}: {e.Message}");
                }
            }
            // An extracted installation also counts, when every required file is there.
            for (var i = 0; i < resources.Count; ++i)
            {
                var path = Path.Combine(folder, resources[i].Replace('/', '\\'));
                if (!File.Exists(path))
                {
                    continue;
                }
                try
                {
                    if (!ResourceValidator.IsValid(File.ReadAllBytes(path), resources[i]))
                    {
                        return new(PackStatus.Incomplete, $"invalid or truncated {path}");
                    }
                }
                catch (IOException e)
                {
                    return new(PackStatus.Incomplete, $"cannot read {path}: {e.Message}");
                }
                found[i] = true;
                gameData = true;
            }
        }
        for (var i = 0; i < resources.Count; ++i)
        {
            if (!found[i])
            {
                return gameData
                    ? new(PackStatus.Incomplete, $"incomplete installation (missing {resources[i]})")
                    : new(PackStatus.Missing, "not installed");
            }
        }
        return new(PackStatus.Ready, $"{resources.Count} required files intact");
    }
}
