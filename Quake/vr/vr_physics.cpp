// vr_physics.cpp -- server-side Quake VR physics: hand and weapon touches, teleport,
// room-scale movement, head-relative movement, swimming, the second think timer and touch rules.
//
// The touches, the second think timer and the touch rules are inactive unless the server runs
// Quake VR progs; the movement works with any progs (and moves a mod's shots to the gun).

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_move.hpp"
#include "vr_server.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_particles.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace qvr;
using namespace qvr::progs;

namespace
{

// QC constants (QC/defs.qc, QC/vr_defs.qc).
constexpr int FL_EASYHANDTOUCH = 8192;
constexpr int VRBITS0_TELEPORTING = 1 << 0;
constexpr float HAND_OFF = 0.f;
constexpr float HAND_MAIN = 1.f;
constexpr float HAND_FAKE = 2.f; // body touch standing in for a hand

constexpr float handHalfSize = 2.5f;
constexpr float easyHandTouchBonus = 4.5f;

[[nodiscard]] bool active()
{
    return bindings().isVrProgs;
}

[[nodiscard]] const FieldOffsets& f()
{
    return fields();
}

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

[[nodiscard]] bool hasFlag(edict_t* ent, int flag)
{
    return (static_cast<int>(ent->v.flags) & flag) != 0;
}

[[nodiscard]] int solidOf(edict_t* ent)
{
    return static_cast<int>(ent->v.solid);
}

[[nodiscard]] bool isClient(edict_t* ent)
{
    return hasFlag(ent, FL_CLIENT);
}

// Whether `player`'s hands come from real tracking (see QVR_BUTTON_HANDSTRACKED).
[[nodiscard]] bool handsTracked(edict_t* player)
{
    const VrMove* move = server::clientMove(player);
    return move && (move->buttons & protocol::QVR_BUTTON_HANDSTRACKED);
}

[[nodiscard]] bool boxesOverlap(const glm::vec3& aMin, const glm::vec3& aMax,
    const glm::vec3& bMin, const glm::vec3& bMax)
{
    return aMin.x <= bMax.x && aMax.x >= bMin.x && aMin.y <= bMax.y &&
           aMax.y >= bMin.y && aMin.z <= bMax.z && aMax.z >= bMin.z;
}

[[nodiscard]] bool absBoxesOverlap(edict_t* a, edict_t* b)
{
    return boxesOverlap(vec(a->v.absmin), vec(a->v.absmax), vec(b->v.absmin), vec(b->v.absmax));
}

[[nodiscard]] float handTouchBonus(edict_t* target)
{
    return hasFlag(target, FL_EASYHANDTOUCH) ? easyHandTouchBonus : 0.f;
}

// Whether a hand at `hand` touches `target`: an object that can be carried or pulled by the
// model's own turned box, a small margin round it; anything else by its box (and the easy-touch
// bonus).
[[nodiscard]] bool handOn(edict_t* target, const glm::vec3& hand)
{
    if(hasFlag(target, physics::FL_FORCEGRABBABLE))
    {
        return physics::pointInModelBox(target, hand, 2.f);
    }
    const glm::vec3 extent{handHalfSize};
    const glm::vec3 bonus{handTouchBonus(target)};
    const glm::vec3 origin = vec(target->v.origin);
    return boxesOverlap(hand - extent, hand + extent, origin + vec(target->v.mins) - bonus,
        origin + vec(target->v.maxs) + bonus);
}

[[nodiscard]] glm::vec3 forwardFromAngles(const glm::vec3& angles)
{
    vec3_t in{angles.x, angles.y, angles.z}, forward, right, up;
    AngleVectors(in, forward, right, up);
    return vec(forward);
}

void setHandtouchParams(float hand, edict_t* player, edict_t* target)
{
    fieldFloat(player, f().touchinghand) = hand;
    fieldFloat(target, f().handtouch_hand) = hand;
    fieldInt(target, f().handtouch_ent) = EDICT_TO_PROG(player);
}

// Runs a function field of `self` with `other`, preserving self/other.
void callField(edict_t* self, edict_t* other, func_t fn)
{
    const int oldSelf = pr_global_struct->self;
    const int oldOther = pr_global_struct->other;

    pr_global_struct->self = EDICT_TO_PROG(self);
    pr_global_struct->other = EDICT_TO_PROG(other);
    pr_global_struct->time = qcvm->time;
    PR_ExecuteProgram(fn);

    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
}

// SV_Impact for an arbitrary function field (handtouch, vr_wpntouch).
void impactField(edict_t* e1, edict_t* e2, int ofs)
{
    if(const func_t fn = fieldFunc(e1, ofs); fn && solidOf(e1) != SOLID_NOT)
    {
        callField(e1, e2, fn);
    }

    if(const func_t fn = fieldFunc(e2, ofs); fn && solidOf(e2) != SOLID_NOT)
    {
        callField(e2, e1, fn);
    }
}

[[nodiscard]] trace_t moveTrace(const glm::vec3& start, const glm::vec3& mins,
    const glm::vec3& maxs, const glm::vec3& end, int type, edict_t* pass)
{
    vec3_t s{start.x, start.y, start.z}, mi{mins.x, mins.y, mins.z},
        ma{maxs.x, maxs.y, maxs.z}, e{end.x, end.y, end.z};
    return SV_Move(s, mi, ma, e, type, pass);
}

// Hands touching things along the player's reach: traces from the body (and from a box
// enclosing body and hands) towards each hand, and from each hand along its direction.
void handTouches(edict_t* ent)
{
    const glm::vec3 handExtent{handHalfSize};
    const glm::vec3 hands[2] = {fieldVec(ent, f().offhandpos), fieldVec(ent, f().handpos)};
    const glm::vec3 ends[2] = {hands[0] + forwardFromAngles(fieldVec(ent, f().offhandrot)),
        hands[1] + forwardFromAngles(fieldVec(ent, f().handrot))};

    const auto checkTrace = [&](const trace_t& trace) {
        edict_t* target = trace.ent;
        if(!target || !fieldFunc(target, f().handtouch))
        {
            return;
        }

        for(int h = 0; h < 2; h++)
        {
            if(handOn(target, hands[h]))
            {
                setHandtouchParams(h == 0 ? HAND_OFF : HAND_MAIN, ent, target);
                impactField(ent, target, f().handtouch);
            }
        }
    };

    glm::vec3 lo = vec(ent->v.origin) + vec(ent->v.mins);
    glm::vec3 hi = vec(ent->v.origin) + vec(ent->v.maxs);
    for(const glm::vec3& hand : hands)
    {
        lo = glm::min(lo, hand - handExtent);
        hi = glm::max(hi, hand + handExtent);
    }
    const glm::vec3 unionCentre = (lo + hi) * 0.5f;
    const glm::vec3 unionHalf = (hi - lo) * 0.5f;

    for(int h = 1; h >= 0; h--) // main hand first
    {
        checkTrace(moveTrace(vec(ent->v.origin), vec(ent->v.mins), vec(ent->v.maxs), ends[h],
            MOVE_NORMAL, ent));
        checkTrace(moveTrace(unionCentre, -unionHalf, unionHalf, ends[h], MOVE_NORMAL, ent));
    }

    for(int h = 1; h >= 0; h--)
    {
        checkTrace(moveTrace(hands[h], -handExtent, handExtent, ends[h], MOVE_NORMAL, ent));
    }
}

// Weapons poking things: the gun from each hand to its muzzle, using the networked hand
// and muzzle positions (so it works for every client, not just a listen server's).
void weaponTouches(edict_t* ent)
{
    const glm::vec3 gunExtent{1.f};
    const int handPos[2] = {f().handpos, f().offhandpos};
    const int muzzlePos[2] = {f().muzzlepos, f().offmuzzlepos};
    const float handIndex[2] = {HAND_MAIN, HAND_OFF};

    for(int i = 0; i < 2; i++)
    {
        const trace_t trace = moveTrace(fieldVec(ent, handPos[i]), -gunExtent, gunExtent,
            fieldVec(ent, muzzlePos[i]), MOVE_NORMAL, ent);

        if(trace.fraction < 1.f && trace.ent && fieldFunc(trace.ent, f().vr_wpntouch))
        {
            setHandtouchParams(handIndex[i], ent, trace.ent);
            impactField(ent, trace.ent, f().vr_wpntouch);
        }
    }
}

[[nodiscard]] bool canBeTouched(edict_t* target)
{
    return (target->v.touch || fieldFunc(target, f().handtouch)) &&
           solidOf(target) != SOLID_NOT;
}

// Body touches: triggers and touchable non-solids always; other solids only with
// vr_gameplayfix_touchsolids. Bodies also "hand"-touch things when hands aren't tracked.
void touch(edict_t* ent, edict_t* target)
{
    if(!absBoxesOverlap(ent, target))
    {
        return;
    }

    const int solid = solidOf(target);
    if(target->v.touch && (solid == SOLID_TRIGGER || solid == SOLID_NOT_BUT_TOUCHABLE ||
                              vr_gameplayfix_touchsolids.value))
    {
        callField(target, ent, target->v.touch);
    }

    const func_t handtouch = fieldFunc(target, f().handtouch);
    if(handtouch && !target->free && isClient(ent) &&
        (!fieldFloatOr(ent, f().ishuman, 0.f) || vr_body_interactions.value || !handsTracked(ent)))
    {
        setHandtouchParams(HAND_FAKE, ent, target);
        callField(target, ent, handtouch);
    }
}

void handTouch(edict_t* ent, edict_t* target)
{
    const func_t handtouch = fieldFunc(target, f().handtouch);
    if(!handtouch || solidOf(target) == SOLID_NOT)
    {
        return;
    }

    // The entity's own box, not its abs box: Quake widens items' abs boxes by 15 units for walking
    // over them, which made a 6-unit ammo box grabbable from a hand's width away.
    const bool offHit = handOn(target, fieldVec(ent, f().offhandpos));
    const bool mainHit = handOn(target, fieldVec(ent, f().handpos));

    if(offHit || mainHit)
    {
        setHandtouchParams(offHit ? HAND_OFF : HAND_MAIN, ent, target);
        callField(target, ent, handtouch);
    }
}

[[nodiscard]] bool handsReach(edict_t* ent, edict_t* target)
{
    const glm::vec3 reach{handHalfSize + easyHandTouchBonus};
    const glm::vec3 tMin = vec(target->v.absmin);
    const glm::vec3 tMax = vec(target->v.absmax);

    for(const int ofs : {f().offhandpos, f().handpos})
    {
        const glm::vec3 hand = fieldVec(ent, ofs);
        if(boxesOverlap(hand - reach, hand + reach, tMin, tMax))
        {
            return true;
        }
    }

    return false;
}

} // namespace

