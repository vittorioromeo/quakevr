// vr_box3d_diag_piles.cpp -- the physics commands that make things to test with: a loose object, stacks,
// pyramids, piles and the stress tests' big piles (and taking them away), a blast. Moved out of vr_box3d_diag.cpp;
// what they share with it is vr_box3d_diag_internal.hpp.

#include "vr_stats.hpp"
#include "vr_alloccount.h"
#include "vr_geom.hpp"
#include "vr_progs.hpp"

#include "vr_box3d_diag_internal.hpp"
#include "vr_box3d_internal.hpp"
#include "vr_hands.hpp"
#include "vr_units.hpp"

namespace box3d::detail
{

// vr_physics_loose <number | classname>: made a loose physics object, as a hand's knock makes it (QC's
// VR_Carry_Loose: a toss, rigid, touchable), where it is: a hanging armour, a pickup. For tests.
void loose_f()
{
    if(!sv.active || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_physics_loose <number | classname>\n");
        return;
    }
    const progs::ServerVm vm;
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

namespace
{

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

} // namespace

// vr_physics_stack <number | classname | props> <count> <x> <y> <z> [<yaw> [<gap>]]: the first `count` of them in a
// column from x y z (their bottoms `gap` units apart, 0.5 by default), still, level, awake.
void stack_f()
{
    if(!sv.active || Cmd_Argc() < 6)
    {
        Con_Printf("usage: vr_physics_stack <number | classname | props> <count> <x> <y> <z> [<yaw> [<gap>]]\n");
        return;
    }
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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

namespace
{

// The rocks and bricks a big pile is made of (vr_debris.qc's models, precached on every map): rocks, then bricks.
constexpr const char* pileModels[] = {"progs/vr_rock1.mdl", "progs/vr_rock2.mdl", "progs/vr_rock3.mdl", "progs/vr_rock4.mdl",
    "progs/vr_rock5.mdl", "progs/vr_brick1.mdl", "progs/vr_brick2.mdl", "progs/vr_brick3.mdl", "progs/vr_brick4.mdl"};
constexpr int pileRocks = 5;
constexpr int pileBricks = 4;

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

} // namespace

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
    const progs::ServerVm vm;
    const func_t pieceFn = qvr::progs::findFunction("vr_debris_piece");
    const func_t crateFn = qvr::progs::findFunction("vr_crate");
    if((kind != PileKind::Crates && !pieceFn) || ((kind == PileKind::Crates || kind == PileKind::Mixed) && !crateFn))
    {
        Con_Printf("vr_physics_bigpile: no vr_debris_piece or vr_crate in the progs\n");
        return;
    }
    edict_t* player = progs::playerEdict();
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
    const progs::ServerVm vm;
    int removed = 0;
    progs::forEachEdict(svs.maxclients + 1, [&](edict_t* e) {
        const bool tagged = (static_cast<int>(e->v.spawnflags) & stressTagBit) != 0;
        if(tagged || !strcmp(PR_GetString(e->v.classname), "vr_crate_piece"))
        {
            ED_Free(e);
            removed++;
        }
    });
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
    const progs::ServerVm vm;
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

} // namespace box3d::detail
