namespace QuakeVR.Installer.Core.Audio;

/// <summary>A sound ready to mix: mono floats at <see cref="SoundMixer.Rate"/>.</summary>
public sealed record SoundClip(string Name, float[] Samples);

/// <summary>
/// The installer's software mixer, without a device (the window's <c>SoundEngine</c> feeds it to wave-out; the
/// self-tests and the screenshot harness render it offline): one-shot sounds and a loop, mixed into 16-bit mono.
/// Nothing it does steps the output: every voice fades in and out over a few milliseconds (Quake's 8-bit sounds
/// rarely start or end on zero), a stolen voice fades out, the loop's end is crossfaded into its start, a mute ramps,
/// and the limiter is a smooth curve.
/// </summary>
public sealed class SoundMixer
{
    public const int Rate = 44100;

    /// <summary>A one-shot voice's fade in, and its fade out before its last sample (frames).</summary>
    public const int FadeInFrames = Rate * 2 / 1000;

    public const int FadeOutFrames = Rate * 4 / 1000;

    /// <summary>How fast a stolen voice fades out, and the master volume ramps (frames).</summary>
    public const int StealFrames = Rate * 6 / 1000;

    public const int MasterRampFrames = Rate * 10 / 1000;

    /// <summary>The loop's seam: its last this many frames are crossfaded into its first.</summary>
    public const int SeamFrames = Rate * 40 / 1000;

    /// <summary>The limiter is linear up to here, then bends smoothly towards full scale.</summary>
    public const float KneeStart = 0.6f;

    /// <summary>At most this many voices of one sound (a burst of clicks or typing): the oldest fades out.</summary>
    public const int VoicesPerSound = 3;

    sealed class Voice
    {
        public required float[] Samples;
        public double Position;
        public double Step = 1;
        public float Volume;
        public float Target;
        public float Fade; // Volume change per frame.
        public bool Loop; // Wraps (also while it fades out after being replaced).
        public bool Stopping; // Fading out to be removed (stolen, or a loop replaced).
        public int Age; // Frames played.
        public float Last; // Its last output sample (the edge probe).
    }

    readonly object _lock = new();
    readonly List<Voice> _voices = [];
    float[] _mix = [];
    Voice? _loop;
    SoundClip? _loopClip;
    float _master = 1; // Ramps towards MasterVolume.

    /// <summary>All sounds' volume (0 mutes, after a short ramp).</summary>
    public float MasterVolume { get; set; } = 1;

    /// <summary>Something to mix: a voice plays and the master volume is not (yet) 0.</summary>
    public bool Busy
    {
        get
        {
            lock (_lock)
            {
                return _voices.Count > 0 && (MasterVolume > 0 || _master > 0);
            }
        }
    }

    /// <summary>The edge probe: the largest step a voice put into the output when it started, stopped or wrapped
    /// (full scale 1), and how many were over 0.01 (an audible click). The harness's offline mix reports it.</summary>
    public double MaxEdge { get; private set; }

    public int Edges { get; private set; }

    void Edge(double step)
    {
        MaxEdge = Math.Max(MaxEdge, step);
        Edges += step > 0.01 ? 1 : 0;
    }

    /// <summary>Plays a sound once; <paramref name="pitch"/> 1 is its own pitch.</summary>
    public void Play(SoundClip clip, float volume, double pitch = 1)
    {
        if (clip.Samples.Length < 2)
        {
            return;
        }
        lock (_lock)
        {
            // No pile-ups: the oldest of too many voices of one sound fades out (a cut would click).
            var same = _voices.Where(v => v.Samples == clip.Samples && !v.Stopping && !v.Loop).ToList();
            if (same.Count >= VoicesPerSound)
            {
                Stop(same[0], StealFrames);
            }
            _voices.Add(new Voice { Samples = clip.Samples, Volume = volume, Target = volume, Step = pitch });
        }
    }

