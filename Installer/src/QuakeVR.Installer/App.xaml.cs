using System.Windows;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

public partial class App
{
    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        var options = StartupOptions.Parse(e.Args);
        if (options.Screenshots is { } dir)
        {
            // QuakeVR-Setup --screenshots <dir> [--package <pkg> --target <dir> --shortcuts-dir <dir>]: renders every
            // page off screen to PNG files and exits (for reviews; no window is shown).
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            var code = await ScreenshotHarness.RunAsync(options, dir);
            Shutdown(code);
            return;
        }
        var window = new MainWindow { DataContext = new MainViewModel(new WindowsSystemProbe(), options) };
        MainWindow = window;
        window.Show();
    }
}
