// vr_audiotest.cpp -- vr_snd_test: offline renders through the spatial audio's own mixer and simulation
// (vr_audio.cpp, vr_audiosim.cpp), measured, each test a line and PASS or FAIL. Made scenes (boxes and walls in
// Steam Audio's scene, as the map's faces are), made sounds (noise, a sine), 44.1 kHz; nothing of the game's mix is
// touched (the tests have their own mixer and simulator). The renders are written to <game>/sound_tests/test_<name>.wav.
//
//   circle     a noise circling the head at 2 m: left/right level and time differences, front/back brightness
//   clicks     a tone turning round the head, each HRTF interpolation: steps at the frames' edges
//   occlusion  a noise 5 m ahead, then behind a wall (a static one, then a brush model's instance moved in and out)
//   reverb     a click in a small room and a big hall, each reverb quality: the simulated and the measured RT60
//   doppler    a 1 kHz tone passing the head at 1000 units/s: the pitch coming and going
//   nearfield  a noise at the right ear (0.2 m) and 2 m to the right: the level difference between the ears
//   distance   (in a map) how loud by the distance, in sight of the head: Quake's panning against the spatial mix
//   hands      (in a map, in VR) the player's weapon channels' origins and panning: the hands'
//   bench      32 voices, every feature on, then off; and Quake's own mixing of 32 channels: ms per 90 Hz frame
//   all        each of them

#include "vr_audio.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Log10.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>

extern "C" int VR_SndSpatialize(channel_t* ch);

namespace qvr::audio
{

// ----------------------------------------------------------------------------
// Measurements

namespace
{

float db(double meanSquare)
{
    return meanSquare > 1e-20 ? static_cast<float>(10.0 * za::log10(meanSquare / (32768.0 * 32768.0))) : -200.f;
}

} // namespace

Levels measure(const float* l, const float* r, int n, int rate)
{
    Levels lv;
    if(n <= 0)
    {
        return lv;
    }
    // Four one-pole stages each (24 dB an octave): below 500 Hz, above 4 kHz.
    const double aLow = 1.0 - za::exp(-2.0 * 3.14159265 * 500.0 / rate);
    const double aHigh = 1.0 - za::exp(-2.0 * 3.14159265 * 4000.0 / rate);
    double sl = 0, sr = 0, low = 0, high = 0;
    double lp[2][4]{}, hp[2][4]{};
    for(int i = 0; i < n; i++)
    {
        sl += static_cast<double>(l[i]) * l[i];
        sr += static_cast<double>(r[i]) * r[i];
        for(int ear = 0; ear < 2; ear++)
        {
            double x = ear == 0 ? l[i] : r[i];
            double y = x;
            for(int k = 0; k < 4; k++)
            {
                lp[ear][k] += aLow * (y - lp[ear][k]);
                y = lp[ear][k];
            }
            low += y * y;
            for(int k = 0; k < 4; k++)
            {
                hp[ear][k] += aHigh * (x - hp[ear][k]);
                x = x - hp[ear][k];
            }
            high += x * x;
        }
    }
    lv.left = db(sl / n);
    lv.right = db(sr / n);
    lv.rms = db((sl + sr) / (2.0 * n));
    lv.low = db(low / (2.0 * n));
    lv.high = db(high / (2.0 * n));
    return lv;
}

namespace
{

constexpr int testRate = 44100;

// A made mono sound, as Quake's cache holds one (16-bit).
struct Sound
{
    za::Vector<unsigned char> bytes;
    [[nodiscard]] const sfxcache_t* cache() const
    {
        return reinterpret_cast<const sfxcache_t*>(bytes.data());
    }
};

Sound makeSound(int length, bool loop, float (*gen)(int i, unsigned& seed))
{
    Sound s;
    s.bytes.clear();
    s.bytes.resize(sizeof(sfxcache_t) + static_cast<za::SizeT>(length) * 2 + 16, 0);
    sfxcache_t* sc = reinterpret_cast<sfxcache_t*>(s.bytes.data());
    sc->length = length;
    sc->loopstart = loop ? 0 : -1;
    sc->speed = testRate;
    sc->width = 2;
    sc->stereo = 0;
    short* data = reinterpret_cast<short*>(sc->data);
    unsigned seed = 12345;
    for(int i = 0; i < length; i++)
    {
        data[i] = static_cast<short>(za::clamp(gen(i, seed), -32767.f, 32767.f));
    }
    return s;
}

float noise(int, unsigned& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return (static_cast<float>(seed >> 9) / static_cast<float>(1u << 23) * 2.f - 1.f) * 8000.f;
}

float sine1k(int i, unsigned&)
{
    return 8000.f * za::sin(2.f * 3.14159265f * 1000.f * static_cast<float>(i) / testRate);
}

float click(int i, unsigned&)
{
    return i < 16 ? 20000.f : 0.f;
}

struct Render
{
    za::Vector<float> l, r;
};

// Voice 0 alone for `seconds`; `move` sets its input before each frame (t: seconds from the start).
Render renderVoice(Mixer& m, const sfxcache_t* sc, const Listener& lis, const Features& f,
    const IPLReflectionEffectParams* reverb, float seconds, jobs::FunctionRef<void(double, VoiceInput&)> move)
{
    Render out;
    const int frame = m.frameSize();
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        m.stop(v);
    }
    m.start(0, sc, 0.0);
    const int blocks = static_cast<int>(za::ceil(seconds * testRate / frame));
    za::Vector<float> l(frame), r(frame);
    for(int b = 0; b < blocks; b++)
    {
        VoiceInput in;
        in.gain = 1.f;
        move(static_cast<double>(b) * frame / testRate, in);
        if(m.active(0))
        {
            m.set(0, in);
        }
        za::fill(l.begin(), l.end(), 0.f);
        za::fill(r.begin(), r.end(), 0.f);
        m.render(1, lis, f, reverb, l.data(), r.data());
        out.l.emplaceBackRange(l.data(), l.size());
        out.r.emplaceBackRange(r.data(), r.size());
        if(!m.active(0) || m.ended(0))
        {
            m.stop(0);
        }
    }
    return out;
}

void writeWav(const char* name, const Render& r)
{
    za::String path = za::String{com_gamedir} + "/sound_tests/test_" + name + ".wav";
    COM_CreatePath(path.data());
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        return;
    }
    const auto u32 = [&](unsigned v) { fwrite(&v, 4, 1, f); };
    const auto u16 = [&](unsigned short v) { fwrite(&v, 2, 1, f); };
    const unsigned n = static_cast<unsigned>(r.l.size());
    fwrite("RIFF", 1, 4, f);
    u32(36 + n * 4);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(testRate);
    u32(testRate * 4);
    u16(4);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(n * 4);
    for(unsigned i = 0; i < n; i++)
    {
        const short s[2] = {static_cast<short>(za::clamp(r.l[i], -32768.f, 32767.f)),
            static_cast<short>(za::clamp(r.r[i], -32768.f, 32767.f))};
        fwrite(s, 2, 2, f);
    }
    fclose(f);
}

