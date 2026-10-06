using System.Net;
using System.Net.Sockets;
using System.Text;

namespace QuakeVR.Installer.SelfTest;

/// <summary>A tiny HTTP/1.1 server on 127.0.0.1 for the download tests (no URL reservation needed, unlike
/// HttpListener): GET with optional "Range: bytes=N-", one request per connection. Nothing leaves the machine.</summary>
sealed class LocalHttpServer : IDisposable
{
    readonly TcpListener _listener = new(IPAddress.Loopback, 0);
    readonly CancellationTokenSource _cts = new();
    readonly Dictionary<string, (byte[] Body, bool Ranges)> _routes = [];
    readonly Lock _lock = new();

    public int Requests;
    public int RangeRequests;

    public LocalHttpServer()
    {
        _listener.Start();
        _ = Task.Run(Loop);
    }

    public Uri Url(string path) => new($"http://127.0.0.1:{((IPEndPoint)_listener.LocalEndpoint).Port}/{path.TrimStart('/')}");

    public void Serve(string path, byte[] body, bool ranges = true)
    {
        lock (_lock)
        {
            _routes["/" + path.TrimStart('/')] = (body, ranges);
        }
    }

    public void Serve(string path, string body) => Serve(path, Encoding.UTF8.GetBytes(body));

    async Task Loop()
    {
        while (!_cts.IsCancellationRequested)
        {
            TcpClient client;
            try
            {
                client = await _listener.AcceptTcpClientAsync(_cts.Token);
            }
            catch (OperationCanceledException)
            {
                return;
            }
            _ = Task.Run(() => Handle(client));
        }
    }

    async Task Handle(TcpClient client)
    {
        using var _ = client;
        var stream = client.GetStream();
        var reader = new StreamReader(stream, Encoding.ASCII, leaveOpen: true);
        var requestLine = await reader.ReadLineAsync() ?? "";
        long? from = null;
        string? line;
        while (!string.IsNullOrEmpty(line = await reader.ReadLineAsync()))
        {
            if (line.StartsWith("Range: bytes=", StringComparison.OrdinalIgnoreCase))
            {
                from = long.Parse(line["Range: bytes=".Length..].TrimEnd('-').Split('-')[0]);
            }
        }
        Interlocked.Increment(ref Requests);
        var path = requestLine.Split(' ').ElementAtOrDefault(1) ?? "/";
        (byte[] Body, bool Ranges) route;
        bool found;
        lock (_lock)
        {
            found = _routes.TryGetValue(path, out route);
        }
        string head;
        byte[] body = [];
        if (!found)
        {
            head = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n";
        }
        else if (from is { } f && route.Ranges)
        {
            Interlocked.Increment(ref RangeRequests);
            if (f >= route.Body.Length)
            {
                head = $"HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */{route.Body.Length}\r\nContent-Length: 0\r\n";
            }
            else
            {
                body = route.Body[(int)f..];
                head = $"HTTP/1.1 206 Partial Content\r\nContent-Range: bytes {f}-{route.Body.Length - 1}/{route.Body.Length}\r\nContent-Length: {body.Length}\r\n";
            }
        }
        else
        {
            body = route.Body;
            head = $"HTTP/1.1 200 OK\r\nContent-Length: {body.Length}\r\n";
        }
        var bytes = Encoding.ASCII.GetBytes(head + "Connection: close\r\n\r\n");
        await stream.WriteAsync(bytes);
        await stream.WriteAsync(body);
        await stream.FlushAsync();
    }

    public void Dispose()
    {
        _cts.Cancel();
        _listener.Stop();
    }
}
