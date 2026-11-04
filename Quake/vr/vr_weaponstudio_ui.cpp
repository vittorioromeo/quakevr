// vr_weaponstudio_ui.cpp -- the Weapon Studio's window: every weapon setting on one screen, in pages that match what
// they are for, with the aiming readout and the hold previews beside them. See vr_weaponstudio.hpp.
//
// It is a new consumer of the store the Weapon Offsets / Weapon Weights menus already use: a row writes through
// detail::writeValue / writeText, which is the slot's stand-in cvar (vr_wofs_<key>_<slot+1>) written with
// Cvar_SetValueQuick / Cvar_SetQuick -- so the menus, the config file and the game see the change at once, exactly as
// a menu edit. It changes nothing else about weapons.
//
// It never closes itself from inside its own window: the Close button only asks, and vr_weaponstudio.cpp closes after
// ImGui::End (docs/vr-port/ROUND23.md: the wave editor's Play and Close crashed when closed mid-window).

#include "vr_weaponstudio_internal.hpp"

#include "vr_cvars.hpp"
#include "vr_toolgun.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <stdio.h>
#include <string.h>

using namespace qvr;

namespace qvr::weaponstudio
{
namespace
{

constexpr float pageColW = 186.f; // one page column (the pages' list sits in pageCols of them)
constexpr float labelW = 216.f;   // where a key's editor starts
constexpr float comboW = 200.f;   // a Combo's width: left to themselves they take the whole row and wrap the line
constexpr float keyRowsMin = 150.f; // the key rows stay at least this tall, whatever the screen
constexpr int pageCols = 2;       // 16 pages in one column are taller than a 540-row screen's window

// A page's short name, for its test spot (page_muzzle, page_hotspots, ...).
const char* const groupTags[] = {"place", "fingers", "muzzle", "2hand", "hotspots", "stock", "ammo", "holster",
                                 "effects", "torch", "weights", "blade", "reload", "embed", "model", "other"};

// What each page holds, in the window's own words (the reason these keys are together).
const char* const groupHints[] = {
    "Where the weapon sits in your hand: its place and turn, its size and length, how far the hand sinks into it, and "
    "the whole-weapon and hand-only posing.",
    "The fingers on the grip: how the hand opens, curls and places itself, and how far each is biased.",
    "Where the gun's muzzle is, where it shoots, and what the sights look like: this is the page that fixes aiming.",
    "The two-handed grips: the helping hand's place, its turn, and whether a two-handed aim is used.",
    "The four grip spots the other hand can hold (a foregrip, a pump, a magazine, a blade's cup) and the posing they "
    "carry. Pick one below to see only its keys.",
    "The virtual stock: where a stock that is not modelled is planted, and how the weapon is held against it.",
    "The ammo read-out on the weapon: its button and its text.",
    "How it hangs in a hip, upper-arm or shoulder holster. Pick the holster below to see only its keys.",
    "Recoil, muzzle flash, tracers and the effects the muzzle plays.",
    "The flashlight and the clip it sits in.",
    "What the weapon feels like: its mass and balance, its springs, its damage, how it is thrown and dropped. (The "
    "Weapon Weights page's own keys, in the same units the menu's sliders use.)",
    "A blade's shape: how far it reaches and where it bites.",
    "The loading port, the magazine and the well a weapon is loaded through. (Its own page: these are reload keys, "
    "kept here for reference.)",
    "A melee weapon embedded in what it hit.",
    "The model's own name, and the slot this one copies its settings from.",
    "Keys the pages above did not claim: shown here so nothing in the store is hidden (vr_weaponstudio_test counts "
    "them)."};

// What a key's numbers mean (the units its page's sliders work in).
[[nodiscard]] const char* kindHint(const detail::Kind kind)
{
    return detail::kindLabel(kind);
}

// ---- The window's buffers (file scope: the style keeps nothing function-local and static) ------------------------

char slotList[weapons::numSlots * 48];
char kindList[64 * 48];
char weaponList[64 * 48];
char anchorText[128];
char textBuf[96];
char spotBuf[64];
weapons::Key anchorKeyShown{weapons::Key::Count};
int anchorSlotShown{-1};

[[nodiscard]] const char* shortModel(const char* path)
{
    const char* s = path;
    for(const char* c = path; c && *c; c++)
        if(*c == '/')
            s = c + 1;
    return s;
}

// A Combo's word list: the words, each ended by \0, then one more \0. `pos` is where the next word goes.
void addWord(char* list, size_t size, int& pos, const char* word)
{
    if(pos + 2 >= static_cast<int>(size))
        return;
    pos += q_snprintf(list + pos, size - static_cast<size_t>(pos), "%s%c", word ? word : "?", 0);
}

void closeList(char* list, size_t size, int pos)
{
    if(pos + 1 < static_cast<int>(size))
        list[pos] = 0;
}

// The slots in use, named by their model, marked with what the player is holding and what they copy.
int buildSlotList()
{
    int pos = 0;
    slotList[0] = 0;
    for(int s = 0; s < weapons::slotsUsed(); s++)
    {
        const cvar_t* id = weapons::cvar(s, weapons::Key::ID);
        char row[48];
        q_snprintf(row, sizeof(row), "%s", id && id->string ? shortModel(id->string) : "?");
        if(s == weapons::heldSlot(1))
            q_strlcat(row, "  (main hand)", sizeof(row));
        else if(s == weapons::heldSlot(0))
            q_strlcat(row, "  (off hand)", sizeof(row));
        if(weapons::inheritsFrom(s) >= 0)
            q_strlcat(row, "  (copied)", sizeof(row));
        addWord(slotList, sizeof(slotList), pos, row);
    }
    closeList(slotList, sizeof(slotList), pos);
    return weapons::slotsUsed();
}

int buildKindList()
{
    int pos = 0;
    kindList[0] = 0;
    const int n = za::min(toolgun::armed::kindCount(), 64);
    for(int k = 0; k < n; k++)
        addWord(kindList, sizeof(kindList), pos, toolgun::armed::kindName(k));
    closeList(kindList, sizeof(kindList), pos);
    return n;
}

// The weapons a kind may hold. Off hand: "none" first, so an empty hand can be chosen.
int buildWeaponList(const int kind, const bool offHand)
{
    int pos = 0;
    weaponList[0] = 0;
    if(offHand)
        addWord(weaponList, sizeof(weaponList), pos, "none");
    const int n = za::min(toolgun::armed::weaponCount(), 63);
    for(int i = 0; i < n; i++)
        addWord(weaponList, sizeof(weaponList), pos, toolgun::armed::codeName(toolgun::armed::weaponCode(i)));
    closeList(weaponList, sizeof(weaponList), pos);
    return n + (offHand ? 1 : 0);
}

// A Combo over a word list (built above); true when the choice changed. Its spot is what the tests click.
bool wordCombo(const char* label, int& cur, const char* list, const int count, const char* spot)
{
    if(count <= 0)
    {
        ImGui::TextDisabled("%s: none available yet", label);
        return false;
    }
    int chosen = za::clamp(cur, 0, count - 1);
    ImGui::SetNextItemWidth(comboW);
    const bool changed = ImGui::Combo(label, &chosen, list, count);
    if(changed)
        cur = chosen;
    if(spot)
        noteSpot(spot);
    return changed;
}

// A button that is also a spot the tests can click.
[[nodiscard]] bool btn(const char* label, const char* spot)
{
    const bool on = ImGui::Button(label);
    noteSpot(spot);
    return on;
}

// ---- One key's row ---------------------------------------------------------------------------------------------

void keyRow(const detail::Entry& e, const int slot)
{
    ImGui::PushID(e.name);
    const bool changedHere = detail::touched(slot, e.key);
    ImGui::AlignTextToFramePadding();
    q_snprintf(textBuf, sizeof(textBuf), "%s%s", changedHere ? "* " : "  ", e.label);
    ImGui::TextUnformatted(textBuf);
    ImGui::SameLine(labelW);

    const cvar_t* c = weapons::cvar(slot, e.key);
    if(!c)
    {
        ImGui::TextDisabled("this slot has no such key");
        ImGui::PopID();
        return;
    }
    if(weapons::retired(e.key))
    {
        // Retired in round 21 (fitted hands): registered, no longer read by the game. Read here, not written.
        ImGui::TextDisabled("retired key (still %.4f): the game no longer reads it", c->value);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", e.name);
        ImGui::PopID();
        return;
    }

    if(e.kind == detail::Kind::Anchor)
    {
        if(anchorKeyShown != e.key || anchorSlotShown != slot)
        {
            q_strlcpy(anchorText, weapons::anchor(slot, e.key) ? weapons::anchor(slot, e.key) : "",
                      sizeof(anchorText));
            anchorKeyShown = e.key;
            anchorSlotShown = slot;
        }
        ImGui::SetNextItemWidth(190.f);
        if(ImGui::InputText("##anchor", anchorText, sizeof(anchorText), ImGuiInputTextFlags_EnterReturnsTrue))
            detail::writeText(e, slot, anchorText);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", weapons::anchor(slot, e.key));
    }
    else if(const char* const* names = detail::choiceNamesFor(e))
    {
        const char* items[10] = {nullptr};
        int count = 0;
        for(; count < 10 && names[count]; count++)
            items[count] = names[count];
        int chosen = za::clamp(static_cast<int>(c->value), 0, za::max(0, count - 1));
        q_snprintf(textBuf, sizeof(textBuf), "##%s", e.name);
        if(ImGui::Combo(textBuf, &chosen, items, count))
            detail::writeValue(e, slot, static_cast<float>(chosen));
        ImGui::SameLine();
        ImGui::TextDisabled("%s: %s", kindHint(e.kind), chosen == static_cast<int>(c->value) ? names[chosen] : "?");
    }
    else
    {
        float v = c->value;
        ImGui::SetNextItemWidth(150.f);
        q_snprintf(textBuf, sizeof(textBuf), "##%s", e.name);
        if(ImGui::DragFloat(textBuf, &v, e.step, e.lo, e.hi, "%.3f"))
            detail::writeValue(e, slot, v);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.f);
        q_snprintf(textBuf, sizeof(textBuf), "##n_%s", e.name);
        if(ImGui::InputFloat(textBuf, &v, 0.f, 0.f, "%.3f"))
            detail::writeValue(e, slot, v);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", kindHint(e.kind));
        if(changedHere)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("was %.3f", detail::savedValue(slot, e.key));
        }
    }
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\n%s %d\nthe console: vr_wofs_%s_%02d\nrange used here: %s %g .. %g", e.name,
                          detail::groupLabel(e.group), slot, e.name, slot + 1, kindHint(e.kind), e.lo, e.hi);
    ImGui::PopID();
}

