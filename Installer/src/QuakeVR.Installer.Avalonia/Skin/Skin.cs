using Avalonia;
using Avalonia.Animation;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;

namespace QuakeVR.Installer.Skin;

/// <summary>
/// The skin's textures as tiled brushes in the app's resources (SkinBackdropBrush, SkinPanelBrush, SkinPlateBrush,
/// SkinTrimBrush), and the lava for the progress bar: the generated ones (Procedural, the WPF skin's own code). The
/// player's Quake's textures (the WPF skin's SkinLoader) are not read here yet.
/// </summary>
static class SkinTextures
{
    public static byte[] LavaBgra { get; private set; } = [];
    public static int LavaSize { get; private set; }
    public static uint[] FireRamp { get; private set; } = Procedural.FireRamp(null);
    public static string Description { get; private set; } = "none";

    public static WriteableBitmap Bitmap(byte[] bgra, int width, int height)
    {
        var bitmap = new WriteableBitmap(new PixelSize(width, height), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);
        using var fb = bitmap.Lock();
        for (var y = 0; y < height; ++y)
        {
            System.Runtime.InteropServices.Marshal.Copy(bgra, y * width * 4, fb.Address + y * fb.RowBytes, width * 4);
        }
        return bitmap;
    }

    static ImageBrush Tile(byte[] bgra, int size, double scale) => new(Bitmap(bgra, size, size))
    {
        TileMode = TileMode.Tile,
        Stretch = Stretch.Fill,
        DestinationRect = new RelativeRect(0, 0, size * scale, size * scale, RelativeUnit.Absolute),
    };

    public static void ApplyGenerated()
    {
        if (Application.Current is not { } app)
        {
            return;
        }
        app.Resources["SkinBackdropBrush"] = Tile(Procedural.Bricks(128, 11), 128, 1);
        app.Resources["SkinPanelBrush"] = Tile(Procedural.Metal(128, 23, rivets: true, rust: 0.55f), 128, 1);
        app.Resources["SkinPlateBrush"] = Tile(Procedural.Metal(128, 37, rivets: false, rust: 0.7f), 128, 1);
        app.Resources["SkinTrimBrush"] = Tile(Procedural.Metal(64, 41, rivets: false, rust: 0.6f), 64, 1);
        LavaBgra = Procedural.Lava(64);
        LavaSize = 64;
        FireRamp = Procedural.FireRamp(null);
        Description = "generated textures (Procedural)";
    }
}

/// <summary>The animations' clock: one timer at 60 Hz for every animated control in view; none in the headless
/// harness, which steps them by hand (<see cref="Step"/>).</summary>
static class FrameClock
{
    static readonly List<Action<double>> Listeners = [];
    static DispatcherTimer? _timer;
    static DateTime _last;

    /// <summary>The harness: no timer; it calls <see cref="Step"/>.</summary>
    public static bool Manual { get; set; }

    public static void Subscribe(Action<double> listener)
    {
        Listeners.Add(listener);
        if (!Manual && _timer is null)
        {
            _last = DateTime.UtcNow;
            _timer = new DispatcherTimer(TimeSpan.FromSeconds(1 / 60.0), DispatcherPriority.Render, (_, _) =>
            {
                var now = DateTime.UtcNow;
                Step(Math.Min(0.1, (now - _last).TotalSeconds));
                _last = now;
            });
            _timer.Start();
        }
    }

    public static void Unsubscribe(Action<double> listener) => Listeners.Remove(listener);

    public static void Step(double dt)
    {
        foreach (var l in Listeners.ToArray())
        {
            l(dt);
        }
    }
}

/// <summary>A control that draws itself each frame while it is in view.</summary>
abstract class AnimatedControl : Control
{
    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnAttachedToVisualTree(e);
        FrameClock.Subscribe(Tick);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        base.OnDetachedFromVisualTree(e);
        FrameClock.Unsubscribe(Tick);
    }

    void Tick(double dt)
    {
        if (IsEffectivelyVisible)
        {
            Advance(dt);
            InvalidateVisual();
        }
    }

    public abstract void Advance(double dt);
}

