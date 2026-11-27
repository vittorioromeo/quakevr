// vr_armourstudio_window.cpp -- the Armour Studio's ImGui window (vr_armourstudio.hpp; docs/vr-port/ARMOUR_STUDIO.md,
// "The ImGui window"): the Debug > Tools > Armour Studio page's rows in a floating flat-screen window, opened and closed
// over play like the Weapon Studio's (vr_armourstudio_window toggles it; its X and Escape close it).
//
// It is a second view of the same studio, not a second studio: every row writes the page's own vr_armourstudio_* cvar
// (Cvar_SetValueQuick, as the menu's slider does) and every button runs the page's own command (Cbuf_AddText), so
// vr_armourstudio.cpp's frame lays them into the tweaks' table, keeps its undo and its subject, and the menu and the
// window always show the same values. Its own state is only where the window is and its test.
//
// Input as the Weapon Studio's: while it is open ImGui gets the keys and the mouse (vr_imgui.cpp), the game's mouse
// look and keys are held back, the cursor is shown; closing gives the mouse back (IN_Activate). A key bound to
// vr_armourstudio_window still reaches its binding while the window is open, so the same key closes it.
// Flat screen only: in a headset session it does not open (ImGui is not started there) and nothing here runs.
// Never with the other ImGui tools (the Rig Studio, the wave editor, the Weapon Studio): it does not open while one is,
// and closes when one opens.

#include "vr_armourstudio.hpp"
#include "vr_armourdata.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_imgui.hpp"
#include "vr_rigstudio.hpp"
#include "vr_testkit.hpp"
#include "vr_waveeditor.hpp"
#include "vr_weaponstudio.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

#include <SDL.h>

#include <float.h>
#include <stdio.h>

extern "C" int VR_IsActive(void);