// ---- The pages' column ------------------------------------------------------------------------------------------

void pagesColumn()
{
    // (16 pages in one column stand taller than a small screen's window, which pushed the key rows and everything
    // below it out of the window: in pageCols columns the whole list fits beside the rows).
    ImGui::Columns(pageCols, "pages", false);
    for(int c = 0; c < pageCols; c++)
        ImGui::SetColumnWidth(c, pageColW);
    for(int g = 0; g < static_cast<int>(detail::Group::Count); g++)
    {
        const detail::Group group = static_cast<detail::Group>(g);
        const int count = detail::countInGroup(group);
        q_snprintf(textBuf, sizeof(textBuf), "%s (%d)", detail::groupLabel(group), count);
        q_snprintf(spotBuf, sizeof(spotBuf), "page_%s", groupTags[g]);
        ImGui::PushID(g);
        const bool chosen = group == st.group;
        if(chosen)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.85f, 0.3f, 1.f});
        if(ImGui::Selectable(textBuf, chosen))
            st.group = group;
        if(chosen)
            ImGui::PopStyleColor();
        ImGui::PopID();
        noteSpot(spotBuf);
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

// ---- The key rows for the page chosen -----------------------------------------------------------------------------

void keysColumn(const int slot)
{
    q_snprintf(textBuf, sizeof(textBuf), "%s  --  slot %d, %d key(s) here, %d changed by this window",
               detail::groupLabel(st.group), slot, detail::countInGroup(st.group), detail::touchedCount(slot));
    ImGui::TextUnformatted(textBuf);
    ImGui::TextWrapped("%s", groupHints[static_cast<int>(st.group)]);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.f, 6.f});
    ImGui::SetNextItemWidth(220.f);
    ImGui::InputTextWithHint("##filter", "filter: part of a key's name", st.filter, sizeof(st.filter));
    noteSpot("filter");
    ImGui::SameLine();
    if(btn("clear filter", "filter_clear"))
        st.filter[0] = 0;
    if(st.filter[0])
    {
        ImGui::SameLine();
        ImGui::TextDisabled("only the keys whose name contains \"%s\"", st.filter);
    }
    ImGui::PopStyleVar();

    // The page's own picks: which hotspot, which holster.
    if(st.group == detail::Group::Hotspots)
    {
        ImGui::TextUnformatted("Which grip spot:");
        for(int h = 1; h <= weapons::maxHotspots; h++)
        {
            ImGui::SameLine();
            q_snprintf(textBuf, sizeof(textBuf), "%d", h);
            if(ImGui::SmallButton(textBuf))
                st.hotspot = h;
            q_snprintf(spotBuf, sizeof(spotBuf), "hotspot_%d", h);
            noteSpot(spotBuf);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("keys of hotspot %d are shown%s", st.hotspot,
                            st.hotspot == 1 ? " together with the ones every hotspot shares" : "");
    }
    if(st.group == detail::Group::Holstered)
    {
        ImGui::TextUnformatted("Which holster:");
        const char* const kinds[3] = {"hip", "upper arm", "shoulder"};
        for(int k = 0; k < weapons::holsterKinds; k++)
        {
            ImGui::SameLine();
            if(ImGui::SmallButton(kinds[k]))
                st.holster = k;
            q_snprintf(spotBuf, sizeof(spotBuf), "holster_%d", k);
            noteSpot(spotBuf);
        }
    }

    ImGui::Separator();
    st.rowsShown = 0;
    for(int i = 0; i < detail::tableSize; i++)
    {
        const detail::Entry& e = detail::tableEntries[i];
        if(e.group != st.group)
            continue;
        if(st.group == detail::Group::Hotspots && e.hotspot > 0 && e.hotspot != st.hotspot)
            continue;
        if(st.group == detail::Group::Holstered && e.holster >= 0 && e.holster != st.holster)
            continue;
        if(!detail::matches(e, st.filter))
            continue;
        keyRow(e, slot);
        st.rowsShown++;
    }
    if(st.rowsShown == 0)
        ImGui::TextDisabled("no key here matches that filter");
}

