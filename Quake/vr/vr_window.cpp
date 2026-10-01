// vr_window.cpp -- the desktop window's view for recording (vr_window_view; vr_window.hpp).
//
// The steadied head: two first-order low-passes in a row on the head's tracking-space orientation (and, for the
// spectator camera, its position), each with half of vr_window_smooth's time constant: the same lag as one of them,
// and a steeper fall above it (a head's shake at 8 Hz is cut to a twentieth at 0.15 s, where one low-pass leaves a
// tenth). Their factors come from the frame's time (1 - exp(-dt / tau)): the same at any frame rate. They run in
// tracking space, and the view is put in the world as the eyes are (through the eye's own world turn), so a snap or
// smooth turn, a teleport or the body's walk carry it exactly with the eyes: only the head's own motion is steadied.
// A jump of the tracking (a recentre) starts them afresh. vr_window_level takes out that much of the roll.
//
// The smoothed mirror turns the left eye's image, which is a pinhole view, to the steadied orientation: a pure turn is
// a homography, so the window's pixels map exactly into the eye's image (vr_stereo.cpp's window shader). The window
// shows a crop of the window's aspect ratio centred on the eye's forward axis, vr_window_zoom times narrower than the
// widest that fits, and the steadied view is kept within the image: when it would show past the edges (or into the
// lenses' hidden area), it is pulled back towards the eye, and the filters with it (a slow turn beyond the margin then
// follows the head at the margin's edge, and settles as soon as the head stops).

#include "vr_engine.hpp"
#include "vr_window.hpp"

#include "vr_cvars.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Tan.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>


