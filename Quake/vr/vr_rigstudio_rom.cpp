// vr_rigstudio_rom.cpp -- the Rig Studio's Range of Motion window (vr_rigstudio_internal.hpp, Rom): the authored rig's
// joints (vr_rig2.hpp) picked in its list or by a click, each posed within its limits by a slider per axis or by dragging
// its limb, swept through its range, a random pose within every limit; its limits drawn; its pivots and limits edited
// live (undoable), saved to quakevr/rigs/<model>.rig2.txt. vr_rig2 picks the rig the mesh is drawn on: 1 the authored
// rig (its blended weights; Game: the engine's skinned model, Shaded: the studio's), 0 the old rig moved alike (each old
// bone as its namesake: the same pose on both, to compare). The pose's faults (vr_rig2_check's measures) are shown live.

#include "vr_rigstudio_internal.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_rig2.hpp"
#include "vr_rigdata.hpp"
#include "vr_rigpose.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Abs.hpp"
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

// The old rig moved as the authored one (vr_rig2 0): each old bone's local from its namesake's motion.
void oldFromRom(const rig2::Rig& rig)
{
    if(!st.rig)
    {
        return;
    }
    rigpose::Rigid Fo[maxBones];
    for(int o = 0; o < st.numBones; o++)
    {
        int m = rig2::boneByName(rig, st.rig->bones[o].name);
        if(m < 0)
        {
            m = st.rig->bones[o].joint == ragdoll::Joint::Loose ? rig2::boneByName(rig, "weapon") : 0;
        }
        Fo[o] = rom.F[za::max(m, 0)];
    }
    rigdata::restOf(*st.rig, &st.doc, st.pivots, st.ends);
    for(int o = 0; o < st.numBones; o++)
    {
        const int p = st.rig->bones[o].parent;
        const rigpose::Rigid L = p >= 0 && p < o ? rigdata::compose(rigdata::inverse(Fo[p]), Fo[o]) : Fo[o];
        st.live[o] = rigdata::fromRigid(L, st.pivots[o]);
    }
}

void updateReport(rig2::Rig& rig)
{
    const int which = romNew() ? 1 : 0;
    if(rom.reportValid && rom.reportRig == which && !memcmp(&rom.reportFor, &rom.pose, sizeof(rig2::Pose)))
    {
        return;
    }
    rig2::Faults f;
    if(rig2::measurePose(const_cast<qmodel_t*>(st.model), rom.pose, which == 0, f))
    {
        q_snprintf(rom.report, sizeof(rom.report), "%s rig: see-through %d, flipped %d, collapsed %d, sharp folds %d (ridge %.0f deg), stretch %.2f..%.2f",
            which ? "authored" : "old", f.seeThrough, f.flipped, f.collapsed, f.folds, f.foldMax, f.stretchMin, f.stretchMax);
    }
    else
    {
        q_strlcpy(rom.report, "no measure", sizeof(rom.report));
    }
    (void)rig;
    rom.reportFor = rom.pose;
    rom.reportRig = which;
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

} // namespace

namespace qvr::rigstudio::detail
{

rig2::Rig* romRig()
{
    return st.model ? rig2::rigFor(st.model) : nullptr;
}

bool romNew()
{
    return rom.on && vr_rig2.value != 0.f && romRig() != nullptr;
}

bool romHidesOldBones()
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
    if(romNew())
    {
        clearLive();
        rig2::drawPosed(st.subject, st.model, rom.F, rig->numBones);
    }
    else
    {
        oldFromRom(*rig);
    }
    updateReport(*rig);
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
    int which = vr_rig2.value != 0.f ? 1 : 0;
    ImGui::TextUnformatted("Mesh drawn on:");
    ImGui::SameLine();
    if(ImGui::RadioButton("Authored rig", which == 1))
    {
        Cvar_SetValueQuick(&vr_rig2, 1.f);
    }
    ImGui::SameLine();
    if(ImGui::RadioButton("Old rig", which == 0))
    {
        Cvar_SetValueQuick(&vr_rig2, 0.f);
    }
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
    if(!strcmp(sub, "rig"))
    {
        Cvar_SetValueQuick(&vr_rig2, Cmd_Argc() > 2 && !strcmp(Cmd_Argv(2), "old") ? 0.f : 1.f);
        Con_Printf("rigstudio rom: mesh on the %s rig\n", vr_rig2.value != 0.f ? "authored" : "old");
    }
    else if(!strcmp(sub, "joint"))
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
        updateReport(*rig);
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
        Con_Printf("vr_rigstudio_rom [on|off] | rig old|new | joint <j> | set <j> <x> [y] [z] | reset | random [seed] | sweep <j> | "
                   "limit <j> swingx|swingy|twist|range <min> <max> | pivot <j> <x y z> | save | camera <j> [yaw] [pitch] [dist] | status\n");
    }
}

void romInit()
{
    Cmd_AddCommand("vr_rigstudio_rom", rom_f);
}

} // namespace qvr::rigstudio::detail
