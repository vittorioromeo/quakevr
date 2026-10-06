namespace QuakeVR.Installer.Core.Platform;

/// <summary>
/// A machine made up in memory: registry values and special folders set by a test (or the screenshot harness).
/// Folders not set resolve to null, so a test never reaches the real desktop or Start menu by accident.
/// </summary>
public sealed class MemorySystemProbe : ISystemProbe
{
    readonly Dictionary<string, Dictionary<string, object>> _keys = new(StringComparer.OrdinalIgnoreCase);
    readonly Dictionary<KnownFolder, string> _folders = [];

    public MemorySystemProbe SetValue(string key, string name, object value)
    {
        if (!_keys.TryGetValue(key, out var values))
        {
            values = new Dictionary<string, object>(StringComparer.OrdinalIgnoreCase);
            _keys[key] = values;
        }
        values[name] = value;
        return this;
    }

    public MemorySystemProbe SetFolder(KnownFolder folder, string path)
    {
        _folders[folder] = path;
        return this;
    }

    public object? GetRegistryValue(string key, string name) =>
        _keys.TryGetValue(key, out var values) && values.TryGetValue(name, out var v) ? v : null;

    public IReadOnlyList<string> GetRegistryValueNames(string key) =>
        _keys.TryGetValue(key, out var values) ? [.. values.Keys] : [];

    public string? GetFolder(KnownFolder folder) => _folders.TryGetValue(folder, out var p) ? p : null;
}
