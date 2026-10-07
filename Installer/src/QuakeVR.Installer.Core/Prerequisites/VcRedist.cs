using System.ComponentModel;
using System.Diagnostics;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.Core.Prerequisites;

/// <summary>Runs a program elevated (Windows' administrator prompt) and waits for it.</summary>
public interface IElevatedRunner
{
    /// <summary>The program's exit code; 1223 (ERROR_CANCELLED) when the player declined the prompt.</summary>
    int Run(string exe, string arguments);
}

/// <summary>The real one: ShellExecute with the "runas" verb (one UAC prompt).</summary>
public sealed class ShellElevatedRunner : IElevatedRunner
{
    public int Run(string exe, string arguments)
    {
        try
        {
            using var p = Process.Start(new ProcessStartInfo(exe, arguments) { UseShellExecute = true, Verb = "runas" })
                          ?? throw new InstallException($"{Path.GetFileName(exe)} did not start");
            p.WaitForExit();
            return p.ExitCode;
        }
        catch (Win32Exception e) when (e.NativeErrorCode == VcRedist.ErrorCancelled)
        {
            return VcRedist.ErrorCancelled;
        }
    }
}

public enum VcRedistOutcome
{
    /// <summary>Nothing to do: the runtime is there (or the redistributable said a same or newer one is).</summary>
    AlreadyInstalled,
    Installed,
    /// <summary>Installed; Windows wants a restart before the runtime is in use everywhere (the game usually starts).</summary>
    RebootRequired,
    /// <summary>The player declined the administrator prompt.</summary>
    Cancelled,
    /// <summary>Another installation is running (Windows Installer's 1618).</summary>
    Busy,
    /// <summary>The file is not Microsoft's signed redistributable (or older than needed): not run.</summary>
    NotTrusted,
    Failed,
    /// <summary>Could not be had: offline, or the download failed.</summary>
    Unavailable,
    /// <summary>Only said what it would do.</summary>
    DryRun,
}

public sealed record VcRedistResult(VcRedistOutcome Outcome, string Message, int? ExitCode = null, string? File = null)
{
    /// <summary>The game's runtime is in place (perhaps after a restart).</summary>
    public bool RuntimeReady => Outcome is VcRedistOutcome.AlreadyInstalled or VcRedistOutcome.Installed or VcRedistOutcome.RebootRequired;
}

public sealed class VcRedistOptions
{
    /// <summary>Say what would be done; download nothing, run nothing (a copy given in LocalCopies is still checked).</summary>
    public bool DryRun { get; init; }
    /// <summary>Never download (a local copy is still used).</summary>
    public bool Offline { get; init; }
    /// <summary>Copies to try first (vc_redist.x64.exe beside the installer): used when they pass the same checks.</summary>
    public IReadOnlyList<string> LocalCopies { get; init; } = [];
    /// <summary>Where it is downloaded from, in order (Microsoft's permanent link; tests: a local server).</summary>
    public IReadOnlyList<Uri> Mirrors { get; init; } = [VcRedist.DefaultUrl];
    public required string DownloadDir { get; init; }
}

/// <summary>
/// Microsoft's Visual C++ 2015-2022 x64 redistributable, installed when the runtime is missing or older than the game
/// needs (<see cref="VcRuntimeDetector"/>). Its permanent link (aka.ms/vs/17/release/vc_redist.x64.exe) serves the
/// current release, which changes, so it is not pinned by hash: it is run only when its Authenticode signature is
/// valid and Microsoft Corporation's, and its version is at least the one required. Then
/// <c>vc_redist.x64.exe /install /quiet /norestart</c> elevated (the install's one administrator prompt); its exit code
/// is read as Windows Installer's (0, 3010/1641 restart, 1638 a same or newer one already there, 1602/1223
/// cancelled, 1618 another install running).
/// </summary>
public sealed class VcRedist(ISignatureVerifier verifier, IElevatedRunner runner, Func<string, Version?> fileVersion, HttpClient? http)
{
    public const string FileName = "vc_redist.x64.exe";
    public const string Arguments = "/install /quiet /norestart";
    public const int ErrorCancelled = 1223;
    public static readonly Uri DefaultUrl = new(VcRuntimeInfo.DownloadUrl);

    /// <summary>The real machine's: WinVerifyTrust, a UAC prompt, the file's version resource.</summary>
    public static VcRedist ForWindows(HttpClient? http) =>
        new(new AuthenticodeVerifier(), new ShellElevatedRunner(), FileVersionOf, http);

    public static Version? FileVersionOf(string path)
    {
        if (!System.IO.File.Exists(path))
        {
            return null;
        }
        var info = FileVersionInfo.GetVersionInfo(path);
        return info.FileMajorPart == 0 && info.FileMinorPart == 0 ? null
            : new Version(info.FileMajorPart, info.FileMinorPart, info.FileBuildPart, info.FilePrivatePart);
    }

