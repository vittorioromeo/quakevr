using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using QuakeVR.Installer.Audio;
using QuakeVR.Installer.Core.Platform;
using QuakeVR.Installer.Skin;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

public partial class App : Application
{
    /// <summary>The command line (Program sets it before the app starts).</summary>
    internal static StartupOptions Options { get; set; } = new();

    public override void Initialize() => AvaloniaXamlLoader.Load(this);

    public override void OnFrameworkInitializationCompleted()
    {
        AvaloniaUiHost.Install();
        SkinTextures.ApplyGenerated();
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var options = Options;
            if (!options.Silent)
            {
                UiSounds.Start(Path.Combine(options.Sandbox is { } sb ? sb.InstallerData : HostPlatform.InstallerDataDir(Probe()), "ui.json"));
            }
            var vm = new MainViewModel(Probe(), options);
            var window = new MainWindow { DataContext = vm };
            if (vm.ShowTestBanner)
            {
                window.Title = "[TEST] " + window.Title;
            }
            desktop.MainWindow = window;
            desktop.Exit += (_, _) => UiSounds.Stop();
            if (options.Uninstall)
            {
                window.Opened += (_, _) => _ = vm.RemoveFromCommandLineAsync();
            }
        }
        base.OnFrameworkInitializationCompleted();
    }

    /// <summary>This computer: the registry on Windows, the XDG folders and files on Linux.</summary>
    internal static ISystemProbe Probe() => HostPlatform.CreateProbe();
}
