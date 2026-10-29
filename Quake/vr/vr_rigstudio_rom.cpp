// vr_rigstudio_rom.cpp -- the Rig Studio's Range of Motion window (vr_rigstudio_internal.hpp, Rom): the authored rig's
// joints (vr_rig2.hpp) picked in its list or by a click, each posed within its limits by a slider per axis or by dragging
// its limb, swept through its range, a random pose within every limit; its limits drawn; its pivots and limits edited
// live (undoable), saved to quakevr/rigs/<model>.rig2.txt. The mesh is drawn on the authored rig (its blended weights;
// Game: the engine's skinned model, Shaded: the studio's). The pose's faults (vr_rig2_check's measures) are shown live.

#include "vr_testkit.hpp"
#include "vr_rigstudio_internal.hpp"
#include "vr_agarms.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_rig2.hpp"
#include "vr_rigdata.hpp"
#include "vr_rigpose.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdlib.h>
#include <string.h>

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace qvr::rigstudio::detail
{
Rom rom;
void romTestTick(rig2::Rig& rig); // (the test's frame: below)
}

namespace
{

constexpr float pi = 3.14159265f;
constexpr float sweepPhase = 1.6f; // seconds a sweep's leg takes (min to max)

[[nodiscard]] ImU32 col(const glm::vec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4{c.x, c.y, c.z, c.w});
}

[[nodiscard]] ImVec2 iv(const glm::vec2& v)
{
    return ImVec2{v.x, v.y};
}

[[nodiscard]] glm::vec4 hue(int b)
{
    const float h = static_cast<float>((b * 7) % 20) / 20.f * 6.f;
    const float x = 1.f - za::abs(h - 2.f * static_cast<float>(static_cast<int>(h / 2.f)) - 1.f);
    glm::vec3 c = h < 1.f ? glm::vec3{1.f, x, 0.f} : h < 2.f ? glm::vec3{x, 1.f, 0.f} : h < 3.f ? glm::vec3{0.f, 1.f, x}
                : h < 4.f ? glm::vec3{0.f, x, 1.f} : h < 5.f ? glm::vec3{x, 0.f, 1.f} : glm::vec3{1.f, 0.f, x};
    return glm::vec4{c * 0.75f + 0.2f, 1.f};
}

[[nodiscard]] bool movable(const rig2::Bone& b)
{
    return b.joint == rig2::Joint::Ball || b.joint == rig2::Joint::Hinge;
}

[[nodiscard]] glm::vec3 worldOf(const glm::vec3& modelPoint)
{
    return toWorld(modelPoint);
}

[[nodiscard]] glm::vec3 mouseRay(const glm::vec2& m)
{
    const View& v = st.view;
    const float x = ((m.x - v.x) / v.w) * 2.f - 1.f;
    const float y = 1.f - ((m.y - v.y) / v.h) * 2.f;
    return glm::normalize(v.fwd + v.right * (x * v.tanX) + v.up * (y * v.tanY));
}

void ensurePose(rig2::Rig& rig)
{
    if(rom.poseFor != &rig)
    {
        rom.pose = rig2::restPose(rig);
        rom.poseFor = &rig;
        rom.joint = za::clamp(rom.joint, -1, rig.numBones - 1);
        rom.reportValid = false;
    }
}

void keepRomPre(const rig2::Rig& rig)
{
    rom.preNum = rig.numBones;
    for(int b = 0; b < rig.numBones; b++)
    {
        rom.preBones[b] = rig.bones[b];
    }
    rom.prePose = rom.pose;
}

void pushRomUndo(const char* what)
{
    Snapshot s;
    s.romEntry = true;
    s.romNumBones = rom.preNum;
    for(int b = 0; b < rom.preNum; b++)
    {
        s.romBones[b] = rom.preBones[b];
    }
    s.romPose = rom.prePose;
    s.what = what;
    st.undo.pushBack(ZA_MOVE(s));
    if(st.undo.size() > static_cast<za::SizeT>(maxUndo))
    {
        st.undo.erase(st.undo.begin());
    }
    st.redo.clear();
}

// An edit made by a command (not a widget): its undo is the state now.
void pushRomUndoNow(rig2::Rig& rig, const char* what)
{
    keepRomPre(rig);
    pushRomUndo(what);
}

void setAngles(rig2::Rig& rig, int b, const glm::vec3& a)
{
    rom.pose.a[b] = rig2::clampAngles(rig, b, a);
}

// A sweep's pose at time t (seconds): each axis min to max and back to rest in turn, then once round the swing's edge.
[[nodiscard]] bool sweepAngles(const rig2::Rig& rig, int b, float t, const glm::vec3& restA, glm::vec3& out)
{
    const rig2::Bone& bone = rig.bones[b];
    out = restA;
    const auto leg = [&](float lo, float hi, float u) { // rest -> lo -> hi -> rest over 2 phases
        if(u < 0.25f)
        {
            return glm::mix(0.f, lo, u / 0.25f);
        }
        if(u < 0.75f)
        {
            return glm::mix(lo, hi, (u - 0.25f) / 0.5f);
        }
        return glm::mix(hi, 0.f, (u - 0.75f) / 0.25f);
    };
    const float span = 2.f * sweepPhase;
    if(bone.joint == rig2::Joint::Hinge)
    {
        if(t >= span)
        {
            return false;
        }
        const float u = t / span;
        out.x = u < 0.5f ? glm::mix(restA.x, bone.range.x, u * 2.f) : glm::mix(bone.range.x, bone.range.y, (u - 0.5f) * 2.f);
        return true;
    }
    if(bone.joint != rig2::Joint::Ball)
    {
        return false;
    }
    const int phase = static_cast<int>(t / span);
    const float u = (t - static_cast<float>(phase) * span) / span;
    switch(phase)
    {
    case 0: out.x = leg(bone.swingX.x, bone.swingX.y, u); out.y = 0.f; return true;
    case 1: out.y = leg(bone.swingY.x, bone.swingY.y, u); out.x = 0.f; return true;
    case 2: out.z = leg(bone.twist.x, bone.twist.y, u); return true;
    case 3:
    {
        const float phi = u * 2.f * pi;
        const float cx = za::cos(phi), cy = za::sin(phi);
        out.x = cx * (cx >= 0.f ? bone.swingX.y : -bone.swingX.x);
        out.y = cy * (cy >= 0.f ? bone.swingY.y : -bone.swingY.x);
        return true;
    }
    default: return false;
    }
}

