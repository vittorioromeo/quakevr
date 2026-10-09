// vr_unstick.cpp -- see vr_unstick.hpp.

#include "vr_unstick.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"


namespace qvr::unstick
{
namespace
{

constexpr double retryDelay = 0.25; // seconds between searches that found nothing (each is up to ~2000 box tests)

struct Stats
{
    int freed = 0;       // players moved out
    int failed = 0;      // searches that found no free spot
    float lastDist = 0.f; // the last move's length
    char lastIn[64] = "";  // what the last one was in
};

Stats stats;
za::Array<double, MAX_SCOREBOARD + 1> nextTry{}; // per client: no search before this server time (after a failed one)

// Monsters (and any other live walking body: SOLID_SLIDEBOX, MOVETYPE_STEP) found inside the map or a brush model, as
// they move against it, and not moving, for vr_unstick_monsters_time seconds: moved to the nearest free spot.
constexpr double monsterCheckEvery = 0.2; // seconds between one body's tests (one box test each)
constexpr float monsterStillWithin = 2.f; // moved less than this since it was first found inside: not getting out

struct MonsterWatch
{
    double since = -1.0; // first found inside (this spell), -1: free
    double next = 0.0;   // its next test
    float at[3]{};       // where it was then
};

struct MonsterStats
{
    int freed = 0;
    int failed = 0;
    float lastDist = 0.f;
    char last[64] = ""; // the last one freed: its classname and number
};

za::Vector<MonsterWatch> monsterWatches; // by edict number
double monsterWatchTime = 0.0;           // the last test's server time (an earlier one: a new map)
MonsterStats monsterStats;

// The spots tried, nearest first: 26 directions (sideways, then up, then down: a player is not sunk into the floor
// while a spot as near is free elsewhere) at growing distances, finely at first (a mover's few units).
// Built before main (no first-call guard): read by any thread.
const za::Vector<glm::vec3> spotOffsets = [] {
    za::Vector<glm::vec3> dirs;
    const auto add = [&](float x, float y, float z) { dirs.pushBack(glm::normalize(glm::vec3{x, y, z})); };
    add(1, 0, 0), add(-1, 0, 0), add(0, 1, 0), add(0, -1, 0); // sideways
    add(0, 0, 1);                                              // up
    add(1, 1, 0), add(1, -1, 0), add(-1, 1, 0), add(-1, -1, 0);
    add(1, 0, 1), add(-1, 0, 1), add(0, 1, 1), add(0, -1, 1);
    add(1, 1, 1), add(1, -1, 1), add(-1, 1, 1), add(-1, -1, 1);
    add(0, 0, -1); // down, last
    add(1, 0, -1), add(-1, 0, -1), add(0, 1, -1), add(0, -1, -1);
    add(1, 1, -1), add(1, -1, -1), add(-1, 1, -1), add(-1, -1, -1);

    za::Vector<float> dists{0.125f, 0.25f, 0.5f, 0.75f};
    for(float d = 1.f; d <= 4.f; d += 0.5f)
    {
        dists.pushBack(d);
    }
    for(float d = 5.f; d <= 16.f; d += 1.f)
    {
        dists.pushBack(d);
    }
    for(float d = 18.f; d <= 48.f; d += 2.f)
    {
        dists.pushBack(d);
    }

    za::Vector<glm::vec3> out;
    out.reserve(dists.size() * dirs.size());
    for(const float d : dists)
    {
        for(const glm::vec3& dir : dirs)
        {
            out.pushBack(dir * d);
        }
    }
    return out;
}();

const za::Vector<glm::vec3>& offsets()
{
    return spotOffsets;
}

const char* nameOf(const edict_t* e)
{
    if(!e)
    {
        return "nothing";
    }
    return e == qcvm->edicts ? "the world" : PR_GetString(e->v.classname);
}

bool boxFreeAt(edict_t* ent, const vec3_t p, int type)
{
    vec3_t at{p[0], p[1], p[2]};
    return !SV_Move(at, ent->v.mins, ent->v.maxs, at, type, ent).startsolid;
}

// The nearest spot to `from` where the player's box is free of `type`'s solids, reached from `from` without its centre
// crossing a wall (not out through the far side of one); false if none within 48 units.
bool findFree(edict_t* ent, const vec3_t from, int type, vec3_t out)
{
    for(const glm::vec3& o : offsets())
    {
        vec3_t p{from[0] + o.x, from[1] + o.y, from[2] + o.z};
        if(!boxFreeAt(ent, p, type))
        {
            continue;
        }
        vec3_t start{from[0], from[1], from[2]};
        const trace_t line = SV_Move(start, vec3_origin, vec3_origin, p, MOVE_NOMONSTERS, ent);
        if(line.fraction < 1.f && !line.startsolid)
        {
            continue; // behind a wall the centre is outside of
        }
        VectorCopy(p, out);
        return true;
    }
    return false;
}

// vr_stuck_info: the first player's server position, what its box is in, and the movers near it.
void info_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm); // a console command: no VM is current
    edict_t* ent = EDICT_NUM(1);
    edict_t* in = SV_TestEntityPosition(ent);
    Con_Printf("vr_stuck_info: player at %.2f %.2f %.2f, in %s (#%d); freed %d times (last %.2f units, from %s), "
               "%d searches failed\n",
        ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], nameOf(in), in ? NUM_FOR_EDICT(in) : -1, stats.freed,
        stats.lastDist, stats.lastIn[0] ? stats.lastIn : "-", stats.failed);
    Con_Printf("vr_stuck_info: monsters freed %d times (last %s, %.2f units), %d searches failed\n", monsterStats.freed,
        monsterStats.last[0] ? monsterStats.last : "-", monsterStats.lastDist, monsterStats.failed);
    edict_t* e = NEXT_EDICT(qcvm->edicts);
    for(int i = 1; i < qcvm->num_edicts; ++i, e = NEXT_EDICT(e))
    {
        if(e->free || static_cast<int>(e->v.movetype) != MOVETYPE_PUSH)
        {
            continue;
        }
        bool nearby = true;
        for(int k = 0; k < 3; ++k)
        {
            nearby = nearby && e->v.absmin[k] < ent->v.absmax[k] + 32.f && e->v.absmax[k] > ent->v.absmin[k] - 32.f;
        }
        if(nearby)
        {
            Con_Printf("  #%d %s: %.2f %.2f %.2f .. %.2f %.2f %.2f, velocity %.0f %.0f %.0f\n", i, nameOf(e),
                e->v.absmin[0], e->v.absmin[1], e->v.absmin[2], e->v.absmax[0], e->v.absmax[1], e->v.absmax[2],
                e->v.velocity[0], e->v.velocity[1], e->v.velocity[2]);
        }
    }
    PR_PopQCVM(oldvm);
}

