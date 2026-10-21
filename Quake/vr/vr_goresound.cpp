// vr_goresound.cpp -- see vr_goresound.hpp.

#include "vr_goresound.hpp"
#include "vr_bullettime.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gore.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include <string.h>
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Random/FastNonCryptoRng.hpp"
#include "vr_zancle.hpp"

#include <stdlib.h>

using namespace qvr;

namespace qvr::goresound
{
namespace
{

constexpr int kindCount = static_cast<int>(Kind::Count);
constexpr int maxVariants = 3;
constexpr int maxCap = 4;

// Each kind: its name, its files (quakevr/sound/vr/gore, cut from CC0 recordings by make_gore_sounds.py; new variants
// go here, up to maxVariants), its loudness, how many may play at once (the nearest), the least real seconds between
// two (a much nearer one still plays: nearerShare), its attenuation, how far it is heard at all (0: always), and its
// length when its file can't be loaded (no sound device: the caps counted as if it played).
struct KindInfo
{
    const char* name;
    const char* files[maxVariants];
    int count;
    float gain;
    int cap;
    float gap;
    float attenuation;
    float maxDistance;
    float seconds;
};

constexpr KindInfo kinds[kindCount] = {
    {"spurt", {"vr/gore/spurt1.wav", "vr/gore/spurt2.wav", "vr/gore/spurt3.wav"}, 3, 0.7f, 3, 0.08f, 1.f, 1200.f, 0.36f},
    {"pour", {"vr/gore/pour1.wav", "vr/gore/pour2.wav", nullptr}, 2, 0.35f, 3, 0.f, 1.5f, 800.f, 0.6f},
    {"choke", {"vr/gore/choke1.wav", "vr/gore/choke2.wav", nullptr}, 2, 1.f, 4, 0.f, 1.f, 0.f, 1.05f},
    {"tear", {"vr/gore/tear1.wav", "vr/gore/tear2.wav", nullptr}, 2, 0.9f, 3, 0.05f, 1.f, 1500.f, 0.41f},
    {"squelch", {"vr/gore/squelch1.wav", "vr/gore/squelch2.wav", "vr/gore/squelch3.wav"}, 3, 0.6f, 2, 0.2f, 1.f, 600.f, 0.25f},
    {"step", {"vr/gore/step1.wav", "vr/gore/step2.wav", "vr/gore/step3.wav"}, 3, 0.45f, 1, 0.15f, 1.f, 0.f, 0.24f},
    {"slap", {"vr/gore/slap1.wav", "vr/gore/slap2.wav", "vr/gore/slap3.wav"}, 3, 0.7f, 3, 0.1f, 1.f, 1000.f, 0.2f},
    {"splash", {"vr/gore/splash1.wav", "vr/gore/splash2.wav", "vr/gore/splash3.wav"}, 3, 0.55f, 1, 0.25f, 1.f, 0.f, 0.3f},
};

// Each kind's own volume (vr_gore_sound_vol_<kind>, times vr_gore_sound), in Kind's order.
cvar_t* const kindVolumes[kindCount] = {&vr_gore_sound_vol_spurt, &vr_gore_sound_vol_pour, &vr_gore_sound_vol_choke,
    &vr_gore_sound_vol_tear, &vr_gore_sound_vol_squelch, &vr_gore_sound_vol_step, &vr_gore_sound_vol_slap,
    &vr_gore_sound_vol_splash};

constexpr float nearerShare = 0.5f;   // within a kind's gap, one this much nearer than the last played still plays
constexpr int chanVoice = 2;          // QC CHAN_VOICE: the cries
constexpr int channelBase = 200;      // the capped kinds' channels on entity 0: one per slot (a new one there cuts it)
constexpr float slowPitchShare = 0.5f; // bullet time: the pitch takes this share of its time scale's slowing
constexpr float pourGrainOverlap = 0.08f; // s: a stream's next grain starts this long before one ends (their fades)
constexpr int maxPours = 64;
constexpr float stepLength = 30.f;   // units walked on the ground for a step (a stride of about 0.8 m)
constexpr float wetSeconds = 4.f;    // steps stay wet this long after a pool, fading
constexpr float poolWetSeconds = 90.f; // a pool is wet underfoot this long after it was laid
constexpr int maxPools = 64;
constexpr float slapSpeedLo = 80.f, slapSpeedHi = 500.f; // units/s: a gib's wall hit from faint to full
// Server: a monster's choke at most every chokeEvery s, any choke at most every chokeAnyGap s (otherwise its cry plays),
// its voice muted chokeMute s; a ragdoll's squelch at most every squelchEvery s per body.
constexpr double chokeEvery = 3.0;
constexpr double chokeAnyGap = 0.2;
constexpr double chokeMute = 0.3;
constexpr double squelchEvery = 1.0;

struct Voice
{
    double end = 0.0;
    float dist = 0.f;
};

struct Stats
{
    int played = 0, stolen = 0, capped = 0, rated = 0, tooFar = 0, silenced = 0;
};

struct Pour
{
    int key;
    glm::vec3 at;
    float flow;
    float dist;
};

struct Pool
{
    glm::vec3 at{0.f};
    float radius = 0.f;
    double born = -1.0;
};

struct TestEvent
{
    double when;
    Kind kind;
    glm::vec3 at;
};

// A sound asked for this frame (play), played or dropped at the frame's end nearest first (frame).
struct Pending
{
    Kind kind;
    int ent;
    glm::vec3 at;
    float strength;
    float dist;
    double asked;
};
constexpr int maxPending = 128;
constexpr double pendingStale = 0.25; // real s: one asked longer ago than this (no frame since) is dropped

// The sounds, loaded (each variant's length in seconds as recorded).
struct Sounds
{
    sfx_t* sfx[kindCount][maxVariants]{};
    float seconds[kindCount][maxVariants]{};
    bool loaded = false;
};
Sounds sounds;

// The client's state (cleared each map).
struct State
{
    int last[kindCount]{};
    Voice voices[kindCount][maxCap]{};
    double lastPlayed[kindCount]{};
    float lastDist[kindCount]{};
    Stats stats[kindCount]{};
    Pour pours[maxPours]{};
    int pourCount = 0;
    double pourNext[maxPours]{};
    Pool pools[maxPools]{};
    int poolHead = 0;
    glm::vec3 lastFeet{0.f};
    bool hasFeet = false;
    float walked = 0.f;
    double wetUntil = 0.0;
    bool inPool = false; // the last step's in a pool (the next one out of it: no splash)
    int held[2]{};
    za::Vector<TestEvent> tests;
    int testKey = 0;
    Pending pending[maxPending]{};
    int pendingCount = 0;
};
State st;

void playNow(Kind kind, int ent, const glm::vec3& at, float strength); // (below)

// The server's: per edict, a choke's mute (from, until) and the last choke and squelch.
struct ServerEnt
{
    double muteFrom = 0.0, muteUntil = 0.0;
    double choke = -1.0, squelch = -1.0;
};
za::Vector<ServerEnt> serverEnts;
double lastChokeAny = -1.0;

za::FastNonCryptoRng rng{0x60e5bd17ull};

[[nodiscard]] bool serverOn()
{
    return vr_gore.value > 0.f && vr_gore_sound.value > 0.f;
}

[[nodiscard]] bool clientOn()
{
    return gore::level() > 0 && vr_gore_sound.value > 0.f && cls.state == ca_connected && cl.worldmodel;
}

// Bullet time's playback rate: slowPitchShare of its slowing (0.3: 0.65).
[[nodiscard]] float slowPitch()
{
    const float scale = za::clamp(bullettime::scale(), 0.05f, 1.f);
    return 1.f - (1.f - scale) * slowPitchShare;
}

[[nodiscard]] float jitter()
{
    const float pct = za::clamp(vr_snd_pitch_jitter.value, 0.f, 25.f) * 0.01f;
    return 1.f + rng.getF(-pct, pct);
}

void load()
{
    for(int k = 0; k < kindCount; k++)
    {
        for(int v = 0; v < kinds[k].count; v++)
        {
            sfx_t* s = S_PrecacheSound(kinds[k].files[v]);
            sounds.sfx[k][v] = s;
            const sfxcache_t* c = s ? S_LoadSound(s) : nullptr;
            sounds.seconds[k][v] = c && c->speed > 0 ? static_cast<float>(c->length) / static_cast<float>(c->speed)
                                                     : kinds[k].seconds;
            if(vr_gore_sound_debug.value)
            {
                Con_Printf("goresound: %s %s %.2f s\n", kinds[k].files[v], c ? "loaded" : "NOT LOADED",
                    sounds.seconds[k][v]);
            }
        }
    }
    sounds.loaded = true;
}

// One of the kind's variants, never the one played last: its index (-1: none).
[[nodiscard]] int pick(int k)
{
    const int n = kinds[k].count;
    if(n <= 0)
    {
        return -1;
    }
    if(!sounds.loaded)
    {
        load();
    }
    int v = n > 1 ? rng.getI(0, n - 2) : 0;
    if(n > 1 && v >= st.last[k])
    {
        v++;
    }
    st.last[k] = v;
    return v;
}

void log(int k, const char* what, int ent, const glm::vec3& at, float dist, float vol, float pitch)
{
    if(vr_gore_sound_debug.value)
    {
        Con_Printf("goresound: %s %s ent %d at %.0f %.0f %.0f dist %.0f vol %.2f pitch %.2f\n", kinds[k].name, what, ent,
            at.x, at.y, at.z, dist, vol, pitch);
    }
}

// Where you hear from: the view (the sound system's listener is not kept without a sound device).
[[nodiscard]] glm::vec3 listener()
{
    return glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
}

void start(int k, int ent, int channel, const glm::vec3& at, float vol, float pitch, int v)
{
    if(sfx_t* s = sounds.sfx[k][v])
    {
        vec3_t org{at.x, at.y, at.z};
        S_StartSoundPitch(ent, channel, s, org, vol, kinds[k].attenuation, pitch);
    }
}

[[nodiscard]] bool isGib(int num)
{
    if(num <= 0 || num >= cl.num_entities || !cl_entities[num].model)
    {
        return false;
    }
    const qmodel_t* m = cl_entities[num].model;
    return modelmeta::has(m, modelmeta::Trait::Head) || modelmeta::has(m, modelmeta::Trait::ContainsGib) ||
           modelmeta::has(m, modelmeta::Trait::Limb);
}

// The streams landing this frame: the nearest few pour, each a grain back to back.
void pourFrame(float pitch)
{
    const int cap = kinds[static_cast<int>(Kind::Pour)].cap;
    for(int n = 0; n < cap && st.pourCount > 0; n++)
    {
        int best = 0;
        for(int i = 1; i < st.pourCount; i++)
        {
            if(st.pours[i].dist < st.pours[best].dist)
            {
                best = i;
            }
        }
        const Pour p = st.pours[best];
        st.pours[best] = st.pours[--st.pourCount];
        if(p.key < 0 || p.key >= maxPours || realtime < st.pourNext[p.key])
        {
            continue;
        }
        playNow(Kind::Pour, 0, p.at, p.flow);
        const int v = st.last[static_cast<int>(Kind::Pour)];
        const float len = sounds.seconds[static_cast<int>(Kind::Pour)][za::clamp(v, 0, maxVariants - 1)];
        st.pourNext[p.key] = realtime + za::max(0.1f, (len - pourGrainOverlap) / pitch);
    }
    st.pourCount = 0;
}

// Your steps: a splash stepping into a pool, wet for wetSeconds after one in a pool, fading.
void stepFrame()
{
    if(cl.viewentity <= 0 || cl.viewentity >= cl.num_entities || cl.stats[STAT_HEALTH] <= 0)
    {
        st.hasFeet = false;
        return;
    }
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 feet{pl.origin[0], pl.origin[1], pl.origin[2] - 22.f};
    if(!st.hasFeet)
    {
        st.lastFeet = feet;
        st.hasFeet = true;
        return;
    }
    const glm::vec2 d{feet.x - st.lastFeet.x, feet.y - st.lastFeet.y};
    st.lastFeet = feet;
    const float moved = glm::length(d);
    if(moved > 64.f) // (a teleport)
    {
        st.walked = 0.f;
        return;
    }
    if(!cl.onground)
    {
        return;
    }
    st.walked += moved;
    if(st.walked < stepLength)
    {
        return;
    }
    st.walked = 0.f;
    const bool pool = poolUnder(feet);
    if(pool && !st.inPool)
    {
        st.inPool = true;
        playNow(Kind::Splash, 0, feet, 1.f); // (into it: the splash, not a wet step)
        st.wetUntil = realtime + wetSeconds;
        return;
    }
    st.inPool = pool;
    if(pool)
    {
        st.wetUntil = realtime + wetSeconds;
    }
    if(realtime < st.wetUntil)
    {
        playNow(Kind::Step, 0, feet, za::clamp(static_cast<float>((st.wetUntil - realtime) / wetSeconds), 0.15f, 1.f));
    }
}

// A hand taking a gib (its carry stat changing to one): a squelch.
void handFrame()
{
    for(int hand = 0; hand < 2; hand++)
    {
        const int ent = cl.stats[hand == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
        if(ent != st.held[hand] && ent > 0 && isGib(ent) && cl.stats[STAT_HEALTH] > 0)
        {
            const entity_t& e = cl_entities[ent];
            play(Kind::Squelch, ent, glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, 1.f);
        }
        st.held[hand] = ent;
    }
}

[[nodiscard]] int kindByName(const char* s)
{
    for(int k = 0; k < kindCount; k++)
    {
        if(!strcmp(s, kinds[k].name))
        {
            return k;
        }
    }
    return -1;
}

// vr_gore_sound_stats [reset]: each kind's sounds played, taken from a farther one, dropped (capped, rate-limited,
// too far, off) since the map began or the last reset.
void stats_f()
{
    const bool reset = Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset");
    for(int k = 0; k < kindCount; k++)
    {
        const Stats& s = st.stats[k];
        if(!reset)
        {
            Con_Printf("goresound stats: %-7s played %d (took a farther one's place %d), capped %d, rate %d, far %d, off %d\n",
                kinds[k].name, s.played, s.stolen, s.capped, s.rated, s.tooFar, s.silenced);
        }
        st.stats[k] = Stats{};
    }
}

// vr_gore_sound_test <kind> [n] [interval] [spread]: `n` of `kind` (1) every `interval` real s (0: all at once) round
// you, at random from 48 to `spread` units (600) away: the caps and the rate limits at work (vr_gore_sound_debug 1,
// vr_gore_sound_stats).
void test_f()
{
    const int k = Cmd_Argc() > 1 ? kindByName(Cmd_Argv(1)) : -1;
    if(k < 0)
    {
        Con_Printf("usage: vr_gore_sound_test spurt|pour|choke|tear|squelch|step|slap|splash [n] [interval] [spread]\n");
        return;
    }
    const int n = Cmd_Argc() > 2 ? za::clamp(atoi(Cmd_Argv(2)), 1, 200) : 1;
    const float interval = Cmd_Argc() > 3 ? za::clamp(static_cast<float>(atof(Cmd_Argv(3))), 0.f, 5.f) : 0.f;
    const float spread = Cmd_Argc() > 4 ? za::clamp(static_cast<float>(atof(Cmd_Argv(4))), 48.f, 4000.f) : 600.f;
    const glm::vec3 me = listener();
    for(int i = 0; i < n; i++)
    {
        const float yaw = rng.getF(0.f, 6.2831853f);
        const float r = rng.getF(48.f, spread);
        st.tests.pushBack({realtime + interval * static_cast<float>(i), static_cast<Kind>(k),
            me + glm::vec3{glm::cos(yaw) * r, glm::sin(yaw) * r, -24.f}});
    }
}

} // namespace

// --- Server side ---

bool send(Kind kind, int ent, const float* at, float strength)
{
    const int k = static_cast<int>(kind);
    if(k < 0 || k >= kindCount || !serverOn() || sv.state != ss_active || !sv.active ||
        sv.datagram.cursize > MAX_DATAGRAM - 24)
    {
        return false;
    }
    if(ent >= 0 && ent < 65536 && ent >= static_cast<int>(serverEnts.size()))
    {
        serverEnts.resize(static_cast<za::SizeT>(ent) + 1);
    }
    ServerEnt* e = ent >= 0 && ent < static_cast<int>(serverEnts.size()) ? &serverEnts[static_cast<za::SizeT>(ent)] : nullptr;
    const auto recent = [](double then, double every) { return then >= 0.0 && then <= sv.qcvm.time && sv.qcvm.time - then < every; };
    if(kind == Kind::Choke)
    {
        if(!vr_gore_sound_choke.value || vr_gore_sound_vol_choke.value <= 0.f)
        {
            if(vr_gore_sound_debug.value)
            {
                Con_Printf("goresound: choke not sent for ent %d (vr_gore_sound_choke 0: its cry plays)\n", ent);
            }
            return false; // (no mute: its cry plays)
        }
        if(!e || ent <= 0 || recent(e->choke, chokeEvery) || recent(lastChokeAny, chokeAnyGap))
        {
            if(vr_gore_sound_debug.value)
            {
                const bool own = e && ent > 0 && recent(e->choke, chokeEvery);
                Con_Printf("goresound: choke not sent for ent %d (%s)\n", ent,
                    own ? "it choked already" : "another just choked: its cry plays");
            }
            return false;
        }
        e->choke = sv.qcvm.time;
        lastChokeAny = sv.qcvm.time;
        e->muteFrom = sv.qcvm.time;
        e->muteUntil = sv.qcvm.time + chokeMute;
    }
    else if(kind == Kind::Squelch && e)
    {
        if(recent(e->squelch, squelchEvery))
        {
            return false;
        }
        e->squelch = sv.qcvm.time;
    }
    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_GORESOUND);
    MSG_WriteByte(&sv.datagram, k);
    MSG_WriteShort(&sv.datagram, za::clamp(ent, 0, 32767));
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, at[i], sv.protocolflags);
    }
    MSG_WriteByte(&sv.datagram, za::clamp(static_cast<int>(strength * 255.f + 0.5f), 0, 255));
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
    return true;
}

