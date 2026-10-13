// vr_bodycontact.cpp -- the player's body against monsters': see vr_bodycontact.hpp.

#include "vr_bodycontact.hpp"

#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_foegrab.hpp"
#include "vr_hull.hpp"

#include "Zancle/Base/Limits.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

using namespace qvr;

namespace
{

constexpr float maxInset = 12.f;   // units: vr_body_monster_inset's most
constexpr float minHalf = 2.f;     // units: a box's least half width once inset
constexpr float pushSpeed = 80.f;  // units/s: an overlapping pair pushed apart this fast (serverFrame)
constexpr float deeperSlack = 0.01f; // units: a move this much deeper in still counts as not deeper (rounding)
constexpr float probeStep = 0.25f; // units along a move where its way in is told (letsOut)
constexpr float nearReach = 64.f;  // units: monsters this far out of a player's box are not looked at (serverFrame)

// The client number (1..maxclients) of a player's edict on the server; 0 not one.
[[nodiscard]] int clientOf(const edict_t* ent)
{
    if(!ent || !sv.active || qcvm != &sv.qcvm || !qcvm->edicts || qcvm->edict_size <= 0)
    {
        return 0;
    }
    const int num = static_cast<int>((reinterpret_cast<const byte*>(ent) - reinterpret_cast<const byte*>(qcvm->edicts)) /
                                     qcvm->edict_size);
    return num >= 1 && num <= svs.maxclients ? num : 0;
}

// A monster's body (not a corpse lying touchable).
[[nodiscard]] bool isMonster(const edict_t* ent)
{
    const int solid = ent ? static_cast<int>(ent->v.solid) : SOLID_NOT;
    return ent && (static_cast<int>(ent->v.flags) & FL_MONSTER) && solid != SOLID_NOT &&
           solid != SOLID_NOT_BUT_TOUCHABLE && solid != SOLID_TRIGGER;
}

// The box is the entity's own (not a box QC traces with).
[[nodiscard]] bool ownBox(const edict_t* ent, const float* mins, const float* maxs)
{
    for(int i = 0; i < 3; ++i)
    {
        if(mins[i] != ent->v.mins[i] || maxs[i] != ent->v.maxs[i])
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] float insetSetting()
{
    return za::clamp(vr_body_monster_inset.value, 0.f, maxInset);
}

[[nodiscard]] bool stepInOn()
{
    return vr_foegrab_step_in.value != 0.f;
}

[[nodiscard]] bool anyOn()
{
    return insetSetting() > 0.f || stepInOn();
}

// A body's move (mover's own box mins, maxs) against touch: a player's and a monster's, either way.
[[nodiscard]] bool isPair(const edict_t* mover, const edict_t* touch, const float* mins, const float* maxs)
{
    if(!mover || !touch || !ownBox(mover, mins, maxs))
    {
        return false;
    }
    return (clientOf(mover) && isMonster(touch)) || (isMonster(mover) && clientOf(touch));
}

// The player and the monster of a pair, whichever moves.
void sides(edict_t* mover, edict_t* touch, edict_t*& player, edict_t*& monster)
{
    const bool moverIsPlayer = clientOf(mover) != 0;
    player = moverIsPlayer ? mover : touch;
    monster = moverIsPlayer ? touch : mover;
}

// A box's level sides in by `by` each, never narrower than minHalf a side.
void insetBox(float* mins, float* maxs, float by)
{
    for(int i = 0; i < 2; ++i)
    {
        const float half = (maxs[i] - mins[i]) * 0.5f;
        const float s = za::clamp(by, 0.f, za::max(half - minHalf, 0.f));
        mins[i] += s;
        maxs[i] -= s;
    }
}

// The hull the pair meets with (SV_HullForEntity's: touch's box less the mover's, from touch's origin): each box as the
// hull code narrows it (vr_hull_ent_width, vr_mhull_ents: as world.c's SV_ClipMoveToBoxEntityQVR), then inset.
void pairHull(edict_t* mover, edict_t* touch, const float* mins, const float* maxs, float* hullMins, float* hullMaxs)
{
    float m[3], mx[3], t[3], tx[3];
    if(!(hull::entBox(mover, mins, maxs, m, mx) && hull::narrowsAgainst(mover, touch)))
    {
        VectorCopy(mins, m);
        VectorCopy(maxs, mx);
    }
    if(!hull::touchBox(touch, mover, t, tx))
    {
        VectorCopy(touch->v.mins, t);
        VectorCopy(touch->v.maxs, tx);
    }
    const float by = insetSetting();
    insetBox(m, mx, by);
    insetBox(t, tx, by);
    for(int i = 0; i < 3; ++i)
    {
        hullMins[i] = t[i] - mx[i];
        hullMaxs[i] = tx[i] - m[i];
    }
}

// How deep a point is in the hull: the least way out (> 0 inside, <= 0 outside).
[[nodiscard]] float depthIn(const float* hullMins, const float* hullMaxs, const glm::vec3& p)
{
    float d = p[0] - hullMins[0];
    for(int i = 0; i < 3; ++i)
    {
        d = za::min(d, p[i] - hullMins[i], hullMaxs[i] - p[i]);
    }
    return d;
}

// The axis and way (-1, +1) out of the hull from p with the least move, and how far (depthIn).
float wayOut(const float* hullMins, const float* hullMaxs, const glm::vec3& p, int& axis, float& sign)
{
    float best = ZA_FLOAT_INFINITY;
    axis = 0;
    sign = 1.f;
    for(int i = 0; i < 3; ++i)
    {
        const float lo = p[i] - hullMins[i], hi = hullMaxs[i] - p[i];
        if(lo < best)
        {
            best = lo;
            axis = i;
            sign = -1.f;
        }
        if(hi < best)
        {
            best = hi;
            axis = i;
            sign = 1.f;
        }
    }
    return best;
}

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

// Moves the player by d if nothing is in the way, linked (his triggers touched): how far he went.
float movePlayer(edict_t* p, const glm::vec3& d)
{
    vec3_t end;
    for(int i = 0; i < 3; ++i)
    {
        end[i] = p->v.origin[i] + d[i];
    }
    const trace_t tr = SV_Move(p->v.origin, p->v.mins, p->v.maxs, end, MOVE_NORMAL, p);
    if(tr.allsolid || tr.startsolid || tr.fraction <= 0.f)
    {
        return 0.f;
    }
    VectorCopy(tr.endpos, p->v.origin);
    SV_LinkEdict(p, true);
    return tr.fraction * glm::length(d);
}

// Pushes an overlapping pair a step apart: true if they overlapped.
bool pushApart(edict_t* p, edict_t* m, float dt)
{
    float hullMins[3], hullMaxs[3];
    pairHull(p, m, p->v.mins, p->v.maxs, hullMins, hullMaxs);
    int axis = 0;
    float sign = 1.f;
    const float depth = wayOut(hullMins, hullMaxs, vec(p->v.origin) - vec(m->v.origin), axis, sign);
    if(depth <= deeperSlack || axis == 2)
    {
        return false; // apart, or the player over it or under it (its top: Quake's standing on a monster)
    }
    const float step = za::min(depth + deeperSlack, pushSpeed * dt);
    glm::vec3 d{0.f};
    d[axis] = sign * step; // the player's way out
    // The monster first (a walk step: not off a ledge, not into a wall), then the player for the rest.
    vec3_t move;
    move[0] = -d[0];
    move[1] = -d[1];
    move[2] = 0.f;
    float rest = step;
    if((static_cast<int>(m->v.flags) & (FL_ONGROUND | FL_FLY | FL_SWIM)) && SV_movestep(m, move, true))
    {
        rest = 0.f;
    }
    if(rest > 0.f)
    {
        rest -= movePlayer(p, d * (rest / step));
    }
    if(vr_foegrab_debug.value >= 2.f)
    {
        Con_Printf("body contact: %s (%d) %.2f deep, pushed apart %.2f\n", PR_GetString(m->v.classname),
            NUM_FOR_EDICT(m), static_cast<double>(depth), static_cast<double>(step - rest));
    }
    return true;
}

// Whether a player's hand holds this monster and so they don't meet (vr_foegrab_step_in).
[[nodiscard]] bool steppedIn(const edict_t* player, const edict_t* monster)
{
    return stepInOn() && foegrab::holds(player, monster);
}

[[nodiscard]] bool within(const edict_t* p, const edict_t* m, float reach)
{
    for(int i = 0; i < 3; ++i)
    {
        if(m->v.absmin[i] > p->v.absmax[i] + reach || m->v.absmax[i] < p->v.absmin[i] - reach)
        {
            return false;
        }
    }
    return true;
}

// The first player's nearest live monster: how far (origins, level), how deep in their pair's hull (> 0 overlapping),
// held, stepped in.
void status_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_body_contact_status: no game\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm); // a console command: no VM is current
    edict_t* p = EDICT_NUM(1);
    edict_t* best = nullptr;
    float bestDist = ZA_FLOAT_INFINITY;
    for(int e = svs.maxclients + 1; e < qcvm->num_edicts; ++e)
    {
        edict_t* m = EDICT_NUM(e);
        if(m->free || !isMonster(m) || m->v.health <= 0.f)
        {
            continue;
        }
        glm::vec3 d = vec(m->v.origin) - vec(p->v.origin);
        d.z = 0.f;
        const float dist = glm::length(d);
        if(dist < bestDist)
        {
            bestDist = dist;
            best = m;
        }
    }
    if(!best)
    {
        Con_Printf("body contact: no live monster (inset %.1f, step in %d)\n", static_cast<double>(insetSetting()),
            stepInOn() ? 1 : 0);
    }
    else
    {
        float hullMins[3], hullMaxs[3];
        pairHull(p, best, p->v.mins, p->v.maxs, hullMins, hullMaxs);
        const float depth = depthIn(hullMins, hullMaxs, vec(p->v.origin) - vec(best->v.origin));
        Con_Printf("body contact: %s (%d) dist %.2f depth %.2f hull %.1f..%.1f inset %.1f step in %d held %d\n",
            PR_GetString(best->v.classname), NUM_FOR_EDICT(best), static_cast<double>(bestDist),
            static_cast<double>(depth), static_cast<double>(hullMins[0]), static_cast<double>(hullMaxs[0]),
            static_cast<double>(insetSetting()), stepInOn() ? 1 : 0, foegrab::holds(p, best) ? 1 : 0);
    }
    PR_PopQCVM(oldvm);
}

} // namespace

