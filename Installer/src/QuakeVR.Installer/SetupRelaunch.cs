using System.Diagnostics;
using System.IO;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

/// <summary>
/// An uninstall run from the install's own copy of Setup (<c>&lt;QVR&gt;\setup</c>, as Apps &amp; Features starts it) restarts
/// from a copy in %TEMP%: a running exe cannot be deleted, so the copy in the install is removed with the rest. The
/// %TEMP% copy is left for Windows' temporary-file clean-up (a few MB).
/// </summary>
static class SetupRelaunch
{
    public static void Start(IReadOnlyList<string> arguments)
    {
        var exe = SetupCopy.CopyToTemp(MainViewModel.OwnSetupFiles(), Path.GetTempPath());
        var start = new ProcessStartInfo(exe) { UseShellExecute = false, WorkingDirectory = Path.GetDirectoryName(exe)! };
        foreach (var a in arguments)
        {
            start.ArgumentList.Add(a);
        }
        Process.Start(start)?.Dispose();
    }
}
