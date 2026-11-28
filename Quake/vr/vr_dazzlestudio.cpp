// vr_dazzlestudio.cpp -- the Dazzle Studio (vr_dazzlestudio.hpp; his words 2026-11-28, item 14: "An imgui-based dazzle
// studio with gizmos would be the best option here"): the flashlight dazzle's shield pose tweaked per armed humanoid kind
// (vr_dazzledata.hpp) in an ImGui window over flat-screen play (vr_dazzlestudio [open|close|toggle|test]; Debug > Tools >
// Dazzle Studio; Tunables > Flashlight Dazzle).
//
// Its subject: an armed humanoid of the kind picked (vr_dazzlestudio_kind) put ahead of you (QC VR_DazzleStudio_Spawn, on
// the studios' shared VR_StudioSubject: the Armour Studio's), holding its own weapon (the single set) or two revolvers (the
// dual set, vr_dazzlestudio_dual), held in its shield pose by .vr_ag_dazzle_hold (frozen at full, or its hand rising and
// falling: vr_dazzlestudio_live), every armed humanoid held still meanwhile (vr_agrunt_freeze, put back on closing). The
// right-button drag turns it (vr_dazzlestudio_turn), as the Armour Studio's.
//
// The edits go straight into dazzledata's table, which QC's pose reads every frame (dazzletweak): the subject, and every
// dazzled humanoid of the kind, show them at once. The shared gizmo (vr_gizmo.hpp, as the Reload Studio's and the Armour
// Studio's) sits on the target picked (its dot clicked, or keys 1 2 3): the shielding hand (moves its offset; with two
// weapons, E turns the left weapon in it), the elbow (moves its way) or the gun hand (moves it away). Sliders for every
// value, the forearm's twist among them. Save writes quakevr/data/dazzle.txt; Revert reads it again.
//
// Input as the other ImGui tools': while it is open ImGui has the keys and the mouse (vr_imgui.cpp); flat screen only;
// never with another ImGui tool open (it closes when one opens).

#include "vr_dazzlestudio.hpp"

#include "vr_armourstudio.hpp"
#include "vr_cvars.hpp"
#include "vr_dazzledata.hpp"
#include "vr_engine.hpp"
#include "vr_gizmo.hpp"
#include "vr_held.hpp"
#include "vr_hitreact.hpp"
#include "vr_imgui.hpp"
#include "vr_progs.hpp"
#include "vr_reloadstudio.hpp"
#include "vr_rigstudio.hpp"
#include "vr_testkit.hpp"
#include "vr_waveeditor.hpp"
#include "vr_weapon_ids.h"
#include "vr_weaponstudio.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "vr_api.h"

#include <glm/gtc/quaternion.hpp>

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>

extern "C" int VR_IsActive(void);

namespace qvr::dazzlestudio
{

namespace
{

using dazzledata::Tweak;

constexpr const char* testName = "dazzlestudio";
constexpr const char* testFile = "data/dazzle_studiotest.txt";
constexpr int testStepFrames = 900; // frames a test step may wait for its condition (a safety net: FAIL "timed out")
constexpr float poleUnits = 8.f;    // the elbow's handle: units dragged for 1 of its way's offset
constexpr float livePeriod = 3.f;   // seconds of the live pose's rise and fall
constexpr float degPerPixel = 0.5f; // the right-button drag's turn

enum Target : int
{
    TargetHand,
    TargetElbow,
    TargetGun,
    TargetCount
};
constexpr const char* targetLabels[TargetCount] = {"Hand", "Elbow", "Gun Arm"};

// What the subject shows now (world).
struct Pose
{
    bool ok{false};
    float yaw{0.f};
    float shieldw{0.f};
    bool dual{false};
    glm::vec3 at[TargetCount]{glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}}; // the hand sent, its elbow drawn, the gun hand drawn
    bool shown[TargetCount]{false, false, false};
};

struct Window
{
    bool open{false};
    bool askClose{false};
    int target{TargetHand};
    bool rotate{false};
    // The subject.
    int subject{0};
    int subjectKind{-1};
    int subjectDual{-1};
    char subjectMap[64]{};
    bool froze{false};
    float freezeWas{0.f};
    // The gizmo's drag, the turn's.
    gizmo::Handle h, h0;
    gizmo::Projection view;
    bool dragging{false};
    bool turning{false};
    int part{-1}, hover{-1};
    glm::vec2 start{0.f}, last{0.f};
    glm::vec3 planeStart{0.f};
    glm::vec3 axes0[3]{glm::vec3{1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f}, glm::vec3{0.f, 0.f, 1.f}};
    Tweak t0;
    float sweep{0.f};
    // The test.
    int step{0};
    int stepFrames{0};
    int still{0};
    bool shots{false}; // (test shots: screenshots on the way)
    int shotAt{0};
    char fileWas[64]{};
    glm::vec3 prev[TargetCount]{glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}}; // last frame's (settled)
    glm::vec3 base[TargetCount]{glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}}; // with no tweaks
    glm::vec3 dualBase{0.f};
    testkit::Tally tally;
};
Window win; // seat-shared: one debug tool's window (as the Armour Studio)

