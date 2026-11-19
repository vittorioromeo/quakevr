using System.Windows;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer;

/// <summary>The view models' <see cref="UiHost"/> under WPF: the theme's resources, MessageBox, the Win32 dialogs.</summary>
static class WpfUiHost
{
    public static void Install()
    {
        UiHost.Resource = key => Application.Current?.TryFindResource(key);
        UiHost.Shutdown = () => Application.Current.Shutdown();
        UiHost.CopyText = Clipboard.SetText;
        UiHost.ShowCredits = () => new Views.CreditsWindow(Application.Current.MainWindow).ShowDialog();
        UiHost.Requery = System.Windows.Input.CommandManager.InvalidateRequerySuggested;
        UiHost.Prompt = (text, title, buttons, icon) => Task.FromResult(MessageBox.Show(text, title,
            buttons switch { UiButtons.OKCancel => MessageBoxButton.OKCancel, UiButtons.YesNo => MessageBoxButton.YesNo, _ => MessageBoxButton.OK },
            icon switch { UiIcon.Question => MessageBoxImage.Question, UiIcon.Warning => MessageBoxImage.Warning, UiIcon.Error => MessageBoxImage.Error, _ => MessageBoxImage.Information })
            switch { MessageBoxResult.OK => UiResult.OK, MessageBoxResult.Yes => UiResult.Yes, MessageBoxResult.No => UiResult.No, _ => UiResult.Cancel });
        UiHost.PickFolder = title =>
        {
            var dialog = new Microsoft.Win32.OpenFolderDialog { Title = title };
            return Task.FromResult(dialog.ShowDialog() == true ? dialog.FolderName : null);
        };
        UiHost.PickFile = (title, filters) =>
        {
            var dialog = new Microsoft.Win32.OpenFileDialog
            {
                Title = title,
                Filter = string.Join("|", filters.Select(f => $"{f.Name}|{string.Join(";", f.Patterns.Select(p => p == "*" ? "*.*" : p))}")),
            };
            return Task.FromResult(dialog.ShowDialog() == true ? dialog.FileName : null);
        };
    }
}
