namespace QuakeVR.Installer.Core.Shortcuts;

public sealed class ShortcutOptions
{
    public bool Desktop { get; set; } = true;
    public bool StartMenu { get; set; } = true;
    public bool Flat { get; set; } = true;
    public bool Log { get; set; } = true;
    /// <summary>Where the desktop shortcut goes (the real desktop, or a test folder).</summary>
    public string? DesktopDir { get; set; }
    /// <summary>The Start menu's Programs folder (the real one, or a test folder); a "Quake VR Unleashed" folder is made in it.</summary>
    public string? StartMenuDir { get; set; }
}

/// <summary>The shortcuts of section 7 of docs/vr-port/INSTALLER.md, for a given install.</summary>
public static class ShortcutPlanner
{
    /// <summary>The product's name, "Quake VR: Unleashed", as a file name (Windows refuses ':' in names).</summary>
    public const string Name = "Quake VR Unleashed";
    public const string StartMenuFolder = Name;
    /// <summary>The Start menu folder of installs made before the "Unleashed" name (removed once empty).</summary>
    public const string LegacyStartMenuFolder = "Quake VR";

    public static IReadOnlyList<ShortcutSpec> Plan(ShortcutOptions o, string quakeDir, string qvrDir)
    {
        var exe = Path.Combine(qvrDir, LaunchCommand.Exe);
        ShortcutSpec Launch(string folder, string name, LaunchVariant v, string description) =>
            new(Path.Combine(folder, name + ".lnk"), exe, LaunchCommand.Arguments(quakeDir, qvrDir, v), qvrDir, description, exe);

        var list = new List<ShortcutSpec>();
        o = new ShortcutOptions
        {
            Desktop = o.Desktop, StartMenu = o.StartMenu, Flat = o.Flat, Log = o.Log,
            DesktopDir = o.DesktopDir is null ? null : PathUtil.TryNormalize(o.DesktopDir),
            StartMenuDir = o.StartMenuDir is null ? null : PathUtil.TryNormalize(o.StartMenuDir),
        };
        if (o.Desktop && o.DesktopDir is not null)
        {
            list.Add(Launch(o.DesktopDir, Name, LaunchVariant.Vr, "Play Quake VR: Unleashed in your headset"));
        }
        // The variants go in the Start menu; without one, beside the desktop shortcut.
        var variants = o.StartMenu && o.StartMenuDir is not null ? Path.Combine(o.StartMenuDir, StartMenuFolder)
            : o.Desktop ? o.DesktopDir : null;
        if (o.StartMenu && o.StartMenuDir is not null)
        {
            list.Add(Launch(variants!, Name, LaunchVariant.Vr, "Play Quake VR: Unleashed in your headset"));
        }
        if (variants is not null)
        {
            if (o.Flat)
            {
                list.Add(Launch(variants, Name + " (flat screen)", LaunchVariant.Flat, "Play Quake VR: Unleashed on the monitor, without a headset"));
            }
            if (o.Log)
            {
                list.Add(Launch(variants, Name + " (log for bug reports)", LaunchVariant.Log,
                    "Play Quake VR: Unleashed and write qconsole.log for a bug report"));
            }
        }
        if (o.StartMenu && o.StartMenuDir is not null)
        {
            var files = Path.Combine(qvrDir, "quakevr");
            list.Add(new ShortcutSpec(Path.Combine(variants!, Name + " files.lnk"), files, "", files,
                "Screenshots, notes, saves and settings"));
        }
        return list;
    }
}
