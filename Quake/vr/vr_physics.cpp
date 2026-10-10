// vr_physics.cpp -- server-side Quake VR physics: hand and weapon touches, teleport,
// room-scale movement, head-relative movement, swimming, the second think timer and touch rules.
//
// The touches, the second think timer and the touch rules are inactive unless the server runs
// Quake VR progs; the movement works with any progs (and moves a mod's shots to the gun).

#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
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
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_zancle.hpp"

#include <stdlib.h>
#include <string.h>

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

[[nodiscard]] bool finite(const glm::vec3& v)
{
    return ZA_ISFINITE(v.x) && ZA_ISFINITE(v.y) && ZA_ISFINITE(v.z);
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

// Whether `player`'s hand `which` touches `target`: an object that can be carried or pulled by its fist against the
// thing's drawn surface (held::grabTouch: the palm and the curled fingers, not a point ahead of the hand); anything else
// by the hand's box against the entity's box (and the easy-touch bonus).
// A thing a hand carries (VR_Carry_Setup's hand touch) that can't be force-grabbed: the explosive boxes.
[[nodiscard]] bool carried(edict_t* target)
{
    const func_t carry = bindings().Carry_Handtouch;
    return carry && fieldFunc(target, f().handtouch) == carry;
}

// vr_debug_carry 2: a hand's box touching a carried thing (the old test, vr_carry_grab_drawn 0): how far its fist is
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
    const bool byFist = vr_carry_grab_drawn.value && vr_box3d_hand_push_fist.value && NUM_FOR_EDICT(player) == 1;
    if(box3d::isRagdoll(NUM_FOR_EDICT(target)))
    {
        // A ragdoll: a hand on one of its limbs (its box is round all of them).
        return box3d::ragdollReach(NUM_FOR_EDICT(target), fieldVec(player, which == HAND_OFF ? f().offhandpos : f().handpos));
    }
    if(hasFlag(target, physics::FL_FORCEGRABBABLE) || (byFist && carried(target)))
    {
        // A round lying about (a shell: QC vr_reload.qc) is small, and lies flat on the floor under the lowest the fist
        // gets: taken within vr_reload_grab_slack of its surface.
        const int mi = static_cast<int>(target->v.modelindex);
        const qmodel_t* model = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
        const float slack = model && (modelmeta::has(model, modelmeta::Trait::LiveShell) ||
                                      modelmeta::has(model, modelmeta::Trait::Magazine) ||
                                      modelmeta::has(model, modelmeta::Trait::LiveRound) || modelmeta::isQuakeGrenade(model))
                                ? za::max(vr_reload_grab_slack.value, 0.f) * 0.01f * units::metresToUnits()
                                : 0.f;
        return held::grabTouch(target, player, which == HAND_OFF ? 0 : 1, slack);
    }
    if(weaponByFist(target))
    {
        // (With vr_weapon_grab_slack more if it lies on the floor or is stuck: a gun lying flat is thinner than the lowest
        // the fist gets over the floor. Not one in the air: a catch is by the closed fist on it, as a prop's.)
        // A hand at its handle (its origin; nearer than vr_weapon_grab_anywhere_min, where it is held by the handle and
        // not anywhere) with vr_weapon_grab_handle_leniency more: a gun is taken by its grip from as far off it as the
        // grip is from the fist's lowest over the floor. Not in the air: a force grab's catch takes it by its own
        // handtouch, and a weapon flying at the hand is caught by the fist on it.
        const int h = which == HAND_OFF ? 0 : 1;
        const float m2u = units::metresToUnits();
        float slack = 0.f;
        if(lyingWeapon(target))
        {
            slack = za::max(vr_weapon_grab_slack.value, 0.f) * 0.01f * m2u;
            const float leniency = za::max(vr_weapon_grab_handle_leniency.value, 0.f) * 0.01f * m2u;
            const glm::vec3 hand = fieldVec(player, which == HAND_OFF ? f().offhandpos : f().handpos);
            if(leniency > 0.f &&
                glm::distance(hand, vec(target->v.origin)) <= za::max(vr_weapon_grab_anywhere_min.value, 0.f) * 0.01f * m2u)
            {
                slack += leniency;
            }
        }
        if(held::grabTouch(target, player, h, slack))
        {
            return true;
        }
        // Not touching it: not taken (the author's notes hip1m1_2026-10-07_22-33-15, vrfiringrange_2026-10-07_22-49-20:
        // the hand's 5-unit box at its point against the box round the handle caught it with the fist ~20 cm off).
        // Without a fist (no jointed hand model) or with vr_weapon_grab_box 1: by its box, as before.
        if(!held::fist(h).empty() && !vr_weapon_grab_box.value)
        {
            return false;
        }
    }
    const glm::vec3 hand = fieldVec(player, which == HAND_OFF ? f().offhandpos : f().handpos);
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

    const bool drawn = weaponByFist(target);
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

namespace
{
void waterFeedback(edict_t* ent); // "Water splashes and sounds" below
} // namespace

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
        const glm::vec3 q = p + glm::vec3{0.f, 0.f, wet ? za::min(d, range) : -za::min(d, range)};
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
int lastVariant[static_cast<int>(WaterSound::Count)]{-1, -1, -1, -1, -1, -1, -1, -1};

// One of a water sound's recordings, at random, never the one played last: its precache index (0: not precached).
[[nodiscard]] int variant(WaterSound sound)
{
    const int s = static_cast<int>(sound);
    const int count = waterSoundFiles[s].count;
    int& prev = lastVariant[s];
    int k = count > 1 ? rand() % (prev >= 0 ? count - 1 : count) : 0;
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
    const bool in = isLiquid(contents);
    const bool crossed = in != isLiquid(was.contents) && (contents == CONTENTS_EMPTY || was.contents == CONTENTS_EMPTY);
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
    const bool onBottom = hasFlag(ent, FL_ONGROUND) || contentsAt(feet) == CONTENTS_SOLID;
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
    const float pitch = 0.88f + 0.2f * hard + 0.04f * (static_cast<float>(rand() % 1001) / 1000.f - 0.5f);
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
    if(glm::length(vel) < vr_water_splash_speed.value || contentsAt(from) != CONTENTS_EMPTY)
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
        if(c <= CONTENTS_CURRENT_0 && c >= CONTENTS_CURRENT_DOWN)
        {
            c = CONTENTS_WATER; // as SV_PointContents
        }
        if(isLiquid(c))
        {
            const glm::vec3 point = w.from + w.delta * ((f1 + f2) * 0.5);
            c = VR_LiquidContents(sv.worldmodel, &point.x, c);
        }
        if((isLiquid(c) && w.last == CONTENTS_EMPTY) || (c == CONTENTS_EMPTY && isLiquid(w.last)))
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
        if(SV_PointContents(p) > CONTENTS_WATER || !(speedMs > minSpeed)) // out of the water (or slime, lava); still
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
    const glm::vec3 added = glm::vec3{ent->v.velocity[0], ent->v.velocity[1], ent->v.velocity[2]} - was;
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
    const int mark = Hunk_LowMark();
    const int space = qcvm->num_edicts * 3;
    edict_t** list = static_cast<edict_t**>(Hunk_AllocNoFill(space * sizeof(edict_t*)));
    int found = 0;

    const bool client = isClient(ent);
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

namespace qvr::physics
{
void propTouchStats_f()
{
    Con_Printf("prop touch verification: %llu callbacks checked\n", static_cast<unsigned long long>(touchVerified));
}
}
