// vr_rigstudio_view.cpp -- the Rig Studio's viewport (vr_rigstudio.hpp): the camera (Blender's mouse), the model's
// skinned mesh as the studio draws it (shaded, wire, x-ray; vertex colours by bone or a bone's weights as heat), the
// bones (octahedral or sticks), the handles (rotate rings, move arrows and planes, IK ends, the rest pose's points), the
// picks (bones, vertices, a box), the weight brush. Everything is drawn with ImGui's background list, over the game's
// own view of the scene.

#include "vr_rigstudio_internal.hpp"
#include "vr_agarms.hpp"
#include "vr_agbody.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_rigdata.hpp"
#include "vr_rigpose.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Tan.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace
{

constexpr float pi = 3.14159265f;
constexpr float deg = pi / 180.f;

// Handle parts (dragAxis): 0..2 the axes (rings or arrows), 3 the view ring / the view plane, 4..6 the planes (yz, zx,
// xy), 10 the rest pose's pivot, 11 its end.
struct Handle
{
    bool shown{false};
    glm::vec3 centre{0.f};  // world
    glm::vec3 axis[3]{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}}; // world
    float radius{10.f};     // world units for handle_size pixels
    int owner{0};           // 0 the active bone, 1 the aim target, 2 the Pose Keys' part
    bool move{false};       // its arrows, planes and centre
    bool rot{false};        // its rings (with arrows too: outside them, parts 20..22)
};

constexpr float ringOut = 1.6f; // a handle with both: its rings this far out of its arrows

// A step of the mock mouse (the tests): where it is, whether its left button is down.
struct MockStep
{
    glm::vec2 at{0.f};
    bool down{false};
};

// The viewport's state between frames (the main thread's).
struct ViewState
{
    Handle h;
    int hoverPart{-1};
    float snapAccum{0.f}, snapApplied{0.f};
    glm::vec3 moveAccum{0.f}, moveApplied{0.f};
    glm::vec3 planeStart{0.f};
    bool windingFlip{false};
    int paintFrame{-1};
    int restPoint{10}; // the rest tool's point: 10 pivot, 11 end
    Handle dragH; // the handle as the drag began (its moves measured on it)
    // A drag of the aim target or of a Pose Keys part: its place (frame) as it began
    glm::vec3 aimStart{0.f};
    rigpose::Rigid deskStart;
    // The mock mouse's queued steps (the tests), the next one
    za::Vector<MockStep> mock;
    int mockNext{0};
};
ViewState vs;

[[nodiscard]] ImU32 col(const glm::vec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4{c.x, c.y, c.z, c.w});
}

[[nodiscard]] ImVec2 iv(const glm::vec2& v)
{
    return ImVec2{v.x, v.y};
}

[[nodiscard]] const entity_t* subjectEntity()
{
    if(st.subject <= 0 || !cl_entities || st.subject >= cl_max_edicts)
    {
        return nullptr;
    }
    return &cl_entities[st.subject];
}

[[nodiscard]] bool visibleBone(int b)
{
    if(st.solo >= 0)
    {
        return b == st.solo;
    }
    return !(st.hidden & (1u << b));
}

// The ray from the camera through a window point (world).
[[nodiscard]] glm::vec3 rayDir(const glm::vec2& m)
{
    const View& v = st.view;
    const float x = ((m.x - v.x) / v.w) * 2.f - 1.f;
    const float y = 1.f - ((m.y - v.y) / v.h) * 2.f;
    return glm::normalize(v.fwd + v.right * (x * v.tanX) + v.up * (y * v.tanY));
}

[[nodiscard]] bool rayPlane(const glm::vec2& m, const glm::vec3& p0, const glm::vec3& n, glm::vec3& hit)
{
    const glm::vec3 d = rayDir(m);
    const float den = glm::dot(d, n);
    if(za::abs(den) < 1e-4f)
    {
        return false;
    }
    const float t = glm::dot(p0 - st.view.org, n) / den;
    if(t <= 0.f)
    {
        return false;
    }
    hit = st.view.org + d * t;
    return true;
}

[[nodiscard]] float segDist(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
{
    const glm::vec2 ab = b - a;
    const float l2 = glm::dot(ab, ab);
    const float t = l2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return glm::length(p - (a + ab * t));
}

[[nodiscard]] glm::vec4 heat(float w)
{
    // blue 0, green 0.5, red 1 (Blender's weight paint)
    w = za::clamp(w, 0.f, 1.f);
    if(w < 0.5f)
    {
        const float t = w * 2.f;
        return glm::vec4{0.f, t, 1.f - t, 1.f};
    }
    const float t = (w - 0.5f) * 2.f;
    return glm::vec4{t, 1.f - t, 0.f, 1.f};
}

[[nodiscard]] float weightOn(int v, int bone)
{
    const rigdata::VertWeights& w = st.weights[static_cast<za::SizeT>(v)];
    for(int i = 0; i < w.n; i++)
    {
        if(w.bone[i] == bone)
        {
            return w.w[i];
        }
    }
    return 0.f;
}

[[nodiscard]] glm::vec4 vertColor(int v)
{
    glm::vec4 romC;
    if(romColor(v, romC))
    {
        return romC; // (the authored rig's blended weights: its joint picked as heat, or its bones' colours)
    }
    if(st.colors == Colors::Heat)
    {
        return heat(st.active >= 0 ? weightOn(v, st.active) : 0.f);
    }
    const rigdata::VertWeights& w = st.weights[static_cast<za::SizeT>(v)];
    glm::vec4 c{0.f};
    for(int i = 0; i < w.n; i++)
    {
        c += boneColor(w.bone[i]) * w.w[i];
    }
    c.w = 1.f;
    return c;
}

// ---- The handles ----

// Where the handle is (the active bone's pivot, or the rest tool's point; IK: none) and its axes.
[[nodiscard]] bool rotatablePart(int p)
{
    return p == 0 || p == 1 || p == 6 || p == 7; // (the weapons, the chest, the pelvis)
}

void placeHandle()
{
    vs.h.shown = false;
    rigpose::Rigid part;
    if(st.aim.selected && st.aim.show)
    {
        // The aim target: moved in the world's axes
        vs.h.owner = 1;
        vs.h.move = true;
        vs.h.rot = false;
        vs.h.centre = st.aim.at;
        vs.h.axis[0] = glm::vec3{1.f, 0.f, 0.f};
        vs.h.axis[1] = glm::vec3{0.f, 1.f, 0.f};
        vs.h.axis[2] = glm::vec3{0.f, 0.f, 1.f};
    }
    else if(st.tool == Tool::PoseKeys && posePart(st.pdPart, part))
    {
        // A Pose Keys part: in its key's space's axes (the subject's facing: x forward, y left, z up)
        vs.h.owner = 2;
        vs.h.move = st.pdPart != 6;
        vs.h.rot = rotatablePart(st.pdPart);
        vs.h.centre = part.pos;
        const glm::quat q = bodyTurn();
        vs.h.axis[0] = q * glm::vec3{1.f, 0.f, 0.f};
        vs.h.axis[1] = q * glm::vec3{0.f, 1.f, 0.f};
        vs.h.axis[2] = q * glm::vec3{0.f, 0.f, 1.f};
    }
    else
    {
        if(st.active < 0 || st.active >= st.numBones || !visibleBone(st.active) ||
            (st.tool != Tool::Rotate && st.tool != Tool::Move && st.tool != Tool::Rest))
        {
            return;
        }
        vs.h.owner = 0;
        vs.h.move = st.tool != Tool::Rotate;
        vs.h.rot = st.tool == Tool::Rotate;
        const int b = st.active;
        const glm::vec3 local = st.tool == Tool::Rest && vs.restPoint == 11 ? st.ends[b] : st.pivots[b];
        vs.h.centre = toWorld(rigpose::apply(st.F[b], local));
        // Its axes: the bone's own (its rest axes turned with it); the rest tool's the model's.
        const glm::quat r = st.tool == Tool::Rest ? glm::quat{1.f, 0.f, 0.f, 0.f} : st.F[b].rot;
        for(int i = 0; i < 3; i++)
        {
            glm::vec3 a{0.f};
            a[i] = 1.f;
            vs.h.axis[i] = glm::normalize(toWorld(r * a) - toWorld(glm::vec3{0.f}));
        }
    }
    glm::vec2 s;
    float depth = 1.f;
    if(!project(vs.h.centre, s, &depth))
    {
        return;
    }
    vs.h.radius = vr_rigstudio_handle_size.value * depth * 2.f * st.view.tanY / st.view.h;
    vs.h.shown = true;
}

// A ring's points (screen), `n` of them round axis `a`.
[[nodiscard]] float ringRadius()
{
    return vs.h.radius * (vs.h.move && vs.h.rot ? ringOut : 1.f);
}

void ringPoints(const glm::vec3& a, glm::vec2* out, bool* front, int n)
{
    glm::vec3 u = glm::abs(a.z) < 0.9f ? glm::cross(a, glm::vec3{0.f, 0.f, 1.f}) : glm::cross(a, glm::vec3{1.f, 0.f, 0.f});
    u = glm::normalize(u);
    const glm::vec3 v = glm::cross(a, u);
    const float r = ringRadius();
    for(int i = 0; i < n; i++)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n) * 2.f * pi;
        const glm::vec3 p = vs.h.centre + (u * za::cos(t) + v * za::sin(t)) * r;
        project(p, out[i]);
        front[i] = glm::dot(p - vs.h.centre, st.view.org - vs.h.centre) >= -0.05f * r;
    }
}

constexpr int ringN = 48;

// The ring (0..2) nearest `m` within 7 pixels, else -1.
[[nodiscard]] int hitRing(const glm::vec2& m)
{
    int best = -1;
    float bestD = 7.f;
    for(int i = 0; i < 3; i++)
    {
        glm::vec2 pts[ringN];
        bool front[ringN];
        ringPoints(vs.h.axis[i], pts, front, ringN);
        for(int k = 0; k < ringN; k++)
        {
            if(!front[k] && !front[(k + 1) % ringN])
            {
                continue;
            }
            const float d = segDist(m, pts[k], pts[(k + 1) % ringN]);
            if(d < bestD)
            {
                bestD = d;
                best = i;
            }
        }
    }
    return best;
}

