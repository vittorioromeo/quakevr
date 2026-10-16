#include "vr_alloccount.h"
// vr_box3d_standing.cpp -- players on props (standing, riding, pressing, nudges, unsticking), a prop's shape for moves and shots, the SV_ hooks.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// Standing on props (vr_box3d_player_stand; see capsuleStandsOn), each frame after the players have moved.

// How long a jump pushes the prop jumped from (s): the legs' push, which Quake's jump gives at once.
constexpr float jumpPushTime = 0.15f;

// How fast a solid prop in a player's body is pushed out of it at least (m/s; unstickProps).
constexpr float unstickSpeed = 1.f;

// The solid prop client `player` stands on now (FL_ONGROUND on a Box3D prop: VR_StandsOn made it ground), else 0.
[[nodiscard]] int standingOn(edict_t* player)
{
    if(!(static_cast<int>(player->v.flags) & FL_ONGROUND) || static_cast<int>(player->v.movetype) != MOVETYPE_WALK ||
       player->v.health <= 0.f)
    {
        return 0;
    }
    edict_t* ground = PROG_TO_EDICT(player->v.groundentity);
    const int num = NUM_FOR_EDICT(ground);
    return num > svs.maxclients && num < static_cast<int>(world->slots.size()) && !ground->free &&
                   world->slots[num].kind == Kind::Prop && B3_IS_NON_NULL(world->slots[num].body)
               ? num
               : 0;
}

// The body of prop `num` if it still is one (null: not).
[[nodiscard]] b3BodyId standBody(int num)
{
    if(num <= 0 || num >= static_cast<int>(world->slots.size()) || num >= qcvm->num_edicts || EDICT_NUM(num)->free ||
       world->slots[num].kind != Kind::Prop)
    {
        return b3_nullBodyId;
    }
    return world->slots[num].body;
}
} // namespace

// Before the step: where each player stands (in the prop's body, to carry him after it), a prop just landed on woken
// (to feel the weight), a jump's push on the prop jumped from.
void beforeStanding()
{
    world->stands.resize(static_cast<size_t>(svs.maxclients) + 1);
    const float mass = za::max(vr_box3d_player_mass.value, 0.f);
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        World::Stand& st = world->stands[static_cast<size_t>(i)];
        const bool live = vr_box3d_player_stand.value && !player->free && svs.clients[i - 1].active;
        const int num = live ? standingOn(player) : 0;
        if(!num)
        {
            // Off it with a jump (up at a jump's speed from standing on it last frame): the prop takes a share of the
            // push, down where the feet were. As a force over jumpPushTime (pressStanding), as legs push: a blow (an
            // impulse at once) was a hit on what the prop stands on, at the speed it gave the prop (a small explosive
            // box, pushed at its edge, hit the floor at 12 m/s and blew up: ROUND21.md, "Standing on props 2").
            const b3BodyId was = standBody(st.ground);
            if(live && B3_IS_NON_NULL(was) && player->v.velocity[2] > 100.f && vr_box3d_player_jump_push.value > 0.f &&
               mass > 0.f)
            {
                const float impulse = mass * player->v.velocity[2] / world->m2u * vr_box3d_player_jump_push.value; // N s
                st.pushed = st.ground;
                st.pushLocal = st.local;
                st.pushForce = impulse / jumpPushTime;
                st.pushLeft = jumpPushTime;
                b3Body_SetAwake(was, true);
                if(vr_debug_box3d.value)
                {
                    Con_Printf("box3d: player %d jumps off %d, pushing it with %.0f N s\n", i, st.ground, impulse);
                }
            }
            st.ground = 0;
            continue;
        }
        const b3BodyId body = world->slots[num].body;
        if(num != st.ground && mass > 0.f)
        {
            b3Body_SetAwake(body, true); // (landed on it: it feels the weight, then sleeps again once still)
        }
        if(vr_debug_box3d.value && num != st.ground)
        {
            Con_Printf("box3d: player %d stands on %d %s\n", i, num, PR_GetString(EDICT_NUM(num)->v.classname));
        }
        st.ground = num;
        st.feet = glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2] + player->v.mins[2]};
        // (In the prop's drawn box: his weight on its top even where his box overhangs its edge, which a prop standing
        // on its own doesn't tip over from.)
        const Slot& s = world->slots[num];
        const glm::vec3 local =
            glm::clamp(glmv(b3Body_GetLocalPoint(body, world->toM(st.feet))), s.mins / world->m2u, s.maxs / world->m2u);
        st.local = b3v(local);
        st.feet = world->toU(b3Body_GetWorldPoint(body, st.local));
        st.top = EDICT_NUM(num)->v.absmax[2];
        if(vr_debug_box3d.value >= 2.f)
        {
            Con_Printf("box3d: player %d on %d at %.2f %.2f %.2f in it (m)\n", i, num, local.x, local.y, local.z);
        }
    }
}

} // namespace box3d::detail

extern "C" {
extern cvar_t sv_accelerate; // (sv_user.c)
extern cvar_t sv_maxspeed;
}

