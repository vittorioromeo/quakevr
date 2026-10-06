using System.IO;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace QuakeVR.Installer.Views;

/// <summary>Who made what: the installer, its fonts (with their SIL Open Font License texts, as the licence asks),
/// Ko-fi's logo, and where the Quake textures and sounds come from.</summary>
sealed class CreditsWindow : Window
{
    public CreditsWindow(Window? owner)
    {
        Owner = owner;
        Title = "Credits · Quake VR: Unleashed Setup";
        Width = 680;
        Height = 560;
        WindowStartupLocation = owner is null ? WindowStartupLocation.CenterScreen : WindowStartupLocation.CenterOwner;
        Background = (Brush)FindResource("Bg");
        var text = new TextBox
        {
            Text = Build(),
            IsReadOnly = true,
            TextWrapping = TextWrapping.Wrap,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            FontFamily = (FontFamily)FindResource("UiFont"),
            FontSize = 13.5,
            VerticalContentAlignment = VerticalAlignment.Top,
            Margin = new Thickness(16),
        };
        Content = text;
    }

    static string Build()
    {
        var sb = new StringBuilder();
        sb.AppendLine("QUAKE VR: UNLEASHED, by Vittorio Romeo. Free software (GPL-2.0-or-later).");
        sb.AppendLine("Support the project: https://ko-fi.com/vittorioromeovee");
        sb.AppendLine();
        sb.AppendLine("Quake's textures and sounds in this installer are read while it runs from your own copy of Quake");
        sb.AppendLine("(id Software). None of them is included with Quake VR: Unleashed or its installer; before Quake is");
        sb.AppendLine("found (or without it) the installer makes its own textures and sounds.");
        sb.AppendLine();
        sb.AppendLine("Fonts, embedded under the SIL Open Font License 1.1 (full texts below):");
        sb.AppendLine("  Grenze Gotisch: Copyright 2020 The Grenze Gotisch Project Authors (github.com/Omnibus-Type/Grenze-Gotisch)");
        sb.AppendLine("  Barlow, Barlow Semi Condensed: Copyright 2017 The Barlow Project Authors (github.com/jpt/barlow)");
        sb.AppendLine();
        sb.AppendLine("The Ko-fi logo is Ko-fi's (ko-fi.com), from its brand assets, linking to the author's Ko-fi page.");
        sb.AppendLine("Icons: Segoe Fluent Icons (Windows).");
        foreach (var name in new[] { "OFL-GrenzeGotisch.txt", "OFL-Barlow.txt" })
        {
            sb.AppendLine();
            sb.AppendLine(new string('-', 60));
            sb.AppendLine(name);
            sb.AppendLine(new string('-', 60));
            try
            {
                var info = Application.GetResourceStream(new Uri($"pack://application:,,,/QuakeVR-Setup;component/Fonts/{name}"));
                using var r = new StreamReader(info!.Stream);
                sb.AppendLine(r.ReadToEnd());
            }
            catch (Exception e) when (e is IOException or NullReferenceException)
            {
                sb.AppendLine("(missing)");
            }
        }
        return sb.ToString();
    }
}
