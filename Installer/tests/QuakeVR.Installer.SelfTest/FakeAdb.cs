using System.Formats.Tar;
using System.Security.Cryptography;
using System.Text;
using QuakeVR.Installer.Core.Quest;

namespace QuakeVR.Installer.SelfTest;

/// <summary>
/// A fake adb for the Quest target's tests: records every command and answers like adb and the headset's shell would,
/// with the headset's app files folder (<see cref="QuestFiles.DeviceRoot"/>) as a folder on this PC. It understands the
/// commands QuestInstaller sends (devices, install, getprop, pm path, md5sum, mkdir, find/stat, chmod, df, exec-in tar)
/// and simulates no device, an unauthorized one, several, a failed install, a full disk and a disk filling mid-copy.
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

    string Local(string devicePath) => Path.GetFullPath(Path.Combine(DeviceFiles, devicePath.Replace('/', '\\')));

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
        words = [.. words.Where(w => w != "2>/dev/null")];
        var sb = new StringBuilder();
        switch (words[0])
        {
            case "getprop":
                return Ok(words[1] == "ro.product.manufacturer" ? Maker + "\n" : "\n");
            case "pm" when words is ["pm", "path", var package]:
                return Ok(package == QuestFiles.Package && InstalledApkMd5 is not null ? "package:/data/app/~~x/" + package + "/base.apk\n" : "");
            case "mkdir":
                Directory.CreateDirectory(Local(words[^1] == QuestFiles.DeviceRoot ? "" : words[^1]));
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
