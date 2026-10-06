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
using QuakeVR.Installer.Core.Shortcuts;

namespace QuakeVR.Installer.ViewModels;

public enum Page
{
    Welcome,
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
            }
        }
        o.Package = o.Package is { } pk ? PathUtil.TryNormalize(pk) ?? pk : null;
        o.Textures = o.Textures is { } tx ? PathUtil.TryNormalize(tx) ?? tx : null;
        o.ShortcutsDir = o.ShortcutsDir is { } sc ? PathUtil.TryNormalize(sc) ?? sc : null;
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
        _installDir = (options.Target is { } t ? PathUtil.TryNormalize(t) : null) ?? DefaultInstallDir;
        Steps = [new(1, "Welcome"), new(2, "Your PC"), new(3, "Options"), new(4, "Install"), new(5, "Play"), new(6, "Thanks")];

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
        OpenUrlCommand = new RelayCommand(p => OpenUrl(p as string ?? ""));
        OpenKofiCommand = new RelayCommand(() => OpenUrl(KofiUrl));
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
    public const string ProductName = "Quake VR: Unleashed";
    public SoundSettings Sound => UiSounds.Settings;

    // ---- Pages and navigation ------------------------------------------------------------------------------------

    public ObservableCollection<StepItem> Steps { get; }
    public string InstallerVersion => $"Installer {typeof(MainViewModel).Assembly.GetName().Version?.ToString(3)}";
    public string DefaultInstallDir { get; }

    public Page Page
    {
        get => _page;
        private set
        {
            if (Set(ref _page, value))
            {
                Raise(nameof(IsWelcome), nameof(IsDetect), nameof(IsOptions), nameof(IsInstall), nameof(IsDone), nameof(IsSupport), nameof(NextText), nameof(ShowBack), nameof(FooterHint));
            }
        }
    }

    public bool IsWelcome => Page == Page.Welcome;
    public bool IsDetect => Page == Page.Detect;
    public bool IsOptions => Page == Page.Options;
    public bool IsInstall => Page == Page.Install;
    public bool IsDone => Page == Page.Done;
    public bool IsSupport => Page == Page.Support;
    public bool ShowBack => Page is Page.Detect or Page.Options or Page.Support || (Page == Page.Install && !Installing);
    public bool ShowNext => !(Page == Page.Install);

    public string NextText => Page switch
    {
        Page.Welcome => _existing is null ? "Get started" : "Update",
        Page.Options => _existing is null ? "Install" : "Update",
        Page.Done => "Next",
        Page.Support => "Finish",
        _ => "Next",
    };

    public string FooterHint => Page switch
    {
        Page.Welcome => $"Nothing is changed until you press {(_existing is null ? "Install" : "Update")}.",
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
    public ICommand ShowCreditsCommand { get; }
    public ICommand RetryFeedCommand { get; }
    public ICommand PickPackageAndInstallCommand { get; }

    public void GoTo(Page page)
    {
        Page = page;
        for (var i = 0; i < Steps.Count; ++i)
        {
            Steps[i].State = i < (int)page ? StepState.Done : i == (int)page ? StepState.Current : StepState.Upcoming;
        }
        Raise(nameof(ShowNext));
    }

    bool CanNext() => Page switch
    {
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
            GoTo(Page == Page.Install ? Page.Options : Page == Page.Support ? Page.Done : Page - 1);
        }
    }

    // ---- Welcome: an existing install ----------------------------------------------------------------------------

    public bool HasExisting => _existing is not null;
    public string ExistingText => _existing is null ? "" :
        $"Quake VR: Unleashed {_existing.Version} is installed in {InstallDir}. Update it (your settings, saves and relit maps are kept), or remove it.";

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
        Raise(nameof(HasExisting), nameof(ExistingText), nameof(NextText), nameof(FooterHint));
    }

    async Task UninstallAsync()
    {
        if (_existing is null)
        {
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
            var result = await Task.Run(() => Uninstaller.Uninstall(dir, new UninstallOptions { RemoveHdTextures = textures }));
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
                    "In the game: VR Settings > OpenXR Runtime > Virtual Desktop (VDXR). Keep the Streamer's \"Emulate Index controllers\" off.")
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
            : new CheckItem(CheckStatus.Error, "Visual C++ runtime",
                r.VcRuntime.Installed is null ? "Not installed." : $"{r.VcRuntime.Installed} is too old (Quake VR needs {r.VcRuntime.Required} or later).",
                "Install Microsoft's Visual C++ Redistributable (x64), then check again.")
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
        _feed?.Components.GetValueOrDefault("hdtextures") is { } f ? $"{PathUtil.FormatSize(f.Size)} download" :
        FeedUnavailableForTextures ? "download unavailable" : "about 0.6 GB download";

    /// <summary>The HD textures would be downloaded, and the download cannot be reached: they are skipped unless the
    /// player picks the texture pack's zip.</summary>
    public bool FeedUnavailableForTextures => _options.Textures is null && _feedState == FeedState.Unavailable;

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
        FeedAvailable && _feed is { } feed ? $"{ProductName} {feed.Version}{(feed.Package is { } fp ? $", {PathUtil.FormatSize(fp.Size)}" : "")}, from GitHub (mirror: vittorioromeo.com), checked against its published SHA-256." :
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
            nameof(PackageSourceText), nameof(FeedErrorDetail), nameof(TexturesSizeText), nameof(FeedUnavailableForTextures), nameof(CanPickPackageAfterError));
        CommandManager.InvalidateRequerySuggested();
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
        CommandManager.InvalidateRequerySuggested();
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
            var texturesOnline = HdTextures && _options.Textures is null && !FeedUnavailableForTextures;
            var needDownload = _options.Package is null || texturesOnline;
            var installStart = needDownload ? 50.0 : 0.0;
            var package = _options.Package;
            if (package is null)
            {
                package = await DownloadAsync(http, ProductName, f => f.Package, 0, HdTextures ? 15 : 50, ct)
                          ?? throw new InstallException("No Quake VR package to install: the release list names none.");
            }
            string? textures = null;
            if (HdTextures)
            {
                textures = _options.Textures;
                if (textures is null && !texturesOnline)
                {
                    AddLog(LogLevel.Warning, "HD textures skipped: their download isn't available right now. Run Setup again later to add them, or pick the texture pack's zip on the Options page.");
                }
                else if (textures is null)
                {
                    try
                    {
                        textures = await DownloadAsync(http, "HD textures", f => f.Components.GetValueOrDefault("hdtextures"), 15, 50, ct);
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
            var plan = new InstallPlan
            {
                PackagePath = package,
                TargetDir = InstallDir,
                QuakeDir = quake,
                QuakeStore = SelectedQuake.Install.Store.ToString(),
                RelightOnFirstRun = Relight,
                HdTexturesZip = textures,
                OwnedPacks = owned,
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

    async Task<string?> DownloadAsync(HttpClient http, string what, Func<ReleaseFeed, FeedFile?> pick, double from, double to, CancellationToken ct)
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
        if (pick(_feed) is not { } file)
        {
            return null;
        }
        var dir = _options.Downloads ?? Path.Combine(_probe.GetFolder(KnownFolder.LocalAppData) ?? Path.GetTempPath(), "QuakeVR-Installer", "downloads");
        var dest = Path.Combine(dir, Path.GetFileName(file.File));
        AddLog(LogLevel.Info, $"Downloading {what} {_feed.Version} ({PathUtil.FormatSize(file.Size)}).");
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

    // ---- Done ---------------------------------------------------------------------------------------------------

    public InstallRecord? Record
    {
        get => _record;
        private set => Set(ref _record, value);
    }

    public ObservableCollection<CheckItem> DoneNotes { get; } = [];

    public string SteamLaunchOptions => SelectedQuake?.Install.BaseDir is { } q ? LaunchCommand.Arguments(q, InstallDir, LaunchVariant.Vr) : "";
    public string SteamExePath => Path.Combine(InstallDir, LaunchCommand.Exe);

    public void BuildDoneNotes()
    {
        DoneNotes.Clear();
        DoneNotes.Add(new CheckItem(CheckStatus.Info, "First start: VR Calibration",
            "The game takes you to the calibration room once: your height and body, then the main settings on its wall buttons."));
        if (Record?.RelightPending == true)
        {
            DoneNotes.Add(new CheckItem(CheckStatus.Info, "Relit maps",
                "Start with Play in VR below: the game relights every map with the HD textures (about a minute; the wrist gadget shows its progress). " +
                "Later: Graphics > Relighting."));
        }
        if (_report?.Vr.SuggestVdxr == true)
        {
            DoneNotes.Add(new CheckItem(CheckStatus.Info, "Virtual Desktop",
                "For the author's setup, pick VR Settings > OpenXR Runtime > Virtual Desktop (VDXR) in the game."));
        }
        Raise(nameof(SteamLaunchOptions), nameof(SteamExePath));
    }

    void Launch(LaunchVariant variant)
    {
        if (Record is null || SelectedQuake?.Install.BaseDir is not { } quake)
        {
            return;
        }
        string? extra = null;
        if (variant == LaunchVariant.Vr && Record.RelightPending)
        {
            extra = LaunchCommand.FirstRunRelight;
            Record.RelightPending = false;
            Record.Save(InstallDir);
        }
        Process.Start(new ProcessStartInfo(Path.Combine(InstallDir, LaunchCommand.Exe), LaunchCommand.Arguments(quake, InstallDir, variant, extra))
        {
            WorkingDirectory = InstallDir,
            UseShellExecute = false,
        });
    }

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
        AddLog(LogLevel.Info, "Could not read the release list (github.com: 404; vittorioromeo.com: 404).");
        InstallError = message;
        AddLog(LogLevel.Error, message);
        Raise(nameof(ShowNext));
    }
}