namespace qvr::armourstudio
{

namespace
{

using namespace armourdata;

// The page's slider rows (vr_menu_debug.inc pageArmourStudio): the same cvars, ranges and steps. Typed entry goes past
// the slider's range (the table clamps what it must).
struct Row
{
    const char* label;
    cvar_t* cvar;
    float lo, hi, step;
    const char* format;
};
const Row fieldRows[NumFields] = {
    {"Forward", &vr_armourstudio_offset_x, -6.f, 6.f, 0.1f, "%.2f"},
    {"Left", &vr_armourstudio_offset_y, -6.f, 6.f, 0.1f, "%.2f"},
    {"Up", &vr_armourstudio_offset_z, -6.f, 6.f, 0.1f, "%.2f"},
    {"Pitch", &vr_armourstudio_pitch, -45.f, 45.f, 1.f, "%.1f deg"},
    {"Yaw", &vr_armourstudio_yaw, -45.f, 45.f, 1.f, "%.1f deg"},
    {"Roll", &vr_armourstudio_roll, -45.f, 45.f, 1.f, "%.1f deg"},
    {"Scale", &vr_armourstudio_scale, 0.5f, 1.5f, 0.02f, "%.3fx"},
    {"Stand-off", &vr_armourstudio_standoff, -1.f, 2.f, 0.05f, "%.2f"},
    {"Start Along", &vr_armourstudio_start, -4.f, 4.f, 0.1f, "%.2f"},
    {"End Along", &vr_armourstudio_end, -4.f, 4.f, 0.1f, "%.2f"},
    {"Rows", &vr_armourstudio_rows, 0.f, 8.f, 1.f, "%.0f"},
    {"Taper Start", &vr_armourstudio_taper_start, 0.5f, 1.5f, 0.02f, "%.3fx"},
    {"Taper End", &vr_armourstudio_taper_end, 0.5f, 1.5f, 0.02f, "%.3fx"},
    {"Leather", &vr_armourstudio_leather, 0.f, 1.f, 1.f, "%.0f"},
    {"Leather Folds", &vr_armourstudio_folds, 0.f, 4.f, 1.f, "%.0f"},
    {"Fold Depth", &vr_armourstudio_fold_depth, 0.f, 1.f, 0.02f, "%.2f"},
    {"Hero Plates", &vr_armourstudio_hero, 0.f, 1.f, 1.f, "%.0f"},
    {"Sag", &vr_armourstudio_sag, -4.f, 4.f, 0.1f, "%.2f"},
    {"Size Fwd", &vr_armourstudio_scale_x, 0.5f, 1.5f, 0.02f, "%.3fx"},
    {"Size Left", &vr_armourstudio_scale_y, 0.5f, 1.5f, 0.02f, "%.3fx"},
    {"Size Up", &vr_armourstudio_scale_z, 0.5f, 1.5f, 0.02f, "%.3fx"},
    {"Own End Fwd", &vr_armourstudio_start_offset_x, -6.f, 6.f, 0.1f, "%.2f"},
    {"Own End Left", &vr_armourstudio_start_offset_y, -6.f, 6.f, 0.1f, "%.2f"},
    {"Own End Up", &vr_armourstudio_start_offset_z, -6.f, 6.f, 0.1f, "%.2f"},
    {"Joined End Fwd", &vr_armourstudio_end_offset_x, -6.f, 6.f, 0.1f, "%.2f"},
    {"Joined End Left", &vr_armourstudio_end_offset_y, -6.f, 6.f, 0.1f, "%.2f"},
    {"Joined End Up", &vr_armourstudio_end_offset_z, -6.f, 6.f, 0.1f, "%.2f"},
    {"Own End Size", &vr_armourstudio_start_radius, 0.5f, 2.f, 0.02f, "%.3fx"},
    {"Joined End Size", &vr_armourstudio_end_radius, 0.5f, 2.f, 0.02f, "%.3fx"},
    {"Own End Far Edge", &vr_armourstudio_start_edge, 0.f, 1.f, 1.f, "%.0f"},
    {"Joined End Far Edge", &vr_armourstudio_end_edge, 0.f, 1.f, 1.f, "%.0f"},
    {"Twist", &vr_armourstudio_twist, -90.f, 90.f, 1.f, "%.0f deg"},
    {"Bulge Fwd", &vr_armourstudio_bulge_x, -4.f, 4.f, 0.1f, "%.2f"},
    {"Bulge Left", &vr_armourstudio_bulge_y, -4.f, 4.f, 0.1f, "%.2f"},
    {"Bulge Up", &vr_armourstudio_bulge_z, -4.f, 4.f, 0.1f, "%.2f"},
    {"Segments", &vr_armourstudio_segments, 0.f, 8.f, 1.f, "%.0f"},
    {"Closed All Round", &vr_armourstudio_fill, 0.f, 1.f, 1.f, "%.0f"},
};
// The subject's place and tilt (vr_armourstudio_subject_*: a look from another angle; never in armour.txt).
const Row subjectRows[5] = {
    {"Away", &vr_armourstudio_subject_x, -48.f, 48.f, 1.f, "%.0f"},
    {"Left", &vr_armourstudio_subject_y, -48.f, 48.f, 1.f, "%.0f"},
    {"Up", &vr_armourstudio_subject_z, -32.f, 48.f, 1.f, "%.0f"},
    {"Pitch", &vr_armourstudio_subject_pitch, -90.f, 90.f, 1.f, "%.0f deg"},
    {"Roll", &vr_armourstudio_subject_roll, -90.f, 90.f, 1.f, "%.0f deg"},
};
constexpr int maxSubjects = 32;
constexpr const char* tierLabels[3] = {"Green", "Yellow", "Red"};
constexpr const char* partLabels[NumParts] = {"Cuirass", "Helmet", "Pauldrons (Upper Arms)", "Bracers (Forearms)", "Thighs",
    "Shins", "Elbow Sleeves (Upper Arm's)", "Elbow Sleeves (Forearm's)", "Knee Sleeves (Thigh's)", "Knee Sleeves (Shin's)",
    "Cuirass's Leather Junction", "Helmet's Leather Junction", "Plate 8", "Plate 9", "Plate 8's Leather Junction", "Plate 9's Leather Junction",
    "Tail", "Tail's Leather Junctions", "Plate 8 Back", "Plate 8 Back's Leather Junction"};
// (a beast's limb parts' junctions: to the piece each joins)
constexpr const char* beastSleeveLabels[4] = {"Pauldrons' Leather Junctions", "Bracers' Leather Junctions", "Thighs' Leather Junctions",
    "Knee Leather (Shins' Junctions)"};
constexpr const char* sideLabels[NumSides] = {"Both (Mirrored)", "Left Only", "Right Only"};

constexpr float winMinW = 420.f, winMinH = 320.f;
constexpr int maxSpots = 48;
constexpr int testStepFrames = 400; // frames a test step may wait for its condition (a safety net: FAIL "timed out")
constexpr const char* testName = "armourwin";
constexpr const char* testFile = "data/armour_wintest.txt";

// A widget's middle, by name (the test's mouse clicks there).
struct Spot
{
    char name[32];
    float x, y;
};

struct Window
{
    bool open{false};
    bool askClose{false};
    bool placed{false};
    float x{0.f}, y{0.f}, w{560.f}, h{720.f};
    float drawn[NumFields]{}; // (the values its rows showed, last frame: the test reads them)
    Spot spots[maxSpots]{};
    int spotCount{0};
    // The test (vr_armourstudio_window test): its step, its clicks, its tally.
    int step{0};
    int stepFrames{0};
    int clickPhase{0}; // 1 resolve and move there, 2 press, 3 let go
    int keyPhase{0};   // Escape: 1 down, 2 up
    char clickName[32]{};
    float clickX{0.f}, clickY{0.f};
    float savedValue{0.f};
    char fileWas[64]{};
    testkit::Tally tally;
};
Window win;

[[nodiscard]] int cvarIndex(const cvar_t& c, const int lo, const int hi)
{
    return za::clamp(static_cast<int>(c.value), lo, hi);
}

void noteSpot(const char* name)
{
    if(win.spotCount >= maxSpots)
    {
        return;
    }
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    Spot& s = win.spots[win.spotCount++];
    q_strlcpy(s.name, name, sizeof(s.name));
    s.x = (a.x + b.x) * 0.5f;
    s.y = (a.y + b.y) * 0.5f;
}

[[nodiscard]] const Spot* spotNamed(const char* name)
{
    for(int i = 0; i < win.spotCount; i++)
    {
        if(!ZA_STRCMP(win.spots[i].name, name))
        {
            return &win.spots[i];
        }
    }
    return nullptr;
}

// A button that runs one of the page's commands (the menu's own row), named for the test's mouse.
void commandButton(const char* label, const char* command, const char* spot, const char* tip)
{
    if(ImGui::Button(label))
    {
        Cbuf_AddText(va("%s\n", command));
    }
    noteSpot(spot);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s: %s", command, tip);
    }
}

void comboRow(const char* label, cvar_t& c, const char* const* items, const int count, const int first)
{
    int chosen = cvarIndex(c, first, first + count - 1) - first;
    ImGui::SetNextItemWidth(220.f);
    if(ImGui::Combo(label, &chosen, items, count))
    {
        Cvar_SetValueQuick(&c, static_cast<float>(chosen + first));
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", c.name);
    }
}

// A value: its slider (Ctrl+click types into it) and a typed box with its - / + steps, the same cvar.
void valueRow(const Row& r, const int field)
{
    ImGui::PushID(field);
    float v = r.cvar->value;
    win.drawn[field] = v;
    if(field == Leather)
    {
        // (a sleeve's leather: shown or hidden, a checkbox)
        bool shown = v > 0.5f;
        if(ImGui::Checkbox("Leather Sleeve Shown", &shown))
        {
            Cvar_SetValueQuick(r.cvar, shown ? 1.f : 0.f);
        }
        noteSpot("leather");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s: this sleeve part's leather (elbow or knee) drawn, or not at all (leather \"0\" in the file)", r.cvar->name);
        }
        ImGui::PopID();
        return;
    }
    if(field == Hero)
    {
        // (the piece's hero plates: on or off, a checkbox)
        bool on = v > 0.5f;
        if(ImGui::Checkbox("Hero Plates", &on))
        {
            Cvar_SetValueQuick(r.cvar, on ? 1.f : 0.f);
        }
        noteSpot("hero");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s: this part's hero plates (a row's plates as one: the breastplate, the pauldrons' top row, the elbows' "
                              "and knees' cops), or every plate its own (hero \"0\" in the file)", r.cvar->name);
        }
        ImGui::PopID();
        return;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(r.label);
    ImGui::SameLine(96.f);
    ImGui::SetNextItemWidth(200.f);
    if(ImGui::SliderFloat("##slider", &v, r.lo, r.hi, r.format))
    {
        Cvar_SetValueQuick(r.cvar, field == Rows ? static_cast<float>(static_cast<int>(v + 0.5f)) : v);
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s (%g .. %g; Ctrl+click to type)", r.cvar->name, r.lo, r.hi);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.f);
    float typed = r.cvar->value;
    if(ImGui::InputFloat("##typed", &typed, r.step, r.step * 10.f, field == Rows ? "%.0f" : "%.3f"))
    {
        Cvar_SetValueQuick(r.cvar, field == Rows ? static_cast<float>(static_cast<int>(za::max(0.f, typed) + 0.5f)) : typed);
    }
    // (the typed box's + button: the last thing in its row, a frame's height square)
    {
        char name[32];
        q_snprintf(name, sizeof(name), "plus_%s", fieldNames[field]);
        if(win.spotCount < maxSpots)
        {
            const ImVec2 b = ImGui::GetItemRectMax(), a = ImGui::GetItemRectMin();
            Spot& s = win.spots[win.spotCount++];
            q_strlcpy(s.name, name, sizeof(s.name));
            s.x = b.x - ImGui::GetFrameHeight() * 0.5f;
            s.y = (a.y + b.y) * 0.5f;
        }
    }
    ImGui::PopID();
}