// The interaural time difference: the lag (samples, within 1 ms) at which the right ear best matches the left one;
// positive when the right ear hears it first.
int itd(const Render& r, int from, int n)
{
    int best = 0;
    double bestC = -1e30;
    for(int lag = -44; lag <= 44; lag++)
    {
        double c = 0;
        for(int i = from + 50; i < from + n - 50; i++)
        {
            c += static_cast<double>(r.l[i]) * r.r[i - lag];
        }
        if(c > bestC)
        {
            bestC = c;
            best = lag;
        }
    }
    return best;
}

// Octave levels (dB, both ears' power) of 1-2, 2-4, 4-8 and 8-16 kHz over 4096 samples from `from`: Goertzel at six
// frequencies an octave, Hann-windowed, averaged over 4 windows.
struct Octaves
{
    float band[4]{};
};
Octaves octaves(const Render& r, int from)
{
    Octaves o;
    constexpr int n = 4096;
    double power[4]{};
    for(int w = 0; w < 4; w++)
    {
        const int start = from + w * n;
        if(start + n > static_cast<int>(r.l.size()))
        {
            break;
        }
        for(int k = 0; k < 24; k++)
        {
            const double f = 1000.0 * za::pow(2.0, (k + 0.5) / 6.0);
            const double coeff = 2.0 * za::cos(2.0 * 3.14159265 * f / testRate);
            for(int ear = 0; ear < 2; ear++)
            {
                const za::Vector<float>& x = ear == 0 ? r.l : r.r;
                double s1 = 0, s2 = 0;
                for(int i = 0; i < n; i++)
                {
                    const double hann = 0.5 - 0.5 * za::cos(2.0 * 3.14159265 * i / (n - 1));
                    const double s = x[start + i] * hann + coeff * s1 - s2;
                    s2 = s1;
                    s1 = s;
                }
                power[k / 6] += s1 * s1 + s2 * s2 - coeff * s1 * s2;
            }
        }
    }
    for(int b = 0; b < 4; b++)
    {
        o.band[b] = static_cast<float>(10.0 * za::log10(za::max(power[b], 1e-9)));
    }
    return o;
}

// Frequency by zero crossings (upwards) over [from, from + n).
double pitch(const za::Vector<float>& x, int from, int n)
{
    int first = -1;
    int last = -1;
    int count = 0;
    for(int i = from + 1; i < from + n && i < static_cast<int>(x.size()); i++)
    {
        if(x[i - 1] < 0.f && x[i] >= 0.f)
        {
            // (the crossing, interpolated: in samples x 1000)
            if(first < 0)
            {
                first = i;
            }
            else
            {
                count++;
            }
            last = i;
        }
    }
    return count > 0 && last > first ? static_cast<double>(count) * testRate / (last - first) : 0.0;
}

// RT60 from a response's decay (Schroeder's backward integral, the -5 to -25 dB slope), from sample `from`.
float rt60(const Render& r, int from)
{
    const int n = static_cast<int>(r.l.size());
    za::Vector<double> edc(n + 1, 0.0);
    for(int i = n - 1; i >= from; i--)
    {
        edc[i] = edc[i + 1] + static_cast<double>(r.l[i]) * r.l[i] + static_cast<double>(r.r[i]) * r.r[i];
    }
    if(edc[from] <= 0.0)
    {
        return 0.f;
    }
    int t5 = -1;
    int t25 = -1;
    for(int i = from; i < n; i++)
    {
        const double level = 10.0 * za::log10(za::max(edc[i], 1e-30) / edc[from]);
        if(t5 < 0 && level <= -5.0)
        {
            t5 = i;
        }
        if(t25 < 0 && level <= -25.0)
        {
            t25 = i;
            break;
        }
    }
    if(t5 < 0 || t25 < 0)
    {
        return -1.f; // (it didn't decay by 25 dB within the render)
    }
    return 3.f * static_cast<float>(t25 - t5) / testRate;
}

