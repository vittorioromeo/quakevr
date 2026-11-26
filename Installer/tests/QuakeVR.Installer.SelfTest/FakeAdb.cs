using System.Formats.Tar;
using System.Security.Cryptography;
using System.Text;
using QuakeVR.Installer.Core.Quest;

namespace QuakeVR.Installer.SelfTest;

/// <summary>
/// A fake adb for the Quest target's tests: records every command and answers like adb and the headset's shell would,
/// with the headset's app files folder (<see cref="QuestFiles.DeviceRoot"/>) as a folder on this PC. It understands the
/// commands QuestInstaller sends (devices, install, push, getprop, pm path, md5sum, mkdir, find/stat, chmod, df, mv, rm,
/// echo, exec-in tar) and simulates no device, an unauthorized one, several, a failed install, a full disk, a disk filling
/// mid-copy, folders the game made, and adbd's "remote fchown failed" (its target unlinked; /data/local/tmp as another folder).
/// </summary>
sealed class FakeAdb(string deviceFiles) : IAdb
{
    public const string Serial = "1WMHH000000000";
    public string DeviceFiles { get; } = Directory.CreateDirectory(deviceFiles).FullName;
    public List<string> Commands { get; } = [];
    public string Devices { get; set; } = $"List of devices attached\n{Serial}          device usb:1-1 product:eureka model:Quest_3 device:eureka transport_id:1\n\n";
    public string Maker { get; set; } = "Oculus";
    /// <summary>adb install's answer; null: Success.</summary>
    public string? InstallFailure { get; set; }
    public string? InstalledApkMd5 { get; private set; }
    public int Installs { get; private set; }
    /// <summary>What df says is free.</summary>
    public long FreeBytes { get; set; } = 100L << 30;
    /// <summary>The bytes tar can still write before the disk is full (null: no limit; df then says what is left).</summary>
    public long? WriteQuota { get; set; }
    public int TarFiles { get; private set; }
    public List<string> Chmodded { get; } = [];
    /// <summary>Folders the game made itself (u0_aNNN, 770): adb push into them is refused (the real Quest's quakevr/doom).</summary>
    public HashSet<string> GameFolders { get; } = new(StringComparer.Ordinal);
    /// <summary>adb push into the app's folder fails "remote fchown failed" after unlinking its target (questb6's Quest 3).</summary>
    public bool FchownFails { get; set; }
    public int FchownFailures { get; private set; }
    /// <summary>adb's shell's own folder (/data/local/tmp), another filesystem: a mv from it into the app's folder copies.</summary>
    public const string TmpRoot = "/data/local/tmp";
    public string TmpFiles { get; } = Directory.CreateDirectory(deviceFiles + "-tmp").FullName;
    /// <summary>Every path a shell rm/mv removed or moved away (the headset's paths as sent).</summary>
    public List<string> Removed { get; } = [];

    public AdbResult Run(IReadOnlyList<string> args, Stream? stdin, CancellationToken ct)
    {
        Commands.Add(string.Join(" ", args));
        if (args is ["devices", "-l"])
        {
            return Ok(Devices);
        }
        if (args.Count < 3 || args[0] != "-s" || args[1] != Serial)
        {
            return new AdbResult(1, "", "error: device not found");
        }
        switch (args[2])
        {
            case "install":
                ++Installs;
                if (InstallFailure is { } failure)
                {
                    return new AdbResult(1, "Performing Streamed Install", $"adb: failed to install {args[^1]}: Failure [{failure}]");
                }
                using (var s = File.OpenRead(args[^1]))
                {
                    InstalledApkMd5 = Convert.ToHexStringLower(MD5.HashData(s));
                }
                return Ok("Performing Streamed Install\nSuccess\n");
            case "exec-in":
                return Tar(args[3], stdin!);
            case "push":
                return Push([.. args.Skip(3).SkipLast(1)], args[^1]);
            case "shell":
                return Shell(args[3]);
        }
        return new AdbResult(1, "", $"unknown command {args[2]}");
    }

    static AdbResult Ok(string output) => new(0, output, "");

