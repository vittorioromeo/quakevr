using System.IO.Compression;
using System.Security.Cryptography;

namespace QuakeVR.Installer.Core.Packaging;

public enum ModAction
{
    NotInstalled,
    Keep,
    Replace,
    Restore,
}

/// <summary>A third-party mod's folder (Arcane Dimensions, Dwell) found in an owned Quake root.</summary>
public enum ModCopyState
{
    /// <summary>The release the game plays, complete: it plays it where it is.</summary>
    Ready,
    /// <summary>Complete, but not that release (another version): the game refuses it.</summary>
    OtherVersion,
    /// <summary>Some of its data, not all (or a damaged pak).</summary>
    Incomplete,
}

public sealed record ModCopy(string Root, string Folder, ModCopyState State, string Detail)
{
    public bool Usable => State == ModCopyState.Ready;
}

/// <summary>What adding a third-party mod component did (never an exception: a failed or cancelled step leaves the rest
/// of the install as it is).</summary>
public sealed record ModOutcome(bool Installed, bool Cancelled, string Message);

/// <summary>
/// One optional third-party mod component the installer downloads from its authors' own hosts and unpacks as released
/// into <c>&lt;install&gt;\&lt;Folder&gt;</c>: Arcane Dimensions (<see cref="ArcaneDimensions.Spec"/>) and Dwell
/// (<see cref="Dwell.Spec"/>). Everything they share (checks, staging, install.json records, plan, the whole optional
/// step) is in <see cref="ModComponent"/>; a spec holds only what differs.
/// </summary>
/// <param name="Component">The install.json component name, the feed's <c>components.&lt;name&gt;</c> and the CLI's flag.</param>
/// <param name="Folder">The folder in the install (and in the game's roots).</param>
/// <param name="Title">The name shown to the player.</param>
/// <param name="Marker">A file the zip must have, relative to the folder (anything else is not this mod).</param>
/// <param name="Map">The zip entry (forward slashes) to its path under the folder; null: not installed. Null function:
/// a single top folder the zip has is that folder.</param>
/// <param name="GetPin">The file name and SHA-256 recorded for the installed zip.</param>
/// <param name="SetPin">Records (or, with nulls, clears) them.</param>
/// <param name="SetChoice">Records the install choice.</param>
public sealed record ModSpec(string Component, string Folder, string Title, string Marker,
    Func<string, string?>? Map,
    Func<InstallRecord, (string? File, string? Sha256)> GetPin,
    Action<InstallRecord, string?, string?> SetPin,
    Action<InstallRecord, bool> SetChoice,
    string Later);

public static class ModComponent
{
    /// <summary>The zip's files as they go in the install: each under <c>&lt;folder&gt;/</c>.</summary>
    public static List<(string Relative, ZipArchiveEntry Entry)> Entries(ModSpec spec, ZipArchive zip)
    {
        var files = zip.Entries.Where(e => !e.FullName.EndsWith('/') && !e.FullName.EndsWith('\\')).ToList();
        var names = files.Select(e => e.FullName.Replace('\\', '/')).ToList();
        var result = new List<(string, ZipArchiveEntry)>();
        if (spec.Map is null)
        {
            var tops = names.Select(n => n.Split('/')[0]).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            var prefix = tops.Count == 1 && names.All(n => n.Contains('/')) ? tops[0] + "/" : "";
            return [.. files.Select((e, i) => ($"{spec.Folder}/{names[i][prefix.Length..]}", e))];
        }
        for (var i = 0; i < files.Count; ++i)
        {
            if (spec.Map(names[i]) is { } rel)
            {
                result.Add(($"{spec.Folder}/{rel}", files[i]));
            }
        }
        return result;
    }

    /// <summary>Install-time and repair-time plan: what an update or repair does with the installed copy.</summary>
    public static (ModAction Action, string Text) Plan(ModSpec spec, string installDir, InstallRecord installed, FeedFile? target, bool verify)
    {
        var files = installed.Files.Where(f => f.Component == spec.Component).ToList();
        if (files.Count == 0)
        {
            return (ModAction.NotInstalled, $"{spec.Title}: not installed in Quake VR's folder");
        }
        if (verify)
        {
            var broken = files.Count(f =>
            {
                var p = PathUtil.SafeCombine(installDir, f.Path);
                return !File.Exists(p) || new FileInfo(p).Length != f.Size || !string.Equals(PackageManifest.HashFile(p), f.Sha256, StringComparison.OrdinalIgnoreCase);
            });
            if (broken > 0)
            {
                return target is not null
                    ? (ModAction.Restore, $"{spec.Title}: {broken} of {files.Count} files missing or changed: {target.File} downloaded again ({PathUtil.FormatSize(target.Size)})")
                    : (ModAction.Keep, $"{spec.Title}: {broken} of {files.Count} files missing or changed, and nothing to restore them from");
            }
        }
        if (target is null)
        {
            return (ModAction.Keep, $"{spec.Title}: kept ({files.Count} files)");
        }
        var (file, sha) = spec.GetPin(installed);
        return string.Equals(sha, target.Sha256, StringComparison.OrdinalIgnoreCase)
            ? (ModAction.Keep, $"{spec.Title}: unchanged ({file ?? target.File}): not downloaded")
            : (ModAction.Replace, $"{spec.Title}: {target.File} ({PathUtil.FormatSize(target.Size)}) replaces {file ?? "the installed copy"}");
    }

