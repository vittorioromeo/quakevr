#include "vr_alloccount.h"
// vr_box3d_diag.cpp -- the physics commands: spawning, piles, blasts, lists, hashes, benches and tests (and their
// registration).
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_hands.hpp"
#include "vr_units.hpp"

namespace box3d::detail
{
namespace
{
// ---------------------------------------------------------------------------------------------------------------------
// Test commands (either engine: they only move entities)

// The entities `which` names: a number, a classname (every one), or "props" (every rigid body).
[[nodiscard]] za::Vector<edict_t*> entitiesNamed(const char* which)
{
    za::Vector<edict_t*> out;
    if(which[0] >= '0' && which[0] <= '9')
    {
        const int num = Q_atoi(which);
        if(num > 0 && num < qcvm->num_edicts && !EDICT_NUM(num)->free)
        {
            out.pushBack(EDICT_NUM(num));
        }
        return out;
    }
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(!e->free && (!strcmp(which, "props") ? isRigid(e) && modelOf(e) : !strcmp(PR_GetString(e->v.classname), which)))
        {
            out.pushBack(e);
        }
    }
    return out;
}

// vr_physics_loose <number | classname>: made a loose physics object, as a hand's knock makes it (QC's
// VR_Carry_Loose: a toss, rigid, touchable), where it is: a hanging armour, a pickup. For tests.
void loose_f()
{
    if(!sv.active || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_physics_loose <number | classname>\n");
        return;
    }
    const VmScope vm;
    for(edict_t* e : entitiesNamed(Cmd_Argv(1)))
    {
        e->v.movetype = MOVETYPE_TOSS;
        e->v.solid = SOLID_NOT_BUT_TOUCHABLE;
        setFieldFloat(e, fields().vr_rigid, 1.f);
        setFlag(e, FL_ONGROUND, false);
        SV_LinkEdict(e, false);
        Con_Printf("vr_physics_loose: %d %s\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname));
    }
}

void placeStill(edict_t* e, const glm::vec3& at, float yaw)
{
    const FieldOffsets& f = fields();
    store(at, e->v.origin);
    e->v.angles[0] = 0.f;
    e->v.angles[1] = yaw;
    e->v.angles[2] = 0.f;
    VectorCopy(vec3_origin, e->v.velocity);
    VectorCopy(vec3_origin, e->v.avelocity);
    setFieldVec(e, f.vr_spin, glm::vec3{0.f});
    setFlag(e, FL_ONGROUND, false);
    SV_LinkEdict(e, false);
}

// vr_physics_stack <number | classname | props> <count> <x> <y> <z> [<yaw> [<gap>]]: the first `count` of them in a
// column from x y z (their bottoms `gap` units apart, 0.5 by default), still, level, awake.
void stack_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_stack <number | classname | props> <count> <x> <y> <z> [<yaw> [<gap>]]\n");
        return;
    }
    const VmScope vm;
    za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const size_t count = za::min(list.size(), static_cast<size_t>(za::max(0, Q_atoi(Cmd_Argv(2)))));
    glm::vec3 at{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float yaw = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 0.f;
    const float gap = Cmd_Argc() > 7 ? Q_atof(Cmd_Argv(7)) : 0.5f;
    for(size_t i = 0; i < count; i++)
    {
        edict_t* e = list[i];
        glm::vec3 lo, hi;
        localBox(e, modelOf(e), lo, hi);
        placeStill(e, at - glm::vec3{0.f, 0.f, lo.z}, yaw);
        Con_Printf("vr_physics_stack: %d %s at %.1f %.1f %.1f\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname), e->v.origin[0],
            e->v.origin[1], e->v.origin[2]);
        at.z += hi.z - lo.z + gap;
    }
}

// vr_physics_pyramid <number | classname | props> <rows> <x> <y> <z> [<yaw>]: a pyramid along the yaw's side, `rows`
// at the bottom (rows (rows + 1) / 2 of them), its middle at x y, standing on z.
void pyramid_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_pyramid <number | classname | props> <rows> <x> <y> <z> [<yaw>]\n");
        return;
    }
    const VmScope vm;
    za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const int rows = za::max(1, Q_atoi(Cmd_Argv(2)));
    const glm::vec3 centre{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float yaw = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 0.f;
    const glm::vec3 side{za::cos(glm::radians(yaw + 90.f)), za::sin(glm::radians(yaw + 90.f)), 0.f};
    size_t next = 0;
    float z = centre.z;
    for(int row = 0; row < rows && next < list.size(); row++)
    {
        const int n = rows - row;
        float height = 0.f;
        for(int k = 0; k < n && next < list.size(); k++)
        {
            edict_t* e = list[next++];
            glm::vec3 lo, hi;
            localBox(e, modelOf(e), lo, hi);
            const float width = (hi.y - lo.y) + 1.f;
            const glm::vec3 at = centre + side * ((static_cast<float>(k) - (n - 1) * 0.5f) * width);
            placeStill(e, glm::vec3{at.x, at.y, z - lo.z + 0.25f}, yaw);
            height = za::max(height, hi.z - lo.z);
            Con_Printf("vr_physics_pyramid: %d %s at %.1f %.1f %.1f\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname), e->v.origin[0],
                e->v.origin[1], e->v.origin[2]);
        }
        z += height + 0.5f;
    }
}

// vr_physics_pile <number | classname | props> <per column> <x> <y> <z> [<spacing>]: all of them in columns of
// `per column` (as vr_physics_stack), side by side along x, `spacing` units apart (24): towers that fall into a pile.
void pile_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_pile <number | classname | props> <per column> <x> <y> <z> [<spacing>]\n");
        return;
    }
    const VmScope vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(1));
    const size_t per = static_cast<size_t>(za::max(1, Q_atoi(Cmd_Argv(2))));
    const glm::vec3 base{Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5))};
    const float spacing = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 24.f;
    glm::vec3 at = base;
    for(size_t i = 0; i < list.size(); i++)
    {
        if(i % per == 0)
        {
            at = base + glm::vec3{spacing * static_cast<float>(i / per), 0.f, 0.f};
        }
        edict_t* e = list[i];
        glm::vec3 lo, hi;
        localBox(e, modelOf(e), lo, hi);
        placeStill(e, at - glm::vec3{0.f, 0.f, lo.z}, static_cast<float>((i * 37) % 90));
        at.z += hi.z - lo.z + 0.5f;
    }
    Con_Printf("vr_physics_pile: %d in %d columns\n", static_cast<int>(list.size()), static_cast<int>((list.size() + per - 1) / per));
}

// The rocks and bricks a big pile is made of (vr_debris.qc's models, precached on every map): rocks, then bricks.
constexpr const char* pileModels[] = {"progs/vr_rock1.mdl", "progs/vr_rock2.mdl", "progs/vr_rock3.mdl", "progs/vr_rock4.mdl",
    "progs/vr_rock5.mdl", "progs/vr_brick1.mdl", "progs/vr_brick2.mdl", "progs/vr_brick3.mdl", "progs/vr_brick4.mdl"};
constexpr int pileRocks = 5;
constexpr int pileBricks = 4;

// What the physics stress tests spawn carries this bit in its spawnflags (no map sets it; the spawn functions read only
// their low bits), so that vr_physics_clearpiles takes away theirs and nothing the map placed. 2^20: exact as a float.
constexpr int stressTagBit = 1 << 20;

enum class PileKind
{
    Debris, // rocks and bricks in turn
    Rocks,
    Bricks,
    Crates, // walls of small crates
    Mixed,  // rocks and bricks, every fourth column small crates
};

struct PileKindName
{
    const char* name;
    PileKind kind;
};

constexpr PileKindName pileKindNames[] = {{"debris", PileKind::Debris}, {"rocks", PileKind::Rocks},
    {"bricks", PileKind::Bricks}, {"crates", PileKind::Crates}, {"mixed", PileKind::Mixed}};

// One prop of the stress tests spawned at `at` by its spawn function `fn` (`model` given to it if not null), tagged
// (stressTagBit). Null if it removed itself.
edict_t* spawnStressProp(func_t fn, const char* classname, const char* model, const glm::vec3& at)
{
    edict_t* e = ED_Alloc();
    store(at, e->v.origin);
    e->v.classname = PR_SetEngineString(classname);
    VR_EdictIndex_Touch(e); // (the edict index: a classname set in C)
    if(model)
    {
        e->v.model = PR_SetEngineString(model);
    }
    e->v.spawnflags = static_cast<float>(stressTagBit);
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    PR_ExecuteProgram(fn); // rests it on the floor below (vr_debris_piece, vr_crate)
    return e->free ? nullptr : e;
}

// vr_physics_bigpile [<count>] [<distance>] [debris | rocks | bricks | crates | mixed]: a big pile of props for Box3D on
// the pool (NOTES.md vrfiringrange_2026-10-01_16-36-40, _22-46-00; Debug > Tests > Physics Stress). `count` props
// (vr_test_pile_count, 500, or for crates vr_test_pile_crates, 80; at most 2000) `distance` units (96) ahead of the
// first player:
// - debris (the default), rocks, bricks, mixed: in columns of 10 (small crates: 3) on a square grid 20 units apart (36
//   with crates), each prop set a little further along than the one under it, so that the columns topple into one pile:
//   more awake bodies than Physics Threads From (vr_box3d_threads_bodies, 150) while they fall and settle. Mixed: rocks
//   and bricks, every fourth column small crates.
// - crates: walls of small crates facing you, 8 wide and 5 high, each next one 48 units behind.
// Each is tagged (stressTagBit): vr_physics_clearpiles takes them away. vr_physics_steptime (Physics Step Time) before
// and after gives the step's time; vr_physics_mtbench the same alone.
void bigPile_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_bigpile [<count>] [<distance>] [debris | rocks | bricks | crates | mixed] (in a game)\n");
        return;
    }
    PileKind kind = PileKind::Debris;
    int numbers = 0;
    int count = -1;
    float distance = 96.f;
    for(int a = 1; a < Cmd_Argc(); a++)
    {
        const char* arg = Cmd_Argv(a);
        if((arg[0] >= '0' && arg[0] <= '9') || arg[0] == '.' || arg[0] == '-')
        {
            if(numbers++ == 0)
            {
                count = CLAMP(1, Q_atoi(arg), 2000);
            }
            else
            {
                distance = static_cast<float>(Q_atof(arg));
            }
            continue;
        }
        bool known = false;
        for(const PileKindName& k : pileKindNames)
        {
            if(!q_strcasecmp(k.name, arg))
            {
                kind = k.kind;
                known = true;
            }
        }
        if(!known)
        {
            Con_Printf("vr_physics_bigpile: no kind %s (debris, rocks, bricks, crates, mixed)\n", arg);
            return;
        }
    }
    if(count < 0)
    {
        const float v = kind == PileKind::Crates ? vr_test_pile_crates.value : vr_test_pile_count.value;
        count = CLAMP(1, static_cast<int>(v), 2000);
    }
    const VmScope vm;
    const func_t pieceFn = qvr::progs::findFunction("vr_debris_piece");
    const func_t crateFn = qvr::progs::findFunction("vr_crate");
    if((kind != PileKind::Crates && !pieceFn) || ((kind == PileKind::Crates || kind == PileKind::Mixed) && !crateFn))
    {
        Con_Printf("vr_physics_bigpile: no vr_debris_piece or vr_crate in the progs\n");
        return;
    }
    edict_t* player = EDICT_NUM(1);
    vec3_t yaw{0.f, player->v.angles[1], 0.f};
    vec3_t forward, right, up;
    AngleVectors(yaw, forward, right, up);
    const glm::vec3 fwd{forward[0], forward[1], 0.f};
    const glm::vec3 side{right[0], right[1], 0.f};
    const glm::vec3 origin = vec(player->v.origin);
    int made = 0;

    if(kind == PileKind::Crates)
    {
        constexpr int wide = 8;
        constexpr int high = 5;
        constexpr float wallGap = 48.f;
        const int across = za::min(count, wide);
        float floorZ[wide] = {}; // the floor under each column of this wall (where crateplace rested its bottom crate)
        float step = 33.f;       // a crate's width and a unit between (from the first crate)
        for(int i = 0; i < count; i++)
        {
            const int wall = i / (across * high);
            const int inWall = i % (across * high);
            const int column = inWall % across;
            const int row = inWall / across;
            const float cx = static_cast<float>(column) - static_cast<float>(across - 1) * 0.5f;
            const glm::vec3 at = origin + fwd * (distance + static_cast<float>(wall) * wallGap) + side * (cx * step);
            edict_t* e = spawnStressProp(crateFn, "vr_crate", nullptr, at);
            if(!e)
            {
                continue;
            }
            glm::vec3 lo, hi;
            localBox(e, modelOf(e), lo, hi);
            if(i == 0)
            {
                step = hi.x - lo.x + 1.f;
            }
            if(row == 0)
            {
                floorZ[column] = e->v.absmin[2];
            }
            const float z = floorZ[column] + static_cast<float>(row) * (hi.z - lo.z + 0.5f);
            placeStill(e, glm::vec3{at.x, at.y, z - lo.z + 0.5f}, player->v.angles[1]);
            made++;
        }
        Con_Printf("vr_physics_bigpile: %d of %d crates in walls of %d x %d ahead (vr_physics_clearpiles: away)\n", made,
            count, across, high);
        return;
    }

    // Columns of props (pieces 10, a crate column 3) on a square grid.
    const bool mixed = kind == PileKind::Mixed;
    const float spacing = mixed ? 36.f : 20.f;
    constexpr float lean = 1.5f; // units each prop is set along from the one under it
    const auto crateColumn = [mixed](int column) { return mixed && column % 4 == 3; };
    int columns = 0;
    for(int left = count; left > 0; columns++)
    {
        left -= crateColumn(columns) ? 3 : 10;
    }
    const int across = za::max(1, static_cast<int>(glm::ceil(za::sqrt(static_cast<float>(columns)))));
    const int firstModel = kind == PileKind::Bricks ? pileRocks : 0;
    const int models = kind == PileKind::Rocks ? pileRocks : kind == PileKind::Bricks ? pileBricks : pileRocks + pileBricks;
    int column = 0;
    int row = 0;
    int pieces = 0;
    float z = 0.f; // the bottom of the next prop of this column
    for(int i = 0; i < count; i++)
    {
        const bool crate = crateColumn(column);
        const float cx = static_cast<float>(column % across) - static_cast<float>(across - 1) * 0.5f;
        const float cy = static_cast<float>(column / across);
        const glm::vec3 base = origin + fwd * (distance + cy * spacing) + side * (cx * spacing);
        edict_t* e = crate ? spawnStressProp(crateFn, "vr_crate", nullptr, base)
                           : spawnStressProp(pieceFn, "vr_debris_piece", pileModels[firstModel + pieces++ % models], base);
        if(e)
        {
            if(row == 0)
            {
                z = e->v.absmin[2]; // the floor under the column
            }
            glm::vec3 lo, hi;
            localBox(e, modelOf(e), lo, hi);
            const glm::vec3 at = base + fwd * (lean * static_cast<float>(row));
            placeStill(e, glm::vec3{at.x, at.y, z - lo.z + 0.5f}, player->v.angles[1] + static_cast<float>((i * 37) % 90));
            z += hi.z - lo.z + 0.5f;
            made++;
        }
        if(++row >= (crate ? 3 : 10))
        {
            row = 0;
            column++;
        }
    }
    Con_Printf("vr_physics_bigpile: %d of %d props in %d columns ahead (vr_physics_steptime: the step's time; "
               "vr_physics_clearpiles: away)\n",
        made, count, columns);
}

