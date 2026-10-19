#include "vr_alloccount.h"
// vr_box3d_contacts.cpp -- contact filters (self collision, shouldCollide, preSolve), portal copies, carried ragdolls
// and corpses, pushes and their limits.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// A shape of a ragdoll's part as GJK's point cloud (its body's frame, in `points`): a hull's corners, a capsule's two
// ends and radius; shrunk about its middle to `core` of its size (1: as it is). False: another kind of shape.
using ProxyPoints = za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS>;
[[nodiscard]] bool partProxy(b3ShapeId shape, float core, b3ShapeProxy& out, ProxyPoints& points)
{
    switch(b3Shape_GetType(shape))
    {
        case b3_hullShape:
        {
            const b3HullData* hull = b3Shape_GetHull(shape);
            const b3Vec3* corners = b3GetHullPoints(hull);
            out.count = za::min(hull->vertexCount, static_cast<int32_t>(B3_MAX_SHAPE_CAST_POINTS));
            for(int i = 0; i < out.count; i++)
            {
                points[static_cast<za::SizeT>(i)] = b3Lerp(hull->center, corners[i], core);
            }
            out.radius = 0.f;
            break;
        }
        case b3_capsuleShape:
        {
            const b3Capsule c = b3Shape_GetCapsule(shape);
            const b3Vec3 mid = b3Lerp(c.center1, c.center2, 0.5f);
            points[0] = b3Lerp(mid, c.center1, core);
            points[1] = b3Lerp(mid, c.center2, core);
            out.count = 2;
            out.radius = c.radius * core;
            break;
        }
        default: return false;
    }
    out.points = points.data();
    return out.count > 0;
}

constexpr float selfApart = 0.002f; // m: two of a ragdoll's parts nearer than this overlap (self-collision)

// The distance between two shapes (metres; 0 overlapping; 1e30 another kind of shape), each shrunk to `core` of its size.
[[nodiscard]] float shapeGap(b3ShapeId a, b3ShapeId b, float core = 1.f)
{
    b3DistanceInput in{};
    ProxyPoints pointsA, pointsB;
    if(!partProxy(a, core, in.proxyA, pointsA) || !partProxy(b, core, in.proxyB, pointsB))
    {
        return 1e30f;
    }
    in.transform = b3InvMulWorldTransforms(b3Body_GetTransform(b3Shape_GetBody(a)), b3Body_GetTransform(b3Shape_GetBody(b)));
    in.useRadii = true;
    b3SimplexCache cache{};
    return b3ShapeDistance(&in, &cache, nullptr, 0).distance;
}

// A corpse's arm meets its torso, including deep contacts and reaches towards a wound. Box3D's bounded contact
// push-out resolves initial overlaps. Living animation/pin safeguards remain in selfTouches below.
[[nodiscard]] bool selfMeets(b3ShapeId a, b3ShapeId b, int num)
{
    if(vr_ragdoll_self_collide.value == 0.f || num >= static_cast<int>(world->slots.size()) || world->slots[num].ragdoll < 0)
    {
        return false;
    }
    const RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
    const b3BodyId ba = b3Shape_GetBody(a), bb = b3Shape_GetBody(b);
    int pa = -1, pb = -1;
    for(int p = 0; p < r.count && (pa < 0 || pb < 0); p++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(p)];
        pa = pa < 0 && B3_ID_EQUALS(body, ba) ? p : pa;
        pb = pb < 0 && B3_ID_EQUALS(body, bb) ? p : pb;
    }
    return pa >= 0 && pb >= 0 && pa != pb && r.rig->bones[pa].joint != ragdoll::Joint::Loose &&
           r.rig->bones[pb].joint != ragdoll::Joint::Loose && (!r.knocked || shapeGap(a, b, 0.7f) > selfApart);
}

// Whether part `p` of ragdoll `r` is in an arm its drive reaches with now (a hand to a wound or a stump, a catch, a lean,
// a ledge's hold: ragactive::State::reach): its upper arm, its forearm or below.
[[nodiscard]] bool reachingArm(const RagdollBodies& r, int p)
{
    if(!r.active.on)
    {
        return false;
    }
    for(int arm = 0; arm < 2; arm++)
    {
        const auto& chain = r.active.chain[static_cast<za::SizeT>(arm)];
        // (Or about to: a hand of what is round him wanting a hold, its reach still easing in (a lip falling past is
        // reached for at once: met, the arm stuck on the head and missed it).)
        const bool wants = r.active.env.hand[static_cast<za::SizeT>(arm)].want > 0.f;
        if((r.active.reach[static_cast<za::SizeT>(arm)].weight <= 0.05f && !wants) || chain[0] < 0)
        {
            continue;
        }
        for(int b = p; b >= 0; b = r.rig->bones[b].parent) // (p or a bone it hangs from)
        {
            if(b == chain[0] || b == chain[1])
            {
                return true;
            }
        }
    }
    return false;
}

// Self-collision contacts: corpses retain contacts during reaches and deep overlaps. Keep the existing living
// ragdoll exceptions for animation/pin transitions and living reaches (ragmove2).
[[nodiscard]] bool selfTouches(b3ShapeId a, b3ShapeId b, int num, b3Pos point)
{
    const RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(world->slots[num].ragdoll)];
    const b3BodyId ba = b3Shape_GetBody(a), bb = b3Shape_GetBody(b);
    int pa = -1, pb = -1;
    for(int p = 0; p < r.count && (pa < 0 || pb < 0); p++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(p)];
        pa = pa < 0 && B3_ID_EQUALS(body, ba) ? p : pa;
        pb = pb < 0 && B3_ID_EQUALS(body, bb) ? p : pb;
    }
    if(pa < 0 || pb < 0)
    {
        return true;
    }
    // A shoulder's socket is intentionally shared by the arm and chest. Contacts farther along the arm
    // prevent its elbow and upper arm entering the chest without pulling the shoulder joint apart.
    const int child = r.rig->bones[pa].parent == pb ? pa : r.rig->bones[pb].parent == pa ? pb : -1;
    if(child >= 0)
    {
        const ragdoll::Bone& bone = r.rig->bones[child];
        const float k = r.scale / world->m2u;
        const b3Pos socket = b3Body_GetWorldPoint(r.body[static_cast<za::SizeT>(child)], b3v(bone.pivot * k));
        const float radius = glm::length(bone.end - bone.pivot) * k * 0.4f;
        if(glm::length(glmv(b3SubPos(point, socket))) < radius)
        {
            return false;
        }
    }
    // (Staggering, rising or climbing, its body follows its animation's frames (a climb: its pin's way), whose limbs pass through each other (a knight's
    // get-up: a hand on a knee, a forearm in the thigh): met, they held the pose off by 15 units (3 without) and a steady
    // stagger's feet tripped on each other now and then.)
    if(r.active.on && (r.active.stagger.on || r.active.rise.on || r.active.climb.on))
    {
        return false;
    }
    // Living reaches keep their prior safeguard. A corpse hand pushed into the torso must be pushed out again.
    return !r.knocked || (!reachingArm(r, pa) && !reachingArm(r, pb) && shapeGap(a, b, 0.7f) > selfApart);
}
} // namespace