[[nodiscard]] int kindNow()
{
    return za::clamp(static_cast<int>(vr_dazzlestudio_kind.value), 0, dazzledata::kindCount - 1);
}

[[nodiscard]] int setNow()
{
    return vr_dazzlestudio_dual.value != 0.f ? 1 : 0;
}

[[nodiscard]] Tweak& cur()
{
    return dazzledata::at(kindNow(), setNow());
}

// Its own weapon (the single set's), as the dazzle test's kinds hold.
[[nodiscard]] int weaponOf(int kind)
{
    return kind == 0 ? WID_SHOTGUN : kind == 1 ? WID_NAILGUN : kind == 5 ? WID_ADCROSSBOW : WID_SWORD;
}

// Its body's axes (forward, left, up) by its yaw.
void bodyAxes(float yawDeg, glm::vec3 out[3])
{
    const float y = yawDeg * (3.14159265f / 180.f);
    out[0] = glm::vec3{za::cos(y), za::sin(y), 0.f};
    out[1] = glm::vec3{-za::sin(y), za::cos(y), 0.f};
    out[2] = glm::vec3{0.f, 0.f, 1.f};
}

[[nodiscard]] glm::vec3 toWorld(float yawDeg, const glm::vec3& l)
{
    glm::vec3 a[3];
    bodyAxes(yawDeg, a);
    return a[0] * l.x + a[1] * l.y + a[2] * l.z;
}

// The turn `v` (pitch, yaw, roll as QC's makevectors reads them) after a turn of `a` radians about its frame's axis `axis`.
[[nodiscard]] glm::vec3 turnAfter(const glm::vec3& v, int axis, float a)
{
    glm::vec3 e{0.f};
    e[axis] = 1.f;
    const glm::mat3 r = glm::mat3_cast(glm::angleAxis(a, e));
    glm::vec3 out;
    held::anglesFromAxes(r * held::axesFromAngles(&v[0], true), &out[0], true);
    return out;
}