namespace box3d::detail
{
// Whether prop `num` is a solid prop (an explosive box) with a body: what players stand on, shove and are let out of.
[[nodiscard]] bool solidProp(int num)
{
    return num > svs.maxclients && num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts &&
           world->slots[num].kind == Kind::Prop && B3_IS_NON_NULL(world->slots[num].body) && !EDICT_NUM(num)->free &&
           static_cast<int>(EDICT_NUM(num)->v.solid) == SOLID_BBOX;
}

namespace
{
// Whether `num` is a pushable corpse with a body (vr_corpse_collide 2, 4): a player walking into it shoves it as a solid
// prop (VR_PlayerBumps, shoveBumped), when he meets it (vr_corpse_collide_player).
[[nodiscard]] bool pushableCorpse(int num)
{
    return num > svs.maxclients && num < static_cast<int>(world->slots.size()) && num < qcvm->num_edicts &&
           world->slots[num].kind == Kind::Corpse && world->slots[num].corpseDynamic && B3_IS_NON_NULL(world->slots[num].body);
}

// A walked-through ragdoll's nudge (vr_box3d_walk_nudge): the limbs his legs overlap go along his pace at this share of
// his speed, at most walkNudgeSpeed, pushed at most this much harder than the floor's friction holds them.
constexpr float walkNudgeShare = 0.3f;
constexpr float walkNudgeSpeed = 0.6f; // m/s
constexpr float walkNudgeAccel = 3.f;  // m/s^2
} // namespace

// A nonblocking corpse still feels feet walking through it. Apply a gentle,
// bounded horizontal nudge to nearby limbs without changing player collision.
void nudgeWalkedRagdolls(float dt)
{
    if(!vr_box3d_walk_nudge.value) { return; }
    world->walkOrigins.resize(svs.maxclients + 1);
    for(int player = 1; player <= svs.maxclients; player++)
    {
        edict_t* e = EDICT_NUM(player);
        const glm::vec3 origin = vec(e->v.origin);
        glm::vec3& previous = world->walkOrigins[static_cast<size_t>(player)];
        const glm::vec3 walked{origin.x - previous.x, origin.y - previous.y, 0.f};
        const bool physical = previous != glm::vec3{0.f} && glm::length(walked) < world->m2u;
        previous = origin;
        if(e->free || e->v.health <= 0.f) { continue; }
        glm::vec3 pace{e->v.velocity[0], e->v.velocity[1], 0.f};
        if(physical && dt > 0.f && glm::length(walked) / dt > glm::length(pace)) { pace = walked / dt; }
        const float speed = glm::length(pace) / world->m2u;
        if(speed < 0.05f) { continue; }
        pace = glm::normalize(pace);
        const glm::vec3 lo = vec(e->v.absmin), hi = vec(e->v.absmax);
        const float legs = lo.z + 0.5f * (hi.z - lo.z); // (his legs: the lower half of his box)
        const float target = za::min(speed * walkNudgeShare, walkNudgeSpeed); // m/s
        const float g = -b3World_GetGravity(world->id).z;
        for(size_t num = svs.maxclients + 1; num < world->slots.size(); num++)
        {
            const RagdollBodies* r = ragdollOf(static_cast<int>(num));
            if(!r) { continue; } // (its parts are dynamic bodies whatever Corpse Collision is)
            int nudged = 0;
            for(int part = 0; part < r->count; part++)
            {
                const b3BodyId b = r->body[static_cast<za::SizeT>(part)];
                const b3AABB box = b3Body_ComputeAABB(b);
                const glm::vec3 a = world->toU(box.lowerBound), z = world->toU(box.upperBound);
                if(a.x > hi.x || z.x < lo.x || a.y > hi.y || z.y < lo.y || a.z > legs || z.z < lo.z - 2.f) { continue; }
                const float along = glm::dot(glmv(b3Body_GetLinearVelocity(b)), pace);
                if(along >= target) { continue; }
                // (Its friction with the floor first, then walkNudgeAccel: at walkNudgeAccel alone (3 m/s^2) a limb lying
                // on the floor (friction 0.8 with its 0.6, ~7 m/s^2) never slid, and the nudge never showed.)
                b3ShapeId shape;
                const float mu = b3Body_GetShapes(b, &shape, 1) == 1 ? b3Shape_GetFriction(shape) : 0.8f;
                const float change = za::min(target - along, (mu * g + walkNudgeAccel) * dt);
                b3Body_ApplyLinearImpulseToCenter(b, b3v(pace * (b3Body_GetMass(b) * change)), true);
                nudged++;
            }
            if(nudged && vr_debug_box3d.value >= 2.f)
            {
                Con_Printf("box3d: player %d walking at %.1f m/s nudges %d parts of %d towards %.2f m/s\n", player, speed, nudged,
                    static_cast<int>(num), target);
            }
        }
    }
}

// Before the step: each solid prop a player walked into this frame (VR_PlayerBumps: Quake's move stopped at its side)
// is shoved, as Source's player shadow shoves what it walks into: towards his pace (vr_box3d_player_push_speed at full
// stick) shared by their masses (vr_box3d_player_shove of his vr_box3d_player_mass, and its own: a light box goes at
// nearly his pace, a heavy one slower), with at most that share of his weight (a force: a prop too heavy for it to
// overcome the floor's friction doesn't move). His next move follows it.
void shoveBumped(float dt)
{
    const float share = vr_box3d_player_shove.value;
    const float mp = za::max(vr_box3d_player_mass.value, 1.f);
    const float g = -b3World_GetGravity(world->id).z;
    for(const World::Bump& b : world->bumps)
    {
        if(share <= 0.f || (!solidProp(b.num) && !pushableCorpse(b.num)))
        {
            continue;
        }
        const b3BodyId body = world->slots[b.num].body;
        const RagdollBodies* rag = ragdollOf(b.num); // (a ragdoll: shoved whole, each part its share)
        float mb = 0.f;
        for(int k = 0; k < (rag ? rag->count : 1); k++)
        {
            mb += b3Body_GetMass(rag ? rag->body[static_cast<za::SizeT>(k)] : body);
        }
        mb = za::max(mb, 0.1f);
        // His effort: his speed into it, against a frame's acceleration at full stick (Quake stopped him at it last
        // frame: what he has is what this frame's move gave him, less the ground's friction), up to all of it.
        const float full = za::max(0.5f * sv_accelerate.value * sv_maxspeed.value * dt, 1.f);
        const float u = za::max(vr_box3d_player_push_speed.value, 0.1f) * za::min(b.speed / full, 1.f);
        const float target = u * share * mp / (share * mp + mb); // (his shove's mass and its own)
        const float along = glm::dot(glmv(b3Body_GetLinearVelocity(body)), b.dir);
        if(target <= along)
        {
            continue;
        }
        const float impulse = za::min(mb * (target - along), share * mp * g * dt); // N s
        // (Low, a quarter of its height up, under its centre: the floor's friction, at its bottom, doesn't tip a tall box
        // over as pushed at its centre it did.)
        edict_t* ent = EDICT_NUM(b.num);
        const b3Pos centre = b3Body_GetWorldCenter(body);
        const glm::vec3 at{static_cast<float>(centre.x), static_cast<float>(centre.y), (ent->v.absmin[2] + 0.25f * (ent->v.absmax[2] - ent->v.absmin[2])) / world->m2u};
        if(rag)
        {
            for(int k = 0; k < rag->count; k++)
            {
                const b3BodyId part = rag->body[static_cast<za::SizeT>(k)];
                b3Body_ApplyLinearImpulse(part, b3v(b.dir * (impulse * b3Body_GetMass(part) / mb)), b3Body_GetWorldCenter(part), true);
            }
        }
        else
        {
            b3Body_ApplyLinearImpulse(body, b3v(b.dir * impulse), b3v(at), true);
        }
        if(vr_debug_box3d.value)
        {
            Con_Printf("box3d: player %d walks into %d at %.0f u/s: shoved with %.1f N s (%.0f kg, to %.2f m/s)\n", b.player,
                b.num, b.speed, impulse, mb, target);
        }
    }
    world->bumps.clear();
}

namespace
{
[[nodiscard]] bool feetInTop(edict_t* p, edict_t* prop); // (below, with the players' shape against props)
} // namespace

// Before the step: a solid prop in a player's body (toppled onto him, shoved into him; its drawn shape in his box, less
// a unit), not the one he stands on nor one his feet are a little into the top of, is pushed out of it, away from his
// middle (never down), at 1 m/s at least; his moves aren't blocked by it meanwhile (VR_PropLetsOut). Source pushes a
// prop stuck in the player out likewise.
void unstickProps()
{
    if(!vr_box3d_player_unstick.value)
    {
        return;
    }
    for(int i = 1; i <= svs.maxclients && i < qcvm->num_edicts; i++)
    {
        edict_t* player = EDICT_NUM(i);
        if(player->free || !svs.clients[i - 1].active || player->v.health <= 0.f ||
           static_cast<int>(player->v.movetype) == MOVETYPE_NOCLIP)
        {
            continue;
        }
        vec3_t mins, maxs;
        if(!VR_HullEntBox(player, player->v.mins, player->v.maxs, mins, maxs))
        {
            VectorCopy(player->v.mins, mins);
            VectorCopy(player->v.maxs, maxs);
        }
        const glm::vec3 lo = glm::vec3{mins[0], mins[1], mins[2]} + glm::vec3{1.f}, hi = glm::vec3{maxs[0], maxs[1], maxs[2]} - glm::vec3{1.f};
        const glm::vec3 origin = vec(player->v.origin);
        za::Array<b3Vec3, 8> corners;
        for(int k = 0; k < 8; k++)
        {
            corners[static_cast<size_t>(k)] = world->toM(glm::vec3{k & 1 ? hi.x : lo.x, k & 2 ? hi.y : lo.y, k & 4 ? hi.z : lo.z});
        }
        const b3ShapeProxy proxy{corners.data(), 8, 0.f};
        b3QueryFilter filter = b3DefaultQueryFilter();
        filter.categoryBits = catPlayer;
        filter.maskBits = catSolid;
        struct Query
        {
            int ground;
            za::Array<int, 8> nums;
            int count;
        } q{standingOn(player), {}, 0};
        b3World_OverlapShape(world->id, world->toM(origin), &proxy, filter,
            [](b3ShapeId shape, void* context) {
                Query& oq = *static_cast<Query*>(context);
                const int num = numOf(shape);
                if(num != oq.ground && solidProp(num) && oq.count < static_cast<int>(oq.nums.size()) &&
                   za::find(oq.nums.begin(), oq.nums.begin() + oq.count, num) == oq.nums.begin() + oq.count)
                {
                    oq.nums[static_cast<size_t>(oq.count++)] = num;
                }
                return true;
            },
            &q);
        const glm::vec3 middle = origin + 0.5f * (glm::vec3{mins[0], mins[1], mins[2]} + glm::vec3{maxs[0], maxs[1], maxs[2]});
        for(int k = 0; k < q.count; k++)
        {
            if(vr_box3d_player_hold.value && feetInTop(player, EDICT_NUM(q.nums[static_cast<size_t>(k)])))
            {
                continue; // (he is on it: put back on top, SV_CheckStuck)
            }
            const b3BodyId body = world->slots[q.nums[static_cast<size_t>(k)]].body;
            glm::vec3 away = world->toU(b3Body_GetWorldCenter(body)) - middle;
            away.z = za::max(away.z, 0.f);
            away = glm::length(away) > 0.1f ? glm::normalize(away) : glm::vec3{0.f, 0.f, 1.f};
            const float along = glm::dot(glmv(b3Body_GetLinearVelocity(body)), away);
            if(along < unstickSpeed)
            {
                b3Body_ApplyLinearImpulseToCenter(body, b3v(away * (unstickSpeed - along) * b3Body_GetMass(body)), true);
            }
            if(vr_debug_box3d.value)
            {
                Con_Printf("box3d: %d in player %d's body: pushed out (%.2f %.2f %.2f)\n", q.nums[static_cast<size_t>(k)], i,
                    away.x, away.y, away.z);
            }
        }
    }
}

namespace
{
// How much of a player's weight (or jump) the prop under his feet takes, by how fast the point he stands on goes down
// (local, in the prop's body): all of it while it holds (at rest, rocking, sinking under him), less as it gives way,
// none once it drops at weightFadeSpeed (a box tipping over, or sliding off what held it, as a tilted box on a slope
// or on another prop does). His feet don't push what falls away under them: his weight on a tipping box made it land
// harder than it could by falling (a tall box's top lands at 11 m/s by itself, 14 blows it up; ROUND21.md, "Standing
// on props 3").
constexpr float weightFadeSpeed = 1.f; // m/s

[[nodiscard]] float underFeet(b3BodyId body, b3Vec3 local)
{
    const float down = -b3Body_GetLocalPointVelocity(body, local).z;
    return za::clamp(1.f - down / weightFadeSpeed, 0.f, 1.f);
}
} // namespace

// Each piece of the step: the weight of each player standing on a prop (vr_box3d_player_mass) where he stands. Not
// waking it: a prop asleep under him stays so (a stack at rest stays still). On a floating prop, at most half the
// prop's own, through its centre: the water lifts a prop by its weight, not its volume, and at its centre (beforeStep),
// so a player heavier than a box would sink it, and his weight on its top would capsize a tall one; so it floats nine
// tenths under, upright.
void pressStanding(float dt)
{
    // A jump's push, until its time is spent (beforeStanding). A force, like the weight: a prop on the floor is held up
    // by it, with no blow (a hit) on it; a floating one is pushed under.
    for(World::Stand& st : world->stands)
    {
        const b3BodyId pushed = standBody(st.pushed);
        if(st.pushLeft > 0.f && B3_IS_NON_NULL(pushed))
        {
            const float share = za::min(dt, st.pushLeft) / dt * underFeet(pushed, st.pushLocal);
            b3Body_ApplyForce(pushed, b3Vec3{0.f, 0.f, -st.pushForce * share}, b3Body_GetWorldPoint(pushed, st.pushLocal), true);
            st.pushLeft -= dt;
        }
        else
        {
            st.pushed = 0;
            st.pushLeft = 0.f;
        }
    }
    const float mass = vr_box3d_player_mass.value;
    if(mass <= 0.f)
    {
        return;
    }
    const float g = -b3World_GetGravity(world->id).z; // m/s^2
    for(const World::Stand& st : world->stands)
    {
        const b3BodyId body = standBody(st.ground);
        if(B3_IS_NON_NULL(body))
        {
            if(world->slots[st.ground].wet)
            {
                const float most = za::min(mass, 0.5f * b3Body_GetMass(body));
                b3Body_ApplyForceToCenter(body, b3Vec3{0.f, 0.f, -most * g}, false);
            }
            else
            {
                b3Body_ApplyForce(body, b3Vec3{0.f, 0.f, -mass * g * underFeet(body, st.local)}, world->toM(st.feet), false);
            }
        }
    }
}

// After the step: each player standing on a prop that moved is carried across with the point he stands on (its turn
// too, but not his view: VR), and up or down with the top of the prop's Quake box, which his box stands on (solidBox:
// round the prop as it turns, so a box tipping over lowers him with its box, not with the point, which goes down its
// side). His box is traced there, the world (and every other entity) stopping it, never from inside a wall; if the prop
// turned so that its box now holds him, he steps up out of it.
void rideStanding()
{
    for(int i = 1; i < static_cast<int>(world->stands.size()) && i < qcvm->num_edicts; i++)
    {
        World::Stand& st = world->stands[static_cast<size_t>(i)];
        const b3BodyId body = standBody(st.ground);
        if(B3_IS_NULL(body))
        {
            continue;
        }
        edict_t* player = EDICT_NUM(i);
        edict_t* ground = EDICT_NUM(st.ground);
        const glm::vec3 to = world->toU(b3Body_GetWorldPoint(body, st.local));
        // (Meeting its drawn shape, vr_box3d_player_shape, he stands on its face: up or down with the point.)
        const glm::vec3 move{to.x - st.feet.x, to.y - st.feet.y,
            vr_box3d_player_shape.value ? to.z - st.feet.z : ground->v.absmax[2] - st.top};
        const float distance = glm::length(move);
        if(distance < 0.01f || distance > 64.f)
        {
            continue;
        }
        const float solid = ground->v.solid;
        ground->v.solid = SOLID_NOT; // (the prop has moved already: the player's box meets it where it is now)
        vec3_t end;
        for(int k = 0; k < 3; k++)
        {
            end[k] = player->v.origin[k] + move[k];
        }
        const trace_t tr = SV_Move(player->v.origin, player->v.mins, player->v.maxs, end, MOVE_NORMAL, player);
        ground->v.solid = solid;
        if(tr.allsolid || tr.startsolid)
        {
            continue;
        }
        VectorCopy(tr.endpos, player->v.origin);
        const trace_t in = SV_Move(player->v.origin, player->v.mins, player->v.maxs, player->v.origin, MOVE_NORMAL, player);
        if(in.startsolid && in.ent == ground)
        {
            vec3_t up;
            VectorCopy(player->v.origin, up);
            up[2] += 18.f; // (a step)
            const trace_t down = SV_Move(up, player->v.mins, player->v.maxs, player->v.origin, MOVE_NORMAL, player);
            if(!down.startsolid)
            {
                VectorCopy(down.endpos, player->v.origin);
            }
        }
        SV_LinkEdict(player, true);
        if(vr_debug_box3d.value >= 2.f)
        {
            Con_Printf("box3d: player %d carried %.2f %.2f %.2f by %d\n", i, move.x, move.y, move.z, st.ground);
        }
    }
}

namespace
{
template <typename Shape>
void addNormal(Shape& p, glm::dvec3 n)
{
    const double length = glm::length(n);
    if(length < 1e-6)
    {
        return;
    }
    n /= length;
    for(int k = 0; k < 3; k++)
    {
        n[k] = za::abs(n[k]) < 1e-7 ? 0.0 : n[k]; // (a wall's normal is flat: Quake's step up needs normal z 0)
    }
    n = glm::normalize(n);
    p.normals[static_cast<size_t>(p.count++)] = n;
    p.normals[static_cast<size_t>(p.count++)] = -n;
}

// The prop `touch` (slot s) and the player's box (mins, maxs from his origin) as they meet.
[[nodiscard]] PropShape propShapeAt(const glm::vec3& origin, const glm::mat3& axes, const Slot& s, const float* mins, const float* maxs);
} // namespace

[[nodiscard]] PropShape propShape(edict_t* touch, const Slot& s, const float* mins, const float* maxs)
{
    return propShapeAt(vec(touch->v.origin), held::axesFromAngles(touch->v.angles, s.brush), s, mins, maxs);
}

namespace
{
// The same with the prop at origin, turned to axes (its body's pose: vr_physics_inside).
[[nodiscard]] PropShape propShapeAt(const glm::vec3& origin, const glm::mat3& axes, const Slot& s, const float* mins, const float* maxs)
{
    PropShape p;
    const glm::vec3 mid = (s.mins + s.maxs) * 0.5f;
    p.centre = glm::dvec3{origin} + glm::dvec3{axes * mid};
    p.half = glm::dvec3{(s.maxs - s.mins) * 0.5f};
    int up = 0;
    for(int k = 0; k < 3; k++)
    {
        p.axes[static_cast<size_t>(k)] = glm::normalize(glm::dvec3{axes[k]});
        if(za::abs(axes[k].z) > za::abs(axes[up].z))
        {
            up = k;
        }
    }
    // The column turned with the prop's sides (its yaw): an upright box's faces meet its flats, at exactly his half-width.
    const glm::dvec3 side = p.axes[static_cast<size_t>((up + 1) % 3)];
    const double yaw = za::atan2(side.y, side.x);
    const double r = 0.5 * za::min(maxs[0] - mins[0], maxs[1] - mins[1]);
    const double corner = r / za::cos(glm::pi<double>() / columnSides);
    const glm::dvec2 at{0.5 * (mins[0] + maxs[0]), 0.5 * (mins[1] + maxs[1])};
    for(int k = 0; k < columnSides; k++)
    {
        const double a = yaw + (k + 0.5) * 2.0 * glm::pi<double>() / columnSides;
        p.ring[static_cast<size_t>(k)] = at + corner * glm::dvec2{za::cos(a), za::sin(a)};
    }
    p.bottom = mins[2];
    p.top = maxs[2];
    const glm::dvec3 z{0.0, 0.0, 1.0};
    for(const glm::dvec3& a : p.axes)
    {
        addNormal(p, a);
        addNormal(p, glm::cross(a, z));
    }
    addNormal(p, z);
    for(int k = 0; k < columnSides / 2; k++)
    {
        const double a = yaw + k * 2.0 * glm::pi<double>() / columnSides;
        const glm::dvec3 flat{za::cos(a), za::sin(a), 0.0}, edge{-za::sin(a), za::cos(a), 0.0};
        addNormal(p, flat);
        for(const glm::dvec3& b : p.axes)
        {
            addNormal(p, glm::cross(b, edge));
        }
    }
    return p;
}
} // namespace

// How far the player's origin may go along n: the support of the prop's box less the column's (their Minkowski sum).
[[nodiscard]] double propSupport(const PropShape& p, const glm::dvec3& n)
{
    double box = glm::dot(n, p.centre);
    for(int k = 0; k < 3; k++)
    {
        box += p.half[k] * za::abs(glm::dot(n, p.axes[static_cast<size_t>(k)]));
    }
    double ring = -1e300;
    for(const glm::dvec2& v : p.ring)
    {
        ring = za::max(ring, -(n.x * v.x + n.y * v.y));
    }
    return box + ring + za::max(-n.z * p.bottom, -n.z * p.top);
}

// How far a shot's origin may go along n: the prop's box's support plus the shot's box's.
[[nodiscard]] double propSupport(const ShotShape& p, const glm::dvec3& n)
{
    double h = glm::dot(n, p.centre);
    for(int k = 0; k < 3; k++)
    {
        h += p.half[k] * za::abs(glm::dot(n, p.axes[static_cast<size_t>(k)])) + p.ext[k] * za::abs(n[k]);
    }
    return h;
}

// The shortest distance from the player's upright centre line (origin, his box's height) to the prop's drawn box.
[[nodiscard]] double columnGap(const PropShape& p, const glm::dvec3& origin)
{
    double best = 1e300;
    for(int k = 0; k <= 32; k++)
    {
        const glm::dvec3 at{origin.x, origin.y, origin.z + p.bottom + (p.top - p.bottom) * k / 32.0};
        const glm::dvec3 d = at - p.centre;
        glm::dvec3 out{0.0};
        for(int i = 0; i < 3; i++)
        {
            const double t = glm::dot(d, p.axes[static_cast<size_t>(i)]);
            out[i] = za::max(za::abs(t) - p.half[i], 0.0);
        }
        best = za::min(best, glm::length(out));
    }
    return best;
}

// vr_physics_player: how far under the feet of player p the solid prop he stands on is, as drawn and as its upright
// box (with vr_box3d_player_shape he stands on the first; off, on the second).
void playerOverProp(edict_t* p)
{
    edict_t* g = PROG_TO_EDICT(p->v.groundentity);
    if(!world || !(static_cast<int>(p->v.flags) & FL_ONGROUND) || !solidProp(NUM_FOR_EDICT(g)))
    {
        return;
    }
    vec3_t mins, maxs;
    if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs))
    {
        VectorCopy(p->v.mins, mins);
        VectorCopy(p->v.maxs, maxs);
    }
    const PropShape shape = propShape(g, world->slots[NUM_FOR_EDICT(g)], mins, maxs);
    trace_t tr{};
    tr.fraction = 1.f;
    const glm::dvec3 from{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
    const bool hit = traceProp(shape, from, from - glm::dvec3{0.0, 0.0, 64.0}, tr);
    Con_Printf("vr_physics_player: on %d: its drawn shape %s under your feet, its upright box's top %.2f\n", NUM_FOR_EDICT(g),
        !hit ? "not" : tr.startsolid ? "in them" : va("%.2f units", tr.fraction * 64.f), p->v.origin[2] + p->v.mins[2] - g->v.absmax[2]);
}

