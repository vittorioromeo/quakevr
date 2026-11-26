// vr_navshaft.cpp -- lift shafts kept clear (vr_navshaft.hpp; docs/vr-port/NAVMESH.md, section 24).

#include "vr_navshaft.hpp"

#include "vr_geom.hpp"
#include "vr_progs.hpp"
#include "vr_tunables.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::navshaft
{
namespace
{

constexpr float stepSize = 18.f;  // Quake's step (sv_move.c STEPSIZE)
constexpr float exitPast = 8.f;   // an escape or a cleared point goes this far past the margin
constexpr int maxCorners = 64;    // a train's path corners looked at

// The map's shafts (afterLoad).
za::Vector<Shaft> mapShafts; // seat-shared: the one map loaded

float margin()
{
    return za::max(vr_shaft_margin.value, 0.f);
}

edict_t* findByTargetname(const char* name)
{
    edict_t* found = nullptr;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(!found && e->v.targetname && !ZA_STRCMP(PR_GetString(e->v.targetname), name))
        {
            found = e;
        }
    });
    return found;
}

// A mover's shaft from its stops' absmin z (lowest, highest) when it moves straight up and down farther than its own
// height and a step: there is room under it at its top.
// `lo`: its footprint's low corner (x, y) at its stops.
void addIf(edict_t* e, const glm::vec2& lo, float zlo, float ztop, float spread)
{
    const float height = e->v.maxs[2] - e->v.mins[2];
    if(spread > 1.f || ztop - zlo <= height + stepSize)
    {
        return;
    }
    Shaft s;
    s.mover = NUM_FOR_EDICT(e);
    s.lo = lo;
    s.hi = lo + glm::vec2{e->v.maxs[0] - e->v.mins[0], e->v.maxs[1] - e->v.mins[1]};
    s.zlo = zlo;
    s.ztop = ztop;
    mapShafts.pushBack(s);
}

// The box's shortest way out of the shaft's widened footprint across (how deep it is in it).
float depthIn(const Shaft& s, const glm::vec3& lo, const glm::vec3& hi, float m)
{
    const float px = za::min(hi.x - (s.lo.x - m), (s.hi.x + m) - lo.x);
    const float py = za::min(hi.y - (s.lo.y - m), (s.hi.y + m) - lo.y);
    return za::min(px, py);
}

const edict_t* moverOf(const Shaft& s)
{
    const edict_t* m = progs::edictAt(s.mover);
    return m && !m->free && static_cast<int>(m->v.solid) == SOLID_BSP ? m : nullptr;
}

bool entityIn(const Shaft& s, const edict_t* e, float m)
{
    return e && !e->free && boxIn(s, geom::vec(e->v.absmin), geom::vec(e->v.absmax), m);
}

// A reason for the monster to be in the shaft: its enemy there (fighting it), or its goal entity (an item it goes to
// pick up, a path corner of its own; not a bare point such as the stealth AI's marker).
// `goal`: a bot's own goal (its QC's target1, waypoints left out by the QC); nullptr: the monster's goalentity.
bool hasReason(const edict_t* ent, const Shaft& s, const edict_t* goal = nullptr)
{
    const float m = margin();
    const edict_t* world = progs::edictAt(0);
    const edict_t* enemy = PROG_TO_EDICT(ent->v.enemy);
    if(enemy != world && enemy != ent && enemy->v.health > 0.f && entityIn(s, enemy, m))
    {
        return true;
    }
    // (a thing: an item, a path corner; a bare point the AI walks to, as the stealth AI's marker, is never a reason)
    if(!goal)
    {
        goal = PROG_TO_EDICT(ent->v.goalentity);
    }
    return goal != world && goal != ent && goal != enemy && static_cast<int>(goal->v.solid) != SOLID_NOT && entityIn(s, goal, m);
}

// (anyone alive: the monsters' callers check FL_MONSTER, the bots' builtin is the bots' QC's)
bool appliesTo(const edict_t* ent)
{
    return vr_shaft.value != 0.f && ent && !ent->free && ent->v.health > 0.f && !mapShafts.empty();
}

