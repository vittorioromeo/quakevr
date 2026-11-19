using System.Buffers.Binary;
using System.IO.Compression;
using System.Security.Cryptography;
using QuakeVR.Installer.Core.Detection;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>The release's pak2.pak as the engine checks it (vr_ad.cpp <c>versionStatus</c>): its size, and the SHA-256
/// of its 12-byte header followed by its directory.</summary>
public sealed record AdPak2Pin(long Size, string DirSha256);

/// <summary>
/// Arcane Dimensions, the installer's experimental opt-in component (docs/vr-port/ARCANE_DIMENSIONS.md, section 10):
/// sock's unmodified release, <c>ad_v1_80p1final.zip</c> (<see cref="BuiltInComponents.ArcaneDimensions"/>), downloaded
/// from its own hosts, checked (size and SHA-256), and unpacked as it is into <c>&lt;install&gt;\ad</c> (the Quake VR
/// folder is the game's last -basedir, so one of the owned roots the game finds AD in). Its files are recorded in
/// install.json as component <c>ad</c>: an update keeps them, a repair restores them, the uninstall removes them. A copy
/// of 1.80 patch 1 already in an owned Quake root is used where it is instead (nothing downloaded).
/// </summary>
public static class ArcaneDimensions
{
    public const string Folder = "ad";
    public const string Title = "Arcane Dimensions";
    /// <summary>The engine's pin (Quake/vr/vr_ad_data.inc: VR_AD_PAK2_SIZE, VR_AD_PAK2_DIRSHA; the self-test compares).</summary>
    public static readonly AdPak2Pin Latest = new(22_309_198, "38c6fcb87d8aa70c1bec9e3ad75b81c6f07266ed478555f7ae793e9a8172a34a");
    /// <summary>1.80 patch 1's files unpacked (the three paks, quake.rc, the txt, def and fgd files).</summary>
    public const long InstalledSize = 830_590_195;
    /// <summary>The release's page (its readme, credits and links to sock's site).</summary>
    public const string HomeUrl = "https://www.quaddicted.com/reviews/ad_v1_80p1final.html";
    public const string ReadmeFile = "ad_v1_80_readme.txt";
    public const string Credit = "free, by sock and team";