extern "C" int VR_RunThink2(edict_t* ent)
{
    if(!active() || f().nextthink2 < 0)
    {
        return 1;
    }

    const func_t think2 = fieldFunc(ent, f().think2);
    float thinktime = fieldFloat(ent, f().nextthink2);
    if(!think2 || thinktime <= 0.f || thinktime > qcvm->time + host_frametime)
    {
        return 1;
    }

    if(thinktime < qcvm->time)
    {
        thinktime = static_cast<float>(qcvm->time);
    }

    fieldFloat(ent, f().nextthink2) = 0.f;
    pr_global_struct->time = thinktime;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    PR_ExecuteProgram(think2);

    return !ent->free;
}

namespace
{
void waterFeedback(edict_t* ent); // "Water splashes and sounds" below
} // namespace

extern "C" void VR_ClientPreMove(edict_t* ent)
{
    server::rebaseHands(ent);
    if(!active())
    {
        return;
    }

    waterFeedback(ent); // hands and guns slapping the water, wading
    handTouches(ent);
    weaponTouches(ent);
}

extern "C" int VR_ClientTeleport(edict_t* ent)
{
    const VrMove* move = server::clientMove(ent);
    if(!move || !(move->vrBits0 & VRBITS0_TELEPORTING))
    {
        return 0;
    }

    if(!SV_RunThink(ent))
    {
        return -1;
    }

    // The client picks the target: accept it only within the teleport range (with some slack for
    // the arc) and where the player fits.
    const glm::vec3 target = move->teleportTarget;
    if(glm::distance(vec(ent->v.origin), target) > std::max(vr_teleport_range.value, 100.f) * 1.5f + 64.f)
    {
        return 0;
    }
    vec3_t t{target.x, target.y, target.z};
    if(SV_Move(t, ent->v.mins, ent->v.maxs, t, MOVE_NOMONSTERS, ent).startsolid)
    {
        return 0;
    }

    ent->v.teleport_time = static_cast<float>(qcvm->time) + 0.3f;
    for(int i = 0; i < 3; i++)
    {
        ent->v.origin[i] = ent->v.oldorigin[i] = target[i];
    }

    return 1;
}

