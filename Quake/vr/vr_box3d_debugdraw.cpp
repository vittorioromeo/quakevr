#include "vr_alloccount.h"
// vr_box3d_debugdraw.cpp -- the debug drawing of bodies, hulls and ragdolls.
// Split from vr_box3d.cpp (see its comment for the whole); the shared declarations are in
// vr_box3d_internal.hpp.

#include "vr_box3d_internal.hpp"

namespace qvr::box3d
{
namespace
{
// vr_debug_physics_shapes: a body's colour by what it is and does.
[[nodiscard]] glm::vec4 shapeColour(const Slot& s, bool awake)
{
    switch(s.kind)
    {
    case Kind::Prop:
        if(!awake)
        {
            return {0.35f, 0.5f, 1.f, 0.9f}; // asleep: blue
        }
        return s.bullet ? glm::vec4{1.f, 1.f, 1.f, 1.f} : glm::vec4{0.2f, 1.f, 0.3f, 1.f}; // awake: green (fast: white)
    case Kind::Held: return {1.f, 0.9f, 0.1f, 1.f};    // held: yellow
    case Kind::Mover: return {0.9f, 0.3f, 1.f, 0.7f};  // doors, plats: purple
    case Kind::Actor: return {1.f, 0.45f, 0.1f, 0.9f}; // monsters: orange
    case Kind::Player: return {0.2f, 0.9f, 1.f, 0.6f}; // players: cyan
    case Kind::Fixture: return {0.75f, 0.75f, 0.75f, 0.8f}; // pickups hanging: grey
    case Kind::Corpse:
        return s.corpseDynamic && awake ? glm::vec4{1.f, 0.4f, 0.6f, 1.f} : glm::vec4{0.7f, 0.2f, 0.3f, 0.9f}; // corpses: dark red (pushed: pink)
    default: return {1.f, 0.f, 0.f, 1.f};
    }
}

void drawHull(const b3HullData* hull, const b3WorldTransform& xf, const glm::vec4& colour, float width)
{
    const b3Vec3* points = b3GetHullPoints(hull);
    const b3HullHalfEdge* edges = b3GetHullEdges(hull);
    for(int i = 0; i < hull->edgeCount; i++)
    {
        const b3HullHalfEdge& e = edges[i];
        if(e.twin < i)
        {
            continue; // each edge once
        }
        const glm::vec3 a = world->toU(b3TransformPoint(xf, points[e.origin]));
        const glm::vec3 b = world->toU(b3TransformPoint(xf, points[edges[e.twin].origin]));
        lines::line(a, b, width, colour, colour);
    }
}

void drawCapsule(const b3Capsule& c, const b3WorldTransform& xf, const glm::vec4& colour, float width)
{
    const glm::vec3 a = world->toU(b3TransformPoint(xf, c.center1)), b = world->toU(b3TransformPoint(xf, c.center2));
    const float r = c.radius * world->m2u;
    constexpr int sides = 12;
    for(const glm::vec3& centre : {a, b})
    {
        for(int i = 0; i < sides; i++)
        {
            const float t0 = 2.f * glm::pi<float>() * static_cast<float>(i) / sides, t1 = 2.f * glm::pi<float>() * static_cast<float>(i + 1) / sides;
            lines::line(centre + r * glm::vec3{za::cos(t0), za::sin(t0), 0.f}, centre + r * glm::vec3{za::cos(t1), za::sin(t1), 0.f}, width,
                colour, colour);
        }
    }
    for(int i = 0; i < 4; i++)
    {
        const float t = glm::half_pi<float>() * static_cast<float>(i);
        const glm::vec3 o = r * glm::vec3{za::cos(t), za::sin(t), 0.f};
        lines::line(a + o, b + o, width, colour, colour);
    }
}

} // namespace

void activeRagdollDraw()
{
    if(!world || !sv.active || !vr_ragdoll_active_debug.value)
    {
        return;
    }
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num > 0 && r.active.on)
        {
            ragactive::debugDraw(r.active, activeParts(r));
        }
    }
}