void updateReport()
{
    if(rom.reportValid && !memcmp(&rom.reportFor, &rom.pose, sizeof(rig2::Pose)))
    {
        return;
    }
    rig2::Faults f;
    if(rig2::measurePose(const_cast<qmodel_t*>(st.model), rom.pose, f))
    {
        q_snprintf(rom.report, sizeof(rom.report), "authored rig: see-through %d, flipped %d, collapsed %d, sharp folds %d (ridge %.0f deg), stretch %.2f..%.2f",
            f.seeThrough, f.flipped, f.collapsed, f.folds, f.foldMax, f.stretchMin, f.stretchMax);
    }
    else
    {
        q_strlcpy(rom.report, "no measure", sizeof(rom.report));
    }
    rom.reportFor = rom.pose;
    rom.reportValid = true;
}

[[nodiscard]] int pickJoint(const glm::vec2& m, const rig2::Rig& rig)
{
    int best = -1;
    float bestD = 12.f;
    for(int b = 0; b < rig.numBones; b++)
    {
        const rig2::Bone& bone = rig.bones[b];
        glm::vec2 a, e;
        if(!project(worldOf(rigpose::apply(rom.F[b], bone.pivot)), a) || !project(worldOf(rigpose::apply(rom.F[b], bone.end)), e))
        {
            continue;
        }
        const glm::vec2 ab = e - a;
        const float l2 = glm::dot(ab, ab);
        const float t = l2 > 1e-6f ? za::clamp(glm::dot(m - a, ab) / l2, 0.f, 1.f) : 0.f;
        const float d = glm::length(m - (a + ab * t));
        if(d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

void say2(const char* text)
{
    say(text);
}

// The authored rig's bone a hand's weapon rides (0 right: `weapon`, fixed on hand_r; 1 left: `weapon_l` if the rig has
// one, else hand_l: a fixed bone moves as its parent does). -1: none.
[[nodiscard]] int weaponBone(const rig2::Rig& rig, int side)
{
    const int w = rig2::boneByName(rig, side == 0 ? "weapon" : "weapon_l");
    return w >= 0 ? w : rig2::boneByName(rig, side == 0 ? "hand_r" : "hand_l");
}

} // namespace

namespace qvr::rigstudio::detail
{

rig2::Rig* romRig()
{
    return st.model ? rig2::rigFor(st.model) : nullptr;
}

bool romNew()
{
    return rom.on && romRig() != nullptr;
}

void romFrame()
{
    rig2::Rig* rig = romRig();
    if(!rom.on || !rig)
    {
        return;
    }
    ensurePose(*rig);
    if(!ImGui::IsAnyItemActive() && !rom.dragging)
    {
        keepRomPre(*rig);
    }
    // A sweep playing
    if(rom.sweepJoint >= 0 && rom.sweepJoint < rig->numBones)
    {
        rom.sweepT += static_cast<float>(host_frametime);
        glm::vec3 a;
        if(sweepAngles(*rig, rom.sweepJoint, rom.sweepT, rig2::restAngles(*rig, rom.sweepJoint), a))
        {
            rom.pose.a[rom.sweepJoint] = rig2::clampAngles(*rig, rom.sweepJoint, a);
        }
        else
        {
            rom.pose.a[rom.sweepJoint] = rig2::restAngles(*rig, rom.sweepJoint);
            Con_Printf("rigstudio rom: sweep %s done\n", rig->bones[rom.sweepJoint].name);
            rom.sweepJoint = -1;
        }
    }
    rig2::forward(*rig, rom.pose, rom.F);
    st.source = Source::Rest; // (the pose is the Range of Motion's: on the rest pose)
    clearLive();
    rig2::drawPosed(st.subject, st.model, rom.F, rig->numBones);
    updateReport();
    romTestTick(*rig);
}

bool romGeometry()
{
    rig2::Rig* rig = romRig();
    if(!romNew() || !rig || st.restVerts.empty())
    {
        return false;
    }
    const int nv = static_cast<int>(st.restVerts.size());
    st.baseVerts.resize(static_cast<za::SizeT>(nv));
    st.posedVerts.resize(static_cast<za::SizeT>(nv));
    st.worldVerts.resize(static_cast<za::SizeT>(nv));
    st.screen.resize(static_cast<za::SizeT>(nv));
    st.depth.resize(static_cast<za::SizeT>(nv));
    st.facing.resize(static_cast<za::SizeT>(nv));
    st.normals.resize(static_cast<za::SizeT>(nv));
    for(int v = 0; v < nv; v++)
    {
        st.baseVerts[static_cast<za::SizeT>(v)] = st.restVerts[static_cast<za::SizeT>(v)];
    }
    rig2::skin(*rig, rom.F, st.restVerts.data(), nv, st.posedVerts.data(), vr_rig2_dq.value != 0.f);
    finishGeometry();
    return true;
}

bool romClick(const glm::vec2& m)
{
    rig2::Rig* rig = romRig();
    if(!rom.on || !rig)
    {
        return false;
    }
    const int b = pickJoint(m, *rig);
    if(b < 0)
    {
        return false;
    }
    if(b == rom.joint && movable(rig->bones[b]))
    {
        // a second click on the picked limb: drag it by its end
        keepRomPre(*rig);
        pushRomUndo("pose (drag)");
        rom.dragging = true;
        rom.dragBone = b;
        rom.sweepJoint = -1;
        return true;
    }
    rom.joint = b;
    return true;
}

void romDragTick(const glm::vec2& m)
{
    rig2::Rig* rig = romRig();
    if(!rom.dragging || !rig || rom.dragBone < 0 || rom.dragBone >= rig->numBones)
    {
        rom.dragging = false;
        return;
    }
    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        rom.dragging = false;
        return;
    }
    const int b = rom.dragBone;
    const rig2::Bone& bone = rig->bones[b];
    // The limb's end follows the mouse on the plane through its joint facing the camera.
    const glm::vec3 pivotW = worldOf(rigpose::apply(rom.F[b], bone.pivot));
    const glm::vec3 d = mouseRay(m);
    const float den = glm::dot(d, st.view.fwd);
    if(za::abs(den) < 1e-4f)
    {
        return;
    }
    const float t = glm::dot(pivotW - st.view.org, st.view.fwd) / den;
    const glm::vec3 hit = st.view.org + d * t;
    const glm::vec3 dirModel = toModelDir(hit - pivotW);
    if(glm::length(dirModel) < 0.5f)
    {
        return;
    }
    const rigpose::Rigid Fp = bone.parent >= 0 ? rom.F[bone.parent] : rigpose::Rigid{};
    rom.pose.a[b] = rig2::aimAngles(*rig, b, Fp, glm::normalize(dirModel), rom.pose.a[b]);
}

void romDraw(ImDrawList* dl)
{
    rig2::Rig* rig = romRig();
    if(!rom.on || !rig)
    {
        return;
    }
    for(int b = 0; b < rig->numBones; b++)
    {
        const rig2::Bone& bone = rig->bones[b];
        glm::vec2 a, e;
        if(!project(worldOf(rigpose::apply(rom.F[b], bone.pivot)), a) || !project(worldOf(rigpose::apply(rom.F[b], bone.end)), e))
        {
            continue;
        }
        const bool sel = b == rom.joint;
        const glm::vec4 c = sel ? glm::vec4{1.f, 0.75f, 0.2f, 1.f} : glm::vec4{0.85f, 0.9f, 1.f, 0.85f};
        dl->AddLine(iv(a), iv(e), col(glm::vec4{0.f, 0.f, 0.f, 0.6f}), sel ? 5.f : 4.f);
        dl->AddLine(iv(a), iv(e), col(c), sel ? 3.f : 2.f);
        dl->AddCircleFilled(iv(a), sel ? 5.f : 3.5f, col(movable(bone) ? glm::vec4{1.f, 0.3f, 0.25f, 1.f} : glm::vec4{0.6f, 0.6f, 0.6f, 1.f}), 12);
    }
    // The picked joint's limits: where its bone's end can go (a ball's swing ellipse, a hinge's arc), its twist's ends
    const int b = rom.joint;
    if(!rom.showLimits || b < 0 || b >= rig->numBones || !movable(rig->bones[b]))
    {
        return;
    }
    const rig2::Bone& bone = rig->bones[b];
    const glm::vec3 pivotW = worldOf(rigpose::apply(rom.F[b], bone.pivot));
    glm::vec2 pv;
    if(!project(pivotW, pv))
    {
        return;
    }
    rig2::Pose p = rom.pose;
    rigpose::Rigid Ft[rig2::maxBones];
    constexpr int segments = 48;
    glm::vec2 prev{0.f};
    bool havePrev = false;
    const int n = bone.joint == rig2::Joint::Hinge ? 24 : segments;
    for(int i = 0; i <= n; i++)
    {
        glm::vec3 a = p.a[b];
        if(bone.joint == rig2::Joint::Hinge)
        {
            a.x = glm::mix(bone.range.x, bone.range.y, static_cast<float>(i) / static_cast<float>(n));
        }
        else
        {
            const float phi = 2.f * pi * static_cast<float>(i) / static_cast<float>(n);
            const float cx = za::cos(phi), cy = za::sin(phi);
            a.x = cx * (cx >= 0.f ? bone.swingX.y : -bone.swingX.x);
            a.y = cy * (cy >= 0.f ? bone.swingY.y : -bone.swingY.x);
        }
        p.a[b] = a;
        rig2::forward(*rig, p, Ft);
        glm::vec2 s;
        if(project(worldOf(rigpose::apply(Ft[b], bone.end)), s))
        {
            if(havePrev)
            {
                dl->AddLine(iv(prev), iv(s), col(glm::vec4{1.f, 0.2f, 0.9f, 0.9f}), 2.f);
            }
            if(i % (n / 8) == 0)
            {
                dl->AddLine(iv(pv), iv(s), col(glm::vec4{1.f, 0.2f, 0.9f, 0.35f}), 1.f);
            }
            prev = s;
            havePrev = true;
        }
    }
    if(bone.joint == rig2::Joint::Ball)
    {
        // the twist's ends: a tick off the bone's middle, turned to each end (cyan)
        for(int k = 0; k < 2; k++)
        {
            p.a[b] = rom.pose.a[b];
            p.a[b].z = k == 0 ? bone.twist.x : bone.twist.y;
            rig2::forward(*rig, p, Ft);
            const glm::vec3 mid = (bone.pivot + bone.end) * 0.5f;
            const glm::vec3 dir = glm::normalize(bone.end - bone.pivot);
            glm::vec3 side = glm::cross(dir, glm::vec3{0.f, 0.f, 1.f});
            side = glm::length(side) < 0.1f ? glm::vec3{1.f, 0.f, 0.f} : glm::normalize(side);
            glm::vec2 s0, s1;
            if(project(worldOf(rigpose::apply(Ft[b], mid)), s0) && project(worldOf(rigpose::apply(Ft[b], mid + side * 3.f)), s1))
            {
                dl->AddLine(iv(s0), iv(s1), col(glm::vec4{0.2f, 1.f, 1.f, 0.9f}), 2.f);
            }
        }
    }
}

bool romColor(int v, glm::vec4& out)
{
    rig2::Rig* rig = romRig();
    if(!romNew() || !rig)
    {
        return false;
    }
    if(st.colors == Colors::Heat)
    {
        const float w = rom.joint >= 0 ? rig2::weightOn(*rig, v, rom.joint) : 0.f;
        const float t = za::clamp(w, 0.f, 1.f);
        out = t < 0.5f ? glm::vec4{0.f, t * 2.f, 1.f - t * 2.f, 1.f} : glm::vec4{(t - 0.5f) * 2.f, 1.f - (t - 0.5f) * 2.f, 0.f, 1.f};
        return true;
    }
    out = glm::vec4{0.f};
    if(v < static_cast<int>(rig->weights.size()))
    {
        const rig2::Weights& w = rig->weights[static_cast<za::SizeT>(v)];
        for(int i = 0; i < w.n; i++)
        {
            out += hue(w.bone[i]) * w.w[i];
        }
    }
    out.w = 1.f;
    return true;
}

bool romSwap(Snapshot& s, Snapshot& back)
{
    if(!s.romEntry)
    {
        return false;
    }
    rig2::Rig* rig = romRig();
    back.romEntry = true;
    back.what = s.what;
    back.romPose = rom.pose;
    back.romNumBones = rig ? rig->numBones : 0;
    for(int b = 0; rig && b < rig->numBones; b++)
    {
        back.romBones[b] = rig->bones[b];
    }
    if(rig && s.romNumBones == rig->numBones)
    {
        for(int b = 0; b < rig->numBones; b++)
        {
            rig->bones[b] = s.romBones[b];
        }
        rom.dirty = true;
    }
    rom.pose = s.romPose;
    rom.reportValid = false;
    return true;
}

void romWindow()
{
    if(!rom.on)
    {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2{20.f, 360.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{380.f, 560.f}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Range of Motion", &rom.on))
    {
        ImGui::End();
        return;
    }
    rig2::Rig* rig = romRig();
    if(!rig)
    {
        ImGui::TextWrapped("No authored rig for %s (quakevr/rigs/<model>.rig2.txt: Misc/quakevr/make_rig2.py).", st.model ? st.model->name : "-");
        ImGui::End();
        return;
    }
    ensurePose(*rig);
    bool dq = vr_rig2_dq.value != 0.f;
    if(ImGui::Checkbox("Dual quaternions", &dq))
    {
        Cvar_SetValueQuick(&vr_rig2_dq, dq ? 1.f : 0.f);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show limits", &rom.showLimits);
    ImGui::TextWrapped("%s", rom.report);
    ImGui::Separator();
    if(ImGui::Button("Random Pose"))
    {
        pushRomUndoNow(*rig, "random pose");
        rom.pose = rig2::randomPose(*rig, rom.seed);
        rom.sweepJoint = -1;
    }
    ImGui::SameLine();
    if(ImGui::Button("Reset All"))
    {
        pushRomUndoNow(*rig, "reset pose");
        rom.pose = rig2::restPose(*rig);
        rom.sweepJoint = -1;
    }
    ImGui::SameLine();
    if(ImGui::Button(rom.dirty ? "Save *" : "Save"))
    {
        za::String path;
        say2(rig2::save(*rig, path) ? va("saved %s", path.cStr()) : va("could not write %s", path.cStr()));
        rom.dirty = false;
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload"))
    {
        rig2::reload();
        rom.poseFor = nullptr;
        rom.dirty = false;
    }
    // The joints
    ImGui::BeginChild("joints", ImVec2{0.f, 150.f}, ImGuiChildFlags_Borders);
    for(int b = 0; b < rig->numBones; b++)
    {
        const rig2::Bone& bone = rig->bones[b];
        int depth = 0;
        for(int p = bone.parent; p >= 0 && depth < 8; p = rig->bones[p].parent)
        {
            depth++;
        }
        ImGui::Indent(static_cast<float>(depth) * 10.f);
        char label[64];
        q_snprintf(label, sizeof(label), "%s (%s)##j%d", bone.name,
            bone.joint == rig2::Joint::Ball ? "ball" : bone.joint == rig2::Joint::Hinge ? "hinge" : bone.joint == rig2::Joint::Fixed ? "fixed" : "root", b);
        if(ImGui::Selectable(label, rom.joint == b))
        {
            rom.joint = b;
        }
        ImGui::Unindent(static_cast<float>(depth) * 10.f);
    }
    ImGui::EndChild();
    const int b = rom.joint;
    if(b >= 0 && b < rig->numBones)
    {
        rig2::Bone& bone = rig->bones[b];
        ImGui::Text("%s: %s, parent %s", bone.name, bone.joint == rig2::Joint::Ball ? "ball joint" : bone.joint == rig2::Joint::Hinge ? "hinge" : "no joint",
            bone.parent >= 0 ? rig->bones[bone.parent].name : "-");
        glm::vec3& a = rom.pose.a[b];
        bool changed = false;
        if(bone.joint == rig2::Joint::Ball)
        {
            changed |= ImGui::SliderFloat("Swing X (fwd)", &a.x, bone.swingX.x, bone.swingX.y, "%.1f deg");
            if(ImGui::IsItemActivated()) { pushRomUndo("pose"); }
            changed |= ImGui::SliderFloat("Swing Y (side)", &a.y, bone.swingY.x, bone.swingY.y, "%.1f deg");
            if(ImGui::IsItemActivated()) { pushRomUndo("pose"); }
            changed |= ImGui::SliderFloat("Twist", &a.z, bone.twist.x, bone.twist.y, "%.1f deg");
            if(ImGui::IsItemActivated()) { pushRomUndo("pose"); }
        }
        else if(bone.joint == rig2::Joint::Hinge)
        {
            changed |= ImGui::SliderFloat("Bend", &a.x, bone.range.x, bone.range.y, "%.1f deg");
            if(ImGui::IsItemActivated()) { pushRomUndo("pose"); }
        }
        if(changed)
        {
            setAngles(*rig, b, a); // (a swing onto its ellipse)
            rom.sweepJoint = -1;
        }
        if(movable(bone))
        {
            if(ImGui::Button("Sweep"))
            {
                rom.sweepJoint = b;
                rom.sweepT = 0.f;
            }
            ImGui::SameLine();
            if(ImGui::Button("Reset Joint"))
            {
                pushRomUndoNow(*rig, "reset joint");
                rom.pose.a[b] = rig2::restAngles(*rig, b);
            }
            const glm::vec3 r = rig2::restAngles(*rig, b);
            ImGui::TextDisabled("rest %.1f %.1f %.1f; a second click on the limb drags it", r.x, r.y, r.z);
        }
        // Its joint's place and limits (the rig's: undoable, saved by Save)
        ImGui::SeparatorText("Joint (rest pose, limits)");
        bool edited = false;
        edited |= ImGui::DragFloat3("Pivot", &bone.pivot.x, 0.05f, -64.f, 64.f, "%.2f");
        if(ImGui::IsItemActivated()) { pushRomUndo("joint pivot"); }
        edited |= ImGui::DragFloat3("End", &bone.end.x, 0.05f, -64.f, 64.f, "%.2f");
        if(ImGui::IsItemActivated()) { pushRomUndo("bone end"); }
        if(bone.joint == rig2::Joint::Ball)
        {
            edited |= ImGui::DragFloat2("Swing X min/max", &bone.swingX.x, 0.5f, -180.f, 180.f, "%.0f");
            if(ImGui::IsItemActivated()) { pushRomUndo("limit"); }
            edited |= ImGui::DragFloat2("Swing Y min/max", &bone.swingY.x, 0.5f, -180.f, 180.f, "%.0f");
            if(ImGui::IsItemActivated()) { pushRomUndo("limit"); }
            edited |= ImGui::DragFloat2("Twist min/max", &bone.twist.x, 0.5f, -180.f, 180.f, "%.0f");
            if(ImGui::IsItemActivated()) { pushRomUndo("limit"); }
            edited |= ImGui::DragFloat3("Neutral dir", &bone.neutral.x, 0.01f, -1.f, 1.f, "%.2f");
            if(ImGui::IsItemActivated()) { pushRomUndo("neutral"); }
        }
        else if(bone.joint == rig2::Joint::Hinge)
        {
            edited |= ImGui::DragFloat2("Bend min/max", &bone.range.x, 0.5f, -180.f, 180.f, "%.0f");
            if(ImGui::IsItemActivated()) { pushRomUndo("limit"); }
            edited |= ImGui::DragFloat3("Axis", &bone.axis.x, 0.01f, -1.f, 1.f, "%.2f");
            if(ImGui::IsItemActivated()) { pushRomUndo("hinge axis"); }
        }
        if(edited)
        {
            rom.dirty = true;
            rom.reportValid = false;
            setAngles(*rig, b, rom.pose.a[b]);
        }
    }
    ImGui::End();
}

// ---- The test (vr_rigstudio_rom test; ends on "rigstudio rom test: N passed, M failed") ----

namespace
{
struct RomTest
{
    int step{0}; // 0 none; 20.. the weapon test's (vr_rigstudio_rom test weapon)
    int passed{0}, failed{0};
    int frames{0};
    // The weapon test: its case, the frames since the case's pose was set
    int wcase{0};
    int wframes{0};
    bool left{false}; // a weapon in its left hand too
};
RomTest romTest;

// The weapon test's poses (each from the rest pose; every angle held to its joint's limits).
struct WeaponCase
{
    const char* name;
    const char* bone[3];
    glm::vec3 a[3];
    bool random;
};
constexpr WeaponCase weaponCases[] = {
    {"at rest", {nullptr, nullptr, nullptr}, {}, false},
    {"right arm raised", {"upperarm_r", "forearm_r", nullptr}, {glm::vec3{40.f, 45.f, 10.f}, glm::vec3{120.f, 0.f, 0.f}}, false},
    {"right wrist bent", {"hand_r", "forearm_r", nullptr}, {glm::vec3{25.f, -10.f, 10.f}, glm::vec3{40.f, 0.f, 0.f}}, false},
    {"left arm and wrist", {"upperarm_l", "forearm_l", "hand_l"}, {glm::vec3{40.f, 45.f, -10.f}, glm::vec3{110.f, 0.f, 0.f},
        glm::vec3{-25.f, 10.f, -10.f}}, false},
    {"random pose 1", {nullptr, nullptr, nullptr}, {}, true},
    {"random pose 2", {nullptr, nullptr, nullptr}, {}, true},
};
constexpr int weaponCaseCount = static_cast<int>(sizeof(weaponCases) / sizeof(weaponCases[0]));

struct GripReading
{
    bool ok{false};
    float dist{0.f}, deg{0.f}; // the drawn weapon's grip frame off the hand bone's: units, degrees
};

// A held weapon as drawn (its entity's origin and angles), taken back through its grip offset to the hand's grip frame,
// against where the authored rig's hand bone carries the rest grip frame (world).
[[nodiscard]] GripReading readGrip(const rig2::Rig& rig, int side)
{
    GripReading r;
    const int num = rom.wprop[side];
    rigpose::Rigid grip;
    int handBone = -1;
    const int b = weaponBone(rig, side);
    const entity_t* e = st.subject > 0 && cl_entities ? &cl_entities[st.subject] : nullptr;
    if(rom.wframe[side] < 0 || host_framecount - rom.wframe[side] > 2 || num <= 0 || !e || b < 0 ||
        !agarms::ragdollGrip(st.model, side, grip, handBone))
    {
        Con_Printf("rigstudio rom weapon: no reading (placed %d frames ago, weapon %d, bone %d)\n", host_framecount - rom.wframe[side], num, b);
        return r;
    }
    const entity_t& p = cl_entities[num];
    vec3_t a{-p.angles[0], p.angles[1], p.angles[2]}, f, rt, u;
    AngleVectors(a, f, rt, u);
    const glm::mat3 R{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{rt[0], rt[1], rt[2]}, glm::vec3{u[0], u[1], u[2]}};
    const rigpose::Rigid back = rigdata::inverse(rom.woffset[side]);
    const glm::vec3 drawn = glm::vec3{p.origin[0], p.origin[1], p.origin[2]} + R * (back.pos * za::max(ENTSCALE_DECODE(e->scale), 0.01f));
    const glm::mat3 drawnRot = R * glm::mat3_cast(back.rot);
    const rigpose::Rigid want = rigdata::compose(rom.F[b], grip);
    const float y = e->angles[1] * (pi / 180.f);
    const glm::mat3 yawM{glm::vec3{za::cos(y), za::sin(y), 0.f}, glm::vec3{-za::sin(y), za::cos(y), 0.f}, glm::vec3{0.f, 0.f, 1.f}};
    const glm::mat3 wantRot = yawM * glm::mat3_cast(want.rot);
    r.dist = glm::length(drawn - worldOf(want.pos));
    const glm::mat3 d = glm::transpose(wantRot) * drawnRot;
    r.deg = za::acos(za::clamp((d[0][0] + d[1][1] + d[2][2] - 1.f) * 0.5f, -1.f, 1.f)) * (180.f / pi);
    r.ok = true;
    return r;
}

void expect(bool ok, const char* what)
{
    romTest.passed += ok ? 1 : 0;
    romTest.failed += ok ? 0 : 1;
    Con_Printf("rigstudio rom test: %s %s\n", ok ? "PASS" : "FAIL", what);
}

void finishTest()
{
    qvr::testkit::verdict("rigstudio rom test", romTest.passed, romTest.failed);
    romTest = RomTest{};
}

void finishWeaponTest()
{
    qvr::testkit::verdict("rigstudio rom weapon", romTest.passed, romTest.failed);
    romTest = RomTest{};
}

// vr_rigstudio_rom test weapon: the subject's held weapons on the authored rig's hands, case by case (weaponCases): each
// pose set, a few frames drawn (the arms' solve and the weapons placed on it), each weapon's grip read back from its
// entity against its hand bone's (within 0.5 units and 2 degrees), a screenshot. Ends on "rigstudio rom weapon: N passed,
// M failed".
void weaponTestTick(rig2::Rig& rig)
{
    if(romTest.step == 20)
    {
        // (its subject standing, its right weapon held: noted by the arms' solve, or placed on the authored rig already)
        const bool noted = st.noteFrame >= 0 && host_framecount - st.noteFrame <= 5 && st.note.ent == st.subject && st.note.prop > 0;
        const bool placed = rom.wframe[0] >= 0 && host_framecount - rom.wframe[0] <= 2;
        if(st.subject <= 0 || (!noted && !placed) || (scr_con_current > 0.f && romTest.frames < 144)) // (the console up, for the shots)
        {
            return;
        }
        romTest.left = noted ? st.note.propL > 0 : rom.wframe[1] >= 0 && host_framecount - rom.wframe[1] <= 2;
        rigpose::Rigid grip;
        int handBone = -1;
        for(int side = 0; side < 2; side++)
        {
            const int b = weaponBone(rig, side);
            if(b >= 0 && agarms::ragdollGrip(st.model, side, grip, handBone))
            {
                const rig2::Bone& hb = rig.bones[rig.bones[b].joint == rig2::Joint::Fixed ? za::max(rig.bones[b].parent, 0) : b];
                Con_Printf("rigstudio rom weapon: the %s hand's rest grip at %.2f %.2f %.2f (%s: %.2f from its pivot, %.2f from its end)\n",
                    side ? "left" : "right", grip.pos.x, grip.pos.y, grip.pos.z, hb.name, glm::length(grip.pos - hb.pivot),
                    glm::length(grip.pos - hb.end));
            }
        }
        st.showTools = st.showOutliner = st.showProps = st.showTimeline = st.showSequence = st.showPoseData = false;
        romTest.wcase = 0;
        romTest.step = 21;
        romTest.wframes = 0;
        return;
    }
    const WeaponCase& c = weaponCases[romTest.wcase];
    if(romTest.step == 21)
    {
        // the case's pose
        rom.pose = rig2::restPose(rig);
        rom.sweepJoint = -1;
        if(c.random)
        {
            rom.pose = rig2::randomPose(rig, rom.seed);
        }
        for(int i = 0; i < 3; i++)
        {
            const int b = c.bone[i] ? rig2::boneByName(rig, c.bone[i]) : -1;
            if(b >= 0)
            {
                setAngles(rig, b, c.a[i]);
            }
        }
        // (the camera on the chest, from the subject's front right)
        const int chest = rig2::boneByName(rig, "chest");
        const entity_t* e = cl_entities ? &cl_entities[st.subject] : nullptr;
        st.cam.focus = worldOf(rigpose::apply(rom.F[za::max(chest, 0)], rig.bones[za::max(chest, 0)].pivot)) + glm::vec3{0.f, 0.f, -6.f};
        st.cam.yaw = (e ? e->angles[1] : 0.f) + 180.f + 35.f;
        st.cam.pitch = 10.f;
        st.cam.dist = 40.f;
        romTest.step = 22;
        romTest.wframes = 0;
        return;
    }
    if(romTest.step == 22)
    {
        if(++romTest.wframes < 4)
        {
            return; // (the pose drawn: the arms' solve and the weapons placed on it, a few frames)
        }
        for(int side = 0; side < 2; side++)
        {
            if(side == 1 && !romTest.left)
            {
                continue; // (no left weapon)
            }
            const GripReading g = readGrip(rig, side);
            const bool ok = g.ok && g.dist < 0.5f && g.deg < 2.f;
            romTest.passed += ok ? 1 : 0;
            romTest.failed += ok ? 0 : 1;
            Con_Printf("rigstudio rom weapon %d: %s: the %s weapon's grip off the hand bone's %.3f units, %.2f deg: %s\n",
                romTest.wcase + 1, c.name, side ? "left" : "right", g.dist, g.deg, !g.ok ? "FAIL (no reading)" : ok ? "PASS" : "FAIL");
        }
        Cbuf_AddText("screenshot\n");
        romTest.step = 23;
        romTest.wframes = 0;
        return;
    }
    if(++romTest.wframes < 2)
    {
        return; // (the screenshot taken first, of this case)
    }
    if(++romTest.wcase >= weaponCaseCount)
    {
        rom.pose = rig2::restPose(rig);
        finishWeaponTest();
        return;
    }
    romTest.step = 21;
}
} // namespace

// A frame of the test (romFrame's, after the pose is made): its steps in turn, a sweep's frames waited for.
void romTestTick(rig2::Rig& rig)
{
    if(romTest.step == 0)
    {
        return;
    }
    romTest.frames++;
    if(romTest.frames > 72 * 60)
    {
        expect(false, "timed out");
        romTest.step >= 20 ? finishWeaponTest() : finishTest();
        return;
    }
    if(romTest.step >= 20)
    {
        weaponTestTick(rig);
        return;
    }
    switch(romTest.step)
    {
    case 1:
    {
        expect(rig.numBones >= 19 && rig2::boneByName(rig, "upperarm_r") >= 0 && rig2::boneByName(rig, "shin_l") >= 0,
            va("the authored rig: %d bones", rig.numBones));
        const int b = rig2::boneByName(rig, "upperarm_r");
        rom.pose = rig2::restPose(rig);
        setAngles(rig, b, glm::vec3{200.f, 200.f, 200.f});
        expect(rig2::withinLimits(rig, b, rom.pose.a[b], 0.05f), va("a slider past the limits is held at them (%.1f %.1f %.1f)",
            rom.pose.a[b].x, rom.pose.a[b].y, rom.pose.a[b].z));
        const int k = rig2::boneByName(rig, "shin_l");
        setAngles(rig, k, glm::vec3{-30.f, 0.f, 0.f});
        expect(rom.pose.a[k].x >= rig.bones[k].range.x - 0.01f, "the knee does not bend backwards");
        rom.pose = rig2::restPose(rig);
        rom.sweepJoint = rig2::boneByName(rig, "upperarm_l");
        rom.sweepT = 0.f;
        romTest.step = 2;
        break;
    }
    case 2:
        if(rom.sweepJoint >= 0)
        {
            const int b = rom.sweepJoint;
            if(!rig2::withinLimits(rig, b, rom.pose.a[b], 0.05f))
            {
                expect(false, "the sweep stays within the limits");
                rom.sweepJoint = -1;
            }
            break;
        }
        expect(true, "the sweep ran to its end within the limits");
        {
            // a limit edited, then undone
            const int b = rig2::boneByName(rig, "forearm_r");
            const glm::vec2 before = rig.bones[b].range;
            pushRomUndoNow(rig, "limit");
            rig.bones[b].range = glm::vec2{10.f, 90.f};
            undo();
            expect(rig.bones[b].range == before, va("a limit's edit undone (%g %g)", rig.bones[b].range.x, rig.bones[b].range.y));
            // random poses within every limit
            bool all = true;
            for(int i = 0; i < 50; i++)
            {
                const rig2::Pose p = rig2::randomPose(rig, rom.seed);
                for(int j = 0; j < rig.numBones; j++)
                {
                    all = all && rig2::withinLimits(rig, j, p.a[j], 0.05f);
                }
            }
            expect(all, "50 random poses within every limit");
            // the file written and read back
            za::String path, text, err;
            const bool saved = rig2::save(rig, path);
            rig2::Rig back;
            const bool read = saved && files::readText(path.cStr(), text) && rig2::read(text.cStr(), back, err);
            bool same = read && back.numBones == rig.numBones && back.weights.size() == rig.weights.size();
            for(int j = 0; same && j < rig.numBones; j++)
            {
                same = glm::length(back.bones[j].pivot - rig.bones[j].pivot) < 1e-3f && back.bones[j].joint == rig.bones[j].joint;
            }
            expect(same, va("saved and read back (%s)", path.cStr()));
            rom.pose = rig2::restPose(rig);
            romTest.step = 3;
            romTest.frames = 0;
        }
        break;
    case 3:
        if(romTest.frames < 3)
        {
            break; // (a few frames drawn: the authored rig's model made and swapped in)
        }
        {
            char name[MAX_QPATH];
            q_snprintf(name, sizeof(name), "%s#rig2", st.model->name);
            qmodel_t* m = Mod_ForName(name, false);
            const auto* hdr = m && m->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(m)) : nullptr;
            expect(hdr && hdr->numbones == rig.numBones, va("the game draws it on its %d bones (%s)", hdr ? hdr->numbones : 0, name));
            rig2::Faults f;
            const bool measured = rig2::measurePose(const_cast<qmodel_t*>(st.model), rom.pose, f);
            expect(measured && f.seeThrough == 0 && f.flipped == 0 && f.collapsed == 0, "the rest pose measures clean");
        }
        finishTest();
        break;
    default: finishTest(); break;
    }
}

// ---- Commands (the tests and the eyeshots) ----

void rom_f()
{
    const char* sub = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!*sub || !strcmp(sub, "on") || !strcmp(sub, "off"))
    {
        rom.on = strcmp(sub, "off") != 0;
        if(!rom.on)
        {
            clearLive();
        }
        Con_Printf("rigstudio rom: %s\n", rom.on ? "on" : "off");
        return;
    }
    if(!strcmp(sub, "test"))
    {
        // (it waits for its subject: run as the studio opens; `weapon`: the weapon test)
        rom.on = true;
        romTest = RomTest{};
        romTest.step = Cmd_Argc() > 2 && !strcmp(Cmd_Argv(2), "weapon") ? 20 : 1;
        return;
    }
    rig2::Rig* rig = romRig();
    if(!rig)
    {
        Con_Printf("rigstudio rom: no authored rig for the subject\n");
        return;
    }
    ensurePose(*rig);
    const auto joint = [&](int i) {
        const int b = Cmd_Argc() > i ? rig2::boneByName(*rig, Cmd_Argv(i)) : -1;
        if(b < 0)
        {
            Con_Printf("rigstudio rom: no joint %s\n", Cmd_Argc() > i ? Cmd_Argv(i) : "");
        }
        return b;
    };
    if(!strcmp(sub, "joint"))
    {
        const int b = joint(2);
        rom.joint = b >= 0 ? b : rom.joint;
    }
    else if(!strcmp(sub, "set"))
    {
        const int b = joint(2);
        if(b >= 0)
        {
            pushRomUndoNow(*rig, "pose");
            glm::vec3 a = rom.pose.a[b];
            for(int i = 0; i < 3 && Cmd_Argc() > 3 + i; i++)
            {
                a[i] = static_cast<float>(atof(Cmd_Argv(3 + i)));
            }
            setAngles(*rig, b, a);
            rom.joint = b;
            Con_Printf("rigstudio rom: %s %.1f %.1f %.1f\n", rig->bones[b].name, rom.pose.a[b].x, rom.pose.a[b].y, rom.pose.a[b].z);
        }
    }
    else if(!strcmp(sub, "reset"))
    {
        pushRomUndoNow(*rig, "reset pose");
        rom.pose = rig2::restPose(*rig);
        rom.sweepJoint = -1;
    }
    else if(!strcmp(sub, "random"))
    {
        pushRomUndoNow(*rig, "random pose");
        if(Cmd_Argc() > 2)
        {
            rom.seed = static_cast<uint32_t>(atoi(Cmd_Argv(2))) | 1u;
        }
        rom.pose = rig2::randomPose(*rig, rom.seed);
    }
    else if(!strcmp(sub, "sweep"))
    {
        const int b = joint(2);
        if(b >= 0)
        {
            rom.joint = b;
            rom.sweepJoint = b;
            rom.sweepT = 0.f;
        }
    }
    else if(!strcmp(sub, "limit") && Cmd_Argc() > 5)
    {
        const int b = joint(2);
        if(b >= 0)
        {
            pushRomUndoNow(*rig, "limit");
            rig2::Bone& bone = rig->bones[b];
            const glm::vec2 v{static_cast<float>(atof(Cmd_Argv(4))), static_cast<float>(atof(Cmd_Argv(5)))};
            const char* f = Cmd_Argv(3);
            glm::vec2* dst = !strcmp(f, "swingx") ? &bone.swingX : !strcmp(f, "swingy") ? &bone.swingY : !strcmp(f, "twist") ? &bone.twist
                           : !strcmp(f, "range") ? &bone.range : nullptr;
            if(dst)
            {
                *dst = v;
                rom.dirty = true;
                setAngles(*rig, b, rom.pose.a[b]);
            }
            Con_Printf("rigstudio rom: %s %s %g %g\n", bone.name, f, v.x, v.y);
        }
    }
    else if(!strcmp(sub, "pivot") && Cmd_Argc() > 5)
    {
        const int b = joint(2);
        if(b >= 0)
        {
            pushRomUndoNow(*rig, "joint pivot");
            rig->bones[b].pivot = glm::vec3{static_cast<float>(atof(Cmd_Argv(3))), static_cast<float>(atof(Cmd_Argv(4))),
                static_cast<float>(atof(Cmd_Argv(5)))};
            rom.dirty = true;
        }
    }
    else if(!strcmp(sub, "save"))
    {
        za::String path;
        const bool ok = rig2::save(*rig, path);
        rom.dirty = rom.dirty && !ok;
        Con_Printf("rigstudio rom: %s %s\n", ok ? "saved" : "could not write", path.cStr());
    }
    else if(!strcmp(sub, "camera"))
    {
        // the camera on a joint (its posed pivot), from yaw/pitch (degrees, the subject's facing: 0 its front) and a distance
        const int b = joint(2);
        if(b >= 0)
        {
            const entity_t* e = st.subject > 0 && cl_entities ? &cl_entities[st.subject] : nullptr;
            st.cam.focus = worldOf(rigpose::apply(rom.F[b], (rig->bones[b].pivot + rig->bones[b].end) * 0.5f));
            st.cam.yaw = (e ? e->angles[1] : 0.f) + 180.f + (Cmd_Argc() > 3 ? static_cast<float>(atof(Cmd_Argv(3))) : 0.f);
            st.cam.pitch = Cmd_Argc() > 4 ? static_cast<float>(atof(Cmd_Argv(4))) : 10.f;
            st.cam.dist = Cmd_Argc() > 5 ? static_cast<float>(atof(Cmd_Argv(5))) : 40.f;
        }
    }
    else if(!strcmp(sub, "status"))
    {
        rig2::forward(*rig, rom.pose, rom.F);
        updateReport();
        Con_Printf("rigstudio rom: %s; joint %s", rom.report, rom.joint >= 0 ? rig->bones[rom.joint].name : "-");
        if(rom.joint >= 0)
        {
            const glm::vec3& a = rom.pose.a[rom.joint];
            Con_Printf(" %.1f %.1f %.1f (%s)", a.x, a.y, a.z, rig2::withinLimits(*rig, rom.joint, a, 0.05f) ? "within its limits" : "OUTSIDE its limits");
        }
        Con_Printf("\n");
    }
    else
    {
        Con_Printf("vr_rigstudio_rom [on|off] | joint <j> | set <j> <x> [y] [z] | reset | random [seed] | sweep <j> | "
                   "limit <j> swingx|swingy|twist|range <min> <max> | pivot <j> <x y z> | save | camera <j> [yaw] [pitch] [dist] | status | test [weapon]\n");
    }
}

void romInit()
{
    Cmd_AddCommand("vr_rigstudio_rom", rom_f);
}

} // namespace qvr::rigstudio::detail

bool qvr::rigstudio::authoredWeapon(int ent, int side, int prop, const rigpose::Rigid& offset, rigpose::Rigid& out)
{
    rig2::Rig* rig = st.model && rom.on ? romRig() : nullptr;
    const int b = rig && side >= 0 && side < 2 ? weaponBone(*rig, side) : -1;
    rigpose::Rigid grip;
    int handBone = -1;
    if(b < 0 || ent <= 0 || ent != st.subject || !agarms::ragdollGrip(st.model, side, grip, handBone))
    {
        return false;
    }
    // (rom.F: the bones drawn this frame, romFrame's)
    out = rigdata::compose(rigdata::compose(rom.F[b], grip), offset);
    rom.wprop[side] = prop;
    rom.woffset[side] = offset;
    rom.wframe[side] = host_framecount;
    return true;
}
