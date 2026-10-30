// vr_audiotest.cpp -- vr_snd_test: offline renders through the spatial audio's own mixer and simulation
// (vr_audio.cpp, vr_audiosim.cpp), measured, each test a line and PASS or FAIL. Made scenes (boxes and walls in
// Steam Audio's scene, as the map's faces are), made sounds (noise, a sine), 44.1 kHz; nothing of the game's mix is
// touched (the tests have their own mixer and simulator). The renders are written to <game>/sound_tests/test_<name>.wav.
//
//   circle     a noise circling the head at 2 m: left/right level and time differences, front/back brightness
//   ild        a noise at -90..90 degrees by 5 (and +-30 up): the 2-8 kHz left/right balance alone and among 15
//              voices, and at 0.25-5 kHz through Quake's 11 kHz lowpass, with the voices' anti-aliasing and without
//   clicks     a tone turning round the head, each HRTF interpolation: steps at the frames' edges
//   occlusion  a noise 5 m ahead, then behind a wall (a static one, then a brush model's instance moved in and out)
//   reverb     a click in a small room and a big hall, each reverb quality: the simulated and the measured RT60
//              and a wall 1 m to one side: the reverb's lean to it, the listener turned four ways
//   doppler    a 1 kHz tone passing the head at 1000 units/s: the pitch coming and going
//   nearfield  a noise at the right ear (0.2 m) and 2 m to the right: the level difference between the ears
//   distance   (in a map) how loud by the distance, in sight of the head: Quake's panning against the spatial mix
//   hands      (in a map, in VR) the player's weapon channels' origins and panning: the hands'
//   golden     fixed renders of the whole mix path (24 moving voices, every feature; slowed; the band-limited copies)
//              against a reference copy (sound_tests/golden_<case>_ref.f32): bit-identical, or the SNR
//   bench      32 voices, every feature on, then off; and Quake's own mixing of 32 channels: ms per 90 Hz frame
//   all        each of them

#include "vr_audiotest_internal.hpp"

namespace qvr::audio
{

using namespace testkit;


// ----------------------------------------------------------------------------
// Measurements

namespace testkit
{

float db(double meanSquare)
{
    return meanSquare > 1e-20 ? static_cast<float>(10.0 * za::log10(meanSquare / (32768.0 * 32768.0))) : -200.f;
}

} // namespace testkit

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

namespace testkit
{

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
        za::fill(l, 0.f);
        za::fill(r, 0.f);
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

} // namespace testkit

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
    if(all || !ZA_STRCMP(which, "ild"))
    {
        testIld(res);
    }
    if(all || !ZA_STRCMP(which, "fullband"))
    {
        testFullBand(res);
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
        testReverbSide(res);
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
    if(all || !ZA_STRCMP(which, "golden"))
    {
        testGolden(res);
    }
    if(all || !ZA_STRCMP(which, "bench"))
    {
        bench(res);
    }
    Con_Printf("snd_test: %d passed, %d failed (%.1f s)\n", res.passed, res.failed, Sys_DoubleTime() - start);
}

} // namespace qvr::audio