namespace
{
// How deep the player's origin o is inside the sum (negative: outside, by that much along the nearest face).
[[nodiscard]] double insideDepth(const PropShape& p, const glm::dvec3& o)
{
    double depth = 1e300;
    for(int k = 0; k < p.count; k++)
    {
        const glm::dvec3& n = p.normals[static_cast<size_t>(k)];
        depth = za::min(depth, propSupport(p, n) - glm::dot(n, o));
    }
    return depth;
}

// How deep a player's origin is inside solid prop `prop` (his box mins, maxs as he meets it; shaped: its drawn shape,
// else its Quake box), and the nearest way out (the face of their sum it is nearest; negative: outside by that much).
[[nodiscard]] double depthIn(edict_t* prop, const float* origin, const float* mins, const float* maxs, bool shaped, glm::dvec3& out)
{
    const glm::dvec3 o{origin[0], origin[1], origin[2]};
    double depth = 1e300;
    const auto face = [&](const glm::dvec3& n, double d) {
        if(d < depth)
        {
            depth = d;
            out = n;
        }
    };
    if(shaped)
    {
        const PropShape p = propShape(prop, world->slots[static_cast<size_t>(NUM_FOR_EDICT(prop))], mins, maxs);
        for(int k = 0; k < p.count; k++)
        {
            const glm::dvec3& n = p.normals[static_cast<size_t>(k)];
            face(n, propSupport(p, n) - glm::dot(n, o));
        }
    }
    else
    {
        for(int k = 0; k < 3; k++) // (Quake's box against its box: their sum's six faces)
        {
            glm::dvec3 n{0.0};
            n[k] = 1.0;
            face(n, prop->v.origin[k] + prop->v.maxs[k] - mins[k] - o[k]);
            face(-n, o[k] - (prop->v.origin[k] + prop->v.mins[k] - maxs[k]));
        }
    }
    return depth;
}

// A player inside a solid prop with his feet only a little into its top: the nearest way out is up, through a face he
// would stand on, at most a step. He landed on it, or it rose or turned into his feet (rocking under him, woken by his
// weight), and is on it, not under it (ROUND21.md, "Phasing through a toppled box").
[[nodiscard]] bool feetInTop(edict_t* prop, const float* origin, const float* mins, const float* maxs, bool shaped)
{
    glm::dvec3 out{0.0};
    const double depth = depthIn(prop, origin, mins, maxs, shaped, out);
    return out.z > 0.7 && depth <= VR_StepSize(18.f);
}

// The same for player p where he is, with his box as his moves meet prop (vr_hull_ent_width).
[[nodiscard]] bool feetInTop(edict_t* p, edict_t* prop)
{
    vec3_t mins, maxs;
    if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs) || !VR_HullNarrowsAgainst(p, prop))
    {
        VectorCopy(p->v.mins, mins);
        VectorCopy(p->v.maxs, maxs);
    }
    return feetInTop(prop, p->v.origin, mins, maxs, vr_box3d_player_shape.value != 0.f);
}
} // namespace