// A QC call with the player as self (the studios' spawners).
void spawnCall(int kind, int wid, int wid2)
{
    if(!sv.active)
    {
        return;
    }
    const progs::ServerVm vm;
    const func_t fn = progs::findFunction("VR_DazzleStudio_Spawn");
    edict_t* player = progs::playerEdict();
    if(!fn || !player)
    {
        win.subject = 0;
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_FLOAT(OFS_PARM0) = static_cast<float>(kind);
    G_FLOAT(OFS_PARM1) = za::clamp(vr_dazzlestudio_ahead.value, 24.f, 256.f);
    G_FLOAT(OFS_PARM2) = static_cast<float>(wid);
    G_FLOAT(OFS_PARM3) = static_cast<float>(wid2);
    PR_ExecuteProgram(fn);
    edict_t* made = PROG_TO_EDICT(G_INT(OFS_RETURN));
    win.subject = made && made != sv.qcvm.edicts ? NUM_FOR_EDICT(made) : 0;
}

void removeSubject()
{
    if(win.subject || win.subjectKind >= 0)
    {
        spawnCall(-1, 0, 0);
    }
    win.subject = 0;
    win.subjectKind = -1;
    win.subjectDual = -1;
}

void spawnSubject()
{
    const int kind = kindNow();
    const int dual = setNow();
    spawnCall(kind, dual ? WID_REVOLVER : weaponOf(kind), dual ? WID_REVOLVER : 0);
    win.subjectKind = kind;
    win.subjectDual = dual;
    q_strlcpy(win.subjectMap, sv.name, sizeof(win.subjectMap));
    Con_Printf("dazzlestudio: %s (%s) %s\n", dazzledata::kindLabel(kind), dual ? "two revolvers" : "its own weapon",
        win.subject ? "put ahead of you" : "could not be put there (AD's kinds need AD's data)");
}

[[nodiscard]] Pose readPose()
{
    Pose p;
    if(!win.subject || !sv.active)
    {
        return p;
    }
    {
        const progs::ServerVm vm;
        edict_t* e = progs::edictAt(win.subject);
        if(!e)
        {
            return p;
        }
        p.ok = true;
        p.yaw = e->v.angles[1];
        const progs::FieldOffsets& f = progs::fields();
        p.shieldw = progs::fieldFloatOr(e, f.vr_ag_shieldw, 0.f);
        p.dual = f.vr_ag_prop2 >= 0 && progs::fieldInt(e, f.vr_ag_prop2) != 0;
        p.at[TargetHand] = progs::fieldVec(e, f.vr_ag_shield_at);
        p.shown[TargetHand] = p.shieldw > 0.01f;
    }
    glm::vec3 sh, el, hd;
    if(hitreact::drawnArm(win.subject, 1, sh, el, hd))
    {
        p.at[TargetElbow] = el;
        p.shown[TargetElbow] = true;
    }
    if(hitreact::drawnArm(win.subject, 0, sh, el, hd))
    {
        p.at[TargetGun] = hd;
        p.shown[TargetGun] = true;
    }
    return p;
}

// Each frame while open: the subject there (a new one for a new kind or set, or a new map), facing you as turned, held in
// its shield pose.
void keepSubject()
{
    const bool ready = sv.active && cls.signon == SIGNONS && progs::localPlayer() != nullptr;
    if(!ready)
    {
        win.subject = 0;
        win.subjectKind = -1;
        return;
    }
    if(!win.froze)
    {
        win.froze = true;
        win.freezeWas = vr_agrunt_freeze.value;
        Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f);
    }
    const bool gone = win.subject && [] {
        const progs::ServerVm vm;
        return progs::edictAt(win.subject) == nullptr;
    }();
    if(win.subjectKind != kindNow() || win.subjectDual != setNow() || gone || ZA_STRCMP(win.subjectMap, sv.name) != 0)
    {
        spawnSubject();
    }
    if(!win.subject)
    {
        return;
    }
    const progs::ServerVm vm;
    edict_t* e = progs::edictAt(win.subject);
    const edict_t* player = progs::playerEdict();
    if(!e || !player)
    {
        return;
    }
    const float dx = player->v.origin[0] - e->v.origin[0], dy = player->v.origin[1] - e->v.origin[1];
    e->v.angles[1] = za::atan2(dy, dx) * (180.f / 3.14159265f) + vr_dazzlestudio_turn.value;
    e->v.takedamage = DAMAGE_NO;
    float hold = 1.f;
    if(vr_dazzlestudio_live.value != 0.f)
    {
        const double t = qcvm->time / static_cast<double>(livePeriod);
        const float ph = static_cast<float>(t - static_cast<double>(static_cast<long long>(t)));
        hold = za::max(0.001f, 1.f - za::abs(2.f * ph - 1.f));
    }
    progs::setFieldFloat(e, progs::fields().vr_ag_dazzle_hold, hold);
}

void edited()
{
    dazzledata::touched();
}

// ---- The gizmo on the target picked -----------------------------------------------------------------------------------

[[nodiscard]] bool rotateOk()
{
    return win.target == TargetHand && setNow() == 1;
}

void placeGizmo(const Pose& p)
{
    win.h.shown = false;
    if(!p.ok || !p.shown[win.target])
    {
        return;
    }
    win.h.centre = p.at[win.target];
    bodyAxes(p.yaw, win.h.axis);
    const bool rot = win.rotate && rotateOk();
    win.h.move = !rot;
    win.h.rot = rot;
    win.h.scale = false;
    gizmo::place(win.h, win.view, 60.f);
}

// The table's change for the drag to `m` (from its start: the tweak as it began plus the move or turn since).
void dragTo(const glm::vec2& m)
{
    Tweak& t = cur();
    const gizmo::Handle& h = win.h0;
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
        const glm::vec3 l{glm::dot(world, win.axes0[0]), glm::dot(world, win.axes0[1]), glm::dot(world, win.axes0[2])};
        if(win.target == TargetHand)
        {
            t.hand = win.t0.hand + l;
        }
        else if(win.target == TargetElbow)
        {
            t.elbow = win.t0.elbow + l / poleUnits;
        }
        else
        {
            t.gun = win.t0.gun + l;
        }
    }
    else if(win.part <= 2)
    {
        win.sweep += gizmo::ringSweepExact(h, win.view, h.axis[win.part], win.last, m);
        t.handTurn = turnAfter(win.t0.handTurn, win.part, win.sweep);
    }
    win.last = m;
    edited();
}