// Self-collision's check (vr_ragdoll_list): ragdoll `r`'s pairs of parts that would meet (not jointed, not a loose piece)
// overlapping now, of `pairs` such pairs.
[[nodiscard]] int partsOverlapping(const RagdollBodies& r, int& pairs, int& deep)
{
    constexpr int most = 4;
    pairs = 0;
    deep = 0;
    int overlapping = 0;
    for(int i = 0; i < r.count; i++)
    {
        for(int j = i + 1; j < r.count; j++)
        {
            const ragdoll::Bone &bi = r.rig->bones[i], &bj = r.rig->bones[j];
            if(partCut(r, i) || partCut(r, j) || bi.joint == ragdoll::Joint::Loose || bj.joint == ragdoll::Joint::Loose ||
                bi.parent == j || bj.parent == i)
            {
                continue;
            }
            pairs++;
            za::Array<b3ShapeId, most> si{}, sj{};
            const int ni = b3Body_GetShapes(r.body[static_cast<za::SizeT>(i)], si.data(), most);
            const int nj = b3Body_GetShapes(r.body[static_cast<za::SizeT>(j)], sj.data(), most);
            bool touching = false, cores = false;
            for(int a = 0; a < ni; a++)
            {
                for(int b = 0; b < nj; b++)
                {
                    const b3ShapeId sa = si[static_cast<za::SizeT>(a)], sb = sj[static_cast<za::SizeT>(b)];
                    touching = touching || shapeGap(sa, sb) <= selfApart;
                    cores = cores || shapeGap(sa, sb, 0.7f) <= selfApart;
                }
            }
            overlapping += touching ? 1 : 0;
            deep += cores ? 1 : 0;
        }
    }
    return overlapping;
}

// Include directly jointed upper arms in diagnostics: generic overlap counts intentionally omit joint pairs.
// Distal points in a chest shrunk to 80% distinguish a buried arm/elbow from the shared shoulder socket.
[[nodiscard]] int armsInsideChest(const RagdollBodies& r, int& samples)
{
    samples = 0;
    int inside = 0, chest = -1;
    for(int b = 0; b < r.count; b++)
    {
        if(r.rig->bones[b].role == modelmeta::BoneRole::Chest && !partCut(r, b)) { chest = b; break; }
    }
    if(chest < 0) { return 0; }
    za::Array<b3ShapeId, 4> shapes{};
    const int n = b3Body_GetShapes(r.body[static_cast<za::SizeT>(chest)], shapes.data(), 4);
    const float k = r.scale / world->m2u;
    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = r.rig->bones[b];
        if(partCut(r, b) || (strncmp(bone.name, "upperarm", 8) != 0 && strncmp(bone.name, "forearm", 7) != 0)) { continue; }
        for(float t : {0.6f, 0.9f})
        {
            const b3Vec3 point = b3v(glm::mix(bone.pivot, bone.end, t) * k);
            bool buried = false;
            samples++;
            for(int i = 0; i < n; i++)
            {
                b3DistanceInput in{};
                ProxyPoints points;
                if(!partProxy(shapes[static_cast<za::SizeT>(i)], 0.8f, in.proxyA, points)) { continue; }
                in.proxyB.points = &point;
                in.proxyB.count = 1;
                in.proxyB.radius = 0.f;
                in.transform = b3InvMulWorldTransforms(b3Body_GetTransform(r.body[static_cast<za::SizeT>(chest)]),
                    b3Body_GetTransform(r.body[static_cast<za::SizeT>(b)]));
                in.useRadii = true;
                b3SimplexCache cache{};
                buried = buried || b3ShapeDistance(&in, &cache, nullptr, 0).distance <= selfApart;
            }
            inside += buried ? 1 : 0;
        }
    }
    return inside;
}
bool shouldCollide(b3ShapeId a, b3ShapeId b, void*)
{
    if(((b3Shape_GetFilter(a).categoryBits & catHeld) && smallGibSpared(numOf(b))) ||
        ((b3Shape_GetFilter(b).categoryBits & catHeld) && smallGibSpared(numOf(a))))
    {
        return false; // (a held thing and a small gib just torn out: smallGibSpared)
    }
    const bool aReach = (b3Shape_GetFilter(a).categoryBits & catReach) != 0;
    if(aReach || (b3Shape_GetFilter(b).categoryBits & catReach) != 0)
    {
        return reachMeets(aReach ? a : b, aReach ? b : a);
    }
    const int na = numOf(a), nb = numOf(b);
    if(na > 0 && na == nb) { return selfMeets(a, b, na); }
    if(na <= 0 || nb <= 0 || na >= qcvm->num_edicts || nb >= qcvm->num_edicts)
    {
        return true;
    }
    // Two ragdolls (a self-colliding one has catCorpse in its mask whatever vr_ragdoll_collide_each: it decides here).
    if(vr_ragdoll_collide_each.value == 0.f && na < static_cast<int>(world->slots.size()) &&
        nb < static_cast<int>(world->slots.size()) && world->slots[na].ragdoll >= 0 && world->slots[nb].ragdoll >= 0)
    {
        return false;
    }
    if(!world->inside.empty() && bornInside(na, nb))
    {
        return false; // (a small gib made inside this body: noteBornInside)
    }
    if(!world->ragdollsInside.empty() && ragdollsInside(na, nb))
    {
        return false; // (ragdolls made in each other: createRagdoll)
    }
    // A corpse and a loose prop: as the settings say, by whether it flies from a throw (vr_corpse_collide_thrown) or not
    // (vr_corpse_collide_props).
    const Kind ka = na < static_cast<int>(world->slots.size()) ? world->slots[na].kind : Kind::None;
    const Kind kb = nb < static_cast<int>(world->slots.size()) ? world->slots[nb].kind : Kind::None;
    if((ka == Kind::Corpse && kb == Kind::Prop) || (kb == Kind::Corpse && ka == Kind::Prop))
    {
        const Slot& prop = world->slots[ka == Kind::Prop ? na : nb];
        return (prop.flight ? vr_corpse_collide_thrown.value : vr_corpse_collide_props.value) != 0.f;
    }
    const edict_t* ea = EDICT_NUM(na);
    const edict_t* eb = EDICT_NUM(nb);
    return ea->v.owner != EDICT_TO_PROG(eb) && eb->v.owner != EDICT_TO_PROG(ea);
}

