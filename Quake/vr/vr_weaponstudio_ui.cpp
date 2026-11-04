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

constexpr float pageColW = 178.f; // one page column (the pages' list sits in pageCols of them)
constexpr float labelW = 216.f;   // where a key's editor starts
constexpr float comboW = 176.f;   // a Combo's width: left to themselves they take the whole row and wrap the line
constexpr float filterW = 200.f;  // the key filter box
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

// ---- Rows that fit the screen ------------------------------------------------------------------------------------

// (Buttons and combos sit side by side and ImGui does not wrap them: on a 960x540 screen the window's top rows ran
// past its right edge, where neither a real mouse nor the tests' synthetic click reaches a widget. Before one that
// would not fit in what is left of the line, drop to a new line.)
[[nodiscard]] float buttonW(const char* label)
{
    return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.f;
}

void sameLine(const float w)
{
    ImGui::SameLine();
    if(w > ImGui::GetContentRegionAvail().x)
        ImGui::NewLine();
}

void sameLineBtn(const char* label)
{
    sameLine(buttonW(label));
}

void sameLineCombo() // (a Combo is as wide as SetNextItemWidth made it, whatever its label says)
{
    sameLine(comboW);
}

void sameLineCheck(const char* label)
{
    sameLine(ImGui::CalcTextSize(label).x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2.f);
}

