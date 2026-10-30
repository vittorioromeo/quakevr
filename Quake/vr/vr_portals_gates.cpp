// vr_portals_gates.cpp -- part of vr_portals.cpp (see vr_portals.hpp, vr_portals_internal.hpp): crossing a gate (players,
// monsters and what flies, the split body, unsticking), the AI's images and routes through gates, pulls and tosses.

#include "vr_portals_internal.hpp"

using namespace qvr;

extern "C" void SV_AddToFatPVS(vec3_t org, mnode_t* node, qmodel_t* worldmodel); // sv_main.c

namespace qvr::portals
{
using namespace detail;

namespace detail
{

ClientState clients[MAX_SCOREBOARD];

int unstuckCount = 0; // times a player was got out of the wall behind a gate (vr_portals_stuck prints it)

za::Vector<ClientState> monsters; // by edict number

ClientState& crossingState(edict_t* ent)
{
    const int num = NUM_FOR_EDICT(ent);
    if(num > svs.maxclients && static_cast<za::SizeT>(num) >= monsters.size())
    {
        monsters.resize(static_cast<za::SizeT>(za::max(num + 1, qcvm->num_edicts)));
    }
    ClientState& state = num > svs.maxclients ? monsters[static_cast<za::SizeT>(num)] : clients[num - 1];
    if(state.generation != worldGeneration())
    {
        state = {};
        state.generation = worldGeneration();
    }
    return state;
}

[[nodiscard]] bool walkOn()
{
    return enabled() && vr_portals.value > 0.f && vr_portals_walk.value > 0.f && sv.active && sv.state == ss_active &&
        sv.worldmodel;
}

// A living monster (not a player) walking (vr_portals_monsters): it goes through paired gates as a player does, else by
// Quake's teleport.
[[nodiscard]] bool monsterWalks(edict_t* ent)
{
    return vr_portals_monsters.value > 0.f && !ent->free && NUM_FOR_EDICT(ent) > svs.maxclients &&
        (static_cast<int>(ent->v.flags) & FL_MONSTER) && ent->v.health > 0.f &&
        static_cast<int>(ent->v.solid) == SOLID_SLIDEBOX && static_cast<int>(ent->v.movetype) == MOVETYPE_STEP;
}

// As teleport_touch: a trigger waiting to be fired (a targetname, not IGNORE_TARGETNAME) is shut until then.
[[nodiscard]] bool triggerActive(const edict_t* trig)
{
    if(trig->free || static_cast<int>(trig->v.solid) != SOLID_TRIGGER)
    {
        return false;
    }
    const bool official = vr_campaign.value >= 3.f && vr_campaign.value <= 5.f;
    const int ignoreTargetnameFlag = official ? 4 : 8;
    return !PR_GetString(trig->v.targetname)[0] ||
        (static_cast<int>(trig->v.spawnflags) & ignoreTargetnameFlag) || trig->v.nextthink >= qcvm->time;
}

[[nodiscard]] bool overlaps(const edict_t* a, const edict_t* b)
{
    for(int i = 0; i < 3; i++)
    {
        if(a->v.absmin[i] > b->v.absmax[i] + 1.f || a->v.absmax[i] < b->v.absmin[i] - 1.f)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] glm::vec3 carried(const Side& sd, const glm::vec3& p)
{
    return sd.turn * (p - sd.from) + sd.to;
}

[[nodiscard]] Side reverseSide(const Side& entry)
{
    // One of the built sides: its exit as made in build (the same numbers).
    const uintptr_t at = reinterpret_cast<uintptr_t>(&entry), first = reinterpret_cast<uintptr_t>(sides.data());
    if(exits.size() == sides.size() && at >= first && at < first + sides.size() * sizeof(Side))
    {
        return exits[static_cast<za::SizeT>((at - first) / sizeof(Side))];
    }
    Side exitGate;
    exitGate = entry;
    exitGate.from = entry.to;
    exitGate.to = entry.from;
    exitGate.normal = entry.turn * -entry.normal;
    exitGate.dist = glm::dot(exitGate.normal, entry.to);
    exitGate.turn = glm::transpose(entry.turn);
    exitGate.mins = glm::vec3{1e9f}; exitGate.maxs = glm::vec3{-1e9f};
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 p = carried(entry, glm::vec3{(c&1)?entry.maxs.x:entry.mins.x,
                (c&2)?entry.maxs.y:entry.mins.y, (c&4)?entry.maxs.z:entry.mins.z});
        exitGate.mins = glm::min(exitGate.mins, p); exitGate.maxs = glm::max(exitGate.maxs, p);
    }
    exitGate.yaw = -entry.yaw;
    return exitGate;
}

// (`margin` units of slack: a point on the plane lands a rounding step off a wall or floor gate's flat axis, where
// mins and maxs are equal; 1 for points from a mix along a crossing, as VR_PortalTrace's.)
[[nodiscard]] bool onGate(const Side& sd, const glm::vec3& onPlane, float margin)
{
    return !glm::any(glm::lessThan(onPlane, sd.mins - margin)) && !glm::any(glm::greaterThan(onPlane, sd.maxs + margin));
}

// The player's head (the client's, from its box; else Quake's view).
[[nodiscard]] glm::vec3 headOf(edict_t* ent)
{
    const VrMove* move = server::clientMove(ent);
    const glm::vec3 origin = vec(ent->v.origin);
    return move ? origin + (move->headPos - move->origin) : origin + vec(ent->v.view_ofs);
}

// The player's torso, as his collision box has it: the point on the body's own axis where its middle plane is, the
// plane that halves the box (his feet to the top of his head). A gate takes the player when that plane is through its
// plane: leaning in, reaching with the hands, or brushing the aperture does not cross. The box and not the head, because
// the box is what the world stops: what he collides with and what goes through are then the same thing.
[[nodiscard]] glm::vec3 torsoOf(edict_t* ent)
{
    return vec(ent->v.origin) + glm::vec3{0.f, 0.f, 0.5f * (ent->v.mins[2] + ent->v.maxs[2])};
}

// A recognised gate owns its player trigger even at its frame or below its sill. Waiting there must never bypass
// collision through QuakeC's old teleport. Triggers with no teleporter faces retain Quake's behaviour.
[[nodiscard]] bool engineCarries(int t)
{
    for(const Side& sd : sides)
    {
        if(sd.trigger == t)
        {
            return true;
        }
    }
    return false;
}

// His box swept at the gate's plane: how near that plane his torso's middle plane can be brought (a sill, a ledge or
// bars just past the plane stop the box short). 0 or under: it reaches it and goes through.
[[nodiscard]] float reachOf(edict_t* ent, const Side& sd, const glm::vec3& origin, float d)
{
    if(d <= 0.f)
    {
        return 0.f;
    }
    vec3_t start{origin.x, origin.y, origin.z};
    const glm::vec3 to = origin - sd.normal * (d + 1.f);
    vec3_t end{to.x, to.y, to.z};
    const trace_t tr = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    return tr.startsolid ? d : d - tr.fraction * (d + 1.f);
}

// The effective collision box's footprint on the gate plane must fit inside the opening. A centre inside it is not
// enough: the shoulders may still overlap the frame. Use the same narrowed box as SV_Move, including its hull height.
[[nodiscard]] bool bodyFitsGateAt(edict_t* ent, const Side& sd, const glm::vec3& origin)
{
    vec3_t mins, maxs;
    if(!VR_HullMoveBox(ent, ent->v.mins, ent->v.maxs, mins, maxs))
    {
        VectorCopy(ent->v.mins, mins);
        VectorCopy(ent->v.maxs, maxs);
    }
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 corner = origin + glm::vec3{(c & 1) ? maxs[0] : mins[0],
            (c & 2) ? maxs[1] : mins[1], (c & 4) ? maxs[2] : mins[2]};
        const glm::vec3 projected = corner - sd.normal * (glm::dot(sd.normal, corner) - sd.dist);
        if(!onGate(sd, projected, 0.03125f)) // the hull trace's 1/32-unit contact tolerance
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool bodyFitsGate(edict_t* ent, const Side& sd)
{
    return bodyFitsGateAt(ent, sd, vec(ent->v.origin));
}

// A two-sided sheet: whether `p` is in front of the other face of `sd`'s gate (its side turned the other way).
[[nodiscard]] bool inFrontOfOpposite(const Side& sd, const glm::vec3& p)
{
    for(const Side& o : sides)
    {
        if(&o != &sd && o.trigger == sd.trigger && glm::dot(o.normal, sd.normal) < -0.99f &&
           glm::dot(o.normal, p) - o.dist > 0.f)
        {
            return true;
        }
    }
    return false;
}

void setVec(float* out, const glm::vec3& v)
{
    geom::store(v, out);
}

// Keep the exact transform; the split body trace checks exit clearance without a forward or sideways snap.
// What Quake's teleport does besides moving `ent` (QuakeC's VR_Portal_Crossed: the trigger's targets, what the hands
// carry, the stealth AI's points carried along): where it came from, whether it went back out of the side it was
// leaving (`returnSide`), the gate's yaw, the face it went into and the face in the new room that leads back
// (aiGateFace's numbering; 0 none).
void portalCrossed(edict_t* ent, const Side& sd, edict_t* trig, const glm::vec3& from, int returnSide)
{
    const func_t fn = progs::findFunction("VR_Portal_Crossed");
    if(!fn)
    {
        return;
    }
    const int count = static_cast<int>(sides.size());
    const int index = returnSide >= 0 ? returnSide : static_cast<int>(&sd - sides.data());
    int face = 0, back = 0;
    if(index >= 0 && index < count)
    {
        const int pair = sides[index].paired ? sides[index].pair : -1;
        const int other = pair >= 0 && pair < count ? pair + 1 : 0;
        face = returnSide >= 0 ? other : index + 1;
        back = returnSide >= 0 ? index + 1 : other;
    }
    const int oldSelf = pr_global_struct->self;
    const int oldOther = pr_global_struct->other;
    pr_global_struct->self = EDICT_TO_PROG(trig);
    pr_global_struct->other = EDICT_TO_PROG(ent);
    pr_global_struct->time = qcvm->time;
    setVec(G_VECTOR(OFS_PARM0), from);
    G_FLOAT(OFS_PARM1) = returnSide >= 0 ? 1.f : 0.f;
    G_FLOAT(OFS_PARM2) = sd.yaw;
    G_FLOAT(OFS_PARM3) = static_cast<float>(face);
    G_FLOAT(OFS_PARM4) = static_cast<float>(back);
    PR_ExecuteProgram(fn);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
}

void crossPlayer(edict_t* ent, const Side& sd, edict_t* trig, ClientState& cs, int returnSide)
{
    const glm::vec3 from = vec(ent->v.origin);
    glm::vec3 to = carried(sd, from);
    // Entry collision already tested both body halves. Keep the exact mapping and retain the split at the exit
    // until the trailing half clears it; a backing wall must not force a forward jump after the torso crosses.

    setVec(ent->v.origin, to);
    setVec(ent->v.oldorigin, to);
    setVec(ent->v.velocity, sd.turn * vec(ent->v.velocity));
    const float* head = server::clientHeadAngles(ent);
    const float yaw = (head ? head[YAW] : ent->v.v_angle[YAW]) + sd.yaw;
    ent->v.angles[PITCH] = ent->v.angles[ROLL] = 0.f;
    ent->v.angles[YAW] = anglemod(yaw);
    ent->v.v_angle[YAW] = anglemod(ent->v.v_angle[YAW] + sd.yaw);
    ent->v.fixangle = 1;
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    SV_LinkEdict(ent, true);
    cs.crossed = qcvm->time;
    cs.leavingSide = returnSide >= 0 ? -1 : static_cast<int>(&sd - sides.data());
    Con_DPrintf("VR portal: carried edict %d through side %d: %.0f %.0f %.0f -> %.0f %.0f %.0f\n", NUM_FOR_EDICT(ent),
        returnSide >= 0 ? returnSide : cs.leavingSide, from.x, from.y, from.z, to.x, to.y, to.z);

    // This machine's player (single player, a listen server's host): its client turns the play space by the gate's yaw
    // and keeps the head's lean and the stairs' easing through the jump, so that the view goes on as it was.
    if(cls.state == ca_connected && svs.maxclients >= 1 && ent == svs.clients[0].edict)
    {
        hands::portalCrossing(sd.yaw);
    }

    // What Quake's teleport does besides moving him (QuakeC: its targets, what the hands carry).
    portalCrossed(ent, sd, trig, from, returnSide);
}

// The player's box in the world's solid at `p` (a gate's split body included: SV_Move takes each half in its room).
[[nodiscard]] bool solidAt(edict_t* ent, const glm::vec3& p)
{
    vec3_t o{p.x, p.y, p.z};
    return SV_Move(o, ent->v.mins, ent->v.maxs, o, MOVE_NOMONSTERS, ent).startsolid;
}

// The safety net (vr_portals_unstick): a player found in the wall behind a gate - his box in the world's solid where he
// stands, his torso by a gate's plane (in front of it by less than his box's half depth, or behind it, up to his box's
// depth and a step more) over its aperture - is got out at once: carried on through if his torso is past the plane and
// he fits at the far side (a little further on if the exit's floor or the wall behind it holds him), else put back in
// front of the plane, else carried on after all. Never left in the wall, however he got there.
bool unstick(edict_t* ent, ClientState& cs)
{
    if(vr_portals_unstick.value <= 0.f || static_cast<int>(ent->v.movetype) != MOVETYPE_WALK)
    {
        return false; // (noclip goes through walls: let it)
    }
    const glm::vec3 origin = vec(ent->v.origin), torso = torsoOf(ent), size = vec(ent->v.maxs) - vec(ent->v.mins);
    const Side* best = nullptr;
    float bestD = 0.f, bestHalf = 0.f;
    for(const Side& sd : sides)
    {
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        const float half = 0.5f * glm::dot(glm::abs(sd.normal), size);
        if(d > half || d < -(2.f * half + 32.f) || !onGate(sd, torso - sd.normal * d, 8.f) ||
           (best && za::fabs(d) >= za::fabs(bestD)))
        {
            continue;
        }
        best = &sd;
        bestD = d;
        bestHalf = half;
    }
    if(!best || !solidAt(ent, origin))
    {
        return false;
    }
    const Side& sd = *best;
    const int index = static_cast<int>(&sd - sides.data());
    const glm::vec3 exitNormal = sd.turn * -sd.normal;
    const auto complete = [&]() -> bool {
        const int oldLeaving = cs.leavingSide;
        cs.leavingSide = index; // (its trailing half behind the exit's plane is split as a crossing's is)
        for(float k = 0.f; k <= 2.f * bestHalf + 32.f; k += 2.f)
        {
            const glm::vec3 to = carried(sd, origin) + exitNormal * k;
            if(!solidAt(ent, to))
            {
                crossPlayer(ent, sd, EDICT_NUM(sd.trigger), cs);
                setVec(ent->v.origin, to);
                setVec(ent->v.oldorigin, to);
                SV_LinkEdict(ent, true);
                Con_DPrintf("VR portal: unstuck edict %d: carried on through side %d (%.0f on)\n", NUM_FOR_EDICT(ent), index, k);
                return true;
            }
        }
        cs.leavingSide = oldLeaving;
        return false;
    };
    const auto revert = [&]() -> bool {
        for(float k = za::max(-bestD, 0.f) + 1.f; k <= za::max(-bestD, 0.f) + bestHalf + 32.f; k += 2.f)
        {
            const glm::vec3 to = origin + sd.normal * k;
            if(!solidAt(ent, to))
            {
                setVec(ent->v.origin, to);
                setVec(ent->v.oldorigin, to);
                SV_LinkEdict(ent, true);
                Con_DPrintf("VR portal: unstuck edict %d: back out of side %d (%.0f)\n", NUM_FOR_EDICT(ent), index, k);
                return true;
            }
        }
        return false;
    };
    const bool done = bestD < 0.f ? (complete() || revert()) : (revert() || complete());
    if(done)
    {
        unstuckCount++;
    }
    else
    {
        Con_DPrintf("VR portal: edict %d stuck by side %d (torso %.1f): no way out found\n", NUM_FOR_EDICT(ent), index, bestD);
    }
    return done;
}

// A monster carried through (VR_PortalMonsterCross): where it is, how it moves, where it faces and means to (ideal_yaw)
// turned and shifted by the side's mapping; then what Quake's teleport does besides moving it (VR_Portal_Crossed: the
// trigger's targets). No flash, no telefrag: it walked through.
void crossMonster(edict_t* ent, const Side& sd, edict_t* trig, ClientState& cs, int returnSide)
{
    const glm::vec3 from = vec(ent->v.origin);
    const glm::vec3 to = carried(sd, from);
    setVec(ent->v.origin, to);
    setVec(ent->v.oldorigin, to);
    setVec(ent->v.velocity, sd.turn * vec(ent->v.velocity));
    ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + sd.yaw);
    ent->v.ideal_yaw = anglemod(ent->v.ideal_yaw + sd.yaw);
    SV_LinkEdict(ent, true);
    cs.crossed = qcvm->time;
    cs.leavingSide = returnSide >= 0 ? -1 : static_cast<int>(&sd - sides.data());
    cs.lastTorso = torsoOf(ent);
    Con_DPrintf("VR portal: carried monster %d through side %d: %.1f %.1f %.1f -> %.1f %.1f %.1f\n", NUM_FOR_EDICT(ent),
        returnSide >= 0 ? returnSide : cs.leavingSide, from.x, from.y, from.z, to.x, to.y, to.z);
    portalCrossed(ent, sd, trig, from, returnSide);
}
} // namespace detail
} // namespace qvr::portals

