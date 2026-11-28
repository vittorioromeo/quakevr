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
#include "vr_flatprocreload.hpp"
#include "vr_gizmo.hpp"
#include "vr_held.hpp"
#include "vr_imgui.hpp"
#include "vr_reloadsys.hpp"
#include "vr_rigstudio.hpp"
#include "vr_testkit.hpp"
#include "vr_waveeditor.hpp"
#include "vr_weapon_ids.h"
#include "vr_weaponstudio.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
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

void placeGizmo()
{
    win.h.shown = false;
    glm::vec3 framePos, handPos, handRot;
    glm::mat3 frame;
    int kind = 0;
    const flatprocreload::Status st = flatprocreload::status();
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

void dragTo(const glm::vec2& m)
{
    if(!keyValid())
    {
        return;
    }
    Key& k = cur().keys[win.key];
    const gizmo::Handle& h = win.h0;
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

void gizmoFrame()
{
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
    ImGui::SeparatorText("Gizmo on the Hand");
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
    ImGui::TextDisabled("The handle shows on the free hand while this key is locked.");
    ImGui::PopID();
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
        const char* guns[8];
        const int n = za::min(reloadsys::count(), 8);
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
            Cbuf_AddText(cur().weapon == WID_SUPER_SHOTGUN ? "impulse 155\n" : "impulse 154\n");
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
                next();
            }
            break;
        default: testVerdict(); break;
    }
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
        Con_Printf("vr_reloadstudio [open|close|toggle|test]: the Reload Studio's flat-screen ImGui window\n");
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
        float s = vr_flat_prl_scrub.value + dt * win.speed / totalTime(cur());
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
    if(!win.open)
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

} // namespace qvr::reloadstudio