    AdbResult Tar(string command, Stream stdin)
    {
        var words = Words(command);
        if (words is not ["tar", "-xf", "-", "-C", var dir] || dir != QuestFiles.DeviceRoot)
        {
            return new AdbResult(1, "", $"unexpected tar command {command}");
        }
        using var reader = new TarReader(stdin);
        var buffer = new byte[65536];
        while (reader.GetNextEntry() is { } entry)
        {
            // (the Quest's toybox tar refuses .NET 9's GNU entries, their access/change times where ustar keeps its prefix:
            // "tar: Not tar", the whole stream, and adb exec-in still exits 0)
            if (entry.Format is not TarEntryFormat.Ustar) // (nor .NET 9 pax: "tar: bad header")
            {
                return Ok("");
            }
            var path = Local(entry.Name);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            using var output = File.Create(path);
            ++TarFiles;
            if (entry.DataStream is null)
            {
                continue;
            }
            int n;
            while ((n = entry.DataStream.Read(buffer, 0, buffer.Length)) > 0)
            {
                var take = WriteQuota is { } q ? (int)Math.Min(q, n) : n;
                output.Write(buffer, 0, take);
                if (WriteQuota is not null)
                {
                    WriteQuota -= take;
                    if (WriteQuota == 0)
                    {
                        FreeBytes = 0;
                        return new AdbResult(0, "", "tar: write: No space left on device");
                    }
                }
            }
        }
        return Ok("");
    }

    /// <summary>adb push: one file to a path, or several into a folder ("dir/"), which must exist (as adb's), in the app's
    /// files folder or /data/local/tmp; counts each file written in <see cref="TarFiles"/>; a full disk stops it ("No space
    /// left on device", exit 1). As adbd: the target is unlinked before it is written, and stays unlinked when the push
    /// then fails (<see cref="FchownFails"/>: the real Quest 3's "remote fchown failed" in the app's folder).</summary>
    AdbResult Push(List<string> sources, string target)
    {
        var intoDir = target.EndsWith('/');
        var dest = Resolve(target.TrimEnd('/'));
        if (dest is null)
        {
            return new AdbResult(1, "", $"adb: error: unexpected target {target}");
        }
        if ((sources.Count > 1 && !intoDir) || (intoDir && !Directory.Exists(dest)))
        {
            return new AdbResult(1, "", $"adb: error: target '{target}' is not a directory");
        }
        foreach (var source in sources)
        {
            var path = intoDir ? Path.Combine(dest, Path.GetFileName(source)) : dest;
            var rel = DeviceRel(path);
            if (rel is not null && GameFolders.Contains(DirOf(rel)))
            {
                return new AdbResult(1, "", $"adb: error: failed to copy '{source}' to '{target}': remote couldn't create file: Permission denied");
            }
            File.Delete(path); // (adbd: "don't delete files before copying if they are not regular": these are)
            if (rel is not null && FchownFails)
            {
                ++FchownFailures;
                return new AdbResult(1, "", $"adb: error: failed to copy '{source}' to '{target}': remote fchown failed: Operation not permitted");
            }
            using var input = File.OpenRead(source);
            using var output = File.Create(path);
            if (!Copy(input, output))
            {
                return new AdbResult(1, "", $"adb: error: failed to copy '{source}' to '{target}': remote No space left on device");
            }
            ++TarFiles;
        }
        return Ok($"{sources.Count} files pushed.\n");
    }

    /// <summary>Writes, within <see cref="WriteQuota"/>: false when the disk filled up (df then says 0).</summary>
    bool Copy(Stream input, Stream output)
    {
        var buffer = new byte[65536];
        int n;
        while ((n = input.Read(buffer, 0, buffer.Length)) > 0)
        {
            var take = WriteQuota is { } q ? (int)Math.Min(q, n) : n;
            output.Write(buffer, 0, take);
            if (WriteQuota is not null)
            {
                WriteQuota -= take;
                if (WriteQuota == 0 && (take < n || input.Position < input.Length))
                {
                    FreeBytes = 0;
                    return false;
                }
            }
        }
        return true;
    }

    string Local(string devicePath) => Path.GetFullPath(Path.Combine(DeviceFiles, devicePath.Replace('/', '\\')));

