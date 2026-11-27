// vr_armourstudio_gizmo.cpp -- the Armour Studio window's handle on the part picked (vr_armourstudio.hpp, "gizmo";
// docs/vr-port/ARMOUR_STUDIO.md, "The handle"): the Rig Studio's transform handle (vr_gizmo.hpp, shared) drawn over the
// game's own view at the part's middle, in its own frame (the subject's bone as posed, turned by the part's rotate).
//
// Its modes (vr_armourstudio_gizmo; the window's Handle row; keys Q W E R): 1 move (arrows along the part's axes, planes,
// the centre in the view's plane), 2 rotate (rings about its axes), 3 scale (arms with box ends: Size Fwd / Left / Up;
// the centre: Scale, by the mouse's move across). A drag writes the rows' own cvars (offset, rotate, scale, scale_x-_z),
// so the studio lays them into the table as it does a row's change; the whole drag is one undo step (gizmoEdit). Ctrl
// snaps to the rows' steps (0.1 units, 5 degrees, 0.02x).
//
// Flat screen only, as the window is. In a headset the menu page's sliders are the way (no hand grab of the part: not
// built).

#include "vr_armourstudio.hpp"
#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"
#include "vr_armourdata.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_geom.hpp"
#include "vr_gizmo.hpp"
#include "vr_imgui.hpp"
#include "vr_testkit.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

extern "C" int VR_IsActive(void);