// SV_Move: while straddling a gate, each half of the body collides in its own room. The normal, intact world trace
// remains the fallback outside a fully fitting aperture, and the gate's rim limits lateral motion during entry.
// A monster's too (vr_portals_monsters), at paired gates only (else Quake's teleport takes it, at the trigger).
extern "C" int VR_PortalBodyMove(edict_t* ent, const float* start, const float* mins, const float* maxs,
                                 const float* end, int type, trace_t* trace)
{
    using namespace qvr::portals;
    if(!ent || !walkOn() || (type != MOVE_NORMAL && type != MOVE_NOMONSTERS) || NUM_FOR_EDICT(ent) < 1 ||
       (NUM_FOR_EDICT(ent) > svs.maxclients && !monsterWalks(ent)) || ent->v.health <= 0.f ||
       vec(mins) != vec(ent->v.mins) || vec(maxs) != vec(ent->v.maxs))
    {
        return 0;
    }
    const bool monster = NUM_FOR_EDICT(ent) > svs.maxclients;
    if(!current()) { build(); }
    const glm::vec3 a = vec(start), b = vec(end);
    const glm::vec3 torso = a + glm::vec3{0.f, 0.f, 0.5f * (mins[2] + maxs[2])};
    const Side* gate = nullptr;
    Side exitGate;
    ClientState& state = crossingState(ent);
    if(state.leavingSide >= 0 && state.leavingSide < static_cast<int>(sides.size()))
    {
        const Side& entry = sides[state.leavingSide];
        exitGate = reverseSide(entry);
        vec3_t lo, hi;
        if(!VR_HullMoveBox(ent, mins, maxs, lo, hi))
        {
            VectorCopy(mins, lo);
            VectorCopy(maxs, hi);
        }
        bool clear = true;
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p =
                a + glm::vec3{(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2]};
            clear &= glm::dot(exitGate.normal, p) - exitGate.dist > 0.03125f;
        }
        if(clear || !bodyFitsGateAt(ent, exitGate, a)) { state.leavingSide = -1; }
        else { gate = &exitGate; }
    }
    float nearest = kCross;
    for(const Side& sd : sides)
    {
        if(gate == &exitGate) { break; }
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        if(d < -kCross || za::fabs(d) >= nearest || (monster && !sd.paired) || !triggerActive(EDICT_NUM(sd.trigger)) ||
           !bodyFitsGateAt(ent, sd, a))
        {
            continue;
        }
        // Do not enter a sheet's opposite face while leaving it: the body in front of the sheet's other face (the same
        // gate's side turned the other way) walking into that face. (Any torso behind a plane walking back out of it
        // was taken for that, and a player half through a gate in a wall, not carried yet, could not back out of it:
        // he stood frozen in the wall, ROUND21.md "Teleporters: shooting yourself, stuck behind a gate".)
        if(d < 0.f && glm::dot(sd.normal, b - a) > 0.f && inFrontOfOpposite(sd, torso)) { continue; }
        nearest = za::fabs(d);
        gate = &sd;
    }
    if(!gate) { return 0; }
    const Side& sd = *gate;
    float limit = 1.f;
    if(!bodyFitsGateAt(ent, sd, b))
    {
        float lo = 0.f, hi = 1.f;
        for(int i = 0; i < 24; i++)
        {
            const float mid = (lo + hi) * 0.5f;
            if(bodyFitsGateAt(ent, sd, glm::mix(a, b, mid))) { lo = mid; }
            else { hi = mid; }
        }
        limit = lo;
    }
    const glm::vec3 stop = glm::mix(a, b, limit);
    vec3_t boxLo, boxHi;
    if(!VR_HullMoveBox(ent, mins, maxs, boxLo, boxHi))
    {
        VectorCopy(mins, boxLo);
        VectorCopy(maxs, boxHi);
    }
    vec3_t from{a.x, a.y, a.z}, to{stop.x, stop.y, stop.z};
    float sourcePlane[4]{sd.normal.x, sd.normal.y, sd.normal.z, sd.dist};
    trace_t source = SV_MovePortalHalf(from, boxLo, boxHi, to, type, ent, sourcePlane);
    const glm::vec3 destStart = carried(sd, a), destEnd = carried(sd, stop);
    glm::vec3 destLo{1e9f}, destHi{-1e9f};
    for(int c = 0; c < 8; c++)
    {
        const glm::vec3 offset = sd.turn * glm::vec3{(c & 1) ? boxHi[0] : boxLo[0], (c & 2) ? boxHi[1] : boxLo[1],
                                                     (c & 4) ? boxHi[2] : boxLo[2]};
        destLo = glm::min(destLo, offset);
        destHi = glm::max(destHi, offset);
    }
    const glm::vec3 ahead = sd.turn * -sd.normal;
    float destPlane[4]{ahead.x, ahead.y, ahead.z, glm::dot(ahead, sd.to)};
    vec3_t da{destStart.x, destStart.y, destStart.z}, db{destEnd.x, destEnd.y, destEnd.z};
    vec3_t dlo{destLo.x, destLo.y, destLo.z}, dhi{destHi.x, destHi.y, destHi.z};
    trace_t dest = SV_MovePortalHalf(da, dlo, dhi, db, type, ent, destPlane);
    const bool destHit = dest.fraction < source.fraction;
    *trace = destHit ? dest : source;
    if(destHit)
    {
        const glm::vec3 normal = glm::transpose(sd.turn) * vec(dest.plane.normal);
        setVec(trace->plane.normal, normal);
        trace->plane.dist = dest.plane.dist - glm::dot(vec(dest.plane.normal), sd.to) + glm::dot(normal, sd.from);
    }
    trace->startsolid = source.startsolid || dest.startsolid;
    trace->allsolid = source.allsolid || dest.allsolid;
    trace->fraction *= limit;
    if(limit < 1.f && trace->fraction >= limit && !trace->startsolid)
    {
        const glm::vec3 tangent = (b - a) - sd.normal * glm::dot(sd.normal, b - a);
        if(glm::length(tangent) > 1e-6f) { setVec(trace->plane.normal, -glm::normalize(tangent)); }
        trace->ent = qcvm->edicts;
    }
    setVec(trace->endpos, glm::mix(a, b, trace->fraction));
    return 1;
}

