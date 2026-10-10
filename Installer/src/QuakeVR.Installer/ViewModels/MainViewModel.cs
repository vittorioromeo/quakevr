using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Windows;
using System.Windows.Input;
using QuakeVR.Installer.Audio;
using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Prerequisites;
using QuakeVR.Installer.Core.Shortcuts;

namespace QuakeVR.Installer.ViewModels;

public enum Page
{
    Welcome,
    /// <summary>The author's statement on AI usage: four YES/NO answers, all YES to go on.</summary>
    Statement,
    Detect,
    Options,
    Install,
    Done,
    /// <summary>After Play: a word from the author, and Ko-fi.</summary>
    Support,
}

/// <summary>Whether the online release can be downloaded (only asked when no local package is used).</summary>
public enum FeedState
{
    Unknown,
    Checking,
    Available,
    Unavailable,
}

/// <summary>Command-line choices (also what the screenshot harness and manual tests use).</summary>
public sealed class StartupOptions
{
    public string? Package { get; set; }
    public string? Textures { get; set; }
    public string? Target { get; set; }
    /// <summary>Shortcuts go into &lt;dir&gt;\Desktop and &lt;dir&gt;\Programs instead of the real ones (tests).</summary>
    public string? ShortcutsDir { get; set; }
    public List<string> Feeds { get; } = [];
    public string? Screenshots { get; set; }
    /// <summary>Where downloads go (default %LOCALAPPDATA%\QuakeVR-Installer\downloads; tests use a scratch folder).</summary>
    public string? Downloads { get; set; }
    /// <summary>Never ask the network: the online release counts as unavailable (tests, screenshots).</summary>
    public bool Offline { get; set; }
    /// <summary>The generated look even when Quake is found (screenshots of the fallback).</summary>
    public bool NoQuakeLook { get; set; }
    /// <summary>Animations off, as with Windows' "Animation effects" off.</summary>
    public bool ReduceMotion { get; set; }
    /// <summary>No sound at all (screenshots).</summary>
    public bool Silent { get; set; }
    /// <summary>The harness also writes a strip of flame frames and a sheet of Quake's textures.</summary>
    public bool Extras { get; set; }
    /// <summary>Never install the VC++ runtime (it is only detected).</summary>
    public bool NoPrerequisites { get; set; }
    /// <summary>Say what the VC++ runtime's install would do; download and run nothing (tests; the harness always).</summary>
    public bool VcRedistDryRun { get; set; }
    /// <summary>Remove the install in --target (Apps &amp; Features' Uninstall): the Remove dialogs, or none with --quiet.</summary>
    public bool Uninstall { get; set; }
    public bool Quiet { get; set; }
    /// <summary>This is the copy an uninstall started from %TEMP% (so the install's own copy can be removed).</summary>
    public bool FromTemp { get; set; }
    /// <summary>The Apps &amp; Features entry goes into this made-up registry root (a JSON file) instead of HKCU (tests).</summary>
    public string? RegistryFile { get; set; }
    /// <summary>--sandbox &lt;dir&gt;: a test install kept in that folder (<see cref="Core.Packaging.Sandbox"/>): the
    /// install, shortcuts, downloads and the installer's settings go there, no Apps &amp; Features entry, the VC++ runtime
    /// only checked.</summary>
    public Sandbox? Sandbox { get; set; }

    /// <summary>The arguments an uninstall passes on to its copy in %TEMP%.</summary>
    public List<string> UninstallArguments(string target)
    {
        var args = new List<string> { "--uninstall", "--target", target, "--from-temp" };
        if (Quiet)
        {
            args.Add("--quiet");
        }
        if (RegistryFile is { } f)
        {
            args.AddRange(["--registry-file", f]);
        }
        return args;
    }

    public static StartupOptions Parse(string[] args)
    {
        var o = new StartupOptions();
        for (var i = 0; i < args.Length; ++i)
        {
            string? Next() => i + 1 < args.Length ? args[++i] : null;
            switch (args[i].ToLowerInvariant())
            {
                case "--package": o.Package = Next(); break;
                case "--textures": o.Textures = Next(); break;
                case "--target": o.Target = Next(); break;
                case "--shortcuts-dir": o.ShortcutsDir = Next(); break;
                case "--feed": if (Next() is { } f) { o.Feeds.Add(f); } break;
                case "--screenshots": o.Screenshots = Next(); break;
                case "--downloads": o.Downloads = Next(); break;
                case "--offline": o.Offline = true; break;
                case "--no-quake-look": o.NoQuakeLook = true; break;
                case "--reduce-motion": o.ReduceMotion = true; break;
                case "--silent": o.Silent = true; break;
                case "--extras": o.Extras = true; break;
                case "--no-prerequisites": o.NoPrerequisites = true; break;
                case "--vcredist-dry-run": o.VcRedistDryRun = true; break;
                case "--uninstall": o.Uninstall = true; break;
                case "--quiet": o.Quiet = true; break;
                case "--from-temp": o.FromTemp = true; break;
                case "--registry-file": o.RegistryFile = Next(); break;
                case "--sandbox": if (Next() is { } sb) { o.Sandbox = new Sandbox(sb); } break;
            }
        }
        // QVR_SETUP_FEED, like --feed (a local test release's server); --feed wins.
        if (o.Feeds.Count == 0)
        {
            o.Feeds.AddRange(InstallerSettings.FeedsFromEnvironment());
        }
        if (o.Sandbox is { } sandbox)
        {
            o.Target ??= sandbox.Target;
            o.ShortcutsDir ??= sandbox.ShortcutsDir;
            o.Downloads ??= sandbox.Downloads;
            o.VcRedistDryRun = true;
        }
        o.Package = o.Package is { } pk ? PathUtil.TryNormalize(pk) ?? pk : null;
        o.Textures = o.Textures is { } tx ? PathUtil.TryNormalize(tx) ?? tx : null;
        o.ShortcutsDir = o.ShortcutsDir is { } sc ? PathUtil.TryNormalize(sc) ?? sc : null;
        o.Target = o.Target is { } tg ? PathUtil.TryNormalize(tg) ?? tg : null;
        o.RegistryFile = o.RegistryFile is { } rg ? PathUtil.TryNormalize(rg) ?? rg : null; // (passed on to a copy in %TEMP%, which runs elsewhere)
        // A package beside the installer (an offline download: QuakeVR.zip, the unzipped QuakeVR folder, or another
        // QuakeVR*.zip with a manifest inside): installed from there without any network.
        var here = AppContext.BaseDirectory;
        o.Package ??= LocalPackages.FindBeside(here);
        o.Textures ??= Directory.EnumerateFiles(here, "quakevr-hq-textures-*.zip").FirstOrDefault();
        return o;
    }

}

/// <summary>The whole wizard's state. Pages bind to it; the work is done by QuakeVR.Installer.Core.</summary>
public sealed class MainViewModel : ObservableObject
{
    readonly ISystemProbe _probe;
    readonly StartupOptions _options;
    readonly InstallerSettings _settings;
    Page _page;
    bool _detecting;
    bool _detected;
    DetectionReport? _report;
    QuakeChoice? _selectedQuake;
    string _installDir;
    string? _installDirError;
    bool _hdTextures = true;
    bool _relight = true;
    bool _desktopShortcut = true;
    bool _startMenuShortcuts = true;
    bool _flatShortcut = true;
    bool _logShortcut = true;
    double _progress;
    string _statusText = "";
    bool _installing;
    string? _installError;
    CancellationTokenSource? _cts;
    InstallRecord? _record;
    InstallRecord? _existing;
    InstallDetection _found = new();
    bool _reinstall;
    bool _showReinstall;
    bool _resetSettings;
    bool _removeSaves;
    int _settingsFiles;
    int _savesFiles;
    InstallMode _runMode;
    ReleaseFeed? _feed;
    long? _coreSize;
    Task _details = Task.CompletedTask;
    int _detailsVersion;

