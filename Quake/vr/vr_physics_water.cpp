// vr_physics_water.cpp -- server-side water: splashes and sounds (things, shots, hands and wading going in or out of a
//    liquid) and swimming (vr_swim). Moved out of vr_physics.cpp; the rest of the server physics stays there.
#include "vr_box3d.hpp"
#include "vr_random.hpp"
#include "vr_trace.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_physics_internal.hpp"
#include "vr_held.hpp"
#include "vr_hull.hpp"
#include "vr_progs.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_move.hpp"
#include "vr_server.hpp"
#include "vr_portals.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_tracy.h"

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/Unique.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"
#include "vr_geom.hpp"

#include <stdlib.h>
#include <string.h>

using namespace qvr;
using namespace qvr::progs;
using namespace qvr::physics::detail;

// ----------------------------------------------------------------------------
// Water splashes and sounds (vr_water_splash, vr_water_sounds)
//
// A splash is Quake VR's particle preset (particles::Preset::Splash: drops, foam and ripples,
// drawn by each client as its vr_water_splash says), sent from here, with a sound at the spot
// (vr_water_sounds is their volume; the sounds are recordings, a few of each picked at random:
// variant, below; docs/vr-port/CREDITS.md; precached at each map's start: precacheWaterSounds). They come from:
// - things going into a liquid (SV_CheckWaterTransition: shots, grenades, nails, gibs, items,
//   thrown weapons, a monster falling in), as hard as they go and as big as they are
//   (VR_AllowWaterSplash, below);
// - shots crossing a surface (QC's liquidentry and watersplash: weapons.qc) and the player going
//   in (client.qc's WaterMove, with Quake's own sound);
// - hands and guns hitting the surface or pulled out of it fast (with a buzz in the hand), and
//   wading: sloshes at a walking pace, ripples at the legs (waterFeedback, every frame);
// - swimming strokes: each hand's stroke past its power gate, as the hand passes its fastest
//   (VR_AfterWaterMove, strokeFeedback): near the surface a recorded stroke and a splash, deeper
//   the water swept aside (synthesised: make_sounds.py).

namespace
{

// The surface between a point out of the liquid and one in it.
[[nodiscard]] glm::vec3 surfaceBetween(glm::vec3 dry, glm::vec3 wet)
{
    for(int i = 0; i < 12 && glm::distance(dry, wet) > 0.1f; i++)
    {
        const glm::vec3 mid = (dry + wet) * 0.5f;
        (worldtrace::isLiquid(worldtrace::contents(mid)) ? wet : dry) = mid;
    }
    return (dry + wet) * 0.5f;
}

// The surface on the vertical through `p`, at most `range` units above it (from in the liquid) or
// below it (from out of it); false if there is none (a wall or floor comes first).
[[nodiscard]] bool surfaceOver(const glm::vec3& p, float range, glm::vec3& out)
{
    const bool wet = worldtrace::isLiquid(worldtrace::contents(p));
    glm::vec3 last = p;
    for(float d = 4.f; d < range + 4.f; d += 4.f)
    {
        const glm::vec3 q = p + glm::vec3{0.f, 0.f, wet ? za::min(d, range) : -za::min(d, range)};
        const int c = worldtrace::contents(q);
        if(c == CONTENTS_SOLID || c == CONTENTS_SKY)
        {
            return false;
        }
        if(worldtrace::isLiquid(c) != wet)
        {
            out = wet ? surfaceBetween(q, last) : surfaceBetween(last, q);
            return true;
        }
        last = q;
    }
    return false;
}

// The water sounds: each a few recordings (quakevr/sound/vr; where they come from: docs/vr-port/CREDITS.md), one
// played at random. Precached at each map's start (precacheWaterSounds; QC/world.qc names them too), and played by
// their precache indices: nothing built or compared by name as they play.
enum class WaterSound : int
{
    SplashSmall,
    SplashBig,
    SplashOut, // a hand out (one recording)
    Plip,
    Slosh,
    Stroke,    // a stroke near the surface (recordings)
    SwimSoft,  // a gentle stroke under water (make_sounds.py)
    SwimHard,  // a brisk one
    Count
};

struct WaterSoundFiles
{
    const char* files[4];
    int count;
};

constexpr WaterSoundFiles waterSoundFiles[] = {
    {{"vr/splash_small1.wav", "vr/splash_small2.wav", "vr/splash_small3.wav", "vr/splash_small4.wav"}, 4},
    {{"vr/splash_big1.wav", "vr/splash_big2.wav", "vr/splash_big3.wav", "vr/splash_big4.wav"}, 4},
    {{"vr/splash_out1.wav"}, 1},
    {{"vr/plip1.wav", "vr/plip2.wav", "vr/plip3.wav", "vr/plip4.wav"}, 4},
    {{"vr/slosh1.wav", "vr/slosh2.wav", "vr/slosh3.wav", "vr/slosh4.wav"}, 4},
    {{"vr/stroke1.wav", "vr/stroke2.wav", "vr/stroke3.wav", "vr/stroke4.wav"}, 4},
    {{"vr/swim_soft1.wav", "vr/swim_soft2.wav", "vr/swim_soft3.wav"}, 3},
    {{"vr/swim_hard1.wav", "vr/swim_hard2.wav", "vr/swim_hard3.wav"}, 3},
};
static_assert(za::getArraySize(waterSoundFiles) == static_cast<za::SizeT>(WaterSound::Count));

int waterSoundIndices[static_cast<int>(WaterSound::Count)][4]{}; // this server's precache indices (0: none)
// The water sounds' variant and pitch picks (audible only): a stream of their own, not the C library's rand().
za::U32 waterSoundState = 0x85EBCA6Bu;
int lastVariant[static_cast<int>(WaterSound::Count)]{-1, -1, -1, -1, -1, -1, -1, -1};

// One of a water sound's recordings, at random, never the one played last: its precache index (0: not precached).
[[nodiscard]] int variant(WaterSound sound)
{
    const int s = static_cast<int>(sound);
    const int count = waterSoundFiles[s].count;
    int& prev = lastVariant[s];
    int k = count > 1 ? static_cast<int>(vrand::xorshift32(waterSoundState) % static_cast<za::U32>(prev >= 0 ? count - 1 : count)) : 0;
    if(prev >= 0 && count > 1 && k >= prev)
    {
        k++;
    }
    prev = k;
    return waterSoundIndices[s][k];
}

double soundFrame = -1.0;
int soundsThisFrame = 0;

// Sounds at a point (not an entity's): the world's entity, on the auto channel, with the position
// given -- what SV_StartSound sends, where a sound is, at playback rate `pitch` (1 as recorded). At most
// a few a frame (a shotgun's pellets). `sound`: its precache index (variant).
bool soundAt(const glm::vec3& at, int sound, float volume, float attenuation = 1.f, float pitch = 1.f)
{
    if(VR_NoLiquidEffects(sv.worldmodel, &at.x)) { return false; }
    const float master = CLAMP(0.f, vr_water_sounds.value, 1.f);
    const int vol = static_cast<int>(CLAMP(0.f, volume * master, 1.f) * 255.f);
    const int index = vol > 0 ? sound : 0;
    if(!index)
    {
        return false;
    }
    if(soundFrame != qcvm->time)
    {
        soundFrame = qcvm->time;
        soundsThisFrame = 0;
    }
    if(soundsThisFrame >= 3 || sv.datagram.cursize > MAX_DATAGRAM - 23)
    {
        return true; // played enough of them this frame
    }
    soundsThisFrame++;
    if(developer.value >= 2)
    {
        Con_Printf("VR water sound: %s, volume %.2f, pitch %.2f\n", sv.sound_precache[index], vol / 255.f, pitch);
    }
    const vec3_t origin{at.x, at.y, at.z};
    if(!SV_WriteSound(&sv.datagram, 0, 0, index, vol, attenuation, origin, pitch)) // (the world, channel 0: auto)
    {
        return false;
    }
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
    return true;
}

// The splash's particles, as QC's particle2 sends them (unreliable). One no stronger than another
// already sent this frame close by is left out: a shotgun's pellets land together.
struct SentSplash
{
    glm::vec3 at;
    float strength;
};
double splashFrame = -1.0;
za::Vector<SentSplash> splashesThisFrame;

bool sendSplash(const glm::vec3& at, const glm::vec3& dir, float strength)
{
    if(VR_NoLiquidEffects(sv.worldmodel, &at.x)) { return false; }
    // (Its figures go into the message as integers: none from a NaN or an infinity.)
    if(!finite(at) || !finite(dir) || !ZA_ISFINITE(strength))
    {
        return false;
    }
    if(splashFrame != qcvm->time)
    {
        splashFrame = qcvm->time;
        splashesThisFrame.clear();
    }
    for(const SentSplash& s : splashesThisFrame)
    {
        if(glm::distance(s.at, at) < 12.f && strength <= s.strength)
        {
            return false;
        }
    }
    splashesThisFrame.pushBack({at, strength});
    if(developer.value >= 2)
    {
        Con_Printf("VR splash: %.1f %.1f %.1f, strength %.1f\n", at.x, at.y, at.z, strength);
    }
    if(sv.datagram.cursize > MAX_DATAGRAM - 24)
    {
        return true;
    }
    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_PARTICLE2);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, at[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteChar(&sv.datagram, CLAMP(-128, static_cast<int>(dir[i] * 16.f), 127));
    }
    MSG_WriteByte(&sv.datagram, static_cast<int>(particles::Preset::Splash));
    MSG_WriteShort(&sv.datagram, CLAMP(1, static_cast<int>(strength + 0.5f), 100));
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
    return true;
}

