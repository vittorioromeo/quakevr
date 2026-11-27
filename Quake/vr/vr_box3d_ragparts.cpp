#include "vr_alloccount.h"
#include "vr_progs.hpp"
// vr_box3d_ragparts.cpp -- a ragdoll's parts: blasts, cuts, the players' grabs and holds, points, parts in a box, limbs
// and heads.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_goresound.hpp"
#include "vr_flatplay.hpp"
#include "vr_handfit.hpp"
#include "vr_physics.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"


namespace box3d::detail
{
// A blast of `damage` at `at` (box3d::blast) on a ragdoll's parts: each thrown as a prop of the whole ragdoll's mass
// would be (thrown together, the nearer parts harder: it tumbles), if the blast sees its middle.
void blastRagdoll(RagdollBodies& r, const glm::vec3& at, float damage)
{
    if(r.active.on)
    {
        ragactive::weaken(r.active, 0.35f); // (an active death blasted: its muscles mostly gone)
    }
    const float reach = damage + 40.f;
    constexpr float referenceMass = 6.f; // kg (blast's)
    float total = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        total += partCut(r, b) ? 0.f : b3Body_GetMass(r.body[static_cast<za::SizeT>(b)]);
    }
    const float strength = za::max(tune(EDICT_NUM(r.num), Tune::Blast), 0.f); // (one class lookup per blast)
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(body));
        const float distance = glm::distance(centre, at);
        const float points = damage - 0.5f * distance;
        if(distance > reach || points <= 0.f)
        {
            continue;
        }
        vec3_t from, to;
        store(at, from);
        store(centre, to);
        const trace_t tr = SV_Move(from, vec3_origin, vec3_origin, to, MOVE_NOMONSTERS, nullptr);
        if(tr.fraction < 1.f || tr.startsolid)
        {
            continue;
        }
        glm::vec3 dir = distance > 0.01f ? (centre - at) / distance : glm::vec3{0.f, 0.f, 1.f};
        dir = glm::normalize(dir + glm::vec3{0.f, 0.f, 0.35f});
        const float speed = strength *
                            za::min(4.f * points * CLAMP(0.5f, za::sqrt(referenceMass / za::max(total, 1e-3f)), 2.f), 600.f);
        b3Body_ApplyLinearImpulse(body, world->toM(dir * (speed * b3Body_GetMass(body))), b3Body_GetWorldCenter(body), true);
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d part %d blasted: %.0f points, %.0f u/s\n", r.num, b, points, speed);
        }
    }
}

// The part of edict `num`'s ragdoll nearest `at` (units); a null body if it isn't one.
[[nodiscard]] b3BodyId ragdollPartNear(int num, const glm::vec3& at)
{
    if(num <= 0 || num >= static_cast<int>(world->slots.size()) || world->slots[num].ragdoll < 0)
    {
        return b3_nullBodyId;
    }
    const RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
    b3BodyId best = b3_nullBodyId;
    float bestD = 1e30f;
    for(int b = 0; b < r.count; b++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const b3AABB box = b3Body_ComputeAABB(body);
        const glm::vec3 lo = world->toU(box.lowerBound), hi = world->toU(box.upperBound);
        const float d = glm::distance(glm::clamp(at, lo, hi), at) + 0.01f * glm::distance((lo + hi) * 0.5f, at);
        if(d < bestD)
        {
            bestD = d;
            best = body;
        }
    }
    return best;
}

namespace
{
// ---- Decapitation (ROUND21.md, "Decapitation"; QC vr_decap.qc) ----
// A slash at the neck cuts the head off: the rig's head bone and the bones on it (ragdoll::headBones: a rottweiler's jaw)
// lose their bodies (Box3D takes their joints with them) and their vertices are drawn at the neck (vr_ragdoll.cpp
// neckOf). The head flies off as a gib of its own (QC: ragdollCut): from where the head part was, turned as it was, at
// its velocity plus vr_decap_head_speed of the blade's and vr_decap_head_lift up, spinning as the blade's push at its
// base turns it (the share of the blade's speed across the neck over the head's lever from the neck).

constexpr float headSpinMost = 12.f; // rad/s: a cut head's spin at most (about two turns a second)
constexpr float neckReach = 4.f;     // units (times its scale): a hit this near the neck is on the head (ragdollHeadAt)

void endGrab(int index); // (below: "Holding a ragdoll's limb")
} // namespace

// Cuts the limb at joint `bone` off `r` (Limb gore: the bone and every bone on it not cut yet; the head: its bones,
// ragdoll::headBones); `launch`: the piece launched by `blade` (units/s), else (a saved game's) only cut. False if it
// isn't a limb of its rig or it is cut already. The piece as it was cut (ragdollCut 0-3): its middle (the head's: its
// own bone's vertices'; a limb's: ragdoll::limbMiddle of what it takes, its model's origin), turned as `bone` was.
bool cutLimb(RagdollBodies& r, edict_t* ent, int bone, const glm::vec3& blade, bool launch)
{
    const ragdoll::Rig& rig = *r.rig;
    const bool isHead = bone == rig.head && bone >= 0;
    if(!isHead && !ragdoll::limbJoint(rig, bone))
    {
        return false;
    }
    const uint32_t bones = (isHead ? ragdoll::headBones(rig) : ragdoll::limbBones(rig, bone)) & ~r.cut;
    const int head = bone;
    if(!bones || head >= r.count || (r.cut & (1u << head)))
    {
        return false;
    }
    const int parent = ragdoll::uncutParent(rig, head, r.cut);
    if(parent < 0)
    {
        return false;
    }
    // The piece as it is (its middle), and the joint (its pivot on the part it is cut from).
    const b3BodyId hb = r.body[static_cast<za::SizeT>(head)];
    const b3WorldTransform hx = b3Body_GetTransform(hb);
    const glm::vec3 mid = ragdoll::limbMiddle(rig, isHead ? (1u << head) : bones);
    r.headRot = fromB3(hx.q);
    r.headMid = r.headRot * (mid * r.scale) + world->toU(hx.p);
    r.lastCut = bone;
    r.lastCutBones = bones;
    const b3WorldTransform px = b3Body_GetTransform(r.body[static_cast<za::SizeT>(parent)]);
    const glm::vec3 neck = fromB3(px.q) * (rig.bones[head].pivot * r.scale) + world->toU(px.p);
    r.headVel = world->toU(b3Body_GetLinearVelocity(hb));
    r.headSpin = glmv(b3Body_GetAngularVelocity(hb));
    if(launch)
    {
        // (On a flat screen faster, vr_flat_cutoff_fling_mult: his note 2026-11-10.)
        const float flat = qvr::flatplay::active() ? za::max(vr_flat_cutoff_fling_mult.value, 0.f) : 1.f;
        const float share = za::max(vr_decap_head_speed.value, 0.f) * flat;
        r.headVel += blade * share + glm::vec3{0.f, 0.f, za::max(vr_decap_head_lift.value, 0.f) * (isHead ? 1.f : 0.5f) * flat};
        const float lever = glm::distance(r.headMid, neck);
        const glm::vec3 n = lever > 0.1f ? (r.headMid - neck) / lever : glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 across = blade - n * glm::dot(blade, n);
        if(glm::length(across) > 1.f)
        {
            // (Pushed at its base, across the neck: it turns about base x push.)
            r.headSpin += glm::normalize(glm::cross(n, across)) *
                          za::min(glm::length(across) * share / za::max(lever, 2.f), headSpinMost);
        }
    }
    // The hands holding it let go: their joints go with its body.
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        if(g.num == r.num && g.part >= 0 && g.part < ragdoll::maxBones && (bones & (1u << g.part)))
        {
            endGrab(i);
        }
    }
    for(int b = 0; b < r.count; b++)
    {
        if(bones & (1u << b))
        {
            b3DestroyBody(r.body[static_cast<za::SizeT>(b)]);
        }
    }
    r.cut |= bones;
    for(int b = 0; b < r.count; b++)
    {
        if(r.cut & (1u << b)) // (every cut bone: one cut before may have named a part cut now)
        {
            r.body[static_cast<za::SizeT>(b)] = r.body[static_cast<za::SizeT>(ragdoll::uncutParent(rig, b, r.cut))];
        }
        else
        {
            b3Body_SetAwake(r.body[static_cast<za::SizeT>(b)], true);
        }
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d %s %s cut off%s: at %.1f %.1f %.1f, %.0f u/s, spin %.1f rad/s; %d parts left\n", r.num,
            PR_GetString(ent->v.classname), rig.bones[bone].name, launch ? "" : " (made so again)", r.headMid.x, r.headMid.y,
            r.headMid.z, glm::length(r.headVel), glm::length(r.headSpin), partsLeft(r));
    }
    if(fields().vr_limbcut >= 0)
    {
        fieldFloat(ent, fields().vr_limbcut) = static_cast<float>(r.cut);
    }
    return true;
}

// Cuts `r`'s head off (cutLimb at its rig's head).
bool cutHead(RagdollBodies& r, edict_t* ent, const glm::vec3& blade, bool launch)
{
    return r.rig->head >= 0 && cutLimb(r, ent, r.rig->head, blade, launch);
}

namespace
{
// ---- Holding a ragdoll's limb (vr_ragdoll_grab) ----
// A hand gripping on a limb (QC's VR_Ragdoll_Handtouch: ragdollgrab) holds it by a kinematic body at the hand and a
// motor joint to the limb: its spring (at most vr_ragdoll_grab_force) keeps the limb as it was in the hand, so the limb
// follows the hand through the joint chain and the rest of the body hangs from it, swings and is dragged; two hands, two
// joints. Let go of (ragdollrelease), the limb keeps the hand's throw (vr_ragdoll_throw). (Force grab never targets a
// ragdoll: its pull of a limb, ragdollpull, was removed 2026-10-09, TECHDEBT_2026-10-09.md 3.)

constexpr float grabHertz = 8.f;        // the hold's spring (linear; its turn half that)
constexpr float grabLever = 0.04f;      // m: its most torque, the most force at this lever
constexpr float grabStuck = 40.f;       // units: a limb this far from its hand
constexpr double grabStuckTime = 0.5;   // s: this long is let go of (behind a wall, a door shut on it)
constexpr float handleFollow = 12.f;    // 1/s: a blade's handle hold's point on the limb takes the drawn handle's place (what 17)

// A blade's spring's rate (Hz; ragdollTug what 12): Moving: Stiffness, its N per m times Blade Hold Strength (the Hz by
// its square root). (At a limb grab's 8 it held a grunt's chest 15-40 cm off a hand moving it, its pull a tenth to half of
// its most: the forces barely mattered. vr_embed_test 27, 2026-11-08.)
[[nodiscard]] float bladeHertz()
{
    return za::max(vr_embed_move_hertz.value * za::sqrt(za::max(vr_embed_hold_strength.value, 0.f)), 0.5f);
}

// A blade's hold's place of its wound from the hand turned `turn` now (bladeReach as it took hold, turned by vr_embed_move_turn
// of the hand's turn since: a wrist pointing the sword up overhead takes the body on it up, not out in front; his 2026-11-10).
[[nodiscard]] glm::vec3 bladeReachNow(const World::RagdollGrab& g, const glm::quat& turn)
{
    if(g.bladeReach == glm::vec3{0.f})
    {
        return g.bladeReach;
    }
    const glm::quat since = glm::normalize(turn * glm::inverse(g.bladeTurn));
    return glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, since, za::clamp(vr_embed_move_turn.value, 0.f, 1.f)) * g.bladeReach;
}

// The share left now of a handle hold's gap taken at `since` (eased away: smoothstep over vr_embed_move_match s).
[[nodiscard]] float gapLeft(double since, float over)
{
    const float t = over > 0.f ? za::clamp(static_cast<float>(qcvm->time - since) / over, 0.f, 1.f) : 1.f;
    return 1.f - t * t * (3.f - 2.f * t);
}

[[nodiscard]] float handleGapLeft(double since)
{
    return gapLeft(since, za::max(vr_embed_move_match.value, 0.f));
}

// What is left of a ragdoll's handle hold's gap now (bladeGap).
[[nodiscard]] glm::vec3 bladeGapNow(const World::RagdollGrab& g)
{
    if(!g.bladeHandle || g.bladeGap == glm::vec3{0.f})
    {
        return glm::vec3{0.f};
    }
    return g.bladeGap * handleGapLeft(g.bladeGapAt);
}

// A handle hold's stiffness (Hz, its place and its turn): Moving: Handle's Stiffness.
[[nodiscard]] float handleHertz()
{
    return za::max(vr_embed_move_handle_hertz.value, 0.5f);
}

[[nodiscard]] int grabIndex(int player, int hand)
{
    for(int i = 0; i < static_cast<int>(world->ragdollGrabs.size()); i++)
    {
        const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        if(g.player == player && g.hand == hand)
        {
            return i;
        }
    }
    return -1;
}

void endGrab(int index)
{
    World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    if(B3_IS_NON_NULL(g.joint) && b3Joint_IsValid(g.joint))
    {
        b3DestroyJoint(g.joint, true);
    }
    if(B3_IS_NON_NULL(g.anchor) && b3Body_IsValid(g.anchor))
    {
        b3DestroyBody(g.anchor);
    }
    world->ragdollGrabs.erase(world->ragdollGrabs.begin() + index);
}
} // namespace

void dropGrabsOf(int num)
{
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        if(world->ragdollGrabs[static_cast<za::SizeT>(i)].num == num)
        {
            endGrab(i);
        }
    }
}

// The ragdoll of edict `num`, or null.
[[nodiscard]] RagdollBodies* ragdollOf(int num)
{
    if(!world || world->generation != worldGeneration() || num <= svs.maxclients || num >= static_cast<int>(world->slots.size()) || world->slots[num].ragdoll < 0)
    {
        return nullptr;
    }
    return &world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
}

namespace
{
// Hand `hand` (QC's: 1 the main) of `player`: its point and its turn (world units).
void handPose(edict_t* player, int hand, glm::vec3& at, glm::quat& turn)
{
    at = fieldVec(player, progs::handFields(hand).pos);
    const glm::vec3 angles = fieldVec(player, progs::handFields(hand).rot);
    turn = glm::normalize(glm::quat_cast(held::axesFromAngles(&angles[0], true)));
}
} // namespace

// The part of ragdoll `r` whose surface is nearest `at` (units), and how far (units); -1 if none.
[[nodiscard]] int nearestPart(const RagdollBodies& r, const glm::vec3& at, float& distance)
{
    int best = -1;
    distance = 1e30f;
    for(int b = 0; b < r.count; b++)
    {
        b3Vec3 closest;
        const float d = b3Body_GetClosestPoint(r.body[static_cast<za::SizeT>(b)], &closest, world->toM(at)) * world->m2u;
        if(d < distance)
        {
            distance = d;
            best = b;
        }
    }
    return best;
}

