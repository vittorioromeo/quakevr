#include "vr_alloccount.h"
#include "vr_trace.hpp"
// vr_box3d_ragdoll.cpp -- ragdolls: made (deaths, active deaths, cuts), driven (shoves, swimming, climbing, active),
// shocked and written back.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"
#include "vr_agarms.hpp"
#include "vr_rig2.hpp"

namespace box3d::detail
{
namespace
{
// ----------------------------------------------------------------------------
// Ragdolls (vr_ragdoll; experimental, the monsters with a rig: vr_ragdoll.cpp seedTables; ROUND21.md, "Ragdolls" to
// "Ragdolls 4"). A dying grunt (dead, not solid: his
// death code's third frame on), once his death animation is vr_ragdoll_start of the way through, becomes a ragdoll: one
// dynamic body per bone of his rig (vr_ragdoll.cpp: derived from his model's animation), made where his drawn frame has
// each bone and moving as the animation moved it, jointed at the rig's pivots (balls with cone and twist limits, hinges
// with a range; a motor holding each still up to vr_ragdoll_joint_friction: joint friction), a loose piece (his
// shotgun) a body of its own. A corpse's kind, categories, mask and passes (Kind::Corpse, catCorpse, corpseMask,
// shouldCollide, noteCorpseInside), its own parts not meeting each other (its group). Its entity follows its pelvis
// (writeRagdoll: origin, its box round all the parts), touchable and not solid as before: shots, blows, flames, gibbing
// and corpse damage are the QC's; a shot's push (physicspush), a blast (physicsblast) and a QC knock move the parts.
// The client draws its mesh skinned to the parts (vr_ragdoll.cpp: published after each step). At most vr_ragdoll_max:
// more dead grunts lie as corpses. A loaded game's: made again from the frame he lies in.

constexpr double recentBlastTime = 0.6; // s: a blast this recent throws a ragdoll made now (createRagdoll)
} // namespace

// The setting `t` for `ent`'s ragdoll: its class's own, else the global one.
[[nodiscard]] float tune(const edict_t* ent, Tune t)
{
    const auto i = static_cast<za::SizeT>(t);
    const char* classname = ad::vrClass(PR_GetString(ent->v.classname)); // AD support: AD's monsters as their nearest class
    for(const RagdollClass& c : ragdollClasses)
    {
        if(!strcmp(classname, c.classname) && c.own[i]->value >= 0.f)
        {
            return c.own[i]->value;
        }
    }
    return ragdollGlobals[i]->value;
}

namespace
{
// AD support: AD's humanoids (vr_ad_rigs.inc: each rig a pelvis, chest, head and two-bone arms and legs; their get-ups
// their deaths backwards, the zombie knight's and the mummy's their own falls; ROUND22.md, "Active rigs for AD's
// humanoids"); AD's own grunt, enforcer, point knight and zombie share Quake VR's classnames. Not the centurion (on its
// hover board: never knocked down nor lifted, its right leg no shin) nor the skull wizard (a robe: one leg bone, nothing
// to stand or stagger on).
constexpr ActiveClass activeClasses[] = {{"monster_army", true}, {"monster_enforcer", true}, {"monster_knight", true},
    {"monster_hell_knight", true}, {"monster_ogre", true}, {"monster_ogre_marksman", true}, {"monster_zombie", false},
    {"monster_army_grenade", true}, {"monster_army_plasma", true}, {"monster_army_rocket", true},
    {"monster_defender", true}, {"monster_eliminator", true}, {"monster_pyro", true}, {"monster_fumigator", true},
    {"monster_dcrossbow", true}, {"monster_dfury", true}, {"monster_dguard", true}, {"monster_dknight", true},
    {"monster_dsergeant", true}, {"monster_zombiek", false}, {"monster_mummy", false}, {"monster_death_lord", true},
    {"monster_death_guard", true}, {"monster_deflector", true}, {"monster_ogre_flak", true},
    {"monster_dprince", true}, // (the Dark Prince: the death knight's mesh and rig, AdDprince)
    // The other Mini Bosses (Monster Groups): the minotaur (its rig a spine between pelvis and chest, AdMinotaur) and the
    // justice knight (AdJustice; the baron its old classname), the stone golem (AdGolem: a humanoid's rig, 105 units tall;
    // its get-up blended: its body lagged its death backwards 30 units, or lost it twice of three) and the drole (AdDrole:
    // two legs, two raised scythes, two lower claws from its hips).
    {"monster_minotaur", true}, {"monster_justice", true}, {"monster_baron", true}, {"monster_golem", true, true},
    {"monster_drole", true}};

// A joint's limits' share by its group (vr_ragdoll_limits_legs ...; the ragdoll tool's Joint Groups): times the global
// one and its class's.
cvar_t* const groupLimitCvars[ragactive::channelCount] = {&vr_ragdoll_limits_legs, &vr_ragdoll_limits_torso,
    &vr_ragdoll_limits_arms, &vr_ragdoll_limits_head};

[[nodiscard]] float groupLimits(const ragdoll::Bone& bone)
{
    return za::clamp(groupLimitCvars[static_cast<int>(ragactive::channelOf(bone))]->value, 0.05f, 3.f);
}
} // namespace

[[nodiscard]] const ActiveClass* activeClassOf(const edict_t* ent)
{
    const char* classname = PR_GetString(ent->v.classname);
    for(const ActiveClass& c : activeClasses)
    {
        if(!strcmp(classname, c.classname))
        {
            return &c;
        }
    }
    return nullptr;
}

namespace
{
// Whether `ent`'s ragdoll is an active death now: vr_ragdoll_active, his class's, dead in a death animation (`progress`:
// deathProgress) whose frames are seen changing (not a loaded game's: that one is limp, made at Go Limp At).
[[nodiscard]] bool activeDeath(edict_t* ent, int num, float progress)
{
    return vr_ragdoll_active.value != 0.f && progress >= 0.f && ent->v.health <= 0.f && activeClassOf(ent) &&
           num < static_cast<int>(world->corpseWatch.size()) && world->corpseWatch[static_cast<za::SizeT>(num)].changed >= 0.0;
}
} // namespace

[[nodiscard]] int ragdollCount()
{
    int n = 0;
    for(const RagdollBodies& r : world->ragdolls)
    {
        n += r.num > 0 ? 1 : 0;
    }
    return n;
}
[[nodiscard]] bool wantsRagdoll(edict_t* ent, int num)
{
    const bool knocked = knockedDown(ent);
    if(vr_ragdoll.value < 1.f || num <= svs.maxclients || !(knocked || deadMonster(ent)) || hasFlag(ent, FL_SWIM))
    {
        return false;
    }
    qmodel_t* model = modelOf(ent);
    if(!ragdoll::eligible(model))
    {
        return false;
    }
    if(fieldFloatOr(ent, fields().vr_corpse_placed, 0.f) == 1.f)
    {
        return false; // (a mapper's corpse asleep: a corpse as placed till a player comes near, settleCorpses)
    }
    if(num < static_cast<int>(world->slots.size()) && world->slots[num].ragdoll >= 0)
    {
        return true; // (already one: its model is checked by stale)
    }
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    if(!rig)
    {
        return false;
    }
    if(knocked)
    {
        return true; // (knocked down: at once, whatever Most Ragdolls: the QC made room, vr_knockdown.qc)
    }
    const float progress = ragdoll::deathProgress(*rig, static_cast<int>(ent->v.frame));
    // Beheaded: its ragdoll at once; a saved game's is made again so too.
    const bool headless = (fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f && rig->head >= 0) ||
                          fieldFloatOr(ent, fields().vr_limbcut, 0.f) != 0.f; // (a limb cut off: Limb gore)
    // (An active death: at once, from his first frame not solid, his death animation played by his body.)
    const bool limp = progress >= za::clamp(tune(ent, Tune::Start), 0.f, 1.f) || fieldFloatOr(ent, fields().vr_corpse, 0.f) >= 2.f ||
                      (progress < 0.f && isCorpse(ent, num)) || headless || activeDeath(ent, num, progress);
    return limp && vr_ragdoll_max.value >= 1.f;
}

// A rotation that takes +z to `d` (unit).
[[nodiscard]] glm::quat zTo(const glm::vec3& d)
{
    const glm::vec3 z{0.f, 0.f, 1.f};
    const float c = glm::dot(z, d);
    if(c < -0.9999f)
    {
        return glm::quat{0.f, 1.f, 0.f, 0.f};
    }
    const glm::vec3 axis = glm::cross(z, d);
    return glm::normalize(glm::quat{1.f + c, axis.x, axis.y, axis.z});
}
[[nodiscard]] bool isRagdollSlot(int num)
{
    return num < static_cast<int>(world->slots.size()) && world->slots[num].ragdoll >= 0;
}

namespace
{
// Whether a part of ragdoll `r` is in one of edict `other`'s ragdoll's (their shapes, a centimetre less: parts only
// touching are not in each other).
[[nodiscard]] bool ragdollIn(const RagdollBodies& r, int other)
{
    struct Query
    {
        int other;
        bool found;
    } q{other, false};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.categoryBits = catCorpse;
    filter.maskBits = catCorpse;
    constexpr float sink = 0.01f; // m
    for(int b = 0; b < r.count && !q.found; b++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        za::Array<b3ShapeId, maxBodyShapes> shapes;
        const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
        const b3WorldTransform xf = b3Body_GetTransform(body);
        for(int k = 0; k < count && !q.found; k++)
        {
            za::Array<b3Vec3, B3_MAX_SHAPE_CAST_POINTS> points;
            int n = 0;
            float radius = 0.f;
            if(b3Shape_GetType(shapes[k]) == b3_hullShape)
            {
                const b3HullData* hull = b3Shape_GetHull(shapes[k]);
                const b3Vec3* p = b3GetHullPoints(hull);
                const b3Vec3 c = hull->center;
                n = za::min(hull->vertexCount, static_cast<int>(points.size()));
                for(int v = 0; v < n; v++)
                {
                    b3Vec3 o = p[v];
                    o.x = o.x > c.x ? za::max(c.x, o.x - sink) : za::min(c.x, o.x + sink);
                    o.y = o.y > c.y ? za::max(c.y, o.y - sink) : za::min(c.y, o.y + sink);
                    o.z = o.z > c.z ? za::max(c.z, o.z - sink) : za::min(c.z, o.z + sink);
                    points[static_cast<za::SizeT>(v)] = b3RotateVector(xf.q, o);
                }
            }
            else if(b3Shape_GetType(shapes[k]) == b3_capsuleShape)
            {
                const b3Capsule cap = b3Shape_GetCapsule(shapes[k]);
                points[0] = b3RotateVector(xf.q, cap.center1);
                points[1] = b3RotateVector(xf.q, cap.center2);
                n = 2;
                radius = za::max(cap.radius - sink, 0.f);
            }
            if(n == 0)
            {
                continue;
            }
            const b3ShapeProxy proxy{points.data(), n, radius};
            b3World_OverlapShape(world->id, xf.p, &proxy, filter,
                [](b3ShapeId shape, void* raw) {
                    auto* qq = static_cast<Query*>(raw);
                    if(numOf(shape) == qq->other)
                    {
                        qq->found = true;
                        return false;
                    }
                    return true;
                },
                &q);
        }
    }
    return q.found;
}
} // namespace

// Whether edicts `a` and `b` are ragdolls made in each other (they pass through each other: shouldCollide).
[[nodiscard]] bool ragdollsInside(int a, int b)
{
    return za::anyOf(world->ragdollsInside.begin(), world->ragdollsInside.end(),
        [a, b](const World::RagdollPair& p) { return (p.a == a && p.b == b) || (p.a == b && p.b == a); });
}

// Once a frame: the ragdolls made in each other that are clear of each other now (or gone) meet again.
void pruneRagdollsInside()
{
    za::vectorEraseIf(world->ragdollsInside, [](const World::RagdollPair& p) {
        const RagdollBodies* a = ragdollOf(p.a);
        return !a || !ragdollOf(p.b) || !ragdollIn(*a, p.b);
    });
}
[[nodiscard]] bool roomRefused(int num)
{
    return za::anyOf(world->roomRefused.begin(), world->roomRefused.end(), [&](int n) { return n == num; });
}

// The first player's weapons in its hands and holsters, as one number (a change: prepareHeldGuns again).
[[nodiscard]] int heldLoadout()
{
    if(svs.maxclients < 1)
    {
        return 0;
    }
    const FieldOffsets& f = fields();
    edict_t* player = EDICT_NUM(1);
    const int ids[] ={static_cast<int>(player->v.weapon), static_cast<int>(fieldFloatOr(player, f.weapon2, 0.f)),
        static_cast<int>(fieldFloatOr(player, f.holsterweapon0, 0.f)), static_cast<int>(fieldFloatOr(player, f.holsterweapon1, 0.f)),
        static_cast<int>(fieldFloatOr(player, f.holsterweapon2, 0.f)), static_cast<int>(fieldFloatOr(player, f.holsterweapon3, 0.f)),
        static_cast<int>(fieldFloatOr(player, f.holsterweapon4, 0.f)), static_cast<int>(fieldFloatOr(player, f.holsterweapon5, 0.f))};
    int key = 0;
    for(const int id : ids)
    {
        key = key * 31 + id + 1;
    }
    return key;
}

namespace
{
jobs::Site gunPrepSite{"box3d gun pieces ahead"}; // (prepareHeldGuns' parallelFor: vr_jobs_sites)

struct GunPrepWork
{
    const qmodel_t* model{nullptr};
    za::Vector<glm::vec3> triangles; // the model's own (held::modelTriangles: a held gun's key, -2)
    za::Vector<convex::Piece> pieces;
    convex::Report report;
    bool fromDisk{false};
};
} // namespace

// The held guns of every weapon the map can give made at its load (QC's VR_GunPrep_Models lists their models: the
// weapons lying about, its monsters' drops, backpacks, the players' hands and holsters): each one's convex pieces read
// from the disk or cut (cutGun) on the pool, kept in gunShapes under the key a hand holding it asks for (makeReach), so
// a first pickup in play finds them made (the cut was 35-60 ms, at the pickup, when the disk lacked it). Models the
// server has not precached are skipped (their weapons cannot be held here). Once a world.
void prepareHeldGuns()
{
    const int most = za::min(static_cast<int>(vr_box3d_gun_pieces.value), maxBodyShapes);
    const func_t collect = qvr::progs::findFunction("VR_GunPrep_Collect");
    const func_t modelOf = qvr::progs::findFunction("VR_GunPrep_Model");
    if(!vr_gun_pieces_ahead.value || most < 2 || !collect || !modelOf)
    {
        return;
    }
    const double t0 = Sys_DoubleTime();
    char list[2048]{}; // " name name ..." (QC returns a string per call: a tempstring does not outlive the next)
    {
        QcCallGuard guard;
        PR_ExecuteProgram(collect);
        const int count = static_cast<int>(G_FLOAT(OFS_RETURN));
        size_t used = 0;
        for(int k = 0; k < count * 2 && k < 64; k++)
        {
            G_FLOAT(OFS_PARM0) = static_cast<float>(k);
            PR_ExecuteProgram(modelOf);
            const char* name = PR_GetString(G_INT(OFS_RETURN));
            const size_t length = strlen(name);
            if(length && used + length + 2 < sizeof(list))
            {
                list[used++] = ' ';
                memcpy(list + used, name, length + 1);
                used += length;
            }
        }
    }
    if(vr_debug_box3d.value)
    {
        Con_Printf("box3d: held guns the map can give:%s\n", list);
    }
    za::Vector<GunPrepWork> work;
    for(const char* p = list; *p;)
    {
        while(*p == ' ')
        {
            p++;
        }
        const char* end = p;
        while(*end && *end != ' ')
        {
            end++;
        }
        char name[MAX_QPATH];
        const size_t n = za::min(static_cast<size_t>(end - p), sizeof(name) - 1);
        memcpy(name, p, n);
        name[n] = 0;
        p = end;
        qmodel_t* model = nullptr;
        for(int i = 1; n && i < MAX_MODELS && sv.models[i]; i++)
        {
            if(!strcmp(sv.models[i]->name, name))
            {
                model = sv.models[i];
                break;
            }
        }
        if(!model || model->type != mod_alias)
        {
            continue;
        }
        const PropHullKey key{model, -2, {}};
        const bool made = za::anyOf(gunShapes.entries.begin(), gunShapes.entries.end(),
            [&](const GunShapeCache::Entry& e) { return e.key == key && e.most == most; });
        const bool queued = za::anyOf(work.begin(), work.end(), [&](const GunPrepWork& w) { return w.model == model; });
        if(made || queued)
        {
            continue;
        }
        GunPrepWork one;
        one.model = model;
        if(held::modelTriangles(model, false, one.triangles)) // (the main thread: it reads the model's cache)
        {
            work.pushBack(ZA_MOVE(one));
        }
    }
    if(work.empty())
    {
        return;
    }
    convex::Settings settings;
    settings.maxPieces = most;
    const auto cutOne = [&](za::SizeT i) {
        work[i].fromDisk = cutGun(work[i].triangles, settings, work[i].pieces, work[i].report, work[i].model->name);
    };
    if(vr_gun_pieces_cache.value >= 2.f) // (Check: it prints on a difference)
    {
        for(za::SizeT i = 0; i < work.size(); i++)
        {
            cutOne(i);
        }
    }
    else
    {
        jobs::parallelFor(gunPrepSite, work.size(), 1, [&](za::SizeT begin, za::SizeT end) {
            for(za::SizeT i = begin; i < end; i++)
            {
                cutOne(i);
            }
        });
    }
    int read = 0;
    for(GunPrepWork& w : work)
    {
        GunShapeCache::Entry entry{PropHullKey{w.model, -2, {}}, most, static_cast<int>(gunShapes.pieceEnds.size()), 0, {}};
        for(const convex::Piece& piece : w.pieces)
        {
            gunShapes.points.emplaceBackRange(piece.points.data(), piece.points.size());
            gunShapes.pieceEnds.pushBack(static_cast<int>(gunShapes.points.size()));
        }
        entry.pieces = static_cast<int>(w.pieces.size());
        entry.report = w.report;
        gunShapes.entries.pushBack(entry);
        read += w.fromDisk ? 1 : 0;
    }
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    VR_TimeAdd("box3d: the map's held guns' pieces (prepareHeldGuns)", ms / 1000.0); // load timing (vr_startup_times)
    Con_DPrintf("box3d: %d held guns' pieces ready in %.1f ms (%d read from the disk, %d cut):%s\n",
        static_cast<int>(work.size()), ms, read, static_cast<int>(work.size()) - read, list);
}

// QC's VR_Knockdown_MakeRoom for `ent` (a ragdoll retired, a knockdown ended): whether a ragdoll may be made now. Its
// removed edicts' bodies go at once.
bool makeRagdollRoom(edict_t* ent)
{
    const func_t room = qvr::progs::bindings().Knockdown_MakeRoom;
    if(!room) { return false; }
    bool madeRoom = false;
    {
        QcCallGuard guard;
        G_INT(OFS_PARM0) = EDICT_TO_PROG(ent);
        PR_ExecuteProgram(room);
        madeRoom = G_FLOAT(OFS_RETURN) != 0.f;
    }
    for(RagdollBodies& old : world->ragdolls)
    {
        if(old.num > 0 && EDICT_NUM(old.num)->free)
        {
            destroyBody(world->slots[old.num]);
        }
    }
    return madeRoom;
}

// Part `bone`'s hull of its vertices (rest units times `k`: metres), simplified to at most 20 corners (Box3D's hulls have
// at most 128 half edges); one that can't be made, its vertices' box (null: no vertices). `volume` (m^3): the hull's,
// and its capsule's. The ragdoll's parts weigh their share of its mass by it (makeRagdoll), and so does a limb cut off
// (limbMass).
[[nodiscard]] b3HullData* boneHull(const ragdoll::Bone& bone, float k, float& volume)
{
    QVR_TRACY_SCOPE(tracyScope, "ragdoll bone hull");
    za::Array<b3Vec3, 128> pts;
    const int n = za::min(static_cast<int>(bone.points.size()), static_cast<int>(pts.size()));
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(int i = 0; i < n; i++)
    {
        const glm::vec3 p = bone.points[static_cast<za::SizeT>(i)] * k;
        pts[static_cast<za::SizeT>(i)] = b3Vec3{p.x, p.y, p.z};
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    b3HullData* hull = nullptr;
    for(const int corners : {20, 12, 8})
    {
        hull = n >= 4 ? b3CreateHull(pts.data(), n, corners) : nullptr;
        if(hull)
        {
            break;
        }
    }
    if(!hull && n > 0)
    {
        const glm::vec3 c = (lo + hi) * 0.5f, h = glm::max((hi - lo) * 0.5f, glm::vec3{0.01f});
        za::Array<b3Vec3, 8> box;
        for(int i = 0; i < 8; i++)
        {
            box[static_cast<za::SizeT>(i)] = b3Vec3{c.x + (i & 1 ? h.x : -h.x), c.y + (i & 2 ? h.y : -h.y), c.z + (i & 4 ? h.z : -h.z)};
        }
        hull = b3CreateHull(box.data(), 8, 8);
    }
    float v = hull ? hull->volume : 0.f;
    if(bone.capsule > 0.f)
    {
        const float rad = bone.capsule * k, len = glm::length(bone.end - bone.pivot) * k;
        v += glm::pi<float>() * rad * rad * (len + 4.f / 3.f * rad);
    }
    volume = v;
    return hull;
}

// Ragdoll `r`'s neck (bits): its head's bone and those between it and its chest (at most 2), each one's joint to its
// parent; 0 without a head.
[[nodiscard]] uint32_t neckBones(const RagdollBodies& r)
{
    if(!r.rig)
    {
        return 0u;
    }
    const ragdoll::Rig& rig = *r.rig;
    uint32_t out = 0u;
    int b = rig.head;
    for(int n = 0; n < 2 && b > 0 && b < r.count; n++)
    {
        out |= 1u << b;
        const int up = rig.bones[b].parent;
        if(up <= 0 || rig.bones[up].role == modelmeta::BoneRole::Chest)
        {
            break;
        }
        b = up;
    }
    return out;
}

// What the active drive sees of ragdoll `r` (vr_ragactive.hpp).
[[nodiscard]] ragactive::Parts activeParts(const RagdollBodies& r)
{
    ragactive::Parts p;
    p.rig = r.rig;
    p.body = r.body.data();
    p.count = r.count;
    p.cut = r.cut;
    p.loose = r.neckSnapped ? neckBones(r) : 0u;
    p.k = r.scale / world->m2u;
    p.m2u = world->m2u;
    p.turn = r.turn;
    return p;
}

namespace
{
// An active death's roll (its wound's, crawl's and death animation's chances): his entity, the time and
// vr_ragdoll_active_seed.
[[nodiscard]] uint32_t deathSeed(int num)
{
    return static_cast<uint32_t>(num) * 2654435761u ^ static_cast<uint32_t>(qcvm->time * 72.0) ^
           static_cast<uint32_t>(za::max(vr_ragdoll_active_seed.value, 0.f)) * 40503u;
}

// An active death's start (createRagdoll; vr_ragactive.cpp beginDeath): his death animation from `frame` (the one the
// parts were made in), the blow that killed him (QC's .vr_death_hit and .vr_death_hitpos: a headshot or beheading is a
// collapse; the body or a limb, a wound a hand may hold), his motion as he died (a stumble), a roll of his own (the
// wound's and the crawl's chances: his entity and the frame, vr_ragdoll_active_seed).
void startActiveDeath(edict_t* ent, int num, RagdollBodies& r, const b3JointId* joints, const glm::vec3& drawnOrigin,
    int frame, float friction, float stiffness)
{
    const ragdoll::Rig& rig = *r.rig;
    const ragactive::Parts parts = activeParts(r);
    ragactive::begin(r.active, parts, modelOf(ent), glmv(world->toM(drawnOrigin)), joints, friction, stiffness, qcvm->time);
    ragactive::DeathInfo di;
    di.death = -1;
    for(int d = 0; d < rig.deaths; d++)
    {
        di.death = frame >= rig.deathFirst[d] && frame <= rig.deathLast[d] ? d : di.death;
    }
    if(di.death < 0)
    {
        ragactive::end(r.active);
        return;
    }
    di.frame = static_cast<float>(frame);
    const float hitPart = fieldFloatOr(ent, fields().vr_death_hit, -1.f);
    // (The point from his origin as he died: his body thrown since, vr_kill_push, from where his parts are made now.)
    const glm::vec3 hitAt = fields().vr_death_hitpos >= 0 ? fieldVec(ent, fields().vr_death_hitpos) : glm::vec3{0.f};
    di.collapse = (fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f && rig.head >= 0) || hitPart == 1.f;
    di.wound = !di.collapse && hitPart >= 0.f && hitAt != glm::vec3{0.f} && glm::length(hitAt) < 96.f;
    di.woundAt = glmv(world->toM(drawnOrigin + hitAt));
    di.walk = glmv(world->toM(deathMotion(num)));
    di.seed = deathSeed(num);
    const ActiveClass* ac = activeClassOf(ent);
    di.crawler = ac && ac->crawls;
    // A test's (vr_ragdoll_act): the wound's and the crawl's chances sure, or no pin, for this death alone.
    if(const ragtest::Force* f = ragtest::forced(num))
    {
        di.sureWound = f->wound;
        di.sureCrawl = f->crawl;
        di.noPin = f->noPin;
    }
    ragactive::beginDeath(r.active, parts, di, qcvm->time);
    if(vr_debug_ragdoll.value)
    {
        const ragactive::Death& d = r.active.death;
        Con_Printf("ragdoll: %d active death %d from frame %d (%.2f s): %s%s%s%s, its own motion %.0f u/s; struck %g, %.0f "
                   "units from its origin\n", num, di.death,
            frame, d.length, di.collapse ? "a collapse (headshot or beheaded)" : "driven", d.stumble ? ", stumbling" : "",
            d.woundArm >= 0 ? (d.woundArm == 0 ? ", the left hand on the wound" : ", the right hand on the wound") : "",
            d.crawl ? ", to crawl" : "", glm::length(glm::vec2{deathMotion(num)}), static_cast<double>(hitPart),
            hitAt != glm::vec3{0.f} ? glm::length(hitAt) : -1.f);
    }
}

// Killed standing by a limb cut (Limb gore's killing blow: his ragdoll made at once from the frame he stood in, before
// his death code ran): a death animation of his (rolled) played by his body from its start, as an active death's, and
// that frame kept for his balance beat (ragactive::cutReaction, once the cut is made). Not the head's (a pure collapse,
// limp as before), not with vr_ragdoll_active_dismember 0.
void startCutDeath(edict_t* ent, int num, RagdollBodies& r, const b3JointId* joints, const glm::vec3& drawnOrigin,
    int frame, float friction, float stiffness)
{
    const ragdoll::Rig& rig = *r.rig;
    const ragactive::Parts parts = activeParts(r);
    ragactive::begin(r.active, parts, modelOf(ent), glmv(world->toM(drawnOrigin)), joints, friction, stiffness, qcvm->time);
    ragactive::DeathInfo di;
    di.seed = deathSeed(num);
    di.death = static_cast<int>((di.seed >> 8) % static_cast<uint32_t>(rig.deaths));
    di.frame = static_cast<float>(rig.deathFirst[di.death]);
    di.walk = glmv(world->toM(deathMotion(num)));
    const ActiveClass* ac = activeClassOf(ent);
    di.crawler = ac && ac->crawls;
    ragactive::beginDeath(r.active, parts, di, qcvm->time);
    r.active.dismember.liveFrame = frame;
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d killed by a limb cut standing in frame %d: active death %d from its start (%.2f s), its own "
                   "motion %.0f u/s\n", num, frame, di.death, r.active.death.length, glm::length(glm::vec2{deathMotion(num)}));
    }
}
// The part that holds rig `rig`'s loose piece `loose` (a monster's weapon): the part with a vertex nearest the piece's
// middle in the rest pose (the frame its rig was measured in: the weapon in its hands; the enforcer's rifle, held in both,
// his left forearm's). -1: none.
int looseHolder(const ragdoll::Rig& rig, int loose)
{
    const ragdoll::Bone& piece = rig.bones[loose];
    if(piece.points.empty())
    {
        return -1;
    }
    glm::vec3 mid{0.f};
    for(const glm::vec3& p : piece.points)
    {
        mid += p;
    }
    mid /= static_cast<float>(piece.points.size());
    int best = -1;
    float bestD = 1e30f;
    for(int o = 0; o < rig.numBones; o++)
    {
        if(rig.bones[o].joint == ragdoll::Joint::Loose ||
            (rig.looseInHands && strncmp(rig.bones[o].name, "forearm", 7) != 0)) // (held in a hand: Rig::looseInHands)
        {
            continue;
        }
        for(const glm::vec3& p : rig.bones[o].points)
        {
            const float d = glm::distance(p, mid);
            if(d < bestD)
            {
                bestD = d;
                best = o;
            }
        }
    }
    return best;
}

// Ragdoll `r`'s loose piece `loose` welded where it is now to its part `to` (vr_knockdown_weld; createRagdoll). `held`:
// `to` is the hand that holds it, and the piece weighs no more than that hand (a 3 kg rifle on a 0.6 kg hand: the weld
// gave under a landing's jolt, the weapon swinging 15 units off the hand a moment).
void weldLoose(RagdollBodies& r, int num, int loose, int to, float looseVolume, float density, bool held)
{
    const b3BodyId a = r.body[static_cast<za::SizeT>(to)], c = r.body[static_cast<za::SizeT>(loose)];
    if(held && b3Body_GetMass(c) > b3Body_GetMass(a) && b3Body_GetMass(c) > 0.f)
    {
        b3MassData md = b3Body_GetMassData(c);
        const float k = b3Body_GetMass(a) / md.mass;
        md.mass *= k;
        md.inertia.cx = b3MulSV(k, md.inertia.cx);
        md.inertia.cy = b3MulSV(k, md.inertia.cy);
        md.inertia.cz = b3MulSV(k, md.inertia.cz);
        b3Body_SetMassData(c, md);
    }
    const b3WorldTransform xa = b3Body_GetTransform(a), xc = b3Body_GetTransform(c);
    const glm::quat qa = fromB3(xa.q), qc = fromB3(xc.q);
    const glm::vec3 pa = glmv(xa.p), pc = glmv(xc.p);
    b3WeldJointDef jd = b3DefaultWeldJointDef();
    jd.base.bodyIdA = a;
    jd.base.bodyIdB = c;
    jd.base.localFrameA = b3Transform{b3v(glm::inverse(qa) * (pc - pa)), toB3(glm::normalize(glm::inverse(qa) * qc))};
    jd.base.localFrameB = b3Transform{b3Vec3_zero, b3Quat_identity};
    jd.base.collideConnected = false;
    (void)b3CreateWeldJoint(world->id, &jd);
    if(vr_knockdown_debug.value >= 2.f)
    {
        Con_Printf("ragdoll: %d its loose piece (%.2f kg, %.2f times its body's density) welded to its %s (%.2f kg)\n", num,
            b3Body_GetMass(c), b3Body_GetMass(c) / za::max(looseVolume, 1e-6f) / density, r.rig->bones[to].name,
            b3Body_GetMass(a));
    }
}
} // namespace

