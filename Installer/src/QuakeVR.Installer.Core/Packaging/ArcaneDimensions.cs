using System.Buffers.Binary;
using System.IO.Compression;
using System.Security.Cryptography;
using QuakeVR.Installer.Core.Detection;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>An Arcane Dimensions folder found in an owned Quake root.</summary>
public enum AdCopyState
{
    /// <summary>1.80 patch 1 ("1.81"), complete: the game plays it where it is.</summary>
    Ready,
    /// <summary>Complete, but not 1.80 patch 1 (no pak2.pak, or another one): the game refuses it.</summary>
    OtherVersion,
    /// <summary>Some of its data, not all (or a damaged pak).</summary>
    Incomplete,
}

public sealed record AdCopy(string Root, string Folder, AdCopyState State, string Detail)
{
    public bool Usable => State == AdCopyState.Ready;
}

/// <summary>The release's pak2.pak as the engine checks it (vr_ad.cpp <c>versionStatus</c>): its size, and the SHA-256
/// of its 12-byte header followed by its directory.</summary>
public sealed record AdPak2Pin(long Size, string DirSha256);

public enum AdAction
{
    NotInstalled,
    Keep,
    Replace,
    Restore,
}

/// <summary>What adding Arcane Dimensions did (never an exception: a failed or cancelled AD step leaves the rest of the
/// install as it is).</summary>
public sealed record AdOutcome(bool Installed, bool Cancelled, string Message);

/// <summary>
/// Arcane Dimensions, the installer's experimental opt-in component (docs/vr-port/ARCANE_DIMENSIONS.md, section 10):
/// sock's unmodified release, <c>ad_v1_80p1final.zip</c> (<see cref="BuiltInComponents.ArcaneDimensions"/>), downloaded
/// from its own hosts, checked (size and SHA-256), and unpacked as it is into <c>&lt;install&gt;\ad</c> (the Quake VR
/// folder is the game's last -basedir, so one of the owned roots the game finds AD in). Its files are recorded in
/// install.json as component <c>ad</c>: an update keeps them, a repair restores them, the uninstall removes them. A copy
/// of 1.80 patch 1 already in an owned Quake root is used where it is instead (nothing downloaded).
/// </summary>
public static class ArcaneDimensions
{
    public const string Folder = "ad";
    public const string Title = "Arcane Dimensions";
    /// <summary>The engine's pin (Quake/vr/vr_ad_data.inc: VR_AD_PAK2_SIZE, VR_AD_PAK2_DIRSHA; the self-test compares).</summary>
    public static readonly AdPak2Pin Latest = new(22_309_198, "38c6fcb87d8aa70c1bec9e3ad75b81c6f07266ed478555f7ae793e9a8172a34a");
    /// <summary>1.80 patch 1's files unpacked (the three paks, quake.rc, the txt, def and fgd files).</summary>
    public const long InstalledSize = 830_590_195;
    /// <summary>The release's page (its readme, credits and links to sock's site).</summary>
    public const string HomeUrl = "https://www.quaddicted.com/reviews/ad_v1_80p1final.html";
    public const string ReadmeFile = "ad_v1_80_readme.txt";
    public const string Credit = "free, by sock and team";

