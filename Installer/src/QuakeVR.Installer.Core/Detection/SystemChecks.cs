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

public sealed record VcRuntimeInfo(Version? Installed, Version Required)
{
    public bool Ok => Installed is not null && Installed >= Required;
    public const string DownloadUrl = "https://aka.ms/vs/17/release/vc_redist.x64.exe";
}

/// <summary>The Visual C++ 2015-2022 x64 runtime: the engine needs 14.44 or later (older than 14.40 crashes at start
/// in MSVCP140.dll, docs/INSTALL.md).</summary>
public static class VcRuntimeDetector
{
    public static readonly Version Required = new(14, 44);

    public static VcRuntimeInfo Detect(ISystemProbe probe)
    {
        foreach (var key in new[] { @"HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64", @"HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" })
        {
            if (probe.GetRegistryInt(key, "Major") is { } major && probe.GetRegistryInt(key, "Minor") is { } minor)
            {
                return new VcRuntimeInfo(new Version(major, minor, probe.GetRegistryInt(key, "Bld") ?? 0), Required);
            }
        }
        return new VcRuntimeInfo(null, Required);
    }
}
