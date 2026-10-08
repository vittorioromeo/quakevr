using System.IO.Compression;
using System.Security.Cryptography;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Shortcuts;

namespace QuakeVR.Installer.Core.Packaging;

public enum LogLevel
{
    Info,
    Success,
    Warning,
    Error,
}

public sealed record InstallProgress(double Fraction, string Status, string? Log = null, LogLevel Level = LogLevel.Info);

public sealed class InstallException(string message, Exception? inner = null) : Exception(message, inner);

/// <summary>What to install, where, and with which options.</summary>
public sealed class InstallPlan
{
    /// <summary>A package folder or zip (<c>Windows/package-quakevr.ps1</c>'s output), local or just downloaded.</summary>
    public required string PackagePath { get; init; }
    public required string TargetDir { get; init; }
    /// <summary>The first -basedir: the folder with id1\pak0.pak.</summary>
    public required string QuakeDir { get; init; }
    public string? QuakeStore { get; init; }
    /// <summary>Install a package that has no manifest.json (its files are hashed as they are; nothing to check them
    /// against). Developer use only.</summary>
    public bool AllowUnverified { get; init; }
    public ShortcutOptions Shortcuts { get; init; } = new() { Desktop = false, StartMenu = false };
    public bool RelightOnFirstRun { get; init; }
    /// <summary>The HD texture pack's zip (already checked against its pinned SHA-256 by the download), or null.</summary>
    public string? HdTexturesZip { get; init; }
    /// <summary>The pack's SHA-256 when known (the download checked it); null: hashed here. Recorded in install.json.</summary>
    public string? HdTexturesSha256 { get; init; }
    /// <summary>Install (every program file copied), Update or Repair (<see cref="MaintenancePlanner"/>: only the files
    /// that differ; Update asks for the first-start relight only when its inputs changed). Over a folder without an
    /// install, always a full install.</summary>
    public InstallMode Mode { get; init; } = InstallMode.Install;
    /// <summary>A backup already made for this run ("Install again from scratch" moved the player's reset files into it):
    /// the files Setup overwrites go into it too. Null: one is made when needed.</summary>
    public Backup? Backup { get; init; }
    /// <summary>VisPatch's archives (.tgz), already checked against their pinned SHA-256: their data files go into
    /// quakevr\tools\vispatch, where the game's relight finds them (see-through water).</summary>
    public IReadOnlyList<string> VisPatchArchives { get; init; } = [];
    /// <summary>Mission packs the player owns ("hipnotic", "rogue"): only theirs get textures.</summary>
    public IReadOnlyCollection<string> OwnedPacks { get; init; } = [];
    /// <summary>Setup's own files (source, path in the install: <see cref="SetupCopy"/>), kept in the install for the
    /// Apps &amp; Features entry's Uninstall. Empty: none copied (an earlier copy is kept).</summary>
    public IReadOnlyList<(string Source, string Relative)> SetupFiles { get; init; } = [];
    /// <summary>Where the Apps &amp; Features entry goes (<see cref="UninstallEntry"/>): the real HKCU, a test root, or
    /// null for none.</summary>
    public IRegistryWriter? Registry { get; init; }
}

/// <summary>
/// Installs a package into its own folder (layout B, decision 2): the Quake folder is never written to. Files are
/// staged next to their destination as <c>*.qvrnew</c> while their SHA-256 is checked against the manifest, then all
/// moved into place at once, so a cancelled or failed install leaves an existing install as it was. An update removes
/// the files the previous version shipped that this one no longer does (unless the player changed them) and keeps
/// everything else (config, saves, relit maps, Map Library).
/// </summary>
public sealed class InstallEngine
{
    const string StagingSuffix = ".qvrnew";

    /// <summary>Why a folder cannot be the install folder, or null when it can.</summary>
    public static string? ValidateTarget(string target, string quakeDir, ISystemProbe probe)
    {
        var t = PathUtil.TryNormalize(target);
        if (t is null || Path.GetPathRoot(t) == t)
        {
            return "Pick a folder for Quake VR (not a drive's root).";
        }
        if (PathUtil.IsInside(t, quakeDir) || PathUtil.IsInside(quakeDir, t))
        {
            return "Pick a folder outside your Quake folder: Quake VR leaves your Quake files untouched.";
        }
        foreach (var protectedDir in new[] { probe.GetFolder(KnownFolder.ProgramFiles), probe.GetFolder(KnownFolder.ProgramFilesX86),
                     Environment.GetFolderPath(Environment.SpecialFolder.Windows) })
        {
            if (!string.IsNullOrEmpty(protectedDir) && PathUtil.IsInside(t, protectedDir))
            {
                return "Program Files is not writable without administrator rights, and the game saves its settings and " +
                       "games in its own folder. Pick a folder in your user profile (the default) or on another drive.";
            }
        }
        if (Directory.Exists(t) && !File.Exists(Path.Combine(t, InstallRecord.FileName)) && Directory.EnumerateFileSystemEntries(t).Any())
        {
            return "This folder already has files in it. Pick an empty or new folder.";
        }
        return null;
    }

