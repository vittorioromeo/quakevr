// vr_rigstudio_ui.cpp -- the Rig Studio's windows (vr_rigstudio.hpp): the menu bar, Tools (the tools, the display,
// the layers, the weights and the poses), the Outliner (the bones' tree), Properties (the bone's rest pose, limits, IK
// chain and its pose now), the Timeline (playback, the dope sheet, the keys), Sequence (a new sequence's settings and
// events), Pose Studio Data (the armed humanoids' pose keys and grips), Help and Preferences.

#include "vr_rigstudio_internal.hpp"
#include "vr_aggrip.hpp"
#include "vr_agpose.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_imgui.hpp"
#include "vr_rigdata.hpp"
#include "vr_toolgun.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Erase.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>

#include <stdio.h>
#include <string.h>

extern "C" {
extern cvar_t r_lerpmodels;
}

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace
{

constexpr float deg = 3.14159265f / 180.f;

[[nodiscard]] bool dirtyAny()
{
    return st.dirtyRig || za::anyOf(st.dirtySeq.begin(), st.dirtySeq.end(), [](uint8_t d) { return d != 0; });
}

[[nodiscard]] ImU32 u32(const glm::vec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4{c.x, c.y, c.z, c.w});
}

void helpMarker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", text);
    }
}

// An edit's undo snapshot taken when a widget starts being changed (a drag: once).
void undoOnStart(const char* what)
{
    if(ImGui::IsItemActivated())
    {
        pushUndo(what);
    }
}