void qvr::bodycontact::init()
{
    Cmd_AddCommand("vr_body_contact_status", status_f);
}

void qvr::bodycontact::serverFrame()
{
    if(!sv.active || !anyOn() || qcvm != &sv.qcvm)
    {
        return;
    }
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    if(dt <= 0.f)
    {
        return;
    }
    for(int c = 1; c <= svs.maxclients; ++c)
    {
        if(!svs.clients[c - 1].active || !svs.clients[c - 1].spawned)
        {
            continue;
        }
        edict_t* p = EDICT_NUM(c);
        if(static_cast<int>(p->v.solid) != SOLID_SLIDEBOX || p->v.health <= 0.f || static_cast<int>(p->v.movetype) == MOVETYPE_NOCLIP)
        {
            continue;
        }
        for(int e = svs.maxclients + 1; e < qcvm->num_edicts; ++e)
        {
            edict_t* m = EDICT_NUM(e);
            if(m->free || !isMonster(m) || m->v.health <= 0.f || !within(p, m, nearReach) || steppedIn(p, m))
            {
                continue;
            }
            (void)pushApart(p, m, dt);
        }
    }
}

extern "C" int VR_BodyContactHull(edict_t* mover, edict_t* touch, const float* mins, const float* maxs,
    float* hullmins, float* hullmaxs)
{
    if(!anyOn() || !isPair(mover, touch, mins, maxs))
    {
        return 0;
    }
    pairHull(mover, touch, mins, maxs, hullmins, hullmaxs);
    return 1;
}