    sealed record StageItem(string Relative, string Component, Func<Stream> Open, long Size, string? Sha256);

    /// <summary>The last run's plan over an existing install (null: a fresh install).</summary>
    public MaintenancePlan? LastPlan { get; private set; }
    /// <summary>The backup the last run put the player's changed files in (null: none needed).</summary>
    public Backup? LastBackup { get; private set; }

    public Task<InstallRecord> InstallAsync(InstallPlan plan, IProgress<InstallProgress>? progress, CancellationToken ct) =>
        Task.Run(() => Install(plan, progress, ct), ct);

    public InstallRecord Install(InstallPlan plan, IProgress<InstallProgress>? progress, CancellationToken ct)
    {
        void Report(double f, string status, string? log = null, LogLevel level = LogLevel.Info) =>
            progress?.Report(new InstallProgress(f, status, log, level));

        var target = PathUtil.TryNormalize(plan.TargetDir) ?? throw new InstallException($"bad install folder {plan.TargetDir}");
        var quakeDir = PathUtil.TryNormalize(plan.QuakeDir) ?? throw new InstallException($"bad Quake folder {plan.QuakeDir}");
        Report(0, "Reading the package", $"Package: {plan.PackagePath}");
        using var source = PackageSource.FromPath(plan.PackagePath);
        var manifest = source.ReadManifest();
        if (manifest is null)
        {
            if (!plan.AllowUnverified)
            {
                throw new InstallException("The package has no manifest.json, so its files cannot be checked. Use a package made by package-quakevr.ps1.");
            }
            Report(0, "Reading the package", "No manifest.json: installing the package's files unchecked (developer mode).", LogLevel.Warning);
            manifest = new PackageManifest { Version = "unverified" };
            foreach (var f in source.Files.Order(StringComparer.Ordinal))
            {
                manifest.Files.Add(new ManifestFile { Path = f, Size = source.SizeOf(f), Sha256 = "" });
            }
        }
        foreach (var f in manifest.Files)
        {
            if (!source.Files.Contains(f.Path, StringComparer.OrdinalIgnoreCase))
            {
                throw new InstallException($"The package is incomplete: {f.Path} is missing.");
            }
        }
        var extras = source.Files.Count(f => f != PackageManifest.FileName && !manifest.Files.Any(m => string.Equals(m.Path, f, StringComparison.OrdinalIgnoreCase)));
        if (extras > 0)
        {
            Report(0, "Reading the package", $"{extras} file(s) in the package are not in its manifest: not installed.", LogLevel.Warning);
        }
        Report(0, "Reading the package", $"Quake VR: Unleashed {manifest.Version}: {manifest.Files.Count} files, {PathUtil.FormatSize(manifest.TotalSize)}");

        var old = InstallRecord.Load(target);
        var mode = old is null ? InstallMode.Install : plan.Mode;
        MaintenancePlan? maintenance = null;
        LastBackup = plan.Backup;
        if (old is not null)
        {
            Report(0, mode == InstallMode.Repair ? "Repairing" : "Updating", mode switch
            {
                InstallMode.Update => $"Updating Quake VR: Unleashed {old.Version} in {target}; your settings, saves and maps are kept.",
                InstallMode.Repair => $"Repairing Quake VR: Unleashed {old.Version} in {target}; your settings, saves and maps are kept.",
                _ => $"Installing again over Quake VR: Unleashed {old.Version} in {target}; your files are kept unless you chose otherwise.",
            });
            Report(0, "Checking the installed files");
            maintenance = MaintenancePlanner.Plan(target, old, manifest, mode);
            LastPlan = maintenance;
            if (mode != InstallMode.Install)
            {
                Report(0, "Checking the installed files",
                    $"{maintenance.ToCopy.Count()} program file(s) to copy ({PathUtil.FormatSize(maintenance.CopyBytes)}), {maintenance.Count(PlannedAction.Unchanged)} already up to date, " +
                    $"{maintenance.Count(PlannedAction.Remove)} no longer shipped; {maintenance.UserFiles.Count} of your files kept as they are.");
                if (maintenance.KeepsInstalledVersion)
                {
                    Report(0, "Checking the installed files", $"The package ({manifest.Version}) is older than the install: nothing is downgraded, " +
                        $"only files identical in both are restored{(maintenance.Count(PlannedAction.CannotRestore) is > 0 and var n ? $"; {n} damaged file(s) need {old.Version}'s package" : "")}.",
                        maintenance.Count(PlannedAction.CannotRestore) > 0 ? LogLevel.Warning : LogLevel.Info);
                }
            }
        }

        var copyCore = mode == InstallMode.Install || maintenance is null ? null
            : maintenance.ToCopy.Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);
        var items = manifest.Files.Where(f => copyCore is null || copyCore.Contains(f.Path))
            .Select(f => new StageItem(f.Path, Components.Core, () => source.Open(f.Path), f.Size, f.Sha256.Length == 64 ? f.Sha256 : null)).ToList();
        var oldRecorded = old?.Files.ToDictionary(f => f.Path, StringComparer.OrdinalIgnoreCase) ?? [];
        var hdSha = plan.HdTexturesZip is null ? null : plan.HdTexturesSha256 ?? PackageManifest.HashFile(plan.HdTexturesZip);

