using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace QuakeVR.Installer.Core.Settings;

// Setup's settings page (docs/vr-port/INSTALLER.md, "Settings page"): the most common settings, set after installing
// and before the first start. Which settings, and in what groups, is Misc/installer/settings_page.txt; everything else
// about each (label, help, kind, range, choices, default) is the game's own, in quakevr\settings_page.json (the
// manifest, made from the game's menu rows by Misc/installer/settings_page.py and shipped in the package).
// Setup writes only the settings changed on the page, to quakevr\installer_settings.cfg, which the game runs once
// after the saved config (vr_installer_settings in quake.rc; its "// stamp" line is kept in vr_installer_applied).

public sealed record SettingChoice(string Value, string Label, string? Help);

public sealed class SettingDef
{
    public required string Cvar { get; init; }
    public required string Category { get; init; }
    public string Subcategory { get; init; } = "";
    /// <summary>toggle (0/1), choice, slider, or apply (a button: a preset applied once, not saved by the game).</summary>
    public required string Kind { get; init; }
    public required string Label { get; init; }
    public string Help { get; init; } = "";
    /// <summary>Where it is in the game's menus ("VR Settings > Comfort, Teleport and Turning > Vignette").</summary>
    public string Path { get; init; } = "";
    public required string Default { get; init; }
    public double Min { get; init; }
    public double Max { get; init; } = 1;
    public double Step { get; init; } = 0.1;
    public string Format { get; init; } = "%g";
    public double? HardMin { get; init; }
    public double? HardMax { get; init; }
    public IReadOnlyList<SettingChoice> Choices { get; init; } = [];

    public bool IsApply => Kind == SettingKinds.Apply;
}

public static class SettingKinds
{
    public const string Toggle = "toggle";
    public const string Choice = "choice";
    public const string Slider = "slider";
    public const string Apply = "apply";
    public static readonly string[] All = [Toggle, Choice, Slider, Apply];
}

public static class SettingsManifest
{
    public const string FileName = "settings_page.json";
    public const int Schema = 1;

    /// <summary>The manifest's settings, in the page's order. Throws InvalidDataException when it can't be used.</summary>
    public static IReadOnlyList<SettingDef> Load(string path)
    {
        using var doc = JsonDocument.Parse(File.ReadAllText(path));
        var root = doc.RootElement;
        if (!root.TryGetProperty("schema", out var schema) || schema.GetInt32() != Schema)
        {
            throw new InvalidDataException($"{path}: not a schema {Schema} settings manifest");
        }
        var list = new List<SettingDef>();
        foreach (var s in root.GetProperty("settings").EnumerateArray())
        {
            string Str(string k, string fallback = "") => s.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString()! : fallback;
            double? Num(string k) => s.TryGetProperty(k, out var v) && v.ValueKind == JsonValueKind.Number ? v.GetDouble() : null;
            var kind = Str("kind");
            if (!SettingKinds.All.Contains(kind))
            {
                continue; // a kind this Setup doesn't know (a newer manifest): left off the page
            }
            var choices = new List<SettingChoice>();
            if (s.TryGetProperty("choices", out var cs))
            {
                foreach (var c in cs.EnumerateArray())
                {
                    choices.Add(new SettingChoice(c.GetProperty("value").GetString()!, c.GetProperty("label").GetString()!,
                        c.TryGetProperty("help", out var h) ? h.GetString() : null));
                }
            }
            list.Add(new SettingDef
            {
                Cvar = Str("cvar"),
                Category = Str("category", "Settings"),
                Subcategory = Str("subcategory"),
                Kind = kind,
                Label = Str("label", Str("cvar")),
                Help = Str("help"),
                Path = Str("path"),
                Default = Str("default"),
                Min = Num("min") ?? 0,
                Max = Num("max") ?? 1,
                Step = Num("step") ?? 0.1,
                Format = Str("format", "%g"),
                HardMin = Num("hardmin"),
                HardMax = Num("hardmax"),
                Choices = choices,
            });
        }
        return list;
    }
}

