// vr_audiotest_mix.cpp -- vr_snd_test's bench, full band and golden (see vr_audiotest.cpp).

#include "vr_audiotest_internal.hpp"

namespace qvr::audio::testkit
{

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
            za::fill(paint, 0);
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


// ----------------------------------------------------------------------------
// Full band (vr_snd_fullband)

// A sound of `inrate` loaded as S_LoadSound does (snd_mem.c): Quake's held samples at the mix's rate, and the
// band-limited copy after them (VR_SndBandLimit).
struct Loaded
{
    za::Vector<unsigned char> bytes;
    [[nodiscard]] const sfxcache_t* cache() const
    {
        return reinterpret_cast<const sfxcache_t*>(bytes.data());
    }
};

Loaded loadAt(int inrate, int samples, int width, bool loop, float (*gen)(int i, unsigned& seed))
{
    za::Vector<unsigned char> raw(static_cast<za::SizeT>(samples * width), 0);
    unsigned seed = 4321;
    for(int i = 0; i < samples; i++)
    {
        const int v = static_cast<int>(za::clamp(gen(i, seed), -32767.f, 32767.f));
        if(width == 2)
        {
            raw[2 * i] = static_cast<unsigned char>(v & 255);
            raw[2 * i + 1] = static_cast<unsigned char>((v >> 8) & 255);
        }
        else
        {
            raw[i] = static_cast<unsigned char>(za::clamp(v / 256 + 128, 0, 255));
        }
    }
    const float stepscale = static_cast<float>(inrate) / testRate;
    const int outcount = static_cast<int>(static_cast<float>(samples) / stepscale);
    const int fb = (outcount * width + 3) & ~3;
    Loaded s;
    s.bytes.resize(sizeof(sfxcache_t) + static_cast<za::SizeT>(fb + 2 * outcount + 16), 0);
    sfxcache_t* sc = reinterpret_cast<sfxcache_t*>(s.bytes.data());
    sc->length = outcount;
    sc->loopstart = loop ? 0 : -1;
    sc->speed = testRate;
    sc->width = width;
    sc->fullband = fb;
    const int fracstep = static_cast<int>(stepscale * 256);
    int src = 0;
    int frac = 0;
    for(int i = 0; i < outcount; i++) // (ResampleSfx's)
    {
        if(width == 2)
        {
            reinterpret_cast<short*>(sc->data)[i] = static_cast<short>(raw[2 * src] | (raw[2 * src + 1] << 8));
        }
        else
        {
            reinterpret_cast<signed char*>(sc->data)[i] = static_cast<signed char>(static_cast<int>(raw[src]) - 128);
        }
        frac += fracstep;
        src += frac >> 8;
        frac &= 255;
    }
    VR_SndBandLimit(raw.data(), width, samples, loop ? 0 : -1, fracstep, reinterpret_cast<short*>(sc->data + fb),
        outcount);
    return s;
}

za::Vector<float> samplesOf(const sfxcache_t* sc, bool full)
{
    za::Vector<float> x(static_cast<za::SizeT>(sc->length), 0.f);
    const short* f = fullBandData(sc);
    for(int i = 0; i < sc->length; i++)
    {
        x[i] = full ? static_cast<float>(f[i] * S_FULLBAND_SCALE)
               : sc->width == 2 ? static_cast<float>(reinterpret_cast<const short*>(sc->data)[i])
                                : static_cast<float>(static_cast<signed char>(sc->data[i])) * 256.f;
    }
    return x;
}

// Through Quake's 11 kHz lowpass (S_LowpassTest), a mono sound or one ear (`side` its filter's memory).
void quakeLowpass(za::Vector<float>& x, int side)
{
    const int n = static_cast<int>(x.size());
    za::Vector<int> buf(static_cast<za::SizeT>(n));
    for(int i = 0; i < n; i++)
    {
        buf[i] = static_cast<int>(x[i] * 256.f);
    }
    S_LowpassTest(buf.data(), 1, n, side, 1);
    for(int i = 0; i < n; i++)
    {
        x[i] = static_cast<float>(buf[i]) / 256.f;
    }
}

// dB per Hz in [f0, f1) (earBand's power over the band's width).
float density(const za::Vector<float>& x, int from, float f0, float f1)
{
    return earBand(x, from, f0, f1) - 10.f * za::log10(f1 - f0);
}

float sine441(int i, unsigned&)
{
    return 8000.f * za::sin(2.f * 3.14159265f * 441.f * static_cast<float>(i) / 11025.f);
}

// The voices without Quake's 11 kHz lowpass (vr_snd_fullband; ROUND21.md, "Full-band sound"): each sound resampled
// band-limited (VR_SndBandLimit) instead of Quake's held samples. An id-style 8-bit 11025 Hz noise and a 16-bit 22050 Hz
// one: the copy's images (above the sound's own Nyquist) gone where the held samples' are loud, its level below 4.5 kHz
// Quake's (through its lowpass), the 22 kHz sound's 6-10 kHz kept. Then as a voice (HRTF; 60 degrees right, ahead,
// behind): no images; the 22 kHz sound's highs reach the ears (the cues that tell ahead from behind); the 11 kHz sound's
// level at each ear as through the anti-aliasing and Quake's lowpass below 1.5 kHz, and up to ~1 dB brighter above (a
// voice read the held samples, whose response droops to -3.9 dB at 5.5 kHz; Quake's own channels never had that). And a looping sound's seam: a 441 Hz sine whose
// loop is whole periods comes out as the sine all through, the wrap included.
void testFullBand(Result& res)
{
    const int from = 4096;
    const Loaded id11 = loadAt(11025, 11025 * 2, 1, true, noise);
    const Loaded mod22 = loadAt(22050, 22050 * 2, 2, true, noise);

    // The sounds alone.
    {
        const za::Vector<float> held = samplesOf(id11.cache(), false);
        const za::Vector<float> full = samplesOf(id11.cache(), true);
        za::Vector<float> quake = held;
        quakeLowpass(quake, 0);
        const float heldImg = density(held, from, 5800.f, 22000.f) - density(held, from, 250.f, 4500.f);
        const float fullImg = density(full, from, 5800.f, 22000.f) - density(full, from, 250.f, 4500.f);
        const float quakeImg = density(quake, from, 5800.f, 22000.f) - density(quake, from, 250.f, 4500.f);
        const float pass = earBand(full, from, 250.f, 4500.f) - earBand(quake, from, 250.f, 4500.f);
        Con_Printf("snd_test fullband: 11 kHz sound, 5.8-22 kHz against 0.25-4.5 (dB/Hz): held %.1f, Quake's lowpass "
                   "%.1f, band-limited %.1f; 0.25-4.5 kHz band-limited minus Quake's lowpass %+.2f dB\n",
            heldImg, quakeImg, fullImg, pass);
        res.check("fullband: the held 11 kHz sound has images (> -25 dB)", heldImg > -25.f);
        res.check("fullband: the band-limited copy has none (< -65 dB)", fullImg < -65.f);
        res.check("fullband: its 0.25-4.5 kHz as through Quake's lowpass (within 0.5 dB)", za::abs(pass) < 0.5f);

        const za::Vector<float> held22 = samplesOf(mod22.cache(), false);
        const za::Vector<float> full22 = samplesOf(mod22.cache(), true);
        za::Vector<float> quake22 = held22;
        quakeLowpass(quake22, 1);
        const float kept = density(full22, from, 6000.f, 10000.f) - density(full22, from, 1000.f, 4000.f);
        const float keptQuake = density(quake22, from, 6000.f, 10000.f) - density(quake22, from, 1000.f, 4000.f);
        const float img22 = density(full22, from, 11700.f, 22000.f) - density(full22, from, 1000.f, 4000.f);
        const float heldImg22 = density(held22, from, 11700.f, 22000.f) - density(held22, from, 1000.f, 4000.f);
        Con_Printf("snd_test fullband: 22 kHz sound, 6-10 kHz against 1-4 (dB/Hz): band-limited %+.1f, Quake's lowpass "
                   "%+.1f; 11.7-22 kHz: held %.1f, band-limited %.1f\n",
            kept, keptQuake, heldImg22, img22);
        res.check("fullband: the 22 kHz sound keeps 6-10 kHz (within 1 dB)", za::abs(kept) < 1.f);
        res.check("fullband: and none of its images (< -65 dB)", img22 < -65.f);
    }

    // As voices.
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("fullband (no Steam Audio)", false);
        return;
    }
    const Listener lis = centred();
    const float r = 2.f * units::perMetre;
    const auto render = [&](const sfxcache_t* sc, float az, bool fullBand, float rate = 1.f) {
        Features f = baseFeatures();
        f.fullBand = fullBand;
        f.rate = rate;
        const float a = az * 3.14159265f / 180.f;
        const glm::vec3 pos = r * (za::cos(a) * lis.fwd + za::sin(a) * lis.right);
        return renderVoice(m, sc, lis, f, nullptr, 1.5f, [&](double, VoiceInput& in) { in.pos = pos; });
    };
    const auto throughQuake = [](Render& x) {
        AntiAlias aa;
        aa.apply(x.l.data(), x.r.data(), static_cast<int>(x.l.size()));
        quakeLowpass(x.l, 0);
        quakeLowpass(x.r, 1);
    };
    {
        const Render full = render(id11.cache(), 60.f, true);
        Render quake = render(id11.cache(), 60.f, false);
        throughQuake(quake);
        float worstImg = -200.f;
        float worstLevel = 0.f;
        float worstLow = 0.f;
        for(int ear = 0; ear < 2; ear++)
        {
            const za::Vector<float>& x = ear ? full.r : full.l;
            const za::Vector<float>& y = ear ? quake.r : quake.l;
            worstImg = za::max(worstImg, density(x, from, 5800.f, 22000.f) - density(x, from, 250.f, 4500.f));
            const float d = earBand(x, from, 250.f, 4500.f) - earBand(y, from, 250.f, 4500.f);
            worstLevel = za::abs(d) > za::abs(worstLevel) ? d : worstLevel;
            const float low = earBand(x, from, 250.f, 1500.f) - earBand(y, from, 250.f, 1500.f);
            worstLow = za::abs(low) > za::abs(worstLow) ? low : worstLow;
        }
        Con_Printf("snd_test fullband: 11 kHz voice at 60 degrees: 5.8-22 kHz against 0.25-4.5 %.1f dB/Hz (worse ear); "
                   "minus anti-aliased and Quake's lowpass: 0.25-1.5 kHz %+.2f dB, 0.25-4.5 %+.2f (worse ear; the held "
                   "samples' droop gone)\n",
            worstImg, worstLow, worstLevel);
        res.check("fullband: an 11 kHz voice has no images (< -55 dB)", worstImg < -55.f);
        res.check("fullband: an 11 kHz voice as loud as through Quake's lowpass (0.25-1.5 kHz, within 0.5 dB)",
            za::abs(worstLow) < 0.5f);
        res.check("fullband: and no duller (0.25-4.5 kHz, 0..+1.5 dB)", worstLevel >= 0.f && worstLevel < 1.5f);
    }
    {
        const Render ahead = render(mod22.cache(), 0.f, true);
        const Render behind = render(mod22.cache(), 180.f, true);
        Render aheadQ = render(mod22.cache(), 0.f, false);
        Render behindQ = render(mod22.cache(), 180.f, false);
        throughQuake(aheadQ);
        throughQuake(behindQ);
        const auto highs = [&](const Render& x) {
            return 0.5f * (earBand(x.l, from, 6000.f, 10000.f) + earBand(x.r, from, 6000.f, 10000.f)) -
                   0.5f * (earBand(x.l, from, 1000.f, 4000.f) + earBand(x.r, from, 1000.f, 4000.f));
        };
        Con_Printf("snd_test fullband: 22 kHz voice, 6-10 kHz against 1-4 kHz: ahead %+.1f, behind %+.1f dB (through "
                   "Quake's lowpass: %+.1f, %+.1f)\n",
            highs(ahead), highs(behind), highs(aheadQ), highs(behindQ));
        res.check("fullband: a 22 kHz voice's highs reach the ears (6-10 kHz > -15 dB of 1-4)", highs(ahead) > -15.f);
        res.check("fullband: ahead brighter than behind (6-10 kHz, > 2 dB)", highs(ahead) - highs(behind) > 2.f);
    }