    /// <summary>A headset path (absolute in the app's folder or /data/local/tmp, or relative to the app's folder) on this PC; null elsewhere.</summary>
    string? Resolve(string path)
    {
        if (path == QuestFiles.DeviceRoot)
        {
            return DeviceFiles;
        }
        if (path.StartsWith(QuestFiles.DeviceRoot + "/", StringComparison.Ordinal))
        {
            return Local(path[(QuestFiles.DeviceRoot.Length + 1)..]);
        }
        if (path.StartsWith(TmpRoot + "/", StringComparison.Ordinal))
        {
            return Path.GetFullPath(Path.Combine(TmpFiles, path[(TmpRoot.Length + 1)..].Replace('/', '\\')));
        }
        return path.StartsWith('/') ? null : Local(path);
    }

    /// <summary>A local path's headset path relative to the app's folder; null outside it (/data/local/tmp).</summary>
    string? DeviceRel(string local)
    {
        var rel = Path.GetRelativePath(DeviceFiles, local);
        return rel.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(rel) ? null : rel.Replace('\\', '/');
    }

    static string DirOf(string rel) => rel.Contains('/') ? rel[..rel.LastIndexOf('/')] : "";

    AdbResult Shell(string command)
    {
        var words = Words(command);
        if (words.Count >= 3 && words[0] == "cd" && words[2] == "&&")
        {
            if (words[1] != QuestFiles.DeviceRoot)
            {
                return new AdbResult(1, "", "cd: unexpected folder");
            }
            words = words[3..];
        }
        else if (words is ["cd", var root, "||", "exit", "1", ";", ..])
        {
            if (root != QuestFiles.DeviceRoot)
            {
                return new AdbResult(1, "", "cd: unexpected folder");
            }
            words = words[6..];
        }
        words = [.. words.Where(w => w != "2>/dev/null")];
        if (words.Any(w => w is ";" or "&&" or "||") || words[0] is "rm" or "mv" or "echo")
        {
            return Compound(words);
        }
        var sb = new StringBuilder();
        switch (words[0])
        {
            case "getprop":
                return Ok(words[1] == "ro.product.manufacturer" ? Maker + "\n" : "\n");
            case "pm" when words is ["pm", "path", var package]:
                return Ok(package == QuestFiles.Package && InstalledApkMd5 is not null ? "package:/data/app/~~x/" + package + "/base.apk\n" : "");
            case "mkdir":
                foreach (var d in words.Skip(1).Where(w => !w.StartsWith('-')))
                {
                    Directory.CreateDirectory(Resolve(d) ?? throw new Exception($"mkdir outside the fake headset: {d}"));
                }
                return Ok("");
            case "df":
                return Ok($"Filesystem     1K-blocks      Used Available Use% Mounted on\n/dev/fuse      110000000  50000000 {FreeBytes / 1024} 45% /storage/emulated\n");
            case "md5sum":
                foreach (var p in words.Skip(1))
                {
                    if (p.StartsWith("/data/app/", StringComparison.Ordinal))
                    {
                        sb.Append($"{InstalledApkMd5}  {p}\n");
                    }
                    else if (File.Exists(Local(p)))
                    {
                        using var s = File.OpenRead(Local(p));
                        sb.Append($"{Convert.ToHexStringLower(MD5.HashData(s))}  {p}\n");
                    }
                }
                return Ok(sb.ToString());
            case "chmod":
                Chmodded.AddRange(words.Skip(2));
                return Ok("");
            case "chgrp" when words.Count > 2 && words[1] == "ext_data_rw":
                return Ok("");
            case "find":
                var exec = words.IndexOf("-type");
                if (exec < 0 || words[exec..] is not ["-type", "f", "-exec", "stat", "-c", "%s %n", "{}", "+"])
                {
                    return new AdbResult(1, "", "find: unexpected arguments");
                }
                foreach (var root in words[1..exec])
                {
                    var dir = Local(root);
                    if (!Directory.Exists(dir))
                    {
                        continue;
                    }
                    foreach (var f in Directory.EnumerateFiles(dir, "*", SearchOption.AllDirectories))
                    {
                        sb.Append($"{new FileInfo(f).Length} {Path.GetRelativePath(DeviceFiles, f).Replace('\\', '/')}\n");
                    }
                }
                return Ok(sb.ToString());
        }
        return new AdbResult(127, "", $"/system/bin/sh: {words[0]}: not handled by the fake");
    }