        ZipArchive? textures = null;
        try
        {
            if (plan.HdTexturesZip is not null)
            {
                textures = ZipFile.OpenRead(plan.HdTexturesZip);
                var added = 0;
                var skippedOwn = 0;
                foreach (var (rel, entry) in TextureEntries(textures, plan.OwnedPacks))
                {
                    var dest = PathUtil.SafeCombine(target, rel);
                    if (File.Exists(dest) && !oldRecorded.ContainsKey(rel))
                    {
                        ++skippedOwn; // the player's own file: never overwritten
                        continue;
                    }
                    items.Add(new StageItem(rel, Components.HdTextures, entry.Open, entry.Length, null));
                    ++added;
                }
                Report(0, "Reading the package", $"HD textures: {added} files for id1{string.Concat(plan.OwnedPacks.Order().Select(p => ", " + p))}" +
                    (skippedOwn > 0 ? $"; {skippedOwn} of your own texture files left as they are" : ""));
            }

            if (plan.VisPatchArchives.Count > 0)
            {
                var visFiles = 0;
                foreach (var archive in plan.VisPatchArchives)
                {
                    foreach (var (rel, data) in VisPatch.Extract(archive))
                    {
                        var dest = PathUtil.SafeCombine(target, rel);
                        if (File.Exists(dest) && !oldRecorded.ContainsKey(rel))
                        {
                            Report(0, "Reading the package", $"{rel}: your own copy, left as it is");
                            continue;
                        }
                        items.Add(new StageItem(rel, Components.VisPatch, () => new MemoryStream(data, writable: false), data.Length, null));
                        ++visFiles;
                    }
                }
                Report(0, "Reading the package", $"See-through water (VisPatch): {visFiles} data file(s) for the relight");
            }

            // Setup's copy in the install (for Apps & Features' Uninstall). Run from that copy (an update started from
            // it), its files are already in place and in use: kept as they are.
            var keptSetup = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var (setupSource, relative) in plan.SetupFiles)
            {
                if (PathUtil.SamePath(Path.GetFullPath(setupSource), PathUtil.SafeCombine(target, relative)))
                {
                    keptSetup.Add(relative);
                    continue;
                }
                var path = setupSource;
                items.Add(new StageItem(relative, Components.Setup, () => File.OpenRead(path), new FileInfo(setupSource).Length, null));
            }

            var total = items.Sum(i => i.Size);
            var drive = new DriveInfo(Path.GetPathRoot(target)!);
            if (drive.IsReady && drive.AvailableFreeSpace < total + (64L << 20))
            {
                throw new InstallException($"Not enough disk space on {drive.Name}: {PathUtil.FormatSize(total)} needed, {PathUtil.FormatSize(drive.AvailableFreeSpace)} free.");
            }