struct Result
{
    int passed{0};
    int failed{0};
    void check(const char* test, bool ok)
    {
        Con_Printf("snd_test %s: %s\n", test, ok ? "PASS" : "FAIL");
        (ok ? passed : failed)++;
    }
};

Listener centred()
{
    return Listener{}; // at the origin, looking along +x, right -y, up +z
}

Features baseFeatures()
{
    Features f;
    f.hrtf = true;
    f.bilinear = vr_snd_hrtf_interp.value != 0.f;
    f.hrtfGain = za::clamp(vr_snd_hrtf_gain.value, 0.f, 4.f);
    f.unitsPerMetre = units::perMetre;
    return f;
}

int testFrame()
{
    const int f = static_cast<int>(vr_snd_frame.value);
    return f >= 1024 ? 1024 : f >= 512 ? 512 : 256;
}

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
    res.check("circle: ahead and behind balanced (|ILD| < 3 dB)", qza::abs(ild[0]) < 3.f && qza::abs(ild[4]) < 3.f);
    res.check("circle: the right ear first on the right, the left on the left (ITD)", lag[2] > 0 && lag[6] < 0);
    // Ahead and behind: the same level at each ear, told apart by the pinnae's colouring (behind: less of the highs).
    const float frontBack = (oct[0].band[2] - oct[4].band[2]) + (oct[0].band[3] - oct[4].band[3]);
    Con_Printf("snd_test circle: ahead minus behind, 4-16 kHz: %+.1f dB\n", frontBack);
    res.check("circle: behind duller than ahead (4-16 kHz, > 2 dB)", frontBack > 2.f);
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
            if(!(qza::abs(x) < 1e30f))
            {
                c.bad++;
                continue;
            }
            peak = za::max(peak, qza::abs(x));
        }
    }
    c.peak = peak;
    for(const za::Vector<float>* ch : {&r.l, &r.r})
    {
        const za::Vector<float>& x = *ch;
        for(za::SizeT i = static_cast<za::SizeT>(za::max(from, 2)); i < x.size(); i++)
        {
            const float e = qza::abs(x[i] - 2.f * x[i - 1] + x[i - 2]) / peak;
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
                za::fill(l.begin(), l.end(), 0.f);
                za::fill(rr.begin(), rr.end(), 0.f);
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

void addWall(Mesh& mesh, float x, float upm)
{
    const float m = upm;
    mesh.addBox(glm::vec3{(x - 0.15f) * m, -2.f * m, -1.5f * m}, glm::vec3{(x + 0.15f) * m, 2.f * m, 1.5f * m}, false,
        SurfaceMaterial::Stone, upm);
}

void makeWallSub(int, Mesh& out, void* user)
{
    addWall(out, 0.f, *static_cast<const float*>(user));
}

void testOcclusion(Result& res)
{
    Mixer m;
    Simulation sim;
    const int frame = testFrame();
    if(!m.create(testRate, frame, "") ||
        !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, 1, 1.f, 1024))
    {
        res.check("occlusion (no Steam Audio)", false);
        return;
    }
    const float upm = units::perMetre;
    const Sound snd = makeSound(testRate, true, noise);
    const Listener lis = centred();
    Features f = baseFeatures();
    f.occlusion = za::clamp(vr_snd_occlusion.value, 0.1f, 2.f);
    SimSettings ss;
    ss.occlusion = true;
    ss.occlusionSamples = static_cast<int>(vr_snd_occlusion_samples.value);
    ss.occlusionRadius = za::clamp(vr_snd_occlusion_radius.value, 0.05f, 4.f);
    const glm::vec3 at{5.f * upm, 0.f, 0.f};
    Simulation::Source src;
    src.active = true;
    src.serial = 1;
    src.pos = toSteam(at, upm);
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);

    const auto measureWith = [&](const char* name, DirectResult& d) {
        sim.runDirectNow(lc, &src, 1, ss);
        const bool have = sim.direct(0, 1, d);
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, 0.6f, [&](double, VoiceInput& in) {
            in.pos = at;
            in.hasDirect = have;
            in.direct = d;
        });
        writeWav(name, out);
        const int from = testRate / 5;
        return measure(out.l.data() + from, out.r.data() + from, static_cast<int>(out.l.size()) - from, testRate);
    };

    // A floor only, then a wall between.
    Mesh open;
    open.addBox(glm::vec3{-20.f * upm, -20.f * upm, -2.f * upm}, glm::vec3{20.f * upm, 20.f * upm, -1.8f * upm}, false,
        SurfaceMaterial::Stone, upm);
    Mesh walled = open;
    addWall(walled, 2.5f, upm);
    sim.buildSceneNow(ZA_MOVE(open));
    DirectResult dOpen, dWall, dMoved, dAway;
    const Levels a = measureWith("occl_open", dOpen);
    sim.buildSceneNow(ZA_MOVE(walled));
    const Levels b = measureWith("occl_wall", dWall);
    Con_Printf("snd_test occlusion: open: occlusion %.2f, %.1f dB (lows %.1f, highs %.1f); behind the wall: occlusion %.2f, "
               "transmission %.2f %.2f %.2f, %.1f dB (lows %.1f, highs %.1f); strength %.2f\n",
        dOpen.occlusion, a.rms, a.low, a.high, dWall.occlusion, dWall.transmission[0], dWall.transmission[1],
        dWall.transmission[2], b.rms, b.low, b.high, f.occlusion);
    res.check("occlusion: behind a wall quieter (> 6 dB)", a.rms - b.rms > 6.f);
    res.check("occlusion: behind a wall duller (highs lose > 3 dB more than lows)", (a.high - b.high) - (a.low - b.low) > 3.f);

    // A brush model (an instance of a sub-scene) moved in the way, then up out of it: a door opening.
    Mesh floorOnly;
    floorOnly.addBox(glm::vec3{-20.f * upm, -20.f * upm, -2.f * upm}, glm::vec3{20.f * upm, 20.f * upm, -1.8f * upm}, false,
        SurfaceMaterial::Stone, upm);
    sim.buildSceneNow(ZA_MOVE(floorOnly));
    const auto place = [&](float x, float up) {
        IPLMatrix4x4 t{};
        t.elements[0][0] = t.elements[1][1] = t.elements[2][2] = t.elements[3][3] = 1.f;
        const IPLVector3 o = toSteam(glm::vec3{x * upm, 0.f, up * upm}, upm);
        t.elements[0][3] = o.x;
        t.elements[1][3] = o.y;
        t.elements[2][3] = o.z;
        float u = upm;
        sim.beginInstances();
        sim.placeInstance(1, 1, t, makeWallSub, &u);
        sim.update(lc, &src, 0, SimSettings{}, 0.0); // (applied: nothing runs)
    };
    place(2.5f, 0.f);
    measureWith("occl_door_shut", dMoved);
    place(2.5f, 10.f);
    measureWith("occl_door_open", dAway);
    Con_Printf("snd_test occlusion: a door (instance) in the way: occlusion %.2f; moved up 10 m: %.2f\n", dMoved.occlusion,
        dAway.occlusion);
    res.check("occlusion: a moving brush model occludes, then not once moved", dMoved.occlusion < 0.3f && dAway.occlusion > 0.9f);
}