// The right-button drag: the subject turned (not over the window, nor while the handle drags).
void turnFrame(const ImGuiIO& io)
{
    if(win.turning && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        win.turning = false;
    }
    else if(!win.turning && !win.dragging && !io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        win.turning = true;
        return;
    }
    if(win.turning && io.MouseDelta.x != 0.f)
    {
        float turn = vr_dazzlestudio_turn.value - io.MouseDelta.x * degPerPixel;
        turn = turn > 180.f ? turn - 360.f : turn < -180.f ? turn + 360.f : turn;
        Cvar_SetValueQuick(&vr_dazzlestudio_turn, turn);
    }
}

void gizmoFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    turnFrame(io);
    win.view = gizmo::refdefProjection(io.DisplaySize.x, io.DisplaySize.y);
    if(!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt)
    {
        constexpr ImGuiKey targetKeys[TargetCount] = {ImGuiKey_1, ImGuiKey_2, ImGuiKey_3};
        for(int i = 0; i < TargetCount; i++)
        {
            win.target = ImGui::IsKeyPressed(targetKeys[i], false) && !win.dragging ? i : win.target;
        }
        win.rotate = ImGui::IsKeyPressed(ImGuiKey_W, false) ? false : ImGui::IsKeyPressed(ImGuiKey_E, false) ? true : win.rotate;
    }
    const Pose p = readPose();
    placeGizmo(p);
    const glm::vec2 m{io.MousePos.x, io.MousePos.y};
    if(win.dragging)
    {
        if(!ImGui::IsMouseDown(ImGuiMouseButton_Left) || !p.ok)
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
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    // The targets' dots: the one picked white, a click on another picks it.
    int dotHover = -1;
    for(int i = 0; i < TargetCount; i++)
    {
        glm::vec2 s;
        if(!p.ok || !p.shown[i] || !win.view.project(p.at[i], s))
        {
            continue;
        }
        const bool near = !overUi && win.hover < 0 && glm::distance(s, m) < 9.f;
        dotHover = near ? i : dotHover;
        const ImU32 col = i == win.target ? IM_COL32(255, 255, 255, 255) : near ? IM_COL32(255, 230, 120, 255) : IM_COL32(255, 170, 60, 220);
        dl->AddCircleFilled(ImVec2(s.x, s.y), i == win.target ? 4.f : 5.5f, col);
        dl->AddText(ImVec2(s.x + 8.f, s.y - 7.f), col, targetLabels[i]);
    }
    if(!win.dragging && dotHover >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        win.target = dotHover;
        return;
    }
    if(!win.dragging && win.hover >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        win.dragging = true;
        win.part = win.hover;
        win.start = win.last = m;
        win.h0 = win.h;
        win.sweep = 0.f;
        win.t0 = cur();
        for(int i = 0; i < 3; i++)
        {
            win.axes0[i] = win.h.axis[i];
        }
        const glm::vec3 n = win.part >= 4 && win.part <= 6 ? win.h.axis[win.part - 4] : win.view.fwd;
        if(!win.view.rayPlane(m, win.h.centre, n, win.planeStart))
        {
            win.planeStart = win.h.centre;
        }
    }
    if(win.h.shown)
    {
        gizmo::draw(win.h, win.view, dl, win.hover);
    }
}

// ---- The window --------------------------------------------------------------------------------------------------

bool editVec(const char* label, glm::vec3& v, float speed, const char* tip)
{
    const bool c = ImGui::DragFloat3(label, &v[0], speed, -360.f, 360.f, "%.2f");
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tip);
    }
    if(c)
    {
        edited();
    }
    return c;
}