// ---- Aiming: the sights, the muzzle, where the gun shoots ---------------------------------------------------------

void aimPanel()
{
    ImGui::Separator();
    ImGui::TextUnformatted("Aiming, lined up: your hands, the weapon, its muzzle, its sights, where it shoots");
    ImGui::Checkbox("markers", &st.showMarkers);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("show the muzzle, the shot line, the barrel, the sight line and the fist, over the view");
    ImGui::SameLine();
    ImGui::Checkbox("crosshair", &st.showCrosshair);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("show the crosshair the shots are lined up with");
    ImGui::SameLine();
    ImGui::Checkbox("hold the game", &st.hold);
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("the game's physics is held while this window is open: monsters and props stand still, the "
                          "player still walks and looks (vr_weaponstudio hold 0 lets them run)");
    noteSpot("hold");

    for(int h = 1; h >= 0; h--)
    {
        const Readout& r = st.hand[h];
        if(!r.held)
        {
            ImGui::TextDisabled("your %s hand holds nothing: hold a weapon to line it up", h ? "main" : "off");
            continue;
        }
        ImGui::Text("%s hand: %s (slot %d)%s", h ? "main" : "off", r.model[0] ? r.model : "?", r.slot,
                    r.fist ? "" : "  (held without the rig's fist)");
        if(r.sights)
        {
            ImGui::Text("   sight line to where it shoots: %.2f deg   muzzle off the sight line: %.1f cm   "
                        "that muzzle to the model's muzzle: %.1f cm%s",
                        r.sightToShot, r.muzzleGapCm, r.realMuzzleCm, r.alignRan ? "   (line-up run: see the console)" : "");
        }
        else
        {
            ImGui::Text("   no sights painted on this model: its muzzle-to-butt line to where it shoots: %.2f deg, "
                        "its own muzzle %.1f cm from the muzzle the game shoots from%s",
                        r.barrelToShot, r.realMuzzleCm, r.alignRan ? "   (line-up run: see the console)" : "");
        }
    }

    ImGui::TextWrapped("The line-up measures a held weapon and prints what would line the sights up with where the gun "
                       "actually shoots; writing it fills the slot's Shot Pitch and Shot Yaw.");
    if(btn("line up both hands (measure)", "align"))
    {
        st.wantAlign = true;
        st.alignHand = 2;
        st.alignApply = false;
    }
    ImGui::SameLine();
    if(btn("main hand", "align_main"))
    {
        st.wantAlign = true;
        st.alignHand = 1;
        st.alignApply = false;
    }
    ImGui::SameLine();
    if(btn("off hand", "align_off"))
    {
        st.wantAlign = true;
        st.alignHand = 0;
        st.alignApply = false;
    }
    ImGui::SameLine();
    if(btn("line up and write both", "align_write"))
    {
        st.wantAlign = true;
        st.alignHand = 2;
        st.alignApply = true;
    }
    ImGui::TextDisabled("console: vr_weaponstudio_align [main | off | both] [apply]");
}