// vr_physics_clearpiles: every prop the stress tests spawned (vr_physics_bigpile: tagged) taken away, and every broken
// crate's piece (theirs or not), as QC's remove() does. Debug > Tests > Physics Stress: Clear the Piles.
void clearPiles_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_physics_clearpiles: in a game\n");
        return;
    }
    const VmScope vm;
    int removed = 0;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        const bool tagged = (static_cast<int>(e->v.spawnflags) & stressTagBit) != 0;
        if(tagged || !strcmp(PR_GetString(e->v.classname), "vr_crate_piece"))
        {
            ED_Free(e);
            removed++;
        }
    }
    Con_Printf("vr_physics_clearpiles: %d removed\n", removed);
}

// vr_physics_blast <x> <y> <z> [<damage>]: an explosion there, from the world (QC's T_RadiusDamage, 120 by default: a
// rocket's; and its effect): what it does to monsters, gibs and the props (physicsblast). For tests.
void blast_f()
{
    if(!sv.active || Cmd_Argc() < 4)
    {
        Con_Printf("usage: vr_physics_blast <x> <y> <z> [<damage>]\n");
        return;
    }
    const VmScope vm;
    dfunction_t* fn = nullptr;
    for(int i = 1; i < qcvm->progs->numfunctions && !fn; i++)
    {
        if(!strcmp(PR_GetString(qcvm->functions[i].s_name), "T_RadiusDamage"))
        {
            fn = &qcvm->functions[i];
        }
    }
    if(!fn)
    {
        Con_Printf("vr_physics_blast: no T_RadiusDamage in the progs\n");
        return;
    }
    const float at[3] = {static_cast<float>(Q_atof(Cmd_Argv(1))), static_cast<float>(Q_atof(Cmd_Argv(2))), static_cast<float>(Q_atof(Cmd_Argv(3)))};
    const float damage = Cmd_Argc() > 4 ? static_cast<float>(Q_atof(Cmd_Argv(4))) : 120.f;
    edict_t* e = ED_Alloc();
    VectorCopy(at, e->v.origin);
    SV_LinkEdict(e, false);
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(e);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
    G_INT(OFS_PARM1) = EDICT_TO_PROG(qcvm->edicts);
    G_FLOAT(OFS_PARM2) = damage;
    G_INT(OFS_PARM3) = EDICT_TO_PROG(qcvm->edicts);
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
    ED_Free(e);
    // Its effect, and so its chunks (vr_explosiondebris.cpp): the next server frame's broadcast (this command runs before
    // the frame, whose SV_ClearDatagram would drop it written to sv.datagram now).
    byte bytes[32];
    sizebuf_t msg{};
    msg.data = bytes;
    msg.maxsize = sizeof(bytes);
    MSG_WriteByte(&msg, svc_temp_entity);
    MSG_WriteByte(&msg, TE_EXPLOSION);
    for(const float c : at)
    {
        MSG_WriteCoord(&msg, c, sv.protocolflags);
    }
    qvr::server::queueBroadcast(msg.data, msg.cursize);
    Con_Printf("vr_physics_blast: %.0f at %.0f %.0f %.0f\n", damage, at[0], at[1], at[2]);
}

// vr_physics_hull_stats: the props' and actors' hulls made this session on the main thread, and the slowest.
void hullStats_f()
{
    const HullBuildStats& s = hullBuilds;
    Con_Printf("vr_physics_hull_stats: %d props' and actors' hulls made, %.1f ms of the sync (the slowest %.2f ms); %d "
               "built ahead on the pool (vr_box3d_hulls_ahead %d), %d of them taken; compared (2): %d the same, %d "
               "different\n",
        s.built, s.ms, s.mostMs, s.posted, static_cast<int>(vr_box3d_hulls_ahead.value), s.ahead, s.same, s.differed);
    for(const HullBuildStats::Slow& slow : s.slowest)
    {
        if(slow.ms > 0.0)
        {
            Con_Printf("  %6.2f ms  %s frame %d (at %.1f s)\n", slow.ms, slow.model, slow.frame, slow.time);
        }
    }
}

// vr_physics_hash [piles]: a hash of every rigid body's origin, angles, velocity and spin, bit for bit (determinism tests:
// two runs of the same script print the same). `piles`: only what the stress tests spawned (vr_physics_bigpile), not the
// map's own props.
void hash_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    const bool piles = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "piles");
    uint64_t h = 1469598103934665603ull;
    int count = 0;
    for(edict_t* e : entitiesNamed("props"))
    {
        if(piles && !(static_cast<int>(e->v.spawnflags) & stressTagBit))
        {
            continue;
        }
        const glm::vec3 v[4] = {vec(e->v.origin), vec(e->v.angles), vec(e->v.velocity), fieldVec(e, fields().vr_spin)};
        const auto* bytes = reinterpret_cast<const unsigned char*>(v);
        for(size_t i = 0; i < sizeof(v); i++)
        {
            h = (h ^ bytes[i]) * 1099511628211ull;
        }
        count++;
    }
    Con_Printf("vr_physics_hash: %d bodies, %016llx\n", count, static_cast<unsigned long long>(h));
}
} // namespace

// Box3D's profile b added to a (fields: floats only).
void addProfile(b3Profile& a, const b3Profile& b)
{
    static_assert(sizeof(b3Profile) % sizeof(float) == 0);
    float* const x = reinterpret_cast<float*>(&a);
    const float* const y = reinterpret_cast<const float*>(&b);
    for(za::SizeT i = 0; i < sizeof(b3Profile) / sizeof(float); i++)
    {
        x[i] += y[i];
    }
}

namespace
{
// The q quantile (0..1) of sorted samples (0: none).
[[nodiscard]] double quantile(const za::Vector<float>& sorted, double q)
{
    return sorted.empty() ? 0.0
                          : static_cast<double>(sorted[za::min(static_cast<za::SizeT>(q * static_cast<double>(sorted.size())),
                                sorted.size() - 1)]);
}

// vr_physics_steptime [bins]: Box3D's step time a frame since it last printed (the average, the median, the 95th
// percentile and the worst, ms), the awake bodies on average, and its workers (vr_box3d_threads); then starts counting
// again. A bench: run it, play, run it again (Misc/quakevr/box3dmt/pilebench.py). `bins`: also the frames by the awake
// bodies as their step began (what Physics Threads From compares), each range's frames, median and 95th percentile; and
// Box3D's own profile (ms a frame: where the step went).
void steptime_f()
{
    if(!world)
    {
        Con_Printf("vr_physics_steptime: no physics world\n");
        return;
    }
    const int n = za::max(world->stepFrames, 1);
    za::Vector<float> ms;
    ms.reserve(world->stepSamples.size());
    for(const World::StepSample& x : world->stepSamples)
    {
        ms.pushBack(x.ms);
    }
    za::quickSort(ms.begin(), ms.end());
    const za::SizeT over = static_cast<za::SizeT>(ms.end() - za::upperBound(ms.begin(), ms.end(), 2.f));
    Con_Printf("vr_physics_steptime: %d frames, step %.3f ms (median %.3f, p95 %.3f, p99 %.3f, worst %.3f; %d over 2 ms), "
               "%.1f bodies awake, %d bodies, %d workers\n",
        world->stepFrames, world->stepTime * 1000.0 / n, quantile(ms, 0.5), quantile(ms, 0.95), quantile(ms, 0.99),
        world->stepTimeMax * 1000.0, static_cast<int>(over), static_cast<double>(world->stepAwake) / n,
        b3World_GetCounters(world->id).bodyCount, world->tasks.workers);
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "bins"))
    {
        constexpr int edges[] = {0, 10, 25, 50, 75, 100, 125, 150, 200, 300, 500, 1000, 100000};
        for(za::SizeT b = 0; b + 1 < sizeof(edges) / sizeof(edges[0]); b++)
        {
            ms.clear();
            for(const World::StepSample& x : world->stepSamples)
            {
                if(x.awake >= edges[b] && x.awake < edges[b + 1])
                {
                    ms.pushBack(x.ms);
                }
            }
            if(ms.empty())
            {
                continue;
            }
            za::quickSort(ms.begin(), ms.end());
            Con_Printf("vr_physics_steptime bin %d %d: %d frames, median %.4f, p95 %.4f\n", edges[b], edges[b + 1],
                static_cast<int>(ms.size()), quantile(ms, 0.5), quantile(ms, 0.95));
        }
        const b3Profile& p = world->stepProfile;
        const float k = 1.f / static_cast<float>(n);
        Con_Printf("vr_physics_steptime profile: step %.3f pairs %.3f collide %.3f solve %.3f (setup %.3f constraints %.3f "
                   "[prepare %.3f intvel %.3f warm %.3f impulses %.3f intpos %.3f relax %.3f restitution %.3f store %.3f] "
                   "transforms %.3f split %.3f refit %.3f bullets %.3f sleep %.3f)\n",
            p.step * k, p.pairs * k, p.collide * k, p.solve * k, p.solverSetup * k, p.constraints * k,
            p.prepareConstraints * k, p.integrateVelocities * k, p.warmStart * k, p.solveImpulses * k,
            p.integratePositions * k, p.relaxImpulses * k, p.restitution * k, p.storeImpulses * k, p.transforms * k,
            p.splitIslands * k, p.refit * k, p.bullets * k, p.sleepIslands * k);
    }
    world->stepProfile = b3Profile{};
    world->stepSamples.clear();
    world->stepTime = 0.0;
    world->stepTimeMax = 0.0;
    world->stepFrames = 0;
    world->stepAwake = 0;
}

