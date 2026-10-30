// vr_box3d_diag_ragdoll.cpp -- the ragdolls' and corpses' physics commands: lists, speeds, poses, the arm,
// environment-hit and blast tests, a prop dropped on a corpse. Moved out of vr_box3d_diag.cpp; what they share with
// it is vr_box3d_diag_internal.hpp.

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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
    const glm::vec3 player = vec(progs::playerEdict()->v.origin);
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
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
    const progs::ServerVm vm;
    const glm::vec3 eye = vec(progs::playerEdict()->v.origin);
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

} // namespace box3d::detail
