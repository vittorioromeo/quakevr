using System.Formats.Tar;
using System.IO.Pipes;
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
/// the files the headset lacks or holds in another version (size, then MD5 against the headset's own md5sum) as one tar
/// stream (<c>exec-in tar</c>: one stream, as Android/README.md does it), checks the free space first and every copied
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
            foreach (var f in toPush)
            {
                TarEntryFor(f.DevicePath); // (every path one the headset's tar takes, before anything is sent)
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

    /// <summary>The files as one tar stream into <c>tar -x</c> on the headset (paths relative to the files folder).</summary>
    void Push(string serial, IReadOnlyList<QuestFile> files, long total, Action<double, string> report, CancellationToken ct)
    {
        using var server = new AnonymousPipeServerStream(PipeDirection.Out);
        using var client = new AnonymousPipeClientStream(PipeDirection.In, server.ClientSafePipeHandle);
        long done = 0;
        long lastReported = 0;
        var producer = Task.Run(() =>
        {
            try
            {
                using var tar = new TarWriter(server, TarEntryFormat.Ustar, leaveOpen: true);
                foreach (var f in files)
                {
                    ct.ThrowIfCancellationRequested();
                    using var data = new CountingStream(File.OpenRead(f.Source), n =>
                    {
                        done += n;
                        if (done - lastReported < total / 500 && done != total)
                        {
                            return;
                        }
                        lastReported = done;
                        report(total == 0 ? 1 : (double)done / total, $"Copying to the Quest: {f.What} ({PathUtil.FormatSize(done)} of {PathUtil.FormatSize(total)})");
                    });
                    var entry = TarEntryFor(f.DevicePath);
                    entry.DataStream = data;
                    entry.Mode = UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.GroupRead | UnixFileMode.GroupWrite |
                                 UnixFileMode.OtherRead | UnixFileMode.OtherWrite;
                    entry.ModificationTime = File.GetLastWriteTimeUtc(f.Source);
                    tar.WriteEntry(entry);
                }
            }
            catch (IOException)
            {
                // tar on the headset stopped reading (full, unplugged): the check after the copy reports it.
            }
            finally
            {
                server.Dispose();
            }
        }, ct);
        var r = adb.Run(["-s", serial, "exec-in", $"tar -xf - -C {Quote(QuestFiles.DeviceRoot)}"], client, ct);
        client.Dispose(); // adb stopped reading (done, or failed): the writer's next write fails instead of waiting
        try
        {
            producer.GetAwaiter().GetResult();
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        if (r.ExitCode != 0)
        {
            throw new InstallException($"Copying to the Quest failed: {r.All}");
        }
    }

    /// <summary>A tar entry the headset's tar (toybox) reads: ustar. Never GNU: .NET 9 writes a GNU entry's access and
    /// change times where ustar keeps its prefix, and toybox refuses the whole stream ("tar: Not tar") while adb exec-in
    /// still exits 0 (the real-headset test: 566 files sent, none arrived). Nor pax (.NET 9's: "tar: bad header" on the
    /// Quest 3). A path ustar cannot hold (over 100 bytes with no '/' splitting it into a 155-byte prefix and a 100-byte
    /// name; the longest the install sends is ~60) is refused here, before anything is sent.</summary>
    public static TarEntry TarEntryFor(string devicePath)
    {
        var bytes = Encoding.UTF8.GetByteCount(devicePath);
        var fits = bytes <= 100;
        for (var i = devicePath.IndexOf('/'); !fits && i > 0; i = devicePath.IndexOf('/', i + 1))
        {
            fits = Encoding.UTF8.GetByteCount(devicePath[..i]) <= 155 && Encoding.UTF8.GetByteCount(devicePath[(i + 1)..]) <= 100;
        }
        return fits ? new UstarTarEntry(TarEntryType.RegularFile, devicePath)
                    : throw new InstallException($"A path too long to copy to the Quest: {devicePath}");
    }

    static string Md5(string path)
    {
        using var s = File.OpenRead(path);
        return Convert.ToHexStringLower(MD5.HashData(s));
    }

    sealed class CountingStream(Stream inner, Action<long> onRead) : Stream
    {
        public override bool CanRead => true;
        public override bool CanSeek => inner.CanSeek;
        public override bool CanWrite => false;
        public override long Length => inner.Length;
        public override long Position { get => inner.Position; set => inner.Position = value; }
        public override void Flush() { }
        public override int Read(byte[] buffer, int offset, int count)
        {
            var n = inner.Read(buffer, offset, count);
            if (n > 0)
            {
                onRead(n);
            }
            return n;
        }
        public override long Seek(long offset, SeekOrigin origin) => inner.Seek(offset, origin);
        public override void SetLength(long value) => throw new NotSupportedException();
        public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                inner.Dispose();
            }
            base.Dispose(disposing);
        }
    }
}