// The ragdoll's parts made for `ent` where its frame has them; false (nothing made) without a rig. `now`: made at once
// from whatever frame it is in (beheaded: ragdollDecap), its loose piece hidden (what it held: its death code drops it).
bool createRagdoll(edict_t* ent, int num, Slot& s, bool now)
{
    QVR_TRACY_SCOPE(tracyScope, "ragdoll create");
    qmodel_t* model = modelOf(ent);
    const ragdoll::Rig* rig = ragdoll::rigFor(model);
    if(!rig)
    {
        return false;
    }
    // All deaths use the same replacement policy as knockdowns. A full pool must not
    // strand a newly killed monster in its animated death pose.
    if(ragdollCount() >= static_cast<int>(za::max(1.f, vr_ragdoll_max.value)))
    {
        if(roomRefused(num))
        {
            return false; // (refused once: a corpse till it is gone)
        }
        if(world->syncingEntities)
        {
            // In syncEntities' edict loop: QC (which removes and spawns edicts, and kills) runs after it; the ragdoll
            // is made the next frame.
            if(!za::anyOf(world->roomWanted.begin(), world->roomWanted.end(), [&](int n) { return n == num; }))
            {
                world->roomWanted.pushBack(num);
            }
            return false;
        }
        if(!makeRagdollRoom(ent))
        {
            world->roomRefused.pushBack(num);
            return false;
        }
    }
    int index = -1;
    for(int i = 0; i < static_cast<int>(world->ragdolls.size()); i++)
    {
        if(world->ragdolls[static_cast<za::SizeT>(i)].num == 0)
        {
            index = i;
            break;
        }
    }
    if(index < 0)
    {
        index = static_cast<int>(world->ragdolls.size());
        world->ragdolls.pushBack(RagdollBodies{});
    }
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(index)];
    r = RagdollBodies{};
    r.num = num;
    r.rig = rig;
    // Its model's scale (a mapper's model_scale on the monster; uniform: its x), as it is drawn.
    r.scale = za::clamp(1.f + fieldVec(ent, fields().model_scale).x, 0.25f, 4.f);
    r.born = qcvm->time;
    r.knocked = knockedDown(ent);
    const float mass = za::max(tune(ent, Tune::Mass), 1.f), friction = za::max(tune(ent, Tune::Friction), 0.f);

    s.kind = Kind::Corpse;
    s.model = model;
    s.frame = static_cast<int>(ent->v.frame);
    s.corpseMode = corpseMode();
    s.corpseMask = corpseMask();
    s.corpseFitted = true;
    s.corpseDynamic = true;
    s.massSetting = mass;
    s.corpseFriction = friction;
    s.ragdoll = index;
    s.born = qcvm->time;

    // Where the frame has each bone, and how the animation moved it over its last frame (vr_ragdoll_inherit).
    const int frame = static_cast<int>(ent->v.frame);
    const int pose = ragdoll::poseOfFrame(model, frame);
    const float progress = ragdoll::deathProgress(*rig, frame);
    // (A mapper's corpse, or one put at its death's end, lay still: none of its animation's motion.)
    const bool still = fieldFloatOr(ent, fields().vr_corpse_placed, 0.f) != 0.f ||
                       (num < static_cast<int>(world->corpseWatch.size()) && world->corpseWatch[static_cast<za::SizeT>(num)].settled);
    const int prevPose = progress > 0.f && !still ? ragdoll::poseOfFrame(model, frame - 1) : pose;
    // Where the client draws him now: as his frame has just changed (this server frame), still the frame before (its lerp
    // to this one not begun); else this frame. The parts are made there, moving as the animation went on (the switch
    // unseen: the ragdoll's first frame is the animated mesh's last; vr_debug_ragdoll compares them).
    const bool justChanged = num < static_cast<int>(world->corpseWatch.size()) &&
                             world->corpseWatch[static_cast<za::SizeT>(num)].changed >= 0.0 &&
                             qcvm->time - world->corpseWatch[static_cast<za::SizeT>(num)].changed < 1e-6;
    const int drawnPose = progress > 0.f && justChanged ? prevPose : pose;
    // (And where it was drawn: the frame before's origin, his death code may have moved him with the frame: ai_back.)
    const glm::vec3 drawnOrigin = drawnPose != pose ? world->corpseWatch[static_cast<za::SizeT>(num)].last : vec(ent->v.origin);
    const glm::quat turn = glm::angleAxis(glm::radians(ent->v.angles[1]), glm::vec3{0.f, 0.f, 1.f});
    r.turn = turn;
    const glm::vec3 origin = vec(ent->v.origin);
    const float k = r.scale / world->m2u; // rest units -> body metres
    const float inherit = za::clamp(tune(ent, Tune::Inherit), 0.f, 2.f) / 0.1f; // (a frame is 0.1 s)
    // Beheaded (now: ragdollDecap) with vr_decap_own_motion: its own motion (its walk, ownMotion: a stepping monster's
    // velocity is none) kept apart from what this frame added to its velocity (the blow's knock), which a head pop scales.
    const bool own = now && vr_decap_own_motion.value != 0.f && num < static_cast<int>(world->motion.size());
    const glm::vec3 walk = own ? ownMotion(num) : glm::vec3{0.f};
    const glm::vec3 entVel = own ? walk + vec(ent->v.velocity) - world->motion[static_cast<za::SizeT>(num)].vel : vec(ent->v.velocity);
    // A loose piece the frame hides (all its vertices at one point: the grunt's shotgun in his death frames, dropped as
    // a weapon of its own, vr_monstermods.cpp) gets no body and stays hidden (published collapsed): the last ones.
    r.count = rig->numBones;
    const bool headless = fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f || fieldFloatOr(ent, fields().vr_limbcut, 0.f) != 0.f;
    while(r.count > 1 && rig->bones[r.count - 1].joint == ragdoll::Joint::Loose &&
          (now || headless || rig->hideLoose || ragdoll::collapsed(*rig, pose, r.count - 1)))
    {
        r.count--;
    }

    // The parts' shapes and volumes first (their density: vr_ragdoll_mass over the body's; a loose piece weighs
    // looseMass): each part's hull of its vertices (boneHull).
    za::Array<b3HullData*, ragdoll::maxBones> hulls{};
    za::Array<float, ragdoll::maxBones> volumes{};
    float volume = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        float v = 0.f;
        hulls[static_cast<za::SizeT>(b)] = boneHull(bone, k, v);
        volumes[static_cast<za::SizeT>(b)] = v;
        volume += bone.joint == ragdoll::Joint::Loose ? 0.f : v;
    }
    constexpr float looseMass = 3.f; // kg: a loose piece (his shotgun)
    const float density = mass / za::max(volume, 1e-6f);
    // Knocked down or staggering, alive (vr_knockdown_weld): its weapon (a loose piece) made in the hand that holds it,
    // where that hand holds it in the rest pose (looseHolder: the part nearest it there), not where its frame has it
    // (a pain frame swings it apart from the hands), and welded to that hand below: it follows the hand through every
    // active-ragdoll state (stagger, knockdown, lift, struggle, get-up) as a dead one's weapon is dropped.
    // (vr_knockdown_weld 2: as before, where its frame has it, welded to the part nearest it there.)
    const bool weld = r.knocked && vr_knockdown_weld.value != 0.f;
    const bool inHand = weld && vr_knockdown_weld.value != 2.f;
    za::Array<int, ragdoll::maxBones> heldBy{};
    for(int b = 0; b < r.count; b++)
    {
        heldBy[static_cast<za::SizeT>(b)] = inHand && rig->bones[b].joint == ragdoll::Joint::Loose ? looseHolder(*rig, b) : -1;
    }

    // Where each bone is drawn now: its frame's, an armed humanoid's arms where its posed arms were drawn (vr_agarms: on
    // its weapons, not the frame's arms; a listen server's own drawing): the weapon then stays on its drawn hand as the
    // ragdoll takes over (vr_hitreact.cpp ragdollProps), and a tug's hold on it stays under the hand that took it.
    const auto drawnBone = [&](int b, glm::quat& rot, glm::vec3& pos) {
        ragdoll::bonePose(*rig, drawnPose, b, rot, pos);
        if(rig->authored && rig2::drawnBone(num, b, rot, pos))
        {
            return; // (where the authored rig drew it, its arms on its weapons too: vr_rig2_game.cpp)
        }
        if(vr_ragdoll_posed_arms.value != 0.f)
        {
            (void)agarms::drawnArmBone(num, model, *rig, b, rot, pos);
        }
    };
    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        const int src = heldBy[static_cast<za::SizeT>(b)] >= 0 ? heldBy[static_cast<za::SizeT>(b)] : b; // (its pose's)
        glm::quat rot, prot;
        glm::vec3 pos, ppos;
        ragdoll::bonePose(*rig, pose, src, rot, pos);
        ragdoll::bonePose(*rig, prevPose, src, prot, ppos);
        glm::quat drot;
        glm::vec3 dpos;
        drawnBone(src, drot, dpos);
        b3BodyDef def = b3DefaultBodyDef();
        def.userData = userOf(num);
        def.type = b3_dynamicBody;
        def.position = world->toM(drawnOrigin + turn * (dpos * r.scale));
        def.rotation = toB3(glm::normalize(turn * drot));
        def.linearDamping = 0.05f;
        def.angularDamping = za::max(tune(ent, Tune::Damping), 0.f);
        // Its motion: the middle of its vertices from the previous frame's place to this one's, and its turn.
        glm::vec3 mid{0.f};
        for(const glm::vec3& p : bone.points)
        {
            mid += p;
        }
        mid /= static_cast<float>(za::max<za::SizeT>(bone.points.size(), 1));
        const glm::vec3 now = turn * ((rot * mid + pos) * r.scale), before = turn * ((prot * mid + ppos) * r.scale);
        def.linearVelocity = world->toM(entVel + (now - before) * inherit);
        const glm::quat dq = glm::normalize(turn * (rot * glm::inverse(prot)) * glm::inverse(turn));
        const float angle = 2.f * za::acos(za::clamp(za::abs(dq.w), 0.f, 1.f));
        const glm::vec3 im{dq.x, dq.y, dq.z};
        const glm::vec3 axis = glm::length(im) > 1e-6f ? glm::normalize(im) * (dq.w < 0.f ? -1.f : 1.f) : glm::vec3{0.f};
        def.angularVelocity = b3v(axis * (angle * inherit));
        if(own)
        {
            r.ownLin[static_cast<za::SizeT>(b)] = glmv(world->toM(walk + (now - before) * inherit));
            r.ownAng[static_cast<za::SizeT>(b)] = axis * (angle * inherit);
        }
        const b3BodyId body = b3CreateBody(world->id, &def);
        r.body[static_cast<za::SizeT>(b)] = body;

        // (Other ragdolls too, vr_ragdoll_collide_each: theirs have catCorpse in their masks, a corpse's not. Its own parts
        // with vr_ragdoll_self_collide: catCorpse in its mask too, no group, shouldCollide's selfMeets deciding which, and
        // whether other ragdolls' parts meet them.)
        const bool selfCollide = vr_ragdoll_self_collide.value != 0.f;
        b3ShapeDef sd = shapeDef(num, catCorpse,
            s.corpseMask | (vr_ragdoll_collide_each.value || selfCollide ? catCorpse : 0));
        sd.enableCustomFiltering = true; // (shouldCollide: thrown things, vr_corpse_collide_thrown; what it is made in)
        sd.filter.groupIndex = selfCollide ? 0 : -num; // (off: its own parts never meet)
        sd.enablePreSolveEvents = selfCollide;         // (preSolve also honours the live toggle)
        sd.enableHitEvents = true;       // (a knocked-down monster's fall damage: touches, callFalls)
        sd.baseMaterial.friction = friction;
        sd.density = bone.joint == ragdoll::Joint::Loose ? looseMass / za::max(volumes[static_cast<za::SizeT>(b)], 1e-6f) : density;
        if(b3HullData* hull = hulls[static_cast<za::SizeT>(b)])
        {
            b3CreateHullShape(body, &sd, hull);
            b3DestroyHull(hull); // (the world keeps its own copy)
        }
        else if(bone.capsule <= 0.f)
        {
            const b3BoxHull box = b3MakeCubeHull(1.f * k); // (no vertices: never so, but never a part without mass)
            b3CreateHullShape(body, &sd, &box.base);
        }
        if(bone.capsule > 0.f)
        {
            const glm::vec3 a = bone.pivot * k, c = bone.end * k;
            const b3Capsule capsule{b3Vec3{a.x, a.y, a.z}, b3Vec3{c.x, c.y, c.z}, bone.capsule * k};
            b3CreateCapsuleShape(body, &sd, &capsule);
        }
    }
    s.body = r.body[0];

    // The joints, at the rig's pivots: where the parent and the child have it in this frame (between them: the
    // animation's limbs stretch a little), in each one's rest space; their frames' z the bone's rest direction (a
    // ball: its cone about it) or the hinge's axis.
    const float jointFriction = za::max(tune(ent, Tune::JointFriction), 0.f);
    const float stiffness = za::max(tune(ent, Tune::JointStiffness), 0.f);
    const float limitsAll = za::clamp(tune(ent, Tune::Limits), 0.05f, 3.f);
    za::Array<b3JointId, ragdoll::maxBones> joints{}; // (each part's joint to its parent: an active ragdoll drives them)
    za::fill(joints, b3_nullJointId);
    for(int b = 0; b < r.count; b++)
    {
        const ragdoll::Bone& bone = rig->bones[b];
        if(bone.parent < 0 || (bone.joint != ragdoll::Joint::Ball && bone.joint != ragdoll::Joint::Hinge))
        {
            continue;
        }
        glm::quat rp, rc;
        glm::vec3 tp, tc;
        drawnBone(bone.parent, rp, tp);
        drawnBone(b, rc, tc);
        const glm::vec3 at = ((rp * bone.pivot + tp) + (rc * bone.pivot + tc)) * 0.5f;
        const glm::vec3 inParent = glm::inverse(rp) * (at - tp) * k, inChild = glm::inverse(rc) * (at - tc) * k;
        const glm::quat frameA = ragpose::jointFrameA(bone), frameB = ragpose::jointFrameB(bone); // (a hinge's axis, else the bone's rest direction: z; the authored rig's cone round its middle)
        const float limits = limitsAll * groupLimits(bone); // (vr_ragdoll_limits_<group>: 1 each by default)
        const auto setBase = [&](b3JointDef& base) {
            base.bodyIdA = r.body[static_cast<za::SizeT>(bone.parent)];
            base.bodyIdB = r.body[static_cast<za::SizeT>(b)];
            base.localFrameA = b3Transform{b3v(inParent), toB3(frameA)};
            base.localFrameB = b3Transform{b3v(inChild), toB3(frameB)};
            // Corpses need shoulder contact too: the upper arm is directly jointed to the chest.
            // preSolve exempts the socket itself, so its necessary joint overlap cannot fight the constraint.
            base.collideConnected = !r.knocked && !bone.framed && // (the authored rig: on its clavicle, its chest no neighbour)
                strncmp(bone.name, "upperarm", 8) == 0;
        };
        if(bone.joint == ragdoll::Joint::Ball)
        {
            b3SphericalJointDef jd = b3DefaultSphericalJointDef();
            setBase(jd.base);
            jd.enableConeLimit = true;
            jd.coneAngle = za::min(bone.cone * limits, glm::pi<float>());
            jd.coneAngleY = bone.coneY > 0.f ? za::min(bone.coneY * limits, glm::pi<float>()) : 0.f; // (the authored rig's ellipse)
            jd.enableTwistLimit = true;
            jd.lowerTwistAngle = -za::min(bone.twist * limits, 0.99f * glm::pi<float>());
            jd.upperTwistAngle = za::min(bone.twist * limits, 0.99f * glm::pi<float>());
            jd.enableMotor = jointFriction > 0.f;
            jd.maxMotorTorque = jointFriction;
            jd.enableSpring = stiffness > 0.f; // (towards the rest pose: how he stood)
            jd.hertz = stiffness;
            jd.dampingRatio = 1.f;
            joints[static_cast<za::SizeT>(b)] = b3CreateSphericalJoint(world->id, &jd);
        }
        else
        {
            b3RevoluteJointDef jd = b3DefaultRevoluteJointDef();
            setBase(jd.base);
            jd.enableLimit = true;
            jd.lowerAngle = za::max(bone.lower * limits, -0.99f * glm::pi<float>());
            jd.upperAngle = za::min(bone.upper * limits, 0.99f * glm::pi<float>());
            jd.enableMotor = jointFriction > 0.f;
            jd.maxMotorTorque = jointFriction;
            jd.enableSpring = stiffness > 0.f;
            jd.hertz = stiffness;
            jd.dampingRatio = 1.f;
            joints[static_cast<za::SizeT>(b)] = b3CreateRevoluteJoint(world->id, &jd);
        }
    }
    // Knocked down (vr_knockdown_weld): a loose piece (his gun) welded where it is to the hand that holds it (made there
    // above: heldBy); one with no holder found, to the part nearest it.
    if(weld)
    {
        for(int b = 0; b < r.count; b++)
        {
            if(rig->bones[b].joint != ragdoll::Joint::Loose)
            {
                continue;
            }
            if(const int holder = heldBy[static_cast<za::SizeT>(b)]; holder >= 0)
            {
                weldLoose(r, num, b, holder, volumes[static_cast<za::SizeT>(b)], density, true);
                continue;
            }
            glm::quat lr;
            glm::vec3 lt;
            ragdoll::bonePose(*rig, drawnPose, b, lr, lt);
            glm::vec3 mid{0.f};
            for(const glm::vec3& p : rig->bones[b].points)
            {
                mid += p;
            }
            mid = lr * (mid / static_cast<float>(za::max<za::SizeT>(rig->bones[b].points.size(), 1))) + lt;
            int best = -1;
            float bestD = 1e30f;
            for(int o = 0; o < r.count; o++)
            {
                if(rig->bones[o].joint == ragdoll::Joint::Loose)
                {
                    continue;
                }
                glm::quat orot;
                glm::vec3 opos;
                ragdoll::bonePose(*rig, drawnPose, o, orot, opos);
                for(const glm::vec3& p : rig->bones[o].points)
                {
                    const float d = glm::distance(orot * p + opos, mid);
                    if(d < bestD)
                    {
                        bestD = d;
                        best = o;
                    }
                }
            }
            if(best >= 0)
            {
                weldLoose(r, num, b, best, volumes[static_cast<za::SizeT>(b)], density, false);
            }
        }
    }
    // Made in another ragdoll: they pass through each other until clear (ragdollsInside).
    for(const RagdollBodies& o : world->ragdolls)
    {
        if(vr_ragdoll_collide_each.value && o.num > 0 && o.num != num && ragdollIn(r, o.num))
        {
            world->ragdollsInside.pushBack(World::RagdollPair{num, o.num});
            if(vr_debug_ragdoll.value)
            {
                Con_Printf("ragdoll: %d made in ragdoll %d: they pass through each other until clear\n", num, o.num);
            }
        }
    }
    for(int b = 0; b < r.count; b++)
    {
        Slot part = s; // (noteCorpseInside reads the slot's body: each part's in turn)
        part.body = r.body[static_cast<za::SizeT>(b)];
        noteCorpseInside(num, part);
    }
    // A blast that has just killed him (or hit him as he died) throws him now.
    for(const World::RecentBlast& rb : world->recentBlasts)
    {
        if(rb.damage > 0.f && qcvm->time - rb.time < recentBlastTime)
        {
            blastRagdoll(r, rb.at, rb.damage);
        }
    }
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    s.origin = origin;
    s.asleep = false;
    // Beheaded before (a saved game's): made headless again.
    if(headless)
    {
        // Its limbs cut off before (Limb gore: .vr_limbcut, the bones; read first: each cut writes it): each cut's joint
        // (a cut bone whose parent wasn't).
        const auto limbs = static_cast<uint32_t>(za::max(fieldFloatOr(ent, fields().vr_limbcut, 0.f), 0.f));
        if(fieldFloatOr(ent, fields().vr_headless, 0.f) != 0.f)
        {
            (void)cutHead(r, ent, glm::vec3{0.f}, false);
        }
        for(int b = 1; b < r.count; b++)
        {
            const int parent = rig->bones[b].parent;
            if((limbs & (1u << b)) && parent >= 0 && !(limbs & (1u << parent)) && !partCut(r, b))
            {
                (void)cutLimb(r, ent, b, glm::vec3{0.f}, false);
            }
        }
    }
    r.joints = joints;
    r.jointFriction = jointFriction;
    r.jointStiffness = stiffness;
    // An active death (vr_ragdoll_active): his body plays his death animation from here (startActiveDeath).
    if(activeDeath(ent, num, progress))
    {
        startActiveDeath(ent, num, r, joints.data(), drawnOrigin, drawnPose != pose ? frame - 1 : frame, jointFriction, stiffness);
    }
    // A limb cut off as he dies (ragdollCutLimb, made now): killed standing by it, driven from his death animation's start
    // (startCutDeath); dying already, from the frame he is in, as an active death.
    if(!r.active.on && now && cutDeathJoint >= 0 && cutDeathJoint != rig->head && progress < 1.f && rig->deaths > 0 &&
        vr_ragdoll_active.value != 0.f && vr_ragdoll_active_dismember.value != 0.f && ent->v.health <= 0.f && activeClassOf(ent))
    {
        if(progress < 0.f)
        {
            startCutDeath(ent, num, r, joints.data(), drawnOrigin, frame, jointFriction, stiffness);
        }
        else
        {
            startActiveDeath(ent, num, r, joints.data(), drawnOrigin, drawnPose != pose ? frame - 1 : frame, jointFriction, stiffness);
        }
    }
    // Its neck snapped before (a saved game's): loose again (ragdollNeckSnapped).
    if(fieldFloatOr(ent, fields().vr_necksnapped, 0.f) != 0.f)
    {
        (void)qvr::box3d::loosenNeck(r, ent);
    }
    // Its limbs taken by hand (vr_ragdoll_grab; QC's VR_Ragdoll_Handtouch), unless the QC gave it a hand touch of its own.
    if(const func_t touch = bindings().Ragdoll_Handtouch; touch && fields().handtouch >= 0 && !fieldFunc(ent, fields().handtouch))
    {
        fieldInt(ent, fields().handtouch) = static_cast<int>(touch);
    }
    if(vr_debug_ragdoll.value)
    {
        Con_Printf("ragdoll: %d %s limp at frame %d (%.0f%% of its death; made in pose %d%s): %d parts, %.1f kg, %.0f litres, %d ragdolls\n", num,
            PR_GetString(ent->v.classname), frame, progress * 100.f, drawnPose, drawnPose != pose ? ", the frame before's" : "",
            r.count, mass, volume * 1e3f, ragdollCount());
    }
    return true;
}