    /// <summary>pak2.pak's size and header-and-directory SHA-256, or null when it is missing or not a pak.</summary>
    public static (long Size, string DirSha256)? Pak2Signature(string pak2)
    {
        try
        {
            using var f = File.OpenRead(pak2);
            var size = f.Length;
            var header = new byte[12];
            if (f.Read(header, 0, 12) != 12 || header[0] != 'P' || header[1] != 'A' || header[2] != 'C' || header[3] != 'K')
            {
                return null;
            }
            var offset = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4));
            var length = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(8));
            if (offset < 12 || length <= 0 || length >= 1 << 20 || offset > size - length)
            {
                return null;
            }
            var bytes = new byte[12 + length];
            header.CopyTo(bytes, 0);
            f.Seek(offset, SeekOrigin.Begin);
            f.ReadExactly(bytes, 12, length);
            return (size, Convert.ToHexStringLower(SHA256.HashData(bytes)));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    /// <summary>Whether this ad folder's pak2.pak is 1.80 patch 1's (the engine's check).</summary>
    public static bool IsLatest(string adFolder, AdPak2Pin? pin = null)
    {
        pin ??= Latest;
        return Pak2Signature(Path.Combine(adFolder, "pak2.pak")) is { } s && s.Size == pin.Size &&
               string.Equals(s.DirSha256, pin.DirSha256, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>The owned roots an existing copy is looked for in: the stores', then the Quake folder's (the engine's
    /// <c>ownedRoots()</c> without the Quake VR folder, which holds only the copy Setup installs), lowest priority first.</summary>
    public static IReadOnlyList<string> SearchRoots(IReadOnlyList<string> storeRoots, string quakeDir) =>
        ExpansionDetector.OwnedRoots(storeRoots, [quakeDir]);

    /// <summary>The copy the game would use, by its rules (Quake/vr/vr_gamedir.cpp <c>discoverCampaigns</c>): the
    /// highest-priority root whose ad folder has any of AD's data; then its state. Null: none.</summary>
    public static AdCopy? FindExisting(IReadOnlyList<string> ownedRoots, AdPak2Pin? pin = null)
    {
        var resources = PackLists.Resources(Folder);
        for (var r = ownedRoots.Count - 1; r >= 0; --r)
        {
            var folder = Path.Combine(ownedRoots[r], Folder);
            if (!Directory.Exists(folder))
            {
                continue;
            }
            var status = PackInspector.Inspect(Folder, resources, [ownedRoots[r]]);
            if (status.Status == PackStatus.Missing)
            {
                continue; // none of its data: a lower root's copy counts
            }
            var root = PathUtil.TryNormalize(ownedRoots[r]) ?? ownedRoots[r];
            var where = PathUtil.TryNormalize(folder) ?? folder;
            if (status.Status == PackStatus.Incomplete)
            {
                return new AdCopy(root, where, AdCopyState.Incomplete, $"incomplete ({status.Detail})");
            }
            return IsLatest(folder, pin)
                ? new AdCopy(root, where, AdCopyState.Ready, "1.80 patch 1")
                : new AdCopy(root, where, AdCopyState.OtherVersion, File.Exists(Path.Combine(folder, "pak2.pak"))
                    ? "not 1.80 patch 1 (its pak2.pak is another release's)" : "not 1.80 patch 1 (no pak2.pak: 1.80 or older)");
        }
        return null;
    }

    /// <summary>The line the Options page and the console show for a found copy.</summary>
    public static string Describe(AdCopy copy) => copy.State switch
    {
        AdCopyState.Ready => $"Found in {copy.Folder}: will be used (nothing downloaded).",
        AdCopyState.OtherVersion => $"Found in {copy.Folder}, but it isn't 1.80 patch 1, the only release Quake VR plays: tick to download 1.80 patch 1 into Quake VR's folder (your copy is left as it is).",
        _ => $"Found in {copy.Folder}, {copy.Detail}: tick to download a complete copy into Quake VR's folder (yours is left as it is).",
    };

    /// <summary>The zip's files as they go in the install: each under <c>ad/</c> (a single top folder the zip has, such as
    /// its own <c>ad/</c>, is that folder).</summary>
    public static List<(string Relative, ZipArchiveEntry Entry)> Entries(ZipArchive zip)
    {
        var files = zip.Entries.Where(e => !e.FullName.EndsWith('/') && !e.FullName.EndsWith('\\')).ToList();
        var names = files.Select(e => e.FullName.Replace('\\', '/')).ToList();
        var tops = names.Select(n => n.Split('/')[0]).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        var prefix = tops.Count == 1 && names.All(n => n.Contains('/')) ? tops[0] + "/" : "";
        return [.. files.Select((e, i) => ($"{Folder}/{names[i][prefix.Length..]}", e))];
    }

    /// <summary>Install-time and repair-time plan: what an update or repair does with the installed copy.</summary>
    public static (AdAction Action, string Text) Plan(string installDir, InstallRecord installed, FeedFile? target, bool verify)
    {
        var files = installed.Files.Where(f => f.Component == Components.ArcaneDimensions).ToList();
        if (files.Count == 0)
        {
            return (AdAction.NotInstalled, "Arcane Dimensions: not installed in Quake VR's folder");
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
                    ? (AdAction.Restore, $"Arcane Dimensions: {broken} of {files.Count} files missing or changed: {target.File} downloaded again ({PathUtil.FormatSize(target.Size)})")
                    : (AdAction.Keep, $"Arcane Dimensions: {broken} of {files.Count} files missing or changed, and nothing to restore them from");
            }
        }
        if (target is null)
        {
            return (AdAction.Keep, $"Arcane Dimensions: kept ({files.Count} files)");
        }
        return string.Equals(installed.ArcaneDimensionsSha256, target.Sha256, StringComparison.OrdinalIgnoreCase)
            ? (AdAction.Keep, $"Arcane Dimensions: unchanged ({installed.ArcaneDimensionsFile ?? target.File}): not downloaded")
            : (AdAction.Replace, $"Arcane Dimensions: {target.File} ({PathUtil.FormatSize(target.Size)}) replaces {installed.ArcaneDimensionsFile ?? "the installed copy"}");
    }

    /// <summary>
    /// Unpacks the checked zip into <c>&lt;install&gt;\ad</c> and records it (install.json, component <c>ad</c>). The zip
    /// must be exactly <paramref name="expected"/> (size, then SHA-256): anything else is refused. Files are staged as
    /// <c>*.qvrnew</c> and moved into place only when all are written, so a failure or a cancel leaves the install (and
    /// an earlier AD copy) as it was. A file of the player's in ad\ that Setup did not put there is never overwritten.
    /// </summary>
    public static InstallRecord Install(string installDir, string zipPath, FeedFile expected, IProgress<InstallProgress>? progress, CancellationToken ct)
    {
        void Report(double f, string status, string? log = null, LogLevel level = LogLevel.Info) =>
            progress?.Report(new InstallProgress(f, status, log, level));

        var target = PathUtil.TryNormalize(installDir) ?? throw new InstallException($"bad install folder {installDir}");
        var record = InstallRecord.Load(target) ?? throw new InstallException($"No Quake VR install in {target}: install Quake VR first.");
        Report(0, "Checking Arcane Dimensions", $"Arcane Dimensions: checking {Path.GetFileName(zipPath)}");
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
        var entries = Entries(zip);
        if (!entries.Any(e => e.Relative.Equals($"{Folder}/pak0.pak", StringComparison.OrdinalIgnoreCase)))
        {
            throw new InstallException($"{Path.GetFileName(zipPath)} has no pak0.pak: not Arcane Dimensions.");
        }
        var old = record.Files.Where(f => f.Component == Components.ArcaneDimensions).ToDictionary(f => f.Path, StringComparer.OrdinalIgnoreCase);
        var ownedPaths = record.Files.Where(f => f.Component != Components.ArcaneDimensions).Select(f => f.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);
        foreach (var (rel, _) in entries)
        {
            var dest = PathUtil.SafeCombine(target, rel);
            if (File.Exists(dest) && !old.ContainsKey(rel))
            {
                throw new InstallException($"{rel} is already there and Setup did not put it there: move it away, then add Arcane Dimensions again.");
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
            throw new InstallException($"Not enough disk space on {drive.Name} for Arcane Dimensions: {PathUtil.FormatSize(total)} needed, {PathUtil.FormatSize(drive.AvailableFreeSpace)} free.");
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
                Report(total == 0 ? 0.9 : 0.9 * done / total, "Copying Arcane Dimensions");
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

        Report(0.92, "Finishing Arcane Dimensions");
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
        record.Files.RemoveAll(f => f.Component == Components.ArcaneDimensions);
        record.Files.AddRange(staged.Select(s => new InstalledFile { Path = s.Rel, Size = s.Size, Sha256 = s.Sha, Component = Components.ArcaneDimensions }));
        record.Directories = [.. record.Directories.Union(createdDirs, StringComparer.OrdinalIgnoreCase).Where(d => d.Length > 0)];
        record.ArcaneDimensionsFile = expected.File;
        record.ArcaneDimensionsSha256 = sha;
        record.Choices.ArcaneDimensions = true;
        record.Save(target);
        Report(1, "Arcane Dimensions installed", $"Arcane Dimensions: {staged.Count} files ({PathUtil.FormatSize(total)}) in {Path.Combine(target, Folder)}, as released (its readme: {ReadmeFile}).", LogLevel.Success);
        return record;
    }

    /// <summary>
    /// The whole optional step, after the main install: the zip (a local one, else downloaded from the component's hosts
    /// into <paramref name="downloadsDir"/>), checked, then <see cref="Install"/>. Never throws: a failure or a cancel is
    /// an outcome with a message saying how to add it later (Setup again: the Update screen's Arcane Dimensions box, or
    /// <c>qvr-setup ad</c>), and the install stays as it is.
    /// </summary>
    public static async Task<AdOutcome> AddAsync(HttpClient http, string installDir, FeedFile component, string downloadsDir, string? localZip,
        IProgress<InstallProgress>? progress, IProgress<DownloadProgress>? download, CancellationToken ct, int attemptsPerMirror = 2)
    {
        const string later = "Quake VR itself is installed. To add Arcane Dimensions later, run Setup again (its Update screen) and tick Arcane Dimensions.";
        try
        {
            var zip = localZip;
            if (zip is null)
            {
                zip = Path.Combine(downloadsDir, Path.GetFileName(component.File));
                progress?.Report(new InstallProgress(0, "Downloading Arcane Dimensions", $"Downloading Arcane Dimensions ({component.File}, {PathUtil.FormatSize(component.Size)}) from {component.Mirrors[0].Host}."));
                await new Downloader(http).DownloadAsync(component.Mirrors, zip, component.Size, component.Sha256, download, ct, attemptsPerMirror).ConfigureAwait(false);
            }
            await Task.Run(() => Install(installDir, zip, component, progress, ct), ct).ConfigureAwait(false);
            return new AdOutcome(true, false, $"Arcane Dimensions installed in {Path.Combine(installDir, Folder)}.");
        }
        catch (OperationCanceledException)
        {
            return new AdOutcome(false, true, $"Arcane Dimensions: cancelled, nothing of it installed. {later}");
        }
        catch (Exception e) when (e is InstallException or HttpRequestException or IOException or UnauthorizedAccessException or InvalidDataException or UriFormatException)
        {
            return new AdOutcome(false, false, $"Arcane Dimensions not installed: {e.Message.TrimEnd('.')}. {later}");
        }
    }
}