// Physical walking in the play space: a second, horizontal move with collision.
extern "C" void VR_ClientRoomscaleMove(edict_t* ent)
{
    const VrMove* vrMove = server::clientMove(ent);
    if(!vrMove)
    {
        return;
    }

    const glm::vec3 move = vrMove->roomscaleMove;
    if(move.x == 0.f && move.y == 0.f)
    {
        return;
    }

    vec3_t oldVelocity;
    VectorCopy(ent->v.velocity, oldVelocity);
    ent->v.velocity[0] = move.x;
    ent->v.velocity[1] = move.y;
    ent->v.velocity[2] = 0.f;

    switch(static_cast<int>(ent->v.movetype))
    {
        case MOVETYPE_WALK:
        {
            // Walking in the room must not make the player airborne.
            const int onGround = static_cast<int>(ent->v.flags) & FL_ONGROUND;
            SV_CheckStuck(ent);
            SV_WalkMove(ent);
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | onGround);
            break;
        }
        case MOVETYPE_FLY: SV_FlyMove(ent, static_cast<float>(host_frametime), nullptr); break;
        case MOVETYPE_NOCLIP:
            VectorMA(ent->v.origin, static_cast<float>(host_frametime), ent->v.velocity,
                ent->v.origin);
            break;
        default: break;
    }

    VectorCopy(oldVelocity, ent->v.velocity);
}