    // Slow motion (a quarter speed): read between the copy's samples, windowed-sinc (linearly, the 22 kHz sound's
    // highs left images at 8-14 kHz).
    {
        const Render slow = render(mod22.cache(), 30.f, true, 0.25f);
        const float img = za::max(density(slow.l, from, 3500.f, 20000.f) - density(slow.l, from, 250.f, 2500.f),
            density(slow.r, from, 3500.f, 20000.f) - density(slow.r, from, 250.f, 2500.f));
        Con_Printf("snd_test fullband: 22 kHz voice at a quarter speed, 3.5-20 kHz against 0.25-2.5 %.1f dB/Hz (worse "
                   "ear)\n",
            img);
        res.check("fullband: slowed, no images (< -50 dB)", img < -50.f);
    }

    // The cost: 32 moving voices (Doppler: read between samples), HRTF, a 90 Hz frame's 490 samples, held samples read
    // linearly against the copies read windowed-sinc.
    {
        const int frame = m.frameSize();
        const int blocks = (testRate / 90 + frame - 1) / frame;
        za::Vector<float> bl(static_cast<za::SizeT>(blocks * frame)), br(static_cast<za::SizeT>(blocks * frame));
        double ms[2]{};
        for(int full = 0; full < 2; full++)
        {
            Features f = baseFeatures();
            f.doppler = 1.f;
            f.fullBand = full != 0;
            for(int v = 0; v < Mixer::maxVoices; v++)
            {
                m.start(v, mod22.cache(), static_cast<double>(v * 1000));
                VoiceInput in;
                const float a = static_cast<float>(v) * 0.7f;
                in.pos = r * 2.f * (za::cos(a) * lis.fwd + za::sin(a) * lis.right);
                in.vel = glm::vec3{300.f, 0.f, 0.f};
                in.gain = 0.1f;
                m.set(v, in);
            }
            for(int k = 0; k < 10; k++)
            {
                m.render(blocks, lis, f, nullptr, bl.data(), br.data());
            }
            const double start = Sys_DoubleTime();
            constexpr int frames = 200;
            for(int k = 0; k < frames; k++)
            {
                m.render(blocks, lis, f, nullptr, bl.data(), br.data());
            }
            ms[full] = (Sys_DoubleTime() - start) * 1000.0 / frames;
        }
        for(int v = 0; v < Mixer::maxVoices; v++)
        {
            m.stop(v);
        }
        Con_Printf("snd_test fullband: 32 moving voices a 90 Hz frame: held samples (linear) %.3f ms, copies "
                   "(windowed sinc) %.3f ms\n",
            ms[0], ms[1]);
    }