namespace
{
// Standing on props (vr_box3d_player_stand), as Source's players stand on its VPhysics objects: Quake's movement
// is the player's (the world first: its traces are authoritative), and a solid prop (SOLID_BBOX: an explosive box) is
// ground to it like a brush (VR_StandsOn: FL_ONGROUND, so the player has friction and can jump); the player's capsule
// doesn't touch the prop under its feet (Box3D would have that kinematic body shove it, dragging it along by friction
// as the player walks on it: the prop slid away under him); instead the player's weight presses where he stands
// (vr_box3d_player_mass), the prop carries him as it moves (ride), and a jump pushes it away
// (vr_box3d_player_jump_push).
// See ROUND21.md, "Standing on props".
//
// Whether player capsule `player` stands on (or steps off) prop `other` at this contact: a solid prop met by the
// capsule's lower half-sphere, or below it. No contact then (preSolve).
[[nodiscard]] bool capsuleStandsOn(b3ShapeId player, b3ShapeId other, b3Pos point, b3Vec3 normal)
{
    const int num = numOf(other), client = numOf(player);
    if(!vr_box3d_player_stand.value || num <= svs.maxclients || num >= static_cast<int>(world->slots.size()) ||
       world->slots[num].kind != Kind::Prop || client < 1 || client > svs.maxclients ||
       static_cast<int>(EDICT_NUM(num)->v.solid) != SOLID_BBOX)
    {
        return false;
    }
    const Slot& ps = world->slots[static_cast<size_t>(client)];
    const b3BodyId body = b3Shape_GetBody(player);
    const b3Vec3 centre = b3Body_GetPosition(body);
    const float feet = centre.z + ps.mins.z / world->m2u; // m
    glm::vec3 n = glmv(normal); // turned to point from the prop to the player
    if(glm::dot(n, glmv(centre) - glmv(b3Body_GetWorldCenter(b3Shape_GetBody(other)))) < 0.f)
    {
        n = -n;
    }
    return point.z < feet + ps.radius || n.z > 0.5f;
}

// A contact's normal turned to point from a hand's body to the prop (towards the prop's centre: Box3D's contacts and its
// continuous collision give it either way round).
[[nodiscard]] glm::vec3 towardsProp(b3BodyId prop, b3Pos point, b3Vec3 normal)
{
    const glm::vec3 n = glmv(normal);
    return glm::dot(n, glmv(b3Body_GetWorldCenter(prop)) - glmv(point)) < 0.f ? -n : n;
}

// Whether a hand's body may hold `prop` up where it meets it (normal `n`, from the body to the prop): not a prop heavier
// than vr_box3d_hand_hold_mass on top of it (the normal within 60 degrees of up), which slips through it.
[[nodiscard]] bool mayHoldUp(b3BodyId prop, const glm::vec3& n)
{
    const float most = vr_box3d_hand_hold_mass.value;
    return most <= 0.f || n.z < 0.5f || b3Body_GetMass(prop) <= most;
}
} // namespace

