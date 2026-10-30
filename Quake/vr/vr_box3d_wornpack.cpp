#include "vr_alloccount.h"
// vr_box3d_wornpack.cpp -- the worn backpack on a ragdoll (QC vr_wornpack.qc; ROUND23.md, "Worn backpacks"): a box shape
// of its chest part's body where the pack sits on its back (agarmor::packWhere as it is made), so the pack weighs, collides
// and is held with the body; a hand's hold taken on it (beginGrab: RagdollGrab::onPack) is measured for the QC, which
// tears the pack off (wornPack 1) on a hard pull, a hard landing or a hit on it. Its place for the drawn pack (QC's
// entity) is the shape's while it is on a ragdoll, else the posed model's.

#include "vr_box3d_internal.hpp"
#include "vr_agarmor.hpp"
#include "vr_held.hpp"

#include <cstring>

namespace box3d::detail
{
namespace
{
constexpr float packShrink = 0.9f; // its shape a little inside the drawn pack (its straps, its rounded corners)

// Its chest part (the rig's "chest"; -1 none, or cut off).
[[nodiscard]] int chestPart(const RagdollBodies& r)
{
    for(int b = 0; b < r.count; b++)
    {
        if(!std::strcmp(r.rig->bones[b].name, "chest") && !partCut(r, b))
        {
            return b;
        }
    }
    return -1;
}

[[nodiscard]] bool packOn(const RagdollBodies& r)
{
    return B3_IS_NON_NULL(r.pack) && b3Shape_IsValid(r.pack) && r.packPart >= 0 && r.packPart < r.count;
}

// Its box now: its middle (units) and its turn (world).
void packBox(const RagdollBodies& r, glm::vec3& mid, glm::quat& turn)
{
    const b3BodyId body = r.body[static_cast<za::SizeT>(r.packPart)];
    mid = world->toU(b3Body_GetWorldPoint(body, r.packLocal.p));
    turn = glm::normalize(fromB3(b3Body_GetRotation(body)) * fromB3(r.packLocal.q));
}
} // namespace

void createRagdollPack(RagdollBodies& r, edict_t* ent, int num, const Slot& s)
{
    r.pack = b3_nullShapeId;
    r.packPart = -1;
    if(fieldFloatOr(ent, fields().vr_wp_worn, 0.f) != 1.f || !r.rig) // (worn: the QC gave it one, vr_worn_backpacks)
    {
        return;
    }
    glm::vec3 origin, mid, half;
    glm::mat3 axes;
    const int part = chestPart(r);
    if(part < 0 || !qvr::agarmor::packWhere(ent, origin, axes, mid, half))
    {
        return;
    }
    const b3BodyId body = r.body[static_cast<za::SizeT>(part)];
    const glm::quat bodyTurn = fromB3(b3Body_GetRotation(body));
    r.packLocal = b3Transform{b3Body_GetLocalPoint(body, world->toM(mid)),
        toB3(glm::normalize(glm::inverse(bodyTurn) * glm::quat_cast(axes)))};
    r.packHalf = half;
    const glm::vec3 h = half * (packShrink / world->m2u);
    const b3BoxHull box = b3MakeTransformedBoxHull(h.x, h.y, h.z, r.packLocal);
    // (As its parts' shapes: createRagdollParts.)
    const bool selfCollide = vr_ragdoll_self_collide.value != 0.f;
    b3ShapeDef sd = shapeDef(num, catCorpse, s.corpseMask | (vr_ragdoll_collide_each.value || selfCollide ? catCorpse : 0));
    sd.enableCustomFiltering = true;
    sd.filter.groupIndex = selfCollide ? 0 : -num;
    sd.enablePreSolveEvents = selfCollide;
    sd.enableHitEvents = true;
    sd.baseMaterial.friction = s.corpseFriction;
    sd.density = za::max(vr_worn_backpack_mass.value, 0.1f) / za::max(8.f * h.x * h.y * h.z, 1e-6f);
    r.pack = b3CreateHullShape(body, &sd, &box.base);
    r.packPart = part;
    if(vr_worn_backpack_debug.value)
    {
        Con_Printf("wornpack: %d's ragdoll made with its pack on part %d (%s), %.1f kg\n", num, part, r.rig->bones[part].name,
            vr_worn_backpack_mass.value);
    }
}

float packDistance(const RagdollBodies& r, const glm::vec3& at)
{
    if(!packOn(r))
    {
        return -1.f;
    }
    glm::vec3 mid;
    glm::quat turn;
    packBox(r, mid, turn);
    const glm::vec3 local = glm::abs(glm::inverse(turn) * (at - mid)) - r.packHalf;
    return glm::length(glm::max(local, glm::vec3{0.f}));
}

} // namespace box3d::detail