// Out of the water: a smaller splash (drops falling off), the sound quieter.
void splashOut(const glm::vec3& at, float strength, float volume)
{
    sendSplash(at, glm::vec3{0.f, 0.f, 1.f}, strength);
    soundAt(at, variant(WaterSound::SplashOut), volume);
}

// Each player's hands, guns and legs in the water.
struct WaterProbe
{
    glm::vec3 pos{0.f};
    int contents{CONTENTS_EMPTY};
    bool valid{false};
};

struct WaterFeel
{
    double time{-1.0};
    WaterProbe probes[4];                   // off hand, main hand, off gun's muzzle, main gun's
    double handSplash[2]{-10.0, -10.0};     // when each hand last splashed
    double stroke[2]{-10.0, -10.0};         // when each hand's last stroke was heard
    glm::vec3 origin{0.f};
    bool originValid{false};
    float wade{0.f};                        // the pace: a slosh at every whole one
    float wadeSpeed{0.f};                   // units/s, smoothed
    glm::vec2 wadeDir{1.f, 0.f};
    double wetSince[2]{-1.0, -1.0};         // since when each hand is in the water (-1: out)
};

WaterFeel waterFeel[MAX_SCOREBOARD];

[[nodiscard]] WaterFeel* feelOf(edict_t* ent)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
    {
        return nullptr;
    }
    return &waterFeel[client];
}