/// <summary>
/// Flames rising from the bottom edge, embers drifting up, and a flickering glow: the WPF FireView's "Doom fire"
/// (heat rising a row a step, drifting sideways, cooling at random) coloured by the skin's fire ramp, drawn smoothly
/// upscaled, with the same properties.
/// </summary>
sealed class FireView : AnimatedControl
{
    const double StepRate = 50;
    static readonly float[] CoolTile = Procedural.Fbm(128, 8, 8, 3, 5, 0.6f);
    static readonly IBrush[] EmberBrushes = [EmberBrush(0xFFFFE8B8, 0xFFFFA040), EmberBrush(0xFFFFD080, 0xFFFF6A1A), EmberBrush(0xFFFFF4D8, 0xFFFFB040)];
    static readonly IBrush PoolMask = new RadialGradientBrush
    {
        Center = new RelativePoint(0.5, 0.36, RelativeUnit.Relative),
        GradientOrigin = new RelativePoint(0.5, 0.36, RelativeUnit.Relative),
        RadiusX = new RelativeScalar(0.62, RelativeUnit.Relative),
        RadiusY = new RelativeScalar(0.6, RelativeUnit.Relative),
        GradientStops = { new GradientStop(Color.FromArgb(255, 0, 0, 0), 0.45), new GradientStop(Color.FromArgb(0, 0, 0, 0), 1) },
    };

    public static readonly StyledProperty<double> CellSizeProperty = AvaloniaProperty.Register<FireView, double>(nameof(CellSize), 4.0);
    public static readonly StyledProperty<bool> ShowFireProperty = AvaloniaProperty.Register<FireView, bool>(nameof(ShowFire), true);
    public static readonly StyledProperty<double> HeatProperty = AvaloniaProperty.Register<FireView, double>(nameof(Heat), 0.7);
    public static readonly StyledProperty<int> EmberCountProperty = AvaloniaProperty.Register<FireView, int>(nameof(EmberCount), 16);
    public static readonly StyledProperty<double> GlowProperty = AvaloniaProperty.Register<FireView, double>(nameof(Glow), 0.0);
    public static readonly StyledProperty<double> FireOpacityProperty = AvaloniaProperty.Register<FireView, double>(nameof(FireOpacity), 1.0);

    public double CellSize { get => GetValue(CellSizeProperty); set => SetValue(CellSizeProperty, value); }
    public bool ShowFire { get => GetValue(ShowFireProperty); set => SetValue(ShowFireProperty, value); }
    public double Heat { get => GetValue(HeatProperty); set => SetValue(HeatProperty, value); }
    public int EmberCount { get => GetValue(EmberCountProperty); set => SetValue(EmberCountProperty, value); }
    public double Glow { get => GetValue(GlowProperty); set => SetValue(GlowProperty, value); }
    public double FireOpacity { get => GetValue(FireOpacityProperty); set => SetValue(FireOpacityProperty, value); }

    sealed class Ember
    {
        public double X, Y, Vx, Vy, Age, Life, Size, Phase;
        public IBrush Brush = Brushes.Transparent;
    }

    readonly Random _rng = new(1996);
    readonly List<Ember> _embers = [];
    float[] _heat = [];
    byte[] _pixels = [];
    WriteableBitmap? _frame;
    int _cols;
    int _rows;
    int _step;
    double _accumulator;
    double _time;
    double _glowLevel;
    Size _built;

    static IBrush EmberBrush(uint core, uint mid)
    {
        static Color C(uint v) => Color.FromArgb((byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8), (byte)v);
        return new RadialGradientBrush
        {
            GradientStops = { new GradientStop(C(core), 0), new GradientStop(C(mid), 0.35), new GradientStop(Color.FromArgb(0, C(mid).R, C(mid).G, 0), 1) },
        };
    }