// The tweaks' table for the part picked: each side's own values and the both-sides ones, and which one each side uses.
void fileValues()
{
    const char* name = subjectEntry();
    const int part = cvarIndex(vr_armourstudio_part, 0, NumParts - 1);
    const bool limb = limbPart(part);
    const Override* both = entry(name, part, SideBoth, false);
    const Override* left = limb ? entry(name, part, SideLeft, false) : nullptr;
    const Override* right = limb ? entry(name, part, SideRight, false) : nullptr;
    ImGui::TextDisabled("kind \"%s\", %s: the table (the file as read, with any edits not saved yet)", name, partNames[part]);
    const int cols = limb ? 6 : 2;
    if(!ImGui::BeginTable("filevalues", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
    {
        return;
    }
    ImGui::TableSetupColumn("value");
    ImGui::TableSetupColumn(limb ? "both" : "set");
    if(limb)
    {
        ImGui::TableSetupColumn("left's own");
        ImGui::TableSetupColumn("right's own");
        ImGui::TableSetupColumn("left uses");
        ImGui::TableSetupColumn("right uses");
    }
    ImGui::TableHeadersRow();
    const auto cell = [](const Override* o, const int f) {
        if(o && (o->has & (fieldBit(f))))
        {
            ImGui::Text("%.3f", o->v[f]);
        }
        else
        {
            ImGui::TextDisabled("-");
        }
    };
    // What a side uses: its own value wins, else the both-sides one (the right's mirrored), else the generator's.
    const auto uses = [&](const Override* own, const bool rightSide, const int f) {
        const uint64_t bit = fieldBit(f);
        if(own && (own->has & bit))
        {
            ImGui::TextColored(ImVec4{0.4f, 1.f, 0.6f, 1.f}, "own %.3f", own->v[f]);
        }
        else if(both && (both->has & bit))
        {
            const bool mirror = rightSide && mirrored(f);
            ImGui::Text("both %.3f", mirror ? -both->v[f] : both->v[f]);
        }
        else
        {
            ImGui::TextDisabled("generator");
        }
    };
    for(int f = 0; f < NumFields; f++)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(fieldNames[f]);
        ImGui::TableSetColumnIndex(1);
        cell(both, f);
        if(limb)
        {
            ImGui::TableSetColumnIndex(2);
            cell(left, f);
            ImGui::TableSetColumnIndex(3);
            cell(right, f);
            ImGui::TableSetColumnIndex(4);
            uses(left, false, f);
            ImGui::TableSetColumnIndex(5);
            uses(right, true, f);
        }
    }
    ImGui::EndTable();
    if(limb)
    {
        ImGui::TextDisabled("A side's own value wins over the both-sides one (mirrored on the right: left, yaw, roll).");
    }
    ImGui::TextDisabled("A model's own entries (kind \"progs/...\") go over the kind's: vr_armour_print lists them.");
}

// The subject's animations (vr_armourstudio_anim*): which, play / pause, speed, the frame (scrubbed while paused).
void animRows()
{
    const int n = animCount();
    ImGui::TextWrapped("%s", animStatus());
    if(n == 0)
    {
        ImGui::TextDisabled("Start a kind's subject to play its animations (the player's kind: your own body moves).");
        return;
    }
    bool on = vr_armourstudio_anim.value != 0.f;
    if(ImGui::Checkbox("Play an Animation", &on))
    {
        Cvar_SetValueQuick(&vr_armourstudio_anim, on ? 1.f : 0.f);
    }
    noteSpot("anim_on");
    const int group = cvarIndex(vr_armourstudio_anim_group, 0, n - 1);
    char preview[48];
    q_snprintf(preview, sizeof(preview), "%s (%d)", animName(group), animFrames(group));
    ImGui::SetNextItemWidth(220.f);
    if(ImGui::BeginCombo("Animation", preview))
    {
        for(int i = 0; i < n; i++)
        {
            char item[48];
            q_snprintf(item, sizeof(item), "%s (%d)##%d", animName(i), animFrames(i), i);
            if(ImGui::Selectable(item, i == group))
            {
                Cvar_SetValueQuick(&vr_armourstudio_anim_group, static_cast<float>(i));
                Cvar_SetValueQuick(&vr_armourstudio_anim, 1.f);
            }
        }
        ImGui::EndCombo();
    }
    const bool playing = vr_armourstudio_anim_play.value != 0.f;
    if(ImGui::Button(playing ? "Pause" : "Play"))
    {
        Cvar_SetValueQuick(&vr_armourstudio_anim_play, playing ? 0.f : 1.f);
        Cvar_SetValueQuick(&vr_armourstudio_anim, 1.f);
    }
    noteSpot("anim_play");
    ImGui::SameLine();
    float speed = vr_armourstudio_anim_speed.value;
    ImGui::SetNextItemWidth(160.f);
    if(ImGui::SliderFloat("Speed", &speed, -2.f, 3.f, "%.2fx"))
    {
        Cvar_SetValueQuick(&vr_armourstudio_anim_speed, speed);
    }
    int frame = za::clamp(static_cast<int>(vr_armourstudio_anim_frame.value), 0, za::max(animFrames(group) - 1, 0));
    ImGui::SetNextItemWidth(220.f);
    if(ImGui::SliderInt("Frame", &frame, 0, za::max(animFrames(group) - 1, 0)))
    {
        // (scrubbing pauses it)
        Cvar_SetValueQuick(&vr_armourstudio_anim_play, 0.f);
        Cvar_SetValueQuick(&vr_armourstudio_anim, 1.f);
        Cvar_SetValueQuick(&vr_armourstudio_anim_frame, static_cast<float>(frame));
    }
}

void drawWindow()
{
    win.spotCount = 0;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    if(!win.placed)
    {
        win.placed = true;
        win.w = za::min(win.w, ds.x);
        win.h = za::min(win.h, ds.y);
        win.x = za::max(0.f, ds.x - win.w - 16.f); // (the right of the screen: the subject stands in the middle)
        win.y = 16.f;
    }
    // A reopen puts it back where it was, and on a smaller screen back onto it (its title bar within reach).
    win.w = za::clamp(win.w, winMinW, za::max(winMinW, ds.x));
    win.h = za::clamp(win.h, winMinH, za::max(winMinH, ds.y));
    win.x = za::clamp(win.x, 0.f, za::max(0.f, ds.x - 96.f));
    win.y = za::clamp(win.y, 0.f, za::max(0.f, ds.y - 32.f));
    ImGui::SetNextWindowPos(ImVec2{win.x, win.y}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2{win.w, win.h}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2{winMinW, winMinH}, ImVec2{za::max(winMinW, ds.x), za::max(winMinH, ds.y)});
    bool keep = true;
    const bool shown = ImGui::Begin("Armour Studio", &keep, ImGuiWindowFlags_NoSavedSettings);
    if(!keep)
    {
        win.askClose = true; // (closed after ImGui::End, never from inside the window)
    }
    if(!win.askClose && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::GetIO().WantTextInput)
    {
        win.askClose = true;
    }
    if(shown)
    {
        ImGui::TextWrapped("%s", status());
        commandButton("Start: the Kind Ahead", "vr_armourstudio", "start",
            "the kind below put ahead of you with its fists, held still and unhurt, the part picked outlined");
        ImGui::SameLine();
        commandButton("Stop (Remove the Subject)", "vr_armourstudio_close", "stop",
            "the subject removed, the armed humanoids free to move again");
        ImGui::SameLine();
        ImGui::TextDisabled(running() ? "running" : "stopped");
        // (the edits at the top: on a small screen they stay in view while the rows below scroll)
        commandButton("Undo", "vr_armourstudio_undo", "undo", "the last change back (a slider's run of steps is one)");
        ImGui::SameLine();
        commandButton("Reset Part", "vr_armourstudio_reset_part", "reset_part", "this part's (and side's) values cleared");
        ImGui::SameLine();
        commandButton("Reset Kind", "vr_armourstudio_reset_kind", "reset_kind", "every part of this kind cleared");
        commandButton("Save", "vr_armourstudio_save", "save", "quakevr/data/armour.txt written (every kind's tweaks)");
        ImGui::SameLine();
        commandButton("Reload the File", "vr_armour_reload", "reload", "armour.txt read again (unsaved edits lost)");
        ImGui::SameLine();
        commandButton("Print the Tweaks", "vr_armour_print", "print", "the tweaks as the file would have them, to the console");

        ImGui::SeparatorText("Subject");
        {
            // (the subjects: the humanoids' kinds, AD's knights, the beasts)
            const char* labels[maxSubjects];
            const int n = za::min(subjectCount(), maxSubjects);
            for(int i = 0; i < n; i++)
            {
                labels[i] = subjectLabel(i);
            }
            comboRow("Kind", vr_armourstudio_kind, labels, n, 0);
        }
        comboRow("Tier", vr_armourstudio_tier, tierLabels, 3, 1);
        float turn = vr_armourstudio_turn.value;
        ImGui::SetNextItemWidth(220.f);
        if(ImGui::SliderFloat("Turn", &turn, -180.f, 180.f, "%.0f deg"))
        {
            Cvar_SetValueQuick(&vr_armourstudio_turn, turn);
        }
        if(ImGui::TreeNode("Place and Tilt"))
        {
            // (its place moved from where it was put, tipped and rolled: its sides, top and underside in view)
            ImGui::PushID("subject");
            for(int i = 0; i < 5; i++)
            {
                const Row& r = subjectRows[i];
                float v = r.cvar->value;
                ImGui::SetNextItemWidth(220.f);
                if(ImGui::SliderFloat(r.label, &v, r.lo, r.hi, r.format))
                {
                    Cvar_SetValueQuick(r.cvar, v);
                }
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s (Ctrl+click to type; not saved in armour.txt)", r.cvar->name);
                }
            }
            if(ImGui::Button("Put Back"))
            {
                for(const Row& r : subjectRows)
                {
                    Cvar_SetValueQuick(r.cvar, 0.f);
                }
                Cvar_SetValueQuick(&vr_armourstudio_turn, 0.f); // (the right-button drag's yaw)
            }
            ImGui::PopID();
            ImGui::TreePop();
        }
        float ghost = vr_armourstudio_ghost.value;
        ImGui::SetNextItemWidth(220.f);
        if(ImGui::SliderFloat("Body Ghost", &ghost, 0.f, 1.f, "%.2f"))
        {
            Cvar_SetValueQuick(&vr_armourstudio_ghost, ghost);
        }
        bool alone = vr_armourstudio_alone.value != 0.f;
        if(ImGui::Checkbox("Wear the Part Alone", &alone))
        {
            Cvar_SetValueQuick(&vr_armourstudio_alone, alone ? 1.f : 0.f);
        }

        ImGui::SeparatorText("Part");
        const int subject = cvarIndex(vr_armourstudio_kind, 0, subjectCount() - 1);
        {
            // (the parts on this subject alone: a beast's slots and the junctions made from them)
            const char* labels[NumParts];
            int parts[NumParts];
            int n = 0, chosen = -1;
            const bool beast = subject >= 7;
            // (a plate 8 split in two, the scrag's: its own part the front half)
            const bool split = beast && partValid(subject, PartPlate8Back);
            for(int p = 0; p < NumParts; p++)
            {
                if(!partValid(subject, p))
                {
                    continue;
                }
                chosen = p == cvarIndex(vr_armourstudio_part, 0, NumParts - 1) ? n : chosen;
                labels[n] = beast && p >= PartPauldronSleeve && p <= PartShinSleeve ? beastSleeveLabels[p - PartPauldronSleeve]
                    : split && p == PartPlate8                                     ? "Plate 8 Front"
                    : split && p == PartPlate8Sleeve                               ? "Plate 8 Front's Leather Junction"
                                                                                   : partLabels[p];
                parts[n++] = p;
            }
            ImGui::SetNextItemWidth(220.f);
            int pick = chosen;
            if(n > 0 && ImGui::Combo("Part", &pick, labels, n) && pick >= 0)
            {
                Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(parts[pick]));
            }
            if(ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", vr_armourstudio_part.name);
            }
        }
        if(limbPart(cvarIndex(vr_armourstudio_part, 0, NumParts - 1)))
        {
            comboRow("Side", vr_armourstudio_side, sideLabels, NumSides, 0);
        }
        else
        {
            ImGui::TextDisabled("Side: one piece (no sides)");
        }
        gizmoRows(); // (the handle on the part in the view: vr_armourstudio_gizmo.cpp)
        const int partNow = cvarIndex(vr_armourstudio_part, 0, NumParts - 1);
        for(int f = 0; f < NumFields; f++)
        {
            // (the values that apply to the part: armourstudio::fieldShown)
            if(!partValid(subject, partNow) || !fieldShown(subject, partNow, f))
            {
                continue;
            }
            valueRow(fieldRows[f], f);
        }

        // (each section its own ID scope: the Animation header and its Animation combo were one ID, his screenshot 2026-11-27)
        if(ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("anim");
            animRows();
            ImGui::PopID();
        }

        if(ImGui::CollapsingHeader("The file's values (this part)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("file");
            fileValues();
            ImGui::PopID();
        }
        ImGui::TextDisabled("Escape or the X closes this window, and the studio with it (vr_armourstudio_autoclose 0: it keeps running); the menu page is the same studio.");
    }
    const ImVec2 p = ImGui::GetWindowPos();
    win.x = p.x;
    win.y = p.y;
    if(!ImGui::IsWindowCollapsed())
    {
        const ImVec2 s = ImGui::GetWindowSize();
        win.w = s.x;
        win.h = s.y;
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
        Con_Printf("armourstudio: the ImGui window is flat-screen only (start the game with -flatscreenmode); the menu page "
                   "Debug > Tools > Armour Studio works in the headset\n");
        return;
    }
    if(rigstudio::active() || waveeditor::shown() || waveeditor::holds() || weaponstudio::active())
    {
        Con_Printf("armourstudio: close the other ImGui tool first (the Rig Studio, the wave editor or the Weapon Studio)\n");
        return;
    }
    if(!imgui::ensure())
    {
        Con_Printf("armourstudio: ImGui could not be started\n");
        return;
    }
    win.open = true;
    win.askClose = false;
    Key_ClearStates(); // (keys held as it opens let go: their releases go to the window)
    key_dest = key_game;
    m_state = m_none;
    Con_Printf("armourstudio: window open -- vr_armourstudio_window (or its key) toggles it, Escape or its X closes it\n");
}