// A hand (or a gun) crossing the surface: into it going down, or out of it going up, fast.
void handCrossing(edict_t* ent, WaterFeel& w, int probe, const WaterProbe& was, const glm::vec3& p, int contents, float dt)
{
    const bool in = worldtrace::isLiquid(contents);
    const bool crossed = in != worldtrace::isLiquid(was.contents) && (contents == CONTENTS_EMPTY || was.contents == CONTENTS_EMPTY);
    const int hand = probe & 1;
    if(!crossed || !(dt > 0.f) || qcvm->time - w.handSplash[hand] < 0.3)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 vel = (p - was.pos) / dt / m2u; // m/s, the body's motion too
    const float speed = glm::length(vel);
    const float needed = in ? 0.9f : 1.4f; // m/s, down into it or up out of it
    if(!(speed <= 12.f) || !((in ? -vel.z : vel.z) >= needed)) // (a NaN turned away too)
    {
        return; // a tracking jump, or too slow (a hand dipped in)
    }
    const float hard = CLAMP(0.f, (speed - needed) / 3.f, 1.f);
    const float gun = probe >= 2 ? 1.25f : 1.f;
    w.handSplash[hand] = qcvm->time;
    if(in)
    {
        const glm::vec3 at = surfaceBetween(was.pos, p);
        sendSplash(at, vel / speed, (4.f + 12.f * hard) * gun);
        soundAt(at, variant(WaterSound::SplashSmall), 0.35f + 0.65f * hard);
        server::sendHaptic(ent, hand, 0.f, 0.06f + 0.08f * hard, 60.f, 0.3f + 0.5f * hard);
    }
    else
    {
        splashOut(surfaceBetween(p, was.pos), (2.f + 5.f * hard) * gun, 0.2f + 0.3f * hard);
    }
}

// Wading: with the legs in the water (not the head), walking on the bottom (or in the room),
// a slosh at a walking pace (quicker the faster) and ripples at the legs.
void wading(edict_t* ent, WaterFeel& w, float dt)
{
    const glm::vec3 origin = vec(ent->v.origin);
    const bool valid = w.originValid;
    const glm::vec3 last = w.origin;
    w.origin = origin;
    w.originValid = true;
    const float level = ent->v.waterlevel;
    if(!valid || dt <= 0.f || level < 1.f || level > 2.f)
    {
        w.wade = 0.f;
        w.wadeSpeed = 0.f;
        return;
    }
    const glm::vec2 moved{origin.x - last.x, origin.y - last.y};
    const float distance = glm::length(moved);
    if(distance > 32.f)
    {
        return; // a teleport
    }
    // The speed smoothed (moves come in unevenly, some frames none), and where it goes.
    w.wadeSpeed += (distance / dt - w.wadeSpeed) * za::min(1.f, dt / 0.25f);
    if(distance > 0.01f)
    {
        w.wadeDir = moved / distance;
    }
    const glm::vec3 feet = origin + glm::vec3{0.f, 0.f, ent->v.mins[2] - 4.f};
    const bool onBottom = hasFlag(ent, FL_ONGROUND) || worldtrace::contents(feet) == CONTENTS_SOLID;
    if(w.wadeSpeed < 15.f || !onBottom) // slower than 0.6 m/s: standing, shuffling
    {
        w.wade = za::min(w.wade, 0.6f); // the next step soon after moving again
        return;
    }
    const float interval = CLAMP(0.3f, 0.62f - w.wadeSpeed / 600.f, 0.6f); // a step at a walk (1.4 m/s) about 0.55 s
    w.wade += dt / interval;
    if(w.wade < 1.f)
    {
        return;
    }
    w.wade -= 1.f;

    glm::vec3 at;
    const glm::vec3 ahead{w.wadeDir, 0.f};
    if(!surfaceOver(origin + ahead * 6.f, 64.f, at))
    {
        return;
    }
    const float deep = level >= 2.f ? 1.f : 0.65f;
    sendSplash(at, ahead, 2.f + 2.f * deep);
    soundAt(at, variant(WaterSound::Slosh), CLAMP(0.3f, 0.25f + w.wadeSpeed / 200.f, 0.8f) * deep);
}

} // namespace

namespace qvr::physics::detail
{

void waterFeedback(edict_t* ent)
{
    WaterFeel* feel = feelOf(ent);
    if(!feel)
    {
        return;
    }
    WaterFeel& w = *feel;
    const double now = qcvm->time;
    if(now < w.time || now - w.time > 0.25) // a new map, a pause: start afresh
    {
        w = {};
    }
    const float dt = w.time < 0.0 ? 0.f : static_cast<float>(now - w.time);
    w.time = now;

    const VrMove* move = server::clientMove(ent);
    if(!move || static_cast<int>(ent->v.movetype) == MOVETYPE_NOCLIP || ent->v.health <= 0.f)
    {
        w = {};
        w.time = now;
        return;
    }

    for(int i = 0; i < 4; i++)
    {
        const int hand = i & 1;
        const glm::vec3 p = i < 2 ? move->hands[hand].pos : move->muzzlePos[hand];
        WaterProbe& probe = w.probes[i];
        // No gun (the muzzle at or near the hand): the hand is enough.
        if(i >= 2 && (p == glm::vec3{0.f} || glm::distance(p, move->hands[hand].pos) < 6.f))
        {
            probe = {};
            continue;
        }
        const int contents = worldtrace::contents(p);
        if(probe.valid)
        {
            handCrossing(ent, w, i, probe, p, contents, dt);
        }
        if(i < 2)
        {
            if(!worldtrace::isLiquid(contents))
            {
                w.wetSince[hand] = -1.0;
            }
            else if(w.wetSince[hand] < 0.0)
            {
                w.wetSince[hand] = now;
            }
        }
        probe = {p, contents, true};
    }

    wading(ent, w, dt);
}

} // namespace qvr::physics::detail