// VR_ClientSpecialMove (SV_Physics_Client, before the move): the player carried through a seamless teleporter (its
// trigger touched, active) once his torso reaches the plane and his collision box fits the aperture.
extern "C" void VR_PortalClientCross(edict_t* ent)
{
    using namespace qvr::portals;
    if(!walkOn())
    {
        return;
    }
    if(!current())
    {
        build();
    }
    const int num = NUM_FOR_EDICT(ent) - 1;
    if(sides.empty() || num < 0 || num >= MAX_SCOREBOARD || ent->v.health <= 0.f)
    {
        return;
    }
    ClientState& cs = crossingState(ent);
    if(cs.leavingSide >= 0 && cs.leavingSide < static_cast<int>(sides.size()))
    {
        const int index = cs.leavingSide;
        const Side exitGate = reverseSide(sides[index]);
        if(glm::dot(exitGate.normal, torsoOf(ent)) < exitGate.dist && bodyFitsGate(ent, exitGate))
        {
            crossPlayer(ent, exitGate, EDICT_NUM(exitGate.trigger), cs, index);
            return;
        }
    }
    if(cs.crossed == qcvm->time || unstick(ent, cs)) // (one crossing a tick; out of a gate's wall)
    {
        return;
    }
    const glm::vec3 torso = torsoOf(ent);
    // The gate his body stands in the middle of: the side whose plane his torso's middle plane is nearest to (a
    // sheet's two sides: the one his body is in, not the one behind it). Its trigger touched and active, as Quake's
    // teleport would need it.
    const Side* best = nullptr;
    float bestD = kCross;
    for(const Side& sd : sides)
    {
        const edict_t* trig = EDICT_NUM(sd.trigger);
        if(!triggerActive(trig) || !overlaps(ent, trig))
        {
            continue;
        }
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        // A thick two-sided sheet has another face behind this one. Being behind that opposite face is not entry:
        // the body must move into this face, rather than away from it or merely jump in front of the sheet.
        if(d > 0.f || za::fabs(d) >= bestD || glm::dot(sd.normal, vec(ent->v.velocity)) >= 0.f ||
           !onGate(sd, torso - sd.normal * d, kAperture) || !bodyFitsGate(ent, sd))
        {
            continue;
        }
        best = &sd;
        bestD = za::fabs(d);
    }
    if(!best)
    {
        return;
    }
    // Collision must let the torso actually reach the plane. A frame or sill stopping the box is never permission
    // to cross; raised openings require a jump into them.
    crossPlayer(ent, *best, EDICT_NUM(best->trigger), cs);
}