// Box3D's pre-solve, for the fists' contacts and the reach bodies' (their shapes ask for it), each step: none with what
// their player just threw (graced) or what the reach body skips (reachSkips), nor holding up what is heavier than
// vr_box3d_hand_hold_mass (the prop on top: the normal from the hand's body within 60 degrees of up); pushed from the
// side or below, it still is. (A fist that held up the big box toppled onto its player, its drawn fist dozens of
// kinematic spheres, wedged it into the wall: Box3D threw it off at 10 to 15 m/s. ROUND21.md, "Props regressions".)
bool preSolve(b3ShapeId a, b3ShapeId b, b3Pos point, b3Vec3 normal, void*)
{
    // The level's mesh asks too, point by point (a local change to Box3D's mesh contacts: a prop passing through a
    // teleporter must not meet the wall behind it): only the portal copies' clip planes concern it.
    const bool level = B3_ID_EQUALS(a, world->worldShape) || B3_ID_EQUALS(b, world->worldShape);
    if(level && world->portalCopies.empty()) { return true; }
    for(const World::PortalCopy& copy : world->portalCopies)
    {
        for(const b3ShapeId shape : {a, b})
        {
            const b3BodyId body = b3Shape_GetBody(shape);
            const glm::vec4* clip = B3_ID_EQUALS(body, copy.original) ? &copy.here : B3_ID_EQUALS(body, copy.copy) ? &copy.there : nullptr;
            if(clip && glm::dot(*clip, glm::vec4{world->toU(point), 1.f}) < -0.03125f) { return false; }
        }
    }
    if(level) { return true; }
    // (A ragdoll's own parts: self-collision, vr_ragdoll_self_collide.)
    if(const int na = numOf(a); na > svs.maxclients && na == numOf(b) && na < static_cast<int>(world->slots.size()) &&
                                world->slots[na].ragdoll >= 0)
    {
        return selfTouches(a, b, na, point);
    }

    const bool aPlayer = (b3Shape_GetFilter(a).categoryBits & catPlayer) != 0;
    if(aPlayer || (b3Shape_GetFilter(b).categoryBits & catPlayer) != 0)
    {
        const b3ShapeId capsule = aPlayer ? a : b, other = aPlayer ? b : a;
        if(vr_box3d_throw_grace_body.value && graced(numOf(capsule), numOf(other)))
        {
            return false; // (what its player just threw: noteThrows)
        }
        return !capsuleStandsOn(capsule, other, point, normal);
    }
    const bool aFist = (b3Shape_GetFilter(a).categoryBits & catHand) != 0;
    if(aFist || (b3Shape_GetFilter(b).categoryBits & catHand) != 0)
    {
        const b3ShapeId fist = aFist ? a : b, prop = aFist ? b : a;
        if(graced(numOf(fist), numOf(prop))) // (noteThrows)
        {
            return false;
        }
        const b3BodyId propBody = b3Shape_GetBody(prop);
        return mayHoldUp(propBody, towardsProp(propBody, point, normal));
    }
    const bool aReach = (b3Shape_GetFilter(a).categoryBits & catReach) != 0;
    const b3ShapeId reach = aReach ? a : b, other = aReach ? b : a;
    if((b3Shape_GetFilter(reach).categoryBits & catReach) == 0)
    {
        return !passesGun(numOf(a), numOf(b)) && !passesGun(numOf(b), numOf(a));
    }
    const World::HandBody* hb = reachOf(b3Shape_GetBody(reach), numOf(reach));
    if(hb && reachSkips(*hb, numOf(reach), numOf(other)))
    {
        return false;
    }
    const b3BodyId reachBody = b3Shape_GetBody(reach), propBody = b3Shape_GetBody(other);
    const glm::vec3 n = towardsProp(propBody, point, normal);
    // Moving apart faster than 0.5 m/s (a prop just struck, flying off ahead of the body: sweepReach): no contact, which
    // Box3D's continuous collision would otherwise make of the body it leaves (stopping the prop there).
    const glm::vec3 relative =
        glmv(b3Body_GetWorldPointVelocity(propBody, point)) - glmv(b3Body_GetWorldPointVelocity(reachBody, point));
    const float apart = glm::dot(relative, n);
    if(vr_debug_box3d.value >= 2.f)
    {
        Con_Printf("box3d: reach contact with %d: normal %.2f %.2f %.2f, apart at %.2f m/s\n", numOf(other), n.x, n.y, n.z,
            apart);
    }
    if(apart > 0.5f)
    {
        return false;
    }
    return mayHoldUp(propBody, n);
}

// The crossing half has a body in the other room. Both shapes are clipped
// by their contact points, so the wall behind the aperture cannot catch it.
// Dynamic copies return their contact impulses, rotated, to their owner.
// A ragdoll's parts are copied together, through the gate its parts' box goes into (a corpse falling or thrown into a
// gate: its limbs past the plane meet the far room, not the wall behind the gate), until its pelvis crosses and it is
// carried whole (carryRagdolls).
void syncPortalCopies(float dt)
{
    for(const World::PortalCopy& c : world->portalCopies)
    {
        if(b3Body_IsValid(c.copy)) { b3DestroyBody(c.copy); }
    }
    world->portalCopies.clear();
    // Its box over this step too (where its motion takes it: a thrown prop goes from short of the gate to the wall
    // behind it in one step, and continuous collision met that wall before any copy was made: it bounced off), with
    // a few units' slack for a tumbling box's corners round the aperture.
    const auto sweptBox = [dt](b3BodyId body, glm::vec3& lo, glm::vec3& hi) {
        const b3AABB box = b3Body_ComputeAABB(body);
        const glm::vec3 step = glmv(b3Body_GetLinearVelocity(body)) * (1.5f * dt);
        lo = glm::min(lo, glm::min(glmv(box.lowerBound), glmv(box.lowerBound) + step));
        hi = glm::max(hi, glm::max(glmv(box.upperBound), glmv(box.upperBound) + step));
    };
    const auto addThrough = [&](b3BodyId body, const portals::LightGate& gate) {
        World::PortalCopy c;
        c.original = body;
        c.turn = gate.turn;
        c.shift = gate.to - gate.turn * gate.from;
        c.here = glm::vec4{gate.normal, -gate.dist};
        const glm::vec3 normal = -gate.turn * gate.normal;
        c.there = glm::vec4{normal, -glm::dot(normal, gate.to)};
        c.dynamic = b3Body_GetType(body) == b3_dynamicBody;
        b3BodyDef def = b3DefaultBodyDef();
        def.type = c.dynamic ? b3_dynamicBody : b3_kinematicBody;
        def.gravityScale = 0.f;
        const b3WorldTransform pose = b3Body_GetTransform(body);
        def.position = world->toM(gate.turn * world->toU(pose.p) + c.shift);
        def.rotation = toB3(glm::normalize(glm::quat_cast(gate.turn) * fromB3(pose.q)));
        def.userData = b3Body_GetUserData(body);
        c.copy = b3CreateBody(world->id, &def);
        za::Array<b3ShapeId, 16> shapes;
        const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < count; i++)
        {
            const b3ShapeId original = shapes[i];
            b3ShapeDef shape = b3DefaultShapeDef();
            shape.userData = b3Shape_GetUserData(original);
            shape.filter = b3Shape_GetFilter(original);
            shape.density = b3Shape_GetDensity(original);
            shape.baseMaterial.friction = b3Shape_GetFriction(original);
            shape.baseMaterial.restitution = b3Shape_GetRestitution(original);
            shape.enablePreSolveEvents = true;
            shape.enableCustomFiltering = true;
            b3Shape_EnablePreSolveEvents(original, true);
            switch(b3Shape_GetType(original))
            {
            case b3_hullShape: b3CreateHullShape(c.copy, &shape, b3Shape_GetHull(original)); break;
            case b3_sphereShape: { const b3Sphere v = b3Shape_GetSphere(original); b3CreateSphereShape(c.copy, &shape, &v); break; }
            case b3_capsuleShape: { const b3Capsule v = b3Shape_GetCapsule(original); b3CreateCapsuleShape(c.copy, &shape, &v); break; }
            default: break;
            }
        }
        world->portalCopies.pushBack(c);
    };
    const auto add = [&](b3BodyId body) {
        if(B3_IS_NULL(body) || !b3Body_IsValid(body)) { return; }
        glm::vec3 lo{1e30f}, hi{-1e30f};
        sweptBox(body, lo, hi);
        portals::LightGate gate;
        const glm::vec3 velocity = world->toU(b3Body_GetLinearVelocity(body));
        if(!portals::splitBounds(world->toU(b3v(lo)), world->toU(b3v(hi)), gate, 4.f, &velocity)) { return; }
        addThrough(body, gate);
    };
    for(const Slot& slot : world->slots)
    {
        // (A dead monster's body (vr_corpse_collide) too; a ragdoll's parts below.)
        if(slot.kind == Kind::Held || slot.kind == Kind::Prop || (slot.kind == Kind::Corpse && slot.ragdoll < 0))
        {
            add(slot.body);
        }
    }
    // A ragdoll: the gate one of its parts goes into (its box across the plane, over the aperture), and a copy through it
    // of each of its parts over that aperture, in front of the plane or past it (one against the frame beside it has
    // none: the frame stops it, as it would a prop).
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num == 0 || r.count == 0 || B3_IS_NULL(r.body[0]) || !b3Body_IsValid(r.body[0])) { continue; }
        // A part's box over the step, in units, a few units in (lying on the floor its box reaches a little into it,
        // under a gate's bottom edge).
        const auto partBox = [&](int b, glm::vec3& ulo, glm::vec3& uhi) {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(partCut(r, b) || B3_IS_NULL(body) || !b3Body_IsValid(body)) { return false; }
            glm::vec3 lo{1e30f}, hi{-1e30f};
            sweptBox(body, lo, hi);
            ulo = world->toU(b3v(lo));
            uhi = world->toU(b3v(hi));
            const glm::vec3 inset = glm::min(glm::vec3{4.f}, (uhi - ulo) * 0.25f);
            ulo += inset;
            uhi -= inset;
            return true;
        };
        portals::LightGate gate;
        const glm::vec3 velocity = world->toU(b3Body_GetLinearVelocity(r.body[0]));
        bool found = false;
        for(int b = 0; b < r.count && !found; b++)
        {
            glm::vec3 ulo, uhi;
            found = partBox(b, ulo, uhi) && portals::splitBounds(ulo, uhi, gate, 4.f, &velocity);
        }
        if(!found) { continue; }
        for(int b = 0; b < r.count; b++)
        {
            glm::vec3 ulo, uhi;
            if(!partBox(b, ulo, uhi)) { continue; }
            bool over = true;
            for(int c = 0; c < 8 && over; c++)
            {
                const glm::vec3 p{(c & 1) ? uhi.x : ulo.x, (c & 2) ? uhi.y : ulo.y, (c & 4) ? uhi.z : ulo.z};
                const glm::vec3 onPlane = p - gate.normal * (glm::dot(gate.normal, p) - gate.dist);
                over = !glm::any(glm::lessThan(onPlane, gate.mins - 4.f)) && !glm::any(glm::greaterThan(onPlane, gate.maxs + 4.f));
            }
            if(over) { addThrough(r.body[static_cast<za::SizeT>(b)], gate); }
        }
    }
    for(const auto& pair : world->hands)
    {
        for(const World::HandBody& hand : pair) { add(hand.body); add(hand.reach); }
    }
}

