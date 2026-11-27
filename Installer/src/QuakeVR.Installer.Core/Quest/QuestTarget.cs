using QuakeVR.Installer.Core.Packaging;

namespace QuakeVR.Installer.Core.Quest;

/// <summary>Where the Quest target's inputs come from (the window and qvr-setup share these).</summary>
public static class QuestTarget
{
    /// <summary>latest.json's component with the APK (make_release.py --apk).</summary>
    public const string FeedComponent = "quest";
    public const string ApkFileName = "QuakeVR.apk";

    /// <summary>A Quest-only install: the same install steps fill this folder (no shortcuts, no Apps &amp; Features entry),
    /// which the headset is then copied from; it is kept, so the next run only updates and copies what changed.
    /// <paramref name="installerData"/>: %LOCALAPPDATA%\QuakeVR-Installer.</summary>
    public static string StagingDir(string installerData, Sandbox? sandbox) =>
        Path.Combine(sandbox?.Root ?? installerData, "quest-files");

    /// <summary>QuakeVR.apk beside Setup (a local release folder), else null.</summary>
    public static string? LocalApk(string? exePath)
    {
        var dir = exePath is null ? null : Path.GetDirectoryName(exePath);
        var apk = dir is null ? null : Path.Combine(dir, ApkFileName);
        return apk is not null && File.Exists(apk) ? apk : null;
    }

    /// <summary>Whether a QuakeVR.apk beside Setup is the release's own: its size and SHA-256 against the release
    /// list's quest component. Null when there is no component to check against (offline, a local package): it is
    /// used then. An old QuakeVR.apk left in Downloads must not reach the headset (as the 2026-11-27 report's old
    /// QuakeVR.zip reached the PC).</summary>
    public static bool? LocalApkMatches(string apk, FeedFile? expected)
    {
        if (expected is null || expected.Sha256.Length == 0)
        {
            return null;
        }
        if (expected.Size > 0 && new FileInfo(apk).Length != expected.Size)
        {
            return false;
        }
        using var stream = File.OpenRead(apk);
        return string.Equals(Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(stream)), expected.Sha256, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Where to start the game in the headset (sideloaded apps are listed there, not with the store's).</summary>
    public const string StartNote = "Start it in the headset from Library > Unknown Sources (the app list's filter, top right).";

    /// <summary>What happened on the headset, for the Install page's Quest card, the Done page and the console: installed
    /// (<paramref name="result"/>: the app, the files copied and already there, where to start it), or not (the last
    /// problem, or skipped, and what to do).</summary>
    public static (bool Ok, string Title, string Text) Outcome(QuestResult? result, string? problem)
    {
        if (result is null)
        {
            return (false, "Not installed on the Quest",
                $"{(string.IsNullOrWhiteSpace(problem) ? "The Quest was skipped." : problem.TrimEnd())} Run Setup again with Meta Quest ticked: " +
                "what was copied is kept, and skipped next time.");
        }
        return (true, $"Installed on your {result.Device}",
            $"{(result.ApkInstalled ? "The app was installed" : "The app was already up to date")}; {result.Pushed} files copied" +
            $"{(result.Pushed > 0 ? $" ({PathUtil.FormatSize(result.PushedBytes)})" : "")}, {result.Skipped} already there. " +
            $"{(result.NotWritable.Count > 0 ? $"{result.NotWritable.Count} not copied: a folder the game made itself ({result.NotWritable[0][..result.NotWritable[0].LastIndexOf('/')]}) only it can write. " : "")}{StartNote}");
    }

    /// <summary>The steps before the first install, for the window and the console.</summary>
    public static readonly IReadOnlyList<string> SetupSteps =
    [
        "Developer mode: in the Meta Horizon app on your phone, open Devices > your Quest > Headset settings > Developer mode, and turn it on " +
        "(the first time, Meta asks you to register as a developer: free). Restart the headset.",
        "Connect the Quest to this PC with a USB cable that carries data (the one that came with it does).",
        "Put the headset on: it asks \"Allow USB debugging?\". Tick \"Always allow from this computer\" and press Allow.",
        "Press Check again here.",
    ];
}