namespace qvr::portals
{

// vr_portals_stuck (Debug > Teleporters): whether the local player's box is in the world's solid where he stands (a
// gate's split body included: each half in its own room), the gate side nearest his torso and how far his torso is in
// front of its plane (under 0: behind it), and how many times he was got out of the wall behind a gate.
void stuck_f()
{
    edict_t* ent = progs::playerEdict();
    if(!ent || !enabled())
    {
        Con_Printf("portal stuck: no server, or teleporters off\n");
        return;
    }
    progs::ServerVm serverVm;
    if(!current()) { build(); }
    vec3_t o{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const trace_t tr = SV_Move(o, ent->v.mins, ent->v.maxs, o, MOVE_NOMONSTERS, ent);
    const glm::vec3 torso = torsoOf(ent);
    int nearest = -1;
    float nearestD = 1e9f;
    for(int i = 0; i < static_cast<int>(sides.size()); i++)
    {
        const Side& sd = sides[static_cast<za::SizeT>(i)];
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        if(za::fabs(d) < za::fabs(nearestD) && onGate(sd, torso - sd.normal * d, 16.f)) { nearest = i; nearestD = d; }
    }
    Con_Printf("portal stuck: %d at %.1f %.1f %.1f, side %d torso %.1f, unstuck %d\n", tr.startsolid ? 1 : 0, o[0], o[1],
        o[2], nearest, nearest >= 0 ? nearestD : 0.f, unstuckCount);
}
} // namespace qvr::portals

// CL_RelinkEntities: an entity's last two places far apart (over 100 units on an axis: Quake's teleport, no lerp). The
// client draws a tick behind the server, lerping from the older place to the newer; snapping to the newer at a
// seamless crossing jumped the view a tick ahead and then held it still for a frame, a hitch at every teleporter. When
// the older place, near a gate's plane and over its opening (either way through), carried through it lands within a
// tick's move of the newer, `from` is it carried (and `yaw` the turn): the lerp goes on in the new room.
extern "C" int VR_PortalLerpFrom(const float older[3], const float newer[3], float from[3], float* yaw)
{
    using namespace qvr::portals;
    if(!walkOn() || !current())
    {
        return 0;
    }
    const glm::vec3 o = vec(older), n = vec(newer);
    float best = kStraddle * 2.f;
    for(const Side& entry : sides)
    {
        for(int back = 0; back < 2; back++)
        {
            const Side sd = back ? reverseSide(entry) : entry;
            const float d = glm::dot(sd.normal, o) - sd.dist;
            if(za::fabs(d) >= kStraddle || !onGate(sd, o - sd.normal * d, kStraddle))
            {
                continue;
            }
            const glm::vec3 c = carried(sd, o);
            const float miss = glm::distance(c, n);
            if(miss < best)
            {
                best = miss;
                setVec(from, c);
                *yaw = sd.yaw;
            }
        }
    }
    return best < kStraddle * 2.f ? 1 : 0;
}

namespace qvr::portals
{

glm::vec3 aiMap(int gate, const glm::vec3& value, bool direction)
{
    if(!walkOn() || !vr_portals_ai.value) { return value; }
    if(!current()) { build(); }
    if(gate <= 0 || gate > 2 * static_cast<int>(sides.size())) { return value; }
    const int count = static_cast<int>(sides.size());
    const Side sd = gate <= count ? sides[gate - 1] : reverseSide(sides[gate - count - 1]);
    if(!triggerActive(EDICT_NUM(sd.trigger))) { return value; }
    const Side inverse = reverseSide(sd);
    return direction ? inverse.turn * value : carried(inverse, value);
}

// The stealth AI's gates (QC vr_stealth.qc), numbered as aiImage's: 1..count the sides' own faces, count+1..2 count
// their exits.
int aiGateCount()
{
    if(!walkOn() || !vr_portals_ai.value) { return 0; }
    if(!current()) { build(); }
    return static_cast<int>(sides.size());
}

namespace detail
{

[[nodiscard]] bool aiGateSide(int gate, Side& out)
{
    const int count = aiGateCount();
    if(gate <= 0 || gate > 2 * count) { return false; }
    out = gate <= count ? sides[gate - 1] : reverseSide(sides[gate - count - 1]);
    return true;
}
} // namespace detail

int aiGateFlags(int gate)
{
    Side sd;
    if(!aiGateSide(gate, sd)) { return 0; }
    const int count = static_cast<int>(sides.size());
    const edict_t* trig = EDICT_NUM(sd.trigger);
    if(!triggerActive(trig)) { return 0; }
    int flags = 1;
    if(!sd.paired) { return flags; }
    flags |= 2;
    // The face a monster walks into: this side's own, or (an exit) its pair's, which carries it back.
    const int face = gate <= count ? gate - 1 : sd.pair;
    if(face < 0 || face >= count) { return flags; }
    const edict_t* faceTrig = EDICT_NUM(sides[face].trigger);
    if(vr_portals_monsters.value > 0.f && sides[face].paired && triggerActive(faceTrig) &&
       !(static_cast<int>(faceTrig->v.spawnflags) & 1))
    {
        flags |= 4;
    }
    return flags;
}

int aiGateFace(int gate)
{
    Side sd;
    if(!aiGateSide(gate, sd)) { return 0; }
    const int count = static_cast<int>(sides.size());
    if(gate <= count) { return gate; }
    return sd.paired && sd.pair >= 0 && sd.pair < count ? sd.pair + 1 : 0;
}

glm::vec3 aiGateVec(int gate, int what, const glm::vec3& p)
{
    Side sd;
    if(!aiGateSide(gate, sd)) { return p; }
    switch(what)
    {
        case 0: return (sd.mins + sd.maxs) * 0.5f;
        case 1: return sd.normal;
        case 2: return glm::clamp(p - sd.normal * (glm::dot(sd.normal, p) - sd.dist), sd.mins, sd.maxs);
        default: return carried(sd, p);
    }
}

int aiImage(edict_t* observer, edict_t* target, const glm::vec3& from, const glm::vec3& point,
    int gate, glm::vec3& image)
{
    image = point;
    if(!walkOn() || !vr_portals_ai.value || !target || target->free ||
       !(static_cast<int>(target->v.flags) & FL_CLIENT) || target->v.health <= 0.f) { return 0; }
    if(!current()) { build(); }
    float best = 1e30f;
    int picked = 0;
    for(int i = 0; i < 2 * static_cast<int>(sides.size()); i++)
    {
        if(gate > 0 && gate != i + 1) { continue; }
        const int count = static_cast<int>(sides.size());
        const Side sd = i < count ? sides[i] : reverseSide(sides[i - count]);
        if(!triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1)) { continue; }
        const Side inverse = reverseSide(sd);
        const glm::vec3 candidate = carried(inverse, point);
        const float a = glm::dot(sd.normal, from) - sd.dist, b = glm::dot(sd.normal, candidate) - sd.dist;
        const float distance = glm::distance(from, candidate);
        if(a <= 0.f || b >= 0.f || distance >= best) { continue; }
        const glm::vec3 entry = glm::mix(from, candidate, a / (a - b));
        if(!onGate(sd, entry, 1.f)) { continue; }
        const glm::vec3 exit = carried(sd, entry);
        const glm::vec3 ray = point - exit;
        if(glm::length(ray) < 0.01f) { continue; }
        const glm::vec3 farStart = exit + glm::normalize(ray) * 0.05f;
        // Ordinary traces in each room: they cannot accidentally recurse through another gate.
        const trace_t nearTrace = worldtrace::serverLine(from, entry, observer, MOVE_NORMAL);
        if(nearTrace.startsolid || nearTrace.fraction < 0.999f) { continue; }
        const trace_t farTrace = worldtrace::serverLine(farStart, point, observer, MOVE_NORMAL);
        if(farTrace.startsolid || (farTrace.fraction < 0.999f && farTrace.ent != target)) { continue; }
        // *teleport shimmer reports water at the entry plane; it is not a water boundary.
        // Attack traces keep their ordinary contents checks in the reached room.
        image = candidate; best = distance; picked = i + 1;
    }
    return picked;
}

void pullSearchOrigins(const glm::vec3& from, za::Vector<glm::vec3>& out, float range)
{
    out.clear();
    out.pushBack(from);
    if(!walkOn()) { return; }
    if(!current()) { build(); }
    for(const Side& sd : sides)
    {
        if(!triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1) ||
           glm::dot(sd.normal, from) - sd.dist < 0.f) { continue; }
        // A gate whose aperture is out of reach takes nothing to the hand (pullImage's line from the hand crosses the
        // aperture within the range): its room is not searched (range < 0: every gate).
        if(range >= 0.f && glm::distance(nearestPoint(sd, from), from) > range + 1.f) { continue; }
        out.pushBack(carried(sd, from));
    }
}