    // The loop's seam.
    {
        const Loaded sine = loadAt(11025, 2500, 2, true, sine441); // 100 periods
        const za::Vector<float> x = samplesOf(sine.cache(), true);
        float worst = 0.f;
        for(int i = 0; i < static_cast<int>(x.size()); i++)
        {
            const float ideal = 8000.f * za::sin(2.f * 3.14159265f * static_cast<float>(i % 100) / 100.f);
            worst = za::max(worst, za::abs(x[i] - ideal));
        }
        Con_Printf("snd_test fullband: looping 441 Hz sine, the copy's largest error %.1f of 8000 (its seam included)\n",
            worst);
        res.check("fullband: a loop's copy goes round its seam (error < 1%)", worst < 80.f);
    }
}

// ----------------------------------------------------------------------------
// golden: fixed renders through the mix's whole path, kept to check that an optimisation leaves the sound as it was.
// renderVoices with 24 voices of made sounds loaded as S_LoadSound loads them (their band-limited copies), moving round
// the head (Doppler, the HRTF's directions, the near field), each with its occlusion EQ and air, the reverb, the
// anti-aliasing; at the normal speed and slowed to 0.25; and the band-limited copies themselves. Each is written raw
// (float32, left and right) to sound_tests/golden_<case>.f32 and, when golden_<case>_ref.f32 is there (a copy of an
// earlier build's), compared with it: bit-identical, else the SNR (PASS over 60 dB) and the largest difference.