void closeWindow()
{
    if(!win.open)
    {
        return;
    }
    win.open = false;
    win.askClose = false;
    if(win.keyPhase > 0)
    {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false); // (the test's Escape, never left held)
        win.keyPhase = 0;
    }
    win.clickPhase = 0;
    gizmoEdit(false); // (a handle's drag cut short ends here)
    imgui::want(false);
    IN_Activate();
    Con_Printf("armourstudio: window closed\n");
}

// Closed by the player (its X, Escape, vr_armourstudio_window close or toggle, vr_armourstudio_window_close): the studio
// with it (vr_armourstudio_autoclose; his note 2026-11-10: the armed humanoids stayed frozen after it). Not when another
// tool or a headset takes the screen, nor on shutdown.
void closeWindowByPlayer()
{
    const bool was = win.open;
    closeWindow();
    if(was)
    {
        windowClosed();
    }
}

// ---- The test (vr_armourstudio_window test) ----------------------------------------------------------------------

void testVerdict()
{
    Cvar_Set("vr_armour_file", win.fileWas);
    (void)load();
    win.step = 0;
    win.tally.verdict(testName);
}

void click(const char* name)
{
    q_strlcpy(win.clickName, name, sizeof(win.clickName));
    win.clickPhase = 1;
}

// The mock mouse and key, before the frame begins: a click moves there, presses, lets go (a press is a click once it
// lets go); Escape is down a frame and up the next.
void feedMock()
{
    if(win.keyPhase > 0)
    {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, win.keyPhase == 1);
        win.keyPhase = win.keyPhase == 1 ? 2 : 0;
    }
    if(win.clickPhase == 1)
    {
        const Spot* s = spotNamed(win.clickName);
        if(!s)
        {
            win.tally.check(false, testName, va("no widget named %s", win.clickName));
            win.clickPhase = 0;
            return;
        }
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        win.tally.check(s->x >= 0.f && s->x < ds.x && s->y >= 0.f && s->y < ds.y, testName,
            va("%s off the screen (%.0f,%.0f on %.0fx%.0f)", win.clickName, s->x, s->y, ds.x, ds.y));
        win.clickX = s->x;
        win.clickY = s->y;
        imgui::feedMouse(win.clickX, win.clickY, false, 0.f);
        win.clickPhase = 2;
    }
    else if(win.clickPhase == 2)
    {
        imgui::feedMouse(win.clickX, win.clickY, true, 0.f);
        win.clickPhase = 3;
    }
    else if(win.clickPhase == 3)
    {
        imgui::feedMouse(win.clickX, win.clickY, false, 0.f);
        win.clickPhase = 0;
    }
}