// ---- How it is held: your IK, and an armed humanoid ---------------------------------------------------------------

void holdPanel()
{
    ImGui::Separator();
    ImGui::TextUnformatted("Held: your hands' rig above, and an armed humanoid standing in front of you");
    const int kinds = buildKindList();
    q_snprintf(textBuf, sizeof(textBuf), "kind##kind");
    if(wordCombo(textBuf, st.previewKind, kindList, kinds, "preview_kind"))
        st.previewCode2 = 0;
    ImGui::SameLine();
    const int weapons = buildWeaponList(st.previewKind, false);
    int mainIndex = 0;
    for(int i = 0; i < weapons; i++)
    {
        if(toolgun::armed::weaponCode(i) == st.previewCode)
        {
            mainIndex = i;
            break;
        }
    }
    q_snprintf(textBuf, sizeof(textBuf), "main hand##code");
    if(wordCombo(textBuf, mainIndex, weaponList, weapons, "preview_code"))
        st.previewCode = weapons > 0 ? toolgun::armed::weaponCode(mainIndex) : 0;
    ImGui::SameLine();
    const int offCount = buildWeaponList(st.previewKind, true);
    int offIndex = 0;
    for(int i = 1; i < offCount; i++)
    {
        if(toolgun::armed::weaponCode(i - 1) == st.previewCode2)
        {
            offIndex = i;
            break;
        }
    }
    q_snprintf(textBuf, sizeof(textBuf), "off hand##code2");
    if(wordCombo(textBuf, offIndex, weaponList, offCount, "preview_code2"))
        st.previewCode2 = offIndex > 0 ? toolgun::armed::weaponCode(offIndex - 1) : 0;

    if(btn(st.preview.made ? "place another" : "place one", "preview_spawn"))
    {
        st.wantMake = true; // (placing one takes the previous one away first)
    }
    ImGui::SameLine();
    if(btn("remove it", "preview_remove"))
        st.wantRemove = true;
    ImGui::TextDisabled("it stands frozen in front of you, facing you, like the Rig Studio's subject");

    if(!st.preview.made)
    {
        ImGui::TextDisabled("nothing placed: the window's buttons, or vr_weaponstudio preview [kind weapon weapon2]");
        return;
    }
    const char* pose = st.preview.pose[0] ? st.preview.pose : "none seen yet";
    ImGui::Text("   %s holding %s%s: the shipped arms solve reports its pose \"%s\" with its weapons' slots %d and %d",
                toolgun::armed::kindName(st.preview.kind) ? toolgun::armed::kindName(st.preview.kind) : "?",
                toolgun::armed::codeName(st.preview.code) ? toolgun::armed::codeName(st.preview.code) : "?",
                st.preview.code2 ? " and one more in its off hand" : "", pose, st.preview.slot, st.preview.slotL);
    ImGui::TextDisabled("   so its grips, hotspots and two-handed posing are shown by the game itself, on a body: "
                        "the same pose and IK code that poses an armed monster in the game");
}

