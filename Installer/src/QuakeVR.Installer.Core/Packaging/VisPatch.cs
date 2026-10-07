using System.Formats.Tar;
using System.IO.Compression;
using System.Security.Cryptography;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>One of VisPatch's "vispatch data" 1.0 archives: its game, file name, size and pinned SHA-256.</summary>
public sealed record VisPatchArchive(string Game, string File, long Size, string Sha256);

/// <summary>
/// The VisPatch data for see-through water in the relit maps (part of the relight component, ticked with it). The
/// game's own relight applies it (<c>vr_relight_seethrough</c>, Quake/vr/vr_relight_vis.cpp) whenever the data is in
/// <c>&lt;QVR&gt;\quakevr\tools\vispatch</c>: <c>id1.vis</c>, <c>hipnotic.vis</c>, <c>rogue.vis</c> (or
/// <c>&lt;game&gt;\vispatch.dat</c>). Its licence is unknown (1997 data, the SourceForge VisPatch project), so it is never
/// shipped or mirrored by us (sezero/vispatch on GitHub has the tool's source only, no data: INSTALLER.md): downloaded on
/// demand from its original location, checked against the pinned SHA-256,
/// unpacked here (System.Formats.Tar over GZip), installed and recorded like any other file of the install.
/// </summary>
public static class VisPatch
{
    /// <summary>Where the game looks for the data, relative to the install folder.</summary>
    public const string Folder = "quakevr/tools/vispatch";

    /// <summary>The 1.0 archives as SourceForge serves them (fetched 2026-10-07).</summary>
    public static readonly IReadOnlyList<VisPatchArchive> Archives =
    [
        new("id1", "id1_vis.tgz", 949_270, "b16e400eaa650f30b17d8651c15ad4041b73270c97ad62cd62719b48be0d55f0"),
        new("hipnotic", "hipnotic_vis.tgz", 806_759, "254febdcdaa4f2c065633494614027c9cce993e803978794feefac88ce60fe49"),
        new("rogue", "rogue_vis.tgz", 786_802, "6dd44ae5920f2cf7abee27d2953d71d46d715c5375e8a2d7d2249bd24e6a796a"),
    ];

    /// <summary>The archives for id1 and the mission packs the player owns.</summary>
    public static IEnumerable<VisPatchArchive> For(IEnumerable<string> ownedPacks, IReadOnlyList<VisPatchArchive>? archives = null)
    {
        var owned = new HashSet<string>(ownedPacks, StringComparer.OrdinalIgnoreCase) { "id1" };
        return (archives ?? Archives).Where(a => owned.Contains(a.Game));
    }

    /// <summary>The download addresses of an archive: each template's <c>{file}</c> replaced, in order.</summary>
    public static IReadOnlyList<Uri> Mirrors(VisPatchArchive archive, IEnumerable<string> templates) =>
        [.. templates.Select(t => new Uri(t.Replace("{file}", Uri.EscapeDataString(archive.File), StringComparison.Ordinal)))];

    /// <summary>The data files of an archive: <c>&lt;game&gt;.vis</c> at its top, or <c>&lt;game&gt;/vispatch.dat</c>, with
    /// their install paths (<see cref="Folder"/>, lower case). Anything else (rogue.txt, links, folders, a path
    /// leaving the folder) is skipped. Throws InvalidDataException for a damaged archive.</summary>
    public static List<(string Relative, byte[] Data)> Extract(string tgzPath)
    {
        var files = new List<(string, byte[])>();
        using var file = File.OpenRead(tgzPath);
        using var gzip = new GZipStream(file, CompressionMode.Decompress);
        using var tar = new TarReader(gzip);
        try
        {
            while (tar.GetNextEntry() is { } entry)
            {
                if (entry.EntryType is not (TarEntryType.RegularFile or TarEntryType.V7RegularFile) || entry.DataStream is null)
                {
                    continue;
                }
                var name = entry.Name.Replace('\\', '/').ToLowerInvariant();
                while (name.StartsWith("./", StringComparison.Ordinal))
                {
                    name = name[2..]; // "./id1.vis", as some tar tools write it; "../" is refused below.
                }
                var parts = name.Split('/');
                var isData = (parts.Length == 1 && name.EndsWith(".vis", StringComparison.Ordinal)) ||
                             (parts.Length == 2 && parts[1] == "vispatch.dat");
                if (!isData || parts.Any(p => p is "" or "." or ".." || p.Contains(':')) || entry.Length > 64L << 20)
                {
                    continue;
                }
                using var ms = new MemoryStream((int)entry.Length);
                entry.DataStream.CopyTo(ms);
                files.Add(($"{Folder}/{name}", ms.ToArray()));
            }
        }
        catch (Exception e) when (e is EndOfStreamException or FormatException or InvalidOperationException)
        {
            throw new InvalidDataException($"{Path.GetFileName(tgzPath)} is damaged ({e.Message})", e);
        }
        return files;
    }

    /// <summary>Whether a file is the archive (size and SHA-256): a copy the player already has can be used as is.</summary>
    public static bool Matches(string path, VisPatchArchive archive)
    {
        if (!File.Exists(path) || new FileInfo(path).Length != archive.Size)
        {
            return false;
        }
        using var s = File.OpenRead(path);
        return string.Equals(Convert.ToHexStringLower(SHA256.HashData(s)), archive.Sha256, StringComparison.OrdinalIgnoreCase);
    }
}