    public MainViewModel(ISystemProbe probe, StartupOptions options)
    {
        _probe = probe;
        _options = options;
        _settings = InstallerSettings.Load(Path.Combine(AppContext.BaseDirectory, "installer-settings.json"));
        if (options.Feeds.Count > 0)
        {
            _settings.FeedUrls = options.Feeds;
        }
        DefaultInstallDir = Path.Combine(probe.GetFolder(KnownFolder.LocalAppData) ?? @"C:\QuakeVR", "Programs", "QuakeVR");
        // An existing install: the folder given, this Setup's own install (its copy in <QVR>\setup), the Apps & Features
        // entry, the default folder (InstallDetection; Browse picks another).
        _found = InstallDetection.Find(RegistryFor(options), options.Target, Environment.ProcessPath, DefaultInstallDir);
        _installDir = (options.Target is { } t ? PathUtil.TryNormalize(t) : null) ?? _found.Picked?.Dir ?? DefaultInstallDir;
        Steps = [new(1, "Welcome"), new(2, AiStatement.Title), new(3, "Your PC"), new(4, "Options"), new(5, "Install"), new(6, "Play"), new(7, "Thanks")];
        StatementChoices = [.. Enumerable.Range(0, AiStatement.Claims.Count).Select(i => new StatementChoice(Statement, i))];
        Statement.Changed += () =>
        {
            foreach (var c in StatementChoices)
            {
                c.Refresh();
            }
            Raise(nameof(FooterHint));
            CommandManager.InvalidateRequerySuggested();
        };

        NextCommand = new RelayCommand(Next, CanNext);
        BackCommand = new RelayCommand(Back, () => ShowBack);
        CancelCommand = new RelayCommand(() => _cts?.Cancel(), () => Installing);
        RescanCommand = new RelayCommand(() => _ = _detection = DetectAsync(), () => !Detecting);
        BrowseQuakeCommand = new RelayCommand(BrowseQuake);
        SelectQuakeCommand = new RelayCommand(p => SelectedQuake = p as QuakeChoice ?? SelectedQuake);
        BrowseInstallDirCommand = new RelayCommand(BrowseInstallDir);
        BrowsePackageCommand = new RelayCommand(() => BrowsePackage());
        BrowseTexturesCommand = new RelayCommand(BrowseTextures);
        LaunchVrCommand = new RelayCommand(() => Launch(LaunchVariant.Vr), () => Record is not null);
        LaunchFlatCommand = new RelayCommand(() => Launch(LaunchVariant.Flat), () => Record is not null);
        OpenFolderCommand = new RelayCommand(() => OpenUrl(InstallDir));
        CopySteamOptionsCommand = new RelayCommand(() => Clipboard.SetText(SteamLaunchOptions));
        UninstallCommand = new RelayCommand(() => _ = UninstallAsync(), () => _existing is not null && !Installing);
        ToggleReinstallCommand = new RelayCommand(() => ShowReinstall = !ShowReinstall);
        ReinstallCommand = new RelayCommand(() =>
        {
            Reinstall = true;
            GoTo(Page.Statement);
        }, () => _existing is not null && !Installing);
        BrowseExistingCommand = new RelayCommand(BrowseExisting, () => !Installing);
        OpenUrlCommand = new RelayCommand(p => OpenUrl(p as string ?? ""));
        OpenKofiCommand = new RelayCommand(() => OpenUrl(KofiUrl));
        OpenDiscordCommand = new RelayCommand(() => OpenUrl(DiscordUrl));
        ShowCreditsCommand = new RelayCommand(() => new Views.CreditsWindow(Application.Current.MainWindow).ShowDialog());
        RetryFeedCommand = new RelayCommand(() => _ = CheckFeedAsync(), () => !HasLocalPackage && _feedState != FeedState.Checking);
        PickPackageAndInstallCommand = new RelayCommand(() =>
        {
            if (BrowsePackage())
            {
                _ = InstallAsync();
            }
        }, () => !Installing);
        LoadExisting();
        RefreshPackageInfo();
        GoTo(Page.Welcome);
        if (options.Screenshots is null && options.Package is null)
        {
            _ = CheckFeedAsync();
        }
    }

    public const string KofiUrl = "https://ko-fi.com/vittorioromeovee";
    public const string DiscordUrl = "https://discord.me/quakevr";
    public const string ProductName = "Quake VR: Unleashed";
    public SoundSettings Sound => UiSounds.Settings;

    // ---- Pages and navigation ------------------------------------------------------------------------------------

    public ObservableCollection<StepItem> Steps { get; }
    public string InstallerVersion => $"Installer {InstallerBuild.Version}";

    /// <summary>A test run says so on every page: another feed than the release hosts' (--feed, QVR_SETUP_FEED,
    /// installer-settings.json), a sandbox. Null for a real install.</summary>
    public string? TestBanner =>
        string.Join("   ·   ", new[]
        {
            _settings.HasDefaultFeeds ? null : $"TEST FEED: {string.Join(", ", _settings.FeedUrls)}",
            _options.Sandbox is { } sb ? $"SANDBOX: {sb.Root}" : null,
        }.OfType<string>()) is { Length: > 0 } text ? text : null;
    public bool ShowTestBanner => TestBanner is not null;
    public string DefaultInstallDir { get; }

    public Page Page
    {
        get => _page;
        private set
        {
            if (Set(ref _page, value))
            {
                Raise(nameof(IsWelcome), nameof(IsStatement), nameof(IsDetect), nameof(IsOptions), nameof(IsInstall), nameof(IsDone), nameof(IsSupport), nameof(NextText), nameof(ShowBack), nameof(FooterHint));
            }
        }
    }

    public bool IsWelcome => Page == Page.Welcome;
    public bool IsStatement => Page == Page.Statement;
    public bool IsDetect => Page == Page.Detect;
    public bool IsOptions => Page == Page.Options;
    public bool IsInstall => Page == Page.Install;
    public bool IsDone => Page == Page.Done;
    public bool IsSupport => Page == Page.Support;
    public bool ShowBack => Page is Page.Statement or Page.Detect or Page.Options or Page.Support || (Page == Page.Install && !Installing);
    public bool ShowNext => !(Page == Page.Install);

    public string NextText => Page switch
    {
        Page.Welcome => _existing is null ? "Get started" : !QuickUpdate ? "Update…" : OfferedMode == InstallMode.Repair ? "Repair" : "Update",
        Page.Statement => "Continue",
        Page.Options => Reinstall ? "Install again" : _existing is null ? "Install" : "Update",
        Page.Done => "Next",
        Page.Support => "Finish",
        _ => "Next",
    };

    public string FooterHint => Page switch
    {
        Page.Welcome => _existing is null ? "Nothing is changed until you press Install." :
                        $"Nothing is changed until you press {(OfferedMode == InstallMode.Repair ? "Repair" : "Update")}. Your settings, saves and maps are kept.",
        Page.Statement => Statement.AllYes ? "Your answers are not saved or sent anywhere." : "Continue needs YES to all four.",
        Page.Detect => "Your Quake files are only read, never changed.",
        Page.Options => "Free and open source. No telemetry: nothing is sent about you.",
        Page.Support => "Quake VR: Unleashed is free, and it stays free.",
        _ => "",
    };

    public ICommand NextCommand { get; }
    public ICommand BackCommand { get; }
    public ICommand CancelCommand { get; }
    public ICommand RescanCommand { get; }
    public ICommand BrowseQuakeCommand { get; }
    public ICommand SelectQuakeCommand { get; }
    public ICommand BrowseInstallDirCommand { get; }
    public ICommand BrowsePackageCommand { get; }
    public ICommand BrowseTexturesCommand { get; }
    public ICommand LaunchVrCommand { get; }
    public ICommand LaunchFlatCommand { get; }
    public ICommand OpenFolderCommand { get; }
    public ICommand CopySteamOptionsCommand { get; }
    public ICommand UninstallCommand { get; }
    public ICommand OpenUrlCommand { get; }
    public ICommand OpenKofiCommand { get; }
    public ICommand OpenDiscordCommand { get; }
    public ICommand ShowCreditsCommand { get; }
    public ICommand RetryFeedCommand { get; }
    public ICommand PickPackageAndInstallCommand { get; }
    public ICommand ToggleReinstallCommand { get; }
    public ICommand ReinstallCommand { get; }
    public ICommand BrowseExistingCommand { get; }

    public void GoTo(Page page)
    {
        if (page == Page.Welcome)
        {
            Reinstall = false; // back on the Update screen: its own button updates
        }
        Page = page;
        for (var i = 0; i < Steps.Count; ++i)
        {
            Steps[i].State = i < (int)page ? StepState.Done : i == (int)page ? StepState.Current : StepState.Upcoming;
        }
        Raise(nameof(ShowNext));
        // The buttons' enabled state follows the new page at once (the Statement page's Continue starts disabled).
        CommandManager.InvalidateRequerySuggested();
    }

    bool CanNext() => Page switch
    {
        // The Update screen: Update/Repair needs the package (local, or the release list read).
        Page.Welcome => _existing is null || !QuickUpdate || PackageReady,
        Page.Statement => Statement.AllYes,
        Page.Detect => !Detecting && SelectedQuake is { Playable: true },
        Page.Options => InstallDirError is null && SelectedQuake is not null && PackageReady,
        Page.Install => false,
        _ => true,
    };

    void Next()
    {
        switch (Page)
        {
            case Page.Welcome:
                if (_existing is not null && QuickUpdate)
                {
                    _ = UpdateAsync(); // the Update screen's one button: straight to the update (or repair)
                }
                else
                {
                    GoTo(Page.Statement);
                }
                break;
            case Page.Statement:
                if (!Statement.AllYes)
                {
                    break;
                }
                GoTo(Page.Detect);
                _ = EnsureDetectedAsync();
                break;
            case Page.Detect:
                GoTo(Page.Options);
                ValidateInstallDir();
                break;
            case Page.Options:
                _ = InstallAsync();
                break;
            case Page.Done:
                GoTo(Page.Support);
                break;
            case Page.Support:
                Application.Current.Shutdown();
                break;
        }
    }

