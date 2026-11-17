using System.Text;
using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Shortcuts;

/// <summary>
/// A Linux shortcut: a freedesktop.org desktop entry (<c>.desktop</c>, Desktop Entry Specification 1.5), the Linux
/// counterpart of <see cref="ShellLink"/>. <see cref="ShortcutSpec.TargetPath"/> and <see cref="ShortcutSpec.Arguments"/>
/// become <c>Exec</c> (a folder target opens with <c>xdg-open</c>), the working directory <c>Path</c>, the description
/// <c>Comment</c>. Written executable (GNOME asks before running a desktop entry that is not). Pure managed code: the
/// self-tests write and read them on Windows too.
/// </summary>
public static class DesktopEntry
{
    public const string Extension = ".desktop";

    /// <summary>A path or argument for Exec: double-quoted, with the reserved characters escaped (spec, "The Exec key").</summary>
    public static string QuoteExec(string value)
    {
        var sb = new StringBuilder("\"");
        foreach (var c in value)
        {
            if (c is '"' or '`' or '$' or '\\')
            {
                sb.Append('\\');
            }
            sb.Append(c);
        }
        return sb.Append('"').ToString().Replace("%", "%%");
    }

    /// <summary>The Exec line for a spec. Its Arguments are LaunchCommand's (paths already in double quotes), so only
    /// the characters a shell or the field codes would read are escaped. A spec without arguments is a folder (the
    /// "files" shortcut): opened with xdg-open.</summary>
    public static string Exec(ShortcutSpec spec) =>
        Directory.Exists(spec.TargetPath) || spec.Arguments.Length == 0
            ? "xdg-open " + QuoteExec(spec.TargetPath)
            : QuoteExec(spec.TargetPath) + " " + spec.Arguments.Replace("$", "\\$").Replace("`", "\\`").Replace("%", "%%");

    /// <summary>A value for a string key: no newlines (escaped as the spec says).</summary>
    static string Value(string s) => s.Replace("\\", "\\\\").Replace("\n", "\\n").Replace("\r", "\\r").Replace("\t", "\\t");

    public static string Format(ShortcutSpec spec)
    {
        var name = Path.GetFileNameWithoutExtension(spec.LinkPath);
        var sb = new StringBuilder();
        sb.Append("[Desktop Entry]\n");
        sb.Append("Type=Application\n");
        sb.Append("Version=1.5\n");
        sb.Append("Name=").Append(Value(name)).Append('\n');
        sb.Append("Comment=").Append(Value(spec.Description)).Append('\n');
        sb.Append("Exec=").Append(Exec(spec)).Append('\n');
        sb.Append("Path=").Append(Value(spec.WorkingDirectory)).Append('\n');
        if (spec.IconPath is not null)
        {
            sb.Append("Icon=").Append(Value(spec.IconPath)).Append('\n');
        }
        sb.Append("Terminal=false\n");
        sb.Append("Categories=Game;ActionGame;\n");
        // (Read back by Load, and how an uninstall knows the entry is Quake VR's: the target, as on Windows.)
        sb.Append("X-QuakeVR-Target=").Append(Value(spec.TargetPath)).Append('\n');
        sb.Append("X-QuakeVR-Arguments=").Append(Value(spec.Arguments)).Append('\n');
        return sb.ToString();
    }

    public static void Save(ShortcutSpec spec)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(spec.LinkPath)!);
        File.WriteAllText(spec.LinkPath, Format(spec), new UTF8Encoding(false));
        HostPlatform.MakeExecutable(spec.LinkPath);
    }

    static string Unescape(string s)
    {
        var sb = new StringBuilder();
        for (var i = 0; i < s.Length; ++i)
        {
            if (s[i] == '\\' && i + 1 < s.Length)
            {
                ++i;
                sb.Append(s[i] switch { 'n' => '\n', 'r' => '\r', 't' => '\t', 's' => ' ', var c => c });
                continue;
            }
            sb.Append(s[i]);
        }
        return sb.ToString();
    }

    /// <summary>Reads an entry this class wrote (others: the X-QuakeVR keys are missing, the target is empty).</summary>
    public static ShortcutSpec Load(string path)
    {
        var keys = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var raw in File.ReadLines(path))
        {
            var eq = raw.IndexOf('=');
            if (eq > 0 && !raw.StartsWith('#') && !raw.StartsWith('['))
            {
                keys.TryAdd(raw[..eq].Trim(), Unescape(raw[(eq + 1)..]));
            }
        }
        string Get(string k) => keys.TryGetValue(k, out var v) ? v : "";
        return new ShortcutSpec(path, Get("X-QuakeVR-Target"), Get("X-QuakeVR-Arguments"), Get("Path"), Get("Comment"),
            keys.TryGetValue("Icon", out var icon) ? icon : null);
    }
}

/// <summary>A shortcut of either kind: a Windows .lnk (<see cref="ShellLink"/>) or a Linux .desktop (<see cref="DesktopEntry"/>), by its extension.</summary>
public static class Shortcut
{
    public static bool IsDesktopEntry(string path) => path.EndsWith(DesktopEntry.Extension, StringComparison.OrdinalIgnoreCase);

    public static bool IsShortcutFile(string path) => IsDesktopEntry(path) || path.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase);

    public static void Save(ShortcutSpec spec)
    {
        if (IsDesktopEntry(spec.LinkPath))
        {
            DesktopEntry.Save(spec);
        }
        else if (OperatingSystem.IsWindows())
        {
            ShellLink.Save(spec);
        }
        else
        {
            throw new PlatformNotSupportedException($".lnk shortcuts are Windows only: {spec.LinkPath}");
        }
    }

    public static ShortcutSpec Load(string path)
    {
        if (IsDesktopEntry(path))
        {
            return DesktopEntry.Load(path);
        }
        if (OperatingSystem.IsWindows())
        {
            return ShellLink.Load(path);
        }
        throw new PlatformNotSupportedException($".lnk shortcuts are Windows only: {path}");
    }
}