void writeRaw(const char* name, const za::Vector<float>& x)
{
    za::String path = za::String{com_gamedir} + "/sound_tests/golden_" + name + ".f32";
    COM_CreatePath(path.data());
    FILE* f = fopen(path.cStr(), "wb");
    if(f)
    {
        fwrite(x.data(), sizeof(float), x.size(), f);
        fclose(f);
    }
}

void compareRaw(Result& res, const char* name, const za::Vector<float>& x)
{
    writeRaw(name, x);
    const za::String path = za::String{com_gamedir} + "/sound_tests/golden_" + name + "_ref.f32";
    FILE* f = fopen(path.cStr(), "rb");
    if(!f)
    {
        Con_Printf("snd_test golden %s: %d samples written (no _ref to compare)\n", name, static_cast<int>(x.size()));
        return;
    }
    za::Vector<float> ref(x.size(), 0.f);
    const za::SizeT got = fread(ref.data(), sizeof(float), ref.size(), f);
    fclose(f);
    double signal = 0.0;
    double noiseSum = 0.0;
    double worst = 0.0;
    int differ = 0;
    for(za::SizeT i = 0; i < x.size(); i++)
    {
        const double d = static_cast<double>(x[i]) - static_cast<double>(ref[i]);
        signal += static_cast<double>(ref[i]) * ref[i];
        noiseSum += d * d;
        worst = za::max(worst, za::abs(d));
        differ += x[i] != ref[i] ? 1 : 0;
    }
    const double snr = noiseSum > 0.0 ? 10.0 * za::log10(signal / noiseSum) : 999.0;
    Con_Printf("snd_test golden %s: %s, %d of %d samples differ, SNR %.1f dB, largest difference %.3g (of 32768)\n", name,
        differ == 0 && got == x.size() ? "bit-identical" : "differs", differ, static_cast<int>(x.size()), snr, worst);
    res.check((za::String{"golden "} + name + ": as the reference (SNR over 60 dB)").cStr(), got == x.size() && snr > 60.0);
}