int hitHandle(const glm::vec2& m)
{
    if(!vs.h.shown)
    {
        return -1;
    }
    glm::vec2 c;
    project(vs.h.centre, c);
    if(vs.h.rot && !vs.h.move)
    {
        int best = hitRing(m);
        const float vr = vr_rigstudio_handle_size.value * 1.18f;
        if(best < 0 && vs.h.owner == 0 && za::abs(glm::length(m - c) - vr) < 7.f)
        {
            best = 3; // (a bone's view ring)
        }
        return best >= 0 && vs.h.owner == 2 ? 20 + best : best;
    }
    // Move and the rest tool: the arrows, the planes, the centre
    if(glm::length(m - c) < 8.f)
    {
        return 3;
    }
    for(int i = 0; i < 3; i++)
    {
        glm::vec2 e;
        project(vs.h.centre + vs.h.axis[i] * vs.h.radius, e);
        if(segDist(m, c, e) < 6.f)
        {
            return i;
        }
    }
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 a = vs.h.axis[(i + 1) % 3], b = vs.h.axis[(i + 2) % 3];
        glm::vec2 q[4];
        project(vs.h.centre + (a * 0.25f + b * 0.25f) * vs.h.radius, q[0]);
        project(vs.h.centre + (a * 0.45f + b * 0.25f) * vs.h.radius, q[1]);
        project(vs.h.centre + (a * 0.45f + b * 0.45f) * vs.h.radius, q[2]);
        project(vs.h.centre + (a * 0.25f + b * 0.45f) * vs.h.radius, q[3]);
        const glm::vec2 mid = (q[0] + q[2]) * 0.5f;
        if(glm::length(m - mid) < za::max(6.f, glm::length(q[2] - q[0]) * 0.5f))
        {
            return 4 + i;
        }
    }
    if(vs.h.rot)
    {
        const int r = hitRing(m); // (the rings outside the arrows)
        return r >= 0 ? 20 + r : -1;
    }
    return -1;
}

void drawHandle(ImDrawList* dl)
{
    if(!vs.h.shown)
    {
        return;
    }
    const glm::vec4 axisCol[3] = {{0.95f, 0.25f, 0.3f, 1.f}, {0.4f, 0.85f, 0.2f, 1.f}, {0.25f, 0.5f, 1.f, 1.f}};
    const glm::vec4 hot{1.f, 0.9f, 0.3f, 1.f};
    glm::vec2 c;
    project(vs.h.centre, c);
    const int part = st.dragging ? st.dragAxis : vs.hoverPart;
    if(vs.h.rot)
    {
        const int ringBase = vs.h.owner == 2 ? 20 : 0;
        for(int i = 0; i < 3; i++)
        {
            glm::vec2 pts[ringN];
            bool front[ringN];
            ringPoints(vs.h.axis[i], pts, front, ringN);
            for(int k = 0; k < ringN; k++)
            {
                const int k2 = (k + 1) % ringN;
                const bool f = front[k] && front[k2];
                glm::vec4 cc = part == ringBase + i ? hot : axisCol[i];
                cc.w = f ? 1.f : 0.25f;
                dl->AddLine(iv(pts[k]), iv(pts[k2]), col(cc), f ? 2.5f : 1.2f);
            }
        }
        if(vs.h.owner == 0)
        {
            const float vr = vr_rigstudio_handle_size.value * 1.18f;
            dl->AddCircle(iv(c), vr, col(part == 3 ? hot : glm::vec4{0.9f, 0.9f, 0.9f, 0.8f}), 48, 1.6f);
        }
        if(st.dragging && (st.dragKind == 1 || st.dragKind == 12))
        {
            dl->AddLine(iv(c), ImGui::GetIO().MousePos, col(glm::vec4{1.f, 1.f, 1.f, 0.5f}), 1.f);
            const float a = st.dragAngle / deg;
            char text[32];
            q_snprintf(text, sizeof(text), "%.1f deg", a);
            dl->AddText(ImVec2{ImGui::GetIO().MousePos.x + 14.f, ImGui::GetIO().MousePos.y + 4.f}, col(glm::vec4{1.f}), text);
        }
        if(!vs.h.move)
        {
            return;
        }
    }
    for(int i = 0; i < 3; i++)
    {
        glm::vec2 e;
        project(vs.h.centre + vs.h.axis[i] * vs.h.radius, e);
        const ImU32 cc = col(part == i ? hot : axisCol[i]);
        dl->AddLine(iv(c), iv(e), cc, 2.5f);
        const glm::vec2 d = glm::length(e - c) > 1e-3f ? glm::normalize(e - c) : glm::vec2{1.f, 0.f};
        const glm::vec2 n{-d.y, d.x};
        dl->AddTriangleFilled(iv(e + d * 10.f), iv(e + n * 5.f), iv(e - n * 5.f), cc);
    }
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 a = vs.h.axis[(i + 1) % 3], b = vs.h.axis[(i + 2) % 3];
        glm::vec2 q[4];
        project(vs.h.centre + (a * 0.25f + b * 0.25f) * vs.h.radius, q[0]);
        project(vs.h.centre + (a * 0.45f + b * 0.25f) * vs.h.radius, q[1]);
        project(vs.h.centre + (a * 0.45f + b * 0.45f) * vs.h.radius, q[2]);
        project(vs.h.centre + (a * 0.25f + b * 0.45f) * vs.h.radius, q[3]);
        glm::vec4 cc = part == 4 + i ? hot : axisCol[i];
        cc.w = 0.45f;
        dl->AddQuadFilled(iv(q[0]), iv(q[1]), iv(q[2]), iv(q[3]), col(cc));
    }
    dl->AddCircleFilled(iv(c), 5.f, col(part == 3 ? hot : glm::vec4{1.f, 1.f, 1.f, 0.85f}));
}

// ---- The drags ----

void beginDrag(int kind, int axis, const glm::vec2& m)
{
    st.dragging = true;
    st.dragKind = kind;
    st.dragAxis = axis;
    st.dragStart = m;
    st.dragLast = m;
    st.dragAngle = 0.f;
    vs.dragH = vs.h;
    vs.snapAccum = vs.snapApplied = 0.f;
    vs.moveAccum = vs.moveApplied = glm::vec3{0.f};
    if(kind == 2 || kind == 4 || kind == 5 || kind == 10 || kind == 11)
    {
        const glm::vec3 n = axis >= 4 && axis <= 6 ? vs.h.axis[axis - 4] : st.view.fwd;
        if(!rayPlane(m, vs.h.centre, n, vs.planeStart))
        {
            vs.planeStart = vs.h.centre;
        }
    }
    if(kind == 10)
    {
        // The aim target: its place undone as one edit; off the camera once moved by hand
        vs.aimStart = st.aim.at;
        st.aim.atCamera = false;
        pushAimUndo();
        return;
    }
    if(kind == 11 || kind == 12)
    {
        // A Pose Keys part: the key the drag edits (posestudio's mapping), its value now kept for the undo
        if(!posePart(st.pdPart, vs.deskStart) ||
            !posestudio::deskBegin(st.note, st.pdPart, agpose::code(st.pdKind, st.pdClass, st.pdPose, agpose::isStrike(st.pdPose) ? st.pdPhase : 0), st.pdPose))
        {
            st.dragging = false;
            st.dragKind = 0;
            say("that part is not one of the key's now");
            return;
        }
        pushPoseUndo(posestudio::deskCode());
        return;
    }
    pushUndo(kind == 1 ? "rotate" : kind == 2 ? "move" : kind == 3 ? "IK" : kind == 7 ? "paint" : kind >= 4 && kind <= 5 ? "rest pose" : "edit");
}

[[nodiscard]] bool ctrl()
{
    return ImGui::GetIO().KeyCtrl;
}

// The selection's roots (selected bones none of whose ancestors is selected): each turned or moved by the drag.
void forRoots(const rigpose::Rigid& worldModel, bool aboutOwn, const glm::quat& rot)
{
    for(int b = 0; b < st.numBones; b++)
    {
        int p = st.rig->bones[b].parent;
        bool under = false;
        while(p >= 0)
        {
            under = under || (st.selBones & (1u << p));
            p = st.rig->bones[p].parent;
        }
        if(!(st.selBones & (1u << b)) || under)
        {
            continue;
        }
        if(aboutOwn)
        {
            evaluate(st.frame, st.F, st.P, true);
            const glm::vec3 c = rigpose::apply(st.F[b], st.pivots[b]);
            rigpose::Rigid w;
            w.rot = rot;
            w.pos = c - rot * c;
            moveBone(b, w);
        }
        else
        {
            moveBone(b, worldModel);
        }
    }
}

void dragRotate(const glm::vec2& m)
{
    glm::vec2 c;
    project(vs.h.centre, c);
    const glm::vec2 a0 = st.dragLast - c, a1 = m - c;
    if(glm::length(a0) < 2.f || glm::length(a1) < 2.f)
    {
        st.dragLast = m;
        return;
    }
    float d = za::atan2(-a1.y, a1.x) - za::atan2(-a0.y, a0.x);
    d = d > pi ? d - 2.f * pi : d < -pi ? d + 2.f * pi : d;
    st.dragLast = m;
    const glm::vec3 axisW = st.dragAxis == 3 ? -st.view.fwd : vs.h.axis[st.dragAxis];
    const float sign = st.dragAxis == 3 ? 1.f : (glm::dot(axisW, st.view.fwd) < 0.f ? 1.f : -1.f);
    vs.snapAccum += d * sign;
    float apply = vs.snapAccum - vs.snapApplied;
    if(ctrl() || st.snap)
    {
        const float step = 15.f * deg;
        apply = za::floor(vs.snapAccum / step + 0.5f) * step - vs.snapApplied;
    }
    if(apply == 0.f)
    {
        return;
    }
    vs.snapApplied += apply;
    st.dragAngle = vs.snapApplied;
    const glm::vec3 axisM = glm::normalize(toModelDir(axisW));
    forRoots(rigpose::Rigid{}, true, glm::angleAxis(apply, axisM));
    publish();
}

[[nodiscard]] glm::vec3 dragDisplacement(const glm::vec2& m)
{
    // (the handle as the drag began: the part moving under it changes neither its arrows' scale nor its plane)
    const Handle& h = vs.dragH;
    if(st.dragAxis <= 2)
    {
        glm::vec2 c, e;
        project(h.centre, c);
        project(h.centre + h.axis[st.dragAxis] * h.radius, e);
        const glm::vec2 A = e - c;
        const float l2 = za::max(glm::dot(A, A), 1e-3f);
        return h.axis[st.dragAxis] * (h.radius * glm::dot(m - st.dragStart, A) / l2);
    }
    const glm::vec3 n = st.dragAxis >= 4 ? h.axis[st.dragAxis - 4] : st.view.fwd;
    glm::vec3 hit;
    if(!rayPlane(m, h.centre, n, hit))
    {
        return glm::vec3{0.f};
    }
    return hit - vs.planeStart;
}

void dragMove(const glm::vec2& m, bool rest)
{
    glm::vec3 total = dragDisplacement(m);
    glm::vec3 totalM = toModelDir(total);
    if(ctrl() || st.snap)
    {
        totalM = glm::floor(totalM + glm::vec3{0.5f});
    }
    const glm::vec3 apply = totalM - vs.moveApplied;
    if(apply == glm::vec3{0.f})
    {
        return;
    }
    vs.moveApplied = totalM;
    if(rest)
    {
        // The rest pose's point moved as drawn: back through the bone's turn to the rest space.
        const int b = st.active;
        rigdata::BoneEdit& e = st.doc.bones[b];
        if(!e.on)
        {
            const ragdoll::Bone& rb = st.rig->bones[b];
            e.on = true;
            e.pivot = rb.pivot;
            e.end = rb.end;
            e.cone = rb.cone / deg;
            e.twist = rb.twist / deg;
            e.lower = rb.lower / deg;
            e.upper = rb.upper / deg;
            e.hinge = rb.hinge;
        }
        const glm::vec3 d = glm::inverse(st.F[b].rot) * apply;
        (st.dragKind == 5 ? e.end : e.pivot) += d;
        touchedRig();
        return;
    }
    rigpose::Rigid w;
    w.pos = apply;
    forRoots(w, false, glm::quat{1.f, 0.f, 0.f, 0.f});
    publish();
}

