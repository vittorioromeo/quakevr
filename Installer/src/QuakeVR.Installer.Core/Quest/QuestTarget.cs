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