bool voiceMuted(int ent, int channel)
{
    if(channel != chanVoice || ent <= 0 || ent >= static_cast<int>(serverEnts.size()))
    {
        return false;
    }
    const ServerEnt& e = serverEnts[static_cast<za::SizeT>(ent)];
    const bool muted = sv.qcvm.time >= e.muteFrom && sv.qcvm.time < e.muteUntil;
    if(muted && vr_gore_sound_debug.value)
    {
        Con_Printf("goresound: ent %d's cry dropped (it chokes)\n", ent);
    }
    return muted;
}

// --- Client side ---

void parseEvent()
{
    const int k = MSG_ReadByte();
    const int ent = MSG_ReadShort();
    glm::vec3 at;
    for(int i = 0; i < 3; i++)
    {
        at[i] = MSG_ReadCoord(cl.protocolflags);
    }
    const float strength = static_cast<float>(MSG_ReadByte()) / 255.f;
    if(k >= 0 && k < kindCount)
    {
        play(static_cast<Kind>(k), ent, at, strength);
    }
}

void play(Kind kind, int ent, const glm::vec3& at, float strength)
{
    if(st.pendingCount >= maxPending)
    {
        st.stats[static_cast<int>(kind)].rated++;
        return;
    }
    st.pending[st.pendingCount++] = Pending{kind, ent, at, strength, glm::distance(at, listener()), realtime};
}