    /// <summary>Why a file must not be run (null: it may): Microsoft's valid signature and a recent enough version.</summary>
    public string? Reject(string path)
    {
        var signature = verifier.Verify(path);
        if (!signature.IsMicrosoft)
        {
            return $"{Path.GetFileName(path)} is not signed by Microsoft ({signature.Detail}{(signature.Signer is { } s ? "; signer " + s : "")})";
        }
        var version = fileVersion(path);
        if (version is null || version < VcRuntimeDetector.Required)
        {
            return $"{Path.GetFileName(path)} is version {version?.ToString() ?? "unknown"}, older than the {VcRuntimeDetector.Required} needed";
        }
        return null;
    }

    public static VcRedistResult FromExitCode(int code, string file) => code switch
    {
        0 => new(VcRedistOutcome.Installed, "Visual C++ runtime installed.", code, file),
        3010 or 1641 => new(VcRedistOutcome.RebootRequired, "Visual C++ runtime installed; restart Windows if the game does not start.", code, file),
        1638 => new(VcRedistOutcome.AlreadyInstalled, "Visual C++ runtime: the same or a newer version is already installed.", code, file),
        1602 or ErrorCancelled => new(VcRedistOutcome.Cancelled, "Visual C++ runtime not installed: the administrator prompt was declined.", code, file),
        1618 => new(VcRedistOutcome.Busy, "Visual C++ runtime not installed: another installation is running. Let it finish, then run Setup again.", code, file),
        _ => new(VcRedistOutcome.Failed, $"Visual C++ runtime: its installer failed (exit code {code}).", code, file),
    };

    public async Task<VcRedistResult> EnsureAsync(VcRuntimeInfo info, VcRedistOptions options, IProgress<InstallProgress>? progress, CancellationToken ct)
    {
        void Log(string text, LogLevel level = LogLevel.Info) => progress?.Report(new InstallProgress(0, "Visual C++ runtime", text, level));

        if (info.Ok)
        {
            return new VcRedistResult(VcRedistOutcome.AlreadyInstalled, $"Visual C++ runtime {info.Installed}: nothing to do.");
        }
        string? file = null;
        foreach (var local in options.LocalCopies.Where(System.IO.File.Exists))
        {
            if (Reject(local) is { } why)
            {
                Log($"{local} not used: {why}.", LogLevel.Warning);
                continue;
            }
            file = local;
            break;
        }
        if (file is null)
        {
            var dest = Path.Combine(options.DownloadDir, FileName);
            if (options.DryRun)
            {
                return new VcRedistResult(VcRedistOutcome.DryRun,
                    $"Would download {options.Mirrors.FirstOrDefault()} to {dest}, check that Microsoft signed it (version {VcRuntimeDetector.Required} or later) " +
                    $"and run it with {Arguments} (one administrator prompt).", File: dest);
            }
            if (options.Offline || http is null)
            {
                return new VcRedistResult(VcRedistOutcome.Unavailable, "Visual C++ runtime not installed: offline. Install it from Microsoft (aka.ms/vs/17/release/vc_redist.x64.exe).");
            }
            // Always a fresh copy: the link serves Microsoft's current release, and a part left by an older one must not be resumed.
            foreach (var stale in new[] { dest, dest + ".part" }.Where(System.IO.File.Exists))
            {
                System.IO.File.Delete(stale);
            }
            Log($"Downloading Microsoft's Visual C++ runtime ({options.Mirrors.FirstOrDefault()?.Host}).");
            try
            {
                await new Downloader(http).DownloadAsync(options.Mirrors, dest, null, null, null, ct).ConfigureAwait(false);
            }
            catch (Exception e) when (e is InstallException or HttpRequestException or IOException)
            {
                return new VcRedistResult(VcRedistOutcome.Unavailable, $"Visual C++ runtime not downloaded: {e.Message}");
            }
            if (Reject(dest) is { } why)
            {
                System.IO.File.Delete(dest);
                return new VcRedistResult(VcRedistOutcome.NotTrusted, $"Visual C++ runtime not installed: {why}.", File: dest);
            }
            Log("Its signature is Microsoft's.", LogLevel.Success);
            file = dest;
        }
        if (options.DryRun)
        {
            return new VcRedistResult(VcRedistOutcome.DryRun, $"Would run {file} {Arguments} (one administrator prompt).", File: file);
        }
        progress?.Report(new InstallProgress(0, "Installing the Visual C++ runtime (Windows asks for permission)",
            "Installing Microsoft's Visual C++ runtime: Windows asks for permission once."));
        var code = await Task.Run(() => runner.Run(file, Arguments), ct).ConfigureAwait(false);
        return FromExitCode(code, file);
    }
}
