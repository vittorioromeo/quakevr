using System.Diagnostics;
using System.IO.Compression;
using System.Text;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Prerequisites;

namespace QuakeVR.Installer.Core.Quest;

/// <summary>What one adb command printed, and its exit code.</summary>
public sealed record AdbResult(int ExitCode, string Output, string Error)
{
    public string All => (Output + "\n" + Error).Trim();
}

/// <summary>
/// adb, behind an interface: the real one runs Google's adb.exe (<see cref="ProcessAdb"/>); the self-tests use a fake
/// that records every command and simulates a headset in a folder. <paramref name="stdin"/> (exec-in's tar stream) is
/// copied to adb's standard input, then closed.
/// </summary>
public interface IAdb
{
    AdbResult Run(IReadOnlyList<string> args, Stream? stdin, CancellationToken ct);
}

/// <summary>Google's adb.exe as a child process (no window, output captured).</summary>
public sealed class ProcessAdb(string adbPath) : IAdb
{
    public string Path { get; } = adbPath;

    public AdbResult Run(IReadOnlyList<string> args, Stream? stdin, CancellationToken ct)
    {
        var psi = new ProcessStartInfo(Path)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            RedirectStandardInput = stdin is not null,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8,
        };
        foreach (var a in args)
        {
            psi.ArgumentList.Add(a);
        }
        using var p = Process.Start(psi) ?? throw new InstallException($"adb could not be started ({Path})");
        var output = p.StandardOutput.ReadToEndAsync(ct);
        var error = p.StandardError.ReadToEndAsync(ct);
        if (stdin is not null)
        {
            try
            {
                stdin.CopyTo(p.StandardInput.BaseStream, 1 << 20);
            }
            catch (IOException)
            {
                // adb closed its end (the device went away): its output says why.
            }
            finally
            {
                try
                {
                    p.StandardInput.Close();
                }
                catch (IOException)
                {
                }
            }
        }
        p.WaitForExitAsync(ct).GetAwaiter().GetResult();
        return new AdbResult(p.ExitCode, output.GetAwaiter().GetResult(), error.GetAwaiter().GetResult());
    }
}

/// <summary>
/// Finds adb.exe (a path given, the cache folder, PATH, the Android SDK's platform-tools) or fetches Google's official
/// platform-tools zip from dl.google.com into the cache folder.
/// </summary>
public static class AdbLocator
{
    /// <summary>Google's official SDK Platform Tools for Windows, always the latest (developer.android.com/tools/releases/platform-tools).</summary>
    public const string PlatformToolsUrl = "https://dl.google.com/android/repository/platform-tools-latest-windows.zip";

    /// <summary>The files platform-tools needs beside adb.exe on Windows.</summary>
    static readonly string[] AdbFiles = ["adb.exe", "AdbWinApi.dll", "AdbWinUsbApi.dll"];

    /// <summary>The cache folder's adb: <c>&lt;cache&gt;\platform-tools\adb.exe</c>.</summary>
    public static string CachedAdb(string cacheDir) => System.IO.Path.Combine(cacheDir, "platform-tools", "adb.exe");

    public static string? Find(string? explicitPath, string? cacheDir, Func<string, string?>? env = null)
    {
        env ??= Environment.GetEnvironmentVariable;
        if (explicitPath is not null)
        {
            return File.Exists(explicitPath) ? System.IO.Path.GetFullPath(explicitPath) : null;
        }
        var candidates = new List<string>();
        if (cacheDir is not null)
        {
            candidates.Add(CachedAdb(cacheDir));
        }
        foreach (var dir in (env("PATH") ?? "").Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            candidates.Add(System.IO.Path.Combine(dir, "adb.exe"));
        }
        foreach (var sdk in new[] { env("ANDROID_HOME"), env("ANDROID_SDK_ROOT"),
                     env("LOCALAPPDATA") is { } la ? System.IO.Path.Combine(la, "Android", "Sdk") : null })
        {
            if (!string.IsNullOrEmpty(sdk))
            {
                candidates.Add(System.IO.Path.Combine(sdk, "platform-tools", "adb.exe"));
            }
        }
        foreach (var c in candidates)
        {
            try
            {
                if (File.Exists(c))
                {
                    return System.IO.Path.GetFullPath(c);
                }
            }
            catch (ArgumentException)
            {
                // a junk PATH entry
            }
        }
        return null;
    }

    /// <summary>Downloads platform-tools (<paramref name="url"/>: Google's; tests pass a local server's) into
    /// <paramref name="cacheDir"/> and unpacks adb.exe and its two DLLs into <c>platform-tools\</c>. There is no pinned
    /// hash (Google publishes the latest under one name): the HTTPS download from dl.google.com is the trust, and
    /// adb.exe's Authenticode signature is logged (<paramref name="verifier"/>; null: not checked).</summary>
    public static async Task<string> FetchAsync(HttpClient http, string cacheDir, IProgress<InstallProgress>? progress, CancellationToken ct,
        string url = PlatformToolsUrl, ISignatureVerifier? verifier = null)
    {
        Directory.CreateDirectory(cacheDir);
        var zip = System.IO.Path.Combine(cacheDir, "platform-tools-latest-windows.zip");
        progress?.Report(new InstallProgress(0, "Downloading adb (Google's platform-tools)", $"Downloading Google's platform-tools from {url}"));
        var dl = new Progress<DownloadProgress>(p => progress?.Report(new InstallProgress(
            p.Total is > 0 ? Math.Min(1, (double)p.Received / p.Total.Value) : 0, $"Downloading adb: {PathUtil.FormatSize(p.Received)}")));
        if (File.Exists(zip))
        {
            File.Move(zip, zip + ".old", overwrite: true); // "latest" changes: never resumed from an older one
        }
        await new Downloader(http).DownloadAsync([new Uri(url)], zip, null, null, dl, ct);
        var dest = System.IO.Path.Combine(cacheDir, "platform-tools");
        Directory.CreateDirectory(dest);
        using (var archive = ZipFile.OpenRead(zip))
        {
            foreach (var name in AdbFiles)
            {
                var entry = archive.GetEntry("platform-tools/" + name)
                            ?? throw new InstallException($"The platform-tools download has no {name}: not Google's zip?");
                var path = System.IO.Path.Combine(dest, name);
                entry.ExtractToFile(path + ".part", overwrite: true);
                File.Move(path + ".part", path, overwrite: true);
            }
        }
        var adb = CachedAdb(cacheDir);
        if (verifier is not null)
        {
            var sig = verifier.Verify(adb);
            progress?.Report(new InstallProgress(1, "adb ready", sig.Trusted
                ? $"adb.exe is signed by {sig.Signer}."
                : $"adb.exe's signature: {sig.Detail} (downloaded over HTTPS from Google, used anyway).", sig.Trusted ? LogLevel.Info : LogLevel.Warning));
        }
        progress?.Report(new InstallProgress(1, "adb ready", $"adb: {adb}", LogLevel.Success));
        return adb;
    }
}
