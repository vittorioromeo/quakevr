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

public sealed record QuestResult(string Device, bool ApkInstalled, int Pushed, int Skipped, long PushedBytes)
{
    /// <summary>Files not copied: their folder on the headset was made by the game itself (u0_aNNN, mode 770), which adb's
    /// shell user cannot write into (e.g. quakevr/doom, made when Doom was opened before its WADs were there).</summary>
    public IReadOnlyList<string> NotWritable { get; init; } = [];
}

/// <summary>
/// Installs Quake VR on a Quest over adb: checks the headset (none, not allowed yet, several: <see cref="QuestDeviceException"/>),
/// installs the APK (<c>install -r</c>, which keeps the app's files; skipped when the same APK is installed), then copies
/// the files the headset lacks or holds in another version (size, then MD5 against the headset's own md5sum) by
/// <c>adb push</c> (several files of a folder a push: <see cref="PushBatches"/>) into a staging folder, then moves each into
/// place (<see cref="Push"/>), checks the free space first and every copied file's size afterwards. It only ever adds and
/// replaces the files it ships, each only once its new copy is complete: a failed copy leaves the headset's file as it was,
/// and the player's saves and configs (never shipped) are never touched. The only things it removes are its own: the
/// staging folders (<see cref="Stage"/>, <see cref="TmpStage"/>) and a half-moved copy (<see cref="PartSuffix"/>).
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

        List<QuestFile> refused = [];
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
            var pushed = Push(serial, toPush, bytes, (f, s, log) => Report(0.2 + 0.75 * f, s, log, LogLevel.Warning), ct);
            refused = pushed.Refused;
            Report(0.96, "Finishing on the Quest");
            MakeReadable(serial, plan.Files, ct);

            // Checked: every copied file moved into place, there at its size (a file not moved in may still be its older
            // copy, of the same size).
            var after = ListSizes(serial, roots, ct);
            var bad = toPush.Where(f => !refused.Contains(f) &&
                                        (!pushed.Placed.Contains(f.DevicePath) || !after.TryGetValue(f.DevicePath, out var s) || s != f.Size)).ToList();
            if (bad.Count > 0)
            {
                var full = pushed.Full || FreeBytes(serial, ct) is < SpaceMargin;
                throw new InstallException($"{bad.Count} file(s) did not arrive complete on the Quest (first: {bad[0].DevicePath})" +
                                           (full ? ": its storage is full. Free some space in the headset, then try again." : ". Check the cable, then try again: what arrived is kept and skipped.") +
                                           " The Quest's files are only replaced by complete copies: those not copied are as they were.");
            }
            Report(0.99, "Finishing on the Quest", $"{toPush.Count - refused.Count} files copied to the Quest and checked.", LogLevel.Success);
            if (refused.Count > 0)
            {
                Report(0.99, "Finishing on the Quest", NotWritableText(refused), LogLevel.Warning);
            }
        }
        else
        {
            MakeReadable(serial, plan.Files, ct); // (nothing new, but an older copy's folders may still be unreadable)
        }
        Report(1, "Done", "Quake VR is on the Quest: start it from Library > Unknown Sources (the app list's filter, top right).", LogLevel.Success);
        return new QuestResult(status.Device.Model ?? serial, !apkSame, toPush.Count - refused.Count, skipped, bytes - refused.Sum(f => f.Size))
        {
            NotWritable = [.. refused.Select(f => f.DevicePath)],
        };
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

    /// <summary>Every planned file and every folder on its way readable by the game (what adb's shell writes is the shell's:
    /// Android/README.md), on every run, the files already there and skipped too. Only the copied files' folders before
    /// (2026-11-24, questmods): a folder a push or an older Setup had left 2770 (hipnotic, ad, dopa, dwellv2p2 on his Quest 3)
    /// kept its files, unchanged and so skipped, out of the game's reach: "hipnotic missing", and Quoth's start failed
    /// "Mod_LoadModel: progs/s_exp_small.spr not found" (Arcane Dimensions' sprite: Quoth plays by AD's rules).</summary>
    void MakeReadable(string serial, IReadOnlyList<QuestFile> files, CancellationToken ct)
    {
        var dirs = files.SelectMany(f => Ancestors(f.DevicePath)).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal);
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && chmod 2777", dirs)) // (set-group-ID kept: Push)
        {
            Shell(serial, cmd + " 2>/dev/null", ct);
        }
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && chmod 0666", files.Select(f => f.DevicePath)))
        {
            Shell(serial, cmd + " 2>/dev/null", ct);
        }
    }

    static IEnumerable<string> Ancestors(string devicePath)
    {
        for (var i = devicePath.IndexOf('/'); i > 0; i = devicePath.IndexOf('/', i + 1))
        {
            yield return devicePath[..i];
        }
    }

    static IEnumerable<string> Batches(string prefix, IEnumerable<string> paths) => Joined(prefix, paths.Select(Quote), " ");

    /// <summary>Command lines under <see cref="MaxCommand"/>: <paramref name="prefix"/>, then the items, each after <paramref name="separator"/>.</summary>
    static IEnumerable<string> Joined(string prefix, IEnumerable<string> items, string separator)
    {
        var sb = new StringBuilder(prefix);
        var any = false;
        foreach (var item in items)
        {
            var q = separator + item;
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

    /// <summary>The files by <c>adb push</c> (adb's sync protocol, flow-controlled): several files of one headset folder in one push
    /// when their names on the PC are those on the headset, the others one by one; the folders made first. Not one tar
    /// stream into <c>adb exec-in tar -x</c> (the first design): on the real Quest 3 adb's server takes the whole stream at
    /// once (2.8 GB in 3 s), the headset's tar got ~400 MB of it, the server died and the headset went offline, while
    /// adb exec-in exited 0 with no output (and toybox refused .NET 9's GNU and pax tar headers besides).
    /// Never onto the file itself: adbd unlinks a push's target before writing it and leaves it unlinked when the push fails
    /// (questpush, 2026-11-26; seen by questb6 on the real Quest 3: "remote fchown failed: Operation not permitted", 24 files
    /// lost in one push; a pulled cable mid-push does the same). Each batch goes into <see cref="Stage"/>, then each file is
    /// moved beside its target (".name.qvrpart") and renamed over it: a target is only replaced by a complete copy. When adb
    /// cannot push into the app's folder at all (that fchown failure, which then unlinks only the staged copy), this batch
    /// and every later one go through <see cref="TmpStage"/> (the shell's own folder): the move is then a copy (another
    /// filesystem) into the ".qvrpart" name, so a copy cut short (a full storage) still leaves the target as it was.</summary>
    PushOutcome Push(string serial, IReadOnlyList<QuestFile> files, long total, Action<double, string, string?> report, CancellationToken ct)
    {
        var stageName = Stage[(QuestFiles.DeviceRoot.Length + 1)..];
        Shell(serial, StageCleanup + " 2>/dev/null", ct); // (what a cancelled run left)
        var dirs = files.Select(f => Dir(f.DevicePath)).Append(stageName).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal);
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && mkdir -p", dirs))
        {
            Shell(serial, cmd, ct);
        }
        // adb push into Android/data needs its folders in the storage's group, set-group-ID (as files/ is, so folders made
        // in it are): in a folder of the shell's group (an older "chmod 0777" cleared the bit) every push failed "remote
        // fchown failed", and a folder adb push makes itself "secure_mkdirs failed" (the real Quest 3). Every folder on the
        // way: the storage's group, mode 2777 (the shell's own folders; the app's refuse quietly and need neither).
        var all = files.SelectMany(f => Ancestors(f.DevicePath)).Append(stageName).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal).ToList();
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && chgrp {StorageGroup}", all))
        {
            Shell(serial, cmd + " 2>/dev/null", ct);
        }
        foreach (var cmd in Batches($"cd {Quote(QuestFiles.DeviceRoot)} && chmod 2777", all))
        {
            Shell(serial, cmd + " 2>/dev/null", ct);
        }
        long done = 0;
        var outcome = new PushOutcome([], [], false);
        var viaTmp = false;
        try
        {
            foreach (var batch in PushBatches(files))
            {
                ct.ThrowIfCancellationRequested();
                var first = batch[0];
                var status = $"Copying to the Quest: {first.What} ({PathUtil.FormatSize(done)} of {PathUtil.FormatSize(total)})";
                report(total == 0 ? 1 : (double)done / total, status, null);
                var r = PushInto(serial, batch, viaTmp ? TmpStage : Stage, ct);
                if (r.ExitCode != 0 && !viaTmp && !NoSpace(r.All))
                {
                    report(total == 0 ? 1 : (double)done / total, status,
                        $"adb could not copy into the app's folder ({r.All.Trim()}): copying through {TmpStage} instead. Nothing on the Quest was lost.");
                    viaTmp = true;
                    Shell(serial, $"mkdir -p {Quote(TmpStage)}", ct);
                    r = PushInto(serial, batch, TmpStage, ct);
                }
                if (r.ExitCode != 0)
                {
                    if (NoSpace(r.All))
                    {
                        return outcome with { Full = true }; // the check after the copy reports what is missing and that the storage is full
                    }
                    throw new InstallException($"Copying to the Quest failed ({first.DevicePath}): {r.All.Trim()}. Nothing on the Quest was replaced or removed.");
                }
                var errors = new StringBuilder();
                foreach (var cmd in Joined($"cd {Quote(QuestFiles.DeviceRoot)} || exit 1", batch.Select(f => MoveInto(viaTmp ? TmpStage : Stage, f)), " ; "))
                {
                    foreach (var line in Shell(serial, cmd, ct).All.Split('\n'))
                    {
                        var l = line.TrimEnd('\r');
                        if (l.StartsWith(PlacedMark, StringComparison.Ordinal))
                        {
                            outcome.Placed.Add(l[PlacedMark.Length..]);
                        }
                        else if (l.Length > 0)
                        {
                            errors.AppendLine(l);
                        }
                    }
                }
                var missed = batch.Where(f => !outcome.Placed.Contains(f.DevicePath)).ToList();
                if (missed.Count > 0)
                {
                    var err = errors.ToString().Trim();
                    if (NoSpace(err))
                    {
                        return outcome with { Full = true };
                    }
                    if (!err.Contains("Permission denied", StringComparison.OrdinalIgnoreCase))
                    {
                        throw new InstallException($"Moving the copied files into place on the Quest failed ({missed[0].DevicePath}): " +
                                                   $"{(err.Length > 0 ? err : "adb said nothing")}. Those files are as they were.");
                    }
                    outcome.Refused.AddRange(missed); // a folder the game made (NotWritable): the others still go
                }
                done += batch.Sum(f => f.Size);
            }
        }
        finally
        {
            try
            {
                Shell(serial, StageCleanup + " 2>/dev/null", CancellationToken.None);
            }
            catch (InstallException)
            {
                // adb gone: the next run clears them first
            }
        }
        report(1, $"Copying to the Quest ({PathUtil.FormatSize(total)})", null);
        return outcome;
    }

    /// <summary>One push of a batch into a staging folder (under the files' headset names), again after a transient failure:
    /// adb's server restarted under it (another adb version on the PC, e.g. an SDK's or Meta's tools, takes the server
    /// over; seen on the real headset: "cannot connect to daemon").</summary>
    AdbResult PushInto(string serial, List<QuestFile> batch, string stage, CancellationToken ct)
    {
        var target = batch.Count == 1 ? $"{stage}/{FileName(batch[0].DevicePath)}" : $"{stage}/";
        var r = adb.Run(["-s", serial, "push", .. batch.Select(f => f.Source), target], null, ct);
        for (var attempt = 1; r.ExitCode != 0 && attempt <= PushRetries && Transient(r.All); ++attempt)
        {
            ct.WaitHandle.WaitOne(TimeSpan.FromSeconds(3 * attempt));
            ct.ThrowIfCancellationRequested();
            r = adb.Run(["-s", serial, "push", .. batch.Select(f => f.Source), target], null, ct);
        }
        return r;
    }

    /// <summary>The shell commands that move a staged file into place: beside its target first, then renamed over it, and
    /// the marker printed; anything failing removes the half-moved copy (never the target).</summary>
    static string MoveInto(string stage, QuestFile f)
    {
        var part = $"{Dir(f.DevicePath)}/.{FileName(f.DevicePath)}{PartSuffix}";
        return $"mv -f {Quote($"{stage}/{FileName(f.DevicePath)}")} {Quote(part)} && mv -f {Quote(part)} {Quote(f.DevicePath)} && " +
               $"echo {Quote(PlacedMark + f.DevicePath)} || rm -f {Quote(part)}";
    }

    static string Dir(string devicePath) => devicePath[..devicePath.LastIndexOf('/')];
    static string FileName(string devicePath) => devicePath[(devicePath.LastIndexOf('/') + 1)..];
    static bool NoSpace(string adbOutput) => adbOutput.Contains("No space left", StringComparison.OrdinalIgnoreCase);

    /// <summary>What <see cref="Push"/> did: the files moved into place (headset paths), those a folder the game made kept out,
    /// and whether it stopped on a full storage.</summary>
    sealed record PushOutcome(HashSet<string> Placed, List<QuestFile> Refused, bool Full);

    /// <summary>Setup's staging folder in the app's files folder: the files' own storage, so moving out of it is a rename.
    /// Only Setup writes there; it is removed after every copy (and before, for what a cancelled run left).</summary>
    public const string Stage = QuestFiles.DeviceRoot + "/.quakevr-setup";
    /// <summary>The staging folder when adb cannot push into the app's folder (adb's shell's own folder).</summary>
    public const string TmpStage = "/data/local/tmp/quakevr-setup";
    /// <summary>A copy moved beside its target before the rename over it (".name.qvrpart").</summary>
    public const string PartSuffix = ".qvrpart";
    const string PlacedMark = "QVR_PLACED ";
    static readonly string StageCleanup = $"rm -rf {Quote(Stage)} {Quote(TmpStage)}";

    /// <summary>The warning for files a folder the game made kept out (<see cref="QuestResult.NotWritable"/>).</summary>
    public static string NotWritableText(IEnumerable<QuestFile> files)
    {
        var list = files.ToList();
        var dirs = list.Select(f => f.DevicePath[..f.DevicePath.LastIndexOf('/')]).Distinct(StringComparer.Ordinal).ToList();
        return $"{list.Count} file(s) not copied: the game itself made {string.Join(", ", dirs)} on the Quest, and only the game can " +
               "write there. Everything else was copied. Uninstalling the app in the headset clears it (with its saves: copy them first).";
    }

    const int PushRetries = 4;

    /// <summary>Android/data's group (ext_data_rw), which adb's shell user is in.</summary>
    const string StorageGroup = "ext_data_rw";

    /// <summary>adb lost its server or the headset for a moment (not a refusal by the headset).</summary>
    public static bool Transient(string adbOutput) =>
        new[] { "cannot connect to daemon", "daemon not running", "device offline", "no devices/emulators found", "device not found", "protocol fault", "connection reset" }
            .Any(s => adbOutput.Contains(s, StringComparison.OrdinalIgnoreCase));

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
