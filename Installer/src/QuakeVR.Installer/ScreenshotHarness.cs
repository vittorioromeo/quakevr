using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Windows;
using UserControl = System.Windows.Controls.UserControl;
using Grid = System.Windows.Controls.Grid;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using QuakeVR.Installer.Audio;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Audio;
using QuakeVR.Installer.Core.Packaging;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Skin;
using QuakeVR.Installer.ViewModels;
using QuakeVR.Installer.Views;

namespace QuakeVR.Installer;

/// <summary>
/// Renders each wizard page off screen to a PNG (<c>QuakeVR-Setup --screenshots &lt;dir&gt;</c>), for reviews. Detection
/// is the real one (read-only), and so is the skin: the player's Quake's textures when one is found (unless
/// <c>--no-quake-look</c>), the generated ones otherwise. With <c>--package</c>, <c>--target</c> and
/// <c>--shortcuts-dir</c> it also runs a real install into those folders (never the real desktop or Start menu) and
/// shows its log and result; without them the Install and Done pages show a made-up run. <c>--offline</c> never asks
/// the network. <c>--extras</c> also writes a strip of flame frames, a sheet of Quake's textures and report.txt
/// (what was loaded, what a frame of the flames costs).
/// </summary>
static class ScreenshotHarness
{
    // The window's client area at its default size (MainWindow: the size with its frame).
    const int Width = (int)(MainWindow.DefaultWidth - MainWindow.FrameWidth);
    const int Height = (int)(MainWindow.DefaultHeight - MainWindow.FrameHeight);

    // The fit check (fit.txt): each page laid out in the client area the window gets on these screens (the window is
    // never taller than the work area, in DIPs: the screen less a 48 px taskbar at 100%, scaled), and whatever would
    // need scrolling there. Widths: the default fits the first three; the last is the window at its minimum size.
    const int MinWidth = (int)(MainWindow.MinimumWidth - MainWindow.FrameWidth);
    const int MinHeight = (int)(MainWindow.MinimumHeight - MainWindow.FrameHeight);
    static readonly (string Screen, int Width, int Height)[] FitScreens =
    [
        ("default", Width, Height),
        ("1366x768 at 100%", Width, Math.Min(Height, 768 - 48 - (int)MainWindow.FrameHeight)),
        ("1920x1080 at 150%", Width, Math.Min(Height, (1080 - 72) * 2 / 3 - (int)MainWindow.FrameHeight)),
        ("minimum window", MinWidth, MinHeight),
    ];
    static readonly StringBuilder FitReport = new();