public static class SettingValues
{
    /// <summary>Two values the same: as numbers when both are ("1" and "1.0"), else as text (the game's sameValue).</summary>
    public static bool Same(string? a, string? b)
    {
        a ??= "";
        b ??= "";
        if (double.TryParse(a, NumberStyles.Float, CultureInfo.InvariantCulture, out var x) &&
            double.TryParse(b, NumberStyles.Float, CultureInfo.InvariantCulture, out var y))
        {
            return (float)x == (float)y;
        }
        return a == b;
    }

    public static double Number(string? value, double fallback = 0) =>
        double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : fallback;

    /// <summary>A number as the game writes it ("45", "3.25").</summary>
    public static string Text(double value) => value.ToString("0.######", CultureInfo.InvariantCulture);

    /// <summary>A slider's value with its menu format ("%.2f", "%.1f s", "%.0f%%").</summary>
    public static string Format(string format, double value) =>
        Regex.Replace(format.Replace("%%", "\u0001"), @"%(?:\.(\d+))?[fgd]", m =>
            m.Value.EndsWith('g') ? value.ToString("0.####", CultureInfo.InvariantCulture) :
            value.ToString("F" + (m.Value.EndsWith('d') ? "0" : m.Groups[1].Success ? m.Groups[1].Value : "6"), CultureInfo.InvariantCulture))
        .Replace("\u0001", "%");
}

/// <summary>A saved config's settings: its `name "value"` lines (binds, aliases and comments left out).</summary>
public static class GameConfig
{
    public const string FileName = "ironwail.cfg";

    public static Dictionary<string, string> ReadValues(string path)
    {
        var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (!File.Exists(path))
        {
            return values;
        }
        foreach (var raw in File.ReadLines(path))
        {
            var line = raw.Trim();
            if (line.Length == 0 || line.StartsWith("//", StringComparison.Ordinal))
            {
                continue;
            }
            var space = line.IndexOfAny([' ', '\t']);
            if (space <= 0)
            {
                continue;
            }
            var name = line[..space];
            var rest = line[space..].Trim();
            string value;
            if (rest.StartsWith('"'))
            {
                var end = rest.IndexOf('"', 1);
                value = end > 0 ? rest[1..end] : rest[1..];
            }
            else
            {
                var end = rest.IndexOfAny([' ', '\t', ';']);
                value = end > 0 ? rest[..end] : rest;
            }
            values[name] = value;
        }
        return values;
    }
}

/// <summary>quakevr\installer_settings.cfg: what Setup's page changed, run once by the game.</summary>
public sealed class InstallerSettingsFile
{
    public const string FileName = "installer_settings.cfg";
    public const string StampTag = "// stamp ";
    /// <summary>The cvar the game keeps the last stamp it ran in (saved with its config).</summary>
    public const string AppliedCvar = "vr_installer_applied";

    public string? Stamp { get; init; }
    public List<KeyValuePair<string, string>> Values { get; init; } = [];

    public static InstallerSettingsFile? Read(string path)
    {
        if (!File.Exists(path))
        {
            return null;
        }
        string? stamp = null;
        foreach (var line in File.ReadLines(path))
        {
            if (line.StartsWith(StampTag, StringComparison.Ordinal))
            {
                stamp = line[StampTag.Length..].Trim();
                break;
            }
        }
        return new InstallerSettingsFile
        {
            Stamp = stamp,
            Values = GameConfig.ReadValues(path).ToList(),
        };
    }

    public static string NewStamp(DateTime now) =>
        $"{now:yyyyMMdd-HHmmss}-{Convert.ToHexString(RandomNumberGenerator.GetBytes(3)).ToLowerInvariant()}";

    public string Text()
    {
        var sb = new StringBuilder();
        sb.Append("// Quake VR: the settings changed on Setup's settings page (docs/vr-port/INSTALLER.md, \"Settings page\").\n");
        sb.Append("// The game runs this file once, after your saved settings (vr_installer_settings in quake.rc). Change one\n");
        sb.Append("// in the game after that and the game's value is kept: this file is not run again until Setup writes a new one.\n");
        sb.Append(StampTag).Append(Stamp).Append('\n');
        foreach (var (name, value) in Values)
        {
            sb.Append(name).Append(" \"").Append(value.Replace("\"", "")).Append("\"\n");
        }
        return sb.ToString();
    }
}