    protected override void OnPropertyChanged(AvaloniaPropertyChangedEventArgs change)
    {
        base.OnPropertyChanged(change);
        if (change.Property == CellSizeProperty || change.Property == ShowFireProperty || change.Property == HeatProperty ||
            change.Property == EmberCountProperty || change.Property == GlowProperty || change.Property == FireOpacityProperty)
        {
            _built = default;
        }
    }

    void Rebuild()
    {
        var w = Bounds.Width;
        var h = Bounds.Height;
        _built = Bounds.Size;
        if (w < 1 || h < 1)
        {
            return;
        }
        var cell = Math.Max(1, CellSize);
        _cols = ShowFire ? Math.Max(8, (int)Math.Ceiling(w / cell)) : 0;
        _rows = ShowFire ? Math.Max(8, (int)Math.Ceiling(h / cell)) : 0;
        _heat = new float[(_rows + 2) * _cols];
        _pixels = new byte[_rows * _cols * 4];
        _frame = _cols > 0 ? new WriteableBitmap(new PixelSize(_cols, _rows), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul) : null;
        _embers.Clear();
        for (var i = 0; i < Math.Max(0, EmberCount); ++i)
        {
            var e = new Ember();
            Spawn(e, scatter: true);
            _embers.Add(e);
        }
        // Start burning: the first frame shows full flames.
        for (var i = 0; i < _rows * 2 + 30; ++i)
        {
            Step();
        }
        Paint();
        _glowLevel = Glow;
    }

    void Spawn(Ember e, bool scatter)
    {
        var w = Math.Max(1, Bounds.Width);
        var h = Math.Max(1, Bounds.Height);
        e.X = w * (0.04 + _rng.NextDouble() * 0.92);
        e.Y = h - _rng.NextDouble() * h * 0.25;
        e.Vx = (_rng.NextDouble() - 0.5) * 16;
        e.Vy = -(22 + _rng.NextDouble() * 48);
        e.Life = 2.2 + _rng.NextDouble() * 3.2;
        e.Age = scatter ? _rng.NextDouble() * e.Life : 0;
        e.Size = 0.9 + _rng.NextDouble() * 1.8;
        e.Phase = _rng.NextDouble() * Math.PI * 2;
        e.Brush = EmberBrushes[_rng.Next(EmberBrushes.Length)];
        if (scatter)
        {
            e.Y += e.Vy * e.Age;
        }
    }

    void Step()
    {
        if (_cols == 0)
        {
            return;
        }
        var cols = _cols;
        var rows = _rows;
        var h = _heat;
        ++_step;
        var fuelRow = ((_step / 3) & 127) * 128;
        for (var x = 0; x < cols; ++x)
        {
            var fuel = 0.55f + CoolTile[fuelRow + ((x * 3) & 127)] * 0.7f;
            if (_rng.Next(8) == 0)
            {
                fuel *= 0.6f;
            }
            h[(rows + 1) * cols + x] = fuel;
        }
        var decay = 2.2f / (float)Math.Max(4, Heat * rows);
        for (var y = 0; y <= rows; ++y)
        {
            var src = (y + 1) * cols;
            var dst = y * cols;
            for (var x = 0; x < cols; ++x)
            {
                var v = h[src + x];
                if (v <= 0)
                {
                    h[dst + x] = 0;
                    continue;
                }
                var r = _rng.Next(4);
                var to = x + (r == 0 ? -1 : r == 3 ? 1 : 0);
                to = to < 0 ? 0 : to >= cols ? cols - 1 : to;
                v -= (r & 1) * decay;
                h[dst + to] = v > 0 ? v : 0;
            }
        }
    }