// vr_stuck_test <x> <y> <z>: the first player put there as if it had walked there (its last free spot too, so Quake's
// "back to where it was" cannot free it): a spot in a wall or a door tests vr_unstick.
void test_f()
{
    if(!sv.active || svs.maxclients < 1 || Cmd_Argc() < 4)
    {
        Con_Printf("vr_stuck_test <x> <y> <z>: puts the player there (inside something, to test vr_unstick)\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    edict_t* ent = EDICT_NUM(1);
    for(int i = 0; i < 3; ++i)
    {
        ent->v.origin[i] = ent->v.oldorigin[i] = Q_atof(Cmd_Argv(1 + i));
        ent->v.velocity[i] = 0.f;
    }
    SV_LinkEdict(ent, false);
    edict_t* in = SV_TestEntityPosition(ent);
    Con_Printf("vr_stuck_test: player at %.2f %.2f %.2f, in %s\n", ent->v.origin[0], ent->v.origin[1],
        ent->v.origin[2], nameOf(in));
    PR_PopQCVM(oldvm);
}

void printTrace(const char* what, const trace_t& t)
{
    Con_Printf("  %s: fraction %.4f, end %.2f %.2f %.2f, normal %.3f %.3f %.3f (dist %.2f)%s%s, %s\n", what, t.fraction,
        t.endpos[0], t.endpos[1], t.endpos[2], t.plane.normal[0], t.plane.normal[1], t.plane.normal[2], t.plane.dist,
        t.startsolid ? ", start solid" : "", t.allsolid ? ", all solid" : "", t.ent ? nameOf(t.ent) : "nothing");
}

// vr_stuck_trace <dx> <dy> <dz> [edict]: the first player's (or that entity's) box moved by that much from where it is,
// as its moves meet things (SV_Move: the narrow box and the compiled hull), then against the world in Quake's own hull
// (hull 1 or 2 by its width): where each stops and the plane it meets. For a "can't walk up there" report.
void trace_f()
{
    if(!sv.active || svs.maxclients < 1 || Cmd_Argc() < 4)
    {
        Con_Printf("vr_stuck_trace <dx> <dy> <dz> [edict]: the player's box moved by that much: where it stops, the plane "
                   "it meets\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    const int num = Cmd_Argc() > 4 ? Q_atoi(Cmd_Argv(4)) : 1;
    if(num >= 1 && num < qcvm->num_edicts)
    {
        edict_t* ent = EDICT_NUM(num);
        vec3_t start, end;
        VectorCopy(ent->v.origin, start);
        for(int i = 0; i < 3; ++i)
        {
            end[i] = start[i] + Q_atof(Cmd_Argv(1 + i));
        }
        Con_Printf("vr_stuck_trace %s #%d from %.2f %.2f %.2f:\n", nameOf(ent), num, start[0], start[1], start[2]);
        printTrace("its move", SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent));
        printTrace("Quake's hull", SV_ClipMoveToEntity(qcvm->edicts, start, ent->v.mins, ent->v.maxs, end));
    }
    PR_PopQCVM(oldvm);
}

// A live walking body (a monster: SOLID_SLIDEBOX, MOVETYPE_STEP), not a player.
bool walker(const edict_t* e)
{
    return !e->free && static_cast<int>(e->v.solid) == SOLID_SLIDEBOX && static_cast<int>(e->v.movetype) == MOVETYPE_STEP &&
           e->v.health > 0.f;
}

// vr_stuck_sink [edict] [depth]: that monster (else the live one nearest the first player) put `depth` units (24) down
// into the floor under it, as a bad drop to the floor would leave it: vr_unstick_monsters should free it within its time.
void sink_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_stuck_sink [edict] [depth]: needs a map\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    int num = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0;
    const float depth = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 24.f;
    if(num <= svs.maxclients || num >= qcvm->num_edicts)
    {
        const edict_t* player = EDICT_NUM(1);
        float best = 1e30f;
        num = 0;
        for(int i = svs.maxclients + 1; i < qcvm->num_edicts; ++i)
        {
            const edict_t* e = EDICT_NUM(i);
            if(!walker(e))
            {
                continue;
            }
            float d = 0.f;
            for(int k = 0; k < 3; ++k)
            {
                d += (e->v.origin[k] - player->v.origin[k]) * (e->v.origin[k] - player->v.origin[k]);
            }
            if(d < best)
            {
                best = d;
                num = i;
            }
        }
    }
    if(num <= svs.maxclients || num >= qcvm->num_edicts || !walker(EDICT_NUM(num)))
    {
        Con_Printf("vr_stuck_sink: no live monster\n");
        PR_PopQCVM(oldvm);
        return;
    }
    edict_t* ent = EDICT_NUM(num);
    ent->v.origin[2] -= depth;
    SV_LinkEdict(ent, false);
    Con_Printf("vr_stuck_sink: %s #%d sunk %.0f units, at %.2f %.2f %.2f: %s\n", nameOf(ent), num, depth,
        ent->v.origin[0], ent->v.origin[1], ent->v.origin[2],
        boxFreeAt(ent, ent->v.origin, MOVE_NOMONSTERS) ? "free (not inside anything)" : "inside the map");
    PR_PopQCVM(oldvm);
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_stuck_info", info_f);
    Cmd_AddCommand("vr_stuck_test", test_f);
    Cmd_AddCommand("vr_stuck_trace", trace_f);
    Cmd_AddCommand("vr_stuck_sink", sink_f);
}

} // namespace qvr::unstick

// SV_Physics_Step, after its think: a monster inside the map or a brush model (as its own moves meet them: its narrow box
// or its hull) that hasn't moved for vr_unstick_monsters_time seconds is moved to the nearest free spot, as a player is
// (vr_unstick). Every move it tries from inside starts in solid and fails, so without this it stands there for good:
// e5m4's fiend sunk into the ground by the drop to the floor (vr_gameplay.cpp), a monster pushed into a wall, a map's
// misplaced one. One box test per monster every 0.2 s.
extern "C" void VR_UnstickMonster(edict_t* ent)
{
    using namespace qvr;
    using namespace qvr::unstick;
    if(!vr_unstick_monsters.value || ent->free)
    {
        return;
    }
    const int num = NUM_FOR_EDICT(ent);
    if(num <= svs.maxclients)
    {
        return;
    }
    if(qcvm->time < monsterWatchTime)
    {
        monsterWatches.clear(); // a new map (or a load): its clock started again
    }
    monsterWatchTime = qcvm->time;
    if(monsterWatches.size() < static_cast<za::SizeT>(qcvm->max_edicts))
    {
        monsterWatches.resize(static_cast<za::SizeT>(qcvm->max_edicts));
    }
    MonsterWatch& w = monsterWatches[static_cast<za::SizeT>(num)];
    if(!walker(ent))
    {
        w.since = -1.0;
        return;
    }
    if(qcvm->time < w.next && w.next - qcvm->time <= monsterCheckEvery)
    {
        return;
    }
    w.next = qcvm->time + monsterCheckEvery;
    vec3_t org;
    VectorCopy(ent->v.origin, org);
    if(boxFreeAt(ent, org, MOVE_NOMONSTERS))
    {
        w.since = -1.0;
        return;
    }
    float moved = 0.f;
    for(int k = 0; k < 3; ++k)
    {
        moved = za::max(moved, za::fabs(org[k] - w.at[k]));
    }
    if(w.since < 0.0 || qcvm->time < w.since || moved > monsterStillWithin)
    {
        w.since = qcvm->time;
        VectorCopy(org, w.at);
        return;
    }
    const double inside = qcvm->time - w.since;
    if(inside < static_cast<double>(vr_unstick_monsters_time.value))
    {
        return;
    }
    edict_t* in = SV_TestEntityPosition(ent);
    vec3_t to;
    if(!findFree(ent, org, MOVE_NORMAL, to) && !findFree(ent, org, MOVE_NOMONSTERS, to))
    {
        ++monsterStats.failed;
        w.since = qcvm->time; // tried again after another spell
        Con_DPrintf("vr_unstick: %s #%d inside %s at %.1f %.1f %.1f: no free spot near\n", nameOf(ent), num, nameOf(in),
            org[0], org[1], org[2]);
        return;
    }
    VectorCopy(to, ent->v.origin);
    if(!(static_cast<int>(ent->v.flags) & (FL_FLY | FL_SWIM)))
    {
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~(FL_ONGROUND | FL_PARTIALGROUND)); // falls to the floor from there
    }
    SV_LinkEdict(ent, true);
    w.since = -1.0;
    ++monsterStats.freed;
    monsterStats.lastDist = za::sqrt((to[0] - org[0]) * (to[0] - org[0]) + (to[1] - org[1]) * (to[1] - org[1]) +
                                     (to[2] - org[2]) * (to[2] - org[2]));
    q_snprintf(monsterStats.last, sizeof(monsterStats.last), "%s #%d", nameOf(ent), num);
    Con_DPrintf("vr_unstick: %s #%d freed from %s after %.1f s inside, at %.1f %.1f %.1f, moved %.2f %.2f %.2f\n",
        nameOf(ent), num, nameOf(in), inside, org[0], org[1], org[2], to[0] - org[0], to[1] - org[1], to[2] - org[2]);
}

