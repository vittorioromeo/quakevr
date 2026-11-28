// vr_reloadstudio.cpp -- the Reload Studio (reloadsys, his words 2026-11-28: "We should create a new studio for this, like
// procedural load studio, that uses imGUI, and we can use imGUI to basically control everything about these animations"):
// an ImGui window over flat-screen play (vr_reloadstudio [open|close|toggle|test]; Debug > Tools > Flatscreen: Reload
// Studio) on the procedural reloads' keyframes (vr_reloadsys.hpp): pick a gun (and hold it), play / pause / step / scrub
// its keys (the key-pose lock, vr_flat_prl_lock: -1 the scrub, N key N), lock any key, edit each key's timing, ease, the
// gun hand's tilt and move, the free hand's target, offset and turn (sliders, or the shared gizmo on the hand: vr_gizmo.hpp),
// the item's turn at its destination; add / remove / reorder keys; Save (quakevr/data/reloads.txt), Revert (the file again),
// Reset (the gun's built-in keys). Its success readout: the item's gap to its destination as the last insert's key ended (a
// real reload's, or the lock's at an insert key), and the real reloads' counts (seated, dropped, cancelled, failsafes).
//
// The edits go straight into reloadsys's table: the locked pose and a real reload use them from the next frame. Input as the
// other ImGui tools': while it is open ImGui has the keys and the mouse (vr_imgui.cpp); flat screen only; never with
// another ImGui tool open (it closes when one opens).

#include "vr_reloadstudio.hpp"

#include "vr_armourstudio.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatprocreload.hpp"
#include "vr_flycam.hpp"
#include "vr_gizmo.hpp"
#include "vr_held.hpp"
#include "vr_imgui.hpp"
#include "vr_reloadsys.hpp"
#include "vr_rigstudio.hpp"
#include "vr_testkit.hpp"
#include "vr_tunables.hpp"
#include "vr_waveeditor.hpp"
#include "vr_weapon_ids.h"
#include "vr_weaponstudio.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_api.h"

#include <glm/gtc/quaternion.hpp>

#include <SDL.h>

#include <stdio.h>

extern "C" int VR_IsActive(void);

namespace qvr::reloadstudio
{

namespace
{

using reloadsys::Key;
using reloadsys::Reload;
using reloadsys::Target;

constexpr const char* testName = "reloadstudio";
constexpr const char* testFile = "data/reloads_studiotest.txt";
constexpr int testStepFrames = 300; // frames a test step may wait for its condition (a safety net: FAIL "timed out")

enum GizmoMode : int
{
    GizmoOff,
    GizmoMove,
    GizmoRotate,
    GizmoCount
};

struct Window
{
    bool open{false};
    bool askClose{false};
    int gun{0};          // reloadsys::at(gun)
    int key{0};          // the key edited
    bool playing{false};
    float speed{1.f};
    double lastReal{-1.0};
    int gizmo{GizmoMove};
    // The gizmo's drag.
    gizmo::Handle h, h0;
    gizmo::Projection view;
    bool dragging{false};
    int part{-1}, hover{-1};
    glm::vec2 start{0.f}, last{0.f};
    glm::vec3 planeStart{0.f};
    glm::mat3 frame0{1.f};
    glm::vec3 ofs0{0.f}, rot0{0.f};
    float sweep{0.f};
    int dragKind{0};
    // reloadstudio2: the gizmo on the free hand or on the gun (its key's tilt and move), and the gun's angles at the drag's
    // start (a turn's change measured from them).
    bool onGun{false};
    glm::vec3 gunAngles0{0.f};
    glm::vec3 gunRot0{0.f}, gunOfs0{0.f};
    float gunSide{1.f};
    // The gun pose copied (Copy Gun Pose; Paste, Paste Down / Up).
    bool copied{false};
    glm::vec3 copyRot{0.f}, copyOfs{0.f};
    // The orbit camera (reloadstudio2, his note item 9): round his eye, turned off his view's yaw (degrees), pitched (down
    // positive), so far off (units), its focus moved across the view (right, up; units).
    bool orbit{false};
    float camYaw{150.f}, camPitch{15.f}, camDist{48.f};
    glm::vec2 camPan{0.f};
    bool hideUi{false}; // (the frame strip: the window's controls not drawn, the scene alone in its shots)
    // The test.
    int step{0};
    int stepFrames{0};
    char fileWas[64]{};
    testkit::Tally tally;
};
Window win;

[[nodiscard]] Reload& cur()
{
    return reloadsys::at(win.gun);
}

[[nodiscard]] float sideOfFree()
{
    // (the free hand's side: the off hand's, -1, for a gun in the main hand; the lock's gun decides)
    const flatprocreload::Status st = flatprocreload::status();
    return st.lockGun == 0 ? 1.f : -1.f;
}

void lockKey(int k)
{
    win.playing = false;
    Cvar_SetValueQuick(&vr_flat_prl_lock, static_cast<float>(k + 1));
}

void edited()
{
    reloadsys::touched();
}

bool editVec(const char* label, glm::vec3& v, float speed, float lo, float hi, const char* fmt, const char* tip)
{
    const bool c = ImGui::DragFloat3(label, &v[0], speed, lo, hi, fmt);
    if(tip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tip);
    }
    if(c)
    {
        edited();
    }
    return c;
}

bool editFloat(const char* label, float& v, float speed, float lo, float hi, const char* fmt, const char* tip)
{
    const bool c = ImGui::DragFloat(label, &v, speed, lo, hi, fmt);
    if(tip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tip);
    }
    if(c)
    {
        edited();
    }
    return c;
}

template<class E>
bool editEnum(const char* label, E& v, int count, const char* (*name)(E), const char* tip)
{
    int i = static_cast<int>(v);
    bool c = false;
    if(ImGui::BeginCombo(label, name(v)))
    {
        for(int n = 0; n < count; n++)
        {
            if(ImGui::Selectable(name(static_cast<E>(n)), n == i))
            {
                v = static_cast<E>(n);
                c = true;
            }
        }
        ImGui::EndCombo();
    }
    if(tip && ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tip);
    }
    if(c)
    {
        edited();
    }
    return c;
}

[[nodiscard]] float totalTime(const Reload& r)
{
    float t = 0.f;
    for(int i = 0; i < r.keyCount; i++)
    {
        t += reloadsys::handTime(r.keys[i]);
    }
    return za::max(t, 0.02f);
}

// ---- The gizmo on the free hand (the locked key's) ----------------------------------------------------------------

// The stored offset's change for a world move `d` in the key's frame (axes: forward, left, up): the body's frame stores
// (forward, right, up), the gun's (forward, left, up), each's side-ways value mirrored by the free hand's side.
[[nodiscard]] glm::vec3 ofsDelta(const glm::mat3& frame, const glm::vec3& d, int kind)
{
    const float side = sideOfFree();
    const glm::vec3 l{glm::dot(d, frame[0]), glm::dot(d, frame[1]), glm::dot(d, frame[2])};
    const bool gunFrame = kind == static_cast<int>(Target::Mouth) || kind == static_cast<int>(Target::Dest);
    return {l.x, side * (gunFrame ? l.y : -l.y), l.z};
}

