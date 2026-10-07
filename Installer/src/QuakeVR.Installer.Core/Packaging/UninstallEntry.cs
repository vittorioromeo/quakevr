using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Shortcuts;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// The Apps &amp; Features entry ("Installed apps"): per user, no administrator rights,
/// <c>HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\QuakeVRUnleashed</c>. Its Uninstall button runs the copy of
/// Setup kept in the install (<see cref="SetupCopy"/>) with <c>--uninstall</c>, which removes what install.json lists.
/// Written at every install and update; removed by the uninstall when it points at that install.
/// </summary>
public static class UninstallEntry
{
    public const string Key = @"HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\QuakeVRUnleashed";
    public const string DisplayName = "Quake VR: Unleashed";
    public const string Publisher = "Vittorio Romeo";
    public const string AboutUrl = "https://github.com/vittorioromeo/quakevr";

    public static string UninstallString(string setupExe, string installDir) =>
        $"\"{setupExe}\" --uninstall --target {LaunchCommand.Quote(installDir)}";

    public static string QuietUninstallString(string setupExe, string installDir) =>
        $"\"{setupExe}\" --uninstall --quiet --target {LaunchCommand.Quote(installDir)}";

    /// <summary>The installed files' size in KiB (what Apps &amp; Features shows).</summary>
    public static int EstimatedSizeKb(InstallRecord record) =>
        (int)Math.Min(int.MaxValue, record.Files.Sum(f => f.Size) / 1024);

    public static void Write(IRegistryWriter registry, string installDir, InstallRecord record, string setupExe)
    {
        var dir = PathUtil.TrimEnd(installDir);
        registry.SetString(Key, "DisplayName", DisplayName);
        registry.SetString(Key, "DisplayVersion", record.Version);
        registry.SetString(Key, "Publisher", Publisher);
        registry.SetString(Key, "DisplayIcon", Path.Combine(dir, LaunchCommand.Exe) + ",0");
        registry.SetString(Key, "InstallLocation", dir);
        registry.SetString(Key, "InstallDate", record.InstalledAt.ToString("yyyyMMdd", System.Globalization.CultureInfo.InvariantCulture));
        registry.SetDword(Key, "EstimatedSize", EstimatedSizeKb(record));
        registry.SetString(Key, "UninstallString", UninstallString(setupExe, dir));
        registry.SetString(Key, "QuietUninstallString", QuietUninstallString(setupExe, dir));
        registry.SetString(Key, "URLInfoAbout", AboutUrl);
        registry.SetDword(Key, "NoModify", 1);
        registry.SetDword(Key, "NoRepair", 1);
    }

    /// <summary>Removes the entry when it is this install's (another folder's entry is left alone).</summary>
    public static bool Remove(IRegistryWriter registry, string installDir)
    {
        if (registry.GetValue(Key, "InstallLocation") is not string location || !PathUtil.SamePath(location, installDir))
        {
            return false;
        }
        registry.DeleteKey(Key);
        return true;
    }
}

/// <summary>
/// The copy of Setup kept in the install (<c>&lt;QVR&gt;\setup\QuakeVR-Setup.exe</c>), which the Apps &amp; Features entry runs to
/// uninstall (and which can update or remove the install later). Staged and recorded like the package's files
/// (component <c>setup</c>): checked, kept by updates run from it, removed by the uninstall. A published Setup is one
/// self-contained file; a development build is its exe with the DLLs and JSON files beside it.
/// </summary>
public static class SetupCopy
{
    public const string Folder = "setup";
    public const string ExeName = "QuakeVR-Setup.exe";
    public const string SettingsName = "installer-settings.json";

    /// <summary>Whether a Setup exe is a published single file (a development build has its .dll beside it).</summary>
    public static bool IsSingleFile(string exe) => !File.Exists(Path.ChangeExtension(exe, ".dll"));

    /// <summary>The files to copy (source, path in the install) for Setup running as <paramref name="processPath"/>.</summary>
    /// <param name="singleFile">A published single-file exe (its assembly has no location).</param>
    public static IReadOnlyList<(string Source, string Relative)> FilesOf(string processPath, bool singleFile)
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(processPath))!;
        var list = new List<(string, string)> { (processPath, $"{Folder}/{ExeName}") };
        if (!singleFile)
        {
            // (A development build: only its build outputs, never a package, a download or another program beside it.)
            foreach (var f in Directory.EnumerateFiles(dir).Order(StringComparer.OrdinalIgnoreCase))
            {
                var name = Path.GetFileName(f);
                if (name.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
                    name.EndsWith(".runtimeconfig.json", StringComparison.OrdinalIgnoreCase) ||
                    name.EndsWith(".deps.json", StringComparison.OrdinalIgnoreCase))
                {
                    list.Add((f, $"{Folder}/{name}"));
                }
            }
        }
        if (File.Exists(Path.Combine(dir, SettingsName)))
        {
            list.Add((Path.Combine(dir, SettingsName), $"{Folder}/{SettingsName}"));
        }
        return list;
    }

    /// <summary>Copies Setup's files (as <see cref="FilesOf"/>, folders flattened) into a new folder under
    /// <paramref name="tempRoot"/> and returns the copy's exe: an uninstall run from the install's own copy restarts
    /// from there, so the copy in the install can be removed (a running exe cannot be deleted).</summary>
    public static string CopyToTemp(IReadOnlyList<(string Source, string Relative)> files, string tempRoot)
    {
        var dir = Path.Combine(tempRoot, "QuakeVR-Setup-" + Guid.NewGuid().ToString("N")[..8]);
        Directory.CreateDirectory(dir);
        foreach (var (source, relative) in files)
        {
            File.Copy(source, Path.Combine(dir, Path.GetFileName(relative)), overwrite: true);
        }
        return Path.Combine(dir, ExeName);
    }

    /// <summary>The install a copy of Setup belongs to (it runs from &lt;install&gt;\setup), or null.</summary>
    public static string? InstallOf(string processPath)
    {
        var dir = Path.GetDirectoryName(Path.GetFullPath(processPath));
        var parent = dir is null ? null : Path.GetDirectoryName(dir);
        return parent is not null && string.Equals(Path.GetFileName(dir), Folder, StringComparison.OrdinalIgnoreCase) &&
               File.Exists(Path.Combine(parent, InstallRecord.FileName)) ? parent : null;
    }
}
