namespace QuakeVR.Installer.Core.Detection;

public enum Id1Kind
{
    /// <summary>No id1\pak0.pak.</summary>
    Missing,
    /// <summary>pak0 only, small: the shareware episode. Quake VR needs the full game.</summary>
    Shareware,
    /// <summary>The original game: pak0.pak and pak1.pak.</summary>
    Original,
    /// <summary>The 2021 rerelease: one large pak0.pak (and QuakeEX.kpf beside id1).</summary>
    Rerelease,
    /// <summary>A pak that is not a valid archive.</summary>
    Corrupt,
}

public sealed record Id1Info(Id1Kind Kind, string Detail, bool KnownVersion = false)
{
    /// <summary>Whether the engine can start from this folder as its first -basedir (it checks id1\pak0.pak).</summary>
    public bool Playable => Kind is Id1Kind.Original or Id1Kind.Rerelease;
}

/// <summary>What a folder's <c>id1</c> holds (section 2 of docs/vr-port/INSTALLER.md, "What counts as present").</summary>
public static class QuakeData
{
    /// <summary>The sizes of Quake 1.06's paks (the version every store sells as the original).</summary>
    public const long Pak0Size106 = 18_689_235;
    public const long Pak1Size106 = 34_257_856;

    /// <summary>A pak0 this large is the rerelease's (about 220 MB; the original's is 18 MB).</summary>
    public const long RereleasePak0MinSize = 100L << 20;

    public static Id1Info Inspect(string root)
    {
        var id1 = Path.Combine(root, "id1");
        var pak0 = Path.Combine(id1, "pak0.pak");
        var pak1 = Path.Combine(id1, "pak1.pak");
        if (!File.Exists(pak0))
        {
            return new(Id1Kind.Missing, $"no id1\\pak0.pak in {root}");
        }
        if (!ValidPak(pak0))
        {
            return new(Id1Kind.Corrupt, $"{pak0} is not a valid Quake archive");
        }
        var pak0Size = new FileInfo(pak0).Length;
        if (File.Exists(pak1))
        {
            if (!ValidPak(pak1))
            {
                return new(Id1Kind.Corrupt, $"{pak1} is not a valid Quake archive");
            }
            var pak1Size = new FileInfo(pak1).Length;
            var known = pak0Size == Pak0Size106 && pak1Size == Pak1Size106;
            return new(Id1Kind.Original, known ? "Quake 1.06 (pak0 + pak1)" : "pak0 + pak1 (unrecognised version; should still work)", known);
        }
        if (pak0Size >= RereleasePak0MinSize || File.Exists(Path.Combine(root, "QuakeEX.kpf")))
        {
            return new(Id1Kind.Rerelease, $"2021 rerelease ({PathUtil.FormatSize(pak0Size)} pak0)", true);
        }
        return new(Id1Kind.Shareware, "shareware only: the full game's id1\\pak1.pak is missing");
    }

    static bool ValidPak(string path)
    {
        try
        {
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            return PakFile.TryReadDirectory(fs, maxEntries: 1 << 16) is not null;
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
    }

    /// <summary>A folder holding music files read in place (decision 8): the rerelease's <c>id1\music</c> and the
    /// original's tracks. Null when there is none.</summary>
    public static string? FindMusic(string root)
    {
        foreach (var dir in new[] { Path.Combine(root, "id1", "music"), Path.Combine(root, "music") })
        {
            try
            {
                if (Directory.Exists(dir) && Directory.EnumerateFiles(dir).Any(f =>
                        Path.GetExtension(f).ToLowerInvariant() is ".ogg" or ".mp3" or ".flac" or ".wav" or ".opus"))
                {
                    return dir;
                }
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
        return null;
    }
}
