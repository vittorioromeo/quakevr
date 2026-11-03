// vr_waveeditor.cpp -- the Arena Wave Editor: see vr_waveeditor.hpp.

#include "vr_waveeditor.hpp"

#include "vr_agpose_shared.h"
#include "vr_arenawaves.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_imgui.hpp"
#include "vr_menuui.hpp"
#include "vr_progs.hpp"
#include "vr_renderdoc.hpp"
#include "vr_rigstudio.hpp"
#include "vr_testkit.hpp"
#include "vr_toolgun.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <SDL.h>

#include <stdio.h>
#include <string.h>

extern "C" int VR_IsActive(void);

using namespace qvr;

namespace
{

namespace aw = qvr::arenawaves;

// The panel in the headset: this many window pixels, this many texels each.
constexpr int panelPixelsX = 1120;
constexpr int panelPixelsY = 840;
constexpr float panelTexels = 2.f;
constexpr float panelFontScale = 1.25f;
constexpr float dropBelowEyes = 0.2f; // (of its distance: as the perk cards)
constexpr float wheelPerSecond = 6.f; // the stick's wheel, full over
constexpr float stickDeadzone = 0.25f;

constexpr const char* const armourWords[] = {"none", "green", "yellow", "red", "random"};
constexpr const char* const pieceWords[] = {"chest+helmet", "chest", "helmet", "chest+helmet+shoulders", "chest+helmet+shoulders+forearms",
    "chest+helmet+thighs+shins", "full", "random"};
constexpr const char* const afterWords[] = {"End: ARENA CLEARED", "Repeat the last wave", "Loop from the first"};
constexpr const char keyRows[] = "abcdefghijklmnopqrstuvwxyz0123456789_-";

constexpr int maxKinds = 24;
constexpr int maxInfect = 16;
constexpr int maxSpots = 400;
constexpr int armedRows = 4;  // (QC VR_AM_COUNT)
constexpr int codeTop = 128;  // the "weapon" keys asked: 1..127 (QC VR_AG_Decode's codes)

// The game's lists: asked of QC once a map. The weapons by each armed row, all of them whatever vr_agknight and
// vr_agdknight say (those are about what a map's knights become; the toolgun's list follows them, a preset names its kind).
struct Catalogue
{
    const void* progs{nullptr};
    char map[64]{};
    int kinds{0};
    char kindName[maxKinds][24]{};
    int kindRow[maxKinds]{}; // its armed row, -1 a plain monster
    int infect{0};
    char infectName[maxInfect][24]{};
    float ad{-1.f};
    int fits[armedRows][codeTop]{}; // QC VR_AM_UiWeapon's: 0 not; 1 + 2 one-handed + 4 * its class
    char codeName[codeTop][40]{};
    int codes[codeTop]{};           // those any row holds: by class (melee first), then code
    int codeCount{0};
};

// A widget laid out this frame: the tests click them by name.
struct Spot
{
    char name[56]{};
    ImVec2 centre{0.f, 0.f};
};

struct Editor
{
    bool open{false};
    aw::Preset preset;
    char name[49]{};     // the file it is from or was saved as ("": a new one)
    bool dirty{false};
    int wave{0};
    int entry{0};
    char map[64]{"vrarena"};
    int playWave{1};
    char status[256]{};
    char saveName[49]{};
    char pick[49]{};     // the Load list's choice
    bool askDiscard{false};
    bool askDelete{false};
    bool askSaveAs{false};
    bool hold{true};
    float columnsBottom{0.f}; // the window's middle row's bottom (its lists end there)

    Catalogue cat;
    za::Vector<za::String> files;   // the folder's presets
    bool filesStale{true};
    za::Vector<za::String> assignMaps;

