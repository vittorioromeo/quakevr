namespace QuakeVR.Installer.Core.Platform;

/// <summary>
/// A Linux machine: no registry (every value null), the XDG base directories for the special folders, no file version
/// resources. Read-only. Tests make one over a made-up home: <paramref name="environment"/> gives the variables (HOME,
/// XDG_*), and <paramref name="systemRoot"/> is prefixed to the system folders (/etc/xdg, /usr/share) so a test never
/// reads the real ones.
/// <list type="table">
/// <item><see cref="KnownFolder.UserProfile"/>: $HOME</item>
/// <item><see cref="KnownFolder.LocalAppData"/>: $XDG_DATA_HOME, else ~/.local/share (the installer's data, the default install)</item>
/// <item><see cref="KnownFolder.ConfigHome"/>: $XDG_CONFIG_HOME, else ~/.config</item>
/// <item><see cref="KnownFolder.CacheHome"/>: $XDG_CACHE_HOME, else ~/.cache (downloads)</item>
/// <item><see cref="KnownFolder.SystemConfig"/>: the first of $XDG_CONFIG_DIRS, else /etc/xdg</item>
/// <item><see cref="KnownFolder.Desktop"/>: XDG_DESKTOP_DIR of ~/.config/user-dirs.dirs, else ~/Desktop</item>
/// <item><see cref="KnownFolder.StartMenuPrograms"/>: ~/.local/share/applications (the menu's .desktop entries)</item>
/// <item>ProgramFiles, ProgramFilesX86, ProgramData, System64: null</item>
/// </list>
/// </summary>
public sealed class LinuxSystemProbe(Func<string, string?>? environment = null, string? systemRoot = null) : ISystemProbe
{
    readonly Func<string, string?> _env = environment ?? Environment.GetEnvironmentVariable;

    public HostOs Os => HostOs.Linux;

    public object? GetRegistryValue(string key, string name) => null;

    public IReadOnlyList<string> GetRegistryValueNames(string key) => [];

    public Version? GetFileVersion(string path) => null;

    string? Home => _env("HOME") is { Length: > 0 } h ? h : null;

    /// <summary>An XDG variable when it holds an absolute path (the spec ignores relative ones), else the fallback under $HOME.</summary>
    string? Xdg(string variable, string fallback) =>
        _env(variable) is { Length: > 0 } v && v.StartsWith('/') ? v : Home is { } h ? Path.Combine(h, fallback) : null;

    /// <summary>A system path (absolute, from /), under the test root when there is one.</summary>
    public string SystemPath(string absolute) =>
        systemRoot is null ? absolute : Path.Combine(systemRoot, absolute.TrimStart('/'));

    /// <summary>$XDG_CONFIG_DIRS in order (default /etc/xdg), each under the test root.</summary>
    public IReadOnlyList<string> ConfigDirs =>
        [.. (_env("XDG_CONFIG_DIRS") is { Length: > 0 } dirs ? dirs : "/etc/xdg")
            .Split(':', StringSplitOptions.RemoveEmptyEntries).Where(d => d.StartsWith('/')).Select(SystemPath)];

    public string? GetFolder(KnownFolder folder) => folder switch
    {
        KnownFolder.UserProfile => Home,
        KnownFolder.LocalAppData => Xdg("XDG_DATA_HOME", ".local/share"),
        KnownFolder.ConfigHome => Xdg("XDG_CONFIG_HOME", ".config"),
        KnownFolder.CacheHome => Xdg("XDG_CACHE_HOME", ".cache"),
        KnownFolder.SystemConfig => ConfigDirs.FirstOrDefault(),
        KnownFolder.Desktop => DesktopDir(),
        KnownFolder.StartMenuPrograms => Xdg("XDG_DATA_HOME", ".local/share") is { } data ? Path.Combine(data, "applications") : null,
        _ => null,
    };

    /// <summary>XDG_DESKTOP_DIR from user-dirs.dirs (<c>XDG_DESKTOP_DIR="$HOME/Desktop"</c>), else ~/Desktop.</summary>
    string? DesktopDir()
    {
        if (Home is not { } home)
        {
            return null;
        }
        if (_env("XDG_DESKTOP_DIR") is { Length: > 0 } fromEnv && fromEnv.StartsWith('/'))
        {
            return fromEnv;
        }
        var dirsFile = Path.Combine(Xdg("XDG_CONFIG_HOME", ".config")!, "user-dirs.dirs");
        try
        {
            if (File.Exists(dirsFile))
            {
                foreach (var raw in File.ReadLines(dirsFile))
                {
                    var line = raw.Trim();
                    if (!line.StartsWith("XDG_DESKTOP_DIR=", StringComparison.Ordinal))
                    {
                        continue;
                    }
                    var value = line["XDG_DESKTOP_DIR=".Length..].Trim('"');
                    value = value.StartsWith("$HOME", StringComparison.Ordinal) ? home + value["$HOME".Length..] : value;
                    if (Path.IsPathRooted(value))
                    {
                        return value.TrimEnd('/');
                    }
                }
            }
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
        }
        return Path.Combine(home, "Desktop");
    }
}