void testReverb(Result& res)
{
    const float upm = units::perMetre;
    const Listener lis = centred();
    const Sound snd = makeSound(testRate * 4, false, click);
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);
    struct Room
    {
        const char* name;
        glm::vec3 size; // metres
    };
    const Room rooms[2] = {{"small", {3.f, 3.f, 2.5f}}, {"hall", {30.f, 20.f, 12.f}}};
    const IPLReflectionEffectType types[3] = {
        IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, IPL_REFLECTIONEFFECTTYPE_HYBRID, IPL_REFLECTIONEFFECTTYPE_CONVOLUTION};
    const char* typeNames[3] = {"parametric", "hybrid", "convolution"};
    for(int t = 0; t < 3; t++)
    {
        float measured[2]{};
        float simulated[2]{};
        for(int k = 0; k < 2; k++)
        {
            Mixer m;
            Simulation sim;
            const int frame = testFrame();
            if(!m.create(testRate, frame, "") || !sim.create(testRate, frame, types[t], 1, 2.f, 4096))
            {
                res.check("reverb (no Steam Audio)", false);
                return;
            }
            m.setReverb(types[t], 1, 2.f);
            Mesh room;
            const glm::vec3 half = rooms[k].size * 0.5f * upm;
            room.addBox(-half, half, true, SurfaceMaterial::Stone, upm);
            sim.buildSceneNow(ZA_MOVE(room));
            SimSettings ss;
            ss.reverb = true;
            ss.rays = 4096;
            ss.bounces = 32;
            ss.duration = 2.f;
            ss.order = 1;
            sim.runReflectionsNow(lc, ss);
            IPLReflectionEffectParams p{};
            if(!sim.reflections(p))
            {
                res.check("reverb (no simulation)", false);
                return;
            }
            simulated[k] = p.reverbTimes[1];
            Features f = baseFeatures();
            f.reverb = 1.f;
            const glm::vec3 at = lis.fwd * upm; // 1 m ahead
            const Render out = renderVoice(m, snd.cache(), lis, f, &p, 3.f, [&](double, VoiceInput& in) { in.pos = at; });
            writeWav((za::String{"reverb_"} + typeNames[t] + "_" + rooms[k].name).cStr(), out);
            const int from = testRate / 50; // (after the dry click: 20 ms)
            measured[k] = rt60(out, from);
            // The wet energy after 20 ms against the dry click's (its first 5 ms).
            // The energy after the dry click (its first 5 ms) in three windows, against the click's.
            double dry = 0, early = 0, mid = 0, late = 0;
            for(int i = 0; i < static_cast<int>(out.l.size()); i++)
            {
                const double e = static_cast<double>(out.l[i]) * out.l[i] + static_cast<double>(out.r[i]) * out.r[i];
                const double t = static_cast<double>(i) / testRate;
                (t < 0.005 ? dry : t < 0.1 ? early : t < 0.5 ? mid : late) += e;
            }
            const auto rel = [&](double e) { return 10.0 * za::log10(za::max(e, 1e-9) / za::max(dry, 1e-9)); };
            Con_Printf("snd_test reverb %s %s (%.0f x %.0f x %.0f m): simulated RT60 %.2f %.2f %.2f s, measured %.2f s; "
                       "energy after the click, 5-100 ms %+.1f dB, 0.1-0.5 s %+.1f dB, 0.5-3 s %+.1f dB\n",
                typeNames[t], rooms[k].name, rooms[k].size.x, rooms[k].size.y, rooms[k].size.z, p.reverbTimes[0],
                p.reverbTimes[1], p.reverbTimes[2], measured[k], rel(early), rel(mid), rel(late));
        }
        char name[96];
        q_snprintf(name, sizeof name, "reverb %s: the hall rings longer than the small room (x2)", typeNames[t]);
        // (Convolution has no decay times of its own: the measured one only.)
        const bool simulatedOk = types[t] == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION || simulated[1] > 2.f * simulated[0];
        res.check(name, simulatedOk && measured[1] > 2.f * za::max(measured[0], 0.01f));
    }
}