namespace
{

// A swimming stroke's sound, from the hand, once a stroke that passed its power gate, as the hand
// passes its fastest (VR_AfterWaterMove): each hand its own (vrfiringrange_2026-10-08_22-26-37), as
// loud as the stroke was fast, its pitch from its speed too. Near the surface, the recorded strokes
// (water thrown about) and a splash; deeper, water pushed aside (make_sounds.py's swim_soft, or swim_hard
// for a brisk stroke: recorded sloshes slowed, muffled and swelling in gently; 2026-10-10).
// The open hands' slaps and whooshes are not heard under water (QC vr_melee.qc VR_Melee_Slaps).
void strokeFeedback(edict_t* ent, int handIndex, const glm::vec3& hand, float peak)
{
    WaterFeel* w = feelOf(ent);
    // A hand just gone in (a slap at the surface) has its splash instead.
    const double wet = w ? w->wetSince[handIndex] : -1.0;
    if(!w || qcvm->time - w->stroke[handIndex] < 0.25 || wet < 0.0 || qcvm->time - wet < 0.15)
    {
        return;
    }
    w->stroke[handIndex] = qcvm->time;
    const float hard = CLAMP(0.f, (peak - 1.f) / 2.5f, 1.f);
    // Its pitch with its speed too: a slow sweep's water deeper, a brisk one's a little higher (and a little
    // apart each time).
    const float pitch = 0.88f + 0.2f * hard + 0.04f * (static_cast<float>(vrand::xorshift32(waterSoundState) % 1001u) / 1000.f - 0.5f);
    glm::vec3 at;
    if(surfaceOver(hand, 10.f, at))
    {
        soundAt(hand, variant(WaterSound::Stroke), 0.3f + 0.55f * hard, 1.f, pitch);
        sendSplash(at, glm::vec3{0.f, 0.f, 1.f}, 3.f + 4.f * hard);
        return;
    }
    soundAt(hand, variant(peak >= 2.2f ? WaterSound::SwimHard : WaterSound::SwimSoft), 0.25f + 0.65f * hard, 1.f, pitch);
}

// A thing's size for its splash: the half diagonal of its model (or its box).
[[nodiscard]] float thingRadius(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    glm::vec3 extent = vec(ent->v.maxs) - vec(ent->v.mins);
    if(model && model->type == mod_alias)
    {
        extent = vec(model->maxs) - vec(model->mins);
    }
    return glm::length(extent) * 0.5f;
}

// A thing (not a player) going into a liquid at `at` (entering) or out of it: a splash as hard as it
// goes and as big as it is, and going in, its sound in place of Quake's h2ohit1; coming out, a
// smaller one (Quake's sound stays). True if it played its own sound.
bool thingSplash(edict_t* ent, const glm::vec3& at, bool entering)
{
    const glm::vec3 vel = vec(ent->v.velocity);
    const float speed = glm::length(vel);
    const float size = CLAMP(0.35f, thingRadius(ent) / 8.f, 3.f);
    const float strength = CLAMP(2.f, 6.f * CLAMP(0.3f, speed / 400.f, 2.f) * size, 45.f);
    if(developer.value >= 2)
    {
        Con_Printf("VR splash: %s %s at %.0f u/s, radius %.0f\n", PR_GetString(ent->v.classname), entering ? "in" : "out", speed,
            thingRadius(ent));
    }
    if(!entering)
    {
        sendSplash(at, glm::vec3{0.f, 0.f, 1.f}, strength * 0.4f);
        return false;
    }
    if(!sendSplash(at, speed > 0.f ? vel / speed : glm::vec3{0.f, 0.f, -1.f}, strength))
    {
        return true; // one like it just here (a nailgun's nails): no sound either
    }
    if(thingRadius(ent) < 6.f) // a nail, a grenade: a shot's plip
    {
        return soundAt(at, variant(WaterSound::Plip), 0.6f);
    }
    return soundAt(at, variant(strength >= 18.f ? WaterSound::SplashBig : WaterSound::SplashSmall), CLAMP(0.35f, 0.3f + strength / 25.f, 1.f));
}

} // namespace

