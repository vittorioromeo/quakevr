// vr_audiotest_own.cpp -- vr_snd_test own: the player's own gun through the spatial mix, against its sound (see
// vr_audiotest.cpp; ROUND26.md, "muffled").
//
// The shotgun's report (weapons/guncock.wav, read from the game's files and loaded as S_LoadSound loads it: the
// band-limited copy the voices read) rendered as a voice from where a held gun's muzzle is (aimed ahead, at the hip,
// held down at the side), with the settings' own features stage by stage: Quake's panning, the HRTF, its near field,
// then the room's reverb (a small stone room, a hall). Each stage's level and brightness (the 2-5 kHz band against the
// 150-1000 Hz one, both ears) against the sound itself, as flat Quake plays a player's own sound (the same in both
// ears). A player's report reported "very muffled" in VR (2026-11-27).

#include "vr_audiotest_internal.hpp"
#include "vr_cvars.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Abs.hpp"
#include "vr_api.h"

#include <stdlib.h>

namespace qvr::audio::testkit
{

namespace
{

struct Tone
{
    float level{0.f}; // dB, both ears' power
    float low{0.f};   // dB, 150-1000 Hz
    float high{0.f};  // dB, 2-5 kHz
    float attack{0.f}; // dB, the first 25 ms against 25-400 ms (the crack against what rings after it)
    [[nodiscard]] float brightness() const
    {
        return high - low;
    }
};

float bothEars(float l, float r)
{
    return static_cast<float>(10.0 * za::log10((za::pow(10.0, l / 10.0) + za::pow(10.0, r / 10.0)) * 0.5));
}

Tone toneOf(const za::Vector<float>& l, const za::Vector<float>& r)
{
    Tone t;
    double e = 0.0;
    double first = 0.0;
    double after = 0.0;
    // (From its onset, the first sample at a twentieth of the peak: the HRTF's frame of latency left out.)
    float peak = 0.f;
    for(za::SizeT i = 0; i < l.size(); i++)
    {
        peak = za::max(peak, za::max(za::abs(l[i]), za::abs(r[i])));
    }
    za::SizeT onset = 0;
    while(onset < l.size() && za::abs(l[onset]) < peak * 0.05f && za::abs(r[onset]) < peak * 0.05f)
    {
        onset++;
    }
    const za::SizeT a = onset + testRate / 40;
    const za::SizeT b = onset + testRate * 2 / 5;
    for(za::SizeT i = 0; i < l.size(); i++)
    {
        const double s = static_cast<double>(l[i]) * l[i] + static_cast<double>(r[i]) * r[i];
        e += s;
        if(i < onset)
        {
            continue;
        }
        if(i < a)
        {
            first += s;
        }
        else if(i < b)
        {
            after += s;
        }
    }
    t.attack = static_cast<float>(10.0 * za::log10(za::max(first, 1e-9) / za::max(after, 1e-9)));
    t.level = db(e / za::max(1.0, 2.0 * static_cast<double>(l.size())));
    t.low = bothEars(earBand(l, 0, 150.f, 1000.f), earBand(r, 0, 150.f, 1000.f));
    t.high = bothEars(earBand(l, 0, 2000.f, 5000.f), earBand(r, 0, 2000.f, 5000.f));
    return t;
}

// sound/<name> read from the game's files and loaded as S_LoadSound would (mono; the first channel of a stereo one).
bool loadGameSound(const char* name, Loaded& out)
{
    char path[MAX_QPATH];
    q_snprintf(path, sizeof path, "sound/%s", name);
    byte* data = COM_LoadMallocFile(path, nullptr);
    if(!data)
    {
        return false;
    }
    const int size = static_cast<int>(com_filesize);
    const wavinfo_t info = GetWavinfo(name, data, size);
    const bool ok = info.channels == 1 && (info.width == 1 || info.width == 2) && info.samples > 0 &&
                    info.dataofs + info.samples * info.width <= size;
    if(ok)
    {
        out = loadRaw(data + info.dataofs, info.rate, info.samples, info.width, info.loopstart);
    }
    free(data);
    return ok;
}

} // namespace

// vr_snd_test own (in the game's files: id1's shotgun): see the top of this file.
void testOwn(Result& res)
{
    const char* name = "weapons/guncock.wav";
    Loaded snd;
    if(!loadGameSound(name, snd))
    {
        Con_Printf("snd_test own: %s not found (no id1?)\n", name);
        res.check("own: the shotgun's sound loads", false);
        return;
    }
    const sfxcache_t* sc = snd.cache();
    const float upm = units::perMetre;
    const Listener lis = centred();
    const float seconds = static_cast<float>(sc->length) / testRate + 1.f;
    const int total = static_cast<int>(seconds * testRate);

    // The sound itself, as flat Quake plays the player's own: the same in both ears (the band-limited copy).
    za::Vector<float> src(static_cast<za::SizeT>(total), 0.f);
    const short* full = reinterpret_cast<const short*>(sc->data + sc->fullband);
    for(int i = 0; i < sc->length && i < total; i++)
    {
        src[i] = static_cast<float>(full[i] * S_FULLBAND_SCALE);
    }
    const Tone ref = toneOf(src, src);
    Con_Printf("snd_test own: %s (%.2f s) itself: level %.1f dB, brightness %+.1f dB, attack %+.1f dB; the stages "
               "against it\n",
        name, static_cast<float>(sc->length) / testRate, ref.level, ref.brightness(), ref.attack);

    // Where a held gun's muzzle is from the eyes (metres: ahead, to the right, up).
    struct Pose
    {
        const char* name;
        glm::vec3 at;
    };
    const Pose poses[3] = {{"aimed", {0.75f, 0.2f, -0.25f}}, {"hip", {0.5f, 0.25f, -0.5f}}, {"down", {0.1f, 0.3f, -0.8f}}};
    const auto place = [&](const glm::vec3& m) { return (lis.fwd * m.x + lis.right * m.y + lis.up * m.z) * upm; };

    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("own (no Steam Audio)", false);
        return;
    }
    const Features live = featuresFromCvars();
    const auto render = [&](const Features& f, const IPLReflectionEffectParams* reverb, const glm::vec3& at, Mixer& mixer) {
        Render out = renderVoice(mixer, sc, lis, f, reverb, seconds, [&](double, VoiceInput& in) {
            in.pos = at;
            in.attached = true;
        });
        out.l.resize(static_cast<za::SizeT>(total), 0.f);
        out.r.resize(static_cast<za::SizeT>(total), 0.f);
        return out;
    };
    const auto report = [&](const char* stage, const char* pose, const Render& out) {
        const Tone t = toneOf(out.l, out.r);
        Con_Printf("snd_test own: %-20s %-5s level %+5.1f dB, low %+5.1f, high %+5.1f, brightness %+5.1f, attack %+5.1f\n",
            stage, pose, t.level - ref.level, t.low - ref.low, t.high - ref.high, t.brightness() - ref.brightness(),
            t.attack - ref.attack);
        return t;
    };