void testDoppler(Result& res)
{
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("doppler (no Steam Audio)", false);
        return;
    }
    const float upm = units::perMetre;
    const Sound snd = makeSound(441, true, sine1k); // (ten periods: loops seamlessly)
    const Listener lis = centred();
    const float speed = 1000.f; // units/s: a rocket
    const float seconds = 3.f;
    const auto run = [&](float scale, double& coming, double& going) {
        Features f = baseFeatures();
        f.doppler = scale;
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, seconds, [&](double t, VoiceInput& in) {
            const float x = -1500.f + speed * static_cast<float>(t);
            in.pos = glm::vec3{x, -1.f * upm, 0.f};
            in.vel = glm::vec3{speed, 0.f, 0.f};
        });
        if(scale > 0.f)
        {
            writeWav("doppler", out);
        }
        coming = pitch(out.l, testRate / 5, testRate / 3);
        going = pitch(out.l, static_cast<int>(out.l.size()) - testRate / 2, testRate / 3);
    };
    double coming = 0, going = 0, coming0 = 0, going0 = 0;
    run(1.f, coming, going);
    run(0.f, coming0, going0);
    const double c = 343.0 * upm;
    const double expectComing = 1000.0 * c / (c - speed * 0.99);
    const double expectGoing = 1000.0 * c / (c + speed * 0.99);
    Con_Printf("snd_test doppler: %.0f units/s (%.1f m/s): coming %.1f Hz (expected %.1f), going %.1f Hz (expected %.1f); "
               "off: %.1f, %.1f Hz\n",
        speed, speed / upm, coming, expectComing, going, expectGoing, coming0, going0);
    res.check("doppler: higher coming, lower going (within 2%)",
        qza::abs(coming / expectComing - 1.0) < 0.02 && qza::abs(going / expectGoing - 1.0) < 0.02);
    res.check("doppler: off leaves the pitch (within 0.5%)", qza::abs(coming0 / 1000.0 - 1.0) < 0.005 &&
                                                                 qza::abs(going0 / 1000.0 - 1.0) < 0.005);
}

void testNearField(Result& res)
{
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("nearfield (no Steam Audio)", false);
        return;
    }
    const float upm = units::perMetre;
    const Sound snd = makeSound(testRate, true, noise);
    const Listener lis = centred();
    const auto ild = [&](float metres, float strength) {
        Features f = baseFeatures();
        f.nearfield = strength;
        const glm::vec3 at = lis.right * (metres * upm);
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, 0.5f, [&](double, VoiceInput& in) { in.pos = at; });
        const int from = testRate / 10;
        const Levels lv = measure(out.l.data() + from, out.r.data() + from, static_cast<int>(out.l.size()) - from, testRate);
        return lv.right - lv.left;
    };
    const float strength = za::max(0.25f, za::clamp(vr_snd_nearfield.value, 0.f, 2.f));
    const float nearOn = ild(0.2f, strength);
    const float farOn = ild(2.f, strength);
    const float nearOff = ild(0.2f, 0.f);
    Con_Printf("snd_test nearfield: ILD at 0.2 m %+.1f dB (near field off: %+.1f), at 2 m %+.1f dB\n", nearOn, nearOff, farOn);
    res.check("nearfield: a sound at the ear has a larger ILD than at 2 m (> 4 dB more)", nearOn - farOn > 4.f);
    res.check("nearfield: off, the same ILD near and far (within 1.5 dB)", qza::abs(nearOff - farOn) < 1.5f);
}