/// <summary>One run of the page: the values it starts with (the player's on an update), and the file it writes.</summary>
public sealed class SettingsSession
{
    public required string GameDir { get; init; }
    public required IReadOnlyList<SettingDef> Settings { get; init; }
    /// <summary>The value each setting starts with on the page. For a preset (apply) on an update: "" (as it is).</summary>
    public required IReadOnlyDictionary<string, string> Initial { get; init; }
    /// <summary>A saved config was found: an update (the page starts from the player's values).</summary>
    public bool HasConfig { get; init; }
    /// <summary>Picks of an earlier run of the page the game has not run yet (kept when writing).</summary>
    public IReadOnlyList<KeyValuePair<string, string>> Pending { get; init; } = [];

    public const string KeepAsIs = "";

    /// <summary>The page for an install (its quakevr folder); null when the package has no manifest (an older game).</summary>
    public static SettingsSession? Open(string installDir)
    {
        var gameDir = System.IO.Path.Combine(installDir, "quakevr");
        var manifest = System.IO.Path.Combine(gameDir, SettingsManifest.FileName);
        if (!File.Exists(manifest))
        {
            return null;
        }
        var settings = SettingsManifest.Load(manifest);
        if (settings.Count == 0)
        {
            return null;
        }
        var configPath = System.IO.Path.Combine(gameDir, GameConfig.FileName);
        var hasConfig = File.Exists(configPath);
        var config = GameConfig.ReadValues(configPath);
        var file = InstallerSettingsFile.Read(System.IO.Path.Combine(gameDir, InstallerSettingsFile.FileName));
        var applied = config.GetValueOrDefault(InstallerSettingsFile.AppliedCvar, "");
        var pending = file is { Stamp: { } stamp } && stamp != applied ? file.Values : [];
        var initial = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (var s in settings)
        {
            initial[s.Cvar] = s.IsApply ? (hasConfig ? KeepAsIs : s.Default) : config.GetValueOrDefault(s.Cvar, s.Default);
        }
        foreach (var (name, value) in pending)
        {
            if (initial.ContainsKey(name))
            {
                initial[name] = value;
            }
        }
        return new SettingsSession { GameDir = gameDir, Settings = settings, Initial = initial, HasConfig = hasConfig, Pending = pending };
    }

    /// <summary>The settings whose value differs from the page's start.</summary>
    public List<KeyValuePair<string, string>> Changes(IReadOnlyDictionary<string, string> values) =>
        Settings.Where(s => values.TryGetValue(s.Cvar, out var v) && !(s.IsApply && v == KeepAsIs) && !SettingValues.Same(v, Initial[s.Cvar]))
                .Select(s => new KeyValuePair<string, string>(s.Cvar, values[s.Cvar]))
                .ToList();

    /// <summary>Writes installer_settings.cfg with the changed settings (and any picks still pending); null when nothing
    /// changed (nothing written: the game's own values stay as they are).</summary>
    public string? Write(IReadOnlyDictionary<string, string> values, DateTime now)
    {
        var changes = Changes(values);
        if (changes.Count == 0)
        {
            return null;
        }
        var merged = Pending.Where(p => changes.All(c => !c.Key.Equals(p.Key, StringComparison.OrdinalIgnoreCase))).ToList();
        merged.AddRange(changes);
        var file = new InstallerSettingsFile { Stamp = InstallerSettingsFile.NewStamp(now), Values = merged };
        var path = System.IO.Path.Combine(GameDir, InstallerSettingsFile.FileName);
        var tmp = path + ".qvrnew";
        File.WriteAllText(tmp, file.Text(), new UTF8Encoding(false));
        File.Move(tmp, path, overwrite: true);
        return path;
    }
}