bool applies(const edict_t* ent)
{
    return appliesTo(ent) && (static_cast<int>(ent->v.flags) & FL_MONSTER);
}

// The sides out of the shaft (+x, -x, +y, -y) for a box at `org`, nearest first: each one's direction and the way to go.
struct Exit
{
    glm::vec3 dir;
    float dist;
};
int exitsOf(const Shaft& s, const glm::vec3& lo, const glm::vec3& hi, Exit* out)
{
    const float m = margin();
    out[0] = {{1.f, 0.f, 0.f}, (s.hi.x + m) - lo.x};
    out[1] = {{-1.f, 0.f, 0.f}, hi.x - (s.lo.x - m)};
    out[2] = {{0.f, 1.f, 0.f}, (s.hi.y + m) - lo.y};
    out[3] = {{0.f, -1.f, 0.f}, hi.y - (s.lo.y - m)};
    for(int i = 1; i < 4; ++i) // (insertion sort, 4 items)
    {
        for(int k = i; k > 0 && out[k].dist < out[k - 1].dist; --k)
        {
            const Exit t = out[k];
            out[k] = out[k - 1];
            out[k - 1] = t;
        }
    }
    return 4;
}

// The way from `org` to `to` clear for the box (a step's height up, as walking goes: the world and brush entities).
bool clearWay(const edict_t* ent, const glm::vec3& org, const glm::vec3& to)
{
    vec3_t a{org.x, org.y, org.z + stepSize}, b{to.x, to.y, to.z + stepSize};
    vec3_t mins, maxs;
    VectorCopy(ent->v.mins, mins);
    VectorCopy(ent->v.maxs, maxs);
    const trace_t tr = SV_Move(a, mins, maxs, b, MOVE_NOMONSTERS, const_cast<edict_t*>(ent));
    return !tr.allsolid && !tr.startsolid && tr.fraction >= 1.f;
}

} // namespace

void afterLoad()
{
    mapShafts.clear();
    if(!sv.active)
    {
        return;
    }
    const int pos1 = progs::fields().pos1, pos2 = progs::fields().pos2;
    progs::forEachEdict(1, [&](edict_t* e) {
        if(static_cast<int>(e->v.solid) != SOLID_BSP || static_cast<int>(e->v.movetype) != MOVETYPE_PUSH || !e->v.model ||
            PR_GetString(e->v.model)[0] != '*')
        {
            return;
        }
        const char* klass = e->v.classname ? PR_GetString(e->v.classname) : "";
        if(!ZA_STRCMP(klass, "train") || !ZA_STRCMP(klass, "func_train"))
        {
            // (a train's stops: its path corners; Quake's train_next puts its absmin at each)
            float zlo = 1e30f, ztop = -1e30f, spread = 0.f;
            glm::vec2 first{0.f};
            const edict_t* start = nullptr;
            const char* name = e->v.target ? PR_GetString(e->v.target) : nullptr;
            int n = 0;
            for(; n < maxCorners && name && name[0]; ++n)
            {
                edict_t* corner = findByTargetname(name);
                if(!corner || corner == start)
                {
                    break;
                }
                const glm::vec3 c = geom::vec(corner->v.origin);
                if(!start)
                {
                    start = corner;
                    first = {c.x, c.y};
                }
                spread = za::max(spread, glm::length(glm::vec2{c.x, c.y} - first));
                zlo = za::min(zlo, c.z);
                ztop = za::max(ztop, c.z);
                name = corner->v.target ? PR_GetString(corner->v.target) : nullptr;
            }
            if(n >= 2)
            {
                addIf(e, first, zlo, ztop, spread);
            }
            return;
        }
        if(pos1 < 0 || pos2 < 0)
        {
            return;
        }
        // (a plat, a door, a mod's mover built on them: its stops are pos1 and pos2, its origin at each)
        const glm::vec3 a = progs::fieldVec(e, pos1), b = progs::fieldVec(e, pos2);
        const float spread = glm::length(glm::vec2{a.x - b.x, a.y - b.y});
        addIf(e, glm::vec2{a.x + e->v.mins[0], a.y + e->v.mins[1]}, za::min(a.z, b.z) + e->v.mins[2],
            za::max(a.z, b.z) + e->v.mins[2], spread);
    });
    if(!mapShafts.empty())
    {
        Con_DPrintf("navshaft: %d lift shafts\n", static_cast<int>(mapShafts.size()));
    }
}

