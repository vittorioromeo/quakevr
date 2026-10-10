using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>One downloadable file: the same name, size and SHA-256 on every host.</summary>
public sealed class FeedFile
{
    public string File { get; set; } = "";
    public long Size { get; set; }
    public string Sha256 { get; set; } = "";
    /// <summary>Mirrors in order (the GitHub release's own file first; a release may list others after it).</summary>
    public List<string> Urls { get; set; } = [];

    [JsonIgnore]
    public IReadOnlyList<Uri> Mirrors => [.. Urls.Select(u => new Uri(u))];
}

/// <summary>
/// <c>latest.json</c>, an asset of each GitHub release (section 5, "Updates"): the newest version, the package, and the
/// optional components. The installer reads it from the latest release only (decision 2026-10-08: GitHub is the one feed).
/// <code>
/// { "schema": 1, "version": "2026-10-06 c131f4bf",
///   "package":    { "file": "QuakeVR.zip", "size": ..., "sha256": "...", "urls": ["https://github.com/..."] },
///   "components": { "hdtextures": { "file": "quakevr-hq-textures-png-2026-10-03.zip", ... } } }
/// </code>
/// </summary>
public sealed class ReleaseFeed
{
    public int Schema { get; set; } = 1;
    public string Version { get; set; } = "";
    public FeedFile? Package { get; set; }
    public Dictionary<string, FeedFile> Components { get; set; } = [];
    public string? Notes { get; set; }

    public static ReleaseFeed Parse(string json)
    {
        var feed = JsonSerializer.Deserialize<ReleaseFeed>(json, PackageManifest.Json) ?? throw new InvalidDataException("empty release feed");
        if (feed.Schema != 1)
        {
            throw new InvalidDataException($"release feed schema {feed.Schema}: update the installer");
        }
        foreach (var f in feed.Components.Values.Append(feed.Package).OfType<FeedFile>())
        {
            if (f.Sha256.Length != 64 || f.Urls.Count == 0 || f.Urls.Any(u => !Uri.TryCreate(u, UriKind.Absolute, out var uri) || uri.Scheme is not ("https" or "http")))
            {
                throw new InvalidDataException($"release feed: bad entry for {f.File}");
            }
        }
        return feed;
    }

    /// <summary>The first feed that answers and parses, from the hosts in order.</summary>
    public static async Task<ReleaseFeed> FetchAsync(HttpClient http, IEnumerable<Uri> feeds, CancellationToken ct)
    {
        var errors = new List<string>();
        foreach (var uri in feeds)
        {
            try
            {
                using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
                cts.CancelAfter(TimeSpan.FromSeconds(20));
                return Parse(await http.GetStringAsync(uri, cts.Token).ConfigureAwait(false));
            }
            catch (Exception e) when (e is HttpRequestException or InvalidDataException or JsonException)
            {
                errors.Add($"{uri.Host}: {e.Message}");
            }
            catch (OperationCanceledException) when (!ct.IsCancellationRequested)
            {
                errors.Add($"{uri.Host}: timed out");
            }
        }
        throw new InstallException($"Could not read the release list ({string.Join("; ", errors)}).");
    }
}

/// <summary>
/// The optional components the installer knows without a feed: when latest.json cannot be read, or names no such
/// component, these are used instead (a feed that names one always wins). The same check as a feed's: size and SHA-256
/// after the download. Pinned from <c>Misc/release/support_assets.json</c> (the support-files release); bumping the
/// texture pack means changing both (docs/vr-port/RELEASING.md, "Support files"; the self-test compares them).
/// </summary>
public static class BuiltInComponents
{
    /// <summary>The HD texture pack: the support-files release first, then the release it was first published on.</summary>
    public static FeedFile HdTextures() => new()
    {
        File = "quakevr-hq-textures-png-2026-10-03.zip",
        Size = 614_919_925,
        Sha256 = "0c0df0e7b19525ba3fabfd1a88cce871b4b1321a5cae0134d5ae21636116b706",
        Urls =
        [
            "https://github.com/vittorioromeo/quakevr/releases/download/assets-2026-10-08/quakevr-hq-textures-png-2026-10-03.zip",
            "https://github.com/vittorioromeo/quakevr/releases/download/textures-2026-10-03/quakevr-hq-textures-png-2026-10-03.zip",
        ],
    };

    /// <summary>Every built-in component by its feed name (new copies: a caller may change them).</summary>
    public static Dictionary<string, FeedFile> All() => new(StringComparer.OrdinalIgnoreCase) { [Components.HdTextures] = HdTextures() };
}

/// <summary>An optional component to download: from the feed, or the installer's built-in copy of it.</summary>
public sealed record ResolvedComponent(string Name, FeedFile File, bool BuiltIn)
{
    public string SourceText => BuiltIn ? "built into the installer" : "from the release list";
}

/// <summary>GitHub's releases API (<c>/repos/{owner}/{repo}/releases/latest</c>): the newest release's assets, for
/// when latest.json is not reachable. GitHub reports each asset's SHA-256 as <c>digest: "sha256:..."</c>.</summary>
public static class GitHubReleases
{
    public sealed record Asset(string Name, long Size, string Url, string? Sha256);

