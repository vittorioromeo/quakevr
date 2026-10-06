namespace QuakeVR.Installer.Core;

/// <summary>Path helpers shared by detection, install and uninstall.</summary>
public static class PathUtil
{
    /// <summary>A full path with backslashes and no trailing separator (except a drive root), or null if invalid.</summary>
    public static string? TryNormalize(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return null;
        }
        try
        {
            var full = Path.GetFullPath(path.Trim().Trim('"'));
            return TrimEnd(full);
        }
        catch (Exception e) when (e is ArgumentException or NotSupportedException or PathTooLongException)
        {
            return null;
        }
    }

    /// <summary>The path as the file system spells it (Steam's registry value is all lowercase), or the path itself
    /// when a part does not exist.</summary>
    public static string RealCase(string path)
    {
        try
        {
            var full = TryNormalize(path);
            if (full is null)
            {
                return path;
            }
            var root = Path.GetPathRoot(full)!;
            var result = root.ToUpperInvariant();
            foreach (var part in full[root.Length..].Split('\\', StringSplitOptions.RemoveEmptyEntries))
            {
                var match = Directory.EnumerateFileSystemEntries(result, part).FirstOrDefault();
                if (match is null)
                {
                    return full;
                }
                result = Path.Combine(result, Path.GetFileName(match));
            }
            return result;
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or ArgumentException)
        {
            return path;
        }
    }

    public static string TrimEnd(string path)
    {
        var root = Path.GetPathRoot(path) ?? "";
        while (path.Length > root.Length && (path.EndsWith('\\') || path.EndsWith('/')))
        {
            path = path[..^1];
        }
        return path;
    }

    public static bool SamePath(string a, string b) =>
        string.Equals(TryNormalize(a), TryNormalize(b), StringComparison.OrdinalIgnoreCase);

    /// <summary>Whether <paramref name="path"/> is <paramref name="folder"/> or inside it.</summary>
    public static bool IsInside(string path, string folder)
    {
        var p = TryNormalize(path);
        var f = TryNormalize(folder);
        if (p is null || f is null)
        {
            return false;
        }
        return string.Equals(p, f, StringComparison.OrdinalIgnoreCase) ||
               p.StartsWith(f.EndsWith('\\') ? f : f + "\\", StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>
    /// <paramref name="root"/> joined with a manifest's relative path (forward slashes). Throws for anything that is
    /// not a plain relative path inside <paramref name="root"/> (rooted, "..", drive letters, empty parts): a
    /// manifest or install record can never make the installer write or delete outside its folder.
    /// </summary>
    public static string SafeCombine(string root, string relative)
    {
        if (string.IsNullOrEmpty(relative) || relative.Contains(':') || relative.StartsWith('/') || relative.StartsWith('\\'))
        {
            throw new InvalidDataException($"not a relative path: \"{relative}\"");
        }
        var parts = relative.Split('/', '\\');
        if (parts.Any(p => p.Length == 0 || p == "." || p == ".." || p.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0))
        {
            throw new InvalidDataException($"unsafe path in manifest: \"{relative}\"");
        }
        var full = Path.GetFullPath(Path.Combine([root, .. parts]));
        if (!IsInside(full, root))
        {
            throw new InvalidDataException($"path escapes the install folder: \"{relative}\"");
        }
        return full;
    }

    /// <summary>A path relative to <paramref name="root"/>, with forward slashes (the manifests' form).</summary>
    public static string ToRelative(string root, string full) => Path.GetRelativePath(root, full).Replace('\\', '/');

    public static string FormatSize(long bytes) => bytes switch
    {
        >= 1L << 30 => $"{bytes / (double)(1L << 30):0.0} GB",
        >= 1L << 20 => $"{bytes / (double)(1L << 20):0} MB",
        >= 1L << 10 => $"{bytes / (double)(1L << 10):0} KB",
        _ => $"{bytes} bytes",
    };
}
