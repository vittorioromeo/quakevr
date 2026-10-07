using System.Diagnostics;
using System.Runtime.InteropServices;
using QuakeVR.Installer.Core.Audio;

namespace QuakeVR.Installer.Audio;

/// <summary>
/// The installer's sounds on the Windows wave-out API (winmm; no dependency): <see cref="SoundMixer"/>'s 16-bit mono
/// at 44.1 kHz in a ring of short buffers. Its thread sleeps, and nothing is sent to the device, while nothing plays.
/// </summary>
sealed class SoundEngine : IDisposable
{
    public const int Rate = SoundMixer.Rate;

    readonly SoundMixer _mixer = new();
    readonly int _bufferFrames;
    readonly int _bufferCount;
    readonly AutoResetEvent _deviceEvent = new(false);
    readonly AutoResetEvent _wake = new(false);
    readonly IntPtr[] _headers;
    readonly IntPtr[] _buffers;
    readonly short[] _pcm;
    readonly Thread? _thread;
    readonly IntPtr _device;
    volatile bool _stop;
    readonly bool _offline;

    public bool Available => _device != IntPtr.Zero;

    /// <summary>Buffers sent to the device so far (the harness's check that the mixer runs).</summary>
    public int BuffersWritten { get; private set; }

    /// <summary>Refills that found every buffer done (WHDR_DONE) while sounds played: the ring had drained, the
    /// device was starved (a gap of silence, heard as a crackle).</summary>
    public int Underruns { get; private set; }

    /// <summary>The fewest buffers still queued at a refill while sounds played (the margin left; 0 is an underrun).</summary>
    public int MinQueued { get; private set; } = int.MaxValue;

    /// <summary>Wakes by the timeout instead of the device's event (the device stalled, or never signals).</summary>
    public int TimeoutWakes { get; private set; }

    /// <summary>Wall-clock seconds the thread streamed (sounds playing), and the audio it sent meanwhile.</summary>
    public double StreamedSeconds => (_streamTicks + (_streamStart > 0 ? Stopwatch.GetTimestamp() - _streamStart : 0)) / (double)Stopwatch.Frequency;

    public double SentSeconds => _sentFrames / (double)Rate;

    /// <summary>Audio the device has played (waveOutGetPosition), in seconds; -1 when it cannot say. Played well short
    /// of <see cref="StreamedSeconds"/> means gaps, even those <see cref="Underruns"/> misses (Windows' wave-out
    /// emulation can hold the last buffer while it starves). Read it at the end of a test: reading it at every refill
    /// changes the emulation's timing and hides the gaps.</summary>
    public double PlayedSeconds
    {
        get
        {
            if (!Available)
            {
                return -1;
            }
            var t = new MmTime { wType = TimeSamples };
            return waveOutGetPosition(_device, ref t, Marshal.SizeOf<MmTime>()) == 0 && t.wType == TimeSamples ? t.u / (double)Rate : -1;
        }
    }

    long _streamTicks;
    long _streamStart;
    long _sentFrames;

    /// <summary>All sounds' volume (0 mutes; the thread then sends nothing).</summary>
    public float MasterVolume
    {
        get => _mixer.MasterVolume;
        set
        {
            _mixer.MasterVolume = value;
            _wake.Set();
        }
    }

    /// <summary>How long a change of <see cref="MasterVolume"/> takes (<see cref="SoundMixer.MasterFadeSeconds"/>).</summary>
    public double MasterFadeSeconds
    {
        get => _mixer.MasterFadeSeconds;
        set => _mixer.MasterFadeSeconds = value;
    }

    /// <summary>The ring: 8 buffers of 10 ms (Windows' audio engine period). Windows' wave-out (emulated over
    /// WASAPI) starves with 50 ms or less queued: the old 4 x 512 frames (46 ms) played 20.1 s of audio in 26.1 s of
    /// the live window, with 430 underruns; 6 x 441 held with one buffer to spare, 8 x 441 with six (INSTALLER.md, "Sounds").
    /// What is queued delays a click: 80 ms at most.</summary>
    public const int BufferFrames = Rate / 100;

    public const int BufferCount = 8;

    /// <summary>The longest the thread sleeps without the device's event: two buffers, well inside the ring.</summary>
    const int WaitMs = 20;

    public SoundEngine() : this(BufferFrames, BufferCount)
    {
    }

    /// <summary>No device and no thread: the mixer alone, pulled by <see cref="MixOffline"/> (the harness's check that
    /// Play mutes the sounds, without a sound).</summary>
    internal static SoundEngine Offline() => new(offline: true);

    SoundEngine(bool offline)
    {
        _offline = offline;
        _headers = [];
        _buffers = [];
        _pcm = [];
    }

    /// <summary>The next <paramref name="pcm"/>.Length frames of the offline engine's mix.</summary>
    internal void MixOffline(Span<short> pcm)
    {
        if (_offline)
        {
            _mixer.Mix(pcm);
        }
    }

