using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace QuakeVR.Installer.Core.Platform;

/// <summary>
/// The registry writes the installer makes (only its Apps &amp; Features entry, per user). The real one is
/// <see cref="WindowsRegistryWriter"/> (HKCU only); tests, the console and the screenshot harness use a
/// <see cref="JsonFileRegistry"/>, a made-up registry root in a file, so nothing they run writes the real registry.
/// </summary>
public interface IRegistryWriter
{
    /// <summary>A value (string or int), or null. <paramref name="key"/> starts with HKCU\.</summary>
    object? GetValue(string key, string name);
    void SetString(string key, string name, string value);
    void SetDword(string key, string name, int value);
    /// <summary>Removes the key and its values (nothing when it is absent).</summary>
    void DeleteKey(string key);
}

/// <summary>The real registry, HKCU only (no administrator rights needed; HKLM is refused).</summary>
public sealed class WindowsRegistryWriter : IRegistryWriter
{
    static string SubKey(string key)
    {
        foreach (var prefix in new[] { @"HKCU\", @"HKEY_CURRENT_USER\" })
        {
            if (key.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            {
                return key[prefix.Length..];
            }
        }
        throw new ArgumentException($"only HKCU is written: {key}");
    }

    static RegistryKey BaseKey() => RegistryKey.OpenBaseKey(RegistryHive.CurrentUser, RegistryView.Registry64);

    public object? GetValue(string key, string name)
    {
        using var root = BaseKey();
        using var k = root.OpenSubKey(SubKey(key));
        return k?.GetValue(name);
    }

    public void SetString(string key, string name, string value)
    {
        using var root = BaseKey();
        using var k = root.CreateSubKey(SubKey(key), writable: true);
        k.SetValue(name, value, RegistryValueKind.String);
    }

    public void SetDword(string key, string name, int value)
    {
        using var root = BaseKey();
        using var k = root.CreateSubKey(SubKey(key), writable: true);
        k.SetValue(name, value, RegistryValueKind.DWord);
    }

    public void DeleteKey(string key)
    {
        using var root = BaseKey();
        root.DeleteSubKeyTree(SubKey(key), throwOnMissingSubKey: false);
    }
}

/// <summary>
/// A made-up registry root: keys and values in a JSON file (<c>{"HKCU\\...": {"DisplayName": "...", "EstimatedSize": 123}}</c>),
/// read and written at each call, or in memory when no file is given. Strings stay strings, DWORDs are numbers.
/// </summary>
public sealed class JsonFileRegistry(string? path = null) : IRegistryWriter
{
    JsonObject _memory = [];

    public string? Path { get; } = path;

    JsonObject Load()
    {
        if (Path is null)
        {
            return _memory;
        }
        return File.Exists(Path) ? JsonNode.Parse(File.ReadAllText(Path)) as JsonObject ?? [] : [];
    }

    void Save(JsonObject root)
    {
        if (Path is null)
        {
            _memory = root;
            return;
        }
        Directory.CreateDirectory(System.IO.Path.GetDirectoryName(System.IO.Path.GetFullPath(Path))!);
        File.WriteAllText(Path, root.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
    }

    static JsonObject? Find(JsonObject root, string key) =>
        root.FirstOrDefault(p => string.Equals(p.Key, key, StringComparison.OrdinalIgnoreCase)).Value as JsonObject;

    public object? GetValue(string key, string name)
    {
        var values = Find(Load(), key);
        var node = values?.FirstOrDefault(p => string.Equals(p.Key, name, StringComparison.OrdinalIgnoreCase)).Value;
        return node?.GetValueKind() switch
        {
            JsonValueKind.String => node.GetValue<string>(),
            JsonValueKind.Number => node.GetValue<int>(),
            _ => null,
        };
    }

    void Set(string key, string name, JsonNode value)
    {
        var root = Load();
        var values = Find(root, key);
        if (values is null)
        {
            values = [];
            root[key] = values;
        }
        values[name] = value;
        Save(root);
    }

    public void SetString(string key, string name, string value) => Set(key, name, JsonValue.Create(value));

    public void SetDword(string key, string name, int value) => Set(key, name, JsonValue.Create(value));

    public void DeleteKey(string key)
    {
        var root = Load();
        foreach (var k in root.Select(p => p.Key).Where(k => string.Equals(k, key, StringComparison.OrdinalIgnoreCase)).ToList())
        {
            root.Remove(k);
        }
        Save(root);
    }

    /// <summary>The keys there are (tests).</summary>
    public IReadOnlyList<string> Keys => [.. Load().Select(p => p.Key)];
}
