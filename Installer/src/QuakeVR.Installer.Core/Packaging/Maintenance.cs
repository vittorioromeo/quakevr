namespace QuakeVR.Installer.Core.Packaging;

/// <summary>What an install over an existing one does.</summary>
public enum InstallMode
{
    /// <summary>A full install (a fresh one, or "Install again from scratch"): every program file is copied.</summary>
    Install,
    /// <summary>A newer package over the install: only the program files that differ are copied, the player's files
    /// are kept, the HD textures and the first-start relight are skipped when unchanged.</summary>
    Update,
    /// <summary>The same (or an older) package: the installed program files are checked and the missing or changed
    /// ones restored. An older package never downgrades: it restores only files identical in both versions.</summary>
    Repair,
}

public enum PlannedAction
{
    /// <summary>New in this version.</summary>
    Add,
    /// <summary>Changed in this version.</summary>
    Replace,
    /// <summary>Missing or changed on disk: put back as shipped.</summary>
    Restore,
    /// <summary>Already as shipped: not copied.</summary>
    Unchanged,
    /// <summary>No longer shipped, and as installed: removed.</summary>
    Remove,
    /// <summary>No longer shipped, but the player changed it: kept (it becomes the player's).</summary>
    KeepChanged,
    /// <summary>A repair from an older package: the installed version's file, kept as it is.</summary>
    KeepInstalled,
    /// <summary>A repair from an older package: missing or changed, and the package has another version of it.</summary>
    CannotRestore,
}

public sealed record PlannedFile(string Path, PlannedAction Action, long Size);

/// <summary>What happens to the HD textures at an update or a repair.</summary>
public enum HdTexturesAction
{
    /// <summary>Not installed: nothing to do (an update never adds them; "Install again from scratch" can).</summary>
    NotInstalled,
    /// <summary>The installed pack is the target's (same SHA-256): kept, not downloaded.</summary>
    Keep,
    /// <summary>Installed before Setup recorded the pack's SHA-256: kept, not downloaded.</summary>
    KeepUnknown,
    /// <summary>Another pack (the target's SHA-256 differs): downloaded and installed.</summary>
    Replace,
    /// <summary>A repair found installed texture files missing or changed: the pack is downloaded again.</summary>
    Restore,
}

/// <summary>
/// The plan of an update, a repair, or an install over an existing install, made from install.json, the package's
/// manifest and the files on disk (each program file hashed). The console's --dry-run prints it; InstallEngine follows it.
/// </summary>
public sealed class MaintenancePlan
{
    public InstallMode Mode { get; init; }
    public string InstalledVersion { get; init; } = "";
    public string PackageVersion { get; init; } = "";
    public VersionOrder Order { get; init; }
    /// <summary>The package's files and the installed version's (component core) with what happens to each.</summary>
    public List<PlannedFile> Files { get; } = [];
    /// <summary>Files copied into the backup before they are overwritten: program files the player changed, and the
    /// player's own files where this version ships one.</summary>
    public List<string> BackupFirst { get; } = [];
    /// <summary>The relight's inputs (<see cref="MaintenancePlanner.IsRelightInput"/>) this version adds, changes or removes.</summary>
    public List<string> RelightInputsChanged { get; } = [];
    /// <summary>The player's files (never touched), sorted.</summary>
    public List<(string Path, UserDataKind Kind)> UserFiles { get; } = [];
    /// <summary>A repair from an older package keeps the installed version (record, Apps &amp; Features).</summary>
    public bool KeepsInstalledVersion => Mode == InstallMode.Repair && Order == VersionOrder.Older;

    public IEnumerable<PlannedFile> ToCopy => Files.Where(f => f.Action is PlannedAction.Add or PlannedAction.Replace or PlannedAction.Restore);
    public long CopyBytes => ToCopy.Sum(f => f.Size);
    public int Count(PlannedAction a) => Files.Count(f => f.Action == a);

    public string ModeText => Mode switch
    {
        InstallMode.Update => $"update {InstalledVersion} -> {PackageVersion}",
        InstallMode.Repair when KeepsInstalledVersion => $"repair {InstalledVersion} (the package, {PackageVersion}, is older: no downgrade; only files identical in both are restored)",
        InstallMode.Repair => $"repair {InstalledVersion}",
        _ => $"install {PackageVersion} again over {InstalledVersion} (every program file copied)",
    };