namespace qvr::armourstudio
{

namespace
{

using namespace armourdata;
using geom::deg;

constexpr float scaleLo = 0.2f, scaleHi = 3.f; // (scale_x's range in armour.txt; the rows' typed entry goes as far)
constexpr float scalePixels = 150.f;           // the centre's scale drag: this many pixels across doubles it
constexpr float snapMove = 0.1f, snapTurn = 5.f, snapScale = 0.02f; // Ctrl: the rows' steps

enum Mode
{
    ModeOff,
    ModeMove,
    ModeRotate,
    ModeScale,
    NumModes
};
constexpr const char* modeLabels[NumModes] = {"Off (Q)", "Move (W)", "Rotate (E)", "Scale (R)"};

cvar_t* const offsetCvars[3] = {&vr_armourstudio_offset_x, &vr_armourstudio_offset_y, &vr_armourstudio_offset_z};
cvar_t* const turnCvars[3] = {&vr_armourstudio_pitch, &vr_armourstudio_yaw, &vr_armourstudio_roll};
cvar_t* const sizeCvars[3] = {&vr_armourstudio_scale_x, &vr_armourstudio_scale_y, &vr_armourstudio_scale_z};

// A step of the test's mock mouse: where, whether its left button is down.
struct MockStep
{
    glm::vec2 at{0.f};
    bool down{false};
};

// The handle between frames, and its drag.
struct Gizmo
{
    gizmo::Handle h;
    gizmo::Projection view;
    bool found{false};     // the part found (its handle placed, on the screen or not)
    glm::mat3 parent{1.f}; // the part's tweaks' space -> world (agarmor::studioPartFrame)
    glm::mat3 turn{1.f};   // its rotate (the tweaks' turn, in that space)
    int mode{ModeOff};
    int hover{-1};
    // The drag: its part, its mode, where it began and was last, the handle and the values as it began
    bool dragging{false};
    int part{-1};
    int dragMode{ModeOff};
    int dragSubject{-1}, dragPart{-1}; // (the kind and part it began on)
    glm::vec2 start{0.f}, last{0.f};
    gizmo::Handle h0;
    glm::mat3 parent0{1.f}, turn0{1.f};
    glm::vec3 offset0{0.f}, size0{1.f};
    float scale0{1.f};
    float sweep{0.f};
    glm::vec3 planeStart{0.f};
    // The test's mock mouse
    za::Vector<MockStep> mock;
    int mockNext{0};
};
Gizmo gz;

[[nodiscard]] glm::vec3 cvars3(cvar_t* const* c)
{
    return glm::vec3{c[0]->value, c[1]->value, c[2]->value};
}

void setCvars3(cvar_t* const* c, const glm::vec3& v)
{
    for(int i = 0; i < 3; i++)
    {
        if(c[i]->value != v[i])
        {
            Cvar_SetValueQuick(c[i], v[i]);
        }
    }
}

[[nodiscard]] float snapped(float v, float step)
{
    return za::floor(v / step + 0.5f) * step;
}

// The tweaks' turn's pitch, yaw and roll (degrees) back from its matrix (agarmor::detail::tweakTurn: yaw about up, pitch
// about left, roll about forward, yaw * pitch * roll).
[[nodiscard]] glm::vec3 anglesOf(const glm::mat3& m)
{
    const float sp = za::clamp(-m[0][2], -1.f, 1.f);
    const float pitch = za::atan2(sp, za::sqrt(za::max(0.f, 1.f - sp * sp)));
    const float yaw = za::atan2(m[0][1], m[0][0]);
    const float roll = za::atan2(m[1][2], m[2][2]);
    return glm::vec3{pitch, yaw, roll} / deg;
}

[[nodiscard]] int subjectNow()
{
    return za::clamp(static_cast<int>(vr_armourstudio_kind.value), 0, subjectCount() - 1);
}

[[nodiscard]] int partNow()
{
    return za::clamp(static_cast<int>(vr_armourstudio_part.value), 0, NumParts - 1);
}

// The mode's rows apply to the part picked (the handle edits nothing the window does not show).
[[nodiscard]] bool modeApplies(int mode)
{
    const int subject = subjectNow(), part = partNow();
    if(!partValid(subject, part) || sleevePart(part))
    {
        return false;
    }
    switch(mode)
    {
    case ModeMove:
        return fieldShown(subject, part, OffsetX);
    case ModeRotate:
        return fieldShown(subject, part, Pitch);
    case ModeScale:
        return fieldShown(subject, part, Scale) && fieldShown(subject, part, ScaleX);
    default:
        return false;
    }
}

// The handle at the part's middle, in its frame, for this frame's view.
void placeGizmo()
{
    gz.h.shown = false;
    gz.found = false;
    gz.mode = za::clamp(static_cast<int>(vr_armourstudio_gizmo.value), 0, NumModes - 1);
    glm::vec3 centre;
    if(gz.mode == ModeOff || !running() || !modeApplies(gz.mode) || !agarmor::studioPartFrame(centre, gz.parent))
    {
        return;
    }
    gz.found = true;
    gz.turn = agarmor::detail::tweakTurn(vr_armourstudio_pitch.value, vr_armourstudio_yaw.value, vr_armourstudio_roll.value);
    const glm::mat3 local = gz.parent * gz.turn;
    gz.h.centre = centre;
    for(int i = 0; i < 3; i++)
    {
        gz.h.axis[i] = geom::safeNormalize(local[i], glm::vec3{0.f}, 1e-6f);
    }
    gz.h.move = gz.mode == ModeMove;
    gz.h.rot = gz.mode == ModeRotate;
    gz.h.scale = gz.mode == ModeScale;
    gizmo::place(gz.h, gz.view, za::max(vr_armourstudio_gizmo_size.value, 16.f));
}

void beginDrag(int part, const glm::vec2& m)
{
    gz.dragging = true;
    gz.part = part;
    gz.dragMode = gz.mode;
    gz.dragSubject = subjectNow();
    gz.dragPart = partNow();
    gz.start = gz.last = m;
    gz.h0 = gz.h;
    gz.parent0 = gz.parent;
    gz.turn0 = gz.turn;
    gz.offset0 = cvars3(offsetCvars);
    gz.size0 = cvars3(sizeCvars);
    gz.scale0 = vr_armourstudio_scale.value;
    gz.sweep = 0.f;
    const glm::vec3 n = part >= 4 && part <= 6 ? gz.h.axis[part - 4] : gz.view.fwd;
    if(!gz.view.rayPlane(m, gz.h.centre, n, gz.planeStart))
    {
        gz.planeStart = gz.h.centre;
    }
    gizmoEdit(true);
}

void endDrag()
{
    if(gz.dragging)
    {
        gz.dragging = false;
        gizmoEdit(false);
    }
}

// Look at the Part (the window's button, vr_armourstudio_gizmo_look): the flat camera turned to the part picked, the part
// a quarter of the way across the screen (clear of the window on its right). False: no part placed yet.
bool lookAtPart()
{
    if(!gz.found)
    {
        return false;
    }
    const glm::vec3 d = gz.h.centre - gz.view.org;
    const float flat = za::sqrt(d.x * d.x + d.y * d.y);
    const float aside = za::atan2(0.5f * gz.view.tanX, 1.f) / deg;
    cl.viewangles[YAW] = za::atan2(d.y, d.x) / deg - aside;
    cl.viewangles[PITCH] = -za::atan2(d.z, za::max(flat, 1e-3f)) / deg;
    cl.viewangles[ROLL] = 0.f;
    return true;
}

// The handle in the left part of the screen (a test's drags there: the window is on the right).
[[nodiscard]] bool handleInView()
{
    glm::vec2 c;
    return gz.h.shown && gz.view.project(gz.h.centre, c) && c.x > 40.f && c.x < 0.4f * gz.view.w && c.y > 40.f &&
           c.y < gz.view.h - 40.f;
}

// The drag's move this frame: the values from where it began, by the mouse's move since.
void dragTo(const glm::vec2& m, bool snap)
{
    const gizmo::Handle& h = gz.h0;
    if(gz.dragMode == ModeMove)
    {
        glm::vec3 world{0.f};
        if(gz.part <= 2)
        {
            world = gizmo::axisMoveExact(h, gz.view, gz.part, gz.start, m);
        }
        else
        {
            const glm::vec3 n = gz.part >= 4 ? h.axis[gz.part - 4] : gz.view.fwd;
            glm::vec3 hit;
            if(!gz.view.rayPlane(m, h.centre, n, hit))
            {
                return;
            }
            world = hit - gz.planeStart;
        }
        glm::vec3 offset = gz.offset0 + glm::inverse(gz.parent0) * world;
        if(snap)
        {
            offset = glm::vec3{snapped(offset.x, snapMove), snapped(offset.y, snapMove), snapped(offset.z, snapMove)};
        }
        setCvars3(offsetCvars, offset);
    }
    else if(gz.dragMode == ModeRotate && gz.part <= 2)
    {
        gz.sweep += gizmo::ringSweepExact(h, gz.view, h.axis[gz.part], gz.last, m);
        const float a = snap ? snapped(gz.sweep / deg, snapTurn) * deg : gz.sweep;
        glm::vec3 e{0.f};
        e[gz.part] = 1.f;
        // (about the part's own axis: its turn, then this)
        const glm::mat3 now = gz.turn0 * glm::mat3_cast(glm::angleAxis(a, e));
        setCvars3(turnCvars, anglesOf(now));
    }
    else if(gz.dragMode == ModeScale)
    {
        if(gz.part <= 2)
        {
            const float along = glm::dot(gizmo::axisMoveExact(h, gz.view, gz.part, gz.start, m), h.axis[gz.part]);
            float v = gz.size0[gz.part] * za::max(0.05f, 1.f + along / h.radius);
            v = za::clamp(snap ? snapped(v, snapScale) : v, scaleLo, scaleHi);
            if(sizeCvars[gz.part]->value != v)
            {
                Cvar_SetValueQuick(sizeCvars[gz.part], v);
            }
        }
        else
        {
            float v = gz.scale0 * za::max(0.05f, 1.f + (m.x - gz.start.x) / scalePixels);
            v = za::clamp(snap ? snapped(v, snapScale) : v, scaleLo, scaleHi);
            if(vr_armourstudio_scale.value != v)
            {
                Cvar_SetValueQuick(&vr_armourstudio_scale, v);
            }
        }
    }
    gz.last = m;
}

// What the drag has made, by the mouse.
void dragLabel(ImDrawList* dl, const ImVec2& at)
{
    char text[96];
    if(gz.dragMode == ModeMove)
    {
        q_snprintf(text, sizeof(text), "Fwd %.2f  Left %.2f  Up %.2f", vr_armourstudio_offset_x.value, vr_armourstudio_offset_y.value,
            vr_armourstudio_offset_z.value);
    }
    else if(gz.dragMode == ModeRotate)
    {
        q_snprintf(text, sizeof(text), "Pitch %.1f  Yaw %.1f  Roll %.1f", vr_armourstudio_pitch.value, vr_armourstudio_yaw.value,
            vr_armourstudio_roll.value);
    }
    else
    {
        q_snprintf(text, sizeof(text), "Scale %.3f  Size %.3f / %.3f / %.3f", vr_armourstudio_scale.value, vr_armourstudio_scale_x.value,
            vr_armourstudio_scale_y.value, vr_armourstudio_scale_z.value);
    }
    dl->AddText(ImVec2{at.x + 14.f, at.y + 4.f}, IM_COL32(255, 255, 255, 255), text);
}

// ---- The test (vr_armourstudio_gizmo_test): a scripted drag in each mode changes its value by what it should ----

constexpr const char* testName = "armourgizmo";
constexpr const char* testFile = "data/armour_gizmotest.txt";
constexpr int testStepFrames = 600;  // frames a step may wait for its condition (a safety net: FAIL "timed out")
constexpr float testMove = 2.f;      // units along the part's arrow longest on the screen
constexpr float testTurn = 30.f;     // degrees round its ring facing the eye most
constexpr float testSize = 0.25f;    // of the arm's length out along its arm longest on the screen
constexpr float testScalePx = 75.f;  // pixels across from the centre
constexpr float testBeastMove = 1.5f; // units along a beast piece's arrow longest on the screen
constexpr int stillNeeded = 8;       // frames the handle must stay put before a drag (the subject landed)
constexpr int testBeastKind = 7;     // (the first beast: the rottweiler)

struct Test
{
    int step{0};
    int frames{0};
    testkit::Tally tally;
    char fileWas[64]{};
    int undoBefore{0};
    glm::vec3 expect{0.f}; // (the values the drag should make)
    int arm{0};            // (the arm a move drags)
    glm::vec3 lastCentre{0.f}, lastFwd{0.f};
    int stillFrames{0}; // (frames the handle has not moved: a subject just put down settles first)
};
Test test;

[[nodiscard]] bool mockDone()
{
    return gz.mockNext >= static_cast<int>(gz.mock.size()) && !gz.dragging;
}

void mockPath(const glm::vec2& from, const za::Vector<glm::vec2>& path)
{
    gz.mock.clear();
    gz.mockNext = 0;
    gz.mock.pushBack(MockStep{from, false}); // (hovered)
    gz.mock.pushBack(MockStep{from, true});  // (pressed: the drag begins)
    for(const glm::vec2& p : path)
    {
        gz.mock.pushBack(MockStep{p, true});
    }
    gz.mock.pushBack(MockStep{path.back(), true});
    gz.mock.pushBack(MockStep{path.back(), false});                         // (let go)
    gz.mock.pushBack(MockStep{path.back() + glm::vec2{0.f, 300.f}, false}); // (and away)
}

void mockLine(const glm::vec2& from, const glm::vec2& to)
{
    za::Vector<glm::vec2> path;
    for(int i = 1; i <= 8; i++)
    {
        path.pushBack(from + (to - from) * (static_cast<float>(i) / 8.f));
    }
    mockPath(from, path);
}

// The arm longest on the screen (one pointing at the eye is a dot: its drag is the centre's).
[[nodiscard]] int longestArm()
{
    int best = 0;
    float bestLen = -1.f;
    glm::vec2 c, e;
    (void)gz.view.project(gz.h.centre, c);
    for(int i = 0; i < 3; i++)
    {
        (void)gz.view.project(gz.h.centre + gz.h.axis[i] * gz.h.radius, e);
        if(glm::length(e - c) > bestLen)
        {
            bestLen = glm::length(e - c);
            best = i;
        }
    }
    return best;
}

// A drag along arm `axis` by `amount` world units (from 0.6 of the way out on it).
bool mockArm(int axis, float amount)
{
    glm::vec2 c, e;
    if(!gz.h.shown || !gz.view.project(gz.h.centre, c) || !gz.view.project(gz.h.centre + gz.h.axis[axis] * gz.h.radius, e))
    {
        return false;
    }
    // (from 0.6 of the way out to the point `amount` further along it, as the screen shows them)
    glm::vec2 p0, p1;
    const glm::vec3 from = gz.h.centre + gz.h.axis[axis] * (gz.h.radius * 0.6f);
    if(!gz.view.project(from, p0) || !gz.view.project(from + gz.h.axis[axis] * amount, p1))
    {
        return false;
    }
    mockLine(p0, p1);
    return true;
}

// A drag round ring `axis` by `degrees` (a right-handed turn about it).
bool mockRing(int axis, float degrees)
{
    // (from the ring's grip, its point on the ring turned round the axis step by step, as the screen shows it)
    glm::vec2 p0;
    glm::vec3 w0;
    const glm::vec3 a = gz.h.axis[axis];
    if(!gz.h.shown || !gizmo::ringGrip(gz.h, gz.view, axis, p0) || !gz.view.rayPlane(p0, gz.h.centre, a, w0))
    {
        return false;
    }
    za::Vector<glm::vec2> path;
    const int steps = za::max(2, static_cast<int>(za::abs(degrees) / 6.f) + 1);
    for(int i = 1; i <= steps; i++)
    {
        const glm::quat q = glm::angleAxis(degrees * deg * static_cast<float>(i) / static_cast<float>(steps), a);
        glm::vec2 p;
        if(!gz.view.project(gz.h.centre + q * (w0 - gz.h.centre), p))
        {
            return false;
        }
        path.pushBack(p);
    }
    mockPath(p0, path);
    return true;
}

void testVerdict()
{
    gz.mock.clear();
    gz.mockNext = 0;
    Cvar_Set("vr_armour_file", test.fileWas);
    (void)load();
    Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(ModeMove));
    Cbuf_AddText("vr_armourstudio_close\nvr_armourstudio_window close\n");
    test.step = 0;
    test.tally.verdict(testName);
}

// A drag's verdict: the values it made against what it should have, and one undo step for it.
void checkDrag(const glm::vec3& got, float tolerance, const char* what)
{
    auto& t = test.tally;
    const glm::vec3 d = glm::abs(got - test.expect);
    t.check(d.x < tolerance && d.y < tolerance && d.z < tolerance, testName,
        va("%s: %.3f %.3f %.3f, not %.3f %.3f %.3f", what, got.x, got.y, got.z, test.expect.x, test.expect.y, test.expect.z));
    t.check(undoCount() == test.undoBefore + 1, testName, va("%s: %d undo steps, not one", what, undoCount() - test.undoBefore));
    glm::vec2 c{0.f};
    (void)gz.view.project(gz.h0.centre, c);
    Con_Printf("%s: %s gave %.3f %.3f %.3f (handle part %d, at %.0f %.0f, %.2f units for its size)\n", testName, what, got.x, got.y,
        got.z, gz.part, c.x, c.y, gz.h0.radius);
}

void startTest()
{
    test = Test{};
    gz.mock.clear();
    gz.mockNext = 0;
    if(VR_IsActive())
    {
        test.tally.check(false, testName, "the window's handle is flat-screen only (-flatscreenmode)");
        test.tally.verdict(testName);
        return;
    }
    if(!sv.active)
    {
        Con_Printf("%s: a map first\n", testName);
        return;
    }
    q_strlcpy(test.fileWas, vr_armour_file.string, sizeof(test.fileWas));
    Cvar_Set("vr_armour_file", testFile);
    (void)files::writeText(filePath(), ""); // (the test's own file, empty: armour.txt is never touched)
    (void)load();
    Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(KindGrunt));
    Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartCuirass));
    Cvar_SetValueQuick(&vr_armourstudio_side, static_cast<float>(SideBoth));
    Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(ModeMove));
    Cbuf_AddText("vr_armourstudio_window open\nvr_armourstudio\n");
    test.step = 1;
}

