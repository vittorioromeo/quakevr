namespace QuakeVR.Installer.Core.Platform;

/// <summary>Folders the installer reads or writes outside the install folder.</summary>
public enum KnownFolder
{
    LocalAppData,
    ProgramData,
    ProgramFiles,
    ProgramFilesX86,
    Desktop,
    StartMenuPrograms,
    UserProfile,
}

/// <summary>
/// Everything detection reads from the machine besides plain files: the registry and the special folders. The real
/// one is <see cref="WindowsSystemProbe"/>; tests and the screenshot harness use <see cref="MemorySystemProbe"/>, so
/// nothing ever depends on (or writes to) the real registry, desktop or Start menu.
/// </summary>
public interface ISystemProbe
{
    /// <summary>A registry value, or null. <paramref name="key"/> starts with HKLM\ or HKCU\ (64-bit view).</summary>
    object? GetRegistryValue(string key, string name);

    /// <summary>The value names of a registry key (empty when the key is absent).</summary>
    IReadOnlyList<string> GetRegistryValueNames(string key);

    /// <summary>A special folder, or null when it cannot be resolved.</summary>
    string? GetFolder(KnownFolder folder);
}

public static class SystemProbeExtensions
{
    public static string? GetRegistryString(this ISystemProbe probe, string key, string name) =>
        probe.GetRegistryValue(key, name) as string is { Length: > 0 } s ? s : null;

    public static int? GetRegistryInt(this ISystemProbe probe, string key, string name) =>
        probe.GetRegistryValue(key, name) switch
        {
            int i => i,
            long l => (int)l,
            string s when int.TryParse(s, out var p) => p,
            _ => null,
        };
}
