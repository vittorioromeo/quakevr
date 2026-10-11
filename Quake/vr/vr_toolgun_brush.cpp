// vr_toolgun_brush.cpp -- the toolgun's Brush tool (vr_toolgun_brush.hpp): creating a box by dragging its base and
// raising it, moving, resizing by a face's handle, painting, copying and removing boxes (vr_brushes.cpp), on a grid.
// The changes are made when the trigger lets go (or is pressed again): while a hold lasts only its ghost moves (lines
// in the tool's colour, see-through faces, the size written over it), the brush's model is made once.

#include "vr_toolgun_brush.hpp"

#include "vr_backend.hpp"
#include "vr_brushes.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_physsound.hpp"
#include "vr_text3d.hpp"
#include "vr_toolgun.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"

#include <string.h>

using namespace qvr;
using qvr::toolgun::brushtool::Input;
using qvr::toolgun::brushtool::Mode;

namespace
{

constexpr const char* modeNames[] = {"Create", "Move", "Resize", "Paint", "Copy", "Delete", "Rotate"};
constexpr int modeOrder[] = {0, 1, 2, 6, 3, 4, 5}; // (the panel's: Rotate after Resize)
constexpr glm::vec4 ghostColour{1.f, 0.6f, 0.2f, 1.f};    // the tool's (orange)
constexpr glm::vec4 refusedColour{1.f, 0.15f, 0.1f, 1.f}; // a place a box can't be

// The tool's state (the main thread: the host frame).
struct BrushToolState
{
    // Create: 0 nothing, 1 the base dragged (trigger held), 2 the height (the aim raises it; the trigger makes it).
    int phase{0};
    int axis{2};         // the surface's normal's axis,
    float sign{1.f};     // its way along it,
    float plane{0.f};    // and the base's plane (the coordinate on that axis)
    glm::vec3 a{0.f}, b{0.f}; // the base's corners (on the plane)
    float height{0.f};
    // Move and Resize: the brush held, its box as taken, the grab.
    int held{0};
    glm::vec3 lo0{0.f}, hi0{0.f};
    float distance{0.f};
    glm::vec3 grab0{0.f};
    glm::vec3 delta{0.f}; // Move: the way it goes (grid steps)
    int face{-1};         // Resize: the handle held (0..5: axis * 2 + (1: its + face)), or aimed at
    float faceTo{0.f};    // ... where the face goes
    int hoverId{0};       // the brush under the aim (Resize's handles)
    glm::vec3 hand0{0.f}; // Rotate: the hand's angles when it took the brush,
    glm::vec3 turn{0.f};  // ... the turn so far (snapped angles)
    // Copy: the brush copied (its text's plan), and its box.
    bool haveCopy{false};
    brushes::Brush copy;
    glm::vec3 copyLo{0.f}, copyHi{0.f};
    bool picking{false};
    // The ghost drawn this frame (for vr_toolgun_brush_status), and whether it can be made there.
    bool ghost{false};
    glm::vec3 ghostLo{0.f}, ghostHi{0.f};
    bool ghostOk{true};
    int glowNum{0};
    float glow{0.f};
};
BrushToolState bt;

// Texts handed out by pointer: valid until the same function's next call.
struct BrushToolReadouts
{
    char screen[48];
    char state[160];
};
BrushToolReadouts readouts;

[[nodiscard]] Mode currentMode()
{
    return static_cast<Mode>(za::clamp(static_cast<int>(vr_toolgun_brush_mode.value), 0, static_cast<int>(Mode::Count) - 1));
}

[[nodiscard]] float grid()
{
    return za::clamp(vr_toolgun_brush_grid.value, 1.f, 256.f);
}

[[nodiscard]] float snap(float v)
{
    const float g = grid();
    return za::round(v / g) * g;
}

void tick(int hand, float amplitude)
{
    Backend* be = backend();
    if(be && hand >= 0 && !vr_disablehaptics.value)
    {
        be->haptic(hand, 0.015f, 200.f, amplitude);
    }
}

[[nodiscard]] int axisOf(const glm::vec3& n)
{
    int axis = 0;
    for(int i = 1; i < 3; i++)
    {
        if(za::fabs(n[i]) > za::fabs(n[axis]))
        {
            axis = i;
        }
    }
    return axis;
}

// The brush whose entity `num` is (0: none).
[[nodiscard]] int brushOf(int num)
{
    return num > 0 ? brushes::ofEntity(num) : 0;
}

[[nodiscard]] bool boundsOf(int id, glm::vec3& lo, glm::vec3& hi)
{
    const brushes::Brush* b = brushes::find(id);
    glm::dvec3 l, h;
    if(!b || !brushes::bounds(*b, l, h))
    {
        return false;
    }
    lo = glm::vec3{l};
    hi = glm::vec3{h};
    return true;
}

// The player or a monster (anything with a box that moves by itself) that a box lo..hi would hold inside; 0: none.
[[nodiscard]] int holds(const glm::vec3& lo, const glm::vec3& hi)
{
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int solid = static_cast<int>(e->v.solid);
        if(e->free || (solid != SOLID_SLIDEBOX && solid != SOLID_BBOX) ||
            !(static_cast<int>(e->v.flags) & (FL_CLIENT | FL_MONSTER)) || e->v.health <= 0.f)
        {
            continue;
        }
        const glm::vec3 elo{e->v.absmin[0] + 1.f, e->v.absmin[1] + 1.f, e->v.absmin[2] + 1.f};
        const glm::vec3 ehi{e->v.absmax[0] - 1.f, e->v.absmax[1] - 1.f, e->v.absmax[2] - 1.f};
        if(glm::all(glm::lessThan(lo, ehi)) && glm::all(glm::greaterThan(hi, elo)))
        {
            return i;
        }
    }
    return 0;
}