// The scene: 24 voices round the head, moving, for 200 calls of two frames.
za::Vector<float> goldenMix(const za::Vector<Loaded>& sounds, float rate, int fullBand, const IPLReflectionEffectParams* reverb,
    IPLReflectionEffectType type, bool hrtf)
{
    za::Vector<float> out;
    Mixer m;
    const int frame = testFrame();
    if(!m.create(testRate, frame, ""))
    {
        return out;
    }
    m.setReverb(type, 1, 2.0f);
    const float upm = units::perMetre;
    Features f = baseFeatures();
    f.hrtf = hrtf;
    f.occlusion = 1.f;
    f.air = true;
    f.reverb = 0.5f;
    f.doppler = 1.f;
    f.nearfield = 1.f;
    f.rate = rate;
    f.fullBand = fullBand >= 1;
    const float savedFullBand = vr_snd_fullband.value;
    Cvar_SetValueQuick(&vr_snd_fullband, static_cast<float>(fullBand));
    const Listener lis = centred();
    AntiAlias aa;
    za::Vector<float> mixL(Mixer::maxSamples, 0.f), mixR(Mixer::maxSamples, 0.f);
    za::Vector<float> revL(Mixer::maxSamples, 0.f), revR(Mixer::maxSamples, 0.f);
    constexpr int voices = 24;
    const int count = static_cast<int>(sounds.size());
    for(int i = 0; i < voices; i++)
    {
        m.start(i, sounds[i % count].cache(), i * 37.0);
    }
    const int blocks = 2;
    const double dt = static_cast<double>(blocks * frame) / testRate;
    for(int k = 0; k < 200; k++)
    {
        for(int i = 0; i < voices; i++)
        {
            if(!m.active(i) || m.ended(i))
            {
                m.start(i, sounds[i % count].cache(), 0.0);
            }
            const float speed = 0.6f * static_cast<float>(1 + i % 3);
            const float a = static_cast<float>(i) * 2.399963f + static_cast<float>(k * dt) * speed;
            const float r = (0.3f + static_cast<float>(i % 6)) * upm;
            VoiceInput in;
            in.pos = glm::vec3{za::cos(a) * r, za::sin(a) * r, static_cast<float>(i % 3 - 1) * 20.f};
            in.vel = glm::vec3{-za::sin(a) * r * speed, za::cos(a) * r * speed, 0.f};
            in.gain = 0.15f;
            in.attached = i % 7 == 0;
            in.hasDirect = true;
            in.direct.occlusion = static_cast<float>(i % 4) / 3.f;
            in.direct.transmission[0] = 0.5f;
            in.direct.transmission[1] = 0.3f;
            in.direct.transmission[2] = 0.1f;
            in.direct.air[0] = 1.f;
            in.direct.air[1] = 0.9f - 0.02f * static_cast<float>(i % 5);
            in.direct.air[2] = 0.7f - 0.05f * static_cast<float>(i % 5);
            m.set(i, in);
        }
        renderVoices(m, blocks, lis, f, reverb, mixL, mixR, revL, revR, aa);
        for(int s = 0; s < blocks * frame; s++)
        {
            out.pushBack(mixL[s]);
            out.pushBack(mixR[s]);
        }
    }
    Cvar_SetValueQuick(&vr_snd_fullband, savedFullBand);
    return out;
}

