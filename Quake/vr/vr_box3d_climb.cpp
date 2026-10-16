#include "vr_alloccount.h"
// vr_box3d_climb.cpp -- climbing props: a held prop's climb shape and its pull.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace box3d::detail
{
namespace
{
// Climbing props (vr_climb.cpp, "Holds on props"): the shapes made for their bodies (climbShape), by edict number.
struct ClimbShapeCache
{
    struct Entry
    {
        b3BodyId body{b3_nullBodyId};
        qvr::box3d::ClimbShape shape;
    };
    za::Vector<Entry> byNum;
    auto members() { return mem::list(byNum); }
};
mem::Cache<ClimbShapeCache> climbShapes{"box3d climb shapes", mem::MapChange};

[[nodiscard]] za::SizeT heldBytes(const ClimbShapeCache::Entry& e)
{
    return qvr::box3d::heldBytes(e.shape);
}

// A hull's faces into `out` (its points in the body's frame, units).
void addClimbHull(const b3HullData* hull, qvr::box3d::ClimbShape& out)
{
    const b3Vec3* pts = b3GetHullPoints(hull);
    const b3HullHalfEdge* edges = b3GetHullEdges(hull);
    const b3Plane* planes = b3GetHullPlanes(hull);
    const b3HullFace* faces = b3GetHullFaces(hull);
    if(!pts || !edges || !planes || !faces)
    {
        return;
    }
    const int base = static_cast<int>(out.points.size()), faceBase = static_cast<int>(out.faces.size());
    for(int i = 0; i < hull->vertexCount; i++)
    {
        out.points.pushBack(world->toU(pts[i]));
    }
    for(int f = 0; f < hull->faceCount; f++)
    {
        qvr::box3d::ClimbShape::Face face;
        face.normal = glmv(planes[f].normal);
        face.first = static_cast<int>(out.corner.size());
        int e = faces[f].edge;
        for(int guard = 0; guard < hull->edgeCount; guard++)
        {
            out.corner.pushBack(base + edges[e].origin);
            out.across.pushBack(faceBase + edges[edges[e].twin].face);
            e = edges[e].next;
            if(e == faces[f].edge)
            {
                break;
            }
        }
        face.count = static_cast<int>(out.corner.size()) - face.first;
        out.faces.pushBack(face);
    }
}

void readClimbBody(int num, qvr::box3d::ClimbBody& body)
{
    const b3BodyId id = world->slots[num].body;
    const b3WorldTransform xf = b3Body_GetTransform(id);
    body.origin = world->toU(xf.p);
    body.turn = glm::mat3_cast(fromB3(xf.q));
    body.centre = world->toU(b3Body_GetWorldCenter(id));
    body.velocity = world->toU(b3Body_GetLinearVelocity(id));
    body.spin = glmv(b3Body_GetAngularVelocity(id));
    body.mass = qvr::box3d::propMass(EDICT_NUM(num));
    body.fixed = b3Body_GetType(id) != b3_dynamicBody;
    body.asleep = !b3Body_IsAwake(id);
}
} // namespace

// The climbing hands' pulls (climbPull), through each piece of the step.
void pressClimbing()
{
    for(const World::ClimbPull& p : world->climbPulls)
    {
        if(p.num < static_cast<int>(world->slots.size()) && B3_ID_EQUALS(world->slots[p.num].body, p.body) && b3Body_IsValid(p.body))
        {
            b3Body_ApplyForce(p.body, p.force, b3Body_GetWorldPoint(p.body, p.local), true);
            if(vr_debug_box3d.value >= 2.f)
            {
                const glm::vec3 v = world->toU(b3Body_GetLinearVelocity(p.body));
                Con_Printf("box3d: climbing pull on %d: %.0f N (%.0f kg, %s), its velocity %.1f %.1f %.1f\n", p.num,
                    glm::length(glmv(p.force)), b3Body_GetMass(p.body), b3Body_IsAwake(p.body) ? "awake" : "asleep", v.x, v.y, v.z);
            }
        }
    }
}

} // namespace box3d::detail

namespace qvr::box3d
{
za::SizeT heldBytes(const ClimbShape& s)
{
    return mem::heldBytes(s.points) + mem::heldBytes(s.corner) + mem::heldBytes(s.across) + mem::heldBytes(s.faces);
}

bool climbBody(int num, ClimbBody& body)
{
    if(!world || !solidProp(num))
    {
        return false;
    }
    readClimbBody(num, body);
    return true;
}

const ClimbShape* climbShape(int num, ClimbBody& body)
{
    if(!climbBody(num, body))
    {
        return nullptr;
    }
    za::Vector<ClimbShapeCache::Entry>& byNum = climbShapes.byNum;
    if(byNum.size() <= static_cast<za::SizeT>(num))
    {
        byNum.resize(static_cast<za::SizeT>(num) + 1);
    }
    ClimbShapeCache::Entry& e = byNum[static_cast<za::SizeT>(num)];
    const b3BodyId id = world->slots[num].body;
    if(!B3_ID_EQUALS(e.body, id))
    {
        e.body = id;
        e.shape.points.clear();
        e.shape.corner.clear();
        e.shape.across.clear();
        e.shape.faces.clear();
        za::Array<b3ShapeId, 16> shapes;
        const int count = b3Body_GetShapes(id, shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < count; i++)
        {
            if(b3Shape_GetType(shapes[static_cast<za::SizeT>(i)]) == b3_hullShape)
            {
                if(const b3HullData* hull = b3Shape_GetHull(shapes[static_cast<za::SizeT>(i)]))
                {
                    addClimbHull(hull, e.shape);
                }
            }
        }
    }
    return e.shape.faces.empty() ? nullptr : &e.shape;
}

void climbPull(int num, const glm::vec3& at, const glm::vec3& dir, float kg)
{
    if(!world || !solidProp(num))
    {
        return;
    }
    const b3BodyId id = world->slots[num].body;
    const float g = -b3World_GetGravity(world->id).z; // m/s^2 (Quake's gravity, as the props fall)
    world->climbPulls.pushBack(World::ClimbPull{num, id, b3Body_GetLocalPoint(id, world->toM(at)), b3v(dir * (kg * g))});
}

bool climbRay(const glm::vec3& from, const glm::vec3& to, int skip)
{
    const glm::vec3 delta = to - from;
    if(!world || glm::dot(delta, delta) < 1e-6f)
    {
        return false;
    }
    struct Query
    {
        int skip;
        bool hit;
    } q{skip, false};
    b3QueryFilter filter = b3DefaultQueryFilter();
    filter.maskBits = catProp;
    b3World_CastRay(world->id, world->toM(from), world->toM(delta), filter,
        [](b3ShapeId shape, b3Pos, b3Vec3, float fraction, uint64_t, int, int, void* context) -> float {
            auto& cq = *static_cast<Query*>(context);
            const int num = numOf(shape);
            if(num == cq.skip || !solidProp(num))
            {
                return -1.f; // (not this one)
            }
            cq.hit = true;
            return 0.f; // (one is enough)
        },
        &q);
    return q.hit;
}
} // namespace qvr::box3d