// After the step: a ragdoll whose pelvis went in through a teleporter's aperture is carried whole, every part by the gate's
// mapping (where, how turned, how it moves), as VR_PortalToss carries what flies: its joints stay as they were, and its
// parts still behind the plane meet the room they are in through their portal copies (syncPortalCopies).
void carryRagdolls()
{
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num == 0 || r.count == 0 || B3_IS_NULL(r.body[0]) || !b3Body_IsValid(r.body[0]))
        {
            r.pelvisKnown = false;
            continue;
        }
        glm::vec3 pelvis = world->toU(b3Body_GetTransform(r.body[0]).p);
        portals::LightGate gate;
        if(r.pelvisKnown && portals::crossedGate(r.lastPelvis, pelvis, gate))
        {
            const glm::vec3 shift = gate.to - gate.turn * gate.from;
            const glm::quat q = glm::quat_cast(gate.turn);
            for(int b = 0; b < r.count; b++)
            {
                const za::SizeT i = static_cast<za::SizeT>(b);
                const b3BodyId body = r.body[i];
                if(partCut(r, b) || B3_IS_NULL(body) || !b3Body_IsValid(body)) { continue; }
                const b3WorldTransform pose = b3Body_GetTransform(body);
                b3Body_SetTransform(body, world->toM(gate.turn * world->toU(pose.p) + shift), toB3(glm::normalize(q * fromB3(pose.q))));
                b3Body_SetLinearVelocity(body, b3v(gate.turn * glmv(b3Body_GetLinearVelocity(body))));
                b3Body_SetAngularVelocity(body, b3v(gate.turn * glmv(b3Body_GetAngularVelocity(body))));
                r.ownLin[i] = gate.turn * r.ownLin[i];
                r.ownAng[i] = gate.turn * r.ownAng[i];
            }
            r.turn = glm::normalize(q * r.turn);
            r.headMid = gate.turn * r.headMid + shift;
            r.headVel = gate.turn * r.headVel;
            r.headSpin = gate.turn * r.headSpin;
            r.headRot = glm::normalize(q * r.headRot);
            edict_t* ent = r.num > 0 && r.num < qcvm->num_edicts ? EDICT_NUM(r.num) : nullptr;
            if(ent && !ent->free)
            {
                ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + glm::degrees(std::atan2(gate.turn[0][1], gate.turn[0][0])));
            }
            const glm::vec3 to = gate.turn * pelvis + shift;
            Con_DPrintf("ragdoll: %d carried through a teleporter: pelvis %.1f %.1f %.1f -> %.1f %.1f %.1f\n", r.num, pelvis.x,
                pelvis.y, pelvis.z, to.x, to.y, to.z);
            pelvis = to;
        }
        r.lastPelvis = pelvis;
        r.pelvisKnown = true;
    }
}

