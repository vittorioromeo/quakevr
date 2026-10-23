#include "vr_alloccount.h"
// vr_box3d_ragparts.cpp -- a ragdoll's parts: blasts, cuts, the players' grabs and holds, points, parts in a box, limbs
// and heads.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_goresound.hpp"
#include "vr_handfit.hpp"

#include <cstring>

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
        const float share = za::max(vr_decap_head_speed.value, 0.f);
        r.headVel += blade * share + glm::vec3{0.f, 0.f, za::max(vr_decap_head_lift.value, 0.f) * (isHead ? 1.f : 0.5f)};
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
    const FieldOffsets& f = fields();
    at = fieldVec(player, hand ? f.handpos : f.offhandpos);
    const glm::vec3 angles = fieldVec(player, hand ? f.handrot : f.offhandrot);
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
    const za::Vector<glm::vec4>& fist = held::fist(hand);
    if(vr_ragdoll_grab_fit.value <= 0.f || hand < 0 || hand > 1 || pnum != 1 || cls.state == ca_dedicated || fist.empty())
    {
        return held; // (the hand's frame and fist are the local player's: measured by the client)
    }
    const grip::HandFrame f = grip::handFrame(hand, hand == 0, true);
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
void startHold(World::RagdollGrab& g, edict_t* player, const RagdollBodies& r, b3Vec3 grip, bool fit = true)
{
    glm::vec3 at;
    glm::quat turn;
    handPose(player, g.hand, at, turn);
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
    jd.base.localFrameA = b3Transform{b3Vec3_zero, b3Quat_identity};
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
    startHold(g, player, *gs.r, b3Body_GetLocalPoint(gs.r->body[static_cast<za::SizeT>(part)], world->toM(gs.at)), !g.tug);
    world->ragdollGrabs.pushBack(g);
}

// ragdollgrab: hand `hand` of `player` takes the limb of `corpse`'s ragdoll it is on (within vr_ragdoll_grab_reach of
// its surface).
bool grabRagdoll(edict_t* corpse, edict_t* player, int hand)
{
    GrabStart gs;
    if(vr_ragdoll_grab.value < 1.f || !grabStart(corpse, player, hand, gs))
    {
        return false;
    }
    RagdollBodies* r = gs.r;
    const int num = gs.num;
    float distance = 0.f;
    const int part = nearestPart(*r, gs.at, distance);
    if(part < 0 || distance > za::max(vr_ragdoll_grab_reach.value, 0.f))
    {
        return false;
    }
    World::RagdollGrab g;
    beginGrab(g, gs, player, hand, part);
    // Manhandled (vr_foegrab_manhandle_stagger): a living one staggering, taken by a hand, loses its footing and goes
    // down (its stagger's fall: the QC's VR_Knockdown_Think sees it fallen, down its knockdown's time).
    if(vr_foegrab_manhandle_stagger.value > 0.f && corpse->v.health > 0.f && r->active.on && r->active.stagger.on)
    {
        ragactive::lapse(r->active, activeParts(*r), qcvm->time);
        if(vr_knockdown_debug.value || vr_debug_ragdoll.value)
        {
            Con_Printf("manhandle: %d %s taken by hand %d staggering: it goes down\n", num, PR_GetString(corpse->v.classname),
                hand);
        }
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d part %d (%s) taken by hand %d, %.1f units from its surface\n", num, part, r->rig->bones[part].name,
            hand, distance);
    }
    return true;
}