void drawWindow(const Pose& p)
{
    // (narrow, at the right: the subject stands in the middle of the view)
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(ImVec2(380.f, za::min(680.f, display.y - 30.f)), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(display.x - 390.f, 15.f), ImGuiCond_FirstUseEver);
    bool keep = true;
    const bool shown = ImGui::Begin("Dazzle Studio", &keep, ImGuiWindowFlags_NoSavedSettings);
    if(!keep)
    {
        win.askClose = true;
    }
    if(shown)
    {
        const char* kinds[dazzledata::kindCount];
        for(int i = 0; i < dazzledata::kindCount; i++)
        {
            kinds[i] = dazzledata::kindLabel(i);
        }
        int kind = kindNow();
        ImGui::SetNextItemWidth(200.f);
        if(ImGui::Combo("Kind", &kind, kinds, dazzledata::kindCount))
        {
            Cvar_SetValueQuick(&vr_dazzlestudio_kind, static_cast<float>(kind));
        }
        int set = setNow();
        if(ImGui::RadioButton("One Weapon", set == 0))
        {
            Cvar_SetValueQuick(&vr_dazzlestudio_dual, 0.f);
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("Two Weapons (Dual Wielding)", set == 1))
        {
            Cvar_SetValueQuick(&vr_dazzlestudio_dual, 1.f);
        }
        const bool live = vr_dazzlestudio_live.value != 0.f;
        if(ImGui::RadioButton("Frozen", !live))
        {
            Cvar_SetValueQuick(&vr_dazzlestudio_live, 0.f);
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("Live (Rise and Fall)", live))
        {
            Cvar_SetValueQuick(&vr_dazzlestudio_live, 1.f);
        }
        float turn = vr_dazzlestudio_turn.value;
        ImGui::SetNextItemWidth(200.f);
        if(ImGui::SliderFloat("Subject Turn", &turn, -180.f, 180.f, "%.0f deg"))
        {
            Cvar_SetValueQuick(&vr_dazzlestudio_turn, turn);
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Or drag with the right mouse button over the view");
        }
        if(p.ok)
        {
            ImGui::Text("Subject: %s, shield %.0f%%%s", dazzledata::kindLabel(kindNow()), p.shieldw * 100.f,
                setNow() == 1 && !p.dual ? " (its second revolver not in hand yet)" : "");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.4f, 1.f), "No subject (a level needed; AD's kinds need AD's data)");
        }
        const bool unsaved = dazzledata::revision() != dazzledata::savedRevision();
        if(ImGui::Button("Save"))
        {
            Con_Printf("dazzlestudio: %s %s\n", dazzledata::filePath(), dazzledata::save() ? "written" : "COULD NOT BE WRITTEN");
        }
        ImGui::SameLine();
        if(ImGui::Button("Revert"))
        {
            (void)dazzledata::load();
        }
        ImGui::SameLine();
        if(ImGui::Button("Reset This Set"))
        {
            cur() = Tweak{};
            edited();
        }
        ImGui::SameLine();
        if(ImGui::Button("Copy to Other Set"))
        {
            dazzledata::at(kindNow(), 1 - setNow()) = cur();
            edited();
        }
        ImGui::SameLine();
        ImGui::TextColored(unsaved ? ImVec4(1.f, 0.8f, 0.3f, 1.f) : ImVec4(0.6f, 0.9f, 0.6f, 1.f), unsaved ? "unsaved" : "saved");
        ImGui::SeparatorText("Gizmo (click a dot, or 1 2 3; W move, E turn)");
        for(int i = 0; i < TargetCount; i++)
        {
            if(i)
            {
                ImGui::SameLine();
            }
            if(ImGui::RadioButton(targetLabels[i], win.target == i))
            {
                win.target = i;
            }
        }
        if(ImGui::RadioButton("Move", !win.rotate || !rotateOk()))
        {
            win.rotate = false;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!rotateOk());
        if(ImGui::RadioButton("Turn (the hand, two weapons)", win.rotate && rotateOk()))
        {
            win.rotate = true;
        }
        ImGui::EndDisabled();
        Tweak& t = cur();
        ImGui::SeparatorText("Shielding Hand");
        editVec("Offset", t.hand, 0.05f, "Its grip moved from the base pose: forward, left, up in its body (units)");
        ImGui::BeginDisabled(setNow() == 0);
        editVec("Weapon Turn", t.handTurn, 0.25f,
            "Two weapons: the left weapon turned in the shielding hand (pitch, yaw, roll; degrees). One weapon: a bare hand "
            "points along its forearm (the pose message has no wrist turn): use the forearm's twist");
        ImGui::EndDisabled();
        ImGui::SeparatorText("Elbow and Forearm");
        editVec("Elbow Way", t.elbow, 0.01f, "Added to its elbow's way ('0.3 1 -0.45': out to its side, down a little; forward, left, up)");
        if(ImGui::DragFloat("Forearm Twist", &t.twist, 0.25f, -180.f, 180.f, "%.1f deg"))
        {
            edited();
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("The elbow turned about the line from its shoulder to its hand: the forearm rolled across its face");
        }
        ImGui::SeparatorText("Gun Arm");
        editVec("Moved Away", t.gun, 0.05f, "The gun hand moved while it shields: forward, left, up in its body (units; times the shield's share)");
        ImGui::Text("Amount %.1f units", glm::length(t.gun));
        ImGui::SeparatorText("");
        ImGui::TextWrapped("Every armed humanoid of this kind dazzled by a flashlight takes these at once (the server's pose: "
                           "every client sees it). Dual wielders shield only with Tunables > Flashlight Dazzle > Dual "
                           "Wielders Shield on; the studio's subject always does.");
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
        Con_Printf("dazzlestudio: the Dazzle Studio is flat-screen only (start the game with -flatscreenmode)\n");
        return;
    }
    if(rigstudio::active() || waveeditor::shown() || waveeditor::holds() || weaponstudio::active() || armourstudio::windowOpen() ||
       reloadstudio::open())
    {
        Con_Printf("dazzlestudio: close the other ImGui tool first\n");
        return;
    }
    if(!imgui::ensure())
    {
        Con_Printf("dazzlestudio: ImGui could not be started\n");
        return;
    }
    win.open = true;
    win.askClose = false;
    Key_ClearStates();
    key_dest = key_game;
    m_state = m_none;
    Con_Printf("dazzlestudio: window open -- vr_dazzlestudio toggles it, its X closes it\n");
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
    win.turning = false;
    removeSubject();
    if(win.froze && vr_agrunt_freeze.value != 0.f)
    {
        Cvar_SetValueQuick(&vr_agrunt_freeze, win.freezeWas); // (only while still frozen, as the Armour Studio's)
    }
    win.froze = false;
    imgui::want(false);
    IN_Activate();
    Con_Printf("dazzlestudio: window closed (the armed humanoids free again: vr_agrunt_freeze %g)\n", vr_agrunt_freeze.value);
}

