// vr_gunsfx.cpp -- see vr_gunsfx.hpp.

#include "vr_gunsfx.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_debris.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_physsound.hpp"
#include "vr_random.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_weaponfx.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"

#include <string.h>

extern "C" int VR_SndShot(int entnum, int entchannel, sfx_t* sfx, const float* origin, float* fvol, float attenuation,
    float* pitch);

namespace qvr::gunsfx
{
namespace
{

using sndlayer::LayeredSound;
using sndlayer::SoundFx;
using weapons::Key;

// ---------------------------------------------------------------------------------------------------------------------
// The layers (made as they load: vr_sndlayer.cpp). Thumps: the shot itself an octave down and low-passed; mechanical
// layers: our own action sounds, faster and cut short; tails: the shot's first crack in a room (small: short and close,
// large: long, outdoors: dull, sparse, with a distant echo); casings: the tinks brighter with a metal ring, or duller
// with a wooden knock.

enum Kind : int
{
    KindShot,  // shotguns, the revolver, the grunts' shotguns
    KindLight, // nails, the enforcers' lasers, the plasma gun
    KindHeavy, // rockets and grenades
    KindCount
};

enum Room : int
{
    RoomSmall,
    RoomLarge,
    RoomOutdoor,
    RoomCount
};

constexpr const char* roomNames[RoomCount] = {"small", "large", "outdoor"};

constexpr const char* thumpNames[KindCount] = {
    "vr/derived/gun_thump_shot.wav", "vr/derived/gun_thump_light.wav", "vr/derived/gun_thump_heavy.wav"};

constexpr const char* mechHammer = "vr/derived/gun_mech_hammer.wav"; // a hammer falling, a trigger's snap
constexpr const char* mechTick = "vr/derived/gun_mech_tick.wav";     // a feed's tick
constexpr const char* mechTube = "vr/derived/gun_mech_tube.wav";     // a launcher's clunk

constexpr const char* tailNames[KindCount][RoomCount] = {
    {"vr/derived/gun_tail_shot_small.wav", "vr/derived/gun_tail_shot_large.wav", "vr/derived/gun_tail_shot_out.wav"},
    {"vr/derived/gun_tail_light_small.wav", "vr/derived/gun_tail_light_large.wav", "vr/derived/gun_tail_light_out.wav"},
    {"vr/derived/gun_tail_heavy_small.wav", "vr/derived/gun_tail_heavy_large.wav", "vr/derived/gun_tail_heavy_out.wav"},
};

constexpr const char* metalCasings[3] = {
    "vr/derived/shell_metal1.wav", "vr/derived/shell_metal2.wav", "vr/derived/shell_metal3.wav"};
constexpr const char* woodCasings[3] = {
    "vr/derived/shell_wood1.wav", "vr/derived/shell_wood2.wav", "vr/derived/shell_wood3.wav"};

constexpr SoundFx thumpFx[KindCount] = {{160.f, 0.f, 0.f, 0.f, 0.9f}, {200.f, 0.f, 0.f, 0.f, 0.85f}, {130.f, 0.f, 0.f, 0.f, 0.95f}};
constexpr SoundFx roomFx[RoomCount] = {
    {0.f, 0.35f, 0.004f, 0.f, 0.8f}, // small: a third of a second, the walls right there
    {0.f, 1.4f, 0.02f, 0.f, 0.8f},   // large: a hall's long ring
    {3000.f, 0.8f, 0.035f, 0.42f, 0.8f}, // outdoors: duller, sparser, and a far wall's echo (twice)
};

constexpr LayeredSound gunLayers[] = {
    {thumpNames[KindShot], {{"weapons/shotgn2.wav", 0.5f, 1.f, 0.f, 0.35f, 0.2f}}, 1.4f, thumpFx[KindShot]},
    {thumpNames[KindLight], {{"weapons/rocket1i.wav", 0.5f, 1.f, 0.f, 0.18f, 0.1f}}, 1.2f, thumpFx[KindLight]},
    {thumpNames[KindHeavy],
        {{"weapons/r_exp3.wav", 0.55f, 1.f, 0.f, 0.45f, 0.3f}, {"weapons/sgun1.wav", 0.6f, 0.6f, 0.f, 0.3f, 0.2f}}, 1.6f,
        thumpFx[KindHeavy]},
    {mechHammer, {{"vr/reload_ssg_close.wav", 1.5f, 1.f, 0.f, 0.07f, 0.03f}}, 0.6f, {0.f, 0.f, 0.f, 0.f, 0.9f}},
    {mechTick, {{"vr/reload_mag_in.wav", 1.7f, 1.f, 0.f, 0.05f, 0.02f}}, 0.5f, {0.f, 0.f, 0.f, 0.f, 0.8f}},
    {mechTube, {{"vr/reload_ssg_open.wav", 0.8f, 1.f, 0.f, 0.12f, 0.05f}}, 0.8f, {0.f, 0.f, 0.f, 0.f, 0.9f}},

    {tailNames[KindShot][RoomSmall], {{"weapons/shotgn2.wav", 1.f, 1.f, 0.f, 0.25f, 0.1f}}, 0.3f, roomFx[RoomSmall]},
    {tailNames[KindShot][RoomLarge], {{"weapons/shotgn2.wav", 1.f, 1.f, 0.f, 0.25f, 0.1f}}, 0.3f, roomFx[RoomLarge]},
    {tailNames[KindShot][RoomOutdoor], {{"weapons/shotgn2.wav", 1.f, 1.f, 0.f, 0.25f, 0.1f}}, 0.3f, roomFx[RoomOutdoor]},
    {tailNames[KindLight][RoomSmall], {{"weapons/spike2.wav", 1.f, 1.f, 0.f, 0.12f, 0.05f}}, 0.3f, roomFx[RoomSmall]},
    {tailNames[KindLight][RoomLarge], {{"weapons/spike2.wav", 1.f, 1.f, 0.f, 0.12f, 0.05f}}, 0.3f, roomFx[RoomLarge]},
    {tailNames[KindLight][RoomOutdoor], {{"weapons/spike2.wav", 1.f, 1.f, 0.f, 0.12f, 0.05f}}, 0.3f, roomFx[RoomOutdoor]},
    {tailNames[KindHeavy][RoomSmall],
        {{"weapons/sgun1.wav", 0.85f, 1.f, 0.f, 0.3f, 0.12f}, {"weapons/r_exp3.wav", 1.1f, 0.5f, 0.f, 0.2f, 0.1f}}, 0.3f,
        roomFx[RoomSmall]},
    {tailNames[KindHeavy][RoomLarge],
        {{"weapons/sgun1.wav", 0.85f, 1.f, 0.f, 0.3f, 0.12f}, {"weapons/r_exp3.wav", 1.1f, 0.5f, 0.f, 0.2f, 0.1f}}, 0.3f,
        roomFx[RoomLarge]},
    {tailNames[KindHeavy][RoomOutdoor],
        {{"weapons/sgun1.wav", 0.85f, 1.f, 0.f, 0.3f, 0.12f}, {"weapons/r_exp3.wav", 1.1f, 0.5f, 0.f, 0.2f, 0.1f}}, 0.3f,
        roomFx[RoomOutdoor]},

    {metalCasings[0], {{"vr/shell_tink1.wav", 1.12f, 0.9f}, {"vr/phys/metal_h1.wav", 1.8f, 0.4f, 0.f, 0.25f, 0.15f}}, 0.5f},
    {metalCasings[1], {{"vr/shell_tink2.wav", 1.12f, 0.9f}, {"vr/phys/metal_h2.wav", 1.8f, 0.4f, 0.f, 0.25f, 0.15f}}, 0.5f},
    {metalCasings[2], {{"vr/shell_tink3.wav", 1.12f, 0.9f}, {"vr/phys/metal_h3.wav", 1.8f, 0.4f, 0.f, 0.25f, 0.15f}}, 0.5f},
    {woodCasings[0],
        {{"vr/shell_tink1.wav", 0.7f, 0.6f, 0.f, 0.06f, 0.04f}, {"vr/phys/wood_h1.wav", 1.6f, 0.7f, 0.f, 0.12f, 0.07f}}, 0.5f},
    {woodCasings[1],
        {{"vr/shell_tink2.wav", 0.7f, 0.6f, 0.f, 0.06f, 0.04f}, {"vr/phys/wood_h2.wav", 1.6f, 0.7f, 0.f, 0.12f, 0.07f}}, 0.5f},
    {woodCasings[2],
        {{"vr/shell_tink3.wav", 0.7f, 0.6f, 0.f, 0.06f, 0.04f}, {"vr/phys/wood_h3.wav", 1.6f, 0.7f, 0.f, 0.12f, 0.07f}}, 0.5f},
};

// The shots: a gun's sound on a weapon channel (QC CHAN_WEAPON, CHAN_WEAPON2), whoever fires it. Its mechanical layer
// (none where the shot has its own action in it or the gun has its own: the shotgun's auto pump) and how loud each
// layer is against the shot. Not the lightning gun (a beam, no report).
struct ShotSound
{
    const char* name;
    Kind kind;
    const char* mech;
    float mechGain;
    float thumpGain;
    float tailGain;
};

constexpr ShotSound shots[] = {
    {"weapons/guncock.wav", KindShot, nullptr, 0.f, 0.8f, 0.9f}, // the shotgun (its pump's clacks: vr_autopump.cpp)
    {"weapons/shotgn2.wav", KindShot, mechHammer, 0.55f, 1.f, 1.f}, // the super shotgun
    {"vr/derived/revolver_shot.wav", KindShot, nullptr, 0.f, 0.9f, 1.f}, // (its action's clack is in it)
    {"soldier/sattck1.wav", KindShot, mechHammer, 0.4f, 0.7f, 0.8f}, // a grunt's shotgun, or yours
    {"vr/widowmaker_shot.wav", KindShot, nullptr, 0.f, 0.8f, 0.9f},  // AD support: the Widowmaker
    {"weapons/rocket1i.wav", KindLight, mechTick, 0.4f, 0.55f, 0.6f}, // the nailgun
    {"weapons/spike2.wav", KindLight, mechTick, 0.45f, 0.6f, 0.65f},  // the super nailgun
    {"vr/nailpistol_shot.wav", KindLight, nullptr, 0.f, 0.5f, 0.6f},  // (its slide's clack is in it)
    {"enforcer/enfire.wav", KindLight, nullptr, 0.f, 0.45f, 0.6f},    // an enforcer's laser, or yours
    {"vr/derived/plasma_fire.wav", KindLight, nullptr, 0.f, 0.5f, 0.7f},
    {"weapons/sgun1.wav", KindHeavy, mechTube, 0.5f, 1.f, 1.f},      // the rocket launcher (and the monsters')
    {"weapons/grenade.wav", KindHeavy, mechTube, 0.45f, 0.85f, 0.9f}, // the grenade launcher, an ogre's
};

constexpr const char* dryClickName = "gunclick.wav";

// An action's sound heard from the action: how far along the gun from the hand to the muzzle.
struct ActionSound
{
    const char* name;
    float place;
};

constexpr ActionSound actions[] = {
    {"vr/reload_ssg_open.wav", 0.4f},  // the barrels' hinge (the revolver's crane)
    {"vr/reload_ssg_close.wav", 0.4f},
    {"vr/reload_mag_in.wav", 0.15f},   // the magazine well, just ahead of the grip
    {"vr/reload_mag_out.wav", 0.15f},
    {"vr/reload_shell_in.wav", 0.3f},  // the loading port
    {"vr/reload_shell_in_2.wav", 0.3f},
    {"vr/reload_shell_in_3.wav", 0.3f},
    {"vr/reload_blocked.wav", 0.3f},
};

// ---------------------------------------------------------------------------------------------------------------------
// State.

constexpr int maxRecent = 8;
constexpr float spikeTime = 0.012f; // s: the punch's spike
constexpr float spikeHz = 200.f;
constexpr float tailHz = 70.f;
constexpr float tailShare = 0.5f; // the tail starts at half the spike's strength

struct RoomProbe
{
    float meanDist{0.f}; // units: the horizontal rays' mean length (capped)
    float open{0.f};     // 0..1: the upward rays' share that reached the sky (or nothing)
    Room room{RoomSmall};
};

struct Recent // an entity's last room and tail
{
    int ent{-1};
    glm::vec3 at{0.f};
    double probed{-1e9};
    RoomProbe probe{};
    double lastTail{-1e9};
};

struct LastShot // vr_gunsfx_test: what the last shot did
{
    const char* shot{nullptr};
    const char* mech{nullptr};
    const char* thump{nullptr};
    const char* tail{nullptr};
    RoomProbe probe{};
    float pitch{1.f};
    float volume{1.f};
    int count{0};
};

struct LastDry
{
    int hand{-1};
    bool silenced{false};
    int count{0};
};

struct Pulse
{
    float seconds{0.f};
    float amplitude{0.f};
    float hz{0.f};
    int count{0};
};

struct Punch
{
    double start{-1.0};
    int slot{-1};
    float share{1.f}; // the recoil's share (weaponfx::twoHandRecoil: less held in both hands)
};

struct GunSfxState
{
    za::U32 seed{0x9e3779b9u};
    bool nested{false}; // (a layer's own S_StartSoundPitch)
    Recent recent[maxRecent]{};
    int nextRecent{0};
    LastShot lastShot{};
    LastDry lastDry{};
    Pulse lastPulse[HAND_COUNT]{};
    Punch punches[HAND_COUNT]{};
};
GunSfxState state;

[[nodiscard]] bool on()
{
    return vr_gunsfx.value != 0.f;
}

[[nodiscard]] float rand11()
{
    return vrand::lcg01(state.seed) * 2.f - 1.f;
}

void pulse(int hand, float seconds, float amplitude, float hz)
{
    Pulse& p = state.lastPulse[hand];
    p = {seconds, amplitude, hz, p.count + 1};
    haptics::pulse(hand, seconds, amplitude, hz);
}

[[nodiscard]] const ShotSound* shotOf(const char* name)
{
    for(const ShotSound& s : shots)
    {
        if(!q_strcasecmp(s.name, name))
        {
            return &s;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// The room round a shot: eight rays round it level, four up at 50 degrees and one straight up. Under the sky (two of
// the five upward ones reaching it, or nothing): outdoors; else small or large by how far the level ones reach.

constexpr float probeReach = 1536.f;
constexpr float smallRoom = 320.f; // units: the level rays' mean under it: a small room

[[nodiscard]] Room classify(float meanDist, float open)
{
    if(open >= 0.4f)
    {
        return RoomOutdoor;
    }
    return meanDist < smallRoom ? RoomSmall : RoomLarge;
}

[[nodiscard]] bool hitsSky(const trace_t& tr, const glm::vec3& dir)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    const glm::vec3 end = worldtrace::endPos(tr);
    const msurface_t* s = physsound::surfaceOnSegment(cl.worldmodel, end - dir * 2.f, end + dir * 8.f);
    return s && (s->flags & SURF_DRAWSKY);
}

[[nodiscard]] RoomProbe probeRoom(const glm::vec3& at)
{
    RoomProbe r;
    float sum = 0.f;
    int open = 0;
    for(int i = 0; i < 13; i++)
    {
        glm::vec3 dir{0.f, 0.f, 1.f};
        if(i < 8)
        {
            const float a = static_cast<float>(i) * 0.785398f;
            dir = {za::cos(a), za::sin(a), 0.f};
        }
        else if(i < 12)
        {
            const float a = (static_cast<float>(i - 8) + 0.5f) * 1.570796f;
            dir = {0.643f * za::cos(a), 0.643f * za::sin(a), 0.766f};
        }
        const float reach = i < 8 ? probeReach : probeReach * 0.67f;
        const trace_t tr = worldtrace::world(at, at + dir * reach, false);
        const bool far = tr.fraction >= 1.f || tr.allsolid;
        if(i < 8)
        {
            sum += far ? reach : tr.fraction * reach;
        }
        else if(far || hitsSky(tr, dir))
        {
            open++;
        }
    }
    r.meanDist = sum / 8.f;
    r.open = static_cast<float>(open) / 5.f;
    r.room = classify(r.meanDist, r.open);
    return r;
}

// The entity's room: probed again once it has moved 48 units or after 0.75 s.
[[nodiscard]] Recent& recentFor(int ent, const glm::vec3& at)
{
    for(Recent& r : state.recent)
    {
        if(r.ent == ent)
        {
            if(cl.time - r.probed > 0.75 || cl.time < r.probed || glm::length(at - r.at) > 48.f)
            {
                r.at = at;
                r.probed = cl.time;
                r.probe = probeRoom(at);
            }
            return r;
        }
    }
    Recent& r = state.recent[state.nextRecent];
    state.nextRecent = (state.nextRecent + 1) % maxRecent;
    r = Recent{ent, at, cl.time, probeRoom(at), -1e9};
    return r;
}

void startLayer(const char* name, int ent, int chan, const float* origin, float vol, float atten, float pitch)
{
    sfx_t* sfx = S_PrecacheSound(name);
    if(!sfx || vol <= 0.f)
    {
        return;
    }
    vec3_t org{origin[0], origin[1], origin[2]};
    state.nested = true;
    S_StartSoundPitch(ent, chan, sfx, org, za::min(vol, 2.f), atten, pitch);
    state.nested = false;
}

// A shot: varied, and its layers started with it.
void shot(const ShotSound& s, int entnum, int entchannel, const float* origin, float* fvol, float attenuation, float* pitch)
{
    const float pct = za::clamp(vr_gunsfx_vary.value, 0.f, 15.f) * 0.01f;
    if(pct > 0.f)
    {
        *pitch *= 1.f + rand11() * pct;
        *fvol *= 1.f + rand11() * pct * 0.5f;
    }
    LastShot& last = state.lastShot;
    last = LastShot{s.name, nullptr, nullptr, nullptr, {}, *pitch, *fvol, last.count + 1};

    // The player's from the hand that fired (vr_audio.cpp handOf: CHAN_HAND, CHAN_HAND2), anyone else's from them.
    const bool player = entnum > 0 && entnum == cl.viewentity;
    const int chan = player ? (entchannel == 5 ? SND_CHAN_HAND2 : SND_CHAN_HAND) : 0;
    const float layers = za::clamp(vr_gunsfx_layers.value, 0.f, 2.f);
    const auto layerPitch = [&] { return *pitch * (1.f + rand11() * pct * 0.5f); };
    if(layers > 0.f)
    {
        if(s.mech)
        {
            last.mech = s.mech;
            startLayer(s.mech, entnum, chan, origin, *fvol * s.mechGain * layers, attenuation, layerPitch());
        }
        last.thump = thumpNames[s.kind];
        startLayer(last.thump, entnum, chan, origin, *fvol * s.thumpGain * layers, attenuation, layerPitch());
    }
    const float tail = za::clamp(vr_gunsfx_tail.value, 0.f, 2.f);
    const glm::vec3 at{origin[0], origin[1], origin[2]};
    if(tail > 0.f && cl.worldmodel)
    {
        Recent& r = recentFor(entnum, at);
        last.probe = r.probe;
        const double gap = s.kind == KindLight ? 0.25 : 0.1; // (a nailgun's stream: a tail now and then, not each nail's)
        if(cl.time - r.lastTail >= gap || cl.time < r.lastTail)
        {
            r.lastTail = cl.time;
            last.tail = tailNames[s.kind][r.probe.room];
            const float atten = r.probe.room == RoomOutdoor ? attenuation * 0.5f : attenuation; // (carries further)
            startLayer(last.tail, entnum, chan, origin, *fvol * s.tailGain * tail, atten, layerPitch());
        }
    }
    if(vr_debug_gunsfx.value)
    {
        Con_Printf("gunsfx shot %s ent %d chan %d pitch %.3f vol %.2f mech %s thump %s tail %s (room %s: %.0f units, open %.2f)\n",
            s.name, entnum, entchannel, *pitch, *fvol, last.mech ? last.mech : "-", last.thump ? last.thump : "-",
            last.tail ? last.tail : "-", roomNames[last.probe.room], last.probe.meanDist, last.probe.open);
    }
}

// An empty gun's click: its volume, a little higher or lower, a tick in the hand. 0: not played.
int dryClick(int entnum, int entchannel, float* fvol, float* pitch)
{
    LastDry& d = state.lastDry;
    d = LastDry{-1, false, d.count + 1};
    const float v = za::clamp(vr_gunsfx_dryfire.value, 0.f, 2.f);
    if(v <= 0.f)
    {
        d.silenced = true;
        return 0;
    }
    *fvol *= v;
    const float pct = za::clamp(vr_gunsfx_vary.value, 0.f, 15.f) * 0.01f;
    *pitch *= 1.f + rand11() * pct;
    const bool hand = entchannel == 1 || entchannel == 5 || entchannel == SND_CHAN_HAND || entchannel == SND_CHAN_HAND2;
    if(entnum > 0 && entnum == cl.viewentity && hand)
    {
        d.hand = entchannel == 5 || entchannel == SND_CHAN_HAND2 ? HAND_OFF : HAND_MAIN;
        pulse(d.hand, 0.012f, 0.35f * za::min(v, 1.f), 240.f);
    }
    if(vr_debug_gunsfx.value)
    {
        Con_Printf("gunsfx dry click ent %d chan %d vol %.2f hand %d\n", entnum, entchannel, *fvol, d.hand);
    }
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// The punch: a spike as hard as the weapon's recoil (the mean of its kick back and rise in the units of the old strength
// 1, 1.5 cm and 4 degrees: weapons::recoilFeel; square rooted: a strong gun is felt stronger, not twice as strong), then
// a tail from half that down to nothing (quadratic) over the weapon's kick (its kick up and Recoil Return Time: the old
// Return Time times Recovery's length, vr_wofs_version 74); `share` of each, as the shot's recoil (a weapon held in
// both hands: weaponfx::twoHandRecoil).

[[nodiscard]] float peakOf(int slot, float share = 1.f)
{
    const float recoil = weapons::recoilFeel(weapons::value(slot, Key::RecoilKick) * share,
        weapons::value(slot, Key::RecoilRise) * share);
    const float punch = za::clamp(vr_gunsfx_punch.value, 0.f, 2.f) * za::max(weapons::value(slot, Key::HapticPunch), 0.f);
    return za::clamp(0.85f * punch * za::sqrt(recoil), 0.f, 1.f);
}

[[nodiscard]] float tailOf(int slot, float share = 1.f)
{
    return za::clamp(weapons::recoilKickSeconds(za::max(weapons::value(slot, Key::RecoilReturn) * share, 0.f)), 0.03f,
        1.5f);
}

// The punch `t` s after the shot: its amplitude, frequency and the pulse's length (null amplitude: over).
[[nodiscard]] Pulse punchAt(int slot, float t, float share = 1.f)
{
    if(t < 0.f || slot < 0)
    {
        return {};
    }
    const float peak = peakOf(slot, share);
    if(t < spikeTime)
    {
        return {spikeTime - t, peak, spikeHz};
    }
    const float u = (t - spikeTime) / tailOf(slot, share);
    if(u >= 1.f || peak <= 0.f)
    {
        return {};
    }
    return {0.03f, peak * tailShare * (1.f - u) * (1.f - u), tailHz};
}

void sendPunch(int hand)
{
    Punch& p = state.punches[hand];
    if(p.slot < 0)
    {
        return;
    }
    const Pulse s = punchAt(p.slot, static_cast<float>(cl.time - p.start), p.share);
    if(s.amplitude <= 0.f)
    {
        p = Punch{};
        return;
    }
    pulse(hand, s.seconds, s.amplitude, s.hz);
}

} // namespace

const LayeredSound* layered(const char* name)
{
    for(const LayeredSound& s : gunLayers)
    {
        if(!strcmp(s.name, name))
        {
            return &s;
        }
    }
    return nullptr;
}

float actionPlace(const char* sfxName)
{
    if(!on() || vr_gunsfx_action.value == 0.f || !sfxName)
    {
        return -1.f;
    }
    for(const ActionSound& a : actions)
    {
        if(!q_strcasecmp(a.name, sfxName))
        {
            return a.place;
        }
    }
    return -1.f;
}

const char* casingSound(const glm::vec3& at, const glm::vec3& normal, int variant)
{
    if(!on() || vr_gunsfx_casings.value == 0.f || !cl.worldmodel)
    {
        return nullptr;
    }
    const msurface_t* s = physsound::surfaceOnSegment(cl.worldmodel, at + normal * 2.f, at - normal * 6.f);
    if(!s || !s->texinfo || s->texinfo->texnum < 0 || s->texinfo->texnum >= cl.worldmodel->numtextures ||
        !cl.worldmodel->textures[s->texinfo->texnum])
    {
        return nullptr;
    }
    const int k = za::clamp(variant, 0, 2);
    switch(debris::materialOf(cl.worldmodel->textures[s->texinfo->texnum]->name))
    {
    case debris::Material::Metal: return metalCasings[k];
    case debris::Material::Wood: return woodCasings[k];
    default: return nullptr; // stone and the rest: the tinks as before
    }
}

void fired(int hand)
{
    if(hand < 0 || hand >= HAND_COUNT)
    {
        return;
    }
    const int slot = weapons::heldSlot(hand);
    if(!on() || vr_gunsfx_punch.value <= 0.f || slot < 0 || slot == weapons::fistSlot())
    {
        return;
    }
    const float share = weaponfx::twoHandRecoil(hand, slot);
    state.punches[hand] = Punch{cl.time, slot, share};
    sendPunch(hand); // (the spike on the shot's own frame)
    if(vr_debug_gunsfx.value)
    {
        Con_Printf("gunsfx punch hand %d slot %d peak %.2f tail %.3f s (two-handed %.2f)\n", hand, slot,
            peakOf(slot, share), tailOf(slot, share), share);
    }
}

void frame()
{
    if(cl.paused)
    {
        return;
    }
    for(int hand = 0; hand < HAND_COUNT; hand++)
    {
        sendPunch(hand);
    }
}

bool punchOwns(int hand)
{
    if(hand < 0 || hand >= HAND_COUNT)
    {
        return false;
    }
    const Punch& p = state.punches[hand];
    return p.slot >= 0 && cl.time - p.start <= 0.06 && cl.time >= p.start;
}

void prepare()
{
    for(const LayeredSound& s : gunLayers)
    {
        (void)S_PrecacheSound(s.name);
    }
}

void clear()
{
    for(Recent& r : state.recent)
    {
        r = Recent{};
    }
    for(Punch& p : state.punches)
    {
        p = Punch{};
    }
}

} // namespace qvr::gunsfx

// S_StartSoundPitch, before it takes a channel: a shot varied and its layers started; an empty gun's click (0: not
// played). Anything else as it is.
extern "C" int VR_SndShot(int entnum, int entchannel, sfx_t* sfx, const float* origin, float* fvol, float attenuation,
    float* pitch)
{
    using namespace qvr::gunsfx;
    if(!on() || state.nested || !sfx)
    {
        return 1;
    }
    if(!q_strcasecmp(sfx->name, dryClickName))
    {
        return dryClick(entnum, entchannel, fvol, pitch);
    }
    if(entchannel != 1 && entchannel != 5) // (QC CHAN_WEAPON, CHAN_WEAPON2: a gun's report)
    {
        return 1;
    }
    if(const ShotSound* s = shotOf(sfx->name))
    {
        shot(*s, entnum, entchannel, origin, fvol, attenuation, pitch);
    }
    return 1;
}

#include "vr_gunsfx_test.inc"