    void Back()
    {
        if (Page > Page.Welcome)
        {
            // (An update from the Update screen goes back there.)
            GoTo(Page == Page.Install ? (_runMode == InstallMode.Install ? Page.Options : Page.Welcome) : Page == Page.Support ? Page.Done : Page - 1);
        }
    }

    // ---- Statement: the author's statement on AI usage ------------------------------------------------------------
    // The answers are only in memory for this run of Setup: never saved, never sent.

    public AiStatement Statement { get; } = new();
    public IReadOnlyList<StatementChoice> StatementChoices { get; }
    public string StatementTitle => AiStatement.Title;
    public string StatementSubtitle => AiStatement.Subtitle;
    public IReadOnlyList<string> StatementParagraphs => AiStatement.Paragraphs;

    /// <summary>Whether the forward button is enabled now (tests and the screenshot harness).</summary>
    public bool CanGoNext => NextCommand.CanExecute(null);

    // ---- Welcome: an existing install ----------------------------------------------------------------------------

    // The Update screen: "Quake VR: Unleashed 0.9.0 is installed", Update to the newer package (or Repair with the same
    // or an older one) with the one primary button, Install again from scratch (two choices, a backup first), Remove.

    public bool HasExisting => _existing is not null;
    /// <summary>The package's version: the local package's, else the release list's (null until it is known).</summary>
    string? TargetVersion => HasLocalPackage ? _packageVersion : FeedAvailable ? _feed?.Version : null;
    /// <summary>Update (a newer package, or another build) or Repair (the same or an older one); null while the package's
    /// version is not known.</summary>
    public InstallMode? OfferedMode => _existing is { } e && TargetVersion is { } v ? MaintenancePlanner.ModeFor(e.Version, v) : null;
    /// <summary>The install's Quake folder is still there: Update goes straight to the install (no wizard pages).</summary>
    public bool QuickUpdate => _existing is { } e && File.Exists(Path.Combine(e.QuakeDir, "id1", "pak0.pak"));

    public string ExistingTitle => _existing is { } e ? $"{ProductName} {ReleaseVersion.Parse(e.Version).Short} is installed" : "";

    public string ExistingText
    {
        get
        {
            if (_existing is not { } e)
            {
                return "";
            }
            var to = ReleaseVersion.Parse(TargetVersion).Short;
            return OfferedMode switch
            {
                InstallMode.Update when ReleaseVersion.Compare(e.Version, TargetVersion) == VersionOrder.Other =>
                    $"Another build of {to} is available ({TargetVersion}): Update replaces only the program files that differ.",
                InstallMode.Update => $"Update to {to}: only the program files that changed are replaced.",
                InstallMode.Repair when ReleaseVersion.Compare(e.Version, TargetVersion) == VersionOrder.Older =>
                    $"The package ({to}) is older than the install: Repair checks the files and restores what it can, without going back a version.",
                InstallMode.Repair => "This is the latest version. Repair checks every program file and restores any that are missing or damaged.",
                _ when FeedUnavailable => "The latest version couldn't be found online right now. Try again later, or pick a package (QuakeVR.zip) to update from.",
                _ => "Looking for the latest version…",
            };
        }
    }

    public string ExistingDetail => _existing is not { } e ? "" :
        $"In {InstallDir}. Your settings, saves, screenshots, voice notes, installed maps and relit maps are kept." +
        (QuickUpdate ? "" : $" Its Quake folder ({e.QuakeDir}) is no longer there: Update asks where Quake is.");

    /// <summary>The update could not find a release online: offer a local package and Try again.</summary>
    public bool ExistingNeedsPackage => HasExisting && FeedUnavailable;

    /// <summary>Other installs found (and entries that point nowhere), for the Update screen.</summary>
    public string? OtherInstallsText
    {
        get
        {
            var lines = _found.Found.Where(f => !PathUtil.SamePath(f.Dir, InstallDir)).Select(f => $"Also installed: {ReleaseVersion.Parse(f.Record.Version).Short} in {f.Dir}.")
                .Concat(_found.Notes).ToList();
            return lines.Count > 0 ? string.Join("\n", lines) : null;
        }
    }

    public bool HasOtherInstalls => OtherInstallsText is not null;

    /// <summary>"Install again from scratch" is open on the Update screen.</summary>
    public bool ShowReinstall { get => _showReinstall; private set => Set(ref _showReinstall, value); }
    /// <summary>The wizard runs as "Install again from scratch" (the Update screen's secondary choice).</summary>
    public bool Reinstall
    {
        get => _reinstall;
        private set
        {
            if (Set(ref _reinstall, value))
            {
                Raise(nameof(NextText), nameof(InstallTitle));
            }
        }
    }

    public bool ResetSettings { get => _resetSettings; set => Set(ref _resetSettings, value); }
    public bool RemoveSaves { get => _removeSaves; set => Set(ref _removeSaves, value); }
    public string ReinstallSettingsText => $"Reset settings: configs, retro overrides, body calibration ({_settingsFiles} file{(_settingsFiles == 1 ? "" : "s")})";
    public string ReinstallSavesText => $"Remove saves and the Map Library's installed maps ({_savesFiles} file{(_savesFiles == 1 ? "" : "s")})";
    public string BackupText => $"Whatever is reset or removed is moved first into a dated backup folder, {Path.Combine(InstallDir, UserData.BackupsFolder)}: " +
                                "nothing of yours is deleted. Screenshots, voice notes, relit maps and checklist ticks always stay.";

    void LoadExisting()
    {
        try
        {
            _existing = InstallRecord.Load(InstallDir);
        }
        catch (Exception e) when (e is IOException or InvalidDataException or System.Text.Json.JsonException)
        {
            _existing = null;
        }
        _settingsFiles = _savesFiles = 0;
        if (_existing is not null)
        {
            try
            {
                var mine = UserData.Find(InstallDir, _existing);
                _settingsFiles = mine.Count(f => f.Kind == UserDataKind.Settings);
                _savesFiles = mine.Count(f => f.Kind is UserDataKind.Saves or UserDataKind.Maps);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                // Counts only.
            }
        }
        RaiseExisting();
    }

    void RaiseExisting()
    {
        Raise(nameof(HasExisting), nameof(ExistingTitle), nameof(ExistingText), nameof(ExistingDetail), nameof(OfferedMode), nameof(QuickUpdate),
            nameof(ExistingNeedsPackage), nameof(OtherInstallsText), nameof(HasOtherInstalls), nameof(ReinstallSettingsText), nameof(ReinstallSavesText),
            nameof(BackupText), nameof(NextText), nameof(FooterHint), nameof(InstallTitle));
        CommandManager.InvalidateRequerySuggested();
    }