namespace
{

// A sound played now (or dropped): the frame's asked for ones nearest first (frame), the streams' grains, the steps.
void playNow(Kind kind, int ent, const glm::vec3& at, float strength)
{
    const int k = static_cast<int>(kind);
    const KindInfo& ki = kinds[k];
    Stats& s = st.stats[k];
    const float pitch = slowPitch() * jitter();
    const float vol = za::clamp(vr_gore_sound.value, 0.f, 2.f) * za::clamp(kindVolumes[k]->value, 0.f, 2.f) * ki.gain *
                      za::clamp(strength, 0.f, 1.f);
    const float dist = glm::distance(at, listener());
    if(!clientOn() || vol < 0.01f)
    {
        s.silenced++;
        log(k, "off", ent, at, dist, vol, pitch);
        return;
    }
    if(ki.maxDistance > 0.f && dist > ki.maxDistance)
    {
        s.tooFar++;
        log(k, "far", ent, at, dist, vol, pitch);
        return;
    }
    if(ki.gap > 0.f && realtime - st.lastPlayed[k] < ki.gap && realtime >= st.lastPlayed[k] &&
        dist >= st.lastDist[k] * nearerShare)
    {
        s.rated++;
        log(k, "rate", ent, at, dist, vol, pitch);
        return;
    }
    const int v = pick(k);
    if(v < 0)
    {
        s.silenced++;
        log(k, "missing", ent, at, dist, vol, pitch);
        return;
    }
    const double end = realtime + sounds.seconds[k][v] / za::max(pitch, 0.1f);
    if(kind == Kind::Choke)
    {
        // On its voice channel (its cry, if one plays, cut): never dropped here (the server muted its cry for it).
        start(k, ent, chanVoice, at, vol, pitch, v);
    }
    else if(kind == Kind::Pour)
    {
        start(k, 0, 0, at, vol, pitch, v); // (the streams' own cap: pourFrame)
    }
    else
    {
        // A free slot; else the farthest playing, if this one is nearer.
        int slot = -1;
        for(int i = 0; i < ki.cap; i++)
        {
            if(st.voices[k][i].end <= realtime || st.voices[k][i].end > realtime + 30.0)
            {
                slot = i;
                break;
            }
        }
        if(slot < 0)
        {
            int farthest = 0;
            for(int i = 1; i < ki.cap; i++)
            {
                if(st.voices[k][i].dist > st.voices[k][farthest].dist)
                {
                    farthest = i;
                }
            }
            if(dist >= st.voices[k][farthest].dist)
            {
                s.capped++;
                log(k, "capped", ent, at, dist, vol, pitch);
                return;
            }
            slot = farthest;
            s.stolen++;
        }
        st.voices[k][slot] = Voice{end, dist};
        start(k, 0, channelBase + k * maxCap + slot, at, vol, pitch, v);
    }
    st.lastPlayed[k] = realtime;
    st.lastDist[k] = dist;
    s.played++;
    log(k, sounds.sfx[k][v] ? "played" : "played (no sound loaded)", ent, at, dist, vol, pitch);
}

// The sounds asked for since the last frame, nearest first (the farther ones meet the caps and the gaps).
void flushPending()
{
    for(int i = 1; i < st.pendingCount; i++)
    {
        const Pending p = st.pending[i];
        int j = i;
        for(; j > 0 && st.pending[j - 1].dist > p.dist; j--)
        {
            st.pending[j] = st.pending[j - 1];
        }
        st.pending[j] = p;
    }
    for(int i = 0; i < st.pendingCount; i++)
    {
        const Pending& p = st.pending[i];
        if(realtime - p.asked <= pendingStale)
        {
            playNow(p.kind, p.ent, p.at, p.strength);
        }
    }
    st.pendingCount = 0;
}

} // namespace