void menuBar()
{
    if(!ImGui::BeginMainMenuBar())
    {
        return;
    }
    if(ImGui::BeginMenu("File"))
    {
        if(ImGui::MenuItem("Save", "Ctrl+S"))
        {
            save();
        }
        if(ImGui::MenuItem("Revert to Files"))
        {
            revert();
        }
        if(ImGui::MenuItem("Print as Defaults"))
        {
            printDefaults();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Close Studio"))
        {
            Cbuf_AddText("vr_rigstudio_close\n");
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Edit"))
    {
        if(ImGui::MenuItem("Undo", "Ctrl+Z", false, !st.undo.empty()))
        {
            undo();
        }
        if(ImGui::MenuItem("Redo", "Ctrl+Y", false, !st.redo.empty()))
        {
            redo();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Copy Pose"))
        {
            st.poseClip = currentPose();
            st.havePoseClip = true;
        }
        if(ImGui::MenuItem("Paste Pose", nullptr, false, st.havePoseClip))
        {
            pushUndo("paste pose");
            applyPose(st.poseClip, 0xffffffffu);
            publish();
        }
        if(ImGui::MenuItem("Paste Pose on Selected Bones", nullptr, false, st.havePoseClip && st.selBones))
        {
            pushUndo("paste pose");
            applyPose(st.poseClip, st.selBones);
            publish();
        }
        if(ImGui::MenuItem("Mirror Pose"))
        {
            pushUndo("mirror pose");
            mirrorPose();
            publish();
        }
        if(ImGui::MenuItem("Clear Pose Changes (not keyed)"))
        {
            pushUndo("clear pose");
            clearLive();
            publish();
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Tools", nullptr, &st.showTools);
        ImGui::MenuItem("Outliner", nullptr, &st.showOutliner);
        ImGui::MenuItem("Properties", nullptr, &st.showProps);
        ImGui::MenuItem("Timeline", nullptr, &st.showTimeline);
        ImGui::MenuItem("Sequence", nullptr, &st.showSequence);
        ImGui::MenuItem("Pose Studio Data", nullptr, &st.showPoseData);
        ImGui::Separator();
        if(ImGui::MenuItem("Frame Selection", "F"))
        {
            frameSelection();
        }
        if(ImGui::MenuItem("Front", "1")) { snapView(1, false); }
        if(ImGui::MenuItem("Back", "Ctrl+1")) { snapView(1, true); }
        if(ImGui::MenuItem("Right", "3")) { snapView(3, false); }
        if(ImGui::MenuItem("Left", "Ctrl+3")) { snapView(3, true); }
        if(ImGui::MenuItem("Top", "7")) { snapView(7, false); }
        if(ImGui::MenuItem("Bottom", "Ctrl+7")) { snapView(7, true); }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Subject"))
    {
        toolgun::armed::refresh();
        const int kinds = toolgun::armed::kindCount();
        ImGui::TextDisabled("The armed humanoids");
        for(int k = 0; k < kinds; k++)
        {
            const bool on = !strcmp(st.choice.spawn, toolgun::armed::kindSpawn(k));
            if(ImGui::MenuItem(toolgun::armed::kindName(k), toolgun::armed::kindSpawn(k), on))
            {
                q_strlcpy(st.choice.spawn, toolgun::armed::kindSpawn(k), sizeof(st.choice.spawn));
                respawn();
            }
        }
        ImGui::Separator();
        ImGui::TextDisabled("Other monsters with a rig (no weapons)");
        const char* others[][2] = {{"Grunt", "monster_army"}, {"Enforcer", "monster_enforcer"}, {"Knight", "monster_knight"},
            {"Ogre", "monster_ogre"}, {"Zombie", "monster_zombie"}};
        for(const auto& o : others)
        {
            if(ImGui::MenuItem(o[0], o[1], !strcmp(st.choice.spawn, o[1])))
            {
                q_strlcpy(st.choice.spawn, o[1], sizeof(st.choice.spawn));
                respawn();
            }
        }
        ImGui::Separator();
        const int kind = [&] {
            for(int k = 0; k < kinds; k++)
            {
                if(!strcmp(st.choice.spawn, toolgun::armed::kindSpawn(k)))
                {
                    return k;
                }
            }
            return -1;
        }();
        for(int hand = 0; hand < 2 && kind >= 0; hand++)
        {
            int& code = hand == 0 ? st.choice.weapon : st.choice.weapon2;
            if(ImGui::BeginMenu(hand == 0 ? "Right Hand" : "Left Hand (dual)"))
            {
                if(hand == 1 && ImGui::MenuItem("None", nullptr, code == 0))
                {
                    code = 0;
                    respawn();
                }
                if(hand == 0 && ImGui::MenuItem("Fists", nullptr, code == -1))
                {
                    code = -1;
                    respawn();
                }
                for(int i = 0; i < toolgun::armed::weaponCount(); i++)
                {
                    const int c = toolgun::armed::weaponCode(i);
                    if(!toolgun::armed::fits(kind, c) || (hand == 1 && !toolgun::armed::pairs(kind, st.choice.weapon, c)))
                    {
                        continue;
                    }
                    if(ImGui::MenuItem(toolgun::armed::codeName(c), nullptr, code == c))
                    {
                        code = c;
                        respawn();
                    }
                }
                ImGui::EndMenu();
            }
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Respawn"))
        {
            respawn();
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Help"))
    {
        ImGui::MenuItem("Controls", "F1", &st.showHelp);
        ImGui::MenuItem("Preferences", nullptr, &st.showPrefs);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s%s", st.model ? st.model->name : "(no subject yet)", dirtyAny() ? "  * unsaved" : "");
    if(st.status[0] && realtime - st.statusAt < 6.0)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4{0.6f, 0.9f, 0.6f, 1.f}, "  %s", st.status);
    }
    ImGui::EndMainMenuBar();
}

void toolsWindow()
{
    if(!st.showTools)
    {
        return;
    }
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2{6.f, 26.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{230.f, za::min(ds.y * 0.55f, 430.f)}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Tools", &st.showTools))
    {
        ImGui::End();
        return;
    }
    const char* tools[] = {"Select (Q)", "Rotate (R)", "Move (G)", "IK (E)", "Rest Pose (T)", "Weights (W)", "Paint (P)"};
    int tool = static_cast<int>(st.tool);
    for(int i = 0; i < 7; i++)
    {
        if(ImGui::RadioButton(tools[i], tool == i))
        {
            st.tool = static_cast<Tool>(i);
        }
        if(i % 2 == 0 && i < 6)
        {
            ImGui::SameLine(115.f);
        }
    }
    ImGui::Checkbox("Snap (Ctrl)", &st.snap);
    ImGui::SameLine();
    ImGui::Checkbox("Auto Key", &st.autoKey);
    if(ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const char* meshes[] = {"Game", "Shaded", "Wire", "X-Ray"};
        int mesh = static_cast<int>(st.mesh);
        ImGui::Combo("Mesh", &mesh, meshes, 4);
        st.mesh = static_cast<MeshMode>(mesh);
        const char* colors[] = {"None", "By Bone", "Weights (Heat)"};
        int c = static_cast<int>(st.colors);
        ImGui::Combo("Colours", &c, colors, 3);
        st.colors = static_cast<Colors>(c);
        const char* looks[] = {"Octahedral", "Stick", "Off"};
        int b = static_cast<int>(st.bones);
        ImGui::Combo("Bones", &b, looks, 3);
        st.bones = static_cast<BoneLook>(b);
        ImGui::Checkbox("Joints", &st.joints);
        ImGui::SameLine();
        ImGui::Checkbox("Names", &st.names);
        ImGui::SameLine();
        ImGui::Checkbox("Limits", &st.limits);
        ImGui::Checkbox("Proxies", &st.proxies);
        ImGui::SameLine();
        ImGui::Checkbox("IK", &st.ikTargets);
        ImGui::SameLine();
        ImGui::Checkbox("Grips", &st.grips);
    }
    if(ImGui::CollapsingHeader("Layers (the game's drawing)"))
    {
        bool changed = false;
        changed |= ImGui::Checkbox("Arms on Weapons (agarms IK)", &st.layerArms);
        changed |= ImGui::Checkbox("Legs (one-knee pickup)", &st.layerLegs);
        changed |= ImGui::Checkbox("Upper Body Held", &st.layerUpper);
        changed |= ImGui::Checkbox("Smoothing (r_lerpmodels 3)", &st.layerSmooth);
        changed |= ImGui::Checkbox("Hit Reactions", &st.layerKicks);
        if(changed)
        {
            Cvar_SetValueQuick(&vr_agrunt_ik, st.layerArms ? 1.f : 0.f);
            Cvar_SetValueQuick(&vr_aglegs, st.layerLegs ? 1.f : 0.f);
            Cvar_SetValueQuick(&vr_agrunt_upper, st.layerUpper ? 1.f : 0.f);
            Cvar_SetValue("r_lerpmodels", st.layerSmooth ? 3.f : 1.f);
            Cvar_SetValueQuick(&vr_hit_react, st.layerKicks ? 1.f : 0.f);
        }
        helpMarker("Each of the game's procedural layers on the drawn model (Mesh: Game), to see what it adds. Put back as they "
                   "were when the studio closes.");
    }
    if((st.tool == Tool::Weights || st.tool == Tool::Paint || ImGui::GetIO().KeyAlt) && ImGui::CollapsingHeader("Weights", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int n = 0;
        for(const uint8_t s : st.selVerts)
        {
            n += s;
        }
        ImGui::Text("%d vertices selected", n);
        ImGui::TextDisabled("bone: %s", st.active >= 0 ? st.rig->bones[st.active].name : "(select one)");
        if(ImGui::Button("By Bone") && st.active >= 0)
        {
            selectByBone(st.active, ImGui::GetIO().KeyShift);
        }
        ImGui::SameLine();
        if(ImGui::Button("Grow"))
        {
            growSelection(true);
        }
        ImGui::SameLine();
        if(ImGui::Button("Shrink"))
        {
            growSelection(false);
        }
        ImGui::SameLine();
        if(ImGui::Button("None"))
        {
            za::fill(st.selVerts, static_cast<uint8_t>(0));
        }
        if(ImGui::Button("Assign to Bone") && st.active >= 0)
        {
            pushUndo("assign");
            assignSelected(st.active);
        }
        ImGui::SameLine();
        if(ImGui::Button("Normalize"))
        {
            pushUndo("normalize");
            normalizeSelected();
        }
        ImGui::SetNextItemWidth(90.f);
        ImGui::SliderFloat("##w", &st.setWeight, 0.f, 1.f, "%.2f");
        ImGui::SameLine();
        if(ImGui::Button("Set Weight") && st.active >= 0)
        {
            pushUndo("set weight");
            for(za::SizeT v = 0; v < st.selVerts.size(); v++)
            {
                if(st.selVerts[v])
                {
                    setVertWeight(static_cast<int>(v), st.active, st.setWeight);
                }
            }
            touchedRig();
        }
        ImGui::Separator();
        const char* brushes[] = {"Add", "Subtract", "Smooth"};
        int br = static_cast<int>(st.brush);
        ImGui::Combo("Brush", &br, brushes, 3);
        st.brush = static_cast<Brush>(br);
        ImGui::SliderFloat("Radius", &st.brushRadius, 4.f, 200.f, "%.0f px");
        ImGui::SliderFloat("Strength", &st.brushStrength, 0.01f, 1.f, "%.2f");
        ImGui::Checkbox("Mirror (left/right)", &st.mirror);
        ImGui::TextDisabled("%d vertices with their own weights", static_cast<int>(st.doc.weights.size()));
    }
    if(ImGui::CollapsingHeader("Poses"))
    {
        ImGui::SetNextItemWidth(120.f);
        ImGui::InputText("##pn", st.poseName, sizeof(st.poseName));
        ImGui::SameLine();
        if(ImGui::Button("Save Pose"))
        {
            pushUndo("save pose");
            rigdata::NamedPose np;
            np.name = st.poseName;
            np.pose = currentPose();
            st.doc.poses.pushBack(np);
            touchedRig();
        }
        for(za::SizeT i = 0; i < st.doc.poses.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextUnformatted(st.doc.poses[i].name.cStr());
            ImGui::SameLine(110.f);
            if(ImGui::SmallButton("Apply"))
            {
                pushUndo("apply pose");
                applyPose(st.doc.poses[i].pose, 0xffffffffu);
                publish();
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("Sel"))
            {
                pushUndo("apply pose");
                applyPose(st.doc.poses[i].pose, st.selBones);
                publish();
            }
            ImGui::SameLine();
            if(ImGui::SmallButton("X"))
            {
                pushUndo("delete pose");
                st.doc.poses.erase(st.doc.poses.begin() + static_cast<za::PtrDiffT>(i));
                touchedRig();
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        if(ImGui::Button("Mirror"))
        {
            pushUndo("mirror pose");
            mirrorPose();
            publish();
        }
        ImGui::SameLine();
        if(ImGui::Button("Copy"))
        {
            st.poseClip = currentPose();
            st.havePoseClip = true;
        }
        ImGui::SameLine();
        if(ImGui::Button("Paste") && st.havePoseClip)
        {
            pushUndo("paste pose");
            applyPose(st.poseClip, st.selBones ? st.selBones : 0xffffffffu);
            publish();
        }
    }
    ImGui::End();
}

void boneNode(int b, const ImGuiTextFilter& filter)
{
    const ragdoll::Rig& rig = *st.rig;
    bool hasChild = false;
    for(int c = 0; c < st.numBones; c++)
    {
        hasChild = hasChild || rig.bones[c].parent == b;
    }
    const bool match = filter.PassFilter(rig.bones[b].name);
    ImGui::PushID(b);
    bool eye = !(st.hidden & (1u << b));
    if(ImGui::Checkbox("##eye", &eye))
    {
        st.hidden = eye ? st.hidden & ~(1u << b) : st.hidden | (1u << b);
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Show / hide");
    }
    ImGui::SameLine();
    if(ImGui::SmallButton(st.solo == b ? "S*" : "S"))
    {
        st.solo = st.solo == b ? -1 : b;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Solo: this bone alone");
    }
    ImGui::SameLine();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if(!hasChild)
    {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if(st.selBones & (1u << b))
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const glm::vec4 c = boneColor(b);
    ImGui::PushStyleColor(ImGuiCol_Text, match ? ImVec4{c.x * 0.5f + 0.5f, c.y * 0.5f + 0.5f, c.z * 0.5f + 0.5f, 1.f} : ImVec4{0.5f, 0.5f, 0.5f, 0.6f});
    const bool open = ImGui::TreeNodeEx("##node", flags, "%s%s", rig.bones[b].name, st.doc.bones[b].on ? " *" : "");
    ImGui::PopStyleColor();
    if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        selectBone(b, ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl);
    }
    if(open)
    {
        for(int ch = 0; ch < st.numBones; ch++)
        {
            if(rig.bones[ch].parent == b)
            {
                boneNode(ch, filter);
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void outlinerWindow()
{
    if(!st.showOutliner || !st.rig)
    {
        return;
    }
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2{6.f, za::min(ds.y * 0.55f, 430.f) + 32.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{230.f, za::max(ds.y * 0.4f - 150.f, 150.f)}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Outliner", &st.showOutliner))
    {
        ImGui::End();
        return;
    }
    ImGuiTextFilter filter;
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##search", "search bones", st.search, sizeof(st.search));
    q_strlcpy(filter.InputBuf, st.search, sizeof(filter.InputBuf));
    filter.Build();
    if(ImGui::SmallButton("Show All"))
    {
        st.hidden = 0;
        st.solo = -1;
    }
    ImGui::SameLine();
    if(ImGui::SmallButton("Select All"))
    {
        st.selBones = st.numBones >= 32 ? 0xffffffffu : (1u << st.numBones) - 1u;
    }
    ImGui::Separator();
    for(int b = 0; b < st.numBones; b++)
    {
        if(st.rig->bones[b].parent < 0)
        {
            boneNode(b, filter);
        }
    }
    ImGui::End();
}

void propertiesWindow()
{
    if(!st.showProps || !st.rig)
    {
        return;
    }
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2{ds.x - 286.f, 26.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{280.f, za::min(ds.y * 0.5f, 420.f)}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Properties", &st.showProps))
    {
        ImGui::End();
        return;
    }
    const int b = st.active;
    if(b < 0)
    {
        ImGui::TextDisabled("Select a bone (click it, or in the Outliner).");
        ImGui::End();
        return;
    }
    const ragdoll::Bone& rb = st.rig->bones[b];
    const char* joints[] = {"root", "ball", "hinge", "loose"};
    ImGui::Text("%s  (bone %d, %s joint)", rb.name, b, joints[static_cast<int>(rb.joint)]);
    ImGui::TextDisabled("parent: %s", rb.parent >= 0 ? st.rig->bones[rb.parent].name : "none (the root)");
    int verts = 0;
    for(int v = 0; v < st.rig->numVerts; v++)
    {
        verts += st.rig->vertBone[static_cast<za::SizeT>(v)] == b ? 1 : 0;
    }
    ImGui::TextDisabled("%d vertices own it", verts);
    if(ImGui::CollapsingHeader("Rest Pose", ImGuiTreeNodeFlags_DefaultOpen))
    {
        rigdata::BoneEdit& e = st.doc.bones[b];
        rigdata::BoneEdit cur = e;
        if(!cur.on)
        {
            cur.pivot = rb.pivot;
            cur.end = rb.end;
            cur.cone = rb.cone / deg;
            cur.twist = rb.twist / deg;
            cur.lower = rb.lower / deg;
            cur.upper = rb.upper / deg;
            cur.hinge = rb.hinge;
        }
        bool changed = false;
        changed |= ImGui::DragFloat3("Pivot", &cur.pivot.x, 0.05f, -200.f, 200.f, "%.2f");
        undoOnStart("rest pivot");
        changed |= ImGui::DragFloat3("End", &cur.end.x, 0.05f, -200.f, 200.f, "%.2f");
        undoOnStart("rest end");
        float len = glm::length(cur.end - cur.pivot);
        if(ImGui::DragFloat("Length", &len, 0.05f, 0.1f, 200.f, "%.2f"))
        {
            const glm::vec3 d = glm::length(cur.end - cur.pivot) > 1e-4f ? glm::normalize(cur.end - cur.pivot) : glm::vec3{1.f, 0.f, 0.f};
            cur.end = cur.pivot + d * len;
            changed = true;
        }
        undoOnStart("bone length");
        if(rb.joint == ragdoll::Joint::Ball)
        {
            changed |= ImGui::DragFloat("Cone", &cur.cone, 0.2f, 0.f, 180.f, "%.1f deg");
            undoOnStart("cone");
            changed |= ImGui::DragFloat("Twist", &cur.twist, 0.2f, 0.f, 180.f, "%.1f deg");
            undoOnStart("twist");
        }
        else if(rb.joint == ragdoll::Joint::Hinge)
        {
            changed |= ImGui::DragFloat("Lower", &cur.lower, 0.2f, -180.f, 0.f, "%.1f deg");
            undoOnStart("lower");
            changed |= ImGui::DragFloat("Upper", &cur.upper, 0.2f, 0.f, 180.f, "%.1f deg");
            undoOnStart("upper");
            changed |= ImGui::DragFloat3("Hinge", &cur.hinge.x, 0.01f, -1.f, 1.f, "%.2f");
            undoOnStart("hinge");
        }
        if(changed)
        {
            cur.on = true;
            e = cur;
            touchedRig();
        }
        if(ImGui::Button("Reset to Computed") && e.on)
        {
            pushUndo("reset bone");
            e = rigdata::BoneEdit{};
            touchedRig();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(e.on ? "(overridden)" : "(computed)");
        helpMarker("The rest pose's joint (pivot) and far end, model units, and the joint's limits (the ragdolls'). Saved to "
                   "the rig file; the game takes them when the rig is next made (the next start of the game). The Rest Pose "
                   "tool (T) moves the pivot or the end with the handle.");
    }
    if(ImGui::CollapsingHeader("IK"))
    {
        int lower = -1;
        for(int c = 0; c < st.numBones; c++)
        {
            if(st.rig->bones[c].parent == b && st.rig->bones[c].joint == ragdoll::Joint::Hinge && rb.joint == ragdoll::Joint::Ball)
            {
                lower = c;
            }
        }
        if(rb.joint == ragdoll::Joint::Hinge && rb.parent >= 0 && st.rig->bones[rb.parent].joint == ragdoll::Joint::Ball)
        {
            ImGui::Text("chain: %s -> %s (its end the target)", st.rig->bones[rb.parent].name, rb.name);
            const glm::vec3 pole = rigpose::apply(st.F[b], st.pivots[b]) - rigpose::apply(st.F[rb.parent], st.pivots[rb.parent]);
            ImGui::TextDisabled("pole (the elbow's way): %.2f %.2f %.2f", pole.x, pole.y, pole.z);
        }
        else if(lower >= 0)
        {
            ImGui::Text("chain: %s -> %s", rb.name, st.rig->bones[lower].name);
        }
        else
        {
            ImGui::TextDisabled("not part of a two-bone chain");
        }
        ImGui::TextDisabled("The IK tool (E) drags a chain's end.");
    }
    if(ImGui::CollapsingHeader("Pose", ImGuiTreeNodeFlags_DefaultOpen))
    {
        rigdata::Local& l = st.live[b];
        ImGui::TextDisabled(st.source == Source::Rest ? "its local (the rest pose's: 0 0 0)" : "a change not keyed yet (I: key)");
        bool changed = ImGui::DragFloat3("Turn", &l.rot.x, 0.25f, -360.f, 360.f, "%.1f");
        undoOnStart("turn");
        changed |= ImGui::DragFloat3("Offset", &l.off.x, 0.05f, -100.f, 100.f, "%.2f");
        undoOnStart("offset");
        if(changed)
        {
            publish();
        }
        if(ImGui::Button("Key (I)"))
        {
            pushUndo("key");
            keySelected();
            publish();
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear"))
        {
            pushUndo("clear");
            l = rigdata::Local{};
            publish();
        }
        const glm::vec3 at = rigpose::apply(st.F[b], st.pivots[b]);
        ImGui::TextDisabled("joint now: %.2f %.2f %.2f", at.x, at.y, at.z);
    }
    ImGui::End();
}

// ---- The timeline ----

void transport()
{
    const char* label = st.source == Source::Rest ? "Rest Pose"
                        : st.source == Source::Anim ? va("Animation: %s", groupAt(st.group).name)
                                                    : va("Sequence: %s", curSeq() ? curSeq()->name.cStr() : "?");
    ImGui::SetNextItemWidth(200.f);
    if(ImGui::BeginCombo("##source", label))
    {
        if(ImGui::Selectable("Rest Pose", st.source == Source::Rest))
        {
            st.source = Source::Rest;
            clearLive();
            setFrame(0.f);
        }
        ImGui::SeparatorText("The model's animations");
        for(int i = 0; i < groupCount(); i++)
        {
            bool has = false;
            for(const rigdata::AnimOverride& o : st.doc.overrides)
            {
                has = has || (!q_strcasecmp(o.anim.cStr(), groupAt(i).name) && !o.tracks.empty());
            }
            if(ImGui::Selectable(va("%s (%d frames)%s##g%d", groupAt(i).name, groupAt(i).count, has ? " *" : "", i),
                   st.source == Source::Anim && st.group == i))
            {
                st.source = Source::Anim;
                st.group = i;
                clearLive();
                setFrame(0.f);
            }
        }
        ImGui::SeparatorText("New sequences");
        for(za::SizeT i = 0; i < st.doc.seqs.size(); i++)
        {
            const bool dirty = i < st.dirtySeq.size() && st.dirtySeq[i];
            if(ImGui::Selectable(va("%s%s##s%d", st.doc.seqs[i].name.cStr(), dirty ? " *" : "", static_cast<int>(i)),
                   st.source == Source::Seq && st.seq == static_cast<int>(i)))
            {
                st.source = Source::Seq;
                st.seq = static_cast<int>(i);
                clearLive();
                setFrame(0.f);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(ImGui::Button("|<"))
    {
        setFrame(0.f);
    }
    ImGui::SameLine();
    if(ImGui::Button("<"))
    {
        setFrame(za::floor(st.frame + 0.5f) - 1.f);
    }
    ImGui::SameLine();
    if(ImGui::Button(st.playing ? "Pause" : "Play"))
    {
        st.playing = !st.playing;
    }
    ImGui::SameLine();
    if(ImGui::Button(">"))
    {
        setFrame(za::floor(st.frame + 0.5f) + 1.f);
    }
    ImGui::SameLine();
    if(ImGui::Button(">|"))
    {
        setFrame(timelineLength() - 1.f);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &st.loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::SliderFloat("Speed", &st.speed, 0.1f, 2.f, "%.2fx");
    ImGui::SameLine();
    ImGui::Text("frame %.2f / %g", st.frame, timelineLength());
    ImGui::SameLine();
    ImGui::Checkbox("Onion", &st.onion);
    ImGui::SameLine();
    if(ImGui::Button("Key (I)"))
    {
        pushUndo("key");
        keySelected();
        publish();
    }
}

void dopeSheet()
{
    za::Vector<rigdata::Track>* tracks = curTracks(false);
    const float len = za::max(timelineLength(), 1.f);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float nameW = 120.f, rowH = 18.f, rulerH = 20.f;
    // The rows: the bones with a track, then the selected others.
    int rows[maxBones];
    int nRows = 0;
    for(int b = 0; b < st.numBones; b++)
    {
        const bool has = tracks && trackOf(*tracks, b, false) && !trackOf(*tracks, b, false)->keys.empty();
        if(has || (st.selBones & (1u << b)))
        {
            rows[nRows++] = b;
        }
    }
    const rigdata::Sequence* seq = st.source == Source::Seq ? curSeq() : nullptr;
    const float eventsH = seq ? rowH : 0.f;
    const float h = za::max(avail.y, rulerH + eventsH + rowH * static_cast<float>(za::max(nRows, 1)) + 4.f);
    ImGui::BeginChild("dope", ImVec2{avail.x, za::max(avail.y, 60.f)}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float gridW = za::max(avail.x - nameW - 12.f, 50.f);
    const float fw = gridW / len;
    const auto xOf = [&](float f) { return o.x + nameW + f * fw; };
    ImGui::InvisibleButton("##canvas", ImVec2{nameW + gridW, h});
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mp = ImGui::GetIO().MousePos;
    // The ruler and the grid
    dl->AddRectFilled(ImVec2{o.x + nameW, o.y}, ImVec2{o.x + nameW + gridW, o.y + rulerH}, u32(glm::vec4{0.18f, 0.18f, 0.2f, 1.f}));
    const int step = fw < 6.f ? 10 : fw < 14.f ? 5 : 1;
    for(int f = 0; f <= static_cast<int>(len); f += step)
    {
        const float x = xOf(static_cast<float>(f));
        dl->AddLine(ImVec2{x, o.y + rulerH * 0.5f}, ImVec2{x, o.y + h}, u32(glm::vec4{1.f, 1.f, 1.f, f % (step * 5) == 0 ? 0.14f : 0.06f}));
        dl->AddText(ImVec2{x + 2.f, o.y + 2.f}, u32(glm::vec4{0.75f, 0.75f, 0.75f, 1.f}), va("%d", f));
    }
    // Events
    float y = o.y + rulerH;
    if(seq)
    {
        dl->AddText(ImVec2{o.x + 4.f, y + 2.f}, u32(glm::vec4{0.9f, 0.8f, 0.4f, 1.f}), "events");
        for(const rigdata::Event& e : seq->events)
        {
            const float x = xOf(static_cast<float>(e.frame));
            dl->AddTriangleFilled(ImVec2{x - 5.f, y + 2.f}, ImVec2{x + 5.f, y + 2.f}, ImVec2{x, y + rowH - 3.f}, u32(glm::vec4{1.f, 0.8f, 0.2f, 1.f}));
            dl->AddText(ImVec2{x + 6.f, y + 2.f}, u32(glm::vec4{1.f, 0.85f, 0.5f, 1.f}), e.name.cStr());
        }
        y += rowH;
    }
    // The rows' keys
    const float rowsTop = y;
    for(int r = 0; r < nRows; r++)
    {
        const int b = rows[r];
        const float ry = rowsTop + static_cast<float>(r) * rowH;
        if(r % 2 == 0)
        {
            dl->AddRectFilled(ImVec2{o.x, ry}, ImVec2{o.x + nameW + gridW, ry + rowH}, u32(glm::vec4{1.f, 1.f, 1.f, 0.03f}));
        }
        const glm::vec4 bc = (st.selBones & (1u << b)) ? glm::vec4{1.f, 0.7f, 0.3f, 1.f} : glm::vec4{0.85f, 0.85f, 0.85f, 1.f};
        dl->AddText(ImVec2{o.x + 4.f, ry + 2.f}, u32(bc), st.rig->bones[b].name);
        const rigdata::Track* t = tracks ? trackOf(*tracks, b, false) : nullptr;
        for(za::SizeT k = 0; t && k < t->keys.size(); k++)
        {
            const rigdata::Key& key = t->keys[k];
            const float x = xOf(key.frame);
            const float cy = ry + rowH * 0.5f;
            bool sel = false;
            for(const KeyRef& kr : st.selKeys)
            {
                sel = sel || (kr.bone == b && static_cast<float>(kr.frame) == key.frame);
            }
            const ImU32 kc = u32(sel ? glm::vec4{1.f, 0.65f, 0.15f, 1.f}
                                 : key.interp == rigdata::Interp::Step ? glm::vec4{0.6f, 0.6f, 0.95f, 1.f}
                                 : key.interp == rigdata::Interp::Linear ? glm::vec4{0.7f, 0.95f, 0.7f, 1.f}
                                                                          : glm::vec4{0.92f, 0.92f, 0.92f, 1.f});
            dl->AddQuadFilled(ImVec2{x, cy - 6.f}, ImVec2{x + 6.f, cy}, ImVec2{x, cy + 6.f}, ImVec2{x - 6.f, cy}, kc);
        }
    }
    // The playhead
    const float px = xOf(st.frame);
    dl->AddLine(ImVec2{px, o.y}, ImVec2{px, o.y + h}, u32(glm::vec4{0.3f, 0.7f, 1.f, 1.f}), 2.f);
    // Interaction
    const auto frameAt = [&](float x) { return za::clamp((x - o.x - nameW) / fw, 0.f, len - (seq && seq->loop ? 0.001f : 1.f)); };
    const auto keyAt = [&](const ImVec2& p, KeyRef& out) {
        for(int r = 0; r < nRows; r++)
        {
            const int b = rows[r];
            const float ry = rowsTop + static_cast<float>(r) * rowH;
            if(p.y < ry || p.y > ry + rowH)
            {
                continue;
            }
            const rigdata::Track* t = tracks ? trackOf(*tracks, b, false) : nullptr;
            for(za::SizeT k = 0; t && k < t->keys.size(); k++)
            {
                if(za::abs(p.x - xOf(t->keys[k].frame)) < 7.f)
                {
                    out.bone = b;
                    out.frame = static_cast<int>(t->keys[k].frame);
                    return true;
                }
            }
        }
        return false;
    };
    ImGuiIO& io = ImGui::GetIO();
    if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        KeyRef kr;
        if(mp.y < o.y + rulerH || mp.x < o.x + nameW)
        {
            st.dragging = true;
            st.dragKind = 8; // scrub
        }
        else if(keyAt(mp, kr))
        {
            bool already = false;
            for(const KeyRef& s : st.selKeys)
            {
                already = already || (s.bone == kr.bone && s.frame == kr.frame);
            }
            if(!io.KeyShift && !already)
            {
                st.selKeys.clear();
            }
            if(!already)
            {
                st.selKeys.pushBack(kr);
            }
            st.dragging = true;
            st.dragKind = 9; // keys moved
            st.dragStart = glm::vec2{mp.x, mp.y};
            st.dragAngle = 0.f;
            pushUndo("move keys");
        }
        else
        {
            if(!io.KeyShift)
            {
                st.selKeys.clear();
            }
            st.dragging = true;
            st.dragKind = 10; // box on the keys
            st.dragStart = glm::vec2{mp.x, mp.y};
        }
    }
    if(st.dragging && st.dragKind >= 8 && st.dragKind <= 10)
    {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if(st.dragKind == 8)
            {
                setFrame(io.KeyShift ? frameAt(mp.x) : za::floor(frameAt(mp.x) + 0.5f));
            }
            else if(st.dragKind == 9 && tracks)
            {
                const int want = static_cast<int>(za::floor((mp.x - st.dragStart.x) / fw + 0.5f));
                const int by = want - static_cast<int>(st.dragAngle);
                if(by != 0)
                {
                    for(KeyRef& kr : st.selKeys)
                    {
                        rigdata::Track* t = trackOf(*tracks, kr.bone, false);
                        for(za::SizeT k = 0; t && k < t->keys.size(); k++)
                        {
                            if(static_cast<int>(t->keys[k].frame) == kr.frame)
                            {
                                t->keys[k].frame = static_cast<float>(za::max(kr.frame + by, 0));
                                kr.frame = static_cast<int>(t->keys[k].frame);
                                break;
                            }
                        }
                    }
                    st.dragAngle = static_cast<float>(want);
                }
            }
            else if(st.dragKind == 10)
            {
                dl->AddRect(ImVec2{st.dragStart.x, st.dragStart.y}, mp, u32(glm::vec4{0.4f, 0.7f, 1.f, 0.9f}));
            }
        }
        else
        {
            if(st.dragKind == 9 && tracks)
            {
                // (in frame order again; a key landing on another replaces it)
                for(rigdata::Track& t : *tracks)
                {
                    for(za::SizeT i = 1; i < t.keys.size(); i++)
                    {
                        for(za::SizeT j = i; j > 0 && t.keys[j].frame < t.keys[j - 1].frame; j--)
                        {
                            const rigdata::Key tmp = t.keys[j];
                            t.keys[j] = t.keys[j - 1];
                            t.keys[j - 1] = tmp;
                        }
                    }
                    for(za::SizeT i = 1; i < t.keys.size();)
                    {
                        if(t.keys[i].frame == t.keys[i - 1].frame)
                        {
                            t.keys.erase(t.keys.begin() + static_cast<za::PtrDiffT>(i - 1));
                        }
                        else
                        {
                            i++;
                        }
                    }
                }
                st.source == Source::Seq ? touchedSeq(st.seq) : touchedRig();
            }
            else if(st.dragKind == 10)
            {
                const float x0 = za::min(st.dragStart.x, mp.x), x1 = za::max(st.dragStart.x, mp.x);
                const float y0 = za::min(st.dragStart.y, mp.y), y1 = za::max(st.dragStart.y, mp.y);
                for(int r = 0; r < nRows && tracks; r++)
                {
                    const float cy = rowsTop + static_cast<float>(r) * rowH + rowH * 0.5f;
                    const rigdata::Track* t = trackOf(*tracks, rows[r], false);
                    for(za::SizeT k = 0; t && k < t->keys.size(); k++)
                    {
                        const float x = xOf(t->keys[k].frame);
                        if(x >= x0 && x <= x1 && cy >= y0 && cy <= y1)
                        {
                            st.selKeys.pushBack(KeyRef{rows[r], static_cast<int>(t->keys[k].frame)});
                        }
                    }
                }
                if(x1 - x0 < 3.f && y1 - y0 < 3.f)
                {
                    setFrame(za::floor(frameAt(mp.x) + 0.5f)); // (a click on the grid: the frame there)
                }
            }
            st.dragging = false;
            st.dragKind = 0;
        }
    }
    // Keys: the interpolation (right click), delete, copy and paste.
    KeyRef rk;
    if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && keyAt(mp, rk))
    {
        bool already = false;
        for(const KeyRef& s : st.selKeys)
        {
            already = already || (s.bone == rk.bone && s.frame == rk.frame);
        }
        if(!already)
        {
            st.selKeys.clear();
            st.selKeys.pushBack(rk);
        }
        ImGui::OpenPopup("keymenu");
    }
    if(ImGui::BeginPopup("keymenu"))
    {
        const rigdata::Interp modes[3] = {rigdata::Interp::Smooth, rigdata::Interp::Linear, rigdata::Interp::Step};
        for(const rigdata::Interp m : modes)
        {
            if(ImGui::MenuItem(rigdata::interpName(m)) && tracks)
            {
                pushUndo("interpolation");
                for(const KeyRef& kr : st.selKeys)
                {
                    rigdata::Track* t = trackOf(*tracks, kr.bone, false);
                    for(za::SizeT k = 0; t && k < t->keys.size(); k++)
                    {
                        if(static_cast<int>(t->keys[k].frame) == kr.frame)
                        {
                            t->keys[k].interp = m;
                        }
                    }
                }
                st.source == Source::Seq ? touchedSeq(st.seq) : touchedRig();
            }
        }
        ImGui::EndPopup();
    }
    if(ImGui::IsWindowFocused() && tracks && !io.WantTextInput)
    {
        if((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_X)) && !st.selKeys.empty())
        {
            pushUndo("delete keys");
            for(const KeyRef& kr : st.selKeys)
            {
                rigdata::Track* t = trackOf(*tracks, kr.bone, false);
                for(za::SizeT k = 0; t && k < t->keys.size(); k++)
                {
                    if(static_cast<int>(t->keys[k].frame) == kr.frame)
                    {
                        t->keys.erase(t->keys.begin() + static_cast<za::PtrDiffT>(k));
                        break;
                    }
                }
            }
            st.selKeys.clear();
            st.source == Source::Seq ? touchedSeq(st.seq) : touchedRig();
        }
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false) && !st.selKeys.empty())
        {
            st.keyClip.clear();
            int first = 1 << 30;
            for(const KeyRef& kr : st.selKeys)
            {
                first = za::min(first, kr.frame);
            }
            for(const KeyRef& kr : st.selKeys)
            {
                const rigdata::Track* t = trackOf(*tracks, kr.bone, false);
                for(za::SizeT k = 0; t && k < t->keys.size(); k++)
                {
                    if(static_cast<int>(t->keys[k].frame) == kr.frame)
                    {
                        rigdata::Track* c = trackOf(st.keyClip, kr.bone, true);
                        rigdata::Key key = t->keys[k];
                        key.frame -= static_cast<float>(first);
                        c->keys.pushBack(key);
                    }
                }
            }
            say(va("%d keys copied", static_cast<int>(st.selKeys.size())));
        }
        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false) && !st.keyClip.empty())
        {
            pushUndo("paste keys");
            za::Vector<rigdata::Track>* dst = curTracks(true);
            const float at = za::floor(st.frame + 0.5f);
            for(const rigdata::Track& c : st.keyClip)
            {
                rigdata::Track* t = trackOf(*dst, c.bone, true);
                for(rigdata::Key key : c.keys)
                {
                    key.frame += at;
                    za::vectorEraseIf(t->keys, [&](const rigdata::Key& k) { return k.frame == key.frame; });
                    za::SizeT pos = 0;
                    while(pos < t->keys.size() && t->keys[pos].frame < key.frame)
                    {
                        pos++;
                    }
                    t->keys.insert(t->keys.begin() + static_cast<za::PtrDiffT>(pos), key);
                }
            }
            st.source == Source::Seq ? touchedSeq(st.seq) : touchedRig();
        }
    }
    if(nRows == 0)
    {
        dl->AddText(ImVec2{o.x + nameW + 8.f, rowsTop + 2.f}, u32(glm::vec4{0.6f, 0.6f, 0.6f, 1.f}),
            st.source == Source::Rest ? "The rest pose: pick an animation or a sequence above to key." : "Select bones to see their tracks; I keys them.");
    }
    ImGui::EndChild();
}

void timelineWindow()
{
    if(!st.showTimeline || !st.rig)
    {
        return;
    }
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2{242.f, ds.y - za::min(ds.y * 0.3f, 220.f) - 6.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{ds.x - 242.f - 292.f, za::min(ds.y * 0.3f, 220.f)}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Timeline", &st.showTimeline))
    {
        ImGui::End();
        return;
    }
    transport();
    dopeSheet();
    ImGui::End();
}

void sequenceWindow()
{
    if(!st.showSequence || !st.rig)
    {
        return;
    }
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2{ds.x - 286.f, za::min(ds.y * 0.5f, 420.f) + 32.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{280.f, za::max(ds.y - za::min(ds.y * 0.5f, 420.f) - 40.f, 160.f)}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Sequence", &st.showSequence))
    {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputText("##newname", st.newSeqName, sizeof(st.newSeqName));
    ImGui::SameLine();
    if(ImGui::Button("New Sequence"))
    {
        newSequence(st.newSeqName, 10, 10.f, false);
    }
    rigdata::Sequence* s = st.source == Source::Seq ? curSeq() : nullptr;
    if(!s)
    {
        ImGui::TextDisabled("Pick a sequence in the timeline, or make one.");
        ImGui::End();
        return;
    }
    bool changed = false;
    char name[64];
    q_strlcpy(name, s->name.cStr(), sizeof(name));
    if(ImGui::InputText("Name", name, sizeof(name), ImGuiInputTextFlags_EnterReturnsTrue) && name[0])
    {
        pushUndo("rename");
        s->name = name;
        changed = true;
    }
    int len = s->length;
    if(ImGui::DragInt("Frames", &len, 0.2f, 1, 2000))
    {
        s->length = za::max(len, 1);
        changed = true;
    }
    undoOnStart("length");
    float secs = rigdata::seconds(*s);
    if(ImGui::DragFloat("Seconds", &secs, 0.01f, 0.05f, 200.f, "%.2f"))
    {
        s->length = za::max(static_cast<int>(secs * s->fps + 0.5f), 1);
        changed = true;
    }
    undoOnStart("length");
    changed |= ImGui::DragFloat("FPS", &s->fps, 0.1f, 0.5f, 120.f, "%.1f");
    undoOnStart("fps");
    changed |= ImGui::Checkbox("Loop", &s->loop);
    undoOnStart("loop");
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Arms", &s->arms);
    undoOnStart("arms");
    helpMarker("Arms: its arm bones place an armed humanoid's hands (its weapons with them), through the game's arm IK. Off: "
               "the arms stay on the weapons as the game poses them.");
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Hold", &s->hold);
    undoOnStart("hold");
    helpMarker("Hold: played over a held frame (its base): the monster's own animation taking over (a pain) ends it.");
    changed |= ImGui::DragFloat("Blend In", &s->blendIn, 0.01f, 0.f, 2.f, "%.2f s");
    undoOnStart("blend");
    changed |= ImGui::DragFloat("Blend Out", &s->blendOut, 0.01f, 0.f, 2.f, "%.2f s");
    undoOnStart("blend");
    if(const aliashdr_t* hdr = hdrOf())
    {
        if(ImGui::BeginCombo("Base Frame", s->base.empty() ? "(the first)" : s->base.cStr()))
        {
            if(ImGui::Selectable("(the first)", s->base.empty()))
            {
                pushUndo("base");
                s->base = "";
                changed = true;
            }
            for(int f = 0; f < hdr->numframes; f++)
            {
                if(ImGui::Selectable(va("%s##f%d", hdr->frames[f].name, f), !q_strcasecmp(s->base.cStr(), hdr->frames[f].name)))
                {
                    pushUndo("base");
                    s->base = hdr->frames[f].name;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SeparatorText("Events (QC's VR_RigSeq_Event)");
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputText("##ev", st.eventName, sizeof(st.eventName));
    ImGui::SameLine();
    if(ImGui::Button(va("Add at %d", static_cast<int>(za::floor(st.frame + 0.5f)))) && st.eventName[0])
    {
        pushUndo("event");
        rigdata::Event e;
        e.frame = static_cast<int>(za::floor(st.frame + 0.5f));
        e.name = st.eventName;
        za::SizeT at = 0;
        while(at < s->events.size() && s->events[at].frame <= e.frame)
        {
            at++;
        }
        s->events.insert(s->events.begin() + static_cast<za::PtrDiffT>(at), e);
        changed = true;
    }
    for(za::SizeT i = 0; i < s->events.size(); i++)
    {
        ImGui::PushID(static_cast<int>(i));
        int f = s->events[i].frame;
        ImGui::SetNextItemWidth(60.f);
        if(ImGui::DragInt("##f", &f, 0.2f, 0, s->length - 1))
        {
            s->events[i].frame = f;
            changed = true;
        }
        undoOnStart("event frame");
        ImGui::SameLine();
        ImGui::TextUnformatted(s->events[i].name.cStr());
        ImGui::SameLine(200.f);
        if(ImGui::SmallButton("X"))
        {
            pushUndo("delete event");
            s->events.erase(s->events.begin() + static_cast<za::PtrDiffT>(i));
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    if(ImGui::Button("Duplicate"))
    {
        pushUndo("duplicate");
        rigdata::Sequence copy = *s;
        copy.name = va("%s_copy", s->name.cStr());
        st.doc.seqs.pushBack(ZA_MOVE(copy));
        st.dirtySeq.pushBack(1);
        st.seq = static_cast<int>(st.doc.seqs.size()) - 1;
        changed = true;
        s = curSeq();
    }
    ImGui::SameLine();
    if(ImGui::Button("Test on Subject"))
    {
        save();
        Cbuf_AddText(va("vr_rigseq_play %s %d\n", s->name.cStr(), st.subject));
    }
    helpMarker("Saves, then plays it on the subject as QC would (vr_rigseq_play): the game's own drawing, its blends, its "
               "events printed with vr_rigseq_debug 1.");
    if(changed)
    {
        touchedSeq(st.seq);
    }
    ImGui::End();
}

void poseDataWindow()
{
    if(!st.showPoseData)
    {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2{250.f, 40.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{360.f, 420.f}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Pose Studio Data", &st.showPoseData))
    {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("The armed humanoids' pose keys and grips, the Pose Studio's own data (vr_agrunt_poses, "
                       "vr_agrunt_grips): edited here, drawn by the game at once.");
    const char* kinds[] = {"Grunt", "Enforcer"};
    ImGui::Combo("Kind", &st.pdKind, kinds, 2);
    if(ImGui::BeginTabBar("pd"))
    {
        if(ImGui::BeginTabItem("Pose Keys"))
        {
            if(ImGui::BeginCombo("Class", agpose::className(st.pdClass)))
            {
                for(int c = 0; c < agpose::NumClasses; c++)
                {
                    if(ImGui::Selectable(agpose::className(c), st.pdClass == c))
                    {
                        st.pdClass = c;
                    }
                }
                ImGui::EndCombo();
            }
            int poses[agpose::NumPoses];
            const int np = agpose::posesOf(st.pdClass, poses, agpose::NumPoses);
            if(ImGui::BeginCombo("Pose", agpose::poseName(st.pdClass, st.pdPose)))
            {
                for(int i = 0; i < np; i++)
                {
                    if(ImGui::Selectable(agpose::poseName(st.pdClass, poses[i]), st.pdPose == poses[i]))
                    {
                        st.pdPose = poses[i];
                    }
                }
                ImGui::EndCombo();
            }
            const int phases = agpose::isStrike(st.pdPose) ? agpose::numPhases : 1;
            st.pdPhase = za::clamp(st.pdPhase, 0, phases - 1);
            if(phases > 1)
            {
                ImGui::SliderInt("Phase", &st.pdPhase, 0, phases - 1, agpose::phaseName(st.pdPhase));
            }
            const int code = agpose::code(st.pdKind, st.pdClass, st.pdPose, st.pdPhase);
            agpose::Key k = agpose::value(code);
            const char* parts[agpose::NumParts] = {"Right Place", "Right Turn", "Left Place", "Left Turn", "Right Elbow Pole", "Left Elbow Pole", "Torso Turn"};
            bool changed = false;
            for(int p = 0; p < agpose::NumParts; p++)
            {
                changed |= ImGui::DragFloat3(parts[p], &k.v[p].x, p == agpose::Torso || p == agpose::RTurn || p == agpose::LTurn ? 0.25f : 0.05f);
                if(ImGui::IsItemActivated())
                {
                    pushPoseUndo(code);
                }
            }
            if(changed)
            {
                agpose::set(code, k);
            }
            if(ImGui::Button("Reset This Pose"))
            {
                for(int ph = 0; ph < agpose::numPhases; ph++)
                {
                    pushPoseUndo(agpose::code(st.pdKind, st.pdClass, st.pdPose, ph));
                }
                agpose::resetPose(st.pdKind, st.pdClass, st.pdPose);
            }
            ImGui::SameLine();
            if(ImGui::Button("Print"))
            {
                Cbuf_AddText("vr_agrunt_pose_dump\n");
            }
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Grips"))
        {
            const char* hands[] = {"Main Hand", "Off Hand (dual)", "Second Grip"};
            ImGui::Combo("Hand", &st.pdHand, hands, 3);
            ImGui::Checkbox("Every Weapon (the kind's global)", &st.pdGlobal);
            const int slot = st.pdGlobal ? aggrip::allWeapons : (st.pdHand == 1 ? st.note.slotL : st.note.slot);
            if(!st.pdGlobal && slot < 0)
            {
                ImGui::TextDisabled("(the subject holds no weapon in that hand)");
            }
            else
            {
                aggrip::Offset o = st.pdGlobal ? aggrip::global(st.pdKind, st.pdHand) : aggrip::offset(st.pdKind, slot, st.pdHand);
                bool changed = ImGui::DragFloat3("Place", &o.pos.x, 0.05f, -40.f, 40.f, "%.2f");
                if(ImGui::IsItemActivated())
                {
                    pushGripUndo(st.pdKind, slot, st.pdHand);
                }
                changed |= ImGui::DragFloat3("Angles", &o.angles.x, 0.25f, -180.f, 180.f, "%.1f");
                if(ImGui::IsItemActivated())
                {
                    pushGripUndo(st.pdKind, slot, st.pdHand);
                }
                if(changed)
                {
                    aggrip::set(st.pdKind, slot, st.pdHand, o);
                }
                if(ImGui::Button("Reset"))
                {
                    pushGripUndo(st.pdKind, slot, st.pdHand);
                    aggrip::resetEntries(st.pdKind, slot);
                }
                ImGui::SameLine();
                if(ImGui::Button("Print"))
                {
                    Cbuf_AddText("vr_agrunt_grip_dump\n");
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void helpWindow()
{
    if(ImGui::IsKeyPressed(ImGuiKey_F1, false))
    {
        st.showHelp = !st.showHelp;
    }
    if(!st.showHelp)
    {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2{260.f, 60.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{430.f, 420.f}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Help: Controls", &st.showHelp))
    {
        ImGui::End();
        return;
    }
    const char* rows[][2] = {
        {"Middle mouse", "orbit round the focus"},
        {"Shift + middle mouse", "pan"},
        {"Wheel", "zoom"},
        {"Alt + left mouse", "orbit (laptops); Shift too: pan"},
        {"Right mouse drag", "look about (the camera stays)"},
        {"F", "frame the selection (or the model)"},
        {"1 / 3 / 7 (numpad too)", "front / right / top; Ctrl: back / left / bottom"},
        {"Left click", "select a bone (Shift: add); a handle: drag it"},
        {"Q R G E T W P", "tools: select, rotate, move, IK, rest pose, weights, paint"},
        {"Ctrl (dragging)", "snap: 15 degrees, 1 unit"},
        {"I", "key the selected bones at the frame"},
        {"Space / Left / Right", "play or pause / a frame back / on"},
        {"H / Alt+H", "hide the selected bones / show all"},
        {"Weights: click, Shift, Ctrl", "select a vertex, add, remove; drag: a box"},
        {"Paint: drag", "the brush (Tools > Weights: add, subtract, smooth)"},
        {"Dope sheet", "click/drag keys (Shift adds), drag the ruler to scrub, a box selects, right click: interpolation, "
                       "Delete/X deletes, Ctrl+C / Ctrl+V copy and paste at the frame"},
        {"Ctrl+Z / Ctrl+Y", "undo / redo (every edit)"},
        {"Ctrl+S", "save (rigs/<model>.rig.txt, anims/<model>/<name>.anim.txt)"},
        {"Esc (dragging)", "cancel the drag"},
        {"F1", "this window"},
        {"` (the console key)", "the console, as ever; vr_rigstudio_close closes the studio"},
    };
    if(ImGui::BeginTable("help", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        for(const auto& r : rows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(r[0]);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", r[1]);
        }
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Docs: docs/vr-port/RIGSTUDIO.md (the files' formats, QC's API).");
    ImGui::End();
}

void prefsWindow()
{
    if(!st.showPrefs)
    {
        return;
    }
    ImGui::SetNextWindowPos(ImVec2{300.f, 80.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2{330.f, 200.f}, ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Preferences", &st.showPrefs))
    {
        ImGui::End();
        return;
    }
    float orbit = vr_rigstudio_orbit_speed.value, pan = vr_rigstudio_pan_speed.value, zoom = vr_rigstudio_zoom_speed.value,
          handle = vr_rigstudio_handle_size.value;
    if(ImGui::SliderFloat("Orbit Speed", &orbit, 0.05f, 2.f, "%.2f deg/px"))
    {
        Cvar_SetValueQuick(&vr_rigstudio_orbit_speed, orbit);
    }
    if(ImGui::SliderFloat("Pan Speed", &pan, 0.1f, 4.f, "%.2f"))
    {
        Cvar_SetValueQuick(&vr_rigstudio_pan_speed, pan);
    }
    if(ImGui::SliderFloat("Zoom Speed", &zoom, 0.02f, 0.5f, "%.2f"))
    {
        Cvar_SetValueQuick(&vr_rigstudio_zoom_speed, zoom);
    }
    if(ImGui::SliderFloat("Handle Size", &handle, 30.f, 250.f, "%.0f px"))
    {
        Cvar_SetValueQuick(&vr_rigstudio_handle_size, handle);
    }
    const char* themes[] = {"Dark", "Light", "Classic"};
    int theme = static_cast<int>(vr_rigstudio_theme.value);
    if(ImGui::Combo("Theme", &theme, themes, 3))
    {
        Cvar_SetValueQuick(&vr_rigstudio_theme, static_cast<float>(theme));
        imgui::setTheme(theme);
    }
    ImGui::TextDisabled("The windows' places are kept in quakevr/imgui.ini.");
    ImGui::End();
}

} // namespace

namespace qvr::rigstudio::detail
{

glm::vec4 boneColor(int b)
{
    // (a hue each, spread round the wheel)
    const float h = static_cast<float>((b * 7) % 16) / 16.f;
    float r, g, bl;
    ImGui::ColorConvertHSVtoRGB(h, 0.7f, 0.95f, r, g, bl);
    return glm::vec4{r, g, bl, 1.f};
}

void windows()
{
    menuBar();
    toolsWindow();
    outlinerWindow();
    propertiesWindow();
    timelineWindow();
    sequenceWindow();
    poseDataWindow();
    helpWindow();
    prefsWindow();
}

} // namespace qvr::rigstudio::detail
