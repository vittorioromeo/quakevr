using System.Security.Cryptography;
using System.Text;
using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.Core.Quest;

public sealed class QuestPlan
{
    /// <summary>QuakeVR.apk (the release's quest component, or a local one).</summary>
    public required string ApkPath { get; init; }
    public required IReadOnlyList<QuestFile> Files { get; init; }
    /// <summary>One of several devices (adb's serial); null: the only one.</summary>
    public string? Serial { get; init; }
}

public sealed record QuestResult(string Device, bool ApkInstalled, int Pushed, int Skipped, long PushedBytes);

/// <summary>
/// Installs Quake VR on a Quest over adb: checks the headset (none, not allowed yet, several: <see cref="QuestDeviceException"/>),
/// installs the APK (<c>install -r</c>, which keeps the app's files; skipped when the same APK is installed), then copies
/// the files the headset lacks or holds in another version (size, then MD5 against the headset's own md5sum) by
/// <c>adb push</c> (several files of a folder a push: <see cref="PushBatches"/>), checks the free space first and every copied
/// file's size afterwards. It only ever adds and replaces the files it ships: nothing on the headset is deleted, and the
/// player's saves and configs (never shipped) are never touched. No command it sends removes anything.
/// </summary>
public sealed class QuestInstaller(IAdb adb)
{
    /// <summary>A shell command line is kept under this (Windows' command line is 32767 characters).</summary>
    const int MaxCommand = 16000;
    /// <summary>Kept free on the headset beyond the files.</summary>
    public const long SpaceMargin = 256L << 20;

    public static string Quote(string s) => "'" + s.Replace("'", "'\\''") + "'";

    public QuestDeviceStatus Detect(string? serial = null)
    {
        var r = adb.Run(["devices", "-l"], null, CancellationToken.None);
        if (r.ExitCode != 0)
        {
            throw new InstallException($"adb could not list the devices: {r.All}");
        }
        return QuestDeviceStatus.Parse(r.Output, serial);
    }

    AdbResult Shell(string serial, string command, CancellationToken ct) => adb.Run(["-s", serial, "shell", command], null, ct);

