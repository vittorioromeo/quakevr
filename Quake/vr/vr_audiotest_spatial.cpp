// vr_audiotest_spatial.cpp -- vr_snd_test's circle, ild and clicks (see vr_audiotest.cpp).

#include "vr_audiotest_internal.hpp"

namespace qvr::audio::testkit
{

// ----------------------------------------------------------------------------
// Tests

void testCircle(Result& res)
{
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("circle (no Steam Audio)", false);
        return;
    }
    const Sound snd = makeSound(testRate, true, noise);
    const Listener lis = centred();
    const Features f = baseFeatures();
    const float r = 2.f * units::perMetre;
    float ild[8]{};
    Octaves oct[8]{};
    int lag[8]{};
    const Levels mono = [&] {
        za::Vector<float> x(testRate);
        const short* d = reinterpret_cast<const short*>(snd.cache()->data);
        for(int i = 0; i < testRate; i++)
        {
            x[i] = d[i];
        }
        return measure(x.data(), x.data(), testRate, testRate);
    }();
    for(int k = 0; k < 8; k++)
    {
        const float a = static_cast<float>(k) * 45.f * 3.14159265f / 180.f; // clockwise from ahead: 90 the right
        const glm::vec3 at = r * (za::cos(a) * lis.fwd + za::sin(a) * lis.right);
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, 0.5f, [&](double, VoiceInput& in) { in.pos = at; });
        const int from = testRate / 10;
        const int n = static_cast<int>(out.l.size()) - from;
        const Levels lv = measure(out.l.data() + from, out.r.data() + from, n, testRate);
        ild[k] = lv.right - lv.left;
        oct[k] = octaves(out, from);
        lag[k] = itd(out, from, za::min(n, 8192));
        Con_Printf("snd_test circle: %3d deg: left %6.1f dB, right %6.1f dB, ILD %+5.1f dB, ITD %+4.0f us; octaves 1-2-4-8-16 "
                   "kHz %.1f %.1f %.1f %.1f dB (mono in: %.1f dB)\n",
            k * 45, lv.left, lv.right, ild[k], lag[k] * 1e6 / testRate, oct[k].band[0], oct[k].band[1], oct[k].band[2],
            oct[k].band[3], mono.rms);
    }
    // A full turn in 4 s, to listen to.
    {
        const Render turn = renderVoice(m, snd.cache(), lis, f, nullptr, 4.f, [&](double t, VoiceInput& in) {
            const float a = static_cast<float>(t / 4.0 * 2.0 * 3.14159265);
            in.pos = r * (za::cos(a) * lis.fwd + za::sin(a) * lis.right);
        });
        writeWav("circle", turn);
    }
    res.check("circle: the right louder on the right (ILD at 90 > 6 dB)", ild[2] > 6.f);
    res.check("circle: the left louder on the left (ILD at 270 < -6 dB)", ild[6] < -6.f);
    res.check("circle: ahead and behind balanced (|ILD| < 3 dB)", za::abs(ild[0]) < 3.f && za::abs(ild[4]) < 3.f);
    res.check("circle: the right ear first on the right, the left on the left (ITD)", lag[2] > 0 && lag[6] < 0);
    // Ahead and behind: the same level at each ear, told apart by the pinnae's colouring (behind: less of the highs).
    const float frontBack = (oct[0].band[2] - oct[4].band[2]) + (oct[0].band[3] - oct[4].band[3]);
    Con_Printf("snd_test circle: ahead minus behind, 4-16 kHz: %+.1f dB\n", frontBack);
    res.check("circle: behind duller than ahead (4-16 kHz, > 2 dB)", frontBack > 2.f);
}

