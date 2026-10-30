// vr_physics.cpp -- server-side Quake VR physics: hand and weapon touches, teleport,
// room-scale movement, head-relative movement, swimming, the second think timer and touch rules.
//
// The touches, the second think timer and the touch rules are inactive unless the server runs
// Quake VR progs; the movement works with any progs (and moves a mod's shots to the gun).

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

// Whether `player`'s hand `which` touches `target`: an object that can be carried or pulled by its fist against the
// thing's drawn surface (held::grabTouch: the palm and the curled fingers, not a point ahead of the hand); anything else
// by the hand's box against the entity's box (and the easy-touch bonus).
// A thing a hand carries (VR_Carry_Setup's hand touch) that can't be force-grabbed: the explosive boxes.
[[nodiscard]] bool carried(edict_t* target)
{
    const func_t carry = bindings().Carry_Handtouch;
    return carry && fieldFunc(target, f().handtouch) == carry;
}

// vr_debug_carry 2: a hand's box touching a carried thing (no fist push, another player's hand): how far its fist is
// from the thing's drawn surface.
void debugBoxTouch(edict_t* target, edict_t* player, int which)
{
    const int hand = which == HAND_OFF ? 0 : 1;
    za::Vector<glm::vec4> spheres;
    held::fistInWorld(hand, fieldVec(player, hand == 0 ? f().offhandpos : f().handpos),
        fieldVec(player, hand == 0 ? f().offhandrot : f().handrot), spheres);
    held::FistContact c;
    const bool found = held::fistContact(target, spheres, 64.f, c);
    Con_Printf("grab: %s hand, %s: the hand's box touches its box; the fist %.2f cm from its surface\n",
        hand == 0 ? "off" : "main", PR_GetString(target->v.classname), found ? c.gap / units::metresToUnits() * 100.f : 999.f);
}

// A weapon lying about or flying (QC's thrown_weapon: dropped, thrown, a monster's, one placed in a map).
[[nodiscard]] bool thrownWeapon(edict_t* target)
{
    return !strcmp(PR_GetString(target->v.classname), "thrown_weapon");
}

// Such a weapon is also taken where the fist meets its drawn shape (vr_weapon_grab_drawn): its box is a small cube round
// its handle (its origin), so a crowbar or a sword lying on the floor could be taken only by its handle, not by its bar
// or blade.
[[nodiscard]] bool weaponByFist(edict_t* target)
{
    return vr_weapon_grab_drawn.value && thrownWeapon(target);
}

// An armed monster's weapon in its hand (QC vr_armedgrunt.qc VR_AG_MakeProp): taken (a tug-of-war, or at once) only by
// the fist on its drawn shape (vr_agrunt_grab_slack), never by its box (a 20-unit cube round its grip: a hand on the
// monster's arm or chest, or swung past it, took it; the author's note vrtesthall_2026-10-24_01-10-14).
[[nodiscard]] bool armedWeapon(edict_t* target)
{
    return !strcmp(PR_GetString(target->v.classname), "vr_ag_weapon");
}

za::Vector<glm::vec4> armedFist; // handOn's: the fist's spheres in the world, on an armed monster's weapon

// An armed monster's weapon as drawn lags the server's by about a frame (the client draws the last place it was sent,
// the hand's place reaches the server a frame later), and a swung blade or a gun turned with its holder moves 5-20 cm a
// frame: the fist is also measured against the weapon's places the frames before (QC VR_AG_PropWas, vr_agrunt_grab_sweep
// of them) and midway to each, the least gap kept (the vrragtriggers_2026-10-27 notes: "I put my hand on it and grab:
// sometimes nothing happens"). Its own place is put back after. A place farther than sweepFarthest from the next is a
// jump (onto a ragdoll's limb, a teleport), not a move: the sweep stops there.
constexpr float sweepFarthest = 32.f; // units

[[nodiscard]] float angleStep(float d)
{
    return d - 360.f * za::floor((d + 180.f) / 360.f);
}