void pour(int key, const glm::vec3& at, float flow)
{
    if(st.pourCount < maxPours && flow > 0.02f)
    {
        st.pours[st.pourCount++] = Pour{key, at, za::clamp(flow, 0.f, 1.f), glm::distance(at, listener())};
    }
}

void notePool(const glm::vec3& at, const glm::vec3& normal, float size)
{
    if(normal.z < 0.7f)
    {
        return;
    }
    st.pools[st.poolHead] = Pool{at, size * 0.5f, realtime};
    st.poolHead = (st.poolHead + 1) % maxPools;
}

bool poolUnder(const glm::vec3& feet, float margin)
{
    for(const Pool& p : st.pools)
    {
        if(p.born < 0.0 || realtime - p.born > poolWetSeconds || realtime < p.born)
        {
            continue;
        }
        const glm::vec2 d{feet.x - p.at.x, feet.y - p.at.y};
        if(glm::dot(d, d) <= (p.radius + margin) * (p.radius + margin) && za::fabs(feet.z - p.at.z) < 20.f)
        {
            return true;
        }
    }
    return false;
}

void gibHit(const glm::vec3& where, const glm::vec3& normal, float speed)
{
    if(normal.z > 0.6f || speed < slapSpeedLo)
    {
        return; // (the floor: the QC's squish)
    }
    play(Kind::Slap, 0, where, 0.3f + 0.7f * za::clamp((speed - slapSpeedLo) / (slapSpeedHi - slapSpeedLo), 0.f, 1.f));
}