glm::vec3 pullImage(const glm::vec3& from, const glm::vec3& point, int* gate)
{
    if(gate) { *gate = 0; }
    if(!walkOn()) { return point; }
    if(!current()) { build(); }
    glm::vec3 result = point;
    float best = 1e30f;
    for(int i = 0; i < static_cast<int>(sides.size()); ++i)
    {
        const Side& sd = sides[i];
        if(!triggerActive(EDICT_NUM(sd.trigger)) || (static_cast<int>(EDICT_NUM(sd.trigger)->v.spawnflags) & 1)) { continue; }
        // Only the inverse point transform is needed here, not reverseSide's eight-corner aperture bounds.
        const glm::vec3 image = glm::transpose(sd.turn) * (point - sd.to) + sd.from;
        if(vr_prop_query_verify.value && image != carried(reverseSide(sd), point))
        {
            Sys_Error("portal inverse point transform differs at gate %d", i);
        }
        const float d0 = glm::dot(sd.normal, from) - sd.dist, d1 = glm::dot(sd.normal, image) - sd.dist;
        const float distance = glm::distance(from, image);
        if(d0 < 0.f || d1 >= 0.f || distance >= best || distance > za::max(1.f, vr_forcegrab_distance.value) * 1.25f) { continue; }
        const glm::vec3 entry = glm::mix(from, image, d0 / (d0 - d1));
        if(!onGate(sd, entry, 1.f)) { continue; }
        const glm::vec3 exit = carried(sd, entry) + glm::normalize(point - carried(sd, entry)) * 0.5f;
        const trace_t first = worldtrace::serverLine(from, entry);
        if(first.startsolid || first.fraction < 0.999f) { continue; }
        const trace_t second = worldtrace::serverLine(exit, point);
        if(second.startsolid || second.fraction < 0.999f) { continue; }
        result = image;
        best = distance;
        if(gate) { *gate = i + 1; }
    }
    return result;
}