// vr_physics_frametime [<label>]: each server frame's physics since it last printed, by phase (framePhaseNames; ms: the
// mean, median, 95th and 99th percentiles and the worst), with the wall-clock time and the host frames between the two
// calls (the whole frame's cost, with vr_fixed_frames: a test run's same frames); then starts counting again (the first
// call starts it: no samples kept before). A bench: Misc/quakevr/physbench/physbench.py.
void frametime_f()
{
    frameTiming = true;
    const double now = Sys_DoubleTime();
    const double wall = frameLastWall > 0.0 ? now - frameLastWall : 0.0;
    const int hostFrames = host_framecount - frameLastHostFrame;
    frameLastWall = now;
    frameLastHostFrame = host_framecount;
    const char* label = Cmd_Argc() > 1 ? Cmd_Argv(1) : "-";
    if(!world)
    {
        Con_Printf("vr_physics_frametime %s: no physics world\n", label);
        return;
    }
    const za::Vector<World::FrameSample>& samples = world->frameSamples;
    double awake = 0.0;
    for(const World::FrameSample& x : samples)
    {
        awake += x.awake;
    }
    const b3Counters c = b3World_GetCounters(world->id);
    Con_Printf("vr_physics_frametime %s: %d frames, %d host frames in %.3f s (%.4f ms a host frame), %.1f awake, %d bodies, "
               "%d contacts\n",
        label, static_cast<int>(samples.size()), hostFrames, wall, hostFrames > 0 ? wall * 1000.0 / hostFrames : 0.0,
        samples.empty() ? 0.0 : awake / static_cast<double>(samples.size()), c.bodyCount, c.contactCount);
    za::Vector<float> ms;
    ms.reserve(samples.size());
    for(int p = 0; p < framePhases; p++)
    {
        ms.clear();
        double sum = 0.0;
        for(const World::FrameSample& x : samples)
        {
            ms.pushBack(x.ms[p]);
            sum += x.ms[p];
        }
        za::quickSort(ms.begin(), ms.end());
        Con_Printf("vr_physics_frametime %s %s: mean %.4f median %.4f p95 %.4f p99 %.4f worst %.4f\n", label,
            framePhaseNames[p], ms.empty() ? 0.0 : sum / static_cast<double>(ms.size()), quantile(ms, 0.5), quantile(ms, 0.95),
            quantile(ms, 0.99), ms.empty() ? 0.0 : static_cast<double>(ms.back()));
    }
    world->frameSamples.clear();
}

// vr_physics_mtbench [<bodies> [<steps>]]: Box3D on the pool, alone (ROUND21.md, "Box3D on the pool"). A world of its
// own: a floor, `bodies` boxes (60 cm and 90 cm cubes: a small explosive box, a crate; 200) in leaning columns of 10 that
// topple into a pile, stepped `steps` times (300) at 1/72 s with vr_box3d_substeps, from the same start with each worker
// count (1, 2, 3, 4, 6, 8, 16 and all the pool's, as far as the pool goes): each count's step time, its awake bodies and
// a hash of every body's place, turn and velocities after, which must be the same for every count.
void mtbench_f()
{
    const int bodies = Cmd_Argc() > 1 ? CLAMP(1, Q_atoi(Cmd_Argv(1)), 5000) : 200;
    const int steps = Cmd_Argc() > 2 ? CLAMP(1, Q_atoi(Cmd_Argv(2)), 100000) : 300;
    const int substeps = CLAMP(1, static_cast<int>(vr_box3d_substeps.value), 8);
    const int most = jobs::pool() ? za::min(jobs::workers() + 1, B3_MAX_WORKERS) : 1;
    za::Vector<int> counts;
    for(const int n : {1, 2, 3, 4, 6, 8, 16})
    {
        if(n < most)
        {
            counts.pushBack(n);
        }
    }
    counts.pushBack(most);
    const za::UniquePtr<StepTasks> tasks = za::makeUnique<StepTasks>();
    za::U64 first = 0;
    bool same = true;
    for(const int workers : counts)
    {
        tasks->workers = workers;
        b3WorldDef def = b3DefaultWorldDef();
        def.gravity = b3Vec3{0.f, 0.f, -9.81f};
        def.enableSleep = true;
        def.enableContinuous = true;
        def.isotropicInertiaTolerance = 0.f; // as the game's world (vr_box3d.cpp)
        def.workerCount = static_cast<uint32_t>(workers);
        def.enqueueTask = enqueueStepTask;
        def.finishTask = finishStepTask;
        def.userTaskContext = tasks.get();
        const b3WorldId id = b3CreateWorld(&def);
        b3BodyDef floorDef = b3DefaultBodyDef();
        floorDef.position = b3Pos{0.f, 0.f, -0.5f};
        const b3ShapeDef shape = b3DefaultShapeDef();
        const b3BoxHull floorHull = b3MakeBoxHull(100.f, 100.f, 0.5f);
        b3CreateHullShape(b3CreateBody(id, &floorDef), &shape, &floorHull.base);
        const b3BoxHull small = b3MakeCubeHull(0.3f), crate = b3MakeCubeHull(0.45f);
        za::Vector<b3BodyId> made;
        const int columns = (bodies + 9) / 10, side = za::max(1, static_cast<int>(za::ceil(za::sqrt(static_cast<float>(columns)))));
        for(int i = 0; i < bodies; i++)
        {
            const int column = i / 10, level = i % 10;
            const bool big = column % 2 == 1;
            const float half = big ? 0.45f : 0.3f;
            b3BodyDef body = b3DefaultBodyDef();
            body.type = b3_dynamicBody;
            // Each level 10 cm further along x than the one under it, and turned (as vr_physics_pile): they lean and fall.
            body.position = b3Pos{static_cast<float>(column % side) * 2.5f + static_cast<float>(level) * 0.1f,
                static_cast<float>(column / side) * 2.5f, 0.02f + static_cast<float>(level) * (2.f * half + 0.02f) + half};
            body.rotation = b3MakeQuatFromAxisAngle(b3Vec3{0.f, 0.f, 1.f}, static_cast<float>((i * 37) % 90) * (3.14159265f / 180.f));
            const b3BodyId b = b3CreateBody(id, &body);
            b3CreateHullShape(b, &shape, big ? &crate.base : &small.base);
            made.pushBack(b);
        }
        double time = 0.0;
        za::U64 awake = 0;
        for(int i = 0; i < steps; i++)
        {
            tasks->next.storeRelaxed(0);
            applySimdWidth(id);
            const double t0 = Sys_DoubleTime();
            b3World_Step(id, 1.f / 72.f, substeps);
            time += Sys_DoubleTime() - t0;
            awake += static_cast<za::U64>(b3World_GetAwakeBodyCount(id));
        }
        za::U64 h = 1469598103934665603ull;
        const auto mix = [&h](const void* p, size_t n)
        {
            for(size_t k = 0; k < n; k++)
            {
                h = (h ^ static_cast<const unsigned char*>(p)[k]) * 1099511628211ull;
            }
        };
        for(const b3BodyId b : made)
        {
            const b3WorldTransform x = b3Body_GetTransform(b);
            const b3Vec3 v[2] = {b3Body_GetLinearVelocity(b), b3Body_GetAngularVelocity(b)};
            mix(&x, sizeof(x));
            mix(v, sizeof(v));
        }
        b3DestroyWorld(id);
        if(workers == counts[0])
        {
            first = h;
        }
        same = same && h == first;
        Con_Printf("vr_physics_mtbench: %d bodies, %d steps of %d: %2d workers %.3f ms a step, %.1f awake, hash %016llx\n",
            bodies, steps, substeps, workers, time * 1000.0 / steps, static_cast<double>(awake) / steps,
            static_cast<unsigned long long>(h));
    }
    Con_Printf("vr_physics_mtbench: %s\n", same ? "the same with every worker count" : "DIFFERENT between worker counts");
}

// vr_corpse_list: the corpses in the physics (vr_corpse_collide): each one's body (fixed or pushable, its box or its
// fitted hulls, its mass), where it lies and turns, and what touches it (props, held things and hands' bodies, the level).
// vr_ragdoll_list [1|2]: the ragdolls (vr_ragdoll): each one's entity, how long since it went limp, its parts (awake), where
// its pelvis is and how high its parts' lowest and highest points are; 1: each part, and the hands holding it
// (how far its point is from the hand); 2: also its flames (how far from its limbs' surface).
// A reproducible shoulder-contact stress pose: rotate one corpse arm and its children into its chest.
// Living ragdolls are deliberately excluded. The solver must free the arm without separating its shoulder.
void ragdollArmTest_f()
{
    if(!sv.active || !world) { Con_Printf("armtest: no Box3D world\n"); return; }
    const VmScope vm;
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0 || r.knocked || EDICT_NUM(r.num)->v.health > 0.f) { continue; }
        for(int b = 0; b < r.count; b++)
        {
            const ragdoll::Bone& bone = r.rig->bones[b];
            if(partCut(r, b) || strncmp(bone.name, "upperarm", 8) != 0 || bone.parent < 0) { continue; }
            const float k = r.scale / world->m2u;
            const b3WorldTransform arm = b3Body_GetTransform(r.body[static_cast<za::SizeT>(b)]);
            const glm::vec3 socket = glmv(b3Body_GetWorldPoint(r.body[static_cast<za::SizeT>(b)], b3v(bone.pivot * k)));
            const glm::vec3 target = glmv(b3Body_GetWorldCenter(r.body[static_cast<za::SizeT>(bone.parent)])) - socket;
            const glm::vec3 direction = fromB3(arm.q) * (bone.end - bone.pivot);
            if(glm::length(target) < 1e-4f || glm::length(direction) < 1e-4f) { continue; }
            const glm::vec3 from = glm::normalize(direction), to = glm::normalize(target);
            const glm::vec3 axis = glm::cross(from, to);
            const float cosine = glm::dot(from, to);
            const glm::quat turn = cosine > -0.999f ? glm::normalize(glm::quat{1.f + cosine, axis.x, axis.y, axis.z}) :
                glm::angleAxis(glm::pi<float>(), glm::normalize(glm::cross(from,
                    za::abs(from.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f})));
            for(int child = b; child < r.count; child++)
            {
                int ancestor = child;
                while(ancestor >= 0 && ancestor != b) { ancestor = r.rig->bones[ancestor].parent; }
                if(ancestor != b || partCut(r, child)) { continue; }
                const b3BodyId body = r.body[static_cast<za::SizeT>(child)];
                const b3WorldTransform pose = b3Body_GetTransform(body);
                b3Body_SetTransform(body, b3v(socket + turn * (glmv(pose.p) - socket)),
                    toB3(glm::normalize(turn * fromB3(pose.q))));
                b3Body_SetLinearVelocity(body, b3Vec3_zero);
                b3Body_SetAngularVelocity(body, b3Vec3_zero);
                b3Body_SetAwake(body, true);
            }
            int samples = 0;
            const int inside = armsInsideChest(r, samples);
            Con_Printf("armtest: %d %s folded, %d of %d distal arm points inside chest\n", r.num, bone.name, inside, samples);
            return;
        }
    }
    Con_Printf("armtest: no intact corpse arm\n");
}

// vr_ragdoll_speeds: each ragdoll's fastest since the last ask (its middle's and any part's, units/s: a blow's launch),
// then forgotten (Debug > Tests).
void ragdollSpeeds_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_ragdoll_speeds: no Box3D world\n");
        return;
    }
    const VmScope vm;
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0)
        {
            continue;
        }
        edict_t* ent = EDICT_NUM(r.num);
        Con_Printf("ragspeed: %d %s (%s): middle %.0f u/s, a part %.0f u/s at most\n", r.num, PR_GetString(ent->v.classname),
            ent->v.health > 0.f ? "alive" : "dead", r.peakMiddle * world->m2u, r.peakPart * world->m2u);
        r.peakMiddle = r.peakPart = 0.f;
    }
}

