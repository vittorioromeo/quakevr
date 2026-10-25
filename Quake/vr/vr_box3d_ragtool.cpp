// vr_box3d_ragtool.cpp -- the ragdoll tool's physics side (vr_ragtool.hpp, docs/vr-port/RAGTOOL.md): the subject's
// ragdoll read and drawn (its parts' frames and bones, its joints' limits, the drive's targets, its contacts, its centre
// of mass over its support) and held by the virtual hand (the physgun's hold on a part of a kind). Only the tool calls it.

#include "vr_box3d_internal.hpp"

#include "vr_ragtool.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cstring>

namespace qvr::ragtool::physics
{
namespace
{
// The lines' width (units: Show::scale times 0.12) and the points' scale (Show::scale), set by draw.
struct DrawWidth
{
    float w{0.12f};
    float s{1.f};
};
DrawWidth drawWidth;
constexpr int mostSupport = 64; // contact points kept for the support's outline

const glm::vec4 white{1.f, 1.f, 1.f, 0.8f};
const glm::vec4 red{1.f, 0.2f, 0.15f, 1.f};
const glm::vec4 green{0.2f, 1.f, 0.3f, 1.f};
const glm::vec4 blue{0.3f, 0.5f, 1.f, 1.f};
const glm::vec4 cyan{0.2f, 0.9f, 1.f, 0.9f};
const glm::vec4 orange{1.f, 0.6f, 0.1f, 0.9f};
const glm::vec4 magenta{1.f, 0.3f, 0.9f, 1.f};

// The support's points this frame (the contacts near the floor), for its outline.
struct SupportScratch
{
    za::Array<glm::vec2, mostSupport> pts{};
    int count{0};
    za::Array<glm::vec2, mostSupport> hull{};
    int hullCount{0};
};
SupportScratch supportScratch;

[[nodiscard]] glm::vec3 partPoint(const ragactive::Parts& p, int b, const glm::vec3& rest)
{
    return world->toU(b3Body_GetWorldPoint(p.body[b], b3v(rest * p.k)));
}

[[nodiscard]] bool ownBody(const RagdollBodies& r, b3BodyId body)
{
    for(int b = 0; b < r.count; b++)
    {
        if(B3_ID_EQUALS(r.body[static_cast<za::SizeT>(b)], body))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] glm::vec3 centreOfMass(const RagdollBodies& r, float& mass)
{
    glm::vec3 sum{0.f};
    mass = 0.f;
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const float m = b3Body_GetMass(body);
        sum += world->toU(b3Body_GetWorldCenter(body)) * m;
        mass += m;
    }
    return mass > 0.f ? sum / mass : sum;
}

[[nodiscard]] float floorUnder(const glm::vec3& at, edict_t* ent)
{
    vec3_t start{at.x, at.y, at.z}, end{at.x, at.y, at.z - 512.f}, zero{0.f, 0.f, 0.f};
    const trace_t tr = SV_Move(start, zero, zero, end, MOVE_NOMONSTERS, ent);
    return tr.fraction < 1.f ? tr.endpos[2] : at.z - 512.f;
}

[[nodiscard]] float cross2(const glm::vec2& o, const glm::vec2& a, const glm::vec2& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// The support's outline (gift wrapping over supportScratch.pts): supportScratch.hull, counter-clockwise.
void supportHull()
{
    SupportScratch& s = supportScratch;
    s.hullCount = 0;
    if(s.count < 1)
    {
        return;
    }
    int start = 0;
    for(int i = 1; i < s.count; i++)
    {
        if(s.pts[i].x < s.pts[start].x || (s.pts[i].x == s.pts[start].x && s.pts[i].y < s.pts[start].y))
        {
            start = i;
        }
    }
    int at = start;
    do
    {
        s.hull[s.hullCount++] = s.pts[at];
        int next = (at + 1) % s.count;
        for(int i = 0; i < s.count; i++)
        {
            if(cross2(s.pts[at], s.pts[next], s.pts[i]) < 0.f)
            {
                next = i;
            }
        }
        at = next;
    } while(at != start && s.hullCount < mostSupport);
}

[[nodiscard]] bool insideHull(const glm::vec2& q)
{
    const SupportScratch& s = supportScratch;
    if(s.hullCount < 3)
    {
        return false;
    }
    for(int i = 0; i < s.hullCount; i++)
    {
        if(cross2(s.hull[i], s.hull[(i + 1) % s.hullCount], q) < 0.f)
        {
            return false;
        }
    }
    return true;
}

void drawFrames(const ragactive::Parts& p, const RagdollBodies& r)
{
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const ragdoll::Bone& bone = r.rig->bones[b];
        const b3WorldTransform xf = b3Body_GetTransform(p.body[b]);
        const glm::vec3 o = world->toU(xf.p);
        const glm::quat q = fromB3(xf.q);
        constexpr float axis = 3.f;
        lines::line(o, o + q * glm::vec3{axis, 0.f, 0.f}, drawWidth.w, red, red);
        lines::line(o, o + q * glm::vec3{0.f, axis, 0.f}, drawWidth.w, green, green);
        lines::line(o, o + q * glm::vec3{0.f, 0.f, axis}, drawWidth.w, blue, blue);
        lines::line(partPoint(p, b, bone.pivot), partPoint(p, b, bone.end), drawWidth.w * 1.5f, white, white);
        lines::point(partPoint(p, b, bone.pivot), 0.5f * drawWidth.s, white);
    }
}

// Each joint's limit: a ball's cone about its frame's z in the parent (cyan, its rim and four rays), a hinge's arc about
// its axis from its lower to its upper limit (orange), the bone where it is now (magenta) to read against it.
void drawLimits(const ragactive::Parts& p, const RagdollBodies& r)
{
    const ragactive::State& s = r.active;
    for(int b = 1; b < r.count; b++)
    {
        const ragdoll::Bone& bone = r.rig->bones[b];
        const b3JointId j = s.joint[static_cast<za::SizeT>(b)];
        if(partCut(r, b) || bone.parent < 0 || B3_IS_NULL(j) || !b3Joint_IsValid(j))
        {
            continue;
        }
        const glm::vec3 at = partPoint(p, b, bone.pivot);
        const float len = za::max(glm::length(bone.end - bone.pivot) * p.k * world->m2u, 4.f);
        const glm::quat parent = fromB3(b3Body_GetTransform(p.body[bone.parent]).q);
        const glm::quat frame = parent * fromB3(b3Joint_GetLocalFrameA(j).q);
        const glm::vec3 z = frame * glm::vec3{0.f, 0.f, 1.f}, x = frame * glm::vec3{1.f, 0.f, 0.f},
                        y = frame * glm::vec3{0.f, 1.f, 0.f};
        if(bone.joint == ragdoll::Joint::Ball)
        {
            const float cone = b3SphericalJoint_GetConeLimit(j);
            constexpr int sides = 16;
            glm::vec3 last{0.f};
            for(int i = 0; i <= sides; i++)
            {
                const float a = glm::two_pi<float>() * static_cast<float>(i) / sides;
                const glm::vec3 d = z * za::cos(cone) + (x * za::cos(a) + y * za::sin(a)) * za::sin(cone);
                const glm::vec3 rim = at + d * len;
                if(i > 0)
                {
                    lines::line(last, rim, drawWidth.w, cyan, cyan);
                }
                if(i % 4 == 0)
                {
                    lines::line(at, rim, drawWidth.w * 0.5f, cyan, cyan);
                }
                last = rim;
            }
        }
        else if(bone.joint == ragdoll::Joint::Hinge)
        {
            const float lo = b3RevoluteJoint_GetLowerLimit(j), hi = b3RevoluteJoint_GetUpperLimit(j);
            // (The bone's rest way in the parent, off the axis: the arc's zero.)
            glm::vec3 rest = parent * glm::normalize(bone.end - bone.pivot + glm::vec3{1e-4f});
            rest = glm::normalize(rest - z * glm::dot(rest, z) + x * 1e-4f);
            constexpr int steps = 12;
            glm::vec3 last{0.f};
            for(int i = 0; i <= steps; i++)
            {
                const float a = lo + (hi - lo) * static_cast<float>(i) / steps;
                const glm::vec3 d = glm::angleAxis(a, z) * rest;
                const glm::vec3 rim = at + d * len;
                if(i > 0)
                {
                    lines::line(last, rim, drawWidth.w, orange, orange);
                }
                if(i == 0 || i == steps)
                {
                    lines::line(at, rim, drawWidth.w * 0.5f, orange, orange);
                }
                last = rim;
            }
        }
        lines::line(at, partPoint(p, b, bone.end), drawWidth.w, magenta, magenta);
    }
}

// The contacts (red: pressed in; pink: touching) with anything but itself; those near the floor kept as its support.
void drawContacts(const RagdollBodies& r, float floor, bool show)
{
    supportScratch.count = 0;
    za::Array<b3ContactData, 16> contacts;
    for(int b = 0; b < r.count; b++)
    {
        if(partCut(r, b))
        {
            continue;
        }
        const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
        const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(body));
        const int n = b3Body_GetContactData(body, contacts.data(), static_cast<int>(contacts.size()));
        for(int c = 0; c < n; c++)
        {
            const b3ContactData& d = contacts[c];
            const bool isA = B3_ID_EQUALS(b3Shape_GetBody(d.shapeIdA), body);
            if(ownBody(r, b3Shape_GetBody(isA ? d.shapeIdB : d.shapeIdA)))
            {
                continue;
            }
            for(int m = 0; m < d.manifoldCount; m++)
            {
                for(int k = 0; k < d.manifolds[m].pointCount; k++)
                {
                    const b3ManifoldPoint& mp = d.manifolds[m].points[k];
                    const glm::vec3 at = centre + world->toU(isA ? mp.anchorA : mp.anchorB);
                    if(show)
                    {
                        lines::point(at, 0.9f * drawWidth.s, mp.separation < 0.f ? red : glm::vec4{1.f, 0.5f, 0.7f, 1.f});
                    }
                    if(at.z < floor + 6.f && supportScratch.count < mostSupport)
                    {
                        supportScratch.pts[supportScratch.count++] = glm::vec2{at.x, at.y};
                    }
                }
            }
        }
    }
}

void drawBalance(const RagdollBodies& r, edict_t* ent, float floor)
{
    float mass = 0.f;
    const glm::vec3 com = centreOfMass(r, mass);
    const glm::vec3 below{com.x, com.y, floor + 0.5f};
    supportHull();
    const bool over = insideHull(glm::vec2{com.x, com.y});
    const glm::vec4 c = over ? green : red;
    lines::point(com, 2.5f * drawWidth.s, glm::vec4{1.f, 1.f, 0.2f, 1.f});
    lines::line(com, below, drawWidth.w, glm::vec4{1.f, 1.f, 0.2f, 1.f}, c);
    lines::point(below, 1.8f * drawWidth.s, c);
    const SupportScratch& s = supportScratch;
    for(int i = 0; i < s.hullCount && s.hullCount > 1; i++)
    {
        const glm::vec2 a = s.hull[i], b = s.hull[(i + 1) % s.hullCount];
        lines::line(glm::vec3{a, floor + 0.5f}, glm::vec3{b, floor + 0.5f}, drawWidth.w * 1.5f, c, c);
    }
    (void)ent;
}

[[nodiscard]] int boneNamed(const ragdoll::Rig& rig, const char* const* names, int count)
{
    for(int n = 0; n < count; n++)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            if(!std::strcmp(rig.bones[b].name, names[n]))
            {
                return b;
            }
        }
    }
    for(int n = 0; n < count; n++) // (a looser match: a prefix)
    {
        for(int b = 0; b < rig.numBones; b++)
        {
            if(!std::strncmp(rig.bones[b].name, names[n], std::strlen(names[n])))
            {
                return b;
            }
        }
    }
    return -1;
}