[[nodiscard]] const Override* bracerBoth()
{
    return entry(kindNames[KindGrunt], PartBracer, SideBoth, false);
}

[[nodiscard]] bool tableHas(const int f, const float v)
{
    const Override* o = bracerBoth();
    return o && (o->has & (fieldBit(f))) && za::abs(o->v[f] - v) < 1e-4f;
}

// One step a frame, each on its own condition; a step that waits too long fails as timed out.
void testFrame()
{
    if(win.step == 0 || win.clickPhase != 0 || win.keyPhase != 0)
    {
        return;
    }
    auto& t = win.tally;
    const auto advance = [](const int next) {
        win.step = next;
        win.stepFrames = 0;
    };
    const auto waitFor = [&](const bool ok, const char* what) {
        if(ok)
        {
            return true;
        }
        if(++win.stepFrames > testStepFrames)
        {
            t.check(false, testName, va("timed out: %s", what));
            closeWindow();
            testVerdict();
        }
        return false;
    };
    switch(win.step)
    {
    case 1: // the window drawn, its widgets named
        if(!waitFor(win.spotCount > 0 && spotNamed("plus_offset_x"), "the window drawn"))
        {
            return;
        }
        t.check(imgui::wanted(), testName, "the window open but ImGui not wanted");
        click("start");
        advance(2);
        return;
    case 2: // Start: the studio runs
        if(!waitFor(running(), "Start did not start the studio"))
        {
            return;
        }
        t.check(true);
        win.savedValue = vr_armourstudio_offset_x.value;
        click("plus_offset_x");
        advance(3);
        return;
    case 3: // the + step: the cvar moved, and the studio laid it into the table
        if(!waitFor(vr_armourstudio_offset_x.value > win.savedValue + 0.05f && tableHas(OffsetX, vr_armourstudio_offset_x.value),
               "the + step not in the cvar and the table"))
        {
            return;
        }
        t.check(true);
        win.savedValue = vr_armourstudio_offset_x.value;
        Con_Printf("%s: Forward + gave %.3f (vr_armourstudio_offset_x, the table's grunt bracer)\n", testName, win.savedValue);
        click("save");
        advance(4);
        return;
    case 4: // Save: the file holds it, the status says saved
    {
        za::String text;
        const bool written = files::readText(filePath(), text) && ZA_STRSTR(text.cStr(), "bracer") && ZA_STRSTR(text.cStr(), "offset");
        if(!waitFor(written && ZA_STRSTR(status(), ", saved") != nullptr, "Save did not write the file"))
        {
            return;
        }
        t.check(true);
        click("reset_part");
        advance(5);
        return;
    }
    case 5: // Reset Part: the value gone from the table and the cvar
        if(!waitFor(!tableHas(OffsetX, win.savedValue) && vr_armourstudio_offset_x.value == 0.f, "Reset Part did not clear it"))
        {
            return;
        }
        t.check(true);
        click("reload");
        advance(6);
        return;
    case 6: // Reload the File: the saved value back, in the cvar the menu's slider shows and in the window's row
        if(!waitFor(za::abs(vr_armourstudio_offset_x.value - win.savedValue) < 1e-4f && tableHas(OffsetX, win.savedValue),
               "Reload did not bring the saved value back"))
        {
            return;
        }
        t.check(true);
        advance(7);
        return;
    case 7: // (a frame drawn after the reload) the window shows it; then a change the menu's way: the cvar written
        t.check(za::abs(win.drawn[OffsetX] - win.savedValue) < 1e-4f, testName, "the window's Forward row not the reloaded value");
        Cvar_SetValueQuick(&vr_armourstudio_offset_z, 0.5f); // (what the menu's Up slider does)
        advance(8);
        return;
    case 8: // the menu's change: in the table, and on the window's row
        if(!waitFor(tableHas(OffsetZ, 0.5f) && za::abs(win.drawn[OffsetZ] - 0.5f) < 1e-4f, "the menu's change not in the window"))
        {
            return;
        }
        t.check(true);
        click("stop");
        advance(9);
        return;
    case 9: // Stop: the studio closed; then Escape closes the window
        if(!waitFor(!running(), "Stop did not close the studio"))
        {
            return;
        }
        t.check(true);
        win.keyPhase = 1;
        advance(10);
        return;
    case 10: // Escape: the window shut and the input given back
        if(!waitFor(!win.open, "Escape did not close the window"))
        {
            return;
        }
        t.check(!imgui::wanted(), testName, "the window closed but ImGui still wanted");
        testVerdict();
        return;
    default:
        return;
    }
}