    /// <summary>A ring of <paramref name="bufferCount"/> buffers of <paramref name="bufferFrames"/> frames (the harness compares rings).</summary>
    internal SoundEngine(int bufferFrames, int bufferCount)
    {
        _bufferFrames = bufferFrames;
        _bufferCount = bufferCount;
        _headers = new IntPtr[bufferCount];
        _buffers = new IntPtr[bufferCount];
        _pcm = new short[bufferFrames];
        var format = new WaveFormatEx
        {
            wFormatTag = 1, nChannels = 1, nSamplesPerSec = Rate, wBitsPerSample = 16, nBlockAlign = 2, nAvgBytesPerSec = Rate * 2,
        };
        // CALLBACK_EVENT: the device sets _deviceEvent when it finishes a buffer.
        if (waveOutOpen(out _device, WaveMapper, ref format, _deviceEvent.SafeWaitHandle.DangerousGetHandle(), IntPtr.Zero, CallbackEvent) != 0)
        {
            _device = IntPtr.Zero;
            return;
        }
        var headerSize = Marshal.SizeOf<WaveHeader>();
        for (var i = 0; i < bufferCount; ++i)
        {
            _buffers[i] = Marshal.AllocHGlobal(bufferFrames * 2);
            _headers[i] = Marshal.AllocHGlobal(headerSize);
            Marshal.StructureToPtr(new WaveHeader { lpData = _buffers[i], dwBufferLength = (uint)(bufferFrames * 2) }, _headers[i], false);
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
        if (!Available && !_offline)
        {
            return;
        }
        _mixer.Play(clip, volume, pitch);
        _wake.Set();
    }

    /// <summary>Starts (or changes) the background loop, fading in over <paramref name="fadeSeconds"/>; null fades it out.</summary>
    public void SetLoop(SoundClip? clip, float volume, double fadeSeconds = 1.5)
    {
        if (!Available && !_offline)
        {
            return;
        }
        _mixer.SetLoop(clip, volume, fadeSeconds);
        _wake.Set();
    }

    void Run()
    {
        var headerSize = Marshal.SizeOf<WaveHeader>();
        var streaming = false;
        WaitHandle[] waits = [_deviceEvent, _wake];
        while (!_stop)
        {
            if (!_mixer.Busy)
            {
                if (_streamStart > 0)
                {
                    _streamTicks += Stopwatch.GetTimestamp() - _streamStart;
                    _streamStart = 0;
                }
                streaming = false;
                _wake.WaitOne(500);
                continue;
            }
            var done = 0;
            for (var i = 0; i < _bufferCount; ++i)
            {
                done += (Marshal.ReadInt32(_headers[i], FlagsOffset) & WhdrDone) != 0 ? 1 : 0;
            }
            if (streaming && done > 0)
            {
                Underruns += done == _bufferCount ? 1 : 0;
                MinQueued = Math.Min(MinQueued, _bufferCount - done);
            }
            var wrote = false;
            for (var i = 0; i < _bufferCount && !_stop; ++i)
            {
                if ((Marshal.ReadInt32(_headers[i], FlagsOffset) & WhdrDone) == 0)
                {
                    continue;
                }
                _mixer.Mix(_pcm);
                Marshal.Copy(_pcm, 0, _buffers[i], _bufferFrames);
                Marshal.WriteInt32(_headers[i], FlagsOffset, Marshal.ReadInt32(_headers[i], FlagsOffset) & ~WhdrDone);
                waveOutWrite(_device, _headers[i], headerSize);
                ++BuffersWritten;
                if (_streamStart == 0)
                {
                    _streamStart = Stopwatch.GetTimestamp();
                }
                _sentFrames += _bufferFrames;
                wrote = true;
            }
            streaming |= wrote;
            if (!wrote)
            {
                // Woken by the device finishing a buffer (CALLBACK_EVENT), or by a new sound: either refills at once.
                TimeoutWakes += WaitHandle.WaitAny(waits, WaitMs) == WaitHandle.WaitTimeout ? 1 : 0;
            }
        }
    }

    /// <summary>A sound file's samples resampled to the engine's rate.</summary>
    public static float[] Resample(float[] samples, int rate) => SoundMixer.Resample(samples, rate);

    public void Dispose()
    {
        _stop = true;
        _wake.Set();
        _thread?.Join(500);
        if (_device != IntPtr.Zero)
        {
            waveOutReset(_device);
            var headerSize = Marshal.SizeOf<WaveHeader>();
            for (var i = 0; i < _bufferCount; ++i)
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
    const uint TimeSamples = 2;

    [StructLayout(LayoutKind.Sequential)]
    struct MmTime
    {
        public uint wType;
        public uint u;
        public uint pad;
    }

    [DllImport("winmm.dll")]
    static extern int waveOutGetPosition(IntPtr hWaveOut, ref MmTime time, int size);
    [DllImport("winmm.dll")]
    static extern int waveOutReset(IntPtr hWaveOut);
    [DllImport("winmm.dll")]
    static extern int waveOutClose(IntPtr hWaveOut);
}