// The slot the window edits, kept a real one (a slot stops being in use when a model goes).
[[nodiscard]] int shownSlot()
{
    if(st.slot >= 0 && st.slot < weapons::slotsUsed())
        return st.slot;
    const int held = weapons::heldSlot(1);
    return held >= 0 && held < weapons::slotsUsed() ? held : 0;
}
} // namespace

// ---- The window ---------------------------------------------------------------------------------------------

// (Outside the anonymous namespace above: vr_main.cpp's client frame reaches it through the internal header.)
bool drawWindow()
{
    bool askClose = false;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2{14.f, 14.f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2{za::min(1220.f, ds.x - 28.f), za::min(900.f, ds.y - 28.f)}, ImGuiCond_Always);
    ImGui::Begin("Weapon Studio", nullptr, ImGuiWindowFlags_NoSavedSettings);

    const int slot = shownSlot();
    st.slot = slot;

    // Slot, and what the player is holding.
    const int slotCount = buildSlotList();
    int chosen = za::clamp(slot, 0, za::max(0, slotCount - 1));
    if(wordCombo("slot##slot", chosen, slotList, slotCount, "slot"))
        st.slot = chosen;
    ImGui::SameLine();
    if(btn("edit what my main hand holds", "held_main"))
    {
        const int h = weapons::heldSlot(1);
        if(h >= 0 && h < weapons::slotsUsed())
            st.slot = h;
    }
    ImGui::SameLine();
    if(btn("off hand", "held_off"))
    {
        const int h = weapons::heldSlot(0);
        if(h >= 0 && h < weapons::slotsUsed())
            st.slot = h;
    }
    ImGui::SameLine();
    if(btn("reset this slot to its defaults", "reset"))
        st.wantReset = true;
    ImGui::SameLine();
    if(btn("print this slot's changes", "print"))
        st.wantPrint = true;
    ImGui::SameLine();
    if(btn("close", "close"))
        askClose = true; // (done after ImGui::End, in vr_weaponstudio.cpp)

    // What this slot is, and the ways to move settings around it.
    const cvar_t* id = weapons::cvar(slot, weapons::Key::ID);
    const int from = weapons::inheritsFrom(slot);
    ImGui::Text("slot %d: %s", slot, id && id->string ? shortModel(id->string) : "?");
    ImGui::SameLine();
    ImGui::TextDisabled("%d slots in use, %d keys in all, %d changed by this window", weapons::slotsUsed(),
                        detail::tableSize, detail::touchedCount(slot));
    if(from >= 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("it copies slot %d", from);
        ImGui::SameLine();
        if(btn("stop copying", "stop_copy"))
            st.wantStopCopy = true;
    }
    ImGui::SameLine();
    int cf = za::clamp(st.copyFromSlot, 0, za::max(0, slotCount - 1));
    if(wordCombo("copy another slot's values into this one##copyfrom", cf, slotList, slotCount, "copy_from"))
        st.copyFromSlot = cf;
    ImGui::SameLine();
    if(btn("copy", "copy_into"))
    {
        st.copyFromSlot = cf;
        st.wantCopy = true;
    }

    // The aiming read-out and the held-weapon panel come first; the pages and the key rows fill what is left. Kept
    // below the pages' list they stood past the bottom of a small screen's window: their buttons were drawn outside the
    // window's clip, so a click (the tests' synthetic mouse, or a real one) never reached them.
    aimPanel();
    holdPanel();
    ImGui::Separator();
    ImGui::BeginGroup();
    pagesColumn();
    ImGui::EndGroup();
    ImGui::SameLine();
    const float rowsH = za::max(keyRowsMin, ImGui::GetContentRegionAvail().y);
    if(ImGui::BeginChild("keys", ImVec2{0.f, rowsH}, true))
        keysColumn(slot);
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled("every key here is the game's own weapon setting: written through its stand-in cvar, so the "
                        "Weapon Offsets and Weapon Weights menus, the config file and the game all see it at once. The "
                        "menus are unchanged and keep working.");

    ImGui::End();
    return !askClose;
}
} // namespace qvr::weaponstudio