void debugDraw()
{
    if(!world || !sv.active)
    {
        return;
    }
    za::Array<b3ShapeId, 64> shapes;
    za::Array<b3ContactData, 16> contacts;
    constexpr float width = 0.15f;
    for(int num = 1; num < static_cast<int>(world->slots.size()); num++)
    {
        const Slot& s = world->slots[num];
        if(s.kind == Kind::None || !b3Body_IsValid(s.body))
        {
            continue;
        }
        const bool awake = b3Body_IsAwake(s.body);
        glm::vec4 colour = shapeColour(s, awake);
        if(s.kind == Kind::Player && num == cl.viewentity)
        {
            colour.a *= 0.4f; // your own, round you: faint
        }
        const b3WorldTransform xf = b3Body_GetTransform(s.body);
        const int count = b3Body_GetShapes(s.body, shapes.data(), static_cast<int>(shapes.size()));
        for(int i = 0; i < count; i++)
        {
            switch(b3Shape_GetType(shapes[i]))
            {
            case b3_hullShape: drawHull(b3Shape_GetHull(shapes[i]), xf, colour, width); break;
            case b3_capsuleShape: drawCapsule(b3Shape_GetCapsule(shapes[i]), xf, colour, width); break;
            default: break;
            }
        }
        if(s.kind != Kind::Prop && s.kind != Kind::Held)
        {
            continue;
        }
        // The centre of mass, and (awake) where it touches things: red dots, pressed into them (or held apart: pink).
        lines::point(world->toU(b3Body_GetWorldCenter(s.body)), 0.8f, colour);
        if(!awake)
        {
            continue;
        }
        const int touching = b3Body_GetContactData(s.body, contacts.data(), static_cast<int>(contacts.size()));
        for(int c = 0; c < touching; c++)
        {
            const b3ContactData& d = contacts[c];
            const bool isA = B3_ID_EQUALS(b3Shape_GetBody(d.shapeIdA), s.body);
            const glm::vec3 centre = world->toU(b3Body_GetWorldCenter(s.body));
            for(int m = 0; m < d.manifoldCount; m++)
            {
                for(int p = 0; p < d.manifolds[m].pointCount; p++)
                {
                    const b3ManifoldPoint& mp = d.manifolds[m].points[p];
                    const glm::vec3 at = centre + world->toU(isA ? mp.anchorA : mp.anchorB);
                    lines::point(at, 0.7f, mp.separation < 0.f ? glm::vec4{1.f, 0.1f, 0.1f, 1.f} : glm::vec4{1.f, 0.5f, 0.7f, 1.f});
                }
            }
        }
    }
    // The ragdolls' parts but their pelvis (drawn above, with their slot): a corpse's colour.
    for(const RagdollBodies& r : world->ragdolls)
    {
        for(int b = 1; b < r.count && r.num > 0; b++)
        {
            const b3BodyId body = r.body[static_cast<za::SizeT>(b)];
            if(partCut(r, b) || !b3Body_IsValid(body))
            {
                continue;
            }
            const glm::vec4 colour = b3Body_IsAwake(body) ? glm::vec4{1.f, 0.4f, 0.6f, 1.f} : glm::vec4{0.7f, 0.2f, 0.3f, 0.9f};
            const b3WorldTransform xf = b3Body_GetTransform(body);
            const int count = b3Body_GetShapes(body, shapes.data(), static_cast<int>(shapes.size()));
            for(int i = 0; i < count; i++)
            {
                switch(b3Shape_GetType(shapes[i]))
                {
                case b3_hullShape: drawHull(b3Shape_GetHull(shapes[i]), xf, colour, width); break;
                case b3_capsuleShape: drawCapsule(b3Shape_GetCapsule(shapes[i]), xf, colour, width); break;
                default: break;
                }
            }
        }
    }
    activeRagdollDraw(); // (and the active ragdolls' targets, with vr_ragdoll_active_debug)
    // The hands' and weapons' reach bodies (syncReach): white-blue.
    const glm::vec4 reachColour{0.6f, 0.85f, 1.f, 0.7f};
    for(const auto& both : world->hands)
    {
        for(const World::HandBody& hb : both)
        {
            if(B3_IS_NULL(hb.reach) || !b3Body_IsValid(hb.reach))
            {
                continue;
            }
            const b3WorldTransform xf = b3Body_GetTransform(hb.reach);
            const int count = b3Body_GetShapes(hb.reach, shapes.data(), static_cast<int>(shapes.size()));
            for(int i = 0; i < count; i++)
            {
                switch(b3Shape_GetType(shapes[i]))
                {
                case b3_hullShape: drawHull(b3Shape_GetHull(shapes[i]), xf, reachColour, width); break;
                case b3_capsuleShape: drawCapsule(b3Shape_GetCapsule(shapes[i]), xf, reachColour, width); break;
                default: break;
                }
            }
        }
    }
}
} // namespace qvr::box3d