extern "C" int VR_BodyContactLetsOut(edict_t* mover, edict_t* touch, const float* hullmins, const float* hullmaxs,
    const float* start, const float* end)
{
    // How deep the mover is now (from its origin): a move starting in deeper (a monster's step, SV_movestep, traces down
    // from where it would be) is stopped as Quake's.
    const float now = depthIn(hullmins, hullmaxs, vec(mover->v.origin) - vec(touch->v.origin));
    const glm::vec3 a = vec(start), b = vec(end);
    const float d0 = depthIn(hullmins, hullmaxs, a);
    if(now <= 0.f || d0 <= 0.f || d0 > now + deeperSlack)
    {
        return 0; // not in it now, starting at its surface, or deeper: the hull check's own
    }
    const float len = glm::length(b - a);
    if(len < 0.001f)
    {
        return 1; // a test where it is (SV_TestEntityPosition): not stuck
    }
    // The depth along the move is concave (a box's): no deeper a little way along, and no deeper at its end, it is never
    // deeper on the way.
    const glm::vec3 probe = a + (b - a) * za::min(1.f, probeStep / len);
    return depthIn(hullmins, hullmaxs, probe) <= d0 + deeperSlack && depthIn(hullmins, hullmaxs, b) <= d0 + deeperSlack
               ? 1
               : 0;
}

extern "C" int VR_BodyContactPasses(edict_t* mover, edict_t* touch, const float* mins, const float* maxs)
{
    if(!stepInOn() || !isPair(mover, touch, mins, maxs))
    {
        return 0;
    }
    edict_t* player = nullptr;
    edict_t* monster = nullptr;
    sides(mover, touch, player, monster);
    return foegrab::holds(player, monster) ? 1 : 0;
}
