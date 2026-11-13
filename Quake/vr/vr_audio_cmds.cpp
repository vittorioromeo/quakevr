// vr_audio_cmds.cpp -- the spatial mixer's commands (vr_audio.cpp): vr_snd_info, the captures, the test plays,
// vr_snd_dump, the burst and the vr_snd_bench run.

#include "vr_audio_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_testkit.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // (vr_snd_wav_check: a WAV laid against an unreadable page)
#endif

namespace qvr::audio
{

namespace detail
{

// ----------------------------------------------------------------------------
// Commands

void info_f()
{
    Con_Printf("Spatial audio (vr_snd_spatial %g): %s\n", vr_snd_spatial.value,
        running() ? "on" : wanted() ? "wanted, not running" : "off");
    Con_Printf("  Steam Audio: %s\n", steamaudio::status());
    if(!live)
    {
        return;
    }
    Live& L = *live;
    if(L.ok)
    {
        const Features f = featuresFromCvars();
        Con_Printf("  %d Hz, %d-sample frames; HRTF %s%s, voices %d of %d, mix %.3f ms a call\n", L.rate, L.frame,
            f.hrtf ? "on" : "off", L.mixer.customHrtf() ? " (SOFA)" : "", L.voicesUsed, static_cast<int>(vr_snd_voices.value),
            L.mixMs);
        Con_Printf("  scene: %d triangles (%.1f ms to build), %d brush models (of %d seen); direct %.2f ms, reverb %.2f ms\n",
            L.sim.sceneTriangles(), L.sim.sceneBuildMs(), L.sim.instanceCount(), L.brushSeen, L.sim.directMs(), L.sim.reflectionsMs());
        if(L.haveReverb && L.reverb.type == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION)
        {
            Con_Printf("  reverb: convolution, %d-sample response (%d channels)\n", L.reverb.irSize, L.reverb.numChannels);
        }
        else if(L.haveReverb)
        {
            Con_Printf("  reverb: RT60 %.2f %.2f %.2f s (low, mid, high)\n", L.reverb.reverbTimes[0], L.reverb.reverbTimes[1],
                L.reverb.reverbTimes[2]);
        }
    }
    Con_Printf("  mix: limiter %s (ceiling %.1f dB, %.0f ms release), deepest %.1f dB since the last info; %lld samples "
               "over full scale, %lld clipped\n",
        vr_snd_limiter.value != 0.f ? "on" : "off (Quake's hard clip)", za::clamp(vr_snd_limiter_ceiling.value, -12.f, 0.f),
        za::clamp(vr_snd_limiter_release.value, 0.02f, 1.f) * 1000.f,
        L.bus.minGain > 0.f ? 20.f * za::log10(L.bus.minGain) : -200.f, L.bus.over, L.bus.clipped);
    L.bus.minGain = 1.f;
    Con_Printf("  game-time render (vr_timescale_wav, vr_snd_capture_game): %s, %.3f ms a call; WAV: %s\n",
        L.shadow.running ? (L.shadow.mixer.valid() ? "on, with its own voices" : "on, Quake's mix only") : "off",
        L.shadow.ms, L.wav.file ? L.wav.path.cStr() : "none");
    const mleaf_t* leaf = cl.worldmodel ? Mod_PointInLeaf(&L.listener.pos.x, cl.worldmodel) : nullptr;
    Con_Printf("  listener: %s at %.0f %.0f %.0f (leaf contents %d), speed %.0f units/s\n", L.head ? "the head" : "the view",
        L.listener.pos.x, L.listener.pos.y, L.listener.pos.z, leaf ? leaf->contents : 0, glm::length(L.listener.vel));
}

void startCapture(bool game)
{
    const char* cmd = game ? "vr_snd_capture_game" : "vr_snd_capture";
    if(!live || !shm)
    {
        Con_Printf("%s: no sound (-nosound?)\n", cmd);
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf(game ? "%s <seconds> [name]: the mix in the game's time (as at normal speed in slow motion; no music) to "
                          "<game>/sound_tests/capture_game_<name>.wav, and its levels\n"
                        : "%s <seconds> [name]: the final mix to <game>/sound_tests/capture_<name>.wav, and its levels\n",
            cmd);
        return;
    }
    Capture& c = game ? live->gameCapture : live->capture;
    c.wanted = static_cast<int>(za::clamp(Q_atof(Cmd_Argv(1)), 0.05f, 60.f) * static_cast<float>(shm->speed));
    c.name = Cmd_Argc() > 2 ? Cmd_Argv(2) : "capture";
    c.left.clear();
    c.right.clear();
    c.left.reserve(c.wanted);
    c.right.reserve(c.wanted);
    c.busSeen = 0;
    c.busPeak = 0.f;
    c.busOver = 0;
    c.outPeak = 0.f;
    c.outClipped = 0;
    c.minGain = 1.f;
    c.running = true;
    if(game)
    {
        live->gameBus.look = 0; // (fresh: nothing of an old mix in its delay)
    }
}

void capture_f()
{
    startCapture(false);
}

void captureGame_f()
{
    startCapture(true);
}

// Looping sounds in a ring round the listener (static sounds, till the map changes): a load for vr_profile.
void benchSpawn_f()
{
    if(!live || !shm || cls.state != ca_connected)
    {
        Con_Printf("vr_snd_bench_spawn: needs a map and sound\n");
        return;
    }
    const int count = Cmd_Argc() > 1 ? za::clamp(Q_atoi(Cmd_Argv(1)), 1, 128) : 32;
    const float radius = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 200.f;
    const char* names[] = {"ambience/fire1.wav", "ambience/hum1.wav", "ambience/drip1.wav", "ambience/comp1.wav"};
    for(int i = 0; i < count; i++)
    {
        sfx_t* sfx = S_PrecacheSound(names[i % 4]);
        const float a = static_cast<float>(i) * 2.399963f; // (the golden angle: spread round)
        const float r = radius * (0.5f + 0.5f * static_cast<float>(i % 7) / 6.f);
        vec3_t at{live->listener.pos.x + za::cos(a) * r, live->listener.pos.y + za::sin(a) * r,
            live->listener.pos.z + static_cast<float>(i % 3 - 1) * 24.f};
        S_StaticSound(sfx, at, 255.f, 1.f);
    }
    Con_Printf("vr_snd_bench_spawn: %d looping sounds within %.0f units\n", count, radius);
}

void sceneObj_f()
{
    if(!live || !live->sim.hasScene())
    {
        Con_Printf("vr_snd_scene_obj: no scene (spatial audio off, or no map)\n");
        return;
    }
    za::String base = za::String{com_gamedir} + "/sound_tests/scene";
    COM_CreatePath(base.data());
    live->sim.saveObj(base.cStr());
    Con_Printf("vr_snd_scene_obj: %s.obj (%d triangles)\n", base.cStr(), live->sim.sceneTriangles());
}

} // namespace detail

void playAt(sfx_t* sfx, const glm::vec3& at, float volume, float attenuation, int entity)
{
    vec3_t org{at.x, at.y, at.z};
    S_StartSound(entity, 0, sfx, org, volume, attenuation);
}

void playLocal(sfx_t* sfx, float volume)
{
    S_StartSound(cl.viewentity, -1, sfx, vec3_origin, volume, 1.f);
}

namespace detail
{

// A sound at a place (the world's, channel auto): vr_snd_play <sample> <x> <y> <z> [volume] [attenuation].
void play_f()
{
    if(Cmd_Argc() < 5 || cls.state != ca_connected)
    {
        Con_Printf("vr_snd_play <sample> <x> <y> <z> [volume] [attenuation]: a sound from that place (in a map)\n");
        return;
    }
    sfx_t* sfx = S_PrecacheSound(Cmd_Argv(1));
    const glm::vec3 at{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    const float volume = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 1.f;
    const float attenuation = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 1.f;
    playAt(sfx, at, volume, attenuation);
}

// A loaded sound as the mixers read it (vr_snd_dump <sample>): Quake's held samples and the band-limited copy
// (vr_snd_fullband), each a WAV at the mix's rate in sound_tests/ (dump_<sample>_held.wav, _full.wav), at half volume.
void dump_f()
{
    if(Cmd_Argc() < 2 || !shm)
    {
        Con_Printf("vr_snd_dump <sample>: the sound's held samples and band-limited copy to sound_tests/\n");
        return;
    }
    sfx_t* sfx = S_PrecacheSound(Cmd_Argv(1));
    const sfxcache_t* sc = sfx ? S_LoadSound(sfx) : nullptr;
    if(!sc)
    {
        Con_Printf("vr_snd_dump: %s isn't loaded\n", Cmd_Argv(1));
        return;
    }
    const short* full = fullBandData(sc);
    za::Vector<float> held(static_cast<za::SizeT>(sc->length)), copy(static_cast<za::SizeT>(sc->length));
    for(int i = 0; i < sc->length; i++)
    {
        // (both at the copy's scale, 1/S_FULLBAND_SCALE: its overshoot of a clipped sound not clipped in the file)
        held[i] = (sc->width == 2 ? static_cast<float>(reinterpret_cast<const short*>(sc->data)[i])
                                  : static_cast<float>(static_cast<signed char>(sc->data[i])) * 256.f) /
                  S_FULLBAND_SCALE;
        copy[i] = full ? static_cast<float>(full[i]) : held[i];
    }
    za::String name = Cmd_Argv(1);
    for(char& c : name)
    {
        c = c == '/' || c == '\\' || c == '.' ? '_' : c;
    }
    const za::String base = za::String{com_gamedir} + "/sound_tests/dump_" + name;
    COM_CreatePath((base + "_held.wav").data());
    writeWav16((base + "_held.wav").cStr(), held, held, shm->speed);
    writeWav16((base + "_full.wav").cStr(), copy, copy, shm->speed);
    Con_Printf("vr_snd_dump %s: %d samples at %d Hz, %d-bit, %s (%s_held.wav, _full.wav)\n", Cmd_Argv(1), sc->length,
        shm->speed, sc->width * 8, full ? "band-limited copy" : "no copy (loaded at the mix's rate)", base.cStr());
}

// A sound from a direction (vr_snd_play_dir <sample> <azimuth> [elevation] [metres] [volume] [attenuation]): degrees
// clockwise from where the listener faces (90 the right), up from level, at that distance (2 m) from the listener. For
// the left/right balance against the angle (ROUND21.md, "HRTF balance").
void playDir_f()
{
    if(Cmd_Argc() < 3 || cls.state != ca_connected || !live)
    {
        Con_Printf("vr_snd_play_dir <sample> <azimuth> [elevation] [metres] [volume] [attenuation]: a sound from that "
                   "direction (degrees, 90 the right) and distance (2 m)\n");
        return;
    }
    sfx_t* sfx = S_PrecacheSound(Cmd_Argv(1));
    const float a = Q_atof(Cmd_Argv(2)) * 3.14159265f / 180.f;
    const float e = (Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 0.f) * 3.14159265f / 180.f;
    const float distance = (Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 2.f) * units::metresToUnits();
    const float volume = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 1.f;
    const float attenuation = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 1.f;
    const Listener& l = live->listener;
    const glm::vec3 p =
        l.pos + distance * (za::cos(e) * (za::cos(a) * l.fwd + za::sin(a) * l.right) + za::sin(e) * l.up);
    playAt(sfx, p, volume, attenuation, -1);
}

// Copies of a sound at once (vr_snd_burst <sample> [count] [distance] [spread] [volume]): ahead of the listener,
// spread round a circle, all started this frame as explosive boxes blowing up together are (Quake's S_StartSound
// offsets identical sounds started together a little). For the effects' bus's limiter (vr_snd_limiter).
void burst_f()
{
    if(cls.state != ca_connected || !live)
    {
        Con_Printf("vr_snd_burst <sample> [count] [distance] [spread] [volume]: copies of a sound at once, ahead of you\n");
        return;
    }
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "weapons/r_exp3.wav";
    const int count = Cmd_Argc() > 2 ? za::clamp(Q_atoi(Cmd_Argv(2)), 1, 32) : 5;
    const float distance = Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 96.f;
    const float spread = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 32.f;
    const float volume = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 1.f;
    sfx_t* sfx = S_PrecacheSound(name);
    const Listener& l = live->listener;
    for(int i = 0; i < count; i++)
    {
        const float a = static_cast<float>(i) * 2.399963f;
        const glm::vec3 p = l.pos + l.fwd * distance + l.right * (za::cos(a) * spread) + l.up * (za::sin(a) * spread * 0.5f);
        playAt(sfx, p, volume, 1.f, -1);
    }
}

void benchFinish()
{
    Live& L = *live;
    BenchRun& b = L.bench;
    b.running = false;
    bench::frame();
    bench::stop();
    const int rate = shm ? shm->speed : 44100;
    const long long samples = bench::samples();
    Con_Printf("vr_snd_bench %s: %d frames, %.1f s of sound, %.1f voices (the frames that painted, mean), %d workers\n", b.label.cStr(),
        bench::frames(), static_cast<double>(samples) / rate, bench::meanVoices(), jobs::workers());
    Con_Printf("  %-12s %7s %7s %7s %7s  %8s\n", "stage (ms)", "median", "p95", "p99", "max", "ms/s");
    za::String csvPath = za::String{com_gamedir} + "/sound_tests/bench.csv";
    COM_CreatePath(csvPath.data());
    FILE* csv = fopen(csvPath.cStr(), "ab");
    for(int st = 0; st < bench::Count; st++)
    {
        const bench::Stats x = bench::stats(st, rate);
        Con_Printf("  %-12s %7.3f %7.3f %7.3f %7.3f  %8.3f\n", bench::stageName(st), x.median, x.p95, x.p99, x.max,
            x.perSecond);
        if(csv)
        {
            fprintf(csv, "%s,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n", b.label.cStr(), bench::stageName(st), x.n, x.median, x.p95,
                x.p99, x.max, x.perSecond);
        }
    }
    const auto runLine = [&](const char* name, za::Vector<float>& ms) {
        if(ms.empty())
        {
            Con_Printf("  sim %s: no runs\n", name);
            return;
        }
        za::quickSort(ms.begin(), ms.end());
        const int n = static_cast<int>(ms.size());
        const auto at = [&](double q) { return vrstats::quantileSorted(ms.data(), ms.size(), q, vrstats::Rank::Nearest); };
        Con_Printf("  sim %s (a worker): %d runs, median %.3f ms, p95 %.3f, max %.3f\n", name, n, at(0.5), at(0.95),
            ms[n - 1]);
        if(csv)
        {
            fprintf(csv, "%s,sim_%s,%d,%.4f,%.4f,%.4f,%.4f,0\n", b.label.cStr(), name, n, at(0.5), at(0.95), at(0.99),
                ms[n - 1]);
        }
    };
    runLine("direct", b.directMs);
    runLine("reflections", b.reflectionsMs);
    int loaded = 0;
    int held = 0;
    int full = 0;
    S_SfxMemory(&loaded, &held, &full);
    Con_Printf("  sounds: %d in the cache, %d KiB as Quake mixes them, %d KiB band-limited copies; band-limiting: %d "
               "sounds in the bench (%.2f ms), %d since the start (%.1f ms, worst %.2f ms, %lld KiB)\n",
        loaded, held / 1024, full / 1024, bandLimitStats.sounds - b.bandLimited, bandLimitStats.ms - b.bandLimitMs,
        bandLimitStats.sounds, bandLimitStats.ms, bandLimitStats.worstMs, bandLimitStats.bytes / 1024);
    if(csv)
    {
        fprintf(csv, "%s,memory_kib,%d,%d,%d,0,0,0\n", b.label.cStr(), loaded, held / 1024, full / 1024);
        fprintf(csv, "%s,bandlimit_since_start,%d,%.4f,%.4f,0,0,0\n", b.label.cStr(), bandLimitStats.sounds,
            bandLimitStats.ms, bandLimitStats.worstMs);
        fclose(csv);
    }
}

// The bench's sounds (id's: never shipped, played from the game's own data).
constexpr const char* benchSounds[] = {"weapons/r_exp3.wav", "weapons/rocket1i.wav", "weapons/guncock.wav",
    "weapons/grenade.wav", "weapons/spike2.wav", "weapons/sgun1.wav", "weapons/lhit.wav", "soldier/sight1.wav",
    "soldier/pain1.wav", "soldier/death1.wav", "dog/dattack1.wav", "ogre/ogwake.wav", "ogre/ogsawatk.wav",
    "ogre/ogdrag.wav", "knight/sword1.wav", "knight/khurt.wav", "zombie/z_idle.wav", "enforcer/enfire.wav",
    "wizard/wattack.wav", "demon/dhit2.wav", "shambler/sattck1.wav", "player/pain1.wav"};
constexpr int benchSoundCount = static_cast<int>(sizeof benchSounds / sizeof benchSounds[0]);

void benchFrame()
{
    Live& L = *live;
    BenchRun& b = L.bench;
    if(!b.running)
    {
        return;
    }
    bench::frame();
    const Simulation::Runs r = L.sim.runs();
    if(r.direct != b.runs.direct)
    {
        b.directMs.pushBack(static_cast<float>(r.directMs));
    }
    if(r.reflections != b.runs.reflections)
    {
        b.reflectionsMs.pushBack(static_cast<float>(r.reflectionsMs));
    }
    b.runs = r;
    if(realtime >= b.end)
    {
        benchFinish();
        return;
    }
    // The combat: a sound every 1 / shotsPerSecond, at random, 100-900 units away.
    const auto random = [&]() {
        return vrand::lcg01(b.rng);
    };
    while(b.shotsPerSecond > 0.f && realtime >= b.nextShot && cls.state == ca_connected)
    {
        b.nextShot += 1.0 / b.shotsPerSecond;
        sfx_t* sfx = S_PrecacheSound(benchSounds[za::min(benchSoundCount - 1, static_cast<int>(random() * benchSoundCount))]);
        const float a = random() * 6.2831853f;
        const float d = 100.f + random() * 800.f;
        playAt(sfx, glm::vec3{L.lastPos.x + za::cos(a) * d, L.lastPos.y + za::sin(a) * d, L.lastPos.z + (random() - 0.5f) * 128.f},
            1.f, 1.f);
    }
}

void bench_f()
{
    if(!live || !shm)
    {
        Con_Printf("vr_snd_bench: needs sound\n");
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_snd_bench <seconds> [label] [sounds a second] [orbit units/s]: each sound frame's time by stage "
                   "(median, p95, p99), with a combat scene\n");
        return;
    }
    BenchRun& b = live->bench;
    const double seconds = za::clamp(static_cast<double>(Q_atof(Cmd_Argv(1))), 0.5, 600.0);
    b.running = true;
    b.end = realtime + seconds;
    b.label = Cmd_Argc() > 2 ? Cmd_Argv(2) : "bench";
    b.shotsPerSecond = Cmd_Argc() > 3 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(3))), 0.f, 200.f) : 0.f;
    b.orbitSpeed = Cmd_Argc() > 4 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(4))), 0.f, 4000.f) : 0.f;
    b.orbitStart = realtime;
    b.nextShot = realtime;
    b.rng = 12345u;
    b.runs = live->sim.runs();
    b.directMs.clear();
    b.reflectionsMs.clear();
    b.bandLimited = bandLimitStats.sounds;
    b.bandLimitMs = bandLimitStats.ms;
    bench::start(static_cast<int>(seconds * 1000.0));
    Con_Printf("vr_snd_bench %s: %.1f s\n", b.label.cStr(), seconds);
}

