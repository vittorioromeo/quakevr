using System.Net;
using System.Net.Http.Headers;
using System.Security.Cryptography;

namespace QuakeVR.Installer.Core.Packaging;

public sealed record DownloadProgress(long Received, long? Total, string Source);

/// <summary>
/// Downloads one file from a list of mirrors (GitHub first, then vittorioromeo.com: decision 6), resuming a partial
/// download (<c>&lt;file&gt;.part</c>, HTTP Range) and checking the pinned size and SHA-256 before the file gets its
/// final name. A mirror that serves something else is skipped for the next one.
/// </summary>
public sealed class Downloader(HttpClient http)
{
    public static HttpClient CreateClient()
    {
        var client = new HttpClient(new SocketsHttpHandler { AutomaticDecompression = DecompressionMethods.None })
        {
            Timeout = Timeout.InfiniteTimeSpan, // large files: cancellation and per-read progress instead
        };
        client.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("QuakeVR-Installer", "0.1"));
        return client;
    }

    public async Task DownloadAsync(IReadOnlyList<Uri> mirrors, string destination, long? size, string? sha256,
        IProgress<DownloadProgress>? progress, CancellationToken ct, int attemptsPerMirror = 2)
    {
        if (mirrors.Count == 0)
        {
            throw new InstallException("no download address");
        }
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(destination))!);
        if (File.Exists(destination) && Matches(destination, size, sha256))
        {
            return; // already downloaded (or the player's own copy, "I already have this file")
        }
        var part = destination + ".part";
        var errors = new List<string>();
        foreach (var uri in mirrors)
        {
            for (var attempt = 0; attempt < attemptsPerMirror; ++attempt)
            {
                ct.ThrowIfCancellationRequested();
                try
                {
                    await DownloadOnce(uri, part, size, progress, ct).ConfigureAwait(false);
                    if (Matches(part, size, sha256))
                    {
                        File.Move(part, destination, overwrite: true);
                        return;
                    }
                    errors.Add($"{uri.Host}: the file's SHA-256 or size is wrong");
                    File.Delete(part); // a wrong file is never resumed
                    break; // this mirror serves another file: try the next
                }
                catch (HttpRequestException e)
                {
                    errors.Add($"{uri.Host}: {e.Message}");
                }
                catch (IOException e) when (!ct.IsCancellationRequested)
                {
                    errors.Add($"{uri.Host}: {e.Message}");
                }
            }
        }
        throw new InstallException($"Download failed ({string.Join("; ", errors.Distinct())}). Check your connection, or pick the file by hand.");
    }

    async Task DownloadOnce(Uri uri, string part, long? size, IProgress<DownloadProgress>? progress, CancellationToken ct)
    {
        var have = File.Exists(part) ? new FileInfo(part).Length : 0;
        if (size is { } s && have > s)
        {
            File.Delete(part);
            have = 0;
        }
        using var request = new HttpRequestMessage(HttpMethod.Get, uri);
        if (have > 0)
        {
            request.Headers.Range = new RangeHeaderValue(have, null);
        }
        using var response = await http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, ct).ConfigureAwait(false);
        if (response.StatusCode == HttpStatusCode.RequestedRangeNotSatisfiable)
        {
            return; // the part is already whole; checked by the caller
        }
        response.EnsureSuccessStatusCode();
        var resumed = have > 0 && response.StatusCode == HttpStatusCode.PartialContent;
        if (!resumed)
        {
            have = 0;
        }
        var total = size ?? (response.Content.Headers.ContentLength is { } len ? len + have : null);
        await using var input = await response.Content.ReadAsStreamAsync(ct).ConfigureAwait(false);
        await using var output = new FileStream(part, resumed ? FileMode.Append : FileMode.Create, FileAccess.Write, FileShare.None, 1 << 16, true);
        var buffer = new byte[1 << 16];
        int n;
        var received = have;
        var last = DateTime.MinValue;
        while ((n = await input.ReadAsync(buffer, ct).ConfigureAwait(false)) > 0)
        {
            await output.WriteAsync(buffer.AsMemory(0, n), ct).ConfigureAwait(false);
            received += n;
            if (DateTime.UtcNow - last > TimeSpan.FromMilliseconds(100))
            {
                last = DateTime.UtcNow;
                progress?.Report(new DownloadProgress(received, total, uri.Host));
            }
        }
        progress?.Report(new DownloadProgress(received, total, uri.Host));
    }

    static bool Matches(string path, long? size, string? sha256)
    {
        if (size is { } s && new FileInfo(path).Length != s)
        {
            return false;
        }
        if (sha256 is null)
        {
            return true;
        }
        using var fs = File.OpenRead(path);
        return string.Equals(Convert.ToHexStringLower(SHA256.HashData(fs)), sha256, StringComparison.OrdinalIgnoreCase);
    }
}