// After the step: a dead monster's own body (vr_corpse_collide's pushable kinds, not a ragdoll) whose middle went in
// through a teleporter's aperture is carried whole by the gate's mapping, as carryRagdolls carries a ragdoll; until then
// its portal copy kept the wall behind the gate from stopping it (syncPortalCopies). writeCorpse then moves its entity.
void carryCorpses()
{
    for(za::SizeT num = 0; num < world->slots.size(); num++)
    {
        Slot& s = world->slots[num];
        if(s.kind != Kind::Corpse || s.ragdoll >= 0 || !s.corpseDynamic || B3_IS_NULL(s.body) || !b3Body_IsValid(s.body))
        {
            s.midKnown = false;
            continue;
        }
        glm::vec3 mid = world->toU(b3Body_GetWorldCenter(s.body));
        portals::LightGate gate;
        if(s.midKnown && portals::crossedGate(s.lastMid, mid, gate))
        {
            const glm::vec3 shift = gate.to - gate.turn * gate.from;
            const glm::quat q = glm::quat_cast(gate.turn);
            const b3WorldTransform pose = b3Body_GetTransform(s.body);
            // (A box, not fitted to its pose, stays square to the world: rotationOf; its yaw turns below.)
            const glm::quat turned = s.corpseFitted ? glm::normalize(q * fromB3(pose.q)) : fromB3(pose.q);
            b3Body_SetTransform(s.body, world->toM(gate.turn * world->toU(pose.p) + shift), toB3(turned));
            b3Body_SetLinearVelocity(s.body, b3v(gate.turn * glmv(b3Body_GetLinearVelocity(s.body))));
            b3Body_SetAngularVelocity(s.body, b3v(gate.turn * glmv(b3Body_GetAngularVelocity(s.body))));
            b3Body_SetAwake(s.body, true);
            edict_t* ent = static_cast<int>(num) < qcvm->num_edicts ? EDICT_NUM(static_cast<int>(num)) : nullptr;
            if(ent && !ent->free && !s.corpseFitted)
            {
                ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + glm::degrees(std::atan2(gate.turn[0][1], gate.turn[0][0])));
            }
            s.asleep = false; // (written this frame: writeCorpse)
            const glm::vec3 to = gate.turn * mid + shift;
            Con_DPrintf("corpse: %d carried through a teleporter: middle %.1f %.1f %.1f -> %.1f %.1f %.1f\n", static_cast<int>(num),
                mid.x, mid.y, mid.z, to.x, to.y, to.z);
            mid = to;
        }
        s.lastMid = mid;
        s.midKnown = true;
    }
}
void feedPortalCopies()
{
    for(World::PortalCopy& c : world->portalCopies)
    {
        if(!b3Body_IsValid(c.original)) { continue; }
        const b3WorldTransform pose = b3Body_GetTransform(c.original);
        b3Body_SetTransform(c.copy, world->toM(c.turn * world->toU(pose.p) + c.shift),
            toB3(glm::normalize(glm::quat_cast(c.turn) * fromB3(pose.q))));
        c.velocity = c.turn * glmv(b3Body_GetLinearVelocity(c.original));
        c.spin = c.turn * glmv(b3Body_GetAngularVelocity(c.original));
        b3Body_SetLinearVelocity(c.copy, b3v(c.velocity));
        b3Body_SetAngularVelocity(c.copy, b3v(c.spin));
    }
}
void finishPortalCopies()
{
    for(const World::PortalCopy& c : world->portalCopies)
    {
        if(!c.dynamic || !b3Body_IsValid(c.original)) { continue; }
        const glm::mat3 back = glm::transpose(c.turn);
        b3Body_SetLinearVelocity(c.original, b3v(glmv(b3Body_GetLinearVelocity(c.original)) +
            back * (glmv(b3Body_GetLinearVelocity(c.copy)) - c.velocity)));
        b3Body_SetAngularVelocity(c.original, b3v(glmv(b3Body_GetAngularVelocity(c.original)) +
            back * (glmv(b3Body_GetAngularVelocity(c.copy)) - c.spin)));
    }
}

// Pushes by mass (pushShare): the mass behind a hand's body `shape` (kg), 0 for none (a kinematic body's full push: not
// a hand's, or the setting 0). A carried prop: its own mass and each hand's holding it.
[[nodiscard]] float pusherMass(b3ShapeId shape)
{
    const uint64_t category = b3Shape_GetFilter(shape).categoryBits;
    const float hand = za::max(vr_box3d_hand_mass.value, 0.f);
    if(category & catReach)
    {
        const World::HandBody* hb = reachOf(b3Shape_GetBody(shape), numOf(shape));
        return hb ? reachMass(*hb) : 0.f;
    }
    if(category & catHand)
    {
        return hand;
    }
    if((category & catHeld) && hand > 0.f)
    {
        const int num = numOf(shape);
        int hands = 0;
        for(const auto& player : world->hands)
        {
            for(const World::HandBody& hb : player)
            {
                hands += hb.held == num ? 1 : 0;
            }
        }
        return hand * static_cast<float>(za::max(hands, 1)) + qvr::box3d::propMass(EDICT_NUM(num));
    }
    return 0.f;
}

namespace
{
// What the hands' bodies push by mass (notePushed, limitPushes): a loose prop, a pushable corpse.
[[nodiscard]] bool pushedByMass(const Slot& s)
{
    return s.kind == Kind::Prop || (s.kind == Kind::Corpse && s.corpseDynamic);
}
} // namespace