    /// <summary>pak2.pak's size and header-and-directory SHA-256, or null when it is missing or not a pak.</summary>
    public static (long Size, string DirSha256)? Pak2Signature(string pak2)
    {
        try
        {
            using var f = File.OpenRead(pak2);
            var size = f.Length;
            var header = new byte[12];
            if (f.Read(header, 0, 12) != 12 || header[0] != 'P' || header[1] != 'A' || header[2] != 'C' || header[3] != 'K')
            {
                return null;
            }
            var offset = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(4));
            var length = BinaryPrimitives.ReadInt32LittleEndian(header.AsSpan(8));
            if (offset < 12 || length <= 0 || length >= 1 << 20 || offset > size - length)
            {
                return null;
            }
            var bytes = new byte[12 + length];
            header.CopyTo(bytes, 0);
            f.Seek(offset, SeekOrigin.Begin);
            f.ReadExactly(bytes, 12, length);
            return (size, Convert.ToHexStringLower(SHA256.HashData(bytes)));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    /// <summary>Whether this ad folder's pak2.pak is 1.80 patch 1's (the engine's check).</summary>
    public static bool IsLatest(string adFolder, AdPak2Pin? pin = null)
    {
        pin ??= Latest;
        return Pak2Signature(Path.Combine(adFolder, "pak2.pak")) is { } s && s.Size == pin.Size &&
               string.Equals(s.DirSha256, pin.DirSha256, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>The owned roots an existing copy is looked for in: the stores', then the Quake folder's (the engine's
    /// <c>ownedRoots()</c> without the Quake VR folder, which holds only the copy Setup installs), lowest priority first.</summary>
    public static IReadOnlyList<string> SearchRoots(IReadOnlyList<string> storeRoots, string quakeDir) =>
        ExpansionDetector.OwnedRoots(storeRoots, [quakeDir]);

    /// <summary>The copy the game would use, by its rules (Quake/vr/vr_gamedir.cpp <c>discoverCampaigns</c>): the
    /// highest-priority root whose ad folder has any of AD's data; then its state. Null: none.</summary>
    public static ModCopy? FindExisting(IReadOnlyList<string> ownedRoots, AdPak2Pin? pin = null)
    {
        var resources = PackLists.Resources(Folder);
        for (var r = ownedRoots.Count - 1; r >= 0; --r)
        {
            var folder = Path.Combine(ownedRoots[r], Folder);
            if (!Directory.Exists(folder))
            {
                continue;
            }
            var status = PackInspector.Inspect(Folder, resources, [ownedRoots[r]]);
            if (status.Status == PackStatus.Missing)
            {
                continue; // none of its data: a lower root's copy counts
            }
            var root = PathUtil.TryNormalize(ownedRoots[r]) ?? ownedRoots[r];
            var where = PathUtil.TryNormalize(folder) ?? folder;
            if (status.Status == PackStatus.Incomplete)
            {
                return new ModCopy(root, where, ModCopyState.Incomplete, $"incomplete ({status.Detail})");
            }
            return IsLatest(folder, pin)
                ? new ModCopy(root, where, ModCopyState.Ready, "1.80 patch 1")
                : new ModCopy(root, where, ModCopyState.OtherVersion, File.Exists(Path.Combine(folder, "pak2.pak"))
                    ? "not 1.80 patch 1 (its pak2.pak is another release's)" : "not 1.80 patch 1 (no pak2.pak: 1.80 or older)");
        }
        return null;
    }

    /// <summary>The line the Options page and the console show for a found copy.</summary>
    public static string Describe(ModCopy copy) => copy.State switch
    {
        ModCopyState.Ready => $"Found in {copy.Folder}: will be used (nothing downloaded).",
        ModCopyState.OtherVersion => $"Found in {copy.Folder}, but it isn't 1.80 patch 1, the only release Quake VR plays: tick to download 1.80 patch 1 into Quake VR's folder (your copy is left as it is).",
        _ => $"Found in {copy.Folder}, {copy.Detail}: tick to download a complete copy into Quake VR's folder (yours is left as it is).",
    };

    /// <summary>The shared third-party component path's description of AD (<see cref="ModComponent"/>).</summary>
    public static readonly ModSpec Spec = new(Components.ArcaneDimensions, Folder, Title, "pak0.pak", null,
        r => (r.ArcaneDimensionsFile, r.ArcaneDimensionsSha256),
        (r, file, sha) => { r.ArcaneDimensionsFile = file; r.ArcaneDimensionsSha256 = sha; },
        (r, on) => r.Choices.ArcaneDimensions = on,
        "qvr-setup ad");

    /// <summary>The zip's files as they go in the install: each under <c>ad/</c> (a single top folder the zip has, such as
    /// its own <c>ad/</c>, is that folder).</summary>
    public static List<(string Relative, ZipArchiveEntry Entry)> Entries(ZipArchive zip) => ModComponent.Entries(Spec, zip);

    /// <summary>Install-time and repair-time plan: what an update or repair does with the installed copy.</summary>
    public static (ModAction Action, string Text) Plan(string installDir, InstallRecord installed, FeedFile? target, bool verify) =>
        ModComponent.Plan(Spec, installDir, installed, target, verify);

    /// <summary>Unpacks the checked zip into <c>&lt;install&gt;\ad</c> (<see cref="ModComponent.Install"/>).</summary>
    public static InstallRecord Install(string installDir, string zipPath, FeedFile expected, IProgress<InstallProgress>? progress, CancellationToken ct) =>
        ModComponent.Install(Spec, installDir, zipPath, expected, progress, ct);

    /// <summary>The whole optional step (<see cref="ModComponent.AddAsync"/>); never throws.</summary>
    public static Task<ModOutcome> AddAsync(HttpClient http, string installDir, FeedFile component, string downloadsDir, string? localZip,
        IProgress<InstallProgress>? progress, IProgress<DownloadProgress>? download, CancellationToken ct, int attemptsPerMirror = 2) =>
        ModComponent.AddAsync(Spec, http, installDir, component, downloadsDir, localZip, progress, download, ct, attemptsPerMirror);
}