// An in-place radix-2 FFT (n a power of two).
void fft(za::Vector<double>& re, za::Vector<double>& im)
{
    const int n = static_cast<int>(re.size());
    for(int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for(; j & bit; bit >>= 1)
        {
            j ^= bit;
        }
        j ^= bit;
        if(i < j)
        {
            const double tr = re[i];
            re[i] = re[j];
            re[j] = tr;
            const double ti = im[i];
            im[i] = im[j];
            im[j] = ti;
        }
    }
    for(int len = 2; len <= n; len <<= 1)
    {
        const double a = -2.0 * 3.14159265358979 / len;
        for(int i = 0; i < n; i += len)
        {
            for(int k = 0; k < len / 2; k++)
            {
                const double wr = za::cos(a * k);
                const double wi = za::sin(a * k);
                const int p = i + k;
                const int q = p + len / 2;
                const double xr = re[q] * wr - im[q] * wi;
                const double xi = re[q] * wi + im[q] * wr;
                re[q] = re[p] - xr;
                im[q] = im[p] - xi;
                re[p] += xr;
                im[p] += xi;
            }
        }
    }
}

// One ear's power (dB) in [f0, f1): every FFT bin of 4096-sample Hann windows from `from` (the whole band, not a few
// frequencies of it: an HRTF's response is full of narrow peaks and notches).
float earBand(const za::Vector<float>& x, int from, float f0, float f1)
{
    constexpr int n = 4096;
    za::Vector<double> re(n), im(n);
    double power = 0.0;
    for(int start = from; start + n <= static_cast<int>(x.size()); start += n)
    {
        for(int i = 0; i < n; i++)
        {
            re[i] = x[start + i] * (0.5 - 0.5 * za::cos(2.0 * 3.14159265 * i / (n - 1)));
            im[i] = 0.0;
        }
        fft(re, im);
        const int k0 = za::max(1, static_cast<int>(za::ceil(f0 * n / testRate)));
        const int k1 = za::min(n / 2, static_cast<int>(za::ceil(f1 * n / testRate)));
        for(int k = k0; k < k1; k++)
        {
            power += re[k] * re[k] + im[k] * im[k];
        }
    }
    return static_cast<float>(10.0 * za::log10(za::max(power, 1e-9)));
}

// A render's left/right balance (dB, right minus left) at 0.25-5 kHz after Quake's 11 kHz lowpass (snd_mix.c: every
// fourth sample, filtered), with the voices' anti-aliasing before it or not.
float ildThroughQuake(const Render& out, bool antiAlias, int from)
{
    Render x = out;
    const int n = static_cast<int>(x.l.size());
    if(antiAlias)
    {
        AntiAlias aa;
        aa.apply(x.l.data(), x.r.data(), n);
    }
    za::Vector<int> side(n);
    for(int ear = 0; ear < 2; ear++)
    {
        za::Vector<float>& y = ear == 0 ? x.l : x.r;
        for(int i = 0; i < n; i++)
        {
            side[i] = static_cast<int>(y[i] * 256.f); // (paint buffer units)
        }
        S_LowpassTest(side.data(), 1, n, ear, 1);
        for(int i = 0; i < n; i++)
        {
            y[i] = static_cast<float>(side[i]) / 256.f;
        }
    }
    return earBand(x.r, from, 250.f, 5000.f) - earBand(x.l, from, 250.f, 5000.f);
}