void ragdollList_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_ragdoll_list: no Box3D world\n");
        return;
    }
    const VmScope vm;
    int count = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0)
        {
            continue;
        }
        count++;
        int awake = 0;
        float mass = 0.f;
        glm::vec3 lo{1e30f}, hi{-1e30f};
        int parts = 0;
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            parts++;
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            awake += b3Body_IsAwake(body) ? 1 : 0;
            mass += b3Body_GetMass(body);
            const b3AABB box = b3Body_ComputeAABB(body);
            lo = glm::min(lo, world->toU(box.lowerBound));
            hi = glm::max(hi, world->toU(box.upperBound));
        }
        edict_t* ent = EDICT_NUM(r.num);
        const glm::vec3 p = world->toU(b3Body_GetWorldCenter(r.body[0]));
        Con_Printf("ragdoll %d %s: %.1f s limp, %d parts (%d awake), %.1f kg, pelvis %.1f %.1f %.1f, parts z %.1f .. %.1f, frame %d%s\n", r.num,
            PR_GetString(ent->v.classname), qcvm->time - r.born, parts, awake, mass, p.x, p.y, p.z, lo.z, hi.z,
            static_cast<int>(ent->v.frame), r.cut ? ", headless" : "");
        int pairs = 0, deep = 0;
        const int overlapping = partsOverlapping(r, pairs, deep);
        Con_Printf("  self: %d of its %d unjointed part pairs overlapping, %d deep (Limbs Meet Each Other %s)\n", overlapping, pairs, deep,
            vr_ragdoll_self_collide.value != 0.f ? "on" : "off");
        int armSamples = 0;
        const int buriedArms = armsInsideChest(r, armSamples);
        Con_Printf("  arm interiors: %d of %d distal arm points inside chest\n", buriedArms, armSamples);
        if(r.active.death.on)
        {
            ragactive::describe(r.active, activeParts(r), r.num, qcvm->time);
        }
        if(Cmd_Argc() > 1)
        {
            for(int b = 0; b < r.count; b++)
            {
                if(partCut(r, b))
                {
                    Con_Printf("  %2d %-11s cut off\n", b, r.rig->bones[b].name);
                    continue;
                }
                const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
                const glm::vec3 c = world->toU(b3Body_GetWorldCenter(body));
                const glm::vec3 v = world->toU(b3Body_GetLinearVelocity(body));
                Con_Printf("  %2d %-11s %5.2f kg at %7.1f %7.1f %7.1f, %5.1f u/s%s\n", b, r.rig->bones[b].name, b3Body_GetMass(body), c.x, c.y,
                    c.z, glm::length(v), b3Body_IsAwake(body) ? "" : " (asleep)");
            }
        }
        // vr_ragdoll_list 2: the flames on it (vr_burning.qc's, within its box): how far each is from its limbs' surface.
        if(Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) >= 2)
        {
            int flames = 0;
            float sum = 0.f, most = 0.f;
            for(int e = svs.maxclients + 1; e < qcvm->num_edicts; e++)
            {
                edict_t* fl = EDICT_NUM(e);
                const glm::vec3 at = vec(fl->v.origin);
                if(fl->free || strcmp(PR_GetString(fl->v.classname), "vr_burn_flame") ||
                   glm::any(glm::lessThan(at, lo - glm::vec3{16.f})) || glm::any(glm::greaterThan(at, hi + glm::vec3{16.f})))
                {
                    continue;
                }
                float distance = 0.f;
                (void)nearestPart(r, at, distance);
                flames++;
                sum += distance;
                most = za::max(most, distance);
            }
            Con_Printf("  %d flames on it: %.1f units from its limbs' surface on average, %.1f at most\n", flames,
                sum / static_cast<float>(za::max(flames, 1)), most);
        }
        for(const World::RagdollGrab& g : world->ragdollGrabs)
        {
            if(g.num == r.num && g.player == 0)
            {
                Con_Printf("  held by the physgun: part %d (%s)\n", g.part, r.rig->bones[g.part].name);
            }
            else if(g.num == r.num)
            {
                Con_Printf("  held by client %d's %s hand: part %d (%s), %.1f units from the hand\n",
                    g.player, g.hand ? "main" : "off", g.part, r.rig->bones[g.part].name, grabReach(g.player, g.hand));
            }
        }
    }
    Con_Printf("vr_ragdoll_list: %d ragdolls (vr_ragdoll %d, at most %d); Box3D %d bodies, %d awake\n", count, static_cast<int>(vr_ragdoll.value),
        static_cast<int>(vr_ragdoll_max.value), b3World_GetCounters(world->id).bodyCount, b3World_GetAwakeBodyCount(world->id));
}

// vr_ragdoll_active_list: the active ragdolls (vr_ragdoll_active): each one's drive (how long, its channels' strength,
// its pin, what its death does), and the drive's cost since the last look (ms a frame).
void ragdollActiveList_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_ragdoll_active_list: no Box3D world\n");
        return;
    }
    const VmScope vm;
    int driven = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0 && r.active.death.on)
        {
            ragactive::describe(r.active, activeParts(r), r.num, qcvm->time);
            driven += r.active.on ? 1 : 0;
        }
    }
    double ms = 0.0;
    int frames = 0, calls = 0;
    ragactive::costSince(ms, frames, calls);
    Con_Printf("vr_ragdoll_active_list: %d driven now (vr_ragdoll_active %g); the drive: %.4f ms a frame over %d frames (%d "
               "ragdoll-frames, %.4f ms each)\n",
        driven, static_cast<double>(vr_ragdoll_active.value), frames ? ms / frames : 0.0, frames, calls,
        calls ? ms / calls : 0.0);
}

// vr_ragdoll_env_hit [speed]: each dying hand holding on (a wall, a ledge: vr_ragdoll_active_env) struck as a shot strikes
// it (box3d::push at the hand, `speed` u/s outward from the body, 200 by default): it lets go (Debug > World and Physics;
// the tests).
void ragdollEnvHit_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_ragdoll_env_hit: no Box3D world\n");
        return;
    }
    const VmScope vm;
    const float speed = Cmd_Argc() > 1 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 200.f;
    int struck = 0;
    for(za::SizeT i = 0; i < world->ragdolls.size(); i++)
    {
        RagdollBodies& r = world->ragdolls[i];
        if(r.num <= 0 || !r.active.on)
        {
            continue;
        }
        for(int arm = 0; arm < 2; arm++)
        {
            const ragactive::EnvHand& h = r.active.env.hand[static_cast<za::SizeT>(arm)];
            const int lo = r.active.chain[static_cast<za::SizeT>(arm)][1];
            if(B3_IS_NULL(h.joint) || lo < 0)
            {
                continue;
            }
            const glm::vec3 hand = glmv(b3Body_GetWorldPoint(r.body[static_cast<za::SizeT>(lo)], b3v(r.rig->bones[lo].end * (r.scale / world->m2u)))) * world->m2u;
            const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(r.body[0])) * world->m2u;
            const glm::vec3 away = glm::length(hand - pelvis) > 1e-3f ? glm::normalize(hand - pelvis) : glm::vec3{0.f, 0.f, 1.f};
            qvr::box3d::push(EDICT_NUM(r.num), hand, away * speed, 0.f, 0.f, 0.f);
            Con_Printf("vr_ragdoll_env_hit: %d's %s hand struck at %.0f %.0f %.0f\n", r.num, arm == 0 ? "left" : "right",
                hand.x, hand.y, hand.z);
            struck++;
        }
    }
    if(!struck)
    {
        Con_Printf("vr_ragdoll_env_hit: no hand holding on\n");
    }
}

// vr_ragdoll_blast_test [damage]: a blast's push alone (no damage: nothing gibbed) 24 units beside the ragdoll nearest
// the player, on his side: it is thrown away from him (Debug > Tests).
void ragdollBlastTest_f()
{
    if(!sv.active || !world || svs.maxclients < 1)
    {
        Con_Printf("vr_ragdoll_blast_test: no Box3D world\n");
        return;
    }
    const VmScope vm;
    const glm::vec3 player = vec(svs.clients[0].edict->v.origin);
    RagdollBodies* best = nullptr;
    float bestD = 1e30f;
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0 && glm::distance(world->toU(b3Body_GetWorldCenter(r.body[0])), player) < bestD)
        {
            bestD = glm::distance(world->toU(b3Body_GetWorldCenter(r.body[0])), player);
            best = &r;
        }
    }
    if(!best)
    {
        Con_Printf("vr_ragdoll_blast_test: no ragdoll\n");
        return;
    }
    const float damage = Cmd_Argc() > 1 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 80.f;
    const glm::vec3 c = world->toU(b3Body_GetWorldCenter(best->body[0]));
    glm::vec3 side = player - c;
    side.z = 0.f;
    side = glm::length(side) > 0.01f ? glm::normalize(side) : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 at = c + side * 24.f;
    blastRagdoll(*best, at, damage);
    Con_Printf("vr_ragdoll_blast_test: %.0f at %.0f %.0f %.0f beside ragdoll %d\n", damage, at.x, at.y, at.z, best->num);
}

void corpseList_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_corpse_list: no Box3D world\n");
        return;
    }
    const VmScope vm;
    static constexpr const char* modes[] = {"none", "fixed box", "pushable box", "fixed pose", "pushable pose"};
    int count = 0;
    za::Vector<b3ContactData>& contacts = scratch.pushContacts;
    for(int num = 1; num < qcvm->num_edicts && num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[num];
        if(s.kind != Kind::Corpse || B3_IS_NULL(s.body))
        {
            continue;
        }
        count++;
        edict_t* e = EDICT_NUM(num);
        contacts.resize(static_cast<za::SizeT>(za::max(b3Body_GetContactCapacity(s.body), 1)));
        const int n = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        int props = 0, hands = 0, level = 0;
        for(int c = 0; c < n; c++)
        {
            const b3ContactData& cd = contacts[static_cast<size_t>(c)];
            if(cd.manifoldCount == 0 || cd.manifolds[0].pointCount == 0)
            {
                continue;
            }
            const b3ShapeId other = B3_ID_EQUALS(b3Shape_GetBody(cd.shapeIdA), s.body) ? cd.shapeIdB : cd.shapeIdA;
            const uint64_t category = b3Shape_GetFilter(other).categoryBits;
            props += (category & catProp) ? 1 : 0;
            hands += (category & (catHeld | catReach)) ? 1 : 0;
            level += (category & (catWorld | catMover)) ? 1 : 0;
        }
        Con_Printf("  %d %s: %s, %d shapes, %.0f kg, at %.1f %.1f %.1f yaw %.0f, %s; touching %d props, %d held/hands, %d level\n",
            num, PR_GetString(e->v.classname), modes[CLAMP(0, s.corpseMode, 4)], b3Body_GetShapeCount(s.body),
            s.corpseDynamic ? b3Body_GetMass(s.body) : 0.f, e->v.origin[0], e->v.origin[1], e->v.origin[2], e->v.angles[1],
            b3Body_IsAwake(s.body) ? "awake" : "asleep", props, hands, level);
    }
    Con_Printf("vr_corpse_list: %d corpses in the physics (vr_corpse_collide %d)\n", count, corpseMode());
}

// vr_corpse_poses [1]: every dead monster: its model and frame, where that frame is in its death (a rigged model's), a
// ragdoll or not and why not; a corpse drawn as its model in a frame that is no death's end ("not lying") is counted
// (Debug > Tests; the player's evidence: "Dead ogres standing"). 1: the counts only.
void corpsePoses_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_corpse_poses: no Box3D world\n");
        return;
    }
    const VmScope vm;
    const bool brief = Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) != 0;
    int dead = 0, ragdolls = 0, lying = 0, notLying = 0, placed = 0;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* ent = EDICT_NUM(num);
        // (A mapper's corpse too: misc_corpse, a dead monster's body when vr_corpse_placed is on, else scenery.)
        const bool mapper = !ent->free && (fieldFloatOr(ent, fields().vr_corpse_placed, 0.f) != 0.f ||
                                              !strcmp(PR_GetString(ent->v.classname), "misc_corpse"));
        if(ent->free || !ent->v.modelindex || (!mapper && (!hasFlag(ent, FL_MONSTER) || ent->v.health > 0.f)))
        {
            continue;
        }
        dead++;
        placed += mapper ? 1 : 0;
        qmodel_t* model = modelOf(ent);
        const ragdoll::Rig* rig = ragdoll::eligible(model) ? ragdoll::rigFor(model) : nullptr;
        const int frame = static_cast<int>(ent->v.frame);
        const float progress = rig ? ragdoll::deathProgress(*rig, frame) : -1.f;
        const bool rag = isRagdollSlot(num);
        const float placedState = fieldFloatOr(ent, fields().vr_corpse_placed, 0.f);
        const char* why = rag                                ? "a ragdoll"
                          : mapper && !hasFlag(ent, FL_MONSTER) ? "scenery (vr_corpse_placed 0)"
                          : vr_ragdoll.value < 1.f           ? "vr_ragdoll 0"
                          : placedState == 1.f               ? "placed, asleep till a player comes near"
                          : !ragdoll::eligible(model)        ? "no rig for its model"
                          : !rig                             ? "its rig failed"
                          : !deadMonster(ent)                ? "still dying (solid)"
                          : hasFlag(ent, FL_SWIM)            ? "a swimmer"
                          : roomRefused(num)                 ? "no room (vr_ragdoll_max)"
                          : wantsRagdoll(ent, num)           ? "wanted, not made"
                                                             : "not limp yet";
        // (Mid-death, not a ragdoll: drawn upright or falling. A frame of no death the rig knows counts as lying: the
        // player's other four deaths' ends.)
        const bool flat = rag || !rig || progress < 0.f || progress >= 0.999f;
        ragdolls += rag ? 1 : 0;
        lying += flat ? 1 : 0;
        notLying += flat ? 0 : 1;
        if(!brief)
        {
            Con_Printf("  %d %s: %s frame %d%s, %s%s\n", num, PR_GetString(ent->v.classname), model ? model->name : "(none)", frame,
                rig ? (progress < 0.f ? " (no death frame)" : va(" (%.0f%% of its death)", progress * 100.f)) : "", why,
                flat ? "" : ", NOT LYING");
        }
    }
    Con_Printf("vr_corpse_poses: %d dead monsters (%d placed by the map), %d ragdolls, %d lying, %d not lying (vr_ragdoll %g, "
               "at most %g)\n",
        dead, placed, ragdolls, lying, notLying, vr_ragdoll.value, vr_ragdoll_max.value);
}