[[nodiscard]] int partOf(const ragdoll::Rig& rig, Hold h)
{
    constexpr const char* arm[] = {"forearm_r", "hand_r", "upperarm_r", "forearm", "arm"};
    constexpr const char* leg[] = {"shin_l", "foot_l", "thigh_l", "shin", "leg"};
    switch(h)
    {
    case Hold::Head:
    case Hold::Chest:
        for(int b = 0; b < rig.numBones; b++)
        {
            if(rig.bones[b].role == (h == Hold::Head ? modelmeta::BoneRole::Head : modelmeta::BoneRole::Chest))
            {
                return b;
            }
        }
        return -1;
    case Hold::Arm: return boneNamed(rig, arm, 5);
    case Hold::Leg: return boneNamed(rig, leg, 5);
    default: return 0;
    }
}
} // namespace

const char* holdName(Hold h)
{
    constexpr const char* names[] = {"chest", "head", "arm", "leg", "pelvis"};
    const int i = static_cast<int>(h);
    return i >= 0 && i < static_cast<int>(Hold::Count) ? names[i] : "?";
}

bool focus(int num, glm::vec3& out)
{
    if(num <= 0 || num >= qcvm->num_edicts)
    {
        return false;
    }
    edict_t* ent = EDICT_NUM(num);
    if(ent->free)
    {
        return false;
    }
    if(const RagdollBodies* r = world ? ragdollOf(num) : nullptr; r && r->count > 0 && !partCut(*r, 0))
    {
        out = world->toU(b3Body_GetWorldCenter(r->body[0]));
        return true;
    }
    out = vec(ent->v.origin) + (vec(ent->v.mins) + vec(ent->v.maxs)) * 0.5f;
    return true;
}

