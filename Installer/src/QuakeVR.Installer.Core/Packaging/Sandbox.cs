namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// A test install that stays in one folder (<c>--sandbox &lt;dir&gt;</c>, Misc\release\test_local_release.ps1): the game
/// goes into <c>&lt;dir&gt;\QuakeVR</c>, the shortcuts into <c>&lt;dir&gt;\_shortcuts\Desktop</c> and <c>\Programs</c>
/// instead of the real desktop and Start menu, downloads into <c>&lt;dir&gt;\_downloads</c>, the installer's own settings
/// into <c>&lt;dir&gt;\_installer</c>; no Apps &amp; Features entry is written and the VC++ runtime is only checked. The
/// rest is real: the player's Quake is detected and read (never written), and the game started from the sandbox keeps
/// its config, saves and relit maps in <c>&lt;dir&gt;\QuakeVR\quakevr</c> (its last -basedir).
/// </summary>
public sealed class Sandbox(string root)
{
    public string Root { get; } = PathUtil.TryNormalize(root) ?? Path.GetFullPath(root);
    public string Target => Path.Combine(Root, "QuakeVR");
    public string ShortcutsDir => Path.Combine(Root, "_shortcuts");
    public string Downloads => Path.Combine(Root, "_downloads");
    public string InstallerData => Path.Combine(Root, "_installer");
}