    /// <summary>
    /// Unpacks the checked zip into <c>&lt;install&gt;\&lt;folder&gt;</c> and records it (install.json, the spec's component).
    /// The zip must be exactly <paramref name="expected"/> (size, then SHA-256): anything else is refused. Files are staged
    /// as <c>*.qvrnew</c> and moved into place only when all are written, so a failure or a cancel leaves the install (and
    /// an earlier copy) as it was. A file of the player's there that Setup did not put there is never overwritten.
    /// </summary>
    public static InstallRecord Install(ModSpec spec, string installDir, string zipPath, FeedFile expected, IProgress<InstallProgress>? progress, CancellationToken ct)
    {
        void Report(double f, string status, string? log = null, LogLevel level = LogLevel.Info) =>
            progress?.Report(new InstallProgress(f, status, log, level));

        var title = spec.Title;
        var target = PathUtil.TryNormalize(installDir) ?? throw new InstallException($"bad install folder {installDir}");
        var record = InstallRecord.Load(target) ?? throw new InstallException($"No Quake VR install in {target}: install Quake VR first.");
        Report(0, $"Checking {title}", $"{title}: checking {Path.GetFileName(zipPath)}");
        var size = new FileInfo(zipPath).Length;
        if (size != expected.Size)
        {
            throw new InstallException($"{Path.GetFileName(zipPath)} is {size} bytes, not the {expected.Size} bytes of {expected.File}: not installed.");
        }
        var sha = PackageManifest.HashFile(zipPath);
        if (!string.Equals(sha, expected.Sha256, StringComparison.OrdinalIgnoreCase))
        {
            throw new InstallException($"{Path.GetFileName(zipPath)}'s SHA-256 is not {expected.File}'s: not installed.");
        }
        using var zip = ZipFile.OpenRead(zipPath);
        var entries = Entries(spec, zip);
        if (!entries.Any(e => e.Relative.Equals($"{spec.Folder}/{spec.Marker}", StringComparison.OrdinalIgnoreCase)))
        {
            throw new InstallException($"{Path.GetFileName(zipPath)} has no {spec.Marker}: not {title}.");
        }
        var old = record.Files.Where(f => f.Component == spec.Component).ToDictionary(f => f.Path, StringComparer.OrdinalIgnoreCase);
        var ownedPaths = record.Files.Where(f => f.Component != spec.Component).Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (var (rel, _) in entries)
        {
            var dest = PathUtil.SafeCombine(target, rel);
            if (File.Exists(dest) && !old.ContainsKey(rel))
            {
                throw new InstallException($"{rel} is already there and Setup did not put it there: move it away, then add {title} again.");
            }
            if (ownedPaths.Contains(rel))
            {
                throw new InstallException($"{rel} is a Quake VR program file: not overwritten.");
            }
        }
        var total = entries.Sum(e => e.Entry.Length);
        var drive = new DriveInfo(Path.GetPathRoot(target)!);
        if (drive.IsReady && drive.AvailableFreeSpace < total + (64L << 20))
        {
            throw new InstallException($"Not enough disk space on {drive.Name} for {title}: {PathUtil.FormatSize(total)} needed, {PathUtil.FormatSize(drive.AvailableFreeSpace)} free.");
        }

        var createdDirs = new List<string>();
        var staged = new List<(string Rel, string Dest, string Temp, long Size, string Sha)>();
        try
        {
            long done = 0;
            var buffer = new byte[1 << 20];
            foreach (var (rel, entry) in entries)
            {
                ct.ThrowIfCancellationRequested();
                var dest = PathUtil.SafeCombine(target, rel);
                InstallEngine.EnsureDirectory(Path.GetDirectoryName(dest)!, target, createdDirs);
                var temp = dest + ".qvrnew";
                staged.Add((rel, dest, temp, 0, ""));
                using var hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
                long n = 0;
                using (var input = entry.Open())
                using (var output = new FileStream(temp, FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16))
                {
                    int read;
                    while ((read = input.Read(buffer, 0, buffer.Length)) > 0)
                    {
                        ct.ThrowIfCancellationRequested();
                        hash.AppendData(buffer, 0, read);
                        output.Write(buffer, 0, read);
                        n += read;
                        done += read;
                    }
                }
                staged[^1] = (rel, dest, temp, n, Convert.ToHexStringLower(hash.GetHashAndReset()));
                Report(total == 0 ? 0.9 : 0.9 * done / total, $"Copying {title}");
            }
        }
        catch
        {
            foreach (var s in staged)
            {
                try
                {
                    File.Delete(s.Temp);
                }
                catch (Exception e) when (e is IOException or UnauthorizedAccessException)
                {
                }
            }
            InstallEngine.RemoveEmptyDirectories(createdDirs.Select(d => Path.Combine(target, d)));
            throw;
        }

        Report(0.92, $"Finishing {title}");
        foreach (var s in staged)
        {
            File.Move(s.Temp, s.Dest, overwrite: true);
        }
        var now = staged.Select(s => s.Rel).ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (var f in old.Values.Where(f => !now.Contains(f.Path)))
        {
            var p = PathUtil.SafeCombine(target, f.Path);
            if (File.Exists(p) && PackageManifest.HashFile(p) == f.Sha256)
            {
                File.Delete(p); // the previous copy's file this one does not have (unchanged: ours)
            }
        }
        record.Files.RemoveAll(f => f.Component == spec.Component);
        record.Files.AddRange(staged.Select(s => new InstalledFile { Path = s.Rel, Size = s.Size, Sha256 = s.Sha, Component = spec.Component }));
        record.Directories = [.. record.Directories.Union(createdDirs, StringComparer.OrdinalIgnoreCase).Where(d => d.Length > 0)];
        spec.SetPin(record, expected.File, sha);
        spec.SetChoice(record, true);
        record.Save(target);
        Report(1, $"{title} installed", $"{title}: {staged.Count} files ({PathUtil.FormatSize(total)}) in {Path.Combine(target, spec.Folder)}, as released.", LogLevel.Success);
        return record;
    }