// ----------------------------------------------------------------------------
// The sound code's fixes after vkQuake, checked (ROUND25.md, "vkQuake fixes")

namespace
{

// A WAV made byte by byte (little-endian), 8-bit mono 11025 Hz.
struct WavBytes
{
    unsigned char b[512]{};
    int n{0};

    void id(const char* s)
    {
        for(int i = 0; i < 4; i++)
        {
            b[n++] = static_cast<unsigned char>(s[i]);
        }
    }
    void le32(int v)
    {
        for(int i = 0; i < 4; i++)
        {
            b[n++] = static_cast<unsigned char>((v >> (8 * i)) & 0xff);
        }
    }
    void le16(int v)
    {
        b[n++] = static_cast<unsigned char>(v & 0xff);
        b[n++] = static_cast<unsigned char>((v >> 8) & 0xff);
    }
    void riff()
    {
        id("RIFF");
        le32(0); // (finish)
        id("WAVE");
    }
    void fmt()
    {
        id("fmt ");
        le32(16);
        le16(1);     // PCM
        le16(1);     // mono
        le32(11025); // rate
        le32(11025); // bytes a second
        le16(1);     // block align
        le16(8);     // bits
    }
    void data(int samples)
    {
        id("data");
        le32(samples);
        for(int i = 0; i < samples; i++)
        {
            b[n++] = 128;
        }
        if(samples & 1)
        {
            b[n++] = 0;
        }
    }
    void cue(int loopStart)
    {
        id("cue ");
        le32(28);
        le32(1); // points
        le32(1); // id
        le32(0); // position
        id("data");
        le32(0); // chunk start
        le32(0); // block start
        le32(loopStart);
    }
    // Cool Edit's loop length: a LIST adtl holding a ltxt "mark".
    void listMark(int loopLength)
    {
        id("LIST");
        le32(24);
        id("adtl");
        id("ltxt");
        le32(12);
        le32(1);
        le32(loopLength);
        id("mark");
    }
    // A chunk of `payload` bytes (its header's length that too), the file ending with it.
    void shortChunk(const char* name, int payload)
    {
        id(name);
        le32(payload);
        for(int i = 0; i < payload; i++)
        {
            b[n++] = name[i % 4];
        }
    }
    void finish()
    {
        const int size = n - 8;
        for(int i = 0; i < 4; i++)
        {
            b[4 + i] = static_cast<unsigned char>((size >> (8 * i)) & 0xff);
        }
    }
};

// GetWavinfo on `wav` laid so it ends where an unreadable page starts: 0 parsed, 1 a read past the file faulted.
int wavinfoGuarded(unsigned char* pageEnd, const WavBytes& wav, wavinfo_t* info)
{
    unsigned char* at = pageEnd - wav.n;
    memcpy(at, wav.b, static_cast<size_t>(wav.n));
#ifdef _WIN32
    __try
    {
        *info = GetWavinfo("vr_snd_wav_check", at, wav.n);
    }
    __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return 1;
    }
#else
    *info = GetWavinfo("vr_snd_wav_check", at, wav.n);
#endif
    return 0;
}

