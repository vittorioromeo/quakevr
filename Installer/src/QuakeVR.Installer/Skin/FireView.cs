using System.Globalization;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace QuakeVR.Installer.Skin;

/// <summary>
/// Flames rising from the bottom edge, embers drifting up, and a flickering glow. The flames are the classic "Doom
/// fire" (heat rising a row a step, drifting sideways, cooling at random) coloured by the skin's fire ramp (Quake's
/// palette once Quake is found) and drawn smoothly upscaled. Each ember is its own tiny visual moved by a transform, so
/// a frame only redraws the few small rectangles they cover, not the page behind them. Still when motion is off.
/// </summary>
sealed class FireView : AnimatedElement
{
    const double StepRate = 50; // Simulation steps per second (independent of the frame rate).
    static readonly float[] CoolTile = Procedural.Fbm(128, 8, 8, 3, 5, 0.6f);
    static readonly Brush[] EmberBrushes = [EmberBrush(0xFFFFE8B8, 0xFFFFA040), EmberBrush(0xFFFFD080, 0xFFFF6A1A), EmberBrush(0xFFFFF4D8, 0xFFFFB040)];
    static readonly Brush PoolMask = MakePoolMask();

    public static readonly DependencyProperty CellSizeProperty = DependencyProperty.Register(nameof(CellSize), typeof(double), typeof(FireView),
        new FrameworkPropertyMetadata(4.0, (d, _) => ((FireView)d).Rebuild()));
    public static readonly DependencyProperty ShowFireProperty = DependencyProperty.Register(nameof(ShowFire), typeof(bool), typeof(FireView),
        new FrameworkPropertyMetadata(true, (d, _) => ((FireView)d).Rebuild()));
    public static readonly DependencyProperty HeatProperty = DependencyProperty.Register(nameof(Heat), typeof(double), typeof(FireView),
        new FrameworkPropertyMetadata(0.7, (d, _) => ((FireView)d).Rebuild()));
    public static readonly DependencyProperty EmberCountProperty = DependencyProperty.Register(nameof(EmberCount), typeof(int), typeof(FireView),
        new FrameworkPropertyMetadata(16, (d, _) => ((FireView)d).Rebuild()));
    public static readonly DependencyProperty GlowProperty = DependencyProperty.Register(nameof(Glow), typeof(double), typeof(FireView),
        new FrameworkPropertyMetadata(0.0, (d, _) => ((FireView)d).Rebuild()));
    public static readonly DependencyProperty FireOpacityProperty = DependencyProperty.Register(nameof(FireOpacity), typeof(double), typeof(FireView),
        new FrameworkPropertyMetadata(1.0, (d, _) => ((FireView)d).Rebuild()));

    /// <summary>Pixels per simulation cell (bigger: chunkier, cheaper flames).</summary>
    public double CellSize { get => (double)GetValue(CellSizeProperty); set => SetValue(CellSizeProperty, value); }
    /// <summary>Flames (with an oval mask melting their edges); false: only embers and the glow.</summary>
    public bool ShowFire { get => (bool)GetValue(ShowFireProperty); set => SetValue(ShowFireProperty, value); }
    /// <summary>How high the flames reach, as a fraction of the height.</summary>
    public double Heat { get => (double)GetValue(HeatProperty); set => SetValue(HeatProperty, value); }
    public int EmberCount { get => (int)GetValue(EmberCountProperty); set => SetValue(EmberCountProperty, value); }
    /// <summary>The opacity of the warm glow along the bottom edge (0: none).</summary>
    public double Glow { get => (double)GetValue(GlowProperty); set => SetValue(GlowProperty, value); }
    public double FireOpacity { get => (double)GetValue(FireOpacityProperty); set => SetValue(FireOpacityProperty, value); }

    sealed class Ember
    {
        public double X, Y, Vx, Vy, Age, Life, Size, Phase;
        public readonly DrawingVisual Visual = new();
        public readonly TranslateTransform Move = new();
    }

    readonly Random _rng = new(1996);
    readonly VisualCollection _children;
    readonly DrawingVisual _glowVisual = new();
    readonly DrawingVisual _fireVisual = new();
    readonly List<Ember> _embers = [];
    float[] _heat = [];
    uint[] _pixels = [];
    int _cols;
    int _rows;
    int _step;
    double _accumulator;
    double _time;
    uint[]? _ramp;