namespace
{
// A limb's distance (units) from `p` (the world; 0 in it: its closest point to a point in it is the point itself), for
// handFit's shape (handfit::Shape::ofDistance; `part` its b3BodyId).
[[nodiscard]] float limbDistance(const void* part, const glm::vec3& p)
{
    b3Vec3 closest;
    return b3Body_GetClosestPoint(*static_cast<const b3BodyId*>(part), &closest, world->toM(p)) * world->m2u;
}

// The hand on the limb (vr_ragdoll_grab_fit; as a carried prop against the fingers: vr_held_surface_fit; ROUND21.md,
// "Hands on held ragdolls"). The limb taken where the hand's middle was held that point at the hand, a few units short
// of the limb (the hand reaches vr_ragdoll_grab_reach) or sunk into it (a caught limb, by its middle): the hand was drawn
// off the limb, or in it, its fingers closed on nothing. Instead the hand is moved along the way its palm faces
// (grip::handFrame) until its palm's skin (a patch of it round its middle) rests on the limb, or, if the limb is beside
// the palm, until the closed hand (held::fist) touches it: towards it from a gap, back out of it from in it, less
// vr_held_fit_gap; the point held at the hand is that many units on, so the limb is held on the palm and the fingers
// close round it (vr_view.cpp). Unchanged if no move within the reach (and a little more) does it (the back of the hand
// on it). `held`: the limb's point the hand would hold without it (world); the world point to hold at the hand, and how
// far the hand moved along its palm's normal (`moved`).
[[nodiscard]] glm::vec3 handFit(int pnum, int hand, const glm::vec3& at, const glm::quat& turn, b3BodyId part,
    const glm::vec3& held, float* moved = nullptr)
{
    if(moved)
    {
        *moved = 0.f;
    }
    // (each player's own hand's frame and fist, as his client measured them: B3d; a player whose fist has not come up
    // yet, none)
    const edict_t* player = pnum >= 1 && pnum <= svs.maxclients && sv.qcvm.edicts
                                ? (const edict_t*)((const byte*)sv.qcvm.edicts + pnum * sv.qcvm.edict_size)
                                : nullptr;
    const za::Vector<glm::vec4>& fist = held::fistOf(player, hand);
    if(vr_ragdoll_grab_fit.value <= 0.f || hand < 0 || hand > 1 || !player || fist.empty())
    {
        return held;
    }
    const grip::HandFrame f = grip::handFrameOf(player, hand, hand == 0);
    const glm::vec3 nLocal = glm::normalize(f.palmNormal);
    const glm::vec3 n = glm::normalize(turn * nLocal);
    const glm::mat3 axes = glm::mat3_cast(turn);
    const float reach = za::max(vr_ragdoll_grab_reach.value, 0.f) + 4.f;
    // The palm's skin: its middle and a patch round it, 4 cm across the palm either way (along the grip channel) and
    // 4.5 along it (to the fingers, to the wrist).
    const float cm = 0.01f * units::metresToUnits();
    const glm::vec3 across = glm::normalize(f.channelDir - nLocal * glm::dot(f.channelDir, nLocal));
    const glm::vec3 along = glm::cross(nLocal, across);
    // Both in the world, each point kept its radius clear of the limb (handfit's probes).
    za::Vector<glm::vec4>& palm = scratch.palmPatch;
    palm.clear();
    for(int i = -2; i <= 2; i++)
    {
        for(int j = -2; j <= 2; j++)
        {
            const glm::vec3 p = f.palm + across * (2.f * cm * static_cast<float>(i)) + along * (2.25f * cm * static_cast<float>(j));
            palm.pushBack(glm::vec4{at + axes * p, -0.05f});
        }
    }
    za::Vector<glm::vec4>& closed = scratch.fistPatch;
    closed.clear();
    for(const glm::vec4& sphere : fist)
    {
        closed.pushBack(glm::vec4{at + axes * glm::vec3{sphere}, -sphere.w});
    }
    // Slid along the palm's normal onto the limb (handfit::slide: from a gap towards it, from in it back out): the palm,
    // else the fist.
    const handfit::Shape limb = handfit::Shape::ofDistance(limbDistance, &part);
    const auto onto = [&](const za::Vector<glm::vec4>& points) {
        const handfit::Group all{0, static_cast<int>(points.size()), 1};
        return handfit::slide(limb, handfit::Probes{points.data(), &all, 1}, n, reach);
    };
    const char* by = "the palm";
    handfit::Slide s = onto(palm);
    if(!s.found)
    {
        by = "the fist";
        s = onto(closed);
        by = s.found ? by : nullptr;
    }
    float t = s.t;
    const float from = -s.before; // (how far the nearest was from the limb: negative, in it)
    if(vr_debug_ragdoll.value >= 2.f)
    {
        Con_Printf("ragdoll: hand fit: the palm's normal %.2f %.2f %.2f; %s\n", n.x, n.y, n.z,
            by ? va("%s %.1f units %s the limb, moved %.1f along it", by, za::fabs(from), from > 0.f ? "from" : "into", t)
               : "no move within the reach rests it on the limb");
    }
    if(!by)
    {
        return held;
    }
    t -= vr_held_fit_gap.value * cm;
    if(moved)
    {
        *moved = t;
    }
    return at + n * t;
}

// Grab `g`'s hold: its body at the hand, its joint to the limb holding the limb's point `grip` (its body's space, m) at
// the hand, the limb turned in the hand as it is now; `grip` moved for the limb to rest on the palm (handFit; not with
// `fit` false: a tug's, the hand on the weapon the limb holds, its lever kept).
// `still`: the hold begins unstretched, the limb's point held where it is from the hand (that offset kept in the hand's
// frame) rather than drawn onto the hand: a worn pack's (the hand reached into the pack and the fit moved its point 4.5
// units back out: the spring began stretched by that and drove the pinned body past a still hand at its 4000 N cap: a
// slow drag read as a hard pull: vr_wornpack_test 3).
void startHold(World::RagdollGrab& g, edict_t* player, const RagdollBodies& r, b3Vec3 grip, bool fit = true, bool still = false)
{
    glm::vec3 at;
    glm::quat turn;
    handPose(player, g.hand, at, turn);
    if(g.bladeReach != glm::vec3{0.f})
    {
        g.bladeTurn = turn; // (a blade's held at its wound: its anchor there, turning with the hand from now)
    }
    at += g.bladeReach;
    if(fit)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(g.part)];
        float moved = 0.f;
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(body, grip));
        grip = b3Body_GetLocalPoint(body, world->toM(handFit(g.player, g.hand, at, turn, body, held, &moved)));
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d part %d held %.1f units from the hand's middle, moved %.1f units along the palm's normal "
                       "(vr_ragdoll_grab_fit)\n",
                g.num, g.part, glm::distance(held, at), moved);
        }
    }
    b3BodyDef def = b3DefaultBodyDef();
    def.type = b3_kinematicBody;
    def.position = world->toM(at);
    def.rotation = toB3(turn);
    def.userData = userOf(g.player);
    g.anchor = b3CreateBody(world->id, &def);
    const b3BodyId part = r.body[static_cast<za::SizeT>(g.part)];
    const glm::quat partTurn = fromB3(b3Body_GetRotation(part));
    const float force = g.force >= 0.f ? g.force : za::max(vr_ragdoll_grab_force.value, 0.f);
    const float hertz = g.hertz > 0.f ? g.hertz : grabHertz;
    b3MotorJointDef jd = b3DefaultMotorJointDef();
    jd.base.bodyIdA = g.anchor;
    jd.base.bodyIdB = part;
    const glm::vec3 offset = still ? glm::inverse(turn) * (world->toU(b3Body_GetWorldPoint(part, grip)) - at) : glm::vec3{0.f};
    jd.base.localFrameA = b3Transform{world->toM(offset), b3Quat_identity};
    jd.base.localFrameB = b3Transform{grip, toB3(glm::normalize(glm::inverse(partTurn) * turn))};
    jd.base.collideConnected = false;
    jd.linearVelocity = b3Vec3_zero;
    jd.angularVelocity = b3Vec3_zero;
    jd.maxVelocityForce = 0.f;
    jd.maxVelocityTorque = 0.f;
    jd.linearHertz = hertz;
    jd.linearDampingRatio = 1.f;
    jd.maxSpringForce = force;
    jd.angularHertz = hertz * 0.5f;
    jd.angularDampingRatio = 1.f;
    jd.maxSpringTorque = force * grabLever;
    g.joint = b3CreateMotorJoint(world->id, &jd);
    g.farSince = -1.0;
    for(int b = 0; b < r.count; b++)
    {
        b3Body_SetAwake(r.body[static_cast<za::SizeT>(b)], true);
    }
}

// A hand's hold on a ragdoll about to begin (grabRagdoll, holdSoft): the player's number, the ragdoll's entity and bodies,
// and where the hand is (world units).
struct GrabStart
{
    int pnum{0};
    int num{0};
    RagdollBodies* r{nullptr};
    glm::vec3 at{0.f};
};

// Whether hand `hand` of `player` may take hold of `ent`'s ragdoll (a player's hand holding nothing yet, `ent` a ragdoll):
// `gs` filled in.
[[nodiscard]] bool grabStart(edict_t* ent, edict_t* player, int hand, GrabStart& gs)
{
    gs.pnum = NUM_FOR_EDICT(player);
    if(!world || gs.pnum < 1 || gs.pnum > svs.maxclients || grabIndex(gs.pnum, hand) >= 0)
    {
        return false; // (none, or holding already)
    }
    gs.num = NUM_FOR_EDICT(ent);
    gs.r = ragdollOf(gs.num);
    if(!gs.r)
    {
        return false;
    }
    glm::quat turn;
    handPose(player, hand, gs.at, turn);
    return true;
}

// Hand `hand` holds `part` of the ragdoll `gs` found, from where the hand is (`g`: its spring's settings, if any).
void beginGrab(World::RagdollGrab& g, const GrabStart& gs, edict_t* player, int hand, int part)
{
    g.player = gs.pnum;
    g.hand = hand;
    g.num = gs.num;
    g.part = part;
    // (On its worn backpack: the pack's part, the hand's palm sphere on the pack's box (3.5 cm: vr_foegrab.cpp's palm; it
    // was 3 units, 11 cm, past the box). vr_box3d_wornpack.cpp)
    const float packReach = 0.035f * units::metresToUnits();
    const float packGap = packDistance(*gs.r, gs.at);
    g.onPack = part == gs.r->packPart && !g.tug && packGap >= 0.f && packGap <= packReach;
    startHold(g, player, *gs.r, b3Body_GetLocalPoint(gs.r->body[static_cast<za::SizeT>(part)], world->toM(gs.at)), !g.tug,
        g.onPack);
    world->ragdollGrabs.pushBack(g);
}

// Shared between VR hand touch (grabRagdoll) and flatscreen Grab / Pull (grabRagdollReach): a living staggering
// ragdoll taken by a hand loses its footing and goes down.
void manhandleStagger(edict_t* corpse, RagdollBodies& r, int num, int hand)
{
    if(vr_foegrab_manhandle_stagger.value > 0.f && corpse->v.health > 0.f && r.active.on && r.active.stagger.on)
    {
        ragactive::lapse(r.active, activeParts(r), qcvm->time);
        if(vr_knockdown_debug.value || vr_debug_ragdoll.value)
        {
            Con_Printf("manhandle: %d %s taken by hand %d staggering: it goes down\n", num, PR_GetString(corpse->v.classname),
                hand);
        }
    }
}

// ragdollgrab: hand `hand` of `player` takes the limb of `corpse`'s ragdoll it is on (within vr_ragdoll_grab_reach of
// its surface).
qvr::box3d::RagdollGrabResult grabRagdoll(edict_t* corpse, edict_t* player, int hand)
{
    using Result = qvr::box3d::RagdollGrabResult;
    GrabStart gs;
    if(vr_ragdoll_grab.value < 1.f || !grabStart(corpse, player, hand, gs))
    {
        return Result::None;
    }
    RagdollBodies* r = gs.r;
    const int num = gs.num;
    float distance = 0.f;
    const int part = nearestPart(*r, gs.at, distance);
    const bool onLimb = part >= 0 && distance <= za::max(vr_ragdoll_grab_reach.value, 0.f);
    // The grab query's arbitration (vr_grab.hpp): a living armed one's weapon wins over its limbs. The fist on that weapon
    // (within vr_agrunt_rag_grab_weapon of its drawn shape), or on the hand or forearm holding it (vr_agrunt_rag_limb_grab),
    // is a grab on the weapon (QC VR_Ragdoll_Handtouch hands it to the weapon's grab: a tug), not a hold on the limb: held
    // by the arm, the body was dragged about and the weapon never came out (his note of 2026-10-30). Both touches come in
    // one frame, either first: the weapon's own takes it too, the second swallowed (the same hand's tug).
    if(corpse->v.health > 0.f)
    {
        if(edict_t* w = qvr::physics::armedWeaponOver(corpse, player, hand, onLimb ? part : -1))
        {
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d hand %d on its weapon (%d)%s: the weapon's grab\n", num, hand, NUM_FOR_EDICT(w),
                    onLimb ? "" : ", off its limbs");
            }
            return Result::Weapon;
        }
    }
    if(!onLimb)
    {
        return Result::None;
    }
    World::RagdollGrab g;
    beginGrab(g, gs, player, hand, part);
    manhandleStagger(corpse, *r, num, hand);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) taken by hand %d, %.1f units from its surface\n", num, part, r->rig->bones[part].name,
            hand, distance);
    }
    return Result::Limb;
}

// Flatscreen Grab / Pull: hand `hand` of `player` takes the non-cut limb of `corpse`'s ragdoll nearest `target`
// within `reach` units (preferring a distinct part from `avoidPart` when >= 0 so both hands grab separate limbs/parts).
// Anchors on the actual closest surface point on that bone so the Box3D motor joint pulls it smoothly to the hand.
// Returns part + 1 on success, 0 on failure.
int grabRagdollReachImpl(edict_t* corpse, edict_t* player, int hand, const glm::vec3& target, float reach, int avoidPart)
{
    GrabStart gs;
    if(vr_ragdoll_grab.value < 1.f || !grabStart(corpse, player, hand, gs))
    {
        return 0;
    }
    RagdollBodies* r = gs.r;
    const int num = gs.num;
    const float maxDist = za::max(reach, 0.f);
    int bestPart = -1;
    float bestScore = 1e30f;
    float bestDist = 1e30f;
    b3Vec3 bestClosest = b3Vec3_zero;
    const b3Vec3 targetM = world->toM(target);
    for(int b = 0; b < r->count; b++)
    {
        if(partCut(*r, b) || (r->rig && r->rig->bones[b].joint == ragdoll::Joint::Loose))
        {
            continue;
        }
        b3Vec3 closest;
        const float d = b3Body_GetClosestPoint(r->body[static_cast<za::SizeT>(b)], &closest, targetM) * world->m2u;
        if(d > maxDist)
        {
            continue;
        }
        // Strongly prefer a different part than the other hand's (`avoidPart`) so 2H holds stabilize the body.
        const float score = d + (b == avoidPart ? 24.f : 0.f);
        if(score < bestScore)
        {
            bestScore = score;
            bestDist = d;
            bestPart = b;
            bestClosest = closest;
        }
    }
    if(bestPart < 0)
    {
        return 0;
    }
    const glm::vec3 worldOnLimb = world->toU(bestClosest);
    World::RagdollGrab g;
    g.player = gs.pnum;
    g.hand = hand;
    g.num = num;
    g.part = bestPart;
    g.stuck = za::max(grabStuck, glm::distance(worldOnLimb, gs.at) + 32.f);
    startHold(g, player, *r, b3Body_GetLocalPoint(r->body[static_cast<za::SizeT>(bestPart)], bestClosest), false, false);
    world->ragdollGrabs.pushBack(g);
    manhandleStagger(corpse, *r, num, hand);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) reach-grabbed by hand %d, %.1f u from aim (%.1f u from hand)\n", num, bestPart,
            r->rig ? r->rig->bones[bestPart].name : "?", hand, bestDist, glm::distance(worldOnLimb, gs.at));
    }
    return bestPart + 1;
}

// ragdollHoldSoft: hand `hand` of `player` holds on to the limb of `ent`'s ragdoll nearest it (within `reach` units of
// its surface), by a spring of at most `force` N at `hertz`, let go of past `stuck` units (vr_foegrab.cpp: an enemy
// thrown or lifted, held on). `head`: its head's part instead, if not cut off and its middle within `reach` and 12 units
// more (a neck snapped: the hands held its head).
bool holdSoft(edict_t* ent, edict_t* player, int hand, float reach, float force, float hertz, float stuck, bool head, bool torso,
    bool pack)
{
    GrabStart gs;
    if(!grabStart(ent, player, hand, gs))
    {
        return false;
    }
    const RagdollBodies* r = gs.r;
    const int num = gs.num;
    const glm::vec3& at = gs.at;
    glm::vec3 onPack{0.f};
    if(pack)
    {
        // (By its worn backpack: the pack's point nearest the hand, on the chest part that carries it, pulled to the hand.)
        if(!packNearest(*r, at, onPack) || glm::distance(onPack, at) > reach)
        {
            return false;
        }
        World::RagdollGrab g;
        g.force = za::max(force, 0.f);
        g.hertz = za::max(hertz, 0.1f);
        g.stuck = za::max(stuck, 1.f);
        g.player = gs.pnum;
        g.hand = hand;
        g.num = num;
        g.part = r->packPart;
        g.onPack = true;
        startHold(g, player, *r, b3Body_GetLocalPoint(r->body[static_cast<za::SizeT>(g.part)], world->toM(onPack)), false);
        world->ragdollGrabs.pushBack(g);
        if(vr_debug_ragdoll.value || vr_worn_backpack_debug.value)
        {
            Con_Printf("ragdoll: %d held on by its pack (part %d) by hand %d, %.1f units from the hand (soft: %.0f N, %.1f Hz)\n",
                num, g.part, hand, glm::distance(onPack, at), g.force, g.hertz);
        }
        return true;
    }
    float distance = 0.f;
    int part = nearestPart(*r, at, distance);
    const int headPart = r->rig->head;
    if(head && headPart >= 0 && headPart < r->count && !partCut(*r, headPart))
    {
        const float d = glm::distance(world->toU(b3Body_GetWorldCenter(r->body[static_cast<za::SizeT>(headPart)])), at);
        if(d <= reach + 12.f)
        {
            part = headPart;
            distance = za::min(distance, d);
        }
    }
    else if(torso && part > 0 && part != headPart && ZA_STRCMP(r->rig->bones[part].name, "chest") != 0)
    {
        // (Lifted: its torso rather than an arm across it, if near enough: an arm held hangs slack.)
        int best = -1;
        float bestD = distance + 10.f;
        for(int b = 0; b < r->count; b++)
        {
            if((b == 0 || !ZA_STRCMP(r->rig->bones[b].name, "chest")) && !partCut(*r, b))
            {
                b3Vec3 closest;
                const float d = b3Body_GetClosestPoint(r->body[static_cast<za::SizeT>(b)], &closest, world->toM(at)) * world->m2u;
                if(d <= bestD)
                {
                    best = b;
                    bestD = d;
                }
            }
        }
        if(best >= 0 && bestD <= reach)
        {
            part = best;
            distance = bestD;
        }
    }
    if(part < 0 || distance > reach)
    {
        return false;
    }
    World::RagdollGrab g;
    g.force = za::max(force, 0.f);
    g.hertz = za::max(hertz, 0.1f);
    g.stuck = za::max(stuck, 1.f);
    beginGrab(g, gs, player, hand, part);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) held on by hand %d, %.1f units from its surface (soft: %.0f N, %.1f Hz)\n", num,
            part, r->rig->bones[part].name, hand, distance, g.force, g.hertz);
    }
    return true;
}

// Hand `hand` of `player` from where `r` stands its ground (units, flat).
[[nodiscard]] glm::vec3 tugFromBase(edict_t* player, int hand, const RagdollBodies& r)
{
    glm::vec3 at;
    glm::quat turn;
    handPose(player, hand, at, turn);
    const b3BodyId base = B3_IS_NON_NULL(r.active.pin.anchor) && b3Body_IsValid(r.active.pin.anchor) ? r.active.pin.anchor : r.body[0];
    glm::vec3 d = at - world->toU(b3Body_GetPosition(base));
    d.z = 0.f;
    return d;
}

