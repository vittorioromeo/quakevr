using System.Runtime.InteropServices;

namespace QuakeVR.Installer.Audio;

/// <summary>A sound ready to mix: mono floats at the engine's rate.</summary>
sealed record SoundClip(string Name, float[] Samples);

/// <summary>
/// A tiny software mixer on the Windows wave-out API (winmm; no dependency): one-shot sounds and a loop, mixed into
/// 16-bit mono at 44.1 kHz in four 12 ms buffers. Its thread sleeps, and nothing is sent to the device, while
/// nothing plays.
/// </summary>
sealed class SoundEngine : IDisposable
{
    public const int Rate = 44100;
    const int BufferFrames = 512;
    const int BufferCount = 4;

    sealed class Voice
    {
        public required float[] Samples;
        public double Position;
        public double Step = 1;
        public float Volume;
        public float Target;
        public float Fade; // Volume change per frame.
        public bool Loop;
    }

    readonly object _lock = new();
    readonly List<Voice> _voices = [];
    readonly AutoResetEvent _deviceEvent = new(false);
    readonly AutoResetEvent _wake = new(false);
    readonly IntPtr[] _headers = new IntPtr[BufferCount];
    readonly IntPtr[] _buffers = new IntPtr[BufferCount];
    readonly float[] _mix = new float[BufferFrames];
    readonly short[] _pcm = new short[BufferFrames];
    readonly Thread? _thread;
    readonly IntPtr _device;
    volatile bool _stop;
    Voice? _loop;

    public bool Available => _device != IntPtr.Zero;

    /// <summary>Buffers sent to the device so far (the harness's check that the mixer runs).</summary>
    public int BuffersWritten { get; private set; }

    /// <summary>All sounds' volume (0 mutes; the thread then sends nothing).</summary>
    public float MasterVolume
    {
        get => _master;
        set
        {
            _master = value;
            _wake.Set();
        }
    }

    volatile float _master = 1;

    public SoundEngine()
    {
        var format = new WaveFormatEx
        {
            wFormatTag = 1, nChannels = 1, nSamplesPerSec = Rate, wBitsPerSample = 16, nBlockAlign = 2, nAvgBytesPerSec = Rate * 2,
        };
        if (waveOutOpen(out _device, WaveMapper, ref format, _deviceEvent.SafeWaitHandle.DangerousGetHandle(), IntPtr.Zero, CallbackEvent) != 0)
        {
            _device = IntPtr.Zero;
            return;
        }
        var headerSize = Marshal.SizeOf<WaveHeader>();
        for (var i = 0; i < BufferCount; ++i)
        {
            _buffers[i] = Marshal.AllocHGlobal(BufferFrames * 2);
            _headers[i] = Marshal.AllocHGlobal(headerSize);
            Marshal.StructureToPtr(new WaveHeader { lpData = _buffers[i], dwBufferLength = BufferFrames * 2 }, _headers[i], false);
            waveOutPrepareHeader(_device, _headers[i], headerSize);
            // Mark it done so the thread fills it first.
            Marshal.WriteInt32(_headers[i], FlagsOffset, Marshal.ReadInt32(_headers[i], FlagsOffset) | WhdrDone);
        }
        _thread = new Thread(Run) { IsBackground = true, Name = "Installer sounds", Priority = ThreadPriority.AboveNormal };
        _thread.Start();
    }

    /// <summary>Plays a sound once; <paramref name="pitch"/> 1 is its own pitch.</summary>
    public void Play(SoundClip clip, float volume, double pitch = 1)
    {
        if (!Available || clip.Samples.Length == 0)
        {
            return;
        }
        lock (_lock)
        {
            // No pile-ups: at most three voices of the same sound.
            if (_voices.Count(v => v.Samples == clip.Samples) >= 3)
            {
                _voices.Remove(_voices.First(v => v.Samples == clip.Samples));
            }
            _voices.Add(new Voice { Samples = clip.Samples, Volume = volume, Target = volume, Step = pitch });
        }
        _wake.Set();
    }

    /// <summary>Starts (or changes) the background loop, fading in over <paramref name="fadeSeconds"/>; null fades it out.</summary>
    public void SetLoop(SoundClip? clip, float volume, double fadeSeconds = 1.5)
    {
        if (!Available)
        {
            return;
        }
        lock (_lock)
        {
            var fade = (float)(1 / Math.Max(0.01, fadeSeconds * Rate));
            if (_loop is not null && (clip is null || _loop.Samples != clip.Samples))
            {
                _loop.Target = 0;
                _loop.Fade = Math.Max(_loop.Volume, 0.01f) * fade;
                _loop.Loop = false; // Let it finish fading, then it ends.
                _loop = null;
            }
            if (clip is not null)
            {
                if (_loop is null)
                {
                    _loop = new Voice { Samples = clip.Samples, Volume = 0, Target = volume, Fade = volume * fade, Loop = true };
                    _voices.Add(_loop);
                }
                else
                {
                    _loop.Target = volume;
                    _loop.Fade = Math.Max(volume, 0.01f) * fade;
                }
            }
        }
        _wake.Set();
    }