void draw(int num, const Show& show)
{
    const RagdollBodies* rp = world && sv.active && num > 0 ? ragdollOf(num) : nullptr;
    if(!rp || !rp->rig || rp->count <= 0)
    {
        return;
    }
    const RagdollBodies& r = *rp;
    const ragactive::Parts p = activeParts(r);
    edict_t* ent = EDICT_NUM(num);
    drawWidth.s = za::clamp(show.scale, 0.25f, 20.f);
    drawWidth.w = 0.12f * drawWidth.s;
    float mass = 0.f;
    const float floor = floorUnder(centreOfMass(r, mass), ent);
    if(show.bones)
    {
        drawFrames(p, r);
    }
    if(show.limits && r.active.on)
    {
        drawLimits(p, r);
    }
    if(show.targets && r.active.on)
    {
        ragactive::debugDraw(r.active, p);
    }
    drawContacts(r, floor, show.contacts);
    if(show.balance)
    {
        drawBalance(r, ent, floor);
    }
}

bool takeHold(int num, Hold h, glm::vec3& grip)
{
    const RagdollBodies* r = world ? ragdollOf(num) : nullptr;
    if(!r || !r->rig)
    {
        return false;
    }
    const int part = partOf(*r->rig, h);
    if(part < 0 || part >= r->count || partCut(*r, part))
    {
        return false;
    }
    const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(r->body[static_cast<za::SizeT>(part)]));
    // (The beam straight down onto the part from over it: the grip on its top, as a hand takes it.)
    const glm::vec3 from = centre + glm::vec3{0.f, 0.f, 24.f};
    int got = -1;
    float distance = 0.f;
    if(!box3d::physgunRagdollGrab(num, from, glm::vec3{0.f, 0.f, -1.f}, 48.f, centre, got, distance))
    {
        return false;
    }
    grip = from + glm::vec3{0.f, 0.f, -distance};
    return true;
}

Sample sample(int num)
{
    Sample out;
    const RagdollBodies* rp = world && num > 0 ? ragdollOf(num) : nullptr;
    if(!rp || !rp->rig || rp->count <= 0)
    {
        if(num > 0 && num < qcvm->num_edicts && !EDICT_NUM(num)->free)
        {
            out.pelvis = out.com = vec(EDICT_NUM(num)->v.origin);
        }
        return out;
    }
    const RagdollBodies& r = *rp;
    out.ragdoll = true;
    out.pelvis = world->toU(b3Body_GetWorldCenter(r.body[0]));
    float mass = 0.f;
    out.com = centreOfMass(r, mass);
    out.floor = floorUnder(out.com, EDICT_NUM(num));
    out.driven = ragactive::tracking(r.active, activeParts(r), out.error, out.strength);
    return out;
}
} // namespace qvr::ragtool::physics