    static void Stop(Voice v, int frames)
    {
        v.Stopping = true;
        v.Target = 0;
        v.Fade = Math.Max(v.Volume, 1e-4f) / frames;
    }

    /// <summary>Starts (or changes) the background loop, fading in over <paramref name="fadeSeconds"/>; null fades it out.</summary>
    public void SetLoop(SoundClip? clip, float volume, double fadeSeconds = 1.5)
    {
        lock (_lock)
        {
            var frames = (int)Math.Max(1, fadeSeconds * Rate);
            if (_loop is not null && (clip is null || !ReferenceEquals(_loopClip, clip)))
            {
                Stop(_loop, frames); // It keeps looping while it fades, then it ends.
                _loop = null;
                _loopClip = null;
            }
            if (clip is not null)
            {
                if (_loop is null)
                {
                    _loop = new Voice { Samples = Seamless(clip.Samples), Volume = 0, Target = volume, Fade = volume / frames, Loop = true };
                    _loopClip = clip;
                    _voices.Add(_loop);
                }
                else
                {
                    _loop.Target = volume;
                    _loop.Fade = Math.Max(volume, 1e-4f) / frames;
                }
            }
        }
    }

    /// <summary>A loop without a seam: its last <see cref="SeamFrames"/> crossfaded (equal power) into its first, so
    /// that its end flows into its start (the result is that much shorter).</summary>
    public static float[] Seamless(float[] s)
    {
        var n = Math.Min(SeamFrames, s.Length / 3);
        if (n < 2)
        {
            return s;
        }
        var length = s.Length - n;
        var result = new float[length];
        Array.Copy(s, result, length);
        for (var i = 0; i < n; ++i)
        {
            // The start fades in under the end's tail: result[0] follows s[length - 1] as s[length] would.
            var t = (i + 0.5) / n * Math.PI / 2;
            result[i] = (float)(s[i] * Math.Sin(t) + s[length + i] * Math.Cos(t));
        }
        return result;
    }

    /// <summary>The limiter: unchanged up to <see cref="KneeStart"/>, then a tanh bend towards full scale, never
    /// above it (continuous, and so is its slope; the old knee jumped from 0.85 to 1 at 1).</summary>
    public static float Limit(float s)
    {
        var a = MathF.Abs(s);
        if (a <= KneeStart)
        {
            return s;
        }
        const float room = 1 - KneeStart;
        return MathF.CopySign(KneeStart + room * MathF.Tanh((a - KneeStart) / room), s);
    }

    /// <summary>Mixes the next <c>pcm.Length</c> frames.</summary>
    public void Mix(Span<short> pcm)
    {
        if (_mix.Length != pcm.Length)
        {
            _mix = new float[pcm.Length];
        }
        Array.Clear(_mix);
        var frames = pcm.Length;
        lock (_lock)
        {
            for (var vi = _voices.Count - 1; vi >= 0; --vi)
            {
                var v = _voices[vi];
                var data = v.Samples;
                var length = data.Length;
                var ended = false;
                for (var f = 0; f < frames; ++f)
                {
                    if (v.Fade > 0 && v.Volume != v.Target)
                    {
                        v.Volume = v.Volume < v.Target ? Math.Min(v.Target, v.Volume + v.Fade) : Math.Max(v.Target, v.Volume - v.Fade);
                    }
                    var p = (int)v.Position;
                    float a, b;
                    var envelope = 1.0;
                    if (v.Loop)
                    {
                        if (p >= length)
                        {
                            v.Position -= length;
                            p = (int)v.Position;
                            Edge(Math.Abs(data[0] - data[^1]) * v.Volume);
                        }
                        a = data[p];
                        b = data[p + 1 < length ? p + 1 : 0];
                    }
                    else
                    {
                        if (p >= length - 1)
                        {
                            ended = true;
                            break;
                        }
                        a = data[p];
                        b = data[p + 1];
                        var left = (length - 1 - v.Position) / v.Step;
                        envelope = Math.Min(Math.Min(1.0, (v.Age + 1.0) / FadeInFrames), left / FadeOutFrames);
                    }
                    var c = (a + (b - a) * (float)(v.Position - p)) * v.Volume * (float)envelope;
                    if (v.Age == 0)
                    {
                        Edge(Math.Abs(c));
                    }
                    v.Last = c;
                    _mix[f] += c;
                    v.Position += v.Step;
                    ++v.Age;
                }
                if (ended || v.Stopping && v.Volume <= 0)
                {
                    Edge(Math.Abs(v.Last));
                    _voices.RemoveAt(vi);
                }
            }
        }
        var target = MasterVolume;
        const float ramp = 1f / MasterRampFrames;
        for (var f = 0; f < frames; ++f)
        {
            _master = _master < target ? Math.Min(target, _master + ramp) : Math.Max(target, _master - ramp);
            pcm[f] = (short)MathF.Round(Limit(_mix[f] * _master) * 32000);
        }
    }

