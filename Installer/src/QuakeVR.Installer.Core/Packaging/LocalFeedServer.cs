using System.Net;
using System.Net.Sockets;
using System.Text;

namespace QuakeVR.Installer.Core.Packaging;

/// <summary>
/// A tiny HTTP/1.1 file server on 127.0.0.1 for testing a release before it is published (<c>qvr-setup serve</c>,
/// <c>Misc\release\test_local_release.ps1</c>): it serves the files of one folder (latest.json, QuakeVR.zip, ...) to the
/// installer's real download path, GET and HEAD with "Range: bytes=N-" or "bytes=N-M" (206, 416), one request per
/// connection. Only the folder's own files are served (no subfolders, no "..": the last path segment is the name).
/// <see cref="DropAfter"/> cuts the first whole (not ranged) download of each file after that many bytes, so the
/// installer's resume (a ".part" file, a Range request) is exercised. Nothing listens beyond the loopback interface.
/// </summary>
public sealed class LocalFeedServer : IDisposable
{
    readonly TcpListener _listener;
    readonly CancellationTokenSource _cts = new();
    readonly string _root;
    readonly HashSet<string> _dropped = new(StringComparer.OrdinalIgnoreCase);
    readonly Lock _lock = new();
    readonly Action<string>? _log;

    public int Requests;
    public int RangeRequests;
    public int Drops;

    /// <summary>Cut the first whole download of each file after this many bytes of its body (null: never).</summary>
    public long? DropAfter { get; init; }

    public LocalFeedServer(string folder, int port = 0, Action<string>? log = null)
    {
        _root = Path.GetFullPath(folder);
        if (!Directory.Exists(_root))
        {
            throw new InstallException($"no folder {_root} to serve");
        }
        _log = log;
        _listener = new TcpListener(IPAddress.Loopback, port);
        try
        {
            _listener.Start();
        }
        catch (SocketException e)
        {
            throw new InstallException($"can't listen on 127.0.0.1:{port} ({e.Message}): is another server running there?");
        }
        _ = Task.Run(Loop);
    }

    public int Port => ((IPEndPoint)_listener.LocalEndpoint).Port;
    public Uri Url(string file = "") => new($"http://127.0.0.1:{Port}/{file.TrimStart('/')}");

    async Task Loop()
    {
        while (!_cts.IsCancellationRequested)
        {
            TcpClient client;
            try
            {
                client = await _listener.AcceptTcpClientAsync(_cts.Token).ConfigureAwait(false);
            }
            catch (Exception e) when (e is OperationCanceledException or ObjectDisposedException or SocketException)
            {
                return;
            }
            _ = Task.Run(() => Handle(client));
        }
    }

    async Task Handle(TcpClient client)
    {
        using var _ = client;
        try
        {
            await Serve(client.GetStream()).ConfigureAwait(false);
        }
        catch (Exception e) when (e is IOException or SocketException or ObjectDisposedException or OperationCanceledException)
        {
            // (the client went away, or the server stopped)
        }
    }

