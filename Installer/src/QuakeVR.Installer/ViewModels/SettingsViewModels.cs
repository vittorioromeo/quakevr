using System.Collections.ObjectModel;
using System.Windows.Input;
using QuakeVR.Installer.Core.Settings;

namespace QuakeVR.Installer.ViewModels;

// The settings page (Views/SettingsPage.xaml; Core/Settings/SettingsPage.cs): categories > subcategories > settings, a
// tree the player opens and closes, each setting with the control its kind needs and the game's own help.

public sealed class SettingsPageViewModel : ObservableObject
{
    public SettingsPageViewModel(SettingsSession session)
    {
        Session = session;
        foreach (var def in session.Settings)
        {
            var cat = Categories.FirstOrDefault(c => c.Title == def.Category);
            if (cat is null)
            {
                Categories.Add(cat = new SettingCategoryViewModel(def.Category) { IsExpanded = Categories.Count == 0 });
            }
            var sub = cat.Subcategories.FirstOrDefault(s => s.Title == def.Subcategory);
            if (sub is null)
            {
                cat.Subcategories.Add(sub = new SettingSubcategoryViewModel(def.Subcategory));
            }
            var row = new SettingViewModel(def, session.Initial[def.Cvar], this);
            sub.Settings.Add(row);
            _rows.Add(row);
        }
        ResetAllCommand = new RelayCommand(() =>
        {
            foreach (var r in _rows)
            {
                r.Value = r.Def.Default;
            }
        });
        ExpandAllCommand = new RelayCommand(() =>
        {
            var open = !Categories.All(c => c.IsExpanded);
            foreach (var c in Categories)
            {
                c.IsExpanded = open;
            }
        });
    }

    readonly List<SettingViewModel> _rows = [];
    public SettingsSession Session { get; }
    public ObservableCollection<SettingCategoryViewModel> Categories { get; } = [];
    public ICommand ResetAllCommand { get; }
    public ICommand ExpandAllCommand { get; }

    public Dictionary<string, string> Values() => _rows.ToDictionary(r => r.Def.Cvar, r => r.Value, StringComparer.OrdinalIgnoreCase);
    public int ChangedCount => Session.Changes(Values()).Count;
    public string ChangedText => ChangedCount switch
    {
        0 => Session.HasConfig ? "Nothing changed: your settings stay as they are." : "Nothing changed: the game starts with its defaults.",
        1 => "1 setting changed: it is set at the next start.",
        var n => $"{n} settings changed: they are set at the next start.",
    };
    public string Subtitle => Session.HasConfig
        ? "Your current settings, read from the game. Change any here, or later in the game's VR Settings."
        : "The most asked-about settings, before your first start. All of them are also in the game's VR Settings.";

    internal void OnValueChanged() => Raise(nameof(ChangedCount), nameof(ChangedText));

    /// <summary>Writes installer_settings.cfg (only the changes); the path, or null when nothing changed.</summary>
    public string? Save() => Session.Write(Values(), DateTime.Now);
}

public sealed class SettingCategoryViewModel(string title) : ObservableObject
{
    bool _expanded;
    public string Title { get; } = title;
    public ObservableCollection<SettingSubcategoryViewModel> Subcategories { get; } = [];

    public bool IsExpanded
    {
        get => _expanded;
        set => Set(ref _expanded, value);
    }
}

public sealed class SettingSubcategoryViewModel(string title) : ObservableObject
{
    public string Title { get; } = title;
    public bool HasTitle => Title.Length > 0;
    public ObservableCollection<SettingViewModel> Settings { get; } = [];
}

public sealed class SettingViewModel : ObservableObject
{
    readonly SettingsPageViewModel _page;
    readonly string _initial;
    string _value;

    public SettingViewModel(SettingDef def, string initial, SettingsPageViewModel page)
    {
        Def = def;
        _page = page;
        _initial = initial;
        _value = initial;
        var choices = def.Choices.Select(c => new SettingChoiceViewModel(this, c.Value, c.Label, c.Help)).ToList();
        if (def.IsApply && initial == SettingsSession.KeepAsIs)
        {
            choices.Insert(0, new SettingChoiceViewModel(this, SettingsSession.KeepAsIs, "As it is now", "Leaves these settings as they are in the game."));
        }
        else if (def.Kind == SettingKinds.Choice && !choices.Any(c => SettingValues.Same(c.Value, initial)))
        {
            choices.Insert(0, new SettingChoiceViewModel(this, initial, $"Your own ({initial})", null)); // a value no choice names
        }
        Choices = choices;
        ResetCommand = new RelayCommand(() => Value = Def.Default);
        var lo = Math.Min(def.Min, SettingValues.Number(initial, def.Min));
        var hi = Math.Max(def.Max, SettingValues.Number(initial, def.Max));
        SliderMin = lo;
        SliderMax = hi;
    }

    public SettingDef Def { get; }
    public string Label => Def.Label;
    public string Help => Def.Help;
    public bool HasHelp => Def.Help.Length > 0;
    public string WhereInGame => Def.Path.Length > 0 ? $"In the game: {Def.Path}" : "";
    public string Cvar => Def.Cvar;
    public bool IsToggle => Def.Kind == SettingKinds.Toggle;
    public bool IsChoice => Def.Kind is SettingKinds.Choice or SettingKinds.Apply;
    public bool IsSlider => Def.Kind == SettingKinds.Slider;
    public IReadOnlyList<SettingChoiceViewModel> Choices { get; }
    public double SliderMin { get; }
    public double SliderMax { get; }
    public double SliderStep => Def.Step > 0 ? Def.Step : 0.01;
    public ICommand ResetCommand { get; }

    public string Value
    {
        get => _value;
        set
        {
            if (Set(ref _value, value))
            {
                foreach (var c in Choices)
                {
                    c.Refresh();
                }
                Raise(nameof(IsOn), nameof(Number), nameof(ValueText), nameof(IsDefault), nameof(IsChanged), nameof(SelectedHelp), nameof(HasSelectedHelp));
                _page.OnValueChanged();
            }
        }
    }

    public bool IsOn
    {
        get => SettingValues.Number(Value) != 0;
        set => Value = value ? "1" : "0";
    }

    public double Number
    {
        get => SettingValues.Number(Value, Def.Min);
        set => Value = SettingValues.Text(Math.Round(value / SliderStep) * SliderStep);
    }

    public string ValueText => IsSlider ? SettingValues.Format(Def.Format, Number) : "";
    public bool IsDefault => SettingValues.Same(Value, Def.Default);
    public bool IsChanged => !SettingValues.Same(Value, _initial);
    public string? SelectedHelp => Choices.FirstOrDefault(c => c.IsSelected)?.Help;
    public bool HasSelectedHelp => !string.IsNullOrEmpty(SelectedHelp);
}

public sealed class SettingChoiceViewModel(SettingViewModel owner, string value, string label, string? help) : ObservableObject
{
    public string Value { get; } = value;
    public string Label { get; } = label;
    public string? Help { get; } = help;

    public bool IsSelected
    {
        get => Value == SettingsSession.KeepAsIs ? owner.Value == SettingsSession.KeepAsIs
                                                 : owner.Value != SettingsSession.KeepAsIs && SettingValues.Same(owner.Value, Value);
        set
        {
            if (value)
            {
                owner.Value = Value;
            }
            Raise();
        }
    }

    internal void Refresh() => Raise(nameof(IsSelected));
}