void watchInside()
{
    if(!insideWatch.on || !world || !vr_box3d_player_shape.value)
    {
        return;
    }
    insideWatch.frames++;
    for(int num = 1; num <= svs.maxclients && num < qcvm->num_edicts; num++)
    {
        edict_t* p = EDICT_NUM(num);
        if(p->free || static_cast<int>(p->v.solid) != SOLID_SLIDEBOX || static_cast<int>(p->v.movetype) == MOVETYPE_NOCLIP)
        {
            continue;
        }
        vec3_t mins, maxs;
        if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs))
        {
            VectorCopy(p->v.mins, mins);
            VectorCopy(p->v.maxs, maxs);
        }
        int in = 0;
        double depth = 0.0, traced = 0.0;
        for(int g = svs.maxclients + 1; g < static_cast<int>(world->slots.size()) && g < qcvm->num_edicts && !in; g++)
        {
            edict_t* e = EDICT_NUM(g);
            if(!solidProp(g) || e->free)
            {
                continue;
            }
            // (Its body as it is, not the entity's origin and angles its shape is traced from.)
            const Slot& s = world->slots[static_cast<size_t>(g)];
            const b3WorldTransform xf = b3Body_GetTransform(s.body);
            const PropShape shape = propShapeAt(world->toU(xf.p), glm::mat3_cast(fromB3(xf.q)), s, mins, maxs);
            depth = insideDepth(shape, glm::dvec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]});
            in = depth > 0.01 ? g : 0;
            if(in)
            {
                traced = insideDepth(propShape(e, s, mins, maxs), glm::dvec3{p->v.origin[0], p->v.origin[1], p->v.origin[2]});
            }
        }
        const size_t k = static_cast<size_t>(num);
        if(in)
        {
            insideWatch.inside++;
            insideWatch.deepest = za::max(insideWatch.deepest, depth);
            if(insideWatch.in[k] != in)
            {
                insideWatch.times++;
                edict_t* e = EDICT_NUM(in);
                edict_t* g = PROG_TO_EDICT(p->v.groundentity);
                Con_Printf("vr_physics_inside: %.2f s player %d in %d by %.2f (its traced shape: %.2f) at %.2f %.2f %.2f vel %.0f %.0f %.0f, "
                           "%s on %d; it at %.2f %.2f %.2f angles %.2f %.2f %.2f, %s\n",
                    qcvm->time, num, in, depth, traced, p->v.origin[0], p->v.origin[1], p->v.origin[2], p->v.velocity[0], p->v.velocity[1],
                    p->v.velocity[2], (static_cast<int>(p->v.flags) & FL_ONGROUND) ? "grounded" : "in the air", NUM_FOR_EDICT(g),
                    e->v.origin[0], e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2],
                    world->slots[static_cast<size_t>(in)].asleep ? "asleep" : "awake");
            }
        }
        insideWatch.in[k] = in;
        insideWatch.run[k] = in ? insideWatch.run[k] + 1 : 0;
        insideWatch.longest = za::max(insideWatch.longest, insideWatch.run[k]);
    }
}
} // namespace box3d::detail