    /// <summary>The record cleared of a component none of whose files is left (the uninstaller's partial case).</summary>
    public static void ClearIfGone(ModSpec spec, InstallRecord record)
    {
        if (!record.Files.Any(f => f.Component == spec.Component))
        {
            spec.SetPin(record, null, null);
            spec.SetChoice(record, false);
        }
    }

    /// <summary>
    /// The whole optional step, after the main install: the zip (a local one, else downloaded from the component's hosts
    /// into <paramref name="downloadsDir"/>), checked, then <see cref="Install"/>. Never throws: a failure or a cancel is
    /// an outcome with a message saying how to add it later, and the install stays as it is.
    /// </summary>
    public static async Task<ModOutcome> AddAsync(ModSpec spec, HttpClient http, string installDir, FeedFile component, string downloadsDir, string? localZip,
        IProgress<InstallProgress>? progress, IProgress<DownloadProgress>? download, CancellationToken ct, int attemptsPerMirror = 2)
    {
        var title = spec.Title;
        var later = $"Quake VR itself is installed. To add {title} later, run Setup again (its Update screen) and tick {title}.";
        try
        {
            var zip = localZip;
            if (zip is null)
            {
                zip = Path.Combine(downloadsDir, Path.GetFileName(component.File));
                progress?.Report(new InstallProgress(0, $"Downloading {title}", $"Downloading {title} ({component.File}, {PathUtil.FormatSize(component.Size)}) from {component.Mirrors[0].Host}."));
                await new Downloader(http).DownloadAsync(component.Mirrors, zip, component.Size, component.Sha256, download, ct, attemptsPerMirror).ConfigureAwait(false);
            }
            await Task.Run(() => Install(spec, installDir, zip, component, progress, ct), ct).ConfigureAwait(false);
            return new ModOutcome(true, false, $"{title} installed in {Path.Combine(installDir, spec.Folder)}.");
        }
        catch (OperationCanceledException)
        {
            return new ModOutcome(false, true, $"{title}: cancelled, nothing of it installed. {later}");
        }
        catch (Exception e) when (e is InstallException or HttpRequestException or IOException or UnauthorizedAccessException or InvalidDataException or UriFormatException)
        {
            return new ModOutcome(false, false, $"{title} not installed: {e.Message.TrimEnd('.')}. {later}");
        }
    }
}
