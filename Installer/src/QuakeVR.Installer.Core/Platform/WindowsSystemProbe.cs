using Microsoft.Win32;

namespace QuakeVR.Installer.Core.Platform;

/// <summary>The real machine: the 64-bit registry view and the shell's special folders. Read-only.</summary>
public sealed class WindowsSystemProbe : ISystemProbe
{
    public object? GetRegistryValue(string key, string name)
    {
        try
        {
            using var k = Open(key);
            return k?.GetValue(name);
        }
        catch (Exception e) when (e is System.Security.SecurityException or IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    public IReadOnlyList<string> GetRegistryValueNames(string key)
    {
        try
        {
            using var k = Open(key);
            return k?.GetValueNames() ?? [];
        }
        catch (Exception e) when (e is System.Security.SecurityException or IOException or UnauthorizedAccessException)
        {
            return [];
        }
    }

    public string? GetFolder(KnownFolder folder)
    {
        var path = folder switch
        {
            KnownFolder.LocalAppData => Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            KnownFolder.ProgramData => Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            KnownFolder.ProgramFiles => Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            KnownFolder.ProgramFilesX86 => Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            KnownFolder.Desktop => Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            KnownFolder.StartMenuPrograms => Environment.GetFolderPath(Environment.SpecialFolder.Programs),
            KnownFolder.UserProfile => Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            // (A 32-bit process sees SysWOW64 as System32: Sysnative is the real one.)
            KnownFolder.System64 => Environment.Is64BitProcess || !Environment.Is64BitOperatingSystem
                ? Environment.GetFolderPath(Environment.SpecialFolder.System)
                : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Sysnative"),
            _ => "",
        };
        return string.IsNullOrEmpty(path) ? null : path;
    }

    public Version? GetFileVersion(string path)
    {
        try
        {
            if (!File.Exists(path))
            {
                return null;
            }
            var info = System.Diagnostics.FileVersionInfo.GetVersionInfo(path);
            return info.FileMajorPart == 0 && info.FileMinorPart == 0 && info.FileBuildPart == 0
                ? null
                : new Version(info.FileMajorPart, info.FileMinorPart, info.FileBuildPart, info.FilePrivatePart);
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    static RegistryKey? Open(string key)
    {
        var slash = key.IndexOf('\\');
        var hive = slash < 0 ? key : key[..slash];
        var rest = slash < 0 ? "" : key[(slash + 1)..];
        var root = hive.ToUpperInvariant() switch
        {
            "HKLM" or "HKEY_LOCAL_MACHINE" => RegistryHive.LocalMachine,
            "HKCU" or "HKEY_CURRENT_USER" => RegistryHive.CurrentUser,
            _ => throw new ArgumentException($"unsupported registry hive in {key}"),
        };
        using var baseKey = RegistryKey.OpenBaseKey(root, RegistryView.Registry64);
        return baseKey.OpenSubKey(rest, writable: false);
    }
}