const za::Vector<Shaft>& shafts()
{
    return mapShafts;
}

float liveTop(const Shaft& s)
{
    const edict_t* m = moverOf(s);
    return m ? m->v.absmin[2] : -1e30f;
}

bool boxIn(const Shaft& s, const glm::vec3& lo, const glm::vec3& hi, float m)
{
    return boxInPart(s, lo, hi, m, false);
}

bool boxInPart(const Shaft& s, const glm::vec3& lo, const glm::vec3& hi, float m, bool whole)
{
    const edict_t* mover = moverOf(s);
    const float top = !mover ? -1e30f : whole ? s.ztop + mover->v.maxs[2] - mover->v.mins[2] + 4.f : mover->v.absmin[2];
    if(top < s.zlo + 2.f)
    {
        return false; // (the lift down: nothing under it)
    }
    return lo.x < s.hi.x + m && hi.x > s.lo.x - m && lo.y < s.hi.y + m && hi.y > s.lo.y - m && lo.z < top - 2.f &&
           hi.z > s.zlo;
}

namespace
{

// The step from `from` to `to` takes the box into a live shaft, or deeper into one, where it has no reason to be.
bool refusedFor(const edict_t* ent, const glm::vec3& from, const glm::vec3& to, const edict_t* goal)
{
    const float m = margin();
    const glm::vec3 mins = geom::vec(ent->v.mins), maxs = geom::vec(ent->v.maxs);
    for(const Shaft& s : mapShafts)
    {
        if(!boxIn(s, to + mins, to + maxs, m) || hasReason(ent, s, goal))
        {
            continue;
        }
        if(!boxIn(s, from + mins, from + maxs, m))
        {
            return true; // (into it)
        }
        if(depthIn(s, to + mins, to + maxs, m) > depthIn(s, from + mins, from + maxs, m) + 0.01f)
        {
            return true; // (deeper)
        }
    }
    return false;
}

// The yaw out of the live shaft the box is in (-1: none, or a reason there unless `force`).
float escapeFor(const edict_t* ent, bool force, const edict_t* goal)
{
    const float m = margin();
    const glm::vec3 org = geom::vec(ent->v.origin);
    const glm::vec3 lo = geom::vec(ent->v.absmin), hi = geom::vec(ent->v.absmax);
    for(const Shaft& s : mapShafts)
    {
        // (one that blocked a lift: the whole of the way it moves, it may be on the lift's edge or over it)
        if(!boxInPart(s, lo, hi, m, force) || (!force && hasReason(ent, s, goal)))
        {
            continue;
        }
        Exit exits[4];
        const int n = exitsOf(s, lo, hi, exits);
        for(int i = 0; i < n; ++i)
        {
            if(clearWay(ent, org, org + exits[i].dir * (exits[i].dist + exitPast)))
            {
                return geom::yawOf(exits[i].dir) < 0.f ? geom::yawOf(exits[i].dir) + 360.f : geom::yawOf(exits[i].dir);
            }
        }
        const float yaw = geom::yawOf(exits[0].dir); // (none clear all the way: the nearest; the QC tries either side)
        return yaw < 0.f ? yaw + 360.f : yaw;
    }
    return -1.f;
}

} // namespace

bool stepRefused(const edict_t* ent, const glm::vec3& from, const glm::vec3& to)
{
    return applies(ent) && refusedFor(ent, from, to, nullptr);
}