// Before a step: the props near the hands' bodies (the reach bodies, the fists, the carried props) and their motion.
void notePushed(float dt)
{
    world->pushed.clear();
    world->pushedParts.clear();
    if(vr_box3d_hand_mass.value <= 0.f && vr_box3d_weapon_arm_mass.value <= 0.f)
    {
        return;
    }
    const auto gather = [dt](b3BodyId body) {
        if(B3_IS_NULL(body) || !b3Body_IsValid(body))
        {
            return;
        }
        // Its box, grown by how far it goes this step (and 5 cm).
        b3AABB box = b3Body_ComputeAABB(body);
        const glm::vec3 lo = glmv(box.lowerBound), hi = glmv(box.upperBound);
        const float reach = 0.05f + (glm::length(glmv(b3Body_GetLinearVelocity(body))) +
                                        glm::length(glmv(b3Body_GetAngularVelocity(body))) * 0.5f * glm::length(hi - lo)) *
                                        dt;
        box.lowerBound = b3v(lo - glm::vec3{reach});
        box.upperBound = b3v(hi + glm::vec3{reach});
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.maskBits = catProp | catCorpse;
        b3World_OverlapAABB(world->id, box, filter,
            [](b3ShapeId shape, void*) {
                const int num = numOf(shape);
                if(num <= 0 || num >= static_cast<int>(world->slots.size()) || !pushedByMass(world->slots[num]))
                {
                    return true;
                }
                auto& out = world->pushed;
                if(!za::anyOf(out.begin(), out.end(), [num](const Pushed& p) { return p.num == num; }))
                {
                    const b3BodyId prop = world->slots[num].body;
                    Pushed p{num, glmv(b3Body_GetLinearVelocity(prop)), glmv(b3Body_GetAngularVelocity(prop))};
                    // (A ragdoll: every part's motion, struck by mass as one body: limitRagdollPush.)
                    if(const RagdollBodies* r = world->slots[num].ragdoll >= 0 ? ragdollOf(num) : nullptr)
                    {
                        p.parts = static_cast<int>(world->pushedParts.size());
                        for(int b = 0; b < r->count; b++)
                        {
                            const b3BodyId part = r->body[static_cast<za::SizeT>(b)];
                            const bool ok = !partCut(*r, b) && b3Body_IsValid(part);
                            world->pushedParts.pushBack(ok ? glmv(b3Body_GetLinearVelocity(part)) : glm::vec3{0.f});
                            world->pushedParts.pushBack(ok ? glmv(b3Body_GetAngularVelocity(part)) : glm::vec3{0.f});
                        }
                    }
                    out.pushBack(p);
                }
                return true;
            },
            nullptr);
    };
    for(const auto& player : world->hands)
    {
        for(const World::HandBody& hb : player)
        {
            gather(hb.reach);
            gather(hb.body);
        }
    }
    for(const Slot& s : world->slots)
    {
        if(s.kind == Kind::Held)
        {
            gather(s.body);
        }
    }
}