// The aim target dragged: its place from where it began, by the handle's move (world; Ctrl: whole units).
void dragAim(const glm::vec2& m)
{
    glm::vec3 total = dragDisplacement(m);
    if(ctrl() || st.snap)
    {
        total = glm::floor(total + glm::vec3{0.5f});
    }
    st.aim.at = vs.aimStart + total;
}

// A Pose Keys part dragged: moved (kind 11: by the handle's move, in its key's space's axes; Ctrl: whole units) or
// turned (12: by the ring's sweep round its centre on the screen, as a bone's ring; Ctrl: 15 degrees), its key made by
// the Pose Studio's own mapping (posestudio::deskDrag) and previewed: the game lays it on next frame.
void dragDesk(const glm::vec2& m)
{
    rigpose::Rigid now = vs.deskStart;
    glm::quat turn{1.f, 0.f, 0.f, 0.f};
    const glm::quat body = bodyTurn();
    if(st.dragKind == 11)
    {
        glm::vec3 local = glm::inverse(body) * dragDisplacement(m);
        if(ctrl() || st.snap)
        {
            local = glm::floor(local + glm::vec3{0.5f});
        }
        now.pos += body * local;
    }
    else
    {
        glm::vec2 c;
        project(vs.h.centre, c);
        const glm::vec2 a0 = st.dragLast - c, a1 = m - c;
        if(glm::length(a0) >= 2.f && glm::length(a1) >= 2.f)
        {
            float d = za::atan2(-a1.y, a1.x) - za::atan2(-a0.y, a0.x);
            d = d > pi ? d - 2.f * pi : d < -pi ? d + 2.f * pi : d;
            const glm::vec3 axisW = vs.h.axis[st.dragAxis - 20];
            vs.snapAccum += d * (glm::dot(axisW, st.view.fwd) < 0.f ? 1.f : -1.f);
            st.dragLast = m;
        }
        const float step = 15.f * deg;
        const float a = ctrl() || st.snap ? za::floor(vs.snapAccum / step + 0.5f) * step : vs.snapAccum;
        st.dragAngle = a;
        const glm::vec3 axisW = vs.h.axis[st.dragAxis - 20];
        now.rot = glm::normalize(glm::angleAxis(a, axisW) * vs.deskStart.rot);
        turn = glm::angleAxis(a, glm::inverse(body) * axisW); // (the chest's: in its key's space)
    }
    posestudio::deskDrag(st.note, now, turn);
}

// The IK chains: a hinge bone on a ball one (forearm on upper arm, shin on thigh).
[[nodiscard]] bool chain(int lower, int& upper)
{
    if(!st.rig || lower < 0 || lower >= st.numBones)
    {
        return false;
    }
    const ragdoll::Bone& b = st.rig->bones[lower];
    upper = b.parent;
    return b.joint == ragdoll::Joint::Hinge && upper >= 0 && st.rig->bones[upper].joint == ragdoll::Joint::Ball;
}

// The shortest turn taking direction `a` to `b` (unit vectors).
[[nodiscard]] glm::quat arc(const glm::vec3& a, const glm::vec3& b)
{
    const float d = glm::dot(a, b);
    if(d > 0.99999f)
    {
        return glm::quat{1.f, 0.f, 0.f, 0.f};
    }
    if(d < -0.99999f)
    {
        glm::vec3 axis = glm::cross(glm::vec3{1.f, 0.f, 0.f}, a);
        if(glm::length(axis) < 1e-3f)
        {
            axis = glm::cross(glm::vec3{0.f, 1.f, 0.f}, a);
        }
        return glm::angleAxis(pi, glm::normalize(axis));
    }
    const glm::vec3 c = glm::cross(a, b);
    return glm::normalize(glm::quat{1.f + d, c.x, c.y, c.z});
}

// Two bones turned so the lower's end reaches `target` (model space), the elbow kept in its plane.
void solveIk(int lower, const glm::vec3& target)
{
    int upper = -1;
    if(!chain(lower, upper))
    {
        return;
    }
    evaluate(st.frame, st.F, st.P, true);
    const glm::vec3 S = rigpose::apply(st.F[upper], st.pivots[upper]);
    const glm::vec3 E = rigpose::apply(st.F[lower], st.pivots[lower]);
    const glm::vec3 H = rigpose::apply(st.F[lower], st.ends[lower]);
    const float a = glm::length(E - S), b = glm::length(H - E);
    glm::vec3 T = target;
    const float reach = glm::length(T - S);
    const float most = (a + b) * 0.999f, least = za::abs(a - b) * 1.001f + 0.01f;
    const glm::vec3 dir = reach > 1e-4f ? (T - S) / reach : glm::vec3{1.f, 0.f, 0.f};
    const float d = za::clamp(reach, least, most);
    T = S + dir * d;
    // The bend's plane: the elbow's way off the line now.
    glm::vec3 side = (E - S) - dir * glm::dot(E - S, dir);
    if(glm::length(side) < 1e-3f)
    {
        side = glm::cross(dir, glm::vec3{0.f, 0.f, 1.f});
    }
    side = glm::normalize(side);
    const float cosA = za::clamp((a * a + d * d - b * b) / (2.f * a * d), -1.f, 1.f);
    const glm::vec3 E2 = S + dir * (a * cosA) + side * (a * za::sqrt(za::max(0.f, 1.f - cosA * cosA)));
    // The upper bone turned (E -> E2 about S), then the lower (its end to T about E2).
    const glm::quat r1 = arc(glm::normalize(E - S), glm::normalize(E2 - S));
    rigpose::Rigid w1;
    w1.rot = r1;
    w1.pos = S - r1 * S;
    moveBone(upper, w1);
    evaluate(st.frame, st.F, st.P, true);
    const glm::vec3 E3 = rigpose::apply(st.F[lower], st.pivots[lower]);
    const glm::vec3 H3 = rigpose::apply(st.F[lower], st.ends[lower]);
    const glm::quat r2 = arc(glm::normalize(H3 - E3), glm::normalize(T - E3));
    rigpose::Rigid w2;
    w2.rot = r2;
    w2.pos = E3 - r2 * E3;
    moveBone(lower, w2);
    publish();
}

void endDrag()
{
    if(!st.dragging)
    {
        return;
    }
    const int kind = st.dragKind;
    st.dragging = false;
    st.dragKind = 0;
    st.dragAxis = -1;
    if(kind == 11 || kind == 12)
    {
        posestudio::deskEnd(true); // (the key kept: vr_agrunt_poses, as the window's edits)
        return;
    }
    if(kind == 10)
    {
        return;
    }
    if((kind == 1 || kind == 2 || kind == 3) && st.autoKey && st.source != Source::Rest)
    {
        if(kind == 3 && st.ikChain >= 0)
        {
            int upper = -1;
            (void)chain(st.ikChain, upper);
            const uint32_t keep = st.selBones;
            st.selBones = (1u << st.ikChain) | (upper >= 0 ? 1u << upper : 0u);
            keySelected();
            st.selBones = keep;
        }
        else
        {
            keySelected();
        }
    }
    st.ikChain = -1;
}

// ---- Picks ----

