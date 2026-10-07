using System.IO;
using System.Windows;
using QuakeVR.Installer.Audio;
using QuakeVR.Installer.Core;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Skin;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

public partial class App
{
    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        var options = StartupOptions.Parse(e.Args);
        if (options.ReduceMotion)
        {
            FrameClock.OverrideReduceMotion(true);
        }
        SkinLoader.Disabled = options.NoQuakeLook;
        SkinLoader.ApplyGenerated();
        if (options.Screenshots is { } dir)
        {
            // QuakeVR-Setup --screenshots <dir> [--package <pkg> --target <dir> --shortcuts-dir <dir>]: renders every
            // page off screen to PNG files and exits (for reviews; no window is shown, no sound is played).
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            var code = await ScreenshotHarness.RunAsync(options, dir);
            Shutdown(code);
            return;
        }
        if (options.Uninstall && !StartUninstall(options))
        {
            return;
        }
        if (!options.Silent)
        {
            var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            UiSounds.Start(local.Length > 0 ? Path.Combine(local, "QuakeVR-Installer", "ui.json") : null);
        }
        var vm = new MainViewModel(new WindowsSystemProbe(), options);
        // The skin (and the sounds) come from the player's Quake as soon as one is known.
        vm.QuakeChosen += q => _ = SkinLoader.LoadFromQuakeAsync(q, sounds: !options.Silent);
        var window = new MainWindow { DataContext = vm };
        MainWindow = window;
        FrameClock.Attach(window);
        window.Show();
        // A quiet detection right away: the Your PC page is ready when it is reached, and the Welcome page already
        // wears the player's Quake.
        _ = vm.EnsureDetectedAsync();
        if (options.Uninstall)
        {
            _ = vm.RemoveFromCommandLineAsync();
        }
    }

    /// <summary>
    /// QuakeVR-Setup --uninstall [--target &lt;dir&gt;] [--quiet] (the Apps &amp; Features entry's UninstallString and
    /// QuietUninstallString): run from the install's own copy, it restarts from %TEMP% first. --quiet removes the install
    /// with no window (exit code 0, or 1 on an error); otherwise the window opens on the Remove dialogs. False: done
    /// (shut down).
    /// </summary>
    bool StartUninstall(StartupOptions options)
    {
        options.Target ??= SetupCopy.InstallOf(Environment.ProcessPath ?? "");
        if (options.Target is not { } target || !File.Exists(Path.Combine(target, InstallRecord.FileName)))
        {
            if (!options.Quiet)
            {
                MessageBox.Show($"No Quake VR: Unleashed install was found{(options.Target is null ? "" : " in " + options.Target)}.",
                    "Remove Quake VR: Unleashed", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            Shutdown(1);
            return false;
        }
        if (!options.FromTemp && Environment.ProcessPath is { } self && PathUtil.IsInside(self, target))
        {
            SetupRelaunch.Start(options.UninstallArguments(target));
            Shutdown(0);
            return false;
        }
        if (!options.Quiet)
        {
            return true;
        }
        ShutdownMode = ShutdownMode.OnExplicitShutdown;
        // (No window: what happened goes to %TEMP%\QuakeVR-Setup-uninstall.log.)
        var log = new List<string> { $"{DateTimeOffset.Now:u} uninstall {target}{(options.RegistryFile is { } rf ? " (test registry " + rf + ")" : "")}" };
        var code = 0;
        try
        {
            var r = Uninstaller.Uninstall(target, new UninstallOptions { Registry = MainViewModel.RegistryFor(options) },
                new Core.SyncProgress<InstallProgress>(p => { if (p.Log is not null) { log.Add(p.Log); } }));
            log.Add($"files removed {r.FilesRemoved}, shortcuts {r.ShortcutsRemoved}, changed kept {r.ChangedKept.Count}, in use kept {r.InUseKept.Count}, " +
                    $"player files left {r.PlayerFilesLeft.Count}, folder removed {r.FolderRemoved}, Apps & Features entry removed {r.EntryRemoved}");
        }
        catch (Exception e) when (e is InstallException or IOException or UnauthorizedAccessException or InvalidDataException)
        {
            log.Add("error: " + e.Message);
            code = 1;
        }
        try
        {
            File.AppendAllLines(Path.Combine(Path.GetTempPath(), "QuakeVR-Setup-uninstall.log"), log);
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
        }
        Shutdown(code);
        return false;
    }

    protected override void OnExit(ExitEventArgs e)
    {
        UiSounds.Stop();
        base.OnExit(e);
    }
}