// ---- The test (vr_dazzlestudio test; a level loaded, flat screen): the window opens on a frozen shotgun grunt shielding;
// zero tweaks; a hand tweak moves the hand sent by exactly its offset; the gun hand and the elbow follow theirs; the gizmo's
// math; saved to a test file, cleared, read back; cleared again: the pose as with no tweaks, exactly; the dual set on two
// revolvers apart from the single set; closed: the subject gone, the humanoids free. ------------------------------------

void testVerdict()
{
    Cvar_SetValueQuick(&vr_dazzlestudio_dual, 0.f);
    Cvar_Set("vr_dazzle_file", win.fileWas);
    (void)dazzledata::load();
    win.step = 0;
    closeWindow();
    win.tally.verdict(testName);
}

// The subject shielding fully and drawn (with two weapons: both in hand), the same for `frames` frames in a row (the drawn
// arms settled).
[[nodiscard]] bool settled(const Pose& p, int frames, int set)
{
    const bool ok = p.ok && p.shieldw > 0.999f && p.shown[TargetElbow] && p.shown[TargetGun] && (set == 0 || p.dual);
    bool same = ok;
    for(int i = 0; i < TargetCount; i++)
    {
        same = same && glm::distance(p.at[i], win.prev[i]) < 0.005f;
        win.prev[i] = p.at[i];
    }
    win.still = same ? win.still + 1 : 0;
    return ok && win.still >= frames;
}

// `vr_dazzlestudio test shots`: a screenshot of the window and the gizmo on target `target` (his review), then on once it
// is taken (a few frames).
[[nodiscard]] bool shot(int target, bool rotate)
{
    if(!win.shots)
    {
        return true;
    }
    if(win.shotAt == 0)
    {
        win.target = target;
        win.rotate = rotate;
        win.shotAt = win.stepFrames;
        return false;
    }
    if(win.stepFrames == win.shotAt + 3)
    {
        Cbuf_AddText("screenshot\n"); // (the gizmo moved there and drawn first)
    }
    if(win.stepFrames < win.shotAt + 8)
    {
        return false;
    }
    win.shotAt = 0;
    return true;
}

[[nodiscard]] bool near3(const glm::vec3& a, const glm::vec3& b, float within)
{
    return glm::distance(a, b) < within;
}