// ragdollHoldSoft: hand `hand` of `player` holds on to the limb of `ent`'s ragdoll nearest it (within `reach` units of
// its surface), by a spring of at most `force` N at `hertz`, let go of past `stuck` units (vr_foegrab.cpp: an enemy
// thrown or lifted, held on). `head`: its head's part instead, if not cut off and its middle within `reach` and 12 units
// more (a neck snapped: the hands held its head).
bool holdSoft(edict_t* ent, edict_t* player, int hand, float reach, float force, float hertz, float stuck, bool head, bool torso)
{
    GrabStart gs;
    if(!grabStart(ent, player, hand, gs))
    {
        return false;
    }
    const RagdollBodies* r = gs.r;
    const int num = gs.num;
    const glm::vec3& at = gs.at;
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
    else if(torso && part > 0 && part != headPart && std::strcmp(r->rig->bones[part].name, "chest") != 0)
    {
        // (Lifted: its torso rather than an arm across it, if near enough: an arm held hangs slack.)
        int best = -1;
        float bestD = distance + 10.f;
        for(int b = 0; b < r->count; b++)
        {
            if((b == 0 || !std::strcmp(r->rig->bones[b].name, "chest")) && !partCut(*r, b))
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
    if(what == 2)
    {
        if(ours)
        {
            endGrab(index);
        }
        if(r && r->active.on && r->active.stagger.on)
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
    st.tug = true;
    st.tugStrength = za::clamp(strength, 0.f, 3.f);
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
    World::RagdollGrab ng;
    ng.force = za::max(force, 0.f);
    ng.stuck = 64.f;
    ng.tug = true;
    beginGrab(ng, gs, player, hand, part);
    if(vr_debug_ragdoll.value || vr_knockdown_debug.value)
    {
        Con_Printf("tug: %d %s: hand %d holds its part %d (%s, arm %d) %.1f units from the grip, %.0f N at most, its pull %.2f\n",
            num, PR_GetString(ent->v.classname), hand, part, r->rig->bones[part].name, st.tugArm, distance, ng.force,
            st.tugStrength);
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
// ---- The physgun's hold (vr_toolgun.cpp; vr_toolgun_physgun_ragdoll) ----
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
        const int heldOfs = g.hand ? fields().mainhand_held : fields().offhand_held;
        if(!g.tug && heldOfs >= 0 && fieldInt(player, heldOfs) != EDICT_TO_PROG(EDICT_NUM(g.num)))
        {
            endGrab(i);
            continue;
        }
        glm::vec3 at;
        glm::quat turn;
        handPose(player, g.hand, at, turn);
        const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
        if(B3_IS_NULL(g.anchor) || !b3Body_IsValid(g.anchor) || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
        {
            endGrab(i);
            continue;
        }
        const float force = g.force >= 0.f ? g.force : za::max(vr_ragdoll_grab_force.value, 0.f);
        b3MotorJoint_SetMaxSpringForce(g.joint, force);
        b3MotorJoint_SetMaxSpringTorque(g.joint, force * grabLever);
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
        if(glm::distance(held, at) > (g.stuck > 0.f ? g.stuck : grabStuck))
        {
            if(g.farSince < 0.0)
            {
                g.farSince = qcvm->time;
            }
            else if(qcvm->time - g.farSince > grabStuckTime)
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
        b3Body_SetAwake(part, true);
    }
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

bool ragdollGrab(edict_t* corpse, edict_t* player, int hand)
{
    const bool took = grabRagdoll(corpse, player, hand);
    if(took && corpse && corpse->v.health <= 0.f)
    {
        qvr::goresound::send(qvr::goresound::Kind::Squelch, NUM_FOR_EDICT(corpse), corpse->v.origin, 0.8f); // (a dead body)
    }
    return took;
}

float ragdollTug(edict_t* ent, edict_t* player, int hand, int what, const glm::vec3& grip, float strength, float force)
{
    return world && ent && player ? tugRagdoll(ent, player, hand, what, grip, strength, force) : (what == 1 ? -1.f : 0.f);
}

bool ragdollHoldSoft(edict_t* ent, edict_t* player, int hand, float reach, float force, float hertz, float stuck, bool head,
    bool torso)
{
    return ent && player && holdSoft(ent, player, hand, reach, force, hertz, stuck, head, torso);
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

float ragdollWeight(edict_t* ent)
{
    const RagdollBodies* rp = ent && world ? ragdollOf(NUM_FOR_EDICT(ent)) : nullptr;
    if(!rp)
    {
        return 0.f;
    }
    float mass = 0.f;
    for(int b = 0; b < rp->count; b++)
    {
        if(!partCut(*rp, b))
        {
            mass += b3Body_GetMass(rp->body[static_cast<za::SizeT>(b)]);
        }
    }
    return mass * za::max(-b3World_GetGravity(world->id).z, 0.f);
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
    if(!r || g.part >= r->count || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint))
    {
        return false;
    }
    // (The joint holds its frame B, on the part, at the anchor, the hand: the part's frame is ragdoll::publish's.)
    const b3Transform frameB = b3Joint_GetLocalFrameB(g.joint);
    out.num = g.num;
    out.part = g.part;
    out.at = glmv(frameB.p) * world->m2u;
    out.turn = fromB3(frameB.q);
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
            if(!partCut(*best, b) && !std::strncmp(best->rig->bones[b].name, name, std::strlen(name)))
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
        const bool own = vr_decap_own_motion.value != 0.f;
        for(int b = 0; b < r->count; b++)
        {
            const b3BodyId body = r->body[static_cast<za::SizeT>(b)];
            r->ownLin[static_cast<za::SizeT>(b)] = own && !partCut(*r, b) ? glmv(b3Body_GetLinearVelocity(body)) : glm::vec3{0.f};
            r->ownAng[static_cast<za::SizeT>(b)] = own && !partCut(*r, b) ? glmv(b3Body_GetAngularVelocity(body)) : glm::vec3{0.f};
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
    const auto is = [name](const char* prefix) { return !strncmp(name, prefix, strlen(prefix)); };
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
        if(model && c.head && !strcmp(model, c.head))
        {
            return &c;
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
    // share (every rig's head and jaw: headShare) times vr_limbs_mass_scale. Its scale 0 (cut pieces weighed by their
    // volume): 0, estimated as well.
    const RagdollClass* c = headClass(model);
    const float scale = vr_limbs_mass_scale.value;
    if(!c || scale <= 0.f)
    {
        return 0.f;
    }
    const auto i = static_cast<za::SizeT>(Tune::Mass);
    const float mass = za::max(c->own[i]->value >= 0.f ? c->own[i]->value : ragdollGlobals[i]->value, 1.f);
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
