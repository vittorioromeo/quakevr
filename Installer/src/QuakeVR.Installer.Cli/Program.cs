using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Prerequisites;
using QuakeVR.Installer.Core.Shortcuts;

// qvr-setup <command> [options]. Never writes the real desktop or Start menu: shortcuts go only into --shortcuts-dir.
const string Usage = """
    qvr-setup detect [--qvr <dir>] [--epic-manifests <dir>]
    qvr-setup manifest <package folder> --version <text>
    qvr-setup install (--package <zip|folder> | --feed <latest.json url> [--hd] [--downloads <dir>])
                      (--target <dir> [--shortcuts-dir <dir>] | --sandbox <dir>) [--quake <dir>]
                      [--textures <zip>] [--relight] [--vispatch <id1_vis.tgz>...] [--unverified]
                      [--setup-from <QuakeVR-Setup.exe>] [--registry-file <json> | --register] --accept-statement
                      (--feed: the package, and with --hd the HD textures, downloaded as the window does; QVR_SETUP_FEED
                       is the default feed. --sandbox: <dir>\QuakeVR, shortcuts in <dir>\_shortcuts, downloads in <dir>\_downloads)
    qvr-setup statement                              (prints the author's statement on AI usage; install needs --accept-statement)
    qvr-setup uninstall --target <dir> [--remove-textures] [--registry-file <json> | --register]
    qvr-setup verify --target <dir>
    qvr-setup vcredist [--check <vc_redist.x64.exe>] [--dry-run [--file <vc_redist.x64.exe>] [--assume-missing]] [--downloads <dir>]
    qvr-setup download --url <url> [--url <mirror>...] --out <file> [--size <bytes>] [--sha256 <hex>]
    qvr-setup feed [--url <latest.json url>]                  (default: QVR_SETUP_FEED, else the release hosts' feeds)
    qvr-setup feed --file <latest.json> [--assets <folder with its files>]   (exit 1 when a file's size or SHA-256 differs)
    qvr-setup assets --game <id1 folder> [--map <maps/x.bsp>] [--prefix <path prefix>]
    qvr-setup serve --dir <folder> [--port <n>] [--drop-after <bytes>] [--minutes <n>] [--log <file>]
                    (a local release's assets over HTTP on 127.0.0.1, with Range; --drop-after cuts each file's first
                     download there, to test resuming; runs until Ctrl+C or --minutes)
    """;

if (args.Length == 0)
{
    Console.WriteLine(Usage);
    return 2;
}

var options = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
var positional = new List<string>();
for (var i = 1; i < args.Length; ++i)
{
    if (args[i].StartsWith("--", StringComparison.Ordinal))
    {
        var key = args[i][2..];
        var value = i + 1 < args.Length && !args[i + 1].StartsWith("--", StringComparison.Ordinal) ? args[++i] : "";
        if (!options.TryGetValue(key, out var list))
        {
            options[key] = list = [];
        }
        list.Add(value);
    }
    else
    {
        positional.Add(args[i]);
    }
}
string? Opt(string key) => options.TryGetValue(key, out var v) ? v[^1] : null;
bool Flag(string key) => options.ContainsKey(key);
// The Apps & Features entry: a made-up registry root in a JSON file (tests), the real HKCU only with --register.
IRegistryWriter? Registry() => Opt("registry-file") is { } rf ? new JsonFileRegistry(rf) : Flag("register") ? new WindowsRegistryWriter() : null;
// --url/--feed (repeatable), else QVR_SETUP_FEED, else the release hosts' feeds.
List<Uri> Feeds(string key)
{
    var list = options.TryGetValue(key, out var given) ? given.Where(u => u.Length > 0).ToList() : [];
    if (list.Count == 0)
    {
        list = InstallerSettings.FeedsFromEnvironment();
    }
    if (list.Count == 0)
    {
        list = new InstallerSettings().FeedUrls;
    }
    else if (!InstallerSettings.IsReleaseHostFeeds(list))
    {
        Console.WriteLine($"TEST FEED: {string.Join(", ", list)}");
    }
    return [.. list.Select(u => new Uri(u))];
}

var log = new SyncProgress<InstallProgress>(p =>
{
    if (p.Log is not null)
    {
        Console.WriteLine($"{(p.Level switch { LogLevel.Warning => "warning: ", LogLevel.Error => "error: ", _ => "" })}{p.Log}");
    }
});