    public static async Task<(string Tag, IReadOnlyList<Asset> Assets)> LatestAsync(HttpClient http, Uri api, CancellationToken ct)
    {
        using var request = new HttpRequestMessage(HttpMethod.Get, api);
        request.Headers.Accept.ParseAdd("application/vnd.github+json");
        using var response = await http.SendAsync(request, ct).ConfigureAwait(false);
        response.EnsureSuccessStatusCode();
        using var doc = JsonDocument.Parse(await response.Content.ReadAsStringAsync(ct).ConfigureAwait(false));
        var root = doc.RootElement;
        var tag = root.GetProperty("tag_name").GetString() ?? "";
        var assets = new List<Asset>();
        foreach (var a in root.GetProperty("assets").EnumerateArray())
        {
            var digest = a.TryGetProperty("digest", out var d) && d.ValueKind == JsonValueKind.String ? d.GetString() : null;
            assets.Add(new Asset(
                a.GetProperty("name").GetString() ?? "",
                a.GetProperty("size").GetInt64(),
                a.GetProperty("browser_download_url").GetString() ?? "",
                digest is not null && digest.StartsWith("sha256:", StringComparison.Ordinal) ? digest[7..] : null));
        }
        return (tag, assets);
    }
}

/// <summary>Where the installer looks for releases. Defaults point at the real hosts; a test (or a
/// <c>installer-settings.json</c> beside the exe, or <c>--feed</c>) points them elsewhere.</summary>
public sealed class InstallerSettings
{
    public List<string> FeedUrls { get; set; } = DefaultFeedUrls(InstallerBuild.Version);

    /// <summary>GitHub's Latest release's feed: what a final release's installer reads (the newest release).</summary>
    public const string LatestFeed = "https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json";

    /// <summary>One release's own feed, by its version (<c>releases/download/v&lt;version&gt;/latest.json</c>).</summary>
    public static string TagFeed(string version) => $"https://github.com/vittorioromeo/quakevr/releases/download/v{version}/latest.json";

    /// <summary>The release hosts' feeds for an installer of <paramref name="ownVersion"/>: a final release reads
    /// Latest's alone (it wants the newest). A prerelease (<c>x.y.z-suffix</c>) is never GitHub's Latest, so its own
    /// tag's feed comes first (it finds its own package), then Latest's (docs/vr-port/RELEASING.md, "Prereleases").</summary>
    public static List<string> DefaultFeedUrls(string? ownVersion) =>
        ReleaseVersion.Parse(ownVersion) is { Number: not null, Suffix: not null } v ? [TagFeed(v.Short), LatestFeed] : [LatestFeed];

    public string GitHubApi { get; set; } = "https://api.github.com/repos/vittorioromeo/quakevr/releases/latest";

    /// <summary>Where VisPatch's archives are downloaded, in order (<c>{file}</c>: <c>id1_vis.tgz</c>...): its original
    /// SourceForge location. A mirror added here must serve the same files (their SHA-256 is pinned).</summary>
    public List<string> VisPatchUrls { get; set; } =
    [
        "https://sourceforge.net/projects/vispatch/files/vispatch%20data/1.0/{file}/download",
        "https://downloads.sourceforge.net/project/vispatch/vispatch%20data/1.0/{file}",
    ];

    /// <summary>The optional components used when the feed is unavailable or does not name them (defaults:
    /// <see cref="Packaging.BuiltInComponents"/>; installer-settings.json's <c>builtInComponents</c> replaces them, for
    /// tests).</summary>
    public Dictionary<string, FeedFile> BuiltInComponents { get; set; } = Packaging.BuiltInComponents.All();

    /// <summary>A component to download: the feed's when it names it (the feed wins), else the built-in one, else none.
    /// <paramref name="feed"/> is null when no feed could be read.</summary>
    public ResolvedComponent? Component(ReleaseFeed? feed, string name) =>
        feed?.Components.GetValueOrDefault(name) is { } fromFeed ? new ResolvedComponent(name, fromFeed, false) :
        BuiltInComponents.GetValueOrDefault(name) is { } builtIn ? new ResolvedComponent(name, builtIn, true) :
        null;

    /// <summary>The environment variable that points the installer (window and qvr-setup) at another feed, like
    /// <c>--feed</c>: one URL, or several separated by ';' (Misc\release\test_local_release.ps1's local server).</summary>
    public const string FeedEnvVar = "QVR_SETUP_FEED";

    /// <summary>The feeds <see cref="FeedEnvVar"/> names (none when it is unset).</summary>
    public static List<string> FeedsFromEnvironment() =>
        [.. (Environment.GetEnvironmentVariable(FeedEnvVar) ?? "").Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)];

    /// <summary>The release hosts' own feeds (a build that reads any other is a test: the window says so).</summary>
    public bool HasDefaultFeeds => IsReleaseHostFeeds(FeedUrls);

    /// <summary>Whether every one of these feeds is one of the release hosts' own (only those are not a test).</summary>
    public static bool IsReleaseHostFeeds(IEnumerable<string> feeds) =>
        feeds.All(f => new InstallerSettings().FeedUrls.Contains(f, StringComparer.OrdinalIgnoreCase));

    public static InstallerSettings Load(string? path)
    {
        if (path is null || !File.Exists(path))
        {
            return new InstallerSettings();
        }
        return JsonSerializer.Deserialize<InstallerSettings>(File.ReadAllText(path), PackageManifest.Json) ?? new InstallerSettings();
    }
}

/// <summary>This installer's own build.</summary>
public static class InstallerBuild
{
    /// <summary>Its version as the release named it (<c>1.0.0</c>, <c>1.0.0-beta.1</c>): the assembly's informational
    /// version (make_release.ps1's <c>/p:Version</c>, else the repository's VERSION file) without the <c>+commit</c>
    /// the SDK appends. The assembly and file versions drop a prerelease suffix, so they cannot say this.</summary>
    public static string Version { get; } = Read(typeof(InstallerBuild).Assembly);

    public static string Read(Assembly assembly)
    {
        var text = assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion
            ?? assembly.GetName().Version?.ToString(3) ?? "";
        var plus = text.IndexOf('+');
        return plus >= 0 ? text[..plus] : text;
    }
}
