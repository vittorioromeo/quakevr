namespace QuakeVR.Installer.ViewModels;

/// <summary>The buttons of a <see cref="UiHost.ShowAsync"/> prompt.</summary>
public enum UiButtons
{
    OK,
    OKCancel,
    YesNo,
}

/// <summary>The icon of a <see cref="UiHost.ShowAsync"/> prompt.</summary>
public enum UiIcon
{
    Information,
    Question,
    Warning,
    Error,
}

/// <summary>The answer to a <see cref="UiHost.ShowAsync"/> prompt (closed without one: <see cref="Cancel"/>).</summary>
public enum UiResult
{
    OK,
    Cancel,
    Yes,
    No,
}

/// <summary>A file dialog's filter: a name and its patterns (<c>*.zip</c>, <c>manifest.json</c>).</summary>
public sealed record UiFileFilter(string Name, IReadOnlyList<string> Patterns);

/// <summary>
/// What the view models ask of the window's toolkit, so they are the same code under WPF (Windows) and Avalonia
/// (Windows and Linux): the app sets these at start (WpfUiHost, AvaloniaUiHost). The defaults do nothing and answer
/// "cancelled", which is what the self-tests and the screenshot harnesses want.
/// </summary>
public static class UiHost
{
    /// <summary>A theme resource by key (brushes, fonts), as the toolkit's own object.</summary>
    public static Func<string, object?> Resource { get; set; } = _ => null;

    /// <summary>Closes the window and ends the program.</summary>
    public static Action Shutdown { get; set; } = () => { };

    public static Action<string> CopyText { get; set; } = _ => { };

    /// <summary>The credits window, modal.</summary>
    public static Action ShowCredits { get; set; } = () => { };

    /// <summary>A modal message: text, title, buttons, icon.</summary>
    public static Func<string, string, UiButtons, UiIcon, Task<UiResult>> Prompt { get; set; } =
        (_, _, _, _) => Task.FromResult(UiResult.Cancel);

    /// <summary>A folder picker (title): the folder, or null.</summary>
    public static Func<string, Task<string?>> PickFolder { get; set; } = _ => Task.FromResult<string?>(null);

    /// <summary>A file picker (title, filters): the file, or null.</summary>
    public static Func<string, IReadOnlyList<UiFileFilter>, Task<string?>> PickFile { get; set; } = (_, _) => Task.FromResult<string?>(null);

    /// <summary>The commands' CanExecute may have changed (WPF: CommandManager; Avalonia: every RelayCommand's
    /// CanExecuteChanged).</summary>
    public static Action Requery { get; set; } = RaiseRequery;

    public static void RaiseRequery() => RequerySuggested?.Invoke(null, EventArgs.Empty);

    /// <summary>Raised by the default <see cref="Requery"/>: what RelayCommand listens to outside WPF.</summary>
    public static event EventHandler? RequerySuggested;

    public static Task<UiResult> ShowAsync(string text, string title, UiButtons buttons = UiButtons.OK, UiIcon icon = UiIcon.Information) =>
        Prompt(text, title, buttons, icon);

    /// <summary>A theme brush (or font) for a view model's property; null when no theme is loaded.</summary>
    public static object? Look(string key) => Resource(key);
}