void testFrame()
{
    if(win.step == 0)
    {
        return;
    }
    testkit::Tally& t = win.tally;
    win.stepFrames++;
    const Pose p = readPose();
    const auto next = [&]() {
        win.step++;
        win.stepFrames = 0;
        win.still = 0;
    };
    if(win.stepFrames > testStepFrames)
    {
        t.check(false, testName, va("timed out at step %d (subject %d, shield %.2f, drawn %d %d, dual %d)", win.step, win.subject,
            p.shieldw, p.shown[TargetElbow], p.shown[TargetGun], p.dual));
        testVerdict();
        return;
    }
    constexpr glm::vec3 handTweak{3.f, -2.f, 1.f};
    switch(win.step)
    {
        case 1: // a frozen shotgun grunt, zero tweaks: shielding, settled
            if(settled(p, 20, 0) && shot(TargetHand, false))
            {
                t.check(win.open && imgui::wanted(), testName, "the window open, ImGui wanted");
                t.check(vr_agrunt_freeze.value != 0.f, testName, "the armed humanoids held still");
                for(int i = 0; i < TargetCount; i++)
                {
                    win.base[i] = p.at[i];
                }
                Con_Printf("dazzlestudio: base hand %.2f %.2f %.2f, elbow %.2f %.2f %.2f, gun hand %.2f %.2f %.2f\n", p.at[0].x,
                    p.at[0].y, p.at[0].z, p.at[1].x, p.at[1].y, p.at[1].z, p.at[2].x, p.at[2].y, p.at[2].z);
                cur().hand = handTweak;
                edited();
                next();
            }
            break;
        case 2: // the hand sent moved by exactly its offset (its body's frame)
            if(settled(p, 10, 0))
            {
                const glm::vec3 want = win.base[TargetHand] + toWorld(p.yaw, handTweak);
                const glm::vec3 d = p.at[TargetHand] - win.base[TargetHand];
                t.check(near3(p.at[TargetHand], want, 0.02f), testName,
                    va("the hand moved by its offset (moved %.3f %.3f %.3f, %.3f off)", d.x, d.y, d.z, glm::distance(p.at[TargetHand], want)));
                Con_Printf("dazzlestudio: hand tweak 3 -2 1: moved %.3f (wanted %.3f), %.4f off\n", glm::length(d),
                    glm::length(handTweak), glm::distance(p.at[TargetHand], want));
                cur().hand = glm::vec3{0.f};
                cur().gun = glm::vec3{0.f, -4.f, 0.f};
                edited();
                next();
            }
            break;
        case 3: // the gun hand moved away (to its right), the shield's hand back
            if(settled(p, 10, 0) && shot(TargetGun, false))
            {
                const glm::vec3 d = p.at[TargetGun] - win.base[TargetGun];
                const float right = -glm::dot(d, toWorld(p.yaw, glm::vec3{0.f, 1.f, 0.f}));
                t.check(right > 2.f, testName, va("the gun hand moved away to its right (%.2f units of 4)", right));
                t.check(near3(p.at[TargetHand], win.base[TargetHand], 0.01f), testName, "the shield's hand back with its offset zero");
                Con_Printf("dazzlestudio: gun tweak 0 -4 0: the gun hand %.2f to its right\n", right);
                cur().gun = glm::vec3{0.f};
                cur().elbow = glm::vec3{0.f, 0.f, -1.f};
                edited();
                next();
            }
            break;
        case 4: // the elbow lower
            if(settled(p, 10, 0) && shot(TargetElbow, false))
            {
                const float down = win.base[TargetElbow].z - p.at[TargetElbow].z;
                t.check(down > 1.f, testName, va("the elbow lower by its way's tweak (%.2f units)", down));
                Con_Printf("dazzlestudio: elbow tweak 0 0 -1: the elbow %.2f lower\n", down);
                cur().elbow = glm::vec3{0.f};
                cur().twist = 40.f;
                edited();
                next();
            }
            break;
        case 5: // the forearm twisted: the elbow elsewhere, the hand where it was
            if(settled(p, 10, 0))
            {
                const float moved = glm::distance(p.at[TargetElbow], win.base[TargetElbow]);
                t.check(moved > 1.f, testName, va("the forearm's twist moves the elbow (%.2f units)", moved));
                t.check(near3(p.at[TargetHand], win.base[TargetHand], 0.01f), testName, "the twist keeps the hand where it was");
                Con_Printf("dazzlestudio: twist 40: the elbow %.2f away\n", moved);
                // The gizmo's math: a world move along its body's axes is its offset's; a turn there and back.
                const glm::vec3 r0{10.f, 20.f, 30.f};
                const glm::vec3 r1 = turnAfter(turnAfter(r0, 2, 0.5f), 2, -0.5f);
                t.check(glm::distance(r0, r1) < 0.05f, testName, "the gizmo's turn there and back");
                const glm::vec3 r2 = turnAfter(glm::vec3{0.f}, 2, 0.5f);
                t.check(za::abs(r2.y - 28.6479f) < 0.05f && za::abs(r2.x) < 0.01f && za::abs(r2.z) < 0.01f, testName,
                    va("a turn about up is a yaw (%.2f %.2f %.2f)", r2.x, r2.y, r2.z));
                // Saved, cleared, read back.
                cur().hand = handTweak;
                dazzledata::at(3, 1).gun = glm::vec3{1.f, 2.f, 3.f};
                edited();
                t.check(dazzledata::save(), testName, "saved (the test's file)");
                dazzledata::clear();
                t.check(cur().zero(), testName, "cleared");
                const bool read = dazzledata::load();
                t.check(read && near3(cur().hand, handTweak, 1e-4f) && za::abs(cur().twist - 40.f) < 1e-4f &&
                            near3(dazzledata::at(3, 1).gun, glm::vec3{1.f, 2.f, 3.f}, 1e-4f) && dazzledata::at(3, 0).zero(),
                    testName, "read back from the file");
                dazzledata::clear();
                next();
            }
            break;
        case 6: // no tweaks again: the pose as with none, exactly
            if(settled(p, 20, 0))
            {
                t.check(near3(p.at[TargetHand], win.base[TargetHand], 0.001f), testName,
                    va("no tweaks: the hand as before (%.4f off)", glm::distance(p.at[TargetHand], win.base[TargetHand])));
                t.check(near3(p.at[TargetElbow], win.base[TargetElbow], 0.01f) && near3(p.at[TargetGun], win.base[TargetGun], 0.01f),
                    testName, va("no tweaks: the arms as before (elbow %.4f, gun hand %.4f off)", glm::distance(p.at[TargetElbow],
                        win.base[TargetElbow]), glm::distance(p.at[TargetGun], win.base[TargetGun])));
                Cvar_SetValueQuick(&vr_dazzlestudio_dual, 1.f); // (two revolvers: the dual set)
                next();
            }
            break;
        case 7: // two revolvers, shielding with the left one's hand
            if(settled(p, 20, 1))
            {
                win.dualBase = p.at[TargetHand];
                cur().hand = glm::vec3{0.f, 0.f, 2.f};
                dazzledata::at(0, 0).hand = glm::vec3{5.f, 5.f, 5.f}; // (the single set: not this one's)
                edited();
                next();
            }
            break;
        case 8:
            if(settled(p, 10, 1) && shot(TargetHand, true))
            {
                const glm::vec3 want = win.dualBase + glm::vec3{0.f, 0.f, 2.f};
                t.check(near3(p.at[TargetHand], want, 0.02f), testName,
                    va("two weapons: the dual set's offset alone moves it (%.3f off)", glm::distance(p.at[TargetHand], want)));
                const int subject = win.subject;
                closeWindow();
                const progs::ServerVm vm;
                t.check(progs::edictAt(subject) == nullptr, testName, "closed: the subject gone");
                t.check(vr_agrunt_freeze.value == 0.f, testName, "closed: the armed humanoids free again");
                testVerdict(); // (now: the window's frame no longer runs)
            }
            break;
        default: testVerdict(); break;
    }
}