// In a map: how loud a sound is by its distance from the head, Quake's panning (as vr_snd_spatial 0 mixes it) against
// the spatial mix (the settings' HRTF, occlusion from this map's own scene, the air, the near field; no reverb), each
// in dB against the sound itself. The sounds in sight of the head (the world's line of sight), round it, at three
// heights: the head's, a monster's origin (24 units over the floor), on the floor (2 units over it: an item) and half a
// unit into it (a prop's contact as it knocks: hidden by the floor, 21 dB under Quake, till outOfSolid). The author found sounds 5-10 m away too quiet (NOTES.md vrfiringrange_2026-10-01_00-29-12).
void testDistance(Result& res)
{
    if(!cl.worldmodel || cls.state != ca_connected || !shm)
    {
        Con_Printf("snd_test distance: skipped (needs a map and sound)\n");
        return;
    }
    Mixer m;
    Simulation sim;
    const int frame = testFrame();
    if(!m.create(testRate, frame, "") || !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, 1, 1.f, 1024))
    {
        res.check("distance (no Steam Audio)", false);
        return;
    }
    Features f = featuresFromCvars();
    f.reverb = 0.f;
    f.doppler = 0.f;
    const float upm = f.unitsPerMetre;
    Mesh mesh;
    appendBrushModel(cl.worldmodel, mesh, upm);
    sim.buildSceneNow(ZA_MOVE(mesh));
    Listener lis;
    lis.pos = glm::vec3{listener_origin[0], listener_origin[1], listener_origin[2]};
    lis.fwd = glm::vec3{listener_forward[0], listener_forward[1], listener_forward[2]};
    lis.right = glm::vec3{listener_right[0], listener_right[1], listener_right[2]};
    lis.up = glm::vec3{listener_up[0], listener_up[1], listener_up[2]};
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);
    SimSettings ss;
    ss.occlusion = f.occlusion > 0.f;
    ss.occlusionSamples = static_cast<int>(vr_snd_occlusion_samples.value);
    ss.occlusionRadius = za::clamp(vr_snd_occlusion_radius.value, 0.05f, 4.f);
    ss.air = f.air;

    const Sound snd = makeSound(testRate, true, noise);
    const float monoDb = [&] {
        double s = 0;
        const short* d = reinterpret_cast<const short*>(snd.cache()->data);
        for(int i = 0; i < testRate; i++)
        {
            s += static_cast<double>(d[i]) * d[i];
        }
        return db(s / testRate);
    }();
    const auto clear = [&](const glm::vec3& a, const glm::vec3& b) {
        trace_t trace;
        ZA_MEMSET(&trace, 0, sizeof trace);
        trace.fraction = 1.f;
        vec3_t from{a.x, a.y, a.z};
        vec3_t to{b.x, b.y, b.z};
        SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, from, to, &trace);
        return trace.fraction >= 1.f && !trace.allsolid && !trace.startsolid;
    };
    const float distances[] = {1.f, 2.f, 3.f, 5.f, 7.f, 10.f, 15.f, 20.f};
    const char* heights[] = {"head", "monster", "floor", "contact"};
    const float distMult = 1.f / 1000.f; // ATTN_NORM
    Con_Printf("snd_test distance: from %.0f %.0f %.0f, %.2f units a metre; dB against the sound, Quake's panning | the "
               "spatial mix (without occlusion) | occlusion, the air's highs\n",
        lis.pos.x, lis.pos.y, lis.pos.z, upm);
    float worst = 0.f;
    for(int h = 0; h < 4; h++)
    {
        for(const float d : distances)
        {
            constexpr int around = 36;
            Simulation::Source src[around]{};
            glm::vec3 at[around];
            int n = 0;
            for(int k = 0; k < around; k++)
            {
                const float a = static_cast<float>(k) * 2.f * 3.14159265f / around;
                glm::vec3 p = lis.pos + d * upm * (za::cos(a) * lis.fwd + za::sin(a) * lis.right);
                if(h > 0)
                {
                    // Down to the floor under it.
                    trace_t trace;
                    ZA_MEMSET(&trace, 0, sizeof trace);
                    trace.fraction = 1.f;
                    vec3_t from{p.x, p.y, p.z};
                    vec3_t to{p.x, p.y, p.z - 512.f};
                    SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, from, to, &trace);
                    if(trace.fraction >= 1.f || trace.startsolid)
                    {
                        continue;
                    }
                    p.z = trace.endpos[2] + (h == 1 ? 24.f : h == 2 ? 2.f : -0.5f);
                }
                if(!clear(lis.pos, h == 3 ? p + glm::vec3{0.f, 0.f, 1.f} : p) || qza::abs(glm::length(p - lis.pos) / upm - d) > 0.5f * d)
                {
                    continue;
                }
                at[n] = p;
                src[n].active = true;
                src[n].serial = 1;
                src[n].pos = toSteam(outOfSolid(p, lis.pos), upm); // (as the game places it)
                n++;
            }
            if(n == 0)
            {
                Con_Printf("snd_test distance: %4.0f m, %-7s: no place in sight\n", d, heights[h]);
                continue;
            }
            sim.runDirectNow(lc, src, n, ss);
            double quake = 0, spatial = 0, open = 0, occ = 0, air = 0;
            int guessed = 0; // hidden by the world's line of sight (the stand-in till the first result)
            for(int k = 0; k < n; k++)
            {
                const glm::vec3 dir = glm::normalize(at[k] - lis.pos);
                const float dot = glm::dot(dir, lis.right);
                const float scale = za::max(0.f, 1.f - glm::length(at[k] - lis.pos) * distMult);
                const float power = 0.5f * ((1.f - dot) * (1.f - dot) + (1.f + dot) * (1.f + dot)) * scale * scale;
                quake += 10.0 * za::log10(za::max(power, 1e-12f));
                DirectResult dr;
                const bool have = sim.direct(k, 1, dr);
                for(int withOcclusion = 1; withOcclusion >= 0; withOcclusion--)
                {
                    Features fk = f;
                    fk.occlusion = withOcclusion ? f.occlusion : 0.f;
                    const Render out = renderVoice(m, snd.cache(), lis, fk, nullptr, 0.25f, [&](double, VoiceInput& in) {
                        in.pos = at[k];
                        in.gain = scale;
                        in.hasDirect = have;
                        in.direct = dr;
                    });
                    const int from = testRate / 20;
                    const Levels lv = measure(out.l.data() + from, out.r.data() + from,
                        static_cast<int>(out.l.size()) - from, testRate);
                    (withOcclusion ? spatial : open) += lv.rms - monoDb;
                }
                guessed += clear(lis.pos, outOfSolid(at[k], lis.pos)) ? 0 : 1;
                occ += have ? dr.occlusion : 1.f;
                air += have ? dr.air[2] : 1.f;
            }
            quake /= n;
            spatial /= n;
            open /= n;
            Con_Printf("snd_test distance: %4.0f m, %-7s (%2d places): Quake %+6.1f dB | spatial %+6.1f dB (%+6.1f) | "
                       "occlusion %.2f, air %.2f, %d hidden by the stand-in; spatial - Quake %+5.1f dB\n",
                d, heights[h], n, quake, spatial, open, occ / n, air / n, guessed, spatial - quake);
            worst = za::max(worst, static_cast<float>(qza::abs(spatial - quake)));
        }
    }
    res.check("distance: in sight, as loud as Quake's mix (within 3 dB; a contact too)", worst < 3.f);
}