// A QC's knock (.velocity set: T_Damage, a blast's push) goes to all the parts; a QC's move far away (a teleport) takes
// them along.
// A shove's knockdown, each frame (RagdollBodies::ShoveDrive, set by ragdollShove): its travel and its turn apart.
// - Travel: its middle's level motion along the shove eased to the drive's (speed slowing evenly to none), across it to
//   none (it goes straight away from the shover), every part alike.
// - Turn (until it has turned `angle`, at `time`, and a quarter second more to settle): its parts eased towards turning
//   about its feet (their middle as they are now) at the rate that follows 2u^2 - u^3 of `angle` (u the share of `time`
//   gone: slow to start, fastest past halfway, still turning as it lands), corrected towards that curve by how far its
//   torso (pelvis to head) leans now, never faster than maxSpin; its feet `lag` of the travel behind its middle, its top
//   as far ahead. Their own turning eased to that too (no spin about the vertical, no cartwheels).
// - Over a ledge (until it lands below): its middle kept going along the shove at `speed` (never slower) until it has
//   gone `reach` or dropped below where it stood, and its turn (its parts' about its middle, as one) eased towards
//   `rate` until its torso leans `angle`, then held there, never faster than maxSpin (tipping over the lip flipped it).
// Each part eased a share 1 - e^(-dt / 0.06 s) of the way a frame: driven, not set (its joints still give).
void driveShove(RagdollBodies& r, float dt)
{
    RagdollBodies::ShoveDrive& d = r.shove;
    if(!d.on || dt <= 0.f)
    {
        return;
    }
    const float t = static_cast<float>(qcvm->time - d.start);
    const auto counted = [&](int b) { return !partCut(r, b) && r.rig->bones[b].joint != ragdoll::Joint::Loose; };
    // Its head (its rig's, else its highest part), for its torso's lean (pelvis to head).
    const auto leanHead = [&]() {
        if(r.rig->head >= 0 && r.rig->head < r.count && !partCut(r, r.rig->head))
        {
            return glmv(b3Body_GetWorldCenter(r.body[static_cast<za::SizeT>(r.rig->head)]));
        }
        glm::vec3 top = glmv(b3Body_GetWorldCenter(r.body[0]));
        for(int b = 0; b < r.count; b++)
        {
            const glm::vec3 p = glmv(b3Body_GetWorldCenter(r.body[static_cast<za::SizeT>(b)]));
            top = counted(b) && p.z > top.z ? p : top;
        }
        return top;
    };
    float mass = 0.f;
    glm::vec3 com{0.f}, vcom{0.f}, feet{0.f}, feetVel{0.f};
    int nFeet = 0;
    for(int b = 0; b < r.count; b++)
    {
        if(!counted(b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const float m = b3Body_GetMass(body);
        const glm::vec3 p = glmv(b3Body_GetWorldCenter(body));
        mass += m;
        com += p * m;
        vcom += glmv(b3Body_GetLinearVelocity(body)) * m;
        if((r.feet & (1u << b)) != 0u)
        {
            feet += p;
            feetVel += glmv(b3Body_GetLinearVelocity(body));
            nFeet++;
        }
    }
    if(mass <= 0.f)
    {
        d.on = false;
        return;
    }
    com /= mass;
    vcom /= mass;
    const float g = 1.f - za::exp(-dt / 0.06f);
    const glm::vec3 level{vcom.x, vcom.y, 0.f};
    if(d.ledge)
    {
        // Until it lands below (or 2.5 s): kept going until past the edge, and its turn about the level axis across
        // the shove (its parts' turn about its middle, as one) never faster than maxSpin (tipping over the lip flips it).
        const float gone = glm::dot(com - d.from, d.dir);
        const bool dropped = com.z < d.floor - 0.25f;
        if(t > 2.5f || (dropped && za::abs(vcom.z) < 0.3f))
        {
            d.on = false;
            return;
        }
        const float along = glm::dot(level, d.dir);
        // Past the edge (vr_knockdown_shove_ledge_over): its speed along the shove eased down to `over` of the kept speed,
        // so it tips over the lip and drops near the ledge's foot rather than flying on.
        const float cap = d.speed * d.over;
        glm::vec3 add = gone < d.reach && !dropped && along < d.speed ? d.dir * ((d.speed - along) * g) : glm::vec3{0.f};
        if(gone >= d.reach && d.over < 1.f && along > cap)
        {
            add = d.dir * ((cap - along) * g);
        }
        const glm::vec3 axis = glm::normalize(glm::cross(glm::vec3{0.f, 0.f, 1.f}, d.dir));
        float spinNum = 0.f, spinDen = 0.f;
        for(int b = 0; b < r.count; b++)
        {
            if(!counted(b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            glm::vec3 rel = glmv(b3Body_GetWorldCenter(body)) - com;
            rel -= axis * glm::dot(rel, axis);
            const float m = b3Body_GetMass(body);
            spinNum += m * glm::dot(glm::cross(rel, glmv(b3Body_GetLinearVelocity(body)) - vcom), axis);
            spinDen += m * glm::dot(rel, rel);
        }
        const float spin = spinDen > 1e-6f ? spinNum / spinDen : 0.f;
        // Its torso's lean along the shove turned towards `rate` until it reaches `angle` (then held there), and no
        // faster than maxSpin: eased a share g of the way a frame.
        const glm::vec3 torso = leanHead() - glmv(b3Body_GetWorldCenter(r.body[0]));
        const float lean = za::atan2(glm::dot(torso, d.dir), torso.z);
        const float want = za::min(d.rate * t, d.angle);
        const float wantRate = want < d.angle ? d.rate : 0.f;
        const float cut = (za::clamp(wantRate + 8.f * (want - lean), -d.maxSpin, d.maxSpin) - spin) * g;
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b) || (cut == 0.f && add == glm::vec3{0.f}))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const glm::vec3 p = glmv(b3Body_GetWorldCenter(body));
            b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) + add + glm::cross(axis * cut, p - com)));
            b3Body_SetAngularVelocity(body, b3v(glmv(b3Body_GetAngularVelocity(body)) + axis * cut));
            b3Body_SetAwake(body, true);
        }
        return;
    }
    // How far its torso leans along dir now (rad: 0 upright, a quarter flat, head ahead).
    const glm::vec3 head = leanHead();
    const glm::vec3 pelvis = glmv(b3Body_GetWorldCenter(r.body[0]));
    const glm::vec3 torso = head - pelvis;
    const float lean = za::atan2(glm::dot(torso, d.dir), torso.z);
    // The travel: the drive's (`speed` slowing evenly over `duration`), plus a pull towards where its pelvis should be by
    // now (the drive's way so far and what its lean has carried it about its feet; friction otherwise keeps it short).
    const float settle = 0.25f;
    const float tt = za::min(t, d.duration);
    const float planned = d.speed * tt - 0.5f * d.decel * tt * tt + d.pelvisUp * za::sin(za::clamp(lean, 0.f, 1.5707963f));
    const float off = planned - glm::dot(pelvis - d.pelvis0, d.dir);
    const float travel = za::clamp(za::max(d.speed - d.decel * t, 0.f) + 4.f * off, 0.f, za::max(d.speed, 1.f) * 1.5f);
    const bool turning = d.time > 0.f && t < d.time + settle;
    if(!turning && ((t > d.duration && za::abs(off) < 0.05f) || t > d.duration + 1.f))
    {
        d.on = false;
        return;
    }
    if(!turning)
    {
        // (Lying: its middle's level motion eased to `travel` along dir, every part alike.)
        const glm::vec3 shift = d.dir * travel - level;
        for(int b = 0; b < r.count; b++)
        {
            if(!partCut(r, b))
            {
                const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
                b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) + shift * g));
                b3Body_SetAwake(body, true);
            }
        }
        return;
    }
    // The turn: the curve's lean and rate now.
    const float u = za::clamp(t / d.time, 0.f, 1.f);
    const float want = d.angle * u * u * (2.f - u);
    const float rate = t < d.time ? d.angle * (4.f * u - 3.f * u * u) / d.time : 0.f;
    const float gain = 8.f; // 1/s: how hard a lean off the curve is pulled back to it
    const float spin = za::clamp(rate + gain * (want - lean), -d.maxSpin, d.maxSpin);
    const glm::vec3 axis = glm::normalize(glm::cross(glm::vec3{0.f, 0.f, 1.f}, d.dir)); // (spin > 0: its top along dir)
    const glm::vec3 pivot = nFeet > 0 ? feet / static_cast<float>(nFeet) : com;
    const float pivotUp = nFeet > 0 ? feetVel.z / static_cast<float>(nFeet) : 0.f;
    const float lagNow = d.lag * (1.f - u);
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const glm::vec3 p = glmv(b3Body_GetWorldCenter(body));
        const float share = d.share[static_cast<za::SizeT>(b)];
        const glm::vec3 target = d.dir * (travel * (1.f + lagNow * (share - d.meanShare) * 2.f)) +
                                 glm::vec3{0.f, 0.f, pivotUp} + glm::cross(axis * spin, p - pivot);
        const glm::vec3 lin = glmv(b3Body_GetLinearVelocity(body));
        const glm::vec3 ang = glmv(b3Body_GetAngularVelocity(body));
        b3Body_SetLinearVelocity(body, b3v(lin + (target - lin) * g));
        b3Body_SetAngularVelocity(body, b3v(ang + (axis * spin - ang) * g));
        b3Body_SetAwake(body, true);
    }
}
void feedRagdoll(edict_t* ent, Slot& s)
{
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    // Living and down, it struggles (vr_knockdown_wiggle; his note 2026-10-09: "knocked-down ragdolls don't move"): each
    // joint driven about the pose it settled in, its chest curling, its head nodding, its arms and legs kicking (a hinge
    // about its axis, a ball across its bone), each at its own pace. A drive strong enough to lift a limb lying on the
    // floor against gravity and the joints' friction (a torque of its inertia times a stiff spring, at most its weight's
    // lever a few times over); the parent takes the opposite (the body jerks with the kicks). A pose far off its rest (a
    // throw, a hand) is its new rest.
    // (Not while its own drive moves it: a stagger, a fall, a get-up; vr_ragactive.hpp. It struggles once it lies limp.)
    if(knockedDown(ent) && ent->v.health > 0.f && vr_knockdown_wiggle.value > 0.f && qcvm->time > r.born + 0.6 &&
       !r.shove.on && !r.active.on)
    {
        const float t = static_cast<float>(qcvm->time - r.born);
        const float frequency = za::clamp(vr_knockdown_wiggle_frequency.value, 0.1f, 5.f);
        const float strength = za::clamp(vr_knockdown_wiggle.value, 0.f, 3.f);
        const float pause = za::clamp(vr_knockdown_wiggle_pause.value, 0.f, 5.f);
        const float burst = pause == 0.f ? 1.f : za::max(0.f, za::sin(t * 6.2831853f / (pause + 1.f)));
        const float k = r.scale / world->m2u; // rest units -> metres
        const float friction = tune(ent, Tune::JointFriction); // (one class lookup, not one per bone)
        for(int b = 1; b < r.count; ++b)
        {
            const ragdoll::Bone& bone = r.rig->bones[b];
            if(partCut(r, b) || bone.parent < 0 || partCut(r, bone.parent) ||
                (bone.joint != ragdoll::Joint::Ball && bone.joint != ragdoll::Joint::Hinge)) { continue; }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
            const glm::quat qp = fromB3(b3Body_GetTransform(parent).q), qb = fromB3(b3Body_GetTransform(body).q);
            const glm::quat relative = glm::inverse(qp) * qb;
            if(!r.struggleReady) { r.struggleRest[static_cast<za::SizeT>(b)] = relative; }
            // Its swing (rest space): a hinge about its axis, a ball across its bone; how far by its kind.
            const glm::vec3 along = bone.end - bone.pivot;
            glm::vec3 axis = bone.hinge;
            if(bone.joint == ragdoll::Joint::Ball)
            {
                const glm::vec3 across = glm::cross(along, (b & 1) ? glm::vec3{0.f, 1.f, 0.f} : glm::vec3{1.f, 0.f, 0.f});
                axis = glm::length(across) > 1e-4f ? glm::normalize(across) : glm::vec3{0.f, 1.f, 0.f};
            }
            const float reach = bone.role == modelmeta::BoneRole::Chest ? 12.f : bone.role == modelmeta::BoneRole::Head ? 18.f : 30.f;
            const float pace = frequency * (0.75f + 0.5f * (static_cast<float>(b) * 0.618034f - static_cast<float>(b * 618034 / 1000000)));
            const float angle = glm::radians(reach) * strength * burst * burst *
                za::sin(t * pace * 6.2831853f + static_cast<float>(b) * 2.39996f);
            const glm::quat target = qp * r.struggleRest[static_cast<za::SizeT>(b)] * glm::angleAxis(angle, axis);
            glm::quat delta = glm::normalize(target * glm::inverse(qb));
            if(delta.w < 0.f) { delta = -delta; }
            const glm::vec3 error{delta.x * 2.f, delta.y * 2.f, delta.z * 2.f};
            // A throw or grab must not pull it back to an obsolete resting pose.
            if(glm::length(error) > glm::radians(reach * strength + 25.f))
            {
                r.struggleRest[static_cast<za::SizeT>(b)] = relative;
                continue;
            }
            const float m = b3Body_GetMass(body), length = za::max(glm::length(along) * k, 0.05f);
            const float inertia = m * length * length / 3.f; // (a rod about its end)
            constexpr float omega = 6.2831853f * 3.f;          // the drive's own pace: 3 Hz, damped near critically
            const glm::vec3 speed = glmv(b3Body_GetAngularVelocity(body)) - glmv(b3Body_GetAngularVelocity(parent));
            glm::vec3 torque = inertia * (error * (omega * omega) - speed * (1.6f * omega));
            const float magnitude = glm::length(torque);
            const float limit = friction * 1.8f + m * 9.8f * length * 1.5f * strength;
            if(magnitude > limit) { torque *= limit / magnitude; }
            b3Body_ApplyTorque(body, b3v(torque), true);
            b3Body_ApplyTorque(parent, b3v(-torque), true);
        }
        r.struggleReady = true;
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 moved = origin - s.origin;
    if(glm::length(moved) > 32.f)
    {
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3WorldTransform xf = b3Body_GetTransform(body);
            b3Body_SetTransform(body, b3v(glmv(xf.p) + glmv(world->toM(moved))), xf.q);
            b3Body_SetAwake(body, true);
        }
        s.origin = origin;
    }
    const glm::vec3 v = vec(ent->v.velocity);
    if(glm::length(v) > 1.f)
    {
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) + glmv(world->toM(v))));
            b3Body_SetAwake(body, true);
        }
        if(r.active.on && r.active.stagger.on)
        {
            ragactive::weaken(r.active, 0.5f); // (knocked again as it staggers: its balance half gone)
        }
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d knocked by QC: %.0f u/s\n", NUM_FOR_EDICT(ent), glm::length(v));
        }
        ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    }
    if(r.pinned != 0u)
    {
        // (Thrown: its feet held where they stand, or swept back, the pivot it topples over; they may still rise and
        // fall.)
        const bool hold = qcvm->time < r.pinUntil;
        for(int b = 0; b < r.count && hold; b++)
        {
            if((r.pinned & (1u << b)) == 0u || partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3Vec3 lin = b3Body_GetLinearVelocity(body);
            b3Body_SetLinearVelocity(body, b3Vec3{r.pinVel.x, r.pinVel.y, lin.z});
        }
        r.pinned = hold ? r.pinned : 0u;
    }
    if(r.settle != 1.f)
    {
        // (A head pop: the blast's knock and pushes, all of this frame's, in; the body keeps only this share of them, and
        // its own motion, ownLin, in full.)
        const float k = za::max(r.settle, 0.f);
        float before = 0.f, after = 0.f;
        glm::vec3 pelvisOwn{0.f}, pelvisAfter{0.f};
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const glm::vec3 lin = glmv(b3Body_GetLinearVelocity(body));
            const glm::vec3 own = r.ownLin[static_cast<za::SizeT>(b)], ownAng = r.ownAng[static_cast<za::SizeT>(b)];
            const glm::vec3 kept = own + (lin - own) * k;
            before = za::max(before, glm::length(lin) * world->m2u);
            after = za::max(after, glm::length(kept) * world->m2u);
            if(b == 0)
            {
                pelvisOwn = own * world->m2u;
                pelvisAfter = kept * world->m2u;
            }
            b3Body_SetLinearVelocity(body, b3v(kept));
            b3Body_SetAngularVelocity(body, b3v(ownAng + (glmv(b3Body_GetAngularVelocity(body)) - ownAng) * k));
        }
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d settled after its head popped: fastest part %.0f -> %.0f u/s; pelvis %.0f u/s (its own "
                       "motion %.0f u/s, %.0f level)\n", NUM_FOR_EDICT(ent), before, after, glm::length(pelvisAfter),
                glm::length(pelvisOwn), glm::length(glm::vec2{pelvisOwn}));
        }
        r.settle = 1.f;
    }
}