namespace qvr::window
{
namespace
{

View currentView = View::Raw;

// OpenXR tracking space (+x right, +y up, -z forward) to Quake axes (+x forward, +y left, +z up).
[[nodiscard]] glm::vec3 quakeFromTracking(const glm::vec3& v)
{
    return {-v.z, -v.x, v.y};
}

// A tracking-space orientation as a turn of Quake axes (columns: forward, left, up), in tracking space.
[[nodiscard]] glm::quat quakeTurn(const glm::quat& q)
{
    const glm::mat3 m{quakeFromTracking(q * glm::vec3{0.f, 0.f, -1.f}), quakeFromTracking(q * glm::vec3{-1.f, 0.f, 0.f}),
        quakeFromTracking(q * glm::vec3{0.f, 1.f, 0.f})};
    return glm::normalize(glm::quat_cast(m));
}

// Quake angles to a basis (columns: forward, left, up) and back.
[[nodiscard]] glm::mat3 basisOf(const glm::vec3& angles)
{
    glm::vec3 f, r, u;
    hands::angleVectors(angles, f, r, u);
    return glm::mat3{f, -r, u};
}

[[nodiscard]] glm::vec3 anglesOf(const glm::mat3& b)
{
    return hands::anglesFromVectors(b[0], b[2]);
}

// vr_window_level of the roll taken out of a turn (tracking space's up is the world's).
[[nodiscard]] glm::quat leveled(const glm::quat& q)
{
    const float level = za::clamp(vr_window_level.value, 0.f, 1.f);
    if(level <= 0.f)
    {
        return q;
    }
    glm::vec3 a = anglesOf(glm::mat3_cast(q));
    a.z *= 1.f - level;
    return glm::normalize(glm::quat_cast(basisOf(a)));
}

// The steadied head.
struct Filter
{
    bool valid = false;
    double time = 0.0;
    glm::quat head{1.f, 0.f, 0.f, 0.f}; // this frame's (Quake axes in tracking space)
    glm::vec3 position{0.f};            // tracking space, metres
    glm::quat q1{1.f, 0.f, 0.f, 0.f}, q2{1.f, 0.f, 0.f, 0.f};
    glm::vec3 p1{0.f}, p2{0.f};
    glm::mat3 worldTurn{1.f}; // tracking space's Quake axes to the world's (the eyes' turn: a yaw)
    glm::vec3 worldHead{0.f};
    glm::vec3 headAngles{0.f}; // the eye's world angles
};
Filter filter;
Camera camera;

[[nodiscard]] float factor(float dt, float tau)
{
    return tau > 0.f ? 1.f - za::exp(-dt / tau) : 1.f;
}

void log(const char* what, const glm::vec3& view, float t)
{
    if(vr_window_log.value == 0.f)
    {
        return;
    }
    const glm::vec3& h = filter.headAngles;
    Con_Printf("window %s: frame %d time %.4f head %.3f %.3f %.3f view %.3f %.3f %.3f kept %.2f\n", what, host_framecount,
        filter.time, h.x, h.y, h.z, view.x, view.y, view.z, t);
}

// Whether the crop `tx` by `ty` (tangents) turned by `m` (the eye's axes to the view's) lies within the eye's image.
[[nodiscard]] glm::mat3 mapFor(const glm::mat3& m, const Fov& fov, float tx, float ty)
{
    const float l = za::tan(fov.left), r = za::tan(fov.right), u = za::tan(fov.up), d = za::tan(fov.down);
    // Window (x, y, 1) to the view's ray in its Quake axes: forward 1, left -x tx, up y ty.
    glm::mat3 v{0.f};
    v[2][0] = 1.f;
    v[0][1] = -tx;
    v[1][2] = ty;
    // The eye's ray to its image (uv times the ray's forward, the ray's forward).
    glm::mat3 p{0.f};
    p[0][0] = -l / (r - l);
    p[1][0] = -1.f / (r - l);
    p[0][1] = -d / (u - d);
    p[2][1] = 1.f / (u - d);
    p[0][2] = 1.f;
    return p * m * v;
}

[[nodiscard]] bool insideTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
{
    const auto side = [](const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2) {
        return (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    };
    const float d1 = side(p, a, b), d2 = side(p, b, c), d3 = side(p, c, a);
    const bool neg = d1 < 0.f || d2 < 0.f || d3 < 0.f;
    const bool pos = d1 > 0.f || d2 > 0.f || d3 > 0.f;
    return !(neg && pos);
}

[[nodiscard]] bool fits(const glm::mat3& map, const Fov& fov, const HiddenArea* hidden)
{
    const float l = za::tan(fov.left), r = za::tan(fov.right), u = za::tan(fov.up), d = za::tan(fov.down);
    constexpr float edge = 0.001f;
    // Its corners and the middles of its sides: a turn maps the crop's sides to straight lines, so within the image's
    // rectangle its corners suffice; the middles too for the hidden area, the lenses' rounded corners.
    constexpr glm::vec2 points[] = {{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}, {0.f, -1.f}, {1.f, 0.f},
        {0.f, 1.f}, {-1.f, 0.f}};
    for(const glm::vec2& n : points)
    {
        const glm::vec3 h = map * glm::vec3{n, 1.f};
        if(h.z <= 1e-4f)
        {
            return false;
        }
        const glm::vec2 uv{h.x / h.z, h.y / h.z};
        if(uv.x < edge || uv.x > 1.f - edge || uv.y < edge || uv.y > 1.f - edge)
        {
            return false;
        }
        if(hidden)
        {
            const glm::vec2 t{l + uv.x * (r - l), d + uv.y * (u - d)};
            for(za::SizeT i = 0; i + 2 < hidden->indices.size(); i += 3)
            {
                if(insideTriangle(t, hidden->vertices[hidden->indices[i]], hidden->vertices[hidden->indices[i + 1]],
                       hidden->vertices[hidden->indices[i + 2]]))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace

View view()
{
    return currentView;
}

float spectatorScale()
{
    return za::clamp(vr_spectator_scale.value, 0.25f, 2.f);
}

void update(const FrameState& frame, const hands::State& s)
{
    if(vr_mirror.value <= 0.f)
    {
        currentView = View::Off;
    }
    else
    {
        const int mode = static_cast<int>(vr_window_view.value);
        currentView = mode == 1 ? View::Smoothed : mode == 2 ? View::Spectator : View::Raw;
    }

    const glm::quat head = quakeTurn(frame.eyes[0].pose.orientation);
    const glm::vec3 position = 0.5f * (frame.eyes[0].pose.position + frame.eyes[1].pose.position);
    filter.headAngles = s.eyeAngles[0];
    filter.worldTurn = basisOf(s.eyeAngles[0]) * glm::transpose(glm::mat3_cast(head));
    filter.worldHead = 0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]);
    if(currentView != View::Smoothed && currentView != View::Spectator)
    {
        filter.valid = false;
        return;
    }

    // Afresh after a pause (a load, another view), or a jump of the tracking no head makes in a frame (a recentre).
    const double dt = realtime - filter.time;
    const bool jumped = filter.valid && (qza::abs(glm::dot(head, filter.head)) < za::cos(glm::radians(15.f)) ||
                                            glm::distance(position, filter.position) > 0.3f);
    if(!filter.valid || dt <= 0.0 || dt > 0.25 || jumped)
    {
        filter.valid = true;
        filter.q1 = filter.q2 = head;
        filter.p1 = filter.p2 = position;
    }
    else
    {
        const float a = factor(static_cast<float>(dt), 0.5f * za::clamp(vr_window_smooth.value, 0.f, 0.5f));
        filter.q1 = glm::slerp(filter.q1, head, a);
        filter.q2 = glm::slerp(filter.q2, filter.q1, a);
        const float b = factor(static_cast<float>(dt), 0.5f * za::clamp(vr_spectator_pos_smooth.value, 0.f, 0.3f));
        filter.p1 += (position - filter.p1) * b;
        filter.p2 += (filter.p1 - filter.p2) * b;
    }
    filter.time = realtime;
    filter.head = head;
    filter.position = position;
}

namespace
{
// mirrorMap's widest crop and what it was made for (the main thread's drawing).
struct WidestCrop
{
    Fov fov;
    float aspect = 0.f;
    const HiddenArea* hidden = nullptr;
    za::SizeT count = 0;
    float tx = 0.f, ty = 0.f;
};
WidestCrop widest;
} // namespace

glm::mat3 mirrorMap(const Fov& fov, float aspect, const HiddenArea* hidden)
{
    // The widest crop of the window's shape about the eye's forward axis: within the image and, with a hidden area,
    // within what the lenses show (made again only when the eye's field of view, the hidden area or the window's shape
    // change); narrowed by the zoom.
    const za::SizeT count = hidden ? hidden->indices.size() : 0;
    if(widest.aspect != aspect || widest.hidden != hidden || widest.count != count || widest.fov.left != fov.left ||
        widest.fov.right != fov.right || widest.fov.up != fov.up || widest.fov.down != fov.down)
    {
        float tx = za::min(za::tan(fov.right), -za::tan(fov.left));
        float ty = za::min(za::tan(fov.up), -za::tan(fov.down));
        if(tx / ty > aspect)
        {
            tx = ty * aspect;
        }
        else
        {
            ty = tx / aspect;
        }
        if(!fits(mapFor(glm::mat3{1.f}, fov, tx, ty), fov, hidden))
        {
            float lo = 0.2f, hi = 1.f;
            for(int i = 0; i < 16; i++)
            {
                const float mid = 0.5f * (lo + hi);
                (fits(mapFor(glm::mat3{1.f}, fov, tx * mid, ty * mid), fov, hidden) ? lo : hi) = mid;
            }
            tx *= lo;
            ty *= lo;
        }
        widest = {fov, aspect, hidden, count, tx, ty};
    }
    const float zoom = za::clamp(vr_window_zoom.value, 1.f, 2.f);
    const float tx = widest.tx / zoom;
    const float ty = widest.ty / zoom;

    const glm::quat target = leveled(filter.q2);
    const glm::mat3 eyeT = glm::transpose(glm::mat3_cast(filter.head));
    const auto mapAt = [&](float t) { return mapFor(eyeT * glm::mat3_cast(glm::slerp(filter.head, target, t)), fov, tx, ty); };

    float kept = 1.f;
    glm::mat3 map = mapAt(1.f);
    if(!fits(map, fov, hidden))
    {
        float lo = 0.f, hi = 1.f;
        for(int i = 0; i < 12; i++)
        {
            const float mid = 0.5f * (lo + hi);
            (fits(mapAt(mid), fov, hidden) ? lo : hi) = mid;
        }
        kept = lo;
        map = mapAt(kept);
        filter.q1 = glm::slerp(filter.head, filter.q1, kept);
        filter.q2 = glm::slerp(filter.head, filter.q2, kept);
    }
    log("mirror", anglesOf(filter.worldTurn * glm::mat3_cast(glm::slerp(filter.head, target, kept))), kept);
    return map;
}

const Camera& spectatorCamera()
{
    return camera;
}

const Camera& spectator(float aspect)
{
    const glm::mat3 turn = filter.worldTurn * glm::mat3_cast(leveled(filter.q2));
    camera.angles = anglesOf(turn);
    camera.origin = filter.worldHead + filter.worldTurn * quakeFromTracking(filter.p2 - filter.position) * units::metresToUnits();
    camera.tanX = za::tan(glm::radians(0.5f * za::clamp(vr_spectator_fov.value, 40.f, 130.f)));
    camera.tanY = camera.tanX / za::max(aspect, 0.1f);
    log("spectator", camera.angles, 1.f);
    return camera;
}

} // namespace qvr::window
