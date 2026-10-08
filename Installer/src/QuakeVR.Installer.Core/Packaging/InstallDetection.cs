using QuakeVR.Installer.Core.Platform;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>Where an install was found (in the order one is picked).</summary>
public enum InstallOrigin
{
    /// <summary>The folder the player gave (--target, Browse, a sandbox).</summary>
    Chosen,
    /// <summary>Setup runs from the install's own copy (&lt;QVR&gt;\setup\QuakeVR-Setup.exe).</summary>
    RunningFrom,
    /// <summary>The Apps &amp; Features entry's InstallLocation.</summary>
    Registered,
    /// <summary>The default folder (%LOCALAPPDATA%\Programs\QuakeVR).</summary>
    DefaultFolder,
}

public sealed record FoundInstall(string Dir, InstallRecord Record, InstallOrigin Origin)
{
    public string OriginText => Origin switch
    {
        InstallOrigin.Chosen => "the folder given",
        InstallOrigin.RunningFrom => "Setup's copy in it",
        InstallOrigin.Registered => "Windows' Installed apps",
        _ => "the default folder",
    };
}

/// <summary>
/// Finds an existing Quake VR: Unleashed install: a folder with an <c>install.json</c> (InstallRecord) that reads. Looked
/// for in the folder the player gave, the install this Setup runs from, the Apps &amp; Features entry's InstallLocation and
/// the default folder; the first that has one is picked (several: the others are listed, and the window's "Use another
/// install" browses to any). An entry whose folder has no install any more (moved or deleted by hand) is said and passed
/// over; a moved install is found again by browsing to it (its next update rewrites the entry).
/// </summary>
public sealed class InstallDetection
{
    public FoundInstall? Picked => Found.Count > 0 ? Found[0] : null;
    /// <summary>Every install found, the picked one first (each folder once).</summary>
    public List<FoundInstall> Found { get; } = [];
    /// <summary>What was looked at and passed over (an entry pointing at an empty folder, an unreadable install.json).</summary>
    public List<string> Notes { get; } = [];
    /// <summary>The Apps &amp; Features entry's folder and version, when there is an entry.</summary>
    public string? RegisteredDir { get; private set; }
    public string? RegisteredVersion { get; private set; }

    /// <param name="registry">Where the Apps &amp; Features entry is read (only read), or null to skip it.</param>
    /// <param name="chosen">A folder the player gave, or null.</param>
    /// <param name="processPath">This Setup's exe (its install when it is the copy in one), or null.</param>
    /// <param name="defaultDir">The default install folder, or null.</param>
    public static InstallDetection Find(IRegistryWriter? registry, string? chosen, string? processPath, string? defaultDir)
    {
        var d = new InstallDetection();
        d.Consider(chosen, InstallOrigin.Chosen);
        if (processPath is not null)
        {
            d.Consider(SetupCopy.InstallOf(processPath), InstallOrigin.RunningFrom);
        }
        if (registry is not null)
        {
            try
            {
                d.RegisteredDir = registry.GetValue(UninstallEntry.Key, "InstallLocation") as string;
                d.RegisteredVersion = registry.GetValue(UninstallEntry.Key, "DisplayVersion") as string;
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Security.SecurityException)
            {
                d.Notes.Add($"The Apps & Features entry could not be read: {e.Message}");
            }
            if (d.RegisteredDir is { Length: > 0 } reg && !d.Consider(reg, InstallOrigin.Registered))
            {
                d.Notes.Add($"Windows' Installed apps lists Quake VR: Unleashed {d.RegisteredVersion} in {reg}, but no install is there " +
                            "any more (moved or deleted): browse to it if it moved.");
            }
        }
        d.Consider(defaultDir, InstallOrigin.DefaultFolder);
        return d;
    }

    /// <summary>Adds the install in <paramref name="dir"/> when there is one; true when the folder has an install.</summary>
    bool Consider(string? dir, InstallOrigin origin)
    {
        if (dir is null || PathUtil.TryNormalize(dir) is not { } path)
        {
            return false;
        }
        if (Found.Any(f => PathUtil.SamePath(f.Dir, path)))
        {
            return true;
        }
        try
        {
            if (InstallRecord.Load(path) is { } record)
            {
                Found.Add(new FoundInstall(path, record, origin));
                return true;
            }
        }
        catch (Exception e) when (e is IOException or InvalidDataException or UnauthorizedAccessException or System.Text.Json.JsonException)
        {
            Notes.Add($"{Path.Combine(path, InstallRecord.FileName)} could not be read ({e.Message}): passed over.");
        }
        return false;
    }

    /// <summary>What qvr-setup detect and update print.</summary>
    public string Format()
    {
        var w = new StringWriter();
        if (Picked is null)
        {
            w.WriteLine("Quake VR: Unleashed: not installed (a fresh install).");
        }
        foreach (var f in Found)
        {
            w.WriteLine($"Quake VR: Unleashed {f.Record.Version} in {f.Dir} (found from {f.OriginText}){(f == Picked ? " <- picked" : "")}");
            w.WriteLine($"  installed {f.Record.InstalledAt:yyyy-MM-dd}{(f.Record.UpdatedAt is { } u ? $", updated {u:yyyy-MM-dd}" : "")}; " +
                        $"{f.Record.Files.Count} program files; Quake: {f.Record.QuakeDir}; HD textures: {(f.Record.Files.Any(x => x.Component == Components.HdTextures) ? "yes" : "no")}; " +
                        $"relight at first start: {(f.Record.Choices.RelightOnFirstRun ? "yes" : "no")}");
        }
        if (RegisteredDir is not null)
        {
            w.WriteLine($"Apps & Features entry: {RegisteredVersion} in {RegisteredDir}");
        }
        foreach (var n in Notes)
        {
            w.WriteLine($"note: {n}");
        }
        return w.ToString();
    }
}