void tip(const char* text)
{
    if(ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", text);
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
    if(e.key == weapons::Key::ID)
    {
        // The slot's model: what the slot is, not a number to drag (the store's weapons.txt names it).
        ImGui::TextDisabled("%s", c->string);
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
    ImGui::SetNextItemWidth(filterW);
    ImGui::InputTextWithHint("##filter", "filter: part of a key's name", st.filter, sizeof(st.filter));
    noteSpot("filter");
    sameLineBtn("clear filter");
    if(btn("clear filter", "filter_clear"))
        st.filter[0] = 0;
    if(st.filter[0])
    {
        q_snprintf(textBuf, sizeof(textBuf), "only the keys whose name contains \"%s\"", st.filter);
        sameLine(ImGui::CalcTextSize(textBuf).x);
        ImGui::TextUnformatted(textBuf);
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
    tip("show the muzzle, the shot line, the barrel, the sight line and the fist, over the view");
    sameLineCheck("crosshair");
    ImGui::Checkbox("crosshair", &st.showCrosshair);
    tip("show the crosshair the shots are lined up with");
    sameLineCheck("hold the game");
    ImGui::Checkbox("hold the game", &st.hold);
    tip("the game's physics is held while this window is open: monsters and props stand still. Your own walking is "
        "paused for those frames too -- those keys fly this window's camera, just below -- and your looking is untouched");
    noteSpot("hold");

    // The window's own camera: the view flown out of his eyes, so the weapon he is holding and both hand poses can be
    // seen from outside. Its keys are the Rig Studio's and so are its orbit and zoom speeds; its flying speed is a
    // tunable (Debug > Tunables > Weapons > Other).
    // One row, with the long story in its tip: what sits above the pages' list has to stay short, or that list's
    // buttons are drawn past the bottom of the window and no click reaches them (the tool's own test clicks them).
    ImGui::TextDisabled("fly its camera out: W A S D, Q E, Shift 3x");
    tip("how fast its eye moves: Debug > Tunables > Weapons > Other (vr_weaponstudio_fly_speed). A middle-drag or an Alt+left drag "
        "orbits the view round you and the wheel zooms it, at the Rig Studio's vr_rigstudio_orbit_speed and "
        "vr_rigstudio_zoom_speed. It is off until you press a key -- while it is off the view is your own eyes, and "
        "nothing the game measures changes. Out past the mode's keep-near distance (vr_flat_gun_near) the drawn hands, "
        "the weapon and the crosshair stay with the body instead of following the view, so the held weapon and both "
        "hand poses are seen from outside; the markers here and the read-outs below are measured from your body, not "
        "from this camera. vr_weaponstudio camera reset, or the button in this window's top row, brings the view back "
        "without closing it.");

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

    ImGui::TextWrapped("The line-up prints what would line the sights up with where the gun actually shoots; writing "
                       "it fills the slot's Shot Pitch and Shot Yaw.");
    if(btn("line up both hands (measure)", "align"))
    {
        st.wantAlign = true;
        st.alignHand = 2;
        st.alignApply = false;
    }
    sameLineBtn("main hand");
    if(btn("main hand", "align_main"))
    {
        st.wantAlign = true;
        st.alignHand = 1;
        st.alignApply = false;
    }
    sameLineBtn("off hand");
    if(btn("off hand", "align_off"))
    {
        st.wantAlign = true;
        st.alignHand = 0;
        st.alignApply = false;
    }
    sameLineBtn("line up and write both");
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
    sameLineCombo();
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
    sameLineCombo();
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
    sameLineBtn("remove it");
    if(btn("remove it", "preview_remove"))
        st.wantRemove = true;
    sameLine(470.f); // (its own line when the row is already full: it is a note, not a widget)
    ImGui::TextUnformatted("it stands frozen in front of you, facing you, like the Rig Studio's subject");
    ImGui::TextDisabled("its pose below comes from the game's own arms solve: with \"hold the game\" on the server does "
                        "not frame, so it stays unseen until that box is off");

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

// (Its bounds -- winMinW, winMinH -- are in the internal header: the self-test checks them too.)

// (Outside the anonymous namespace above: vr_main.cpp's client frame reaches it through the internal header.)
bool drawWindow()
{
    bool askClose = false;
    const ImVec2 ds = ImGui::GetIO().DisplaySize;

    // Its pos and size live in the studio's own state (no cvar, no ImGui ini): applied the frame the window
    // *appears*, read back at its End. Set-always every frame is what used to snap back every drag and resize.
    if(!st.winPlaced)
    {
        st.winX = 10.f;
        st.winY = 10.f;
        st.winW = za::min(1220.f, ds.x - 20.f);
        st.winH = za::min(900.f, ds.y - 20.f);
        st.winPlaced = true;
    }
    // A reopen puts it back where it was -- and, on a smaller screen, back onto it: some of its title bar stays
    // on the display, so its X and a drag on it are still within reach.
    st.winW = za::clamp(st.winW, winMinW, za::max(winMinW, ds.x));
    st.winH = za::clamp(st.winH, winMinH, za::max(winMinH, ds.y));
    st.winX = za::clamp(st.winX, 0.f, za::max(0.f, ds.x - 96.f));
    st.winY = za::clamp(st.winY, 0.f, za::max(0.f, ds.y - 32.f));
    ImGui::SetNextWindowPos(ImVec2{st.winX, st.winY}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2{st.winW, st.winH}, ImGuiCond_Appearing);
    // The size he drags it to is kept between those bounds (ImGui clamps the resize itself, grip and border).
    ImGui::SetNextWindowSizeConstraints(ImVec2{winMinW, winMinH},
                                        ImVec2{za::max(winMinW, ds.x), za::max(winMinH, ds.y)});

    // Its title bar carries an X only when Begin is handed the flag, so: the X closes it, and so does Escape --
    // unless a text box has the keyboard (the filter's box, a key's number keep Escape for themselves).
    // Movable, resizable and collapsible: no NoMove, NoResize, NoCollapse or NoTitleBar, so its title bar stays
    // visible with its collapse arrow at the left and its X at the right.
    bool open = true;
    ImGui::Begin("Weapon Studio", &open, ImGuiWindowFlags_NoSavedSettings);
    if(!open)
        askClose = true; // (closed after ImGui::End, in vr_weaponstudio.cpp -- never from inside the window)
    if(!askClose && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::GetIO().WantTextInput)
        askClose = true;

    // Where the window's own frame is, named for the tests' mouse: the middle of its title bar (a drag there moves
    // it), its X at the right edge, and its lower-right corner (a drag from it resizes it). The X sits at
    // SizeFull.x - WindowBorderSize - FramePadding.x - FontSize, its box FontSize wide (imgui.cpp's title bar).
    if(st.testing)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        const float fs = ImGui::GetFontSize();
        noteSpotAt("title_bar", wp.x + ws.x * 0.5f, wp.y + (fs + style.FramePadding.y * 2.f) * 0.5f);
        noteSpotAt("title_x", wp.x + ws.x - style.WindowBorderSize - style.FramePadding.x - fs * 0.5f,
                   wp.y + style.FramePadding.y + fs * 0.5f);
        noteSpotAt("win_corner", wp.x + ws.x - 4.f, wp.y + ws.y - 4.f);
    }

    const int slot = shownSlot();
    st.slot = slot;

    // Slot, what the player is holding, and Close, on one row that has to fit the screen (sameLine drops to a new
    // line rather than pushing a button past the window's right edge, where no click reaches it).
    const int slotCount = buildSlotList();
    int chosen = za::clamp(slot, 0, za::max(0, slotCount - 1));
    if(wordCombo("slot##slot", chosen, slotList, slotCount, "slot"))
        st.slot = chosen;
    sameLineBtn("my main hand");
    if(btn("my main hand", "held_main"))
    {
        const int h = weapons::heldSlot(1);
        if(h >= 0 && h < weapons::slotsUsed())
            st.slot = h;
    }
    tip("edit the weapon your main hand is holding");
    sameLineBtn("my off hand");
    if(btn("my off hand", "held_off"))
    {
        const int h = weapons::heldSlot(0);
        if(h >= 0 && h < weapons::slotsUsed())
            st.slot = h;
    }
    tip("edit the weapon your off hand is holding (a two-handed grip, a shield)");
    sameLineBtn("camera back to my eyes");
    if(btn("camera back to my eyes", "cam_reset"))
        st.wantCamReset = true; // (done after ImGui::End, in vr_weaponstudio.cpp)
    tip("the view is your own eyes again, without closing this window (vr_weaponstudio camera reset)");
    sameLineBtn("close");
    if(btn("close", "close"))
        askClose = true; // (done after ImGui::End, in vr_weaponstudio.cpp)

    ImGui::NewLine();
    // What this slot is, and the ways to move settings around it.
    const cvar_t* id = weapons::cvar(slot, weapons::Key::ID);
    const int from = weapons::inheritsFrom(slot);
    ImGui::AlignTextToFramePadding();
    q_snprintf(textBuf, sizeof(textBuf), "slot %d: %s", slot, id && id->string ? shortModel(id->string) : "?");
    ImGui::TextUnformatted(textBuf);
    sameLine(250.f);
    ImGui::TextDisabled("%d slots in use, %d keys in all, %d changed by this window", weapons::slotsUsed(),
                        detail::tableSize, detail::touchedCount(slot));
    if(from >= 0)
    {
        sameLine(140.f);
        ImGui::TextDisabled("it copies slot %d", from);
        sameLineBtn("stop copying");
        if(btn("stop copying", "stop_copy"))
            st.wantStopCopy = true;
    }
    sameLineCombo();
    int cf = za::clamp(st.copyFromSlot, 0, za::max(0, slotCount - 1));
    if(wordCombo("copy another slot's values into this one##copyfrom", cf, slotList, slotCount, "copy_from"))
        st.copyFromSlot = cf;
    sameLineBtn("copy");
    if(btn("copy", "copy_into"))
    {
        st.copyFromSlot = cf;
        st.wantCopy = true;
    }
    sameLineBtn("reset this slot to its defaults");
    if(btn("reset this slot to its defaults", "reset"))
        st.wantReset = true;
    sameLineBtn("print this slot's changes");
    if(btn("print this slot's changes", "print"))
        st.wantPrint = true;

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
    ImGui::TextWrapped("every key here is the game's own weapon setting, written through its stand-in cvar: the Weapon "
                       "Offsets and Weapon Weights menus, the config file and the game all see it at once. The menus "
                       "are unchanged and keep working.");

    // Read it back before its End: where it sits now is where a reopen puts it back. A collapsed window's size is
    // only its title bar, which is not the size to remember -- its pos is.
    st.winX = ImGui::GetWindowPos().x;
    st.winY = ImGui::GetWindowPos().y;
    if(!ImGui::IsWindowCollapsed())
    {
        st.winW = ImGui::GetWindowSize().x;
        st.winH = ImGui::GetWindowSize().y;
    }

    ImGui::End();
    return !askClose;
}
} // namespace qvr::weaponstudio