            var createdDirs = new List<string>();
            var targetCreated = !Directory.Exists(target);
            var staged = new List<(StageItem Item, string Dest, string Temp, string Sha)>();
            try
            {
                EnsureDirectory(target, target, createdDirs);
                long done = 0;
                var lastReported = -1.0;
                var buffer = new byte[1 << 20];
                foreach (var item in items)
                {
                    ct.ThrowIfCancellationRequested();
                    var dest = PathUtil.SafeCombine(target, item.Relative);
                    EnsureDirectory(Path.GetDirectoryName(dest)!, target, createdDirs);
                    var temp = dest + StagingSuffix;
                    using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
                    long size = 0;
                    staged.Add((item, dest, temp, ""));
                    using (var input = item.Open())
                    using (var output = new FileStream(temp, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16))
                    {
                        int n;
                        while ((n = input.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            ct.ThrowIfCancellationRequested();
                            hash.AppendData(buffer, 0, n);
                            output.Write(buffer, 0, n);
                            size += n;
                            done += n;
                            var fraction = total == 0 ? 0 : 0.9 * done / total;
                            if (fraction - lastReported >= 0.002)
                            {
                                lastReported = fraction;
                                Report(fraction, item.Component switch
                                {
                                    Components.Core => "Copying Quake VR: Unleashed",
                                    Components.VisPatch => "Copying the VisPatch data",
                                    Components.Setup => "Copying Setup",
                                    _ => "Copying HD textures",
                                });
                            }
                        }
                    }
                    var sha = Convert.ToHexStringLower(hash.GetHashAndReset());
                    if (item.Sha256 is not null && (!string.Equals(sha, item.Sha256, StringComparison.OrdinalIgnoreCase) || size != item.Size))
                    {
                        throw new InstallException($"{item.Relative} is damaged in the package (its SHA-256 does not match the manifest). Download the package again.");
                    }
                    staged[^1] = (item, dest, temp, sha);
                }
                Report(0.9, "Checking", $"{items.Count} files copied and checked (SHA-256).", LogLevel.Success);

                // The files about to be overwritten that the player changed (or made, where this version ships one): into
                // the backup first, checked; nothing is moved into place before that is done.
                var backupFirst = maintenance?.BackupFirst.Where(p => File.Exists(PathUtil.SafeCombine(target, p))).ToList() ?? [];
                if (backupFirst.Count > 0)
                {
                    var backup = plan.Backup ?? Backup.Create(target, mode.ToString().ToLowerInvariant(), old?.Version, DateTimeOffset.Now);
                    foreach (var p in backupFirst)
                    {
                        backup.Copy(p, "program file changed by you");
                    }
                    LastBackup = backup;
                    Report(0.91, "Checking", $"{backupFirst.Count} file(s) you changed are replaced as shipped; your versions are in {backup.Dir}.", LogLevel.Warning);
                }
            }
            catch
            {
                foreach (var s in staged)
                {
                    TryDelete(s.Temp);
                }
                RemoveEmptyDirectories(createdDirs.Select(d => Path.Combine(target, d)));
                if (targetCreated)
                {
                    TryDeleteEmptyDirectory(target);
                }
                throw;
            }

            // Commit: quick renames, no longer cancellable.
            Report(0.92, "Finishing", "Moving the new files into place.");
            foreach (var s in staged)
            {
                File.Move(s.Temp, s.Dest, overwrite: true);
            }

            // An update or a repair from the console keeps the shortcuts as they are (it is given no shortcut folders).
            var keepShortcuts = old is not null && mode != InstallMode.Install && plan.Shortcuts.DesktopDir is null && plan.Shortcuts.StartMenuDir is null;
            // The first-start relight: a full install asks for it as ticked. An update asks again only when the relight's
            // inputs changed (a new light, VisPatch data, texture rules or HD texture pack); a repair never; a relight still
            // pending (the game has not started since) stays.
            var relightInputs = maintenance?.RelightInputsChanged.Count > 0 || plan.HdTexturesZip is not null || plan.VisPatchArchives.Count > 0;
            var relight = mode == InstallMode.Install
                ? plan.RelightOnFirstRun
                : FirstStartRelight.Pending(target) || (mode == InstallMode.Update && plan.RelightOnFirstRun && relightInputs);
            var record = new InstallRecord
            {
                Version = maintenance?.KeepsInstalledVersion == true ? old!.Version : manifest.Version,
                InstalledAt = old?.InstalledAt ?? DateTimeOffset.Now,
                UpdatedAt = old is null ? null : DateTimeOffset.Now,
                QuakeDir = quakeDir,
                QuakeStore = plan.QuakeStore ?? old?.QuakeStore,
                PackageSource = source.Location,
                Choices = new InstallChoices
                {
                    HdTextures = plan.HdTexturesZip is not null || (old?.Choices.HdTextures ?? false),
                    RelightOnFirstRun = plan.RelightOnFirstRun,
                    VisPatch = plan.VisPatchArchives.Count > 0 || (old?.Choices.VisPatch ?? false),
                    DesktopShortcut = keepShortcuts ? old!.Choices.DesktopShortcut : plan.Shortcuts.Desktop,
                    StartMenuShortcuts = keepShortcuts ? old!.Choices.StartMenuShortcuts : plan.Shortcuts.StartMenu,
                    FlatShortcut = keepShortcuts ? old!.Choices.FlatShortcut : plan.Shortcuts.Flat,
                    LogShortcut = keepShortcuts ? old!.Choices.LogShortcut : plan.Shortcuts.Log,
                },
                RelightPending = relight,
                HdTexturesFile = plan.HdTexturesZip is not null ? Path.GetFileName(plan.HdTexturesZip) : old?.HdTexturesFile,
                HdTexturesSha256 = hdSha ?? old?.HdTexturesSha256,
            };
            record.Files.AddRange(staged.Select(s => new InstalledFile { Path = s.Item.Relative, Size = new FileInfo(s.Dest).Length, Sha256 = s.Sha, Component = s.Item.Component }));
            if (maintenance is not null && mode != InstallMode.Install)
            {
                // The program files not copied: as shipped already (the manifest's), or kept as installed (a repair from an
                // older package, which keeps them recorded so that verify still sees a damaged one).
                var shippedFiles = manifest.Files.ToDictionary(f => f.Path, StringComparer.OrdinalIgnoreCase);
                foreach (var f in maintenance.Files)
                {
                    if (f.Action == PlannedAction.Unchanged)
                    {
                        var m = shippedFiles[f.Path];
                        record.Files.Add(new InstalledFile { Path = m.Path, Size = m.Size, Sha256 = m.Sha256, Component = Components.Core });
                    }
                    else if (f.Action is PlannedAction.KeepInstalled or PlannedAction.CannotRestore && oldRecorded.TryGetValue(f.Path, out var kept))
                    {
                        record.Files.Add(kept);
                    }
                }
            }
            record.Directories.AddRange((old?.Directories ?? []).Union(createdDirs, StringComparer.OrdinalIgnoreCase).Where(d => d.Length > 0));

            if (old is not null)
            {
                var now = record.Files.Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);
                var removed = 0;
                foreach (var f in old.Files.Where(f => !now.Contains(f.Path)))
                {
                    if ((f.Component == Components.HdTextures && plan.HdTexturesZip is null) ||
                        (f.Component == Components.VisPatch && plan.VisPatchArchives.Count == 0) ||
                        (f.Component == Components.Setup && (plan.SetupFiles.Count == 0 || keptSetup.Contains(f.Path))))
                    {
                        record.Files.Add(f); // textures or VisPatch data installed before and not reinstalled now: still ours
                        continue;
                    }
                    var path = PathUtil.SafeCombine(target, f.Path);
                    if (!File.Exists(path))
                    {
                        continue;
                    }
                    if (PackageManifest.HashFile(path) == f.Sha256)
                    {
                        File.Delete(path);
                        ++removed;
                    }
                    else
                    {
                        Report(0.93, "Finishing", $"Kept {f.Path}: no longer shipped, but you changed it.", LogLevel.Warning);
                    }
                }
                if (removed > 0)
                {
                    Report(0.93, "Finishing", $"Removed {removed} file(s) the previous version shipped and this one does not.");
                }
            }