// ---- Active ragdolls (vr_ragdoll_active; vr_ragactive.hpp) ----
// Each frame before the step, a driven ragdoll's joints set (vr_ragactive.cpp drive): its parts a hand holds (and a
// throw's pinned feet) given no strength, the floor below for a step's or a crawl's hand. vr_ragdoll_active 0 makes a
// driven one limp at once.

// The floor's height (metres) below `at` (metres) within `depth`: the world's and the movers' (-1e30 none).
float activeFloorBelow(const glm::vec3& at, float depth)
{
    struct Hit
    {
        float z{-1e30f};
    } hit;
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = catWorld | catMover;
    b3World_CastRay(world->id, b3v(at), b3Vec3{0.f, 0.f, -za::max(depth, 0.01f)}, filter,
        [](b3ShapeId, b3Pos point, b3Vec3, float fraction, uint64_t, int, int, void* context) -> float {
            static_cast<Hit*>(context)->z = point.z;
            return fraction;
        },
        &hit);
    return hit.z;
}

// Whether `body` lies on the floor: a contact of its with the world or a mover pushing it up (its normal within 60
// degrees of up), whatever its shape's height over it (a bulky part, a pack, lies on it with its middle high).
bool activeOnFloor(b3BodyId body)
{
    za::Array<b3ContactData, 8> contacts;
    const int count = b3Body_GetContactData(body, contacts.data(), static_cast<int>(contacts.size()));
    for(int i = 0; i < count; i++)
    {
        const b3ContactData& d = contacts[static_cast<za::SizeT>(i)];
        const bool isA = B3_ID_EQUALS(b3Shape_GetBody(d.shapeIdA), body);
        const b3ShapeId other = isA ? d.shapeIdB : d.shapeIdA;
        if((b3Shape_GetFilter(other).categoryBits & (catWorld | catMover)) == 0)
        {
            continue;
        }
        for(int m = 0; m < d.manifoldCount; m++)
        {
            // (The normal points from A to B: the floor pushes its part up.)
            const float up = isA ? -d.manifolds[m].normal.z : d.manifolds[m].normal.z;
            if(d.manifolds[m].pointCount > 0 && up > 0.5f)
            {
                return true;
            }
        }
    }
    return false;
}

