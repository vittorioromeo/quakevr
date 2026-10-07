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
    qvr-setup install --package <zip|folder> --target <dir> [--quake <dir>] [--shortcuts-dir <dir>]
                      [--textures <zip>] [--relight] [--vispatch <id1_vis.tgz>...] [--unverified]
                      [--setup-from <QuakeVR-Setup.exe>] [--registry-file <json> | --register]
    qvr-setup uninstall --target <dir> [--remove-textures] [--registry-file <json> | --register]
    qvr-setup verify --target <dir>
    qvr-setup vcredist [--check <vc_redist.x64.exe>] [--dry-run [--file <vc_redist.x64.exe>] [--assume-missing]] [--downloads <dir>]
    qvr-setup download --url <url> [--url <mirror>...] --out <file> [--size <bytes>] [--sha256 <hex>]
    qvr-setup feed --url <latest.json url>
    qvr-setup assets --game <id1 folder> [--map <maps/x.bsp>] [--prefix <path prefix>]
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
        case "install":
        {
            var probe = new WindowsSystemProbe();
            var quake = Opt("quake") ?? DetectionReport.Run(probe).DefaultQuake?.BaseDir
                ?? throw new InstallException("No Quake found: pass --quake <folder with id1>.");
            var target = Opt("target") ?? throw new ArgumentException("--target is required");
            if (InstallEngine.ValidateTarget(target, quake, probe) is { } why)
            {
                throw new InstallException(why);
            }
            var shortcutsDir = Opt("shortcuts-dir");
            var owned = ExpansionDetector.Detect([quake, target], [])
                .Where(e => e.Folder is "hipnotic" or "rogue" && e.State == ExpansionState.Ready).Select(e => e.Folder).ToList();
            var plan = new InstallPlan
            {
                PackagePath = Opt("package") ?? throw new ArgumentException("--package is required"),
                TargetDir = target,
                QuakeDir = quake,
                QuakeStore = "manual",
                AllowUnverified = Flag("unverified"),
                RelightOnFirstRun = Flag("relight"),
                HdTexturesZip = Opt("textures"),
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
            using var http = Downloader.CreateClient();
            var feed = await ReleaseFeed.FetchAsync(http, (options.TryGetValue("url", out var urls) ? urls : []).Select(u => new Uri(u)), CancellationToken.None);
            Console.WriteLine($"version {feed.Version}; package {feed.Package?.File} {PathUtil.FormatSize(feed.Package?.Size ?? 0)}; components: {string.Join(", ", feed.Components.Keys)}");
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
catch (Exception e) when (e is InstallException or ArgumentException or IOException or InvalidDataException or UnauthorizedAccessException)
{
    Console.Error.WriteLine($"error: {e.Message}");
    return 1;
}
