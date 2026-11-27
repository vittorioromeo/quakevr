#pragma once
// vr_gizmo.hpp -- the studios' transform handle (a gizmo) on a flat screen: move arrows, planes and a centre, rotate rings,
// scale arms with box ends, drawn with ImGui's draw lists over the game's view and picked with the mouse. Shared by the
// Rig Studio (vr_rigstudio_view.cpp: its bones, aim target and Pose Keys parts) and the Armour Studio
// (vr_armourstudio_gizmo.cpp: the part picked). Pure geometry and drawing: what a drag does is each studio's own.
//
// Handle parts (what hit returns, what draw shows hot): 0..2 the axes (arrows, scale arms, or the rings of a rotate-only
// handle: style.ringBase + axis), 3 the centre (move: the view plane; scale: all three) or a rotate-only handle's view
// ring, 4..6 the planes (yz, zx, xy; move only), 20..22 the rings of a handle with both arrows and rings (outside them).

#include <glm/glm.hpp>

struct ImDrawList;

namespace qvr::gizmo
{

// A perspective view as the screen has it: its eye, its axes (world), the tangents of its half fields of view, and the
// rectangle it fills (pixels, y down).
struct Projection
{
    glm::vec3 org{0.f}, fwd{1.f, 0.f, 0.f}, right{0.f, -1.f, 0.f}, up{0.f, 0.f, 1.f};
    float tanX{1.f}, tanY{1.f};
    float x{0.f}, y{0.f}, w{1.f}, h{1.f};

    // A world point on the screen (false: behind the eye or too near; `out` far off); its depth along fwd.
    bool project(const glm::vec3& world, glm::vec2& out, float* depth = nullptr) const;
    // The ray from the eye through a screen point (world, unit length).
    [[nodiscard]] glm::vec3 rayDir(const glm::vec2& m) const;
    // Where that ray meets the plane through p0 with normal n (false: parallel, or behind the eye).
    bool rayPlane(const glm::vec2& m, const glm::vec3& p0, const glm::vec3& n, glm::vec3& hit) const;
};

// The game's own view (r_refdef: the flat screen's camera, its fov) on the whole display.
[[nodiscard]] Projection refdefProjection(float displayW, float displayH);

struct Handle
{
    bool shown{false};
    glm::vec3 centre{0.f}; // world
    glm::vec3 axis[3]{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}}; // world, unit
    float radius{10.f};    // world units for its size in pixels (place)
    bool move{false};      // its arrows, planes and centre
    bool rot{false};       // its rings (with arrows too: outside them, parts 20..22)
    bool scale{false};     // its arms with box ends and its centre (no planes)
};

// The parts' looks that differ between the studios.
struct Style
{
    float viewRing{0.f}; // pixels: a rotate-only handle's ring round the view (part 3); 0 none
    int ringBase{0};     // a rotate-only handle's rings are parts ringBase + axis
};

inline constexpr float ringOut = 1.6f; // a handle with both: its rings this far out of its arrows
inline constexpr int ringN = 48;       // the points round a ring

[[nodiscard]] float segDist(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b);
// Its radius for `sizePixels` on the screen at its centre's depth; shown unless the centre is behind the eye.
void place(Handle& h, const Projection& v, float sizePixels);
// A ring's points on the screen, `n` of them round axis `a`, and whether each is on the near side.
void ringPoints(const Handle& h, const Projection& v, const glm::vec3& a, glm::vec2* out, bool* front, int n);
// The part under screen point `m` (-1 none).
[[nodiscard]] int hit(const Handle& h, const Projection& v, const glm::vec2& m, const Style& style = {});
// It drawn, part `hot` (-1 none) highlighted.
void draw(const Handle& h, const Projection& v, ImDrawList* dl, int hot, const Style& style = {});
// An axis drag (parts 0..2): the world move along axis `axis` of the screen move start -> m (on the handle as it began).
[[nodiscard]] glm::vec3 axisMove(const Handle& h, const Projection& v, int axis, const glm::vec2& start, const glm::vec2& m);
// The same, exact under perspective: the axis line's points nearest the mouse's rays (the linear one where the axis points
// along the view).
[[nodiscard]] glm::vec3 axisMoveExact(const Handle& h, const Projection& v, int axis, const glm::vec2& start, const glm::vec2& m);
// A ring drag: the angle (radians) swept round the handle's centre on the screen from `last` to `m`, signed as a turn
// about world axis `axisW` (right-handed); 0 within 2 pixels of the centre.
[[nodiscard]] float ringSweep(const Handle& h, const Projection& v, const glm::vec3& axisW, const glm::vec2& last, const glm::vec2& m);
// The same, exact under perspective: the angle between the mouse's rays' points on the ring's plane (the screen's sweep
// where the plane is seen edge on).
[[nodiscard]] float ringSweepExact(const Handle& h, const Projection& v, const glm::vec3& axisW, const glm::vec2& last, const glm::vec2& m);
// The screen point a test's ring drag starts from: the ring's near point farthest from the centre (false: none in front).
bool ringGrip(const Handle& h, const Projection& v, int axis, glm::vec2& out);

} // namespace qvr::gizmo