[[nodiscard]] float sweptFistGap(edict_t* target, float allowed)
{
    const FieldOffsets& fo = f();
    if(fo.vr_ag_was_o1 < 0 || fo.vr_ag_was_a1 < 0 || fo.vr_ag_was_o2 < 0 || fo.vr_ag_was_a2 < 0 || fo.vr_ag_was_n < 0)
    {
        return ZA_FLOAT_MAX;
    }
    const int frames = za::min(za::min(static_cast<int>(vr_agrunt_grab_sweep.value), 2),
        static_cast<int>(fieldFloatOr(target, fo.vr_ag_was_n, 0.f)));
    const glm::vec3 keepO = vec(target->v.origin), keepA = vec(target->v.angles);
    const auto place = [target](const glm::vec3& o, const glm::vec3& a) {
        for(int i = 0; i < 3; i++)
        {
            target->v.origin[i] = o[i];
            target->v.angles[i] = a[i];
        }
    };
    glm::vec3 prevO = keepO, prevA = keepA;
    float best = ZA_FLOAT_MAX;
    for(int k = 0; k < frames && best > allowed; k++)
    {
        const glm::vec3 o = fieldVec(target, k == 0 ? fo.vr_ag_was_o1 : fo.vr_ag_was_o2);
        const glm::vec3 a = fieldVec(target, k == 0 ? fo.vr_ag_was_a1 : fo.vr_ag_was_a2);
        if(glm::distance(o, prevO) > sweepFarthest)
        {
            break;
        }
        for(const float t : {0.5f, 1.f})
        {
            glm::vec3 at{0.f};
            for(int i = 0; i < 3; i++)
            {
                at[i] = prevA[i] + angleStep(a[i] - prevA[i]) * t;
            }
            place(glm::mix(prevO, o, t), at);
            bool surface = true;
            best = za::min(best, held::fistGap(target, armedFist, za::max(allowed, 0.25f * units::metresToUnits()), surface,
                                     allowed));
            if(best <= allowed)
            {
                break;
            }
        }
        prevO = o;
        prevA = a;
    }
    place(keepO, keepA);
    return best;
}

// Whether `player`'s hand `h` (0 off, 1 main) grips (QVR_VRBITS0_*HAND_GRABBING) or has just pressed its grip (QC
// VR_HandGrabUtil_StartedHandGrabbing's 0.125 s: the tests press it so): a grab's time.
[[nodiscard]] bool gripStarted(edict_t* player, int h)
{
    constexpr int grabBit[2] = {1 << 1, 1 << 3};
    if(f().vrbits0 >= 0 && (static_cast<int>(fieldFloat(player, f().vrbits0)) & grabBit[h]) != 0)
    {
        return true;
    }
    const float grab = fieldFloatOr(player, h == 0 ? f().offhand_lastgrabtime : f().mainhand_lastgrabtime, -1.f);
    const float letGo = fieldFloatOr(player, h == 0 ? f().offhand_lastreleasetime : f().mainhand_lastreleasetime, -1.f);
    const double now = qcvm->time;
    return grab > 0.f && now < grab + 0.125 && now > letGo && letGo + 0.125 <= grab;
}

// A weapon at rest on something (on the floor, a crate, stuck in a wall), not in the air: resting (FL_ONGROUND, or not
// moving at all) or barely moving (lyingSpeed: a gun lying as a Box3D prop is awake, not FL_ONGROUND, while the closing
// hand nudges it, and slid off the fist that would take it: the author's note of 2026-10-08, "very hard to grab guns by
// the main handle while they're in prop form"), with something solid under its handle within lyingFloorReach units (a
// test's weapon left floating with no gravity rests, in the air).
constexpr float lyingFloorReach = 8.f;
constexpr float lyingSpeed = 100.f; // units/s (a thrown or dropped weapon in flight is far faster)

[[nodiscard]] bool lyingWeapon(edict_t* target)
{
    if(static_cast<int>(target->v.movetype) == MOVETYPE_NONE)
    {
        return true;
    }
    if(!hasFlag(target, FL_ONGROUND) && glm::length(vec(target->v.velocity)) > lyingSpeed)
    {
        return false;
    }
    vec3_t start, end, zero{0.f, 0.f, 0.f};
    VectorCopy(target->v.origin, start);
    VectorCopy(target->v.origin, end);
    end[2] -= lyingFloorReach;
    return SV_Move(start, zero, zero, end, MOVE_NORMAL, target).fraction < 1.f;
}

// How far past a hand's reach (units) a weapon's handle may be for its drawn shape to be under the hand: a sword's length.
constexpr float weaponDrawnReach = 48.f;