    public QuestResult Install(QuestPlan plan, IProgress<InstallProgress>? progress, CancellationToken ct)
    {
        void Report(double f, string status, string? log = null, LogLevel level = LogLevel.Info) =>
            progress?.Report(new InstallProgress(f, status, log, level));

        Report(0, "Looking for the Quest");
        var status = Detect(plan.Serial);
        if (!status.Ready)
        {
            throw new QuestDeviceException(status);
        }
        var serial = status.Device!.Serial;
        Report(0.01, "Looking for the Quest", status.Message, LogLevel.Success);
        var maker = Shell(serial, "getprop ro.product.manufacturer", ct).Output.Trim();
        if (!maker.Contains("Oculus", StringComparison.OrdinalIgnoreCase) && !maker.Contains("Meta", StringComparison.OrdinalIgnoreCase))
        {
            Report(0.01, "Looking for the Quest", $"This device's maker is \"{maker}\", not Meta: is it a Quest? Installing anyway.", LogLevel.Warning);
        }

        // The APK: install -r replaces the app and keeps its files folder (an uninstall would delete it: never done).
        ct.ThrowIfCancellationRequested();
        Report(0.02, "Installing the Quake VR app");
        var apkMd5 = Md5(plan.ApkPath);
        var installedApk = Shell(serial, $"pm path {QuestFiles.Package}", ct).Output.Split('\n').Select(l => l.Trim())
            .FirstOrDefault(l => l.StartsWith("package:", StringComparison.Ordinal))?["package:".Length..];
        var apkSame = installedApk is not null &&
                      Shell(serial, $"md5sum {Quote(installedApk)}", ct).Output.StartsWith(apkMd5, StringComparison.OrdinalIgnoreCase);
        if (apkSame)
        {
            Report(0.1, "Installing the Quake VR app", "The Quake VR app on the Quest is this version already.");
        }
        else
        {
            var r = adb.Run(["-s", serial, "install", "-r", plan.ApkPath], null, ct);
            if (!r.Output.Contains("Success", StringComparison.Ordinal) || r.ExitCode != 0)
            {
                throw new InstallException(ApkFailure(r.All));
            }
            Report(0.1, "Installing the Quake VR app", $"Quake VR app installed ({PathUtil.FormatSize(new FileInfo(plan.ApkPath).Length)}).", LogLevel.Success);
        }

        // What is there already: sizes, then MD5s of the files of the same size.
        ct.ThrowIfCancellationRequested();
        Report(0.11, "Comparing with the Quest's files");
        Shell(serial, $"mkdir -p {Quote(QuestFiles.DeviceRoot)}", ct);
        var roots = plan.Files.Select(f => f.DevicePath[..f.DevicePath.IndexOf('/')]).Distinct(StringComparer.Ordinal).ToList();
        var onDevice = ListSizes(serial, roots, ct);
        var sameSize = plan.Files.Where(f => onDevice.TryGetValue(f.DevicePath, out var s) && s == f.Size).ToList();
        var deviceMd5 = DeviceMd5(serial, sameSize.Select(f => f.DevicePath), ct);
        var toPush = new List<QuestFile>();
        var skipped = 0;
        foreach (var f in plan.Files)
        {
            if (deviceMd5.TryGetValue(f.DevicePath, out var h) && string.Equals(h, Md5(f.Source), StringComparison.OrdinalIgnoreCase))
            {
                ++skipped;
            }
            else
            {
                toPush.Add(f);
            }
        }
        var bytes = toPush.Sum(f => f.Size);
        Report(0.2, "Comparing with the Quest's files",
            $"{plan.Files.Count} files for the Quest: {skipped} already there and unchanged, {toPush.Count} to copy ({PathUtil.FormatSize(bytes)}).");

        if (toPush.Count > 0)
        {
            var free = FreeBytes(serial, ct);
            // Files replaced in place free their old size; counted only for those on the headset already.
            var replaced = toPush.Sum(f => onDevice.TryGetValue(f.DevicePath, out var s) ? s : 0);
            if (free is { } fb && fb + replaced < bytes + SpaceMargin)
            {
                throw new InstallException($"Not enough space on the Quest: {PathUtil.FormatSize(bytes)} to copy, " +
                                           $"{PathUtil.FormatSize(fb)} free (and {PathUtil.FormatSize(SpaceMargin)} kept free). Free some space in the headset, then try again.");
            }
            Push(serial, toPush, bytes, (f, s) => Report(0.2 + 0.75 * f, s), ct);

            // Readable by the game (what adb's shell writes is the shell's: Android/README.md).
            Report(0.96, "Finishing on the Quest");
            var dirs = toPush.SelectMany(f => Ancestors(f.DevicePath)).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal);
            foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && chmod 0777", dirs))
            {
                Shell(serial, cmd + " 2>/dev/null", ct);
            }
            foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && chmod 0666", toPush.Select(f => f.DevicePath)))
            {
                Shell(serial, cmd + " 2>/dev/null", ct);
            }

            // Checked: every copied file there, at its size.
            var after = ListSizes(serial, roots, ct);
            var bad = toPush.Where(f => !after.TryGetValue(f.DevicePath, out var s) || s != f.Size).ToList();
            if (bad.Count > 0)
            {
                var full = FreeBytes(serial, ct) is < SpaceMargin;
                throw new InstallException($"{bad.Count} file(s) did not arrive complete on the Quest (first: {bad[0].DevicePath})" +
                                           (full ? ": its storage is full. Free some space in the headset, then try again." : ". Check the cable, then try again: what arrived is kept and skipped."));
            }
            Report(0.99, "Finishing on the Quest", $"{toPush.Count} files copied to the Quest and checked.", LogLevel.Success);
        }
        Report(1, "Done", "Quake VR is on the Quest: start it from Library > Unknown Sources (the app list's filter, top right).", LogLevel.Success);
        return new QuestResult(status.Device.Model ?? serial, !apkSame, toPush.Count, skipped, bytes);
    }

    static string ApkFailure(string output)
    {
        var code = System.Text.RegularExpressions.Regex.Match(output, @"INSTALL_[A-Z_]+").Value;
        return code switch
        {
            "INSTALL_FAILED_UPDATE_INCOMPATIBLE" =>
                "The Quest has another build of Quake VR, signed differently, that cannot be updated in place. Uninstall it in the headset " +
                "(Library > Unknown Sources > Quake VR > Uninstall: this deletes its saves, so copy them first), then try again.",
            "INSTALL_FAILED_INSUFFICIENT_STORAGE" => "Not enough space on the Quest for the app. Free some space in the headset, then try again.",
            "INSTALL_FAILED_VERSION_DOWNGRADE" => "The Quest has a newer Quake VR app than this one: nothing was changed.",
            _ => $"The Quake VR app could not be installed on the Quest: {(output.Length > 0 ? output : "adb said nothing")}",
        };
    }

    static IEnumerable<string> Ancestors(string devicePath)
    {
        for (var i = devicePath.IndexOf('/'); i > 0; i = devicePath.IndexOf('/', i + 1))
        {
            yield return devicePath[..i];
        }
    }

    static IEnumerable<string> Batches(string prefix, IEnumerable<string> paths)
    {
        var sb = new StringBuilder(prefix);
        var any = false;
        foreach (var p in paths)
        {
            var q = " " + Quote(p);
            if (any && sb.Length + q.Length > MaxCommand)
            {
                yield return sb.ToString();
                sb.Clear().Append(prefix);
                any = false;
            }
            sb.Append(q);
            any = true;
        }
        if (any)
        {
            yield return sb.ToString();
        }
    }

    Dictionary<string, long> ListSizes(string serial, IReadOnlyList<string> roots, CancellationToken ct)
    {
        var result = new Dictionary<string, long>(StringComparer.Ordinal);
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && find", roots))
        {
            var r = Shell(serial, cmd + " -type f -exec stat -c '%s %n' {} + 2>/dev/null", ct);
            foreach (var line in r.Output.Split('\n'))
            {
                var l = line.TrimEnd('\r');
                var space = l.IndexOf(' ');
                if (space > 0 && long.TryParse(l[..space], out var size))
                {
                    result[l[(space + 1)..].TrimStart('.', '/')] = size;
                }
            }
        }
        return result;
    }

    Dictionary<string, string> DeviceMd5(string serial, IEnumerable<string> paths, CancellationToken ct)
    {
        var result = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && md5sum", paths))
        {
            foreach (var line in Shell(serial, cmd + " 2>/dev/null", ct).Output.Split('\n'))
            {
                var l = line.TrimEnd('\r');
                if (l.Length > 34 && l[32] == ' ')
                {
                    result[l[34..].TrimStart('.', '/')] = l[..32];
                }
            }
        }
        return result;
    }

    long? FreeBytes(string serial, CancellationToken ct)
    {
        var lines = Shell(serial, $"df -k {Quote(QuestFiles.DeviceRoot)}", ct).Output.Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var parts = lines.Length >= 2 ? lines[^1].Split(' ', StringSplitOptions.RemoveEmptyEntries) : [];
        return parts.Length >= 4 && long.TryParse(parts[3], out var kb) ? kb * 1024 : null;
    }

    /// <summary>The files by <c>adb push</c> (adb's sync protocol, flow-controlled): several files of one folder in one push
    /// when their names on the PC are those on the headset, the others one by one; the folders made first. Not one tar
    /// stream into <c>adb exec-in tar -x</c> (the first design): on the real Quest 3 adb's server takes the whole stream at
    /// once (2.8 GB in 3 s), the headset's tar got ~400 MB of it, the server died and the headset went offline, while
    /// adb exec-in exited 0 with no output (and toybox refused .NET 9's GNU and pax tar headers besides).</summary>
    void Push(string serial, IReadOnlyList<QuestFile> files, long total, Action<double, string> report, CancellationToken ct)
    {
        var dirs = files.Select(f => f.DevicePath[..f.DevicePath.LastIndexOf('/')]).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal);
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && mkdir -p", dirs))
        {
            Shell(serial, cmd, ct);
        }
        long done = 0;
        foreach (var batch in PushBatches(files))
        {
            ct.ThrowIfCancellationRequested();
            var first = batch[0];
            var target = batch.Count == 1 ? $"{QuestFiles.DeviceRoot}/{first.DevicePath}"
                                          : $"{QuestFiles.DeviceRoot}/{first.DevicePath[..first.DevicePath.LastIndexOf('/')]}/";
            report(total == 0 ? 1 : (double)done / total, $"Copying to the Quest: {first.What} ({PathUtil.FormatSize(done)} of {PathUtil.FormatSize(total)})");
            var r = adb.Run(["-s", serial, "push", .. batch.Select(f => f.Source), target], null, ct);
            if (r.ExitCode != 0)
            {
                if (r.All.Contains("No space left", StringComparison.OrdinalIgnoreCase))
                {
                    return; // the check after the copy reports what is missing and that the storage is full
                }
                throw new InstallException($"Copying to the Quest failed ({first.DevicePath}): {r.All.Trim()}");
            }
            done += batch.Sum(f => f.Size);
        }
        report(1, $"Copying to the Quest ({PathUtil.FormatSize(total)})");
    }

    /// <summary>The pushes: files of one headset folder whose PC names are their headset names (case included), up to
    /// <see cref="MaxCommand"/> characters of paths and 256 MB a push (the progress); any other file alone.</summary>
    public static List<List<QuestFile>> PushBatches(IReadOnlyList<QuestFile> files)
    {
        const long maxBytes = 256L << 20;
        var batches = new List<List<QuestFile>>();
        foreach (var group in files.GroupBy(f => f.DevicePath[..f.DevicePath.LastIndexOf('/')], StringComparer.Ordinal))
        {
            List<QuestFile>? cur = null;
            long bytes = 0, chars = 0;
            foreach (var f in group)
            {
                if (Path.GetFileName(f.Source) != f.DevicePath[(f.DevicePath.LastIndexOf('/') + 1)..])
                {
                    batches.Add([f]);
                    continue;
                }
                if (cur is null || bytes + f.Size > maxBytes || chars + f.Source.Length + 3 > MaxCommand)
                {
                    cur = [];
                    batches.Add(cur);
                    bytes = chars = 0;
                }
                cur.Add(f);
                bytes += f.Size;
                chars += f.Source.Length + 3;
            }
        }
        return batches;
    }

    static string Md5(string path)
    {
        using var s = File.OpenRead(path);
        return Convert.ToHexStringLower(MD5.HashData(s));
    }
}