try
{
    switch (args[0])
    {
        case "detect":
        {
            var report = DetectionReport.Run(new WindowsSystemProbe(), Opt("epic-manifests"));
            Console.Write(report.Format(Opt("qvr")));
            return 0;
        }
        case "manifest":
        {
            var folder = positional.FirstOrDefault() ?? throw new ArgumentException("which folder?");
            var m = PackageManifest.Create(folder, Opt("version") ?? "unknown");
            File.WriteAllText(Path.Combine(folder, PackageManifest.FileName), m.ToJson());
            Console.WriteLine($"{m.Files.Count} files, {PathUtil.FormatSize(m.TotalSize)}: {Path.Combine(folder, PackageManifest.FileName)}");
            return 0;
        }
        case "statement":
        {
            Console.Write(AiStatement.Format());
            return 0;
        }
        case "install":
        {
            // The wizard's Statement page: the console installs only with YES to all four, given as --accept-statement.
            if (!Flag("accept-statement"))
            {
                Console.Write(AiStatement.Format());
                Console.WriteLine();
                Console.WriteLine("To install, pass --accept-statement: it answers YES to all four statements above.");
                return 3;
            }
            var probe = new WindowsSystemProbe();
            var quake = Opt("quake") ?? DetectionReport.Run(probe).DefaultQuake?.BaseDir
                ?? throw new InstallException("No Quake found: pass --quake <folder with id1>.");
            var sandbox = Opt("sandbox") is { } sb ? new Sandbox(sb) : null;
            if (sandbox is not null && Flag("register"))
            {
                throw new ArgumentException("--sandbox never writes the Apps & Features entry: no --register");
            }
            if (sandbox is not null)
            {
                Console.WriteLine($"SANDBOX: {sandbox.Root}");
            }
            var target = Opt("target") ?? sandbox?.Target ?? throw new ArgumentException("--target (or --sandbox) is required");
            if (InstallEngine.ValidateTarget(target, quake, probe) is { } why)
            {
                throw new InstallException(why);
            }
            var shortcutsDir = Opt("shortcuts-dir") ?? sandbox?.ShortcutsDir;
            var package = Opt("package");
            var textures = Opt("textures");
            if (package is null)
            {
                // The window's path: latest.json from the feed, then the files it names (Range resume, size, SHA-256).
                if (!options.ContainsKey("feed") && InstallerSettings.FeedsFromEnvironment().Count == 0)
                {
                    throw new ArgumentException("--package or --feed is required");
                }
                using var http = Downloader.CreateClient();
                var feed = await ReleaseFeed.FetchAsync(http, Feeds("feed"), CancellationToken.None);
                var downloads = Opt("downloads") ?? sandbox?.Downloads
                    ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "QuakeVR-Installer", "downloads");
                async Task<string> Fetch(string what, FeedFile file)
                {
                    var dest = Path.Combine(downloads, Path.GetFileName(file.File));
                    Console.WriteLine($"downloading {what} {feed.Version} ({PathUtil.FormatSize(file.Size)}) from {file.Urls[0]}");
                    await new Downloader(http).DownloadAsync(file.Mirrors, dest, file.Size, file.Sha256,
                        new SyncProgress<DownloadProgress>(p => Console.Write($"\r  {p.Source}: {PathUtil.FormatSize(p.Received)} / {PathUtil.FormatSize(p.Total ?? file.Size)}   ")),
                        CancellationToken.None);
                    Console.WriteLine($"\r  {dest}: downloaded and checked (SHA-256)          ");
                    return dest;
                }
                package = await Fetch("Quake VR", feed.Package ?? throw new InstallException("the feed names no package"));
                if (Flag("hd") && textures is null)
                {
                    textures = feed.Components.GetValueOrDefault("hdtextures") is { } hd ? await Fetch("HD textures", hd) : null;
                    if (textures is null)
                    {
                        Console.WriteLine("warning: the feed has no hdtextures component: no HD textures");
                    }
                }
            }
            var owned = ExpansionDetector.Detect([quake, target], [])
                .Where(e => e.Folder is "hipnotic" or "rogue" && e.State == ExpansionState.Ready).Select(e => e.Folder).ToList();
            var plan = new InstallPlan
            {
                PackagePath = package,
                TargetDir = target,
                QuakeDir = quake,
                QuakeStore = "manual",
                AllowUnverified = Flag("unverified"),
                RelightOnFirstRun = Flag("relight"),
                HdTexturesZip = textures,
                VisPatchArchives = options.TryGetValue("vispatch", out var vis) ? vis : [],
                OwnedPacks = owned,
                SetupFiles = Opt("setup-from") is { } setupExe ? SetupCopy.FilesOf(setupExe, SetupCopy.IsSingleFile(setupExe)) : [],
                Registry = Registry(),
                Shortcuts = shortcutsDir is null
                    ? new ShortcutOptions { Desktop = false, StartMenu = false }
                    : new ShortcutOptions { DesktopDir = Path.Combine(shortcutsDir, "Desktop"), StartMenuDir = Path.Combine(shortcutsDir, "Programs") },
            };
            var record = new InstallEngine().Install(plan, log, CancellationToken.None);
            Console.WriteLine($"installed {record.Files.Count} files, {record.Shortcuts.Count} shortcuts");
            return 0;
        }
        case "uninstall":
        {
            var r = Uninstaller.Uninstall(Opt("target") ?? throw new ArgumentException("--target is required"),
                new UninstallOptions { RemoveHdTextures = Flag("remove-textures"), Registry = Registry() }, log);
            Console.WriteLine($"removed {r.FilesRemoved} files, {r.ShortcutsRemoved} shortcuts; changed files kept: {r.ChangedKept.Count}; " +
                              $"player files left: {r.PlayerFilesLeft.Count}; folder removed: {r.FolderRemoved}; Apps & Features entry removed: {r.EntryRemoved}");
            foreach (var f in r.PlayerFilesLeft.Take(20))
            {
                Console.WriteLine($"  left: {f}");
            }
            return 0;
        }
        case "verify":
        {
            var problems = Uninstaller.Verify(Opt("target") ?? throw new ArgumentException("--target is required"));
            foreach (var (path, problem) in problems)
            {
                Console.WriteLine($"{problem}: {path}");
            }
            Console.WriteLine(problems.Count == 0 ? "all files intact" : $"{problems.Count} problem(s)");
            return problems.Count == 0 ? 0 : 1;
        }
        case "vcredist":
        {
            // The VC++ runtime: what is installed, a redistributable's signature, and what the install would do. Without
            // --dry-run it really installs it when it is missing (one administrator prompt).
            var info = VcRuntimeDetector.Detect(new WindowsSystemProbe());
            Console.WriteLine($"VC++ runtime: {info.Describe()} ok: {info.Ok}");
            using var http = Downloader.CreateClient();
            var redist = VcRedist.ForWindows(http);
            if (Opt("check") is { } check)
            {
                var sig = new AuthenticodeVerifier().Verify(check);
                Console.WriteLine($"{check}: trusted {sig.Trusted}, Microsoft {sig.IsMicrosoft}, signer {sig.Signer ?? "none"} ({sig.Detail}); " +
                                  $"version {VcRedist.FileVersionOf(check)?.ToString() ?? "none"}; {redist.Reject(check) ?? "would be run"}");
                return 0;
            }
            if (Flag("assume-missing"))
            {
                info = new VcRuntimeInfo(null, info.Required);
            }
            var result = await redist.EnsureAsync(info, new VcRedistOptions
            {
                DryRun = Flag("dry-run"),
                LocalCopies = Opt("file") is { } file ? [file] : [],
                DownloadDir = Opt("downloads") ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "QuakeVR-Installer", "downloads"),
            }, log, CancellationToken.None);
            Console.WriteLine($"{result.Outcome}: {result.Message}");
            return result.RuntimeReady || result.Outcome == VcRedistOutcome.DryRun ? 0 : 1;
        }
        case "download":
        {
            using var http = Downloader.CreateClient();
            var mirrors = (options.TryGetValue("url", out var urls) ? urls : []).Select(u => new Uri(u)).ToList();
            await new Downloader(http).DownloadAsync(mirrors, Opt("out") ?? throw new ArgumentException("--out is required"),
                Opt("size") is { } s ? long.Parse(s) : null, Opt("sha256"),
                new SyncProgress<DownloadProgress>(p => Console.Write($"\r{p.Source}: {PathUtil.FormatSize(p.Received)}{(p.Total is { } t ? " / " + PathUtil.FormatSize(t) : "")}   ")),
                CancellationToken.None);
            Console.WriteLine("\ndownloaded and checked");
            return 0;
        }
        case "feed":
        {
            ReleaseFeed feed;
            if (Opt("file") is { } feedFile)
            {
                // A latest.json on disk (Misc/release/make_release.ps1 checks the one it made), parsed as a download is.
                feed = ReleaseFeed.Parse(File.ReadAllText(feedFile));
            }
            else
            {
                using var http = Downloader.CreateClient();
                feed = await ReleaseFeed.FetchAsync(http, Feeds("url"), CancellationToken.None);
            }
            Console.WriteLine($"version {feed.Version}; package {feed.Package?.File} {PathUtil.FormatSize(feed.Package?.Size ?? 0)}; components: {string.Join(", ", feed.Components.Keys)}");
            if (Opt("assets") is { } assetsDir)
            {
                // Each file the feed names, beside it: the size and SHA-256 the installer will check after downloading.
                var bad = 0;
                foreach (var (what, f) in feed.Components.Select(c => (c.Key, (FeedFile?)c.Value)).Prepend(("package", feed.Package)))
                {
                    if (f is null)
                    {
                        Console.WriteLine($"{what}: missing from the feed");
                        ++bad;
                        continue;
                    }
                    var path = Path.Combine(assetsDir, f.File);
                    var ok = File.Exists(path) && new FileInfo(path).Length == f.Size && string.Equals(PackageManifest.HashFile(path), f.Sha256, StringComparison.OrdinalIgnoreCase);
                    Console.WriteLine($"{what}: {f.File} {(ok ? "matches" : "DOES NOT MATCH")} ({f.Urls.Count} url(s), first {f.Urls[0]})");
                    bad += ok ? 0 : 1;
                }
                return bad == 0 ? 0 : 1;
            }
            return 0;
        }
        case "assets":
        {
            // What the installer's skin can read from a Quake (nothing is written): pictures, a map's textures, files.
            using var fs = QuakeFileSystem.OpenDir(Opt("game") ?? "") ?? throw new InstallException("no pak0.pak there");
            Console.WriteLine($"{fs.PakCount} paks; palette: {fs.Contains("gfx/palette.lmp")}");
            if (fs.Read("gfx.wad") is { } wad)
            {
                Console.WriteLine("gfx.wad: " + string.Join(" ", QuakeFormats.ReadWad(wad).Values.Select(i => $"{i.Name}({i.Width}x{i.Height})")));
            }
            if (Opt("map") is { } map && fs.Read(map) is { } bsp)
            {
                Console.WriteLine($"{map}: " + string.Join(" ", QuakeFormats.ReadBspTextures(bsp).Select(i => $"{i.Name}({i.Width}x{i.Height})")));
            }
            if (Opt("prefix") is { } prefix)
            {
                Console.WriteLine(string.Join(" ", fs.Names.Where(n => n.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)).Order()));
            }
            return 0;
        }
        case "serve":
        {
            // A local release's assets (make_release.ps1 -Local) for the installer's real download path; 127.0.0.1 only.
            var dir = Opt("dir") ?? positional.FirstOrDefault() ?? throw new ArgumentException("--dir <folder> is required");
            var logFile = Opt("log");
            var logLock = new Lock();
            void Log(string line)
            {
                line = $"{DateTime.Now:HH:mm:ss} {line}";
                Console.WriteLine(line);
                if (logFile is not null)
                {
                    lock (logLock)
                    {
                        File.AppendAllText(logFile, line + Environment.NewLine);
                    }
                }
            }
            using var server = new LocalFeedServer(dir, Opt("port") is { } port ? int.Parse(port) : 0, Log)
            {
                DropAfter = Opt("drop-after") is { } drop ? long.Parse(drop) : null,
            };
            Log($"serving {Path.GetFullPath(dir)} on {server.Url()} (feed: {server.Url("latest.json")}); Ctrl+C stops");
            using var stop = new CancellationTokenSource();
            Console.CancelKeyPress += (_, e) =>
            {
                e.Cancel = true;
                stop.Cancel();
            };
            try
            {
                await Task.Delay(Opt("minutes") is { } m ? TimeSpan.FromMinutes(double.Parse(m, System.Globalization.CultureInfo.InvariantCulture)) : Timeout.InfiniteTimeSpan, stop.Token);
            }
            catch (OperationCanceledException)
            {
            }
            Log($"stopped: {server.Requests} requests, {server.RangeRequests} with Range, {server.Drops} cut");
            return 0;
        }
        case "shortcut-args":
        {
            Console.WriteLine(LaunchCommand.Arguments(Opt("quake") ?? "", Opt("qvr") ?? "", LaunchVariant.Vr));
            return 0;
        }
        default:
            Console.WriteLine(Usage);
            return 2;
    }
}
catch (Exception e) when (e is InstallException or ArgumentException or IOException or InvalidDataException or UnauthorizedAccessException or HttpRequestException or FormatException)
{
    Console.Error.WriteLine($"error: {e.Message}");
    return 1;
}