[[nodiscard]] bool refuse(const glm::vec3& lo, const glm::vec3& hi)
{
    if(const int who = holds(lo, hi))
    {
        Con_Printf("toolgun: no box there: it would hold %s inside\n", PR_GetString(EDICT_NUM(who)->v.classname));
        return true;
    }
    return false;
}

// The ghost: its edges, see-through faces, and its size over it, facing the head.
void drawBox(const glm::vec3& lo, const glm::vec3& hi, const glm::vec4& c, const glm::vec3& head, bool faces)
{
    const auto corner = [&](int k) { return glm::vec3{(k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z}; };
    for(int a = 0; a < 8; a++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(a & bit))
            {
                lines::line(corner(a), corner(a | bit), 0.4f, c, c);
            }
        }
    }
    if(faces)
    {
        const glm::vec4 fill{c.r, c.g, c.b, 0.22f};
        // (each face as two triangles: its corners by the bit that stays)
        for(int axis = 0; axis < 3; axis++)
        {
            const int u = 1 << ((axis + 1) % 3), v = 1 << ((axis + 2) % 3), w = 1 << axis;
            for(const int side : {0, w})
            {
                lines::triangle(corner(side), corner(side | u), corner(side | u | v), fill);
                lines::triangle(corner(side), corner(side | u | v), corner(side | v), fill);
            }
        }
    }
    const glm::vec3 size = hi - lo;
    const glm::vec3 top{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, hi.z + 6.f};
    const glm::vec3 toHead = head - top;
    const float yaw = glm::degrees(za::atan2(toHead.y, toHead.x));
    text3d::queue(va("%.0f x %.0f x %.0f", size.x, size.y, size.z), top, glm::vec3{0.f, yaw, 0.f}, text3d::Align::Centre, 0.06f);
}

void showGhost(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& head)
{
    bt.ghost = true;
    bt.ghostLo = lo;
    bt.ghostHi = hi;
    bt.ghostOk = holds(lo, hi) == 0;
    drawBox(lo, hi, bt.ghostOk ? ghostColour : refusedColour, head, true);
}