// The way a tug's hand pulls (flat, unit), fixed as it takes hold: zero for out through the hand from where `r` stands
// its ground (its arm drawn out: the reach's length), or towards `player` when the hand is past where it stands from you
// (a second hand on a weapon's far end, by its chest: its distance from the pelvis shrank as it pulled, and that hand's
// pull never counted: test 92 phase 1, two hands no stronger than one).
[[nodiscard]] glm::vec3 tugAwayOf(edict_t* player, int hand, const RagdollBodies& r)
{
    const glm::vec3 d = tugFromBase(player, hand, r);
    const b3BodyId base = B3_IS_NON_NULL(r.active.pin.anchor) && b3Body_IsValid(r.active.pin.anchor) ? r.active.pin.anchor : r.body[0];
    glm::vec3 toPlayer = glm::vec3{player->v.origin[0], player->v.origin[1], 0.f} - world->toU(b3Body_GetPosition(base));
    toPlayer.z = 0.f;
    const float lp = glm::length(toPlayer);
    toPlayer = lp > 0.01f ? toPlayer / lp : glm::vec3{1.f, 0.f, 0.f};
    // (Only past it: a hand anywhere on your side of it draws its arm out however it pulls. A standing one's tug only:
    // ragdollTug what 0.)
    return glm::dot(d, toPlayer) >= 0.f ? glm::vec3{0.f} : toPlayer;
}

// A tug's reach (m): hand `hand` of `player` from where `r` stands its ground (flat: a step up or down is no pull), its
// pelvis pin's anchor (a slow pull drags its pelvis off it: that counts too), else its root body: how far, or how far
// along `away` when that is not zero (tugAwayOf).
[[nodiscard]] float tugReach(edict_t* player, int hand, const RagdollBodies& r, const glm::vec3& away)
{
    const glm::vec3 d = tugFromBase(player, hand, r);
    return (away == glm::vec3{0.f} ? glm::length(d) : glm::dot(d, away)) / world->m2u;
}

// ragdolltug (QC vr_armedgrunt_play.qc, the tug-of-war): `ent` (alive, its stagger begun: standing its ground) and hand
// `hand` of `player` on its weapon. what 0: the hand holds the limb nearest the weapon's grip `grip` (units) by a spring
// of at most `force` N, where the hand is (the weapon's lever: no palm fit), its arm pulling at `strength`: 1 if it does;
// 1: the hold's pull now (N; -1 none); 2: over, let go, its stagger up again `strength` s on.
float tugRagdoll(edict_t* ent, edict_t* player, int hand, int what, const glm::vec3& grip, float strength, float force)
{
    const int pnum = NUM_FOR_EDICT(player);
    const int num = NUM_FOR_EDICT(ent);
    const int index = grabIndex(pnum, hand);
    World::RagdollGrab* g = index >= 0 ? &world->ragdollGrabs[static_cast<za::SizeT>(index)] : nullptr;
    const bool ours = g && g->tug && g->num == num;
    RagdollBodies* r = ragdollOf(num);
    if(what == 1)
    {
        if(!ours || B3_IS_NULL(g->joint) || !b3Joint_IsValid(g->joint))
        {
            return -1.f;
        }
        return glm::length(glmv(b3Joint_GetConstraintForce(g->joint)));
    }
    if(what == 3)
    {
        // The tug's stretch (m): how much further the hand is from its root body (its pelvis, pinned: standing its ground)
        // than as it took hold, its arm drawn out (QC vr_agrunt_tug_measure: times vr_agrunt_tug_stiffness, a pull that
        // doesn't depend on the hold spring's give: the held limb follows the hand within 2 cm, its joint force 40-100
        // N however far the arm is drawn).
        if(!ours || !r || r->count < 1 || B3_IS_NULL(g->joint) || !b3Joint_IsValid(g->joint))
        {
            return -1.f;
        }
        return za::max(0.f, tugReach(player, hand, *r, g->tugAway) - g->tugReach);
    }
    if(what == 9)
    {
        // Its held arm drawn up to `strength` times longer towards your hand (QC VR_AGK_TugStretch, eased with the tug):
        // ragdoll::drawnPose's, the physics untouched. 1 if this hand holds it.
        if(!ours)
        {
            return 0.f;
        }
        g->tugStretch = za::max(strength, 1.f);
        return 1.f;
    }
    if(what == 5)
    {
        // The part this hand's hold takes (the tests: the weapon hand's limb, not the support hand's); -1 none.
        return ours ? static_cast<float>(g->part) : -1.f;
    }
    if(what == 7 || what == 8)
    {
        // The tug readout (Show Disarm Volumes): 7 your controller's distance from where its hold holds the limb, 8 the
        // distance the hold lets go at (syncRagdollGrabs' tugGo; units); -1 none.
        if(!ours || !r || g->part < 0 || g->part >= r->count || B3_IS_NULL(g->joint) || !b3Joint_IsValid(g->joint))
        {
            return -1.f;
        }
        if(what == 8)
        {
            const float letGo = vr_agrunt_tug_let_go.value > 0.f ? vr_agrunt_tug_let_go.value * 0.01f * world->m2u : g->stuck;
            return g->tugBack > 0.f ? za::max(letGo, g->stuck) : letGo;
        }
        glm::vec3 at;
        glm::quat turn;
        handPose(player, g->hand, at, turn);
        const b3Transform frameB = b3Joint_GetLocalFrameB(g->joint);
        return glm::distance(world->toU(b3Body_GetWorldPoint(r->body[static_cast<za::SizeT>(g->part)], frameB.p)), at);
    }
    if(what == 10 || what == 11)
    {
        // 10: its body in the tug (ragactive staggerTugSway, staggerArms): the pull's share of what breaks its grip
        // (`strength`) and its way, from its pelvis to this hand; 1 if it stands in a tug. 11 (the tests): the part at the
        // end of its free arm (-1 none).
        if(!ours || !r || !r->active.on || !r->active.stagger.on || !r->active.stagger.tug)
        {
            return what == 11 ? -1.f : 0.f;
        }
        ragactive::Stagger& st = r->active.stagger;
        if(what == 11)
        {
            return st.tugArm < 0 ? -1.f : static_cast<float>(r->active.chain[static_cast<za::SizeT>(1 - st.tugArm)][1]);
        }
        glm::vec3 at;
        glm::quat turn;
        handPose(player, g->hand, at, turn);
        st.tugPull = za::max(strength, 0.f);
        st.tugWay = at - world->toU(b3Body_GetWorldCenter(r->body[0]));
        return 1.f;
    }
    if(what == 4)
    {
        // This hand lets go, the tug going on (a second hand's hold, or the first's with the second still on).
        if(ours)
        {
            endGrab(index);
        }
        return 1.f;
    }
    if(what == 2)
    {
        // (Every tug hold of this player's on it: both hands.)
        for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
        {
            const World::RagdollGrab& o = world->ragdollGrabs[static_cast<za::SizeT>(i)];
            if(o.tug && o.num == num && o.player == pnum)
            {
                endGrab(i);
            }
        }
        if(r && r->active.on && r->active.stagger.on && r->active.stagger.tug) // (a downed hold's: not its own stagger's)
        {
            ragactive::Stagger& st = r->active.stagger;
            st.tug = false;
            st.tugArm = -1;
            if(st.outcome == ragactive::Outcome::Going)
            {
                st.recoverAt = static_cast<float>(qcvm->time - r->active.begun) + za::max(strength, 0.05f);
            }
        }
        return 1.f;
    }
    if(what == 6)
    {
        // A downed one's tug (lying, or staggering already: any ragdoll of it): the hand holds the limb nearest `grip` by
        // a spring of at most `force` N, as a limb grab does, its arm pulling that limb back towards its body at
        // `strength` N (syncRagdollGrabs): your pull drags the weapon, hand and arm (the body too if hard enough), its pull
        // is felt in the hold's force (what 1). 1 if it does.
        GrabStart dgs;
        if(!r || !r->rig || r->count < 1 || !grabStart(ent, player, hand, dgs))
        {
            return 0.f;
        }
        float dd = 0.f;
        const int dpart = nearestPart(*r, grip, dd);
        if(dpart < 0)
        {
            return 0.f;
        }
        World::RagdollGrab dg;
        dg.force = za::max(force, 0.f);
        dg.stuck = 64.f;
        dg.tug = true;
        dg.tugBack = za::max(strength, 0.f);
        // (Out through the hand always: a lying one's limb is dragged any way, and a hand past its pelvis read towards you
        // never won test 91's lying grip.)
        dg.tugAway = glm::vec3{0.f};
        dg.tugReach = tugReach(player, hand, *r, dg.tugAway);
        beginGrab(dg, dgs, player, hand, dpart);
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("tug: %d %s down: hand %d holds its part %d (%s) %.1f units from the grip, %.0f N at most, its pull back %.0f N, pulled %s\n",
                num, PR_GetString(ent->v.classname), hand, dpart, r->rig->bones[dpart].name, dd, dg.force, dg.tugBack,
                dg.tugAway == glm::vec3{0.f} ? "out" : "towards you");
        }
        return 1.f;
    }
    if(what == 15)
    {
        // Its ragdoll's mass (kg; 0: none): what a blade in it lifts (QC VR_Embed_Spring, vr_embed_move_*_lift).
        float kg = 0.f;
        for(int b = 0; r && b < r->count; b++)
        {
            if(!partCut(*r, b))
            {
                kg += b3Body_GetMass(r->body[static_cast<za::SizeT>(b)]);
            }
        }
        return kg;
    }
    if(what == 12)
    {
        // A blade stuck in it (QC vr_embedhold.qc; any ragdoll of it, alive or dead): the hand holds the limb nearest `grip`
        // (where it went in; a loose piece, its weapon, isn't one) where it is now, by a spring of at most `force` N. Never a manhandling
        // (vr_foegrab_manhandle_stagger: grabRagdoll's): the blade staggered it, the hand on it doesn't fell it.
        GrabStart bgs;
        if(!r || !r->rig || r->count < 1 || !grabStart(ent, player, hand, bgs))
        {
            return 0.f;
        }
        int bpart = -1;
        float bd = 1e30f;
        for(int b = 0; b < r->count; b++)
        {
            if(partCut(*r, b) || r->rig->bones[b].joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            b3Vec3 closest;
            const float d = b3Body_GetClosestPoint(r->body[static_cast<za::SizeT>(b)], &closest, world->toM(grip));
            if(d < bd)
            {
                bd = d;
                bpart = b;
            }
        }
        if(bpart < 0)
        {
            return 0.f;
        }
        World::RagdollGrab bg;
        bg.force = za::max(force, 0.f);
        bg.tug = true;
        bg.blade = true;
        // (Held at the wound, the spring's pull goes into the body where the blade is; held at the hand, a point out at the
        // hilt a blade's length away, the body hung and swung from there on the hold's small torque (grabLever): a lift or a
        // swing moved the wound only some 70% of the hand's way, whatever the force. Measured 2026-11-08, vr_embed_test 27.)
        bg.hertz = bladeHertz();
        if(vr_embed_move_at_wound.value >= 1.f)
        {
            bg.bladeReach = grip - bgs.at;
            bgs.at = grip;
        }
        beginGrab(bg, bgs, player, hand, bpart);
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("embed: %d %s: hand %d holds its part %d (%s) by the blade, %.0f N at most\n", num,
                PR_GetString(ent->v.classname), hand, bpart, r->rig->bones[bpart].name, bg.force);
        }
        return 1.f;
    }
    if(what == 13)
    {
        // That hold's force now (N): by how deep the blade is.
        if(ours)
        {
            g->force = za::max(force, 0.f);
        }
        return ours ? 1.f : 0.f;
    }
    if(what == 14)
    {
        // Where that spring pulls to, from the hand (`grip`): the hand's pull along the blade is the blade's, not the body's.
        if(ours)
        {
            g->bladeShift = grip;
        }
        return ours ? 1.f : 0.f;
    }
    if(what == 17)
    {
        // That hold taken by the blade's handle (vr_embed_move_handle; QC VR_Embed_Spring, right after what 12): `grip`,
        // where the handle is (the blade's origin: where the hand is drawn on it). Held at the wound, the wound went
        // round the hand with the wrist (vr_embed_move_turn) while the blade kept the body's own turn: the sword, and the
        // hand drawn on it, were carried off the real hand by the wrist's turn times the blade's length (10 degrees: 10
        // units; "four or five times" a small lift), and by the body's own turning on the wound (12 units, dead). Held by
        // the handle, the blade rigid in the limb: the handle is where the hand is, whatever the body does; the body is
        // turned with the hand by a torque of the hold's force at the blade's length (at a limb grab's 4 cm it hung and
        // swung from the hilt: the 70% of 2026-11-08). 1 if it is.
        if(!ours || !g->blade || !r || g->part < 0 || g->part >= r->count || B3_IS_NULL(g->joint) || !b3Joint_IsValid(g->joint) ||
           B3_IS_NULL(g->anchor) || !b3Body_IsValid(g->anchor))
        {
            return 0.f;
        }
        glm::vec3 at;
        glm::quat turn;
        handPose(player, hand, at, turn);
        const b3BodyId part = r->body[static_cast<za::SizeT>(g->part)];
        const b3Transform frameB = b3Joint_GetLocalFrameB(g->joint);
        if(g->bladeHandle)
        {
            // Each frame after: the handle as drawn now. The blade rides the model's triangle, which bends from this limb's body
            // with the ragdoll's joints (a dead grunt's chest on its spine: the drawn handle 10 units from the rigid one): the
            // hold's point on the limb goes to where the handle is drawn, eased (the drawn blade's turn lags its limb).
            const float dt = za::clamp(static_cast<float>(qcvm->time - g->bladeHandleAt), 0.f, 0.1f);
            g->bladeHandleAt = qcvm->time;
            const b3Vec3 want = b3Body_GetLocalPoint(part, world->toM(grip));
            const float k = 1.f - za::exp(-dt * handleFollow);
            b3Joint_SetLocalFrameB(g->joint, b3Transform{b3Lerp(frameB.p, want, k), frameB.q});
            return 1.f;
        }
        const glm::vec3 wound = world->toU(b3Body_GetWorldPoint(part, frameB.p));
        g->bladeHandle = true;
        g->bladeHandleAt = qcvm->time;
        g->bladeLever = glm::distance(grip, wound) / world->m2u;
        g->bladeReach = glm::vec3{0.f};
        g->bladeGap = grip - at - g->bladeShift; // (less what the QC keeps of it: what 14, set before this)
        g->bladeGapAt = qcvm->time;
        b3Joint_SetLocalFrameA(g->joint, b3Transform{b3Vec3_zero, b3Quat_identity});
        b3Joint_SetLocalFrameB(g->joint, b3Transform{b3Body_GetLocalPoint(part, world->toM(grip)), frameB.q});
        b3Body_SetTransform(g->anchor, world->toM(at + g->bladeShift + g->bladeGap), toB3(turn));
        b3Body_SetLinearVelocity(g->anchor, b3Vec3_zero);
        b3MotorJoint_SetMaxSpringTorque(g->joint, za::max(g->force, 0.f) * za::max(grabLever, g->bladeLever));
        if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
        {
            Con_Printf("embed: %d: hand %d holds it by the blade's handle, %.1f units from the wound, %.1f from the hand\n", num, hand,
                glm::distance(grip, wound), glm::length(g->bladeGap));
        }
        return 1.f;
    }
    if(what == 16)
    {
        // (The tests: how far the held limb's point is from where that spring pulls it, units; -1 none.)
        if(!ours || !r || g->part < 0 || g->part >= r->count || B3_IS_NULL(g->joint) || !b3Joint_IsValid(g->joint) ||
           B3_IS_NULL(g->anchor) || !b3Body_IsValid(g->anchor))
        {
            return -1.f;
        }
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(r->body[static_cast<za::SizeT>(g->part)], b3Joint_GetLocalFrameB(g->joint).p));
        return glm::distance(held, world->toU(b3Body_GetWorldPoint(g->anchor, b3Joint_GetLocalFrameA(g->joint).p)));
    }
    GrabStart gs;
    if(!r || !r->rig || !r->active.on || !r->active.stagger.on || !grabStart(ent, player, hand, gs))
    {
        return 0.f;
    }
    float distance = 0.f;
    const int part = nearestPart(*r, grip, distance);
    if(part < 0)
    {
        return 0.f;
    }
    ragactive::Stagger& st = r->active.stagger;
    if(!st.tug)
    {
        st.tug = true;
        st.tugStrength = za::clamp(strength, 0.f, 3.f);
        // (Its pelvis's pin harder by its pull: it leans back against your hand rather than following it, its feet
        // stepping after it when it must; its resistance is what your pull measures.)
        r->active.pin.force *= 1.f + st.tugStrength;
        st.tugArm = -1;
        const int parent = r->rig->bones[part].parent;
        for(int arm = 0; arm < 2; arm++)
        {
            const auto& ch = r->active.chain[static_cast<za::SizeT>(arm)];
            if(ch[0] == part || ch[1] == part || (parent >= 0 && ch[1] == parent))
            {
                st.tugArm = arm;
            }
        }
    }
    // (else: a second hand joining the tug: one more hold, its stagger and pin as they are)
    World::RagdollGrab ng;
    ng.force = za::max(force, 0.f);
    ng.stuck = 64.f;
    ng.tug = true;
    ng.tugAway = tugAwayOf(player, hand, *r);
    ng.tugReach = tugReach(player, hand, *r, ng.tugAway);
    beginGrab(ng, gs, player, hand, part);
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("tug: %d %s: hand %d holds its part %d (%s, arm %d) %.1f units from the grip, %.0f N at most, its pull %.2f, pulled %s\n",
            num, PR_GetString(ent->v.classname), hand, part, r->rig->bones[part].name, st.tugArm, distance, ng.force,
            st.tugStrength, ng.tugAway == glm::vec3{0.f} ? "out" : "towards you");
    }
    return 1.f;
}

