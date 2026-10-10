using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Prerequisites;
using QuakeVR.Installer.Core.Shortcuts;

// qvr-setup <command> [options]. Never writes the real desktop or Start menu: shortcuts go only into --shortcuts-dir.
const string Usage = """
    qvr-setup detect [--qvr <dir>] [--epic-manifests <dir>] [--registry-file <json> | --sandbox <dir>]
                      (also the Quake VR: Unleashed install: --qvr, the Apps & Features entry (read only), the default folder)
    qvr-setup manifest <package folder> --version <text>
    qvr-setup install (--package <zip|folder> | --feed <latest.json url>) [--downloads <dir>]
                      (--target <dir> [--shortcuts-dir <dir>] | --sandbox <dir>) [--quake <dir>]
                      [--textures <zip>] [--relight] [--vispatch <id1_vis.tgz>...] [--unverified]
                      [--setup-from <QuakeVR-Setup.exe>] [--registry-file <json> | --register] --accept-statement
                      [--hd] [--no-feed] [--dry-run]
                      (--feed: the package downloaded as the window does; QVR_SETUP_FEED is the default feed. --hd: the
                       HD textures, the feed's hdtextures, else the installer's built-in pack (also with --package: the feed
                       is asked, and not needed; --no-feed never asks it). --dry-run: prints what would be downloaded and
                       from where, then stops. --sandbox: <dir>\QuakeVR, shortcuts in <dir>\_shortcuts, downloads in <dir>\_downloads)
    qvr-setup update [--target <dir> | --sandbox <dir>] [--package <zip|folder> | --feed <url>] [--no-feed] [--dry-run]
                      [--downloads <dir>] [--quake <dir>] [--shortcuts-dir <dir>] [--setup-from <QuakeVR-Setup.exe>]
                      [--registry-file <json> | --register]
                      (the install found as detect prints it; a newer package (or another build) updates, the same or an
                       older one repairs (never a downgrade). Only program files that differ are copied; your files are
                       kept; files you changed are backed up first; HD textures only when the pack changed; the relight
                       only when its inputs changed. --dry-run prints the plan (from --feed: the file plan needs the
                       package, unless it is in --downloads already). Shortcuts are kept unless --shortcuts-dir)
    qvr-setup reinstall [--target <dir> | --sandbox <dir>] [--reset-settings] [--remove-saves] [--dry-run]
                      (install's options) --accept-statement
                      (install again from scratch: the chosen files (settings: configs, retro overrides, body calibration;
                       saves and the Map Library's installed maps) are moved into <install>\backups\<date> reinstall,
                       checked, then the full install runs; --dry-run lists them)
    qvr-setup statement                              (prints the author's statement on AI usage; install needs --accept-statement)
    qvr-setup uninstall --target <dir> [--remove-textures] [--registry-file <json> | --register]
    qvr-setup verify --target <dir>
    qvr-setup vcredist [--check <vc_redist.x64.exe>] [--dry-run [--file <vc_redist.x64.exe>] [--assume-missing]] [--downloads <dir>]
    qvr-setup download --url <url> [--url <mirror>...] --out <file> [--size <bytes>] [--sha256 <hex>]
    qvr-setup feed [--url <latest.json url>]                  (default: QVR_SETUP_FEED, else the release hosts' feeds)
    qvr-setup feed --file <latest.json> [--assets <folder with its files>] [--hosted <component>]...
                      (exit 1 when a file's size or SHA-256 differs; a --hosted component is hosted elsewhere, e.g.
                       hdtextures on the support-files release: not looked for in the folder)
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
        Console.WriteLine($"feeds (Setup {InstallerBuild.Version}): {string.Join(" then ", list)}");
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

// The default install folder (the window's), and the existing install: the folder given, the Apps & Features entry (read
// only: the test root of --registry-file, none for a sandbox, else the real HKCU), the default folder.
InstallDetection FindInstall(Sandbox? sandbox, string? chosen) => InstallDetection.Find(
    Opt("registry-file") is { } rf ? new JsonFileRegistry(rf) : sandbox is null ? new WindowsRegistryWriter() : null,
    chosen ?? sandbox?.Target, Environment.ProcessPath,
    sandbox is null ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Programs", "QuakeVR") : null);

string DownloadsDir(Sandbox? sandbox) => Opt("downloads") ?? sandbox?.Downloads
    ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "QuakeVR-Installer", "downloads");

// The window's download path: the file the feed (or the built-in HD textures) names, with Range resume, size, SHA-256.
async Task<string> Fetch(HttpClient http, string downloads, string what, FeedFile file)
{
    var dest = Path.Combine(downloads, Path.GetFileName(file.File));
    Console.WriteLine($"downloading {what} ({PathUtil.FormatSize(file.Size)}) from {file.Urls[0]}");
    await new Downloader(http).DownloadAsync(file.Mirrors, dest, file.Size, file.Sha256,
        new SyncProgress<DownloadProgress>(p => Console.Write($"\r  {p.Source}: {PathUtil.FormatSize(p.Received)} / {PathUtil.FormatSize(p.Total ?? file.Size)}   ")),
        CancellationToken.None);
    Console.WriteLine($"\r  {dest}: downloaded and checked (SHA-256)          ");
    return dest;
}

// A file the feed names, already downloaded and whole (a dry run plans from it instead of downloading), or null.
string? AlreadyDownloaded(string downloads, FeedFile file)
{
    var path = Path.Combine(downloads, Path.GetFileName(file.File));
    return File.Exists(path) && new FileInfo(path).Length == file.Size &&
           string.Equals(PackageManifest.HashFile(path), file.Sha256, StringComparison.OrdinalIgnoreCase) ? path : null;
}

ShortcutOptions ShortcutsFor(string? shortcutsDir, InstallChoices? choices) => shortcutsDir is null
    ? new ShortcutOptions { Desktop = false, StartMenu = false }
    : new ShortcutOptions
    {
        Desktop = choices?.DesktopShortcut ?? true, StartMenu = choices?.StartMenuShortcuts ?? true,
        Flat = choices?.FlatShortcut ?? true, Log = choices?.LogShortcut ?? true,
        DesktopDir = Path.Combine(shortcutsDir, "Desktop"), StartMenuDir = Path.Combine(shortcutsDir, "Programs"),
    };

// install, and reinstall's second half (reinstall: the found install, and the files to set aside first).
async Task<int> InstallCommand(FoundInstall? reinstallOf, ReinstallOptions? reinstall)
{
    // What would be downloaded, and from where: the package (feed or --package) and the HD textures (the feed's
    // hdtextures, else the installer's built-in pack). The feed is read only when something needs it; for the
    // HD textures alone it is optional (--no-feed: never asked).
    var package = Opt("package");
    var textures = Opt("textures");
    var dryRun = Flag("dry-run");
    if (package is null && (Flag("no-feed") || (!options.ContainsKey("feed") && InstallerSettings.FeedsFromEnvironment().Count == 0)))
    {
        throw new ArgumentException("--package or --feed is required");
    }
    using var http = Downloader.CreateClient();
    ReleaseFeed? feed = null;
    var wantHd = Flag("hd") && textures is null;
    if ((package is null || wantHd) && !Flag("no-feed"))
    {
        try
        {
            feed = await ReleaseFeed.FetchAsync(http, Feeds("feed"), CancellationToken.None);
        }
        catch (InstallException e) when (package is not null)
        {
            Console.WriteLine($"no release list: {e.Message}");
        }
    }
    var hd = wantHd ? new InstallerSettings().Component(feed, Components.HdTextures) : null;
    if (wantHd && hd is null)
    {
        Console.WriteLine("warning: no hdtextures component, in the feed or built in: no HD textures");
    }
    var packageFile = package is null ? feed?.Package ?? throw new InstallException("the feed names no package") : null;
    if (dryRun)
    {
        Console.WriteLine(packageFile is null ? $"package: {package} (local)"
            : $"package: {packageFile.File} {feed!.Version} ({PathUtil.FormatSize(packageFile.Size)}, sha256 {packageFile.Sha256}) from {string.Join(" then ", packageFile.Urls)}");
        Console.WriteLine(textures is not null ? $"hdtextures: {textures} (local)"
            : hd is null ? "hdtextures: none"
            : $"hdtextures: {hd.File.File} {hd.SourceText} ({PathUtil.FormatSize(hd.File.Size)}, {hd.File.Size} bytes, sha256 {hd.File.Sha256}) from {string.Join(" then ", hd.File.Urls)}");
        Console.WriteLine("dry run: nothing downloaded or installed");
        return 0;
    }
    // The wizard's Statement page: the console installs only with YES to all four, given as --accept-statement.
    if (!Flag("accept-statement"))
    {
        Console.Write(AiStatement.Format());
        Console.WriteLine();
        Console.WriteLine("To install, pass --accept-statement: it answers YES to all four statements above.");
        return 3;
    }
    var probe = new WindowsSystemProbe();
    var quake = Opt("quake") ?? (reinstallOf is { } ri && Directory.Exists(Path.Combine(ri.Record.QuakeDir, "id1")) ? ri.Record.QuakeDir : null)
        ?? DetectionReport.Run(probe).DefaultQuake?.BaseDir
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
    var target = reinstallOf?.Dir ?? Opt("target") ?? sandbox?.Target ?? throw new ArgumentException("--target (or --sandbox) is required");
    if (InstallEngine.ValidateTarget(target, quake, probe) is { } why)
    {
        throw new InstallException(why);
    }
    var shortcutsDir = Opt("shortcuts-dir") ?? sandbox?.ShortcutsDir;
    var downloads = DownloadsDir(sandbox);
    if (packageFile is not null)
    {
        package = await Fetch(http, downloads, $"Quake VR {feed!.Version}", packageFile);
    }
    if (hd is not null)
    {
        textures = await Fetch(http, downloads, $"HD textures, {hd.SourceText},", hd.File);
    }
    var owned = ExpansionDetector.Detect([quake, target], [])
        .Where(e => e.Folder is "hipnotic" or "rogue" && e.State == ExpansionState.Ready).Select(e => e.Folder).ToList();
    // Install again from scratch: the chosen files of the player's go into a checked backup first, then the full install.
    var backup = reinstall is null ? null : Reinstaller.Prepare(target, reinstall, DateTimeOffset.Now, log);
    var plan = new InstallPlan
    {
        PackagePath = package!,
        TargetDir = target,
        QuakeDir = quake,
        QuakeStore = reinstallOf?.Record.QuakeStore ?? "manual",
        AllowUnverified = Flag("unverified"),
        RelightOnFirstRun = Flag("relight"),
        HdTexturesZip = textures,
        HdTexturesSha256 = hd?.File.Sha256,
        VisPatchArchives = options.TryGetValue("vispatch", out var vis) ? vis : [],
        OwnedPacks = owned,
        SetupFiles = Opt("setup-from") is { } setupExe ? SetupCopy.FilesOf(setupExe, SetupCopy.IsSingleFile(setupExe)) : [],
        Registry = Registry(),
        Shortcuts = ShortcutsFor(shortcutsDir, null),
        Backup = backup,
    };
    var engine = new InstallEngine();
    var record = engine.Install(plan, log, CancellationToken.None);
    Console.WriteLine($"installed {record.Files.Count} files, {record.Shortcuts.Count} shortcuts{(engine.LastBackup is { } b ? $"; backup: {b.Dir} ({b.Files.Count} files)" : "")}");
    return 0;
}

// qvr-setup reinstall: the found install, the files the choice sets aside (dry run: listed), then InstallCommand.
async Task<int> ReinstallCommand()
{
    var sandbox = Opt("sandbox") is { } sb ? new Sandbox(sb) : null;
    var found = FindInstall(sandbox, Opt("target"));
    Console.Write(found.Format());
    if (found.Picked is not { } install)
    {
        Console.WriteLine("nothing to install again: use qvr-setup install");
        return 1;
    }
    var choice = new ReinstallOptions { ResetSettings = Flag("reset-settings"), RemoveSaves = Flag("remove-saves") };
    Console.Write(Reinstaller.Format(Reinstaller.Plan(install.Dir, choice), choice));
    return await InstallCommand(install, choice);
}

// qvr-setup update: the found install against the package (--package, else the feed): newer (or another build) updates,
// the same or older repairs. The HD textures are downloaded only when the pack changed (a repair: or a file is damaged).
async Task<int> UpdateCommand()
{
    var sandbox = Opt("sandbox") is { } sb ? new Sandbox(sb) : null;
    if (sandbox is not null && Flag("register"))
    {
        throw new ArgumentException("--sandbox never writes the Apps & Features entry: no --register");
    }
    var found = FindInstall(sandbox, Opt("target"));
    Console.Write(found.Format());
    if (found.Picked is not { } install)
    {
        Console.WriteLine("no install to update: use qvr-setup install");
        return 1;
    }
    var dryRun = Flag("dry-run");
    var package = Opt("package");
    var downloads = DownloadsDir(sandbox);
    using var http = Downloader.CreateClient();
    ReleaseFeed? feed = null;
    var hasHd = install.Record.Files.Any(f => f.Component == Components.HdTextures);
    if (!Flag("no-feed") && (package is null || hasHd))
    {
        try
        {
            feed = await ReleaseFeed.FetchAsync(http, Feeds("feed"), CancellationToken.None);
        }
        catch (InstallException e) when (package is not null)
        {
            Console.WriteLine($"no release list: {e.Message}");
        }
    }
    string packageVersion;
    if (package is not null)
    {
        packageVersion = LocalPackages.Inspect(package).Manifest?.Version ?? throw new InstallException($"{package} is not a package (no manifest.json)");
    }
    else
    {
        var file = feed?.Package ?? throw new InstallException("--package or --feed is required (the feed names no package)");
        packageVersion = feed.Version;
        Console.WriteLine($"package: {file.File} {feed.Version} ({PathUtil.FormatSize(file.Size)}, sha256 {file.Sha256}) from {string.Join(" then ", file.Urls)}");
        package = AlreadyDownloaded(downloads, file);
        if (package is null && !dryRun)
        {
            package = await Fetch(http, downloads, $"Quake VR {feed.Version}", file);
        }
    }
    var order = ReleaseVersion.Compare(install.Record.Version, packageVersion);
    var mode = MaintenancePlanner.ModeFor(install.Record.Version, packageVersion);
    Console.WriteLine($"installed {install.Record.Version}, package {packageVersion}: {order.ToString().ToLowerInvariant()} -> {mode.ToString().ToLowerInvariant()}");
    var hdTarget = new InstallerSettings().Component(feed, Components.HdTextures);
    var (hdAction, hdText) = MaintenancePlanner.HdTextures(install.Dir, install.Record, hdTarget?.File, verify: mode == InstallMode.Repair);
    Console.WriteLine(hdText);
    var getHd = hdAction is HdTexturesAction.Replace or HdTexturesAction.Restore && hdTarget is not null;
    if (package is null)
    {
        // A dry run from the feed: the package is not downloaded, so the file plan cannot be made.
        Console.WriteLine("program files: planned from the package once it is downloaded (pass --package for the file plan)");
        Console.WriteLine("dry run: nothing downloaded or changed");
        return 0;
    }
    using (var source = PackageSource.FromPath(package))
    {
        var manifest = source.ReadManifest() ?? throw new InstallException($"{package} has no manifest.json");
        var plan = MaintenancePlanner.Plan(install.Dir, install.Record, manifest, mode);
        Console.Write(plan.Format());
        var relight = FirstStartRelight.Pending(install.Dir) ||
                      (mode == InstallMode.Update && install.Record.Choices.RelightOnFirstRun && (plan.RelightInputsChanged.Count > 0 || getHd));
        Console.WriteLine($"first-start relight: {(relight ? "yes" : "no")}{(install.Record.Choices.RelightOnFirstRun ? "" : " (not chosen at install)")}");
        var hasSetup = Opt("setup-from") is not null || install.Record.Files.Any(f => f.Component == Components.Setup);
        Console.WriteLine($"Apps & Features entry: {(Registry() is null ? "not written (pass --register or --registry-file)" : !hasSetup ? "not written (no copy of Setup in the install: pass --setup-from)" : $"version -> {(plan.KeepsInstalledVersion ? install.Record.Version : packageVersion)}")}");
    }
    if (dryRun)
    {
        if (getHd)
        {
            Console.WriteLine($"hdtextures: {hdTarget!.File.File} {hdTarget.SourceText} would be downloaded from {string.Join(" then ", hdTarget.File.Urls)}");
        }
        Console.WriteLine("dry run: nothing downloaded or changed");
        return 0;
    }
    var quake = Opt("quake") ?? install.Record.QuakeDir;
    if (!File.Exists(Path.Combine(quake, "id1", "pak0.pak")))
    {
        throw new InstallException($"The Quake folder this install uses ({quake}) has no id1\\pak0.pak any more: pass --quake <folder with id1>.");
    }
    var textures = getHd ? await Fetch(http, downloads, $"HD textures, {hdTarget!.SourceText},", hdTarget.File) : null;
    var owned = ExpansionDetector.Detect([quake, install.Dir], [])
        .Where(e => e.Folder is "hipnotic" or "rogue" && e.State == ExpansionState.Ready).Select(e => e.Folder).ToList();
    var engine = new InstallEngine();
    var record = engine.Install(new InstallPlan
    {
        Mode = mode,
        PackagePath = package,
        TargetDir = install.Dir,
        QuakeDir = quake,
        QuakeStore = install.Record.QuakeStore,
        RelightOnFirstRun = install.Record.Choices.RelightOnFirstRun,
        HdTexturesZip = textures,
        HdTexturesSha256 = textures is null ? null : hdTarget!.File.Sha256,
        OwnedPacks = owned,
        SetupFiles = Opt("setup-from") is { } setupExe ? SetupCopy.FilesOf(setupExe, SetupCopy.IsSingleFile(setupExe)) : [],
        Registry = Registry(),
        Shortcuts = ShortcutsFor(Opt("shortcuts-dir") ?? sandbox?.ShortcutsDir, install.Record.Choices),
    }, log, CancellationToken.None);
    Console.WriteLine($"{mode.ToString().ToLowerInvariant()}: {record.Version}, {engine.LastPlan?.ToCopy.Count() ?? 0} files copied, {record.Files.Count} recorded" +
                      $"{(engine.LastBackup is { } b ? $"; backup: {b.Dir} ({b.Files.Count} files)" : "")}");
    return 0;
}

try
{
    switch (args[0])
    {
        case "detect":
        {
            var report = DetectionReport.Run(new WindowsSystemProbe(), Opt("epic-manifests"));
            Console.Write(report.Format(Opt("qvr")));
            // The existing Quake VR: Unleashed install (what the window's Update screen and qvr-setup update use).
            Console.Write(FindInstall(Opt("sandbox") is { } dsb ? new Sandbox(dsb) : null, Opt("qvr") ?? Opt("target")).Format());
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
            return await InstallCommand(null, null);
        case "reinstall":
            return await ReinstallCommand();
        case "update":
            return await UpdateCommand();
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
                var hosted = options.TryGetValue("hosted", out var h) ? h : [];
                foreach (var (what, f) in feed.Components.Select(c => (c.Key, (FeedFile?)c.Value)).Prepend(("package", feed.Package)))
                {
                    if (f is null)
                    {
                        Console.WriteLine($"{what}: missing from the feed");
                        ++bad;
                        continue;
                    }
                    if (hosted.Contains(what, StringComparer.OrdinalIgnoreCase))
                    {
                        Console.WriteLine($"{what}: {f.File} hosted, not in the folder ({PathUtil.FormatSize(f.Size)}, sha256 {f.Sha256[..16]}...; {f.Urls.Count} url(s), first {f.Urls[0]})");
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