// A brush's ghost (any shape): its faces' edges and see-through faces, its bounds' size over it.
void showGhost(const brushes::Brush& b, const glm::vec3& head)
{
    glm::dvec3 l, h;
    if(!brushes::bounds(b, l, h))
    {
        return;
    }
    const glm::vec3 lo{l}, hi{h};
    if(brushes::axial(b) && b.faces.size() == 6)
    {
        showGhost(lo, hi, head);
        return;
    }
    bt.ghost = true;
    bt.ghostLo = lo;
    bt.ghostHi = hi;
    bt.ghostOk = holds(lo, hi) == 0;
    const glm::vec4 c = bt.ghostOk ? ghostColour : refusedColour;
    const glm::vec4 fill{c.r, c.g, c.b, 0.22f};
    brushes::forEachPolygon(b, [&](int, const glm::dvec3* p, int n) {
        for(int i = 0; i < n; i++)
        {
            lines::line(glm::vec3{p[i]}, glm::vec3{p[(i + 1) % n]}, 0.4f, c, c);
        }
        for(int i = 2; i < n; i++)
        {
            lines::triangle(glm::vec3{p[0]}, glm::vec3{p[i - 1]}, glm::vec3{p[i]}, fill);
        }
    });
    const glm::vec3 size = hi - lo;
    const glm::vec3 top{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, hi.z + 6.f};
    const glm::vec3 toHead = head - top;
    text3d::queue(va("%.0f x %.0f x %.0f", size.x, size.y, size.z), top,
        glm::vec3{0.f, glm::degrees(za::atan2(toHead.y, toHead.x)), 0.f}, text3d::Align::Centre, 0.06f);
}

[[nodiscard]] brushes::Shape currentShape()
{
    return static_cast<brushes::Shape>(za::clamp(static_cast<int>(vr_toolgun_brush_shape.value), 0, static_cast<int>(brushes::Shape::Count) - 1));
}

// The shape chosen, filling lo..hi, made on the surface whose normal is `axis` the way `sign`: a wedge rising away from
// the gun (its aim's way across the normal).
[[nodiscard]] brushes::Brush createShape(const glm::vec3& lo, const glm::vec3& hi, int axis, float sign, const glm::vec3& dir)
{
    int rise = (axis + 1) % 3;
    for(int k = 0; k < 3; k++)
    {
        if(k != axis && za::fabs(dir[k]) > za::fabs(dir[rise]))
        {
            rise = k;
        }
    }
    return brushes::shape(currentShape(), glm::dvec3{lo}, glm::dvec3{hi}, axis, sign, rise, dir[rise] >= 0.f ? 1.0 : -1.0,
        static_cast<int>(vr_toolgun_brush_sides.value), brushes::currentTexture());
}

// ---------------------------------------------------------------------------------------------------------------------
// Create.

// The aim's point on the plane `axis` = `plane` (false: it doesn't meet it ahead).
[[nodiscard]] bool onPlane(const Input& in, int axis, float plane, glm::vec3& p)
{
    if(za::fabs(in.dir[axis]) < 1e-4f)
    {
        return false;
    }
    const float t = (plane - in.from[axis]) / in.dir[axis];
    if(t <= 0.f || t > 4096.f)
    {
        return false;
    }
    p = in.from + in.dir * t;
    return true;
}

// The base's corner on the grid (its plane's coordinate kept).
[[nodiscard]] glm::vec3 onGrid(glm::vec3 p, int axis, float plane)
{
    for(int k = 0; k < 3; k++)
    {
        p[k] = k == axis ? plane : snap(p[k]);
    }
    return p;
}

void createBox(glm::vec3& lo, glm::vec3& hi)
{
    lo = glm::min(bt.a, bt.b);
    hi = glm::max(bt.a, bt.b);
    lo[bt.axis] = bt.sign > 0.f ? bt.plane : bt.plane - bt.height;
    hi[bt.axis] = bt.sign > 0.f ? bt.plane + bt.height : bt.plane;
}