float sine3k(int i, unsigned&)
{
    return 12000.f * za::sin(2.f * 3.14159265f * 3000.f * static_cast<float>(i) / 11025.f);
}

void testGolden(Result& res)
{
    za::Vector<Loaded> sounds;
    sounds.pushBack(loadAt(11025, 11025, 1, false, noise));     // id's: 8-bit, 11 kHz
    sounds.pushBack(loadAt(22050, 22050 * 2, 2, true, noise));  // a 22 kHz loop
    sounds.pushBack(loadAt(11025, 11025 * 2, 2, true, sine3k)); // a tone near the top of an 11 kHz sound
    sounds.pushBack(loadAt(32000, 16000, 2, false, noise));
    // The copies themselves.
    za::Vector<float> copies;
    for(const Loaded& s : sounds)
    {
        const short* full = fullBandData(s.cache());
        for(int i = 0; full && i < s.cache()->length; i++)
        {
            copies.pushBack(static_cast<float>(full[i]));
        }
    }
    compareRaw(res, "copies", copies);

    IPLReflectionEffectParams parametric{};
    parametric.type = IPL_REFLECTIONEFFECTTYPE_PARAMETRIC;
    parametric.reverbTimes[0] = 1.4f;
    parametric.reverbTimes[1] = 1.1f;
    parametric.reverbTimes[2] = 0.7f;
    parametric.eq[0] = parametric.eq[1] = parametric.eq[2] = 1.f;
    compareRaw(res, "mix", goldenMix(sounds, 1.f, 1, &parametric, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, true));
    compareRaw(res, "slow", goldenMix(sounds, 0.25f, 1, &parametric, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, true));
    compareRaw(res, "nofull", goldenMix(sounds, 1.f, 0, &parametric, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, true));
    compareRaw(res, "panning", goldenMix(sounds, 0.5f, 1, &parametric, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, false));

    // (Not the convolution reverb: its response is simulated with Steam Audio's random rays, a little different each
    // run. It goes through the same calls as the parametric one above.)
}

} // namespace qvr::audio::testkit