// vr_snd_wrap_test's state between its steps: the static sounds before the jump.
struct WrapTest
{
    int statics{0};
    za::Array<const sfx_t*, MAX_CHANNELS> sfx{};
    int wraps{0};
};
WrapTest wrapTest;

} // namespace

// vr_snd_wav_check: snd_mem.c GetWavinfo on WAVs whose last chunk is too short for what is read of it (fmt, cue, LIST:
// vkQuake d235ab8c's crash), a good Cool Edit loop and a loop longer than the data (was Sys_Error), each against an
// unreadable page: a read past the file faults, caught. Ends on "vr_snd_wav_check: N passed, M failed".
void wavCheck_f()
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const SIZE_T page = si.dwPageSize;
    unsigned char* mem = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD old = 0;
    if(!mem || !VirtualProtect(mem + page, page, PAGE_NOACCESS, &old))
    {
        Con_Printf("vr_snd_wav_check: FAIL no guard page\n");
        testkit::verdict("vr_snd_wav_check", 0, 1);
        return;
    }
    unsigned char* pageEnd = mem + page;
#else
    za::Array<unsigned char, 1024> mem{};
    unsigned char* pageEnd = mem.data() + 1024;
#endif
    testkit::Tally tally;
    const auto check = [&](const char* what, const WavBytes& wav, int samples, int loopStart, int width) {
        wavinfo_t info{};
        const int faulted = wavinfoGuarded(pageEnd, wav, &info);
        const bool ok = !faulted && info.samples == samples && info.loopstart == loopStart && info.width == width;
        tally.check(ok);
        Con_Printf("vr_snd_wav_check: %s %s (%s; samples %d, loop %d, width %d)\n", ok ? "PASS" : "FAIL", what,
            faulted ? "read past the file" : "in bounds", info.samples, info.loopstart, info.width);
    };

    WavBytes good;
    good.riff();
    good.fmt();
    good.data(100);
    good.cue(10);
    good.listMark(50);
    good.finish();
    check("Cool Edit loop", good, 60, 10, 1);

    WavBytes shortList;
    shortList.riff();
    shortList.fmt();
    shortList.data(100);
    shortList.cue(10);
    shortList.shortChunk("LIST", 4);
    shortList.finish();
    check("short LIST chunk last", shortList, 100, 10, 1);

    WavBytes shortCue;
    shortCue.riff();
    shortCue.fmt();
    shortCue.data(100);
    shortCue.shortChunk("cue ", 4);
    shortCue.finish();
    check("short cue chunk last", shortCue, 100, -1, 1);

    WavBytes shortFmt;
    shortFmt.riff();
    shortFmt.data(100);
    shortFmt.shortChunk("fmt ", 4);
    shortFmt.finish();
    check("short fmt chunk last (not loaded)", shortFmt, 0, 0, 0);

    WavBytes longLoop;
    longLoop.riff();
    longLoop.fmt();
    longLoop.data(100);
    longLoop.cue(10);
    longLoop.listMark(500);
    longLoop.finish();
    check("loop past the data (played to its end)", longLoop, 100, 10, 1);