    async Task Serve(NetworkStream stream)
    {
        var reader = new StreamReader(stream, Encoding.ASCII, leaveOpen: true);
        var requestLine = await reader.ReadLineAsync().ConfigureAwait(false) ?? "";
        string? rangeHeader = null;
        string? line;
        while (!string.IsNullOrEmpty(line = await reader.ReadLineAsync().ConfigureAwait(false)))
        {
            if (line.StartsWith("Range:", StringComparison.OrdinalIgnoreCase))
            {
                rangeHeader = line["Range:".Length..].Trim();
            }
        }
        Interlocked.Increment(ref Requests);
        var parts = requestLine.Split(' ');
        var method = parts.ElementAtOrDefault(0) ?? "";
        var rawPath = (parts.ElementAtOrDefault(1) ?? "/").Split('?')[0];
        var name = Uri.UnescapeDataString(rawPath).Replace('\\', '/').Split('/')[^1];
        var path = name.Length > 0 && name != ".." && name.IndexOfAny(Path.GetInvalidFileNameChars()) < 0 ? Path.Combine(_root, name) : null;

        if (method is not ("GET" or "HEAD"))
        {
            await Head(stream, "405 Method Not Allowed", 0).ConfigureAwait(false);
            _log?.Invoke($"{method} {rawPath}: 405");
            return;
        }
        if (path is null || !File.Exists(path))
        {
            await Head(stream, "404 Not Found", 0).ConfigureAwait(false);
            _log?.Invoke($"{method} {rawPath}: 404");
            return;
        }
        await using var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 1 << 16, true);
        var length = file.Length;
        long from = 0, to = length - 1;
        var status = "200 OK";
        var extra = "";
        if (rangeHeader is { } r && TryParseRange(r, length, out var rf, out var rt))
        {
            Interlocked.Increment(ref RangeRequests);
            if (rf >= length)
            {
                await Head(stream, "416 Range Not Satisfiable", 0, $"Content-Range: bytes */{length}\r\n").ConfigureAwait(false);
                _log?.Invoke($"{method} {rawPath} ({r}): 416");
                return;
            }
            (from, to) = (rf, Math.Min(rt ?? length - 1, length - 1));
            status = "206 Partial Content";
            extra = $"Content-Range: bytes {from}-{to}/{length}\r\n";
        }
        var count = Math.Max(0, to - from + 1);
        var type = name.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? "application/json"
            : name.EndsWith(".txt", StringComparison.OrdinalIgnoreCase) ? "text/plain; charset=utf-8" : "application/octet-stream";
        await Head(stream, status, count, extra + $"Content-Type: {type}\r\nAccept-Ranges: bytes\r\n").ConfigureAwait(false);
        if (method == "HEAD")
        {
            _log?.Invoke($"HEAD {rawPath}: {status}");
            return;
        }
        // The first whole download of each file is cut short when asked (the resume test).
        long? cut = null;
        if (DropAfter is { } d && from == 0 && rangeHeader is null && count > d)
        {
            lock (_lock)
            {
                if (_dropped.Add(name))
                {
                    cut = d;
                }
            }
        }
        file.Position = from;
        var left = cut ?? count;
        var buffer = new byte[1 << 16];
        while (left > 0)
        {
            var n = await file.ReadAsync(buffer.AsMemory(0, (int)Math.Min(buffer.Length, left)), _cts.Token).ConfigureAwait(false);
            if (n <= 0)
            {
                break;
            }
            await stream.WriteAsync(buffer.AsMemory(0, n), _cts.Token).ConfigureAwait(false);
            left -= n;
        }
        await stream.FlushAsync().ConfigureAwait(false);
        if (cut is not null)
        {
            Interlocked.Increment(ref Drops);
            _log?.Invoke($"GET {rawPath}: {status}, CUT after {cut} of {count} bytes (--drop-after: the installer should resume it)");
        }
        else
        {
            _log?.Invoke($"GET {rawPath}{(rangeHeader is null ? "" : $" ({rangeHeader})")}: {status}, {count} bytes");
        }
    }

    static async Task Head(NetworkStream stream, string status, long length, string extra = "")
    {
        var bytes = Encoding.ASCII.GetBytes($"HTTP/1.1 {status}\r\nContent-Length: {length}\r\n{extra}Cache-Control: no-store\r\nConnection: close\r\n\r\n");
        await stream.WriteAsync(bytes).ConfigureAwait(false);
    }

    /// <summary>"bytes=N-" or "bytes=N-M" (one range; suffix ranges "bytes=-N" too).</summary>
    public static bool TryParseRange(string header, long length, out long from, out long? to)
    {
        from = 0;
        to = null;
        if (!header.StartsWith("bytes=", StringComparison.OrdinalIgnoreCase) || header.Contains(','))
        {
            return false;
        }
        var spec = header["bytes=".Length..].Split('-');
        if (spec.Length != 2)
        {
            return false;
        }
        if (spec[0].Length == 0)
        {
            if (!long.TryParse(spec[1], out var suffix) || suffix <= 0)
            {
                return false;
            }
            from = Math.Max(0, length - suffix);
            return true;
        }
        if (!long.TryParse(spec[0], out from) || from < 0)
        {
            return false;
        }
        if (spec[1].Length > 0)
        {
            if (!long.TryParse(spec[1], out var t) || t < from)
            {
                return false;
            }
            to = t;
        }
        return true;
    }

    public void Dispose()
    {
        _cts.Cancel();
        _listener.Stop();
    }
}
