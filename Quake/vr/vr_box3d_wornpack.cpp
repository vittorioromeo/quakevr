#include "vr_alloccount.h"
// vr_box3d_wornpack.cpp -- the worn backpack on a ragdoll (QC vr_wornpack.qc; ROUND23.md, "Worn backpacks"): a box shape
// of its chest part's body where the pack sits on its back (agarmor::packWhere as it is made), so the pack weighs, collides
// and is held with the body; a hand's hold taken on it (beginGrab: RagdollGrab::onPack) is measured for the QC, which
// tears the pack off (wornPack 1) on a hard pull, a hard landing or a hit on it. Its place for the drawn pack (QC's
// entity) is the shape's while it is on a ragdoll, else the posed model's.

#include "vr_box3d_internal.hpp"
#include "vr_agarmor.hpp"
#include "vr_held.hpp"
#include "vr_foegrab.hpp"

#include <cstring>
#include <utility>

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
    glm::vec3 restOrigin, mid, half;
    glm::mat3 restAxes;
    const int part = chestPart(r);
    if(part < 0 || !qvr::agarmor::packRest(ent, restOrigin, restAxes, mid, half))
    {
        return;
    }
    // On the chest part where it lies on the chest bone's rest pose: the part's body is that bone as drawn when the
    // ragdoll began (createRagdollParts: a rest point p at the body's place + its turn * p * scale), so the pack sits on
    // the back as drawn whatever pose the ragdoll was made from (not the frame's pose: a grunt bent to take a weapon).
    const b3BodyId body = r.body[static_cast<za::SizeT>(part)];
    const glm::quat bodyTurn = fromB3(b3Body_GetRotation(body));
    const glm::vec3 restMid = restOrigin + restAxes * mid;
    const glm::vec3 at = world->toU(b3Body_GetPosition(body)) + bodyTurn * (restMid * r.scale);
    r.packLocal = b3Transform{b3Body_GetLocalPoint(body, world->toM(at)), toB3(glm::normalize(glm::quat_cast(restAxes)))};
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

bool packNearest(const RagdollBodies& r, const glm::vec3& at, glm::vec3& out)
{
    if(!packOn(r))
    {
        return false;
    }
    glm::vec3 mid;
    glm::quat turn;
    packBox(r, mid, turn);
    const glm::vec3 local = glm::inverse(turn) * (at - mid);
    out = mid + turn * glm::clamp(local, -r.packHalf * packShrink, r.packHalf * packShrink);
    return true;
}

} // namespace box3d::detail

