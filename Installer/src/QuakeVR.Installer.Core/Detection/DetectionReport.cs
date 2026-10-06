using System.Text;
using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Detection;

/// <summary>Everything the Detect page shows, gathered in one pass (seconds: the expansion checks read the packs).</summary>
public sealed record DetectionReport(
    IReadOnlyList<QuakeInstall> Quakes,
    IReadOnlyList<string> StoreRoots,
    VrReport Vr,
    VcRuntimeInfo VcRuntime)
{
    public static DetectionReport Run(ISystemProbe probe, string? epicManifestsDir = null)
    {
        var detector = new QuakeDetector(probe, epicManifestsDir);
        return new DetectionReport(detector.FindAll(), ExpansionDetector.StoreRoots(detector),
            VrDetector.Detect(probe, detector.Steam), VcRuntimeDetector.Detect(probe));
    }

    /// <summary>The Quake the installer picks by itself: the first playable one, preferring one that has both the
    /// original game and the rerelease (the author's setup).</summary>
    public QuakeInstall? DefaultQuake =>
        Quakes.FirstOrDefault(q => q.Original.Playable && q.Rerelease.Playable) ?? Quakes.FirstOrDefault(q => q.Playable);

    public IReadOnlyList<ExpansionInfo> Expansions(QuakeInstall quake, string? qvrDir)
    {
        var bases = new List<string>();
        if (quake.BaseDir is { } b)
        {
            bases.Add(b);
        }
        if (qvrDir is not null)
        {
            bases.Add(qvrDir);
        }
        return ExpansionDetector.Detect(bases, StoreRoots);
    }

    /// <summary>A plain-text report (the CLI's <c>detect</c>, bug reports).</summary>
    public string Format(string? qvrDir)
    {
        var sb = new StringBuilder();
        sb.AppendLine("Quake installs:");
        if (Quakes.Count == 0)
        {
            sb.AppendLine("  none found (pick the folder by hand)");
        }
        foreach (var q in Quakes)
        {
            sb.AppendLine($"  [{q.Store}] {q.Name}: {q.InstallDir}");
            sb.AppendLine($"    original:  {q.Original.Kind} - {q.Original.Detail}");
            sb.AppendLine($"    rerelease: {q.Rerelease.Kind} - {q.Rerelease.Detail}{(q.RereleaseRoot is null ? "" : $" ({q.RereleaseRoot})")}");
            sb.AppendLine($"    base dir:  {q.BaseDir ?? "(not playable)"}");
            sb.AppendLine($"    music:     {q.MusicDir ?? "none found"}");
            if (q.ExistingQuakeVr is not null)
            {
                sb.AppendLine($"    existing Quake VR zip install: {q.ExistingQuakeVr}");
            }
        }
        if (DefaultQuake is { } d)
        {
            sb.AppendLine($"Expansions (for {d.Name}, base {d.BaseDir}):");
            foreach (var e in Expansions(d, qvrDir))
            {
                sb.AppendLine($"  {e.Folder,-9} {e.Title,-26} {e.State,-20} {e.Detail}{(e.Root is null ? "" : $" [{e.Root}]")}");
            }
        }
        sb.AppendLine("VR:");
        sb.AppendLine($"  active OpenXR runtime: {(Vr.Active is { } a ? $"{a.Name} ({a.ManifestPath}){(a.ManifestExists ? "" : " MISSING FILE")}" : "none")}");
        foreach (var r in Vr.Available)
        {
            sb.AppendLine($"  available: {r.Name} ({r.ManifestPath}){(r.Enabled ? "" : " disabled")}");
        }
        sb.AppendLine($"  Virtual Desktop: {(Vr.VirtualDesktopInstalled ? Vr.VirtualDesktopDir ?? "registered runtime" : "not found")}");
        sb.AppendLine($"  SteamVR: {(Vr.SteamVrInstalled ? Vr.SteamVrDir ?? "registered runtime" : "not found")}");
        sb.AppendLine($"  suggest VDXR: {Vr.SuggestVdxr}");
        sb.AppendLine($"VC++ runtime: {VcRuntime.Installed?.ToString() ?? "not installed"} (needs {VcRuntime.Required}+): {(VcRuntime.Ok ? "ok" : "install or update")}");
        return sb.ToString();
    }
}