    /// <summary>A shell line of statements (";") of commands joined by "&&" and "||" (left to right, as sh): mv [-f], echo,
    /// rm -f / -rf, exit. A mv from /data/local/tmp into the app's folder copies (another filesystem: within
    /// <see cref="WriteQuota"/>, a cut-short copy left behind as toybox would), into a folder the game made is refused.</summary>
    AdbResult Compound(List<string> words)
    {
        var output = new StringBuilder();
        var error = new StringBuilder();
        var status = 0;
        var start = 0;
        string? op = null;
        for (var i = 0; i <= words.Count; ++i)
        {
            if (i < words.Count && words[i] is not (";" or "&&" or "||"))
            {
                continue;
            }
            var cmd = words[start..i];
            if (cmd.Count > 0 && (op is null or ";" || (op == "&&" && status == 0) || (op == "||" && status != 0)))
            {
                if (cmd is ["exit", var code])
                {
                    return new AdbResult(int.Parse(code), output.ToString(), error.ToString());
                }
                status = Simple(cmd, output, error);
            }
            op = i < words.Count ? words[i] : null;
            start = i + 1;
        }
        return new AdbResult(status, output.ToString(), error.ToString());
    }

    int Simple(List<string> cmd, StringBuilder output, StringBuilder error)
    {
        switch (cmd[0])
        {
            case "echo":
                output.Append(string.Join(" ", cmd.Skip(1))).Append('\n');
                return 0;
            case "rm" when cmd.Count > 1 && cmd[1] is "-f" or "-rf":
                foreach (var p in cmd.Skip(2))
                {
                    var local = Resolve(p) ?? throw new Exception($"rm outside the fake headset: {p}");
                    if (File.Exists(local))
                    {
                        File.Delete(local);
                        Removed.Add(p);
                    }
                    else if (Directory.Exists(local))
                    {
                        if (cmd[1] != "-rf")
                        {
                            error.Append($"rm: {p}: is a directory\n");
                            return 1;
                        }
                        Directory.Delete(local, true);
                        Removed.Add(p);
                    }
                }
                return 0;
            case "mv" when cmd.Count == 4 && cmd[1] == "-f":
                var from = Resolve(cmd[2]);
                var to = Resolve(cmd[3]);
                if (from is null || to is null || !File.Exists(from))
                {
                    error.Append($"mv: {cmd[2]}: No such file or directory\n");
                    return 1;
                }
                if (DeviceRel(to) is { } rel && (GameFolders.Contains(DirOf(rel)) || !Directory.Exists(Path.GetDirectoryName(to))))
                {
                    error.Append($"mv: {cmd[3]}: Permission denied\n");
                    return 1;
                }
                Removed.Add(cmd[2]);
                if (DeviceRel(from) is null && DeviceRel(to) is not null)
                {
                    using (var input = File.OpenRead(from))
                    using (var copy = File.Create(to))
                    {
                        if (!Copy(input, copy))
                        {
                            error.Append($"mv: {cmd[3]}: No space left on device\n");
                            return 1;
                        }
                    }
                    File.Delete(from);
                    return 0;
                }
                File.Move(from, to, overwrite: true);
                return 0;
        }
        error.Append($"/system/bin/sh: {cmd[0]}: not handled by the fake\n");
        return 127;
    }

    /// <summary>The shell's words: spaces split, '...' quotes (with '\'' inside).</summary>
    public static List<string> Words(string command)
    {
        var words = new List<string>();
        var sb = new StringBuilder();
        var inWord = false;
        for (var i = 0; i < command.Length; ++i)
        {
            var c = command[i];
            if (c == '\'')
            {
                var end = command.IndexOf('\'', i + 1);
                sb.Append(command, i + 1, end - i - 1);
                i = end;
                inWord = true;
            }
            else if (c == '\\' && i + 1 < command.Length)
            {
                sb.Append(command[++i]);
                inWord = true;
            }
            else if (c == ' ')
            {
                if (inWord)
                {
                    words.Add(sb.ToString());
                    sb.Clear();
                    inWord = false;
                }
            }
            else
            {
                sb.Append(c);
                inWord = true;
            }
        }
        if (inWord)
        {
            words.Add(sb.ToString());
        }
        return words;
    }
}