// SV_FlyMove: a solid prop is ground to a player (vr_box3d_player_stand) as a brush is, where its face (met at `normal`)
// is no steeper than vr_box3d_player_slope. Steeper, he slides off it, not friction holding him: with Quake's limit (a
// normal's z over 0.7, 45.6 degrees), on a box tilted 33 degrees he stood, creeping down at 7 units a second, its own
// weight on it (ROUND21.md, "Sliding off steep boxes").
extern "C" int VR_Box3DSteps(void)
{
    return world ? world->steps : 0;
}

extern "C" int VR_StandsOn(edict_t* ent, edict_t* ground, const float* normal)
{
    if(!world)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(ent), g = NUM_FOR_EDICT(ground);
    // A corpse he meets (vr_corpse_collide_player): ground, as a step.
    if(num >= 1 && num <= svs.maxclients && g > svs.maxclients && g < static_cast<int>(world->slots.size()) &&
        world->slots[g].kind == Kind::Corpse && vr_corpse_collide_player.value > 0.f)
    {
        return 1;
    }
    if(!vr_box3d_player_stand.value)
    {
        return 0;
    }
    const float steepest = za::clamp(vr_box3d_player_slope.value, 0.f, 90.f);
    return num >= 1 && num <= svs.maxclients && g > svs.maxclients && g < static_cast<int>(world->slots.size()) &&
           world->slots[g].kind == Kind::Prop && static_cast<int>(ground->v.solid) == SOLID_BBOX &&
           normal[2] >= za::cos(glm::radians(steepest)) - 0.0001f;
}

