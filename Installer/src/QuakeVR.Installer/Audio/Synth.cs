namespace QuakeVR.Installer.Audio;

/// <summary>
/// The UI's sounds when Quake is not (yet) found, made in code: metallic ticks and clanks, a bell chime, a low buzz,
/// and a fire's crackle for the background. Short, soft, deterministic.
/// </summary>
static class Synth
{
    const int Rate = SoundEngine.Rate;

    public static Dictionary<Sfx, SoundClip> All() => new()
    {
        [Sfx.Click] = Clip("click", Metal(0.07, [2100, 3150, 4900], [0.5, 0.3, 0.2], 0.016, noise: 0.25)),
        [Sfx.Select] = Clip("select", Mix(Thump(0.22, 150, 70, 0.07), Metal(0.2, [880, 1460, 2350], [0.35, 0.25, 0.15], 0.045, noise: 0.15))),
        [Sfx.Back] = Clip("back", Mix(Metal(0.08, [1700, 2600], [0.5, 0.3], 0.018, noise: 0.2),
            Delay(Metal(0.08, [1150, 1800], [0.5, 0.3], 0.02, noise: 0.15), 0.055))),
        [Sfx.Toggle] = Clip("toggle", Metal(0.06, [1500, 2350, 3700], [0.5, 0.3, 0.15], 0.02, noise: 0.3)),
        [Sfx.Type] = Clip("type", Metal(0.03, [3200, 5200], [0.4, 0.3], 0.006, noise: 0.6)),
        [Sfx.InstallStart] = Clip("start", Mix(Sweep(0.45, 180, 620, 0.5), Metal(0.4, [660, 990], [0.3, 0.2], 0.12, noise: 0))),
        [Sfx.InstallDone] = Clip("done", Mix(Bell(440, 0.0), Bell(523.25, 0.11), Bell(659.25, 0.22), Bell(880, 0.33))),
        [Sfx.Error] = Clip("error", Buzz(0.2, 110)),
        [Sfx.Support] = Clip("support", Mix(Bell(659.25, 0.0, 0.8), Bell(987.77, 0.09, 0.8))),
    };

    static SoundClip Clip(string name, float[] samples)
    {
        // Normalize to a soft peak.
        var peak = samples.Length == 0 ? 1 : samples.Max(MathF.Abs);
        var k = peak > 0 ? 0.8f / peak : 1;
        for (var i = 0; i < samples.Length; ++i)
        {
            samples[i] *= k;
        }
        return new SoundClip("synth:" + name, samples);
    }

    static float[] Metal(double seconds, double[] freqs, double[] amps, double decay, double noise)
    {
        var n = (int)(seconds * Rate);
        var s = new float[n];
        var rng = new Random(n);
        for (var i = 0; i < n; ++i)
        {
            var t = (double)i / Rate;
            var env = Math.Exp(-t / decay);
            double v = 0;
            for (var k = 0; k < freqs.Length; ++k)
            {
                v += amps[k] * Math.Sin(2 * Math.PI * freqs[k] * t) * Math.Exp(-t / (decay * (1 + k * 0.4)));
            }
            v += noise * (rng.NextDouble() * 2 - 1) * Math.Exp(-t / 0.004);
            s[i] = (float)(v * Math.Min(1, i / 40.0) * (0.3 + 0.7 * env));
        }
        return s;
    }

    static float[] Thump(double seconds, double f0, double f1, double decay)
    {
        var n = (int)(seconds * Rate);
        var s = new float[n];
        double phase = 0;
        for (var i = 0; i < n; ++i)
        {
            var t = (double)i / Rate;
            phase += 2 * Math.PI * (f0 + (f1 - f0) * Math.Min(1, t / seconds)) / Rate;
            s[i] = (float)(Math.Sin(phase) * Math.Exp(-t / decay) * Math.Min(1, i / 60.0));
        }
        return s;
    }