namespace
{
// The liquid at `at` (metres), for a dying body's swim (vr_ragactive.cpp, "Liquids").
ragactive::Liquid activeLiquidAt(const glm::vec3& at)
{
    switch(worldtrace::liquidContents(at * world->m2u, false))
    {
        case CONTENTS_WATER: return ragactive::Liquid::Water;
        case CONTENTS_SLIME: return ragactive::Liquid::Slime;
        case CONTENTS_LAVA: return ragactive::Liquid::Lava;
        default: return ragactive::Liquid::None;
    }
}

// The world and the movers along `from` -> `to` (metres): the share of the way to the first hit (1 none), its point
// and normal.
float activeRayHit(const glm::vec3& from, const glm::vec3& to, glm::vec3& at, glm::vec3& normal)
{
    struct Hit
    {
        float fraction{1.f};
        glm::vec3 at{0.f}, normal{0.f};
    } hit;
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = catWorld | catMover;
    b3World_CastRay(world->id, b3v(from), b3v(to - from), filter,
        [](b3ShapeId, b3Pos point, b3Vec3 n, float fraction, uint64_t, int, int, void* context) -> float {
            Hit& h = *static_cast<Hit*>(context);
            h.fraction = fraction;
            h.at = glm::vec3{point.x, point.y, point.z};
            h.normal = glm::vec3{n.x, n.y, n.z};
            return fraction;
        },
        &hit);
    at = hit.at;
    normal = hit.normal;
    return hit.fraction;
}
} // namespace