namespace qvr::physics
{

// Before a thing's move (vr_rigid.cpp): if it goes into a liquid on the way, its splash now, where
// it goes in. Quake only sees it in after the move, and a rocket, a nail or a grenade that also hits
// the bottom in that move is gone (or stopped) by then. The transition that follows is debounced.
void predictWaterEntry(edict_t* ent)
{
    if(!active() || f().lastwatertime < 0 || isClient(ent) || hasFlag(ent, FL_ONGROUND))
    {
        return;
    }
    const glm::vec3 vel = vec(ent->v.velocity);
    const glm::vec3 from = vec(ent->v.origin);
    if(glm::length(vel) < vr_water_splash_speed.value || worldtrace::contents(from) != CONTENTS_EMPTY)
    {
        return;
    }
    glm::vec3 at;
    if(!liquidEntry(from, from + vel * static_cast<float>(host_frametime), at))
    {
        return;
    }
    fieldFloat(ent, f().lastwatertime) = static_cast<float>(qcvm->time);
    thingSplash(ent, at, true);
}

namespace
{

// The world's leaves along a segment, front to back (liquidEntry): the first place where it goes from
// the open into a liquid, or out of one into the open.
struct LiquidWalk
{
    const hull_t* hull;
    glm::dvec3 from, delta;
    int last{0}; // the contents of the leaf before (0: none yet)
    bool found{false};
    double at{0.0}; // the crossing, as a fraction of the segment
};

void walkLeaves(LiquidWalk& w, int num, double f1, double f2)
{
    if(w.found)
    {
        return;
    }
    if(num < 0)
    {
        int c = num;
        if(worldtrace::isLiquid(c))
        {
            c = worldtrace::liquidKind(c); // (a current: water, as SV_PointContents)
            const glm::vec3 point = w.from + w.delta * ((f1 + f2) * 0.5);
            c = VR_LiquidContents(sv.worldmodel, &point.x, c);
        }
        if((worldtrace::isLiquid(c) && w.last == CONTENTS_EMPTY) || (c == CONTENTS_EMPTY && worldtrace::isLiquid(w.last)))
        {
            w.found = true;
            w.at = f1;
        }
        w.last = c;
        return;
    }
    const mclipnode_t& node = w.hull->clipnodes[num];
    const mplane_t& plane = w.hull->planes[node.planenum];
    const glm::dvec3 p1 = w.from + w.delta * f1;
    const glm::dvec3 p2 = w.from + w.delta * f2;
    const glm::dvec3 normal{plane.normal[0], plane.normal[1], plane.normal[2]};
    const double t1 = (plane.type < 3 ? p1[plane.type] : glm::dot(normal, p1)) - plane.dist;
    const double t2 = (plane.type < 3 ? p2[plane.type] : glm::dot(normal, p2)) - plane.dist;
    // SV_HullPointContents' sides: on the plane is in front.
    if(t1 >= 0.0 && t2 >= 0.0)
    {
        walkLeaves(w, node.children[0], f1, f2);
        return;
    }
    if(t1 < 0.0 && t2 < 0.0)
    {
        walkLeaves(w, node.children[1], f1, f2);
        return;
    }
    const double mid = f1 + (f2 - f1) * za::clamp(t1 / (t1 - t2), 0.0, 1.0);
    const int nearSide = t1 >= 0.0 ? 0 : 1;
    walkLeaves(w, node.children[nearSide], f1, mid);
    walkLeaves(w, node.children[1 - nearSide], mid, f2);
}

} // namespace

bool liquidEntry(const glm::vec3& from, const glm::vec3& to, glm::vec3& at)
{
    const glm::vec3 d = to - from;
    if(glm::length(d) < 0.01f || !sv.worldmodel)
    {
        return false;
    }
    // Down the world's BSP (a few dozen nodes for a shot's 2048 units), not point by point: QC asks
    // this for every pellet of every shot (weapons.qc VR_WaterShotSplash).
    LiquidWalk w{&sv.worldmodel->hulls[0], glm::dvec3{from}, glm::dvec3{d}};
    walkLeaves(w, w.hull->firstclipnode, 0.0, 1.0);
    if(w.found)
    {
        at = from + d * static_cast<float>(w.at);
    }
    return w.found;
}

void precacheWaterSounds()
{
    for(auto& indices : waterSoundIndices)
    {
        za::fill(indices, 0);
    }
    if(!active() || sv.state != ss_loading)
    {
        return;
    }
    for(int s = 0; s < static_cast<int>(WaterSound::Count); s++)
    {
        for(int k = 0; k < waterSoundFiles[s].count; k++)
        {
            // As PF_precache_sound (the names are literals: they outlive the server).
            const char* const name = waterSoundFiles[s].files[k];
            for(int i = 0; i < MAX_SOUNDS; i++)
            {
                if(!sv.sound_precache[i])
                {
                    sv.sound_precache[i] = name;
                }
                if(!ZA_STRCMP(sv.sound_precache[i], name))
                {
                    waterSoundIndices[s][k] = i;
                    break;
                }
            }
        }
    }
}

void waterSplash(const glm::vec3& at, const glm::vec3& dir, float strength, SplashSound sound)
{
    const float length = glm::length(dir);
    if(!sendSplash(at, length > 1e-3f ? dir / length : glm::vec3{0.f, 0.f, -1.f}, strength))
    {
        return; // one like it just here (a shotgun's pellets)
    }
    switch(sound)
    {
        case SplashSound::Shot: soundAt(at, variant(WaterSound::Plip), 0.6f); break;
        case SplashSound::Thing:
            soundAt(at, variant(strength >= 18.f ? WaterSound::SplashBig : WaterSound::SplashSmall), CLAMP(0.35f, 0.3f + strength / 25.f, 1.f));
            break;
        default: break;
    }
}

} // namespace qvr::physics

// ----------------------------------------------------------------------------
// Swimming (vr_swim)

namespace
{

// Metres per second beyond vr_swim_stroke_min at which a stroke pushes as vr_swim_stroke says
// (slower less, faster more: the push grows with the square of the speed, vr_swim_speed_exp).
constexpr float strokeSpeed = 1.2f;

// A stroke ends where the hand turns by more than this from the way it was going (cos 75°): a
// frog stroke's sweep, curving out and back, stays one stroke; the turn to come back starts another.
constexpr float strokeTurnCos = 0.26f;
// How quickly (seconds) the stroke's way follows a curving hand.
constexpr float strokeFollow = 0.08f;
// A stroke is heard as its hand slows below this share of its peak speed (just past its fastest).
constexpr float strokeHeardPast = 0.92f;
// Against the remembered stroke, a stroke as fast (over its peak) as vr_swim_reverse_speed is not
// damped at all, and one this much slower than that is damped fully: the relaxed return is slower;
// a deliberate reverse stroke, not.
constexpr float reverseWeakRange = 0.3f;
// Faster than this (m/s), a hand is taken for a tracking jump, not a stroke.
constexpr float glitchSpeed = 8.f;
// Stroke Against Palm (vr_swim_against_palm): a hand moving within 60 degrees of where its palm faces (the palm
// leading) pushes fully, one moving within 60 degrees of where the back of the hand faces pushes by the setting, and
// between (the edge leading) it blends smoothly: over the cosine from this to minus this.
constexpr float palmLeadBlend = 0.5f;

// One hand's stroke: its motion from where it started (or turned) to where it stops or turns.
struct Stroke
{
    bool active{false};
    glm::vec3 way{0.f};   // where the hand goes (followed as it curves)
    glm::vec3 moved{0.f}; // the hand's motion summed (speed-weighted directions)
    float peak{0.f};      // m/s
    float factor{0.f};    // the power gate times the reverse damping reached (vr_swim_power_whole)
    glm::vec3 raw{0.f};   // the push before them
    glm::vec3 given{0.f}; // and given
    float ahead{0.f};     // the part of it along where you look (vr_swim_debug)
    float flat{0.f};      // how flat the hand went, speed-weighted (vr_swim_debug)
    float lead{0.f};      // how much the palm led (1) or the back of the hand (-1), speed-weighted (vr_swim_debug)
    float counted{0.f};   // Stroke Against Palm's weight, speed-weighted: how much of the stroke counted
    float weight{0.f};
    bool gated{false};    // its power gate opened (half way): it is heard (strokeFeedback)
    bool heard{false};    // its sound played (as the hand passed its fastest, past the gate)
    glm::vec3 at{0.f};    // where the hand was last in the water
};

// One hand's last full stroke (vr_swim_intent_memory).
struct Intent
{
    bool valid{false};
    glm::vec3 way{0.f};
    float peak{0.f};
    double time{0.0};
};

struct SwimHand
{
    Stroke stroke;
    Intent intent;
};

struct Swimmer
{
    double time{-1.0};
    SwimHand hands[2];
};

Swimmer swimmers[MAX_SCOREBOARD];

// A VR player's latest move, when its hands are tracked.
[[nodiscard]] const VrMove* swimmer(edict_t* ent)
{
    if(!vr_swim.value || static_cast<int>(ent->v.movetype) == MOVETYPE_NOCLIP)
    {
        return nullptr;
    }
    const VrMove* move = server::clientMove(ent);
    return move && (move->buttons & protocol::QVR_BUTTON_HANDSTRACKED) ? move : nullptr;
}

// A stroke is over (the hand stopped, turned, or left the water): a full one -- it passed the power
// threshold and was not damped as a return, nor led by the back of the hand (Stroke Against Palm) -- becomes
// the hand's intent: a backhand that hardly pushed does not make the next real stroke, against it, a "return".
// A stroke past its gate not heard yet (it ended before slowing) is heard now, where the hand last was in the water.
void endStroke(edict_t* ent, SwimHand& h, int hand, double now)
{
    Stroke& s = h.stroke;
    if(!s.active)
    {
        return;
    }
    if(s.gated && !s.heard)
    {
        strokeFeedback(ent, hand, s.at, s.peak);
    }
    const float counted = s.weight > 0.f ? s.counted / s.weight : 1.f;
    if(vr_swim_debug.value && glm::length(s.raw) > 0.f)
    {
        Con_Printf("swim %s: peak %.2f m/s, flat %.2f, palm lead %+.2f (x%.2f), power x%.2f, push %.0f of %.0f (%+.0f ahead)\n",
            hand ? "main" : "off", s.peak, s.weight > 0.f ? s.flat / s.weight : 0.f, s.weight > 0.f ? s.lead / s.weight : 0.f,
            counted, s.factor, glm::length(s.given), glm::length(s.raw), s.ahead);
    }
    if(s.factor >= 0.5f && counted >= 0.5f && glm::length(s.moved) > 0.f)
    {
        h.intent = {true, glm::normalize(s.moved), s.peak, now};
    }
    s = {};
}

} // namespace

