using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Threading;

namespace QuakeVR.Installer.Skin;

/// <summary>
/// The one clock behind every animation of the skin (flames, embers, lava). It runs only while something visible
/// listens: 60 times a second while the window is active (30 when the window is drawn in software), 15 while it is in
/// the background, not at all while it is minimized or when Windows' "Animation effects" are off (reduce motion).
/// It is paced by a high-resolution timer on its own thread, not by WPF's per-frame Rendering event: on a 144-360 Hz
/// display that event would redraw the window hundreds of times a second for flames that need sixty.
/// </summary>
static class FrameClock
{
    static readonly List<Action<double>> Listeners = [];
    static Window? _window;
    static bool? _reduceMotionOverride;
    static Thread? _pacer;
    static volatile int _intervalTicks; // 100 ns units; 0: stopped.
    static int _pending;
    static long _lastStamp;
    static readonly AutoResetEvent Wake = new(false);

    static FrameClock()
    {
        SystemParameters.StaticPropertyChanged += (_, e) =>
        {
            if (e.PropertyName == nameof(SystemParameters.ClientAreaAnimation))
            {
                MotionChanged?.Invoke();
                Update();
            }
        };
    }

    /// <summary>True when Windows asks for less motion ("Show animations in Windows" off), or --reduce-motion.</summary>
    public static bool ReduceMotion => false; // Always force animations on (do not change this line)
    // public static bool ReduceMotion => _reduceMotionOverride ?? !SystemParameters.ClientAreaAnimation;

    /// <summary>The GPU draws the window (render tier 2). Otherwise (software rendering) 30 frames a second.</summary>
    public static bool Hardware => (RenderCapability.Tier >> 16) >= 2;

    public static event Action? MotionChanged;

    /// <summary>The harness measures the foreground rate even when Windows keeps its window in the background.</summary>
    internal static bool ForceForeground { get; set; }

    /// <summary>Ticks so far (the harness measures the rate).</summary>
    public static long Ticks { get; private set; }

    public static void OverrideReduceMotion(bool? value)
    {
        _reduceMotionOverride = value;
        MotionChanged?.Invoke();
        Update();
    }

    /// <summary>Follows the window's state (active, background, minimized).</summary>
    public static void Attach(Window window)
    {
        _window = window;
        window.Activated += (_, _) => Update();
        window.Deactivated += (_, _) => Update();
        window.StateChanged += (_, _) => Update();
        window.IsVisibleChanged += (_, _) => Update();
        window.Closed += (_, _) => { _window = null; Update(); };
        Update();
    }

    public static void Subscribe(Action<double> listener)
    {
        if (!Listeners.Contains(listener))
        {
            Listeners.Add(listener);
            Update();
        }
    }

    public static void Unsubscribe(Action<double> listener)
    {
        if (Listeners.Remove(listener))
        {
            Update();
        }
    }

    static void Update()
    {
        var running = Listeners.Count > 0 && !ReduceMotion && _window is { WindowState: not WindowState.Minimized, IsVisible: true };
        var rate = !running ? 0 : !(_window!.IsActive || ForceForeground) ? 15 : Hardware && !_throttled ? 60 : 30;
        _intervalTicks = rate == 0 ? 0 : (int)(10_000_000 / rate);
        if (rate > 0 && _pacer is null)
        {
            var dispatcher = Dispatcher.CurrentDispatcher;
            _pacer = new Thread(() => Pace(dispatcher)) { IsBackground = true, Name = "Skin frame clock" };
            _pacer.Start();
        }
        _lastStamp = 0;
        Wake.Set();
    }

    /// <summary>The pacing thread: sleeps on a high-resolution waitable timer, then asks the UI thread for a frame
    /// (never more than one waiting). Parks on an event while the clock is stopped.</summary>
    static void Pace(Dispatcher dispatcher)
    {
        var timer = CreateWaitableTimerExW(IntPtr.Zero, null, CreateWaitableTimerHighResolution, TimerAllAccess);
        while (!dispatcher.HasShutdownStarted)
        {
            var interval = _intervalTicks;
            if (interval == 0)
            {
                Wake.WaitOne(1000);
                continue;
            }
            if (timer != IntPtr.Zero)
            {
                var due = -(long)interval;
                SetWaitableTimer(timer, ref due, 0, IntPtr.Zero, IntPtr.Zero, false);
                WaitForSingleObject(timer, 1000);
            }
            else
            {
                Thread.Sleep(interval / 10_000);
            }
            if (Interlocked.Exchange(ref _pending, 1) == 0)
            {
                dispatcher.BeginInvoke(DispatcherPriority.Render, OnFrame);
            }
        }
    }

    // The governor: when the window's drawing turns out expensive on this PC (software composition, a remote
    // session), the flames drop to 30 frames a second for the rest of the run.
    static bool _throttled;
    static long _governorTicks;
    static TimeSpan _governorCpu;
    static long _governorStamp;

    /// <summary>The process's CPU use over the last two seconds of animation, in percent of one core.</summary>
    public static double LastCpuPercent { get; private set; }

    public static bool Throttled => _throttled;