void startTest()
{
    win.tally.reset();
    win.shots = Cmd_Argc() > 2 && !ZA_STRCMP(Cmd_Argv(2), "shots");
    win.shotAt = 0;
    q_strlcpy(win.fileWas, vr_dazzle_file.string, sizeof(win.fileWas));
    Cvar_Set("vr_dazzle_file", testFile);
    dazzledata::clear();
    Cvar_SetValueQuick(&vr_dazzlestudio_kind, 0.f);
    Cvar_SetValueQuick(&vr_dazzlestudio_dual, 0.f);
    Cvar_SetValueQuick(&vr_dazzlestudio_live, 0.f);
    Cvar_SetValueQuick(&vr_dazzlestudio_turn, 0.f);
    openWindow();
    if(!win.open)
    {
        win.tally.check(false, testName, "the window did not open");
        testVerdict();
        return;
    }
    win.step = 1;
    win.stepFrames = 0;
    win.still = 0;
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
        Con_Printf("vr_dazzlestudio [open|close|toggle|test]: the Dazzle Studio's flat-screen ImGui window\n");
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_dazzlestudio", window_f);
}

void frame()
{
    if(!win.open)
    {
        return;
    }
    if(VR_IsActive() || rigstudio::active() || waveeditor::shown() || weaponstudio::active() || armourstudio::windowOpen() ||
       reloadstudio::open())
    {
        closeWindow();
        return;
    }
    keepSubject();
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
    drawWindow(readPose());
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

} // namespace qvr::dazzlestudio