[[nodiscard]] int pickBone(const glm::vec2& m)
{
    int best = -1;
    float bestD = 9.f;
    for(int b = 0; b < st.numBones; b++)
    {
        if(!visibleBone(b))
        {
            continue;
        }
        glm::vec2 a, e;
        if(!project(toWorld(rigpose::apply(st.F[b], st.pivots[b])), a) || !project(toWorld(rigpose::apply(st.F[b], st.ends[b])), e))
        {
            continue;
        }
        const float d = segDist(m, a, e);
        if(d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

[[nodiscard]] bool pickable(int v)
{
    return st.mesh == MeshMode::Xray || st.facing[static_cast<za::SizeT>(v)];
}

[[nodiscard]] int pickVert(const glm::vec2& m)
{
    int best = -1;
    float bestScore = 1e30f;
    for(za::SizeT v = 0; v < st.screen.size(); v++)
    {
        if(st.depth[v] <= 0.f || !pickable(static_cast<int>(v)))
        {
            continue;
        }
        const float d = glm::length(st.screen[v] - m);
        if(d > 9.f)
        {
            continue;
        }
        const float score = st.depth[v] + d * 0.5f;
        if(score < bestScore)
        {
            bestScore = score;
            best = static_cast<int>(v);
        }
    }
    return best;
}

// The IK ends (each chain's lower bone's end) near the mouse.
[[nodiscard]] int pickIkEnd(const glm::vec2& m)
{
    int best = -1;
    float bestD = 10.f;
    for(int b = 0; b < st.numBones; b++)
    {
        int upper = -1;
        if(!chain(b, upper) || !visibleBone(b))
        {
            continue;
        }
        glm::vec2 s;
        if(!project(toWorld(rigpose::apply(st.F[b], st.ends[b])), s))
        {
            continue;
        }
        const float d = glm::length(s - m);
        if(d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

void paint(const glm::vec2& m)
{
    if(st.active < 0 || vs.paintFrame == host_framecount)
    {
        return;
    }
    vs.paintFrame = host_framecount;
    const float r = st.brushRadius;
    const float dt = za::clamp(static_cast<float>(host_frametime), 0.f, 0.1f) * 10.f; // (a tenth of a second: the strength)
    bool any = false;
    for(za::SizeT v = 0; v < st.screen.size(); v++)
    {
        if(st.depth[v] <= 0.f || !pickable(static_cast<int>(v)))
        {
            continue;
        }
        const float d = glm::length(st.screen[v] - m);
        if(d > r)
        {
            continue;
        }
        const float f = (1.f - d / r) * (1.f - d / r);
        const float amount = za::clamp(st.brushStrength * f * dt, 0.f, 1.f);
        const auto apply = [&](int vert, int bone) {
            if(vert < 0 || bone < 0)
            {
                return;
            }
            const float w = weightOn(vert, bone);
            float nw = w;
            if(st.brush == Brush::Add)
            {
                nw = w + amount;
            }
            else if(st.brush == Brush::Subtract)
            {
                nw = w - amount;
            }
            else
            {
                float sum = 0.f;
                int n = 0;
                for(const int nb : st.nbrs[static_cast<za::SizeT>(vert)])
                {
                    sum += weightOn(nb, bone);
                    n++;
                }
                if(n)
                {
                    nw = w + (sum / static_cast<float>(n) - w) * za::min(amount * 2.f, 1.f);
                }
            }
            setVertWeight(vert, bone, za::clamp(nw, 0.f, 1.f));
            st.weights[static_cast<za::SizeT>(vert)] = editWeights(vert);
        };
        apply(static_cast<int>(v), st.active);
        if(st.mirror)
        {
            apply(mirrorVert(static_cast<int>(v)), mirrorBone(st.active));
        }
        any = true;
    }
    if(any)
    {
        touchedRig();
        st.weightsGen = st.docGen; // (the effective weights kept in step above)
    }
}

void boxSelect(const glm::vec2& a, const glm::vec2& b, bool add, bool remove)
{
    const glm::vec2 lo = glm::min(a, b), hi = glm::max(a, b);
    for(za::SizeT v = 0; v < st.screen.size(); v++)
    {
        const glm::vec2 s = st.screen[v];
        const bool in = st.depth[v] > 0.f && pickable(static_cast<int>(v)) && s.x >= lo.x && s.x <= hi.x && s.y >= lo.y && s.y <= hi.y;
        uint8_t& sel = st.selVerts[v];
        if(remove)
        {
            sel = in ? 0 : sel;
        }
        else if(add)
        {
            sel = in ? 1 : sel;
        }
        else
        {
            sel = in ? 1 : 0;
        }
    }
}

// ---- Drawing ----

void drawMesh(ImDrawList* dl)
{
    if(st.tris.empty() || st.screen.empty())
    {
        return;
    }
    const bool fill = st.mesh == MeshMode::Shaded || st.mesh == MeshMode::Xray || (st.mesh != MeshMode::Game && st.colors != Colors::None);
    const bool xray = st.mesh == MeshMode::Xray;
    const int nt = static_cast<int>(st.tris.size() / 3);
    // Painter's order: far first.
    za::Vector<glm::vec2> order; // (depth, triangle)
    order.reserve(static_cast<za::SizeT>(nt));
    for(int t = 0; t < nt; t++)
    {
        const int a = st.tris[static_cast<za::SizeT>(3 * t)], b = st.tris[static_cast<za::SizeT>(3 * t + 1)], c = st.tris[static_cast<za::SizeT>(3 * t + 2)];
        if(st.depth[static_cast<za::SizeT>(a)] <= 0.f || st.depth[static_cast<za::SizeT>(b)] <= 0.f || st.depth[static_cast<za::SizeT>(c)] <= 0.f)
        {
            continue;
        }
        const glm::vec3& pa = st.worldVerts[static_cast<za::SizeT>(a)];
        glm::vec3 n = glm::cross(st.worldVerts[static_cast<za::SizeT>(b)] - pa, st.worldVerts[static_cast<za::SizeT>(c)] - pa);
        if(vs.windingFlip)
        {
            n = -n;
        }
        const bool front = glm::dot(n, st.view.org - pa) > 0.f;
        if(!front && !xray)
        {
            continue;
        }
        order.pushBack(glm::vec2{st.depth[static_cast<za::SizeT>(a)] + st.depth[static_cast<za::SizeT>(b)] + st.depth[static_cast<za::SizeT>(c)], static_cast<float>(t)});
    }
    // (far first: a shell sort's gaps, the triangles' summed depths)
    for(za::SizeT gap = order.size() / 2; gap > 0; gap /= 2)
    {
        for(za::SizeT i = gap; i < order.size(); i++)
        {
            const glm::vec2 t = order[i];
            za::SizeT j = i;
            while(j >= gap && order[j - gap].x < t.x)
            {
                order[j] = order[j - gap];
                j -= gap;
            }
            order[j] = t;
        }
    }
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    const glm::vec3 light = glm::normalize(-st.view.fwd + st.view.up * 0.6f + st.view.right * 0.3f);
    for(const glm::vec2& o : order)
    {
        const int t = static_cast<int>(o.y);
        const int v3[3] = {st.tris[static_cast<za::SizeT>(3 * t)], st.tris[static_cast<za::SizeT>(3 * t + 1)], st.tris[static_cast<za::SizeT>(3 * t + 2)]};
        const glm::vec3& pa = st.worldVerts[static_cast<za::SizeT>(v3[0])];
        glm::vec3 n = glm::cross(st.worldVerts[static_cast<za::SizeT>(v3[1])] - pa, st.worldVerts[static_cast<za::SizeT>(v3[2])] - pa);
        n = glm::length(n) > 1e-6f ? glm::normalize(vs.windingFlip ? -n : n) : glm::vec3{0.f, 0.f, 1.f};
        const float lit = 0.35f + 0.65f * za::max(glm::dot(n, light), 0.f);
        if(fill)
        {
            dl->PrimReserve(3, 3);
            const ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
            for(int k = 0; k < 3; k++)
            {
                glm::vec4 c = st.colors == Colors::None ? glm::vec4{0.72f, 0.7f, 0.66f, 1.f} : vertColor(v3[k]);
                c = glm::vec4{glm::vec3{c} * lit, xray ? 0.28f : 1.f};
                dl->PrimWriteVtx(iv(st.screen[static_cast<za::SizeT>(v3[k])]), uv, col(c));
            }
            dl->PrimWriteIdx(base);
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1));
            dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
        }
        if(st.mesh == MeshMode::Wire || xray)
        {
            const ImU32 wc = col(glm::vec4{0.1f, 0.1f, 0.1f, xray ? 0.45f : 0.75f});
            for(int k = 0; k < 3; k++)
            {
                dl->AddLine(iv(st.screen[static_cast<za::SizeT>(v3[k])]), iv(st.screen[static_cast<za::SizeT>(v3[(k + 1) % 3])]), wc, 1.f);
            }
        }
    }
    // Vertex dots: the weights' tools (selected ones bright), the colours over the game's own drawing.
    const bool weightsTool = st.tool == Tool::Weights || st.tool == Tool::Paint;
    if(weightsTool || (st.mesh == MeshMode::Game && st.colors != Colors::None))
    {
        for(za::SizeT v = 0; v < st.screen.size(); v++)
        {
            if(st.depth[v] <= 0.f || !pickable(static_cast<int>(v)))
            {
                continue;
            }
            const bool sel = v < st.selVerts.size() && st.selVerts[v];
            const glm::vec4 c = sel ? glm::vec4{1.f, 0.6f, 0.1f, 1.f} : st.colors != Colors::None ? vertColor(static_cast<int>(v)) : glm::vec4{0.1f, 0.1f, 0.1f, 0.8f};
            dl->AddCircleFilled(iv(st.screen[v]), sel ? 3.f : 2.f, col(c), 6);
        }
    }
}

void drawBones(ImDrawList* dl, const rigpose::Rigid* F, float alpha, bool ghost)
{
    if(st.bones == BoneLook::Off && !ghost)
    {
        return;
    }
    for(int b = 0; b < st.numBones; b++)
    {
        if(!visibleBone(b))
        {
            continue;
        }
        const glm::vec3 h = toWorld(rigpose::apply(F[b], st.pivots[b]));
        const glm::vec3 t = toWorld(rigpose::apply(F[b], st.ends[b]));
        const bool sel = (st.selBones & (1u << b)) != 0;
        glm::vec4 c = ghost ? glm::vec4{0.6f, 0.8f, 1.f, 1.f}
                      : b == st.active ? glm::vec4{1.f, 0.75f, 0.25f, 1.f}
                      : sel             ? glm::vec4{0.95f, 0.5f, 0.15f, 1.f}
                      : b == st.hoverBone ? glm::vec4{0.85f, 0.85f, 1.f, 1.f}
                                          : glm::vec4{0.75f, 0.75f, 0.78f, 1.f};
        c.w = alpha;
        glm::vec2 sh, stl;
        if(!project(h, sh) || !project(t, stl))
        {
            continue;
        }
        const float len = glm::length(t - h);
        if(st.bones == BoneLook::Octahedral && !ghost && len > 0.5f)
        {
            const glm::vec3 d = (t - h) / len;
            glm::vec3 u = glm::abs(d.z) < 0.9f ? glm::cross(d, glm::vec3{0.f, 0.f, 1.f}) : glm::cross(d, glm::vec3{1.f, 0.f, 0.f});
            u = glm::normalize(u);
            const glm::vec3 v = glm::cross(d, u);
            const float w = 0.1f * len;
            const glm::vec3 m = h + d * (0.15f * len);
            glm::vec2 s[4];
            const glm::vec3 r[4] = {m + u * w, m + v * w, m - u * w, m - v * w};
            bool ok = true;
            for(int k = 0; k < 4; k++)
            {
                ok = ok && project(r[k], s[k]);
            }
            if(!ok)
            {
                continue;
            }
            glm::vec4 fc = c;
            fc.w = alpha * 0.35f;
            for(int k = 0; k < 4; k++)
            {
                dl->AddTriangleFilled(iv(sh), iv(s[k]), iv(s[(k + 1) % 4]), col(fc));
                dl->AddTriangleFilled(iv(stl), iv(s[k]), iv(s[(k + 1) % 4]), col(fc));
            }
            for(int k = 0; k < 4; k++)
            {
                dl->AddLine(iv(sh), iv(s[k]), col(c), 1.2f);
                dl->AddLine(iv(stl), iv(s[k]), col(c), 1.2f);
                dl->AddLine(iv(s[k]), iv(s[(k + 1) % 4]), col(c), 1.2f);
            }
        }
        else
        {
            dl->AddLine(iv(sh), iv(stl), col(c), ghost ? 1.5f : 3.f);
        }
        if(st.joints || ghost)
        {
            dl->AddCircleFilled(iv(sh), ghost ? 2.f : 3.5f, col(c), 10);
            dl->AddCircle(iv(stl), ghost ? 1.5f : 2.5f, col(c), 10);
        }
    }
}

void drawLimits(ImDrawList* dl)
{
    if(!st.limits || st.active < 0)
    {
        return;
    }
    const int b = st.active;
    const ragdoll::Bone& rb = st.rig->bones[b];
    const int p = rb.parent;
    if(p < 0)
    {
        return;
    }
    const rigdata::BoneEdit& e = st.doc.bones[b];
    const glm::vec3 c = toWorld(rigpose::apply(st.F[b], st.pivots[b]));
    const glm::vec3 restDir = glm::normalize(st.ends[b] - st.pivots[b]);
    const glm::vec3 dirW = glm::normalize(toWorld(st.F[p].rot * restDir) - toWorld(glm::vec3{0.f}));
    const float len = glm::length(st.ends[b] - st.pivots[b]);
    if(rb.joint == ragdoll::Joint::Ball)
    {
        const float cone = (e.on ? e.cone : rb.cone / deg) * deg;
        glm::vec3 u = glm::abs(dirW.z) < 0.9f ? glm::cross(dirW, glm::vec3{0.f, 0.f, 1.f}) : glm::cross(dirW, glm::vec3{1.f, 0.f, 0.f});
        u = glm::normalize(u);
        const glm::vec3 v = glm::cross(dirW, u);
        glm::vec2 prev{0.f}, first{0.f}, s0;
        project(c, s0);
        for(int i = 0; i <= 32; i++)
        {
            const float t = static_cast<float>(i) / 32.f * 2.f * pi;
            const glm::vec3 d = dirW * za::cos(cone) + (u * za::cos(t) + v * za::sin(t)) * za::sin(cone);
            glm::vec2 s;
            project(c + d * len, s);
            if(i > 0)
            {
                dl->AddLine(iv(prev), iv(s), col(glm::vec4{0.2f, 0.9f, 0.9f, 0.8f}), 1.2f);
            }
            if(i % 8 == 0)
            {
                dl->AddLine(iv(s0), iv(s), col(glm::vec4{0.2f, 0.9f, 0.9f, 0.35f}), 1.f);
            }
            prev = s;
        }
        (void)first;
        agarms::ShoulderInfo sh;
        if(agarms::shoulderLimits(st.model, b, sh) && sh.chest >= 0 && sh.chest < st.rig->numBones)
        {
            // An armed humanoid's shoulder as its arms' IK has it (vr_agarms_limits, _clavicle): its clavicle from the
            // pivot near the neck to the joint (yellow), the cone its upper arm keeps within (magenta; dimmed when off).
            const rigpose::Rigid& fc = st.F[sh.chest];
            glm::vec2 sk, sc;
            if(project(toWorld(rigpose::apply(fc, sh.clavicle)), sk) && project(c, sc))
            {
                dl->AddLine(iv(sk), iv(sc), col(glm::vec4{1.f, 0.9f, 0.2f, 0.9f}), 2.f);
                dl->AddCircleFilled(iv(sk), 3.f, col(glm::vec4{1.f, 0.9f, 0.2f, 0.9f}), 10);
            }
            const glm::vec3 ax = glm::normalize(toWorld(fc.rot * sh.axis) - toWorld(glm::vec3{0.f}));
            const float ikCone = sh.cone * deg;
            glm::vec3 iu = glm::abs(ax.z) < 0.9f ? glm::cross(ax, glm::vec3{0.f, 0.f, 1.f}) : glm::cross(ax, glm::vec3{1.f, 0.f, 0.f});
            iu = glm::normalize(iu);
            const glm::vec3 iw = glm::cross(ax, iu);
            const glm::vec4 kc{1.f, 0.3f, 0.9f, sh.on ? 0.85f : 0.3f};
            glm::vec2 ip{0.f}, sa;
            project(c + ax * len, sa);
            dl->AddLine(iv(sc), iv(sa), col(kc), 1.f);
            for(int i = 0; i <= 32; i++)
            {
                const float t = static_cast<float>(i) / 32.f * 2.f * pi;
                const glm::vec3 d = ax * za::cos(ikCone) + (iu * za::cos(t) + iw * za::sin(t)) * za::sin(ikCone);
                glm::vec2 s;
                project(c + d * len, s);
                if(i > 0)
                {
                    dl->AddLine(iv(ip), iv(s), col(kc), 1.5f);
                }
                ip = s;
            }
        }
    }
    else if(rb.joint == ragdoll::Joint::Hinge)
    {
        const glm::vec3 axis = glm::normalize(toWorld(st.F[p].rot * (e.on ? e.hinge : rb.hinge)) - toWorld(glm::vec3{0.f}));
        const float lo = (e.on ? e.lower : rb.lower / deg) * deg, hi = (e.on ? e.upper : rb.upper / deg) * deg;
        glm::vec2 prev{0.f}, s0;
        project(c, s0);
        for(int i = 0; i <= 24; i++)
        {
            const float a = lo + (hi - lo) * static_cast<float>(i) / 24.f;
            const glm::vec3 d = glm::angleAxis(a, axis) * dirW;
            glm::vec2 s;
            project(c + d * len, s);
            if(i > 0)
            {
                dl->AddLine(iv(prev), iv(s), col(glm::vec4{1.f, 0.6f, 0.15f, 0.85f}), 1.5f);
            }
            if(i == 0 || i == 24)
            {
                dl->AddLine(iv(s0), iv(s), col(glm::vec4{1.f, 0.6f, 0.15f, 0.5f}), 1.f);
            }
            prev = s;
        }
    }
}

void drawProxies(ImDrawList* dl)
{
    if(!st.proxies || !st.rig)
    {
        return;
    }
    const int chest = boneByName("chest");
    if(chest < 0)
    {
        return;
    }
    const agbody::Proxies* px = agbody::of(st.model, *st.rig, chest, 1.f);
    if(!px || !px->ok)
    {
        return;
    }
    const agbody::Posed q = agbody::pose(*px, st.F, nullptr);
    for(int i = 0; i < q.n; i++)
    {
        const glm::vec3 a = toWorld(q.a[i]), b = toWorld(q.b[i]);
        glm::vec2 sa, sb;
        if(!project(a, sa) || !project(b, sb))
        {
            continue;
        }
        const float r = (q.radii[i].y + q.back[i].y) * 0.5f;
        const glm::vec3 side = glm::normalize(toWorld(q.rot[i] * glm::vec3{0.f, 1.f, 0.f}) - toWorld(glm::vec3{0.f}));
        glm::vec2 e;
        project(a + side * r, e);
        const float pr = glm::length(e - sa);
        const ImU32 c = col(glm::vec4{0.3f, 1.f, 0.5f, 0.7f});
        dl->AddCircle(iv(sa), pr, c, 24, 1.2f);
        dl->AddCircle(iv(sb), pr, c, 24, 1.2f);
        const glm::vec2 d = glm::length(sb - sa) > 1e-3f ? glm::normalize(sb - sa) : glm::vec2{1.f, 0.f};
        const glm::vec2 n{-d.y * pr, d.x * pr};
        dl->AddLine(iv(sa + n), iv(sb + n), c, 1.2f);
        dl->AddLine(iv(sa - n), iv(sb - n), c, 1.2f);
    }
}

void drawAxes(ImDrawList* dl, const glm::vec3& org, const glm::quat& rot, float len, float alpha)
{
    glm::vec2 o;
    if(!project(org, o))
    {
        return;
    }
    const glm::vec4 c[3] = {{1.f, 0.3f, 0.3f, alpha}, {0.3f, 1.f, 0.3f, alpha}, {0.3f, 0.5f, 1.f, alpha}};
    for(int i = 0; i < 3; i++)
    {
        glm::vec3 a{0.f};
        a[i] = 1.f;
        glm::vec2 e;
        if(project(org + rot * a * len, e))
        {
            dl->AddLine(iv(o), iv(e), col(c[i]), 2.f);
        }
    }
}

// The arms' targets as the game solved them (relinkSkinned's note), the IK tool's ends.
void drawTargets(ImDrawList* dl)
{
    if(st.tool == Tool::IK)
    {
        for(int b = 0; b < st.numBones; b++)
        {
            int upper = -1;
            if(!chain(b, upper) || !visibleBone(b))
            {
                continue;
            }
            glm::vec2 s;
            if(project(toWorld(rigpose::apply(st.F[b], st.ends[b])), s))
            {
                const bool hot = st.ikChain == b;
                const ImU32 c = col(hot ? glm::vec4{1.f, 0.9f, 0.2f, 1.f} : glm::vec4{1.f, 0.4f, 0.9f, 0.95f});
                dl->AddQuadFilled(ImVec2{s.x, s.y - 7.f}, ImVec2{s.x + 7.f, s.y}, ImVec2{s.x, s.y + 7.f}, ImVec2{s.x - 7.f, s.y}, c);
            }
        }
    }
    if(st.noteFrame < 0 || host_framecount - st.noteFrame > 5 || (!st.ikTargets && !st.grips))
    {
        return;
    }
    const posestudio::Note& n = st.note;
    const float ya = n.space.yawDeg * deg;
    const auto w = [&](const glm::vec3& p) {
        const glm::vec3 s = p * n.space.scale;
        return n.space.origin + glm::vec3{za::cos(ya) * s.x - za::sin(ya) * s.y, za::sin(ya) * s.x + za::cos(ya) * s.y, s.z};
    };
    const glm::quat yawQ = glm::angleAxis(ya, glm::vec3{0.f, 0.f, 1.f});
    for(int side = 0; side < 2; side++)
    {
        if(st.ikTargets && n.t.goal[side] != agarms::Goal::None)
        {
            glm::vec2 s;
            if(project(w(n.t.weapon[side].pos), s))
            {
                dl->AddCircle(iv(s), 6.f, col(glm::vec4{1.f, 0.2f, 0.2f, 0.9f}), 12, 2.f);
                dl->AddText(ImVec2{s.x + 8.f, s.y - 6.f}, col(glm::vec4{1.f, 0.5f, 0.5f, 1.f}), side ? "L target" : "R target");
            }
            glm::vec2 e, sh;
            if(project(w(n.placed.elbow[side]), e) && project(w(n.placed.shoulder[side]), sh))
            {
                dl->AddCircle(iv(e), 4.f, col(glm::vec4{1.f, 0.8f, 0.2f, 0.9f}), 10, 1.5f);
            }
        }
        if(st.grips && n.placed.on[side])
        {
            drawAxes(dl, w(n.placed.weapon[side].pos), yawQ * n.placed.weapon[side].rot, 6.f, 0.9f);
            drawAxes(dl, w(n.placed.hand[side].pos), yawQ * n.placed.hand[side].rot, 3.f, 0.6f);
        }
    }
}

[[nodiscard]] bool noteFresh()
{
    return st.noteFrame >= 0 && host_framecount - st.noteFrame <= 5 && st.note.ent == st.subject && st.subject > 0;
}

// The aim target: a ring and a cross, a line to it from the right weapon as drawn; picked by a click on it.
void drawAim(ImDrawList* dl)
{
    if(!st.aim.show)
    {
        return;
    }
    glm::vec2 s;
    if(!project(st.aim.at, s))
    {
        return;
    }
    const glm::vec4 c = st.aim.selected ? glm::vec4{1.f, 0.9f, 0.3f, 1.f} : glm::vec4{1.f, 0.55f, 0.1f, 0.95f};
    dl->AddCircle(iv(s), 9.f, col(c), 20, 2.f);
    dl->AddLine(ImVec2{s.x - 13.f, s.y}, ImVec2{s.x + 13.f, s.y}, col(c), 1.5f);
    dl->AddLine(ImVec2{s.x, s.y - 13.f}, ImVec2{s.x, s.y + 13.f}, col(c), 1.5f);
    dl->AddText(ImVec2{s.x + 12.f, s.y + 4.f}, col(c), st.aim.on ? (st.aim.atCamera ? "aim (camera)" : "aim") : "aim (off)");
    if(st.aim.on && noteFresh() && st.note.placed.on[0])
    {
        const glm::vec3 gun = st.note.space.origin +
                              glm::angleAxis(st.note.space.yawDeg * deg, glm::vec3{0.f, 0.f, 1.f}) * (st.note.placed.weapon[0].pos * st.note.space.scale);
        glm::vec2 g;
        if(project(gun, g))
        {
            dl->AddLine(iv(g), iv(s), col(glm::vec4{1.f, 0.55f, 0.1f, 0.35f}), 1.f);
        }
    }
}

[[nodiscard]] bool pickAim(const glm::vec2& m)
{
    glm::vec2 s;
    return st.aim.show && project(st.aim.at, s) && glm::length(s - m) < 11.f;
}

constexpr const char* posePartNames[] = {"right weapon", "left weapon", "right fist", "left hand", "right elbow", "left elbow",
    "torso", "pelvis", "planted foot", "kneeling knee", "kneeling foot", "planted knee"};
constexpr int posePartCount = static_cast<int>(sizeof(posePartNames) / sizeof(posePartNames[0]));

// The Pose Keys tool's parts: each a dot (the picked one's handle drawn), an elbow's and a knee's pole as a line.
void drawPoseParts(ImDrawList* dl, const glm::vec2& m, bool overUi)
{
    if(st.tool != Tool::PoseKeys)
    {
        return;
    }
    if(!noteFresh())
    {
        dl->AddText(ImVec2{st.view.x + 12.f, st.view.y + st.view.h - 40.f}, col(glm::vec4{1.f, 0.6f, 0.4f, 1.f}),
            "Pose Keys: the subject's arms not solved (an armed humanoid, the Arms layer on)");
        return;
    }
    const bool match = st.pdKind == st.subjKind && st.pdClass == st.subjClass;
    const glm::quat body = bodyTurn();
    for(int p = 0; p < posePartCount; p++)
    {
        rigpose::Rigid f;
        glm::vec2 s;
        if(!posePart(p, f) || !project(f.pos, s))
        {
            continue;
        }
        const bool picked = st.pdPart == p, hover = !overUi && glm::length(s - m) < 9.f;
        const glm::vec4 c = picked ? glm::vec4{1.f, 0.9f, 0.3f, 1.f} : hover ? glm::vec4{1.f, 1.f, 1.f, 1.f} : glm::vec4{0.3f, 0.9f, 1.f, 0.9f};
        dl->AddCircleFilled(iv(s), picked ? 4.f : 5.f, col(c), 12);
        dl->AddCircle(iv(s), 7.f, col(c), 12, 1.5f);
        if(p == 4 || p == 5)
        {
            // (the elbow's pole as sent: its way, from the elbow; the chest's space)
            const glm::vec3 pole = st.note.elbowPole[p - 4];
            glm::vec2 e;
            if(glm::length(pole) > 1e-3f &&
                project(f.pos + body * (glm::normalize(st.note.placed.chest.rot) * glm::normalize(pole)) * 8.f, e))
            {
                dl->AddLine(iv(s), iv(e), col(glm::vec4{c.x, c.y, c.z, 0.7f}), 1.5f);
            }
        }
        if(hover || picked)
        {
            dl->AddText(ImVec2{s.x + 9.f, s.y - 16.f}, col(c), posePartNames[p]);
        }
    }
    if(!match)
    {
        dl->AddText(ImVec2{st.view.x + 12.f, st.view.y + st.view.h - 40.f}, col(glm::vec4{1.f, 0.6f, 0.4f, 1.f}),
            "Pose Keys: the window's kind or class is not the subject's (its parts are drawn as it stands)");
    }
}

[[nodiscard]] int pickPosePart(const glm::vec2& m)
{
    int best = -1;
    float bestD = 9.f;
    for(int p = 0; p < posePartCount && st.tool == Tool::PoseKeys && noteFresh(); p++)
    {
        rigpose::Rigid f;
        glm::vec2 s;
        if(posePart(p, f) && project(f.pos, s) && glm::length(s - m) < bestD)
        {
            bestD = glm::length(s - m);
            best = p;
        }
    }
    return best;
}

void drawOnion(ImDrawList* dl)
{
    if(!st.onion || st.source == Source::Rest || st.active < 0)
    {
        return;
    }
    za::Vector<rigdata::Track>* tracks = curTracks(false);
    rigdata::Track* t = tracks ? trackOf(*tracks, st.active, false) : nullptr;
    if(!t || t->keys.empty())
    {
        return;
    }
    // The keys before and after the frame (onionSpan each way), ghosted.
    za::Vector<float> frames;
    int before = 0, after = 0;
    for(za::SizeT i = t->keys.size(); i-- > 0;)
    {
        if(t->keys[i].frame < st.frame - 0.01f && before < st.onionSpan)
        {
            frames.pushBack(t->keys[i].frame);
            before++;
        }
    }
    for(const rigdata::Key& k : t->keys)
    {
        if(k.frame > st.frame + 0.01f && after < st.onionSpan)
        {
            frames.pushBack(k.frame);
            after++;
        }
    }
    for(const float f : frames)
    {
        rigpose::Rigid F[maxBones], P[maxBones];
        evaluate(f, F, P, false);
        drawBones(dl, F, f < st.frame ? 0.45f : 0.3f, true);
    }
    evaluate(st.frame, st.F, st.P, true); // (the pivots and the bases as they were)
}

void cameraInput()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool overUi = io.WantCaptureMouse;
    const glm::vec2 d{io.MouseDelta.x, io.MouseDelta.y};
    const float orbit = vr_rigstudio_orbit_speed.value;
    const bool mmb = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    const bool altL = io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if(!overUi || st.cam.look)
    {
        if((mmb && io.KeyShift) || (altL && io.KeyShift))
        {
            const float scale = st.cam.dist * 2.f * st.view.tanY / za::max(st.view.h, 1.f) * vr_rigstudio_pan_speed.value;
            st.cam.focus += (-st.view.right * d.x + st.view.up * d.y) * scale;
        }
        else if(mmb || altL)
        {
            st.cam.yaw -= d.x * orbit;
            st.cam.pitch = za::clamp(st.cam.pitch + d.y * orbit, -89.f, 89.f);
        }
        if(ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if(!st.cam.look && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !overUi)
            {
                st.cam.look = true;
            }
            if(st.cam.look)
            {
                // Looking about: the camera stays where it is, the focus goes round it.
                const glm::vec3 pos = st.cam.focus - st.view.fwd * st.cam.dist;
                st.cam.yaw -= d.x * orbit;
                st.cam.pitch = za::clamp(st.cam.pitch + d.y * orbit, -89.f, 89.f);
                const float y = st.cam.yaw * deg, p = st.cam.pitch * deg;
                const glm::vec3 fwd{za::cos(p) * za::cos(y), za::cos(p) * za::sin(y), -za::sin(p)};
                st.cam.focus = pos + fwd * st.cam.dist;
            }
        }
        else
        {
            st.cam.look = false;
        }
        if(!overUi && io.MouseWheel != 0.f)
        {
            st.cam.dist = za::clamp(st.cam.dist * (1.f - io.MouseWheel * vr_rigstudio_zoom_speed.value), 8.f, 4000.f);
        }
    }
}

void keyInput()
{
    ImGuiIO& io = ImGui::GetIO();
    if(io.WantTextInput)
    {
        return;
    }
    const bool c = io.KeyCtrl;
    if(c && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if(io.KeyShift)
        {
            redo();
        }
        else
        {
            undo();
        }
    }
    if(c && ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        redo();
    }
    if(c && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        save();
    }
    if(io.WantCaptureKeyboard)
    {
        return;
    }
    if(ImGui::IsKeyPressed(ImGuiKey_F, false))
    {
        frameSelection();
    }
    const ImGuiKey views[3][2] = {{ImGuiKey_Keypad1, ImGuiKey_1}, {ImGuiKey_Keypad3, ImGuiKey_3}, {ImGuiKey_Keypad7, ImGuiKey_7}};
    const int which[3] = {1, 3, 7};
    for(int i = 0; i < 3; i++)
    {
        if(ImGui::IsKeyPressed(views[i][0], false) || ImGui::IsKeyPressed(views[i][1], false))
        {
            snapView(which[i], c);
        }
    }
    if(!c)
    {
        if(ImGui::IsKeyPressed(ImGuiKey_Q, false)) { st.tool = Tool::Select; }
        if(ImGui::IsKeyPressed(ImGuiKey_R, false)) { st.tool = Tool::Rotate; }
        if(ImGui::IsKeyPressed(ImGuiKey_G, false)) { st.tool = Tool::Move; }
        if(ImGui::IsKeyPressed(ImGuiKey_E, false)) { st.tool = Tool::IK; }
        if(ImGui::IsKeyPressed(ImGuiKey_T, false)) { st.tool = Tool::Rest; }
        if(ImGui::IsKeyPressed(ImGuiKey_W, false)) { st.tool = Tool::Weights; }
        if(ImGui::IsKeyPressed(ImGuiKey_P, false)) { st.tool = Tool::Paint; }
        if(ImGui::IsKeyPressed(ImGuiKey_I, false)) { pushUndo("key"); keySelected(); publish(); }
        if(ImGui::IsKeyPressed(ImGuiKey_Space, false)) { st.playing = !st.playing; }
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) { setFrame(za::floor(st.frame + 0.5f) - 1.f); }
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { setFrame(za::floor(st.frame + 0.5f) + 1.f); }
        if(ImGui::IsKeyPressed(ImGuiKey_H, false))
        {
            if(io.KeyAlt)
            {
                st.hidden = 0;
                st.solo = -1;
            }
            else
            {
                st.hidden |= st.selBones;
            }
        }
        if(ImGui::IsKeyPressed(ImGuiKey_O, false)) { st.tool = Tool::PoseKeys; }
        if(ImGui::IsKeyPressed(ImGuiKey_Escape, false) && st.dragging)
        {
            if(st.dragKind == 11 || st.dragKind == 12)
            {
                posestudio::deskEnd(false); // (its preview dropped; the key as it was, the undo's entry)
            }
            st.dragging = false;
            st.dragKind = 0;
            undo(); // (the drag's snapshot: back to before it)
        }
    }
}

} // namespace

namespace qvr::rigstudio::detail
{

glm::quat bodyTurn()
{
    if(noteFresh())
    {
        return glm::angleAxis(st.note.space.yawDeg * deg, glm::vec3{0.f, 0.f, 1.f});
    }
    const entity_t* e = subjectEntity();
    return glm::angleAxis((e ? e->angles[1] : 0.f) * deg, glm::vec3{0.f, 0.f, 1.f});
}

bool posePart(int part, rigpose::Rigid& out)
{
    if(part < 0 || part >= posePartCount || !noteFresh() || !posestudio::deskPart(st.note, part, st.pdPose, out))
    {
        return false;
    }
    if(part == 6)
    {
        out.pos = posestudio::deskWaist(st.note); // (the torso turns about its waist: its rings there)
    }
    return true;
}

bool mockDrag(int owner, int part, int axis, float amount)
{
    if(owner == 1)
    {
        st.aim.selected = true;
        st.aim.show = true;
    }
    else
    {
        st.aim.selected = false;
        st.pdPart = part;
    }
    placeHandle();
    if(!vs.h.shown || vs.h.owner != owner || (axis >= 20 ? !vs.h.rot : !vs.h.move))
    {
        return false;
    }
    glm::vec2 c, p0;
    project(vs.h.centre, c);
    za::Vector<glm::vec2> path;
    if(axis < 3)
    {
        glm::vec2 e;
        project(vs.h.centre + vs.h.axis[axis] * vs.h.radius, e);
        p0 = c + (e - c) * 0.6f;
        const glm::vec2 p1 = p0 + (e - c) * (amount / vs.h.radius);
        for(int i = 1; i <= 8; i++)
        {
            path.pushBack(p0 + (p1 - p0) * (static_cast<float>(i) / 8.f));
        }
    }
    else
    {
        // (a ring: from its point farthest out on the screen, swept round the centre by the angle asked: the drag turns by
        // the sweep, its sign by the ring's facing)
        glm::vec2 pts[ringN];
        bool front[ringN];
        ringPoints(vs.h.axis[axis - 20], pts, front, ringN);
        int best = -1;
        for(int k = 0; k < ringN; k++)
        {
            if(front[k] && front[(k + 1) % ringN] && (best < 0 || glm::length(pts[k] - c) > glm::length(pts[best] - c)))
            {
                best = k;
            }
        }
        if(best < 0)
        {
            return false;
        }
        p0 = pts[best];
        const float sign = glm::dot(vs.h.axis[axis - 20], st.view.fwd) < 0.f ? 1.f : -1.f;
        const float sweep = amount * deg * sign;
        const glm::vec2 r0{p0.x - c.x, -(p0.y - c.y)};
        const int steps = za::max(2, static_cast<int>(za::abs(amount) / 6.f) + 1);
        for(int i = 1; i <= steps; i++)
        {
            const float t = sweep * static_cast<float>(i) / static_cast<float>(steps);
            const glm::vec2 r{r0.x * za::cos(t) - r0.y * za::sin(t), r0.x * za::sin(t) + r0.y * za::cos(t)};
            path.pushBack(glm::vec2{c.x + r.x, c.y - r.y});
        }
    }
    vs.mock.clear();
    vs.mockNext = 0;
    vs.mock.pushBack(MockStep{p0, false}); // (hovered)
    vs.mock.pushBack(MockStep{p0, true});  // (pressed: the drag begins)
    for(const glm::vec2& p : path)
    {
        vs.mock.pushBack(MockStep{p, true});
    }
    vs.mock.pushBack(MockStep{path.back(), true});
    vs.mock.pushBack(MockStep{path.back(), false}); // (let go)
    vs.mock.pushBack(MockStep{path.back() + glm::vec2{0.f, 400.f}, false}); // (and away)
    return true;
}

void mockFeed()
{
    if(vs.mockNext >= static_cast<int>(vs.mock.size()))
    {
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    const MockStep& s = vs.mock[static_cast<za::SizeT>(vs.mockNext)];
    io.AddMousePosEvent(s.at.x, s.at.y);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, s.down);
    vs.mockNext++;
}

bool mockBusy()
{
    return vs.mockNext < static_cast<int>(vs.mock.size());
}

glm::vec3 toWorld(const glm::vec3& p)
{
    const entity_t* e = subjectEntity();
    if(!e)
    {
        return p;
    }
    const float y = e->angles[1] * deg;
    const float s = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    const glm::vec3 q = p * s;
    return glm::vec3{e->origin[0] + za::cos(y) * q.x - za::sin(y) * q.y, e->origin[1] + za::sin(y) * q.x + za::cos(y) * q.y,
        e->origin[2] + q.z};
}

glm::vec3 toModelDir(const glm::vec3& d)
{
    const entity_t* e = subjectEntity();
    if(!e)
    {
        return d;
    }
    const float y = -e->angles[1] * deg;
    const float s = za::max(ENTSCALE_DECODE(e->scale), 0.01f);
    return glm::vec3{za::cos(y) * d.x - za::sin(y) * d.y, za::sin(y) * d.x + za::cos(y) * d.y, d.z} / s;
}

bool project(const glm::vec3& world, glm::vec2& out, float* depth)
{
    const View& v = st.view;
    const glm::vec3 d = world - v.org;
    const float z = glm::dot(d, v.fwd);
    if(depth)
    {
        *depth = z;
    }
    if(z < 0.5f)
    {
        out = glm::vec2{-10000.f};
        return false;
    }
    const float x = glm::dot(d, v.right) / z / v.tanX;
    const float y = glm::dot(d, v.up) / z / v.tanY;
    out = glm::vec2{v.x + (x * 0.5f + 0.5f) * v.w, v.y + (0.5f - y * 0.5f) * v.h};
    return true;
}

void cameraView(float* origin, float* angles)
{
    const float y = st.cam.yaw * deg, p = st.cam.pitch * deg;
    const glm::vec3 fwd{za::cos(p) * za::cos(y), za::cos(p) * za::sin(y), -za::sin(p)};
    const glm::vec3 org = st.cam.focus - fwd * st.cam.dist;
    for(int i = 0; i < 3; i++)
    {
        origin[i] = org[i];
    }
    angles[0] = st.cam.pitch;
    angles[1] = st.cam.yaw;
    angles[2] = 0.f;
}

void snapView(int which, bool flip)
{
    const entity_t* e = subjectEntity();
    const float yaw = e ? e->angles[1] : st.stageYaw;
    switch(which)
    {
    case 1: // front (Ctrl: back)
        st.cam.yaw = yaw + (flip ? 0.f : 180.f);
        st.cam.pitch = 0.f;
        break;
    case 3: // its right side (Ctrl: its left)
        st.cam.yaw = yaw + (flip ? -90.f : 90.f);
        st.cam.pitch = 0.f;
        break;
    case 7: // top (Ctrl: bottom)
        st.cam.pitch = flip ? -89.f : 89.f;
        break;
    default: break;
    }
}

void frameSelection()
{
    glm::vec3 lo{1e30f}, hi{-1e30f};
    int n = 0;
    const auto add = [&](const glm::vec3& p) {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
        n++;
    };
    for(int b = 0; b < st.numBones; b++)
    {
        if(st.selBones & (1u << b))
        {
            add(toWorld(rigpose::apply(st.F[b], st.pivots[b])));
            add(toWorld(rigpose::apply(st.F[b], st.ends[b])));
        }
    }
    for(za::SizeT v = 0; v < st.selVerts.size() && v < st.worldVerts.size(); v++)
    {
        if(st.selVerts[v])
        {
            add(st.worldVerts[v]);
        }
    }
    if(n == 0)
    {
        for(const glm::vec3& p : st.worldVerts)
        {
            add(p);
        }
    }
    if(n == 0)
    {
        return;
    }
    st.cam.focus = (lo + hi) * 0.5f;
    const float r = za::max(glm::length(hi - lo) * 0.5f, 4.f);
    st.cam.dist = za::clamp(r / za::max(st.view.tanY, 0.1f) * 2.f, 30.f, 4000.f);
}

void buildGeometry()
{
    if(st.geomFor)
    {
        return;
    }
    const aliashdr_t* hdr = hdrOf();
    if(!hdr || !st.rig)
    {
        return;
    }
    const int nv = hdr->numverts;
    st.restVerts.resize(static_cast<za::SizeT>(nv));
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    for(int v = 0; v < nv; v++)
    {
        const trivertx_t& t = base[v];
        st.restVerts[static_cast<za::SizeT>(v)] = glm::vec3{t.v[0], t.v[1], t.v[2]} * glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]} +
                                                  glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    }
    st.tris.clear();
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            st.tris.pushBack(za::clamp(static_cast<int>(desc[idx[i + k]].vertindex), 0, nv - 1));
        }
    }
    st.nbrs.clear();
    st.nbrs.resize(static_cast<za::SizeT>(nv));
    st.edges.clear();
    for(za::SizeT t = 0; t + 2 < st.tris.size(); t += 3)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = st.tris[t + static_cast<za::SizeT>(k)], b = st.tris[t + static_cast<za::SizeT>((k + 1) % 3)];
            if(a == b)
            {
                continue;
            }
            bool seen = false;
            for(const int n : st.nbrs[static_cast<za::SizeT>(a)])
            {
                seen = seen || n == b;
            }
            if(!seen)
            {
                st.nbrs[static_cast<za::SizeT>(a)].pushBack(b);
                st.nbrs[static_cast<za::SizeT>(b)].pushBack(a);
                st.edges.pushBack(a);
                st.edges.pushBack(b);
            }
        }
    }
    // The triangles' winding: outward if their normals point away from the middle, mostly.
    glm::vec3 mid{0.f};
    for(const glm::vec3& p : st.restVerts)
    {
        mid += p;
    }
    mid /= static_cast<float>(za::max(nv, 1));
    float out = 0.f;
    for(za::SizeT t = 0; t + 2 < st.tris.size(); t += 3)
    {
        const glm::vec3& a = st.restVerts[static_cast<za::SizeT>(st.tris[t])];
        const glm::vec3& b = st.restVerts[static_cast<za::SizeT>(st.tris[t + 1])];
        const glm::vec3& c = st.restVerts[static_cast<za::SizeT>(st.tris[t + 2])];
        out += glm::dot(glm::cross(b - a, c - a), (a + b + c) / 3.f - mid);
    }
    vs.windingFlip = out < 0.f;
    st.geomFor = true;
}

