namespace QuakeVR.Installer.Core.Shortcuts;

public enum LaunchVariant
{
    /// <summary>In the headset.</summary>
    Vr,
    /// <summary>On the monitor, no headset (<c>+vr_enabled 0</c>, run after quakevr.cfg's <c>vr_enabled 1</c>).</summary>
    Flat,
    /// <summary>In the headset, writing qconsole.log for a bug report (<c>-condebug</c>).</summary>
    Log,
}

/// <summary>
/// The command line of every launch (docs/vr-port/INSTALLER.md, sections 1 and 7):
/// <c>ironwail.exe -basedir "&lt;Quake&gt;" -basedir "&lt;QVR&gt;" -game quakevr</c>, started in the Quake VR folder. The
/// first base dir must hold id1\pak0.pak; the last one is where the game writes (config, saves, relit maps).
/// </summary>
public static class LaunchCommand
{
    public const string Exe = "ironwail.exe";

    /// <summary>A path for the command line: no trailing backslash (it would escape the closing quote) and quoted.</summary>
    public static string Quote(string path) => "\"" + PathUtil.TrimEnd(path) + "\"";

    public static string Arguments(string quakeDir, string qvrDir, LaunchVariant variant, string? extra = null)
    {
        var args = $"-basedir {Quote(quakeDir)} -basedir {Quote(qvrDir)} -game quakevr";
        args += variant switch
        {
            LaunchVariant.Flat => " +vr_enabled 0",
            LaunchVariant.Log => " -condebug",
            _ => "",
        };
        return string.IsNullOrWhiteSpace(extra) ? args : args + " " + extra.Trim();
    }
}
