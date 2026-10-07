using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using QuakeVR.Installer.Core.Assets;
using QuakeVR.Installer.Core.Audio;
using QuakeVR.Installer.ViewModels;

namespace QuakeVR.Installer.Audio;

public enum Sfx
{
    None,
    /// <summary>A secondary button (Browse, Check again).</summary>
    Click,
    /// <summary>The main button (Next, Install).</summary>
    Select,
    Back,
    Toggle,
    Type,
    InstallStart,
    InstallDone,
    Error,
    Support,
}

/// <summary>The installer's sound settings (bound by the sidebar's speaker button).</summary>
public sealed class SoundSettings : ObservableObject
{
    bool _muted;

    public bool Muted
    {
        get => _muted;
        set
        {
            if (Set(ref _muted, value))
            {
                UiSounds.ApplyMute();
                UiSounds.SaveSettings();
            }
        }
    }

    /// <summary>Muted without remembering it (the mute at Play): the speaker button shows it; the next run of the
    /// installer starts as the player last set the button.</summary>
    internal void MuteWithoutSaving()
    {
        if (Set(ref _muted, true, nameof(Muted)))
        {
            UiSounds.ApplyMute();
        }
    }

    public string Source { get; internal set; } = "";
}

/// <summary>
/// The UI's sounds: Quake's own menu sounds (and a few others) read from the player's pak files once Quake is found,
/// synthesized ones before that or without Quake. Buttons, check boxes and typing play theirs through class handlers,
/// so no page has to wire anything; <c>UiSounds.Kind</c> on a button picks another sound. Quiet, throttled, mutable.
/// </summary>
public static class UiSounds
{
    static readonly Dictionary<Sfx, string> QuakeFiles = new()
    {
        [Sfx.Click] = "sound/misc/menu1.wav",
        [Sfx.Select] = "sound/misc/menu2.wav",
        [Sfx.Back] = "sound/misc/menu3.wav",
        [Sfx.Toggle] = "sound/misc/menu1.wav",
        [Sfx.Type] = "sound/misc/menu1.wav",
        [Sfx.InstallStart] = "sound/weapons/pkup.wav",
        [Sfx.InstallDone] = "sound/misc/secret.wav",
        [Sfx.Error] = "sound/misc/talk.wav",
        [Sfx.Support] = "sound/items/health1.wav",
    };
    const string QuakeAmbience = "sound/ambience/fire1.wav";

    static readonly Dictionary<Sfx, float> Volumes = new()
    {
        [Sfx.Click] = 0.30f,
        [Sfx.Select] = 0.34f,
        [Sfx.Back] = 0.30f,
        [Sfx.Toggle] = 0.26f,
        [Sfx.Type] = 0.10f,
        [Sfx.InstallStart] = 0.32f,
        [Sfx.InstallDone] = 0.40f,
        [Sfx.Error] = 0.30f,
        [Sfx.Support] = 0.34f,
    };
    const float AmbienceVolume = 0.10f;

    public static readonly DependencyProperty KindProperty = DependencyProperty.RegisterAttached(
        "Kind", typeof(Sfx), typeof(UiSounds), new PropertyMetadata(Sfx.Click));

    public static Sfx GetKind(DependencyObject d) => (Sfx)d.GetValue(KindProperty);
    public static void SetKind(DependencyObject d, Sfx value) => d.SetValue(KindProperty, value);

    static SoundEngine? _engine;
    static Dictionary<Sfx, SoundClip> _clips = [];
    static SoundClip? _ambience;
    static readonly Dictionary<Sfx, DateTime> LastPlayed = [];
    static readonly Random Rng = new();
    static string? _settingsPath;

    static bool _hooked;
    static double? _nextFade;

    /// <summary>The fade out when Play starts the game (the speaker button's mute is 10 ms).</summary>
    public const double GameStartFadeSeconds = 0.3;

    public static SoundSettings Settings { get; } = new();

    /// <summary>The engine the sounds play on (the harness's offline one, after <c>Start(null, SoundEngine.Offline())</c>).</summary>
    internal static SoundEngine? Engine => _engine;

    /// <summary>Opens the device, makes the synthesized sounds and hooks the controls (the real window only).</summary>
    public static void Start(string? settingsPath) => Start(settingsPath, new SoundEngine());

    internal static void Start(string? settingsPath, SoundEngine engine)
    {
        _settingsPath = settingsPath;
        try
        {
            if (settingsPath is not null && File.Exists(settingsPath) &&
                JsonDocument.Parse(File.ReadAllText(settingsPath)).RootElement.TryGetProperty("muted", out var m))
            {
                Settings.Muted = m.GetBoolean();
            }
        }
        catch (Exception e) when (e is IOException or JsonException or InvalidOperationException or UnauthorizedAccessException)
        {
            // A damaged settings file: the defaults.
        }
        _engine = engine;
        UseSynthesized();
        ApplyMute();
        if (_hooked)
        {
            return;
        }
        _hooked = true;
        EventManager.RegisterClassHandler(typeof(ButtonBase), ButtonBase.ClickEvent, new RoutedEventHandler(OnClick));
        EventManager.RegisterClassHandler(typeof(TextBoxBase), UIElement.PreviewTextInputEvent, new TextCompositionEventHandler((_, _) => Play(Sfx.Type)));
        EventManager.RegisterClassHandler(typeof(TextBoxBase), UIElement.PreviewKeyDownEvent, new KeyEventHandler((s, e) =>
        {
            if (e.Key is Key.Back or Key.Delete && s is TextBox { IsReadOnly: false })
            {
                Play(Sfx.Type);
            }
        }));
    }