void prepare()
{
    load();
}

void frame()
{
    QVR_PROFILE("gore sounds");
    for(za::SizeT i = 0; i < st.tests.size();)
    {
        if(st.tests[i].when <= realtime)
        {
            const TestEvent t = st.tests[i];
            st.tests[i] = st.tests[st.tests.size() - 1];
            st.tests.popBack();
            if(t.kind == Kind::Pour)
            {
                st.testKey = (st.testKey + 1) % maxPours;
                pour(st.testKey, t.at, 1.f);
            }
            else
            {
                play(t.kind, 0, t.at, 1.f);
            }
        }
        else
        {
            i++;
        }
    }
    if(!clientOn())
    {
        st.pourCount = 0;
        st.hasFeet = false;
        flushPending(); // (each dropped: "off")
        return;
    }
    pourFrame(slowPitch());
    stepFrame();
    handFrame();
    flushPending();
}

void clear()
{
    st = State{};
    serverEnts.clear();
    lastChokeAny = -1.0;
}

void registerCommands()
{
    Cmd_AddCommand("vr_gore_sound_stats", stats_f);
    Cmd_AddCommand("vr_gore_sound_test", test_f);
}

} // namespace qvr::goresound

extern "C" int VR_GoreSoundMutes(int ent, int channel)
{
    return qvr::goresound::voiceMuted(ent, channel) ? 1 : 0;
}