void testHands(Result& res)
{
    const hands::State& hs = hands::current();
    if(!hs.valid || !VR_IsActive() || cl.viewentity <= 0 || !shm)
    {
        Con_Printf("snd_test hands: skipped (needs a map, VR and sound)\n");
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        channel_t ch{};
        ch.entnum = cl.viewentity;
        ch.entchannel = h == HAND_MAIN ? 1 : 5;
        ch.master_vol = 255;
        ch.dist_mult = 1.f / 1000.f;
        VectorCopy(listener_origin, ch.origin);
        const int handled = VR_SndSpatialize(&ch);
        const glm::vec3 at{ch.origin[0], ch.origin[1], ch.origin[2]};
        const glm::vec3 hand = hs.muzzleValid[h] ? hs.muzzle[h] : hs.pos[h];
        const glm::vec3 head{listener_origin[0], listener_origin[1], listener_origin[2]};
        const glm::vec3 right{listener_right[0], listener_right[1], listener_right[2]};
        const float side = glm::dot(hand - head, right);
        Con_Printf("snd_test hands: %s hand's channel %d: from %.0f %.0f %.0f (the hand %.1f units off it, %.1f from the "
                   "head, %.1f to the right), left %d, right %d\n",
            h == HAND_MAIN ? "main" : "off", ch.entchannel, at.x, at.y, at.z, glm::length(at - hand), glm::length(at - head),
            side, ch.leftvol, ch.rightvol);
        const bool panned = qza::abs(side) < 2.f || (side > 0.f) == (ch.rightvol > ch.leftvol);
        res.check(h == HAND_MAIN ? "hands: the main hand's weapon channel plays from it" : "hands: the off hand's from it",
            handled && glm::length(at - hand) < 0.5f && panned);
    }
}