// ----------------------------------------------------------------------------
// Water splashes and sounds (vr_water_splash, vr_water_sounds)
//
// A splash is Quake VR's particle preset (particles::Preset::Splash: drops, foam and ripples,
// drawn by each client as its vr_water_splash says), sent from here, with a sound at the spot
// (vr_water_sounds is their volume; the sounds are made by Misc/quakevr/make_sounds.py and
// precached by QC/world.qc). They come from:
// - things going into a liquid (SV_CheckWaterTransition: shots, grenades, nails, gibs, items,
//   thrown weapons, a monster falling in), as hard as they go and as big as they are
//   (VR_AllowWaterSplash, below);
// - shots crossing a surface (QC's liquidentry and watersplash: weapons.qc) and the player going
//   in (client.qc's WaterMove, with Quake's own sound);
// - hands and guns hitting the surface or pulled out of it fast (with a buzz in the hand), and
//   wading: sloshes at a walking pace, ripples at the legs (waterFeedback, every frame);
// - swimming strokes: a stroke's sound as its power gate opens (VR_AfterWaterMove), and a splash
//   if it breaks the surface.

namespace
{

[[nodiscard]] bool isLiquid(int contents)
{
    return contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA; // currents are water here
}

[[nodiscard]] int contentsAt(const glm::vec3& p)
{
    vec3_t v{p.x, p.y, p.z};
    return SV_PointContents(v);
}

// The surface between a point out of the liquid and one in it.
[[nodiscard]] glm::vec3 surfaceBetween(glm::vec3 dry, glm::vec3 wet)
{
    for(int i = 0; i < 12 && glm::distance(dry, wet) > 0.1f; i++)
    {
        const glm::vec3 mid = (dry + wet) * 0.5f;
        (isLiquid(contentsAt(mid)) ? wet : dry) = mid;
    }
    return (dry + wet) * 0.5f;
}

// The surface on the vertical through `p`, at most `range` units above it (from in the liquid) or
// below it (from out of it); false if there is none (a wall or floor comes first).
[[nodiscard]] bool surfaceOver(const glm::vec3& p, float range, glm::vec3& out)
{
    const bool wet = isLiquid(contentsAt(p));
    glm::vec3 last = p;
    for(float d = 4.f; d < range + 4.f; d += 4.f)
    {
        const glm::vec3 q = p + glm::vec3{0.f, 0.f, wet ? std::min(d, range) : -std::min(d, range)};
        const int c = contentsAt(q);
        if(c == CONTENTS_SOLID || c == CONTENTS_SKY)
        {
            return false;
        }
        if(isLiquid(c) != wet)
        {
            out = wet ? surfaceBetween(q, last) : surfaceBetween(last, q);
            return true;
        }
        last = q;
    }
    return false;
}

// Sounds at a point (not an entity's): the world's entity, on the auto channel, with the position
// given -- what SV_StartSound sends, where a sound is. At most a few a frame (a shotgun's pellets).
[[nodiscard]] int soundIndex(const char* sample)
{
    for(int i = 1; i < MAX_SOUNDS && sv.sound_precache[i]; i++)
    {
        if(!std::strcmp(sample, sv.sound_precache[i]))
        {
            return i;
        }
    }
    return 0;
}

double soundFrame = -1.0;
int soundsThisFrame = 0;

bool soundAt(const glm::vec3& at, const char* sample, float volume, float attenuation = 1.f)
{
    const float master = CLAMP(0.f, vr_water_sounds.value, 1.f);
    const int vol = static_cast<int>(CLAMP(0.f, volume * master, 1.f) * 255.f);
    const int index = vol > 0 ? soundIndex(sample) : 0;
    if(!index)
    {
        return false;
    }
    if(soundFrame != qcvm->time)
    {
        soundFrame = qcvm->time;
        soundsThisFrame = 0;
    }
    if(soundsThisFrame >= 3 || sv.datagram.cursize > MAX_DATAGRAM - 21)
    {
        return true; // played enough of them this frame
    }
    soundsThisFrame++;
    if(developer.value >= 2)
    {
        Con_Printf("VR water sound: %s, volume %.2f\n", sample, vol / 255.f);
    }

    int mask = 0;
    if(vol != DEFAULT_SOUND_PACKET_VOLUME)
    {
        mask |= SND_VOLUME;
    }
    if(attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
    {
        mask |= SND_ATTENUATION;
    }
    if(index >= 256)
    {
        if(sv.protocol == PROTOCOL_NETQUAKE)
        {
            return false;
        }
        mask |= SND_LARGESOUND;
    }
    MSG_WriteByte(&sv.datagram, svc_sound);
    MSG_WriteByte(&sv.datagram, mask);
    if(mask & SND_VOLUME)
    {
        MSG_WriteByte(&sv.datagram, vol);
    }
    if(mask & SND_ATTENUATION)
    {
        MSG_WriteByte(&sv.datagram, static_cast<int>(attenuation * 64.f));
    }
    MSG_WriteShort(&sv.datagram, 0); // the world, channel 0 (auto)
    if(mask & SND_LARGESOUND)
    {
        MSG_WriteShort(&sv.datagram, index);
    }
    else
    {
        MSG_WriteByte(&sv.datagram, index);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, at[i], sv.protocolflags);
    }
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
std::vector<SentSplash> splashesThisFrame;

bool sendSplash(const glm::vec3& at, const glm::vec3& dir, float strength)
{
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
    splashesThisFrame.push_back({at, strength});
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
    return true;
}

// Out of the water: a smaller splash (drops falling off), the sound quieter.
void splashOut(const glm::vec3& at, float strength, float volume)
{
    sendSplash(at, glm::vec3{0.f, 0.f, 1.f}, strength);
    soundAt(at, "vr/splash_small.wav", volume);
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
    double stroke{-10.0};                   // when the last stroke was heard
    int strokeSound{0};
    glm::vec3 origin{0.f};
    bool originValid{false};
    float wade{0.f};                        // the pace: a slosh at every whole one
    float wadeSpeed{0.f};                   // units/s, smoothed
    glm::vec2 wadeDir{1.f, 0.f};
    double wetSince[2]{-1.0, -1.0};         // since when each hand is in the water (-1: out)
    int sloshSound{0};
};

WaterFeel waterFeel[MAX_SCOREBOARD];

[[nodiscard]] WaterFeel* feelOf(edict_t* ent)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= std::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
    {
        return nullptr;
    }
    return &waterFeel[client];
}

// A hand (or a gun) crossing the surface: into it going down, or out of it going up, fast.
void handCrossing(edict_t* ent, WaterFeel& w, int probe, const WaterProbe& was, const glm::vec3& p, int contents, float dt)
{
    const bool in = isLiquid(contents);
    const bool crossed = in != isLiquid(was.contents) && (contents == CONTENTS_EMPTY || was.contents == CONTENTS_EMPTY);
    const int hand = probe & 1;
    if(!crossed || dt <= 0.f || qcvm->time - w.handSplash[hand] < 0.3)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    const glm::vec3 vel = (p - was.pos) / dt / m2u; // m/s, the body's motion too
    const float speed = glm::length(vel);
    const float needed = in ? 0.9f : 1.4f; // m/s, down into it or up out of it
    if(speed > 12.f || (in ? -vel.z : vel.z) < needed)
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
        soundAt(at, "vr/splash_small.wav", 0.35f + 0.65f * hard);
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
    w.wadeSpeed += (distance / dt - w.wadeSpeed) * std::min(1.f, dt / 0.25f);
    if(distance > 0.01f)
    {
        w.wadeDir = moved / distance;
    }
    const glm::vec3 feet = origin + glm::vec3{0.f, 0.f, ent->v.mins[2] - 4.f};
    const bool onBottom = hasFlag(ent, FL_ONGROUND) || contentsAt(feet) == CONTENTS_SOLID;
    if(w.wadeSpeed < 40.f || !onBottom)
    {
        w.wade = std::min(w.wade, 0.6f); // the next step soon after moving again
        return;
    }
    const float interval = CLAMP(0.3f, 0.62f - w.wadeSpeed / 1000.f, 0.6f);
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
    soundAt(at, (w.sloshSound++ & 1) ? "vr/slosh2.wav" : "vr/slosh1.wav", CLAMP(0.25f, w.wadeSpeed / 300.f, 0.8f) * deep);
}

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
        const int contents = contentsAt(p);
        if(probe.valid)
        {
            handCrossing(ent, w, i, probe, p, contents, dt);
        }
        if(i < 2)
        {
            if(!isLiquid(contents))
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

// A swimming stroke's sound, as it passes its power gate (once a stroke), from the hand; a
// splash if it breaks the surface.
void strokeFeedback(edict_t* ent, int handIndex, const glm::vec3& hand, float peak)
{
    WaterFeel* w = feelOf(ent);
    // Both hands at once: one sound. A hand just gone in (a slap) has its splash instead.
    const double wet = w ? w->wetSince[handIndex] : -1.0;
    if(!w || qcvm->time - w->stroke < 0.3 || wet < 0.0 || qcvm->time - wet < 0.15)
    {
        return;
    }
    w->stroke = qcvm->time;
    const float hard = CLAMP(0.f, (peak - 1.f) / 2.5f, 1.f);
    soundAt(hand, (w->strokeSound++ & 1) ? "vr/stroke2.wav" : "vr/stroke1.wav", 0.3f + 0.55f * hard);
    glm::vec3 at;
    if(surfaceOver(hand, 10.f, at))
    {
        sendSplash(at, glm::vec3{0.f, 0.f, 1.f}, 3.f + 4.f * hard);
    }
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

} // namespace

namespace qvr::physics
{

bool liquidEntry(const glm::vec3& from, const glm::vec3& to, glm::vec3& at)
{
    const glm::vec3 d = to - from;
    const float length = glm::length(d);
    if(length < 0.01f)
    {
        return false;
    }
    const int steps = std::clamp(static_cast<int>(std::ceil(length / 6.f)), 1, 512);
    glm::vec3 last = from;
    int lastContents = contentsAt(from);
    for(int i = 1; i <= steps; i++)
    {
        const glm::vec3 p = from + d * (static_cast<float>(i) / static_cast<float>(steps));
        const int c = contentsAt(p);
        if(isLiquid(c) && lastContents == CONTENTS_EMPTY)
        {
            at = surfaceBetween(last, p);
            return true;
        }
        if(c == CONTENTS_EMPTY && isLiquid(lastContents))
        {
            at = surfaceBetween(p, last);
            return true;
        }
        last = p;
        lastContents = c;
    }
    return false;
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
        case SplashSound::Shot: soundAt(at, "vr/plip.wav", 0.6f); break;
        case SplashSound::Thing:
            soundAt(at, strength >= 18.f ? "vr/splash_big.wav" : "vr/splash_small.wav", CLAMP(0.35f, 0.3f + strength / 25.f, 1.f));
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
// Against the remembered stroke, a stroke as fast (over its peak) as vr_swim_reverse_speed is not
// damped at all, and one this much slower than that is damped fully: the relaxed return is slower;
// a deliberate reverse stroke, not.
constexpr float reverseWeakRange = 0.3f;
// Faster than this (m/s), a hand is taken for a tracking jump, not a stroke.
constexpr float glitchSpeed = 8.f;

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
    float weight{0.f};
    bool heard{false};    // its sound played (as the power gate opened)
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
// threshold and was not damped as a return -- becomes the hand's intent.
void endStroke(SwimHand& h, int hand, double now)
{
    Stroke& s = h.stroke;
    if(!s.active)
    {
        return;
    }
    if(vr_swim_debug.value && glm::length(s.raw) > 0.f)
    {
        Con_Printf("swim %s: peak %.2f m/s, flat %.2f, power x%.2f, push %.0f of %.0f (%+.0f ahead)\n",
            hand ? "main" : "off", s.peak, s.weight > 0.f ? s.flat / s.weight : 0.f, s.factor, glm::length(s.given),
            glm::length(s.raw), s.ahead);
    }
    if(s.factor >= 0.5f && glm::length(s.moved) > 0.f)
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
// flat than at an angle (vr_swim_palm).
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
extern "C" void VR_AfterWaterMove(edict_t* ent, float forwardmove, float sidemove, float upmove)
{
    const VrMove* move = swimmer(ent);
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(!move || client < 0 || client >= std::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
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
    const float minSpeed = std::max(0.f, vr_swim_stroke_min.value); // m/s
    const float palmWeight = CLAMP(0.f, vr_swim_palm.value, 1.f);
    const float sideKept = 1.f - CLAMP(0.f, vr_swim_look.value, 1.f);
    const float recovery = CLAMP(0.f, vr_swim_recovery.value, 1.f);
    const float flatExp = CLAMP(0.25f, vr_swim_flat_exp.value, 4.f);
    const float speedExp = CLAMP(0.5f, vr_swim_speed_exp.value, 3.f);
    const float palmDir = CLAMP(0.f, vr_swim_palm_dir.value, 1.f);
    const float powerMin = std::max(0.f, vr_swim_power_threshold.value);
    const float powerKnee = std::max(0.f, vr_swim_power_knee.value);
    const bool whole = vr_swim_power_whole.value != 0.f;
    const float memory = std::max(0.f, vr_swim_intent_memory.value);
    const float reverseDamp = CLAMP(0.f, vr_swim_reverse_damp.value, 1.f);
    const float reverseSpeed = CLAMP(reverseWeakRange, vr_swim_reverse_speed.value, 3.f);
    glm::vec3 vel = vec(ent->v.velocity);

    // The glide: SV_WaterMove's friction took dt * sv_friction of the speed; give part of it back.
    const float glide = CLAMP(0.f, vr_swim_glide.value, 1.f);
    if(glide > 0.f && !(static_cast<int>(ent->v.flags) & FL_ONGROUND))
    {
        static const cvar_t* friction = Cvar_FindVar("sv_friction");
        const float lost = dt * (friction ? friction->value : 4.f);
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
        if(speedMs > glitchSpeed)
        {
            continue;
        }
        if(SV_PointContents(p) > CONTENTS_WATER || speedMs <= minSpeed) // out of the water (or slime, lava); still
        {
            endStroke(state, h, now);
            continue;
        }
        const glm::vec3 dir = hand.vel / speedMs;

        // The stroke: it goes on while the hand keeps its way (curving), and ends where it turns.
        if(stroke.active && glm::dot(dir, stroke.way) < strokeTurnCos)
        {
            endStroke(state, h, now);
        }
        if(!stroke.active)
        {
            stroke.active = true;
            stroke.way = dir;
        }
        else
        {
            const glm::vec3 way = glm::mix(stroke.way, dir, std::min(1.f, dt / strokeFollow));
            stroke.way = glm::length(way) > 0.f ? glm::normalize(way) : dir;
        }
        stroke.moved += hand.vel * dt;
        stroke.peak = std::max(stroke.peak, speedMs);

        // The palm (and the back of the hand) faces the hand's side: how flat the hand meets the
        // water. Edge first it slices through (the recovery) and hardly pushes.
        vec3_t a{hand.rot.x, hand.rot.y, hand.rot.z}, f, r, u;
        AngleVectors(a, f, r, u);
        const glm::vec3 side{r[0], r[1], r[2]};
        const float facing = glm::dot(side, dir);
        const float flat = std::abs(facing);
        const float palm = (1.f - palmWeight) + palmWeight * std::pow(flat, flatExp);
        const float edge = recovery + (1.f - recovery) * glm::smoothstep(0.1f, 0.45f, flat);

        // As water's drag, the push grows with the square of the speed (vr_swim_speed_exp; as the
        // linear push at a brisk stroke): the stroke, faster, outdoes the return for the next one.
        const float beyond = (speedMs - minSpeed) * units::metresToUnits();
        const float strength = beyond * std::pow(beyond / (strokeSpeed * units::metresToUnits()), speedExp - 1.f);

        // The body goes against the hand (or, vr_swim_palm_dir, away from where the palm faces).
        glm::vec3 push = -dir;
        if(palmDir > 0.f)
        {
            const glm::vec3 blended = glm::mix(-dir, side * (facing < 0.f ? 1.f : -1.f), palmDir);
            push = glm::length(blended) > 0.001f ? glm::normalize(blended) : -dir;
        }
        const float along = glm::dot(push, wishDir); // 1: the push goes where the stick points
        const float steer = std::max(0.f, 1.f + assist * along);

        // Along where you look (ahead or back) it counts fully, sideways of it less.
        const float ahead = glm::dot(push, look);
        const glm::vec3 biased = look * ahead + (push - look * ahead) * sideKept;
        const glm::vec3 raw = biased * (strength * vr_swim_stroke.value * palm * edge * steer * dt);

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
        stroke.factor = whole ? std::max(stroke.factor, factor) : factor;
        if(!stroke.heard && factor >= 0.5f)
        {
            stroke.heard = true;
            strokeFeedback(ent, h, hand.pos, stroke.peak); // its sound, a splash at the surface
        }
        stroke.raw += raw;
        stroke.given += give;
        stroke.ahead += glm::dot(give, look);
        stroke.flat += flat * speedMs;
        stroke.weight += speedMs;
        vel += give;
    }

    const float maxSpeed = std::max(0.f, vr_swim_max_speed.value);
    const float len = glm::length(vel);
    if(len > maxSpeed && len > 0.f)
    {
        vel *= maxSpeed / len;
    }
    ent->v.velocity[0] = vel.x;
    ent->v.velocity[1] = vel.y;
    ent->v.velocity[2] = vel.z;
}

// Locomotion follows the head, not the aiming hand.
extern "C" float* VR_MoveAngles(edict_t* ent, float* fallback)
{
    float* head = server::clientHeadAngles(ent);
    return head ? head : fallback;
}

// Compatibility mode (a mod's progs): its weapons fire from the player's origin plus '0 0 16'
// (rockets 8 units further along the aim; bullets at 70% of the player's height) in the aim's
// direction, which is the hand's (.v_angle). While PlayerPostThink runs its weapon code, the
// player is moved -- not relinked: nothing touches it, and traces skip it anyway -- so that
// point is the main hand's muzzle; then moved back.
namespace
{
struct PostThinkShift
{
    bool active{false};
    edict_t* ent{nullptr};
    glm::vec3 origin{0.f}, absmin{0.f}, absmax{0.f}, delta{0.f};
};
PostThinkShift shift;
} // namespace

extern "C" void VR_BeforePlayerPostThink(edict_t* ent)
{
    server::rebaseHands(ent);
    shift.active = false;
    const VrMove* move = server::clientMove(ent);
    if(active() || !move || !vr_compat_muzzle.value)
    {
        return;
    }

    glm::vec3 muzzle = move->muzzlePos[1];
    if(muzzle == glm::vec3{0.f})
    {
        muzzle = move->hands[1].pos;
    }
    if(muzzle == glm::vec3{0.f})
    {
        return;
    }

    vec3_t fwd, right, up;
    AngleVectors(ent->v.v_angle, fwd, right, up);
    const glm::vec3 shotPoint = vec(ent->v.origin) + glm::vec3{0.f, 0.f, 16.f};
    const glm::vec3 delta = (muzzle - vec(fwd) * 8.f) - shotPoint;
    if(glm::length(delta) > 96.f)
    {
        return; // not where the player is (a teleport, a respawn)
    }

    shift = {true, ent, vec(ent->v.origin), vec(ent->v.absmin), vec(ent->v.absmax), delta};
    if(developer.value >= 2)
    {
        Con_Printf("VR compat: shots from %.1f %.1f %.1f (moved %.1f)\n", muzzle.x, muzzle.y, muzzle.z, glm::length(delta));
    }
    for(int i = 0; i < 3; i++)
    {
        ent->v.origin[i] += delta[i];
        ent->v.absmin[i] += delta[i];
        ent->v.absmax[i] += delta[i];
    }
}

extern "C" void VR_AfterPlayerPostThink(edict_t* ent)
{
    if(!shift.active || shift.ent != ent || ent->free)
    {
        shift.active = false;
        return;
    }
    shift.active = false;

    // Back where it was, unless the progs moved it meanwhile.
    if(vec(ent->v.origin) == shift.origin + shift.delta)
    {
        for(int i = 0; i < 3; i++)
        {
            ent->v.origin[i] = shift.origin[i];
            ent->v.absmin[i] = shift.absmin[i];
            ent->v.absmax[i] = shift.absmax[i];
        }
    }
}

extern "C" float VR_StepSize(float fallback)
{
    return active() ? vr_player_stepsize.value : fallback;
}

extern "C" void VR_OnWaterLevelChange(edict_t* ent, float oldWaterLevel)
{
    if(active() && f().lastwatertime >= 0 && ent->v.waterlevel != oldWaterLevel)
    {
        fieldFloat(ent, f().lastwatertime) = static_cast<float>(qcvm->time);
    }
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

    // A thing going in (the watertype is still the old one): a splash as hard as it goes and as
    // big as it is, its sound in place of Quake's (unless vr_water_sounds is off); coming out, a
    // smaller one, and Quake's sound.
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 vel = vec(ent->v.velocity);
    const float speed = glm::length(vel);
    const float size = CLAMP(0.35f, thingRadius(ent) / 8.f, 3.f);
    const float strength = CLAMP(2.f, 6.f * CLAMP(0.3f, speed / 400.f, 2.f) * size, 45.f);
    glm::vec3 at;
    const bool entering = ent->v.watertype == CONTENTS_EMPTY;
    if(developer.value >= 2)
    {
        Con_Printf("VR splash: %s %s at %.0f u/s, radius %.0f\n", PR_GetString(ent->v.classname), entering ? "in" : "out", speed,
            thingRadius(ent));
    }
    if(!surfaceOver(origin, std::max(48.f, speed * static_cast<float>(host_frametime) * 1.5f), at))
    {
        return 1;
    }
    if(!entering)
    {
        sendSplash(at, glm::vec3{0.f, 0.f, 1.f}, strength * 0.4f);
        return 1;
    }
    sendSplash(at, speed > 0.f ? vel / speed : glm::vec3{0.f, 0.f, -1.f}, strength);
    return !soundAt(at, strength >= 18.f ? "vr/splash_big.wav" : "vr/splash_small.wav", CLAMP(0.35f, 0.3f + strength / 25.f, 1.f));
}

extern "C" int VR_TouchLinks(edict_t* ent)
{
    if(!active())
    {
        return 0;
    }

    // Unlike Ironwail's trigger-only area walk, hands can reach outside the body's box and
    // touch non-triggers, so scan every edict (touch functions may relink, hence the copy).
    const int mark = Hunk_LowMark();
    edict_t** list = static_cast<edict_t**>(Hunk_AllocNoFill(qcvm->num_edicts * sizeof(edict_t*)));
    int count = 0;

    const bool client = isClient(ent);
    for(int e = 1; e < qcvm->num_edicts; e++)
    {
        edict_t* target = EDICT_NUM(e);
        if(target == ent || target->free || !canBeTouched(target))
        {
            continue;
        }

        if(absBoxesOverlap(ent, target) || (client && handsReach(ent, target)))
        {
            list[count++] = target;
        }
    }

    for(int i = 0; i < count; i++)
    {
        edict_t* target = list[i];
        if(target->free || ent->free)
        {
            continue;
        }

        touch(ent, target);
        if(client && !target->free)
        {
            handTouch(ent, target);
        }
    }

    Hunk_FreeToLowMark(mark);
    return 1;
}

extern "C" int VR_ExpandAbsBox(edict_t* ent)
{
    if(!active() || !hasFlag(ent, FL_EASYHANDTOUCH))
    {
        return 0;
    }

    ent->v.absmin[0] -= easyHandTouchBonus;
    ent->v.absmin[1] -= easyHandTouchBonus;
    ent->v.absmax[0] += easyHandTouchBonus;
    ent->v.absmax[1] += easyHandTouchBonus;
    return 1;
}

extern "C" float VR_MissileExtent(float fallback)
{
    return vr_gameplayfix_missilesize.value > 0.f ? vr_gameplayfix_missilesize.value : fallback;
}