    /// <summary>Use another install (one moved by hand, or a second one): a folder with install.json.</summary>
    void BrowseExisting()
    {
        var dialog = new Microsoft.Win32.OpenFolderDialog { Title = $"Where is {ProductName} installed?" };
        if (dialog.ShowDialog() != true)
        {
            return;
        }
        if (!File.Exists(Path.Combine(dialog.FolderName, InstallRecord.FileName)))
        {
            MessageBox.Show($"No {ProductName} install was found in {dialog.FolderName} (it has no {InstallRecord.FileName}).", ProductName,
                MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }
        InstallDir = PathUtil.TryNormalize(dialog.FolderName) ?? dialog.FolderName;
    }

    /// <summary>The Install page's title: what the run does.</summary>
    public string InstallTitle => _runMode switch
    {
        InstallMode.Update => $"Updating {ProductName}",
        InstallMode.Repair => $"Repairing {ProductName}",
        _ => Reinstall ? $"Installing {ProductName} again" : $"Installing {ProductName}",
    };

    /// <summary>The Update screen's Update (or Repair): the package (local, or downloaded and checked), then only the
    /// program files that differ; the HD textures only when the pack changed (a repair: or a texture is damaged); the
    /// relight only when its inputs changed; the shortcuts as chosen at install. The statement was answered at install.</summary>
    async Task UpdateAsync()
    {
        if (_existing is not { } existing)
        {
            return;
        }
        var quake = existing.QuakeDir;
        var dir = InstallDir;
        var expected = OfferedMode ?? InstallMode.Update;
        _runMode = expected;
        Raise(nameof(InstallTitle));
        GoTo(Page.Install);
        Log.Clear();
        InstallError = null;
        Progress = 0;
        Installing = true;
        _cts = new CancellationTokenSource();
        var ct = _cts.Token;
        using var http = Downloader.CreateClient();
        try
        {
            UiSounds.Play(Sfx.InstallStart);
            var hdTarget = _settings.Component(_feed, Components.HdTextures);
            StatusText = "Checking the installed files";
            var (hdAction, hdText) = await Task.Run(() => MaintenancePlanner.HdTextures(dir, existing, hdTarget?.File, verify: expected == InstallMode.Repair), ct);
            AddLog(LogLevel.Info, hdText);
            var getHd = hdAction is HdTexturesAction.Replace or HdTexturesAction.Restore && hdTarget is not null;
            var needDownload = _options.Package is null || (getHd && _options.Textures is null);
            var installStart = needDownload ? 50.0 : 0.0;
            var package = _options.Package
                          ?? await DownloadAsync(http, ProductName, f => f.Package, 0, getHd ? 15 : 50, ct)
                          ?? throw new InstallException("No Quake VR package to install: the release list names none.");
            var manifest = InspectPackage(package).Manifest ?? throw new InstallException($"{package} is not a {ProductName} package.");
            var mode = MaintenancePlanner.ModeFor(existing.Version, manifest.Version);
            _runMode = mode;
            Raise(nameof(InstallTitle));
            string? textures = null;
            if (getHd)
            {
                if (_options.Textures is { } local)
                {
                    textures = local;
                }
                else if (_options.Offline)
                {
                    AddLog(LogLevel.Warning, "HD textures: kept as they are (nothing is downloaded in offline mode).");
                }
                else
                {
                    try
                    {
                        textures = await DownloadFileAsync(http, "HD textures", hdTarget!.BuiltIn ? "HD textures" : $"HD textures {_feed?.Version}", hdTarget.File, 15, 50, ct);
                    }
                    catch (Exception e) when (e is InstallException or HttpRequestException)
                    {
                        AddLog(LogLevel.Warning, $"HD textures kept as they are: {e.Message.TrimEnd('.')}.");
                    }
                }
            }
            var owned = ExpansionDetector.Detect([quake, dir], [])
                .Where(e => e.Folder is "hipnotic" or "rogue" && e.State == ExpansionState.Ready).Select(e => e.Folder).ToList();
            var plan = new InstallPlan
            {
                Mode = mode,
                PackagePath = package,
                TargetDir = dir,
                QuakeDir = quake,
                QuakeStore = existing.QuakeStore,
                RelightOnFirstRun = existing.Choices.RelightOnFirstRun,
                HdTexturesZip = textures,
                HdTexturesSha256 = textures is not null && textures != _options.Textures ? hdTarget!.File.Sha256 : null,
                OwnedPacks = owned,
                SetupFiles = OwnSetupFiles(),
                Registry = RegistryFor(_options),
                Shortcuts = new ShortcutOptions
                {
                    Desktop = existing.Choices.DesktopShortcut,
                    StartMenu = existing.Choices.StartMenuShortcuts,
                    Flat = existing.Choices.FlatShortcut,
                    Log = existing.Choices.LogShortcut,
                    DesktopDir = _options.ShortcutsDir is { } sd ? Path.Combine(sd, "Desktop") : _probe.GetFolder(KnownFolder.Desktop),
                    StartMenuDir = _options.ShortcutsDir is { } sm ? Path.Combine(sm, "Programs") : _probe.GetFolder(KnownFolder.StartMenuPrograms),
                },
            };
            var progress = new Progress<InstallProgress>(p =>
            {
                Progress = installStart + (100 - installStart) * p.Fraction;
                StatusText = p.Status;
                if (p.Log is not null)
                {
                    AddLog(p.Level, p.Log);
                }
            });
            Record = await new InstallEngine().InstallAsync(plan, progress, ct);
            await EnsureVcRuntimeAsync(http, ct);
            LoadExisting();
            BuildDoneNotes();
            GoTo(Page.Done);
            UiSounds.Play(Sfx.InstallDone);
        }
        catch (OperationCanceledException)
        {
            InstallError = "Cancelled. Nothing was changed.";
            AddLog(LogLevel.Warning, InstallError);
        }
        catch (Exception e) when (e is InstallException or IOException or UnauthorizedAccessException or InvalidDataException or HttpRequestException)
        {
            InstallError = e.Message;
            AddLog(LogLevel.Error, e.Message);
            UiSounds.Play(Sfx.Error);
        }
        finally
        {
            Installing = false;
            Raise(nameof(ShowNext));
            CommandManager.InvalidateRequerySuggested();
        }
    }

    /// <summary>The Apps &amp; Features entry's registry: a made-up root (--registry-file), the real HKCU for a real install,
    /// none for test installs (--shortcuts-dir) and the screenshot harness.</summary>
    public static IRegistryWriter? RegistryFor(StartupOptions o) =>
        o.RegistryFile is { } f ? new JsonFileRegistry(f) : o.Screenshots is null && o.ShortcutsDir is null && o.Sandbox is null ? new WindowsRegistryWriter() : null;

    /// <summary>This Setup's own files, for its copy in the install (SetupCopy).</summary>
    // IL3000 (the single-file publish's analyser): an empty Location is exactly the test here, "am I a single file?".
#pragma warning disable IL3000
    public static IReadOnlyList<(string Source, string Relative)> OwnSetupFiles() =>
        Environment.ProcessPath is { } exe ? SetupCopy.FilesOf(exe, string.IsNullOrEmpty(typeof(MainViewModel).Assembly.Location)) : [];
#pragma warning restore IL3000

    /// <summary>Started as Apps &amp; Features' Uninstall (--uninstall): the Remove dialogs at once; the window closes
    /// when the install is gone (cancelled: it stays, for an update).</summary>
    public async Task RemoveFromCommandLineAsync()
    {
        await UninstallAsync();
        if (_existing is null)
        {
            Application.Current.Shutdown();
        }
    }

    async Task UninstallAsync()
    {
        if (_existing is null)
        {
            return;
        }
        // This Setup is the install's own copy: the uninstall restarts from a copy in %TEMP%, so this one can be removed.
        if (!_options.FromTemp && Environment.ProcessPath is { } self && PathUtil.IsInside(self, InstallDir))
        {
            SetupRelaunch.Start(_options.UninstallArguments(InstallDir));
            Application.Current.Shutdown();
            return;
        }
        if (MessageBox.Show($"Remove Quake VR: Unleashed from {InstallDir}?\n\nYour settings, saves, screenshots and relit maps are kept, and your Quake folder is not touched.",
                "Remove Quake VR: Unleashed", MessageBoxButton.OKCancel, MessageBoxImage.Question) != MessageBoxResult.OK)
        {
            return;
        }
        var textures = _existing.Files.Any(f => f.Component == Components.HdTextures) &&
                       MessageBox.Show("Also remove the HD textures (about 0.6 GB)?", "Remove Quake VR: Unleashed", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes;
        try
        {
            var dir = InstallDir;
            var registry = RegistryFor(_options);
            var result = await Task.Run(() => Uninstaller.Uninstall(dir, new UninstallOptions { RemoveHdTextures = textures, Registry = registry }));
            var left = result.PlayerFilesLeft.Count > 0 ? $"\n\n{result.PlayerFilesLeft.Count} of your own files are still in {dir} (settings, saves...): delete the folder yourself if you no longer want them." : "";
            MessageBox.Show($"Quake VR: Unleashed was removed: {result.FilesRemoved} files and {result.ShortcutsRemoved} shortcuts.{left}", "Remove Quake VR: Unleashed",
                MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception e) when (e is InstallException or IOException or UnauthorizedAccessException or InvalidDataException)
        {
            MessageBox.Show(e.Message, "Remove Quake VR: Unleashed", MessageBoxButton.OK, MessageBoxImage.Error);
        }
        LoadExisting();
    }

    // ---- Detect -------------------------------------------------------------------------------------------------

    public bool Detecting
    {
        get => _detecting;
        private set => Set(ref _detecting, value);
    }

    public ObservableCollection<QuakeChoice> Quakes { get; } = [];
    public ObservableCollection<CheckItem> QuakeChecks { get; } = [];
    public ObservableCollection<ExpansionRow> Expansions { get; } = [];
    public ObservableCollection<CheckItem> SystemChecks { get; } = [];
    public bool NoQuake => _detected && Quakes.Count == 0;
    public bool HasQuake => SelectedQuake is not null;

    public QuakeChoice? SelectedQuake
    {
        get => _selectedQuake;
        set
        {
            if (_selectedQuake is not null)
            {
                _selectedQuake.IsSelected = false;
            }
            if (Set(ref _selectedQuake, value))
            {
                if (value is not null)
                {
                    value.IsSelected = true;
                    QuakeChosen?.Invoke(value.Install);
                }
                Raise(nameof(HasQuake));
                _details = RefreshQuakeDetailsAsync();
                ValidateInstallDir();
            }
        }
    }

    Task? _detection;

    /// <summary>A Quake was picked (by the detection or by hand): the skin reads its textures and sounds.</summary>
    public event Action<QuakeInstall>? QuakeChosen;

    /// <summary>The detection, started once (at start, quietly) and shared by whoever needs it.</summary>
    public Task EnsureDetectedAsync() => _detected ? Task.CompletedTask : _detection is { IsCompleted: false } d ? d : _detection = DetectAsync();

    public async Task DetectAsync()
    {
        Detecting = true;
        Quakes.Clear();
        QuakeChecks.Clear();
        Expansions.Clear();
        SystemChecks.Clear();
        try
        {
            var probe = _probe;
            _report = await Task.Run(() => DetectionReport.Run(probe));
            foreach (var q in _report.Quakes)
            {
                Quakes.Add(new QuakeChoice(q));
            }
            BuildSystemChecks(_report);
            var preferred = _report.DefaultQuake;
            if (_existing is not null)
            {
                preferred = _report.Quakes.FirstOrDefault(q => q.BaseDir is { } b && PathUtil.SamePath(b, _existing.QuakeDir)) ?? preferred;
            }
            SelectedQuake = Quakes.FirstOrDefault(q => q.Install == preferred);
            await _details;
        }
        finally
        {
            _detected = true;
            Detecting = false;
            Raise(nameof(NoQuake));
            CommandManager.InvalidateRequerySuggested();
        }
    }

    void BrowseQuake()
    {
        var dialog = new Microsoft.Win32.OpenFolderDialog { Title = "Pick your Quake folder (the one with id1)" };
        if (dialog.ShowDialog() != true)
        {
            return;
        }
        var q = QuakeDetector.DescribeManual(dialog.FolderName);
        if (!q.Playable)
        {
            MessageBox.Show($"No full Quake in {q.InstallDir}: {q.Summary}.\n\nPick the folder that contains id1 (with PAK0.PAK and PAK1.PAK), or a 2021 rerelease folder.",
                ProductName, MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        var choice = new QuakeChoice(q);
        Quakes.Add(choice);
        SelectedQuake = choice;
        Raise(nameof(NoQuake));
    }

    /// <summary>The selected Quake's music, notes and expansions. A newer call (another Quake picked meanwhile) wins.</summary>
    async Task RefreshQuakeDetailsAsync()
    {
        var version = ++_detailsVersion;
        QuakeChecks.Clear();
        Expansions.Clear();
        if (_report is null || SelectedQuake is not { } choice)
        {
            return;
        }
        var q = choice.Install;
        QuakeChecks.Add(q.MusicDir is not null
            ? new CheckItem(CheckStatus.Ok, "Music", $"Read in place from {q.MusicDir}")
            : new CheckItem(CheckStatus.Info, "Music", "No soundtrack found: the game plays without music.",
                "The 2021 rerelease's soundtrack is used when it is installed (it is free with the original on Steam and GOG)."));
        if (q.Original.Playable && !q.Original.KnownVersion)
        {
            QuakeChecks.Add(new CheckItem(CheckStatus.Info, "Game data", q.Original.Detail));
        }
        if (q.ExistingQuakeVr is not null)
        {
            QuakeChecks.Add(new CheckItem(CheckStatus.Info, "Older Quake VR", $"A Quake VR zip install is in {q.ExistingQuakeVr}.",
                "It keeps working; this install is separate and leaves it (and its saves) alone."));
        }
        var report = _report;
        var dir = InstallDir;
        var list = await Task.Run(() => report.Expansions(q, dir));
        if (version != _detailsVersion)
        {
            return;
        }
        foreach (var e in list)
        {
            Expansions.Add(new ExpansionRow(e));
        }
        Raise(nameof(TexturesText));
    }

    void BuildSystemChecks(DetectionReport r)
    {
        var vr = r.Vr;
        SystemChecks.Add(vr.Active switch
        {
            null => new CheckItem(CheckStatus.Warning, "OpenXR runtime", "No OpenXR runtime is active.",
                "Install and start SteamVR, Virtual Desktop or Meta Quest Link: Quake VR works with any OpenXR runtime. Flat-screen play works without one."),
            { ManifestExists: false } a => new CheckItem(CheckStatus.Warning, "OpenXR runtime", $"The active runtime ({a.Name}) points to a missing file.",
                "Reinstall or restart your VR software so it registers itself again."),
            var a => new CheckItem(CheckStatus.Ok, "OpenXR runtime", $"{a.Name} is active."),
        });
        if (vr.VirtualDesktopInstalled)
        {
            SystemChecks.Add(vr.SuggestVdxr
                ? new CheckItem(CheckStatus.Info, "Virtual Desktop", "Installed. Quake VR plays best through its own OpenXR runtime (VDXR), as the author plays it.",
                    "The game picks VDXR by itself while the Streamer is running (or SteamVR, when that's the OpenXR runtime chosen in the Streamer; VR Settings > OpenXR Runtime: Auto). Keep the Streamer's \"Emulate Index controllers\" off.")
                : new CheckItem(CheckStatus.Ok, "Virtual Desktop", "Installed, and its runtime (VDXR) is active: the author's setup.",
                    "Keep the Streamer's \"Emulate Index controllers\" off."));
        }
        else
        {
            SystemChecks.Add(new CheckItem(CheckStatus.Absent, "Virtual Desktop", "Not installed (optional)."));
        }
        SystemChecks.Add(vr.SteamVrInstalled
            ? new CheckItem(CheckStatus.Ok, "SteamVR", "Installed.")
            : new CheckItem(CheckStatus.Absent, "SteamVR", "Not installed (optional)."));
        SystemChecks.Add(r.VcRuntime.Ok
            ? new CheckItem(CheckStatus.Ok, "Visual C++ runtime", $"{r.VcRuntime.Installed} installed.")
            : new CheckItem(CheckStatus.Warning, "Visual C++ runtime", r.VcRuntime.Describe(),
                "Setup installs Microsoft's Visual C++ Redistributable (x64) with Quake VR: Windows asks for permission once.")
            {
                ActionText = "Get it from Microsoft",
                Action = new RelayCommand(() => OpenUrl(VcRuntimeInfo.DownloadUrl)),
            });
    }

    // ---- Options ------------------------------------------------------------------------------------------------

    public string InstallDir
    {
        get => _installDir;
        set
        {
            if (Set(ref _installDir, value))
            {
                ValidateInstallDir();
                LoadExisting();
            }
        }
    }

    public string? InstallDirError
    {
        get => _installDirError;
        private set
        {
            if (Set(ref _installDirError, value))
            {
                Raise(nameof(HasInstallDirError));
            }
        }
    }

    public bool HasInstallDirError => InstallDirError is not null;

    public string DiskText
    {
        get
        {
            var need = (_coreSize ?? 200L << 20) + (HdTextures ? 650L << 20 : 0) + (Relight ? 220L << 20 : 0);
            try
            {
                var root = Path.GetPathRoot(Path.GetFullPath(InstallDir));
                var drive = root is null ? null : new DriveInfo(root);
                return drive is { IsReady: true }
                    ? $"Needs about {PathUtil.FormatSize(need)}; {PathUtil.FormatSize(drive.AvailableFreeSpace)} free on {drive.Name.TrimEnd('\\')}."
                    : $"Needs about {PathUtil.FormatSize(need)}.";
            }
            catch (Exception e) when (e is ArgumentException or IOException or NotSupportedException)
            {
                return $"Needs about {PathUtil.FormatSize(need)}.";
            }
        }
    }

    internal void ValidateInstallDir()
    {
        var quake = SelectedQuake?.Install.BaseDir;
        InstallDirError = quake is null ? null : InstallEngine.ValidateTarget(InstallDir, quake, _probe);
        Raise(nameof(DiskText));
    }

    void BrowseInstallDir()
    {
        var dialog = new Microsoft.Win32.OpenFolderDialog { Title = "Where should Quake VR go?" };
        if (dialog.ShowDialog() == true)
        {
            var picked = dialog.FolderName;
            // An empty folder is used as is; otherwise a QuakeVR folder inside it.
            InstallDir = Directory.Exists(picked) && Directory.EnumerateFileSystemEntries(picked).Any() &&
                         !File.Exists(Path.Combine(picked, InstallRecord.FileName)) ? Path.Combine(picked, "QuakeVR") : picked;
        }
    }

    public bool HdTextures { get => _hdTextures; set { if (Set(ref _hdTextures, value)) { Raise(nameof(DiskText)); } } }
    public bool Relight { get => _relight; set { if (Set(ref _relight, value)) { Raise(nameof(DiskText)); } } }
    public bool DesktopShortcut { get => _desktopShortcut; set => Set(ref _desktopShortcut, value); }
    public bool StartMenuShortcuts { get => _startMenuShortcuts; set => Set(ref _startMenuShortcuts, value); }
    public bool FlatShortcut { get => _flatShortcut; set => Set(ref _flatShortcut, value); }
    public bool LogShortcut { get => _logShortcut; set => Set(ref _logShortcut, value); }

    public string CoreSizeText => _coreSize is { } s ? PathUtil.FormatSize(s) : "about 200 MB";

    public string TexturesText
    {
        get
        {
            var owned = Expansions.Where(e => e.Status == CheckStatus.Ok && e.Title is "Scourge of Armagon" or "Dissolution of Eternity").Select(e => e.Title).ToList();
            var forWhat = owned.Count == 0 ? "Quake" : "Quake, " + string.Join(" and ", owned);
            return $"The Quake Revitalization Project's high-resolution textures for {forWhat} (only the packs you own).";
        }
    }

    public string TexturesSizeText =>
        _options.Textures is { } t && File.Exists(t) ? PathUtil.FormatSize(new FileInfo(t).Length) :
        TexturesDownloadUnavailable ? "download unavailable" :
        _settings.Component(_feed, Components.HdTextures) is { } c ? $"{PathUtil.FormatSize(c.File.Size)} download" :
        "about 0.6 GB download";

    /// <summary>The HD textures would be downloaded, and there is nothing to download them from (offline mode, or no
    /// release list and no built-in pack): they are skipped unless the player picks the texture pack's zip. Without a
    /// release list the installer's built-in pack is downloaded (<see cref="BuiltInComponents"/>).</summary>
    public bool TexturesDownloadUnavailable => _options.Textures is null &&
        (_options.Offline || (_feedState == FeedState.Unavailable && _settings.Component(_feed, Components.HdTextures) is null));

    public string TexturesUnavailableText => _options.Offline
        ? "Nothing is downloaded in offline mode: they are skipped (run Setup again later), or"
        : "Their download isn't available right now: they are skipped (run Setup again later), or";

    // ---- Where the package comes from: a local one, or the online release ----

    string? _packageVersion;
    string? _packageError;
    string? _feedError;
    FeedState _feedState;

    public bool HasLocalPackage => _options.Package is not null;
    public bool FeedChecking => !HasLocalPackage && _feedState is FeedState.Checking or FeedState.Unknown;
    public bool FeedAvailable => !HasLocalPackage && _feedState == FeedState.Available;
    public bool FeedUnavailable => !HasLocalPackage && _feedState == FeedState.Unavailable;
    /// <summary>Something to install: a local package, or a release online.</summary>
    public bool PackageReady => HasLocalPackage || FeedAvailable;
    public string? PackageError { get => _packageError; private set { if (Set(ref _packageError, value)) { Raise(nameof(HasPackageError)); } } }
    public bool HasPackageError => PackageError is not null;
    public string? FeedErrorDetail => _feedError is null ? null : $"Details: {_feedError}";

    public string PackageSourceTitle =>
        HasLocalPackage ? "Installing from a local package" :
        FeedAvailable ? "Downloading the latest release" :
        FeedUnavailable ? "The online download isn't available right now" :
        "Looking for the latest release online…";

    public string PackageSourceText =>
        _options.Package is { } p ? $"{Path.GetFileName(p.TrimEnd('\\', '/'))}{(_packageVersion is { } v ? $" · version {v}" : "")}{(_coreSize is { } cs ? $" · {PathUtil.FormatSize(cs)}" : "")} · no internet needed. {p}" :
        FeedAvailable && _feed is { } feed ? $"{ProductName} {feed.Version}{(feed.Package is { } fp ? $", {PathUtil.FormatSize(fp.Size)}" : "")}, from GitHub, checked against its published SHA-256." :
        FeedUnavailable ? "No release could be found online: none may be published yet, or this PC is offline. If you have a " +
                          $"{ProductName} package (QuakeVR.zip, or its unzipped folder with manifest.json), pick it and the installer uses it without any internet." :
        "One moment…";

    /// <summary>Asks the release hosts for latest.json (in the background at start; again with Try again).</summary>
    public async Task CheckFeedAsync()
    {
        if (HasLocalPackage)
        {
            return;
        }
        SetFeedState(FeedState.Checking, null);
        if (_options.Offline)
        {
            SetFeedState(FeedState.Unavailable, "the network was not asked (offline mode)");
            return;
        }
        try
        {
            using var http = Downloader.CreateClient();
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(25));
            _feed = await ReleaseFeed.FetchAsync(http, _settings.FeedUrls.Select(u => new Uri(u)), cts.Token);
            SetFeedState(_feed.Package is null ? FeedState.Unavailable : FeedState.Available, _feed.Package is null ? "the release list names no package" : null);
        }
        catch (Exception e) when (e is InstallException or HttpRequestException or OperationCanceledException or UriFormatException)
        {
            _feed = null;
            SetFeedState(FeedState.Unavailable, e.Message);
        }
    }

    void SetFeedState(FeedState state, string? error)
    {
        _feedState = state;
        _feedError = error;
        Raise(nameof(FeedChecking), nameof(FeedAvailable), nameof(FeedUnavailable), nameof(PackageReady), nameof(PackageSourceTitle),
            nameof(PackageSourceText), nameof(FeedErrorDetail), nameof(TexturesSizeText), nameof(TexturesDownloadUnavailable), nameof(CanPickPackageAfterError));
        RaiseExisting();
    }

    /// <summary>A package's manifest, or why it is not a package.</summary>
    public static (PackageManifest? Manifest, string? Error) InspectPackage(string path) => LocalPackages.Inspect(path, ProductName);

    void RefreshPackageInfo()
    {
        _coreSize = null;
        _packageVersion = null;
        if (_options.Package is { } p && InspectPackage(p).Manifest is { } m)
        {
            _coreSize = m.TotalSize;
            _packageVersion = m.Version;
        }
        Raise(nameof(CoreSizeText), nameof(PackageSourceText), nameof(PackageSourceTitle), nameof(TexturesSizeText), nameof(DiskText),
            nameof(HasLocalPackage), nameof(FeedChecking), nameof(FeedAvailable), nameof(FeedUnavailable), nameof(PackageReady),
            nameof(CanPickPackageAfterError));
        RaiseExisting();
    }

    /// <summary>Picks a package (a zip, or a folder's manifest.json). True when one was accepted.</summary>
    bool BrowsePackage()
    {
        var dialog = new Microsoft.Win32.OpenFileDialog
        {
            Title = $"Pick a {ProductName} package",
            Filter = $"{ProductName} package (*.zip; manifest.json)|*.zip;manifest.json|All files|*.*",
        };
        if (dialog.ShowDialog() != true)
        {
            return false;
        }
        var path = Path.GetFileName(dialog.FileName).Equals(PackageManifest.FileName, StringComparison.OrdinalIgnoreCase)
            ? Path.GetDirectoryName(dialog.FileName)!
            : dialog.FileName;
        return UsePackage(path);
    }

    /// <summary>Uses a local package from now on (no download); false (and PackageError) when it is not one.</summary>
    internal bool UsePackage(string path)
    {
        var (manifest, error) = InspectPackage(path);
        PackageError = error;
        if (manifest is null)
        {
            UiSounds.Play(Sfx.Error);
            return false;
        }
        _options.Package = path;
        RefreshPackageInfo();
        return true;
    }

    void BrowseTextures()
    {
        var dialog = new Microsoft.Win32.OpenFileDialog { Title = "Pick the HD texture pack", Filter = "Texture pack (*.zip)|*.zip" };
        if (dialog.ShowDialog() == true)
        {
            _options.Textures = dialog.FileName;
            Raise(nameof(TexturesSizeText));
        }
    }

    // ---- Install ------------------------------------------------------------------------------------------------

    public ObservableCollection<LogLine> Log { get; } = [];

    public double Progress
    {
        get => _progress;
        private set
        {
            if (Set(ref _progress, value))
            {
                Raise(nameof(ProgressText));
            }
        }
    }

    public string ProgressText => $"{Progress:0}%";

    public string StatusText
    {
        get => _statusText;
        private set => Set(ref _statusText, value);
    }

    public bool Installing
    {
        get => _installing;
        private set
        {
            if (Set(ref _installing, value))
            {
                Raise(nameof(ShowBack));
            }
        }
    }

    public string? InstallError
    {
        get => _installError;
        private set
        {
            if (Set(ref _installError, value))
            {
                Raise(nameof(HasInstallError), nameof(CanPickPackageAfterError));
            }
        }
    }

    public bool HasInstallError => InstallError is not null;

    /// <summary>The install stopped because the release could not be downloaded: offer a local package instead.</summary>
    public bool CanPickPackageAfterError => HasInstallError && !HasLocalPackage && _feedState == FeedState.Unavailable;

    void AddLog(LogLevel level, string text) => Log.Add(new LogLine(level, text));

    async Task InstallAsync()
    {
        if (SelectedQuake?.Install.BaseDir is not { } quake)
        {
            return;
        }
        _runMode = InstallMode.Install;
        Raise(nameof(InstallTitle));
        GoTo(Page.Install);
        Log.Clear();
        InstallError = null;
        Progress = 0;
        Installing = true;
        _cts = new CancellationTokenSource();
        var ct = _cts.Token;
        using var http = Downloader.CreateClient();
        try
        {
            UiSounds.Play(Sfx.InstallStart);
            // Over an existing install through the wizard (its Quake folder moved): an update, keeping what is unchanged;
            // "Install again from scratch": a full install, after the chosen files went into the backup.
            var mode = _existing is null || Reinstall ? InstallMode.Install : InstallMode.Update;
            var keepHd = false;
            if (mode == InstallMode.Update && HdTextures && _options.Textures is null)
            {
                var (hdAction, hdText) = MaintenancePlanner.HdTextures(InstallDir, _existing!, _settings.Component(_feed, Components.HdTextures)?.File, verify: false);
                keepHd = hdAction is HdTexturesAction.Keep or HdTexturesAction.KeepUnknown;
                if (keepHd)
                {
                    AddLog(LogLevel.Info, hdText);
                }
            }
            var texturesOnline = HdTextures && !keepHd && _options.Textures is null && !TexturesDownloadUnavailable;
            var needDownload = _options.Package is null || texturesOnline;
            var installStart = needDownload ? 50.0 : 0.0;
            var package = _options.Package;
            if (package is null)
            {
                package = await DownloadAsync(http, ProductName, f => f.Package, 0, HdTextures ? 15 : 50, ct)
                          ?? throw new InstallException("No Quake VR package to install: the release list names none.");
            }
            string? textures = null;
            if (HdTextures && !keepHd)
            {
                textures = _options.Textures;
                if (textures is null && !texturesOnline)
                {
                    AddLog(LogLevel.Warning, $"HD textures skipped: {(_options.Offline ? "nothing is downloaded in offline mode" : "their download isn't available right now")}. " +
                                             "Run Setup again later to add them, or pick the texture pack's zip on the Options page.");
                }
                else if (textures is null)
                {
                    try
                    {
                        textures = await DownloadTexturesAsync(http, 15, 50, ct);
                    }
                    catch (Exception e) when (e is InstallException or HttpRequestException)
                    {
                        AddLog(LogLevel.Warning, $"HD textures skipped: {e.Message.TrimEnd('.')}.");
                    }
                }
            }
            var owned = Expansions.Where(e => e.Status == CheckStatus.Ok).Select(e => e.Title switch
            {
                "Scourge of Armagon" => "hipnotic",
                "Dissolution of Eternity" => "rogue",
                _ => "",
            }).Where(s => s.Length > 0).ToList();
            // See-through water: VisPatch's data with the relight (never fatal: without it the water stays opaque).
            // (An update keeps the VisPatch data installed before: it is pinned, the same files.)
            var visPatch = Relight && !(mode == InstallMode.Update && _existing!.Choices.VisPatch) ? await GetVisPatchAsync(http, owned, ct) : [];
            var progress = new Progress<InstallProgress>(p =>
            {
                Progress = installStart + (100 - installStart) * p.Fraction;
                StatusText = p.Status;
                if (p.Log is not null)
                {
                    AddLog(p.Level, p.Log);
                }
            });
            // Install again from scratch: the player's chosen files go into a checked, dated backup before anything else.
            Backup? backup = null;
            if (mode == InstallMode.Install && Reinstall && _existing is not null)
            {
                var choice = new ReinstallOptions { ResetSettings = ResetSettings, RemoveSaves = RemoveSaves };
                var dir = InstallDir;
                backup = await Task.Run(() => Reinstaller.Prepare(dir, choice, DateTimeOffset.Now, progress), ct);
                if (backup is null)
                {
                    AddLog(LogLevel.Info, "Installing again: nothing of yours to reset or remove.");
                }
            }
            var plan = new InstallPlan
            {
                Mode = mode,
                Backup = backup,
                PackagePath = package,
                TargetDir = InstallDir,
                QuakeDir = quake,
                QuakeStore = SelectedQuake.Install.Store.ToString(),
                RelightOnFirstRun = Relight,
                HdTexturesZip = textures,
                VisPatchArchives = visPatch,
                OwnedPacks = owned,
                SetupFiles = OwnSetupFiles(),
                Registry = RegistryFor(_options),
                Shortcuts = new ShortcutOptions
                {
                    Desktop = DesktopShortcut,
                    StartMenu = StartMenuShortcuts,
                    Flat = FlatShortcut,
                    Log = LogShortcut,
                    DesktopDir = _options.ShortcutsDir is { } sd ? Path.Combine(sd, "Desktop") : _probe.GetFolder(KnownFolder.Desktop),
                    StartMenuDir = _options.ShortcutsDir is { } sm ? Path.Combine(sm, "Programs") : _probe.GetFolder(KnownFolder.StartMenuPrograms),
                },
            };
            Record = await new InstallEngine().InstallAsync(plan, progress, ct);
            await EnsureVcRuntimeAsync(http, ct);
            LoadExisting();
            BuildDoneNotes();
            GoTo(Page.Done);
            UiSounds.Play(Sfx.InstallDone);
        }
        catch (OperationCanceledException)
        {
            InstallError = "Cancelled. Nothing was changed.";
            AddLog(LogLevel.Warning, InstallError);
        }
        catch (Exception e) when (e is InstallException or IOException or UnauthorizedAccessException or InvalidDataException or HttpRequestException)
        {
            InstallError = e.Message;
            AddLog(LogLevel.Error, e.Message);
            UiSounds.Play(Sfx.Error);
        }
        finally
        {
            Installing = false;
            Raise(nameof(ShowNext));
            CommandManager.InvalidateRequerySuggested();
        }
    }

    /// <summary>Microsoft's VC++ runtime when it is missing or too old (VcRedist): a signed copy beside the installer, or
    /// a download checked by its Microsoft signature, run with one administrator prompt. Never fatal: the files are in
    /// place, and the Play page says what is left to do. The screenshot harness and --vcredist-dry-run only log it.</summary>
    async Task EnsureVcRuntimeAsync(HttpClient http, CancellationToken ct)
    {
        VcResult = null;
        if (_options.NoPrerequisites)
        {
            return;
        }
        var info = VcRuntimeDetector.Detect(_probe);
        if (info.Ok)
        {
            return;
        }
        StatusText = "Visual C++ runtime";
        AddLog(LogLevel.Info, $"Visual C++ runtime: {info.Describe()}");
        var dir = _options.Downloads ?? Path.Combine(_probe.GetFolder(KnownFolder.LocalAppData) ?? Path.GetTempPath(), "QuakeVR-Installer", "downloads");
        var progress = new Progress<InstallProgress>(p =>
        {
            StatusText = p.Status;
            if (p.Log is not null)
            {
                AddLog(p.Level, p.Log);
            }
        });
        VcResult = await VcRedist.ForWindows(http).EnsureAsync(info, new VcRedistOptions
        {
            DryRun = _options.VcRedistDryRun || _options.Screenshots is not null,
            Offline = _options.Offline,
            LocalCopies = [Path.Combine(AppContext.BaseDirectory, VcRedist.FileName)],
            DownloadDir = dir,
        }, progress, ct);
        AddLog(VcResult.RuntimeReady ? LogLevel.Success : VcResult.Outcome == VcRedistOutcome.DryRun ? LogLevel.Info : LogLevel.Warning, VcResult.Message);
    }

    /// <summary>What the last install did about the VC++ runtime (null: nothing needed).</summary>
    public VcRedistResult? VcResult { get; private set; }

    async Task<string?> DownloadAsync(HttpClient http, string what, Func<ReleaseFeed, FeedFile?> pick, double from, double to, CancellationToken ct)
    {
        var feed = await GetFeedAsync(http, what, ct);
        return pick(feed) is { } file ? await DownloadFileAsync(http, what, $"{what} {feed.Version}", file, from, to, ct) : null;
    }

    /// <summary>The HD textures: the release list's hdtextures when it names them (the list wins), else the installer's
    /// built-in pack (also when the list cannot be read). The SHA-256 is checked either way.</summary>
    async Task<string?> DownloadTexturesAsync(HttpClient http, double from, double to, CancellationToken ct)
    {
        const string what = "HD textures";
        ReleaseFeed? feed = null;
        try
        {
            feed = await GetFeedAsync(http, what, ct);
        }
        catch (InstallException) when (!_options.Offline)
        {
            // No release list: the built-in pack below.
        }
        if (_settings.Component(feed, Components.HdTextures) is not { } c)
        {
            throw new InstallException("their download isn't available right now. Run Setup again later to add them");
        }
        if (c.BuiltIn)
        {
            AddLog(LogLevel.Info, $"HD textures: the installer's built-in pack ({c.File.File}), as the release list names none.");
        }
        return await DownloadFileAsync(http, what, c.BuiltIn ? what : $"{what} {feed!.Version}", c.File, from, to, ct);
    }

    /// <summary>The release list (read once): throws an InstallException that says why when it cannot be read.</summary>
    async Task<ReleaseFeed> GetFeedAsync(HttpClient http, string what, CancellationToken ct)
    {
        StatusText = "Checking for the latest release";
        if (_feed is null && _options.Offline)
        {
            throw new InstallException(what == "HD textures" ? "offline mode: nothing is downloaded" : "Offline mode: nothing is downloaded. Pick a local package.");
        }
        if (_feed is null)
        {
            try
            {
                _feed = await ReleaseFeed.FetchAsync(http, _settings.FeedUrls.Select(u => new Uri(u)), ct);
            }
            catch (InstallException e)
            {
                SetFeedState(FeedState.Unavailable, e.Message);
                AddLog(LogLevel.Info, e.Message);
                throw new InstallException(what == "HD textures"
                    ? "their download isn't available right now. Run Setup again later to add them"
                    : "The online release couldn't be reached: none may be published yet, or this PC is offline. " +
                      $"Pick a local {ProductName} package (QuakeVR.zip) to install without the internet, or try again later.");
            }
        }
        return _feed;
    }

    /// <summary>One file into the downloads folder, resumed, from its mirrors in order, checked (size, SHA-256).</summary>
    async Task<string> DownloadFileAsync(HttpClient http, string what, string label, FeedFile file, double from, double to, CancellationToken ct)
    {
        var dir = _options.Downloads ?? Path.Combine(_probe.GetFolder(KnownFolder.LocalAppData) ?? Path.GetTempPath(), "QuakeVR-Installer", "downloads");
        var dest = Path.Combine(dir, Path.GetFileName(file.File));
        AddLog(LogLevel.Info, $"Downloading {label} ({PathUtil.FormatSize(file.Size)}).");
        var progress = new Progress<DownloadProgress>(p =>
        {
            var total = p.Total ?? file.Size;
            Progress = from + (to - from) * (total > 0 ? (double)p.Received / total : 0);
            StatusText = $"Downloading {what} from {p.Source}: {PathUtil.FormatSize(p.Received)} of {PathUtil.FormatSize(total)}";
        });
        await new Downloader(http).DownloadAsync(file.Mirrors, dest, file.Size, file.Sha256, progress, ct);
        AddLog(LogLevel.Success, $"{what} downloaded and checked (SHA-256).");
        return dest;
    }

    /// <summary>VisPatch's archives for id1 and the owned packs: a copy beside the installer or in the downloads folder
    /// when it matches the pinned SHA-256, else a download from its original location (installer-settings.json's
    /// visPatchUrls). Each one that cannot be had is skipped with a note; the relight still runs.</summary>
    async Task<List<string>> GetVisPatchAsync(HttpClient http, IReadOnlyList<string> owned, CancellationToken ct)
    {
        var result = new List<string>();
        var dir = _options.Downloads ?? Path.Combine(_probe.GetFolder(KnownFolder.LocalAppData) ?? Path.GetTempPath(), "QuakeVR-Installer", "downloads");
        var missing = new List<string>();
        foreach (var archive in VisPatch.For(owned))
        {
            var beside = Path.Combine(AppContext.BaseDirectory, archive.File);
            if (VisPatch.Matches(beside, archive))
            {
                result.Add(beside);
                continue;
            }
            if (_options.Offline)
            {
                missing.Add(archive.File);
                continue;
            }
            StatusText = $"Downloading the VisPatch data ({archive.File})";
            try
            {
                var dest = Path.Combine(dir, archive.File);
                await new Downloader(http).DownloadAsync(VisPatch.Mirrors(archive, _settings.VisPatchUrls), dest, archive.Size, archive.Sha256, null, ct);
                AddLog(LogLevel.Info, $"VisPatch data: {archive.File} ({PathUtil.FormatSize(archive.Size)}) downloaded and checked (SHA-256).");
                result.Add(dest);
            }
            catch (Exception e) when (e is InstallException or HttpRequestException or IOException or UriFormatException)
            {
                AddLog(LogLevel.Info, $"{archive.File}: {e.Message}");
                missing.Add(archive.File);
            }
        }
        if (missing.Count > 0)
        {
            AddLog(LogLevel.Warning, $"See-through water skipped for {string.Join(", ", missing)}: the VisPatch data could not be downloaded. " +
                                     "The maps are still relit, their water stays opaque; run Setup again later to add it.");
        }
        return result;
    }

    // ---- Done ---------------------------------------------------------------------------------------------------

    public InstallRecord? Record
    {
        get => _record;
        private set => Set(ref _record, value);
    }

    public ObservableCollection<CheckItem> DoneNotes { get; } = [];

    public string SteamLaunchOptions => (SelectedQuake?.Install.BaseDir ?? Record?.QuakeDir) is { Length: > 0 } q ? LaunchCommand.Arguments(q, InstallDir, LaunchVariant.Vr) : "";
    public string SteamExePath => Path.Combine(InstallDir, LaunchCommand.Exe);

    public void BuildDoneNotes()
    {
        DoneNotes.Clear();
        DoneNotes.Add(new CheckItem(CheckStatus.Info, "First start: VR Calibration",
            "The game takes you to the calibration room once: your height and body, then the main settings on its wall buttons."));
        if (Record?.RelightPending == true)
        {
            // The installer relights nothing itself: the game does, once, and skips what it relit before (an update).
            var before = FirstStartRelight.RelitCopies(InstallDir);
            DoneNotes.Add(new CheckItem(CheckStatus.Info, "Relit maps", before > 0
                ? $"The {before} maps relit before are kept. At its next start (Play below, a shortcut or Steam) the game relights only the maps that are new or changed since, " +
                  "and skips the rest (the wrist gadget shows its progress). Later: Graphics > Relighting."
                : "At its first start (Play below, a shortcut or Steam) the game relights every map with the HD textures (a few minutes, about two on a fast PC; the wrist gadget shows its progress). " +
                  "Later: Graphics > Relighting."));
        }
        if (VcResult is { } vc && vc.Outcome != VcRedistOutcome.AlreadyInstalled)
        {
            DoneNotes.Add(vc.Outcome switch
            {
                VcRedistOutcome.Installed => new CheckItem(CheckStatus.Ok, "Visual C++ runtime", vc.Message),
                VcRedistOutcome.RebootRequired => new CheckItem(CheckStatus.Info, "Visual C++ runtime", vc.Message),
                VcRedistOutcome.DryRun => new CheckItem(CheckStatus.Info, "Visual C++ runtime", vc.Message),
                _ => new CheckItem(CheckStatus.Warning, "Visual C++ runtime", vc.Message,
                    "The game needs it to start: install Microsoft's Visual C++ Redistributable (x64), or run Setup again.")
                {
                    ActionText = "Get it from Microsoft",
                    Action = new RelayCommand(() => OpenUrl(VcRuntimeInfo.DownloadUrl)),
                },
            });
        }
        if (_report?.Vr.SuggestVdxr == true)
        {
            DoneNotes.Add(new CheckItem(CheckStatus.Info, "Virtual Desktop",
                "The game picks Virtual Desktop's runtime (VDXR), the author's setup, by itself while the Streamer is running (or SteamVR, when that's the OpenXR runtime chosen in the Streamer; VR Settings > OpenXR Runtime: Auto)."));
        }
        Raise(nameof(SteamLaunchOptions), nameof(SteamExePath));
    }

    void Launch(LaunchVariant variant)
    {
        // (After an update from the Update screen no Quake was detected: the install's own.)
        if (Record is null || (SelectedQuake?.Install.BaseDir ?? Record.QuakeDir) is not { Length: > 0 } quake)
        {
            return;
        }
        // (The relight at the first start needs no argument: the game starts it from the installer's marker,
        // FirstStartRelight, however it is started.)
        StartGame(new ProcessStartInfo(Path.Combine(InstallDir, LaunchCommand.Exe), LaunchCommand.Arguments(quake, InstallDir, variant))
        {
            WorkingDirectory = InstallDir,
            UseShellExecute = false,
        });
        // The game has the sound now: the installer's fades out and stays muted (the speaker button shows it).
        UiSounds.MuteForGame();
    }

    /// <summary>Starts the game (the screenshot harness records the start instead).</summary>
    internal Action<ProcessStartInfo> StartGame { get; set; } = info => Process.Start(info);

    static void OpenUrl(string target)
    {
        if (target.Length > 0)
        {
            Process.Start(new ProcessStartInfo(target) { UseShellExecute = true });
        }
    }

    // ---- The screenshot harness's hooks -------------------------------------------------------------------------

    internal void SimulateInstallProgress(double percent, string status, IEnumerable<(LogLevel, string)> lines)
    {
        GoTo(Page.Install);
        Installing = true;
        Progress = percent;
        StatusText = status;
        Log.Clear();
        foreach (var (level, text) in lines)
        {
            AddLog(level, text);
        }
    }

    internal void SimulateDone(InstallRecord record)
    {
        Installing = false;
        CommandManager.InvalidateRequerySuggested();
        Record = record;
        BuildDoneNotes();
        GoTo(Page.Done);
    }

    internal Task RunInstallForHarness() => InstallAsync();

    internal void SimulateInstallError(string message)
    {
        GoTo(Page.Install);
        Installing = false;
        Log.Clear();
        AddLog(LogLevel.Info, "Could not read the release list (github.com: 404).");
        InstallError = message;
        AddLog(LogLevel.Error, message);
        Raise(nameof(ShowNext));
    }
}