    /// <summary>A sound file's samples resampled to <see cref="Rate"/> (linear; Quake's are mostly 11 kHz).</summary>
    public static float[] Resample(float[] samples, int rate)
    {
        if (rate == Rate || samples.Length == 0)
        {
            return samples;
        }
        var n = (int)((long)samples.Length * Rate / rate);
        var result = new float[n];
        var step = (double)rate / Rate;
        for (var i = 0; i < n; ++i)
        {
            var pos = i * step;
            var p = (int)pos;
            var t = (float)(pos - p);
            var a = samples[Math.Min(p, samples.Length - 1)];
            var b = samples[Math.Min(p + 1, samples.Length - 1)];
            result[i] = a + (b - a) * t;
        }
        return result;
    }

    /// <summary>What a rendered mix looks like: its peak and its sample-to-sample jumps (clicks are big jumps).</summary>
    public readonly record struct Analysis(int Frames, double Peak, double MaxJump, int Jumps, double MaxCurve, int Curves, int FullScale);

    /// <param name="jump">A jump (|x[n] - x[n-1]|, full scale 1) counted when above this.</param>
    /// <param name="curve">A curve (|x[n] - 2 x[n-1] + x[n-2]|, the second difference) counted when above this.</param>
    public static Analysis Analyze(ReadOnlySpan<short> pcm, double jump = 0.05, double curve = 0.05)
    {
        double peak = 0, maxJump = 0, maxCurve = 0;
        int jumps = 0, curves = 0, full = 0;
        for (var i = 0; i < pcm.Length; ++i)
        {
            var x = pcm[i] / 32768.0;
            peak = Math.Max(peak, Math.Abs(x));
            if (Math.Abs(pcm[i]) >= 31990)
            {
                ++full;
            }
            if (i >= 1)
            {
                var d = Math.Abs(x - pcm[i - 1] / 32768.0);
                maxJump = Math.Max(maxJump, d);
                jumps += d > jump ? 1 : 0;
            }
            if (i >= 2)
            {
                var c = Math.Abs(x - 2 * (pcm[i - 1] / 32768.0) + pcm[i - 2] / 32768.0);
                maxCurve = Math.Max(maxCurve, c);
                curves += c > curve ? 1 : 0;
            }
        }
        return new Analysis(pcm.Length, peak, maxJump, jumps, maxCurve, curves, full);
    }

    /// <summary>A 16-bit mono WAV at <see cref="Rate"/>.</summary>
    public static void WriteWav(string path, ReadOnlySpan<short> pcm)
    {
        using var w = new BinaryWriter(File.Create(path));
        w.Write("RIFF"u8);
        w.Write(36 + pcm.Length * 2);
        w.Write("WAVEfmt "u8);
        w.Write(16);
        w.Write((short)1);
        w.Write((short)1);
        w.Write(Rate);
        w.Write(Rate * 2);
        w.Write((short)2);
        w.Write((short)16);
        w.Write("data"u8);
        w.Write(pcm.Length * 2);
        foreach (var s in pcm)
        {
            w.Write(s);
        }
    }
}