// vr_corpse_drop [<height>]: the loose prop nearest the first player put `height` units (32) over the top of the corpse
// nearest him, at rest, to fall on it. For tests (Debug menu: Drop the Nearest Prop on the Nearest Corpse).
void corpseDrop_f()
{
    if(!sv.active || !world || svs.maxclients < 1)
    {
        return;
    }
    const VmScope vm;
    const glm::vec3 eye = vec(EDICT_NUM(1)->v.origin);
    int corpse = 0, prop = 0;
    float corpseD = 1e9f, propD = 1e9f;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[num];
        const float d = glm::distance(eye, vec(EDICT_NUM(num)->v.origin));
        if(s.kind == Kind::Corpse && d < corpseD)
        {
            corpse = num;
            corpseD = d;
        }
        else if(s.kind == Kind::Prop && d < propD)
        {
            prop = num;
            propD = d;
        }
    }
    if(!corpse || !prop)
    {
        Con_Printf("vr_corpse_drop: no %s\n", corpse ? "loose prop" : "corpse in the physics");
        return;
    }
    edict_t* c = EDICT_NUM(corpse);
    edict_t* p = EDICT_NUM(prop);
    const float height = Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : 32.f;
    const b3AABB box = b3Body_ComputeAABB(world->slots[corpse].body);
    const glm::vec3 top = (world->toU(box.lowerBound) + world->toU(box.upperBound)) * 0.5f;
    const float bottom = world->toU(box.upperBound).z + height;
    const glm::vec3 to{top.x - 0.5f * (p->v.mins[0] + p->v.maxs[0]), top.y - 0.5f * (p->v.mins[1] + p->v.maxs[1]), bottom - p->v.mins[2]};
    store(to, p->v.origin);
    p->v.velocity[0] = p->v.velocity[1] = p->v.velocity[2] = 0.f;
    setFlag(p, FL_ONGROUND, false);
    SV_LinkEdict(p, false);
    Con_Printf("vr_corpse_drop: %d %s put over corpse %d %s (its top at %.1f) at %.1f %.1f %.1f\n", prop,
        PR_GetString(p->v.classname), corpse, PR_GetString(c->v.classname), world->toU(box.upperBound).z, to.x, to.y, to.z);
}

// vr_physics_list [<classname | props>]: the rigid bodies (or those), where they are and how they move.
void list_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props");
    Con_Printf("%d, %s:\n", static_cast<int>(list.size()), world ? "Box3D" : "no Box3D world");
    for(edict_t* e : list)
    {
        const int num = NUM_FOR_EDICT(e);
        const char* body = world && num < static_cast<int>(world->slots.size()) ? kindName(world->slots[num].kind) : "-";
        Con_Printf("  %d %s %.1f %.1f %.1f angles %.0f %.0f %.0f vel %.0f %s (%s)%s\n", num, PR_GetString(e->v.classname), e->v.origin[0],
            e->v.origin[1], e->v.origin[2], e->v.angles[0], e->v.angles[1], e->v.angles[2], VectorLength(e->v.velocity),
            hasFlag(e, FL_ONGROUND) ? "asleep" : "awake", body, e->v.takedamage ? va(" health %.0f", e->v.health) : "");
        if(vr_debug_box3d.value)
        {
            const qmodel_t* model = modelOf(e);
            const float mass = qvr::box3d::propMass(e);
            Con_Printf("    %s: %.1f kg%s, thrown x%.2f\n", model ? model->name : "no model", mass,
                model && propMassSetting(model) > 0.f ? " (Held Object Offsets)" : model && weaponMassSetting(e, model) > 0.f ? " (Weapon Weights)" : "",
                qvr::props::throwScale(model ? qvr::props::slotForModel(model) : -1, mass));
            Con_Printf("    movetype %d, solid %d, rigid %d, flags %d\n", static_cast<int>(e->v.movetype), static_cast<int>(e->v.solid),
                isRigid(e) ? 1 : 0, static_cast<int>(e->v.flags));
            // Its Size (Held Object Offsets), for tests: the body's box (the world's axes), its Quake box, and the scale
            // it is drawn at (the client's entity matrix: its first axis' length).
            glm::vec3 body{0.f};
            if(world && num < static_cast<int>(world->slots.size()) && B3_IS_NON_NULL(world->slots[num].body) &&
                b3Body_IsValid(world->slots[num].body))
            {
                const b3AABB box = b3Body_ComputeAABB(world->slots[num].body);
                body = world->toU(box.upperBound) - world->toU(box.lowerBound);
            }
            float drawn = 0.f;
            if(num < cl.num_entities && cl_entities[num].model)
            {
                const entity_t& ce = cl_entities[num];
                vec3_t o, a{-ce.angles[0], ce.angles[1], ce.angles[2]};
                VectorCopy(ce.origin, o);
                float m[16];
                R_EntityMatrix(m, o, a, ce.scale);
                VR_BrushTransform(&ce, m);
                drawn = za::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            }
            Con_Printf("    size x%.2f: body %.1f %.1f %.1f, box %.1f %.1f %.1f, drawn x%.3f\n",
                model ? qvr::props::drawnSize(model) : 1.f, body.x, body.y, body.z, e->v.size[0], e->v.size[1], e->v.size[2],
                drawn);
        }
    }
}

// The extents of `body`'s hull shapes in its own frame (units; false: none), and the depth of `point` (world units)
// inside them (the most negative plane distance: > 0 inside; < 0 outside, roughly how far).
bool hullExtents(b3BodyId body, glm::vec3& lo, glm::vec3& hi, const glm::vec3* point, float* depth)
{
    za::Array<b3ShapeId, maxBodyShapes> shapes;
    const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
    const b3WorldTransform xf = b3Body_GetTransform(body);
    const glm::vec3 at = world->toU(xf.p);
    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    bool any = false;
    float inside = -1e9f;
    for(int i = 0; i < count; i++)
    {
        if(b3Shape_GetType(shapes[i]) != b3_hullShape)
        {
            continue;
        }
        const b3HullData* hull = b3Shape_GetHull(shapes[i]);
        const b3Vec3* p = b3GetHullPoints(hull);
        for(int k = 0; k < hull->vertexCount; k++)
        {
            lo = glm::min(lo, world->toU(p[k]));
            hi = glm::max(hi, world->toU(p[k]));
        }
        any = true;
        if(point && depth)
        {
            const glm::vec3 local = world->toU(b3InvRotateVector(xf.q, world->toM(*point - at)));
            const b3Plane* planes = b3GetHullPlanes(hull);
            float worst = -1e9f;
            for(int k = 0; k < hull->faceCount; k++)
            {
                worst = za::max(worst, glm::dot(glmv(planes[k].normal), local) - planes[k].offset * world->m2u);
            }
            inside = za::max(inside, -worst);
        }
    }
    if(depth)
    {
        *depth = inside;
    }
    return any;
}

// vr_physics_shapes [classname...]: for tests (the reload by contact's collision, ROUND21.md): each such prop's body (its
// hull's extents in its own frame) against its drawn model's (the server's drawn vertices, as the client draws it), and
// the first player's held weapons' reach bodies (their hulls) against the weapon as drawn (the client's view entity) and
// how deep its loading port lies inside the hull. Units and cm.
void shapes_f()
{
    if(!sv.active || !world)
    {
        return;
    }
    const VmScope vm;
    const float cm = 100.f / world->m2u;
    const auto box = [&](const char* what, const glm::vec3& lo, const glm::vec3& hi) {
        const glm::vec3 s = hi - lo;
        Con_Printf("    %s %.2f x %.2f x %.2f units (%.1f x %.1f x %.1f cm)\n", what, s.x, s.y, s.z, s.x * cm, s.y * cm,
            s.z * cm);
    };
    // A gun's convex pieces (gunPieces): how many, and the most their hulls (and its one hull) lie off the drawn gun.
    const auto pieces = [&](const PropHullKey& key, b3BodyId body) {
        const auto it = world->gunReports.find(key);
        if(it == world->gunReports.end())
        {
            Con_Printf("    convex pieces: none (one hull: vr_box3d_gun_pieces %g)\n", vr_box3d_gun_pieces.value);
            return;
        }
        const convex::Report& r = it->second;
        Con_Printf("    convex pieces: %d shapes, made in %.1f ms (cell %.2f units, %d cells); the most a hull lies off the "
                   "gun %.2f units (%.1f cm), one hull %.2f (%.1f cm); volume %.1f, one hull %.1f, the gun %.1f\n",
            b3Body_GetShapeCount(body), r.ms, r.cell, r.cells, r.gap, r.gap * cm, r.wholeGap, r.wholeGap * cm, r.volume,
            r.wholeVolume, r.solid);
    };
    for(int arg = 1; arg < za::max(Cmd_Argc(), 2); arg++)
    {
        for(edict_t* e : entitiesNamed(Cmd_Argc() > arg ? Cmd_Argv(arg) : "props"))
        {
            const int num = NUM_FOR_EDICT(e);
            if(num >= static_cast<int>(world->slots.size()) || B3_IS_NULL(world->slots[num].body) ||
                !b3Body_IsValid(world->slots[num].body))
            {
                continue;
            }
            const qmodel_t* model = modelOf(e);
            Con_Printf("  %d %s (%s, drawn x%.2f):\n", num, PR_GetString(e->v.classname), model ? model->name : "no model",
                model ? qvr::props::drawnSize(model) : 1.f);
            glm::vec3 lo, hi;
            if(hullExtents(world->slots[num].body, lo, hi, nullptr, nullptr))
            {
                box("body (its hull)", lo, hi);
            }
            else
            {
                Con_Printf("    body: a box (not a hull)\n");
            }
            za::Vector<glm::vec3>& vertices = scratch.propVerts;
            if(held::drawnVertices(e, vertices) && !vertices.empty())
            {
                glm::vec3 dlo{1e9f}, dhi{-1e9f};
                for(const glm::vec3& v : vertices)
                {
                    dlo = glm::min(dlo, v);
                    dhi = glm::max(dhi, v);
                }
                box("drawn          ", dlo, dhi);
            }
            box("Quake box      ", glm::vec3{e->v.mins[0], e->v.mins[1], e->v.mins[2]},
                glm::vec3{e->v.maxs[0], e->v.maxs[1], e->v.maxs[2]});
            if(model && isGun(e))
            {
                const Slot& slot = world->slots[num];
                pieces(PropHullKey{model, static_cast<int>(e->v.frame),
                           {slot.mins.x, slot.mins.y, slot.mins.z, slot.maxs.x, slot.maxs.y, slot.maxs.z}},
                    slot.body);
            }
        }
    }
    if(world->hands.size() < 2)
    {
        return;
    }
    const FieldOffsets& f = fields();
    edict_t* player = EDICT_NUM(1);
    for(int h = 0; h < 2; h++)
    {
        const World::HandBody& hb = world->hands[1][static_cast<size_t>(h)];
        if(B3_IS_NULL(hb.reach) || hb.key.what != World::ReachKey::Weapon)
        {
            continue;
        }
        const glm::vec3 port = fieldVec(player, h ? f.loadportpos : f.offloadportpos);
        glm::vec3 lo, hi;
        float depth = 0.f;
        if(!hullExtents(hb.reach, lo, hi, &port, &depth))
        {
            continue;
        }
        Con_Printf("  %s hand's weapon %s:\n", h ? "main" : "off", hb.key.model ? hb.key.model->name : "?");
        box("reach body (its hull)", lo, hi);
        // As drawn: the frame's vertices through the view entity (view::modelPoint), into the reach body's frame.
        if(const view::ViewEntity* ve = view::heldWeapon(h); ve && ve->ent.model == hb.key.model)
        {
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ve->ent.model));
            const auto* verts = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes) +
                                hdr->frames[0].firstpose * hdr->numverts;
            const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
            const glm::vec3 at = world->toU(xf.p);
            glm::vec3 dlo{1e9f}, dhi{-1e9f};
            for(int k = 0; k < hdr->numverts; k++)
            {
                const glm::vec3 m{hdr->scale_origin[0] + hdr->scale[0] * verts[k].v[0],
                    hdr->scale_origin[1] + hdr->scale[1] * verts[k].v[1], hdr->scale_origin[2] + hdr->scale[2] * verts[k].v[2]};
                const glm::vec3 local = world->toU(b3InvRotateVector(xf.q, world->toM(view::modelPoint(*ve, m) - at)));
                dlo = glm::min(dlo, local);
                dhi = glm::max(dhi, local);
            }
            box("drawn                ", dlo, dhi);
            Con_Printf("    the hull's offset from the drawn: lo %.2f %.2f %.2f, hi %.2f %.2f %.2f units\n", lo.x - dlo.x,
                lo.y - dlo.y, lo.z - dlo.z, hi.x - dhi.x, hi.y - dhi.y, hi.z - dhi.z);
        }
        Con_Printf("    its loading port %s the hull: %.2f units (%.1f cm)\n", depth > 0.f ? "inside" : "outside",
            za::fabs(depth), za::fabs(depth) * cm);
        pieces(PropHullKey{hb.key.model, hb.key.mirrored ? -3 : -2, {}}, hb.reach);
    }
}