    /// <summary>The plan as the console prints it: totals, then each file that changes (at most <paramref name="maxLines"/>).</summary>
    public string Format(int maxLines = 40)
    {
        var w = new StringWriter();
        w.WriteLine($"plan: {ModeText}");
        w.WriteLine($"program files: {Count(PlannedAction.Add)} added, {Count(PlannedAction.Replace)} replaced, {Count(PlannedAction.Restore)} restored, " +
                    $"{Count(PlannedAction.Unchanged)} unchanged (not copied), {Count(PlannedAction.Remove)} removed, {Count(PlannedAction.KeepChanged)} no longer shipped but changed by you (kept)" +
                    (KeepsInstalledVersion ? $", {Count(PlannedAction.KeepInstalled)} kept as installed, {Count(PlannedAction.CannotRestore)} cannot be restored from this package" : "") +
                    $"; {PathUtil.FormatSize(CopyBytes)} to copy");
        var changes = Files.Where(f => f.Action is not (PlannedAction.Unchanged or PlannedAction.KeepInstalled)).ToList();
        foreach (var f in changes.Take(maxLines))
        {
            w.WriteLine($"  {f.Action.ToString().ToLowerInvariant(),-13} {f.Path}");
        }
        if (changes.Count > maxLines)
        {
            w.WriteLine($"  ... and {changes.Count - maxLines} more");
        }
        foreach (var b in BackupFirst)
        {
            w.WriteLine($"  backed up first (yours or changed by you): {b}");
        }
        w.WriteLine(RelightInputsChanged.Count > 0
            ? $"relight inputs changed: {string.Join(", ", RelightInputsChanged)}"
            : "relight inputs unchanged");
        w.WriteLine($"your files, kept as they are: {UserFiles.Count} ({UserData.Summary(UserFiles)})");
        return w.ToString();
    }
}