    public static void Stop()
    {
        _engine?.Dispose();
        _engine = null;
    }

    static void UseSynthesized()
    {
        _clips = Synth.All();
        _ambience = Synth.Crackle();
        Settings.Source = "synthesized sounds";
        StartAmbience();
    }

    /// <summary>Switches to Quake's sounds (any missing one keeps its synthesized stand-in).</summary>
    public static void UseQuake(QuakeFileSystem fs)
    {
        var (clips, ambience, found) = QuakeClips(fs, _clips.Count > 0 ? _clips : Synth.All());
        _clips = clips;
        _ambience = ambience ?? _ambience;
        Settings.Source = $"Quake's sounds ({found} of {QuakeFiles.Count})";
        StartAmbience();
    }

    /// <summary>Quake's sounds over <paramref name="fallback"/> (the synthesized ones), its fire, and how many were found.</summary>
    internal static (Dictionary<Sfx, SoundClip> Clips, SoundClip? Ambience, int Found) QuakeClips(QuakeFileSystem fs, Dictionary<Sfx, SoundClip> fallback)
    {
        var clips = new Dictionary<Sfx, SoundClip>(fallback);
        var found = 0;
        foreach (var (kind, file) in QuakeFiles)
        {
            if (Load(fs, file) is { } clip)
            {
                clips[kind] = clip;
                ++found;
            }
        }
        return (clips, Load(fs, QuakeAmbience), found);
    }

    /// <summary>A sound's volume and pitch as <see cref="Play"/> gives them (the harness's offline mix plays the same).</summary>
    internal static (float Volume, double Pitch) VolumeAndPitch(Sfx kind, Random rng) =>
        (Volumes.GetValueOrDefault(kind, 0.3f), kind switch
        {
            Sfx.Type => 1.2 + rng.NextDouble() * 0.2,
            Sfx.Toggle => 1.1,
            _ => 1.0,
        });

    internal const float AmbienceLevel = AmbienceVolume;

    static SoundClip? Load(QuakeFileSystem fs, string file) =>
        fs.Read(file) is { } data && QuakeFormats.ReadWav(data) is { } wav
            ? new SoundClip(file, SoundEngine.Resample(wav.Samples, wav.SampleRate))
            : null;

    static void StartAmbience()
    {
        if (_engine is not null && _ambience is not null)
        {
            _engine.SetLoop(_ambience, AmbienceVolume, 2.5);
        }
    }

    internal static void ApplyMute()
    {
        if (_engine is not null)
        {
            _engine.MasterFadeSeconds = _nextFade ?? SoundMixer.MasterRampFrames / (double)SoundMixer.Rate;
            _engine.MasterVolume = Settings.Muted ? 0 : 1;
        }
        _nextFade = null;
    }

    /// <summary>Play pressed (the game starts): every sound, the fire's loop too, fades out over
    /// <see cref="GameStartFadeSeconds"/> and stays muted, also after the game exits (the speaker button unmutes).</summary>
    public static void MuteForGame()
    {
        _nextFade = GameStartFadeSeconds;
        Settings.MuteWithoutSaving();
        _nextFade = null;
    }

    internal static void SaveSettings()
    {
        if (_settingsPath is null)
        {
            return;
        }
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath)!);
            File.WriteAllText(_settingsPath, JsonSerializer.Serialize(new { muted = Settings.Muted }));
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException)
        {
            // Not remembered; nothing else depends on it.
        }
    }

    public static void Play(Sfx kind)
    {
        if (_engine is null || Settings.Muted || kind == Sfx.None || !_clips.TryGetValue(kind, out var clip))
        {
            return;
        }
        // No spam: one of each sound per 45 ms (typing, a burst of clicks).
        var now = DateTime.UtcNow;
        if (LastPlayed.TryGetValue(kind, out var last) && (now - last).TotalMilliseconds < 45)
        {
            return;
        }
        LastPlayed[kind] = now;
        var (volume, pitch) = VolumeAndPitch(kind, Rng);
        _engine.Play(clip, volume, pitch);
    }

    static void OnClick(object sender, RoutedEventArgs e)
    {
        if (!ReferenceEquals(sender, e.OriginalSource) && !ReferenceEquals(sender, e.Source))
        {
            return;
        }
        var d = (DependencyObject)sender;
        var kind = d.ReadLocalValue(KindProperty) != DependencyProperty.UnsetValue || d.GetValue(KindProperty) is not Sfx.Click
            ? GetKind(d)
            : sender is ToggleButton ? Sfx.Toggle : Sfx.Click;
        Play(kind);
    }
}