[[nodiscard]] LiquidFn liquidFor(int num)
{
    if(const ragtest::Force* f = ragtest::forced(num); f && f->liquid != ragactive::Liquid::None)
    {
        constexpr LiquidFn forcedLiquid[] = {nullptr, [](const glm::vec3&) { return ragactive::Liquid::Water; },
            [](const glm::vec3&) { return ragactive::Liquid::Slime; }, [](const glm::vec3&) { return ragactive::Liquid::Lava; }};
        return forcedLiquid[static_cast<int>(f->liquid)];
    }
    return activeLiquidAt;
}

namespace
{
// Whether entity `num` is one a falling body grabs at (vr_ragdoll_active_grab): a monster, standing (its box) or its
// ragdoll (knocked down or dead); not a fish, nor a flyer.
bool activeGraspable(int num)
{
    if(num <= 0 || num >= qcvm->num_edicts)
    {
        return false;
    }
    const edict_t* e = EDICT_NUM(num);
    const int flags = static_cast<int>(e->v.flags);
    return !e->free && (flags & FL_MONSTER) != 0 && (flags & (FL_SWIM | FL_FLY)) == 0;
}
constexpr double swimGoalEvery = 1.0; // s: a living swimmer's goal looked for again

// A living knockdown (of the active classes) lying in a liquid deep enough (vr_ragdoll_active_liquids; ROUND22.md,
// "Living knockdowns swim"): its drive begun as a living swim (ragactive::beginLivingSwim: it paddles, or flails, at
// full strength, its head up), its goal where it can stand (standSpot's liquid search: the shallows, the bank), looked
// for again every swimGoalEvery; killed, its swim a dying one's.
void livingSwim(RagdollBodies& r)
{
    edict_t* ent = EDICT_NUM(r.num);
    ragactive::State& a = r.active;
    if(ragactive::swimming(a))
    {
        if(!a.swim.living)
        {
            return;
        }
        if(ent->v.health <= 0.f)
        {
            ragactive::swimDies(a, qcvm->time);
            return;
        }
        if(ragactive::flailing(a))
        {
            return; // (lifted, flailing in the air: no shore to make for)
        }
        if(qcvm->time - r.swimGoalAt >= swimGoalEvery || r.swimGoalAt < 0.0)
        {
            r.swimGoalAt = qcvm->time;
            const glm::vec3 pelvis = world->toU(b3Body_GetPosition(r.body[0]));
            glm::vec3 spot{0.f};
            const bool found = standSpot(ent, pelvis, pelvis, vec(ent->v.mins), vec(ent->v.maxs), 0.f, spot, false);
            ragactive::swimGoal(a, found, glmv(world->toM(spot)));
        }
        return;
    }
    // (Not while its drive leads it otherwise, but a fall: a stagger, a get-up; nor while the shove's topple carries it.)
    // (Its kind driven alive: box3d's activeLiving.)
    if(!knockedDown(ent) || ent->v.health <= 0.f || !activeClassOf(ent) || r.shove.on || (a.on && !a.fall.on) ||
        vr_ragdoll_active_liquids.value == 0.f)
    {
        return;
    }
    const glm::vec3 pelvis = glmv(b3Body_GetPosition(r.body[0]));
    const LiquidFn liquid = liquidFor(r.num);
    if(liquid(pelvis) == ragactive::Liquid::None &&
        liquid(pelvis + glm::vec3{0.f, 0.f, 8.f / world->m2u}) == ragactive::Liquid::None)
    {
        return;
    }
    const bool fresh = !a.on;
    if(fresh)
    {
        // (box3d's beginLiving)
        ragactive::begin(a, activeParts(r), modelOf(ent), glmv(world->toM(vec(ent->v.origin))), r.joints.data(),
            r.jointFriction, r.jointStiffness, qcvm->time);
    }
    ragactive::Context c; // (what beginLivingSwim reads)
    c.world = world->id;
    c.now = qcvm->time;
    c.gravity = za::max(-b3World_GetGravity(world->id).z, 0.f);
    c.liquidAt = liquid;
    c.num = r.num;
    c.born = r.born;
    if(ragactive::beginLivingSwim(a, activeParts(r), c))
    {
        r.swimGoalAt = -1.0;
        if(r.drowning)
        {
            a.swim.panic = 1.f; // (drowning: frantic strokes at first)
            r.drowning = false;
        }
        return;
    }
    if(fresh)
    {
        ragactive::end(a);
    }
}

// A living knockdown (of the active classes) not driven, falling past a ledge's lip near a hand (dropped from a hand,
// toppled off a lip; vr_ragdoll_active_pullup; ROUND22.md, "Ledge pull-ups"): its drive begun holding the frame it went
// down in, a climb (ragactive::beginClimb: it grabs the lip, hangs, pulls itself up). A driven one's fall asks drive()'s
// own. Looked at every third frame while it falls (every frame past 2.5 m/s: past the lip in a moment).
void livingClimb(RagdollBodies& r, float dt)
{
    edict_t* ent = EDICT_NUM(r.num);
    ragactive::State& a = r.active;
    if(a.on || r.shove.on || vr_ragdoll_active_pullup.value <= 0.f || vr_ragdoll_active_env.value == 0.f ||
        vr_ragdoll_active_env_ledge.value == 0.f || !knockedDown(ent) || ent->v.health <= 0.f || !activeClassOf(ent) ||
        !sv.worldmodel || b3Body_GetLinearVelocity(r.body[0]).z > -0.8f ||
        (b3Body_GetLinearVelocity(r.body[0]).z > -2.5f && static_cast<int>(qcvm->time * 72.0 + 0.5) % 3 != 0)) // (falling fast: every frame)
    {
        return;
    }
    const glm::vec3 pelvis = glmv(b3Body_GetPosition(r.body[0]));
    if(activeLiquidAt(pelvis) != ragactive::Liquid::None)
    {
        return;
    }
    ragactive::begin(a, activeParts(r), modelOf(ent), glmv(world->toM(vec(ent->v.origin))), r.joints.data(), r.jointFriction,
        r.jointStiffness, qcvm->time);
    const int frame = static_cast<int>(ent->v.frame); // (the pose it went down in: its tone holds it)
    a.clip = ragactive::Clip{frame, frame, static_cast<float>(frame), qcvm->time, 10.f, false};
    ragactive::Context c;
    c.world = world->id;
    c.now = qcvm->time;
    c.dt = dt;
    c.gravity = za::max(-b3World_GetGravity(world->id).z, 0.f);
    c.floorBelow = activeFloorBelow;
    c.onFloor = activeOnFloor;
    c.rayHit = activeRayHit;
    c.num = r.num;
    c.born = r.born;
    c.envMask = catWorld | catMover | catProp | catSolid | catFixture;
    c.ledges = ledges::of(sv.worldmodel);
    if(!ragactive::beginClimb(a, activeParts(r), c))
    {
        ragactive::end(a);
    }
}
} // namespace

