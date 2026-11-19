using Avalonia;
using Avalonia.Headless;
using Avalonia.Media;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

static class Program
{
    // QuakeVR-Setup [the WPF window's options]: the window. --screenshots <dir>: every page rendered headless
    // (Avalonia.Headless with Skia: no window, no display needed, so it runs on a Linux server or in WSL as is).
    [STAThread]
    public static int Main(string[] args)
    {
        var options = StartupOptions.Parse(args);
        App.Options = options;
        if (options.Screenshots is { } dir)
        {
            BuildHeadless().SetupWithoutStarting();
            return ScreenshotHarness.Run(options, dir);
        }
        return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp() =>
        AppBuilder.Configure<App>().UsePlatformDetect().With(Fonts).LogToTrace();

    static AppBuilder BuildHeadless() =>
        AppBuilder.Configure<App>().UseSkia().UseHeadless(new AvaloniaHeadlessPlatformOptions { UseHeadlessDrawing = false }).With(Fonts);

    // The default face is the embedded Barlow: a Linux without fonts (a minimal distro, WSL) has no system default, and
    // Avalonia stops at the first text without one. Symbols the embedded fonts lack fall back to the system ones.
    static readonly FontManagerOptions Fonts = new() { DefaultFamilyName = "avares://QuakeVR-Setup/Fonts#Barlow" };
}