public static class MaintenancePlanner
{
    /// <summary>
    /// What the first-start relight is made from that a package ships: ericw-tools' light, the VisPatch data and the
    /// texture rules (relight_textures.cfg). The game's batch skips a map relit from the same map, settings, light options
    /// and relight_textures.cfg (vr_relight.cpp, relitAlready); an update that changes none of these files does not ask
    /// for the relight again (a pending one stays).
    /// </summary>
    public static bool IsRelightInput(string relative)
    {
        var p = relative.Replace('\\', '/');
        return p.StartsWith("quakevr/tools/ericw-tools/", StringComparison.OrdinalIgnoreCase) ||
               p.StartsWith("quakevr/tools/vispatch/", StringComparison.OrdinalIgnoreCase) ||
               p.Equals("quakevr/relight_textures.cfg", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>The mode Setup offers for this package over this install: newer (or another build) updates, the same
    /// or an older one repairs.</summary>
    public static InstallMode ModeFor(string installedVersion, string packageVersion) =>
        ReleaseVersion.Compare(installedVersion, packageVersion) is VersionOrder.Newer or VersionOrder.Other ? InstallMode.Update : InstallMode.Repair;

    public static MaintenancePlan Plan(string installDir, InstallRecord installed, PackageManifest manifest, InstallMode mode)
    {
        var order = ReleaseVersion.Compare(installed.Version, manifest.Version);
        var plan = new MaintenancePlan { Mode = mode, InstalledVersion = installed.Version, PackageVersion = manifest.Version, Order = order };
        var recorded = installed.Files.ToDictionary(f => f.Path, StringComparer.OrdinalIgnoreCase);
        var shipped = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var olderRepair = plan.KeepsInstalledVersion;

        foreach (var m in manifest.Files)
        {
            shipped.Add(m.Path);
            var path = PathUtil.SafeCombine(installDir, m.Path);
            var mine = recorded.GetValueOrDefault(m.Path);
            var disk = File.Exists(path) ? PackageManifest.HashFile(path) : null;
            var same = mine is not null && string.Equals(mine.Sha256, m.Sha256, StringComparison.OrdinalIgnoreCase);
            PlannedAction action;
            if (olderRepair && !same)
            {
                // Not this version's file: only a file of the installed version is touched, and only from an identical copy.
                if (mine is null)
                {
                    continue; // not in the installed version (a player's file there stays as it is)
                }
                action = disk is not null && string.Equals(disk, mine.Sha256, StringComparison.OrdinalIgnoreCase)
                    ? PlannedAction.KeepInstalled : PlannedAction.CannotRestore;
            }
            else if (disk is null)
            {
                action = mine is null || !same ? PlannedAction.Add : PlannedAction.Restore;
            }
            else if (string.Equals(disk, m.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                action = PlannedAction.Unchanged;
            }
            else if (mine is not null && string.Equals(disk, mine.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                action = PlannedAction.Replace; // as installed: this version's replaces it
            }
            else
            {
                // Changed by the player (or the player's own file where this version ships one): kept in the backup first.
                action = mine is not null && same ? PlannedAction.Restore : mine is null ? PlannedAction.Add : PlannedAction.Replace;
                plan.BackupFirst.Add(m.Path);
            }
            plan.Files.Add(new PlannedFile(m.Path, action, m.Size));
            if (action is PlannedAction.Add or PlannedAction.Replace && IsRelightInput(m.Path))
            {
                plan.RelightInputsChanged.Add(m.Path);
            }
        }

        foreach (var f in installed.Files.Where(f => f.Component == Components.Core && !shipped.Contains(f.Path)))
        {
            var path = PathUtil.SafeCombine(installDir, f.Path);
            var disk = File.Exists(path) ? PackageManifest.HashFile(path) : null;
            var asInstalled = disk is not null && string.Equals(disk, f.Sha256, StringComparison.OrdinalIgnoreCase);
            if (olderRepair)
            {
                plan.Files.Add(new PlannedFile(f.Path, asInstalled ? PlannedAction.KeepInstalled : PlannedAction.CannotRestore, f.Size));
                continue;
            }
            if (disk is null)
            {
                continue; // gone already
            }
            plan.Files.Add(new PlannedFile(f.Path, asInstalled ? PlannedAction.Remove : PlannedAction.KeepChanged, f.Size));
            if (asInstalled && IsRelightInput(f.Path))
            {
                plan.RelightInputsChanged.Add(f.Path);
            }
        }
        plan.UserFiles.AddRange(UserData.Find(installDir, installed));
        return plan;
    }

    /// <summary>
    /// The HD textures at an update or a repair: kept when the installed pack is <paramref name="target"/> (its SHA-256,
    /// recorded in install.json since this version of Setup) or of unknown origin, else downloaded. A repair
    /// (<paramref name="verify"/>) also checks the installed texture files and downloads the pack again when one is
    /// missing or changed.
    /// </summary>
    public static (HdTexturesAction Action, string Text) HdTextures(string installDir, InstallRecord installed, FeedFile? target, bool verify)
    {
        var files = installed.Files.Where(f => f.Component == Components.HdTextures).ToList();
        if (files.Count == 0)
        {
            return (HdTexturesAction.NotInstalled, "HD textures: not installed (Install again from scratch adds them)");
        }
        if (verify)
        {
            var broken = files.Count(f =>
            {
                var p = PathUtil.SafeCombine(installDir, f.Path);
                return !File.Exists(p) || !string.Equals(PackageManifest.HashFile(p), f.Sha256, StringComparison.OrdinalIgnoreCase);
            });
            if (broken > 0 && target is not null)
            {
                return (HdTexturesAction.Restore, $"HD textures: {broken} of {files.Count} files missing or changed: {target.File} downloaded again ({PathUtil.FormatSize(target.Size)})");
            }
            if (broken > 0)
            {
                return (HdTexturesAction.KeepUnknown, $"HD textures: {broken} of {files.Count} files missing or changed, and no pack to restore them from");
            }
        }
        if (target is null)
        {
            return (HdTexturesAction.Keep, $"HD textures: kept ({files.Count} files; no pack named to compare with)");
        }
        if (installed.HdTexturesSha256 is not { } sha)
        {
            return (HdTexturesAction.KeepUnknown, $"HD textures: kept ({files.Count} files, installed before Setup recorded the pack; Install again from scratch to refresh them)");
        }
        return string.Equals(sha, target.Sha256, StringComparison.OrdinalIgnoreCase)
            ? (HdTexturesAction.Keep, $"HD textures: unchanged ({installed.HdTexturesFile ?? target.File}, same SHA-256): skipped, not downloaded")
            : (HdTexturesAction.Replace, $"HD textures: a new pack, {target.File} ({PathUtil.FormatSize(target.Size)}), replaces {installed.HdTexturesFile ?? "the installed one"}");
    }
}