// ragdollrelease: hand `hand` of `player` lets go: a held limb keeps `velocity` (units/s: the hand's throw, times
// vr_ragdoll_throw) where it is faster than it.
void releaseRagdoll(edict_t* player, int hand, const glm::vec3& velocity)
{
    const int index = grabIndex(NUM_FOR_EDICT(player), hand);
    if(index < 0)
    {
        return;
    }
    const World::RagdollGrab g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    endGrab(index);
    RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part >= r->count)
    {
        return;
    }
    r->thrownPeak = 0.f;
    r->thrownAt = qcvm->time; // (let go of: a throw, as fast as it goes: the ragdoll triggers' Thrown Body)
    const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
    const glm::vec3 v = glmv(world->toM(velocity * za::max(vr_ragdoll_throw.value, 0.f)));
    const glm::vec3 now = glmv(b3Body_GetLinearVelocity(part));
    const float speed = glm::length(v);
    if(speed > 0.01f)
    {
        // Its speed along the throw at least the throw's (what the hold already gave it is kept across it).
        const glm::vec3 dir = v / speed;
        const float along = glm::dot(now, dir);
        if(along < speed)
        {
            b3Body_SetLinearVelocity(part, b3v(now + dir * (speed - along)));
        }
    }
    b3Body_SetAwake(part, true);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d let go of by hand %d at %.0f u/s (the throw %.0f)\n", g.num, g.part, hand,
            glm::length(glmv(b3Body_GetLinearVelocity(part))) * world->m2u, speed * world->m2u);
    }
}
// ---- The physgun's hold (vr_toolgun.cpp) ----
// The toolgun's physgun drags a ragdoll as a hand holds a limb, not by its entity's origin (moved, the body would be
// set there whole each frame it strayed 32 units from it: feedRagdoll's teleport, the jitter and jumps of his note
// 2026-10-18): a kinematic anchor at the beam's point, driven to it each step (b3Body_SetTargetTransform: a velocity,
// never set), and a motor joint from it to the point of the part the beam met. Its spring is linear only (at most
// vr_toolgun_physgun_ragdoll_force times the ragdoll's weight): the part turns freely about that point, the rest of the
// body hangs from it and swings. Its hold is a RagdollGrab of player 0 (no hand): an active ragdoll's drive gives that
// part no strength as for a hand's; the struggles, the hands' drawing and QC's ragdollheld pass it by. At most one.

constexpr float physgunHertz = 6.f;      // the spring's rate (Hz), critically damped, as on the whole body's mass
constexpr float physgunHertzMost = 30.f; // (Box3D's substeps at 288 Hz: well under a quarter of that)

[[nodiscard]] int physgunIndex()
{
    for(int i = 0; i < static_cast<int>(world->ragdollGrabs.size()); i++)
    {
        if(world->ragdollGrabs[static_cast<za::SizeT>(i)].player == 0)
        {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] float ragdollMass(const RagdollBodies& r)
{
    float mass = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        mass += partCut(r, b) ? 0.f : b3Body_GetMass(r.body[static_cast<za::SizeT>(b)]);
    }
    return mass;
}

// The spring's most force on `r` (N).
[[nodiscard]] float physgunForce(const RagdollBodies& r)
{
    return ragdollMass(r) * za::max(-b3World_GetGravity(world->id).z, 1.f) *
           za::max(vr_toolgun_physgun_ragdoll_force.value, 0.f);
}

// The spring's rate on part `part` of `r` (Hz). Box3D's soft spring is as stiff as for the part's own mass: a forearm
// (2 of a grunt's 80 kg) at physgunHertz hung 40 units under its target with the body on it. Its rate is raised by the
// root of the body's mass over the part's, so it pulls as on the whole body at physgunHertz (at most physgunHertzMost).
[[nodiscard]] float physgunSpringHertz(const RagdollBodies& r, int part)
{
    const float own = za::max(b3Body_GetMass(r.body[static_cast<za::SizeT>(part)]), 0.01f);
    return za::min(physgunHertz * za::sqrt(za::max(ragdollMass(r) / own, 1.f)), physgunHertzMost);
}

// Before the step: the anchor to its target. False: the hold is over (the ragdoll gone, its part cut off).
[[nodiscard]] bool physgunFrame(World::RagdollGrab& g, float dt)
{
    const RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part < 0 || g.part >= r->count || partCut(*r, g.part) || B3_IS_NULL(g.anchor) || !b3Body_IsValid(g.anchor) ||
        B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
    {
        return false;
    }
    b3MotorJoint_SetMaxSpringForce(g.joint, physgunForce(*r));
    b3MotorJoint_SetLinearHertz(g.joint, physgunSpringHertz(*r, g.part)); // (a part cut off: lighter)
    b3Body_SetTargetTransform(g.anchor, b3WorldTransform{world->toM(g.target), b3Quat_identity}, za::max(dt, 1e-4f), true);
    for(int b = 0; b < r->count; b++)
    {
        b3Body_SetAwake(r->body[static_cast<za::SizeT>(b)], true);
    }
    return true;
}

// The part of `r` the beam from `from` along `dir` (unit) meets first within `range` units, and the point (metres);
// -1 if none.
[[nodiscard]] int physgunRay(const RagdollBodies& r, const glm::vec3& from, const glm::vec3& dir, float range, b3Vec3& point)
{
    int best = -1;
    float nearest = 1.f;
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        za::Array<b3ShapeId, maxBodyShapes> shapes;
        const int n = b3Body_GetShapes(r.body[static_cast<za::SizeT>(b)], shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < n; i++)
        {
            const b3WorldCastOutput out = b3Shape_RayCast(shapes[i], world->toM(from), world->toM(dir * range));
            if(out.hit && out.fraction < nearest)
            {
                nearest = out.fraction;
                best = b;
                point = out.point;
            }
        }
    }
    return best;
}

bool physgunGrab(int num, const glm::vec3& from, const glm::vec3& dir, float range, const glm::vec3& fallback, int& partOut,
    float& distance)
{
    RagdollBodies* r = ragdollOf(num);
    if(!r || r->count <= 0)
    {
        return false;
    }
    if(const int old = physgunIndex(); old >= 0)
    {
        endGrab(old);
    }
    b3Vec3 point{};
    int part = physgunRay(*r, from, dir, range, point);
    if(part < 0)
    {
        // (Its box met, not a part: the part nearest that point, held at its surface's point nearest it.)
        float d = 0.f;
        part = nearestPart(*r, fallback, d);
        if(part < 0)
        {
            return false;
        }
        (void)b3Body_GetClosestPoint(r->body[static_cast<za::SizeT>(part)], &point, world->toM(fallback));
    }
    const glm::vec3 grip = world->toU(point);
    const b3BodyId body = r->body[static_cast<za::SizeT>(part)];
    World::RagdollGrab g;
    g.player = 0;
    g.hand = -1;
    g.num = num;
    g.part = part;
    g.target = grip;
    b3BodyDef def = b3DefaultBodyDef();
    def.type = b3_kinematicBody;
    def.position = point;
    g.anchor = b3CreateBody(world->id, &def);
    b3MotorJointDef jd = b3DefaultMotorJointDef();
    jd.base.bodyIdA = g.anchor;
    jd.base.bodyIdB = body;
    jd.base.localFrameA = b3Transform{b3Vec3_zero, b3Quat_identity};
    jd.base.localFrameB = b3Transform{b3Body_GetLocalPoint(body, point), b3Quat_identity};
    jd.base.collideConnected = false;
    jd.linearVelocity = b3Vec3_zero;
    jd.angularVelocity = b3Vec3_zero;
    jd.maxVelocityForce = 0.f;
    jd.maxVelocityTorque = 0.f;
    jd.linearHertz = physgunSpringHertz(*r, part);
    jd.linearDampingRatio = 1.f;
    jd.maxSpringForce = physgunForce(*r);
    jd.angularHertz = 0.f; // (no turn: it hangs from the point)
    jd.maxSpringTorque = 0.f;
    g.joint = b3CreateMotorJoint(world->id, &jd);
    world->ragdollGrabs.pushBack(g);
    for(int b = 0; b < r->count; b++)
    {
        b3Body_SetAwake(r->body[static_cast<za::SizeT>(b)], true);
    }
    partOut = part;
    distance = glm::dot(grip - from, dir);
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) taken by the physgun %.0f units off\n", num, part, r->rig->bones[part].name, distance);
    }
    return true;
}

void physgunRelease(const glm::vec3& velocity)
{
    const int index = physgunIndex();
    if(index < 0)
    {
        return;
    }
    const World::RagdollGrab g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    endGrab(index);
    const RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part >= r->count || partCut(*r, g.part))
    {
        return;
    }
    // (As a hand's throw: its speed along the beam's swing at least that.)
    const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
    const glm::vec3 v = glmv(world->toM(velocity));
    const glm::vec3 now = glmv(b3Body_GetLinearVelocity(part));
    const float speed = glm::length(v);
    if(speed > 0.01f)
    {
        const glm::vec3 way = v / speed;
        const float along = glm::dot(now, way);
        if(along < speed)
        {
            b3Body_SetLinearVelocity(part, b3v(now + way * (speed - along)));
        }
    }
    b3Body_SetAwake(part, true);
}
} // namespace

bool tugHolds(int player, int hand, int num)
{
    const int index = world ? grabIndex(player, hand) : -1;
    if(index < 0)
    {
        return false;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    return g.tug && g.num == num;
}

// ---- A blade held in a prop (his note 2026-11-05: "pulling either the blade or the prop moves both, a Box3D joint"; QC
// vr_embedhold.qc VR_Embed_PropSpring, embedprophold). A hand on the handle of a blade left in a crate or a barrel holds the
// prop as it holds a ragdoll by a blade (tugRagdoll what 17): a kinematic body at the hand, a motor joint from it to the prop
// at the handle's point, stiff (handleHertz), its force `force` at most level and `lift` up (dragged and tipped, not
// hoisted on one blade), its torque that force at the blade's length. The handle stays in the hand unless the prop is too
// heavy or held back; the prop moved by anything else takes the blade along (it rides the prop: vr_axestick.cpp).
namespace
{
[[nodiscard]] int propBladeIndex(int player, int hand)
{
    for(int i = 0; i < static_cast<int>(world->propBlades.size()); i++)
    {
        const World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(i)];
        if(h.player == player && h.hand == hand)
        {
            return i;
        }
    }
    return -1;
}

void endPropBlade(int index)
{
    World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(index)];
    if(B3_IS_NON_NULL(h.joint) && b3Joint_IsValid(h.joint))
    {
        b3DestroyJoint(h.joint, true);
    }
    if(B3_IS_NON_NULL(h.anchor) && b3Body_IsValid(h.anchor))
    {
        b3DestroyBody(h.anchor);
    }
    world->propBlades.erase(world->propBlades.begin() + index);
}

// Whether the hold `h` still has its prop's body on its joint.
[[nodiscard]] bool propBladeValid(const World::PropBlade& h)
{
    return solidProp(h.num) && B3_IS_NON_NULL(h.joint) && b3Joint_IsValid(h.joint) && B3_IS_NON_NULL(h.anchor) && b3Body_IsValid(h.anchor) &&
           B3_ID_EQUALS(b3Joint_GetBodyB(h.joint), world->slots[h.num].body);
}

// ---- Pushed as well as dragged (vr_flat_grab_push; World::PushKeep): flat-screen Grab / Pull's holds, a big prop's drag
// (holdPropDrag, syncPropBlades) and a ragdoll's limb (grabRagdollReachImpl, syncRagdollGrabs), the same rules for both.

// The level forward and left of `player`'s yaw (a pushed hold's arm, PushKeep::arm).
void yawAxes(const edict_t* player, glm::vec2& forward, glm::vec2& left)
{
    const float yaw = player->v.v_angle[1] * (3.14159265f / 180.f);
    forward = glm::vec2{za::cos(yaw), za::sin(yaw)};
    left = glm::vec2{-forward.y, forward.x};
}

// `k` begins on the point `point` (units) as `player` takes hold of it: kept (vr_flat_grab_push) where it is, level,
// relative to his body and yaw.
void keepBegin(World::PushKeep& k, const edict_t* player, const glm::vec3& point)
{
    k = World::PushKeep{};
    k.keep = vr_flat_grab_push.value != 0.f;
    glm::vec2 forward, left;
    yawAxes(player, forward, left);
    const glm::vec2 from = glm::vec2{point.x - player->v.origin[0], point.y - player->v.origin[1]};
    k.arm = glm::vec2{glm::dot(from, forward), glm::dot(from, left)};
}

// A kept hold's target: `at` (the hand's) moved level to as far ahead of the body of `player` as it took hold, turned with
// him (his note 2026-11-26: "keep the same arm length as when I grabbed it, so that it gets pushed as I move forward"), its
// height still the hand's. Not kept: `at` as it is.
void keepTarget(const World::PushKeep& k, const edict_t* player, glm::vec3& at)
{
    if(!k.keep)
    {
        return;
    }
    glm::vec2 forward, left;
    yawAxes(player, forward, left);
    const glm::vec2 level = glm::vec2{player->v.origin[0], player->v.origin[1]} + forward * k.arm.x + left * k.arm.y;
    at.x = level.x;
    at.y = level.y;
}

// How far round (degrees, level, seen from the eyes of `player`) a pushed hold's point `held` is from `at`, where the arm
// reaches for it (vr_flat_grab_push_angle).
[[nodiscard]] float armAngle(const edict_t* player, const glm::vec3& at, const glm::vec3& held)
{
    const glm::vec2 eye = glm::vec2{player->v.origin[0] + player->v.view_ofs[0], player->v.origin[1] + player->v.view_ofs[1]};
    const glm::vec2 reach = glm::vec2{at.x, at.y} - eye, point = glm::vec2{held.x, held.y} - eye;
    const float across = glm::length(reach) * glm::length(point);
    return across > 1.f ? za::acos(za::clamp(glm::dot(reach, point) / across, -1.f, 1.f)) * (180.f / 3.14159265f) : 0.f;
}

// Each step (before it), a kept hold of `player`: how far its point `held` lags its target `at`, level (m), how fast it slips
// from it (m/s: the target was at `was`, the point goes at `pointVel` m/s), and how far round from the arm's reach it is,
// seen from the eyes (for vr_debug_ragdoll 2's print; keepLetGo's own lets go). A ragdoll's: its body's mass centre and
// where that is kept (syncRagdollStrains).
void keepMeasure(World::PushKeep& k, const edict_t* player, const glm::vec3& at, const glm::vec3& held, const glm::vec3& was,
    const glm::vec3& pointVel, float dt)
{
    k.lag = k.slip = glm::vec2{0.f};
    if(!k.keep || dt <= 0.f || glm::distance(was, at) > 0.5f * world->m2u) // (a teleport: none)
    {
        return;
    }
    k.lag = glm::vec2{at.x - held.x, at.y - held.y} / world->m2u;
    k.slip = glm::vec2{at.x - was.x, at.y - was.y} / (world->m2u * dt) - glm::vec2{pointVel.x, pointVel.y};
    k.angle = armAngle(player, at, held);
}

// Then the force it would need to keep its point with its target (the spring's at `hertz`, uncapped: `share` kg, its share
// of the mass, by `lag` and `slip`: its own, or its body's: syncRagdollGrabs) and its strain above vr_flat_grab_push_force
// (keepLetGo lets go past it). `what`, `num`: for vr_debug_ragdoll 2's print (none: nullptr).
void keepStrain(World::PushKeep& k, const edict_t* player, const glm::vec2& lag, const glm::vec2& slip, float share, float hertz, float dt,
    const char* what, int num)
{
    if(!k.keep || dt <= 0.f)
    {
        return;
    }
    const float omega = 2.f * 3.14159265f * hertz;
    k.need = share * glm::length(lag * (omega * omega) + slip * (2.f * omega));
    // (Over it for long enough, by how far over: a short jolt is borne, a hold far too heavy goes at once. Only while he
    // moves: his pace is what is too fast for it. Stopped, a body still sliding on from his push doesn't wrench it off his
    // hands: an 80 kg grunt pushed at a walk slid on at 6 m/s as he stopped, its hold needing 9-16k N; the hold's own
    // distance still lets go of what slides or falls far off.)
    constexpr float movingLeast = 40.f; // u/s, level
    const float limit = za::max(vr_flat_grab_push_force.value, 1.f);
    const bool moving = glm::length(glm::vec2{player->v.velocity[0], player->v.velocity[1]}) > movingLeast;
    k.strain = moving && k.need > limit ? k.strain + dt * (k.need / limit) : za::max(k.strain - dt, 0.f);
    k.needMost = za::max(k.needMost, k.need);
    if(what && vr_debug_ragdoll.value >= 2.f)
    {
        Con_Printf("%s %d pushed: lags %.2f m, slips %.2f m/s, he %.0f u/s: needs %.0f N a hand, strained %.2f s, %.0f degrees round\n",
            what, num, glm::length(lag), glm::length(slip), glm::length(glm::vec2{player->v.velocity[0], player->v.velocity[1]}), k.need,
            k.strain, k.angle);
    }
}

// Why a kept hold of `player` lets go now (nullptr: it holds on): its point `held` too far round from where the arm reaches
// for it, `asked` (vr_flat_grab_push_angle: too steep), or its hold needing more than vr_flat_grab_push_force for a moment
// (too heavy for that pace: keepStrain).
[[nodiscard]] const char* keepLetGo(World::PushKeep& k, const edict_t* player, const glm::vec3& asked, const glm::vec3& held)
{
    constexpr float strainMost = 0.15f; // s above the push force, times how far over (keepStrain)
    if(!k.keep)
    {
        return nullptr;
    }
    k.angle = armAngle(player, asked, held); // (as he turns now: keepStrain's is a frame old)
    k.angleMost = za::max(k.angleMost, k.angle);
    if(k.angle > za::max(vr_flat_grab_push_angle.value, 1.f))
    {
        return va("too steep: its point %.0f degrees round from the arm (at most %.0f)", k.angle, vr_flat_grab_push_angle.value);
    }
    if(k.strain > strainMost)
    {
        return va("too heavy for that pace: its hold needs %.0f N (at most %.0f)", k.need, vr_flat_grab_push_force.value);
    }
    return nullptr;
}

// Where a dragged prop's hold `h` asks its point to be now (units; before the lift's limit, syncPropBlades): the hand, its
// gap still being eased away (holdPropDrag); pushed as well (keepTarget).
[[nodiscard]] glm::vec3 dragTarget(const World::PropBlade& h, edict_t* player, glm::quat& turn)
{
    glm::vec3 at;
    handPose(player, h.hand, at, turn);
    at += h.gap * gapLeft(h.gapAt, h.gapOver);
    keepTarget(h.push, player, at);
    return at;
}

