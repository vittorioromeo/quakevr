using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Detection;

public enum RuntimeKind
{
    VirtualDesktop,
    SteamVR,
    MetaQuestLink,
    WindowsMixedReality,
    Other,
}

public sealed record OpenXrRuntime(RuntimeKind Kind, string Name, string ManifestPath, bool Enabled, bool ManifestExists);

public sealed record VrReport(
    OpenXrRuntime? Active,
    IReadOnlyList<OpenXrRuntime> Available,
    string? VirtualDesktopDir,
    string? SteamVrDir)
{
    public bool VirtualDesktopInstalled => VirtualDesktopDir is not null || Available.Any(r => r.Kind == RuntimeKind.VirtualDesktop);
    public bool SteamVrInstalled => SteamVrDir is not null || Available.Any(r => r.Kind == RuntimeKind.SteamVR);

    /// <summary>Decision 10: when Virtual Desktop is installed, suggest its OpenXR runtime (VDXR, the author's).</summary>
    public bool SuggestVdxr => VirtualDesktopInstalled && Active?.Kind != RuntimeKind.VirtualDesktop;
}

/// <summary>The OpenXR runtimes (Khronos' registry keys, as the engine's <c>vr_xr_runtime</c> lookup), Virtual
/// Desktop and SteamVR.</summary>
public static class VrDetector
{
    public const string OpenXrKey = @"HKLM\SOFTWARE\Khronos\OpenXR\1";
    public const string AvailableRuntimesKey = @"HKLM\SOFTWARE\Khronos\OpenXR\1\AvailableRuntimes";

    public static RuntimeKind Classify(string manifestPath) =>
        Path.GetFileName(manifestPath).ToLowerInvariant() switch
        {
            "virtualdesktop-openxr.json" => RuntimeKind.VirtualDesktop,
            "steamxr_win64.json" => RuntimeKind.SteamVR,
            "oculus_openxr_64.json" => RuntimeKind.MetaQuestLink,
            "mixedrealityruntime.json" => RuntimeKind.WindowsMixedReality,
            _ => RuntimeKind.Other,
        };

    public static string DisplayName(RuntimeKind kind) => kind switch
    {
        RuntimeKind.VirtualDesktop => "Virtual Desktop (VDXR)",
        RuntimeKind.SteamVR => "SteamVR",
        RuntimeKind.MetaQuestLink => "Meta Quest Link",
        RuntimeKind.WindowsMixedReality => "Windows Mixed Reality",
        _ => "Other OpenXR runtime",
    };

    static OpenXrRuntime Runtime(string path, bool enabled) =>
        new(Classify(path), DisplayName(Classify(path)), path, enabled, File.Exists(path));

    public static VrReport Detect(ISystemProbe probe, SteamLocator steam)
    {
        OpenXrRuntime? active = probe.GetRegistryString(OpenXrKey, "ActiveRuntime") is { } a ? Runtime(a, true) : null;
        var available = probe.GetRegistryValueNames(AvailableRuntimesKey)
            .Where(n => n.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            .Select(n => Runtime(n, probe.GetRegistryInt(AvailableRuntimesKey, n) is null or 0)) // 0 = enabled
            .ToList();
        string? vd = null;
        if (probe.GetFolder(KnownFolder.ProgramFiles) is { } pf)
        {
            var dir = Path.Combine(pf, "Virtual Desktop Streamer");
            if (File.Exists(Path.Combine(dir, "VirtualDesktop.Streamer.exe")) || File.Exists(Path.Combine(dir, "OpenXR", "virtualdesktop-openxr.json")))
            {
                vd = dir;
            }
        }
        var steamVr = steam.FindApp(SteamLocator.SteamVrAppId);
        return new VrReport(active, available, vd, steamVr);
    }
}

public sealed record VcRuntimeInfo(Version? Installed, Version Required, IReadOnlyList<string> Problems)
{
    public VcRuntimeInfo(Version? installed, Version required) : this(installed, required, []) { }

    /// <summary>The runtime is there, new enough, and so are the DLLs the game imports.</summary>
    public bool Ok => Installed is not null && Installed >= Required && Problems.Count == 0;
    public const string DownloadUrl = "https://aka.ms/vs/17/release/vc_redist.x64.exe";

    /// <summary>One line for the Your PC page and the report.</summary>
    public string Describe() =>
        Ok ? $"{Installed} installed."
        : Problems.Count > 0 ? string.Join("; ", Problems) + $" (Quake VR needs {Required} or later)."
        : Installed is null ? "Not installed."
        : $"{Installed} is too old (Quake VR needs {Required} or later).";
}

/// <summary>
/// The Visual C++ 2015-2022 x64 runtime: the engine needs 14.44 or later (older than 14.40 crashes at start in
/// MSVCP140.dll, docs/INSTALL.md). The game is built with the 14.44 toolset and the DLL C runtime (/MD): it imports
/// MSVCP140.dll, VCRUNTIME140.dll and VCRUNTIME140_1.dll (and the Universal CRT's api-ms-win-crt-*, part of Windows 10
/// and later). mimalloc (Quake/vr/vr_crtheap.c) only took the heap functions out of its imports; the DLLs are the same.
/// The redistributable's registry key (Major/Minor/Bld) says what is installed; the DLLs in System32 are checked too
/// (a key left by a broken uninstall, or DLLs older than the key).
/// </summary>
public static class VcRuntimeDetector
{
    public static readonly Version Required = new(14, 44);

    /// <summary>The runtime DLLs ironwail.exe imports (dumpbin /dependents).</summary>
    public static readonly IReadOnlyList<string> Dlls = ["msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"];

    public static readonly IReadOnlyList<string> RegistryKeys =
        [@"HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64", @"HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64"];

    public static VcRuntimeInfo Detect(ISystemProbe probe)
    {
        Version? registry = null;
        foreach (var key in RegistryKeys)
        {
            if (probe.GetRegistryInt(key, "Major") is { } major && probe.GetRegistryInt(key, "Minor") is { } minor)
            {
                registry = new Version(major, minor, probe.GetRegistryInt(key, "Bld") ?? 0);
                break;
            }
        }
        var problems = new List<string>();
        Version? oldestDll = null;
        if (probe.GetFolder(KnownFolder.System64) is { } system)
        {
            foreach (var dll in Dlls)
            {
                var v = probe.GetFileVersion(Path.Combine(system, dll));
                if (v is null)
                {
                    problems.Add($"{dll} is missing");
                    continue;
                }
                if (v < Required)
                {
                    problems.Add($"{dll} is {v.Major}.{v.Minor}.{v.Build}");
                }
                oldestDll = oldestDll is null || v < oldestDll ? v : oldestDll;
            }
            if (problems.Count > 0)
            {
                oldestDll = null;
            }
        }
        // (No key but the DLLs all there: installed some other way; their oldest version is what counts.)
        return new VcRuntimeInfo(registry ?? oldestDll, Required, problems);
    }
}