// The stored turn after a turn of `a` radians about the frame's axis `axis`: axes(m(v')) = R * axes(m(v)), so (the mirror's
// reflection S across the frame's forward-up plane: axes(m(v)) = S axes(v) S) axes(v') = S R S axes(v).
[[nodiscard]] glm::vec3 rotAfter(const glm::vec3& v, int axis, float a)
{
    glm::vec3 e{0.f};
    e[axis] = 1.f;
    glm::mat3 r = glm::mat3_cast(glm::angleAxis(a, e));
    if(sideOfFree() < 0.f)
    {
        const glm::mat3 s{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, -1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
        r = s * r * s;
    }
    glm::vec3 out;
    held::anglesFromAxes(r * held::axesFromAngles(&v[0], true), &out[0], true);
    return out;
}

[[nodiscard]] bool keyValid()
{
    return win.key >= 0 && win.key < cur().keyCount;
}

// The gun's gizmo (reloadstudio2, his note item 12): on the gun hand as drawn; moves along the body's frame (forward, left,
// up: the key's Move, "in" mirrored by the gun's side), turns about the gun's own axes (the key's Tilt: the turned gun's
// angles less the drawn ones, added to the key's, as the reload adds them to the hand's).
void placeGunGizmo(const flatprocreload::Status& st)
{
    glm::vec3 pos, rot, fwd, right;
    float side = 1.f;
    if(win.gizmo == GizmoOff || !st.locked || st.lockKey != win.key || st.lockWeapon != cur().weapon ||
       !flatprocreload::lockGunFrame(pos, rot, fwd, right, side))
    {
        return;
    }
    win.h.centre = pos;
    const glm::mat3 gunAxes = held::axesFromAngles(&rot[0], true);
    const glm::mat3 body{fwd, -right, glm::vec3{0.f, 0.f, 1.f}};
    const glm::mat3& axes = win.gizmo == GizmoRotate ? gunAxes : body;
    for(int i = 0; i < 3; i++)
    {
        win.h.axis[i] = axes[i];
    }
    win.h.move = win.gizmo == GizmoMove;
    win.h.rot = win.gizmo == GizmoRotate;
    win.h.scale = false;
    if(!win.dragging)
    {
        win.frame0 = body;
        win.gunAngles0 = rot;
        win.gunSide = side;
    }
    gizmo::place(win.h, win.view, 70.f);
}

void placeGizmo()
{
    win.h.shown = false;
    glm::vec3 framePos, handPos, handRot;
    glm::mat3 frame;
    int kind = 0;
    const flatprocreload::Status st = flatprocreload::status();
    if(win.onGun)
    {
        placeGunGizmo(st);
        return;
    }
    if(win.gizmo == GizmoOff || !st.locked || st.lockKey != win.key || st.lockWeapon != cur().weapon ||
       !flatprocreload::lockFrames(framePos, frame, handPos, handRot, kind))
    {
        return;
    }
    const bool rotOk = kind == static_cast<int>(Target::Pouch) || kind == static_cast<int>(Target::Mouth) ||
                       kind == static_cast<int>(Target::Dest);
    if(kind == static_cast<int>(Target::Hold) || (win.gizmo == GizmoRotate && !rotOk))
    {
        return;
    }
    win.h.centre = handPos;
    for(int i = 0; i < 3; i++)
    {
        win.h.axis[i] = frame[i];
    }
    win.h.move = win.gizmo == GizmoMove;
    win.h.rot = win.gizmo == GizmoRotate;
    win.h.scale = false;
    win.dragKind = win.dragging ? win.dragKind : kind;
    win.frame0 = win.dragging ? win.frame0 : frame;
    gizmo::place(win.h, win.view, 70.f);
}

// The key's gun Move for a world move `d` (the body's frame: forward, right, up; "in" is the stored y, mirrored by the side).
[[nodiscard]] glm::vec3 gunOfsAfter(const glm::vec3& ofs0, const glm::mat3& body, const glm::vec3& d, float side)
{
    const float right = -glm::dot(d, body[1]); // (body[1]: left)
    return ofs0 + glm::vec3{glm::dot(d, body[0]), -side * right, glm::dot(d, body[2])};
}

// The key's gun Tilt for the drawn gun (angles `angles0`) turned `a` radians about the world axis `axis`: the turned axes'
// angles less the drawn ones (each the short way round), mirrored back by the side, on the Tilt at the drag's start.
[[nodiscard]] glm::vec3 gunRotAfter(const glm::vec3& rot0, const glm::vec3& angles0, const glm::vec3& axis, float a, float side)
{
    const glm::mat3 turned = glm::mat3_cast(glm::angleAxis(a, axis)) * held::axesFromAngles(&angles0[0], true);
    glm::vec3 now;
    held::anglesFromAxes(turned, &now[0], true);
    glm::vec3 d;
    for(int i = 0; i < 3; i++)
    {
        d[i] = za::remainder(now[i] - angles0[i], 360.f);
    }
    return rot0 + glm::vec3{d.x, side * d.y, side * d.z};
}

void dragGun(Key& k, const gizmo::Handle& h, const glm::vec2& m)
{
    if(h.move)
    {
        glm::vec3 world{0.f};
        if(win.part <= 2)
        {
            world = gizmo::axisMoveExact(h, win.view, win.part, win.start, m);
        }
        else
        {
            const glm::vec3 n = win.part >= 4 ? h.axis[win.part - 4] : win.view.fwd;
            glm::vec3 hit;
            if(!win.view.rayPlane(m, h.centre, n, hit))
            {
                return;
            }
            world = hit - win.planeStart;
        }
        k.gunOfs = gunOfsAfter(win.gunOfs0, win.frame0, world, win.gunSide);
    }
    else if(win.part <= 2)
    {
        win.sweep += gizmo::ringSweepExact(h, win.view, h.axis[win.part], win.last, m);
        k.gunRot = gunRotAfter(win.gunRot0, win.gunAngles0, h.axis[win.part], win.sweep, win.gunSide);
    }
}

void dragTo(const glm::vec2& m)
{
    if(!keyValid())
    {
        return;
    }
    Key& k = cur().keys[win.key];
    const gizmo::Handle& h = win.h0;
    if(win.onGun)
    {
        dragGun(k, h, m);
        win.last = m;
        edited();
        return;
    }
    if(win.h0.move)
    {
        glm::vec3 world{0.f};
        if(win.part <= 2)
        {
            world = gizmo::axisMoveExact(h, win.view, win.part, win.start, m);
        }
        else
        {
            const glm::vec3 n = win.part >= 4 ? h.axis[win.part - 4] : win.view.fwd;
            glm::vec3 hit;
            if(!win.view.rayPlane(m, h.centre, n, hit))
            {
                return;
            }
            world = hit - win.planeStart;
        }
        k.handOfs = win.ofs0 + ofsDelta(win.frame0, world, win.dragKind);
    }
    else if(win.part <= 2)
    {
        win.sweep += gizmo::ringSweepExact(h, win.view, h.axis[win.part], win.last, m);
        k.handRot = rotAfter(win.rot0, win.part, win.sweep);
    }
    win.last = m;
    edited();
}

// ---- The orbit camera (reloadstudio2) -----------------------------------------------------------------------------

constexpr float camYawStart = 150.f; // (in front of him and to his right, looking back at the gun and the free hand)
constexpr float camPitchStart = 15.f;
constexpr float camDistStart = 48.f;
constexpr float camOrbitSpeed = 0.3f;  // degrees a pixel
constexpr float camPanSpeed = 0.0025f; // of the distance a pixel
constexpr float camZoomStep = 0.1f;    // of the distance a wheel notch
constexpr float camFocusDown = 14.f;   // units below his eye it looks at (the hands and the gun)

void resetView()
{
    win.camYaw = camYawStart;
    win.camPitch = camPitchStart;
    win.camDist = camDistStart;
    win.camPan = glm::vec2{0.f};
}

// As the Rig Studio's and the Armour Studio's: right drag orbits, middle drag pans, the wheel zooms, over the scene (not the
// window); any of them puts the camera in orbit.
void cameraInput()
{
    ImGuiIO& io = ImGui::GetIO();
    if(io.WantCaptureMouse || win.dragging)
    {
        return;
    }
    const glm::vec2 d{io.MouseDelta.x, io.MouseDelta.y};
    if(ImGui::IsMouseDown(ImGuiMouseButton_Right) && (d.x != 0.f || d.y != 0.f))
    {
        win.orbit = true;
        win.camYaw -= d.x * camOrbitSpeed;
        win.camPitch = za::clamp(win.camPitch + d.y * camOrbitSpeed, -85.f, 85.f);
    }
    if(ImGui::IsMouseDown(ImGuiMouseButton_Middle) && (d.x != 0.f || d.y != 0.f))
    {
        win.orbit = true;
        win.camPan += glm::vec2{-d.x, d.y} * (win.camDist * camPanSpeed);
    }
    if(io.MouseWheel != 0.f)
    {
        win.orbit = true;
        win.camDist = za::clamp(win.camDist * (1.f - io.MouseWheel * camZoomStep), 8.f, 400.f);
    }
}

void gizmoFrame()
{
    cameraInput();
    ImGuiIO& io = ImGui::GetIO();
    win.view = gizmo::refdefProjection(io.DisplaySize.x, io.DisplaySize.y);
    placeGizmo();
    const glm::vec2 m{io.MousePos.x, io.MousePos.y};
    if(win.dragging)
    {
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !keyValid())
        {
            win.dragging = false;
        }
        else if(m != win.last)
        {
            dragTo(m);
        }
    }
    const bool overUi = io.WantCaptureMouse && !win.dragging;
    win.hover = win.dragging ? win.part : (overUi || !win.h.shown) ? -1 : gizmo::hit(win.h, win.view, m);
    if(!win.dragging && win.hover >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && keyValid())
    {
        win.dragging = true;
        win.part = win.hover;
        win.start = win.last = m;
        win.h0 = win.h;
        win.sweep = 0.f;
        win.ofs0 = cur().keys[win.key].handOfs;
        win.rot0 = cur().keys[win.key].handRot;
        win.gunOfs0 = cur().keys[win.key].gunOfs;
        win.gunRot0 = cur().keys[win.key].gunRot;
        const glm::vec3 n = win.part >= 4 && win.part <= 6 ? win.h.axis[win.part - 4] : win.view.fwd;
        if(!win.view.rayPlane(m, win.h.centre, n, win.planeStart))
        {
            win.planeStart = win.h.centre;
        }
    }
    if(win.h.shown)
    {
        gizmo::draw(win.h, win.view, ImGui::GetBackgroundDrawList(), win.hover);
    }
}