// The hands of `player` dragging prop `num` (PropBlade::need: each its share of the mass).
[[nodiscard]] int dragHands(int player, int num)
{
    int n = 0;
    for(const World::PropBlade& h : world->propBlades)
    {
        n += h.drag && h.player == player && h.num == num ? 1 : 0;
    }
    return n;
}
} // namespace

bool dragsProp(int player, int num)
{
    if(!world || vr_flat_grab_push.value == 0.f)
    {
        return false;
    }
    for(const World::PropBlade& h : world->propBlades)
    {
        if(h.drag && h.push.keep && h.player == player && h.num == num)
        {
            return true;
        }
    }
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        if(g.push.keep && g.player == player && g.num == num)
        {
            return true; // (a ragdoll he pushes by its limbs)
        }
    }
    return false;
}

void releasePropBlade(int player, int hand)
{
    if(const int index = world ? propBladeIndex(player, hand) : -1; index >= 0)
    {
        if(const World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(index)]; h.drag && h.push.keep && vr_debug_ragdoll.value)
        {
            Con_Printf("propdrag: prop %d: hand %d let go of; its hold needed %.0f N at most, %.0f degrees round at most\n", h.num, hand,
                h.push.needMost, h.push.angleMost);
        }
        endPropBlade(index);
    }
}

float holdPropBlade(int num, edict_t* player, int hand, const glm::vec3& handle, const glm::vec3& wound, const glm::vec3& shift, float force,
    float lift)
{
    const int pnum = NUM_FOR_EDICT(player);
    if(!world || !solidProp(num) || pnum < 1 || pnum > svs.maxclients)
    {
        return -1.f;
    }
    int index = propBladeIndex(pnum, hand);
    if(index >= 0 && (world->propBlades[static_cast<za::SizeT>(index)].num != num || world->propBlades[static_cast<za::SizeT>(index)].drag ||
                         !propBladeValid(world->propBlades[static_cast<za::SizeT>(index)])))
    {
        endPropBlade(index);
        index = -1;
    }
    const b3BodyId body = world->slots[num].body;
    glm::vec3 at;
    glm::quat turn;
    handPose(player, hand, at, turn);
    const float lever = glm::distance(handle, wound) / world->m2u;
    if(index < 0)
    {
        World::PropBlade h;
        h.player = pnum;
        h.hand = hand;
        h.num = num;
        h.force = za::max(force, 0.f);
        h.lift = za::max(lift, 0.f);
        h.lever = lever;
        h.shift = shift;
        h.gap = handle - at - shift;
        h.gapAt = qcvm->time;
        h.called = qcvm->time;
        b3BodyDef def = b3DefaultBodyDef();
        def.type = b3_kinematicBody;
        def.position = world->toM(handle);
        def.rotation = toB3(turn);
        def.userData = userOf(pnum);
        h.anchor = b3CreateBody(world->id, &def);
        b3MotorJointDef jd = b3DefaultMotorJointDef();
        jd.base.bodyIdA = h.anchor;
        jd.base.bodyIdB = body;
        jd.base.localFrameA = b3Transform{b3Vec3_zero, b3Quat_identity};
        jd.base.localFrameB = b3Transform{b3Body_GetLocalPoint(body, world->toM(handle)),
            toB3(glm::normalize(glm::inverse(fromB3(b3Body_GetRotation(body))) * turn))};
        jd.base.collideConnected = false;
        jd.linearVelocity = b3Vec3_zero;
        jd.angularVelocity = b3Vec3_zero;
        jd.maxVelocityForce = 0.f;
        jd.maxVelocityTorque = 0.f;
        jd.linearHertz = handleHertz();
        jd.linearDampingRatio = 1.f;
        jd.maxSpringForce = h.force;
        jd.angularHertz = handleHertz();
        jd.angularDampingRatio = 1.f;
        jd.maxSpringTorque = h.force * za::max(grabLever, lever);
        h.joint = b3CreateMotorJoint(world->id, &jd);
        b3Body_SetAwake(body, true);
        world->propBlades.pushBack(h);
        if(vr_debug_ragdoll.value || vr_debug_axestick.value)
        {
            Con_Printf("embed: prop %d: hand %d holds it by the blade's handle, %.1f units from the wound, %.1f from the hand, %.0f N at most (%.0f up)\n",
                num, hand, glm::distance(handle, wound), glm::length(handle - at), h.force, h.lift);
        }
        return 0.f;
    }
    World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(index)];
    h.force = za::max(force, 0.f);
    h.lift = za::max(lift, 0.f);
    h.lever = lever;
    h.shift = shift;
    h.called = qcvm->time;
    // (The handle as it is on the prop now: the blade rides it rigidly, and slides along its own line in it.)
    b3Joint_SetLocalFrameB(h.joint, b3Transform{b3Body_GetLocalPoint(body, world->toM(handle)), b3Joint_GetLocalFrameB(h.joint).q});
    return glm::length(glmv(b3Joint_GetConstraintForce(h.joint)));
}

// ---- A big prop dragged by its surface (flat-screen Grab / Pull, his note 2026-11-26: "for larger props (crates/barrels,
// explosive boxes), the hands should connect to their visible model and drag them similarly to ragdolls"; QC vr_flatgrab.qc
// propdrag). The blade hold's kinematic body at the hand and motor joint, at the point of the prop's shape nearest `target`,
// but its spring linear only: the prop hangs from the hand by that point (two hands: from the line between them), swings and
// tips, its far side dragging on the floor (`lift` N up at most, `force` level). The point comes to the hand over
// vr_flat_grab_reach_time (its gap eased away); it is let go of when it stays more than `letGo` units off the hand
// (caught on something, too heavy). The hand is drawn on that point (vr_view_hands.cpp limbHold, propDragHold).
float holdPropDrag(int num, edict_t* player, int hand, const glm::vec3& target, float force, float lift, float hertz, float letGo)
{
    const int pnum = NUM_FOR_EDICT(player);
    if(!world || !solidProp(num) || pnum < 1 || pnum > svs.maxclients)
    {
        return -1.f;
    }
    int index = propBladeIndex(pnum, hand);
    if(index >= 0 && (world->propBlades[static_cast<za::SizeT>(index)].num != num || !world->propBlades[static_cast<za::SizeT>(index)].drag ||
                         !propBladeValid(world->propBlades[static_cast<za::SizeT>(index)])))
    {
        endPropBlade(index);
        index = -1;
    }
    const b3BodyId body = world->slots[num].body;
    glm::vec3 at;
    glm::quat turn;
    handPose(player, hand, at, turn);
    if(index < 0)
    {
        b3Vec3 closest;
        (void)b3Body_GetClosestPoint(body, &closest, world->toM(target));
        const glm::vec3 point = world->toU(closest);
        World::PropBlade h;
        h.player = pnum;
        h.hand = hand;
        h.num = num;
        h.drag = true;
        h.force = za::max(force, 0.f);
        h.lift = za::max(lift, 0.f);
        h.hertz = za::max(hertz, 0.5f);
        h.letGo = za::max(letGo, 1.f);
        h.gapOver = za::clamp(vr_flat_grab_reach_time.value, 0.02f, 0.6f);
        h.gap = point - at;
        h.gapAt = qcvm->time;
        h.called = qcvm->time;
        keepBegin(h.push, player, point);
        b3BodyDef def = b3DefaultBodyDef();
        def.type = b3_kinematicBody;
        def.position = closest;
        def.rotation = toB3(turn);
        def.userData = userOf(pnum);
        h.anchor = b3CreateBody(world->id, &def);
        b3MotorJointDef jd = b3DefaultMotorJointDef();
        jd.base.bodyIdA = h.anchor;
        jd.base.bodyIdB = body;
        jd.base.localFrameA = b3Transform{b3Vec3_zero, b3Quat_identity};
        // (Its turn there: the hand's in the prop's frame as it took hold, for the hand drawn on it; no spring turns it.)
        jd.base.localFrameB = b3Transform{b3Body_GetLocalPoint(body, closest), toB3(glm::normalize(glm::inverse(fromB3(b3Body_GetRotation(body))) * turn))};
        jd.base.collideConnected = false;
        jd.linearVelocity = b3Vec3_zero;
        jd.angularVelocity = b3Vec3_zero;
        jd.maxVelocityForce = 0.f;
        jd.maxVelocityTorque = 0.f;
        jd.linearHertz = h.hertz;
        jd.linearDampingRatio = 1.f;
        jd.maxSpringForce = h.force;
        jd.angularHertz = 0.f;
        jd.angularDampingRatio = 0.f;
        jd.maxSpringTorque = 0.f;
        h.joint = b3CreateMotorJoint(world->id, &jd);
        b3Body_SetAwake(body, true);
        world->propBlades.pushBack(h);
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("propdrag: prop %d: hand %d takes hold of it at %.0f %.0f %.0f, %.1f units from the hand, %.0f N at most (%.0f up)%s\n",
                num, hand, point.x, point.y, point.z, glm::length(point - at), h.force, h.lift,
                h.push.keep ? va(", kept %.1f units ahead, %.1f left of the body", h.push.arm.x, h.push.arm.y) : "");
        }
        return 0.f;
    }
    World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(index)];
    h.force = za::max(force, 0.f);
    h.lift = za::max(lift, 0.f);
    h.hertz = za::max(hertz, 0.5f);
    h.letGo = za::max(letGo, 1.f);
    h.called = qcvm->time;
    const glm::vec3 held = world->toU(b3Body_GetWorldPoint(body, b3Joint_GetLocalFrameB(h.joint).p));
    const glm::vec3 asked = dragTarget(h, player, turn);
    // Let go of: pushed or dragged, its point too far round from the arm's reach (vr_flat_grab_push_angle: too steep) or its
    // hold needing more than vr_flat_grab_push_force for a moment (too heavy for that pace: keepLetGo); its point too far
    // off where the hold asks it (caught on something, too heavy).
    const char* why = keepLetGo(h.push, player, asked, held);
    if(!why && glm::distance(held, asked) > h.letGo + glm::length(h.gap) * gapLeft(h.gapAt, h.gapOver))
    {
        why = va("its point %.1f units off the hand (at most %.1f)", glm::distance(held, asked), h.letGo);
    }
    if(why)
    {
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("propdrag: prop %d: hand %d lets go, %s; it needed %.0f N at most, %.0f degrees\n", num, hand, why, h.push.needMost,
                h.push.angleMost);
        }
        endPropBlade(index);
        return -1.f;
    }
    return glm::length(glmv(b3Joint_GetConstraintForce(h.joint)));
}

bool propDragOf(int player, int hand, int& num, glm::vec3& at, glm::quat& turn, float& off)
{
    const int index = world ? propBladeIndex(player, hand) : -1;
    if(index < 0 || !world->propBlades[static_cast<za::SizeT>(index)].drag)
    {
        return false;
    }
    // (The client's call, in no QC VM: Box3D's ids alone, not solidProp's edict.)
    const World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(index)];
    if(h.num < 1 || h.num >= static_cast<int>(world->slots.size()) || B3_IS_NULL(world->slots[h.num].body) || B3_IS_NULL(h.joint) ||
       !b3Joint_IsValid(h.joint) || !B3_ID_EQUALS(b3Joint_GetBodyB(h.joint), world->slots[h.num].body))
    {
        return false;
    }
    const b3BodyId body = world->slots[h.num].body;
    const b3Transform frameB = b3Joint_GetLocalFrameB(h.joint);
    num = h.num;
    at = world->toU(b3Body_GetWorldPoint(body, frameB.p));
    turn = glm::normalize(fromB3(b3Body_GetRotation(body)) * fromB3(frameB.q));
    off = B3_IS_NON_NULL(h.anchor) && b3Body_IsValid(h.anchor) ? glm::distance(at, world->toU(b3Body_GetPosition(h.anchor))) : 0.f;
    return true;
}

// Before the step, after the ragdolls' holds: each prop's hold's body to its hand; those the QC no longer makes ended.
void syncPropBlades(float dt)
{
    constexpr double uncalled = 0.25; // s without the QC's call: its hand let go, the blade came out, the prop broke
    for(int i = static_cast<int>(world->propBlades.size()) - 1; i >= 0; i--)
    {
        World::PropBlade& h = world->propBlades[static_cast<za::SizeT>(i)];
        edict_t* player = h.player < qcvm->num_edicts ? EDICT_NUM(h.player) : nullptr;
        if(!player || player->free || player->v.health <= 0.f || !svs.clients[h.player - 1].active || !propBladeValid(h) ||
           qcvm->time - h.called > uncalled || qcvm->time < h.called)
        {
            endPropBlade(i);
            continue;
        }
        glm::vec3 at;
        glm::quat turn;
        if(h.drag)
        {
            at = dragTarget(h, player, turn);
        }
        else
        {
            handPose(player, h.hand, at, turn);
            at += h.shift + h.gap * handleGapLeft(h.gapAt);
        }
        const b3BodyId body = world->slots[h.num].body;
        // (Up, no more than its lift: by how far up the handle is asked from where it is.)
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(body, b3Joint_GetLocalFrameB(h.joint).p));
        if(h.drag)
        {
            // (A dragged prop: pulled level at its full force, up by no more than its lift. The hand above it asks no
            // higher than the spring's stretch for that lift (its mass at h.hertz): too heavy to hoist, it slides along on
            // its far side, the hand drawn down to it: vr_view_hands.cpp limbHold.)
            const float omega = 2.f * 3.14159265f * h.hertz;
            const float stretch = h.lift / za::max(b3Body_GetMass(body) * omega * omega, 1.f) * world->m2u;
            at.z = za::min(at.z, held.z + stretch);
        }
        const glm::vec3 way = at - held;
        const float far = glm::length(way);
        const float up = far > 0.05f ? za::clamp(way.z / far, 0.f, 1.f) : 0.f;
        const float most = glm::mix(h.force, za::min(h.force, h.lift), up);
        b3MotorJoint_SetMaxSpringForce(h.joint, most);
        // (A dragged prop's spring is linear only: holdPropDrag.)
        b3MotorJoint_SetMaxSpringTorque(h.joint, h.drag ? 0.f : most * za::max(grabLever, h.lever));
        b3MotorJoint_SetLinearHertz(h.joint, h.drag ? h.hertz : handleHertz());
        b3MotorJoint_SetAngularHertz(h.joint, h.drag ? 0.f : handleHertz());
        if(h.drag && h.push.keep)
        {
            // Pushed or dragged (vr_flat_grab_push): its strain and angle (keepStrain; holdPropDrag lets go past either).
            const float share = b3Body_GetMass(body) / static_cast<float>(za::max(dragHands(h.player, h.num), 1));
            keepMeasure(h.push, player, at, held, world->toU(b3Body_GetPosition(h.anchor)), glmv(b3Body_GetWorldPointVelocity(body, world->toM(held))), dt);
            keepStrain(h.push, player, h.push.lag, h.push.slip, share, h.hertz, dt, h.hand == 1 ? "propdrag: prop" : nullptr, h.num);
        }
        if(glm::distance(world->toU(b3Body_GetPosition(h.anchor)), at) > 0.5f * world->m2u)
        {
            b3Body_SetTransform(h.anchor, world->toM(at), toB3(turn)); // (a jump: a teleport)
            b3Body_SetLinearVelocity(h.anchor, b3Vec3_zero);
        }
        else
        {
            b3Body_SetTargetTransform(h.anchor, b3WorldTransform{world->toM(at), toB3(turn)}, dt, true);
        }
        b3Body_SetAwake(body, true);
    }
}

// The mass centre of ragdoll `r` (units) and its velocity (m/s); false: no mass.
[[nodiscard]] bool ragdollCentre(const RagdollBodies& r, glm::vec3& at, glm::vec3& vel)
{
    float mass = 0.f;
    glm::vec3 sum{0.f}, mom{0.f};
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const float m = b3Body_GetMass(body);
        mass += m;
        sum += world->toU(b3Body_GetWorldCenter(body)) * m;
        mom += glmv(b3Body_GetLinearVelocity(body)) * m;
    }
    if(mass <= 0.f)
    {
        return false;
    }
    at = sum / mass;
    vel = mom / mass;
    return true;
}

// After syncRagdollGrabs' holds: a kept limb hold (vr_flat_grab_push) carries its body as a whole, as a dragged prop is
// carried: its mass centre kept as far ahead of the player as it was when taken (RagdollGrab::bodyArm), level, by a big
// prop's drag spring on the whole mass (vr_flat_grab_drag_hertz, at most vr_flat_grab_drag_force a hand: each hand its
// share, spread over the parts by their masses), and its strain by that spring's need (keepMeasure, keepStrain). A limb's
// own hold alone could not: it is a spring as stiff as for the limb's mass (a hand of 1 kg), so an 80 kg grunt pushed at a
// walk by its hand folded the arm and lagged 0.35 m and more (its hold "needing" 14-25k N, let go of at once), and two hands
// on two limbs pull against each other as the body bends. The limb holds still keep the held limbs at the hands.
void syncRagdollStrains(float dt)
{
    const float hertz = za::max(vr_flat_grab_drag_hertz.value, 0.5f);
    const float omega = 2.f * 3.14159265f * hertz;
    for(World::RagdollGrab& g : world->ragdollGrabs)
    {
        const RagdollBodies* r = g.push.keep ? ragdollOf(g.num) : nullptr;
        edict_t* player = g.player >= 1 && g.player < qcvm->num_edicts ? EDICT_NUM(g.player) : nullptr;
        glm::vec3 centre, vel;
        if(!r || !player || !ragdollCentre(*r, centre, vel))
        {
            continue;
        }
        int hands = 0;
        for(const World::RagdollGrab& o : world->ragdollGrabs)
        {
            hands += o.push.keep && o.player == g.player && o.num == g.num ? 1 : 0;
        }
        glm::vec3 kept = centre;
        World::PushKeep body = g.push;
        body.arm = g.bodyArm;
        keepTarget(body, player, kept);
        const float angle = g.push.angle; // (the held point's, keepLetGo's)
        keepMeasure(g.push, player, kept, centre, g.bodyWas, vel, dt);
        g.push.angle = angle;
        g.bodyWas = kept;
        const float mass = ragdollMass(*r);
        const float share = mass / static_cast<float>(za::max(hands, 1));
        keepStrain(g.push, player, g.push.lag, g.push.slip, share, hertz, dt, g.hand == 1 ? "ragdoll: grab of" : nullptr, g.num);
        // (This hand's pull on the body: the spring's, level, at most the drag's force.)
        glm::vec2 pull = (g.push.lag * (omega * omega) + g.push.slip * (2.f * omega)) * share;
        const float most = za::max(vr_flat_grab_drag_force.value, 0.f);
        if(const float len = glm::length(pull); len > most)
        {
            pull *= most / len;
        }
        for(int b = 0; b < r->count && mass > 0.f; b++)
        {
            if(!partCut(*r, b))
            {
                const b3BodyId part = r->body[static_cast<za::SizeT>(b)];
                const float of = b3Body_GetMass(part) / mass;
                b3Body_ApplyForceToCenter(part, b3Vec3{pull.x * of, pull.y * of, 0.f}, true);
            }
        }
    }
}

