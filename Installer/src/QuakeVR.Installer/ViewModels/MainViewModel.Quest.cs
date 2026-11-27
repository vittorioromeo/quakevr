using System.IO;
using System.Net.Http;
using System.Windows.Input;
using QuakeVR.Installer.Audio;
using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Detection;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Core.Prerequisites;
using QuakeVR.Installer.Core.Quest;

namespace QuakeVR.Installer.ViewModels;

/// <summary>
/// The Quest target (Core/Quest, Installer/README.md "Quest"): ticked on the Welcome page, beside or instead of this PC.
/// The PC install steps run as before (Quest only: into a staging folder, without shortcuts, Apps &amp; Features, the
/// VC++ runtime or the preparation run); then the Install page's Quest card finds adb (or fetches Google's
/// platform-tools), checks the headset, shows the steps and Check again until it is ready, and copies the APK and the
/// files from that install, with the same progress bar and log.
/// </summary>
public sealed partial class MainViewModel
{
    bool _targetPc = true;
    bool _targetQuest;
    string? _pcInstallDir;
    bool _questPending;
    bool _questActive;
    bool _questShowSteps;
    bool _questFinished;
    string _questStatus = "";
    string? _questApk;
    string? _questAdb;
    ICommand? _questCheck;
    ICommand? _questContinue;
    QuestResult? _questResult;

    string InstallerData => Path.Combine(_probe.GetFolder(KnownFolder.LocalAppData) ?? Path.GetTempPath(), "QuakeVR-Installer");
    string DownloadsDir => _options.Downloads ?? Path.Combine(InstallerData, "downloads");

    /// <summary>Install on this PC (PC VR and flatscreen). Off (Quest only): the install folder is the Quest's staging folder.</summary>
    public bool TargetPc
    {
        get => _targetPc;
        set
        {
            if (!Set(ref _targetPc, value))
            {
                return;
            }
            if (value)
            {
                InstallDir = _pcInstallDir ?? InstallDir;
            }
            else
            {
                _pcInstallDir = InstallDir;
                InstallDir = QuestTarget.StagingDir(InstallerData, _options.Sandbox);
            }
            Raise(nameof(NoTarget));
            CommandManager.InvalidateRequerySuggested();
        }
    }

    /// <summary>Install on a Quest too (after the PC steps), over USB.</summary>
    public bool TargetQuest
    {
        get => _targetQuest;
        set
        {
            if (Set(ref _targetQuest, value))
            {
                Raise(nameof(NoTarget));
                CommandManager.InvalidateRequerySuggested();
            }
        }
    }

    public bool NoTarget => !TargetPc && !TargetQuest;
    public bool QuestActive { get => _questActive; private set => Set(ref _questActive, value); }
    public bool QuestShowSteps { get => _questShowSteps; private set => Set(ref _questShowSteps, value); }
    public bool QuestFinished { get => _questFinished; private set => Set(ref _questFinished, value); }
    public string QuestStatus { get => _questStatus; private set => Set(ref _questStatus, value); }
    public IReadOnlyList<string> QuestSteps { get; } = [.. QuestTarget.SetupSteps.Select((s, i) => $"{i + 1}. {s}")];

    /// <summary>The Done page's Quest card (Quest ticked): installed (the app, the files, where to start it) or not (why,
    /// what to do). A Quest-only run's Done page has no PC Play buttons, Steam card nor PC notes.</summary>
    public bool QuestDoneOk => QuestTarget.Outcome(_questResult, QuestStatus).Ok;
    public string QuestDoneTitle => QuestTarget.Outcome(_questResult, QuestStatus).Title;
    public string QuestDoneText => QuestTarget.Outcome(_questResult, QuestStatus).Text;

    /// <summary>The screenshot harness: the Done page after a Quest run (a result) or one skipped (null, its message).</summary>
    public void ShowQuestDone(bool pc, QuestResult? result, string status)
    {
        TargetQuest = true;
        TargetPc = pc;
        _questResult = result;
        QuestStatus = status;
        QuestFinished = result is not null;
        BuildDoneNotes();
        RaiseQuestDone();
    }

    void RaiseQuestDone() => Raise(nameof(QuestDoneOk), nameof(QuestDoneTitle), nameof(QuestDoneText), nameof(DoneTitle), nameof(DoneSubtitle));

    public ICommand QuestCheckCommand => _questCheck ??= new RelayCommand(() => _ = QuestRunAsync(), () => !Installing);
    public ICommand QuestContinueCommand => _questContinue ??= new RelayCommand(() =>
    {
        QuestActive = false;
        BuildDoneNotes();
        RaiseQuestDone();
        if (TargetPc)
        {
            GoToSettingsOrDone();
        }
        else
        {
            GoTo(Page.Done);
        }
        UiSounds.Play(Sfx.InstallDone);
    }, () => !Installing);