// ---- The window --------------------------------------------------------------------------------------------------

void transportRows()
{
    Reload& r = cur();
    const flatprocreload::Status st = flatprocreload::status();
    const int lock = static_cast<int>(vr_flat_prl_lock.value + (vr_flat_prl_lock.value >= 0.f ? 0.5f : -0.5f));
    if(lock > 0 && st.locked && st.lockWeapon == r.weapon && st.lockKey >= 0 && st.lockKey < r.keyCount)
    {
        win.key = st.lockKey; // (locked from the console or the menu: the key edited follows)
    }
    if(ImGui::Button(win.playing ? "Pause" : "Play"))
    {
        win.playing = !win.playing;
        if(win.playing)
        {
            Cvar_SetValueQuick(&vr_flat_prl_lock, -1.f);
            win.lastReal = realtime;
        }
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Plays the keys over and over (the lock's scrub, vr_flat_prl_lock -1): nothing is taken or loaded");
    }
    ImGui::SameLine();
    if(ImGui::Button("<"))
    {
        win.key = (win.key + r.keyCount - 1) % za::max(1, r.keyCount);
        lockKey(win.key);
    }
    ImGui::SameLine();
    if(ImGui::Button(">"))
    {
        win.key = (win.key + 1) % za::max(1, r.keyCount);
        lockKey(win.key);
    }
    ImGui::SameLine();
    if(ImGui::Button("Lock Key"))
    {
        lockKey(win.key);
    }
    ImGui::SameLine();
    if(ImGui::Button("Unlock"))
    {
        win.playing = false;
        Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload (R)"))
    {
        win.playing = false;
        Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
        Cbuf_AddText("impulse 55\n");
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("A real reload of the gun in hand (impulse 55), unlocked: the success readout below follows it");
    }
    ImGui::SetNextItemWidth(220.f);
    float scrub = vr_flat_prl_scrub.value;
    if(ImGui::SliderFloat("Scrub", &scrub, 0.f, 1.f, "%.3f"))
    {
        win.playing = false;
        Cvar_SetValueQuick(&vr_flat_prl_lock, -1.f);
        Cvar_SetValueQuick(&vr_flat_prl_scrub, scrub);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::SliderFloat("Speed", &win.speed, 0.1f, 2.f, "%.2fx");
    ImGui::Text("%s: %s", lock == 0 ? "Unlocked" : lock < 0 ? "Scrubbing" : "Locked",
        st.locked ? r.keys[za::clamp(st.lockKey, 0, r.keyCount - 1)].name : (st.tripOn ? "a reload playing" : "-"));
    if(st.lockGun < 0)
    {
        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "Hold a gun with a procedural reload in one hand, the other empty, to see it.");
    }
    else if(st.lockWeapon != r.weapon)
    {
        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "The gun in hand isn't the one edited: Hold This Gun.");
    }
    ImGui::Text("Item to destination: now %.2f u, at the last insert %.2f u", st.portGap, st.insertGap);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("The item's gap to its destination (the port: it goes in within ~1.6 u); -1: no item in the hand");
    }
    ImGui::Text("Reloads %d: seated %d, dropped %d, cancelled %d, failsafes %d (last: %s)", st.trips, st.seated, st.dropped, st.cancels,
        st.failsafes, st.lastCancel);
}

void reloadRows()
{
    Reload& r = cur();
    editEnum("Item", r.item, static_cast<int>(reloadsys::Item::Count), reloadsys::itemName, "What the free hand carries");
    editEnum("Destination", r.dest, static_cast<int>(reloadsys::Dest::Count), reloadsys::destName, "Where it goes on the gun");
    editVec("Item Turn", r.itemRot, 0.5f, -180.f, 180.f, "%.1f",
        "The item's turn at its destination in the gun's frame (pitch, yaw, roll): the hand is solved backwards from it");
    editFloat("Speed", r.speed, 0.01f, reloadsys::minSpeed, reloadsys::maxSpeed, "%.2fx",
        "This gun's whole reload played this much faster (1: the keys' own times), times Reload Speed (vr_reload_speed) for every gun");
    ImGui::SameLine();
    ImGui::TextDisabled("(all guns %.2fx)", vr_reload_speed.value);
    int loop[2] = {r.loopFrom + 1, r.loopTo + 1};
    if(ImGui::DragInt2("Loop Keys", loop, 0.1f, 0, r.keyCount))
    {
        r.loopFrom = loop[0] - 1;
        r.loopTo = loop[1] - 1;
        edited();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Keys repeated while the gun takes more (from, to; counted from 1; 0 none)");
    }
}

