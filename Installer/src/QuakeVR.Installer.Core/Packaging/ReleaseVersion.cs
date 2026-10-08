using System.Globalization;
using System.Text.RegularExpressions;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>How a package's version stands against the installed one.</summary>
public enum VersionOrder
{
    /// <summary>The package is newer: Update.</summary>
    Newer,
    /// <summary>The same build: Repair.</summary>
    Same,
    /// <summary>The package is older: Repair (never a downgrade; only "Install again from scratch" installs it).</summary>
    Older,
    /// <summary>Another build that cannot be ordered (the same version and day, another commit; an unreadable text):
    /// treated as an update (its program files replace the installed ones).</summary>
    Other,
}

/// <summary>
/// A version as manifest.json and latest.json give it: <c>0.9.1 (2026-10-08 abcdef12)</c> (a release,
/// make_release.ps1), <c>0.9.1-dev (2026-10-08 abcdef12-dirty)</c> (a development package), <c>0.9.1</c>, or the older
/// stamp alone, <c>2026-10-06 c131f4bf</c>. Ordered by MAJOR.MINOR.PATCH, then a pre-release suffix (none is newer than
/// any: 0.9.1-dev &lt; 0.9.1), then the build's commit date.
/// </summary>
public sealed partial record ReleaseVersion(string Text, Version? Number, string? Suffix, DateOnly? Date, string? Commit)
{
    [GeneratedRegex(@"^\s*(?<num>\d+\.\d+\.\d+)(?:-(?<suffix>[0-9A-Za-z][0-9A-Za-z.]*))?")]
    private static partial Regex NumberPattern();

    [GeneratedRegex(@"(?<date>\d{4}-\d{2}-\d{2})\s+(?<commit>[0-9a-f]{6,40})")]
    private static partial Regex StampPattern();

    public static ReleaseVersion Parse(string? text)
    {
        text = (text ?? "").Trim();
        var n = NumberPattern().Match(text);
        var s = StampPattern().Match(text);
        return new ReleaseVersion(text,
            n.Success ? Version.Parse(n.Groups["num"].Value) : null,
            n.Success && n.Groups["suffix"].Success ? n.Groups["suffix"].Value : null,
            s.Success && DateOnly.TryParseExact(s.Groups["date"].Value, "yyyy-MM-dd", CultureInfo.InvariantCulture, DateTimeStyles.None, out var d) ? d : null,
            s.Success ? s.Groups["commit"].Value : null);
    }

    /// <summary>The number and suffix alone ("0.9.1", "0.9.1-dev"), or the whole text when it has none.</summary>
    public string Short => Number is null ? Text : Suffix is null ? Number.ToString() : $"{Number}-{Suffix}";

    /// <summary>How <paramref name="package"/> stands against <paramref name="installed"/>.</summary>
    public static VersionOrder Compare(string? installed, string? package)
    {
        var a = Parse(installed);
        var b = Parse(package);
        if (a.Text.Length > 0 && string.Equals(a.Text, b.Text, StringComparison.OrdinalIgnoreCase))
        {
            return VersionOrder.Same;
        }
        int? order = null;
        if (a.Number is not null && b.Number is not null)
        {
            order = b.Number.CompareTo(a.Number);
            if (order == 0)
            {
                // A pre-release comes before its release; two suffixes in text order.
                order = (a.Suffix, b.Suffix) switch
                {
                    (null, null) => 0,
                    (null, _) => -1,
                    (_, null) => 1,
                    _ => string.CompareOrdinal(b.Suffix, a.Suffix),
                };
            }
        }
        else if (a.Number is not null || b.Number is not null)
        {
            // The old stamp-only versions came before the numbered ones.
            order = b.Number is not null ? 1 : -1;
        }
        if (order is null or 0 && a.Date is { } da && b.Date is { } db && da != db)
        {
            order = db.CompareTo(da);
        }
        return order switch
        {
            > 0 => VersionOrder.Newer,
            < 0 => VersionOrder.Older,
            0 when a.Commit is not null && string.Equals(a.Commit, b.Commit, StringComparison.OrdinalIgnoreCase) => VersionOrder.Same,
            0 when a.Commit is null && b.Commit is null && a.Date == b.Date => VersionOrder.Same,
            _ => VersionOrder.Other,
        };
    }
}