    void Paint()
    {
        if (_frame is null)
        {
            return;
        }
        var ramp = SkinTextures.FireRamp;
        var cols = _cols;
        var h = _heat;
        for (var y = 0; y < _rows; ++y)
        {
            for (var x = 0; x < cols; ++x)
            {
                var i = y * cols + x;
                var l = x > 0 ? h[i - 1] : h[i];
                var r = x < cols - 1 ? h[i + 1] : h[i];
                var up = y > 0 ? h[i - cols] : h[i];
                var v = (h[i] * 4 + l + r + up + h[i + cols]) * (255f / 8);
                var c = ramp[v >= 255 ? 255 : (int)v];
                _pixels[i * 4] = (byte)c;
                _pixels[i * 4 + 1] = (byte)(c >> 8);
                _pixels[i * 4 + 2] = (byte)(c >> 16);
                _pixels[i * 4 + 3] = (byte)(c >> 24);
            }
        }
        using var fb = _frame.Lock();
        for (var y = 0; y < _rows; ++y)
        {
            System.Runtime.InteropServices.Marshal.Copy(_pixels, y * cols * 4, fb.Address + y * fb.RowBytes, cols * 4);
        }
    }

    /// <summary>For the harness's report: the simulation's size and its hottest cell.</summary>
    internal string Describe() => $"{Bounds.Width:0}x{Bounds.Height:0} cells {_cols}x{_rows} max heat {(_heat.Length > 0 ? _heat.Max() : 0):0.00} embers {_embers.Count}";

    public override void Advance(double dt)
    {
        if (_built != Bounds.Size)
        {
            return;
        }
        _time += dt;
        if (_cols > 0)
        {
            _accumulator += dt;
            var steps = 0;
            while (_accumulator >= 1 / StepRate && steps < 6)
            {
                Step();
                _accumulator -= 1 / StepRate;
                ++steps;
            }
            if (steps > 0)
            {
                Paint();
            }
        }
        _glowLevel = Glow * (_cols > 0 ? 0.85 + 0.15 * Math.Sin(_time * 7.3) * Math.Sin(_time * 2.9 + 1) : 1);
        foreach (var e in _embers)
        {
            e.Age += dt;
            e.X += (e.Vx + Math.Sin(_time * 1.7 + e.Phase) * 10) * dt;
            e.Y += e.Vy * dt;
            if (e.Age >= e.Life || e.Y < -10)
            {
                Spawn(e, scatter: false);
            }
        }
    }

    public override void Render(DrawingContext dc)
    {
        if (_built != Bounds.Size)
        {
            Rebuild();
        }
        var w = Bounds.Width;
        var h = Bounds.Height;
        if (Glow > 0)
        {
            var glow = new LinearGradientBrush
            {
                StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
                GradientStops =
                {
                    new GradientStop(Color.FromArgb(0, 255, 110, 20), 0),
                    new GradientStop(Color.FromArgb(70, 255, 110, 20), 0.7),
                    new GradientStop(Color.FromArgb(150, 255, 140, 40), 1),
                },
            };
            using (dc.PushOpacity(_glowLevel))
            using (ShowFire ? dc.PushOpacityMask(PoolMask, new Rect(0, 0, w, h)) : default(DrawingContext.PushedState?))
            {
                dc.DrawRectangle(glow, null, new Rect(0, h * 0.35, w, h * 0.65));
            }
        }
        if (_frame is not null)
        {
            var cell = Math.Max(1, CellSize);
            using (dc.PushOpacity(FireOpacity))
            using (dc.PushOpacityMask(PoolMask, new Rect(0, 0, w, h)))
            using (dc.PushRenderOptions(new RenderOptions { BitmapInterpolationMode = BitmapInterpolationMode.HighQuality }))
            {
                dc.DrawImage(_frame, new Rect(0, 0, _cols, _rows), new Rect(0, 0, _cols * cell, _rows * cell));
            }
        }
        foreach (var e in _embers)
        {
            var life = e.Age / e.Life;
            var opacity = Math.Clamp(Math.Sin(Math.PI * life) * (0.65 + 0.35 * Math.Sin(_time * 11 + e.Phase * 3)), 0, 1);
            var r = e.Size * 2.4;
            using (dc.PushOpacity(opacity))
            {
                dc.DrawEllipse(e.Brush, null, new Point(e.X, e.Y), r, r);
            }
        }
    }
}

