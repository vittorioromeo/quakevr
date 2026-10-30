// vr_box3d_diag_player.cpp -- the physics commands about the player and shots against props: the player's
// shape, props in the level, approaching and inside them, the shots' bench and fire, the shock check, the knockdown
// test. Moved out of vr_box3d_diag.cpp; what they share with it is vr_box3d_diag_internal.hpp.

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

void player_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        return;
    }
    const progs::ServerVm vm;
    edict_t* p = progs::playerEdict();
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

namespace
{

// vr_knockdown_test 9's parts' places at its last call (by edict and part; x 1e30: none), to tell how far they moved.
struct WiggleCheck
{
    za::Vector<glm::vec3> last;
    double time{-1.0};
};
WiggleCheck wiggleCheck;

} // namespace

// vr_physics_inlevel [<number | classname | props>]: how far each prop's box (its drawn one, as it is turned) is inside
// the level's solids (the world and its brush entities), to half a unit; 0: clear (touching isn't in). A prop held in
// both hands pushed into a wall (vr_carry_two_hands_solid) should stay at 0.
void inLevel_f()
{
    if(!sv.active || !world)
    {
        return;
    }
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
    const za::Vector<edict_t*> list = entitiesNamed(Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
    edict_t* prop = list.empty() ? nullptr : list.front();
    if(!prop || !solidProp(NUM_FOR_EDICT(prop)))
    {
        Con_Printf("vr_physics_approach: no solid prop %s\n", Cmd_Argc() > 1 ? Cmd_Argv(1) : "misc_explobox");
        return;
    }
    edict_t* p = progs::playerEdict();
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
    edict_t* player = progs::playerEdict();
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
    edict_t* player = progs::playerEdict();
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

} // namespace box3d::detail