// Before the step: each hold's body to its hand (a hold whose limb stays far from the hand is let go of: QC sees it
// gone, ragdollheld); those of what is gone ended.
void syncRagdollGrabs(float dt)
{
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        if(g.player == 0)
        {
            if(!physgunFrame(g, dt))
            {
                endGrab(i);
            }
            continue;
        }
        edict_t* player = g.player < qcvm->num_edicts ? EDICT_NUM(g.player) : nullptr;
        const RagdollBodies* r = ragdollOf(g.num);
        if(!player || player->free || player->v.health <= 0.f || !svs.clients[g.player - 1].active || !r || g.part >= r->count ||
           vr_ragdoll_grab.value < 1.f)
        {
            endGrab(i);
            continue;
        }
        // (A hold the QC no longer has: its hand holds something else, or nothing. A tug's: the QC's to end.)
        const int heldOfs = progs::handFields(g.hand).held;
        if(!g.tug && heldOfs >= 0 && fieldInt(player, heldOfs) != EDICT_TO_PROG(EDICT_NUM(g.num)))
        {
            endGrab(i);
            continue;
        }
        glm::vec3 at;
        glm::quat turn;
        handPose(player, g.hand, at, turn);
        at += g.bladeShift + bladeReachNow(g, turn) + bladeGapNow(g);
        keepTarget(g.push, player, at); // (flat-screen Grab / Pull: kept at the grab's arm length, pushed and dragged)
        const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
        if(B3_IS_NULL(g.anchor) || !b3Body_IsValid(g.anchor) || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
        {
            endGrab(i);
            continue;
        }
        const float force = g.force >= 0.f ? g.force : za::max(vr_ragdoll_grab_force.value, 0.f);
        b3MotorJoint_SetMaxSpringForce(g.joint, force);
        b3MotorJoint_SetMaxSpringTorque(g.joint, force * za::max(grabLever, g.bladeLever));
        const glm::vec3 from = world->toU(b3Body_GetPosition(g.anchor));
        if(glm::distance(from, at) > 0.5f * world->m2u)
        {
            b3Body_SetTransform(g.anchor, world->toM(at), toB3(turn)); // (a jump: a teleport, the hand taken back)
            b3Body_SetLinearVelocity(g.anchor, b3Vec3_zero);
        }
        else
        {
            b3Body_SetTargetTransform(g.anchor, b3WorldTransform{world->toM(at), toB3(turn)}, dt, true);
        }
        const b3Transform frameB = b3Joint_GetLocalFrameB(g.joint);
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(part, frameB.p));
        if(g.push.keep)
        {
            // Pushed or dragged by its limb (vr_flat_grab_push): the big prop's rules (its strain by its body as a whole after
            // all the holds, syncRagdollStrains; keepLetGo on the last step's).
            if(const char* why = keepLetGo(g.push, player, at, held))
            {
                if(vr_debug_ragdoll.value)
                {
                    Con_Printf("ragdoll: %d part %d: hand %d lets go, %s; it needed %.0f N at most, %.0f degrees\n", g.num, g.part, g.hand, why,
                        g.push.needMost, g.push.angleMost);
                }
                endGrab(i);
                continue;
            }
        }
        // (A tug's: its hand lets go at once once its controller is vr_agrunt_tug_let_go from where it holds the weapon:
        // pulled away or stepped back before the monster's grip gives. The QC ends the tug, lost: VR_AG_TugRagFrame.)
        const float letGo = vr_agrunt_tug_let_go.value > 0.f ? vr_agrunt_tug_let_go.value * 0.01f * world->m2u : g.stuck;
        // (A downed one's, tugBack: its body still falling or staggering drags the limb further than a standing one's
        // pinned arm; let go only at the limb grab's own lag, `stuck`.)
        const float tugGo = g.tugBack > 0.f ? za::max(letGo, g.stuck) : letGo;
        if(g.blade)
        {
            // (A blade's: how far is the QC's. Its spring's stiffness as tuned now. Staggering, its stance gives way to what the
            // hold passes on to its pelvis.)
            b3MotorJoint_SetLinearHertz(g.joint, bladeHertz());
            if(g.bladeHandle)
            {
                // (By its handle: a stiff hold, its place and its turn, limited by its force and that force at the blade's length.
                // A spring's rate goes by the mass it holds: at the handle, out on the blade's lever, the limb's own counts for
                // little (some 0.6 kg of a 10 kg chest) while the whole body hangs on it: at Moving: Stiffness's 24 Hz the
                // handle sat 14 units from a still hand, and at a limb grab's turn rate the limb alone turned, the body stayed
                // lying: a lift raised the handle, not the wound.)
                b3MotorJoint_SetLinearHertz(g.joint, handleHertz());
                b3MotorJoint_SetAngularHertz(g.joint, handleHertz());
            }
            if(RagdollBodies* moved = ragdollOf(g.num); moved && moved->active.on && moved->active.stagger.on)
            {
                moved->active.stagger.ledGive = za::max(vr_embed_move_give.value * za::max(vr_embed_hold_strength.value, 0.f), 0.01f);
            }
        }
        else if(glm::distance(held, at) > (g.tug ? tugGo : g.stuck > 0.f ? g.stuck : grabStuck))
        {
            if(g.farSince < 0.0 && !g.tug)
            {
                g.farSince = qcvm->time;
            }
            else if(g.tug || qcvm->time - g.farSince > grabStuckTime)
            {
                if(vr_debug_ragdoll.value)
                {
                    Con_Printf("ragdoll: %d part %d left behind by hand %d (%.0f units)\n", g.num, g.part, g.hand, glm::distance(held, at));
                }
                endGrab(i);
                continue;
            }
        }
        else
        {
            g.farSince = -1.0;
        }
        if(g.tug && g.tugBack > 0.f && r->count > 1 && g.part != 0)
        {
            // A downed one's arm pulling its weapon back (ragdollTug what 6): the held limb towards its body's middle, the
            // body the other way (its own pull: it never slides it; your hold's spring feels it).
            const b3BodyId root = r->body[0];
            const glm::vec3 mid = world->toU(b3Body_GetPosition(root));
            const glm::vec3 to = mid - held;
            const float len = glm::length(to);
            if(len > 0.5f)
            {
                const glm::vec3 f = to * (g.tugBack / len);
                b3Body_ApplyForce(part, b3v(f), b3Body_GetWorldPoint(part, frameB.p), true);
                b3Body_ApplyForceToCenter(root, b3v(-f), true);
            }
        }
        b3Body_SetAwake(part, true);
    }
    syncRagdollStrains(dt);
    syncPropBlades(dt);
}

// The distance (units) from hand `hand` of `player` to the point of the limb it holds; -1 if none.
[[nodiscard]] float grabReach(int player, int hand)
{
    const int index = grabIndex(player, hand);
    if(index < 0)
    {
        return -1.f;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    const RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part >= r->count || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
    {
        return -1.f;
    }
    glm::vec3 at;
    glm::quat turn;
    handPose(EDICT_NUM(player), hand, at, turn);
    const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
    const b3Vec3 local = b3Joint_GetLocalFrameB(g.joint).p;
    return glm::distance(world->toU(b3Body_GetWorldPoint(part, local)), at);
}

namespace
{
// ---- Points on a ragdoll's limbs (flames: vr_burning.qc's ragdollbone, ragdollpoint) ----

// The part of edict `num`'s ragdoll whose surface is nearest `at` (units); -1 if it isn't one.
[[nodiscard]] int ragdollBoneNear(int num, const glm::vec3& at)
{
    const RagdollBodies* r = ragdollOf(num);
    float distance = 0.f;
    return r ? nearestPart(*r, at, distance) : -1;
}

// A point of part `bone` of edict `num`'s ragdoll: from the world into the part's space (units), or back (toWorld). The
// point itself if it isn't one.
[[nodiscard]] glm::vec3 box3dRagdollPoint(int num, int bone, const glm::vec3& p, bool toWorld)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!r || bone < 0 || bone >= r->count)
    {
        return p;
    }
    const b3BodyId body = r->body[static_cast<za::SizeT>(bone)];
    if(toWorld)
    {
        return world->toU(b3Body_GetWorldPoint(body, b3v(p / world->m2u)));
    }
    return glmv(b3Body_GetLocalPoint(body, world->toM(p))) * world->m2u;
}
} // namespace

} // namespace box3d::detail

namespace qvr::box3d
{
bool ragdollReach(int num, const glm::vec3& at)
{
    const RagdollBodies* r = ragdollOf(num);
    float distance = 0.f;
    const int part = r && vr_ragdoll_grab.value >= 1.f ? nearestPart(*r, at, distance) : -1;
    if(part >= 0 && vr_debug_ragdoll.value >= 2.f)
    {
        Con_Printf("ragdoll: %d: a hand %.1f units from part %d\n", num, distance, part);
    }
    return part >= 0 && distance <= za::max(vr_ragdoll_grab_reach.value, 0.f);
}

bool ragdollWeaponHold(int num, int rides, int part, const glm::vec3& grip)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!r || !r->rig || rides < 0 || rides >= r->count || rides >= r->rig->numBones || part < 0 || part >= r->count)
    {
        return false;
    }
    const ragdoll::Rig& rig = *r->rig;
    const int holder = rig.bones[rides].joint == ragdoll::Joint::Loose ? looseHolder(rig, rides) : rides;
    if(part == rides || part == holder)
    {
        return true;
    }
    // Its arms: the holder's arm (its hand or forearm holds it), and the other with its hand on the grip (a gun's two
    // hands, or one welded by its forearm, the other's hand at its grip); never a torso's part.
    constexpr float onGrip = 4.f; // units from the grip to the hand's surface
    for(const int limb : {0, 1})
    {
        const int lower = ragactive::limbJointOf(rig, limb, true);
        if(lower < 0 || lower >= r->count)
        {
            continue;
        }
        int hand = lower;
        for(int c = 0; c < r->count && c < rig.numBones; c++)
        {
            if(rig.bones[c].parent == lower)
            {
                hand = c;
                break;
            }
        }
        if(part != hand && part != lower)
        {
            continue;
        }
        if(holder == hand || holder == lower)
        {
            return true;
        }
        b3Vec3 closest;
        if(b3Body_GetClosestPoint(r->body[static_cast<za::SizeT>(hand)], &closest, world->toM(grip)) * world->m2u <= onGrip)
        {
            return true;
        }
    }
    return false;
}

RagdollGrabResult ragdollGrab(edict_t* corpse, edict_t* player, int hand)
{
    const RagdollGrabResult took = grabRagdoll(corpse, player, hand);
    if(took == RagdollGrabResult::Limb && corpse && corpse->v.health <= 0.f)
    {
        qvr::goresound::send(qvr::goresound::Kind::Squelch, NUM_FOR_EDICT(corpse), corpse->v.origin, 0.8f); // (a dead body)
    }
    return took;
}

int ragdollGrabReach(edict_t* corpse, edict_t* player, int hand, const glm::vec3& target, float reach, int avoidPart)
{
    const int took = world && corpse && player ? grabRagdollReachImpl(corpse, player, hand, target, reach, avoidPart) : 0;
    // (Flat-screen Grab / Pull's: kept at the grab's arm length, pushed as you walk into it, dragged as you back away, let go
    // of when too steep or too heavy for the pace, as a big prop's drag: PushKeep.)
    if(const int index = took > 0 ? grabIndex(NUM_FOR_EDICT(player), hand) : -1; index >= 0)
    {
        World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
        const RagdollBodies* r = ragdollOf(g.num);
        if(r && g.part < r->count && B3_IS_NON_NULL(g.joint) && b3Joint_IsValid(g.joint))
        {
            const glm::vec3 held = world->toU(b3Body_GetWorldPoint(r->body[static_cast<za::SizeT>(g.part)], b3Joint_GetLocalFrameB(g.joint).p));
            keepBegin(g.push, player, held);
            if(glm::vec3 centre, vel; g.push.keep && ragdollCentre(*r, centre, vel))
            {
                World::PushKeep body;
                keepBegin(body, player, centre);
                g.bodyArm = body.arm;
                g.bodyWas = centre;
            }
            if(g.push.keep && B3_IS_NON_NULL(g.anchor) && b3Body_IsValid(g.anchor))
            {
                // (Its hold begins where it is kept, the hand's height: not at the hand, a jump the next step would take as
                // the hold's slip, too heavy for that pace at once.)
                glm::vec3 at;
                glm::quat turn;
                handPose(player, hand, at, turn);
                keepTarget(g.push, player, at);
                b3Body_SetTransform(g.anchor, world->toM(at), b3Body_GetRotation(g.anchor));
                b3Body_SetLinearVelocity(g.anchor, b3Vec3_zero);
            }
            if(vr_debug_ragdoll.value && g.push.keep)
            {
                Con_Printf("ragdoll: %d part %d: hand %d keeps it %.1f units ahead, %.1f left of the body\n", g.num, g.part, hand, g.push.arm.x,
                    g.push.arm.y);
            }
        }
    }
    if(took > 0 && corpse && corpse->v.health <= 0.f)
    {
        qvr::goresound::send(qvr::goresound::Kind::Squelch, NUM_FOR_EDICT(corpse), corpse->v.origin, 0.8f);
    }
    return took;
}

bool ragdollThrowWhole(edict_t* corpse, edict_t* player, const glm::vec3& velocity)
{
    if(!world || !corpse || !player)
    {
        return false;
    }
    const int pnum = NUM_FOR_EDICT(player);
    const int num = NUM_FOR_EDICT(corpse);
    for(int i = static_cast<int>(world->ragdollGrabs.size()) - 1; i >= 0; i--)
    {
        const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(i)];
        if(g.player == pnum && g.num == num)
        {
            endGrab(i);
        }
    }
    RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        return false;
    }
    r->thrownPeak = 0.f;
    r->thrownAt = qcvm->time;
    const glm::vec3 v = glmv(world->toM(velocity * za::max(vr_ragdoll_throw.value, 0.f)));
    const float speed = glm::length(v);
    for(int b = 0; b < r->count; b++)
    {
        if(partCut(*r, b))
        {
            continue;
        }
        const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
        if(B3_IS_NULL(body) || !b3Body_IsValid(body))
        {
            continue;
        }
        if(speed > 0.01f)
        {
            const glm::vec3 now = glmv(b3Body_GetLinearVelocity(body));
            const glm::vec3 dir = v / speed;
            const float along = glm::dot(now, dir);
            if(along < speed)
            {
                b3Body_SetLinearVelocity(body, b3v(now + dir * (speed - along)));
            }
        }
        b3Body_SetAwake(body, true);
    }
    if(speed > 0.01f)
    {
        (void)ragdollGrace(corpse, pnum, 0.35f);
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d whole-body %s by player %d at %.0f u/s\n", num, speed > 0.01f ? "thrown" : "dropped", pnum,
            speed * world->m2u);
    }
    return true;
}

float ragdollTug(edict_t* ent, edict_t* player, int hand, int what, const glm::vec3& grip, float strength, float force)
{
    return world && ent && player ? tugRagdoll(ent, player, hand, what, grip, strength, force) : (what == 1 ? -1.f : 0.f);
}

float propBladeHold(int num, edict_t* player, int hand, const glm::vec3& handle, const glm::vec3& wound, const glm::vec3& shift, float force,
    float lift)
{
    return world && player ? holdPropBlade(num, player, hand, handle, wound, shift, force, lift) : -1.f;
}

void propBladeRelease(int player, int hand)
{
    releasePropBlade(player, hand);
}

float propDrag(int num, edict_t* player, int hand, const glm::vec3& target, float force, float lift, float hertz, float letGo)
{
    return world && player ? holdPropDrag(num, player, hand, target, force, lift, hertz, letGo) : -1.f;
}

bool propDragHold(int player, int hand, PropDragHold& out)
{
    out = PropDragHold{};
    return world && propDragOf(player, hand, out.num, out.at, out.turn, out.off);
}

bool ragdollHoldSoft(edict_t* ent, edict_t* player, int hand, float reach, float force, float hertz, float stuck, bool head,
    bool torso, bool pack)
{
    return ent && player && holdSoft(ent, player, hand, reach, force, hertz, stuck, head, torso, pack);
}

float ragdollHoldForce(edict_t* player, int hand, float force, float hertz)
{
    const int index = world && player ? grabIndex(NUM_FOR_EDICT(player), hand) : -1;
    World::RagdollGrab* g = index >= 0 ? &world->ragdollGrabs[static_cast<za::SizeT>(index)] : nullptr;
    if(!g || g->force < 0.f)
    {
        return -1.f; // (none, or the usual hold: vr_ragdoll_grab_force)
    }
    if(force >= 0.f)
    {
        g->force = force; // (syncRagdollGrabs sets the joint's)
    }
    if(hertz > 0.f && hertz != g->hertz && B3_IS_NON_NULL(g->joint) && b3Joint_IsValid(g->joint))
    {
        g->hertz = hertz; // (its turn half that: startHold's)
        b3MotorJoint_SetLinearHertz(g->joint, hertz);
        b3MotorJoint_SetAngularHertz(g->joint, hertz * 0.5f);
    }
    return g->force;
}