namespace qvr::box3d
{
using namespace ::box3d::detail;

bool wornPackBox(edict_t* ent, glm::vec3& mid, glm::mat3& axes, glm::vec3& half)
{
    RagdollBodies* r = ragdollOf(NUM_FOR_EDICT(ent));
    if(r && packOn(*r))
    {
        glm::quat turn;
        packBox(*r, mid, turn);
        axes = glm::mat3_cast(turn);
        half = r->packHalf;
        return true;
    }
    glm::vec3 origin;
    return fieldFloatOr(ent, fields().vr_wp_worn, 0.f) == 1.f && qvr::agarmor::packWhere(ent, origin, axes, mid, half);
}

float wornPackSegment(edict_t* ent, const glm::vec3& a, const glm::vec3& b, float grow, glm::vec3& point, glm::vec3& normal)
{
    glm::vec3 mid, half;
    glm::mat3 axes;
    if(!wornPackBox(ent, mid, axes, half))
    {
        return -1.f;
    }
    // (The slabs of its box grown by `grow`, in its own frame.)
    const glm::mat3 in = glm::transpose(axes);
    const glm::vec3 la = in * (a - mid), d = in * (b - a), h = half + glm::vec3{za::max(grow, 0.f)};
    float lo = 0.f, hi = 1.f;
    int face = -1;
    float faceSign = 0.f;
    for(int k = 0; k < 3; k++)
    {
        if(za::abs(d[k]) < 1e-6f)
        {
            if(za::abs(la[k]) > h[k])
            {
                return -1.f;
            }
            continue;
        }
        float t0 = (-h[k] - la[k]) / d[k], t1 = (h[k] - la[k]) / d[k];
        const float s = d[k] > 0.f ? -1.f : 1.f; // (the face entered: against the move)
        if(t0 > t1)
        {
            std::swap(t0, t1);
        }
        if(t0 > lo)
        {
            lo = t0;
            face = k;
            faceSign = s;
        }
        hi = za::min(hi, t1);
        if(lo > hi)
        {
            return -1.f;
        }
    }
    point = a + (b - a) * lo;
    glm::vec3 n{0.f};
    if(face >= 0)
    {
        n[face] = faceSign;
    }
    normal = face >= 0 ? axes * n : -glm::normalize(b - a); // (started inside it)
    return lo;
}

float wornPack(edict_t* ent, int what, int a, int b, const glm::vec3& p)
{
    const int num = NUM_FOR_EDICT(ent);
    RagdollBodies* r = ragdollOf(num);
    if(what == 4)
    {
        glm::vec3 mid, half;
        glm::mat3 axes;
        if(!wornPackBox(ent, mid, axes, half))
        {
            return -1.f;
        }
        const glm::vec3 local = glm::abs(glm::transpose(axes) * (p - mid)) - half;
        return glm::length(glm::max(local, glm::vec3{0.f}));
    }
    if(what == 5)
    {
        edict_t* player = a >= 1 && a <= svs.maxclients ? EDICT_NUM(a) : nullptr;
        return player ? qvr::foegrab::packStretch(player, b, ent) : -1.f;
    }
    if(what == 7)
    {
        edict_t* player = a >= 1 && a <= svs.maxclients ? EDICT_NUM(a) : nullptr;
        return player ? qvr::foegrab::packPalmGap(player, b, ent) : -1.f;
    }
    if(what == 6)
    {
        return static_cast<float>(qvr::foegrab::packTorn(ent));
    }
    if(what == 8)
    {
        edict_t* player = a >= 1 && a <= svs.maxclients ? EDICT_NUM(a) : nullptr;
        return player ? static_cast<float>(qvr::foegrab::packLift(player, ent)) : 0.f;
    }
    if(!r)
    {
        return what == 0 ? -1.f : what >= 2 ? -1.f : 0.f;
    }
    if(what == 0)
    {
        return packOn(*r) ? 1.f : 0.f;
    }
    if(what == 9)
    {
        if(!packOn(*r))
        {
            return -1.f;
        }
        glm::vec3 mid;
        glm::quat turn;
        packBox(*r, mid, turn);
        return glm::distance(mid, world->toU(b3Body_GetPosition(r->body[static_cast<za::SizeT>(r->packPart)])));
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
        // (3: the hand from the point it holds: the hold's own point on the hand, which begins where the pack's point was
        // (startHold `still`))
        const b3BodyId part = r->body[static_cast<za::SizeT>(g.part)];
        const glm::vec3 held = world->toU(b3Body_GetWorldPoint(part, b3Joint_GetLocalFrameB(g.joint).p));
        const glm::vec3 hand = world->toU(b3Body_GetWorldPoint(g.anchor, b3Joint_GetLocalFrameA(g.joint).p));
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
    if(what == 3)
    {
        // (Its entity's box: round the turned box, not round its diagonal's sphere.)
        glm::vec3 boxMid, boxHalf;
        glm::mat3 boxAxes;
        if(!wornPackBox(ent, boxMid, boxAxes, boxHalf))
        {
            return glm::vec3{0.f};
        }
        glm::vec3 extent{0.f};
        for(int k = 0; k < 3; k++)
        {
            extent += glm::abs(boxAxes[k]) * boxHalf[k];
        }
        return extent;
    }
    return what == 2 ? mid : origin;
}

} // namespace qvr::box3d
