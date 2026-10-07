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
    /// <summary>VisPatch's archives (.tgz), already checked against their pinned SHA-256: their data files go into
    /// quakevr\tools\vispatch, where the game's relight finds them (see-through water).</summary>
    public IReadOnlyList<string> VisPatchArchives { get; init; } = [];
    /// <summary>Mission packs the player owns ("hipnotic", "rogue"): only theirs get textures.</summary>
    public IReadOnlyCollection<string> OwnedPacks { get; init; } = [];
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
        if (old is not null)
        {
            Report(0, "Updating", $"Updating Quake VR {old.Version} in {target}; your settings, saves and maps are kept.");
        }

        var items = manifest.Files.Select(f => new StageItem(f.Path, Components.Core, () => source.Open(f.Path), f.Size,
            f.Sha256.Length == 64 ? f.Sha256 : null)).ToList();
        var oldRecorded = old?.Files.ToDictionary(f => f.Path, StringComparer.OrdinalIgnoreCase) ?? [];

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

            var record = new InstallRecord
            {
                Version = manifest.Version,
                InstalledAt = old?.InstalledAt ?? DateTimeOffset.Now,
                UpdatedAt = old is null ? null : DateTimeOffset.Now,
                QuakeDir = quakeDir,
                QuakeStore = plan.QuakeStore,
                PackageSource = source.Location,
                Choices = new InstallChoices
                {
                    HdTextures = plan.HdTexturesZip is not null || (old?.Choices.HdTextures ?? false),
                    RelightOnFirstRun = plan.RelightOnFirstRun,
                    VisPatch = plan.VisPatchArchives.Count > 0 || (old?.Choices.VisPatch ?? false),
                    DesktopShortcut = plan.Shortcuts.Desktop,
                    StartMenuShortcuts = plan.Shortcuts.StartMenu,
                    FlatShortcut = plan.Shortcuts.Flat,
                    LogShortcut = plan.Shortcuts.Log,
                },
                RelightPending = plan.RelightOnFirstRun,
            };
            record.Files.AddRange(staged.Select(s => new InstalledFile { Path = s.Item.Relative, Size = new FileInfo(s.Dest).Length, Sha256 = s.Sha, Component = s.Item.Component }));
            record.Directories.AddRange((old?.Directories ?? []).Union(createdDirs, StringComparer.OrdinalIgnoreCase).Where(d => d.Length > 0));

            if (old is not null)
            {
                var now = record.Files.Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);
                var removed = 0;
                foreach (var f in old.Files.Where(f => !now.Contains(f.Path)))
                {
                    if ((f.Component == Components.HdTextures && plan.HdTexturesZip is null) ||
                        (f.Component == Components.VisPatch && plan.VisPatchArchives.Count == 0))
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

            // The relight at the first start, however the game is started: the game's own marker (FirstStartRelight).
            FirstStartRelight.Set(target, plan.RelightOnFirstRun);
            if (plan.RelightOnFirstRun && !File.Exists(Path.Combine(target, "quakevr", "tools", "ericw-tools", "light.exe")))
            {
                Report(0.98, "Finishing", "The package has no light.exe: the game will offer to download ericw-tools before relighting.", LogLevel.Warning);
            }
            record.Save(target);
            Report(1, "Done", $"Quake VR: Unleashed {record.Version} installed in {target}.", LogLevel.Success);
            return record;
        }
        finally
        {
            textures?.Dispose();
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