    static void Govern()
    {
        if (++_governorTicks % 120 != 0)
        {
            return;
        }
        var now = System.Diagnostics.Stopwatch.GetTimestamp();
        var cpu = System.Diagnostics.Process.GetCurrentProcess().TotalProcessorTime;
        if (_governorStamp != 0)
        {
            var wall = (now - _governorStamp) / (double)System.Diagnostics.Stopwatch.Frequency;
            LastCpuPercent = (cpu - _governorCpu).TotalSeconds / wall * 100;
            if (LastCpuPercent > 12 && _intervalTicks == 10_000_000 / 60 && !_throttled)
            {
                _throttled = true;
                Update();
            }
        }
        _governorStamp = now;
        _governorCpu = cpu;
    }

    static void OnFrame()
    {
        _pending = 0;
        if (_intervalTicks == 0)
        {
            _governorStamp = 0;
            return;
        }
        Govern();
        var now = System.Diagnostics.Stopwatch.GetTimestamp();
        var dt = _lastStamp == 0 ? _intervalTicks / 1e7 : (now - _lastStamp) / (double)System.Diagnostics.Stopwatch.Frequency;
        _lastStamp = now;
        Tick(dt);
    }

    static void Tick(double dt)
    {
        ++Ticks;
        dt = Math.Clamp(dt, 0, 0.1);
        foreach (var l in Listeners.ToArray())
        {
            l(dt);
        }
    }

    const uint CreateWaitableTimerHighResolution = 0x2;
    const uint TimerAllAccess = 0x1F0003;

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    static extern IntPtr CreateWaitableTimerExW(IntPtr attributes, string? name, uint flags, uint access);

    [DllImport("kernel32.dll")]
    static extern bool SetWaitableTimer(IntPtr timer, ref long dueTime, int period, IntPtr completion, IntPtr arg, bool resume);

    [DllImport("kernel32.dll")]
    static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
}

/// <summary>An element drawn by the frame clock: it listens only while it is visible and motion is allowed.</summary>
abstract class AnimatedElement : FrameworkElement
{
    bool _listening;

    protected AnimatedElement()
    {
        IsHitTestVisible = false;
        IsVisibleChanged += (_, _) => UpdateListening();
        Loaded += (_, _) => UpdateListening();
        Unloaded += (_, _) => UpdateListening();
        FrameClock.MotionChanged += UpdateListening;
    }

    void UpdateListening()
    {
        var want = IsVisible && IsLoaded && !FrameClock.ReduceMotion && !DesignerProperties.GetIsInDesignMode(this);
        if (want == _listening)
        {
            return;
        }
        _listening = want;
        if (want)
        {
            FrameClock.Subscribe(OnTick);
        }
        else
        {
            FrameClock.Unsubscribe(OnTick);
        }
    }

    void OnTick(double dt)
    {
        Advance(dt);
        if (RedrawEveryTick)
        {
            InvalidateVisual();
        }
    }

    /// <summary>False when the element updates its own child visuals instead of redrawing itself.</summary>
    protected virtual bool RedrawEveryTick => true;

    /// <summary>Moves the animation on by <paramref name="dt"/> seconds.</summary>
    public abstract void Advance(double dt);
}

/// <summary>Small attached behaviours: a page that slides in when shown, an element that pulses (glow or opacity).</summary>
static class Fx
{
    // ---- Enter: fade and rise when the element becomes visible ----

    public static readonly DependencyProperty EnterProperty = DependencyProperty.RegisterAttached(
        "Enter", typeof(bool), typeof(Fx), new PropertyMetadata(false, OnEnterChanged));

    public static bool GetEnter(DependencyObject d) => (bool)d.GetValue(EnterProperty);
    public static void SetEnter(DependencyObject d, bool value) => d.SetValue(EnterProperty, value);

    static void OnEnterChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is FrameworkElement fe && (bool)e.NewValue)
        {
            fe.IsVisibleChanged += (_, v) =>
            {
                if ((bool)v.NewValue && !FrameClock.ReduceMotion)
                {
                    var move = new TranslateTransform(0, 14);
                    fe.RenderTransform = move;
                    var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
                    var time = TimeSpan.FromMilliseconds(320);
                    fe.BeginAnimation(UIElement.OpacityProperty, new DoubleAnimation(0, 1, time) { EasingFunction = ease });
                    move.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(14, 0, time) { EasingFunction = ease });
                }
            };
        }
    }

    // ---- Pulse: opacity breathing between PulseFrom and 1 (the Ko-fi text, the current step) ----

    public static readonly DependencyProperty PulseProperty = DependencyProperty.RegisterAttached(
        "Pulse", typeof(double), typeof(Fx), new PropertyMetadata(0.0, OnPulseChanged));

    public static double GetPulse(DependencyObject d) => (double)d.GetValue(PulseProperty);
    public static void SetPulse(DependencyObject d, double value) => d.SetValue(PulseProperty, value);

    static void OnPulseChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
    {
        if (d is not UIElement el)
        {
            return;
        }
        var low = (double)e.NewValue;
        void Apply()
        {
            if (low <= 0 || FrameClock.ReduceMotion || !el.IsVisible)
            {
                el.BeginAnimation(UIElement.OpacityProperty, null);
                return;
            }
            var a = new DoubleAnimation(1, low, TimeSpan.FromSeconds(1.1))
            {
                AutoReverse = true,
                RepeatBehavior = RepeatBehavior.Forever,
                EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
            };
            Timeline.SetDesiredFrameRate(a, 30);
            el.BeginAnimation(UIElement.OpacityProperty, a);
        }
        el.IsVisibleChanged += (_, _) => Apply();
        FrameClock.MotionChanged += Apply;
        Apply();
    }
}
