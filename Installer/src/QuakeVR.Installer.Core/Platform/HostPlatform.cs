using System.Runtime.Versioning;

namespace QuakeVR.Installer.Core.Platform;

/// <summary>The operating systems the installer runs on (docs/vr-port/INSTALLER.md, "Cross-platform").</summary>
public enum HostOs
{
    Windows,
    Linux,
}

/// <summary>
/// The machine the installer runs on: which <see cref="ISystemProbe"/>, which uninstall record, which default folders.
/// The Windows build (net9.0-windows) is always <see cref="HostOs.Windows"/>, as before; the portable build (net9.0) asks
/// the OS. Code that must stay testable on any OS takes a <see cref="HostOs"/> (or the probe's
/// <see cref="ISystemProbe.Os"/>) instead of reading <see cref="Current"/>.
/// </summary>
public static class HostPlatform
{
    public static HostOs Current => OperatingSystem.IsWindows() ? HostOs.Windows : HostOs.Linux;

    /// <summary>The release package's runtime identifier (the feed's <c>packages</c> key): win-x64 or linux-x64.</summary>
    public static string Rid(HostOs os) => os == HostOs.Windows ? "win-x64" : "linux-x64";

    public static string CurrentRid => Rid(Current);

    /// <summary>The real machine's probe (read-only).</summary>
    public static ISystemProbe CreateProbe()
    {
        if (OperatingSystem.IsWindows())
        {
            return new WindowsSystemProbe();
        }
        return new LinuxSystemProbe();
    }

    /// <summary>
    /// The real per-user uninstall record: HKCU's Apps &amp; Features entry on Windows; on Linux the same values in a JSON
    /// file (<see cref="LinuxUninstallRecordPath"/>), so an update or uninstall finds the install as on Windows.
    /// </summary>
    public static IRegistryWriter CreateUserRegistry(ISystemProbe probe)
    {
        if (OperatingSystem.IsWindows())
        {
            return new WindowsRegistryWriter();
        }
        return new JsonFileRegistry(LinuxUninstallRecordPath(probe));
    }

    /// <summary>The installer's own data: %LOCALAPPDATA%\QuakeVR-Installer; on Linux $XDG_DATA_HOME/QuakeVR-Installer.</summary>
    public static string InstallerDataDir(ISystemProbe probe) =>
        Path.Combine(probe.GetFolder(KnownFolder.LocalAppData) ?? ".", "QuakeVR-Installer");

    public static string LinuxUninstallRecordPath(ISystemProbe probe) => Path.Combine(InstallerDataDir(probe), "uninstall.json");

    /// <summary>The default install folder: %LOCALAPPDATA%\Programs\QuakeVR (unchanged); on Linux $XDG_DATA_HOME/QuakeVR.</summary>
    public static string DefaultInstallDir(ISystemProbe probe) => probe.Os == HostOs.Windows
        ? Path.Combine(probe.GetFolder(KnownFolder.LocalAppData) ?? ".", "Programs", "QuakeVR")
        : Path.Combine(probe.GetFolder(KnownFolder.LocalAppData) ?? ".", "QuakeVR");

    /// <summary>Where downloads go: %LOCALAPPDATA%\QuakeVR-Installer\downloads; on Linux $XDG_CACHE_HOME/QuakeVR-Installer/downloads.</summary>
    public static string DownloadsDir(ISystemProbe probe) => probe.Os == HostOs.Windows
        ? Path.Combine(InstallerDataDir(probe), "downloads")
        : Path.Combine(probe.GetFolder(KnownFolder.CacheHome) ?? InstallerDataDir(probe), "QuakeVR-Installer", "downloads");

    /// <summary>Marks a file executable (rwxr-xr-x) on Linux; nothing on Windows.</summary>
    public static void MakeExecutable(string path)
    {
        if (!OperatingSystem.IsWindows() && File.Exists(path))
        {
            SetMode(path);
        }
    }

    [UnsupportedOSPlatform("windows")]
    static void SetMode(string path) =>
        File.SetUnixFileMode(path, UnixFileMode.UserRead | UnixFileMode.UserWrite | UnixFileMode.UserExecute |
                                   UnixFileMode.GroupRead | UnixFileMode.GroupExecute | UnixFileMode.OtherRead | UnixFileMode.OtherExecute);
}