// SV_ClipToLinks: a body's move (a player's, a monster's) and a corpse in its way (vr_corpse_collide_player, _monsters):
// the box it meets (from the corpse's origin) is its body's as it lies (its fitted shape's or its lying box, as turned) in
// its Quake box,
// no taller than a step (corpseStep: Step Over; a monster always) or than corpseTop (Solid). Not if the move starts in it
// (it fell or was pushed onto him, he stood where it came to lie): he walks out of it.
extern "C" int VR_CorpseBox(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    float* boxmins, float* boxmaxs)
{
    if(!world || !mover)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(touch);
    if(num >= static_cast<int>(world->slots.size()) || world->slots[num].kind != Kind::Corpse ||
        B3_IS_NULL(world->slots[num].body) || (corpseMode() == 0 && world->slots[num].ragdoll < 0))
    {
        return 0; // (a ragdoll is one whatever Corpse Collision is)
    }
    const int m = NUM_FOR_EDICT(mover);
    int how = 0;
    if(m >= 1 && m <= svs.maxclients)
    {
        how = static_cast<int>(vr_corpse_collide_player.value);
    }
    else if(hasFlag(mover, FL_MONSTER) && mover->v.health > 0.f && vr_corpse_collide_monsters.value)
    {
        how = 1;
    }
    if(how <= 0)
    {
        return 0;
    }
    const b3AABB box = b3Body_ComputeAABB(world->slots[num].body);
    const glm::vec3 origin = vec(touch->v.origin);
    // (Within its Quake box: what Quake's moves look for it in, SV_ClipToLinks's broad phase and its area's links. A
    // ragdoll's is its parts' box: writeRagdoll.)
    const bool rag = world->slots[num].ragdoll >= 0;
    const glm::vec3 lo = rag ? vec(touch->v.mins) : glm::max(world->toU(box.lowerBound) - origin, vec(touch->v.mins));
    glm::vec3 hi = rag ? vec(touch->v.maxs) : glm::min(world->toU(box.upperBound) - origin, vec(touch->v.maxs));
    hi.z = za::min(hi.z, lo.z + (how == 1 ? corpseStep : corpseTop));
    if(glm::any(glm::lessThanEqual(hi - lo, glm::vec3{1.f})))
    {
        return 0;
    }
    // His feet a little into its top (walking down a slope onto it; a probe of his box a unit lower): its top is at his
    // feet, he is on it.
    constexpr float in = 0.1f, onTop = 3.f; // units
    const float feet = start[2] + mins[2] - origin.z;
    if(feet < hi.z && feet >= hi.z - onTop)
    {
        hi.z = za::max(feet, lo.z + 1.f);
    }
    for(int i = 0; i < 3; i++)
    {
        if(start[i] + maxs[i] <= origin[i] + lo[i] + in || start[i] + mins[i] >= origin[i] + hi[i] - in)
        {
            store(lo, boxmins);
            store(hi, boxmaxs);
            return 1;
        }
    }
    return 0;
}

