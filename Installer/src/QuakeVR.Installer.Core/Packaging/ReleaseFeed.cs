using System.Text.Json;
using System.Text.Json.Serialization;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>One downloadable file: the same name, size and SHA-256 on every host.</summary>
public sealed class FeedFile
{
    public string File { get; set; } = "";
    public long Size { get; set; }
    public string Sha256 { get; set; } = "";
    /// <summary>Mirrors in order (GitHub first, then vittorioromeo.com).</summary>
    public List<string> Urls { get; set; } = [];

    [JsonIgnore]
    public IReadOnlyList<Uri> Mirrors => [.. Urls.Select(u => new Uri(u))];
}

/// <summary>
/// <c>latest.json</c>, published on both hosts beside each release (section 5, "Updates"): the newest version, the
/// package, and the optional components. The installer reads the first host that answers.
/// <code>
/// { "schema": 1, "version": "2026-10-06 c131f4bf",
///   "package":    { "file": "QuakeVR.zip", "size": ..., "sha256": "...", "urls": ["https://github.com/...", "https://vittorioromeo.com/..."] },
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
    public List<string> FeedUrls { get; set; } =
    [
        "https://github.com/vittorioromeo/quakevr/releases/latest/download/latest.json",
        "https://vittorioromeo.com/quakevr/latest.json",
    ];

    public string GitHubApi { get; set; } = "https://api.github.com/repos/vittorioromeo/quakevr/releases/latest";

    public static InstallerSettings Load(string? path)
    {
        if (path is null || !File.Exists(path))
        {
            return new InstallerSettings();
        }
        return JsonSerializer.Deserialize<InstallerSettings>(File.ReadAllText(path), PackageManifest.Json) ?? new InstallerSettings();
    }
}