    // The headset's panel.
    gfx::Target target;
    bool placed{false};
    bool panelUp{false};  // the panel's frame is what ImGui draws in (else the desktop window)
    glm::vec3 centre{0.f}, right{0.f}, up{0.f};
    bool hit[2]{false, false};
    glm::vec3 point[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec2 cursor[2]{glm::vec2{0.f}, glm::vec2{0.f}};
    bool trigger[2]{false, false};
    float wheel{0.f};
    double lastWheel{0.0};

    // The tests' mouse (a click: moved there, pressed, let go, each a frame).
    za::InPlaceVector<Spot, maxSpots> spots;
    char clickName[56]{};
    int clickPhase{0};
    ImVec2 clickAt{0.f, 0.f};
    bool testing{false};
    int testStep{0};
    int testWait{0};
    int testPassed{0};
    int testFailed{0};
    // The test past a click that shuts the editor (testClosedFrame): armed, the step to reopen on (0: the verdict), the
    // wave Play asked for (0: it was Close), the frames waited for that wave.
    bool testClosing{false};
    int testResume{0};
    int testPlayed{0};
    int testPlayFrames{0};
};
Editor ed;

// ---- The data --------------------------------------------------------------------------------------------------------

template <size_t N>
void setText(char (&dst)[N], const char* src)
{
    q_strlcpy(dst, src, N);
}

void say(const char* s)
{
    setText(ed.status, s);
    Con_Printf("waveeditor: %s\n", s);
}

void refreshCatalogue()
{
    Catalogue& c = ed.cat;
    if(!sv.active)
    {
        c.kinds = 0;
        c.infect = 0;
        c.progs = nullptr;
        return;
    }
    if(c.progs == sv.qcvm.progs && !strcmp(c.map, sv.name) && c.ad == vr_ad_mounted.value)
    {
        return;
    }
    c.progs = sv.qcvm.progs;
    c.ad = vr_ad_mounted.value;
    setText(c.map, sv.name);
    const progs::ServerVm vm;
    const func_t kindCount = progs::findFunction("VR_ARENA_KindCount");
    const func_t kindName = progs::findFunction("VR_ARENA_KindName");
    const func_t kindRow = progs::findFunction("VR_ARENA_ArmedRow");
    const func_t infectCount = progs::findFunction("VR_Infect_UiCount");
    const func_t infectName = progs::findFunction("VR_Infect_UiName");
    c.kinds = za::clamp(static_cast<int>(progs::qcFloat(kindCount)), 0, maxKinds);
    for(int i = 0; i < c.kinds; i++)
    {
        progs::qcText(c.kindName[i], sizeof(c.kindName[i]), kindName, static_cast<float>(i));
        c.kindRow[i] = static_cast<int>(progs::qcFloat(kindRow, static_cast<float>(i)));
    }
    const func_t weapon = progs::findFunction("VR_AM_UiWeapon");
    const func_t weaponName = progs::findFunction("VR_AM_UiWeaponName");
    c.codeCount = 0;
    for(int code = 1; code < codeTop; code++)
    {
        int cls = -1;
        for(int row = 0; row < armedRows; row++)
        {
            c.fits[row][code] = static_cast<int>(progs::qcFloat(weapon, static_cast<float>(row), static_cast<float>(code)));
            if(c.fits[row][code] && cls < 0)
            {
                cls = c.fits[row][code] / 4;
            }
        }
        if(cls < 0)
        {
            continue;
        }
        progs::qcText(c.codeName[code], sizeof(c.codeName[code]), weaponName, static_cast<float>(code));
        const auto classOf = [&](int other) {
            for(int row = 0; row < armedRows; row++)
            {
                if(c.fits[row][other])
                {
                    return c.fits[row][other] / 4;
                }
            }
            return 0;
        };
        int at = c.codeCount++;
        while(at > 0 && classOf(c.codes[at - 1]) > cls)
        {
            c.codes[at] = c.codes[at - 1];
            at--;
        }
        c.codes[at] = code;
    }
    c.infect = za::clamp(static_cast<int>(progs::qcFloat(infectCount)), 0, maxInfect);
    for(int i = 0; i < c.infect; i++)
    {
        progs::qcText(c.infectName[i], sizeof(c.infectName[i]), infectName, static_cast<float>(i));
    }
}

// Armed kind's row (-1 plain; -2 not a kind this game has).
[[nodiscard]] int rowOf(const char* kind)
{
    for(int i = 0; i < ed.cat.kinds; i++)
    {
        if(!strcmp(ed.cat.kindName[i], kind))
        {
            return ed.cat.kindRow[i];
        }
    }
    return -2;
}

[[nodiscard]] int fitsOf(int row, int code)
{
    return row >= 0 && row < armedRows && code > 0 && code < codeTop ? ed.cat.fits[row][code] : 0;
}

// The two held one in each hand: both pair, both melee or both guns (QC VR_AM_UiPairs).
[[nodiscard]] bool pairsOf(int row, int code, int code2)
{
    const int a = fitsOf(row, code), b = fitsOf(row, code2);
    return (a & 2) && (b & 2) && ((a >> 2) == VR_AG_CLASS_MELEE) == ((b >> 2) == VR_AG_CLASS_MELEE);
}

// The weapon's code by its name (0: none such for that row), as QC finds it (VR_ARENA_WeaponCode).
[[nodiscard]] int codeOf(int row, const char* name)
{
    for(int i = 0; i < ed.cat.codeCount; i++)
    {
        const int code = ed.cat.codes[i];
        if(fitsOf(row, code) && !strcmp(ed.cat.codeName[code], name))
        {
            return code;
        }
    }
    return 0;
}

// What's wrong with an entry as this game stands ("": nothing): the red line in the list; QC skips it at the map's start.
[[nodiscard]] const char* problemOf(const aw::Entry& e)
{
    if(const char* why = aw::badReason(e); why[0])
    {
        return why;
    }
    const int row = rowOf(e.kind);
    if(row == -2)
    {
        return "an unknown kind";
    }
    if(row >= 0)
    {
        int mainCode = 0;
        if(strcmp(e.mainHand, "random") != 0 && strcmp(e.mainHand, "none") != 0)
        {
            mainCode = codeOf(row, e.mainHand);
            if(!mainCode)
            {
                return "a weapon its kind can't hold";
            }
        }
        if(strcmp(e.off, "none") != 0)
        {
            const int off = codeOf(row, e.off);
            if(!off)
            {
                return "an off hand weapon its kind can't hold";
            }
            if(!mainCode || !pairsOf(row, mainCode, off))
            {
                return "off hand doesn't pair with the main hand";
            }
        }
    }
    bool known = false;
    for(int i = 0; i < ed.cat.infect; i++)
    {
        known = known || !strcmp(ed.cat.infectName[i], e.infected);
    }
    return known ? "" : "an unknown infection";
}

// One line on what an entry holds.
void describe(char* out, size_t size, const aw::Entry& e)
{
    const int row = rowOf(e.kind);
    char hands[96] = "";
    if(row >= 0)
    {
        if(strcmp(e.off, "none") != 0)
        {
            q_snprintf(hands, sizeof(hands), ": %s + %s", e.mainHand, e.off);
        }
        else
        {
            q_snprintf(hands, sizeof(hands), ": %s", !strcmp(e.mainHand, "none") ? "fists" : !strcmp(e.mainHand, "random") ? "from the pool" : e.mainHand);
        }
    }
    char armour[72] = "";
    if(row >= 0 && strcmp(e.armour, "none") != 0)
    {
        q_snprintf(armour, sizeof(armour), ", %s armour (%s)", e.armour, e.pieces);
    }
    char infected[48] = "";
    if(strcmp(e.infected, "none") != 0)
    {
        q_snprintf(infected, sizeof(infected), ", infected: %s", e.infected);
    }
    char delay[24] = "";
    if(e.delay > 0.f)
    {
        q_snprintf(delay, sizeof(delay), ", after %g s", e.delay);
    }
    q_snprintf(out, size, "%d x %s%s%s%s%s%s", e.count, e.kind, hands, armour, infected, delay, e.boss ? ", BOSS" : "");
}

[[nodiscard]] int foesIn(const aw::Wave& w)
{
    int n = 0;
    for(const aw::Entry& e : w.entries)
    {
        n += e.count;
    }
    return n;
}

[[nodiscard]] aw::Wave* currentWave()
{
    if(ed.wave < 0 || ed.wave >= static_cast<int>(ed.preset.waves.size()))
    {
        return nullptr;
    }
    return &ed.preset.waves[static_cast<za::SizeT>(ed.wave)];
}

[[nodiscard]] aw::Entry* currentEntry()
{
    aw::Wave* w = currentWave();
    if(!w || ed.entry < 0 || ed.entry >= static_cast<int>(w->entries.size()))
    {
        return nullptr;
    }
    return &w->entries[static_cast<za::SizeT>(ed.entry)];
}

void newPreset()
{
    ed.preset = aw::Preset{};
    aw::Wave w;
    w.entries.pushBack(aw::Entry{});
    ed.preset.waves.pushBack(static_cast<aw::Wave&&>(w));
    ed.name[0] = '\0';
    ed.dirty = false;
    ed.wave = 0;
    ed.entry = 0;
}

void loadPick()
{
    aw::Preset p;
    za::String why;
    if(aw::load(ed.pick, p, why))
    {
        ed.preset = static_cast<aw::Preset&&>(p);
        setText(ed.name, ed.pick);
        ed.dirty = false;
        ed.wave = 0;
        ed.entry = 0;
        say(why.empty() ? va("loaded %s", ed.name) : va("loaded %s: %s", ed.name, why.cStr()));
    }
    else
    {
        say(why.cStr());
    }
}

void saveTo(const char* name)
{
    za::String why;
    if(aw::save(name, ed.preset, why))
    {
        setText(ed.name, name);
        ed.dirty = false;
        ed.filesStale = true;
        say(va("saved %s", name));
    }
    else
    {
        say(why.cStr());
    }
}

// Swaps two list items by value.
template <typename T>
void swapItems(za::Vector<T>& v, int a, int b)
{
    T t = static_cast<T&&>(v[static_cast<za::SizeT>(a)]);
    v[static_cast<za::SizeT>(a)] = static_cast<T&&>(v[static_cast<za::SizeT>(b)]);
    v[static_cast<za::SizeT>(b)] = static_cast<T&&>(t);
}

// ---- The panel in the headset ------------------------------------------------------------------------------------------

[[nodiscard]] float panelWidth()
{
    return za::clamp(vr_waveeditor_width.value, 16.f, 200.f);
}

void place(const hands::State& s)
{
    const float yaw = glm::radians(s.headAngles.y);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
    const float d = za::max(vr_waveeditor_distance.value, 8.f);
    const float drop = d * dropBelowEyes;
    ed.centre = s.head + fwd * d - glm::vec3{0.f, 0.f, drop};
    ed.right = glm::vec3{fwd.y, -fwd.x, 0.f};
    const float tilt = za::atan2(drop, d);
    ed.up = glm::normalize(glm::vec3{0.f, 0.f, 1.f} * za::cos(tilt) + fwd * za::sin(tilt));
    ed.placed = true;
}

void quad(glm::vec3& corner, glm::vec3& xAxis, glm::vec3& yAxis)
{
    const float w = panelWidth();
    const float h = w * static_cast<float>(panelPixelsY) / static_cast<float>(panelPixelsX);
    corner = ed.centre - ed.right * (w * 0.5f) - ed.up * (h * 0.5f);
    xAxis = ed.right * w;
    yAxis = ed.up * h;
}

[[nodiscard]] bool laserHit(const hands::State& s, int hand, glm::vec3& point, glm::vec2& cursor)
{
    glm::vec3 origin, dir, r, u, corner, xAxis, yAxis;
    if(!toolgun::controllerFrame(s, hand, origin, dir, r, u))
    {
        return false;
    }
    quad(corner, xAxis, yAxis);
    const glm::vec3 normal = glm::cross(xAxis, yAxis);
    const float along = glm::dot(dir, normal);
    if(za::fabs(along) < 1e-6f)
    {
        return false;
    }
    const float t = glm::dot(corner - origin, normal) / along;
    if(t <= 0.f)
    {
        return false;
    }
    point = origin + dir * t;
    const float uu = glm::dot(point - corner, xAxis) / glm::dot(xAxis, xAxis);
    const float vv = glm::dot(point - corner, yAxis) / glm::dot(yAxis, yAxis);
    if(uu < 0.f || uu > 1.f || vv < 0.f || vv > 1.f)
    {
        return false;
    }
    cursor = glm::vec2{uu * static_cast<float>(panelPixelsX), (1.f - vv) * static_cast<float>(panelPixelsY)};
    return true;
}

// ---- The widgets (each one also a spot the tests can click) -------------------------------------------------------------

void noteSpot(const char* name)
{
    if(ed.spots.size() < ed.spots.capacity())
    {
        Spot s;
        setText(s.name, name);
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        s.centre = ImVec2{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        ed.spots.pushBack(s);
    }
}

// The label's visible part: what's before ##.
void cleanLabel(char* out, size_t size, const char* label)
{
    q_strlcpy(out, label, size);
    if(char* hash = strstr(out, "##"))
    {
        *hash = '\0';
    }
}

[[nodiscard]] bool btn(const char* label, bool enabled = true)
{
    if(!enabled)
    {
        ImGui::BeginDisabled();
    }
    const bool on = ImGui::Button(label);
    if(!enabled)
    {
        ImGui::EndDisabled();
    }
    char name[56];
    cleanLabel(name, sizeof(name), label);
    noteSpot(name);
    return on && enabled;
}

[[nodiscard]] bool pickBegin(const char* label, const char* preview, bool enabled = true)
{
    if(!enabled)
    {
        ImGui::BeginDisabled();
    }
    const bool open = ImGui::BeginCombo(label, preview, ImGuiComboFlags_HeightLargest); // (long lists: less to scroll with a stick)
    if(!enabled)
    {
        ImGui::EndDisabled();
    }
    char name[56];
    cleanLabel(name, sizeof(name), label);
    noteSpot(name);
    return open;
}

[[nodiscard]] bool pickItem(const char* combo, const char* item, bool selected)
{
    ImGui::PushID(item);
    const bool on = ImGui::Selectable(item, selected);
    ImGui::PopID();
    char name[56];
    q_snprintf(name, sizeof(name), "%s:%s", combo, item);
    noteSpot(name);
    if(selected)
    {
        ImGui::SetItemDefaultFocus();
    }
    return on;
}

// A combo over a word list; true when the word changed.
template <size_t N, size_t M>
bool pickWord(const char* label, char (&value)[N], const char* const (&words)[M], bool enabled = true)
{
    bool changed = false;
    if(pickBegin(label, value, enabled))
    {
        for(const char* w : words)
        {
            if(pickItem(label, w, !strcmp(value, w)))
            {
                setText(value, w);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

// ---- The window's parts --------------------------------------------------------------------------------------------

void presetBar()
{
    ImGui::SetNextItemWidth(220.f);
    if(pickBegin("##load", ed.pick[0] ? ed.pick : "(saved presets)"))
    {
        for(const za::String& f : ed.files)
        {
            if(pickItem("load", f.cStr(), !strcmp(ed.pick, f.cStr())))
            {
                setText(ed.pick, f.cStr());
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if(btn("Load", ed.pick[0] != 0))
    {
        if(ed.dirty)
        {
            ed.askDiscard = true;
        }
        else
        {
            loadPick();
        }
    }
    ImGui::SameLine();
    if(btn("New"))
    {
        if(ed.dirty)
        {
            ed.pick[0] = '\0';
            ed.askDiscard = true;
        }
        else
        {
            newPreset();
        }
    }
    ImGui::SameLine();
    if(btn("Save", ed.name[0] != 0))
    {
        saveTo(ed.name);
    }
    ImGui::SameLine();
    if(btn("Save As"))
    {
        setText(ed.saveName, ed.name);
        ed.askSaveAs = true;
    }
    ImGui::SameLine();
    if(btn("Delete", aw::removable(ed.name))) // (a player's copy, never a shipped preset: vr_arenawaves.hpp)
    {
        ed.askDelete = true;
    }
    ImGui::SameLine();
    ImGui::Text("Editing: %s%s", ed.name[0] ? ed.name : "(new)", ed.dirty ? " *" : "");
    ImGui::SetNextItemWidth(240.f);
    int after = static_cast<int>(ed.preset.after);
    if(pickBegin("After the last wave", afterWords[za::clamp(after, 0, 2)]))
    {
        for(int i = 0; i < 3; i++)
        {
            if(pickItem("After the last wave", afterWords[i], after == i))
            {
                ed.preset.after = static_cast<aw::After>(i);
                ed.dirty = true;
            }
        }
        ImGui::EndCombo();
    }
}

void wavesColumn()
{
    ImGui::SeparatorText("Waves");
    if(btn("Add Wave") && static_cast<int>(ed.preset.waves.size()) < aw::maxWaves)
    {
        aw::Wave w;
        w.entries.pushBack(aw::Entry{});
        ed.preset.waves.pushBack(static_cast<aw::Wave&&>(w));
        ed.wave = static_cast<int>(ed.preset.waves.size()) - 1;
        ed.entry = 0;
        ed.dirty = true;
    }
    ImGui::SameLine();
    if(btn("Duplicate Wave", currentWave() != nullptr) && static_cast<int>(ed.preset.waves.size()) < aw::maxWaves)
    {
        aw::Wave copy = *currentWave();
        const za::SizeT at = static_cast<za::SizeT>(ed.wave) + 1;
        ed.preset.waves.insert(ed.preset.waves.begin() + at, static_cast<aw::Wave&&>(copy));
        ed.wave++;
        ed.dirty = true;
    }
    if(btn("Remove Wave", currentWave() != nullptr))
    {
        ed.preset.waves.erase(ed.preset.waves.begin() + static_cast<za::SizeT>(ed.wave));
        ed.wave = za::max(0, za::min(ed.wave, static_cast<int>(ed.preset.waves.size()) - 1));
        ed.entry = 0;
        ed.dirty = true;
    }
    ImGui::SameLine();
    if(btn("Wave Up", ed.wave > 0))
    {
        swapItems(ed.preset.waves, ed.wave, ed.wave - 1);
        ed.wave--;
        ed.dirty = true;
    }
    ImGui::SameLine();
    if(btn("Wave Down", ed.wave + 1 < static_cast<int>(ed.preset.waves.size())))
    {
        swapItems(ed.preset.waves, ed.wave, ed.wave + 1);
        ed.wave++;
        ed.dirty = true;
    }
    if(ImGui::BeginChild("waves", ImVec2{0.f, za::max(ed.columnsBottom - ImGui::GetCursorScreenPos().y, 40.f)}, ImGuiChildFlags_Borders))
    {
        for(int i = 0; i < static_cast<int>(ed.preset.waves.size()); i++)
        {
            const aw::Wave& w = ed.preset.waves[static_cast<za::SizeT>(i)];
            char line[64];
            q_snprintf(line, sizeof(line), "Wave %d: %d foes##w%d", i + 1, foesIn(w), i);
            if(ImGui::Selectable(line, ed.wave == i))
            {
                ed.wave = i;
                ed.entry = 0;
            }
            char name[24];
            q_snprintf(name, sizeof(name), "wave%d", i + 1);
            noteSpot(name);
        }
    }
    ImGui::EndChild();
}

void entriesColumn()
{
    aw::Wave* w = currentWave();
    char head[64];
    q_snprintf(head, sizeof(head), "Wave %d: what it holds", ed.wave + 1);
    ImGui::SeparatorText(head);
    if(!w)
    {
        ImGui::TextDisabled("No waves: Add Wave.");
        return;
    }
    if(btn("Add Entry") && static_cast<int>(w->entries.size()) < aw::maxEntries)
    {
        w->entries.pushBack(aw::Entry{});
        ed.entry = static_cast<int>(w->entries.size()) - 1;
        ed.dirty = true;
    }
    ImGui::SameLine();
    if(btn("Duplicate Entry", currentEntry() != nullptr) && static_cast<int>(w->entries.size()) < aw::maxEntries)
    {
        const aw::Entry copy = *currentEntry();
        w->entries.insert(w->entries.begin() + static_cast<za::SizeT>(ed.entry) + 1, copy);
        ed.entry++;
        ed.dirty = true;
    }
    if(btn("Remove Entry", currentEntry() != nullptr))
    {
        w->entries.erase(w->entries.begin() + static_cast<za::SizeT>(ed.entry));
        ed.entry = za::max(0, za::min(ed.entry, static_cast<int>(w->entries.size()) - 1));
        ed.dirty = true;
    }
    ImGui::SameLine();
    if(btn("Entry Up", ed.entry > 0))
    {
        swapItems(w->entries, ed.entry, ed.entry - 1);
        ed.entry--;
        ed.dirty = true;
    }
    ImGui::SameLine();
    if(btn("Entry Down", ed.entry + 1 < static_cast<int>(w->entries.size())))
    {
        swapItems(w->entries, ed.entry, ed.entry + 1);
        ed.entry++;
        ed.dirty = true;
    }
    const int foes = foesIn(*w);
    ImGui::SetNextItemWidth(160.f);
    if(ImGui::SliderInt("Up at once (0: the arena's own)", &w->pool, 0, aw::maxCount))
    {
        ed.dirty = true;
    }
    noteSpot("Pool");
    if(foes > aw::maxCount)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4{1.f, 0.4f, 0.3f, 1.f}, "%d foes: only the first %d come", foes, aw::maxCount);
    }
    if(ImGui::BeginChild("entries", ImVec2{0.f, za::max(ed.columnsBottom - ImGui::GetCursorScreenPos().y, 40.f)}, ImGuiChildFlags_Borders))
    {
        for(int i = 0; i < static_cast<int>(w->entries.size()); i++)
        {
            const aw::Entry& e = w->entries[static_cast<za::SizeT>(i)];
            char line[256];
            describe(line, sizeof(line) - 8, e);
            const char* why = problemOf(e);
            ImGui::PushID(i);
            if(why[0])
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.4f, 0.3f, 1.f});
            }
            if(ImGui::Selectable(line, ed.entry == i))
            {
                ed.entry = i;
            }
            if(why[0])
            {
                ImGui::PopStyleColor();
                if(ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s: skipped when the map starts", why);
                }
            }
            ImGui::PopID();
            char name[24];
            q_snprintf(name, sizeof(name), "entry%d", i + 1);
            noteSpot(name);
        }
    }
    ImGui::EndChild();
}

void entryColumn()
{
    aw::Entry* e = currentEntry();
    ImGui::SeparatorText("Entry");
    if(!e)
    {
        ImGui::TextDisabled("No entry chosen.");
        return;
    }
    if(ed.cat.kinds == 0)
    {
        ImGui::TextDisabled("No game running: its lists come with a map.");
    }
    const float width = ImGui::GetContentRegionAvail().x * 0.62f;
    ImGui::SetNextItemWidth(width);
    if(pickBegin("Kind", e->kind))
    {
        for(int i = 0; i < ed.cat.kinds; i++)
        {
            if(pickItem("Kind", ed.cat.kindName[i], !strcmp(e->kind, ed.cat.kindName[i])))
            {
                setText(e->kind, ed.cat.kindName[i]);
                ed.dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    const int row = rowOf(e->kind);
    // What the new kind can't hold falls back to what it can.
    if(row < 0 && (strcmp(e->mainHand, "none") != 0 || strcmp(e->off, "none") != 0 || strcmp(e->armour, "none") != 0))
    {
        if(row == -1)
        {
            setText(e->mainHand, "none");
            setText(e->off, "none");
            setText(e->armour, "none");
        }
    }
    if(row >= 0 && strcmp(e->mainHand, "random") != 0 && strcmp(e->mainHand, "none") != 0 && !codeOf(row, e->mainHand) && ed.dirty)
    {
        setText(e->mainHand, "random");
    }
    const bool armed = row >= 0;
    const int mainCode = armed && strcmp(e->mainHand, "random") != 0 && strcmp(e->mainHand, "none") != 0 ? codeOf(row, e->mainHand) : 0;
    if(armed && strcmp(e->off, "none") != 0 && ed.dirty && (!mainCode || !pairsOf(row, mainCode, codeOf(row, e->off))))
    {
        setText(e->off, "none");
    }
    ImGui::SetNextItemWidth(width);
    if(pickBegin("Main Hand", e->mainHand, armed))
    {
        if(pickItem("Main Hand", "random", !strcmp(e->mainHand, "random")))
        {
            setText(e->mainHand, "random");
            ed.dirty = true;
        }
        if(pickItem("Main Hand", "none", !strcmp(e->mainHand, "none")))
        {
            setText(e->mainHand, "none");
            ed.dirty = true;
        }
        for(int i = 0; i < ed.cat.codeCount; i++)
        {
            const int code = ed.cat.codes[i];
            const char* name = ed.cat.codeName[code];
            if(fitsOf(row, code) && pickItem("Main Hand", name, !strcmp(e->mainHand, name)))
            {
                setText(e->mainHand, name);
                ed.dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(width);
    if(pickBegin("Off Hand", e->off, armed && (fitsOf(row, mainCode) & 2) != 0))
    {
        if(pickItem("Off Hand", "none", !strcmp(e->off, "none")))
        {
            setText(e->off, "none");
            ed.dirty = true;
        }
        for(int i = 0; i < ed.cat.codeCount; i++)
        {
            const int code = ed.cat.codes[i];
            const char* name = ed.cat.codeName[code];
            if(pairsOf(row, mainCode, code) && pickItem("Off Hand", name, !strcmp(e->off, name)))
            {
                setText(e->off, name);
                ed.dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(width);
    if(pickWord("Armour", e->armour, armourWords, armed))
    {
        ed.dirty = true;
    }
    ImGui::SetNextItemWidth(width);
    if(pickWord("Pieces", e->pieces, pieceWords, armed && strcmp(e->armour, "none") != 0))
    {
        ed.dirty = true;
    }
    ImGui::SetNextItemWidth(width);
    if(pickBegin("Infected With", e->infected))
    {
        for(int i = 0; i < ed.cat.infect; i++)
        {
            if(pickItem("Infected With", ed.cat.infectName[i], !strcmp(e->infected, ed.cat.infectName[i])))
            {
                setText(e->infected, ed.cat.infectName[i]);
                ed.dirty = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(width);
    if(ImGui::SliderInt("Count", &e->count, 0, aw::maxCount))
    {
        ed.dirty = true;
    }
    noteSpot("Count");
    ImGui::SetNextItemWidth(width);
    if(ImGui::SliderFloat("Delay (s)", &e->delay, 0.f, 120.f, "%.0f"))
    {
        e->delay = static_cast<float>(static_cast<int>(e->delay + 0.5f)); // (whole seconds: what the file then says)
        ed.dirty = true;
    }
    noteSpot("Delay");
    bool boss = e->boss != 0;
    if(ImGui::Checkbox("Boss", &boss))
    {
        e->boss = boss ? 1 : 0;
        ed.dirty = true;
    }
    noteSpot("Boss");
}

void assignBar()
{
    ImGui::SeparatorText("Which preset plays");
    ImGui::SetNextItemWidth(170.f);
    if(pickBegin("Map", ed.map))
    {
        if(sv.active && pickItem("Map", sv.name, !strcmp(ed.map, sv.name)))
        {
            setText(ed.map, sv.name);
        }
        if(strcmp(sv.active ? sv.name : "", "vrarena") != 0 && pickItem("Map", "vrarena", !strcmp(ed.map, "vrarena")))
        {
            setText(ed.map, "vrarena");
        }
        for(const za::String& m : ed.assignMaps)
        {
            if(strcmp(m.cStr(), "vrarena") != 0 && strcmp(m.cStr(), sv.active ? sv.name : "") != 0 && pickItem("Map", m.cStr(), !strcmp(ed.map, m.cStr())))
            {
                setText(ed.map, m.cStr());
            }
        }
        ImGui::EndCombo();
    }
    const float comboWidth = za::max((ImGui::GetContentRegionAvail().x - 170.f) / 4.f - 80.f, 90.f);
    for(int skill = 0; skill < aw::skillCount; skill++)
    {
        ImGui::SameLine();
        const za::String now = aw::assigned(ed.map, skill);
        ImGui::SetNextItemWidth(comboWidth);
        if(pickBegin(aw::skillNames[skill], now.empty() ? "Default" : now.cStr()))
        {
            if(pickItem(aw::skillNames[skill], "Default", now.empty()))
            {
                aw::setAssigned(ed.map, skill, "");
                ed.assignMaps = aw::listedMaps();
                say(va("%s %s: Default", ed.map, aw::skillNames[skill]));
            }
            for(const za::String& f : ed.files)
            {
                if(pickItem(aw::skillNames[skill], f.cStr(), !strcmp(now.cStr(), f.cStr())))
                {
                    aw::setAssigned(ed.map, skill, f.cStr());
                    ed.assignMaps = aw::listedMaps();
                    say(va("%s %s: %s", ed.map, aw::skillNames[skill], f.cStr()));
                }
            }
            ImGui::EndCombo();
        }
    }
}

// Closed by Play, the Close button, the window's X, a headset session's change.
void closeEditor();

void playBar()
{
    ImGui::SetNextItemWidth(170.f);
    const int waves = za::max(1, static_cast<int>(ed.preset.waves.size()));
    ed.playWave = za::clamp(ed.playWave, 1, waves);
    ImGui::SliderInt("Play from wave", &ed.playWave, 1, waves);
    noteSpot("Play from wave");
    ImGui::SameLine();
    if(btn("Play", !ed.preset.waves.empty()))
    {
        const char* name = ed.name[0] ? ed.name : "(unsaved)";
        aw::play(ed.map, name, ed.preset, ed.playWave);
        closeEditor();
        return;
    }
    if(ed.panelUp)
    {
        ImGui::SameLine();
        if(btn("Recentre"))
        {
            ed.placed = false;
        }
    }
    ImGui::SameLine();
    if(btn("Close"))
    {
        closeEditor();
        return;
    }
    ImGui::TextWrapped("%s", ed.status);
    if(const za::String& r = aw::lastReport(); !r.empty())
    {
        ImGui::TextDisabled("This map's arena said:");
        ImGui::TextWrapped("%s", r.cStr());
    }
}

// The popups: asked for from the bar (their own frame's ID stack), one at a time.
void popups()
{
    if(ed.askDiscard)
    {
        ImGui::OpenPopup("Discard your changes?");
        ed.askDiscard = false;
    }
    if(ed.askDelete)
    {
        ImGui::OpenPopup("Delete this preset?");
        ed.askDelete = false;
    }
    if(ed.askSaveAs)
    {
        ImGui::OpenPopup("Save As");
        ed.askSaveAs = false;
    }
    if(ImGui::BeginPopupModal("Discard your changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s has unsaved changes.", ed.name[0] ? ed.name : "This new preset");
        if(btn("Discard"))
        {
            if(ed.pick[0])
            {
                loadPick();
            }
            else
            {
                newPreset();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(btn("Keep Editing"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if(ImGui::BeginPopupModal("Delete this preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Delete %s? Its file goes; this can't be undone.", ed.name);
        if(btn("Yes, Delete"))
        {
            const bool gone = aw::removeFile(ed.name);
            say(va("%s %s", gone ? "deleted" : "couldn't delete", ed.name));
            if(!strcmp(ed.pick, ed.name))
            {
                ed.pick[0] = '\0';
            }
            ed.filesStale = true;
            newPreset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(btn("Keep It"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if(ImGui::BeginPopupModal("Save As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemWidth(320.f);
        ImGui::InputText("Name", ed.saveName, sizeof(ed.saveName), ImGuiInputTextFlags_CharsNoBlank);
        noteSpot("Name");
        if(ed.panelUp)
        {
            // The window's own keys: no keyboard in a headset.
            for(int i = 0; keyRows[i]; i++)
            {
                char key[8];
                q_snprintf(key, sizeof(key), "%c##k%d", keyRows[i], i);
                if(i % 13 != 0)
                {
                    ImGui::SameLine();
                }
                if(ImGui::Button(key, ImVec2{36.f, 36.f}))
                {
                    const size_t n = strlen(ed.saveName);
                    if(n + 1 < sizeof(ed.saveName))
                    {
                        ed.saveName[n] = keyRows[i];
                        ed.saveName[n + 1] = '\0';
                    }
                }
                char name[12];
                q_snprintf(name, sizeof(name), "key:%c", keyRows[i]);
                noteSpot(name);
            }
            if(btn("Backspace"))
            {
                const size_t n = strlen(ed.saveName);
                if(n > 0)
                {
                    ed.saveName[n - 1] = '\0';
                }
            }
            ImGui::SameLine();
            if(btn("Clear"))
            {
                ed.saveName[0] = '\0';
            }
        }
        const bool ok = aw::validName(ed.saveName);
        if(!ok)
        {
            ImGui::TextColored(ImVec4{1.f, 0.4f, 0.3f, 1.f}, "Letters, digits, _ and - only.");
        }
        if(btn("Save It", ok))
        {
            saveTo(ed.saveName);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if(btn("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void drawWindow()
{
    ed.spots.clear();
    refreshCatalogue();
    if(ed.filesStale)
    {
        ed.files = aw::list();
        ed.assignMaps = aw::listedMaps();
        ed.filesStale = false;
    }
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if(ed.panelUp)
    {
        ImGui::SetNextWindowPos(ImVec2{0.f, 0.f});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    }
    else
    {
        // (never its place from an imgui.ini written on a larger window: all of it on the screen, every time it opens)
        const ImVec2 screen = ImGui::GetIO().DisplaySize;
        flags |= ImGuiWindowFlags_NoSavedSettings;
        ImGui::SetNextWindowPos(ImVec2{0.f, 0.f}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2{za::min(1120.f, screen.x), za::min(760.f, screen.y)}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(ImVec2{320.f, 240.f}, screen);
    }
    bool keep = true;
    if(!ImGui::Begin("Arena Wave Editor", &keep, flags))
    {
        ImGui::End();
        return;
    }
    presetBar();
    const float bottom = 150.f;
    const float height = za::max(ImGui::GetContentRegionAvail().y - bottom, 120.f);
    ed.columnsBottom = ImGui::GetCursorScreenPos().y + height - 4.f;
    if(ImGui::BeginTable("columns", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable, ImVec2{0.f, height}))
    {
        ImGui::TableSetupColumn("waves", ImGuiTableColumnFlags_WidthStretch, 0.24f);
        ImGui::TableSetupColumn("entries", ImGuiTableColumnFlags_WidthStretch, 0.46f);
        ImGui::TableSetupColumn("entry", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        wavesColumn();
        ImGui::TableSetColumnIndex(1);
        entriesColumn();
        ImGui::TableSetColumnIndex(2);
        entryColumn();
        ImGui::EndTable();
    }
    assignBar();
    popups();
    if(ed.open)
    {
        playBar();
    }
    ImGui::End();
    if(!keep && ed.open)
    {
        closeEditor();
    }
}

void closeEditor()
{
    ed.open = false;
    ed.testing = false;
    imgui::want(false);
    imgui::setPanel(0, 0, 0, 1.f);
    gfx::releaseTarget(ed.target);
    ed.panelUp = false;
    ed.placed = false;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    IN_Activate();
    Con_Printf("waveeditor: closed%s\n", ed.dirty ? " (unsaved changes kept in this session: vr_waveeditor to go on)" : "");
}

// ---- The tests' mouse, and the test ---------------------------------------------------------------------------------

void click(const char* name)
{
    setText(ed.clickName, name);
    ed.clickPhase = 1;
}

[[nodiscard]] const Spot* spotNamed(const char* name)
{
    for(const Spot& s : ed.spots)
    {
        if(!strcmp(s.name, name))
        {
            return &s;
        }
    }
    return nullptr;
}

// What the tests' mouse does this frame: fed to ImGui before its frame.
void feedMock()
{
    if(ed.clickPhase <= 0)
    {
        return;
    }
    if(ed.clickPhase == 1)
    {
        const Spot* s = spotNamed(ed.clickName);
        if(!s)
        {
            ed.testFailed++;
            Con_Printf("waveeditor test: FAIL no widget named \"%s\" (step %d; %d files, %d widgets)\n", ed.clickName, ed.testStep,
                static_cast<int>(ed.files.size()), static_cast<int>(ed.spots.size()));
            ed.clickPhase = 0;
            return;
        }
        ed.clickAt = s->centre;
        imgui::feedMouse(ed.clickAt.x, ed.clickAt.y, false, 0.f);
    }
    else if(ed.clickPhase == 2)
    {
        imgui::feedMouse(ed.clickAt.x, ed.clickAt.y, true, 0.f);
    }
    else if(ed.clickPhase == 3)
    {
        imgui::feedMouse(ed.clickAt.x, ed.clickAt.y, false, 0.f);
    }
    ed.clickPhase = ed.clickPhase >= 4 ? 0 : ed.clickPhase + 1;
}

// How long the test waits for Play's arena to begin its first wave (frames; a stuck run's net, not its end).
static constexpr int testPlayGiveUp = 72 * 30;

void check(bool ok, const char* what)
{
    (ok ? ed.testPassed : ed.testFailed)++;
    Con_Printf("waveeditor test: %s: %s\n", ok ? "PASS" : "FAIL", what);
}

void open_f();

void testVerdict()
{
    ed.testing = false;
    ed.testClosing = false;
    aw::playOff(); // (the next map: what's assigned, not the test's preset)
    qvr::testkit::verdict("waveeditor test", ed.testPassed, ed.testFailed);
}

// A click that should shut the editor; then testClosedFrame goes on (0: to the verdict, else reopening on that step).
void clickToClose(const char* name, int played, int resume)
{
    ed.testClosing = true;
    ed.testPlayed = played;
    ed.testPlayFrames = 0;
    ed.testResume = resume;
    click(name);
}

// The whole editor through its widgets, as a hand would. One step each time the last click is over.
void testFrame()
{
    if(!ed.testing || ed.clickPhase > 0)
    {
        return;
    }
    if(ed.testWait > 0) // (a popup or a list comes up a frame after what opens it)
    {
        ed.testWait--;
        return;
    }
    constexpr const char* const testName = "fixture_ui";
    const int step = ed.testStep++;
    ed.testWait = 1;
    const aw::Wave* w0 = ed.preset.waves.empty() ? nullptr : &ed.preset.waves[0];
    switch(step)
    {
    case 0: newPreset(); click("Add Wave"); break;
    case 1: check(ed.preset.waves.size() == 2 && ed.wave == 1, "Add Wave: two waves, the new one chosen"); click("Duplicate Wave"); break;
    case 2: check(ed.preset.waves.size() == 3 && ed.wave == 2, "Duplicate Wave: three"); click("Remove Wave"); break;
    case 3: check(ed.preset.waves.size() == 2 && ed.wave == 1, "Remove Wave: two"); click("Add Entry"); break;
    case 4: check(ed.preset.waves[1].entries.size() == 2 && ed.entry == 1, "Add Entry: the wave holds two"); click("Kind"); break;
    case 5: click("Kind:ogre"); break;
    case 6: check(!strcmp(ed.preset.waves[1].entries[1].kind, "ogre") && !strcmp(ed.preset.waves[1].entries[1].mainHand, "none"), "Kind: an ogre, no weapon"); click("Infected With"); break;
    case 7: click("Infected With:shambler"); break;
    case 8: check(!strcmp(ed.preset.waves[1].entries[1].infected, "shambler"), "Infected With: a shambler"); click("entry1"); break;
    case 9: click("Main Hand"); break;
    case 10: click("Main Hand:Axe"); break;
    case 11: check(!strcmp(ed.preset.waves[1].entries[0].mainHand, "Axe"), va("Main Hand: the axe (%s, entry %d)", ed.preset.waves[1].entries[0].mainHand, ed.entry)); click("Armour"); break;
    case 12: click("Armour:red"); break;
    case 13: check(!strcmp(ed.preset.waves[1].entries[0].armour, "red"), "Armour: red"); click("Wave Up"); break;
    case 14: check(ed.wave == 0 && ed.preset.waves[0].entries.size() == 2, "Wave Up: it's first now"); click("Entry Down"); break;
    case 15: check(!strcmp(ed.preset.waves[0].entries[1].kind, "grunt") && ed.entry == 1, "Entry Down"); click("Save As"); break;
    case 16: setText(ed.saveName, testName); click("Name"); break; // (the field typed in: below, with the panel's keys)
    case 17: if(ed.panelUp) { click("key:x"); } break;
    case 18: if(ed.panelUp) { check(!strcmp(ed.saveName, "fixture_uix"), "the panel's keys type"); click("Backspace"); } break;
    case 19: check(!strcmp(ed.saveName, testName), "the name to save under"); click("Save It"); break;
    case 20: check(!strcmp(ed.name, testName) && !ed.dirty, "Save As: saved, nothing unsaved"); click("New"); break;
    case 21: check(ed.preset.waves.size() == 1 && !ed.name[0], "New: one wave, no name"); setText(ed.pick, testName); click("Load"); break;
    case 22: check(!strcmp(ed.name, testName) && ed.preset.waves.size() == 2 && !strcmp(w0->entries[1].kind, "grunt"), "Load: it's back"); click("Normal"); break;
    case 23: click(va("Normal:%s", testName)); break;
    case 24: check(!strcmp(aw::assigned(ed.map, 1).cStr(), testName), "Normal's preset for this map"); click("Normal"); break;
    case 25: click("Normal:Default"); break;
    case 26: check(aw::assigned(ed.map, 1).empty(), "Normal back to Default"); click("Delete"); break;
    case 27: click("Keep It"); break;
    case 28: check(!strcmp(ed.name, testName), "Delete asks: Keep It keeps it"); click("Delete"); break;
    case 29: click("Yes, Delete"); break;
    case 30:
    {
        ed.filesStale = true;
        ed.files = aw::list();
        bool left = false;
        for(const za::String& f : ed.files)
        {
            left = left || !strcmp(f.cStr(), testName);
        }
        check(!left, "Yes, Delete: its file is gone");
        clickToClose("Close", 0, 32);
        break;
    }
    // (31, 35, 38: the click before should have shut the editor; a step only an editor still open comes to)
    case 31: check(false, "Close: the editor stayed open"); testVerdict(); break;
    case 32: click("Add Wave"); break;
    case 33: click("Add Wave"); break;
    case 34: check(ed.preset.waves.size() == 3, "three waves to play"); ed.playWave = 1; clickToClose("Play", 1, 36); break;
    case 35: check(false, "Play from wave 1: the editor stayed open"); testVerdict(); break;
    case 36: click("Play from wave"); break;
    case 37:
        check(ed.playWave > 1, va("the slider, clicked: wave %d", ed.playWave));
        clickToClose("Play", ed.playWave, 0);
        break;
    case 38: check(false, "Play from a later wave: the editor stayed open"); testVerdict(); break;
    default: break;
    }
}

// The test while the editor's shut by its click: it really shut, and Play's arena began at the wave asked for.
void testClosedFrame()
{
    if(!ed.testClosing)
    {
        return;
    }
    if(ed.testPlayed == 0)
    {
        check(!imgui::wanted() && !imgui::frameOpen() && !ed.panelUp && !ed.target.framebuffer, "Close: the editor's shut, its frame and panel gone");
    }
    else
    {
        // (Play's map command ran before this frame: these are the new map's globals)
        const progs::Bindings& b = progs::bindings();
        const float began = sv.active && b.arenaWaveAt ? *b.arenaWaveAt : 0.f;
        if(began <= 0.f && ++ed.testPlayFrames < testPlayGiveUp)
        {
            return;
        }
        const int wave = began > 0.f ? static_cast<int>(*b.arenaWave) : 0;
        const int waves = b.arenaPresetWaves ? static_cast<int>(*b.arenaPresetWaves) : 0;
        check(!imgui::wanted() && !ed.panelUp, "Play: the editor's shut");
        check(wave == ed.testPlayed, va("Play from wave %d: the arena began at wave %d (%s, %d frames)", ed.testPlayed, wave, sv.name, ed.testPlayFrames));
        check(waves == static_cast<int>(ed.preset.waves.size()) && aw::playing(), va("its preset is the one playing (%d waves)", waves));
    }
    ed.testClosing = false;
    if(ed.testResume == 0)
    {
        testVerdict();
        return;
    }
    open_f();
    ed.testing = true;
    ed.testStep = ed.testResume;
    ed.testWait = 2;
}

// ---- Commands -----------------------------------------------------------------------------------------------------

void open_f()
{
    if(rigstudio::active())
    {
        Con_Printf("waveeditor: close the Rig Studio first (vr_rigstudio_close)\n");
        return;
    }
    if(ed.open)
    {
        return;
    }
    if(ed.preset.waves.empty())
    {
        newPreset();
    }
    if(!sv.active)
    {
        Cbuf_InsertText("map vrarena\n"); // (the game's lists come with a map)
    }
    else
    {
        setText(ed.map, sv.name);
    }
    ed.open = true;
    ed.filesStale = true;
    ed.placed = false;
    ed.hold = true;
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
    Con_Printf("waveeditor: open (%s)\n", VR_IsActive() ? "on a panel in the headset" : "on the screen");
}

void close_f()
{
    if(ed.open)
    {
        closeEditor();
    }
}

// vr_waveeditor_test: every button, list and popup clicked by the tests' mouse. Open it first. Ends on "waveeditor test:
// N passed, M failed".
void test_f()
{
    if(!ed.open)
    {
        open_f();
    }
    ed.testing = true;
    ed.testStep = 0;
    ed.testWait = 2;
    ed.testPassed = 0;
    ed.testFailed = 0;
}

// vr_waveeditor_load <name>: the preset into the editor (a picture's first step; Load from the console).
void load_f()
{
    if(Cmd_Argc() > 1)
    {
        setText(ed.pick, Cmd_Argv(1));
        loadPick();
    }
}

} // namespace

namespace qvr::waveeditor
{

void init()
{
    Cmd_AddCommand("vr_waveeditor", open_f);
    Cmd_AddCommand("vr_waveeditor_close", close_f);
    Cmd_AddCommand("vr_waveeditor_test", test_f);
    Cmd_AddCommand("vr_waveeditor_load", load_f);
    arenawaves::registerCommands();
}

void shutdown()
{
    if(ed.open)
    {
        closeEditor();
    }
}

bool shown()
{
    return ed.open;
}

bool holds()
{
    return ed.open && ed.hold;
}

void clientFrame()
{
    if(!ed.open)
    {
        testClosedFrame();
        return;
    }
    ed.panelUp = VR_IsActive() != 0;
    if(ed.panelUp)
    {
        gfx::ensureTarget(ed.target, static_cast<int>(panelPixelsX * panelTexels), static_cast<int>(panelPixelsY * panelTexels), false, "wave editor");
        imgui::setPanel(ed.target.framebuffer, panelPixelsX, panelPixelsY, panelTexels);
    }
    else
    {
        imgui::setPanel(0, 0, 0, 1.f);
    }
    imgui::want(true);
    if(ed.clickPhase > 0)
    {
        feedMock();
    }
    else if(ed.panelUp)
    {
        // The laser's hand: the one its trigger holds, else the main hand on the panel, else the off hand.
        int hand = ed.trigger[HAND_MAIN] ? HAND_MAIN : ed.trigger[HAND_OFF] ? HAND_OFF : ed.hit[HAND_MAIN] ? HAND_MAIN : HAND_OFF;
        if(ed.hit[hand])
        {
            imgui::feedMouse(ed.cursor[hand].x, ed.cursor[hand].y, ed.trigger[hand], ed.wheel);
        }
        else
        {
            imgui::feedMouse(-FLT_MAX, -FLT_MAX, ed.trigger[hand], 0.f);
        }
        ed.wheel = 0.f;
    }
    if(!imgui::beginFrame())
    {
        return;
    }
    if(!ed.panelUp)
    {
        // The mouse free and shown (the game's own look held), as the Rig Studio's.
        if(SDL_GetRelativeMouseMode())
        {
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
        if(SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
        {
            SDL_ShowCursor(SDL_ENABLE);
        }
    }
    if(ed.panelUp && panelFontScale != ImGui::GetStyle().FontScaleMain)
    {
        ImGui::GetStyle().FontScaleMain = panelFontScale;
    }
    testFrame();
    drawWindow();
}

void frame(const hands::State& s)
{
    if(!ed.open || !VR_IsActive() || !s.valid)
    {
        return;
    }
    if(!ed.placed)
    {
        place(s);
    }
    for(int h = 0; h < 2; h++)
    {
        ed.hit[h] = laserHit(s, h, ed.point[h], ed.cursor[h]);
    }
}

bool button(int hand, posing::Button b, bool down)
{
    if(!ed.open || b != posing::Button::Trigger || hand < 0 || hand > 1)
    {
        return false;
    }
    ed.trigger[hand] = down;
    return true;
}

bool sticks(const glm::vec2& off, const glm::vec2& main)
{
    if(!ed.open)
    {
        return false;
    }
    const float y = za::fabs(main.y) > za::fabs(off.y) ? main.y : off.y;
    const double now = realtime;
    const float dt = static_cast<float>(za::min(now - ed.lastWheel, 0.1));
    ed.lastWheel = now;
    if(za::fabs(y) > stickDeadzone)
    {
        ed.wheel += y * wheelPerSecond * dt;
    }
    return true;
}

void drawInEye(const hands::State& s)
{
    if(!ed.open || !ed.placed || !ed.panelUp || renderdoc::hidingPanel() || !ed.target.texture || !s.valid)
    {
        return;
    }
    glm::vec3 corner, xAxis, yAxis;
    quad(corner, xAxis, yAxis);
    const glm::vec4 white{1.f};
    const gfx::Vertex c[4] = {{corner, {0.f, 0.f}, white}, {corner + xAxis, {1.f, 0.f}, white},
        {corner + xAxis + yAxis, {1.f, 1.f}, white}, {corner + yAxis, {0.f, 1.f}, white}};
    const gfx::Vertex tris[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};
    gfx::draw(tris, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Opaque, .depthTest = false, .depthWrite = false}, ed.target.texture);
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 origin, fwd, r, u;
        if(toolgun::controllerFrame(s, h, origin, fwd, r, u))
        {
            menuui::drawLaser(origin, ed.hit[h] ? ed.point[h] : origin + fwd * 8.f, ed.hit[h], ed.trigger[h], 0.5f);
        }
    }
}

} // namespace qvr::waveeditor
