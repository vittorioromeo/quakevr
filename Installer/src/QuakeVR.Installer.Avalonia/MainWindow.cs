using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Platform;
using QuakeVR.Installer.Views;

namespace QuakeVR.Installer;

/// <summary>The wizard's window: the shell view, at the WPF window's sizes (its client area), never larger than the
/// screen's work area.</summary>
public sealed class MainWindow : Window
{
    // The WPF window's sizes less its frame (MainWindow.xaml.cs): Avalonia sizes the client area.
    public const double DefaultWidth = 1184, DefaultHeight = 810, MinimumWidth = 984, MinimumHeight = 680;

    public MainWindow()
    {
        Title = "Quake VR: Unleashed Setup";
        Width = DefaultWidth;
        Height = DefaultHeight;
        MinWidth = MinimumWidth;
        MinHeight = MinimumHeight;
        WindowStartupLocation = WindowStartupLocation.CenterScreen;
        Background = new SolidColorBrush(Color.Parse("#FF0C0A08"));
        Icon = new WindowIcon(AssetLoader.Open(new Uri("avares://QuakeVR-Setup/Assets/app.ico")));
        Content = new ShellView();
        Opened += (_, _) =>
        {
            if (Screens.ScreenFromWindow(this) is { } screen)
            {
                var area = screen.WorkingArea.Size.ToSize(screen.Scaling);
                MinWidth = Math.Min(MinWidth, area.Width);
                MinHeight = Math.Min(MinHeight, area.Height - 32);
                Width = Math.Min(Width, area.Width);
                Height = Math.Min(Height, area.Height - 32);
            }
        };
    }
}
