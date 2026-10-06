using System.IO;
using System.Windows;
using QuakeVR.Installer.Audio;
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
    }

    protected override void OnExit(ExitEventArgs e)
    {
        UiSounds.Stop();
        base.OnExit(e);
    }
}