    public static async Task<int> RunAsync(StartupOptions options, string dir)
    {
        Directory.CreateDirectory(dir);
        var report = new StringBuilder();
        var vm = new MainViewModel(new WindowsSystemProbe(), options) { HdTextures = options.Textures is not null || options.Feeds.Count > 0 };
        var view = new ShellView { DataContext = vm };

        // As in the window: a quiet detection first, and the skin from the Quake it finds.
        await vm.EnsureDetectedAsync();
        if (vm.SelectedQuake is { } q && !options.NoQuakeLook)
        {
            await SkinLoader.LoadFromQuakeAsync(q.Install, sounds: false);
        }
        report.AppendLine($"skin: {SkinResources.Current.Description}");
        await vm.CheckFeedAsync();
        report.AppendLine($"package: {vm.PackageSourceTitle}");

        await Save(view, Path.Combine(dir, "1-welcome.png"));
        foreach (var f in FindAll<FireView>(view))
        {
            report.AppendLine($"fire: {f.Describe()}");
        }

        if (await StatementCheck(vm, view, dir, report) is { } failed)
        {
            report.AppendLine($"statement check FAILED: {failed}");
            await File.WriteAllTextAsync(Path.Combine(dir, "report.txt"), report.ToString());
            Console.Error.WriteLine($"statement check FAILED: {failed}");
            return 1;
        }

        vm.GoTo(Page.Detect);
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
                report.AppendLine($"install error: {vm.InstallError}");
                await File.WriteAllTextAsync(Path.Combine(dir, "report.txt"), report.ToString());
                return 1;
            }
            record = vm.Record;
            lines = [.. vm.Log.Select(l => (l.Level, l.Text))];
            report.AppendLine($"installed: {record.Version} into {options.Target}");
        }
        else
        {
            record = new InstallRecord { Version = "2026-10-06 c131f4bf", RelightPending = true };
            lines =
            [
                (LogLevel.Info, @"Package: C:\Users\you\Downloads\QuakeVR.zip"),
                (LogLevel.Info, "Quake VR: Unleashed 2026-10-06 c131f4bf: 2741 files, 196 MB"),
                (LogLevel.Info, "HD textures: 1054 files for id1, hipnotic, rogue"),
            ];
        }
        // The Install page caught mid-way: the first lines of the log, the bar at 64%.
        vm.SimulateInstallProgress(64, "Copying HD textures", lines.TakeWhile(l => !l.Item2.Contains("copied and checked")));
        await Save(view, Path.Combine(dir, "4-install.png"));

        vm.SimulateDone(record);
        await Save(view, Path.Combine(dir, "5-done.png"));
        if (await PlayMuteCheck(vm, view, report) is { } muteFailed)
        {
            report.AppendLine($"play mute check FAILED: {muteFailed}");
            await File.WriteAllTextAsync(Path.Combine(dir, "report.txt"), report.ToString());
            Console.Error.WriteLine($"play mute check FAILED: {muteFailed}");
            return 1;
        }
        vm.GoTo(Page.Support);
        await Save(view, Path.Combine(dir, "6-support.png"));

        // A PC with nothing on it (an empty made-up machine): the problems and their fixes.
        var empty = new MainViewModel(new MemorySystemProbe(), new StartupOptions { Offline = true });
        var emptyView = new ShellView { DataContext = empty };
        empty.GoTo(Page.Detect);
        await empty.DetectAsync();
        await Save(emptyView, Path.Combine(dir, "2b-detect-nothing-found.png"));

        // No package beside the installer and no release online: the friendly way out, then the install's own error.
        if (options.Package is null)
        {
            var offline = new MainViewModel(new WindowsSystemProbe(), new StartupOptions { Offline = true, Target = options.Target }) { HdTextures = true };
            var offlineView = new ShellView { DataContext = offline };
            await offline.EnsureDetectedAsync();
            await offline.CheckFeedAsync();
            offline.GoTo(Page.Options);
            await Save(offlineView, Path.Combine(dir, "3b-options-no-release-online.png"));
            offline.SimulateInstallError("The online release couldn't be reached: none may be published yet, or this PC is offline. " +
                                         "Pick a local Quake VR: Unleashed package (QuakeVR.zip) to install without the internet, or try again later.");
            await Save(offlineView, Path.Combine(dir, "4b-install-no-release-online.png"));
        }

        // Started again after the install: the Welcome page offers the update and the removal.
        if (vm.Record is not null)
        {
            var again = new MainViewModel(new WindowsSystemProbe(), options);
            var againView = new ShellView { DataContext = again };
            await Save(againView, Path.Combine(dir, "1b-welcome-installed.png"));
            // The Update screen's secondary choice opened: Install again from scratch, its two choices ticked.
            again.ToggleReinstallCommand.Execute(null);
            again.ResetSettings = again.RemoveSaves = true;
            await Save(againView, Path.Combine(dir, "1f-welcome-install-again.png"));
            report.AppendLine($"update screen: {again.ExistingTitle} / {again.ExistingText} / button {again.NextText}");
        }

        // High scaling: the Welcome page at 150% (the sidebar's logos, crisp), and the Statement page in the window a
        // 1080p screen at 150% leaves room for (the page and the sidebar scroll).
        vm.GoTo(Page.Welcome);
        await Save(view, Path.Combine(dir, "7-welcome-150pct.png"), scale: 1.5);
        vm.GoTo(Page.Statement);
        await Save(view, Path.Combine(dir, "7b-statement-150pct-1080p.png"), scale: 1.5, height: FitScreens[2].Height);
        // The Statement page in the window at its minimum size (the paragraphs wrap beside the photo, narrower).
        await Save(view, Path.Combine(dir, "7c-statement-minimum.png"), width: MinWidth, height: MinHeight);
        await File.WriteAllTextAsync(Path.Combine(dir, "fit.txt"), FitReport.ToString());
        Console.WriteLine(FitReport.ToString().TrimEnd());

        if (options.Extras)
        {
            await FlameStrip(Path.Combine(dir, "flames-strip.png"), report);
            if (vm.SelectedQuake?.Install is { } quake && QuakeFileSystem.Open(quake) is { } fs)
            {
                using (fs)
                {
                    await TextureSheet(fs, Path.Combine(dir, "quake-textures.png"));
                    report.AppendLine($"sounds: {SoundCheck(fs)}");
                    var quakeSounds = UiSounds.QuakeClips(fs, Synth.All());
                    report.AppendLine($"offline mix, Quake's sounds: {MixCheck(quakeSounds.Clips, quakeSounds.Ambience ?? Synth.Crackle(), Path.Combine(dir, "ui-mix-quake.wav"))}");
                    // The window's sound path, muted: the device, the class handlers, Quake's clips, a click.
                    UiSounds.Settings.Muted = true;
                    UiSounds.Start(null);
                    UiSounds.UseQuake(fs);
                    var button = new System.Windows.Controls.Button();
                    button.RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Primitives.ButtonBase.ClickEvent, button));
                    report.AppendLine($"ui sounds: started muted, {UiSounds.Settings.Source}");
                    UiSounds.Stop();
                }
            }
            report.AppendLine($"offline mix, synthesized sounds: {MixCheck(Synth.All(), Synth.Crackle(), Path.Combine(dir, "ui-mix-synth.wav"))}");
            report.AppendLine(SoundEngineCheck());
            report.AppendLine(await LiveWindowCheck(options));
            report.AppendLine($"synthesized sounds: {string.Join(", ", Synth.All().Select(kv => $"{kv.Key} {kv.Value.Samples.Length * 1000 / SoundEngine.Rate} ms"))}");
            await File.WriteAllTextAsync(Path.Combine(dir, "report.txt"), report.ToString());
        }
        return 0;
    }

    /// <summary>Eight frames of the sidebar's flames, 1/15 s apart, side by side (the animation, on paper), and what a
    /// frame costs.</summary>
    static async Task FlameStrip(string path, StringBuilder report)
    {
        const int w = 240, h = 214, frames = 8;
        var fire = new FireView { Heat = 0.62, CellSize = 4, EmberCount = 14, FireOpacity = 0.85, Glow = 0.35 };
        var host = new Grid { Background = new SolidColorBrush(Color.FromRgb(0x10, 0x0D, 0x0B)), Width = w, Height = h, Children = { fire } };
        Layout(host, w, h);
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        var strip = new DrawingVisual();
        using (var dc = strip.RenderOpen())
        {
            for (var i = 0; i < frames; ++i)
            {
                fire.Advance(1.0 / 15);
                fire.InvalidateVisual();
                host.UpdateLayout();
                var frame = new RenderTargetBitmap(w, h, 96, 96, PixelFormats.Pbgra32);
                frame.Render(host);
                dc.DrawImage(frame, new Rect(i * w, 0, w, h));
            }
        }
        SavePng(strip, frames * w, h, path);
        // The cost: 600 frames at 60 Hz, simulation and painting (the drawing itself is on the GPU).
        var sw = Stopwatch.StartNew();
        for (var i = 0; i < 600; ++i)
        {
            fire.Advance(1.0 / 60);
        }
        report.AppendLine(string.Create(CultureInfo.InvariantCulture, $"flames: {sw.Elapsed.TotalMilliseconds / 600:0.000} ms per frame (sidebar, {w}x{h})"));
        var lava = new LavaView { Width = 600, Height = 20 };
        Layout(lava, 600, 20);
        sw.Restart();
        for (var i = 0; i < 600; ++i)
        {
            lava.Advance(1.0 / 60);
        }
        report.AppendLine(string.Create(CultureInfo.InvariantCulture, $"lava: {sw.Elapsed.TotalMilliseconds / 600:0.000} ms per frame"));
    }

    /// <summary>Every texture the skin can pick from, labelled (to choose them).</summary>
    static async Task TextureSheet(QuakeFileSystem fs, string path)
    {
        var all = SkinAssets.AllTextures(fs).Where(t => t.Image.PixelWidth <= 128 && t.Image.PixelHeight <= 128).ToList();
        const int cell = 96, cols = 14;
        var rows = (all.Count + cols - 1) / cols;
        var v = new DrawingVisual();
        var typeface = new Typeface("Segoe UI");
        using (var dc = v.RenderOpen())
        {
            dc.DrawRectangle(Brushes.Black, null, new Rect(0, 0, cols * cell, rows * (cell + 14)));
            for (var i = 0; i < all.Count; ++i)
            {
                var (name, image) = all[i];
                var x = i % cols * cell;
                var y = i / cols * (cell + 14);
                dc.DrawImage(image, new Rect(x + 2, y + 2, cell - 4, cell - 4));
                dc.DrawText(new FormattedText(name, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, typeface, 11, Brushes.White, 1),
                    new Point(x + 2, y + cell - 1));
            }
        }
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        SavePng(v, cols * cell, rows * (cell + 14), path);
    }

    /// <summary>Which of Quake's sounds the installer would use, read and decoded (nothing is played).</summary>
    static string SoundCheck(QuakeFileSystem fs)
    {
        var names = new[] { "sound/misc/menu1.wav", "sound/misc/menu2.wav", "sound/misc/menu3.wav", "sound/weapons/pkup.wav",
            "sound/misc/secret.wav", "sound/misc/talk.wav", "sound/items/health1.wav", "sound/ambience/fire1.wav" };
        return string.Join(", ", names.Select(n => fs.Read(n) is { } d && QuakeFormats.ReadWav(d) is { } w
            ? $"{Path.GetFileName(n)} {w.SampleRate} Hz {w.Samples.Length * 1000 / w.SampleRate} ms"
            : $"{Path.GetFileName(n)} missing"));
    }

    /// <summary>The mixer on the real device, inaudible (a synthesized click at a ten-thousandth of its volume).</summary>
    static string SoundEngineCheck()
    {
        using var engine = new SoundEngine();
        if (!engine.Available)
        {
            return "sound engine: no audio device (the installer stays silent)";
        }
        engine.Play(Synth.All()[Sfx.Select], 0.0001f);
        Thread.Sleep(400);
        var during = engine.BuffersWritten;
        Thread.Sleep(600);
        var after = engine.BuffersWritten;
        return $"sound engine: {during} buffers while a 220 ms sound played, {after - during} more in the next 600 ms (idle: nothing sent)";
    }

    /// <summary>The wave-out rings the live window check compares (frames per buffer, buffers): the old one (it
    /// crackled: INSTALLER.md, "Sounds"), one just too short, the shortest that holds, and the installer's.</summary>
    static readonly (int Frames, int Count)[] DeviceRings = [(512, 4), (441, 5), (441, 6), (SoundEngine.BufferFrames, SoundEngine.BufferCount)];

    /// <summary>A scripted session mixed offline (no device): the fire, clicks, a burst of typing (voices of one
    /// sound stolen), the install's sounds, a mute and back; written to <paramref name="wav"/> and measured.</summary>
    static string MixCheck(Dictionary<Sfx, SoundClip> clips, SoundClip ambience, string wav)
    {
        var mixer = new SoundMixer();
        var rng = new Random(7);
        var events = new List<(double At, Action Do)>
        {
            (0, () => mixer.SetLoop(ambience, UiSounds.AmbienceLevel, 0.5)),
            (7.5, () => mixer.MasterVolume = 0),
            (8.0, () => mixer.MasterVolume = 1),
        };
        void At(double t, Sfx kind) => events.Add((t, () =>
        {
            var (volume, pitch) = UiSounds.VolumeAndPitch(kind, rng);
            mixer.Play(clips[kind], volume, pitch);
        }));
        At(0.6, Sfx.Select);
        At(0.9, Sfx.Click);
        At(1.1, Sfx.Back);
        At(1.3, Sfx.Toggle);
        for (var t = 1.6; t < 3.0; t += 0.055)
        {
            At(t, Sfx.Type);
        }
        for (var t = 3.2; t < 4.0; t += 0.1)
        {
            At(t, Sfx.Click);
            At(t + 0.05, Sfx.Toggle);
        }
        At(4.2, Sfx.InstallStart);
        At(5.0, Sfx.InstallDone);
        At(6.0, Sfx.Error);
        At(6.5, Sfx.Support);
        At(7.2, Sfx.Select);
        events.Sort((a, b) => a.At.CompareTo(b.At));
        const int block = 256;
        var pcm = new short[(int)(9.0 * SoundMixer.Rate) / block * block];
        var next = 0;
        for (var f = 0; f < pcm.Length; f += block)
        {
            while (next < events.Count && events[next].At * SoundMixer.Rate <= f)
            {
                events[next++].Do();
            }
            mixer.Mix(pcm.AsSpan(f, block));
        }
        SoundMixer.WriteWav(wav, pcm);
        var a = SoundMixer.Analyze(pcm);
        // The loudest a source alone moves from one sample to the next (at its volume): jumps above it are the mixer's.
        var source = clips.Max(kv => MaxJump(kv.Value.Samples) * UiSounds.VolumeAndPitch(kv.Key, new Random(0)).Volume);
        var dc = clips.Values.Max(c => Math.Abs(c.Samples.Average()));
        var ends = clips.Values.Max(c => Math.Max(Math.Abs(c.Samples[0]), Math.Abs(c.Samples[^1])));
        return string.Create(CultureInfo.InvariantCulture,
            $"{Path.GetFileName(wav)}: sources' DC up to {dc:0.0000}, first/last samples up to {ends:0.000}; the mixer's own steps: largest {mixer.MaxEdge:0.0000}, {mixer.Edges} over 0.01; peak {a.Peak:0.000}, {a.FullScale} samples at full scale, largest jump {a.MaxJump:0.0000} ({a.Jumps} over 0.05; the sources' own largest {source:0.0000}), largest second difference {a.MaxCurve:0.0000} ({a.Curves} over 0.05)");
    }

    static double MaxJump(float[] s)
    {
        double m = 0;
        for (var i = 1; i < s.Length; ++i)
        {
            m = Math.Max(m, Math.Abs(s[i] - s[i - 1]));
        }
        return m;
    }

    /// <summary>The real window, shown off screen for two seconds: how often the frame clock ticks and what the
    /// process costs while animating, then with reduced motion.</summary>
    static async Task<string> LiveWindowCheck(StartupOptions options)
    {
        var windowsAnimations = SystemParameters.ClientAreaAnimation;
        FrameClock.ForceForeground = true;
        FrameClock.OverrideReduceMotion(false); // Measure the animated window even where Windows' animations are off.
        var vm = new MainViewModel(new WindowsSystemProbe(), new StartupOptions { Offline = true, Screenshots = "x" });
        var window = new MainWindow { DataContext = vm, Left = -20000, Top = -20000, ShowInTaskbar = false, ShowActivated = true, WindowStartupLocation = WindowStartupLocation.Manual };
        FrameClock.Attach(window);
        window.Show();
        window.Activate();
        await Task.Delay(500);
        var size = string.Create(CultureInfo.InvariantCulture,
            $"window {window.ActualWidth:0}x{window.ActualHeight:0} (min {window.MinWidth:0}x{window.MinHeight:0}; work area {SystemParameters.WorkArea.Width:0}x{SystemParameters.WorkArea.Height:0})");
        var proc = Process.GetCurrentProcess();
        double Measure(out long ticks)
        {
            proc.Refresh();
            var cpu0 = proc.TotalProcessorTime;
            var t0 = FrameClock.Ticks;
            var sw = Stopwatch.StartNew();
            var frame = new DispatcherFrame();
            var timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
            timer.Tick += (_, _) => { timer.Stop(); frame.Continue = false; };
            timer.Start();
            Dispatcher.PushFrame(frame);
            proc.Refresh();
            ticks = FrameClock.Ticks - t0;
            return (proc.TotalProcessorTime - cpu0).TotalMilliseconds / sw.Elapsed.TotalMilliseconds * 100;
        }
        var active = window.IsActive;
        // The device under the window's load: rings of buffers mixing the fire and a click every 120 ms at a
        // thousandth of their volume (inaudible), counting the times the device ran dry.
        var rings = DeviceRings.Select(r => new SoundEngine(r.Frames, r.Count) { MasterVolume = 0.001f }).ToList();
        var clips = Synth.All();
        foreach (var e in rings)
        {
            e.SetLoop(Synth.Crackle(), 0.1f, 0.01);
        }
        var clicker = new Timer(_ =>
        {
            foreach (var e in rings)
            {
                e.Play(clips[Sfx.Click], 0.3f);
            }
        }, null, 0, 120);
        var ringClock = Stopwatch.StartNew();
        Measure(out _); // Warm up: the first frames compile shaders and fill caches.
        var busy = Measure(out var ticks);
        var governed = Measure(out var governedTicks); // After the governor's first look.
        var governor = string.Create(CultureInfo.InvariantCulture,
            $"governor: {(FrameClock.Throttled ? "throttled to 30 Hz" : "kept 60 Hz")} (it saw {FrameClock.LastCpuPercent:0.0}%), then {governedTicks / 2.0:0} ticks/s at {governed:0.0}%;");
        var fires = FindAll<FireView>(window).ToList();
        var detail = new StringBuilder();
        foreach (var f in fires)
        {
            f.Visibility = Visibility.Collapsed;
            var c = Measure(out _);
            detail.Append(string.Create(CultureInfo.InvariantCulture, $" without {f.ActualWidth:0}x{f.ActualHeight:0}: {c:0.0}%;"));
            f.Visibility = Visibility.Visible;
        }
        foreach (var f in fires)
        {
            f.Visibility = Visibility.Collapsed;
        }
        foreach (var f in fires)
        {
            f.Visibility = Visibility.Visible;
            var only = Measure(out _);
            detail.Append(string.Create(CultureInfo.InvariantCulture, $" only {f.ActualWidth:0}x{f.ActualHeight:0}: {only:0.0}%;"));
            f.Visibility = Visibility.Collapsed;
        }
        var none = Measure(out _);
        detail.Append(string.Create(CultureInfo.InvariantCulture, $" no flames at all: {none:0.0}%"));
        foreach (var f in fires)
        {
            f.Visibility = Visibility.Visible;
        }
        FrameClock.OverrideReduceMotion(true);
        var still = Measure(out var stillTicks);
        FrameClock.OverrideReduceMotion(options.ReduceMotion ? true : null);
        window.Close();
        clicker.Dispose();
        var ringSeconds = ringClock.Elapsed.TotalSeconds;
        var ringReport = string.Join("; ", rings.Zip(DeviceRings, (e, r) => string.Create(CultureInfo.InvariantCulture,
            $"{r.Count}x{r.Frames} ({r.Count * r.Frames * 1000.0 / SoundEngine.Rate:0} ms queued): {e.Underruns} underruns, fewest queued {(e.MinQueued == int.MaxValue ? "-" : e.MinQueued.ToString(CultureInfo.InvariantCulture))}, {e.TimeoutWakes} timeouts, played {e.PlayedSeconds:0.00} s of audio in {e.StreamedSeconds:0.00} s")));
        foreach (var e in rings)
        {
            e.Dispose();
        }
        return string.Create(CultureInfo.InvariantCulture,
            $"device rings over {ringSeconds:0} s of the live window: {(rings.All(e => e.Available) ? ringReport : "no audio device")}; Windows animation effects {(windowsAnimations ? "on" : "off")}; {(FrameClock.Hardware ? "GPU" : "software")} rendering; live window (active {active}, {size}): {ticks / 2.0:0} ticks/s, process CPU {busy:0.0}% of one core; {governor} reduced motion: {stillTicks / 2.0:0} ticks/s, CPU {still:0.0}%;{detail}");
    }

    static void Layout(FrameworkElement e, double w, double h)
    {
        e.Measure(new Size(w, h));
        e.Arrange(new Rect(0, 0, w, h));
        e.UpdateLayout();
    }

    static void SavePng(Visual v, int w, int h, string path, double scale = 1)
    {
        var bitmap = new RenderTargetBitmap((int)Math.Round(w * scale), (int)Math.Round(h * scale), 96 * scale, 96 * scale, PixelFormats.Pbgra32);
        bitmap.Render(v);
        var encoder = new PngBitmapEncoder();
        encoder.Frames.Add(BitmapFrame.Create(bitmap));
        using var file = File.Create(path);
        encoder.Save(file);
    }

    static async Task Save(FrameworkElement view, string path, double scale = 1, int height = Height, int width = Width)
    {
        // Let bindings and item containers settle, then lay out and render at the window's size.
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        if (scale == 1 && height == Height && width == Width)
        {
            await FitCheck(view, Path.GetFileNameWithoutExtension(path));
        }
        Layout(view, width, height);
        await Task.Delay(350); // The controls' short animations (a check mark popping in) end.
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        view.UpdateLayout();
        // Pages fade in when shown: settle that at once off screen.
        foreach (var page in FindAll<UserControl>(view))
        {
            page.BeginAnimation(UIElement.OpacityProperty, null);
            page.Opacity = 1;
            page.RenderTransform = Transform.Identity;
        }
        view.UpdateLayout();
        SavePng(view, width, height, path, scale);
    }

    /// <summary>The page laid out on each of <see cref="FitScreens"/>: what would scroll there, and by how much (the
    /// shown page's scrolling parts and the sidebar's).</summary>
    static async Task FitCheck(FrameworkElement view, string name)
    {
        var line = new StringBuilder($"{name}:");
        foreach (var (screen, width, height) in FitScreens)
        {
            Layout(view, width, height);
            await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
            view.UpdateLayout();
            var over = FindAll<System.Windows.Controls.ScrollViewer>(view)
                .Where(sv => Shown(sv, view) && sv.ExtentHeight > sv.ViewportHeight + 0.5)
                .Select(sv => string.Create(CultureInfo.InvariantCulture,
                    $"{(sv.Name is { Length: > 0 } n ? n : Owner(sv)?.GetType().Name ?? "?")} +{sv.ExtentHeight - sv.ViewportHeight:0}px"))
                .ToList();
            line.Append($" {screen} ({width}x{height}): {(over.Count == 0 ? "fits" : "scrolls " + string.Join(", ", over))};");
        }
        FitReport.AppendLine(line.ToString());
    }

    static bool Shown(DependencyObject d, DependencyObject root)
    {
        for (; d is not null && d != root; d = VisualTreeHelper.GetParent(d))
        {
            if (d is UIElement { Visibility: not Visibility.Visible })
            {
                return false;
            }
        }
        return true;
    }

    static UserControl? Owner(DependencyObject d)
    {
        for (d = VisualTreeHelper.GetParent(d); d is not null; d = VisualTreeHelper.GetParent(d))
        {
            if (d is UserControl u)
            {
                return u;
            }
        }
        return null;
    }

    /// <summary>
    /// The Statement page, unanswered, mixed and all YES, saved to PNG and driven through its real controls (the radio
    /// buttons' automation peers, as a screen reader or a click would): the switches start with neither YES nor NO, a
    /// picked one cannot go back to neither, and the footer's Continue is enabled only with YES to all three. Returns
    /// what went wrong, or null.
    /// </summary>
    /// <summary>The Play page's two Play buttons, pressed through their automation peers with the game's start
    /// recorded instead of run: each mutes the installer (the speaker button's flag), the fire's loop fading out over
    /// <see cref="UiSounds.GameStartFadeSeconds"/> on an offline engine (no device: nothing heard), then silence. Null
    /// when it holds (or when no Quake is found: Play does nothing then).</summary>
    static async Task<string?> PlayMuteCheck(MainViewModel vm, ShellView view, StringBuilder report)
    {
        if (vm.SelectedQuake is null)
        {
            report.AppendLine("play mute: skipped (no Quake found: Play starts nothing)");
            return null;
        }
        var started = new List<string>();
        var startGame = vm.StartGame;
        vm.StartGame = info => started.Add(info.Arguments);
        UiSounds.Settings.Muted = false;
        UiSounds.Start(null, SoundEngine.Offline());
        var engine = UiSounds.Engine!;
        short Peak(double seconds, double lastSeconds)
        {
            var pcm = new short[(int)(seconds * SoundEngine.Rate)];
            engine.MixOffline(pcm);
            var from = pcm.Length - (int)(lastSeconds * SoundEngine.Rate);
            short peak = 0;
            for (var i = Math.Max(0, from); i < pcm.Length; ++i)
            {
                peak = Math.Max(peak, Math.Abs(pcm[i]));
            }
            return peak;
        }
        try
        {
            var buttons = FindAll<System.Windows.Controls.Button>(view).ToList();
            var steps = new List<string>();
            foreach (var command in new[] { vm.LaunchVrCommand, vm.LaunchFlatCommand })
            {
                var button = buttons.SingleOrDefault(b => b.Command == command);
                if (button is null)
                {
                    return "a Play button is missing from the Play page";
                }
                var before = Peak(1.0, 0.1); // (the fire fades in over 2.5 s)
                ((System.Windows.Automation.Provider.IInvokeProvider)new System.Windows.Automation.Peers.ButtonAutomationPeer(button)
                    .GetPattern(System.Windows.Automation.Peers.PatternInterface.Invoke)).Invoke();
                await Dispatcher.Yield(DispatcherPriority.Background);
                var fading = Peak(UiSounds.GameStartFadeSeconds / 2, 0.01);
                var faded = Peak(UiSounds.GameStartFadeSeconds / 2 + 0.05, 0.03);
                var after = Peak(1.0, 1.0);
                var label = command == vm.LaunchVrCommand ? "Play in VR" : "Play on the monitor";
                steps.Add($"{label}: peak {before} before, {fading} half-way through the fade, {faded} at its end, {after} in the next second; muted {UiSounds.Settings.Muted}");
                if (started.Count != steps.Count)
                {
                    return $"{label}: the game was not started";
                }
                if (!UiSounds.Settings.Muted)
                {
                    return $"{label}: the installer is not muted";
                }
                if (command == vm.LaunchVrCommand && (before == 0 || fading == 0))
                {
                    return $"{label}: no fade (peak {before} before, {fading} half-way)";
                }
                if (faded != 0 || after != 0)
                {
                    return $"{label}: not silent after the fade (peak {faded}, then {after})";
                }
            }
            report.AppendLine($"play mute: {string.Join("; ", steps)}");
            return null;
        }
        finally
        {
            vm.StartGame = startGame;
            UiSounds.Stop();
            UiSounds.Settings.Muted = false;
        }
    }

    static async Task<string?> StatementCheck(MainViewModel vm, ShellView view, string dir, StringBuilder report)
    {
        vm.GoTo(Page.Statement);
        await Save(view, Path.Combine(dir, "1c-statement-unset.png"));
        var page = FindAll<StatementPage>(view).Single();
        var radios = FindAll<System.Windows.Controls.RadioButton>(page).ToList();
        var next = FindAll<System.Windows.Controls.Button>(view).Single(b => b.Command == vm.NextCommand);
        if (radios.Count != Core.AiStatement.Claims.Count * 2)
        {
            return $"{radios.Count} switch halves, not {Core.AiStatement.Claims.Count * 2}";
        }
        var states = new List<string>();
        string? Check(bool continueEnabled, string state)
        {
            var on = string.Join("", radios.Select(r => r.IsChecked == true ? "1" : "0"));
            states.Add($"{state}: switches {on}, continue {(next.IsEnabled ? "enabled" : "disabled")}");
            return next.IsEnabled != continueEnabled || vm.CanGoNext != continueEnabled ? $"{state}: Continue should be {(continueEnabled ? "enabled" : "disabled")} (button {next.IsEnabled}, command {vm.CanGoNext})" : null;
        }
        static System.Windows.Automation.Provider.ISelectionItemProvider Peer(System.Windows.Controls.RadioButton r) =>
            (System.Windows.Automation.Provider.ISelectionItemProvider)new System.Windows.Automation.Peers.RadioButtonAutomationPeer(r)
                .GetPattern(System.Windows.Automation.Peers.PatternInterface.SelectionItem);
        // Radio i*2 is claim i's YES, i*2+1 its NO.
        void Pick(int claim, bool yes) => Peer(radios[claim * 2 + (yes ? 0 : 1)]).Select();

        if (radios.Any(r => r.IsChecked != false) || vm.Statement.Unanswered != Core.AiStatement.Claims.Count)
        {
            return "the switches do not start with neither YES nor NO";
        }
        if (Check(false, "unset") is { } e1)
        {
            return e1;
        }

        Pick(0, true);
        Pick(1, false);
        try
        {
            Peer(radios[0]).RemoveFromSelection();
            return "a picked YES went back to neither";
        }
        catch (InvalidOperationException)
        {
        }
        await Save(view, Path.Combine(dir, "1d-statement-mixed.png"));
        if (vm.Statement[0] != true || vm.Statement[1] != false || vm.Statement[2] is not null)
        {
            return $"the mixed answers did not reach the statement ({string.Join(",", Enumerable.Range(0, Core.AiStatement.Claims.Count).Select(i => vm.Statement[i]?.ToString() ?? "unset"))}; switches {string.Join("", radios.Select(r => r.IsChecked == true ? "1" : "0"))})";
        }
        if (Check(false, "mixed") is { } e2)
        {
            return e2;
        }

        Pick(1, true);
        Pick(2, true);
        await Save(view, Path.Combine(dir, "1e-statement-all-yes.png"));
        if (Check(true, "all yes") is { } e3)
        {
            return e3;
        }
        Pick(2, false);
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        if (Check(false, "one switched to no") is { } e4)
        {
            return e4;
        }
        Pick(2, true);
        await Dispatcher.Yield(DispatcherPriority.ApplicationIdle);
        if (Check(true, "back to all yes") is { } e5)
        {
            return e5;
        }
        foreach (var s in states)
        {
            report.AppendLine($"statement {s}");
        }
        await File.WriteAllLinesAsync(Path.Combine(dir, "1-statement-check.txt"), states);
        return null;
    }

    static IEnumerable<T> FindAll<T>(DependencyObject root) where T : DependencyObject
    {
        for (var i = 0; i < VisualTreeHelper.GetChildrenCount(root); ++i)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            if (child is T t)
            {
                yield return t;
            }
            foreach (var d in FindAll<T>(child))
            {
                yield return d;
            }
        }
    }
}