void driveActive(Slot& s, float dt)
{
    QVR_TRACY_SCOPE(tracyScope, "ragdoll active update");
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    const int every = static_cast<int>(vr_ragdoll_active_trace.value);
    const bool trace = every > 0 && static_cast<int>(qcvm->time * 72.0 + 0.5) % every == 0;
    // Its struggle in a player's hands (vr_struggle): what holds it, its drive begun (c.struggle below).
    ragactive::StruggleInput struggleIn;
    if(vr_ragdoll_active.value != 0.f)
    {
        livingSwim(r);
        livingClimb(r, dt);
        if(vr_struggle.value != 0.f || r.active.struggle.on)
        {
            livingStruggle(r, struggleIn);
        }
    }
    if(!r.active.on && !trace && !ragactive::liquidPending(r.active, qcvm->time))
    {
        return;
    }
    const ragactive::Parts parts = activeParts(r);
    if(vr_ragdoll_active.value == 0.f && r.active.on)
    {
        ragactive::release(r.active, parts);
    }
    if(r.active.on)
    {
        r.struggleReady = false; // (its struggle's rest: the pose it is left in)
        // A living one's drive (a stagger, a get-up) let go: killed (its body goes limp as it lies), or a get-up the QC
        // stopped asking for (hit or held down meanwhile: it slumps, and gets up again later).
        const edict_t* ent = EDICT_NUM(r.num);
        const ragactive::State& a = r.active;
        if((a.stagger.on || a.rise.on || a.fall.on || a.climb.on) && !a.death.on && ent->v.health <= 0.f)
        {
            ragactive::release(r.active, parts);
        }
        else if(a.rise.on && qcvm->time - a.rise.polled > 0.35)
        {
            if(vr_knockdown_debug.value)
            {
                Con_Printf("getup: %d lapsed (its get-up not asked for since %.2f s): it slumps\n", r.num,
                    qcvm->time - a.rise.polled);
            }
            ragactive::lapse(r.active, parts, qcvm->time);
            r.riseAsked = false; // (the QC already waits: not one of its lapses)
        }
        else if(a.rise.on)
        {
            // Carried off (ROUND22.md, "Carried into water: no snap back"): its pelvis in a liquid (it swims, and gets up
            // there later), or a player's hand holding it far from its get-up's spot: it slumps (one of its lapses), its
            // spot forgotten, never pinned back to where it lay.
            const b3Vec3 p = b3Body_GetPosition(r.body[0]);
            bool held = false;
            for(const World::RagdollGrab& g : world->ragdollGrabs)
            {
                held = held || g.num == r.num;
            }
            constexpr float carriedOff = 64.f; // units, level, from the get-up's spot
            const bool wet = liquidFor(r.num)(glmv(p)) != ragactive::Liquid::None;
            if(wet || (held && glm::length(glm::vec2{world->toU(p) - r.riseSpot}) > carriedOff))
            {
                if(vr_knockdown_debug.value)
                {
                    Con_Printf("getup: %d carried off its get-up (%s): it slumps\n", r.num,
                        wet ? "into a liquid" : "held far from its spot");
                }
                ragactive::lapse(r.active, parts, qcvm->time);
            }
        }
    }
    ragactive::Context c;
    c.world = world->id;
    c.now = qcvm->time;
    c.dt = dt;
    c.gravity = za::max(-b3World_GetGravity(world->id).z, 0.f);
    c.held = r.pinned;
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        c.held |= g.num == r.num && g.part >= 0 && g.part < r.count ? (1u << g.part) : 0u;
    }
    c.floorBelow = activeFloorBelow;
    c.onFloor = activeOnFloor;
    c.liquidAt = liquidFor(r.num); // (a test's swim on dry land: that liquid everywhere for its drive)
    c.rayHit = activeRayHit;
    c.num = r.num;
    c.born = r.born;
    c.trace = trace;
    // What is round him (vr_ragdoll_active_env): the world, the movers and the props his probes see; the world's ledges.
    c.envMask = catWorld | catMover | catProp | catSolid | catFixture;
    // (Looked up only for a drive whose env is on, or not started yet: not every frame for every collapse or swim.)
    const ragactive::Env& env = r.active.env;
    // (And a living one's fall or climb: vr_ragdoll_active_pullup.)
    const bool climbs = vr_ragdoll_active_pullup.value > 0.f && !r.active.death.on && (r.active.fall.on || r.active.climb.on);
    c.ledges = vr_ragdoll_active_env.value != 0.f && vr_ragdoll_active_env_ledge.value != 0.f && r.active.on &&
                       ((r.active.death.on && (!env.started || env.on)) || climbs) && sv.worldmodel
                   ? ledges::of(sv.worldmodel)
                   : nullptr;
    c.carried = r.shove.on;
    // Others to grab falling (vr_ragdoll_active_grab): monsters' boxes, ragdolls' and corpses' parts.
    c.othersMask = vr_ragdoll_active_grab.value > 0.f ? catActor | catCorpse : 0u;
    c.graspable = activeGraspable;
    c.struggle = &struggleIn;
    if(const ragtest::Force* f = ragtest::forced(r.num); f && f->grab)
    {
        c.grabSure = true; // (a test's: its chance made sure)
    }
    // Its forces this frame, kept for the step's later pieces (liftAgain).
    r.driveForce = {};
    r.driveForceAt = qcvm->time;
    ragactive::Parts driven = parts;
    driven.force = r.driveForce.data();
    ragactive::drive(r.active, driven, c);
    // A hand holding a stump: its palm bloodied (a light melee wound on the model where the hand is: vr_wounds.cpp).
    glm::vec3 at{0.f}, dir{0.f};
    while(ragactive::takeHandBlood(r.active, parts, at, dir))
    {
        constexpr int woundMelee = 3; // (QC QVR_WOUND_MELEE)
        const glm::vec3 u = at * world->m2u;
        const float org[3]{u.x, u.y, u.z}, way[3]{dir.x, dir.y, dir.z};
        progs::sendWound(r.num, org, way, woundMelee, 20, 0);
        if(vr_debug_ragdoll.value)
        {
            Con_Printf("ragdoll: %d its hand on the stump bloodied at %.0f %.0f %.0f\n", r.num, u.x, u.y, u.z);
        }
    }
}

namespace
{
// ---- Shocked ragdolls (vr_shock_seizure; QC vr_shock.qc) ----
// A dead body the lightning struck (alive or dead: its shock goes on through its death) convulses while it is shocked
// (.vr_shock_until, over .vr_shock_len), as long as its arcs crawl over it (vr_shock.cpp drawBodyDeath): each
// limb is driven, relative to the part it hangs from, towards a turning speed that jerks one way then back (shockPulse a
// second), about an axis of its own that changes every few jerks, the chest now and then bucking up. Driven as a speed
// (not a force) and shared between the two parts by their masses, it is bounded (never faster than shockLimbSpeed times
// vr_shock_seizure), it pushes the body nowhere as a whole (what turns a limb turns its parent back) and it eases off as
// the arcs thin out, letting go over the last moment: the body falls still as the arcs end.

constexpr float shockPulse = 9.f;      // jerks a second
constexpr float shockLimbSpeed = 14.f; // rad/s: a limb's turning speed at vr_shock_seizure 1, the shock fresh
constexpr float shockGrip = 25.f;      // 1/s: how fast a limb takes that speed
constexpr float shockBuck = 0.9f;      // m/s: the chest's buck up at vr_shock_seizure 1, fresh
constexpr float shockBuckChance = 0.2f; // a jerk's chance to buck
constexpr float shockFlop = 3.f;        // m/s: a limb's flop up off the floor at vr_shock_seizure 1, fresh
constexpr float shockFlopChance = 0.45f; // a limb's chance to flop at each jerk
constexpr float shockLetGo = 0.4f;       // s: the convulsions let go over the shock's last moment

[[nodiscard]] float shockHash(uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h >> 8) / static_cast<float>(1u << 24);
}
} // namespace

// How shocked `ent` is now: 1 fresh .. 0 over (0: not).
[[nodiscard]] float shockLeft(edict_t* ent)
{
    if(fields().vr_shock_until < 0)
    {
        return 0.f;
    }
    const double until = fieldFloat(ent, fields().vr_shock_until);
    if(until <= qcvm->time)
    {
        return 0.f;
    }
    const double len = za::max(0.1, static_cast<double>(fieldFloatOr(ent, fields().vr_shock_len, 1.f)));
    return static_cast<float>(za::clamp((until - qcvm->time) / len, 0.0, 1.0));
}

namespace
{
// A part's turning inertia (kg m^2, as a ball's) about a point `lever` metres off its middle.
[[nodiscard]] float shockInertia(b3BodyId body, float lever)
{
    const b3Matrix3 i = b3Body_GetLocalRotationalInertia(body);
    return (i.cx.x + i.cy.y + i.cz.z) / 3.f + b3Body_GetMass(body) * lever * lever;
}
} // namespace