// The left/right balance against a source's angle: a noise at 2 m, azimuth -90..90 in 5 degree steps (+ the right), at
// three elevations, its interaural level difference (right minus left, dB) at 2-8 kHz. Alone (a fresh voice each angle,
// as the circle) and among 15 other voices (silent, other directions, turning: their HRTFs in the same lanes), with each
// interpolation: a voice's balance is its own direction's, whatever else plays. Level, also at 0.25-5 kHz as the voice
// makes it, through Quake's 11 kHz lowpass, and through it after the voices' anti-aliasing (AntiAlias): that must keep
// the voice's own balance (without it the highs fold down: ROUND21.md, "HRTF balance").
void testIld(Result& res)
{
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("ild (no Steam Audio)", false);
        return;
    }
    const Sound snd = makeSound(testRate, true, noise);
    const Listener lis = centred();
    const float r = 2.f * units::perMetre;
    const auto at = [&](float az, float el) {
        const float a = az * 3.14159265f / 180.f;
        const float e = el * 3.14159265f / 180.f;
        return r * (za::cos(e) * (za::cos(a) * lis.fwd + za::sin(a) * lis.right) + za::sin(e) * lis.up);
    };
    constexpr int steps = 37;
    constexpr int crowd = 16;
    const int frame = m.frameSize();
    const int from = 4096;
    const int blocks = (from + 2 * 4096 + frame - 1) / frame;
    float worstAhead = 0.f;
    float worstSym = 0.f;
    float worstCrowd = 0.f;
    float worstStep = 0.f;
    float worstAliased = 0.f;
    float worstKept = 0.f;
    for(int bilinear = 0; bilinear < 2; bilinear++)
    {
        Features f = baseFeatures();
        f.bilinear = bilinear != 0;
        for(const float el : {0.f, 30.f, -30.f})
        {
            float ild[2][steps]{};
            float chain[3][steps]{}; // 0.25-5 kHz: the voice's, through Quake's lowpass, anti-aliased first
            for(int withCrowd = 0; withCrowd < 2; withCrowd++)
            {
                for(int k = 0; k < steps; k++)
                {
                    const float az = -90.f + 5.f * static_cast<float>(k);
                    for(int v = 0; v < Mixer::maxVoices; v++)
                    {
                        m.stop(v);
                    }
                    const int count = withCrowd ? crowd : 1;
                    for(int v = 0; v < count; v++)
                    {
                        m.start(v, snd.cache(), static_cast<double>(v * 101));
                    }
                    Render out;
                    za::Vector<float> l(frame), rr(frame);
                    for(int b = 0; b < blocks; b++)
                    {
                        const double t = static_cast<double>(b) * frame / testRate;
                        for(int v = 0; v < count; v++)
                        {
                            VoiceInput in;
                            in.gain = v == 0 ? 1.f : 0.f;
                            in.pos = v == 0 ? at(az, el)
                                            : at(static_cast<float>(v * 360 / crowd + t * 200.0),
                                                  static_cast<float>(50.0 * za::sin(t * 6.0 + v)));
                            m.set(v, in);
                        }
                        za::fill(l, 0.f);
                        za::fill(rr, 0.f);
                        m.render(1, lis, f, nullptr, l.data(), rr.data());
                        out.l.emplaceBackRange(l.data(), l.size());
                        out.r.emplaceBackRange(rr.data(), rr.size());
                    }
                    ild[withCrowd][k] = earBand(out.r, from, 2000.f, 8000.f) - earBand(out.l, from, 2000.f, 8000.f);
                    if(!withCrowd && el == 0.f)
                    {
                        chain[0][k] = earBand(out.r, from, 250.f, 5000.f) - earBand(out.l, from, 250.f, 5000.f);
                        chain[1][k] = ildThroughQuake(out, false, from);
                        chain[2][k] = ildThroughQuake(out, true, from);
                        worstAliased = za::max(worstAliased, za::abs(chain[1][k] - chain[0][k]));
                        worstKept = za::max(worstKept, za::abs(chain[2][k] - chain[0][k]));
                    }
                }
                char line[512];
                int len = 0;
                for(int k = 0; k < steps; k++)
                {
                    len += snprintf(line + len, sizeof(line) - static_cast<za::SizeT>(len), "%s%.1f", k ? " " : "",
                        ild[withCrowd][k]);
                }
                Con_Printf("snd_test ild: %s, elevation %+3.0f, %s; 2-8 kHz ILD at -90..90 by 5: %s\n",
                    bilinear ? "bilinear" : "nearest", el, withCrowd ? "among 15 voices" : "alone", line);
            }
            if(el == 0.f)
            {
                const char* stage[3] = {"the voice", "Quake's lowpass", "anti-aliased, Quake's lowpass"};
                for(int c = 0; c < 3; c++)
                {
                    char line[512];
                    int len = 0;
                    for(int k = 0; k < steps; k++)
                    {
                        len += snprintf(line + len, sizeof(line) - static_cast<za::SizeT>(len), "%s%.1f", k ? " " : "",
                            chain[c][k]);
                    }
                    Con_Printf("snd_test ild: %s, elevation +0, %s; 0.25-5 kHz: %s\n", bilinear ? "bilinear" : "nearest",
                        stage[c], line);
                }
            }
            constexpr int mid = steps / 2;
            for(int k = 0; k < steps; k++)
            {
                worstCrowd = za::max(worstCrowd, za::abs(ild[1][k] - ild[0][k]));
            }
            if(el == 0.f)
            {
                worstAhead = za::max(worstAhead, za::abs(ild[0][mid]));
                for(int k = 1; k <= mid; k++)
                {
                    worstSym = za::max(worstSym, za::abs(ild[0][mid + k] + ild[0][mid - k]));
                }
                // Steady with the angle across the front (+-30): no step back against the turn.
                for(int k = mid - 6; k < mid + 6; k++)
                {
                    worstStep = za::max(worstStep, ild[0][k] - ild[0][k + 1]);
                }
            }
        }
    }
    Con_Printf("snd_test ild: ahead %.1f dB, worst asymmetry %.1f dB, worst step back (+-30) %.1f dB, crowd against alone "
               "%.1f dB; 0.25-5 kHz through Quake's lowpass against the voice's: %.1f dB, anti-aliased first %.1f dB\n",
        worstAhead, worstSym, worstStep, worstCrowd, worstAliased, worstKept);
    res.check("ild: a voice's balance is its own (among other voices: within 0.5 dB)", worstCrowd < 0.5f);
    res.check("ild: Quake's 11 kHz lowpass keeps the voices' balance, anti-aliased (within 1 dB at 0.25-5 kHz)",
        worstKept < 1.f);
}