glm::vec3 pullImageSeen(const glm::vec3& from, const glm::vec3& point)
{
    if(!walkOn()) { return point; }
    progs::ServerVm serverVm;
    const glm::vec3 image = pullImage(from, point);
    return image;
}
} // namespace qvr::portals

// A force grab homes in the destination room until it crosses the exit aperture,
// then maps its position, angles and velocities back to the player's room.
extern "C" void VR_PortalPullTarget(edict_t* ent, const float hand[3], int begin, float out[3])
{
    using namespace qvr::portals;
    const int field = qvr::progs::fields().fg_portal;
    setVec(out, vec(hand));
    if(field < 0) { return; }
    eval_t* state = GetEdictFieldValue(ent, field);
    if(begin)
    {
        int gate = 0;
        pullImage(vec(hand), physics::modelCentre(ent), &gate);
        state->_float = static_cast<float>(gate);
    }
    const int index = static_cast<int>(state->_float) - 1;
    if(index < 0) { return; }
    if(!walkOn() || index >= static_cast<int>(sides.size())) { state->_float = -1.f; return; }
    const Side& sd = sides[index];
    if(!triggerActive(EDICT_NUM(sd.trigger))) { state->_float = -1.f; return; }
    const Side exit = reverseSide(sd);
    const glm::vec3 middle = physics::modelCentre(ent);
    const float distance = glm::dot(exit.normal, middle) - exit.dist;
    if(distance <= 0.f)
    {
        // A moving hand can pull the trajectory outside the aperture. Drop
        // the object where it is rather than catching it through a wall.
        if(!onGate(exit, middle - exit.normal * distance, 2.f)) { state->_float = -1.f; return; }
        setVec(ent->v.origin, carried(exit, vec(ent->v.origin)));
        setVec(ent->v.oldorigin, vec(ent->v.origin));
        setVec(ent->v.velocity, exit.turn * vec(ent->v.velocity));
        ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + exit.yaw);
        state->_float = 0.f;
        SV_LinkEdict(ent, false);
        Con_DPrintf("force grab: crossed teleporter %d\n", index + 1);
        return;
    }
    setVec(out, carried(sd, vec(hand)));
}