void gizmoTest_f()
{
    startTest();
}

} // namespace

void gizmoInit()
{
    Cmd_AddCommand("vr_armourstudio_gizmo_test", gizmoTest_f);
    Cmd_AddCommand("vr_armourstudio_gizmo_look", [] {
        if(!lookAtPart())
        {
            Con_Printf("armourgizmo: no part placed (the window open, the studio running, a handle mode on)\n");
        }
    });
    Cmd_AddCommand("vr_armourstudio_gizmo_info", [] {
        glm::vec2 s{0.f};
        const bool on = gz.h.shown && gz.view.project(gz.h.centre, s);
        Con_Printf("armourgizmo: mode %d, %s at %.0f %.0f %.0f (screen %.0f %.0f of %.0f x %.0f), eye %.0f %.0f %.0f\n", gz.mode,
            on ? "shown" : "not shown", gz.h.centre.x, gz.h.centre.y, gz.h.centre.z, s.x, s.y, gz.view.w, gz.view.h, gz.view.org.x,
            gz.view.org.y, gz.view.org.z);
    });
}

void gizmoFeed()
{
    if(gz.mockNext >= static_cast<int>(gz.mock.size()))
    {
        return;
    }
    const MockStep& s = gz.mock[static_cast<za::SizeT>(gz.mockNext++)];
    imgui::feedMouse(s.at.x, s.at.y, s.down, 0.f);
}