    public FireView()
    {
        _children = new VisualCollection(this) { _glowVisual, _fireVisual };
        SkinResources.Changed += () => { _ramp = null; Paint(); };
    }

    protected override bool RedrawEveryTick => false;
    protected override int VisualChildrenCount => _children?.Count ?? 0; // Asked by the base constructor already.
    protected override Visual GetVisualChild(int index) => _children[index];

    static Brush EmberBrush(uint core, uint mid)
    {
        static Color C(uint v) => Color.FromArgb((byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8), (byte)v);
        var b = new RadialGradientBrush(
        [
            new GradientStop(C(core), 0),
            new GradientStop(C(mid), 0.35),
            new GradientStop(Color.FromArgb(0, C(mid).R, C(mid).G, 0), 1),
        ]);
        b.Freeze();
        return b;
    }

    /// <summary>An oval pool of fire: opaque in the middle, melting away at the sides and at the bottom edge.</summary>
    static Brush MakePoolMask()
    {
        var b = new RadialGradientBrush(
        [
            new GradientStop(Color.FromArgb(255, 0, 0, 0), 0.45),
            new GradientStop(Color.FromArgb(0, 0, 0, 0), 1),
        ])
        { Center = new Point(0.5, 0.36), GradientOrigin = new Point(0.5, 0.36), RadiusX = 0.62, RadiusY = 0.6 };
        b.Freeze();
        return b;
    }

    protected override void OnRenderSizeChanged(SizeChangedInfo sizeInfo)
    {
        base.OnRenderSizeChanged(sizeInfo);
        Rebuild();
    }

    void Rebuild()
    {
        var w = ActualWidth;
        var h = ActualHeight;
        if (w < 1 || h < 1)
        {
            return;
        }
        var cell = Math.Max(1, CellSize);
        _cols = ShowFire ? Math.Max(8, (int)Math.Ceiling(w / cell)) : 0;
        _rows = ShowFire ? Math.Max(8, (int)Math.Ceiling(h / cell)) : 0;
        _heat = new float[(_rows + 2) * _cols];
        _pixels = new uint[_rows * _cols];
        _fireVisual.Opacity = FireOpacity;
        _fireVisual.OpacityMask = PoolMask;
        // The glow: drawn once; it flickers by its opacity (behind flames only: the page's own glow stays still).
        using (var dc = _glowVisual.RenderOpen())
        {
            if (Glow > 0)
            {
                var glow = new LinearGradientBrush(
                [
                    new GradientStop(Color.FromArgb(0, 255, 110, 20), 0),
                    new GradientStop(Color.FromArgb(70, 255, 110, 20), 0.7),
                    new GradientStop(Color.FromArgb(150, 255, 140, 40), 1),
                ], 90);
                dc.DrawRectangle(glow, null, new Rect(0, h * 0.35, w, h * 0.65));
            }
        }
        _glowVisual.Opacity = Glow;
        _glowVisual.OpacityMask = ShowFire ? PoolMask : null;
        // The embers: one small visual each.
        foreach (var e in _embers)
        {
            _children.Remove(e.Visual);
        }
        _embers.Clear();
        for (var i = 0; i < Math.Max(0, EmberCount); ++i)
        {
            var e = new Ember();
            e.Visual.Transform = e.Move;
            Spawn(e, scatter: true);
            _embers.Add(e);
            _children.Add(e.Visual);
        }
        // Start burning: the first frame (and a still one when motion is off) shows full flames.
        for (var i = 0; i < _rows * 2 + 30; ++i)
        {
            Step();
        }
        Paint();
        PlaceEmbers();
    }