void createFrame(const Input& in, int hand)
{
    if(bt.phase == 0)
    {
        if(in.normal == glm::vec3{0.f})
        {
            return;
        }
        // Where it would start: a point on the grid on the surface aimed at.
        const glm::vec3 hit = in.from + in.dir * in.worldDistance;
        const int axis = axisOf(in.normal);
        const float plane = za::round((hit[axis] - in.normal[axis] * 0.03125f) * 8.f) / 8.f; // (Quake's 1/32 off it)
        const glm::vec3 p = onGrid(hit, axis, plane);
        lines::glowPoint(p, 1.2f, ghostColour);
        if(in.fire)
        {
            bt.phase = 1;
            bt.axis = axis;
            bt.sign = in.normal[axis] >= 0.f ? 1.f : -1.f;
            bt.plane = plane;
            bt.a = bt.b = p;
            bt.height = grid();
            tick(hand, 0.3f);
        }
        return;
    }
    if(bt.phase == 1)
    {
        glm::vec3 p;
        if(onPlane(in, bt.axis, bt.plane, p))
        {
            const glm::vec3 b = onGrid(p, bt.axis, bt.plane);
            const glm::vec3 clamped = glm::clamp(b, bt.a - glm::vec3{1024.f}, bt.a + glm::vec3{1024.f});
            if(clamped != bt.b)
            {
                tick(hand, 0.1f);
            }
            bt.b = clamped;
        }
        if(!in.trigger)
        {
            // A base with no width on an axis: one grid step that way.
            for(int k = 0; k < 3; k++)
            {
                if(k != bt.axis && bt.a[k] == bt.b[k])
                {
                    bt.b[k] = bt.a[k] + grid();
                }
            }
            bt.phase = 2;
        }
        glm::vec3 lo = glm::min(bt.a, bt.b), hi = glm::max(bt.a, bt.b);
        showGhost(lo, hi, in.head);
        return;
    }
    // The height: where the aim passes nearest the base's middle's line along the normal.
    const glm::vec3 c = (bt.a + bt.b) * 0.5f;
    glm::vec3 n{0.f};
    n[bt.axis] = bt.sign;
    const glm::vec3 w0 = in.from - c;
    const float b = glm::dot(in.dir, n), d = glm::dot(in.dir, w0), e = glm::dot(n, w0);
    const float denom = 1.f - b * b;
    if(denom > 1e-4f)
    {
        const float s = (e - b * d) / denom;
        const float h = za::clamp(snap(s), grid(), 2048.f);
        if(h != bt.height)
        {
            tick(hand, 0.1f);
        }
        bt.height = h;
    }
    glm::vec3 lo, hi;
    createBox(lo, hi);
    const brushes::Brush made = createShape(lo, hi, bt.axis, bt.sign, in.dir);
    showGhost(made, in.head);
    if(in.fire)
    {
        if(!refuse(lo, hi))
        {
            const int id = brushes::create(made);
            if(id)
            {
                Con_Printf("toolgun: %s %d made, %.0f %.0f %.0f .. %.0f %.0f %.0f\n",
                    currentShape() == brushes::Shape::Box ? "box" : brushes::shapeName(currentShape()), id, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
                tick(hand, 0.6f);
            }
        }
        bt.phase = 0;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Move, Resize.

void moveFrame(const Input& in, int hand)
{
    if(!bt.held)
    {
        const int id = brushOf(in.target);
        bt.glowNum = id ? in.target : 0;
        bt.glow = 0.6f;
        if(in.fire && id && boundsOf(id, bt.lo0, bt.hi0))
        {
            bt.held = id;
            bt.distance = in.targetDistance;
            bt.grab0 = in.from + in.dir * bt.distance;
            bt.delta = glm::vec3{0.f};
            tick(hand, 0.3f);
        }
        return;
    }
    if(!brushes::find(bt.held))
    {
        bt.held = 0;
        return;
    }
    bt.glowNum = brushes::entityOf(bt.held);
    bt.glow = 0.6f;
    const glm::vec3 grab = in.from + in.dir * bt.distance;
    glm::vec3 delta;
    for(int k = 0; k < 3; k++)
    {
        delta[k] = snap(grab[k] - bt.grab0[k]) + 0.f; // (no -0)
    }
    if(delta != bt.delta)
    {
        tick(hand, 0.1f);
        bt.delta = delta;
    }
    brushes::Brush moved = *brushes::find(bt.held);
    brushes::translate(moved, glm::dvec3{delta});
    showGhost(moved, in.head);
    if(!in.trigger)
    {
        if(delta != glm::vec3{0.f} && !refuse(bt.lo0 + delta, bt.hi0 + delta))
        {
            brushes::Brush b = *brushes::find(bt.held);
            brushes::translate(b, glm::dvec3{delta});
            if(brushes::change(bt.held, b))
            {
                Con_Printf("toolgun: box %d moved %.0f %.0f %.0f\n", bt.held, delta.x, delta.y, delta.z);
                tick(hand, 0.5f);
            }
        }
        bt.held = 0;
    }
}

[[nodiscard]] glm::vec3 handlePoint(const glm::vec3& lo, const glm::vec3& hi, int h)
{
    glm::vec3 p = (lo + hi) * 0.5f;
    p[h / 2] = (h & 1) ? hi[h / 2] : lo[h / 2];
    return p;
}

// The handle nearest the aim's line, within reach of it; -1 none.
[[nodiscard]] int aimedHandle(const Input& in, const glm::vec3& lo, const glm::vec3& hi)
{
    int best = -1;
    float bestOff = 1e30f;
    for(int h = 0; h < 6; h++)
    {
        const glm::vec3 p = handlePoint(lo, hi, h);
        const float along = glm::dot(p - in.from, in.dir);
        if(along <= 0.f)
        {
            continue;
        }
        const float off = glm::length(p - (in.from + in.dir * along));
        if(off < 3.f + along * 0.04f && off < bestOff)
        {
            bestOff = off;
            best = h;
        }
    }
    return best;
}

void resizeFrame(const Input& in, int hand)
{
    glm::vec3 lo, hi;
    if(!bt.held)
    {
        const int id = brushOf(in.target);
        bt.hoverId = id;
        if(!id || !boundsOf(id, lo, hi))
        {
            bt.face = -1;
            return;
        }
        bt.glowNum = in.target;
        bt.glow = 0.5f;
        bt.face = aimedHandle(in, lo, hi);
        drawBox(lo, hi, ghostColour, in.head, false);
        for(int h = 0; h < 6; h++)
        {
            const glm::vec3 p = handlePoint(lo, hi, h);
            if(h == bt.face)
            {
                lines::point(p, 3.f, glm::vec4{1.f, 0.4f, 0.1f, 1.f});
                lines::glowPoint(p, 5.f, glm::vec4{1.f, 0.5f, 0.1f, 0.8f});
            }
            else
            {
                lines::point(p, 1.8f, glm::vec4{0.3f, 0.9f, 1.f, 1.f});
            }
        }
        if(in.fire && bt.face >= 0)
        {
            bt.held = id;
            bt.lo0 = lo;
            bt.hi0 = hi;
            const glm::vec3 p = handlePoint(lo, hi, bt.face);
            bt.distance = glm::length(p - in.from);
            bt.grab0 = in.from + in.dir * bt.distance;
            bt.faceTo = p[bt.face / 2];
            tick(hand, 0.3f);
        }
        return;
    }
    if(!brushes::find(bt.held))
    {
        bt.held = 0;
        return;
    }
    bt.glowNum = brushes::entityOf(bt.held);
    bt.glow = 0.5f;
    const int axis = bt.face / 2;
    const bool positive = (bt.face & 1) != 0;
    const glm::vec3 grab = in.from + in.dir * bt.distance;
    float to = snap(handlePoint(bt.lo0, bt.hi0, bt.face)[axis] + (grab[axis] - bt.grab0[axis]));
    to = positive ? za::max(to, bt.lo0[axis] + grid()) : za::min(to, bt.hi0[axis] - grid());
    if(to != bt.faceTo)
    {
        tick(hand, 0.1f);
        bt.faceTo = to;
    }
    lo = bt.lo0;
    hi = bt.hi0;
    (positive ? hi : lo)[axis] = to;
    // Stretched by its bounds (a box: the face whose normal is that axis's way moves, the others stay).
    brushes::Brush b = *brushes::find(bt.held);
    brushes::scaleTo(b, glm::dvec3{bt.lo0}, glm::dvec3{bt.hi0}, glm::dvec3{lo}, glm::dvec3{hi});
    showGhost(b, in.head);
    lines::point(handlePoint(lo, hi, bt.face), 3.f, glm::vec4{1.f, 0.4f, 0.1f, 1.f});
    if(!in.trigger)
    {
        const float was = positive ? bt.hi0[axis] : bt.lo0[axis];
        if(to != was && !refuse(lo, hi))
        {
            if(brushes::change(bt.held, b))
            {
                Con_Printf("toolgun: box %d resized to %.0f %.0f %.0f .. %.0f %.0f %.0f\n", bt.held, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
                tick(hand, 0.5f);
            }
        }
        bt.held = 0;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Paint, Pick, Copy, Delete.

// The texture of what the aim's trace hit: a brush's face, or the world's surface there.
[[nodiscard]] const char* textureHit(const Input& in)
{
    if(in.normal == glm::vec3{0.f})
    {
        return nullptr;
    }
    if(const int id = brushOf(in.hitEntity))
    {
        const brushes::Brush* b = brushes::find(id);
        const brushes::Face* best = nullptr;
        for(const brushes::Face& f : b->faces)
        {
            if(!best || glm::dot(glm::vec3{f.normal}, in.normal) > glm::dot(glm::vec3{best->normal}, in.normal))
            {
                best = &f;
            }
        }
        return best ? best->texture : nullptr;
    }
    if(in.hitEntity != 0 || !sv.worldmodel)
    {
        return nullptr;
    }
    const glm::vec3 hit = in.from + in.dir * in.worldDistance;
    const msurface_t* s = physsound::surfaceOnSegment(sv.worldmodel, hit - in.dir * 2.f, hit + in.dir * 2.f);
    if(!s || !s->texinfo || s->texinfo->texnum < 0 || s->texinfo->texnum >= sv.worldmodel->numtextures)
    {
        return nullptr;
    }
    const texture_t* t = sv.worldmodel->textures[s->texinfo->texnum];
    return t ? t->name : nullptr;
}

void paintFrame(const Input& in, int hand)
{
    const int id = brushOf(in.hitEntity);
    bt.glowNum = id ? in.hitEntity : 0;
    bt.glow = 0.3f;
    if(!in.fire || !id)
    {
        return;
    }
    brushes::Brush b = *brushes::find(id);
    const char* tex = brushes::currentTexture();
    const bool all = vr_toolgun_brush_paint_all.value != 0.f;
    int best = 0;
    for(za::SizeT i = 1; i < b.faces.size(); i++)
    {
        if(glm::dot(glm::vec3{b.faces[i].normal}, in.normal) > glm::dot(glm::vec3{b.faces[static_cast<za::SizeT>(best)].normal}, in.normal))
        {
            best = static_cast<int>(i);
        }
    }
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        if(all || static_cast<int>(i) == best)
        {
            q_strlcpy(b.faces[i].texture, tex, sizeof(b.faces[i].texture));
        }
    }
    if(brushes::change(id, b))
    {
        Con_Printf("toolgun: box %d painted %s%s\n", id, tex, all ? "" : " (one face)");
        tick(hand, 0.4f);
    }
}

void copyFrame(const Input& in, int hand)
{
    if(!bt.haveCopy)
    {
        const int id = brushOf(in.target);
        bt.glowNum = id ? in.target : 0;
        bt.glow = 0.7f;
        if(in.fire && id && boundsOf(id, bt.copyLo, bt.copyHi))
        {
            bt.copy = *brushes::find(id);
            bt.haveCopy = true;
            Con_Printf("toolgun: box %d copied\n", id);
            tick(hand, 0.3f);
        }
        return;
    }
    if(in.normal == glm::vec3{0.f})
    {
        return;
    }
    // Sitting on the surface aimed at, its middle over the point (on the grid).
    const glm::vec3 hit = in.from + in.dir * in.worldDistance;
    const int axis = axisOf(in.normal);
    const float plane = za::round((hit[axis] - in.normal[axis] * 0.03125f) * 8.f) / 8.f;
    const glm::vec3 size = bt.copyHi - bt.copyLo;
    glm::vec3 lo;
    for(int k = 0; k < 3; k++)
    {
        lo[k] = k == axis ? (in.normal[axis] >= 0.f ? plane : plane - size[k]) : snap(hit[k] - size[k] * 0.5f);
    }
    brushes::Brush b = bt.copy;
    brushes::translate(b, glm::dvec3{lo - bt.copyLo});
    showGhost(b, in.head);
    if(in.fire && !refuse(lo, lo + size))
    {
        const int id = brushes::create(b);
        if(id)
        {
            Con_Printf("toolgun: box %d made as a copy\n", id);
            tick(hand, 0.6f);
        }
    }
}

// Rotate: the brush held turned as the hand has turned since it took it (each angle in snap steps), about its bounds'
// middle (on whole units).
[[nodiscard]] float rotateStep()
{
    return vr_toolgun_snap.value > 0.f ? vr_toolgun_snap.value : 15.f;
}

void rotateFrame(const Input& in, int hand)
{
    if(!bt.held)
    {
        const int id = brushOf(in.target);
        bt.glowNum = id ? in.target : 0;
        bt.glow = 0.15f;
        if(in.fire && id && boundsOf(id, bt.lo0, bt.hi0))
        {
            bt.held = id;
            bt.hand0 = in.handRot;
            bt.turn = glm::vec3{0.f};
            tick(hand, 0.3f);
        }
        return;
    }
    if(!brushes::find(bt.held))
    {
        bt.held = 0;
        return;
    }
    bt.glowNum = brushes::entityOf(bt.held);
    bt.glow = 0.15f;
    const glm::mat3 turn = held::axesFromAngles(&in.handRot[0], true) * glm::transpose(held::axesFromAngles(&bt.hand0[0], true));
    float angles[3];
    held::anglesFromAxes(turn, angles, true);
    const float step = rotateStep();
    glm::vec3 snapped;
    for(int k = 0; k < 3; k++)
    {
        snapped[k] = za::round(angles[k] / step) * step + 0.f;
    }
    if(snapped != bt.turn)
    {
        tick(hand, 0.1f);
        bt.turn = snapped;
    }
    brushes::Brush b = *brushes::find(bt.held);
    const glm::dvec3 pivot = glm::floor(glm::dvec3{bt.lo0 + bt.hi0} * 0.5 + 0.5);
    brushes::rotate(b, brushes::turnFromAngles(glm::dvec3{snapped}), pivot, vr_toolgun_brush_texlock.value != 0.f);
    showGhost(b, in.head);
    if(!in.trigger)
    {
        if(snapped != glm::vec3{0.f} && !refuse(bt.ghostLo, bt.ghostHi) && brushes::change(bt.held, b))
        {
            Con_Printf("toolgun: brush %d turned %.0f %.0f %.0f\n", bt.held, snapped.x, snapped.y, snapped.z);
            tick(hand, 0.5f);
        }
        bt.held = 0;
    }
}

void deleteFrame(const Input& in, int hand)
{
    const int id = brushOf(in.target);
    bt.glowNum = id ? in.target : 0;
    bt.glow = 2.9f; // (red)
    if(in.fire && id && brushes::remove(id))
    {
        Con_Printf("toolgun: box %d removed\n", id);
        tick(hand, 0.5f);
        bt.glowNum = 0;
    }
}

void pickFrame(const Input& in, int hand)
{
    if(!in.fire)
    {
        return;
    }
    const char* tex = textureHit(in);
    if(tex)
    {
        brushes::setCurrentTexture(tex);
    }
    Con_Printf("toolgun: texture %s%s\n", brushes::currentTexture(), tex && !strcmp(brushes::currentTexture(), tex) ? " (picked)" : " (that one can't be picked)");
    bt.picking = false;
    tick(hand, 0.4f);
}

// vr_toolgun_brush_status (tests): the mode, what is held, the ghost.
void status_f()
{
    Con_Printf("toolgun brush: mode %s, grid %.0f, texture %s, create phase %d, held %d, face %d, copy %s, picking %d\n",
        modeNames[static_cast<int>(currentMode())], grid(), brushes::currentTexture(), bt.phase, bt.held, bt.face,
        bt.haveCopy ? "yes" : "no", bt.picking ? 1 : 0);
    if(bt.ghost)
    {
        Con_Printf("toolgun brush: ghost %.0f %.0f %.0f .. %.0f %.0f %.0f%s\n", bt.ghostLo.x, bt.ghostLo.y, bt.ghostLo.z, bt.ghostHi.x,
            bt.ghostHi.y, bt.ghostHi.z, bt.ghostOk ? "" : " (refused: someone inside)");
    }
}

// vr_toolgun_brush_mode_set <n | name>: the mode (the panel's tiles), its holds let go.
void modeSet_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("toolgun: brush mode %s\n", modeNames[static_cast<int>(currentMode())]);
        return;
    }
    int m = Q_atoi(Cmd_Argv(1));
    for(int i = 0; i < static_cast<int>(Mode::Count); i++)
    {
        if(!q_strcasecmp(Cmd_Argv(1), modeNames[i]))
        {
            m = i;
        }
    }
    Cvar_SetValueQuick(&vr_toolgun_brush_mode, static_cast<float>(za::clamp(m, 0, static_cast<int>(Mode::Count) - 1)));
    toolgun::brushtool::reset();
    Con_Printf("toolgun: brush mode %s\n", modeNames[static_cast<int>(currentMode())]);
}

} // namespace

namespace qvr::toolgun::brushtool
{

void frame(const Input& in)
{
    bt.ghost = false;
    bt.glowNum = 0;
    if(!in.valid)
    {
        return;
    }
    const int hand = toolgun::heldHand();
    if(bt.picking)
    {
        pickFrame(in, hand);
        return;
    }
    switch(currentMode())
    {
    case Mode::Create: createFrame(in, hand); break;
    case Mode::Move: moveFrame(in, hand); break;
    case Mode::Resize: resizeFrame(in, hand); break;
    case Mode::Paint: paintFrame(in, hand); break;
    case Mode::Copy: copyFrame(in, hand); break;
    case Mode::Delete: deleteFrame(in, hand); break;
    case Mode::Rotate: rotateFrame(in, hand); break;
    default: break;
    }
}

int modeAt(int slot)
{
    return slot >= 0 && slot < static_cast<int>(Mode::Count) ? modeOrder[slot] : 0;
}

void push(float units)
{
    if(bt.held && currentMode() == Mode::Move)
    {
        bt.distance = za::max(bt.distance + units, 16.f);
    }
}

void reset()
{
    bt.phase = 0;
    bt.held = 0;
    bt.face = -1;
    bt.haveCopy = false;
    bt.copy = brushes::Brush{};
    bt.picking = false;
    bt.ghost = false;
    bt.glowNum = 0;
}

int glowEntity(float& glow)
{
    glow = bt.glow;
    return bt.glowNum;
}

int modeCount()
{
    return static_cast<int>(Mode::Count);
}

const char* modeName(int mode)
{
    return mode >= 0 && mode < static_cast<int>(Mode::Count) ? modeNames[mode] : "";
}

void pickTexture()
{
    bt.picking = !bt.picking;
    Con_Printf("toolgun: %s\n", bt.picking ? "the next shot picks the texture it hits" : "picking off");
}

bool picking()
{
    return bt.picking;
}

const char* screenText()
{
    q_snprintf(readouts.screen, sizeof(readouts.screen), "%s", bt.picking ? "PICK TEXTURE" : va("BRUSH %s", modeNames[static_cast<int>(currentMode())]));
    for(char* c = readouts.screen; *c; c++)
    {
        *c = (*c >= 'a' && *c <= 'z') ? static_cast<char>(*c - 32) : *c;
    }
    return readouts.screen;
}

const char* stateLine()
{
    const Mode m = currentMode();
    const char* what = "";
    switch(m)
    {
    case Mode::Create:
        what = bt.phase == 1 ? "drag the base" : bt.phase == 2 ? "raise it, trigger to make it" : "trigger on a surface: a box's base";
        break;
    case Mode::Move: what = bt.held ? "let go to put it there" : "hold the trigger on a box"; break;
    case Mode::Resize: what = bt.held ? "let go to size it" : "hold the trigger on a face's handle"; break;
    case Mode::Paint: what = "trigger on a box"; break;
    case Mode::Copy: what = bt.haveCopy ? "trigger to put a copy" : "trigger on a box to copy it"; break;
    case Mode::Delete: what = "trigger on a box removes it"; break;
    case Mode::Rotate:
        what = bt.held ? va("turn your hand: %.0f %.0f %.0f, let go to set it", bt.turn.x, bt.turn.y, bt.turn.z)
                       : "hold the trigger on a brush and turn your hand";
        break;
    default: break;
    }
    q_snprintf(readouts.state, sizeof(readouts.state), "Brush %s: %s (grid %.0f, %s)", modeNames[static_cast<int>(m)],
        bt.picking ? "shoot a surface to pick its texture" : what, grid(), brushes::currentTexture());
    return readouts.state;
}

void registerCommands()
{
    Cmd_AddCommand("vr_toolgun_brush_status", status_f);
    Cmd_AddCommand("vr_toolgun_brush_mode_set", modeSet_f);
}

} // namespace qvr::toolgun::brushtool
