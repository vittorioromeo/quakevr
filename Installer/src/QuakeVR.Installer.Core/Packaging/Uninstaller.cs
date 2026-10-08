using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Shortcuts;

namespace QuakeVR.Installer.Core.Packaging;

public sealed class UninstallOptions
{
    /// <summary>Also remove the HD textures the installer put in (they are large and slow to fetch again, so they are
    /// kept unless the player says so, like the rest of their data).</summary>
    public bool RemoveHdTextures { get; init; }
    /// <summary>Where the Apps &amp; Features entry is (the real HKCU, a test root), or null to leave it.</summary>
    public IRegistryWriter? Registry { get; init; }
}

public sealed class UninstallResult
{
    public int FilesRemoved { get; set; }
    public int ShortcutsRemoved { get; set; }
    /// <summary>Installed files the player changed since: kept.</summary>
    public List<string> ChangedKept { get; } = [];
    /// <summary>What is left in the folder: the player's own files (config, saves, screenshots, relit maps...).</summary>
    public List<string> PlayerFilesLeft { get; } = [];
    public bool FolderRemoved { get; set; }
    /// <summary>Installed files that could not be removed (in use: Setup's copy when the uninstall runs from it).</summary>
    public List<string> InUseKept { get; } = [];
    public bool EntryRemoved { get; set; }
}

/// <summary>
/// Removes exactly what <c>install.json</c> lists (section 5: "removes the manifest's files"): the shipped files that
/// are unchanged, the shortcuts that still point into the install, and the folders the installer made once they are
/// empty. The player's files stay, and the Quake folder is never touched (nothing in it is ever recorded).
/// </summary>
public static class Uninstaller
{
    public static UninstallResult Uninstall(string installDir, UninstallOptions options, IProgress<InstallProgress>? progress = null)
    {
        var target = PathUtil.TryNormalize(installDir) ?? throw new InstallException($"bad folder {installDir}");
        var record = InstallRecord.Load(target) ?? throw new InstallException($"No Quake VR install record ({InstallRecord.FileName}) in {target}.");
        var result = new UninstallResult();
        var keptTextures = false;
        var i = 0;
        foreach (var f in record.Files)
        {
            progress?.Report(new InstallProgress(0.9 * ++i / Math.Max(1, record.Files.Count), "Removing Quake VR"));
            if (f.Component == Components.HdTextures && !options.RemoveHdTextures)
            {
                keptTextures = true;
                continue;
            }
            var path = PathUtil.SafeCombine(target, f.Path);
            if (!File.Exists(path))
            {
                continue;
            }
            if (!string.Equals(PackageManifest.HashFile(path), f.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                result.ChangedKept.Add(f.Path);
                continue;
            }
            try
            {
                File.Delete(path);
                ++result.FilesRemoved;
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                result.InUseKept.Add(f.Path);
            }
        }
        foreach (var link in record.Shortcuts)
        {
            if (RemoveShortcut(link, target))
            {
                ++result.ShortcutsRemoved;
            }
        }
        FirstStartRelight.Set(target, false); // (the game had not started since: the installer's marker)
        InstallEngine.RemoveEmptyDirectories(record.Directories.Select(d => PathUtil.SafeCombine(target, d)));

        var remaining = Directory.EnumerateFiles(target, "*", SearchOption.AllDirectories)
            .Select(p => PathUtil.ToRelative(target, p))
            .Where(p => p != InstallRecord.FileName)
            .ToList();
        // The player's: everything left that the installer did not put there (kept textures and changed files are ours).
        var ours = record.Files.Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);
        result.PlayerFilesLeft.AddRange(remaining.Where(p => !ours.Contains(p)));
        if (options.Registry is not null)
        {
            try
            {
                result.EntryRemoved = UninstallEntry.Remove(options.Registry, target);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Security.SecurityException)
            {
                progress?.Report(new InstallProgress(0.95, "Removing Quake VR", $"The Apps & Features entry stays: {e.Message}", LogLevel.Warning));
            }
        }
        if (keptTextures || result.ChangedKept.Count > 0 || result.InUseKept.Count > 0)
        {
            // Keep a record of what is still ours, so a later uninstall (or reinstall) can finish the job.
            record.Files.RemoveAll(f => !File.Exists(PathUtil.SafeCombine(target, f.Path)));
            record.Shortcuts.Clear();
            if (!record.Files.Any(f => f.Component == Components.HdTextures))
            {
                record.HdTexturesFile = record.HdTexturesSha256 = null;
            }
            record.Save(target);
        }
        else
        {
            File.Delete(Path.Combine(target, InstallRecord.FileName));
            result.FolderRemoved = remaining.Count == 0 && InstallEngine.TryDeleteEmptyDirectory(target);
        }
        progress?.Report(new InstallProgress(1, "Done", $"Removed {result.FilesRemoved} files and {result.ShortcutsRemoved} shortcuts.", LogLevel.Success));
        return result;
    }

    /// <summary>Deletes a shortcut only when it is a .lnk whose target is inside the install (never someone else's
    /// shortcut of the same name), then its Start menu folder when that is empty.</summary>
    public static bool RemoveShortcut(string link, string installDir)
    {
        if (!link.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase) || !File.Exists(link))
        {
            return false;
        }
        try
        {
            var spec = ShellLink.Load(link);
            if (!PathUtil.IsInside(spec.TargetPath, installDir))
            {
                return false;
            }
            File.Delete(link);
            var folder = Path.GetDirectoryName(link);
            if (folder is not null && Path.GetFileName(folder) is ShortcutPlanner.StartMenuFolder or ShortcutPlanner.LegacyStartMenuFolder)
            {
                InstallEngine.TryDeleteEmptyDirectory(folder);
            }
            return true;
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Runtime.InteropServices.COMException)
        {
            return false;
        }
    }

    /// <summary>The installed files that are missing or changed (for "Verify / repair").</summary>
    public static IReadOnlyList<(string Path, string Problem)> Verify(string installDir)
    {
        var target = PathUtil.TryNormalize(installDir) ?? installDir;
        var record = InstallRecord.Load(target) ?? throw new InstallException($"No install record in {target}.");
        var problems = new List<(string, string)>();
        foreach (var f in record.Files)
        {
            var path = PathUtil.SafeCombine(target, f.Path);
            if (!File.Exists(path))
            {
                problems.Add((f.Path, "missing"));
            }
            else if (!string.Equals(PackageManifest.HashFile(path), f.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                problems.Add((f.Path, "changed"));
            }
        }
        return problems;
    }
}