namespace qvr::portals
{

int aiRoute(edict_t* entity)
{
    if(!entity || !vr_portals_ai.value) { return 0; }
    const int offset = progs::fields().vr_ai_gate;
    return offset >= 0 ? static_cast<int>(GetEdictFieldValue(entity, offset)->_float) : 0;
}
} // namespace qvr::portals

// SV_Physics_Toss, before the move: what flies (missiles, grenades, gibs) carried through a seamless teleporter as this
// frame's path crosses its plane over the gate (from the front, nothing in the way), its speed and heading turned with it.
extern "C" void VR_PortalToss(edict_t* ent)
{
    using namespace qvr::portals;
    if(!walkOn())
    {
        return;
    }
    if(!current())
    {
        build();
    }
    // Its middle's path (a box's origin is at its foot: one sliding on a floor under a gate's sill is in its gate): a
    // model's drawn middle (a rigid body's box is its model's, not its Quake box: physics::modelCentre), else its box's.
    const glm::vec3 mid = physics::modelCentre(ent) - vec(ent->v.origin);
    const glm::vec3 o = vec(ent->v.origin) + mid, v = vec(ent->v.velocity);
    const bool debug = vr_portals_debug_split.value != 0.f;
    if(debug)
    {
        Con_Printf("toss %d: middle %.1f %.1f %.1f velocity %.1f %.1f %.1f\n", NUM_FOR_EDICT(ent), o.x, o.y, o.z, v.x, v.y, v.z);
    }
    if(sides.empty() || glm::dot(v, v) < 1.f)
    {
        return;
    }
    const glm::vec3 e = o + v * static_cast<float>(host_frametime);
    const int count = static_cast<int>(sides.size());
    const int route = aiRoute(ent);
    for(int i = -1; i < count; i++)
    {
        if(i < 0 && (route <= count || route > 2 * count)) { continue; }
        const Side sd = i < 0 ? reverseSide(sides[route - count - 1]) : sides[i];
        const float d0 = glm::dot(sd.normal, o) - sd.dist, d1 = glm::dot(sd.normal, e) - sd.dist;
        // Already past the plane, still straddling it and moving in (a frame it found no room in the destination, or
        // pushed in slowly by Box3D, whose copy at the destination lets it past the wall behind: else it went on
        // into that wall, uncarried): carried now.
        const float halfDepth = 0.5f * glm::dot(glm::abs(sd.normal), vec(ent->v.maxs) - vec(ent->v.mins));
        const bool behind = d0 < 0.f && d0 > -halfDepth && glm::dot(sd.normal, v) < 0.f;
        if(!behind && (d0 < 0.f || d1 >= 0.f))
        {
            continue;
        }
        const glm::vec3 c = behind ? o - sd.normal * d0 : o + (e - o) * (d0 / (d0 - d1));
        edict_t* trig = EDICT_NUM(sd.trigger);
        if(!onGate(sd, c, 2.f) || !triggerActive(trig) || (static_cast<int>(trig->v.spawnflags) & 1)) // PLAYER_ONLY
        {
            continue;
        }
        // Its middle's way to the plane clear (its box, wider than the gate's brush, would reach the wall behind it).
        const trace_t tr = behind ? trace_t{} : worldtrace::serverLine(o, c, ent);
        if(!behind && (tr.startsolid || tr.fraction < 1.f))
        {
            if(debug) { Con_Printf("toss %d: blocked on the way (%.2f)\n", NUM_FOR_EDICT(ent), tr.fraction); }
            continue; // it hits something on the way
        }
        // Its middle carried through (its box's offset kept: it is axis-aligned), a little further on where it does not fit.
        const glm::vec3 dir = sd.turn * v;
        glm::vec3 to = carried(sd, vec(ent->v.origin));
        // A small thing (a nail, a rocket: its box shallower than its middle's way to the plane) would come out wholly
        // behind the exit's plane, inside the wall there, and strike it: its middle put just out of the plane instead
        // (where its path crossed, carried), the frame's move going on from there.
        if(!behind && halfDepth < d0)
        {
            const Side out = reverseSide(sd);
            to = carried(sd, c) + out.normal * (halfDepth + 0.5f) - sd.turn * mid;
        }
        // Keep the straddling placement. Only its destination half needs to
        // clear the level; the trailing half belongs to the original room.
        const Side exit = reverseSide(sd);
        const float plane[4]{exit.normal.x, exit.normal.y, exit.normal.z, exit.dist};
        vec3_t at{to.x, to.y, to.z};
        // Its box turned with it (a gate turning 90 degrees swaps a long box's sides), 4 units in from each face: a prop
        // sliding on the floor rests on it (its Quake box a little round its model, under the floor), not a wall in the way.
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p = sd.turn * glm::vec3{(c & 1) ? ent->v.maxs[0] : ent->v.mins[0],
                (c & 2) ? ent->v.maxs[1] : ent->v.mins[1], (c & 4) ? ent->v.maxs[2] : ent->v.mins[2]};
            lo = glm::min(lo, p); hi = glm::max(hi, p);
        }
        const glm::vec3 inset = glm::min(glm::vec3{4.f}, (hi - lo) * 0.25f); // (a Quake box a little round its model)
        lo += inset; hi -= inset;
        vec3_t boxLo{lo.x, lo.y, lo.z}, boxHi{hi.x, hi.y, hi.z};
        const trace_t placement = SV_MovePortalHalf(at, boxLo, boxHi, at, MOVE_NOMONSTERS, ent, plane);
        if(placement.startsolid || placement.allsolid)
        {
            if(debug)
            {
                Con_Printf("toss %d: no room at %.1f %.1f %.1f (box %.1f %.1f %.1f .. %.1f %.1f %.1f, %s)\n", NUM_FOR_EDICT(ent),
                    to.x, to.y, to.z, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z, placement.allsolid ? "all solid" : "start solid");
            }
            continue;
        }
        setVec(ent->v.origin, to);
        setVec(ent->v.oldorigin, to);
        setVec(ent->v.velocity, dir);
        ent->v.angles[YAW] = anglemod(ent->v.angles[YAW] + sd.yaw);
        const int spinField = progs::fields().vr_spin; // a rigid body's spin (rad/s, the world's axes)
        if(spinField >= 0)
        {
            eval_t* spin = GetEdictFieldValue(ent, spinField);
            setVec(spin->vector, sd.turn * vec(spin->vector));
        }
        const int gateField = progs::fields().vr_ai_gate;
        if(gateField >= 0) { GetEdictFieldValue(ent, gateField)->_float = 0.f; }
        // Come out of a gate, it meets its owner (VR_PortalHitsOwner): your nail at your own image hits you.
        if(progs::fields().vr_gate_crossed >= 0) { progs::fieldFloat(ent, progs::fields().vr_gate_crossed) = za::max(static_cast<float>(qcvm->time), 0.001f); }
        SV_LinkEdict(ent, false);
        return;
    }
}