void startTest()
{
    win.tally.reset();
    if(VR_IsActive())
    {
        // In the headset: the window must not open, and nothing of ImGui may start.
        openWindow();
        win.tally.check(!win.open && !imgui::wanted() && !imgui::ready(), testName, "the window opened in a headset session");
        win.tally.verdict(testName);
        return;
    }
    if(!sv.active)
    {
        Con_Printf("%s: a map first\n", testName);
        return;
    }
    q_strlcpy(win.fileWas, vr_armour_file.string, sizeof(win.fileWas));
    Cvar_Set("vr_armour_file", testFile);
    (void)files::writeText(filePath(), ""); // (the test's own file, empty: armour.txt is never touched)
    (void)load();
    Cvar_SetValueQuick(&vr_armourstudio_kind, static_cast<float>(KindGrunt));
    Cvar_SetValueQuick(&vr_armourstudio_part, static_cast<float>(PartBracer));
    Cvar_SetValueQuick(&vr_armourstudio_side, static_cast<float>(SideBoth));
    openWindow();
    if(!win.tally.check(win.open, testName, "the window did not open"))
    {
        testVerdict();
        return;
    }
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
        closeWindowByPlayer();
    }
    else if(!ZA_STRCMP(sub, "test"))
    {
        startTest();
    }
    else if(!ZA_STRCMP(sub, "toggle"))
    {
        if(win.open)
        {
            closeWindowByPlayer();
        }
        else
        {
            openWindow();
        }
    }
    else
    {
        Con_Printf("vr_armourstudio_window [open|close|toggle|test]: the Armour Studio's flat-screen ImGui window\n");
    }
}

} // namespace

void windowInit()
{
    Cmd_AddCommand("vr_armourstudio_window", window_f);
    Cmd_AddCommand("vr_armourstudio_window_close", closeWindowByPlayer);
}

void windowFrame()
{
    gizmoTestFrame(); // (its steps: before the window opens too)
    if(!win.open)
    {
        if(win.step != 0)
        {
            testFrame(); // (its last step: the window shut by its Escape)
        }
        return;
    }
    if(VR_IsActive() || rigstudio::active() || waveeditor::shown() || weaponstudio::active())
    {
        closeWindow(); // (a headset session started, or another tool opened: theirs from now on)
        return;
    }
    imgui::setPanel(0, 0, 0, 1.f); // (the screen only: no headset panel)
    imgui::want(true);
    feedMock();
    gizmoFeed();
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
        return; // (the test's verdict closed it)
    }
    drawWindow();
    gizmoFrame(); // (the handle on the part, over the game's view: after the window, so its hover is known)
    if(win.askClose)
    {
        closeWindowByPlayer(); // (after ImGui::End)
    }
}

void windowShutdown()
{
    closeWindow();
}

bool windowOpen()
{
    return win.open;
}

} // namespace qvr::armourstudio