/// <summary>Lava, warped the way Quake warps its liquids and slowly flowing, tiled: the progress bar's fill.</summary>
sealed class LavaView : AnimatedControl
{
    byte[] _warped = [];
    WriteableBitmap? _frame;
    double _time;

    void Warp()
    {
        var n = SkinTextures.LavaSize;
        var source = SkinTextures.LavaBgra;
        if (n == 0)
        {
            return;
        }
        if (_frame is null || _warped.Length != source.Length)
        {
            _warped = new byte[source.Length];
            _frame = new WriteableBitmap(new PixelSize(n, n), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);
        }
        var amp = n / 16.0;
        var flow = _time * 5;
        for (var y = 0; y < n; ++y)
        {
            for (var x = 0; x < n; ++x)
            {
                var sx = (int)Math.Floor(x + amp * Math.Sin(y * 2 * Math.PI / n + _time * 1.4) + flow);
                var sy = (int)Math.Floor(y + amp * Math.Sin(x * 2 * Math.PI / n + _time * 1.4));
                var si = ((((sy % n) + n) % n) * n + ((sx % n) + n) % n) * 4;
                var di = (y * n + x) * 4;
                _warped[di] = source[si];
                _warped[di + 1] = source[si + 1];
                _warped[di + 2] = source[si + 2];
                _warped[di + 3] = 255;
            }
        }
        using var fb = _frame.Lock();
        for (var y = 0; y < n; ++y)
        {
            System.Runtime.InteropServices.Marshal.Copy(_warped, y * n * 4, fb.Address + y * fb.RowBytes, n * 4);
        }
    }

    public override void Advance(double dt)
    {
        _time += dt;
        Warp();
    }

    public override void Render(DrawingContext dc)
    {
        if (_frame is null)
        {
            Warp();
        }
        if (_frame is null)
        {
            return;
        }
        var n = SkinTextures.LavaSize;
        var brush = new ImageBrush(_frame)
        {
            TileMode = TileMode.Tile,
            Stretch = Stretch.Fill,
            DestinationRect = new RelativeRect(0, 0, n, n, RelativeUnit.Absolute),
        };
        dc.DrawRectangle(brush, null, new Rect(Bounds.Size));
    }
}

/// <summary>Attached effects: Enter (a page fades and rises in when it appears) and Pulse (a slow glow of the
/// opacity, down to the value given).</summary>
static class Fx
{
    public static readonly AttachedProperty<bool> EnterProperty = AvaloniaProperty.RegisterAttached<Control, bool>("Enter", typeof(Fx));
    public static readonly AttachedProperty<double> PulseProperty = AvaloniaProperty.RegisterAttached<Control, double>("Pulse", typeof(Fx), 1.0);

    public static bool GetEnter(Control c) => c.GetValue(EnterProperty);
    public static void SetEnter(Control c, bool value) => c.SetValue(EnterProperty, value);
    public static double GetPulse(Control c) => c.GetValue(PulseProperty);
    public static void SetPulse(Control c, double value) => c.SetValue(PulseProperty, value);

    static Fx()
    {
        EnterProperty.Changed.AddClassHandler<Control>((c, e) =>
        {
            if (e.NewValue is true)
            {
                c.Transitions ??= [];
                c.Transitions.Add(new DoubleTransition { Property = Visual.OpacityProperty, Duration = TimeSpan.FromSeconds(0.28) });
                c.PropertyChanged += (_, p) =>
                {
                    if (p.Property == Visual.IsVisibleProperty && p.NewValue is true && !FrameClock.Manual)
                    {
                        c.Opacity = 0;
                        Dispatcher.UIThread.Post(() => c.Opacity = 1, DispatcherPriority.Background);
                    }
                };
            }
        });
        PulseProperty.Changed.AddClassHandler<Control>((c, e) =>
        {
            if (e.NewValue is double low && low < 1)
            {
                var time = 0.0;
                FrameClock.Subscribe(dt =>
                {
                    time += dt;
                    c.Opacity = low + (1 - low) * (0.5 + 0.5 * Math.Cos(time * 2.2));
                });
            }
        });
    }
}
