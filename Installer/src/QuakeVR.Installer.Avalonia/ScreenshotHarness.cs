using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Headless;
using Avalonia.Threading;
using Avalonia.VisualTree;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Skin;
using QuakeVR.Installer.ViewModels;
using QuakeVR.Installer.Views;
using Page = QuakeVR.Installer.ViewModels.Page;

namespace QuakeVR.Installer;

/// <summary>
/// The Avalonia window's pages rendered headless to PNG files (<c>QuakeVR-Setup --screenshots &lt;dir&gt;</c>): the
/// WPF harness's pages and names, with Avalonia.Headless and Skia (no display: Windows, Linux, WSL alike). Detection is
/// the real one (read-only); the Install and Done pages show a made-up run. It also checks the Statement page (the
/// switches start unanswered; Continue only with every claim YES) and writes report.txt. Exit code 0 when all pass.
/// </summary>
static class ScreenshotHarness
{
    static readonly StringBuilder Report = new();
    static int _failures;

    static string? FindSettingsManifest()
    {
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d is not null; d = d.Parent)
        {
            var p = Path.Combine(d.FullName, "quakevr", Core.Settings.SettingsManifest.FileName);
            if (File.Exists(p))
            {
                return p;
            }
        }
        return null;
    }

    public static int Run(StartupOptions options, string dir)
    {
        Directory.CreateDirectory(dir);
        FrameClock.Manual = true;
        var task = RunAsync(options, dir);
        // The headless dispatcher runs only when asked: pump it until the harness is done.
        while (!task.IsCompleted)
        {
            Dispatcher.UIThread.RunJobs();
            Thread.Sleep(1);
        }
        var code = task.Result == 0 && _failures == 0 ? 0 : 1;
        Report.AppendLine($"avalonia harness: {(code == 0 ? "PASS" : $"FAIL ({_failures} check(s))")}");
        File.WriteAllText(Path.Combine(dir, "report.txt"), Report.ToString());
        Console.WriteLine($"avalonia harness: {(code == 0 ? "PASS" : "FAIL")}, {Directory.GetFiles(dir, "*.png").Length} pages in {dir}");
        return code;
    }

    static void Check(bool ok, string what)
    {
        Report.AppendLine($"{(ok ? "ok" : "FAILED")}: {what}");
        _failures += ok ? 0 : 1;
    }

    static async Task<int> RunAsync(StartupOptions options, string dir)
    {
        Report.AppendLine($"host: {HostPlatform.Current} ({HostPlatform.CurrentRid}), Avalonia headless (Skia)");
        Report.AppendLine($"skin: {SkinTextures.Description}");
        var vm = new MainViewModel(HostPlatform.CreateProbe(), options) { HdTextures = options.Textures is not null || options.Feeds.Count > 0 };
        var window = new Window { Width = MainWindow.DefaultWidth, Height = MainWindow.DefaultHeight, Content = new ShellView(), DataContext = vm };
        window.Show();

        await vm.EnsureDetectedAsync();
        await vm.CheckFeedAsync();
        Report.AppendLine($"package: {vm.PackageSourceTitle}");

        Save(window, dir, "1-welcome.png");
        foreach (var f in window.GetVisualDescendants().OfType<FireView>())
        {
            Report.AppendLine($"fire: {f.Describe()}");
        }

        StatementCheck(vm, window);
        Save(window, dir, "1c-statement.png");

        vm.GoTo(Page.Detect);
        Save(window, dir, "2-detect.png");

        vm.GoTo(Page.Options);
        vm.InstallDir = vm.InstallDir; // validates
        Save(window, dir, "3-options.png");

        var record = new InstallRecord { Version = "2026-10-06 c131f4bf", RelightPending = true };
        vm.SimulateInstallProgress(64, "Copying HD textures",
        [
            (LogLevel.Info, OperatingSystem.IsWindows() ? @"Package: C:\Users\you\Downloads\QuakeVR.zip" : "Package: /home/you/.cache/QuakeVR-Installer/downloads/QuakeVR-linux-x64.zip"),
            (LogLevel.Info, "Quake VR: Unleashed 2026-10-06 c131f4bf: 2741 files, 196 MB"),
            (LogLevel.Info, "HD textures: 1054 files for id1, hipnotic, rogue"),
        ]);
        Save(window, dir, "4-install.png");
        vm.SimulatePreparing(57, "Relighting: 16 of 38 maps (0:12)",
        [
            (LogLevel.Info, "Preparing the game (1.0.0): its first maps loaded once, then every map relit, so the first start doesn't wait for them."),
            (LogLevel.Info, "Prepared the calibration room (2.2 s)."),
            (LogLevel.Info, "Prepared the hub (13.5 s)."),
        ]);
        Save(window, dir, "4-preparing.png");

        if (FindSettingsManifest() is { } settingsManifest)
        {
            var fake = Path.Combine(dir, "settings-install");
            Directory.CreateDirectory(Path.Combine(fake, "quakevr"));
            File.Copy(settingsManifest, Path.Combine(fake, "quakevr", Core.Settings.SettingsManifest.FileName), true);
            if (Core.Settings.SettingsSession.Open(fake) is { } session)
            {
                vm.SimulateSettings(session);
                Save(window, dir, "5-settings.png");
                var page = vm.SettingsPage!;
                foreach (var c in page.Categories)
                {
                    c.IsExpanded = true;
                }
                var rows = page.Categories.SelectMany(c => c.Subcategories).SelectMany(s => s.Settings).ToList();
                if (rows.FirstOrDefault(r => r.IsChoice && r.Choices.Count > 2) is { } choice)
                {
                    choice.Value = choice.Choices[2].Value;
                }
                if (rows.FirstOrDefault(r => r.IsSlider) is { } slider)
                {
                    slider.Number = (slider.SliderMin + slider.SliderMax) / 2;
                }
                Save(window, dir, "5-settings-changed.png");
                Report.AppendLine($"settings page: {session.Settings.Count} settings in {page.Categories.Count} categories; {page.ChangedText}");
            }
        }
        else
        {
            Report.AppendLine("settings page: no quakevr/settings_page.json above the harness (not rendered)");
        }

        vm.SimulateDone(new InstallRecord { Version = record.Version, QuakeDir = record.QuakeDir },
            new PreparationResult { Started = true, Finished = true, MapsOk = 3, RelightAsked = true, RelightDone = true, Relit = 38 });
        Save(window, dir, "5-done-prepared.png");
        vm.SimulateDone(record);
        Save(window, dir, "5-done.png");
        vm.GoTo(Page.Support);
        Save(window, dir, "6-support.png");

        // The window at its minimum size: nothing cut off that the WPF one shows (a look, not a check).
        window.Width = MainWindow.MinimumWidth;
        window.Height = MainWindow.MinimumHeight;
        vm.GoTo(Page.Welcome);
        Save(window, dir, "7c-welcome-minimum.png");
        window.Close();
        return 0;
    }

    /// <summary>The Statement page: as many switch halves as claims times two, none picked at first; Continue only
    /// with every claim YES (the WPF harness's StatementCheck).</summary>
    static void StatementCheck(MainViewModel vm, Window window)
    {
        vm.GoTo(Page.Statement);
        Settle(window);
        var page = window.GetVisualDescendants().OfType<StatementPage>().Single();
        var radios = page.GetVisualDescendants().OfType<RadioButton>().ToList();
        var claims = Core.AiStatement.Claims.Count;
        Check(radios.Count == claims * 2, $"statement: {radios.Count} switch halves for {claims} claims");
        Check(radios.All(r => r.IsChecked != true) && vm.Statement.Unanswered == claims, "statement: the switches start with neither YES nor NO");
        Check(!vm.CanGoNext, "statement: Continue disabled while unanswered");
        Save(window, Path.GetTempPath(), "qvr-avalonia-statement-scratch.png", write: false);
        // NO to the first, YES to the rest: still disabled; then YES to all: enabled.
        for (var i = 0; i < claims; ++i)
        {
            radios[i * 2 + (i == 0 ? 1 : 0)].IsChecked = true;
        }
        Settle(window);
        Check(!vm.CanGoNext, "statement: Continue disabled with a NO");
        radios[0].IsChecked = true;
        Settle(window);
        Check(vm.CanGoNext, "statement: Continue enabled with every claim YES");
        var next = window.GetVisualDescendants().OfType<Button>().FirstOrDefault(b => ReferenceEquals(b.Command, vm.NextCommand));
        Check(next is { IsEffectivelyEnabled: true }, "statement: the Continue button is enabled too (the command's CanExecute reached the button)");
    }

    static void Settle(Window window)
    {
        Dispatcher.UIThread.RunJobs();
        window.UpdateLayout();
        Dispatcher.UIThread.RunJobs();
    }

    static void Save(Window window, string dir, string name, bool write = true)
    {
        Settle(window);
        // A few frames of the flames and embers, as the WPF harness's first frame shows them.
        FrameClock.Step(1 / 60.0);
        Settle(window);
        using var frame = window.CaptureRenderedFrame();
        if (frame is null)
        {
            Check(false, $"{name}: no frame rendered");
            return;
        }
        if (write)
        {
#pragma warning disable CS0618 // the encoder-options overload: the defaults are what we want
            frame.Save(Path.Combine(dir, name));
#pragma warning restore CS0618
            Report.AppendLine($"page: {name} {frame.PixelSize.Width}x{frame.PixelSize.Height}");
        }
    }
}