            // Shortcuts.
            if (keepShortcuts)
            {
                record.Shortcuts.AddRange(old!.Shortcuts);
            }
            else
            {
                Report(0.95, "Creating shortcuts");
                var specs = ShortcutPlanner.Plan(plan.Shortcuts, quakeDir, target);
                foreach (var spec in specs)
                {
                    ShellLink.Save(spec);
                    record.Shortcuts.Add(spec.LinkPath);
                    Report(0.96, "Creating shortcuts", $"Shortcut: {spec.LinkPath}");
                }
                foreach (var stale in (old?.Shortcuts ?? []).Except(record.Shortcuts, StringComparer.OrdinalIgnoreCase))
                {
                    Uninstaller.RemoveShortcut(stale, target);
                }
            }

            // The relight at the first start, however the game is started: the game's own marker (FirstStartRelight).
            FirstStartRelight.Set(target, relight);
            if (mode != InstallMode.Install && old is not null && plan.RelightOnFirstRun)
            {
                Report(0.97, "Finishing", relight
                    ? relightInputs ? "The relight's inputs changed: the game relights the maps they affect at its next start (the rest are skipped)."
                                    : "The relight asked for before still runs at the game's next start."
                    : "Relit maps: the relight's inputs are unchanged, so no relight at the next start.");
            }
            if (relight && !File.Exists(Path.Combine(target, "quakevr", "tools", "ericw-tools", "light.exe")))
            {
                Report(0.98, "Finishing", "The package has no light.exe: the game will offer to download ericw-tools before relighting.", LogLevel.Warning);
            }
            record.Save(target);
            if (plan.Registry is not null)
            {
                RegisterUninstall(plan.Registry, target, record, Report);
            }
            Report(1, "Done", mode switch
            {
                InstallMode.Update => $"Quake VR: Unleashed updated to {record.Version} in {target}.",
                InstallMode.Repair => $"Quake VR: Unleashed {record.Version} repaired in {target}.",
                _ => $"Quake VR: Unleashed {record.Version} installed in {target}.",
            }, LogLevel.Success);
            return record;
        }
        finally
        {
            textures?.Dispose();
        }
    }

    /// <summary>The Apps &amp; Features entry, pointing at Setup's copy in the install. Never fatal.</summary>
    static void RegisterUninstall(IRegistryWriter registry, string target, InstallRecord record, Action<double, string, string?, LogLevel> report)
    {
        var setup = record.Files.FirstOrDefault(f => f.Component == Components.Setup &&
                                                    string.Equals(Path.GetFileName(f.Path), SetupCopy.ExeName, StringComparison.OrdinalIgnoreCase));
        if (setup is null)
        {
            report(0.99, "Finishing", "No copy of Setup in the install: no Apps & Features entry (remove it with Setup).", LogLevel.Warning);
            return;
        }
        try
        {
            UninstallEntry.Write(registry, target, record, PathUtil.SafeCombine(target, setup.Path));
            report(0.99, "Finishing", "Added to Windows' Installed apps (Apps & Features), with its Uninstall.", LogLevel.Info);
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Security.SecurityException)
        {
            report(0.99, "Finishing", $"Could not add the Apps & Features entry: {e.Message}", LogLevel.Warning);
        }
    }

    /// <summary>The HD texture pack's files that go in the Quake VR folder (a base dir, so the game mounts them):
    /// id1's always, a mission pack's only when the player owns it, never its copy of quakevr\textures_quetoo (the
    /// package ships it).</summary>
    static IEnumerable<(string Relative, ZipArchiveEntry Entry)> TextureEntries(ZipArchive zip, IReadOnlyCollection<string> ownedPacks)
    {
        var files = zip.Entries.Where(e => !e.FullName.EndsWith('/')).ToList();
        var tops = files.Select(e => e.FullName.Replace('\\', '/').Split('/')[0]).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        var prefix = tops.Count == 1 && !tops[0].Equals("id1", StringComparison.OrdinalIgnoreCase) ? tops[0] + "/" : "";
        foreach (var e in files)
        {
            var rel = e.FullName.Replace('\\', '/');
            if (!rel.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }
            rel = rel[prefix.Length..];
            var parts = rel.Split('/');
            if (parts.Length < 3 || !parts[1].Equals("textures", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }
            var game = parts[0].ToLowerInvariant();
            if (game == "id1" || ownedPacks.Contains(game, StringComparer.OrdinalIgnoreCase))
            {
                yield return (rel, e);
            }
        }
    }

    static void EnsureDirectory(string dir, string target, List<string> created)
    {
        var missing = new Stack<string>();
        for (var d = dir; d is not null && !Directory.Exists(d); d = Path.GetDirectoryName(d))
        {
            missing.Push(d);
        }
        while (missing.Count > 0)
        {
            var d = missing.Pop();
            Directory.CreateDirectory(d);
            if (PathUtil.IsInside(d, target) && !PathUtil.SamePath(d, target))
            {
                created.Add(PathUtil.ToRelative(target, d));
            }
        }
    }

    internal static void RemoveEmptyDirectories(IEnumerable<string> dirs)
    {
        foreach (var d in dirs.OrderByDescending(d => d.Length))
        {
            TryDeleteEmptyDirectory(d);
        }
    }

    internal static bool TryDeleteEmptyDirectory(string dir)
    {
        try
        {
            if (Directory.Exists(dir) && !Directory.EnumerateFileSystemEntries(dir).Any())
            {
                Directory.Delete(dir);
                return true;
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
        return false;
    }

    static void TryDelete(string file)
    {
        try
        {
            File.Delete(file);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