// QuakeC (portal_handles, teleport_touch): 1 if the engine carries this player through the trigger's teleporter
// (vr_portals_walk), so that Quake's teleport leaves him be until the body enters the opening.
extern "C" float VR_PortalHandles(edict_t* trig, edict_t* who)
{
    using namespace qvr::portals;
    const int num = NUM_FOR_EDICT(who) - 1;
    if(!walkOn() || num < 0 || (num >= svs.maxclients && !monsterWalks(who)) || (num < svs.maxclients && num >= MAX_SCOREBOARD))
    {
        return 0.f;
    }
    if(!current())
    {
        build();
    }
    if(num < svs.maxclients)
    {
        return engineCarries(NUM_FOR_EDICT(trig)) ? 1.f : 0.f;
    }
    // A monster (vr_portals_monsters): a paired gate's (it comes out of the other gate's face) that its box goes into as
    // it stands, or a step up (a sill), or already straddling; else Quake's teleport takes it (a shambler at a player's
    // gate, one beside the opening touching the trigger: never stuck at the frame).
    const int t = NUM_FOR_EDICT(trig);
    const glm::vec3 o = vec(who->v.origin);
    for(const Side& sd : sides)
    {
        if(sd.trigger != t || !sd.paired) { continue; }
        const float d = glm::dot(sd.normal, torsoOf(who)) - sd.dist;
        float low = 1e9f, high = -1e9f;
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p = o + glm::vec3{(c & 1) ? who->v.maxs[0] : who->v.mins[0],
                (c & 2) ? who->v.maxs[1] : who->v.mins[1], (c & 4) ? who->v.maxs[2] : who->v.mins[2]};
            const float pd = glm::dot(sd.normal, p) - sd.dist;
            low = za::min(low, pd); high = za::max(high, pd);
        }
        const bool straddling = low < 0.f && high > 0.f && d > -kCross;
        if(straddling || bodyFitsGateAt(who, sd, o) || bodyFitsGateAt(who, sd, o + glm::vec3{0.f, 0.f, 18.f}))
        {
            return 1.f;
        }
    }
    return 0.f;
}

// SV_Physics_Step, after its think (its walk, a leap): a monster (vr_portals_monsters) carried through a paired gate as
// its torso's middle plane goes in through the plane over the aperture, its box fitting it (as a player is:
// VR_PortalClientCross), kept where it is and how it moves and faces relative to the gate. Stepping back out through
// the exit before its trailing half cleared it carries it back.
extern "C" void VR_PortalMonsterCross(edict_t* ent)
{
    using namespace qvr::portals;
    if(!walkOn() || !monsterWalks(ent))
    {
        return;
    }
    if(!current())
    {
        build();
    }
    if(sides.empty())
    {
        return;
    }
    ClientState& cs = crossingState(ent);
    const glm::vec3 torso = torsoOf(ent);
    const glm::vec3 was = cs.torsoKnown ? cs.lastTorso : torso;
    cs.lastTorso = torso;
    cs.torsoKnown = true;
    const glm::vec3 moved = torso - was;
    if(cs.leavingSide >= 0 && cs.leavingSide < static_cast<int>(sides.size()))
    {
        const int index = cs.leavingSide;
        const Side exitGate = reverseSide(sides[index]);
        if(glm::dot(exitGate.normal, torso) < exitGate.dist && glm::dot(exitGate.normal, moved) < 0.f &&
            bodyFitsGate(ent, exitGate))
        {
            crossMonster(ent, exitGate, EDICT_NUM(exitGate.trigger), cs, index);
            return;
        }
    }
    if(cs.crossed == qcvm->time) // (one crossing a tick)
    {
        return;
    }
    const Side* best = nullptr;
    float bestD = kCross;
    for(const Side& sd : sides)
    {
        const edict_t* trig = EDICT_NUM(sd.trigger);
        if(!sd.paired || !triggerActive(trig) || !overlaps(ent, trig))
        {
            continue;
        }
        const float d = glm::dot(sd.normal, torso) - sd.dist;
        if(d > 0.f || za::fabs(d) >= bestD || glm::dot(sd.normal, moved) >= 0.f ||
           !onGate(sd, torso - sd.normal * d, kAperture) || !bodyFitsGate(ent, sd))
        {
            continue;
        }
        best = &sd;
        bestD = za::fabs(d);
    }
    if(best)
    {
        crossMonster(ent, *best, EDICT_NUM(best->trigger), cs);
    }
}