    void Spawn(Ember e, bool scatter)
    {
        var w = Math.Max(1, ActualWidth);
        var h = Math.Max(1, ActualHeight);
        e.X = w * (0.04 + _rng.NextDouble() * 0.92);
        e.Y = h - _rng.NextDouble() * h * 0.25;
        e.Vx = (_rng.NextDouble() - 0.5) * 16;
        e.Vy = -(22 + _rng.NextDouble() * 48);
        e.Life = 2.2 + _rng.NextDouble() * 3.2;
        e.Age = scatter ? _rng.NextDouble() * e.Life : 0;
        e.Size = 0.9 + _rng.NextDouble() * 1.8;
        e.Phase = _rng.NextDouble() * Math.PI * 2;
        if (scatter)
        {
            e.Y += e.Vy * e.Age;
        }
        var r = e.Size * 2.4;
        using var dc = e.Visual.RenderOpen();
        dc.DrawEllipse(EmberBrushes[_rng.Next(EmberBrushes.Length)], null, new Point(0, 0), r, r);
    }

    /// <summary>One step of the heat simulation: the classic "Doom fire". Each cell's heat moves up one row, drifting
    /// a cell left or right at random and losing a little heat half the time, which makes flickering tongues. The fuel
    /// below the view burns unevenly along a slowly moving noise, so the flames rise in clumps.</summary>
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
                fuel *= 0.6f; // A gap: flames break apart.
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

    /// <summary>The heat as colours, lightly blurred (the simulation's speckle becomes soft flame), into a new frozen
    /// bitmap: the same on screen and off screen.</summary>
    void Paint()
    {
        if (_cols == 0)
        {
            using (_fireVisual.RenderOpen())
            {
            }
            return;
        }
        _ramp ??= SkinResources.Current?.FireRamp ?? Procedural.FireRamp(null);
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
                _pixels[i] = _ramp[v >= 255 ? 255 : (int)v];
            }
        }
        var frame = BitmapSource.Create(_cols, _rows, 96, 96, PixelFormats.Pbgra32, null, _pixels, _cols * 4);
        frame.Freeze();
        var cell = Math.Max(1, CellSize);
        using var dc = _fireVisual.RenderOpen();
        dc.DrawImage(frame, new Rect(0, 0, _cols * cell, _rows * cell));
    }

    void PlaceEmbers()
    {
        foreach (var e in _embers)
        {
            e.Move.X = e.X;
            e.Move.Y = e.Y;
            var life = e.Age / e.Life;
            e.Visual.Opacity = Math.Clamp(Math.Sin(Math.PI * life) * (0.65 + 0.35 * Math.Sin(_time * 11 + e.Phase * 3)), 0, 1);
        }
    }

    /// <summary>For the harness's report: the simulation's size and its hottest cell.</summary>
    internal string Describe() => $"{ActualWidth:0}x{ActualHeight:0} cells {_cols}x{_rows} max heat {(_heat.Length > 0 ? _heat.Max() : 0):0.00} embers {_embers.Count}";

    public override void Advance(double dt)
    {
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
            if (Glow > 0)
            {
                _glowVisual.Opacity = Glow * (0.85 + 0.15 * Math.Sin(_time * 7.3) * Math.Sin(_time * 2.9 + 1));
            }
        }
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
        PlaceEmbers();
    }
}

/// <summary>Lava, warped the way Quake warps its liquids (each texel displaced by sines of the other axis) and slowly
/// flowing, tiled at the skin's texture scale: the progress bar's fill.</summary>
sealed class LavaView : AnimatedElement
{
    ImageBrush? _brush;
    bool _tiledLinear;
    byte[] _source = [];
    byte[] _warped = [];
    int _size;
    double _time;

    public LavaView()
    {
        SkinResources.Changed += () => { Build(); InvalidateVisual(); };
        Build();
    }

    void Build()
    {
        if (SkinResources.Current is not { } skin)
        {
            return;
        }
        _size = skin.LavaSize;
        _source = skin.LavaBgra;
        _warped = new byte[_source.Length];
        _tiledLinear = !skin.FromQuake;
        Warp();
    }