float escapeYaw(const edict_t* ent, bool force)
{
    return applies(ent) ? escapeFor(ent, force, nullptr) : -1.f;
}

float moveOutYaw(const edict_t* ent, bool force)
{
    if(!applies(ent) || !(static_cast<int>(ent->v.flags) & FL_ONGROUND))
    {
        return -1.f;
    }
    return escapeFor(ent, force, nullptr);
}

float botStep(const edict_t* bot, const edict_t* goal, const glm::vec3& wish, bool force)
{
    if(!appliesTo(bot)) // (on the ground or not: a bot wading in water is often off it)
    {
        return -1.f;
    }
    const float out = escapeFor(bot, force, goal);
    if(out >= 0.f)
    {
        return out;
    }
    const glm::vec2 w{wish.x, wish.y};
    if(glm::length(w) < 0.01f)
    {
        return -1.f;
    }
    // (a probe farther than a step: a running bot slides on a few frames once its keys are let go, and its AI may think
    // only each 0.05 s: 96 units at a run)
    const float speed = glm::length(glm::vec2{bot->v.velocity[0], bot->v.velocity[1]});
    const glm::vec3 org = geom::vec(bot->v.origin);
    const glm::vec3 to = org + glm::vec3{glm::normalize(w), 0.f} * (16.f + speed * 0.25f);
    return refusedFor(bot, org, to, goal) ? -2.f : -1.f;
}

glm::vec3 clearPoint(const edict_t* ent, const glm::vec3& at)
{
    if(!applies(ent))
    {
        return at;
    }
    const float m = margin();
    const glm::vec3 mins = geom::vec(ent->v.mins), maxs = geom::vec(ent->v.maxs);
    glm::vec3 p = at;
    for(const Shaft& s : mapShafts)
    {
        if(!boxIn(s, p + mins, p + maxs, m))
        {
            continue;
        }
        Exit exits[4];
        const int n = exitsOf(s, p + mins, p + maxs, exits);
        // (traced from the shaft's middle, which the lift goes through: a point by its edge may have the box in a wall)
        const glm::vec3 mid{(s.lo + s.hi) * 0.5f, p.z};
        for(int i = 0; i < n; ++i)
        {
            const glm::vec3 to = p + exits[i].dir * (exits[i].dist + exitPast);
            if(clearWay(ent, mid, to))
            {
                p = to;
                break;
            }
        }
    }
    return p;
}

int blocks(const edict_t* ent, const glm::vec3& to)
{
    if(!applies(ent))
    {
        return -1;
    }
    const float m = margin();
    const glm::vec3 mins = geom::vec(ent->v.mins), maxs = geom::vec(ent->v.maxs);
    for(za::SizeT i = 0; i < mapShafts.size(); ++i)
    {
        if(boxIn(mapShafts[i], to + mins, to + maxs, m) && !hasReason(ent, mapShafts[i]))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace qvr::navshaft

// SV_movestep (sv_move.c): a monster's step into a live lift shaft where it has no reason to be is refused.
extern "C" int VR_MonsterShaftStep(edict_t* ent, const float* from, const float* to)
{
    return qvr::navshaft::stepRefused(ent, glm::vec3{from[0], from[1], from[2]}, glm::vec3{to[0], to[1], to[2]}) ? 1 : 0;
}

// SV_MoveToGoal (sv_move.c): the yaw a monster on the ground walks out of a live shaft by (no reason there, or it blocked
// the lift a moment ago: QC .vr_shaft_flee), -1: none. Every mod's monsters that walk by movetogoal (AD's, Copper's, the
// mission packs') leave a shaft as the QC's VR_Shaft_Leave makes id1's.
extern "C" float VR_MonsterShaftOut(edict_t* ent)
{
    const bool flee = qvr::progs::fieldFloatOr(ent, qvr::progs::fields().vr_shaft_flee, 0.f) > static_cast<float>(qcvm->time);
    return qvr::navshaft::moveOutYaw(ent, flee);
}