#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#endif
    tally.verdict("vr_snd_wav_check");
}

// vr_snd_wrap_test [1|2]: the sample counter's wrap (snd_dma.c GetSoundtime, after ~6h45m: every sound was stopped, the
// static ones for good; vkQuake 03fa1ce8). 1 (on a map with static sounds): the counter jumped a buffer short of the
// wrap, the statics noted; the script waits for "sample counter wrapped" (vr_waitprint); 2: the same statics still play,
// the counter back near 0, the mix not stalled. Ends on "vr_snd_wrap_test: N passed, M failed".
void wrapTest_f()
{
    const int step = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 1;
    if(!shm || !shm->buffer)
    {
        Con_Printf("vr_snd_wrap_test: FAIL no sound (run with sound)\n");
        testkit::verdict("vr_snd_wrap_test", 0, 1);
        return;
    }
    S_MixJoin();
    const int fullSamples = shm->samples / shm->channels;
    const int firstStatic = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
    if(step == 1)
    {
        wrapTest = WrapTest{};
        for(int i = firstStatic; i < total_channels; i++)
        {
            wrapTest.sfx[i] = snd_channels[i].sfx;
            wrapTest.statics += snd_channels[i].sfx ? 1 : 0;
        }
        wrapTest.wraps = S_SoundTimeWraps();
        const int buffers = (0x40000000 - paintedtime) / fullSamples + 1;
        S_SoundTimeJump(buffers);
        Con_Printf("vr_snd_wrap_test: %d static sounds; the counter jumped %d buffers to %d\n", wrapTest.statics, buffers,
            paintedtime);
        return;
    }
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        tally.check(ok);
        Con_Printf("vr_snd_wrap_test: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    int still = 0;
    int bad = 0;
    for(int i = firstStatic; i < total_channels; i++)
    {
        const channel_t& ch = snd_channels[i];
        if(!wrapTest.sfx[i])
        {
            continue;
        }
        // (still its sound, its end moved with the counter: one out of earshot is not painted, its end not renewed, behind
        // the counter until it is heard again and loops)
        if(ch.sfx == wrapTest.sfx[i] && ch.end - paintedtime < 0x10000000 && paintedtime - ch.end < 0x10000000)
        {
            still++;
        }
        else
        {
            bad++;
        }
    }
    Con_Printf("vr_snd_wrap_test: statics %d before, %d playing on (%d not); counter %d, soundtime %d, wraps %d\n",
        wrapTest.statics, still, bad, paintedtime, soundtime, S_SoundTimeWraps() - wrapTest.wraps);
    check(wrapTest.statics > 0, "the map has static sounds");
    check(S_SoundTimeWraps() > wrapTest.wraps, "the counter wrapped");
    check(paintedtime > 0 && paintedtime < 0x10000000, "the counter is back near 0");
    check(paintedtime >= soundtime && paintedtime - soundtime <= fullSamples, "the mix goes on (painted ahead of the device)");
    check(still == wrapTest.statics && bad == 0, "every static sound plays on");
    tally.verdict("vr_snd_wrap_test");
}

} // namespace detail
} // namespace qvr::audio