void shockRagdoll(edict_t* ent, Slot& s, float dt)
{
    const float left = shockLeft(ent);
    const float strength = za::clamp(vr_shock_seizure.value, 0.f, 3.f);
    if(left <= 0.f || strength <= 0.f || dt <= 0.f)
    {
        return;
    }
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    // Easing off as the arcs do (1 fresh .. 0.45 worn off), let go over the last shockLetGo seconds: still as they end.
    const float t = static_cast<float>(qcvm->time);
    const float secondsLeft = static_cast<float>(fieldFloat(ent, fields().vr_shock_until) - qcvm->time);
    const float x = za::clamp(secondsLeft / shockLetGo, 0.f, 1.f);
    const float fade = (0.45f + 0.55f * left) * x * x * (3.f - 2.f * x);
    const float amp = shockLimbSpeed * strength * fade;
    const float release = za::min(1.f, fade * 4.f); // (letting go as it ends)
    const float friction = za::max(tune(ent, Tune::JointFriction), 0.f);
    float spun = 0.f, pushed = 0.f;
    int driven = 0, awakeParts = 0;
    const uint32_t num = static_cast<uint32_t>(r.num);
    const uint32_t jerk = static_cast<uint32_t>(za::max(0.f, t * shockPulse));
    for(int b = 1; b < r.count; ++b)
    {
        const ragdoll::Bone& bone = r.rig->bones[b];
        if(partCut(r, b) || bone.parent < 0 || partCut(r, bone.parent) || bone.joint == ragdoll::Joint::Loose)
        {
            continue;
        }
        const uint32_t ub = static_cast<uint32_t>(b);
        // An axis of its own for three jerks at a time, back and forth along it (each limb out of step with the next).
        const uint32_t phase = jerk + static_cast<uint32_t>(shockHash(num, ub, 7u) * 3.f);
        const glm::vec3 axis = glm::normalize(glm::vec3{shockHash(num, ub, phase / 3u * 3u + 1u) - 0.5f,
            shockHash(num, ub, phase / 3u * 3u + 2u) - 0.5f, shockHash(num, ub, phase / 3u * 3u + 3u) - 0.5f} + 1e-3f);
        const float sign = (phase & 1u) ? 1.f : -1.f;
        const float size = (0.45f + 0.55f * shockHash(num, ub, phase * 5u + 11u)) *
                           (bone.role == modelmeta::BoneRole::Chest ? 0.5f : 1.f);
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
        const glm::vec3 rel = glmv(b3Body_GetAngularVelocity(body)) - glmv(b3Body_GetAngularVelocity(parent));
        const glm::vec3 error = axis * (sign * size * amp) - rel;
        // The pair's inertia about the joint (each part's own, and its mass half the way between their middles off it).
        const float half = 0.5f * glm::length(glmv(b3Body_GetWorldCenter(body)) - glmv(b3Body_GetWorldCenter(parent)));
        const float ib = shockInertia(body, half), ip = shockInertia(parent, half);
        const float pair = ib * ip / za::max(ib + ip, 1e-6f);
        glm::vec3 torque = error * (pair * shockGrip);
        const float miss = glm::length(error);
        if(miss > 1e-4f)
        {
            torque += error / miss * (friction * za::min(1.f, miss / 0.5f)); // (the joint's friction overcome)
        }
        torque *= release;
        const float most = (pair * shockGrip * amp * 2.f + friction * 1.5f) * release;
        if(const float m = glm::length(torque); m > most)
        {
            torque *= most / m;
        }
        b3Body_ApplyTorque(body, b3v(torque), true);
        b3Body_ApplyTorque(parent, b3v(-torque), true);
        spun += glm::length(rel);
        pushed += glm::length(torque);
        awakeParts += b3Body_IsAwake(body) ? 1 : 0;
        ++driven;
    }
    if(vr_debug_ragdoll.value >= 2.f && (jerk % 9u) == 0u && static_cast<int>(jerk) != r.shockJerk)
    {
        Con_Printf("ragdoll: %d shocked: left %.2f, speed %.1f rad/s wanted, %.1f mean; torque %.2f N m mean (friction %.2f), %d of %d awake, dt %.4f\n", r.num, left, amp,
            driven ? spun / static_cast<float>(driven) : 0.f, driven ? pushed / static_cast<float>(driven) : 0.f, friction,
            awakeParts, driven, dt);
    }
    // Each jerk, as it starts: some limbs flop up off the floor (a lying limb's own drive can't overcome the floor's grip
    // on it), the part they hang from pushed the other way (the floor takes it: the body as a whole stays where it lies);
    // now and then the chest bucks up.
    if(static_cast<int>(jerk) != r.shockJerk)
    {
        r.shockJerk = static_cast<int>(jerk);
        const bool buck = shockHash(num, 99u, jerk) < shockBuckChance;
        for(int b = 1; b < r.count; ++b)
        {
            const ragdoll::Bone& bone = r.rig->bones[b];
            if(partCut(r, b) || bone.parent < 0 || partCut(r, bone.parent) || bone.joint == ragdoll::Joint::Loose)
            {
                continue;
            }
            const uint32_t ub = static_cast<uint32_t>(b);
            const bool chest = bone.role == modelmeta::BoneRole::Chest;
            if(chest ? !buck : shockHash(num, ub, jerk * 7u + 3u) >= shockFlopChance)
            {
                continue;
            }
            const float speed = (chest ? shockBuck : shockFlop) * strength * fade * (0.6f + 0.4f * shockHash(num, ub, jerk * 7u + 4u));
            const glm::vec3 kick{(shockHash(num, ub, jerk * 7u + 5u) - 0.5f) * 0.6f * speed,
                (shockHash(num, ub, jerk * 7u + 6u) - 0.5f) * 0.6f * speed, speed};
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
            const float mb = b3Body_GetMass(body), mp = za::max(b3Body_GetMass(parent), 1e-4f);
            b3Body_SetLinearVelocity(body, b3v(glmv(b3Body_GetLinearVelocity(body)) + kick));
            b3Body_SetLinearVelocity(parent, b3v(glmv(b3Body_GetLinearVelocity(parent)) - kick * za::min(1.f, mb / mp)));
            b3Body_SetAwake(body, true);
            if(vr_debug_ragdoll.value >= 3.f)
            {
                Con_Printf("ragdoll: %d part %d (mass %.1f, parent's %.1f) flops %.2f m/s, now %.2f\n", r.num, b, mb, mp,
                    glm::length(kick), glm::length(glmv(b3Body_GetLinearVelocity(body))));
            }
        }
    }
}

bool limbsBleed(edict_t* ent)
{
    // Alive: knocked down (vr_knockdown 1, 2 getting up; Killed clears it), or a monster with health left (its get-up's
    // last blend, after QC has stood it up). A corpse's limbs always bleed, as before.
    const bool alive = fieldFloatOr(ent, fields().vr_knockdown, 0.f) != 0.f ||
                       (ent->v.health > 0.f && (static_cast<int>(ent->v.flags) & FL_MONSTER) != 0);
    return !alive || fieldFloatOr(ent, fields().vr_bloodied, 1.f) != 0.f;
}

// After the step: the entity where its frame would have its pelvis where the ragdoll has it (a saved game's ragdoll is
// made again from its frame there: lying where it lay), its box round its parts (shots and blows meet the ragdoll's
// box, then its mesh: vr_hitmodel.cpp; then the QC's corpse damage), on the ground and still to Quake; the parts
// published for the client.
void writeRagdoll(edict_t* ent, Slot& s)
{
    QVR_TRACY_SCOPE(tracyScope, "ragdoll publish");
    RagdollBodies& r = world->ragdolls[static_cast<za::SizeT>(s.ragdoll)];
    const ragdoll::Rig& rig = *r.rig;
    {
        // (vr_ragdoll_speeds: how fast it goes after this step.)
        glm::vec3 momentum{0.f};
        float mass = 0.f;
        for(int b = 0; b < r.count; b++)
        {
            if(partCut(r, b))
            {
                continue;
            }
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            const glm::vec3 v = glmv(b3Body_GetLinearVelocity(body));
            const float m = b3Body_GetMass(body);
            momentum += v * m;
            mass += m;
            r.peakPart = za::max(r.peakPart, glm::length(v));
        }
        r.peakMiddle = za::max(r.peakMiddle, mass > 0.f ? glm::length(momentum) / mass : 0.f);
    }
    // Knocked down, then killed where it lay ("Knockdowns"): its loose pieces (its weapon, welded) go, as a dead one's
    // death frames hide them (the QC drops the weapon itself: vr_enemyguns.qc and the like).
    if(r.knocked && ent->v.health <= 0.f)
    {
        r.knocked = false;
        // A body killed while down keeps its original living joints; enable its shoulder contact now too.
        for(int b = 0; b < r.count; b++)
        {
            const ragdoll::Bone& bone = rig.bones[b];
            if(partCut(r, b) || bone.parent < 0 || strncmp(bone.name, "upperarm", 8) != 0) { continue; }
            const b3BodyId parent = r.body[static_cast<za::SizeT>(bone.parent)];
            za::Array<b3JointId, ragdoll::maxBones> attached{};
            const int count = b3Body_GetJoints(r.body[static_cast<za::SizeT>(b)], attached.data(), ragdoll::maxBones);
            for(int j = 0; j < count; j++)
            {
                const b3JointId joint = attached[static_cast<za::SizeT>(j)];
                if(B3_ID_EQUALS(b3Joint_GetBodyA(joint), parent) || B3_ID_EQUALS(b3Joint_GetBodyB(joint), parent))
                {
                    b3Joint_SetCollideConnected(joint, true);
                }
            }
        }
        while(r.count > 1 && rig.bones[r.count - 1].joint == ragdoll::Joint::Loose)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(r.count - 1)];
            if(B3_IS_NON_NULL(body) && b3Body_IsValid(body))
            {
                b3DestroyBody(body);
            }
            r.body[static_cast<za::SizeT>(r.count - 1)] = b3_nullBodyId;
            r.count--;
        }
    }
    za::Array<glm::quat, ragdoll::maxBones> rot{};
    za::Array<glm::vec3, ragdoll::maxBones> pos{};
    glm::vec3 lo{1e30f}, hi{-1e30f};
    bool awake = false;
    for(int b = 0; b < r.count; b++)
    {
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const b3WorldTransform xf = b3Body_GetTransform(body);
        rot[static_cast<za::SizeT>(b)] = fromB3(xf.q);
        pos[static_cast<za::SizeT>(b)] = world->toU(xf.p);
        awake = awake || b3Body_IsAwake(body);
        if(rig.bones[b].joint == ragdoll::Joint::Loose || partCut(r, b))
        {
            continue; // (his shotgun lying apart: not in his box)
        }
        const b3AABB box = b3Body_ComputeAABB(body);
        lo = glm::min(lo, world->toU(box.lowerBound));
        hi = glm::max(hi, world->toU(box.upperBound));
    }
    glm::quat frameRot;
    glm::vec3 framePos;
    ragdoll::bonePose(rig, ragdoll::poseOfFrame(r.rig->model, static_cast<int>(ent->v.frame)), 0, frameRot, framePos);
    const glm::vec3 origin = pos[0] - r.turn * (framePos * r.scale);
    if(origin.z < world->map->mins[2] - 1024.f)
    {
        // (Fallen out of the world: it stops there, as a prop.)
        for(int b = 0; b < r.count; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            b3Body_SetLinearVelocity(body, b3Vec3_zero);
            b3Body_SetAngularVelocity(body, b3Vec3_zero);
            b3Body_SetAwake(body, false);
        }
    }
    store(origin, ent->v.origin);
    if(lo.x < hi.x)
    {
        const glm::vec3 mins = lo - origin, maxs = hi - origin;
        if(glm::any(glm::greaterThan(glm::abs(mins - vec(ent->v.mins)), glm::vec3{0.5f})) ||
            glm::any(glm::greaterThan(glm::abs(maxs - vec(ent->v.maxs)), glm::vec3{0.5f})))
        {
            store(mins, ent->v.mins);
            store(maxs, ent->v.maxs);
            store(maxs - mins, ent->v.size);
        }
    }
    ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
    setFlag(ent, FL_ONGROUND, true);
    SV_LinkEdict(ent, false);
    s.origin = origin;
    s.asleep = !awake;
    // The local player's hands holding its limbs (drawn following them between steps: vr_ragdoll_held_local), and the
    // time of the message after this step (qcvm->time is advanced after the physics: SV_Physics).
    int heldBy = 0;
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        if(g.num == r.num && g.player == cl.viewentity && (g.hand == 0 || g.hand == 1))
        {
            heldBy |= 1 << g.hand;
        }
    }
    ragdoll::publish(r.num, r.rig, r.count, rot.data(), pos.data(), r.scale, qcvm->time + host_frametime, heldBy, r.cut,
        limbsBleed(ent));
}
} // namespace box3d::detail