// A 4 m wide, 3 m tall, 0.3 m thick wall across the way ahead (x), centred at `x` metres.
// Clicks: a smooth tone's second difference, |x[i] - 2x[i-1] + x[i-2]| over the render's peak. A 440 Hz sine's own is
// (2 pi 440 / 44100)^2 = 0.004; a step of the signal shows as its size. Split by where it falls: at a frame's edge (the
// first two samples of a frame) or inside one.
struct Clicks
{
    float edge{0.f};     // the largest at the frames' edges
    float interior{0.f}; // the largest inside them
    int count{0};        // samples above 0.02 (five times the tone's own)
    int bad{0};          // samples not a number (or infinite)
    float peak{0.f};
};

Clicks clicks(const Render& r, int frame, int from)
{
    Clicks c;
    float peak = 1.f;
    for(za::SizeT i = static_cast<za::SizeT>(from); i < r.l.size(); i++)
    {
        for(const float x : {r.l[i], r.r[i]})
        {
            if(!(za::abs(x) < 1e30f))
            {
                c.bad++;
                continue;
            }
            peak = za::max(peak, za::abs(x));
        }
    }
    c.peak = peak;
    for(const za::Vector<float>* ch : {&r.l, &r.r})
    {
        const za::Vector<float>& x = *ch;
        for(za::SizeT i = static_cast<za::SizeT>(za::max(from, 2)); i < x.size(); i++)
        {
            const float e = za::abs(x[i] - 2.f * x[i - 1] + x[i - 2]) / peak;
            if(!(e < 1e30f))
            {
                continue;
            }
            float& worst = static_cast<int>(i % static_cast<za::SizeT>(frame)) < 2 ? c.edge : c.interior;
            worst = za::max(worst, e);
            c.count += e > 0.02f ? 1 : 0;
        }
    }
    return c;
}

float sine440(int i, unsigned&)
{
    return 8000.f * za::sin(2.f * 3.14159265f * 440.f * static_cast<float>(i) / testRate);
}