void keyListRows()
{
    Reload& r = cur();
    ImGui::BeginChild("keys", ImVec2(0.f, 170.f), ImGuiChildFlags_Borders);
    for(int i = 0; i < r.keyCount; i++)
    {
        const Key& k = r.keys[i];
        char label[96];
        q_snprintf(label, sizeof(label), "%2d  %-14s %.2f s  %s%s%s%s", i + 1, k.name, k.time, reloadsys::targetName(k.target),
            k.action != reloadsys::Action::None ? ", " : "", k.action != reloadsys::Action::None ? reloadsys::actionName(k.action) : "",
            k.finish ? " (end)" : (i >= r.loopFrom && i <= r.loopTo && r.loopFrom >= 0 ? " (loop)" : ""));
        ImGui::PushID(i);
        if(ImGui::Selectable(label, i == win.key))
        {
            win.key = i;
            lockKey(i);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    if(ImGui::Button("Add") && r.keyCount < reloadsys::maxKeys)
    {
        const int at = keyValid() ? win.key + 1 : r.keyCount;
        for(int i = r.keyCount; i > at; i--)
        {
            r.keys[i] = r.keys[i - 1];
        }
        r.keys[at] = keyValid() ? r.keys[win.key] : Key{};
        q_strlcpy(r.keys[at].name, "new key", sizeof(r.keys[at].name));
        r.keys[at].action = reloadsys::Action::None;
        r.keyCount++;
        r.loopTo += (r.loopTo >= at) ? 1 : 0;
        r.loopFrom += (r.loopFrom > at) ? 1 : 0;
        win.key = at;
        edited();
    }
    ImGui::SameLine();
    if(ImGui::Button("Remove") && keyValid() && r.keyCount > 1)
    {
        for(int i = win.key; i < r.keyCount - 1; i++)
        {
            r.keys[i] = r.keys[i + 1];
        }
        r.keyCount--;
        r.loopTo -= (r.loopTo >= win.key) ? 1 : 0;
        r.loopFrom -= (r.loopFrom > win.key) ? 1 : 0;
        win.key = za::min(win.key, r.keyCount - 1);
        edited();
    }
    ImGui::SameLine();
    if(ImGui::Button("Up") && keyValid() && win.key > 0)
    {
        const Key t = r.keys[win.key - 1];
        r.keys[win.key - 1] = r.keys[win.key];
        r.keys[win.key] = t;
        win.key--;
        edited();
    }
    ImGui::SameLine();
    if(ImGui::Button("Down") && keyValid() && win.key < r.keyCount - 1)
    {
        const Key t = r.keys[win.key + 1];
        r.keys[win.key + 1] = r.keys[win.key];
        r.keys[win.key] = t;
        win.key++;
        edited();
    }
}

// Copy / paste of the gun hand's pose (Tilt and Move) between keys (his note item 12): Paste into this key; Paste Down into
// the next key (and on to it: pressed again, the one after), Paste Up into the previous one.
void pasteGunPose(int to)
{
    Reload& r = cur();
    if(!win.copied || to < 0 || to >= r.keyCount)
    {
        return;
    }
    r.keys[to].gunRot = win.copyRot;
    r.keys[to].gunOfs = win.copyOfs;
    win.key = to;
    lockKey(to);
    edited();
}

void gunPoseRows()
{
    Key& k = cur().keys[win.key];
    if(ImGui::Button("Copy Gun Pose"))
    {
        win.copied = true;
        win.copyRot = k.gunRot;
        win.copyOfs = k.gunOfs;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("This key's gun Tilt and Move, for Paste / Paste Down / Paste Up");
    }
    ImGui::BeginDisabled(!win.copied);
    ImGui::SameLine();
    if(ImGui::Button("Paste"))
    {
        pasteGunPose(win.key);
    }
    ImGui::SameLine();
    if(ImGui::Button("Paste Down"))
    {
        pasteGunPose(win.key + 1);
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("The copied gun pose into the next key, and on to it (again: the one after)");
    }
    ImGui::SameLine();
    if(ImGui::Button("Paste Up"))
    {
        pasteGunPose(win.key - 1);
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("The copied gun pose into the previous key, and on to it (again: the one before)");
    }
    ImGui::EndDisabled();
    if(win.copied)
    {
        ImGui::TextDisabled("Copied: tilt %.1f %.1f %.1f, move %.2f %.2f %.2f", win.copyRot.x, win.copyRot.y, win.copyRot.z, win.copyOfs.x,
            win.copyOfs.y, win.copyOfs.z);
    }
}

void keyRows()
{
    if(!keyValid())
    {
        return;
    }
    Key& k = cur().keys[win.key];
    ImGui::PushID("key");
    if(ImGui::InputText("Name", k.name, sizeof(k.name)))
    {
        edited();
    }
    editFloat("Time", k.time, 0.005f, 0.02f, 3.f, "%.3f s", "Seconds the free hand's move takes");
    editFloat("Gun Time", k.gunTime, 0.005f, 0.f, 3.f, "%.3f s", "Seconds the gun's pose takes (0: the key's time)");
    editEnum("Ease", k.ease, static_cast<int>(reloadsys::Ease::Count), reloadsys::easeName, "The moves' curve");
    editEnum("Action", k.action, static_cast<int>(reloadsys::Action::Count), reloadsys::actionName,
        "take: from the pouch (the key waits for it); insert: waits for the item to go in; open / close: the gun's");
    editEnum("Condition", k.cond, static_cast<int>(reloadsys::Cond::Count), reloadsys::condName, "Played only when this holds");
    if(ImGui::Checkbox("Reload's End", &k.finish))
    {
        edited();
    }
    ImGui::SeparatorText("Free Hand");
    editEnum("Target", k.target, static_cast<int>(Target::Count), reloadsys::targetName,
        "rest: its pose; pouch: the ammo pouch; mouth: the item before its destination; dest: the item in it; hold: where it is");
    const bool gunFrame = k.target == Target::Mouth || k.target == Target::Dest;
    editVec("Offset", k.handOfs, 0.05f, -24.f, 24.f, "%.2f",
        gunFrame ? "Units the item sits off its destination, in the gun's frame (forward, right, up)"
                 : "Units off its target, in the body's frame (forward, right, up)");
    editVec("Turn", k.handRot, 0.5f, -180.f, 180.f, "%.1f",
        gunFrame ? "Degrees the item turns on its destination's turn (pitch, yaw, roll; the gun's frame)"
                 : "The hand's turn (pitch, yaw, roll) on the body's");
    editFloat("Arc", k.arc, 0.05f, 0.f, 24.f, "%.2f u", "Units the way bows up and out");
    editFloat("Approach", k.approach, 0.05f, 0.f, 24.f, "%.2f u", "Units out of the destination's opening (mouth)");
    ImGui::SeparatorText("Gun Hand");
    editVec("Tilt", k.gunRot, 0.5f, -180.f, 180.f, "%.1f", "Degrees: pitch (+ muzzle down), yaw (in), roll");
    editVec("Move", k.gunOfs, 0.05f, -24.f, 24.f, "%.2f", "Units: forward, in (towards the body's middle), up");
    gunPoseRows();
    ImGui::SeparatorText("Gizmo");
    if(ImGui::RadioButton("On the Hand", !win.onGun))
    {
        win.onGun = false;
        win.dragging = false;
    }
    ImGui::SameLine();
    if(ImGui::RadioButton("On the Gun", win.onGun))
    {
        win.onGun = true;
        win.dragging = false;
    }
    constexpr const char* modes[GizmoCount] = {"Off", "Move", "Rotate"};
    for(int i = 0; i < GizmoCount; i++)
    {
        if(i)
        {
            ImGui::SameLine();
        }
        if(ImGui::RadioButton(modes[i], win.gizmo == i))
        {
            win.gizmo = i;
        }
    }
    ImGui::TextDisabled(win.onGun ? "On the gun: Move drags its Move (body's frame), Rotate turns its Tilt about the gun's axes."
                                  : "The handle shows on the free hand while this key is locked.");
    ImGui::PopID();
}

void cameraRows()
{
    if(ImGui::RadioButton("His Eyes", !win.orbit))
    {
        win.orbit = false;
    }
    ImGui::SameLine();
    if(ImGui::RadioButton("Orbit", win.orbit))
    {
        win.orbit = true;
    }
    ImGui::SameLine();
    if(ImGui::Button("Reset View"))
    {
        resetView();
        win.orbit = true;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("The orbit camera back in front of him and to his right, looking at the gun and the free hand");
    }
    ImGui::TextDisabled("Over the scene: right drag orbits, middle drag pans, the wheel zooms (any of them: Orbit).");
}

void drawWindow()
{
    ImGui::SetNextWindowSize(ImVec2(520.f, 760.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 540.f, 20.f), ImGuiCond_FirstUseEver);
    bool keep = true;
    const bool shown = ImGui::Begin("Reload Studio", &keep, ImGuiWindowFlags_NoSavedSettings);
    if(!keep)
    {
        win.askClose = true;
    }
    if(shown)
    {
        const char* guns[16];
        const int n = za::min(reloadsys::count(), 16);
        for(int i = 0; i < n; i++)
        {
            guns[i] = reloadsys::at(i).gun;
        }
        ImGui::SetNextItemWidth(180.f);
        if(ImGui::Combo("Gun", &win.gun, guns, n))
        {
            win.key = 0;
        }
        ImGui::SameLine();
        if(ImGui::Button("Hold This Gun"))
        {
            // (impulse 150 + its id: loaded, in the main hand; past 19, the nail pistol, the revolver: vr_test_give_wid and impulse 149)
            char give[64];
            q_snprintf(give, sizeof(give), cur().weapon < 20 ? "impulse %d\n" : "vr_test_give_wid %d; impulse 149\n",
                cur().weapon < 20 ? 150 + cur().weapon : cur().weapon);
            Cbuf_AddText(give);
        }
        ImGui::SameLine();
        const bool unsaved = reloadsys::revision() != reloadsys::savedRevision();
        ImGui::TextColored(unsaved ? ImVec4(1.f, 0.8f, 0.3f, 1.f) : ImVec4(0.6f, 0.9f, 0.6f, 1.f), unsaved ? "unsaved" : "saved");
        if(ImGui::Button("Save"))
        {
            Con_Printf("reloadstudio: %s %s\n", reloadsys::filePath(), reloadsys::save() ? "written" : "COULD NOT BE WRITTEN");
        }
        ImGui::SameLine();
        if(ImGui::Button("Revert"))
        {
            (void)reloadsys::load();
            win.key = za::min(win.key, cur().keyCount - 1);
        }
        ImGui::SameLine();
        if(ImGui::Button("Reset Gun"))
        {
            cur() = reloadsys::builtIn(cur().weapon);
            win.key = za::min(win.key, cur().keyCount - 1);
            edited();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("This gun's built-in keys (Save to keep them)");
        }
        ImGui::SeparatorText("Camera");
        cameraRows();
        ImGui::SeparatorText("Timeline");
        transportRows();
        ImGui::SeparatorText("Reload");
        reloadRows();
        ImGui::SeparatorText("Keys");
        keyListRows();
        ImGui::SeparatorText("Key");
        keyRows();
    }
    ImGui::End();
}

void openWindow()
{
    if(win.open)
    {
        return;
    }
    if(VR_IsActive())
    {
        Con_Printf("reloadstudio: the Reload Studio is flat-screen only (start the game with -flatscreenmode)\n");
        return;
    }
    if(rigstudio::active() || waveeditor::shown() || waveeditor::holds() || weaponstudio::active() || armourstudio::windowOpen())
    {
        Con_Printf("reloadstudio: close the other ImGui tool first\n");
        return;
    }
    if(!imgui::ensure())
    {
        Con_Printf("reloadstudio: ImGui could not be started\n");
        return;
    }
    win.open = true;
    win.askClose = false;
    Key_ClearStates();
    key_dest = key_game;
    m_state = m_none;
    Con_Printf("reloadstudio: window open -- vr_reloadstudio toggles it, its X closes it\n");
}

void closeWindow()
{
    if(!win.open)
    {
        return;
    }
    win.open = false;
    win.askClose = false;
    win.dragging = false;
    win.playing = false;
    imgui::want(false);
    IN_Activate();
    Con_Printf("reloadstudio: window closed\n");
}

// ---- The test (vr_reloadstudio test): the window opens, a gun held, its keys locked through the window's own calls, the
// gizmo's math, an edit followed live by the locked pose, saved to a test file, read back, reset; the window closed. ------

void testVerdict()
{
    Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
    Cvar_Set("vr_reload_file", win.fileWas);
    (void)reloadsys::load();
    win.step = 0;
    closeWindow();
    win.tally.verdict(testName);
}

void testFrame()
{
    if(win.step == 0)
    {
        return;
    }
    testkit::Tally& t = win.tally;
    win.stepFrames++;
    const flatprocreload::Status st = flatprocreload::status();
    const auto next = [&]() {
        win.step++;
        win.stepFrames = 0;
    };
    if(win.stepFrames > testStepFrames)
    {
        t.check(false, testName, "timed out");
        testVerdict();
        return;
    }
    switch(win.step)
    {
        case 1: // the shotgun held (the test's script gave it): open
            t.check(win.open && imgui::wanted(), testName, "the window open, ImGui wanted");
            win.gun = 0;
            win.key = 2;
            lockKey(win.key);
            next();
            break;
        case 2: // locked at key 3 (the carry's end: the item at the port's mouth)
            if(st.locked && st.lockKey == 2 && win.stepFrames > 10)
            {
                t.check(st.lockWeapon == WID_SHOTGUN, testName, "the lock holds the shotgun");
                t.check(st.portGap > 0.2f && st.portGap < 1.0f, testName, "the item at the port's mouth (approach 0.5)");
                Con_Printf("reloadstudio: key 3 gap %.2f\n", st.portGap);
                win.key = 3;
                lockKey(win.key);
                next();
            }
            break;
        case 3: // key 4 (the insert's end): the item in the port, solved backwards
            if(st.locked && st.lockKey == 3 && win.stepFrames > 10)
            {
                t.check(st.portGap >= 0.f && st.portGap < 0.1f, testName, "the item in the port at the insert's end (backwards solve)");
                Con_Printf("reloadstudio: key 4 gap %.2f\n", st.portGap);
                // An edit, live: the item 1 unit up off the port.
                cur().keys[3].handOfs.z = 1.f;
                edited();
                next();
            }
            break;
        case 4:
            if(win.stepFrames > 5)
            {
                t.check(st.portGap > 0.9f && st.portGap < 1.1f, testName, "an edit followed live (the item 1 unit off its port)");
                Con_Printf("reloadstudio: key 4 edited gap %.2f\n", st.portGap);
                // The gizmo's math: a world move along the gun's forward is the offset's forward; a turn about up and back.
                const glm::mat3 frame{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
                const glm::vec3 d = ofsDelta(frame, glm::vec3{2.f, 0.f, 0.f}, static_cast<int>(Target::Dest));
                t.check(glm::distance(d, glm::vec3{2.f, 0.f, 0.f}) < 1e-4f, testName, "the gizmo's move: forward 2 is offset forward 2");
                const glm::vec3 r0{10.f, 20.f, 30.f};
                const glm::vec3 r1 = rotAfter(rotAfter(r0, 2, 0.5f), 2, -0.5f);
                t.check(glm::distance(r0, r1) < 0.05f, testName, "the gizmo's turn there and back");
                t.check(reloadsys::save(), testName, "saved (the test's file)");
                cur().keys[3].handOfs.z = 0.f;
                (void)reloadsys::load();
                t.check(za::abs(cur().keys[3].handOfs.z - 1.f) < 1e-3f, testName, "read back from the file");
                cur() = reloadsys::builtIn(WID_SHOTGUN);
                t.check(cur().keys[3].handOfs.z == 0.f && cur().keyCount == 6, testName, "Reset Gun: the built-in keys");
                edited();
                // The super shotgun's reload: its keys, its break open and close.
                const Reload* ssg = reloadsys::forWeapon(WID_SUPER_SHOTGUN);
                t.check(ssg && reloadsys::takeKey(*ssg) >= 0 && reloadsys::insertKey(*ssg) > reloadsys::takeKey(*ssg), testName,
                    "the super shotgun's reload: a take and an insert");
                // reloadmags: the magazine guns listed, each a magazine to its receiver, its eject before its take.
                for(const int w : {WID_NAILGUN, WID_SUPER_NAILGUN, WID_LIGHTNING, WID_NAILPISTOL})
                {
                    bool listed = false;
                    for(int i = 0; i < reloadsys::count(); i++)
                    {
                        listed = listed || reloadsys::at(i).weapon == w;
                    }
                    const Reload* mr = reloadsys::forWeapon(w);
                    int eject = -1;
                    for(int i = 0; mr && i < mr->keyCount; i++)
                    {
                        eject = eject < 0 && mr->keys[i].action == reloadsys::Action::Eject ? i : eject;
                    }
                    t.check(listed && mr && mr->item == reloadsys::Item::Magazine && mr->dest == reloadsys::Dest::Receiver && eject >= 0 &&
                                eject < reloadsys::takeKey(*mr) && reloadsys::insertKey(*mr) > reloadsys::takeKey(*mr),
                        testName, mr ? mr->gun : "a magazine gun (none)");
                }
                next();
            }
            break;
        default: testVerdict(); break;
    }
}

// ---- The pose test (vr_reloadstudio posetest; reloadstudio2, his note item 11): the gun in hand (the script gave it, partly
// fired), the lock's pose on its take key (the item in the hand at the pouch) and the key after it (the carry, held off the
// destination by vr_flat_prl_test_miss so nothing goes in), before any real reload of it; then a real reload held on the same
// keys (vr_flat_prl_hold); each pair compared: the hand placed and drawn off the gun hand, the item in the hand, the grasp's
// palm, the fingers. (reloadstudio3: the super shotgun's and the revolver's carry too: the lock draws them open, as the game.)

constexpr const char* poseTestName = "reloadstudio_pose";
constexpr int poseSettle = 50; // frames a pose is let settle before it is sampled (the attach's and the grasp's easing, ~0.3 s)

struct PoseTest
{
    bool on{false};
    int step{0};
    int frames{0};
    int settled{0};
    int k1{-1}, k2{-1};
    flatprocreload::PoseSample lock1, lock2, trip1, trip2;
    testkit::Tally tally;
};
PoseTest pt;

[[nodiscard]] float angleGap(const glm::vec3& a, const glm::vec3& b)
{
    // (the turn between the two: the relative axes' angle)
    const glm::mat3 ra = held::axesFromAngles(&a[0], true), rb = held::axesFromAngles(&b[0], true);
    const float c = za::clamp((ra[0][0] * rb[0][0] + ra[0][1] * rb[0][1] + ra[0][2] * rb[0][2] + ra[1][0] * rb[1][0] + ra[1][1] * rb[1][1] +
                                  ra[1][2] * rb[1][2] + ra[2][0] * rb[2][0] + ra[2][1] * rb[2][1] + ra[2][2] * rb[2][2] - 1.f) * 0.5f,
        -1.f, 1.f);
    return glm::degrees(za::acos(c));
}

void poseCompare(const char* what, const flatprocreload::PoseSample& l, const flatprocreload::PoseSample& t)
{
    float curl = 0.f;
    for(int i = 0; i < 5; i++)
    {
        curl = za::max(curl, za::abs(l.curl[i] - t.curl[i]));
    }
    const float hand = glm::distance(l.handPos, t.handPos), handTurn = angleGap(l.handRot, t.handRot);
    const float drawn = glm::distance(l.drawnPos, t.drawnPos), drawnTurn = angleGap(l.drawnRot, t.drawnRot);
    const float inHand = glm::distance(l.inHandPos, t.inHandPos), inHandTurn = angleGap(l.inHandRot, t.inHandRot);
    // (Short lines: the console wraps long ones.)
    Con_Printf("reloadstudio_pose: %s %s: hand %.3f u %.2f deg, drawn %.3f u %.2f deg\n", cur().gun, what, hand, handTurn, drawn, drawnTurn);
    Con_Printf("reloadstudio_pose: %s %s: item in hand %.3f u %.2f deg, palm %.2f/%.2f u %.1f/%.1f deg, fingers %.3f\n", cur().gun, what,
        inHand, inHandTurn, l.palmMove, t.palmMove, l.palmTurn, t.palmTurn, curl);
    Con_Printf("reloadstudio_pose: %s %s: curls lock %.2f %.2f %.2f %.2f %.2f, real %.2f %.2f %.2f %.2f %.2f\n", cur().gun, what, l.curl[0],
        l.curl[1], l.curl[2], l.curl[3], l.curl[4], t.curl[0], t.curl[1], t.curl[2], t.curl[3], t.curl[4]);
    Con_Printf("reloadstudio_pose: %s %s: the hand off the gun hand: lock %.2f %.2f %.2f, real %.2f %.2f %.2f\n", cur().gun, what,
        l.handPos.x, l.handPos.y, l.handPos.z, t.handPos.x, t.handPos.y, t.handPos.z);
    Con_Printf("reloadstudio_pose: %s %s: the item: lock %s %d, real %s %d\n", cur().gun, what, l.itemModel, l.itemFrame, t.itemModel,
        t.itemFrame);
    Con_Printf("reloadstudio_pose: %s %s: the gun hand: lock %.2f %.2f %.2f (%.1f %.1f %.1f), real %.2f %.2f %.2f (%.1f %.1f %.1f)\n",
        cur().gun, what, l.gunAt.x, l.gunAt.y, l.gunAt.z, l.gunRot.x, l.gunRot.y, l.gunRot.z, t.gunAt.x, t.gunAt.y, t.gunAt.z, t.gunRot.x,
        t.gunRot.y, t.gunRot.z);
    Con_Printf("reloadstudio_pose: %s %s: the free hand: lock %.2f %.2f %.2f, real %.2f %.2f %.2f\n", cur().gun, what, l.freeAt.x,
        l.freeAt.y, l.freeAt.z, t.freeAt.x, t.freeAt.y, t.freeAt.z);
    char label[96];
    const auto check = [&](bool ok, const char* part) {
        q_snprintf(label, sizeof(label), "%s %s: %s", cur().gun, what, part);
        pt.tally.check(ok, poseTestName, label);
    };
    check(l.item && t.item, "an item in both hands");
    check(l.wraps == 1 && t.wraps == 1, "both hands wrap it (the prop's fingers)");
    check(hand < 0.25f && handTurn < 1.5f, "the hand placed alike");
    check(drawn < 0.3f && drawnTurn < 2.f, "the hand drawn alike (after the grasp)");
    check(inHand < 0.05f && inHandTurn < 1.f, "the item in the hand alike");
    check(za::abs(l.palmTurn - t.palmTurn) < 2.f, "the grasp's palm turn alike");
    check(curl < 0.05f, "the fingers alike");
}

void poseVerdict()
{
    Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
    Cvar_SetValueQuick(&vr_flat_prl_hold, 0.f);
    Cvar_SetValueQuick(&vr_flat_prl_test_miss, 0.f);
    pt.on = false;
    closeWindow();
    pt.tally.verdict(poseTestName);
}

void poseTestFrame()
{
    if(!pt.on)
    {
        return;
    }
    const flatprocreload::Status st = flatprocreload::status();
    pt.frames++;
    if(pt.frames > testStepFrames)
    {
        pt.tally.check(false, poseTestName, "timed out");
        poseVerdict();
        return;
    }
    const auto next = [&]() {
        pt.step++;
        pt.frames = 0;
        pt.settled = 0;
    };
    // Settled: the condition held poseSettle frames running.
    const auto settledOn = [&](bool cond) {
        pt.settled = cond ? pt.settled + 1 : 0;
        return pt.settled >= poseSettle;
    };
    switch(pt.step)
    {
        case 1: // the lock on the take key
            if(settledOn(st.locked && st.lockKey == pt.k1))
            {
                pt.tally.check(flatprocreload::poseSample(pt.lock1), poseTestName, "the lock's take pose sampled");
                if(pt.k2 >= 0)
                {
                    lockKey(pt.k2);
                }
                next();
            }
            break;
        case 2: // the lock on the carry key
            if(pt.k2 < 0 || settledOn(st.locked && st.lockKey == pt.k2))
            {
                if(pt.k2 >= 0)
                {
                    pt.tally.check(flatprocreload::poseSample(pt.lock2), poseTestName, "the lock's carry pose sampled");
                }
                Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
                next();
            }
            break;
        case 3: // let go (its ending played): a real reload, held at the take key
            if(settledOn(!st.locked && !st.tripOn))
            {
                Cvar_SetValueQuick(&vr_flat_prl_hold, static_cast<float>(pt.k1 + 1));
                Cbuf_AddText("impulse 55\n");
                next();
            }
            break;
        case 4:
            if(settledOn(st.tripOn && st.tripKey == pt.k1 && st.tripShare >= 1.f))
            {
                pt.tally.check(flatprocreload::poseSample(pt.trip1), poseTestName, "the real reload's take pose sampled");
                Cvar_SetValueQuick(&vr_flat_prl_hold, static_cast<float>(pt.k2 >= 0 ? pt.k2 + 1 : 0));
                next();
            }
            break;
        case 5:
            if(pt.k2 < 0 || settledOn(st.tripOn && st.tripKey == pt.k2 && st.tripShare >= 1.f))
            {
                if(pt.k2 >= 0)
                {
                    pt.tally.check(flatprocreload::poseSample(pt.trip2), poseTestName, "the real reload's carry pose sampled");
                }
                Cvar_SetValueQuick(&vr_flat_prl_hold, 0.f);
                Cvar_SetValueQuick(&vr_flat_prl_test_miss, 0.f);
                poseCompare("take", pt.lock1, pt.trip1);
                if(pt.k2 >= 0)
                {
                    poseCompare("carry", pt.lock2, pt.trip2);
                }
                next();
            }
            break;
        case 6: // the real reload goes on to its end
            if(!st.tripOn)
            {
                poseVerdict();
            }
            break;
        default: poseVerdict(); break;
    }
}

void startPoseTest()
{
    const flatprocreload::Status st = flatprocreload::status();
    pt = PoseTest{};
    pt.tally.reset();
    int gun = -1;
    for(int i = 0; i < reloadsys::count(); i++)
    {
        gun = reloadsys::at(i).weapon == st.lockWeapon ? i : gun;
    }
    if(st.lockGun < 0 || gun < 0)
    {
        pt.tally.check(false, poseTestName, "a gun with a procedural reload in one hand, the other empty");
        pt.tally.verdict(poseTestName);
        return;
    }
    openWindow();
    win.gun = gun;
    const Reload& r = cur();
    pt.k1 = reloadsys::takeKey(r);
    const int after = pt.k1 + 1;
    pt.k2 = after < r.keyCount && (r.keys[after].target == Target::Mouth || r.keys[after].target == Target::Dest) ? after : -1;
    Cvar_SetValueQuick(&vr_flat_prl_test_miss, 6.f);
    win.key = pt.k1;
    lockKey(pt.k1);
    pt.on = true;
    pt.step = 1;
    pt.frames = 0;
    Con_Printf("reloadstudio_pose: %s, keys %d (%s) and %d\n", r.gun, pt.k1 + 1, r.keys[za::max(pt.k1, 0)].name, pt.k2 + 1);
}

// ---- The frame strip (vr_reloadstudio strip; reloadstudio3, for his review): the gun in hand as for the pose test; the
// studio's orbit camera (its window's controls hidden), a screenshot of the lock on each key in turn, then of a real reload
// held at each key's end (vr_flat_prl_hold) in turn, the same keys; "reloadstudio_strip: key N lock|game" before each shot,
// a key the real reload skipped (its condition unmet) or never reached noted so, "reloadstudio_strip: N passed, M failed" at
// the end. Misc/quakevr/reloadstudio_strip.sh lays the shots out a row a key, studio left, game right.

constexpr const char* stripName = "reloadstudio_strip";
constexpr int stripShotFrames = 4; // frames after a screenshot is asked for before the scene changes
constexpr float stripNoPort = -1000.f; // a port radius no round is ever within (the game's carry and insert reach their ends)

struct Strip
{
    bool on{false};
    int step{0}; // 1 the lock's keys, 2 let go, 3 the real reload's keys, 4 its end
    int key{0};
    int frames{0};
    int settled{0};
    int shot{-1}; // frames since the screenshot was asked for (-1: not yet)
    cvar_t* port{nullptr}; // the gun's port radius (QC VR_Reload_PortRadius's vr_reload_port_<gun>_radius), while blocked
    float portWas{0.f};
    testkit::Tally tally;
};
Strip sp;

// The gun's port radius cvar (QC VR_Reload_PortRadius: the stock guns').
[[nodiscard]] cvar_t* portRadiusVar(int weapon)
{
    const char* name = weapon == WID_SUPER_SHOTGUN      ? "vr_reload_port_sshot_radius"
                       : weapon == WID_NAILGUN          ? "vr_reload_port_nail_radius"
                       : weapon == WID_SUPER_NAILGUN    ? "vr_reload_port_snail_radius"
                       : weapon == WID_LIGHTNING        ? "vr_reload_port_light_radius"
                       : weapon == WID_NAILPISTOL       ? "vr_reload_port_npistol_radius"
                       : weapon == WID_REVOLVER         ? "vr_reload_port_revolver_radius"
                       : weapon == WID_GRENADE_LAUNCHER ? "vr_reload_port_gl_radius"
                       : weapon == WID_PROXIMITY_GUN    ? "vr_reload_port_prox_radius"
                       : weapon == WID_ROCKET_LAUNCHER  ? "vr_reload_port_rl_radius"
                                                        : "vr_reload_port_shot_radius";
    return Cvar_FindVar(name);
}

// The game's port blocked (`on`: the round never goes in, so the carry and the insert are held at their ends as the lock
// shows them, not cut short as the round goes in on the way) or as it was.
void stripBlockPort(bool on, int weapon)
{
    if(on && !sp.port)
    {
        sp.port = portRadiusVar(weapon);
        sp.portWas = sp.port ? sp.port->value : 0.f;
        if(sp.port)
        {
            Cvar_SetValueQuick(sp.port, stripNoPort);
        }
    }
    else if(!on && sp.port)
    {
        Cvar_SetValueQuick(sp.port, sp.portWas);
        sp.port = nullptr;
    }
}

void stripVerdict()
{
    Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
    Cvar_SetValueQuick(&vr_flat_prl_hold, 0.f);
    stripBlockPort(false, 0);
    sp.on = false;
    win.hideUi = false;
    closeWindow();
    sp.tally.verdict(stripName);
}

void stripFrame()
{
    if(!sp.on)
    {
        return;
    }
    const flatprocreload::Status st = flatprocreload::status();
    const Reload& r = cur();
    sp.frames++;
    if(sp.frames > 3 * testStepFrames)
    {
        sp.tally.check(false, stripName, "timed out");
        stripVerdict();
        return;
    }
    const auto settledOn = [&](bool cond) {
        sp.settled = cond ? sp.settled + 1 : 0;
        return sp.settled >= poseSettle;
    };
    // The screenshot asked for, then stripShotFrames frames for it before going on; true once done.
    const auto shoot = [&](const char* what) {
        if(sp.shot < 0)
        {
            Con_Printf("%s: key %d %s (%s)\n", stripName, sp.key + 1, what, r.keys[sp.key].name);
            Cbuf_AddText("screenshot\n");
            sp.shot = 0;
        }
        return ++sp.shot > stripShotFrames;
    };
    const auto nextKey = [&]() {
        sp.key++;
        sp.frames = 0;
        sp.settled = 0;
        sp.shot = -1;
        // (The game's port blocked from the take to the insert: the round waits at each key's end, seated after.)
        const int take = reloadsys::takeKey(r), insert = reloadsys::insertKey(r);
        stripBlockPort(sp.step == 3 && take >= 0 && sp.key > take && sp.key <= insert, r.weapon);
    };
    switch(sp.step)
    {
        case 1:
            if((sp.shot >= 0 || settledOn(st.locked && st.lockKey == sp.key)) && shoot("lock"))
            {
                nextKey();
                if(sp.key < r.keyCount)
                {
                    lockKey(sp.key);
                }
                else
                {
                    Cvar_SetValueQuick(&vr_flat_prl_lock, 0.f);
                    sp.step = 2;
                }
            }
            break;
        case 2: // let go (its ending played): a real reload, held at the first key
            if(settledOn(!st.locked && !st.tripOn))
            {
                sp.key = -1;
                nextKey();
                Cvar_SetValueQuick(&vr_flat_prl_hold, 1.f);
                Cbuf_AddText("impulse 55\n");
                sp.step = 3;
            }
            break;
        case 3:
        {
            // (A key skipped, its condition unmet, or a reload over before it: noted, no shot.)
            const bool past = (st.tripOn && st.tripKey > sp.key) || (!st.tripOn && sp.frames > poseSettle);
            if(sp.shot < 0 && past)
            {
                Con_Printf("%s: key %d game skipped (%s)\n", stripName, sp.key + 1, r.keys[sp.key].name);
                sp.shot = stripShotFrames;
            }
            if((sp.shot >= 0 || settledOn(st.tripOn && st.tripKey == sp.key && st.tripShare >= 1.f)) && shoot("game"))
            {
                sp.tally.check(true, stripName, va("%s key %d done", r.gun, sp.key + 1));
                nextKey();
                Cvar_SetValueQuick(&vr_flat_prl_hold, sp.key < r.keyCount ? static_cast<float>(sp.key + 1) : 0.f);
                sp.step = sp.key < r.keyCount ? 3 : 4;
            }
            break;
        }
        case 4: // the real reload goes on to its end
            if(!st.tripOn)
            {
                stripVerdict();
            }
            break;
        default: stripVerdict(); break;
    }
}

void startStrip()
{
    const flatprocreload::Status st = flatprocreload::status();
    sp = Strip{};
    sp.tally.reset();
    int gun = -1;
    for(int i = 0; i < reloadsys::count(); i++)
    {
        gun = reloadsys::at(i).weapon == st.lockWeapon ? i : gun;
    }
    if(st.lockGun < 0 || gun < 0)
    {
        sp.tally.check(false, stripName, "a gun with a procedural reload in one hand, the other empty");
        sp.tally.verdict(stripName);
        return;
    }
    openWindow();
    win.gun = gun;
    win.orbit = true;
    win.hideUi = true;
    win.camYaw = 210.f;
    win.camPitch = 15.f;
    win.camDist = 40.f; // (from in front of him to his left, the free hand's side: the gun and both hands in the shot)
    win.camPan = glm::vec2{0.f};
    win.key = 0;
    Cvar_SetValueQuick(&vr_flat_prl_test_miss, 0.f);
    lockKey(0);
    sp.on = true;
    sp.step = 1;
    Con_Printf("%s: %s, %d keys\n", stripName, cur().gun, cur().keyCount);
}

void startTest()
{
    win.tally.reset();
    q_strlcpy(win.fileWas, vr_reload_file.string, sizeof(win.fileWas));
    Cvar_Set("vr_reload_file", testFile);
    openWindow();
    win.step = 1;
    win.stepFrames = 0;
}

void window_f()
{
    const char* sub = Cmd_Argc() > 1 ? Cmd_Argv(1) : "toggle";
    if(!ZA_STRCMP(sub, "open"))
    {
        openWindow();
    }
    else if(!ZA_STRCMP(sub, "close"))
    {
        closeWindow();
    }
    else if(!ZA_STRCMP(sub, "test"))
    {
        startTest();
    }
    else if(!ZA_STRCMP(sub, "posetest"))
    {
        startPoseTest();
    }
    else if(!ZA_STRCMP(sub, "strip"))
    {
        startStrip();
    }
    else if(!ZA_STRCMP(sub, "toggle"))
    {
        if(win.open)
        {
            closeWindow();
        }
        else
        {
            openWindow();
        }
    }
    else
    {
        Con_Printf("vr_reloadstudio [open|close|toggle|test|posetest|strip]: the Reload Studio's flat-screen ImGui window\n");
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_reloadstudio", window_f);
}

void frame()
{
    if(!win.open)
    {
        return;
    }
    if(VR_IsActive() || rigstudio::active() || waveeditor::shown() || weaponstudio::active() || armourstudio::windowOpen())
    {
        closeWindow();
        return;
    }
    // The preview's play: the scrub on through the keys.
    if(win.playing)
    {
        const double now = realtime;
        const float dt = win.lastReal >= 0.0 ? static_cast<float>(za::clamp(now - win.lastReal, 0.0, 0.1)) : 0.f;
        win.lastReal = now;
        float s = vr_flat_prl_scrub.value + dt * win.speed * reloadsys::playSpeed(cur()) / totalTime(cur());
        s = s > 1.f ? s - 1.f : s;
        Cvar_SetValueQuick(&vr_flat_prl_scrub, s);
        if(vr_flat_prl_lock.value != -1.f)
        {
            Cvar_SetValueQuick(&vr_flat_prl_lock, -1.f);
        }
    }
    imgui::setPanel(0, 0, 0, 1.f);
    imgui::want(true);
    if(!imgui::beginFrame())
    {
        return;
    }
    if(SDL_GetRelativeMouseMode())
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    if(SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
    {
        SDL_ShowCursor(SDL_ENABLE);
    }
    testFrame();
    poseTestFrame();
    stripFrame();
    if(!win.open || win.hideUi)
    {
        return;
    }
    drawWindow();
    gizmoFrame();
    if(win.askClose)
    {
        closeWindow();
    }
}

void shutdown()
{
    closeWindow();
}

bool open()
{
    return win.open;
}

bool cameraView(float* vieworg, float* viewangles)
{
    if(!win.open || !win.orbit || !flatplay::active() || flycam::on() || cl.stats[STAT_HEALTH] <= 0)
    {
        return false;
    }
    // Round his eye a little below it (the hands' height), turned off his view's yaw; panned across the camera's view.
    const float yaw = glm::radians(viewangles[1] + win.camYaw), pitch = glm::radians(win.camPitch);
    const glm::vec3 fwd{za::cos(pitch) * za::cos(yaw), za::cos(pitch) * za::sin(yaw), -za::sin(pitch)};
    const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3{0.f, 0.f, 1.f}));
    const glm::vec3 up = glm::cross(right, fwd);
    const glm::vec3 focus = glm::vec3{vieworg[0], vieworg[1], vieworg[2] - camFocusDown} + right * win.camPan.x + up * win.camPan.y;
    const glm::vec3 org = focus - fwd * win.camDist;
    for(int i = 0; i < 3; i++)
    {
        vieworg[i] = org[i];
    }
    viewangles[0] = win.camPitch;
    viewangles[1] += win.camYaw;
    viewangles[2] = 0.f;
    flycam::borrowView();
    return true;
}

} // namespace qvr::reloadstudio

// V_CalcRefdef, after the fly camera (vr_api.h): the Reload Studio's orbit camera over the view.
extern "C" void VR_ReloadStudioView(float* vieworg, float* viewangles)
{
    (void)qvr::reloadstudio::cameraView(vieworg, viewangles);
}