    void Run()
    {
        var headerSize = Marshal.SizeOf<WaveHeader>();
        while (!_stop)
        {
            bool busy;
            lock (_lock)
            {
                busy = _voices.Count > 0 && MasterVolume > 0;
            }
            if (!busy)
            {
                _wake.WaitOne(500);
                continue;
            }
            var wrote = false;
            for (var i = 0; i < BufferCount && !_stop; ++i)
            {
                if ((Marshal.ReadInt32(_headers[i], FlagsOffset) & WhdrDone) == 0)
                {
                    continue;
                }
                Mix();
                Marshal.Copy(_pcm, 0, _buffers[i], BufferFrames);
                Marshal.WriteInt32(_headers[i], FlagsOffset, Marshal.ReadInt32(_headers[i], FlagsOffset) & ~WhdrDone);
                waveOutWrite(_device, _headers[i], headerSize);
                ++BuffersWritten;
                wrote = true;
            }
            if (!wrote)
            {
                WaitHandle.WaitAny([_deviceEvent, _wake], 50);
            }
        }
    }

    void Mix()
    {
        Array.Clear(_mix);
        lock (_lock)
        {
            var master = MasterVolume;
            for (var vi = _voices.Count - 1; vi >= 0; --vi)
            {
                var v = _voices[vi];
                var data = v.Samples;
                var ended = false;
                for (var f = 0; f < BufferFrames; ++f)
                {
                    if (v.Fade > 0 && v.Volume != v.Target)
                    {
                        v.Volume = v.Volume < v.Target ? Math.Min(v.Target, v.Volume + v.Fade) : Math.Max(v.Target, v.Volume - v.Fade);
                    }
                    var p = (int)v.Position;
                    if (p >= data.Length - 1)
                    {
                        if (!v.Loop)
                        {
                            ended = true;
                            break;
                        }
                        v.Position -= data.Length - 1;
                        p = (int)v.Position;
                    }
                    var t = (float)(v.Position - p);
                    _mix[f] += (data[p] + (data[p + 1] - data[p]) * t) * v.Volume * master;
                    v.Position += v.Step;
                }
                if (ended || (!v.Loop && v.Fade > 0 && v.Volume <= 0 && v.Target <= 0))
                {
                    _voices.RemoveAt(vi);
                }
            }
        }
        for (var f = 0; f < BufferFrames; ++f)
        {
            var s = _mix[f];
            s = s > 1 ? 1 : s < -1 ? -1 : s - s * s * s * 0.15f; // A soft knee before the clip.
            _pcm[f] = (short)(s * 32000);
        }
    }

    /// <summary>A sound file's samples resampled to the engine's rate (linear; Quake's are mostly 11 kHz).</summary>
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

    public void Dispose()
    {
        _stop = true;
        _wake.Set();
        _thread?.Join(500);
        if (_device != IntPtr.Zero)
        {
            waveOutReset(_device);
            var headerSize = Marshal.SizeOf<WaveHeader>();
            for (var i = 0; i < BufferCount; ++i)
            {
                waveOutUnprepareHeader(_device, _headers[i], headerSize);
                Marshal.FreeHGlobal(_headers[i]);
                Marshal.FreeHGlobal(_buffers[i]);
            }
            waveOutClose(_device);
        }
    }

    // ---- winmm ----

    const uint WaveMapper = 0xFFFFFFFF;
    const uint CallbackEvent = 0x00050000;
    const int WhdrDone = 1;
    static readonly int FlagsOffset = (int)Marshal.OffsetOf<WaveHeader>(nameof(WaveHeader.dwFlags));

    [StructLayout(LayoutKind.Sequential)]
    struct WaveFormatEx
    {
        public ushort wFormatTag;
        public ushort nChannels;
        public uint nSamplesPerSec;
        public uint nAvgBytesPerSec;
        public ushort nBlockAlign;
        public ushort wBitsPerSample;
        public ushort cbSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct WaveHeader
    {
        public IntPtr lpData;
        public uint dwBufferLength;
        public uint dwBytesRecorded;
        public IntPtr dwUser;
        public int dwFlags;
        public uint dwLoops;
        public IntPtr lpNext;
        public IntPtr reserved;
    }

    [DllImport("winmm.dll")]
    static extern int waveOutOpen(out IntPtr hWaveOut, uint uDeviceId, ref WaveFormatEx format, IntPtr callback, IntPtr instance, uint flags);
    [DllImport("winmm.dll")]
    static extern int waveOutPrepareHeader(IntPtr hWaveOut, IntPtr header, int size);
    [DllImport("winmm.dll")]
    static extern int waveOutUnprepareHeader(IntPtr hWaveOut, IntPtr header, int size);
    [DllImport("winmm.dll")]
    static extern int waveOutWrite(IntPtr hWaveOut, IntPtr header, int size);
    [DllImport("winmm.dll")]
    static extern int waveOutReset(IntPtr hWaveOut);
    [DllImport("winmm.dll")]
    static extern int waveOutClose(IntPtr hWaveOut);
}