// A 440 Hz tone at 2 m, turning round the head (120 deg/s, its elevation swinging +-40 deg twice a second: a head
// turning and nodding), its direction new each frame; and still, between the HRTF's measured directions. Each
// interpolation: no steps at the frames' edges (the author heard bilinear crackle, NOTES.md 2026-09-30 23:18).
void testClicks(Result& res)
{
    Mixer m;
    const double made = Sys_DoubleTime();
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("clicks (no Steam Audio)", false);
        return;
    }
    Con_Printf("snd_test clicks: the mixer made in %.1f ms, %d lanes (an HRTF each)\n", (Sys_DoubleTime() - made) * 1000.0,
        m.laneCount());
    const Sound snd = makeSound(testRate, true, sine440);
    const Listener lis = centred();
    const float r = 2.f * units::perMetre;
    const auto at = [&](float azDeg, float elDeg) {
        const float a = azDeg * 3.14159265f / 180.f;
        const float e = elDeg * 3.14159265f / 180.f;
        return r * (za::cos(e) * (za::cos(a) * lis.fwd + za::sin(a) * lis.right) + za::sin(e) * lis.up);
    };
    for(int bilinear = 0; bilinear < 2; bilinear++)
    {
        Features f = baseFeatures();
        f.bilinear = bilinear != 0;
        const char* name = bilinear ? "bilinear" : "nearest";
        const Render moving = renderVoice(m, snd.cache(), lis, f, nullptr, 4.f, [&](double t, VoiceInput& in) {
            in.pos = at(static_cast<float>(t * 120.0), static_cast<float>(40.0 * za::sin(t * 2.0 * 2.0 * 3.14159265)));
        });
        const Render still = renderVoice(m, snd.cache(), lis, f, nullptr, 1.f,
            [&](double, VoiceInput& in) { in.pos = at(37.f, 13.f); });
        writeWav(bilinear ? "clicks_bilinear" : "clicks_nearest", moving);
        const int from = testRate / 10;
        const Clicks cm = clicks(moving, m.frameSize(), from);
        const Clicks cs = clicks(still, m.frameSize(), from);
        Con_Printf("snd_test clicks: %s, moving: frame edges %.4f, inside %.4f, %d over 0.02; still: edges %.4f, inside "
                   "%.4f, %d over\n",
            name, cm.edge, cm.interior, cm.count, cs.edge, cs.interior, cs.count);
        char test[96];
        snprintf(test, sizeof(test), "clicks: %s, a moving tone has no steps (< 0.02)", name);
        res.check(test, cm.bad == 0 && cs.bad == 0 && cm.edge < 0.02f && cm.interior < 0.02f && cs.edge < 0.02f &&
                            cs.interior < 0.02f);

        // 16 voices at once (the game's: one a pool task each), each its own direction, turning; on the pool, then on
        // one thread.
        for(int parallel = 1; parallel >= 0; parallel--)
        {
            constexpr int count = 16;
            for(int v = 0; v < Mixer::maxVoices; v++)
            {
                m.stop(v);
            }
            for(int v = 0; v < count; v++)
            {
                m.start(v, snd.cache(), static_cast<double>(v * 37));
            }
            jobs::setParallel(parallel != 0);
            Render sum;
            const int frame = m.frameSize();
            const int blocks = 2 * testRate / frame;
            za::Vector<float> l(frame), rr(frame);
            for(int b = 0; b < blocks; b++)
            {
                const double t = static_cast<double>(b) * frame / testRate;
                for(int v = 0; v < count; v++)
                {
                    VoiceInput in;
                    in.gain = 1.f / count;
                    in.pos = at(static_cast<float>(v * 360 / count + t * 120.0),
                        static_cast<float>(40.0 * za::sin(t * 4.0 * 3.14159265 + v)));
                    m.set(v, in);
                }
                za::fill(l, 0.f);
                za::fill(rr, 0.f);
                m.render(1, lis, f, nullptr, l.data(), rr.data());
                sum.l.emplaceBackRange(l.data(), l.size());
                sum.r.emplaceBackRange(rr.data(), rr.size());
            }
            jobs::setParallel(true);
            const Clicks c = clicks(sum, frame, from);
            Con_Printf("snd_test clicks: %s, %d voices %s: frame edges %.4f, inside %.4f, %d over 0.02, %d not numbers, "
                       "peak %.0f\n",
                name, count, parallel ? "on the pool" : "on one thread", c.edge, c.interior, c.count, c.bad, c.peak);
            if(parallel)
            {
                writeWav(bilinear ? "clicks_bilinear_voices" : "clicks_nearest_voices", sum);
            }
            snprintf(test, sizeof(test), "clicks: %s, %d voices %s have no steps (< 0.02)", name, count,
                parallel ? "on the pool" : "on one thread");
            res.check(test, c.bad == 0 && c.peak > 1000.f && c.edge < 0.02f && c.interior < 0.02f);
        }
    }
}

} // namespace qvr::audio::testkit