[[nodiscard]] bool handOn(edict_t* target, edict_t* player, int which)
{
    // (An explosive box by the fist only where the fist also pushes it, vr_box3d_hand_push_fist: the listen server's own
    // player. Another player's hand still pushes with the sphere at its point, which keeps the box off its fist.)
    const bool byFist = vr_box3d_hand_push_fist.value && NUM_FOR_EDICT(player) == 1;
    const int h = which == HAND_OFF ? 0 : 1;
    const int handPos = which == HAND_OFF ? f().offhandpos : f().handpos; // (the hand's field)
    const float m2u = units::metresToUnits();
    if(const int num = NUM_FOR_EDICT(target); box3d::isRagdoll(num))
    {
        // A ragdoll: a hand on one of its limbs (its box is round all of them).
        return box3d::ragdollReach(num, fieldVec(player, handPos));
    }
    if(hasFlag(target, physics::FL_FORCEGRABBABLE) || (byFist && carried(target)))
    {
        // A round lying about (a shell: QC vr_reload.qc) is small, and lies flat on the floor under the lowest the fist
        // gets: taken within vr_reload_grab_slack of its surface.
        const int mi = static_cast<int>(target->v.modelindex);
        const qmodel_t* model = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
        const float slack = model && modelmeta::isRound(model)
                                ? za::max(vr_reload_grab_slack.value, 0.f) * 0.01f * m2u
                                : 0.f;
        return held::grabTouch(target, player, h, slack);
    }
    if(armedWeapon(target))
    {
        // (vr_agrunt_grab_leniency: a little way off its surface counts, the fist closing round a weapon that moves)
        const float slack = (vr_agrunt_grab_slack.value + za::max(vr_agrunt_grab_leniency.value, 0.f)) * 0.01f * m2u;
        if(NUM_FOR_EDICT(player) == 1 && !held::fist(h).empty())
        {
            // The fist's spheres against the weapon's surface, by distance alone (not grabTouch's inside test: a held
            // gun's model is open, and a fist 20 cm over a shotgun read as sunk in it). A thin weapon has no inside a
            // fist's sphere middle sinks into without passing near its surface.
            // Measured only for a grab (its grip held, or just pressed: QC VR_AG_PropHandTouch takes it only then) or the
            // tests' reading (vr_agrunt_debug): an open hand by it, or a fist at rest on it, asks nothing.
            const bool reading = vr_agrunt_debug.value && which == HAND_MAIN;
            if(!reading && !gripStarted(player, h))
            {
                return false;
            }
            held::fistInWorld(h, fieldVec(player, handPos), fieldVec(player, which == HAND_OFF ? f().offhandrot : f().handrot),
                armedFist);
            const float allowed = vr_carry_grab_bias.value * 0.01f * m2u + slack;
            bool surface = true;
            // (within: the tests' reading wants the gap a little way out too; a grab alone is done at the first sphere on it)
            float gap = za::min(999.f * m2u, held::fistGap(target, armedFist, za::max(allowed, 0.25f * m2u), surface,
                                                reading ? -1e30f : allowed));
            if(!surface)
            {
                return false; // (no surface to measure)
            }
            if(gap > allowed)
            {
                gap = za::min(gap, sweptFistGap(target, allowed));
            }
            if(vr_agrunt_debug.value && which == HAND_MAIN)
            {
                Cvar_SetValueQuick(&vr_agrunt_grab_gap, gap / m2u * 100.f); // (the tests' reading, cm)
            }
            return gap <= allowed;
        }
        // (No fist known: another player's hand, no jointed hand model) its point in the box the weapon is drawn in.
        return held::nearDrawn(target, fieldVec(player, handPos), za::max(slack, 0.f));
    }
    if(weaponByFist(target))
    {
        // (With vr_weapon_grab_slack more if it lies on the floor or is stuck: a gun lying flat is thinner than the lowest
        // the fist gets over the floor. Not one in the air: a catch is by the closed fist on it, as a prop's.)
        // A hand at its handle (its origin; nearer than vr_weapon_grab_anywhere_min, where it is held by the handle and
        // not anywhere) with vr_weapon_grab_handle_leniency more: a gun is taken by its grip from as far off it as the
        // grip is from the fist's lowest over the floor. Not in the air: a force grab's catch takes it by its own
        // handtouch, and a weapon flying at the hand is caught by the fist on it.
        // In the air (tossed up to re-grip it, flipped, spinning): vr_weapon_catch_handle_leniency at its handle, a trick
        // catch's (ROUND22.md, "Mid-air catches": the closing fist missed a falling axe's handle by a few centimetres).
        // Not one a force grab pulls (QC fg_state 1): its catch is its own (vr_forcegrab_catch_radius).
        const glm::vec3 hand = fieldVec(player, handPos);
        const bool atHandle =
            glm::distance(hand, vec(target->v.origin)) <= za::max(vr_weapon_grab_anywhere_min.value, 0.f) * 0.01f * m2u;
        float slack = 0.f;
        if(lyingWeapon(target))
        {
            slack = za::max(vr_weapon_grab_slack.value, 0.f) * 0.01f * m2u;
            if(atHandle)
            {
                slack += za::max(vr_weapon_grab_handle_leniency.value, 0.f) * 0.01f * m2u;
            }
        }
        else if(atHandle && fieldFloatOr(target, f().fg_state, 0.f) != 1.f)
        {
            slack = za::max(vr_weapon_catch_handle_leniency.value, 0.f) * 0.01f * m2u;
        }
        if(held::grabTouch(target, player, h, slack))
        {
            return true;
        }
        // Not touching it: not taken (the author's notes hip1m1_2026-10-07_22-33-15, vrfiringrange_2026-10-07_22-49-20:
        // the hand's 5-unit box at its point against the box round the handle caught it with the fist ~20 cm off).
        // Without a fist (no jointed hand model): by its box, as before.
        if(!held::fist(h).empty())
        {
            return false;
        }
    }
    const glm::vec3 hand = fieldVec(player, handPos);
    const glm::vec3 extent{handHalfSize};
    const glm::vec3 bonus{handTouchBonus(target)};
    const glm::vec3 origin = vec(target->v.origin);
    const bool on = boxesOverlap(hand - extent, hand + extent, origin + vec(target->v.mins) - bonus,
        origin + vec(target->v.maxs) + bonus);
    if(on && vr_debug_carry.value >= 2.f && carried(target))
    {
        debugBoxTouch(target, player, which);
    }
    return on;
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
    trace_t trace;
    if((type & MOVE_PORTALS) && VR_PortalReachMove(pass, s, mi, ma, e, type & ~MOVE_PORTALS, &trace))
    {
        return trace;
    }
    return SV_Move(s, mi, ma, e, type & ~MOVE_PORTALS, pass);
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
            if(handOn(target, ent, h == 0 ? HAND_OFF : HAND_MAIN))
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
        const auto* tracked = server::clientMove(ent);
        if(tracked && portals::reach(vec(ent->v.origin), tracked->hands[h].pos).gate)
        {
            // A folded reach never sweeps a giant box between the two rooms.
            const glm::vec3 end = tracked->hands[h].pos + forwardFromAngles(tracked->hands[h].rot);
            checkTrace(moveTrace(vec(ent->v.origin), -handExtent, handExtent, end, MOVE_NORMAL | MOVE_PORTALS, ent));
            continue;
        }
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
// A hand's muzzle is never farther from it than this (metres): no weapon is that long. A line longer is not a weapon's
// (a client's muzzle left where the hand was before a map load or a teleport: ROUND21.md, "A far button pressed at a map
// load"; the client carries it with the hand now, vr_client.cpp handMuzzle), and touches nothing.
constexpr float weaponLineMaxMetres = 4.f;

void weaponTouches(edict_t* ent)
{
    const glm::vec3 gunExtent{1.f};
    const int handPos[2] = {f().handpos, f().offhandpos};
    const int muzzlePos[2] = {f().muzzlepos, f().offmuzzlepos};
    const float handIndex[2] = {HAND_MAIN, HAND_OFF};
    const float longest = weaponLineMaxMetres * units::metresToUnits();

    for(int i = 0; i < 2; i++)
    {
        const auto* tracked = server::clientMove(ent);
        const int h = i == 0 ? 1 : 0;
        const glm::vec3 from = tracked ? tracked->hands[h].pos : fieldVec(ent, handPos[i]);
        const glm::vec3 to = tracked ? tracked->muzzlePos[h] : fieldVec(ent, muzzlePos[i]);
        if(glm::distance(from, to) > longest)
        {
            Con_DPrintf("VR: hand %d's line to its muzzle is %.0f units long: no weapon touch\n", h, glm::distance(from, to));
            continue;
        }
        const trace_t trace = moveTrace(from, -gunExtent, gunExtent, to, tracked ? MOVE_NORMAL | MOVE_PORTALS : MOVE_NORMAL, ent);

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

// Known prop callbacks have no work for a rigid prop outside an active throw when
// the toucher takes no damage, or when the contact cannot reach the damage speed.
// Box3D dispatches hard impacts separately. Hands, throws, gib grace bookkeeping
// and unknown callbacks keep their original calls and order.
[[nodiscard]] bool propTouchIsNothing(edict_t* ent, edict_t* target)
{
    const func_t fn = target->v.touch;
    const auto& touches = progs::bindings().propTouches;
    const auto* const end = touches + za::getArraySize(touches);
    if(!fn || isClient(ent) || za::find(touches, end, fn) == end ||
       fieldFloatOr(target, f().vr_rigid, 0.f) == 0.f || fieldFloatOr(target, f().throwhit, 0.f) == 0.f)
    {
        return false;
    }
    if(ent->v.takedamage == 0.f) return true;
    if(vr_prop_touch_fast.value == 0.f) return false;
    if(vr_prop_impact_damage.value == 0.f) return true;
    // Non-gibs have no state writes before VR_Prop_Flung's speed test. Its mass
    // leniency is bounded below by vr_weight_lenient_min, including throwScale's
    // weight cap. An approach can never exceed its input velocity's length.
    // Stay below that universal lower bound, with a margin for QC rounding:
    // no mass/model query and no QC call for resting contacts against crates.
    // Gib grace bookkeeping and every active throw retain their original calls.
    if(fieldFloatOr(target, f().vr_gib, 0.f) != 0.f)
    {
        return false;
    }
    const float least = za::max(1.f, vr_prop_impact_min_speed.value * units::metresToUnits()) *
        za::clamp(vr_weight_lenient_min.value, 0.01f, 1.f) * 0.99f;
    const glm::vec3 own{target->v.velocity[0], target->v.velocity[1], target->v.velocity[2]};
    const glm::vec3 other{ent->v.velocity[0], ent->v.velocity[1], ent->v.velocity[2]};
    return glm::dot(own, own) < least * least || glm::dot(own - other, own - other) < least * least;
}

za::U64 touchVerified = 0;
// The per-frame dedup for verifyPropTouch: file scope, not function-local statics.
int touchVerifyFrame = -1;
unsigned touchSampled = 0;
void verifyPropTouch(edict_t* ent, edict_t* target)
{
    if(vr_prop_touch_verify.value == 0.f) return;
    if(touchVerifyFrame != host_framecount) { touchVerifyFrame = host_framecount; touchSampled = 0; }
    const auto& types = progs::bindings().propTouches;
    const auto* which = za::find(types, types + za::getArraySize(types), target->v.touch);
    const unsigned bit = 1u << static_cast<unsigned>(which - types);
    if(touchSampled & bit) return;
    touchSampled |= bit;
    const int count = qcvm->num_edicts;
    const za::SizeT bytes = static_cast<za::SizeT>(count) * qcvm->edict_size;
    za::Vector<byte> edicts(bytes);
    za::Vector<float> globals(static_cast<za::SizeT>(qcvm->progs->numglobals));
    memcpy(edicts.data(), qcvm->edicts, bytes);
    memcpy(globals.data(), qcvm->globals, globals.size() * sizeof(float));
    const int datagram = sv.datagram.cursize, reliable = sv.reliable_datagram.cursize;
    za::Vector<byte> messages(static_cast<za::SizeT>(datagram + reliable));
    if(datagram) memcpy(messages.data(), sv.datagram.data, datagram);
    if(reliable) memcpy(messages.data() + datagram, sv.reliable_datagram.data, reliable);
    callField(target, ent, target->v.touch);
    if(qcvm->num_edicts != count || memcmp(edicts.data(), qcvm->edicts, bytes) ||
       sv.datagram.cursize != datagram || sv.reliable_datagram.cursize != reliable ||
       (datagram && memcmp(messages.data(), sv.datagram.data, datagram)) ||
       (reliable && memcmp(messages.data() + datagram, sv.reliable_datagram.data, reliable)))
        Sys_Error("prop touch verify: supposedly inactive touch changed entity/network state");
    const za::SizeT time = reinterpret_cast<float*>(&pr_global_struct->time) - qcvm->globals;
    // FTE's shared unnamed temporaries are scratch, like the argument registers.
    // Compare declared globals (including game counters) rather than that workspace.
    for(int d = 0; d < qcvm->progs->numglobaldefs; d++)
    {
        const ddef_t& def = qcvm->globaldefs[d];
        const int type = def.type & ~DEF_SAVEGLOBAL;
        const za::SizeT i = def.ofs;
        const char* name = PR_GetString(def.s_name);
        if(!name[0] || i < OFS_PARM7 + 3 || i == time || type == ev_void) continue;
        const za::SizeT size = type == ev_vector ? 3 : 1;
        if(i + size <= globals.size() && memcmp(&globals[i], &qcvm->globals[i], size * sizeof(float)))
            Sys_Error("prop touch verify: supposedly inactive touch changed QC global %s", name);
    }
    touchVerified++;
}

// Body touches: triggers and touchable non-solids always; other solids only with
// vr_gameplayfix_touchsolids. Bodies also "hand"-touch things when hands aren't tracked.
void touch(edict_t* ent, edict_t* target)
{
    if(VR_AD_PelletsPass(ent, target)) return; // AD support: overlapping pellets never consume one another
    if(!absBoxesOverlap(ent, target))
    {
        return;
    }

    // A gib (a head, a small gib) in another's box: nothing for its touch to do unless it is thrown (QC VR_Gib_Touch;
    // gibs meeting hard are Box3D's hits). A pile of them moving was n x n QC touches a frame.
    if(fieldFloatOr(ent, f().vr_gib, 0.f) != 0.f && fieldFloatOr(target, f().vr_gib, 0.f) != 0.f &&
        fieldFloatOr(target, f().throwhit, 1.f) != 0.f)
    {
        return;
    }
    if(propTouchIsNothing(ent, target))
    {
        verifyPropTouch(ent, target);
        return;
    }

    const int solid = solidOf(target);
    if(target->v.touch && (solid == SOLID_TRIGGER || solid == SOLID_NOT_BUT_TOUCHABLE ||
                              vr_gameplayfix_touchsolids.value))
    {
        callField(target, ent, target->v.touch);
    }

    const func_t handtouch = fieldFunc(target, f().handtouch);
    // (Item Pickup Mode Vanilla, vr_carry 0: walking into a pickup takes it, as in Quake; QC's handtouch with the fake
    // hand. Only pickups: the rest of the body's touches stay vr_body_interactions'.)
    const bool vanillaPickup = vr_carry.value == 0.f && (static_cast<int>(target->v.flags) & FL_ITEM) != 0;
    if(handtouch && !target->free && isClient(ent) &&
        (!fieldFloatOr(ent, f().ishuman, 0.f) || vr_body_interactions.value || vanillaPickup ||
            fieldFloatOr(ent, f().vr_flatscreen, 0.f) != 0.f || !handsTracked(ent))) // (flatscreen: this player's own)
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
    // Each hand on it touches it (round 21, "Hands: both work"): with both on it, only the off hand's touch ran, so the
    // main hand's grip on a thing the off hand rested on (or held a gun against) did nothing.
    const bool offHit = handOn(target, ent, HAND_OFF);
    const bool mainHit = handOn(target, ent, HAND_MAIN);

    if(offHit)
    {
        setHandtouchParams(HAND_OFF, ent, target);
        callField(target, ent, handtouch);
    }
    if(mainHit && !target->free && fieldFunc(target, f().handtouch) && solidOf(target) != SOLID_NOT)
    {
        setHandtouchParams(HAND_MAIN, ent, target);
        callField(target, ent, fieldFunc(target, f().handtouch));
    }
}

[[nodiscard]] bool handsReach(edict_t* ent, edict_t* target)
{
    const glm::vec3 reach{handHalfSize + easyHandTouchBonus};
    const glm::vec3 tMin = vec(target->v.absmin);
    const glm::vec3 tMax = vec(target->v.absmax);

    const bool drawn = weaponByFist(target) || armedWeapon(target);
    for(const int ofs : {f().offhandpos, f().handpos})
    {
        const glm::vec3 hand = fieldVec(ent, ofs);
        if(boxesOverlap(hand - reach, hand + reach, tMin, tMax) || (drawn && held::nearDrawn(target, hand, reach.x)))
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

extern "C" void VR_ClientPreMove(edict_t* ent)
{
    QVR_PROFILE("vr hand touches");
    hull::walkTestFrame(ent); // vr_hull_walktest: the random walk drives the player
    server::rebaseHands(ent);
    if(!active())
    {
        return;
    }

    waterFeedback(ent); // hands and guns slapping the water, wading
    handTouches(ent);
    weaponTouches(ent);
}

// SV_Physics_Client, before the move: a teleport, or a hand hanging from a ledge or mantling (vr_climb.cpp) instead of
// the move: 1 to the post-think, -1 the entity freed, 0 the move as usual.
extern "C" void VR_PortalClientCross(edict_t* ent); // vr_portals.cpp

extern "C" int VR_ClientSpecialMove(edict_t* ent)
{
    VR_PortalClientCross(ent); // through a teleporter as the head reaches it (vr_portals_walk), then the move as usual
    const int teleport = VR_ClientTeleport(ent);
    if(teleport == 1 || teleport == -1)
    {
        return teleport;
    }
    VR_ProfileBegin("climb");
    const int climb = VR_ClientClimb(ent);
    VR_ProfileEnd();
    return climb;
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
    // the arc) and where the player fits. (Written to turn a NaN away too.)
    const glm::vec3 target = move->teleportTarget;
    if(!(glm::distance(vec(ent->v.origin), target) <= za::max(vr_teleport_range.value, 100.f) * 1.5f + 64.f))
    {
        return 0;
    }
    vec3_t t{target.x, target.y, target.z};
    if(SV_Move(t, ent->v.mins, ent->v.maxs, t, MOVE_NOMONSTERS, ent).startsolid)
    {
        return 0;
    }

    ent->v.teleport_time = static_cast<float>(qcvm->time) + 0.3f;
    if(f().carry_teleported >= 0)
    {
        // What the hands carry follows them from the player for a moment (vr_carry.qc VR_Carry_FollowTeleported), not
        // along a line from where it was, which a wall would cut (it was dropped as stuck).
        fieldFloat(ent, f().carry_teleported) = static_cast<float>(qcvm->time) + 0.3f;
    }
    for(int i = 0; i < 3; i++)
    {
        ent->v.origin[i] = ent->v.oldorigin[i] = target[i];
    }

    return 1;
}

// Physical walking in the play space: a second, horizontal move with collision.
extern "C" void VR_ClientRoomscaleMove(edict_t* ent)
{
    // With no room-scale move, through a teleporter the move just made took the torso into, now (with one, after it,
    // below): the tick's message then sends him carried, never past the gate's plane and not yet carried (the client's
    // eye already through it while the server's PVS was the source room's: the room seen lost its doors and floors for
    // a frame; ROUND21.md, "A frame seen through after a teleporter").
    const VrMove* vrMove = server::clientMove(ent);
    if(!vrMove)
    {
        VR_PortalClientCross(ent);
        return;
    }

    // (Units per second of the world's time; the player's frame in its own time is longer: VR_PlayerMoveSpeedup.)
    const glm::vec3 move = vrMove->roomscaleMove / static_cast<float>(VR_PlayerMoveSpeedup());
    if((move.x == 0.f && move.y == 0.f) || !ZA_ISFINITE(move.x) || !ZA_ISFINITE(move.y))
    {
        VR_PortalClientCross(ent);
        return;
    }

    vec3_t oldVelocity;
    VectorCopy(ent->v.velocity, oldVelocity);
    const float oldYaw = ent->v.v_angle[YAW]; // (the crossing turns v_angle by exactly the gate's yaw; angles[YAW] becomes the head's)
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

    // Test the completed room-scale move while its direction is still available.
    // A crossing also rotates the original stick/falling velocity into the new room.
    VR_PortalClientCross(ent);
    const float turn = glm::radians(ent->v.v_angle[YAW] - oldYaw);
    const float c = za::cos(turn), s = za::sin(turn);
    const glm::vec3 restored{oldVelocity[0] * c - oldVelocity[1] * s,
        oldVelocity[0] * s + oldVelocity[1] * c, oldVelocity[2]};
    ent->v.velocity[0] = restored.x;
    ent->v.velocity[1] = restored.y;
    ent->v.velocity[2] = restored.z;
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
    if(!(glm::length(delta) <= 96.f)) // (a NaN turned away too)
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

namespace
{

// The walkable floor each player last stood on (SV_FlyMove, SV_WalkMove's step down): its normal, and when.
struct GroundPlane
{
    glm::vec3 normal{0.f, 0.f, 1.f};
    double time = -1.0;
};

za::Array<GroundPlane, MAX_SCOREBOARD + 1> groundPlanes{};

} // namespace

extern "C" void VR_GroundPlaneMet(edict_t* ent, const float* normal)
{
    const int num = NUM_FOR_EDICT(ent);
    if(num >= 1 && num <= svs.maxclients && num < static_cast<int>(groundPlanes.size()))
    {
        groundPlanes[num] = {{normal[0], normal[1], normal[2]}, qcvm->time};
    }
}

// SV_Physics_Client, gravity just added (before: the velocity without it). On a walkable slope (vr_slope_walk) only the
// part of it into the slope is kept: the floor takes it all, and none of it is left along the slope, downhill. Quake
// keeps that part: about 5 units a second downhill every frame, which its friction takes away at once (that is why you
// don't slide where you stand); walking uphill from a standstill, it took most of what a slow stick adds a frame
// (10 x the wish speed a second): under about 75 units a second (a quarter stick) you never got going up a slope or a
// stairs' ramp, and once stopped there (a jump landing on it, the edge friction of MG1's start stairs) you crawled.
// Flat floors are Quake's exactly.
extern "C" void VR_GroundGravity(edict_t* ent, const float* before)
{
    const int num = NUM_FOR_EDICT(ent);
    if(!vr_slope_walk.value || num < 1 || num > svs.maxclients || num >= static_cast<int>(groundPlanes.size()) ||
        !(static_cast<int>(ent->v.flags) & FL_ONGROUND))
    {
        return;
    }
    const GroundPlane& g = groundPlanes[num];
    const double age = qcvm->time - g.time;
    if(g.time < 0.0 || age < 0.0 || age > 0.1 || g.normal.z <= 0.7f || g.normal.z >= 0.9999f)
    {
        return;
    }
    const glm::vec3 was{before[0], before[1], before[2]};
    const glm::vec3 added = geom::vec(ent->v.velocity) - was;
    const glm::vec3 now = was + g.normal * glm::dot(added, g.normal);
    for(int i = 0; i < 3; ++i)
    {
        ent->v.velocity[i] = now[i];
    }
}

extern "C" void VR_OnWaterLevelChange(edict_t* ent, float oldWaterLevel)
{
    if(active() && f().lastwatertime >= 0 && ent->v.waterlevel != oldWaterLevel)
    {
        fieldFloat(ent, f().lastwatertime) = static_cast<float>(qcvm->time);
    }
}

#ifdef QVR_TRACY
namespace
{
constexpr vr_tracy_loc touchLinksZone{"VR touch links", "QVR", __FILE__, __LINE__, 0}; // a player's only
} // namespace
#endif

extern "C" int VR_TouchLinks(edict_t* ent)
{
    if(!active())
    {
        return 0;
    }

    // Unlike Ironwail's trigger-only area walk, hands can reach outside the body's box and touch
    // non-triggers: the edicts linked near the body's box and each hand's, triggers and solids (every
    // touchable one is linked: only SOLID_NOT isn't), found down the area nodes rather than by a scan
    // of every edict for every mover, and taken in edict order as that scan did (touch functions may
    // relink, hence the copy).
    const bool client = isClient(ent);
#ifdef QVR_TRACY
    // A player's (a few a tick): the touches round his body and hands, zoned (the C6 hitches: ROUND22.md).
    const vr_tracy_loc* const zone = client ? &touchLinksZone : nullptr;
    if(zone) VR_TracyBeginLoc(zone);
#endif
    const int mark = Hunk_LowMark();
    const int space = qcvm->num_edicts * 3;
    edict_t** list = static_cast<edict_t**>(Hunk_AllocNoFill(space * sizeof(edict_t*)));
    int found = 0;

    SV_AreaEdictsUnordered(ent->v.absmin, ent->v.absmax, list, &found, space);
    if(client)
    {
        // handsReach's, and a weapon's length more: a weapon's drawn shape reaches that far from its box.
        const glm::vec3 reach{handHalfSize + easyHandTouchBonus + (vr_weapon_grab_drawn.value ? weaponDrawnReach : 0.f)};
        for(const int ofs : {f().offhandpos, f().handpos})
        {
            const glm::vec3 hand = fieldVec(ent, ofs);
            const float lo[3] = {hand.x - reach.x, hand.y - reach.y, hand.z - reach.z};
            const float hi[3] = {hand.x + reach.x, hand.y + reach.y, hand.z + reach.z};
            SV_AreaEdictsUnordered(lo, hi, list, &found, space);
        }
    }
    za::quickSort(list, list + found); // edict order (the edicts are one array), each once
    found = static_cast<int>(za::unique(list, list + found) - list);

    int count = 0;
    for(int i = 0; i < found; i++)
    {
        edict_t* target = list[i];
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
            QVR_TRACY_SCOPE(handZone, "VR hand touch");
            handTouch(ent, target);
        }
    }

    Hunk_FreeToLowMark(mark);
#ifdef QVR_TRACY
    if(zone) VR_TracyEnd();
#endif
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

namespace qvr::physics
{
void propTouchStats_f()
{
    Con_Printf("prop touch verification: %llu callbacks checked\n", static_cast<unsigned long long>(touchVerified));
}

void grabBench(); // (below: in the server's progs)

void grabBench_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_agrunt_grab_bench: no game\n");
        return;
    }
    qcvm_t* const old = qcvm;
    if(old != &sv.qcvm)
    {
        if(old)
        {
            PR_SwitchQCVM(nullptr);
        }
        PR_SwitchQCVM(&sv.qcvm);
    }
    grabBench();
    if(old != &sv.qcvm)
    {
        PR_SwitchQCVM(nullptr);
        if(old)
        {
            PR_SwitchQCVM(old);
        }
    }
}

void grabBench()
{
    // The armed monsters' weapon nearest your main hand, your main fist against it (held::benchFistGap).
    edict_t* player = sv.active && svs.maxclients > 0 ? EDICT_NUM(1) : nullptr;
    if(!player || held::fist(1).empty())
    {
        Con_Printf("vr_agrunt_grab_bench: no player or no fist\n");
        return;
    }
    const glm::vec3 hand = fieldVec(player, f().handpos);
    edict_t* best = nullptr;
    float bestD = 1e30f;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(armedWeapon(e))
        {
            const float d = glm::distance(hand, vec(e->v.origin));
            if(d < bestD)
            {
                bestD = d;
                best = e;
            }
        }
    });
    if(!best)
    {
        Con_Printf("vr_agrunt_grab_bench: no armed monster's weapon\n");
        return;
    }
    held::fistInWorld(1, hand, fieldVec(player, f().handrot), armedFist);
    const float m2u = units::metresToUnits();
    held::benchFistGap(best, armedFist, vr_carry_grab_bias.value * 0.01f * m2u + vr_agrunt_grab_slack.value * 0.01f * m2u,
        Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 200);
}

bool fistOnArmedWeapon(edict_t* player, int hand)
{
    if(!player || hand < 0 || hand > 1)
    {
        return false;
    }
    const glm::vec3 at = fieldVec(player, hand == 0 ? f().offhandpos : f().handpos);
    const float reach = handHalfSize + weaponDrawnReach;
    const float lo[3] = {at.x - reach, at.y - reach, at.z - reach};
    const float hi[3] = {at.x + reach, at.y + reach, at.z + reach};
    const int mark = Hunk_LowMark();
    const int space = qcvm->num_edicts;
    edict_t** list = static_cast<edict_t**>(Hunk_AllocNoFill(space * sizeof(edict_t*)));
    int found = 0;
    SV_AreaEdictsUnordered(lo, hi, list, &found, space);
    bool on = false;
    for(int i = 0; i < found && !on; i++)
    {
        on = !list[i]->free && armedWeapon(list[i]) && handOn(list[i], player, hand == 0 ? HAND_OFF : HAND_MAIN);
    }
    Hunk_FreeToLowMark(mark);
    return on;
}
}
