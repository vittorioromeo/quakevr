namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// The relight at the first start (the relight component ticked): a marker file in the game folder the game writes in
/// (<c>&lt;QVR&gt;\quakevr\relight_on_first_start.txt</c>). At its next start, however it is started (the installer's
/// Play, a shortcut, Steam), the game removes it and relights every map (<c>vr_relight_batch everything</c>, queued by
/// quake.rc's <c>vr_startgame</c>: Quake/vr/vr_relight.cpp, <c>firstStart</c>). Unticked: no marker, and a marker left by
/// an earlier install is removed. Not in install.json's files: the game deletes it, and the uninstaller removes it.
/// </summary>
public static class FirstStartRelight
{
    /// <summary>The engine's <c>qvr::relight::firstStartMarker</c> (Quake/vr/vr_relight.hpp).</summary>
    public const string MarkerName = "relight_on_first_start.txt";

    public static string MarkerPath(string installDir) => Path.Combine(installDir, "quakevr", MarkerName);

    /// <summary>The maps the game relit in this install before: each copy's <c>.relight</c> note in
    /// <c>quakevr\relit_custom\&lt;game&gt;\maps</c> (not the moved-aside <c>_stale</c> copies nor the <c>_work</c>
    /// folder). The first-start batch skips every one still relit from the same map with the same settings
    /// (vr_relight.cpp, <c>relitAlready</c>): after an update or a reinstall into the same folder only new or changed
    /// maps are relit.</summary>
    public static int RelitCopies(string installDir)
    {
        var root = Path.Combine(installDir, "quakevr", "relit_custom");
        if (!Directory.Exists(root))
        {
            return 0;
        }
        return Directory.EnumerateDirectories(root)
            .Where(d => !Path.GetFileName(d).StartsWith('_'))
            .Select(d => Path.Combine(d, "maps"))
            .Where(Directory.Exists)
            .Sum(d => Directory.EnumerateFiles(d, "*.relight", SearchOption.AllDirectories).Count());
    }

    /// <summary>The game has not started since the installer asked for the relight.</summary>
    public static bool Pending(string installDir) => File.Exists(MarkerPath(installDir));

    /// <summary>Writes the marker (on) or removes it (off).</summary>
    public static void Set(string installDir, bool on)
    {
        var path = MarkerPath(installDir);
        if (on)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.WriteAllText(path,
                "Written by the Quake VR: Unleashed installer (the relight ticked). At its next start the game relights\r\n" +
                "every map not relit with these settings yet (vr_relight_batch everything: the maps it relit before\r\n" +
                "are skipped while they and the settings are unchanged) and removes this file. Remove it to skip that.\r\n");
        }
        else if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}
