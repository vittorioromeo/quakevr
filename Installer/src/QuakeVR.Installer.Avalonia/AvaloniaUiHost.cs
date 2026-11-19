using System.Text;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Input.Platform;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

/// <summary>The view models' <see cref="UiHost"/> under Avalonia: the theme's resources, a themed message window,
/// the storage provider's pickers, the clipboard.</summary>
static class AvaloniaUiHost
{
    static Window? Main => (Application.Current?.ApplicationLifetime as IClassicDesktopStyleApplicationLifetime)?.MainWindow;

    public static void Install()
    {
        UiHost.Resource = key => Application.Current is { } app && app.TryFindResource(key, app.ActualThemeVariant, out var v) ? v : null;
        UiHost.Shutdown = () => (Application.Current?.ApplicationLifetime as IClassicDesktopStyleApplicationLifetime)?.Shutdown();
        UiHost.CopyText = text => _ = Main?.Clipboard?.SetTextAsync(text);
        UiHost.ShowCredits = () =>
        {
            if (Main is { } owner)
            {
                _ = Credits().ShowDialog(owner);
            }
        };
        // Avalonia has no CommandManager: every command asks again whenever the view models say something changed.
        UiHost.Requery = () => Dispatcher.UIThread.Post(UiHost.RaiseRequery);
        UiHost.Prompt = PromptAsync;
        UiHost.PickFolder = async title =>
            Main is { } w && (await w.StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions { Title = title })).FirstOrDefault() is { } f
                ? f.TryGetLocalPath()
                : null;
        UiHost.PickFile = async (title, filters) =>
            Main is { } w && (await w.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = title,
                FileTypeFilter = [.. filters.Select(f => new FilePickerFileType(f.Name) { Patterns = [.. f.Patterns] })],
            })).FirstOrDefault() is { } file
                ? file.TryGetLocalPath()
                : null;
    }

    static IBrush Brush(string key) => UiHost.Look(key) as IBrush ?? Brushes.White;

    static async Task<UiResult> PromptAsync(string text, string title, UiButtons buttons, UiIcon icon)
    {
        if (Main is not { } owner)
        {
            return UiResult.Cancel;
        }
        var result = UiResult.Cancel;
        var window = new Window
        {
            Title = title,
            Width = 480,
            SizeToContent = SizeToContent.Height,
            CanResize = false,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush("Bg"),
        };
        var row = new StackPanel { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 10, Margin = new Thickness(0, 18, 0, 0) };
        void Add(string label, UiResult answer, bool main)
        {
            var b = new Button { Content = label, Classes = { main ? "AccentButton" : "GhostButton" }, IsDefault = main, IsCancel = !main };
            b.Click += (_, _) =>
            {
                result = answer;
                window.Close();
            };
            row.Children.Add(b);
        }
        switch (buttons)
        {
            case UiButtons.OKCancel:
                Add("Cancel", UiResult.Cancel, false);
                Add("OK", UiResult.OK, true);
                break;
            case UiButtons.YesNo:
                Add("No", UiResult.No, false);
                Add("Yes", UiResult.Yes, true);
                break;
            default:
                Add("OK", UiResult.OK, true);
                break;
        }
        window.Content = new Border
        {
            Padding = new Thickness(22, 18),
            Child = new StackPanel
            {
                Children =
                {
                    new TextBlock
                    {
                        Text = icon switch { UiIcon.Warning => "Warning", UiIcon.Error => "Error", UiIcon.Question => title, _ => title },
                        Classes = { "SectionTitle" },
                        Foreground = Brush(icon switch { UiIcon.Warning => "Warn", UiIcon.Error => "Error", _ => "Brass" }),
                        Margin = new Thickness(0, 0, 0, 8),
                    },
                    new TextBlock { Text = text, Classes = { "Body" } },
                    row,
                },
            },
        };
        await window.ShowDialog(owner);
        return result;
    }

    static Window Credits()
    {
        var sb = new StringBuilder();
        sb.AppendLine("QUAKE VR: UNLEASHED, by Vittorio Romeo. Free software (GPL-2.0-or-later).");
        sb.AppendLine("Support the project: https://ko-fi.com/vittorioromeovee");
        sb.AppendLine("Chat, help and news: https://discord.me/quakevr");
        sb.AppendLine();
        sb.AppendLine("Quake's textures and sounds in this installer are read while it runs from your own copy of Quake");
        sb.AppendLine("(id Software). None of them is included with Quake VR: Unleashed or its installer; before Quake is");
        sb.AppendLine("found (or without it) the installer makes its own textures and sounds.");
        sb.AppendLine();
        sb.AppendLine("Fonts, embedded under the SIL Open Font License 1.1 (full texts below):");
        sb.AppendLine("  Grenze Gotisch: Copyright 2020 The Grenze Gotisch Project Authors (github.com/Omnibus-Type/Grenze-Gotisch)");
        sb.AppendLine("  Barlow, Barlow Semi Condensed: Copyright 2017 The Barlow Project Authors (github.com/jpt/barlow)");
        sb.AppendLine("  Inter (Avalonia's default text font): SIL Open Font License 1.1.");
        sb.AppendLine();
        sb.AppendLine("The Ko-fi logo is Ko-fi's (ko-fi.com), from its brand assets, linking to the author's Ko-fi page.");
        sb.AppendLine("The Discord logo is Discord's (discord.com), drawn from Simple Icons' path (CC0), linking to the Quake VR Discord server.");
        sb.AppendLine("Icons: Segoe Fluent Icons (Windows). Window toolkit: Avalonia (MIT), drawing with Skia (BSD) and HarfBuzz (MIT).");
        foreach (var name in new[] { "OFL-GrenzeGotisch.txt", "OFL-Barlow.txt" })
        {
            sb.AppendLine();
            sb.AppendLine(new string('-', 60));
            sb.AppendLine(name);
            sb.AppendLine(new string('-', 60));
            try
            {
                using var r = new StreamReader(AssetLoader.Open(new Uri($"avares://QuakeVR-Setup/Fonts/{name}")));
                sb.AppendLine(r.ReadToEnd());
            }
            catch (Exception e) when (e is IOException or FileNotFoundException)
            {
                sb.AppendLine("(missing)");
            }
        }
        return new Window
        {
            Title = "Credits · Quake VR: Unleashed Setup",
            Width = 680,
            Height = 560,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = Brush("Bg"),
            Content = new TextBox
            {
                Text = sb.ToString(),
                IsReadOnly = true,
                TextWrapping = TextWrapping.Wrap,
                FontSize = 13.5,
                VerticalContentAlignment = VerticalAlignment.Top,
                Margin = new Thickness(16),
            },
        };
    }
}