// After a step: what a hand's body pushed (a contact that pushed, the body moving into it at 0.3 m/s or more) keeps
// only pushShare of the velocity (and of the spin) the step gave it, if away from the body: the masses of all the
// hands' bodies pushing it together against its own; pushed on, as a shove (pushKeep). A hand held still holds things up
// and stops them at full strength.
namespace
{
// The hands' bodies pushing on `prop` after a step (limitPushes): their masses (each body once), whether one has none
// (a kinematic body's full push), the strongest push's normal impulse and normal (from the pusher to the prop).
struct HandPush
{
    float pusher{0.f}, strongest{0.f};
    float fastest{0.f}; // m/s: the fastest of them into it there
    bool full{false};
    glm::vec3 normal{0.f};
    za::Array<b3BodyId, 32> counted; // (two players' hands, reach and body are already 8; held props add more)
    size_t countedN{0};
};

// Adds `prop`'s contacts with the hands' bodies to `hp`; true if one of them is its strongest push now.
bool scanHandPush(b3BodyId prop, HandPush& hp)
{
    za::Vector<b3ContactData>& contacts = scratch.pushContacts;
    // Every contact, not the first few: a drawn fist (vr_box3d_hand_push_fist) is a body of a sphere per joint, and
    // pressed on a box its spheres touch it in dozens of places, the ones that push often not among the first 16 (it
    // then pushed at full strength, as a kinematic body: walked into, a tall box tipped; landing on you, it flew off).
    contacts.resize(static_cast<za::SizeT>(za::max(b3Body_GetContactCapacity(prop), 1)));
    const int count = b3Body_GetContactData(prop, contacts.data(), static_cast<int>(contacts.size()));
    bool strongest = false;
    for(int c = 0; c < count && !hp.full; c++)
    {
        const b3ContactData& cd = contacts[static_cast<size_t>(c)];
        const bool propA = B3_ID_EQUALS(b3Shape_GetBody(cd.shapeIdA), prop);
        const b3ShapeId other = propA ? cd.shapeIdB : cd.shapeIdA;
        if((b3Shape_GetFilter(other).categoryBits & (catReach | catHand | catHeld)) == 0)
        {
            continue;
        }
        const b3BodyId body = b3Shape_GetBody(other);
        for(int i = 0; i < cd.manifoldCount; i++)
        {
            const b3Manifold& mf = cd.manifolds[i];
            float impulse = 0.f;
            glm::vec3 at{0.f};
            for(int k = 0; k < mf.pointCount; k++)
            {
                const b3ManifoldPoint& mp = mf.points[k];
                impulse += mp.totalNormalImpulse;
                at += glmv(propA ? mp.anchorA : mp.anchorB);
            }
            if(impulse <= 0.f || mf.pointCount == 0)
            {
                continue;
            }
            // The normal from the body to the prop (Box3D's points from A to B), and the body's motion into it there.
            const glm::vec3 n = propA ? -glmv(mf.normal) : glmv(mf.normal);
            const glm::vec3 point = glmv(b3Body_GetWorldCenter(prop)) + at / static_cast<float>(mf.pointCount);
            const float into = glm::dot(glmv(b3Body_GetWorldPointVelocity(body, b3v(point))), n);
            if(into < 0.3f)
            {
                continue;
            }
            const float mass = pusherMass(other);
            if(mass <= 0.f)
            {
                hp.full = true;
                break;
            }
            hp.fastest = za::max(hp.fastest, into);
            if(!za::anyOf(hp.counted.begin(), hp.counted.begin() + static_cast<za::PtrDiffT>(hp.countedN),
                   [body](b3BodyId b) { return B3_ID_EQUALS(b, body); }))
            {
                hp.pusher += mass;
                if(hp.countedN < hp.counted.size())
                {
                    hp.counted[hp.countedN++] = body;
                }
            }
            if(impulse > hp.strongest)
            {
                hp.strongest = impulse;
                hp.normal = n;
                strongest = true;
            }
        }
    }
    return strongest;
}

// A ragdoll a hand's body pushed in the step (`p`: its parts' motion before it; ROUND22.md, "Melee on ragdolls"):
// Box3D's kinematic hand, weapon or carried thing gave the part it met its own speed there, and the joints carried the
// rest along, as if it weighed nothing (a sword's tip at 12 m/s flung a corpse off at 7 m/s). As two bodies meeting: the
// ragdoll's mass at that part (the momentum the step gave it all over the part's gain along the push: the part alone up
// to the whole body, as its joints took the rest along) against the pusher's (pusherMass), with vr_melee_ragdoll_bounce
// of restitution, and never more momentum than the pusher brought (pushed on in the next steps: the arm's force,
// vr_box3d_push_force), every part's gain scaled alike, the part's at most vr_melee_ragdoll_most m/s.
void limitRagdollPush(const Pushed& p, float dt)
{
    RagdollBodies* rp = ragdollOf(p.num);
    if(!rp || p.parts < 0 || p.parts + 2 * rp->count > static_cast<int>(world->pushedParts.size()))
    {
        return;
    }
    RagdollBodies& r = *rp;
    HandPush hp;
    int struck = -1;
    for(int b = 0; b < r.count && !hp.full; b++)
    {
        const b3BodyId part = r.body[static_cast<za::SizeT>(b)];
        if(!partCut(r, b) && b3Body_IsValid(part) && scanHandPush(part, hp))
        {
            struck = b;
        }
    }
    if(hp.full || hp.pusher <= 0.f || struck < 0)
    {
        return;
    }
    const auto before = [&](int b, int which) { return world->pushedParts[static_cast<za::SizeT>(p.parts + 2 * b + which)]; };
    glm::vec3 momentum{0.f};
    float whole = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        const b3BodyId part = r.body[static_cast<za::SizeT>(b)];
        if(partCut(r, b) || !b3Body_IsValid(part))
        {
            continue;
        }
        const float m = b3Body_GetMass(part);
        momentum += (glmv(b3Body_GetLinearVelocity(part)) - before(b, 0)) * m;
        whole += m;
    }
    const b3BodyId hit = r.body[static_cast<za::SizeT>(struck)];
    const float gained = glm::dot(glmv(b3Body_GetLinearVelocity(hit)) - before(struck, 0), hp.normal);
    const float given = glm::length(momentum); // N s
    if(given <= 1e-4f)
    {
        return;
    }
    Slot& slot = world->slots[static_cast<size_t>(p.num)];
    const bool shove = slot.pushedStep >= world->steps - 1;
    slot.pushedStep = world->steps;
    const float bounce = za::clamp(vr_melee_ragdoll_bounce.value, 0.f, 1.f);
    // The most the pusher can give it: a hit, its momentum into it and the bounce (whatever the ragdoll's mass there: a
    // weapon pressed on a body lying on the floor squeezed it out at speed, the part's own gain none); pushed on, the arm's
    // force over the step.
    const float force = vr_box3d_push_force.value;
    const float allowed = shove ? (force > 0.f ? force * dt : 1e30f) : (1.f + bounce) * hp.pusher * hp.fastest;
    float keep = za::min(1.f, allowed / given);
    float mass = whole;
    if(gained > 1e-3f)
    {
        // (The share of two masses meeting at that part: hit, a part alone keeps less than the pusher's whole momentum.)
        const float own = b3Body_GetMass(hit);
        mass = za::clamp(glm::dot(momentum, hp.normal) / gained, own, za::max(whole, own));
        if(!shove)
        {
            keep = za::min(keep, (1.f + bounce) * hp.pusher / (hp.pusher + mass));
        }
        const float most = vr_melee_ragdoll_most.value;
        if(most > 0.f && gained * keep > most)
        {
            keep = most / gained;
        }
    }
    if(keep >= 1.f)
    {
        return;
    }
    for(int b = 0; b < r.count; b++)
    {
        const b3BodyId part = r.body[static_cast<za::SizeT>(b)];
        if(partCut(r, b) || !b3Body_IsValid(part))
        {
            continue;
        }
        const glm::vec3 v0 = before(b, 0), w0 = before(b, 1);
        b3Body_SetLinearVelocity(part, b3v(v0 + (glmv(b3Body_GetLinearVelocity(part)) - v0) * keep));
        b3Body_SetAngularVelocity(part, b3v(w0 + (glmv(b3Body_GetAngularVelocity(part)) - w0) * keep));
    }
    if(vr_debug_box3d.value || vr_debug_ragdoll.value)
    {
        Con_Printf("box3d: ragdoll %d %s part %d %s by %.1f kg: %.0f u/s gained there, %.1f kg of its %.1f behind it, x%.2f: "
                   "its middle %.0f u/s faster\n",
            p.num, PR_GetString(EDICT_NUM(p.num)->v.classname), struck, shove ? "shoved" : "hit", hp.pusher,
            gained * world->m2u, mass, whole, keep, glm::length(momentum) / za::max(whole, 1e-3f) * keep * world->m2u);
    }
}
} // namespace

void limitPushes(float dt)
{
    for(const Pushed& p : world->pushed)
    {
        const Slot& s = world->slots[static_cast<size_t>(p.num)];
        if(s.ragdoll >= 0)
        {
            limitRagdollPush(p, dt);
            continue;
        }
        if(!pushedByMass(s) || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const float m = b3Body_GetMass(s.body);
        HandPush hp;
        scanHandPush(s.body, hp);
        const float pusher = hp.pusher;
        const glm::vec3 normal = hp.normal;
        if(hp.full || pusher <= 0.f)
        {
            continue;
        }
        Slot& pushed = world->slots[static_cast<size_t>(p.num)];
        const bool shove = pushed.pushedStep >= world->steps - 1;
        const glm::vec3 v = glmv(b3Body_GetLinearVelocity(s.body)), w = glmv(b3Body_GetAngularVelocity(s.body));
        const float gained = glm::dot(v - p.velocity, normal);
        const float keep = pushKeep(pushed, pusher, m, glm::length(v - p.velocity), dt);
        if(gained <= 0.f || keep >= 1.f)
        {
            continue;
        }
        const glm::vec3 now = p.velocity + (v - p.velocity) * keep;
        b3Body_SetLinearVelocity(s.body, b3v(now));
        b3Body_SetAngularVelocity(s.body, b3v(p.spin + (w - p.spin) * keep));
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: %d %s (%.1f kg) %s by %.1f kg: %.0f u/s gained, x%.2f: %.0f u/s\n", p.num,
                PR_GetString(EDICT_NUM(p.num)->v.classname), m, shove ? "shoved" : "hit", pusher, gained * world->m2u, keep,
                glm::length(now) * world->m2u);
        }
    }
}
} // namespace box3d::detail