void gizmoTestFrame()
{
    if(test.step == 0 || !mockDone())
    {
        return;
    }
    auto& t = test.tally;
    const auto waitFor = [&](const bool ok, const char* what) {
        if(ok)
        {
            return true;
        }
        if(++test.frames > testStepFrames)
        {
            t.check(false, testName, va("timed out: %s", what));
            testVerdict();
        }
        return false;
    };
    const auto next = [&](const int step) {
        test.step = step;
        test.frames = 0;
        test.undoBefore = undoCount();
    };
    // (the part and the view both still: the camera turned to it settles too)
    const bool stillNow = glm::length(gz.h.centre - test.lastCentre) < 0.01f && glm::length(gz.view.fwd - test.lastFwd) < 1e-4f;
    test.stillFrames = gz.h.shown && stillNow ? test.stillFrames + 1 : 0;
    test.lastCentre = gz.h.centre;
    test.lastFwd = gz.view.fwd;
    if(test.step == 1 || test.step == 9)
    {
        (void)lookAtPart(); // (the part where the drags can reach it)
    }
    const bool still = test.stillFrames >= stillNeeded && handleInView();
    switch(test.step)
    {
    case 1: // the window open, the studio running, the handle on the grunt's cuirass, still: a move along an arrow
        if(!waitFor(windowOpen() && running() && gz.h.shown && still && gz.mode == ModeMove, "the handle on the cuirass"))
        {
            return;
        }
    {
        const int arm = longestArm();
        test.expect = cvars3(offsetCvars); // (+ the move along the arm as the drag began: moveExpect)
        test.arm = arm;
        t.check(mockArm(arm, testMove), testName, "no arrow on the screen");
        Con_Printf("%s: move along arrow %d\n", testName, arm);
    }
        next(2);
        return;
    case 2: // the move made, one undo step; then undone
        test.expect += glm::inverse(gz.parent0) * (gz.h0.axis[test.arm] * testMove);
        checkDrag(cvars3(offsetCvars), 0.02f, "move");
        Cbuf_AddText("vr_armourstudio_undo\n");
        next(3);
        return;
    case 3: // undone: back to none; then a turn round its up ring
        if(!waitFor(glm::length(cvars3(offsetCvars)) < 1e-4f, "the move not undone"))
        {
            return;
        }
        t.check(true);
        Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(ModeRotate));
        next(4);
        return;
    case 4:
        if(!waitFor(gz.h.shown && gz.mode == ModeRotate, "the rotate handle"))
        {
            return;
        }
    {
        int ring = 0;
        for(int i = 1; i < 3; i++)
        {
            ring = za::abs(glm::dot(gz.h.axis[i], gz.view.fwd)) > za::abs(glm::dot(gz.h.axis[ring], gz.view.fwd)) ? i : ring;
        }
        constexpr int angleOf[3] = {2, 0, 1}; // (about forward: roll; left: pitch; up: yaw)
        test.expect = glm::vec3{0.f};
        test.expect[angleOf[ring]] = testTurn;
        t.check(mockRing(ring, testTurn), testName, "no ring on the screen");
        Con_Printf("%s: turn round ring %d\n", testName, ring);
    }
        next(5);
        return;
    case 5: // the turn: yaw alone; then a size along its forward arm
        checkDrag(cvars3(turnCvars), 0.5f, "rotate");
        Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(ModeScale));
        next(6);
        return;
    case 6:
        if(!waitFor(gz.h.shown && gz.mode == ModeScale, "the scale handle"))
        {
            return;
        }
    {
        const int arm = longestArm();
        test.expect = glm::vec3{1.f};
        test.expect[arm] = 1.f + testSize;
        t.check(mockArm(arm, testSize * gz.h.radius), testName, "no arm on the screen");
        Con_Printf("%s: size along arm %d\n", testName, arm);
    }
        next(7);
        return;
    case 7: // Size Fwd; then the centre's Scale
    {
        checkDrag(cvars3(sizeCvars), 0.01f, "scale arm");
        glm::vec2 c;
        (void)gz.view.project(gz.h.centre, c);
        test.expect = glm::vec3{1.f + testScalePx / scalePixels, 0.f, 0.f};
        mockLine(c, c + glm::vec2{testScalePx, 0.f});
        next(8);
        return;
    }
    case 8: // Scale; then a beast's piece moved
    {
        checkDrag(glm::vec3{vr_armourstudio_scale.value, 0.f, 0.f}, 0.01f, "scale centre");
        int part = -1;
        for(int p = 0; p < NumParts && part < 0; p++)
        {
            part = partValid(testBeastKind, p) && !sleevePart(p) && fieldShown(testBeastKind, p, OffsetX) ? p : -1;
        }
        if(!t.check(part >= 0, testName, "the rottweiler has no piece to move"))
        {
            testVerdict();
            return;
        }
        Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(testBeastKind));
        Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(part));
        Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(ModeMove));
        Cbuf_AddText("vr_armourstudio\n"); // (the beast put ahead in the grunt's place)
        next(9);
        return;
    }
    case 9:
        if(!waitFor(gz.h.shown && still && gz.mode == ModeMove && subjectShown() == testBeastKind, "the handle on the rottweiler's piece"))
        {
            return;
        }
    {
        const int arm = longestArm();
        test.expect = cvars3(offsetCvars);
        test.arm = arm;
        t.check(mockArm(arm, testBeastMove), testName, "no arrow on the rottweiler's piece");
        Con_Printf("%s: beast move along arrow %d\n", testName, arm);
    }
        next(10);
        return;
    case 10:
        test.expect += glm::inverse(gz.parent0) * (gz.h0.axis[test.arm] * testBeastMove);
        checkDrag(cvars3(offsetCvars), 0.1f, "beast move"); // (1.431 of 1.5 on the rottweiler: open, ROUND26 "studiogizmo")
        testVerdict();
        return;
    default:
        return;
    }
}

void gizmoFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    gz.view = gizmo::refdefProjection(io.DisplaySize.x, io.DisplaySize.y);
    if(!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt)
    {
        constexpr ImGuiKey keys[NumModes] = {ImGuiKey_Q, ImGuiKey_W, ImGuiKey_E, ImGuiKey_R};
        for(int i = 0; i < NumModes; i++)
        {
            if(ImGui::IsKeyPressed(keys[i], false))
            {
                Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(i));
            }
        }
    }
    placeGizmo();
    const glm::vec2 m{io.MousePos.x, io.MousePos.y};
    if(gz.dragging)
    {
        // (the drag goes on by the handle as it began: a frame the part is not placed, as its piece is made again, keeps it)
        const bool same = gz.mode == gz.dragMode && subjectNow() == gz.dragSubject && partNow() == gz.dragPart && running();
        if(!same || !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if(!same)
            {
                Con_DPrintf("armourgizmo: drag ended by a change of mode, kind or part\n");
            }
            endDrag(); // (let go, or the part or mode changed under it)
        }
        else if(m != gz.last)
        {
            dragTo(m, io.KeyCtrl);
        }
    }
    const bool overUi = io.WantCaptureMouse && !gz.dragging;
    gz.hover = gz.dragging ? gz.part : overUi ? -1 : gizmo::hit(gz.h, gz.view, m);
    if(!gz.dragging && gz.hover >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        beginDrag(gz.hover, m);
    }
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    gizmo::draw(gz.h, gz.view, dl, gz.hover);
    if(gz.dragging)
    {
        dragLabel(dl, io.MousePos);
    }
}

void gizmoRows()
{
    ImGui::PushID("gizmo");
    const int mode = za::clamp(static_cast<int>(vr_armourstudio_gizmo.value), 0, NumModes - 1);
    ImGui::TextUnformatted("Handle");
    for(int i = 0; i < NumModes; i++)
    {
        ImGui::SameLine();
        if(ImGui::RadioButton(modeLabels[i], mode == i))
        {
            Cvar_SetValueQuick(&vr_armourstudio_gizmo, static_cast<float>(i));
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("vr_armourstudio_gizmo %d: drag the handle on the part in the view (Ctrl: the rows' steps; one undo a drag)", i);
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Look at the Part"))
    {
        (void)lookAtPart();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("vr_armourstudio_gizmo_look: the view turned to the part, a quarter of the way across (clear of this window)");
    }
    if(mode != ModeOff && !modeApplies(mode))
    {
        ImGui::TextDisabled("(no %s handle on this part)", mode == ModeMove ? "move" : mode == ModeRotate ? "rotate" : "scale");
    }
    ImGui::PopID();
}

} // namespace qvr::armourstudio