// vr_physics_forcegrab <number | classname | props>: whether the force grab may take each (QC's VR_Forcegrab_IsEligible,
// as the first player's hands search), for tests: an explosive box never.
void forcegrabCheck_f()
{
    if(!sv.active || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_physics_forcegrab <number | classname | props>\n");
        return;
    }
    const VmScope vm;
    const func_t fn = qvr::progs::findFunction("VR_Forcegrab_IsEligible");
    if(!fn)
    {
        Con_Printf("vr_physics_forcegrab: no VR_Forcegrab_IsEligible in the progs\n");
        return;
    }
    for(edict_t* e : entitiesNamed(Cmd_Argv(1)))
    {
        const int oldSelf = pr_global_struct->self;
        pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
        pr_global_struct->time = qcvm->time;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
        PR_ExecuteProgram(fn);
        const bool eligible = G_FLOAT(OFS_RETURN) != 0.f;
        pr_global_struct->self = oldSelf;
        Con_Printf("vr_physics_forcegrab: %d %s: %s\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname),
            eligible ? "may be force grabbed" : "never force grabbed");
    }
}

// vr_physics_spawn <classname> [<distance> [<left>]]: a map entity made by its spawn function (a key, a weapon, the
// biosuit, a powerup...) on the floor `distance` units (48) ahead of the first player and `left` units to the left, as
// the map would place it there, turned as vr_test_spawn_facing says. For tests.
void spawn_f()
{
    if(!sv.active || Cmd_Argc() < 2 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_spawn <classname> [<distance> [<left>]]\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    vec3_t yaw{0.f, player->v.angles[1], 0.f};
    vec3_t forward, right, up;
    AngleVectors(yaw, forward, right, up);
    const bool reach = Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "reach");
    const float distance = Cmd_Argc() > 2 && !reach ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 48.f;
    const float left = Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : 0.f;
    glm::vec3 origin{0.f}, angles{0.f};
    for(int i = 0; i < 3; i++)
    {
        origin[i] = player->v.origin[i] + forward[i] * distance - right[i] * left;
    }
    // "reach" (the Debug menu's pickups): in front of you at arm's reach, a little below the eyes (where the head is and
    // faces, not the player's origin and angles: those stray in room scale), lying level, its muzzle away; never through
    // a wall (pulled back short of it), nor in your own box (a prop there was pushed off it, or hit you). It falls from
    // there to the floor before you.
    const hands::State& hs = hands::current();
    if(reach && hs.valid)
    {
        const float m2u = units::metresToUnits();
        glm::vec3 f, r, u;
        hands::angleVectors({0.f, hs.headAngles.y, 0.f}, f, r, u);
        const glm::vec3 from = hs.head;
        const glm::vec3 lo = vec(player->v.absmin) - glm::vec3{8.f}, hi = vec(player->v.absmax) + glm::vec3{8.f};
        float ahead = 0.45f * m2u;
        for(; ahead < 1.f * m2u; ahead += 1.f)
        {
            const glm::vec3 p = from + f * ahead - r * left;
            if(p.x < lo.x || p.x > hi.x || p.y < lo.y || p.y > hi.y)
            {
                break;
            }
        }
        const glm::vec3 to = from + f * ahead - glm::vec3{0.f, 0.f, 0.35f * m2u} - r * left;
        vec3_t a, b;
        store(from, a);
        store(to, b);
        const trace_t tr = SV_Move(a, vec3_origin, vec3_origin, b, MOVE_NOMONSTERS, player);
        const glm::vec3 end{tr.endpos[0], tr.endpos[1], tr.endpos[2]};
        origin = tr.fraction < 1.f ? end - glm::normalize(to - from) * (0.1f * m2u) : to;
        angles = {0.f, hs.headAngles.y, 0.f};
        Con_Printf("vr_physics_spawn: %s at reach: %.0f %.0f %.0f, %.2f m from your eyes, %.2f m ahead\n", Cmd_Argv(1),
            origin.x, origin.y, origin.z, glm::distance(from, origin) / m2u, glm::dot(origin - from, f) / m2u);
    }
    // vr_test_spawn_facing 1: facing the player, 2: facing away (where the player faces); else the spawn's own (0),
    // as before. Set before its spawn function (a monster's ideal_yaw is taken from it).
    // (4: its left side to the player, 5: its right side.)
    const float facing = vr_test_spawn_facing.value;
    if(facing == 1.f || facing == 2.f || facing == 4.f || facing == 5.f)
    {
        angles.y = anglemod(player->v.angles[1] + (facing == 1.f ? 180.f : facing == 2.f ? 0.f : facing == 4.f ? 90.f : -90.f));
    }
    edict_t* e = qvr::box3d::spawnClass(Cmd_Argv(1), origin, angles);
    if(e)
    {
        Con_Printf("vr_physics_spawn: %d %s at %.0f %.0f %.0f\n", NUM_FOR_EDICT(e), Cmd_Argv(1), e->v.origin[0], e->v.origin[1], e->v.origin[2]);
    }
}

// vr_physics_fling <number | classname | props | nearest> <speed> [<yaw> [<up>]]: sets their velocity, `speed` units/s
// along `yaw` (degrees; the player's view by default) and `up` units/s up (0): a prop sent skidding along the floor (its
// scrape: the physics sounds), or tossed. `nearest`: the loose prop nearest the player. For tests (Debug menu: Slide the
// Nearest Prop).
void fling_f()
{
    if(!sv.active || Cmd_Argc() < 3 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_fling <number | classname | props | nearest> <speed> [<yaw> [<up>]]\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    za::Vector<edict_t*> list;
    if(!strcmp(Cmd_Argv(1), "nearest"))
    {
        if(const int found = qvr::box3d::nearestProp())
        {
            list.pushBack(EDICT_NUM(found));
        }
    }
    else
    {
        list = entitiesNamed(Cmd_Argv(1));
    }
    const float speed = static_cast<float>(Q_atof(Cmd_Argv(2)));
    const float yaw = glm::radians(Cmd_Argc() > 3 ? static_cast<float>(Q_atof(Cmd_Argv(3))) : player->v.v_angle[1]);
    const float up = Cmd_Argc() > 4 ? static_cast<float>(Q_atof(Cmd_Argv(4))) : 0.f;
    for(edict_t* e : list)
    {
        store(glm::vec3{za::cos(yaw) * speed, za::sin(yaw) * speed, up}, e->v.velocity);
        setFlag(e, FL_ONGROUND, false);
        // A pushable corpse (not a ragdoll): its body moves it, not its entity's velocity.
        const int n = NUM_FOR_EDICT(e);
        if(world && n < static_cast<int>(world->slots.size()) && world->slots[n].kind == Kind::Corpse &&
            world->slots[n].ragdoll < 0 && world->slots[n].corpseDynamic && B3_IS_NON_NULL(world->slots[n].body) &&
            b3Body_IsValid(world->slots[n].body))
        {
            b3Body_SetLinearVelocity(world->slots[n].body, world->toM(vec(e->v.velocity)));
            b3Body_SetAwake(world->slots[n].body, true);
            world->slots[n].asleep = false;
        }
        Con_Printf("vr_physics_fling: %d %s at %.0f %.0f %.0f u/s\n", NUM_FOR_EDICT(e), PR_GetString(e->v.classname),
            e->v.velocity[0], e->v.velocity[1], e->v.velocity[2]);
    }
    if(list.empty())
    {
        Con_Printf("vr_physics_fling: none\n");
    }
}

// vr_physics_sink [<number | classname | props>]: how far each one's drawn model is inside the floor under it: its
// drawn surface's lowest corners (held::drawnVertices, where the entity is) against the floor straight below them (a
// point trace down from 24 units above), and the Box3D shape's lowest point (its hull's corners where the body is). A
// positive "sunk" is into the floor, negative above it; "hull" is how far the collision shape's bottom is below (-) or
// above (+) the drawn model's. The summary's average and most are of those within 4 units of the floor (lying on it,
// not on a box).
void sink_f()
{
    if(!sv.active)
    {
        return;
    }
    const VmScope vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props");
    za::Vector<glm::vec3> vertices; // (a debug command's: made each call)
    float worst = 0.f, total = 0.f;
    int counted = 0;
    for(edict_t* e : list)
    {
        qmodel_t* model = modelOf(e);
        if(!model || !held::drawnVertices(e, vertices) || vertices.empty())
        {
            continue;
        }
        const int num = NUM_FOR_EDICT(e);
        const glm::mat3 axes = held::axesFromAngles(e->v.angles, model->type == mod_brush);
        const glm::vec3 origin = vec(e->v.origin);
        float low = 1e9f;
        for(glm::vec3& v : vertices)
        {
            v = origin + axes * v;
            low = za::min(low, v.z);
        }
        // The floor under the lowest corners (those within 2 units of the lowest: a gun on its side rests on several).
        float sunk = -1e9f;
        int traced = 0;
        for(const glm::vec3& v : vertices)
        {
            if(v.z > low + 2.f || traced >= 64)
            {
                continue;
            }
            traced++;
            // In a solid: how far under the surface above it (the floor it is sunk into); else how far above the
            // floor under it (negative).
            vec3_t at{v.x, v.y, v.z};
            const bool inside = SV_Move(at, vec3_origin, vec3_origin, at, MOVE_NOMONSTERS, e).startsolid;
            vec3_t start{v.x, v.y, v.z + (inside ? 24.f : 0.01f)}, end{v.x, v.y, inside ? v.z : v.z - 24.f};
            const trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, e);
            if(tr.fraction < 1.f && !tr.startsolid && tr.plane.normal[2] > 0.7f)
            {
                sunk = za::max(sunk, tr.endpos[2] - v.z);
            }
        }
        float hullLow = 1e9f;
        if(world && num < static_cast<int>(world->slots.size()) &&
            B3_IS_NON_NULL(world->slots[num].body) && b3Body_IsValid(world->slots[num].body))
        {
            const b3BodyId body = world->slots[num].body;
            const b3WorldTransform xf = b3Body_GetTransform(body);
            b3ShapeId shapes[maxBodyShapes];
            const int n = b3Body_GetShapes(body, shapes, maxBodyShapes);
            for(int i = 0; i < n; i++)
            {
                const b3HullData* hull = b3Shape_GetType(shapes[i]) == b3_hullShape ? b3Shape_GetHull(shapes[i]) : nullptr;
                const b3Vec3* points = hull ? b3GetHullPoints(hull) : nullptr;
                for(int k = 0; points && k < hull->vertexCount; k++)
                {
                    const b3Vec3 p = b3RotateVector(xf.q, points[k]);
                    hullLow = za::min(hullLow, (p.z + xf.p.z) * world->m2u);
                }
            }
        }
        const bool onFloor = sunk > -1e8f;
        const char* kind = world && num < static_cast<int>(world->slots.size()) ? kindName(world->slots[num].kind) : "-";
        Con_Printf("  %d %s z %.2f drawn low %.2f", num, PR_GetString(e->v.classname), e->v.origin[2], low);
        Con_Printf(onFloor ? " sunk %.2f" : " (no floor)", sunk);
        if(hullLow < 1e8f)
        {
            Con_Printf(" hull %+.2f", hullLow - low);
        }
        Con_Printf(" %s (%s)\n", hasFlag(e, FL_ONGROUND) ? "asleep" : "awake", kind);
        if(onFloor && sunk > -4.f)
        {
            worst = za::max(worst, sunk);
            total += sunk;
            counted++;
        }
    }
    Con_Printf("vr_physics_sink: %d on a floor, sunk %.2f on average, %.2f at most\n", counted,
        counted ? total / static_cast<float>(counted) : 0.f, worst);
}