extern "C" void VR_WalkMoveDebug(edict_t* ent, const char* what, const trace_t* trace)
{
    using namespace qvr;
    if(!vr_debug_walkmove.value || NUM_FOR_EDICT(ent) != 1)
    {
        return;
    }
    Con_Printf("walkmove %.3f %s: at %.2f %.2f %.2f, velocity %.1f %.1f %.1f, flags %d", qcvm->time, what,
        ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], ent->v.velocity[0], ent->v.velocity[1],
        ent->v.velocity[2], static_cast<int>(ent->v.flags) & (FL_ONGROUND | FL_WATERJUMP));
    if(trace)
    {
        Con_Printf("; trace %.3f, normal %.3f %.3f %.3f%s", trace->fraction, trace->plane.normal[0],
            trace->plane.normal[1], trace->plane.normal[2], trace->startsolid ? ", start solid" : "");
    }
    Con_Printf("\n");
}

extern "C" int VR_Unstick(edict_t* ent)
{
    using namespace qvr;
    using namespace qvr::unstick;
    if(!vr_unstick.value)
    {
        return 0;
    }
    const int client = NUM_FOR_EDICT(ent);
    if(client < 1 || client >= static_cast<int>(nextTry.size()) ||
        (qcvm->time < nextTry[client] && nextTry[client] - qcvm->time <= retryDelay)) // (a new map restarts the clock)
    {
        return 0;
    }
    // Only a player in the map's solids or a brush model (a door, a button, a lift): one inside a monster's box or an
    // item's stays Quake's (the monster moves on; the player walks out).
    vec3_t org;
    VectorCopy(ent->v.origin, org);
    if(boxFreeAt(ent, org, MOVE_NOMONSTERS))
    {
        return 0;
    }
    edict_t* in = SV_TestEntityPosition(ent);

    // Free of everything, else free of the map at least (then as a player spawned inside a monster is).
    vec3_t to;
    if(!findFree(ent, org, MOVE_NORMAL, to) && !findFree(ent, org, MOVE_NOMONSTERS, to))
    {
        ++stats.failed;
        nextTry[client] = qcvm->time + retryDelay;
        Con_DPrintf("vr_unstick: no free spot near %.1f %.1f %.1f (in %s)\n", org[0], org[1], org[2], nameOf(in));
        return 0;
    }

    VectorCopy(to, ent->v.origin);
    VectorCopy(to, ent->v.oldorigin);
    SV_LinkEdict(ent, true);
    ++stats.freed;
    stats.lastDist = za::sqrt((to[0] - org[0]) * (to[0] - org[0]) + (to[1] - org[1]) * (to[1] - org[1]) +
                               (to[2] - org[2]) * (to[2] - org[2]));
    q_strlcpy(stats.lastIn, nameOf(in), sizeof(stats.lastIn));
    Con_DPrintf("vr_unstick: freed from %s, moved %.2f %.2f %.2f\n", stats.lastIn, to[0] - org[0], to[1] - org[1],
        to[2] - org[2]);
    return 1;
}