// SV_FlyMove: a player's move stopped by a solid prop's side (not its top: he stands on that) shoves it (shoveBumped).
extern "C" void VR_PlayerBumps(edict_t* ent, edict_t* other, const float* normal)
{
    if(!world || vr_box3d_player_shove.value <= 0.f)
    {
        return;
    }
    const int num = NUM_FOR_EDICT(ent), g = NUM_FOR_EDICT(other);
    if(num < 1 || num > svs.maxclients || (!solidProp(g) && !pushableCorpse(g)) || normal[2] < -0.7f)
    {
        return;
    }
    // (Not the one he stands on: walking into a steep part of it, a tilted box's upper end, he would shove it from under
    // himself, and ride it on.)
    if(num < static_cast<int>(world->stands.size()) && world->stands[static_cast<size_t>(num)].ground == g)
    {
        return;
    }
    glm::vec3 dir{-normal[0], -normal[1], 0.f};
    if(glm::length(dir) < 0.3f)
    {
        return;
    }
    dir = glm::normalize(dir);
    const float speed = ent->v.velocity[0] * dir.x + ent->v.velocity[1] * dir.y;
    if(speed < 1.f)
    {
        return;
    }
    for(World::Bump& b : world->bumps)
    {
        if(b.player == num && b.num == g)
        {
            if(speed > b.speed)
            {
                b.dir = dir;
                b.speed = speed;
            }
            return;
        }
    }
    world->bumps.pushBack({num, g, dir, speed});
}

// A prop wedged in the level (its box still in it 2 units inside its surface: Box3D can't push it out, of the level nor
// of a player in it). A player in one isn't held (VR_PropLetsOut): it would hold him for good. (The big box toppled
// onto the player against a wall stayed 9 units in it a time in four, the player under it walking on the spot; ROUND21.md,
// "Props regressions".)
[[nodiscard]] bool wedgedInLevel(int num)
{
    if(num <= svs.maxclients || num >= static_cast<int>(world->slots.size()))
    {
        return false;
    }
    const Slot& s = world->slots[static_cast<size_t>(num)];
    glm::vec3 lo, hi;
    qvr::box3d::ownBox(s, lo, hi);
    if(B3_IS_NULL(s.body) || hi.x <= lo.x)
    {
        return false;
    }
    const edict_t* ent = EDICT_NUM(num);
    return qvr::box3d::boxInLevel(lo, hi, vec(ent->v.origin), turnOf(ent->v.angles, s.brush), 2.f);
}

// SV_ClipToLinks: a player's move that starts inside a solid prop's box (toppled onto him) goes on, out of it; not in the
// one he stands on (riding it, rideStanding steps him up out of it), nor one his feet are a little into the top of
// (feetInTop: landed on it, or it rocked up into him; he's held, and SV_CheckStuck puts him back on top); and only a
// move that takes him no deeper into it (vr_box3d_player_hold). Letting every move through, he fell into a box that
// rocked up into his feet, or a side that rocked into him, and went on through it: "Phasing through a toppled box",
// ROUND21.md.
extern "C" int VR_PropLetsOut(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    const float* end, int shaped)
{
    if(!world || !vr_box3d_player_unstick.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover), g = NUM_FOR_EDICT(touch);
    if(num < 1 || num > svs.maxclients || !solidProp(g) ||
        ((static_cast<int>(mover->v.flags) & FL_ONGROUND) && PROG_TO_EDICT(mover->v.groundentity) == touch))
    {
        return 0;
    }
    if(vr_box3d_player_hold.value)
    {
        vec3_t entmins, entmaxs;
        const bool narrow = VR_HullEntBox(mover, mins, maxs, entmins, entmaxs) && VR_HullNarrowsAgainst(mover, touch);
        const float* m = narrow ? entmins : mins;
        const float* M = narrow ? entmaxs : maxs;
        glm::dvec3 out{0.0}, to{0.0};
        const double depth = depthIn(touch, start, m, M, shaped != 0, out);
        if(depthIn(touch, end, m, M, shaped != 0, to) > depth + 0.01 && !wedgedInLevel(g))
        {
            return 0; // (deeper: held, the move is stopped)
        }
        if(out.z > 0.7 && depth <= VR_StepSize(18.f)) // (feetInTop)
        {
            return 0;
        }
    }
    return 1;
}

// SV_ClipToLinks, Quake's owner rule (an entity's traces pass its own missiles): a player's traces (his moves, his shots,
// his hands') meet a solid prop even when it is his (vr_box3d_player_hold). A box let go of fast enough to be a throw
// (VR_Carry_Release; a heavy one at a gentle 1.6 m/s: a two-handed topple let go of on its way down) was his from then
// on, and he walked through it until he took it again (the author's note vrfiringrange_2026-10-01_00-33). Starting
// inside it (let go of against his body) he is let out as from any solid prop (VR_PropLetsOut), and it is pushed out of
// him (unstickProps).
extern "C" int VR_OwnPropMeets(edict_t* mover, edict_t* touch)
{
    if(!world || !vr_box3d_player_hold.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover);
    return num >= 1 && num <= svs.maxclients && solidProp(NUM_FOR_EDICT(touch));
}

// SV_ClipToLinks: a player's own box (his move's; mins, maxs) against a solid prop meets its drawn box as it is turned,
// as a round column (boxmins, boxmaxs: his box against entities, vr_hull_ent_width). See propShape.
extern "C" int VR_PropClip(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    const float* boxmins, const float* boxmaxs, const float* end, trace_t* trace)
{
    if(!world || !vr_box3d_player_shape.value)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(mover), g = NUM_FOR_EDICT(touch);
    const auto same = [](const float* a, const float* b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; };
    if(num < 1 || num > svs.maxclients || !solidProp(g) || !same(mins, mover->v.mins) || !same(maxs, mover->v.maxs) ||
       boxmaxs[0] - boxmins[0] <= 0.f)
    {
        return 0;
    }
    const PropShape p = propShape(touch, world->slots[g], boxmins, boxmaxs);
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1.f;
    VectorCopy(end, trace->endpos);
    if(traceProp(p, glm::dvec3{start[0], start[1], start[2]}, glm::dvec3{end[0], end[1], end[2]}, *trace))
    {
        trace->ent = touch;
    }
    trace->inopen = !trace->allsolid;
    return 1;
}