namespace qvr::box3d
{
using namespace ::box3d::detail;

float wornPack(edict_t* ent, int what, int a, int b, const glm::vec3& p)
{
    const int num = NUM_FOR_EDICT(ent);
    RagdollBodies* r = ragdollOf(num);
    if(what == 4)
    {
        if(r && packOn(*r))
        {
            return packDistance(*r, p);
        }
        glm::vec3 origin, mid, half;
        glm::mat3 axes;
        if(fieldFloatOr(ent, fields().vr_wp_worn, 0.f) != 1.f || !qvr::agarmor::packWhere(ent, origin, axes, mid, half))
        {
            return -1.f;
        }
        const glm::vec3 local = glm::abs(glm::transpose(axes) * (p - mid)) - half;
        return glm::length(glm::max(local, glm::vec3{0.f}));
    }
    if(!r)
    {
        return what == 0 ? -1.f : what >= 2 ? -1.f : 0.f;
    }
    if(what == 0)
    {
        return packOn(*r) ? 1.f : 0.f;
    }
    if(what == 1)
    {
        const bool was = packOn(*r);
        if(was)
        {
            b3DestroyShape(r->pack, true);
            for(World::RagdollGrab& g : world->ragdollGrabs)
            {
                g.onPack = g.num == num ? false : g.onPack;
            }
            if(vr_worn_backpack_debug.value)
            {
                Con_Printf("wornpack: %d's pack torn off its ragdoll\n", num);
            }
        }
        r->pack = b3_nullShapeId;
        r->packPart = -1;
        return was ? 1.f : 0.f;
    }
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        if(g.player != a || g.hand != b || g.num != num || !g.onPack || !packOn(*r) || B3_IS_NULL(g.joint) ||
            !b3Joint_IsValid(g.joint))
        {
            continue;
        }
        if(what == 2)
        {
            return glm::length(glmv(b3Joint_GetConstraintForce(g.joint)));
        }
        // (3: the hand from the point it holds)
        const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(part, b3Joint_GetLocalFrameB(g.joint).p));
        const glm::vec3 hand = world->toU(b3Body_GetPosition(g.anchor));
        return glm::distance(held, hand);
    }
    return -1.f;
}

glm::vec3 wornPackWhere(edict_t* ent, int what)
{
    glm::vec3 origin{0.f}, mid{0.f}, half{0.f};
    glm::mat3 axes{1.f};
    RagdollBodies* r = ragdollOf(NUM_FOR_EDICT(ent));
    if(r && packOn(*r))
    {
        glm::quat turn;
        packBox(*r, mid, turn);
        axes = glm::mat3_cast(turn);
        glm::vec3 modelMid;
        qvr::agarmor::packModelBox(modelMid, half);
        origin = mid - axes * modelMid;
    }
    else if(!qvr::agarmor::packWhere(ent, origin, axes, mid, half))
    {
        return glm::vec3{0.f};
    }
    if(what == 1)
    {
        glm::vec3 angles;
        qvr::held::anglesFromAxes(axes, &angles[0], false);
        return angles;
    }
    return what == 2 ? mid : origin;
}

} // namespace qvr::box3d