    static float[] Sweep(double seconds, double f0, double f1, double level)
    {
        var n = (int)(seconds * Rate);
        var s = new float[n];
        double phase = 0;
        var rng = new Random(7);
        double lp = 0;
        for (var i = 0; i < n; ++i)
        {
            var t = (double)i / Rate;
            var x = t / seconds;
            phase += 2 * Math.PI * (f0 + (f1 - f0) * x * x) / Rate;
            lp += ((rng.NextDouble() * 2 - 1) - lp) * (0.02 + 0.2 * x);
            var env = Math.Sin(Math.PI * Math.Min(1, x * 1.2)) * level;
            s[i] = (float)((Math.Sin(phase) * 0.6 + lp * 0.8) * env);
        }
        return s;
    }

    static float[] Bell(double f, double at, double seconds = 1.3)
    {
        var start = (int)(at * Rate);
        var n = start + (int)(seconds * Rate);
        var s = new float[n];
        double[] ratios = [1, 2.76, 5.4, 8.93];
        double[] amps = [0.6, 0.25, 0.12, 0.05];
        for (var i = start; i < n; ++i)
        {
            var t = (double)(i - start) / Rate;
            double v = 0;
            for (var k = 0; k < ratios.Length; ++k)
            {
                v += amps[k] * Math.Sin(2 * Math.PI * f * ratios[k] * t) * Math.Exp(-t * (2.2 + k * 2.5));
            }
            s[i] = (float)(v * Math.Min(1, (i - start) / 30.0));
        }
        return s;
    }

    static float[] Buzz(double seconds, double f)
    {
        var n = (int)(seconds * Rate);
        var s = new float[n];
        for (var i = 0; i < n; ++i)
        {
            var t = (double)i / Rate;
            var square = Math.Sin(2 * Math.PI * f * t) + Math.Sin(2 * Math.PI * f * 3 * t) / 3 + Math.Sin(2 * Math.PI * f * 5 * t) / 5;
            s[i] = (float)(square * 0.5 * Math.Min(1, t / 0.01) * Math.Min(1, (seconds - t) / 0.04));
        }
        return s;
    }

    static float[] Delay(float[] a, double seconds)
    {
        var d = (int)(seconds * Rate);
        var s = new float[a.Length + d];
        Array.Copy(a, 0, s, d, a.Length);
        return s;
    }

    static float[] Mix(params float[][] parts)
    {
        var s = new float[parts.Max(p => p.Length)];
        foreach (var p in parts)
        {
            for (var i = 0; i < p.Length; ++i)
            {
                s[i] += p[i];
            }
        }
        return s;
    }

    /// <summary>A fire's crackle: a low rumble and random pops, six seconds that loop without a seam.</summary>
    public static SoundClip Crackle()
    {
        const double seconds = 6;
        var n = (int)(seconds * Rate);
        var s = new float[n];
        var rng = new Random(1666);
        double lp = 0, lp2 = 0;
        for (var i = 0; i < n; ++i)
        {
            lp += ((rng.NextDouble() * 2 - 1) - lp) * 0.02;
            lp2 += (lp - lp2) * 0.05;
            s[i] = (float)(lp2 * 1.6);
        }
        var pops = (int)(seconds * 18);
        for (var p = 0; p < pops; ++p)
        {
            var at = rng.Next(n);
            var amp = 0.15 + rng.NextDouble() * (rng.NextDouble() < 0.1 ? 0.8 : 0.35);
            var decay = 0.0015 + rng.NextDouble() * 0.004;
            var len = (int)(decay * 6 * Rate);
            for (var i = 0; i < len; ++i)
            {
                var t = (double)i / Rate;
                s[(at + i) % n] += (float)((rng.NextDouble() * 2 - 1) * amp * Math.Exp(-t / decay));
            }
        }
        var peak = s.Max(MathF.Abs);
        for (var i = 0; i < n; ++i)
        {
            s[i] *= 0.7f / peak;
        }
        return new SoundClip("synth:crackle", s);
    }
}
