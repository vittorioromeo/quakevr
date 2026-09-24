// vr_physics.cpp -- server-side Quake VR physics: hand and weapon touches, teleport,
// room-scale movement, head-relative movement, the second think timer and touch rules.
//
// Everything here is inactive unless the server runs Quake VR progs.

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"

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

// Per client: whether its hands come from real tracking (see QVR_BUTTON_HANDSTRACKED).
std::vector<bool> clientHandsTracked;

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

[[nodiscard]] bool handsTracked(edict_t* player)
{
    const int client = NUM_FOR_EDICT(player) - 1;
    return client >= 0 && client < static_cast<int>(clientHandsTracked.size()) &&
           clientHandsTracked[client];
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
    const glm::vec3 handRots[2] = {fieldVec(ent, f().offhandrot), fieldVec(ent, f().handrot)};

    const auto checkTrace = [&](const trace_t& trace) {
        edict_t* target = trace.ent;
        if(!target || !fieldFunc(target, f().handtouch))
        {
            return;
        }

        const glm::vec3 bonus{handTouchBonus(target)};
        const glm::vec3 tMin = vec(target->v.origin) + vec(target->v.mins) - bonus;
        const glm::vec3 tMax = vec(target->v.origin) + vec(target->v.maxs) + bonus;

        for(int h = 0; h < 2; h++)
        {
            if(boxesOverlap(hands[h] - handExtent, hands[h] + handExtent, tMin, tMax))
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
        const glm::vec3 end = hands[h] + forwardFromAngles(handRots[h]);
        checkTrace(moveTrace(vec(ent->v.origin), vec(ent->v.mins), vec(ent->v.maxs), end,
            MOVE_NORMAL, ent));
        checkTrace(moveTrace(unionCentre, -unionHalf, unionHalf, end, MOVE_NORMAL, ent));
    }

    for(int h = 1; h >= 0; h--)
    {
        const glm::vec3 end = hands[h] + forwardFromAngles(handRots[h]);
        checkTrace(moveTrace(hands[h], -handExtent, handExtent, end, MOVE_NORMAL, ent));
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

    const glm::vec3 handExtent{handHalfSize};
    const glm::vec3 bonus{handTouchBonus(target)};
    const glm::vec3 tMin = vec(target->v.absmin) - bonus;
    const glm::vec3 tMax = vec(target->v.absmax) + bonus;

    const glm::vec3 off = fieldVec(ent, f().offhandpos);
    const glm::vec3 main = fieldVec(ent, f().handpos);
    const bool offHit = boxesOverlap(off - handExtent, off + handExtent, tMin, tMax);
    const bool mainHit = boxesOverlap(main - handExtent, main + handExtent, tMin, tMax);

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

namespace qvr::physics
{

void setClientHandsTracked(int client, bool tracked)
{
    if(client >= static_cast<int>(clientHandsTracked.size()))
    {
        clientHandsTracked.resize(client + 1, false);
    }

    clientHandsTracked[client] = tracked;
}

} // namespace qvr::physics

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

extern "C" void VR_ClientPreMove(edict_t* ent)
{
    if(!active())
    {
        return;
    }

    handTouches(ent);
    weaponTouches(ent);
}

extern "C" int VR_ClientTeleport(edict_t* ent)
{
    if(!active() || !(static_cast<int>(fieldFloatOr(ent, f().vrbits0, 0.f)) & VRBITS0_TELEPORTING))
    {
        return 0;
    }

    if(!SV_RunThink(ent))
    {
        return -1;
    }

    const glm::vec3 target = fieldVec(ent, f().teleport_target);
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
    if(!active())
    {
        return;
    }

    const glm::vec3 move = fieldVec(ent, f().roomscalemove);
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

// Locomotion follows the head, not the aiming hand.
extern "C" float* VR_MoveAngles(edict_t* ent, float* fallback)
{
    return active() && f().v_viewangle >= 0 ? fieldPtr(ent, f().v_viewangle) : fallback;
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
    if(!active() || f().lastwatertime < 0)
    {
        return 1;
    }

    float& last = fieldFloat(ent, f().lastwatertime);
    const bool allow = qcvm->time - last > 0.2;
    last = static_cast<float>(qcvm->time);
    return allow;
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