void bench(Result& res)
{
    Mixer m;
    Simulation sim;
    const int frame = testFrame();
    if(!m.create(testRate, frame, "") || !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_HYBRID, 1, 1.5f, 2048))
    {
        res.check("bench (no Steam Audio)", false);
        return;
    }
    m.setReverb(IPL_REFLECTIONEFFECTTYPE_HYBRID, 1, 1.5f);
    const float upm = units::perMetre;
    const Listener lis = centred();
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);
    Mesh room;
    room.addBox(glm::vec3{-10.f, -8.f, -3.f} * upm, glm::vec3{10.f, 8.f, 5.f} * upm, true, SurfaceMaterial::Stone, upm);
    addWall(room, 3.f, upm);
    sim.buildSceneNow(ZA_MOVE(room));
    SimSettings ss;
    ss.reverb = true;
    ss.occlusion = true;
    ss.air = true;
    sim.runReflectionsNow(lc, ss);
    IPLReflectionEffectParams reverb{};
    bool haveReverb = sim.reflections(reverb);

    constexpr int voices = 32;
    za::Vector<Sound> sounds;
    for(int i = 0; i < 4; i++)
    {
        sounds.pushBack(makeSound(testRate + i * 1000, true, noise));
    }
    za::Array<Simulation::Source, voices> src{};
    za::Array<glm::vec3, voices> at{};
    for(int i = 0; i < voices; i++)
    {
        const float a = static_cast<float>(i) * 2.399963f;
        const float r = (1.f + static_cast<float>(i % 8)) * upm;
        at[i] = glm::vec3{za::cos(a) * r, za::sin(a) * r, static_cast<float>(i % 3 - 1) * 20.f};
        src[i].active = true;
        src[i].serial = 1;
        src[i].pos = toSteam(at[i], upm);
    }
    double directMs = Sys_DoubleTime();
    sim.runDirectNow(lc, src.data(), voices, ss);
    directMs = (Sys_DoubleTime() - directMs) * 1000.0;
    double reflMs = Sys_DoubleTime();
    sim.runReflectionsNow(lc, ss);
    reflMs = (Sys_DoubleTime() - reflMs) * 1000.0;

    const int perFrame = testRate / 90; // (a 90 Hz frame's samples: 490)
    const int blocksPerFrame = (perFrame + frame - 1) / frame;
    za::Vector<float> l(blocksPerFrame * frame), r(blocksPerFrame * frame);
    const auto time = [&](const Features& f, bool withDirect, bool withReverb) {
        for(int i = 0; i < voices; i++)
        {
            m.start(i, sounds[i % 4].cache(), 0.0);
            VoiceInput in;
            in.pos = at[i];
            in.gain = 0.2f;
            in.vel = glm::vec3{100.f, 0.f, 0.f};
            in.hasDirect = withDirect && sim.direct(i, 1, in.direct);
            m.set(i, in);
        }
        const int frames = 300;
        for(int k = 0; k < 10; k++) // (warm)
        {
            m.render(blocksPerFrame, lis, f, withReverb && haveReverb ? &reverb : nullptr, l.data(), r.data());
        }
        const double start = Sys_DoubleTime();
        for(int k = 0; k < frames; k++)
        {
            m.render(blocksPerFrame, lis, f, withReverb && haveReverb ? &reverb : nullptr, l.data(), r.data());
        }
        return (Sys_DoubleTime() - start) * 1000.0 / frames;
    };
    Features all = baseFeatures();
    all.occlusion = 1.f;
    all.air = true;
    all.reverb = 0.5f;
    all.doppler = 1.f;
    all.nearfield = 1.f;
    Features hrtfOnly = baseFeatures();
    Features none = baseFeatures();
    none.hrtf = false;
    const double tAll = time(all, true, true);
    const double tHrtf = time(hrtfOnly, false, false);
    const double tNone = time(none, false, false);
    jobs::setParallel(false);
    const double tAllSerial = time(all, true, true);
    jobs::setParallel(true);
    // Each reverb algorithm (2 s of response).
    const IPLReflectionEffectType types[3] = {
        IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, IPL_REFLECTIONEFFECTTYPE_HYBRID, IPL_REFLECTIONEFFECTTYPE_CONVOLUTION};
    double tType[3]{};
    for(int t = 0; t < 3; t++)
    {
        Simulation other;
        other.create(testRate, frame, types[t], 1, 2.f, 2048);
        Mesh again;
        again.addBox(glm::vec3{-10.f, -8.f, -3.f} * upm, glm::vec3{10.f, 8.f, 5.f} * upm, true, SurfaceMaterial::Stone, upm);
        other.buildSceneNow(ZA_MOVE(again));
        SimSettings two = ss;
        two.duration = 2.f;
        other.runReflectionsNow(lc, two);
        haveReverb = other.reflections(reverb);
        m.setReverb(types[t], 1, 2.f);
        tType[t] = time(all, true, true);
    }
    for(int i = 0; i < voices; i++)
    {
        m.stop(i);
    }

    // Quake's own mixing of 32 16-bit channels over the same frame (snd_mix.c SND_PaintChannelFrom16).
    za::Vector<int> paint(perFrame * 2);
    double tQuake = 0.0;
    {
        const int frames = 300;
        volatile int sink = 0;
        const double start = Sys_DoubleTime();
        for(int k = 0; k < frames; k++)
        {
            za::fill(paint.begin(), paint.end(), 0);
            for(int c = 0; c < voices; c++)
            {
                const short* sfx = reinterpret_cast<const short*>(sounds[c % 4].cache()->data) + (k * perFrame) % 20000;
                const int lv = 100 + c;
                const int rv = 150 - c;
                for(int i = 0; i < perFrame; i++)
                {
                    paint[2 * i] += sfx[i] * lv;
                    paint[2 * i + 1] += sfx[i] * rv;
                }
            }
            sink = sink + paint[k % paint.size()];
        }
        tQuake = (Sys_DoubleTime() - start) * 1000.0 / frames;
    }
    Con_Printf("snd_test bench: every feature, with the reverb parametric %.3f ms, hybrid %.3f ms, convolution %.3f ms (2 s)\n",
        tType[0], tType[1], tType[2]);
    Con_Printf("snd_test bench: 32 voices, %d-sample frames, a 90 Hz frame (%d samples): every feature %.3f ms (%.3f ms on one "
               "thread), HRTF only %.3f ms, Quake's panning in the voices %.3f ms; Quake's own mixer %.3f ms. Simulation "
               "(pool tasks): direct paths of 32 %.2f ms, reverb %.2f ms (%d workers)\n",
        frame, perFrame, tAll, tAllSerial, tHrtf, tNone, tQuake, directMs, reflMs, jobs::workers());
    res.check("bench: 32 voices with every feature under 2 ms a frame", tAll < 2.0);
}

} // namespace

void test_f()
{
    const char* which = Cmd_Argc() > 1 ? Cmd_Argv(1) : "all";
    if(!steamaudio::api())
    {
        Con_Printf("snd_test: Steam Audio isn't loaded (%s)\n", steamaudio::status());
        return;
    }
    const bool all = !ZA_STRCMP(which, "all");
    Result res;
    const double start = Sys_DoubleTime();
    if(all || !ZA_STRCMP(which, "circle"))
    {
        testCircle(res);
    }
    if(all || !ZA_STRCMP(which, "clicks"))
    {
        testClicks(res);
    }
    if(all || !ZA_STRCMP(which, "occlusion"))
    {
        testOcclusion(res);
    }
    if(all || !ZA_STRCMP(which, "reverb"))
    {
        testReverb(res);
    }
    if(all || !ZA_STRCMP(which, "doppler"))
    {
        testDoppler(res);
    }
    if(all || !ZA_STRCMP(which, "nearfield"))
    {
        testNearField(res);
    }
    if(all || !ZA_STRCMP(which, "distance"))
    {
        testDistance(res);
    }
    if(all || !ZA_STRCMP(which, "hands"))
    {
        testHands(res);
    }
    if(all || !ZA_STRCMP(which, "bench"))
    {
        bench(res);
    }
    Con_Printf("snd_test: %d passed, %d failed (%.1f s)\n", res.passed, res.failed, Sys_DoubleTime() - start);
}

} // namespace qvr::audio