void player_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        return;
    }
    const VmScope vm;
    edict_t* p = EDICT_NUM(1);
    if(Cmd_Argc() > 2 && !strcmp(Cmd_Argv(1), "onto"))
    {
        const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(2));
        if(list.empty())
        {
            Con_Printf("vr_physics_player: no %s\n", Cmd_Argv(2));
            return;
        }
        const edict_t* e = list.front();
        p->v.origin[0] = (e->v.absmin[0] + e->v.absmax[0]) * 0.5f;
        p->v.origin[1] = (e->v.absmin[1] + e->v.absmax[1]) * 0.5f;
        // (onto <entity> <height>: his feet that far over its box's top, 1 by default; under 0, into it: his feet in a lying
        // box's top, as it rocks up into them.)
        p->v.origin[2] = e->v.absmax[2] - p->v.mins[2] + (Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 1.f);
        VectorCopy(vec3_origin, p->v.velocity);
        VectorCopy(p->v.origin, p->v.oldorigin); // (SV_CheckStuck's last free spot: here, not where he was before)
        p->v.movetype = MOVETYPE_WALK;
        setFlag(p, FL_ONGROUND, false);
        SV_LinkEdict(p, false);
    }
    else if(Cmd_Argc() > 4 && !strcmp(Cmd_Argv(1), "near"))
    {
        // near <number | classname> <direction> <distance>: on the floor the entity's box stands on, <distance> units
        // from its middle towards <direction> (degrees of yaw), facing it (a run-up at a prop from a side).
        const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argv(2));
        if(list.empty())
        {
            Con_Printf("vr_physics_player: no %s\n", Cmd_Argv(2));
            return;
        }
        const edict_t* e = list.front();
        const float a = glm::radians(Q_atof(Cmd_Argv(3))), d = Q_atof(Cmd_Argv(4));
        p->v.origin[0] = (e->v.absmin[0] + e->v.absmax[0]) * 0.5f + d * za::cos(a);
        p->v.origin[1] = (e->v.absmin[1] + e->v.absmax[1]) * 0.5f + d * za::sin(a);
        p->v.origin[2] = e->v.absmin[2] - p->v.mins[2] + 1.f;
        p->v.angles[0] = p->v.angles[2] = 0.f;
        p->v.angles[1] = anglemod(Q_atof(Cmd_Argv(3)) + 180.f);
        p->v.fixangle = 1;
        VectorCopy(vec3_origin, p->v.velocity);
        VectorCopy(p->v.origin, p->v.oldorigin); // (SV_CheckStuck's last free spot: here, not where he was before)
        p->v.movetype = MOVETYPE_WALK;
        setFlag(p, FL_ONGROUND, false);
        SV_LinkEdict(p, false);
    }
    edict_t* g = PROG_TO_EDICT(p->v.groundentity);
    Con_Printf("vr_physics_player: %.2f s at %.2f %.2f %.2f vel %.1f %.1f %.1f %s on %d %s\n", qcvm->time, p->v.origin[0], p->v.origin[1],
        p->v.origin[2], p->v.velocity[0], p->v.velocity[1], p->v.velocity[2],
        (static_cast<int>(p->v.flags) & FL_ONGROUND) ? "grounded" : "in the air", NUM_FOR_EDICT(g),
        g == qcvm->edicts ? "the world" : PR_GetString(g->v.classname));
    // What is under the feet: the player's box moved 2 units down.
    vec3_t down{p->v.origin[0], p->v.origin[1], p->v.origin[2] - 2.f};
    const trace_t tr = SV_Move(p->v.origin, p->v.mins, p->v.maxs, down, MOVE_NORMAL, p);
    Con_Printf("vr_physics_player: below: fraction %.3f%s%s normal z %.2f on %d, movetype %d\n", tr.fraction,
        tr.startsolid ? " startsolid" : "", tr.allsolid ? " allsolid" : "", tr.plane.normal[2], tr.ent ? NUM_FOR_EDICT(tr.ent) : -1,
        static_cast<int>(p->v.movetype));
    playerOverProp(p); // (below: the players' shape against props)
}

void inLevel_f(); // (below: the Box3D queries it uses are)
void approach_f(); // (below, with the players' shape against props)
void shotBench_f(); // (below, with the shots' shape against props)
void fire_f();
void knockdownTest_f();
// vr_knockdown_test 9's parts' places at its last call (by edict and part; x 1e30: none), to tell how far they moved.
struct WiggleCheck
{
    za::Vector<glm::vec3> last;
    double time{-1.0};
};
WiggleCheck wiggleCheck;
void shockCheck_f();
void inside_f();

bool commandsRegistered = false; // (registerCommands: once)

void portalInfo_f()
{
    const VmScope vm;
    Con_Printf("physicsportals: generation=%d current=%d copies=%d\n", world ? world->generation : -1,
        worldGeneration(), world ? static_cast<int>(world->portalCopies.size()) : 0);
    if(world)
    {
        for(const World::PortalCopy& c : world->portalCopies)
        {
            const glm::vec3 here = world->toU(b3Body_GetPosition(c.original));
            const glm::vec3 there = world->toU(b3Body_GetPosition(c.copy));
            Con_Printf("physicsportals: %d %s at %.1f %.1f %.1f -> %.1f %.1f %.1f\n",
                static_cast<int>(reinterpret_cast<uintptr_t>(b3Body_GetUserData(c.original))), c.dynamic ? "dynamic" : "held",
                here.x, here.y, here.z, there.x, there.y, there.z);
        }
    }
}

void* heapAllocate(size_t size, int32_t alignment) { return VR_HeapAlignedAlloc(size, static_cast<size_t>(alignment)); }
void heapFree(void* p, size_t) { VR_HeapAlignedFree(p); }

// vr_gun_pieces_cache_stats: the guns' pieces this session, read from the disk or cut (Debug > Profiling and Memory).
void gunPiecesCacheStats_f()
{
    const GunPiecesDisk& d = gunPiecesDisk;
    Con_Printf("vr_gun_pieces_cache %g: %d guns' pieces read from the disk, %d cut, %d files written; checked %d, %d "
               "differed\n",
        vr_gun_pieces_cache.value, d.read.load(), d.cut.load(), d.written.load(), d.checked.load(), d.differed.load());
}
} // namespace

void registerCommands()
{
    bool& registered = commandsRegistered;
    if(!registered)
    {
        registered = true;
        b3SetAllocator(heapAllocate, heapFree);
        const b3Version v = b3GetVersion();
        Con_Printf("box3d: Box3D %d.%d.%d, %s\n", v.major, v.minor, v.revision,
            !cpuHasAvx2              ? "SSE2 kernels (4 lanes; no AVX2 on this CPU)"
            : vr_box3d_avx2.value    ? "AVX2 kernels (8 lanes)"
                                     : "SSE2 kernels (4 lanes; vr_box3d_avx2 0)");
        Cmd_AddCommand("vr_physics_player", player_f);
        Cmd_AddCommand("vr_gun_pieces_cache_stats", gunPiecesCacheStats_f);
        Cmd_AddCommand("vr_physics_stack", stack_f);
        Cmd_AddCommand("vr_physics_pyramid", pyramid_f);
        Cmd_AddCommand("vr_physics_list", list_f);
        Cmd_AddCommand("vr_physics_shapes", shapes_f);
        Cmd_AddCommand("vr_physics_loose", loose_f);
        Cmd_AddCommand("vr_physics_pile", pile_f);
        Cmd_AddCommand("vr_physics_bigpile", bigPile_f);
        Cmd_AddCommand("vr_physics_clearpiles", clearPiles_f);
        Cmd_AddCommand("vr_physics_hash", hash_f);
        Cmd_AddCommand("vr_struggle_status", struggleStatus_f);
        Cmd_AddCommand("vr_physics_hull_stats", hullStats_f);
        Cmd_AddCommand("vr_physics_steptime", steptime_f);
        Cmd_AddCommand("vr_physics_frametime", frametime_f);
        Cmd_AddCommand("vr_physics_mtbench", mtbench_f);
        Cmd_AddCommand("vr_physics_blast", blast_f);
        Cmd_AddCommand("vr_physics_sink", sink_f);
        Cmd_AddCommand("vr_physics_inlevel", inLevel_f);
        Cmd_AddCommand("vr_physics_approach", approach_f);
        Cmd_AddCommand("vr_physics_shotbench", shotBench_f);
        Cmd_AddCommand("vr_physics_fire", fire_f);
        Cmd_AddCommand("vr_knockdown_test", knockdownTest_f);
        Cmd_AddCommand("vr_shock_ragdoll_check", shockCheck_f);
        Cmd_AddCommand("vr_physics_inside", inside_f);
        Cmd_AddCommand("vr_physics_spawn", spawn_f);
        Cmd_AddCommand("vr_physics_fling", fling_f);
        Cmd_AddCommand("vr_physics_forcegrab", forcegrabCheck_f);
        Cmd_AddCommand("vr_physics_portals", portalInfo_f);
        Cmd_AddCommand("vr_corpse_list", corpseList_f);
        Cmd_AddCommand("vr_corpse_drop", corpseDrop_f);
        Cmd_AddCommand("vr_corpse_poses", corpsePoses_f);
        Cmd_AddCommand("vr_ragdoll_list", ragdollList_f);
        Cmd_AddCommand("vr_ragdoll_speeds", ragdollSpeeds_f);
        Cmd_AddCommand("vr_ragdoll_arm_test", ragdollArmTest_f);
        Cmd_AddCommand("vr_ragdoll_info", ragdoll::info_f);
        Cmd_AddCommand("vr_limb_models", limbmodel::info_f);
        Cmd_AddCommand("vr_drawn_motion_test", ragdoll::motionTest_f);
        Cmd_AddCommand("vr_ragdoll_blast_test", ragdollBlastTest_f);
        Cmd_AddCommand("vr_ragdoll_active_list", ragdollActiveList_f);

        Cmd_AddCommand("vr_ragdoll_env_hit", ragdollEnvHit_f);
    }
}

namespace
{
// vr_physics_inlevel [<number | classname | props>]: how far each prop's box (its drawn one, as it is turned) is inside
// the level's solids (the world and its brush entities), to half a unit; 0: clear (touching isn't in). A prop held in
// both hands pushed into a wall (vr_carry_two_hands_solid) should stay at 0.
void inLevel_f()
{
    if(!sv.active || !world)
    {
        return;
    }
    const VmScope vm;
    for(edict_t* e : entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "props"))
    {
        const int num = NUM_FOR_EDICT(e);
        if(num >= static_cast<int>(world->slots.size()))
        {
            continue;
        }
        const Slot& s = world->slots[static_cast<size_t>(num)];
        glm::vec3 lo, hi;
        qvr::box3d::ownBox(s, lo, hi);
        if(B3_IS_NULL(s.body) || hi.x <= lo.x)
        {
            continue;
        }
        const glm::quat rot = turnOf(e->v.angles, s.brush);
        const glm::vec3 at = vec(e->v.origin);
        const float most = 0.5f * za::min(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
        float depth = 0.f;
        for(float in = 0.5f; in < most; in += 0.5f)
        {
            if(!qvr::box3d::boxInLevel(lo, hi, at, rot, in))
            {
                break;
            }
            depth = in;
        }
        Con_Printf("vr_physics_inlevel: %d %s at %.1f %.1f %.1f: in the level by %.1f units%s (its box %.1f %.1f %.1f to %.1f %.1f %.1f)\n",
            num, PR_GetString(e->v.classname), at.x, at.y, at.z, depth, s.kind == Kind::Held ? " (held)" : "", lo.x, lo.y,
            lo.z, hi.x, hi.y, hi.z);
    }
}

// vr_physics_approach [number | classname]: the first player's box moved (SV_Move, as play moves it) at a solid prop
// (the first explosive box) from 16 directions round it, level, its feet 2 units over the prop's bottom: how far his
// centre stops from its drawn box, turned (vr_hull_approach for walls: his box's half-width).
void approach_f()
{
    if(!sv.active || svs.maxclients < 1 || !world)
    {
        return;
    }
    const VmScope vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
    edict_t* prop = list.empty() ? nullptr : list.front();
    if(!prop || !solidProp(NUM_FOR_EDICT(prop)))
    {
        Con_Printf("vr_physics_approach: no solid prop %s\n", Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
        return;
    }
    edict_t* p = EDICT_NUM(1);
    vec3_t mins, maxs;
    if(!VR_HullEntBox(p, p->v.mins, p->v.maxs, mins, maxs))
    {
        VectorCopy(p->v.mins, mins);
        VectorCopy(p->v.maxs, maxs);
    }
    const PropShape shape = propShape(prop, world->slots[NUM_FOR_EDICT(prop)], mins, maxs);
    const float reach = glm::length(glm::vec3{shape.half}) + 48.f;
    const float z = prop->v.absmin[2] - p->v.mins[2] + 2.f;
    int met = 0;
    double lo = 1e300, hi = -1e300;
    for(int k = 0; k < 16; k++)
    {
        const float a = static_cast<float>(k) * glm::pi<float>() / 8.f;
        vec3_t s{static_cast<float>(shape.centre.x) + za::cos(a) * reach, static_cast<float>(shape.centre.y) + za::sin(a) * reach, z};
        vec3_t e{static_cast<float>(shape.centre.x), static_cast<float>(shape.centre.y), z};
        if(SV_Move(s, p->v.mins, p->v.maxs, s, MOVE_NORMAL, p).startsolid)
        {
            continue;
        }
        const trace_t tr = SV_Move(s, p->v.mins, p->v.maxs, e, MOVE_NORMAL, p);
        if(tr.ent != prop)
        {
            Con_Printf("vr_physics_approach: from %.1f deg stopped by %s first\n", k * 22.5f,
                tr.ent ? PR_GetString(tr.ent->v.classname) : "nothing");
            continue;
        }
        const double gap = columnGap(shape, glm::dvec3{tr.endpos[0], tr.endpos[1], tr.endpos[2]});
        lo = za::min(lo, gap);
        hi = za::max(hi, gap);
        met++;
        if(vr_debug_box3d.value)
        {
            Con_Printf("vr_physics_approach: from %.1f deg: %.2f units, normal %.2f %.2f %.2f\n", k * 22.5f, gap,
                tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]);
        }
    }
    Con_Printf("vr_physics_approach: %d %s (angles %.1f %.1f %.1f): %d of 16 met it, your centre %.2f to %.2f units from its "
               "face (your half-width %.1f; its shape %s)\n",
        NUM_FOR_EDICT(prop), PR_GetString(prop->v.classname), prop->v.angles[0], prop->v.angles[1], prop->v.angles[2], met,
        met ? lo : 0.0, met ? hi : 0.0, 0.5f * (maxs[0] - mins[0]), vr_box3d_player_shape.value ? "as drawn" : "off: its box");
}

void inside_f()
{
    if(Cmd_Argc() > 1)
    {
        insideWatch = InsideWatch{};
        insideWatch.on = Q_atoi(Cmd_Argv(1)) != 0;
        Con_Printf("vr_physics_inside: watch %s\n", insideWatch.on ? "on" : "off");
        return;
    }
    Con_Printf("vr_physics_inside: %s: %d times in a prop, %d of %d frames (at most %d in a row), deepest %.2f units\n",
        insideWatch.on ? "on" : "off", insideWatch.times, insideWatch.inside, insideWatch.frames, insideWatch.longest,
        insideWatch.deepest);
}

// vr_physics_shotbench [<count>]: the cost of a shot's or a missile's clip against the first solid prop (vr_box3d_shot_shape):
// `count` lines (100000 by default) across its box round it, each as a point's shape clip alone, and as a missile's
// whole trace (SV_Move, MOVE_MISSILE: the world and every entity near it) with the option on and off. For tests.
void shotBench_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_physics_shotbench: no physics world\n");
        return;
    }
    const VmScope vm;
    int g = 0;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts && !g; i++)
    {
        g = solidProp(i) ? i : 0;
    }
    if(!g)
    {
        Con_Printf("vr_physics_shotbench: no solid prop\n");
        return;
    }
    edict_t* prop = EDICT_NUM(g);
    const int count = Cmd_Argc() > 1 ? za::max(1, Q_atoi(Cmd_Argv(1))) : 100000;
    // The lines: from 64 units outside its box round it, through a point inside that box, on (a fixed sequence).
    const glm::vec3 lo = vec(prop->v.absmin), hi = vec(prop->v.absmax), mid = (lo + hi) * 0.5f;
    const auto line = [&](int i, vec3_t a, vec3_t b) {
        const float u = static_cast<float>((i * 7919) % 1000) / 1000.f, v = static_cast<float>((i * 104729) % 1000) / 1000.f;
        const float yaw = static_cast<float>(i) * 2.399963f;
        const glm::vec3 through = lo + (hi - lo) * glm::vec3{u, v, 0.5f * (u + v)};
        const glm::vec3 dir{za::cos(yaw), za::sin(yaw), 0.3f * (v - 0.5f)};
        const float reach = glm::length(hi - mid) + 64.f;
        for(int k = 0; k < 3; k++)
        {
            a[k] = through[k] - dir[k] * reach;
            b[k] = through[k] + dir[k] * reach;
        }
    };
    const float zero[3]{0.f, 0.f, 0.f};
    int hits = 0;
    double t0 = Sys_DoubleTime();
    for(int i = 0; i < count; i++)
    {
        vec3_t a, b;
        line(i, a, b);
        trace_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.fraction = 1.f;
        const ShotShape p = shotShape(prop, world->slots[static_cast<size_t>(g)], zero, zero);
        hits += traceProp(p, glm::dvec3{a[0], a[1], a[2]}, glm::dvec3{b[0], b[1], b[2]}, tr) ? 1 : 0;
    }
    const double clip = (Sys_DoubleTime() - t0) * 1e9 / count;
    const float saved = vr_box3d_shot_shape.value;
    double move[2]{0.0, 0.0};
    int met[2]{0, 0};
    for(int on = 0; on < 2; on++)
    {
        Cvar_SetValueQuick(&vr_box3d_shot_shape, static_cast<float>(on));
        t0 = Sys_DoubleTime();
        for(int i = 0; i < count; i++)
        {
            vec3_t a, b, m{0.f, 0.f, 0.f};
            line(i, a, b);
            met[on] += SV_Move(a, m, m, b, MOVE_MISSILE, nullptr).ent == prop ? 1 : 0;
        }
        move[on] = (Sys_DoubleTime() - t0) * 1e9 / count;
    }
    Cvar_SetValueQuick(&vr_box3d_shot_shape, saved);
    Con_Printf("vr_physics_shotbench: prop %d, %d lines: a point's shape clip %.0f ns (%d hit its shape); a missile's whole "
               "trace %.0f ns with the shape (%d met it), %.0f ns with its box (%d met it)\n",
        g, count, clip, hits, move[1], met[1], move[0], met[0]);
}

