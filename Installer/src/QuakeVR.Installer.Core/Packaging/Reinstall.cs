namespace QuakeVR.Installer.Core.Packaging;

/// <summary>"Install again from scratch": what the player chose to reset or remove (both unticked: only the program
/// files are installed again).</summary>
public sealed class ReinstallOptions
{
    /// <summary>The configs, retro overrides and body calibration (<see cref="UserDataKind.Settings"/>).</summary>
    public bool ResetSettings { get; init; }
    /// <summary>The saves and the Map Library's installed maps (<see cref="UserDataKind.Saves"/>, <see cref="UserDataKind.Maps"/>).</summary>
    public bool RemoveSaves { get; init; }

    public bool Includes(UserDataKind kind) => kind switch
    {
        UserDataKind.Settings => ResetSettings,
        UserDataKind.Saves or UserDataKind.Maps => RemoveSaves,
        _ => false,
    };
}

/// <summary>
/// The first step of "Install again from scratch": the player's files it resets or removes are moved into a dated backup
/// (<see cref="Backup"/>, checked file by file), never deleted; then the normal full install runs over the folder
/// (InstallMode.Install, with the same backup for anything else it overwrites). Screenshots, voice notes, relit maps,
/// checklist ticks and caches are never part of it.
/// </summary>
public static class Reinstaller
{
    /// <summary>The files the choice resets or removes (nothing is changed).</summary>
    public static List<(string Path, UserDataKind Kind)> Plan(string installDir, ReinstallOptions options)
    {
        InstallRecord? record = null;
        try
        {
            record = InstallRecord.Load(installDir);
        }
        catch (Exception e) when (e is IOException or InvalidDataException or System.Text.Json.JsonException)
        {
            // Unreadable: every file not Setup's counts as the player's (more goes into the backup, never less).
        }
        return [.. UserData.Find(installDir, record).Where(f => options.Includes(f.Kind))];
    }

    /// <summary>The plan as the console prints it.</summary>
    public static string Format(IReadOnlyList<(string Path, UserDataKind Kind)> files, ReinstallOptions options, int maxLines = 40)
    {
        var w = new StringWriter();
        w.WriteLine($"reinstall: reset settings {(options.ResetSettings ? "yes" : "no")}, remove saves and installed maps {(options.RemoveSaves ? "yes" : "no")}");
        if (files.Count == 0)
        {
            w.WriteLine("  nothing of yours to reset or remove: no backup needed");
        }
        else
        {
            w.WriteLine($"  moved into a dated backup (<install>\\{UserData.BackupsFolder}\\<date> reinstall), checked: {files.Count} file(s) ({UserData.Summary(files)})");
            foreach (var f in files.Take(maxLines))
            {
                w.WriteLine($"    {f.Kind.ToString().ToLowerInvariant(),-9} {f.Path}");
            }
            if (files.Count > maxLines)
            {
                w.WriteLine($"    ... and {files.Count - maxLines} more");
            }
        }
        return w.ToString();
    }

    /// <summary>Moves the chosen files into a new backup and checks it; returns it (null when there was nothing to move).
    /// An error stops it: what was moved is in the backup (backup.json lists it), the rest is where it was.</summary>
    public static Backup? Prepare(string installDir, ReinstallOptions options, DateTimeOffset now, IProgress<InstallProgress>? progress = null)
    {
        var target = PathUtil.TryNormalize(installDir) ?? throw new InstallException($"bad folder {installDir}");
        var files = Plan(target, options);
        if (files.Count == 0)
        {
            return null;
        }
        string? version = null;
        try
        {
            version = InstallRecord.Load(target)?.Version;
        }
        catch (Exception e) when (e is IOException or InvalidDataException or System.Text.Json.JsonException)
        {
        }
        var backup = Backup.Create(target, "reinstall", version, now);
        var i = 0;
        foreach (var (path, kind) in files)
        {
            progress?.Report(new InstallProgress(0.9 * ++i / files.Count, "Backing up your files"));
            backup.Move(path, kind.ToString().ToLowerInvariant());
        }
        // The folders the move emptied (a removed map package's), up to the install's own.
        foreach (var dir in files.Select(f => Path.GetDirectoryName(PathUtil.SafeCombine(target, f.Path))!).Distinct(StringComparer.OrdinalIgnoreCase)
                     .OrderByDescending(d => d.Length))
        {
            for (var d = dir; PathUtil.IsInside(d, target) && !PathUtil.SamePath(d, target) && InstallEngine.TryDeleteEmptyDirectory(d); d = Path.GetDirectoryName(d)!)
            {
            }
        }
        var problems = Backup.Verify(backup.Dir);
        if (problems.Count > 0)
        {
            throw new InstallException($"The backup in {backup.Dir} is not complete ({string.Join("; ", problems.Take(3))}): stopped before installing.");
        }
        progress?.Report(new InstallProgress(1, "Backing up your files",
            $"{files.Count} file(s) of yours set aside in {backup.Dir} ({UserData.Summary(files)}); checked.", LogLevel.Success));
        return backup;
    }
}
