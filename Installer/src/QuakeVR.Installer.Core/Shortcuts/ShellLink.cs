using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text;

namespace QuakeVR.Installer.Core.Shortcuts;

/// <summary>A Windows shortcut (.lnk).</summary>
public sealed record ShortcutSpec(
    string LinkPath,
    string TargetPath,
    string Arguments,
    string WorkingDirectory,
    string Description,
    string? IconPath = null,
    int IconIndex = 0);

/// <summary>Reads and writes .lnk files through the shell's own <c>IShellLinkW</c> (COM; no dependency).</summary>
public static class ShellLink
{
    public static void Save(ShortcutSpec spec)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(spec.LinkPath)!);
        var link = (IShellLinkW)new CShellLink();
        try
        {
            Check(link.SetPath(spec.TargetPath));
            Check(link.SetArguments(spec.Arguments));
            Check(link.SetWorkingDirectory(spec.WorkingDirectory));
            Check(link.SetDescription(spec.Description));
            if (spec.IconPath is not null)
            {
                Check(link.SetIconLocation(spec.IconPath, spec.IconIndex));
            }
            ((IPersistFile)link).Save(spec.LinkPath, true);
        }
        finally
        {
            Marshal.ReleaseComObject(link);
        }
    }

    public static ShortcutSpec Load(string linkPath)
    {
        var link = (IShellLinkW)new CShellLink();
        try
        {
            ((IPersistFile)link).Load(linkPath, 0); // STGM_READ
            var sb = new StringBuilder(1024);
            Check(link.GetPath(sb, sb.Capacity, IntPtr.Zero, 0x4)); // SLGP_RAWPATH
            var target = sb.ToString();
            sb.Clear();
            Check(link.GetArguments(sb, sb.Capacity));
            var args = sb.ToString();
            sb.Clear();
            Check(link.GetWorkingDirectory(sb, sb.Capacity));
            var dir = sb.ToString();
            sb.Clear();
            Check(link.GetDescription(sb, sb.Capacity));
            var desc = sb.ToString();
            sb.Clear();
            Check(link.GetIconLocation(sb, sb.Capacity, out var iconIndex));
            var icon = sb.ToString();
            return new ShortcutSpec(linkPath, target, args, dir, desc, icon.Length > 0 ? icon : null, iconIndex);
        }
        finally
        {
            Marshal.ReleaseComObject(link);
        }
    }

    static void Check(int hr)
    {
        if (hr < 0)
        {
            Marshal.ThrowExceptionForHR(hr);
        }
    }

    [ComImport, Guid("00021401-0000-0000-C000-000000000046")]
    class CShellLink;

    // The vtable order matters: IShellLinkW as declared in ShObjIdl_core.h.
    [ComImport, Guid("000214F9-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IShellLinkW
    {
        [PreserveSig] int GetPath([MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszFile, int cch, IntPtr pfd, uint fFlags);
        [PreserveSig] int GetIDList(out IntPtr ppidl);
        [PreserveSig] int SetIDList(IntPtr pidl);
        [PreserveSig] int GetDescription([MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszName, int cch);
        [PreserveSig] int SetDescription([MarshalAs(UnmanagedType.LPWStr)] string pszName);
        [PreserveSig] int GetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszDir, int cch);
        [PreserveSig] int SetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)] string pszDir);
        [PreserveSig] int GetArguments([MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszArgs, int cch);
        [PreserveSig] int SetArguments([MarshalAs(UnmanagedType.LPWStr)] string pszArgs);
        [PreserveSig] int GetHotkey(out short pwHotkey);
        [PreserveSig] int SetHotkey(short wHotkey);
        [PreserveSig] int GetShowCmd(out int piShowCmd);
        [PreserveSig] int SetShowCmd(int iShowCmd);
        [PreserveSig] int GetIconLocation([MarshalAs(UnmanagedType.LPWStr)] StringBuilder pszIconPath, int cch, out int piIcon);
        [PreserveSig] int SetIconLocation([MarshalAs(UnmanagedType.LPWStr)] string pszIconPath, int iIcon);
        [PreserveSig] int SetRelativePath([MarshalAs(UnmanagedType.LPWStr)] string pszPathRel, uint dwReserved);
        [PreserveSig] int Resolve(IntPtr hwnd, uint fFlags);
        [PreserveSig] int SetPath([MarshalAs(UnmanagedType.LPWStr)] string pszFile);
    }
}