// SV_ClientThink, before the move: in water the stick moves you slower. Feet in the water, a
// little (vr_swim_shallow_speed); waist deep standing on the bottom, more (vr_swim_wade_speed);
// swimming (under, or off the bottom), it barely does (vr_swim_stick_speed): the hands do.
extern "C" float VR_WaterStickScale(edict_t* ent, int swimming)
{
    if(!swimmer(ent) || ent->v.waterlevel < 1.f)
    {
        return 1.f;
    }
    const bool onGround = (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
    float scale = vr_swim_shallow_speed.value;
    if(swimming)
    {
        scale = ent->v.waterlevel >= 3.f || !onGround ? vr_swim_stick_speed.value : vr_swim_wade_speed.value;
    }
    return CLAMP(0.f, scale, 1.f);
}

// After SV_WaterMove: each hand under water pushes the water, and the body goes the other way:
// the push comes from the hand's speed through the water (relative to the body, beyond
// vr_swim_stroke_min) times vr_swim_stroke, more when the palm (or the back of the hand) meets it
// flat than at an angle (vr_swim_palm). That flatness is the same for either side of the hand: which side
// leads is Stroke Against Palm's (vr_swim_against_palm): a stroke pushes with the palm, the hand going where
// the palm faces; with the back of the hand leading (a backhand, a hand turned round to reposition) it pushes
// only by vr_swim_against_palm, blended smoothly through the edge-on hand (palmLeadBlend).
// A hand going edge first -- the recovery, bringing it back for the next stroke -- hardly pushes
// (vr_swim_recovery), and the push grows with the square of the hand's speed, as water's drag:
// the stroke, flat and brisk, outdoes the return. So the stroke decides the way, whichever way:
// a frog stroke (hands from ahead out to the sides and back) swims you forward, the reverse one
// (hands from the sides forward to meet ahead) backward.
// You swim where you look: the part of a push along where the head looks (ahead or back) counts
// fully, a part sideways of it less (vr_swim_look). To rise, look up and push down.
// Strokes towards where the stick points push more, and against it less (vr_swim_stroke_assist),
// so that the stick steers the swimming too.
// Off the bottom you glide between strokes: part of the water friction SV_WaterMove just applied
// is given back (vr_swim_glide).
// Intent: each hand's motion is cut into strokes (from where it starts or turns back to where it
// stops or turns again). A stroke propels only if its peak speed passes vr_swim_power_threshold,
// fading in over vr_swim_power_knee above it, and then all of it counts (vr_swim_power_whole): a
// brisk stroke swims, a relaxed return of the arms does nothing. And each hand remembers its last
// full stroke for vr_swim_intent_memory seconds: a stroke against it, slower than it, is damped by
// vr_swim_reverse_damp (a deliberate reverse stroke, as brisk -- vr_swim_reverse_speed -- is not).
// vr_swim_debug 1 prints each stroke (peak speed, flatness, power, push) to tune these by.
extern "C" cvar_t sv_friction; // sv_phys.c

namespace
{
// A stroke's push tilted up by `pitch` radians (negative: down) in its own vertical plane, its size kept: the last
// adjustment of each (vr_swim_stroke_pitch; "every stroke brings me upward a bit too much", NOTES.md
// vrstart_2026-10-02_15-17-43). A push straight up or down turns towards where you look (up: back over the top as it
// rises further; down: ahead), as if it were a hair that way.
[[nodiscard]] glm::vec3 pitchStroke(const glm::vec3& push, const glm::vec3& look, float pitch)
{
    if(pitch == 0.f)
    {
        return push;
    }
    glm::vec2 flat{push.x, push.y};
    if(glm::length(flat) < 1e-4f * glm::length(push))
    {
        flat = glm::vec2{look.x, look.y};
    }
    if(glm::length(flat) < 1e-6f)
    {
        return push; // (no way to tilt it: nothing pushed, or looking straight up or down at a vertical push)
    }
    // About the horizontal axis across it: Rodrigues' turn of a vector square to its axis.
    const glm::vec3 axis = glm::normalize(glm::cross(glm::vec3{flat, 0.f}, glm::vec3{0.f, 0.f, 1.f}));
    return push * za::cos(pitch) + glm::cross(axis, push) * za::sin(pitch);
}
} // namespace

extern "C" void VR_AfterWaterMove(edict_t* ent, float forwardmove, float sidemove, float upmove)
{
    QVR_PROFILE("vr swim");
    const VrMove* move = swimmer(ent);
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(!move || client < 0 || client >= za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
    {
        return;
    }
    Swimmer& sw = swimmers[client];
    const double now = qcvm->time;
    if(now < sw.time || now - sw.time > 0.5) // a new map, or back in the water: start afresh
    {
        sw = {};
    }
    sw.time = now;

    const float dt = static_cast<float>(host_frametime);
    const float minSpeed = za::max(0.f, vr_swim_stroke_min.value); // m/s
    const float palmWeight = CLAMP(0.f, vr_swim_palm.value, 1.f);
    const float sideKept = 1.f - CLAMP(0.f, vr_swim_look.value, 1.f);
    const float recovery = CLAMP(0.f, vr_swim_recovery.value, 1.f);
    const float flatExp = CLAMP(0.25f, vr_swim_flat_exp.value, 4.f);
    const float speedExp = CLAMP(0.5f, vr_swim_speed_exp.value, 3.f);
    const float palmDir = CLAMP(0.f, vr_swim_palm_dir.value, 1.f);
    const float againstPalm = CLAMP(0.f, vr_swim_against_palm.value, 1.f);
    const float powerMin = za::max(0.f, vr_swim_power_threshold.value);
    const float powerKnee = za::max(0.f, vr_swim_power_knee.value);
    const bool whole = vr_swim_power_whole.value != 0.f;
    const float memory = za::max(0.f, vr_swim_intent_memory.value);
    const float reverseDamp = CLAMP(0.f, vr_swim_reverse_damp.value, 1.f);
    const float reverseSpeed = CLAMP(reverseWeakRange, vr_swim_reverse_speed.value, 3.f);
    const float strokePitch = glm::radians(CLAMP(-90.f, vr_swim_stroke_pitch.value, 90.f));
    glm::vec3 vel = vec(ent->v.velocity);

    // The glide: SV_WaterMove's friction took dt * sv_friction of the speed; give part of it back.
    const float glide = CLAMP(0.f, vr_swim_glide.value, 1.f);
    if(glide > 0.f && !(static_cast<int>(ent->v.flags) & FL_ONGROUND))
    {
        const float lost = dt * sv_friction.value;
        if(lost > 0.f && lost < 0.9f)
        {
            vel *= (1.f - lost * (1.f - glide)) / (1.f - lost);
        }
    }

    // Where the head looks, and where the stick asks to go (as SV_WaterMove steers it).
    glm::vec3 look{0.f};
    glm::vec3 wish{0.f};
    {
        vec3_t fwd, right, up;
        AngleVectors(VR_MoveAngles(ent, ent->v.v_angle), fwd, right, up);
        for(int k = 0; k < 3; k++)
        {
            look[k] = fwd[k];
            wish[k] = fwd[k] * forwardmove + right[k] * sidemove;
        }
        wish.z += upmove;
    }
    const float wishLen = glm::length(wish);
    const glm::vec3 wishDir = wishLen > 1.f ? wish / wishLen : glm::vec3{0.f};
    const float assist = CLAMP(0.f, vr_swim_stroke_assist.value, 1.f);
    for(int h = 0; h < 2; h++)
    {
        const VrHandMove& hand = move->hands[h];
        SwimHand& state = sw.hands[h];
        Stroke& stroke = state.stroke;
        vec3_t p{hand.pos.x, hand.pos.y, hand.pos.z};
        const float speedMs = glm::length(hand.vel); // the move's hand velocities are in m/s
        if(!(speedMs <= glitchSpeed)) // (a NaN turned away too)
        {
            continue;
        }
        if(!worldtrace::isLiquid(SV_PointContents(p)) || !(speedMs > minSpeed)) // out of the water (or slime, lava); still
        {
            endStroke(ent, state, h, now);
            continue;
        }
        const glm::vec3 dir = hand.vel / speedMs;

        // The stroke: it goes on while the hand keeps its way (curving), and ends where it turns.
        if(stroke.active && glm::dot(dir, stroke.way) < strokeTurnCos)
        {
            endStroke(ent, state, h, now);
        }
        if(!stroke.active)
        {
            stroke.active = true;
            stroke.way = dir;
        }
        else
        {
            const glm::vec3 way = glm::mix(stroke.way, dir, za::min(1.f, dt / strokeFollow));
            stroke.way = glm::length(way) > 0.f ? glm::normalize(way) : dir;
        }
        stroke.moved += hand.vel * dt;
        stroke.peak = za::max(stroke.peak, speedMs);
        stroke.at = hand.pos;

        // The palm (and the back of the hand) faces the hand's side: how flat the hand meets the
        // water. Edge first it slices through (the recovery) and hardly pushes.
        vec3_t a{hand.rot.x, hand.rot.y, hand.rot.z}, f, r, u;
        AngleVectors(a, f, r, u);
        const glm::vec3 side{r[0], r[1], r[2]};
        const float facing = glm::dot(side, dir);
        const float flat = za::abs(facing);
        const float palm = (1.f - palmWeight) + palmWeight * za::pow(flat, flatExp);
        const float edge = recovery + (1.f - recovery) * glm::smoothstep(0.1f, 0.45f, flat);

        // Which side leads. The palm's normal is the drawn hand's: the calibrated hand's (hand.rot: Gun Angle and
        // the Hand Calibration's turn) side, its left for a right hand and its right for a left hand (drawn
        // mirrored), as the jointed hand's palm faces (checked in-game against its drawn rig, ROUND21.md). The
        // physical hand: hands[0] is the left controller (HAND_OFF, left-handed too).
        const bool leftHand = h == 0;
        const float lead = leftHand ? facing : -facing; // 1: the palm leads; -1: the back of the hand
        const float against = glm::mix(againstPalm, 1.f, glm::smoothstep(-palmLeadBlend, palmLeadBlend, lead));

        // As water's drag, the push grows with the square of the speed (vr_swim_speed_exp; as the
        // linear push at a brisk stroke): the stroke, faster, outdoes the return for the next one.
        const float beyond = (speedMs - minSpeed) * units::metresToUnits();
        const float strength = beyond * za::pow(beyond / (strokeSpeed * units::metresToUnits()), speedExp - 1.f);

        // The body goes against the hand (or, vr_swim_palm_dir, away from where the palm faces).
        glm::vec3 push = -dir;
        if(palmDir > 0.f)
        {
            const glm::vec3 blended = glm::mix(-dir, side * (facing < 0.f ? 1.f : -1.f), palmDir);
            push = glm::length(blended) > 0.001f ? glm::normalize(blended) : -dir;
        }
        const float along = glm::dot(push, wishDir); // 1: the push goes where the stick points
        const float steer = za::max(0.f, 1.f + assist * along);

        // Along where you look (ahead or back) it counts fully, sideways of it less.
        const float ahead = glm::dot(push, look);
        const glm::vec3 biased = look * ahead + (push - look * ahead) * sideKept;
        const glm::vec3 raw = biased * (strength * vr_swim_stroke.value * palm * edge * against * steer * dt);

        // Intent: the stroke's power gate, from its peak speed so far...
        float factor = 1.f;
        if(powerKnee > 0.f)
        {
            factor = glm::smoothstep(powerMin, powerMin + powerKnee, stroke.peak);
        }
        else if(stroke.peak < powerMin)
        {
            factor = 0.f;
        }
        // ...and against the hand's last full stroke, slower than it: the return, damped.
        const Intent& intent = state.intent;
        if(intent.valid && memory > 0.f && now - intent.time < memory && intent.peak > 0.f)
        {
            const float against = CLAMP(0.f, -glm::dot(stroke.way, intent.way), 1.f);
            const float fade = 1.f - glm::smoothstep(0.5f, 1.f, static_cast<float>(now - intent.time) / memory);
            const float weaker = 1.f - glm::smoothstep(reverseSpeed - reverseWeakRange, reverseSpeed, stroke.peak / intent.peak);
            factor *= 1.f - reverseDamp * against * fade * weaker;
        }

        // Only from the moment the gate opens, or (whole) the stroke so far counts as it opens.
        glm::vec3 give = raw * factor;
        if(whole && factor > stroke.factor)
        {
            give += stroke.raw * (factor - stroke.factor);
        }
        give = pitchStroke(give, look, strokePitch);
        stroke.factor = whole ? za::max(stroke.factor, factor) : factor;
        // Its sound (and a splash at the surface), as loud as the stroke is fast: once past the gate, as the
        // hand passes its fastest.
        stroke.gated = stroke.gated || factor >= 0.5f;
        if(stroke.gated && !stroke.heard && speedMs < strokeHeardPast * stroke.peak)
        {
            stroke.heard = true;
            strokeFeedback(ent, h, hand.pos, stroke.peak);
        }
        stroke.raw += raw;
        stroke.given += give;
        stroke.ahead += glm::dot(give, look);
        stroke.flat += flat * speedMs;
        stroke.lead += lead * speedMs;
        stroke.counted += against * speedMs;
        stroke.weight += speedMs;
        vel += give;
    }

    const float maxSpeed = za::max(0.f, vr_swim_max_speed.value);
    const float len = glm::length(vel);
    if(!ZA_ISFINITE(len))
    {
        return; // (a hand's figures gone wrong: SV_WaterMove's velocity stands)
    }
    if(len > maxSpeed && len > 0.f)
    {
        vel *= maxSpeed / len;
    }
    ent->v.velocity[0] = vel.x;
    ent->v.velocity[1] = vel.y;
    ent->v.velocity[2] = vel.z;
}

// Debounces splashes from hands and room-scale bodies bobbing at the water surface.
extern "C" int VR_AllowWaterSplash(edict_t* ent)
{
    // Things floating and bobbing at the surface cross it all the time: only a thing moving fast
    // enough splashes (players always do).
    if(developer.value >= 3)
    {
        Con_Printf("VR water transition: %s, %.0f u/s\n", PR_GetString(ent->v.classname), VectorLength(ent->v.velocity));
    }
    if(!isClient(ent))
    {
        const float speed = static_cast<float>(VectorLength(ent->v.velocity));
        if(speed < vr_water_splash_speed.value)
        {
            return 0;
        }
    }

    if(!active() || f().lastwatertime < 0)
    {
        return 1;
    }

    float& last = fieldFloat(ent, f().lastwatertime);
    const bool allow = qcvm->time - last > 0.2;
    last = static_cast<float>(qcvm->time);
    if(!allow || isClient(ent))
    {
        return allow;
    }

    // A thing going in (the watertype is still the old one) or coming out (see thingSplash).
    glm::vec3 at;
    const bool entering = ent->v.watertype == CONTENTS_EMPTY;
    const float speed = glm::length(vec(ent->v.velocity));
    if(!surfaceOver(vec(ent->v.origin), za::max(48.f, speed * static_cast<float>(host_frametime) * 1.5f), at))
    {
        return 1;
    }
    return !thingSplash(ent, at, entering);
}