namespace box3d::detail
{
namespace
{
// Whether `mover` is a flying or thrown thing: a rocket, a nail, a grenade, a laser, a Vore pod, the grappling hook, a gib
// (MOVETYPE_FLY, FLYMISSILE, BOUNCE, TOSS, GIB); not a player or a monster (in the air), nor a physics body (Box3D's: a
// prop, one held, a pickup's fixture).
[[nodiscard]] bool flyingThing(edict_t* mover)
{
    if(!mover || mover == qcvm->edicts || (static_cast<int>(mover->v.flags) & (FL_CLIENT | FL_MONSTER)))
    {
        return false;
    }
    const int m = static_cast<int>(mover->v.movetype);
    if(m != MOVETYPE_FLY && m != MOVETYPE_FLYMISSILE && m != MOVETYPE_BOUNCE && m != MOVETYPE_TOSS && m != MOVETYPE_GIB)
    {
        return false;
    }
    const int num = NUM_FOR_EDICT(mover);
    if(num < static_cast<int>(world->slots.size()))
    {
        const Kind k = world->slots[static_cast<size_t>(num)].kind;
        return k != Kind::Prop && k != Kind::Held && k != Kind::Fixture;
    }
    return true;
}

// Whether a trace or move (SV_Move's type, with its flags) by `mover` is a shot's or a missile's: a missile's move
// (MOVE_MISSILE, or any of a flying thing's), or a gun's or the grappling hook's trace (MOVE_HITMODEL of their class, or
// MOVE_HITGIBS alone: the pellets, the lightning, the burst rifle, the laser cannon). Not a melee blow's or a thrown
// weapon's trace (their classes): those keep the box.
[[nodiscard]] bool shotMove(edict_t* mover, int type)
{
    if((type & ~(MOVE_HITGIBS | MOVE_HITMODEL | MOVE_HITMODEL_CLASS)) == MOVE_MISSILE || flyingThing(mover))
    {
        return true;
    }
    if(type & MOVE_HITMODEL)
    {
        return ((type & MOVE_HITMODEL_CLASS) >> MOVE_HITMODEL_CLASS_SHIFT) <= 1; // (guns 0, the grappling hook 1)
    }
    return (type & MOVE_HITGIBS) != 0;
}
} // namespace

// The shot's box (mins, maxs from its origin) against prop `touch` (slot s) as it stands.
[[nodiscard]] ShotShape shotShape(const edict_t* touch, const Slot& s, const float* mins, const float* maxs)
{
    ShotShape p;
    const glm::mat3 axes = held::axesFromAngles(touch->v.angles, s.brush);
    const glm::vec3 mid = (s.mins + s.maxs) * 0.5f;
    const glm::dvec3 offset{0.5 * (mins[0] + maxs[0]), 0.5 * (mins[1] + maxs[1]), 0.5 * (mins[2] + maxs[2])};
    p.centre = glm::dvec3{vec(touch->v.origin)} + glm::dvec3{axes * mid} - offset;
    p.half = glm::dvec3{(s.maxs - s.mins) * 0.5f};
    p.ext = glm::dvec3{0.5 * (maxs[0] - mins[0]), 0.5 * (maxs[1] - mins[1]), 0.5 * (maxs[2] - mins[2])};
    for(int k = 0; k < 3; k++)
    {
        p.axes[static_cast<size_t>(k)] = glm::normalize(glm::dvec3{axes[k]});
        addNormal(p, p.axes[static_cast<size_t>(k)]);
    }
    if(p.ext.x > 0.0 || p.ext.y > 0.0 || p.ext.z > 0.0)
    {
        for(int w = 0; w < 3; w++)
        {
            glm::dvec3 e{0.0};
            e[w] = 1.0;
            addNormal(p, e);
            for(const glm::dvec3& a : p.axes)
            {
                addNormal(p, glm::cross(a, e)); // (parallel ones are skipped)
            }
        }
    }
    return p;
}

} // namespace box3d::detail

// SV_ClipToLinks: a shot or a missile (shotMove: rockets, nails, grenades, lasers, Vore pods, the grappling hook; the
// guns' traces) against a solid prop (an explosive box) meets its drawn box as it is turned, not Quake's box round it: a
// turned box's empty corners let it by, and it is met at its real surface (vr_box3d_shot_shape; the flying hook always,
// as before it: ROUND21.md, the grapple5 section).
extern "C" int VR_PropShotClip(edict_t* mover, edict_t* touch, const float* start, const float* mins, const float* maxs,
    const float* end, int type, trace_t* trace)
{
    if(!world)
    {
        return 0;
    }
    const int g = NUM_FOR_EDICT(touch);
    if(!solidProp(g))
    {
        return 0;
    }
    const bool point = mins[0] == 0.f && mins[1] == 0.f && mins[2] == 0.f && maxs[0] == 0.f && maxs[1] == 0.f && maxs[2] == 0.f;
    const bool hook = point && mover && fieldFloatOr(mover, fields().vr_hitclass, 0.f) == 1.f && flyingThing(mover);
    if(!hook && (!vr_box3d_shot_shape.value || !shotMove(mover, type)))
    {
        return 0;
    }
    const ShotShape p = shotShape(touch, world->slots[static_cast<size_t>(g)], mins, maxs);
    memset(trace, 0, sizeof(*trace));
    trace->fraction = 1.f;
    VectorCopy(end, trace->endpos);
    if(traceProp(p, glm::dvec3{start[0], start[1], start[2]}, glm::dvec3{end[0], end[1], end[2]}, *trace))
    {
        trace->ent = touch;
    }
    trace->inopen = !trace->allsolid;
    return 1;
}

// SV_PushEntity, a flying thing's move that met `other` (vr_debug_missiles): what, where, the surface's normal; a solid
// prop: its drawn box as it stands (centre, half sizes, axes).
extern "C" void VR_MissileHitDebug(edict_t* ent, edict_t* other, const trace_t* trace)
{
    if(!vr_debug_missiles.value || !world || !flyingThing(ent))
    {
        return;
    }
    Con_Printf("missile hit: %s %d hit %s %d at %.2f %.2f %.2f normal %.3f %.3f %.3f\n", PR_GetString(ent->v.classname),
        NUM_FOR_EDICT(ent), other == qcvm->edicts ? "the world" : PR_GetString(other->v.classname), NUM_FOR_EDICT(other),
        static_cast<double>(trace->endpos[0]), static_cast<double>(trace->endpos[1]), static_cast<double>(trace->endpos[2]),
        static_cast<double>(trace->plane.normal[0]), static_cast<double>(trace->plane.normal[1]),
        static_cast<double>(trace->plane.normal[2]));
    const int g = NUM_FOR_EDICT(other);
    if(solidProp(g))
    {
        const float zero[3]{0.f, 0.f, 0.f};
        const ShotShape p = shotShape(other, world->slots[static_cast<size_t>(g)], zero, zero);
        Con_Printf("missile hit: prop %d centre %.3f %.3f %.3f half %.3f %.3f %.3f\n", g, p.centre.x, p.centre.y, p.centre.z,
            p.half.x, p.half.y, p.half.z);
        for(int k = 0; k < 3; k++)
        {
            const glm::dvec3& a = p.axes[static_cast<size_t>(k)];
            Con_Printf("missile hit: prop %d axis %d %.5f %.5f %.5f\n", g, k, a.x, a.y, a.z);
        }
    }
}