float ragdollBodyMass(int num)
{
    const RagdollBodies* rp = world ? ragdollOf(num) : nullptr;
    return rp ? ragdollMass(*rp) : 0.f;
}

float ragdollWeight(edict_t* ent)
{
    return ent && world ? ragdollBodyMass(NUM_FOR_EDICT(ent)) * za::max(-b3World_GetGravity(world->id).z, 0.f) : 0.f;
}

void ragdollRelease(edict_t* player, int hand, const glm::vec3& velocity)
{
    if(world)
    {
        releaseRagdoll(player, hand, velocity);
    }
}

bool physgunRagdollGrab(int num, const glm::vec3& from, const glm::vec3& dir, float range, const glm::vec3& fallback, int& part,
    float& distance)
{
    return world && physgunGrab(num, from, dir, range, fallback, part, distance);
}

bool physgunRagdollMove(const glm::vec3& target)
{
    const int index = world ? physgunIndex() : -1;
    if(index < 0)
    {
        return false;
    }
    world->ragdollGrabs[static_cast<za::SizeT>(index)].target = target;
    return true;
}

void physgunRagdollRelease(const glm::vec3& velocity)
{
    if(world)
    {
        physgunRelease(velocity);
    }
}

bool physgunRagdollPoints(glm::vec3& grip, glm::vec3& root)
{
    const int index = world ? physgunIndex() : -1;
    const World::RagdollGrab* g = index >= 0 ? &world->ragdollGrabs[static_cast<za::SizeT>(index)] : nullptr;
    const RagdollBodies* r = g ? ragdollOf(g->num) : nullptr;
    if(!r || g->part >= r->count || B3_IS_NULL(g->joint) || !b3Joint_IsValid(g->joint))
    {
        return false;
    }
    grip = world->toU(b3Body_GetWorldPoint(r->body[static_cast<za::SizeT>(g->part)], b3Joint_GetLocalFrameB(g->joint).p));
    root = world->toU(b3Body_GetWorldCenter(r->body[0]));
    return true;
}

int ragdollHeld(edict_t* player, int hand)
{
    if(!world)
    {
        return 0;
    }
    const int index = grabIndex(NUM_FOR_EDICT(player), hand);
    return index < 0 ? 0 : 1;
}

float ragdollHandReach(edict_t* player, int hand)
{
    return world ? grabReach(NUM_FOR_EDICT(player), hand) : -1.f;
}

bool ragdollHold(int player, int hand, RagdollHold& out)
{
    out = RagdollHold{};
    const int index = world ? grabIndex(player, hand) : -1;
    if(index < 0)
    {
        return false;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    const RagdollBodies* r = ragdollOf(g.num);
    // (A blade's: the hand is drawn on the blade, its load is the tether's, no struggle against it: no hold on a limb.)
    if(!r || g.blade || g.part >= r->count || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
    {
        return false;
    }
    // (The joint holds its frame B, on the part, at the anchor, the hand: the part's frame is ragdoll::publish's.)
    const b3Transform frameB = b3Joint_GetLocalFrameB(g.joint);
    out.num = g.num;
    out.part = g.part;
    out.at = glmv(frameB.p) * world->m2u;
    out.turn = fromB3(frameB.q);
    out.tug = g.tug;
    out.kept = g.push.keep;
    if(g.tug && B3_IS_NON_NULL(g.anchor) && b3Body_IsValid(g.anchor))
    {
        out.stretch = g.tugStretch;
        out.target = world->toU(b3Body_GetWorldPoint(g.anchor, b3Joint_GetLocalFrameA(g.joint).p));
    }
    return true;
}

bool ragdollHoldPoint(int player, int hand, glm::vec3& out)
{
    const int index = world ? grabIndex(player, hand) : -1;
    if(index < 0)
    {
        return false;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    const RagdollBodies* r = ragdollOf(g.num);
    if(!r || g.part >= r->count || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
    {
        return false;
    }
    // (The joint's frame B is on the part, at the anchor: ragdollHold.)
    const b3Transform frameB = b3Joint_GetLocalFrameB(g.joint);
    out = world->toU(b3TransformPoint(b3Body_GetTransform(r->body[static_cast<za::SizeT>(g.part)]), frameB.p));
    return true;
}

bool ragdollBladeSpring(int player, int hand, glm::vec3& limb, glm::vec3& target, float& force)
{
    const int index = world ? grabIndex(player, hand) : -1;
    if(index < 0 || !world->ragdollGrabs[static_cast<za::SizeT>(index)].blade || !ragdollHoldPoint(player, hand, limb))
    {
        return false;
    }
    const World::RagdollGrab& g = world->ragdollGrabs[static_cast<za::SizeT>(index)];
    if(B3_IS_NULL(g.anchor) || !b3Body_IsValid(g.anchor))
    {
        return false;
    }
    target = world->toU(b3Body_GetWorldPoint(g.anchor, b3Joint_GetLocalFrameA(g.joint).p));
    force = g.force;
    return true;
}

int ragdollPartCentre(const glm::vec3& from, int part, glm::vec3& out, const char* name)
{
    if(!world)
    {
        return 0;
    }
    const RagdollBodies* best = nullptr;
    float bestD = 1e30f;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0 && r.count > 0)
        {
            const float d = glm::distance(world->toU(b3Body_GetPosition(r.body[0])), from);
            if(d < bestD)
            {
                bestD = d;
                best = &r;
            }
        }
    }
    if(best && part == -3) // (its neck: its head's joint)
    {
        glm::vec3 axis{0.f}, head{0.f};
        return ragdollNeck(best->num, out, axis, head) ? best->num : 0;
    }
    if(best && part == -2) // (its head)
    {
        part = best->rig->head >= 0 && !partCut(*best, best->rig->head) ? best->rig->head : best->count;
    }
    else if(best && part == -4) // (the first named so)
    {
        part = best->count;
        for(int b = 0; b < best->count && name; b++)
        {
            if(!partCut(*best, b) && !ZA_STRNCMP(best->rig->bones[b].name, name, ZA_STRLEN(name)))
            {
                part = b;
                break;
            }
        }
    }
    else if(best && part < 0) // (the part nearest `from`)
    {
        float distance = 0.f;
        part = nearestPart(*best, from, distance);
    }
    if(!best || part < 0 || part >= best->count)
    {
        return 0;
    }
    out = world->toU(b3Body_GetWorldCenter(best->body[static_cast<za::SizeT>(part)]));
    return best->num;
}

int ragdollBone(int num, const glm::vec3& at, float within)
{
    if(within < 0.f)
    {
        return ragdollBoneNear(num, at);
    }
    const RagdollBodies* r = ragdollOf(num);
    float distance = 0.f;
    const int part = r ? nearestPart(*r, at, distance) : -1;
    return distance <= within ? part : -1;
}

namespace
{
// ---- Ragdolls in a box (QC rogue_buzzsaw.qc: the buzzsaw's blade cutting corpses and knocked-down monsters) ----

// Whether part `body` of a ragdoll is in the world box lo..hi (its shapes, exactly: GJK against the box's corners).
[[nodiscard]] bool partInBox(b3BodyId body, const glm::vec3& lo, const glm::vec3& hi)
{
    const glm::vec3 half = (hi - lo) * 0.5f;
    b3Vec3 corners[8];
    for(int i = 0; i < 8; i++)
    {
        corners[i] = world->toM(glm::vec3{(i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z});
    }
    const b3ShapeProxy proxy{corners, 8, 0.f};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = ~uint64_t{0};
    filter.maskBits = ~uint64_t{0};
    return b3Body_OverlapShape(body, world->toM((lo + hi) * 0.5f), &proxy, filter, b3Body_GetTransform(body));
}

// The first part of ragdoll `r` after `after` in the box lo..hi; -1 none.
[[nodiscard]] int partInBoxAfter(const RagdollBodies& r, const glm::vec3& lo, const glm::vec3& hi, int after)
{
    for(int b = za::max(after + 1, 0); b < r.count; b++)
    {
        if(!partCut(r, b) && partInBox(r.body[static_cast<za::SizeT>(b)], lo, hi))
        {
            return b;
        }
    }
    return -1;
}

// The lowest edict number above `after` of a ragdoll with a part whose box meets lo..hi (the broad phase: only what
// lies at the box is looked at); 0 none.
[[nodiscard]] int ragdollNearBox(const glm::vec3& lo, const glm::vec3& hi, int after)
{
    struct Query
    {
        int after;
        int best;
    } q{after, 0};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = ~uint64_t{0};
    filter.maskBits = catCorpse;
    b3World_OverlapAABB(world->id, b3AABB{world->toM(lo), world->toM(hi)}, filter,
        [](b3ShapeId shape, void* raw) {
            Query& query = *static_cast<Query*>(raw);
            const int num = numOf(shape);
            if(num > query.after && (query.best == 0 || num < query.best) && isRagdollSlot(num))
            {
                query.best = num;
            }
            return true;
        },
        &q);
    return q.best;
}

} // namespace

int ragdollInBox(const glm::vec3& lo, const glm::vec3& hi, int after)
{
    if(!world || vr_ragdoll.value < 1.f || world->generation != worldGeneration())
    {
        return 0;
    }
    for(int num = ragdollNearBox(lo, hi, after); num > 0; num = ragdollNearBox(lo, hi, num))
    {
        const RagdollBodies* r = ragdollOf(num);
        if(r && partInBoxAfter(*r, lo, hi, -1) >= 0)
        {
            return num;
        }
    }
    return 0;
}

int ragdollPartInBox(int num, const glm::vec3& lo, const glm::vec3& hi, int after, glm::vec3& at)
{
    const RagdollBodies* r = ragdollOf(num);
    const int part = r ? partInBoxAfter(*r, lo, hi, after) : -1;
    if(part >= 0)
    {
        b3Vec3 closest;
        (void)b3Body_GetClosestPoint(r->body[static_cast<za::SizeT>(part)], &closest, world->toM((lo + hi) * 0.5f));
        at = world->toU(closest);
    }
    return part;
}

glm::vec3 ragdollPoint(int num, int bone, const glm::vec3& p, bool toWorld)
{
    return box3dRagdollPoint(num, bone, p, toWorld);
}

glm::vec3 ragdollPartMiddle(int num, int bone)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!world || !r || bone < 0 || bone >= r->count)
    {
        return glm::vec3{0.f};
    }
    const b3BodyId body = r->body[static_cast<za::SizeT>(bone)];
    return world->toU(b3Body_GetWorldPoint(body, b3Body_GetLocalCenter(body)));
}

// ---- Limb gore (ROUND21.md, "Limb gore"; QC vr_limbs.qc) ----
// Every limb as the head: its joint (ragdoll::limbJoint) cut (cutLimb), its bones' bodies gone, the piece flying off as
// a gib of its own (QC: its model vr_limbmodel.cpp's, placed by ragdollCut 0-3) or popped; the stump bleeds.

bool ragdollCutLimb(edict_t* ent, int bone, const glm::vec3& blade, float settle)
{
    if(!world || vr_ragdoll.value < 1.f)
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(ent);
    RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        // (A live monster just killed, a dying one, a corpse: its ragdoll now, from the frame it is in.)
        if(num <= svs.maxclients || ent->free || !deadMonster(ent) || hasFlag(ent, FL_SWIM))
        {
            return false;
        }
        const ragdoll::Rig* rig = ragdoll::rigFor(modelOf(ent));
        const int b = rig ? (bone < 0 ? rig->head : bone) : -1;
        if(!rig || b < 0 || (b != rig->head && !ragdoll::limbJoint(*rig, b)))
        {
            return false;
        }
        Slot& s = slotOf(num);
        destroyBody(s);
        cutDeathJoint = b; // (killed standing by the cut: an active death, startCutDeath)
        const bool made = createRagdoll(ent, num, s, true);
        cutDeathJoint = -1;
        if(!made)
        {
            return false;
        }
        r = ragdollOf(num);
    }
    else
    {
        // (A corpse's: its own motion as it lies or falls; the frame's knocks reach it after, feedRagdoll.)
        for(int b = 0; b < r->count; b++)
        {
            const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
            r->ownLin[static_cast<za::SizeT>(b)] = !partCut(*r, b) ? glmv(b3Body_GetLinearVelocity(body)) : glm::vec3{0.f};
            r->ownAng[static_cast<za::SizeT>(b)] = !partCut(*r, b) ? glmv(b3Body_GetAngularVelocity(body)) : glm::vec3{0.f};
        }
    }
    const int b = r ? (bone < 0 ? r->rig->head : bone) : -1;
    if(!r || b < 0 || !cutLimb(*r, ent, b, blade, true))
    {
        return false;
    }
    r->settle = za::min(r->settle, settle);
    // A driven death's reaction to it (vr_ragactive.cpp, "Dismemberment"): the hand to the stump, the balance beat, ...
    if(r->active.on)
    {
        ragactive::Context c;
        c.world = world->id;
        c.now = qcvm->time;
        c.gravity = za::max(-b3World_GetGravity(world->id).z, 0.f);
        c.floorBelow = activeFloorBelow;
        c.onFloor = activeOnFloor;
        c.num = num;
        c.born = r->born;
        const ActiveClass* ac = activeClassOf(ent);
        ragactive::cutReaction(r->active, activeParts(*r), b, ac && ac->crawls, c);
    }
    if(vr_debug_ragdoll.value)
    {
        const glm::vec3 pelvis = world->toU(b3Body_GetLinearVelocity(r->body[0]));
        Con_Printf("ragdoll: %d %s cut off: its own motion %.0f u/s (pelvis; %.0f level), its pelvis at %.0f u/s now\n", num,
            r->rig->bones[b].name, glm::length(r->ownLin[0] * world->m2u), glm::length(glm::vec2{r->ownLin[0] * world->m2u}),
            glm::length(pelvis));
    }
    if(b == r->rig->head && fields().vr_headless >= 0)
    {
        fieldFloat(ent, fields().vr_headless) = 1.f;
    }
    writeRagdoll(ent, slotOf(num)); // (drawn without it at once)
    return true;
}

bool ragdollDecap(edict_t* ent, const glm::vec3& blade, float settle)
{
    return ragdollCutLimb(ent, -1, blade, settle);
}

glm::vec3 ragdollCut(int num, int what, int bone)
{
    const RagdollBodies* r = ragdollOf(num);
    if(r && bone < 0)
    {
        bone = r->rig->head;
    }
    if(r && what == 6 && bone >= 0 && bone < r->count && !partCut(*r, bone))
    {
        // (The piece's middle now, on: the tests' cut at it.)
        const uint32_t bones = bone == r->rig->head ? (1u << bone) : ragdoll::limbBones(*r->rig, bone) & ~r->cut;
        const glm::vec3 mid = ragdoll::limbMiddle(*r->rig, bones);
        const b3WorldTransform hx = b3Body_GetTransform(r->body[static_cast<za::SizeT>(bone)]);
        return fromB3(hx.q) * (mid * r->scale) + world->toU(hx.p);
    }
    if(r && what == 7)
    {
        return glm::vec3{static_cast<float>(r->cut), static_cast<float>(r->lastCut), static_cast<float>(partsLeft(*r))};
    }
    if(r && what == 8)
    {
        return glm::vec3{static_cast<float>(r->lastCutBones), 0.f, 0.f};
    }
    if(!r || !r->cut)
    {
        return glm::vec3{0.f};
    }
    switch(what)
    {
    case 0: return r->headMid;
    case 1:
    {
        glm::vec3 a;
        held::anglesFromAxes(glm::mat3_cast(r->headRot), &a[0], false);
        return a;
    }
    case 2: return r->headVel;
    case 3: return r->headSpin;
    case 4:
    case 5:
    {
        const ragdoll::Rig& rig = *r->rig;
        if(bone < 0 || bone >= rig.numBones)
        {
            return glm::vec3{0.f};
        }
        const int parent = ragdoll::uncutParent(rig, bone, r->cut);
        if(parent < 0)
        {
            return glm::vec3{0.f};
        }
        const b3WorldTransform px = b3Body_GetTransform(r->body[static_cast<za::SizeT>(parent)]);
        const glm::quat q = fromB3(px.q);
        const ragdoll::Bone& piece = rig.bones[bone];
        if(what == 4)
        {
            return q * (piece.pivot * r->scale) + world->toU(px.p);
        }
        const glm::vec3 d = piece.end - piece.pivot;
        return glm::length(d) > 1e-3f ? q * glm::normalize(d) : q * glm::vec3{0.f, 0.f, 1.f};
    }
    default: return glm::vec3{0.f};
    }
}

