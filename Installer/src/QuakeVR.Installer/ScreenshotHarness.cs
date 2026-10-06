using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.ViewModels;
using QuakeVR.Installer.Views;

namespace QuakeVR.Installer;

/// <summary>
/// Renders each wizard page off screen to a PNG (<c>QuakeVR-Setup --screenshots &lt;dir&gt;</c>), for reviews. Detection
/// is the real one (read-only). With <c>--package</c>, <c>--target</c> and <c>--shortcuts-dir</c> it also runs a real
/// install into those folders (never the real desktop or Start menu) and shows its log and result; without them the
/// Install and Done pages show a made-up run. Downloads only with <c>--feed</c> and <c>--downloads</c> (a local test server).
/// </summary>
static class ScreenshotHarness
{
    // The window's client area (MainWindow.xaml: 1040 x 720 with its frame).
    const int Width = 1024;
    const int Height = 680;

    public static async Task<int> RunAsync(StartupOptions options, string dir)
    {
        Directory.CreateDirectory(dir);
        var vm = new MainViewModel(new WindowsSystemProbe(), options) { HdTextures = options.Textures is not null || options.Feeds.Count > 0 };
        var view = new ShellView { DataContext = vm };

        await Save(view, Path.Combine(dir, "1-welcome.png"));

        vm.GoTo(Page.Detect);
        await vm.DetectAsync();
        await Save(view, Path.Combine(dir, "2-detect.png"));

        vm.GoTo(Page.Options);
        vm.InstallDir = vm.InstallDir; // validates
        await Save(view, Path.Combine(dir, "3-options.png"));

        InstallRecord record;
        List<(LogLevel, string)> lines;
        if ((options.Package is not null || (options.Feeds.Count > 0 && options.Downloads is not null)) && options.Target is not null && options.ShortcutsDir is not null)
        {
            await vm.RunInstallForHarness();
            if (vm.Record is null)
            {
                await Save(view, Path.Combine(dir, "4-install-error.png"));
                return 1;
            }
            record = vm.Record;
            lines = [.. vm.Log.Select(l => (l.Level, l.Text))];
        }
        else
        {
            record = new InstallRecord { Version = "2026-10-06 c131f4bf", RelightPending = true };
            lines =
            [
                (LogLevel.Info, @"Package: C:\Users\you\Downloads\QuakeVR.zip"),
                (LogLevel.Info, "Quake VR 2026-10-06 c131f4bf: 2741 files, 196 MB"),
                (LogLevel.Info, "HD textures: 1054 files for id1, hipnotic, rogue"),
            ];
        }
        // The Install page caught mid-way: the first lines of the log, the bar at 64%.
        vm.SimulateInstallProgress(64, "Copying HD textures", lines.TakeWhile(l => !l.Item2.Contains("copied and checked")));
        await Save(view, Path.Combine(dir, "4-install.png"));

        vm.SimulateDone(record);
        await Save(view, Path.Combine(dir, "5-done.png"));

        // A PC with nothing on it (an empty made-up machine): the problems and their fixes.
        var empty = new MainViewModel(new MemorySystemProbe(), new StartupOptions());
        var emptyView = new ShellView { DataContext = empty };
        empty.GoTo(Page.Detect);
        await empty.DetectAsync();
        await Save(emptyView, Path.Combine(dir, "2b-detect-nothing-found.png"));

        // Started again after the install: the Welcome page offers the update and the removal.
        if (vm.Record is not null)
        {
            var again = new MainViewModel(new WindowsSystemProbe(), options);
            await Save(new ShellView { DataContext = again }, Path.Combine(dir, "1b-welcome-installed.png"));
        }
        return 0;
    }

    static async Task Save(FrameworkElement view, string path)
    {
        // Let bindings and item containers settle, then lay out and render at the window's size.
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        view.Measure(new Size(Width, Height));
        view.Arrange(new Rect(0, 0, Width, Height));
        view.UpdateLayout();
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        view.UpdateLayout();
        var bitmap = new RenderTargetBitmap(Width, Height, 96, 96, PixelFormats.Pbgra32);
        bitmap.Render(view);
        var encoder = new PngBitmapEncoder();
        encoder.Frames.Add(BitmapFrame.Create(bitmap));
        await using var file = File.Create(path);
        encoder.Save(file);
    }
}