    /// <summary>One attempt at the Quest: the APK and adb (found once, kept for Check again), the headset, the copy.</summary>
    async Task QuestRunAsync()
    {
        if (SelectedQuake?.Install.BaseDir is not { } quake)
        {
            return;
        }
        QuestActive = true;
        QuestShowSteps = false;
        QuestFinished = false;
        _questResult = null;
        QuestStatus = "Looking for the Quest…";
        InstallError = null;
        Installing = true;
        Progress = 0;
        _cts = new CancellationTokenSource();
        var ct = _cts.Token;
        var progress = new Progress<InstallProgress>(p =>
        {
            Progress = 100 * p.Fraction;
            StatusText = p.Status;
            if (p.Log is not null)
            {
                AddLog(p.Level, p.Log);
            }
        });
        using var http = Downloader.CreateClient();
        try
        {
            _questApk ??= _options.Apk ?? await CheckedLocalApkAsync(ct) ?? await DownloadApkAsync(http, ct);
            _questAdb ??= AdbLocator.Find(_options.Adb, DownloadsDir) ?? (_options.Offline
                ? throw new InstallException("adb was not found, and nothing is downloaded in offline mode: install Google's platform-tools, or pass --adb.")
                : await AdbLocator.FetchAsync(http, DownloadsDir, progress, ct, verifier: new AuthenticodeVerifier()));
            AddLog(LogLevel.Info, $"Quest: the app {_questApk}; adb {_questAdb}");
            var installDir = InstallDir;
            var probe = _probe;
            var result = await Task.Run(() =>
            {
                var files = QuestFiles.Plan(installDir, quake, ExpansionDetector.StoreRoots(new QuakeDetector(probe)));
                return new QuestInstaller(new ProcessAdb(_questAdb)).Install(new QuestPlan { ApkPath = _questApk, Files = files }, progress, ct);
            }, ct);
            _questResult = result;
            QuestStatus = QuestTarget.Outcome(result, null).Text;
            QuestFinished = true;
            UiSounds.Play(Sfx.InstallDone);
        }
        catch (QuestDeviceException e)
        {
            QuestStatus = e.Message;
            QuestShowSteps = true;
            AddLog(LogLevel.Warning, e.Message);
        }
        catch (OperationCanceledException)
        {
            QuestStatus = "Cancelled. What was copied to the Quest is kept, and skipped next time.";
            QuestShowSteps = true;
        }
        catch (Exception e) when (e is InstallException or IOException or UnauthorizedAccessException or InvalidDataException or HttpRequestException
                                      or System.ComponentModel.Win32Exception)
        {
            QuestStatus = e.Message;
            QuestShowSteps = true;
            AddLog(LogLevel.Error, e.Message);
            UiSounds.Play(Sfx.Error);
        }
        finally
        {
            Installing = false;
            CommandManager.InvalidateRequerySuggested();
        }
    }

    /// <summary>QuakeVR.apk beside Setup, when it is the release's own (QuestTarget.LocalApkMatches against the release
    /// list read at start); null when there is none, or it is another build (the release's is downloaded instead).</summary>
    async Task<string?> CheckedLocalApkAsync(CancellationToken ct)
    {
        if (QuestTarget.LocalApk(Environment.ProcessPath) is not { } apk)
        {
            return null;
        }
        var expected = _feed?.Components.GetValueOrDefault(QuestTarget.FeedComponent);
        if (await Task.Run(() => QuestTarget.LocalApkMatches(apk, expected), ct) == false)
        {
            AddLog(LogLevel.Warning, $"Quest: {QuestTarget.ApkFileName} beside Setup is not the release's ({ReleaseVersion.Parse(_feed!.Version).Short}): not used, downloading the release's instead.");
            return null;
        }
        return apk;
    }

    /// <summary>The release list's quest component (make_release.py --apk).</summary>
    async Task<string> DownloadApkAsync(HttpClient http, CancellationToken ct)
    {
        var feed = await GetFeedAsync(http, "the Quest app", ct);
        if (_settings.Component(feed, QuestTarget.FeedComponent) is not { } c)
        {
            throw new InstallException($"This release has no Quest app ({QuestTarget.ApkFileName}). Put a {QuestTarget.ApkFileName} beside Setup, or start Setup with --apk <file>.");
        }
        return await DownloadFileAsync(http, "the Quest app", $"the Quest app {feed.Version}", c.File, 0, 10, ct);
    }
}