void poseGeometry()
{
    const aliashdr_t* hdr = hdrOf();
    if(!hdr || !st.rig || st.restVerts.empty())
    {
        return;
    }
    ensureWeights();
    if(romGeometry())
    {
        return; // (Range of Motion: the authored rig's skinned mesh)
    }
    const int nv = static_cast<int>(st.restVerts.size());
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const glm::vec3 scale{hdr->scale[0], hdr->scale[1], hdr->scale[2]}, origin{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    st.baseVerts.resize(static_cast<za::SizeT>(nv));
    st.posedVerts.resize(static_cast<za::SizeT>(nv));
    st.worldVerts.resize(static_cast<za::SizeT>(nv));
    st.screen.resize(static_cast<za::SizeT>(nv));
    st.depth.resize(static_cast<za::SizeT>(nv));
    st.facing.resize(static_cast<za::SizeT>(nv));
    st.normals.resize(static_cast<za::SizeT>(nv));
    rigpose::Rigid K[maxBones];
    for(int b = 0; b < st.numBones; b++)
    {
        K[b] = rigdata::compose(st.F[b], rigdata::inverse(st.P[b]));
    }
    const za::SizeT o1 = static_cast<za::SizeT>(st.basePose1) * static_cast<za::SizeT>(nv);
    const za::SizeT o2 = static_cast<za::SizeT>(st.basePose2) * static_cast<za::SizeT>(nv);
    for(int v = 0; v < nv; v++)
    {
        const trivertx_t& a = base[o1 + static_cast<za::SizeT>(v)];
        const trivertx_t& c = base[o2 + static_cast<za::SizeT>(v)];
        const glm::vec3 pa = glm::vec3{a.v[0], a.v[1], a.v[2]} * scale + origin;
        const glm::vec3 pc = glm::vec3{c.v[0], c.v[1], c.v[2]} * scale + origin;
        const glm::vec3 p = pa + (pc - pa) * st.baseBlend;
        st.baseVerts[static_cast<za::SizeT>(v)] = p;
        const rigdata::VertWeights& w = st.weights[static_cast<za::SizeT>(v)];
        glm::vec3 q{0.f};
        float sum = 0.f;
        for(int i = 0; i < w.n; i++)
        {
            q += rigpose::apply(K[w.bone[i]], p) * w.w[i];
            sum += w.w[i];
        }
        st.posedVerts[static_cast<za::SizeT>(v)] = sum > 1e-6f ? q / sum : p;
    }
    finishGeometry();
}

void finishGeometry()
{
    const int nv = static_cast<int>(st.posedVerts.size());
    for(int v = 0; v < nv; v++)
    {
        st.worldVerts[static_cast<za::SizeT>(v)] = toWorld(st.posedVerts[static_cast<za::SizeT>(v)]);
        st.normals[static_cast<za::SizeT>(v)] = glm::vec3{0.f};
    }
    for(za::SizeT t = 0; t + 2 < st.tris.size(); t += 3)
    {
        const int a = st.tris[t], b = st.tris[t + 1], c = st.tris[t + 2];
        glm::vec3 n = glm::cross(st.worldVerts[static_cast<za::SizeT>(b)] - st.worldVerts[static_cast<za::SizeT>(a)],
            st.worldVerts[static_cast<za::SizeT>(c)] - st.worldVerts[static_cast<za::SizeT>(a)]);
        n = vs.windingFlip ? -n : n;
        st.normals[static_cast<za::SizeT>(a)] += n;
        st.normals[static_cast<za::SizeT>(b)] += n;
        st.normals[static_cast<za::SizeT>(c)] += n;
    }
    for(int v = 0; v < nv; v++)
    {
        const za::SizeT i = static_cast<za::SizeT>(v);
        float d = 0.f;
        project(st.worldVerts[i], st.screen[i], &d);
        st.depth[i] = d;
        st.facing[i] = glm::dot(st.normals[i], st.view.org - st.worldVerts[i]) > 0.f ? 1 : 0;
    }
}

void viewport()
{
    ImGuiIO& io = ImGui::GetIO();
    // The view as the camera has it now (the fov the renderer drew with).
    {
        View& v = st.view;
        const float y = st.cam.yaw * deg, p = st.cam.pitch * deg;
        v.fwd = glm::vec3{za::cos(p) * za::cos(y), za::cos(p) * za::sin(y), -za::sin(p)};
        v.org = st.cam.focus - v.fwd * st.cam.dist;
        v.right = glm::normalize(glm::cross(v.fwd, glm::vec3{0.f, 0.f, 1.f}));
        if(!(glm::length(v.right) > 0.5f))
        {
            v.right = glm::vec3{za::sin(y), -za::cos(y), 0.f};
        }
        v.up = glm::cross(v.right, v.fwd);
        const float fx = r_refdef.fov_x > 1.f ? r_refdef.fov_x : 90.f;
        const float fy = r_refdef.fov_y > 1.f ? r_refdef.fov_y : 73.7f;
        v.tanX = za::tan(fx * 0.5f * deg);
        v.tanY = za::tan(fy * 0.5f * deg);
        v.x = 0.f;
        v.y = 0.f;
        v.w = io.DisplaySize.x;
        v.h = io.DisplaySize.y;
    }
    cameraInput();
    keyInput();
    if(st.aim.atCamera && !(st.dragging && st.dragKind == 10))
    {
        st.aim.at = st.view.org; // (Aim at Camera: the subject aims at the studio's eye)
    }
    if(!st.rig || st.restVerts.empty())
    {
        return;
    }
    poseGeometry(); // (with this frame's view)
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const glm::vec2 m{io.MousePos.x, io.MousePos.y};
    const bool overUi = io.WantCaptureMouse;
    placeHandle();
    // Hover
    vs.hoverPart = st.dragging || overUi ? vs.hoverPart : hitHandle(m);
    st.hoverBone = overUi || st.dragging ? -1 : pickBone(m);
    // Clicks
    if(!overUi && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const int posePick = vs.hoverPart < 0 ? pickPosePart(m) : -1;
        if(vs.hoverPart < 0 && romClick(m))
        {
            // (Range of Motion: a joint picked, or its limb's drag begun)
        }
        else if(vs.hoverPart >= 0 && vs.h.owner == 1)
        {
            beginDrag(10, vs.hoverPart, m);
        }
        else if(vs.hoverPart >= 0 && vs.h.owner == 2)
        {
            if(st.pdKind != st.subjKind || st.pdClass != st.subjClass)
            {
                say("the Pose Studio Data's kind or class is not the subject's: Follow the Subject, or pick its own");
            }
            else
            {
                beginDrag(vs.hoverPart >= 20 ? 12 : 11, vs.hoverPart, m);
            }
        }
        else if(vs.hoverPart < 0 && pickAim(m))
        {
            st.aim.selected = true; // (its handle: the move tool's, in the world's axes)
        }
        else if(posePick >= 0)
        {
            st.pdPart = posePick;
            st.aim.selected = false;
        }
        else if(vs.hoverPart >= 0 && (st.tool == Tool::Rotate || st.tool == Tool::Move || st.tool == Tool::Rest))
        {
            const int kind = st.tool == Tool::Rotate ? 1 : st.tool == Tool::Move ? 2 : (vs.restPoint == 11 ? 5 : 4);
            beginDrag(kind, vs.hoverPart, m);
        }
        else if(st.tool == Tool::IK && pickIkEnd(m) >= 0)
        {
            st.ikChain = pickIkEnd(m);
            st.ikTarget = toWorld(rigpose::apply(st.F[st.ikChain], st.ends[st.ikChain]));
            vs.h.centre = st.ikTarget;
            beginDrag(3, 3, m);
            (void)rayPlane(m, st.ikTarget, st.view.fwd, vs.planeStart);
        }
        else if(st.tool == Tool::Weights)
        {
            const int v = pickVert(m);
            if(v >= 0)
            {
                uint8_t& s = st.selVerts[static_cast<za::SizeT>(v)];
                if(io.KeyCtrl)
                {
                    s = 0;
                }
                else if(io.KeyShift)
                {
                    s = 1;
                }
                else
                {
                    za::fill(st.selVerts, static_cast<uint8_t>(0));
                    s = 1;
                }
            }
            else
            {
                st.dragging = true;
                st.dragKind = 6;
                st.dragStart = m;
            }
        }
        else if(st.tool == Tool::Paint)
        {
            beginDrag(7, -1, m);
        }
        else
        {
            st.aim.selected = false;
            const int b = pickBone(m);
            if(b >= 0)
            {
                if(io.KeyShift && (st.selBones & (1u << b)) && st.active == b)
                {
                    st.selBones &= ~(1u << b);
                    st.active = -1;
                }
                else
                {
                    selectBone(b, io.KeyShift);
                }
            }
            else if(!io.KeyShift)
            {
                selectBone(-1, false);
            }
        }
    }
    if(st.tool == Tool::Rest && !overUi && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && st.active >= 0 && vs.hoverPart < 0)
    {
        // (the rest tool: a click on its bone's end or pivot dot picks which point the handle moves)
        glm::vec2 sp, se;
        project(toWorld(rigpose::apply(st.F[st.active], st.pivots[st.active])), sp);
        project(toWorld(rigpose::apply(st.F[st.active], st.ends[st.active])), se);
        if(glm::length(m - se) < 8.f)
        {
            vs.restPoint = 11;
        }
        else if(glm::length(m - sp) < 8.f)
        {
            vs.restPoint = 10;
        }
    }
    // Drags
    romDragTick(m);
    if(st.dragging)
    {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            switch(st.dragKind)
            {
            case 1: dragRotate(m); break;
            case 2: dragMove(m, false); break;
            case 4:
            case 5: dragMove(m, true); break;
            case 3:
            {
                glm::vec3 hit;
                if(rayPlane(m, st.ikTarget, st.view.fwd, hit))
                {
                    const glm::vec3 target = st.ikTarget + (hit - vs.planeStart);
                    const entity_t* e = subjectEntity();
                    glm::vec3 local = target;
                    if(e)
                    {
                        local = toModelDir(target - glm::vec3{e->origin[0], e->origin[1], e->origin[2]});
                    }
                    solveIk(st.ikChain, local);
                }
                break;
            }
            case 6:
            {
                dl->AddRectFilled(iv(glm::min(st.dragStart, m)), iv(glm::max(st.dragStart, m)), col(glm::vec4{0.3f, 0.6f, 1.f, 0.15f}));
                dl->AddRect(iv(glm::min(st.dragStart, m)), iv(glm::max(st.dragStart, m)), col(glm::vec4{0.4f, 0.7f, 1.f, 0.9f}));
                break;
            }
            case 7: paint(m); break;
            case 10: dragAim(m); break;
            case 11:
            case 12: dragDesk(m); break;
            default: break;
            }
        }
        else
        {
            if(st.dragKind == 6)
            {
                boxSelect(st.dragStart, m, io.KeyShift, io.KeyCtrl);
                st.dragging = false;
                st.dragKind = 0;
            }
            else
            {
                endDrag();
            }
        }
    }
    // Drawing: the mesh, the bones, the ghosts, the limits, the proxies, the targets, the handle, the brush, the names.
    evaluate(st.frame, st.F, st.P, true);
    drawMesh(dl);
    drawOnion(dl);
    if(romHidesOldBones())
    {
        romDraw(dl); // (Range of Motion: the authored rig's bones and its joint's limits)
    }
    else
    {
        drawBones(dl, st.F, 1.f, false);
        drawLimits(dl);
    }
    drawProxies(dl);
    drawTargets(dl);
    drawPoseParts(dl, m, overUi);
    drawAim(dl);
    placeHandle();
    drawHandle(dl);
    if(st.tool == Tool::Paint && !overUi)
    {
        dl->AddCircle(io.MousePos, st.brushRadius, col(glm::vec4{1.f, 1.f, 1.f, 0.8f}), 40, 1.5f);
    }
    if(st.names && st.hoverBone >= 0 && !overUi)
    {
        ImGui::SetTooltip("%s", st.rig->bones[st.hoverBone].name);
    }
    if(st.tool == Tool::Weights || st.tool == Tool::Paint)
    {
        const int hv = overUi ? -1 : pickVert(m);
        st.hoverVert = hv;
        if(hv >= 0)
        {
            const rigdata::VertWeights& w = st.weights[static_cast<za::SizeT>(hv)];
            char text[160];
            int n = q_snprintf(text, sizeof(text), "vertex %d:", hv);
            for(int i = 0; i < w.n && n < static_cast<int>(sizeof(text)) - 24; i++)
            {
                n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), " %s %.2f", st.rig->bones[w.bone[i]].name, w.w[i]);
            }
            ImGui::SetTooltip("%s", text);
        }
    }
}

} // namespace qvr::rigstudio::detail
