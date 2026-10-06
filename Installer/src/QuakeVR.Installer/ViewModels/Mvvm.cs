using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;

namespace QuakeVR.Installer.ViewModels;

/// <summary>INotifyPropertyChanged with a setter helper (no MVVM package: phase 1 has no NuGet dependency).</summary>
public abstract class ObservableObject : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return false;
        }
        field = value;
        Raise(name);
        return true;
    }

    protected void Raise([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    protected void Raise(params string[] names)
    {
        foreach (var n in names)
        {
            Raise(n);
        }
    }
}

public sealed class RelayCommand(Action<object?> execute, Func<object?, bool>? canExecute = null) : ICommand
{
    public RelayCommand(Action execute, Func<bool>? canExecute = null)
        : this(_ => execute(), canExecute is null ? null : _ => canExecute())
    {
    }

    public event EventHandler? CanExecuteChanged
    {
        add => CommandManager.RequerySuggested += value;
        remove => CommandManager.RequerySuggested -= value;
    }

    public bool CanExecute(object? parameter) => canExecute?.Invoke(parameter) ?? true;

    public void Execute(object? parameter) => execute(parameter);
}

public enum CheckStatus
{
    Ok,
    Warning,
    Error,
    Info,
    /// <summary>Not there, and that is fine (an expansion the player does not own).</summary>
    Absent,
    /// <summary>There, but the game cannot use it yet (mg1, mg3).</summary>
    NotYet,
    Busy,
}

/// <summary>Brushes and icon glyphs for each status, from the theme.</summary>
public static class StatusLook
{
    public static Brush Brush(CheckStatus s) => (Brush)Application.Current.FindResource(s switch
    {
        CheckStatus.Ok => "Ok",
        CheckStatus.Warning => "Warn",
        CheckStatus.Error => "Error",
        CheckStatus.Info or CheckStatus.Busy => "Info",
        CheckStatus.NotYet => "Subtle",
        _ => "Faint",
    });

    public static string Glyph(CheckStatus s) => s switch
    {
        CheckStatus.Ok => "\uE73E", // check mark
        CheckStatus.Warning => "\uE7BA", // warning
        CheckStatus.Error => "\uE711", // cancel
        CheckStatus.Info => "\uE946", // info
        CheckStatus.NotYet => "\uE823", // clock
        CheckStatus.Busy => "\uE895", // sync
        _ => "\uE738", // minus
    };
}

/// <summary>One line of the Detect page: a status icon, a title, what was found, and how to fix it.</summary>
public sealed class CheckItem(CheckStatus status, string title, string detail, string? hint = null) : ObservableObject
{
    public CheckStatus Status { get; } = status;
    public string Title { get; } = title;
    public string Detail { get; } = detail;
    public string? Hint { get; } = hint;
    public bool HasHint => !string.IsNullOrEmpty(Hint);
    public Brush Brush => StatusLook.Brush(Status);
    public string Glyph => StatusLook.Glyph(Status);
    public string? ActionText { get; init; }
    public ICommand? Action { get; init; }
    public bool HasAction => Action is not null;
}

public enum StepState
{
    Upcoming,
    Current,
    Done,
}

public sealed class StepItem(int number, string title) : ObservableObject
{
    StepState _state;

    public int Number { get; } = number;
    public string Title { get; } = title;

    public StepState State
    {
        get => _state;
        set
        {
            if (Set(ref _state, value))
            {
                Raise(nameof(IsCurrent), nameof(IsDone), nameof(Marker), nameof(MarkerFont), nameof(TitleBrush), nameof(MarkerBrush), nameof(MarkerText));
            }
        }
    }

    public bool IsCurrent => State == StepState.Current;
    public bool IsDone => State == StepState.Done;
    public string Marker => IsDone ? "\uE73E" : Number.ToString();
    public FontFamily MarkerFont => IsDone ? (FontFamily)Application.Current.FindResource("IconFont") : (FontFamily)Application.Current.FindResource("CondensedFont");
    public Brush TitleBrush => (Brush)Application.Current.FindResource(State switch { StepState.Upcoming => "Faint", StepState.Done => "Subtle", _ => "Text" });
    public Brush MarkerBrush => (Brush)Application.Current.FindResource(State switch { StepState.Current => "Accent", StepState.Done => "Brass", _ => "Card" });
    public Brush MarkerText => State == StepState.Upcoming ? (Brush)Application.Current.FindResource("Faint") : (Brush)Application.Current.FindResource("DarkText");
}

public sealed class LogLine(Core.Packaging.LogLevel level, string text)
{
    public Core.Packaging.LogLevel Level { get; } = level;
    public string Text { get; } = text;
    public Brush Brush => StatusLook.Brush(Level switch
    {
        Core.Packaging.LogLevel.Success => CheckStatus.Ok,
        Core.Packaging.LogLevel.Warning => CheckStatus.Warning,
        Core.Packaging.LogLevel.Error => CheckStatus.Error,
        _ => CheckStatus.Absent,
    });
    public string Glyph => Level switch
    {
        Core.Packaging.LogLevel.Success => "\uE73E",
        Core.Packaging.LogLevel.Warning => "\uE7BA",
        Core.Packaging.LogLevel.Error => "\uE711",
        _ => "\uE76C", // chevron
    };
}

/// <summary>A Quake the player can pick on the Detect page.</summary>
public sealed class QuakeChoice(Core.Detection.QuakeInstall install) : ObservableObject
{
    bool _selected;

    public Core.Detection.QuakeInstall Install { get; } = install;
    public string Title => $"{Install.Name}  ·  {Install.Summary}";
    public string Path => Install.InstallDir;
    public bool Playable => Install.Playable;
    public string? Problem => Playable ? null : Install.Summary;

    public bool IsSelected
    {
        get => _selected;
        set => Set(ref _selected, value);
    }
}

/// <summary>An expansion row: status, title, what the game will do with it.</summary>
public sealed class ExpansionRow(Core.Detection.ExpansionInfo info)
{
    public string Title => info.Title;
    public string Detail => info.Detail;
    public CheckStatus Status => info.State switch
    {
        Core.Detection.ExpansionState.Ready => CheckStatus.Ok,
        Core.Detection.ExpansionState.Incomplete => CheckStatus.Warning,
        Core.Detection.ExpansionState.DetectedNotSupported => CheckStatus.NotYet,
        _ => CheckStatus.Absent,
    };
    public Brush Brush => StatusLook.Brush(Status);
    public string Glyph => StatusLook.Glyph(Status);
    public Brush TitleBrush => (Brush)Application.Current.FindResource(Status == CheckStatus.Absent ? "Subtle" : "Text");
}