// `ent`'s bones as drawn now: p_world = rot[b] * (scale * p_rest) + pos[b]: a ragdoll's bodies (the cut bones and the
// hidden ones in `cut`), else its frame's pose where it stands (yaw only). False: no rig.
[[nodiscard]] bool posedBones(edict_t* ent, const ragdoll::Rig*& rig, za::Array<glm::quat, ragdoll::maxBones>& rot,
    za::Array<glm::vec3, ragdoll::maxBones>& pos, float& scale, uint32_t& cut)
{
    const int num = NUM_FOR_EDICT(ent);
    if(world)
    {
        if(const RagdollBodies* r = ragdollOf(num))
        {
            rig = r->rig;
            scale = r->scale;
            cut = r->cut;
            for(int b = 0; b < rig->numBones; b++)
            {
                if(b >= r->count)
                {
                    cut |= 1u << b;
                    continue;
                }
                const b3WorldTransform x = b3Body_GetTransform(r->body[static_cast<za::SizeT>(b)]);
                rot[static_cast<za::SizeT>(b)] = fromB3(x.q);
                pos[static_cast<za::SizeT>(b)] = world->toU(x.p);
            }
            return true;
        }
    }
    qmodel_t* model = modelOf(ent);
    rig = model ? ragdoll::rigFor(model) : nullptr;
    if(!rig)
    {
        return false;
    }
    scale = za::clamp(1.f + fieldVec(ent, fields().model_scale).x, 0.25f, 4.f);
    cut = static_cast<uint32_t>(za::max(fieldFloatOr(ent, fields().vr_limbcut, 0.f), 0.f));
    const int pose = ragdoll::poseOfFrame(model, static_cast<int>(ent->v.frame));
    const glm::quat turn = glm::angleAxis(glm::radians(ent->v.angles[1]), glm::vec3{0.f, 0.f, 1.f});
    const glm::vec3 origin = vec(ent->v.origin);
    for(int b = 0; b < rig->numBones; b++)
    {
        glm::quat q;
        glm::vec3 p;
        ragdoll::bonePose(*rig, pose, b, q, p);
        rot[static_cast<za::SizeT>(b)] = turn * q;
        pos[static_cast<za::SizeT>(b)] = origin + turn * (p * scale);
    }
    return true;
}

int limbAt(edict_t* ent, const glm::vec3& at)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return -2;
    }
    const auto place = [&](int b, const glm::vec3& p) {
        return rot[static_cast<za::SizeT>(b)] * (p * scale) + pos[static_cast<za::SizeT>(b)];
    };
    // The bone struck: its vertex nearest.
    int hit = -1;
    float best = 1e30f;
    for(int b = 0; b < rig->numBones; b++)
    {
        if(cut & (1u << b))
        {
            continue;
        }
        for(const glm::vec3& p : rig->bones[b].points)
        {
            const float d = glm::distance(place(b, p), at);
            if(d < best)
            {
                best = d;
                hit = b;
            }
        }
    }
    if(hit < 0)
    {
        return -1;
    }
    if(ragdoll::headBones(*rig) & (1u << hit))
    {
        return rig->head;
    }
    if(!ragdoll::limbJoint(*rig, hit))
    {
        return -1; // (the torso, a loose piece)
    }
    // The nearest joint: the bone's own (its pivot) or one of its children's.
    int joint = hit;
    float jd = glm::distance(place(hit, rig->bones[hit].pivot), at);
    for(int c = hit + 1; c < rig->numBones; c++)
    {
        if(rig->bones[c].parent == hit && !(cut & (1u << c)) && ragdoll::limbJoint(*rig, c))
        {
            const float d = glm::distance(place(c, rig->bones[c].pivot), at);
            if(d < jd)
            {
                jd = d;
                joint = c;
            }
        }
    }
    return joint;
}

int boneAt(edict_t* ent, const glm::vec3& at, const char** name, glm::vec3* end)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return -1;
    }
    int hit = -1;
    float best = 1e30f;
    for(int b = 0; b < rig->numBones; b++)
    {
        if(cut & (1u << b))
        {
            continue;
        }
        const glm::quat& q = rot[static_cast<za::SizeT>(b)];
        const glm::vec3& o = pos[static_cast<za::SizeT>(b)];
        for(const glm::vec3& p : rig->bones[b].points)
        {
            const float d = glm::distance(q * (p * scale) + o, at);
            if(d < best)
            {
                best = d;
                hit = b;
            }
        }
    }
    if(name)
    {
        *name = hit >= 0 ? rig->bones[hit].name : "";
    }
    if(end && hit >= 0)
    {
        *end = rot[static_cast<za::SizeT>(hit)] * (rig->bones[hit].end * scale) + pos[static_cast<za::SizeT>(hit)];
    }
    return hit;
}

int boneNamed(edict_t* ent, const char* name)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!name || !posedBones(ent, rig, rot, pos, scale, cut))
    {
        return -1;
    }
    for(int b = 0; b < rig->numBones; b++)
    {
        if(!q_strcasecmp(rig->bones[b].name, name))
        {
            return b;
        }
    }
    return -1;
}

uint32_t limbInfo(edict_t* ent, int what, int bone)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return what == 3 ? ~0u : 0u;
    }
    const uint32_t head = ragdoll::headBones(*rig);
    switch(what)
    {
    case 0: // the limb joints not cut (not the head's)
    {
        uint32_t m = 0;
        for(int b = 0; b < rig->numBones; b++)
        {
            m |= ragdoll::limbJoint(*rig, b) && !(head & (1u << b)) && !(cut & (1u << b)) ? 1u << b : 0u;
        }
        return m;
    }
    case 1: return head;
    case 2: return cut;
    case 3: return static_cast<uint32_t>(rig->head);
    case 4: return bone >= 0 && bone < rig->numBones ? (bone == rig->head ? head : ragdoll::limbBones(*rig, bone)) & ~cut : 0u;
    case 5: return bone >= 0 && bone < rig->numBones ? static_cast<uint32_t>(rig->bones[bone].parent) : ~0u;
    case 7: // the part holding loose piece `bone` (its weapon, welded there while down: looseHolder), else `bone`
        return bone < 0 || bone >= rig->numBones ? ~0u
               : static_cast<uint32_t>(rig->bones[bone].joint == ragdoll::Joint::Loose ? looseHolder(*rig, bone) : bone);
    case 6: // limb `bone` % 4's joint by its bones' names (0 the left shoulder, 1 the right, 2 the left hip, 3 the right;
            // 4-7 their elbow or knee), 0 none (the tests' cuts: vr_test_cut)
        return bone < 0 || bone > 7 ? 0u : static_cast<uint32_t>(za::max(ragactive::limbJointOf(*rig, bone & 3, bone >= 4), 0));
    default: return 0u;
    }
}

const char* limbModel(edict_t* ent, int bone)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    const int index = static_cast<int>(ent->v.modelindex);
    if(!posedBones(ent, rig, rot, pos, scale, cut) || index <= 0 || index >= MAX_MODELS || !sv.model_precache[index] ||
        !ragdoll::limbJoint(*rig, bone))
    {
        return "";
    }
    const char* base = sv.model_precache[index];
    const uint32_t all = ragdoll::limbBones(*rig, bone);
    // (After the cut: the piece cut off is what it took then, `lastCutBones`.)
    const RagdollBodies* r = world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    uint32_t bones = all & ~cut;
    if(r && r->lastCut == bone && r->lastCutBones)
    {
        bones = r->lastCutBones;
    }
    if(!bones || !limbmodel::available(base, bone, bones == all ? 0u : bones))
    {
        return "";
    }
    return limbmodel::keptName(base, bone, bones == all ? 0u : bones);
}

// Limb gore: what part `b` of `rig` weighs cut off, as a share of its ragdoll's mass (limbPiece), by the kind of limb its
// name says (the author: a limb far lighter than its body, a hand lighter than a thigh): a person's (an arm 6.3%: the
// upper arm 3.5, the forearm 2, the hand 0.8; a leg 15.5%: the thigh 9.5, the shin 4.5, the foot 1.5; the head 7, a jaw
// 1 of it), two arms' and two legs' shares split among as many as the rig has (a rottweiler's four legs, the centroid's
// six); a part the rig hasn't (a forearm without a hand of its own, an arm without its forearm) weighs on the one that
// has it. Tails and anything else: their hulls' share of the body's volume (as the ragdoll's own parts weigh); a loose
// piece (a gun) nothing.
namespace limbmass
{
enum class Kind : uint8_t { Head, Jaw, UpperArm, Forearm, Hand, Arm, Thigh, Shin, Foot, Leg, Other };
constexpr float headShare = 0.07f, jawShare = 0.01f;
constexpr float upperArmShare = 0.035f, forearmShare = 0.02f, handShare = 0.008f;
constexpr float thighShare = 0.095f, shinShare = 0.045f, footShare = 0.015f;

[[nodiscard]] Kind kindOf(const char* name)
{
    const auto is = [name](const char* prefix) { return !ZA_STRNCMP(name, prefix, ZA_STRLEN(prefix)); };
    if(is("head")) { return Kind::Head; }
    if(is("jaw")) { return Kind::Jaw; }
    if(is("upperarm")) { return Kind::UpperArm; }
    if(is("forearm")) { return Kind::Forearm; }
    if(is("hand") || is("claw")) { return Kind::Hand; }
    if(is("arm")) { return Kind::Arm; } // (upper arm and forearm in one: the knight's, the scrag's)
    if(is("thigh") || is("upperleg")) { return Kind::Thigh; }
    if(is("shin") || is("lowerleg")) { return Kind::Shin; }
    if(is("foot")) { return Kind::Foot; }
    if(is("leg")) { return Kind::Leg; } // (a whole leg in one: the centroid's)
    return Kind::Other; // (tails, the torso)
}
} // namespace limbmass

[[nodiscard]] float limbMassShare(const ragdoll::Rig& rig, int b)
{
    using limbmass::Kind;
    const ragdoll::Bone& bone = rig.bones[b];
    if(bone.joint == ragdoll::Joint::Loose)
    {
        return 0.f;
    }
    const auto hasChild = [&](Kind k) {
        for(int c = b + 1; c < rig.numBones; c++)
        {
            if(rig.bones[c].parent == b && limbmass::kindOf(rig.bones[c].name) == k)
            {
                return true;
            }
        }
        return false;
    };
    int arms = 0, legs = 0;
    for(int c = 0; c < rig.numBones; c++)
    {
        const Kind k = limbmass::kindOf(rig.bones[c].name);
        arms += k == Kind::UpperArm || k == Kind::Arm ? 1 : 0;
        legs += k == Kind::Thigh || k == Kind::Leg ? 1 : 0;
    }
    const float arm = 2.f / static_cast<float>(za::max(arms, 1)), leg = 2.f / static_cast<float>(za::max(legs, 1));
    using namespace limbmass;
    const float hand = hasChild(Kind::Hand) ? 0.f : handShare;
    switch(kindOf(bone.name))
    {
    case Kind::Head: return headShare - (hasChild(Kind::Jaw) ? jawShare : 0.f);
    case Kind::Jaw: return jawShare;
    case Kind::UpperArm: return arm * (upperArmShare + (hasChild(Kind::Forearm) ? 0.f : forearmShare + handShare));
    case Kind::Forearm: return arm * (forearmShare + hand);
    case Kind::Arm: return arm * (upperArmShare + forearmShare + hand);
    case Kind::Hand: return arm * handShare;
    case Kind::Thigh: return leg * (thighShare + (hasChild(Kind::Shin) ? 0.f : shinShare + footShare));
    case Kind::Shin: return leg * (shinShare + (hasChild(Kind::Foot) ? 0.f : footShare));
    case Kind::Foot: return leg * footShare;
    case Kind::Leg: return leg * (thighShare + shinShare + footShare);
    case Kind::Other: break;
    }
    // (Its hull's share of the body's volume, the loose pieces not: as the ragdoll's parts weigh, createRagdoll.)
    float own = 0.f, whole = 0.f;
    for(int c = 0; c < rig.numBones; c++)
    {
        if(rig.bones[c].joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        float v = 0.f;
        if(b3HullData* hull = boneHull(rig.bones[c], 1.f, v))
        {
            b3DestroyHull(hull);
        }
        whole += v;
        own += c == b ? v : 0.f;
    }
    return whole > 0.f ? own / whole : 0.f;
}

glm::vec3 limbPiece(edict_t* ent, int bone, int what)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut))
    {
        return glm::vec3{0.f};
    }
    bone = bone < 0 ? rig->head : bone;
    if(bone < 0 || bone >= rig->numBones || (bone != rig->head && !ragdoll::limbJoint(*rig, bone)))
    {
        return glm::vec3{0.f};
    }
    // The piece: what a cut there takes now (a body gibbed whole: the limb as it is); just cut, what it took then (as
    // limbModel's).
    uint32_t bones = (bone == rig->head ? ragdoll::headBones(*rig) : ragdoll::limbBones(*rig, bone)) & ~cut;
    const RagdollBodies* r = world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(r && r->lastCut == bone && r->lastCutBones && (cut & (1u << bone)))
    {
        bones = r->lastCutBones;
    }
    if(what == 1)
    {
        // (Its model's middle: a limb's model's origin, vr_limbmodel.cpp; the head's, its own bone's, ragdollCut 0.)
        return rig->bones[bone].pivot - ragdoll::limbMiddle(*rig, bone == rig->head ? (1u << bone) : bones);
    }
    float share = 0.f;
    for(int b = 0; b < rig->numBones; b++)
    {
        share += (bones & (1u << b)) ? limbMassShare(*rig, b) : 0.f;
    }
    const float mass = za::max(tune(ent, Tune::Mass), 1.f);
    return glm::vec3{mass * share, mass, share};
}

namespace
{
[[nodiscard]] const RagdollClass* headClass(const char* model)
{
    for(const RagdollClass& c : ragdollClasses)
    {
        if(model && c.head && !ZA_STRCMP(model, c.head))
        {
            return &c;
        }
    }
    // AD support: AD's heads, as their monster's nearest class (h_dknight.mdl the death knight's, monster_hell_knight)
    if(const char* classname = ad::headVrClass(model))
    {
        for(const RagdollClass& c : ragdollClasses)
        {
            if(!ZA_STRCMP(classname, c.classname))
            {
                return &c;
            }
        }
    }
    return nullptr;
}
} // namespace

bool monsterHead(const char* model)
{
    return headClass(model) != nullptr;
}

float headPropMass(const char* model)
{
    // As a head cut off its ragdoll (limbPiece, then the QC's VR_Limb_SetMass): the class's mass times the head's
    // share (every rig's head and jaw: headShare) times vr_limbs_mass_scale (at least 0.05: its 0, the pieces weighed by
    // their volume as before, retired in round 23).
    const RagdollClass* c = headClass(model);
    const float scale = za::max(vr_limbs_mass_scale.value, 0.05f);
    if(!c)
    {
        return 0.f;
    }
    const auto i = static_cast<za::SizeT>(Tune::Mass);
    const float mass = za::max(c->own[i]->value >= 0.f ? c->own[i]->value : *ragdollGlobals[i], 1.f);
    return za::max(mass * limbmass::headShare * scale, 0.05f);
}

glm::vec3 limbPlace(edict_t* ent, int bone, int what)
{
    const ragdoll::Rig* rig = nullptr;
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    float scale = 1.f;
    uint32_t cut = 0;
    if(!posedBones(ent, rig, rot, pos, scale, cut) || bone < 0 || bone >= rig->numBones)
    {
        return glm::vec3{0.f};
    }
    const glm::quat q = rot[static_cast<za::SizeT>(bone)];
    if(what == 1)
    {
        glm::vec3 a;
        held::anglesFromAxes(glm::mat3_cast(q), &a[0], false);
        return a;
    }
    if(what == 2)
    {
        const RagdollBodies* r = world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
        return r && bone < r->count ? world->toU(b3Body_GetLinearVelocity(r->body[static_cast<za::SizeT>(bone)])) : vec(ent->v.velocity);
    }
    const uint32_t bones = (bone == rig->head ? (1u << bone) : ragdoll::limbBones(*rig, bone)) & ~cut;
    if(what == 3)
    {
        // (A point on its own bone's surface: its vertex nearest that bone's middle; the tests' hit.)
        const glm::vec3 mid = ragdoll::limbMiddle(*rig, 1u << bone);
        glm::vec3 best = mid;
        float bestD = 1e30f;
        for(const glm::vec3& p : rig->bones[bone].points)
        {
            const float d = glm::distance(p, mid);
            if(d < bestD)
            {
                bestD = d;
                best = p;
            }
        }
        return q * (best * scale) + pos[static_cast<za::SizeT>(bone)];
    }
    return q * (ragdoll::limbMiddle(*rig, bones) * scale) + pos[static_cast<za::SizeT>(bone)];
}

int ragdollHeadAt(int num, const glm::vec3& at, float neck)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        return -1;
    }
    const ragdoll::Rig& rig = *r->rig;
    const uint32_t bones = ragdoll::headBones(rig);
    if(!bones || rig.head >= r->count || (r->cut & bones))
    {
        return 0;
    }
    float distance = 0.f;
    const int part = nearestPart(*r, at, distance);
    if(part >= 0 && (bones & (1u << part)))
    {
        return 1;
    }
    const int parent = rig.bones[rig.head].parent;
    if(parent < 0)
    {
        return 0;
    }
    const b3WorldTransform px = b3Body_GetTransform(r->body[static_cast<za::SizeT>(parent)]);
    const glm::vec3 neckAt = fromB3(px.q) * (rig.bones[rig.head].pivot * r->scale) + world->toU(px.p);
    return glm::distance(neckAt, at) <= za::max(neckReach * r->scale, neck) ? 1 : 0;
}

bool ragdollNeck(int num, glm::vec3& joint, glm::vec3& axis, glm::vec3& head)
{
    const RagdollBodies* r = ragdollOf(num);
    if(!r)
    {
        return false;
    }
    const ragdoll::Rig& rig = *r->rig;
    const int bone = rig.head;
    if(bone < 0 || bone >= r->count || partCut(*r, bone))
    {
        return false;
    }
    const b3WorldTransform hx = b3Body_GetTransform(r->body[static_cast<za::SizeT>(bone)]);
    const glm::quat hq = fromB3(hx.q);
    head = hq * (ragdoll::limbMiddle(rig, 1u << bone) * r->scale) + world->toU(hx.p);
    const int parent = rig.bones[bone].parent;
    joint = head;
    if(parent >= 0)
    {
        const b3WorldTransform px = b3Body_GetTransform(r->body[static_cast<za::SizeT>(parent)]);
        joint = fromB3(px.q) * (rig.bones[bone].pivot * r->scale) + world->toU(px.p);
    }
    const glm::vec3 d = head - joint;
    axis = glm::length(d) > 0.5f ? glm::normalize(d) : hq * glm::vec3{0.f, 0.f, 1.f};
    return true;
}
} // namespace qvr::box3d