    void Warp()
    {
        if (_size == 0)
        {
            return;
        }
        var n = _size;
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
                _warped[di] = _source[si];
                _warped[di + 1] = _source[si + 1];
                _warped[di + 2] = _source[si + 2];
                _warped[di + 3] = 255;
            }
        }
        var frame = BitmapSource.Create(n, n, 96, 96, PixelFormats.Pbgra32, null, _warped, n * 4);
        frame.Freeze();
        var brush = new ImageBrush(frame)
        {
            TileMode = TileMode.Tile,
            ViewportUnits = BrushMappingMode.Absolute,
            Viewport = new Rect(0, 0, n, n),
        };
        RenderOptions.SetBitmapScalingMode(brush, _tiledLinear ? BitmapScalingMode.Linear : BitmapScalingMode.NearestNeighbor);
        brush.Freeze();
        _brush = brush;
    }

    public override void Advance(double dt)
    {
        _time += dt;
        Warp();
    }

    protected override void OnRender(DrawingContext dc)
    {
        if (_brush is not null)
        {
            dc.DrawRectangle(_brush, null, new Rect(RenderSize));
        }
    }
}

/// <summary>A number in Quake's big status-bar digits once Quake is found (other characters, and every character
/// without Quake, in the skin's condensed face).</summary>
sealed class QuakeDigits : FrameworkElement
{
    public static readonly DependencyProperty TextProperty = DependencyProperty.Register(nameof(Text), typeof(string), typeof(QuakeDigits),
        new FrameworkPropertyMetadata("", FrameworkPropertyMetadataOptions.AffectsMeasure | FrameworkPropertyMetadataOptions.AffectsRender));
    public static readonly DependencyProperty DigitHeightProperty = DependencyProperty.Register(nameof(DigitHeight), typeof(double), typeof(QuakeDigits),
        new FrameworkPropertyMetadata(32.0, FrameworkPropertyMetadataOptions.AffectsMeasure | FrameworkPropertyMetadataOptions.AffectsRender));
    public static readonly DependencyProperty ForegroundProperty = DependencyProperty.Register(nameof(Foreground), typeof(Brush), typeof(QuakeDigits),
        new FrameworkPropertyMetadata(Brushes.White, FrameworkPropertyMetadataOptions.AffectsRender));

    public string Text { get => (string)GetValue(TextProperty); set => SetValue(TextProperty, value); }
    public double DigitHeight { get => (double)GetValue(DigitHeightProperty); set => SetValue(DigitHeightProperty, value); }
    public Brush Foreground { get => (Brush)GetValue(ForegroundProperty); set => SetValue(ForegroundProperty, value); }

    public QuakeDigits()
    {
        RenderOptions.SetBitmapScalingMode(this, BitmapScalingMode.NearestNeighbor);
        SkinResources.Changed += () => { InvalidateMeasure(); InvalidateVisual(); };
    }

    IReadOnlyDictionary<char, BitmapSource>? Digits => SkinResources.Current?.Digits is { Count: 10 } d ? d : null;

    FormattedText Format(string s, double size) => new(s, CultureInfo.InvariantCulture, FlowDirection.LeftToRight,
        new Typeface((FontFamily)FindResource("CondensedFont"), FontStyles.Normal, FontWeights.Bold, FontStretches.Normal), size, Foreground,
        VisualTreeHelper.GetDpi(this).PixelsPerDip);

    protected override Size MeasureOverride(Size availableSize)
    {
        var h = DigitHeight;
        if (Digits is null)
        {
            var f = Format(Text ?? "", h);
            return new Size(f.WidthIncludingTrailingWhitespace, h);
        }
        double w = 0;
        foreach (var c in Text ?? "")
        {
            w += char.IsAsciiDigit(c) ? h : Format(c.ToString(), h * 0.7).WidthIncludingTrailingWhitespace + 2;
        }
        return new Size(w, h);
    }

    protected override void OnRender(DrawingContext dc)
    {
        var h = DigitHeight;
        if (Digits is not { } digits)
        {
            var f = Format(Text ?? "", h);
            dc.DrawText(f, new Point(0, (h - f.Height) / 2));
            return;
        }
        double x = 0;
        foreach (var c in Text ?? "")
        {
            if (char.IsAsciiDigit(c))
            {
                dc.DrawImage(digits[c], new Rect(x, 0, h, h));
                x += h;
            }
            else
            {
                var f = Format(c.ToString(), h * 0.7);
                dc.DrawText(f, new Point(x + 2, h - f.Height));
                x += f.WidthIncludingTrailingWhitespace + 2;
            }
        }
    }
}