// vr_physics_fire <kind> [<x> <y> <z>]: a missile or a shot of yours (QC's VR_Test_Fire), from your eyes at the world
// point, or at the middle of the nearest solid prop: 0 a rocket, 1 a nail, 2 a grenade, 3 a super nail, 4 an enforcer's
// laser, 10 a shotgun pellet (the burst rifle's, the lightning's trace). vr_debug_missiles prints where a missile hits. For
// tests (shots against a turned prop's shape).
void fire_f()
{
    if(!sv.active || Cmd_Argc() < 2 || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_physics_fire <kind> [<x> <y> <z>]\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    dfunction_t* fn = nullptr;
    for(int i = 1; i < qcvm->progs->numfunctions && !fn; i++)
    {
        fn = !strcmp(PR_GetString(qcvm->functions[i].s_name), "VR_Test_Fire") ? &qcvm->functions[i] : nullptr;
    }
    if(!fn || player->free)
    {
        Con_Printf("vr_physics_fire: no VR_Test_Fire in the progs, or no player\n");
        return;
    }
    glm::vec3 at{0.f};
    if(Cmd_Argc() >= 5)
    {
        at = glm::vec3{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    }
    else
    {
        float best = 1e30f;
        for(int i = svs.maxclients + 1; world && i < qcvm->num_edicts; i++)
        {
            if(!solidProp(i))
            {
                continue;
            }
            const float zero[3]{0.f, 0.f, 0.f};
            const glm::vec3 c{shotShape(EDICT_NUM(i), world->slots[static_cast<size_t>(i)], zero, zero).centre};
            const float d = glm::distance(c, vec(player->v.origin));
            if(d < best)
            {
                best = d;
                at = c;
            }
        }
        if(best >= 1e30f)
        {
            Con_Printf("vr_physics_fire: no solid prop to fire at\n");
            return;
        }
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM0) = static_cast<float>(Q_atof(Cmd_Argv(1)));
    G_FLOAT(OFS_PARM1 + 0) = at.x;
    G_FLOAT(OFS_PARM1 + 1) = at.y;
    G_FLOAT(OFS_PARM1 + 2) = at.z;
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
    edict_t* shot = PROG_TO_EDICT(G_INT(OFS_RETURN));
    if(shot != qcvm->edicts)
    {
        Con_Printf("vr_physics_fire: %s %d from %.3f %.3f %.3f vel %.3f %.3f %.3f\n", PR_GetString(shot->v.classname),
            NUM_FOR_EDICT(shot), static_cast<double>(shot->v.origin[0]), static_cast<double>(shot->v.origin[1]),
            static_cast<double>(shot->v.origin[2]), static_cast<double>(shot->v.velocity[0]),
            static_cast<double>(shot->v.velocity[1]), static_cast<double>(shot->v.velocity[2]));
    }
}

// vr_shock_ragdoll_check: each ragdoll's shock (left: 1 fresh .. 0), its limbs' turning speed relative to their parents
// (mean, rad/s), its fastest part (units/s), its joints' worst separation (units: stretched limbs) and where its pelvis is.
// For tests (vr_shock.qc).
void shockCheck_f()
{
    if(!sv.active || !world)
    {
        Con_Printf("vr_shock_ragdoll_check: no game\n");
        return;
    }
    const VmScope vm;
    int count = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0)
        {
            continue;
        }
        float relative = 0.f, fastest = 0.f, separation = 0.f;
        int limbs = 0;
        for(int b = 0; b < r.count; ++b)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            fastest = za::max(fastest, glm::length(glmv(b3Body_GetLinearVelocity(body))) * world->m2u);
            const int parent = r.rig->bones[b].parent;
            if(parent >= 0 && !partCut(r, parent) && r.rig->bones[b].joint != ragdoll::Joint::Loose)
            {
                relative += glm::length(glmv(b3Body_GetAngularVelocity(body)) -
                                        glmv(b3Body_GetAngularVelocity(r.body[static_cast<za::SizeT>(parent)])));
                ++limbs;
            }
            b3JointId joints[8];
            const int n = b3Body_GetJoints(body, joints, 8);
            for(int j = 0; j < n; ++j)
            {
                separation = za::max(separation, b3Joint_GetLinearSeparation(joints[j]) * world->m2u);
            }
        }
        const glm::vec3 pelvis = world->toU(b3Body_GetWorldCenter(r.body[0]));
        Con_Printf("shockcheck: entity=%d left=%.2f limbs=%d relspin=%.2f fastest=%.0f stretch=%.2f pelvis=%.0f %.0f %.0f\n",
            r.num, shockLeft(EDICT_NUM(r.num)), limbs, limbs ? relative / static_cast<float>(limbs) : 0.f, fastest,
            separation, pelvis.x, pelvis.y, pelvis.z);
        ++count;
    }
    Con_Printf("shockcheck: ragdolls=%d\n", count);
}

// vr_knockdown_test <mode>: QC's VR_Knockdown_Test, as the first player: 0 knocks the nearest monster down (whatever the
// chance), 1 gets the knocked-down ones up now, 2 hits the nearest knocked-down one, 3 kills it, 4 gibs it, 5 lists the
// monsters, 21 and 22 shove the nearest with one hand and two (its chance: vr_knockdown_chance 100 for sure; its topple,
// foegrab::shoveTopple). For tests.
void knockdownTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_knockdown_test <mode> (in a game)\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    dfunction_t* fn = nullptr;
    if(Cmd_Argc() >= 2 && Q_atof(Cmd_Argv(1)) == 9 && world)
    {
        float angular = 0.f;
        float deflection = 0.f;
        int bodies = 0;
        float moved = 0.f, movedMost = 0.f;
        int compared = 0;
        for(const RagdollBodies& r : world->ragdolls)
        {
            if(r.num <= 0 || !knockedDown(EDICT_NUM(r.num)) || EDICT_NUM(r.num)->v.health <= 0.f) { continue; }
            for(int b = 0; b < r.count; ++b)
            {
                if(partCut(r, b)) { continue; }
                angular += glm::length(glmv(b3Body_GetAngularVelocity(r.body[static_cast<za::SizeT>(b)])));
                const ragdoll::Bone& bone = r.rig->bones[b];
                const glm::vec3 c = world->toU(b3Body_GetWorldCenter(r.body[static_cast<za::SizeT>(b)]));
                const za::SizeT slot = static_cast<za::SizeT>(r.num) * ragdoll::maxBones + static_cast<za::SizeT>(b);
                if(wiggleCheck.last.size() <= slot)
                {
                    wiggleCheck.last.resize(slot + 1, glm::vec3{1e30f});
                }
                if(wiggleCheck.last[slot].x < 1e29f)
                {
                    const float d = glm::distance(c, wiggleCheck.last[slot]);
                    moved += d;
                    movedMost = za::max(movedMost, d);
                    compared++;
                }
                wiggleCheck.last[slot] = c;
                if(r.struggleReady && bone.parent >= 0 && !partCut(r, bone.parent) &&
                    (bone.joint == ragdoll::Joint::Ball || bone.joint == ragdoll::Joint::Hinge))
                {
                    const glm::quat relative = glm::inverse(fromB3(b3Body_GetTransform(r.body[bone.parent]).q)) *
                        fromB3(b3Body_GetTransform(r.body[b]).q);
                    const glm::quat delta = glm::normalize(relative * glm::inverse(r.struggleRest[b]));
                    deflection = za::max(deflection, glm::degrees(2.f * glm::acos(za::clamp(za::fabs(delta.w), 0.f, 1.f))));
                }
                ++bodies;
            }
        }
        Con_Printf("wigglecheck: strength=%.2f parts=%d angular=%.4f rad/s\n", vr_knockdown_wiggle.value,
            bodies, bodies ? angular / bodies : 0.f);
        Con_Printf("strugglecheck: strength=%.2f deflection=%.3f degrees\n", vr_knockdown_wiggle.value, deflection);
        Con_Printf("strugglemotion: %d parts moved %.2f units on average (most %.2f) over %.2f s\n", compared,
            compared ? moved / static_cast<float>(compared) : 0.f, movedMost,
            wiggleCheck.time >= 0.0 ? qcvm->time - wiggleCheck.time : 0.0);
        wiggleCheck.time = qcvm->time;
        return;
    }
    for(int i = 1; i < qcvm->progs->numfunctions && !fn; i++)
    {
        fn = !strcmp(PR_GetString(qcvm->functions[i].s_name), "VR_Knockdown_Test") ? &qcvm->functions[i] : nullptr;
    }
    if(!fn || player->free)
    {
        Con_Printf("vr_knockdown_test: no VR_Knockdown_Test in the progs, or no player\n");
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM0) = Cmd_Argc() >= 2 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 0.f;
    PR_ExecuteProgram(static_cast<func_t>(fn - qcvm->functions));
}
} // namespace
} // namespace box3d::detail