    float worstDry = 0.f;
    for(const Pose& p : poses)
    {
        const glm::vec3 at = place(p.at);
        Features f = live;
        f.reverb = 0.f;
        f.hrtf = false;
        f.nearfield = 0.f;
        report("panned", p.name, render(f, nullptr, at, m));
        f.hrtf = live.hrtf;
        report("hrtf", p.name, render(f, nullptr, at, m));
        f.nearfield = live.nearfield;
        const Render dry = render(f, nullptr, at, m);
        const Tone t = report("hrtf + near field", p.name, dry);
        worstDry = za::min(worstDry, t.brightness() - ref.brightness());
        if(&p == &poses[0])
        {
            writeWav("own_shotgun_dry", dry);
        }
    }

    // The room's reverb (the settings' wet mix), aimed: a small stone room and a hall.
    struct Room
    {
        const char* name;
        glm::vec3 size; // metres
    };
    const Room rooms[2] = {{"small room", {4.f, 3.f, 3.f}}, {"hall", {20.f, 14.f, 8.f}}};
    float worstWet = 0.f;
    float worstAttack = 0.f;
    for(const Room& room : rooms)
    {
        Mixer rm;
        Simulation sim;
        const int frame = testFrame();
        if(!rm.create(testRate, frame, "") ||
            !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, 1, 2.f, 2048))
        {
            res.check("own reverb (no Steam Audio)", false);
            return;
        }
        rm.setReverb(IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, 1, 2.f);
        Mesh mesh;
        const glm::vec3 half = room.size * 0.5f * upm;
        mesh.addBox(-half, half, true, SurfaceMaterial::Stone, upm);
        sim.buildSceneNow(ZA_MOVE(mesh));
        SimSettings ss;
        ss.reverb = true;
        ss.rays = 2048;
        ss.bounces = 16;
        ss.duration = 2.f;
        ss.order = 1;
        sim.runReflectionsNow(coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm), ss);
        IPLReflectionEffectParams params{};
        if(!sim.reflections(params))
        {
            res.check("own reverb (no simulation)", false);
            return;
        }
        Features any = live;
        any.reverbOwn = 1.f;
        char stage[64];
        q_snprintf(stage, sizeof stage, "%s, as any", room.name);
        const Render before = render(any, &params, place(poses[0].at), rm);
        report(stage, poses[0].name, before);
        q_snprintf(stage, sizeof stage, "%s, own %.2f", room.name, live.reverbOwn);
        const Render wet = render(live, &params, place(poses[0].at), rm);
        const Tone t = report(stage, poses[0].name, wet);
        worstWet = za::min(worstWet, t.brightness() - ref.brightness());
        worstAttack = za::min(worstAttack, t.attack - ref.attack);
        if(&room == &rooms[0])
        {
            writeWav("own_shotgun_room_before", before);
            writeWav("own_shotgun_room", wet);
        }
    }
    Con_Printf("snd_test own: against the sound, at worst: brightness dry %+.1f dB, with the reverb %+.1f dB; attack with "
               "the reverb %+.1f dB\n",
        worstDry, worstWet, worstAttack);
    res.check("own: the player's shotgun from his hand as bright as its sound, dry (2-5 kHz against 150-1000 Hz within 3 dB)",
        worstDry > -3.f);
    res.check("own: and with the room's reverb (within 3 dB)", worstWet > -3.f);
    res.check("own: its crack over the room's ring (the first 25 ms against 25-400 ms within 3 dB of the sound's)",
        worstAttack > -3.f);
}

} // namespace qvr::audio::testkit
