#include "vr_cheats.hpp"
#include "vr_doom.hpp"
#include "vr_toolgun.hpp"
#include "vr_warmup.hpp"
#include "vr_alloccount.h"
// vr_menu.cpp -- the "VR Settings" pages (Options > VR Settings), drawn like Ironwail's options
// pages: scrolling lists of labelled settings, changed with left/right (the sticks in VR), with
// actions on enter (A). "Advanced VR Options" (the main menu's Advanced VR, the corner's) lists further pages: the old
// Quake VR settings pages (vr_menu_pages.inc) and the new body, throwing and force grab tweaks,
// grouped by topic, the long ones split into pages of a screenful or so. In a headset the pages are
// taller (vr_menu_height, vr_menuui.cpp): more rows at once. Escape (B) goes back a page. With the mouse (and the VR laser pointer, vr_menuui.cpp): the row under
// it is selected, a click picks it, and a slider is set where it is clicked and dragged. Sliders of
// placements, angles, scales and distances (extend()) go on past their bar's ends with left and right
// (the bar showing its end, the value the real one); values set further off in the console stay.
// Each page is shown again where it was left (its selected row, found by its label when the page is
// built anew, on the same line of the view), across restarts too (vr_menu_positions).

#include "vr_seated.hpp"
#include "vr_serverrules.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_backend.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_mgroups.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_gfxpresets.hpp"
#include "vr_mapindex.hpp"
#include "vr_mapinstall.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_menu.hpp"
#include "vr_ad.hpp"
#include "vr_menuui.hpp"
#include "vr_panel.hpp"
#include "vr_obs.hpp"
#include "vr_menupaint.hpp"
#include "vr_motion.hpp"
#include "vr_motion_review.hpp"
#include "vr_motion_take.hpp"
#include "vr_weapons.hpp"
#include "vr_hands.hpp"
#include "vr_holstersetup.hpp"
#include "vr_bladeedit.hpp"
#include "vr_posing.hpp"
#include "vr_sightalign.hpp"
#include "vr_setup.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodytrack.hpp"
#include "vr_checklist.hpp"
#include "vr_scenario.hpp"
#include "vr_view.hpp"
#include "vr_units.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_props.hpp"
#include "vr_ragtest.hpp"
#include "vr_ragtool.hpp"
#include "vr_relight.hpp"
#include "vr_relight_tool.hpp"
#include "vr_retro.hpp"
#include "vr_fatigue.hpp"
#include "vr_weight.hpp"
#include "vr_twohand.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Count.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/ReverseIterator.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/Pair.hpp"
#include "vr_zancle.hpp"
#include "vr_xr_runtime.hpp"

#include <stdlib.h>
#include <string.h>

extern "C" {
extern float m_mousex, m_mousey; // menu.c: the mouse in menu coordinates
extern qboolean keydown[MAX_KEYS]; // keys.c
extern cvar_t ui_mouse_sound; // menu.c
extern cvar_t vr_zone_threadcheck; // zone.c
extern cvar_t snd_mixasync; // snd_dma.c
const char* M_Main_RowLabel(void); // menu.c: the main menu's selected row (menu_vr pos)
void M_Main_RowsExtent(float* left, float* right); // menu.c: its rows' ends as last drawn (menu_vr pos)
void M_Main_RowsSpan(float* top, float* bottom, float* canvasTop, float* canvasBottom); // menu.c: its top, its last
                                                                                        // row's bottom, the canvas's
extern int m_singleplayer_cursor; // menu.c: Single Player's (menu_vr pos)
int M_ContentLeft(void); // menu.c: the left edge of what its menu shown draws (menu x)
float M_ContentRightBelow(float y); // menu.c: how far right its menu shown draws below y (the main menu's rows as drawn)
float M_ContentLeftBelow(float y);  // menu.c: and how far left (1e9: nothing below y)
int M_TextLeft(void); // menu.c: its leftmost text (Ironwail's lists; 320 for Quake's menus)
void M_Main_Layout(int* step, int* gap); // menu.c: the main menu's rows' spacing and its groups' gaps
}

using namespace qvr;

namespace
{

struct Choice
{
    float value;
    const char* label;
};

struct Item
{
    enum Kind
    {
        Header,
        Slider,
        Cycle,
        Action,
        Info // a line of text (info()), centred, not selectable
    };

    Kind kind;
    const char* label;
    cvar_t* cvar{nullptr};

    // Slider.
    float min{0.f};
    float max{1.f};
    float step{0.1f};
    const char* format{"%.2f"};

    // Cycle.
    za::Vector<Choice> choices;

    // Action: a function, or a page to open, or a console command (command()).
    void (*action)(){nullptr};
    int page{-1};
    const char* command{nullptr};

    // Info: its text, asked for each time it is drawn.
    const char* (*info)(){nullptr};

    // Info drawn as a progress bar (progressBar()): how far, 0..1 (below 0: an empty row), and its text, right of it.
    float (*progress)(){nullptr};

    // Shown under the list while selected.
    const char* helpText{nullptr};

    // Slider: shown instead of the value while it is negative (-1: a hue following the player's; a class's setting
    // following the global one), the bar's leftmost step; right of it the values from negativeStart on.
    const char* negativeLabel{nullptr};
    float negativeStart{0.f};

    // Slider: left and right go on past the bar's ends, as far as hardMin..hardMax (extend()).
    bool extendable{false};
    float hardMin{0.f};
    float hardMax{0.f};

    // A line of a list (the Review Takes page's takes, its details): the text, the help and the action take `arg`
    // (infoLine(), row()). A row (wide) is an action drawn as its text across the whole width.
    int arg{-1};
    const char* (*infoArg)(int){nullptr};
    const char* (*helpArg)(int){nullptr};
    void (*actionArg)(int){nullptr};
    bool wide{false};

    // A row drawn dimmed while dimArg(arg) (the Checklist's ticked items), and a row that goes on the one `partOf` rows
    // above (a long text's next line: selected with it, never on its own).
    bool (*dimArg)(int){nullptr};
    int partOf{0};

    // A row of buttons side by side (the Checklist's Pass, Fail and Test under each item; ROUND22.md, "Checklist: Pass,
    // Fail and Test"): `buttons` of them, each `buttonLabel(arg, b)`, dimmed while `buttonDim(arg, b)`, its text tinted
    // by `buttonTint(arg, b)` (0 none, 1 green, 2 red). The item's first row (the one selected) has `buttonPress`: left
    // and right pick a button (MenuInstance::button), a press presses it; the mouse or laser over the button row picks
    // the one under it, a click there presses it (a click on the item's text does nothing). `markArg`: the item's text
    // tinted likewise (its state).
    int buttons{0};
    const char* (*buttonLabel)(int, int){nullptr};
    bool (*buttonDim)(int, int){nullptr};
    int (*buttonTint)(int, int){nullptr};
    void (*buttonPress)(int, int){nullptr};
    int (*markArg)(int){nullptr};

    // The menu detail level (vr_menu_level: MenuLevel, below) it is shown from: a row for the curious (advanced()) or for
    // tuning and testing (developer()) is left out of the pages below that level.
    int level{0};

    // A setting that can't be used here (unavailable()): dimmed, shown off, not changed; its help says why.
    bool unavailable{false};

    [[nodiscard]] Item advanced() const
    {
        Item i = *this;
        i.level = 1;
        return i;
    }

    [[nodiscard]] Item developer() const
    {
        Item i = *this;
        i.level = 2;
        return i;
    }

    [[nodiscard]] Item help(const char* text) const
    {
        Item i = *this;
        i.helpText = text;
        return i;
    }

    // Dimmed, shown off and not changed, `why` its help (what it needs: See-Through Liquids without VisPatch's data).
    [[nodiscard]] Item unavailableBecause(const char* why) const
    {
        Item i = *this;
        i.unavailable = true;
        i.helpText = why;
        return i;
    }

    // A slider whose value means something past its bar's ends (offsets, positions, angles, scales,
    // distances; not shares, volumes, colours or chances): left and right step on past them, to
    // lo..hi, the bar showing its end (the laser sets it only along the bar).
    [[nodiscard]] Item extend(float lo, float hi) const
    {
        Item i = *this;
        i.extendable = true;
        i.hardMin = za::fmin(lo, min);
        i.hardMax = za::fmax(hi, max);
        return i;
    }

    // As far as four times the bar's width beyond each end (none below a bar starting at 0 or more).
    [[nodiscard]] Item extend() const
    {
        const float width = max - min;
        return extend(min >= 0.f ? min : min - 4.f * width, max + 4.f * width);
    }
};

void calibrateHeight()
{
    const TrackingState& t = tracking();
    if(vrActive() && t.head.valid)
    {
        Cvar_SetValueQuick(&vr_height_calibration, t.head.position.y);
        Con_Printf("VR: height calibrated to %.2f m\n", t.head.position.y);
    }
}

void restartVr()
{
    Cbuf_AddText("vr_restart\n");
}

[[nodiscard]] Item header(const char* label)
{
    return {Item::Header, label};
}

[[nodiscard]] Item slider(const char* label, cvar_t* cvar, float min, float max, float step, const char* format)
{
    Item i{Item::Slider, label, cvar};
    i.min = min;
    i.max = max;
    i.step = step;
    i.format = format;
    return i;
}

[[nodiscard]] Item slider(const char* label, cvar_t& cvar, float min, float max, float step, const char* format)
{
    return slider(label, &cvar, min, max, step, format);
}

// By name, for settings of Ironwail's too: left out when there is no such cvar.
[[nodiscard]] Item slider(const char* label, const char* cvar, float min, float max, float step, const char* format)
{
    return slider(label, Cvar_FindVar(cvar), min, max, step, format);
}

// A monster class's own setting (Gibs and Corpses > Ragdolls > Grunt): its leftmost step -1, the global one ("Global");
// then from `from` to `max`.
[[nodiscard]] Item classSlider(const char* label, cvar_t& cvar, float from, float max, float step, const char* format)
{
    Item i = slider(label, cvar, from - step, max, step, format);
    i.negativeLabel = "Global";
    i.negativeStart = from;
    return i;
}

// The enemies the training dummy can stand as (vr_dummy_type; QC vr_dummy_types.qc numbers them): a mission pack's only
// when it is installed (else the QC makes it a grunt).
[[nodiscard]] za::Vector<Choice> dummyEnemies()
{
    za::Vector<Choice> out{{0.f, "Grunt"}, {1.f, "Enforcer"}, {2.f, "Knight"}, {3.f, "Death Knight"}, {4.f, "Ogre"},
        {5.f, "Fiend"}, {6.f, "Shambler"}, {7.f, "Zombie"}, {8.f, "Vore"}, {9.f, "Scrag"}, {10.f, "Rottweiler"},
        {11.f, "Spawn"}, {12.f, "Rotfish"}};
    const cvar_t* hipnotic = Cvar_FindVar("vr_hipnotic_available");
    const cvar_t* rogue = Cvar_FindVar("vr_rogue_available");
    if(hipnotic && hipnotic->value != 0.f)
    {
        out.pushBack({13.f, "Gremlin"});
        out.pushBack({14.f, "Centroid"});
    }
    if(rogue && rogue->value != 0.f)
    {
        out.pushBack({15.f, "Mummy"});
        out.pushBack({16.f, "Wrath"});
        out.pushBack({17.f, "Overlord"});
        out.pushBack({18.f, "Electric Eel"});
    }
    if(VR_CampaignDataAvailable("mg3")) // (Dawn of the Machine's own: its data read in place, in any campaign)
    {
        out.pushBack({19.f, "Rocket Ogre"});
        out.pushBack({20.f, "Demo Dog"});
        out.pushBack({21.f, "Ranged Knight"});
        out.pushBack({30.f, "Orb"});
        out.pushBack({32.f, "Lava Man (Dawn of the Machine)"});
        out.pushBack({33.f, "Super Shambler"});
        out.pushBack({50.f, "Shub-Niggurath"});
        out.pushBack({51.f, "Shub's Eye"});
        out.pushBack({40.f, "Chthon (Dawn of the Machine)"});
    }
    out.pushBack({31.f, "Slime"}); // (Dawn of the Machine's splitting spawn: Quake's model)
    return out;
}

// The training dummy's full health (vr_dummy_health), its leftmost step -1: its enemy's own ("Its Own": a grunt's 30).
[[nodiscard]] Item dummyHealthSlider()
{
    Item i = slider("Dummy Health", vr_dummy_health, 0.f, 1000.f, 5.f, "%.0f")
                 .extend(1.f, 100000.f)
                 .help("The training dummy's full health: what its hits take away (shown over it) and, with Dummy Dies, "
                       "what kills it. Its Own: its enemy's (a grunt's 30, an ogre's 200).");
    i.negativeLabel = "Its Own";
    i.negativeStart = 5.f;
    return i;
}

// Dynamic resolution's ceiling (vr_dynres_max), its leftmost step below 0: Render Scale's (the default).
[[nodiscard]] Item dynresMaxSlider()
{
    Item i = slider("Dynamic Highest Scale", vr_dynres_max, 0.45f, 1.5f, 0.05f, "%.2f")
                 .extend(0.45f, 2.f)
                 .help("The highest scale Dynamic Resolution goes to, with headroom. Render Scale: yours (the default), so "
                       "with headroom nothing changes; above it, sharper while the graphics card keeps up.");
    i.negativeLabel = "Render Scale";
    i.negativeStart = 0.5f;
    return i;
}

// Rocks and bricks in a multiplayer map (vr_debris_mp_max), its leftmost step -1: as many as in single player (Most in a
// Map, the default: the author's, 2026-10-06).
[[nodiscard]] Item debrisMultiplayerSlider()
{
    Item i = slider("Most in Multiplayer", vr_debris_mp_max, -8.f, 160.f, 8.f, "%.0f")
                 .extend(-8.f, 400.f)
                 .help("The most in a multiplayer map (yours as the host). They are the server's, to pick up and throw: "
                       "each one in sight costs every player's network packets (about 18 bytes a frame). Single Player's: "
                       "as many as Most in a Map. 0: none. Next map.");
    i.negativeLabel = "Single Player's";
    i.negativeStart = 0.f;
    return i;
}

// An effect's hue (degrees), its leftmost step -1: the player's (vr_player_hue, vr_hue.hpp).
[[nodiscard]] Item hueSlider(const char* label, cvar_t& cvar)
{
    Item i = slider(label, cvar, -5.f, 355.f, 5.f, "%.0f");
    i.negativeLabel = "Player's";
    return i;
}

[[nodiscard]] Item cycle(const char* label, cvar_t* cvar, za::Vector<Choice> choices)
{
    Item i{Item::Cycle, label, cvar};
    i.choices = ZA_MOVE(choices);
    return i;
}

[[nodiscard]] Item cycle(const char* label, cvar_t& cvar, za::Vector<Choice> choices)
{
    return cycle(label, &cvar, ZA_MOVE(choices));
}

[[nodiscard]] Item cycle(const char* label, const char* cvar, za::Vector<Choice> choices)
{
    return cycle(label, Cvar_FindVar(cvar), ZA_MOVE(choices));
}

[[nodiscard]] Item toggle(const char* label, cvar_t& cvar)
{
    return cycle(label, cvar, {{0.f, "Off"}, {1.f, "On"}});
}

[[nodiscard]] Item toggle(const char* label, const char* cvar)
{
    return cycle(label, cvar, {{0.f, "Off"}, {1.f, "On"}});
}

[[nodiscard]] Item action(const char* label, void (*fn)())
{
    Item i{Item::Action, label};
    i.action = fn;
    return i;
}

// A console command run as typed (the Debug pages' buttons: reports, reloads, tests). A command registered only once
// its system first runs (the wrist gadget's, the physics') says so instead of "unknown command".
void runCommand(const char* text)
{
    char name[64];
    size_t n = 0;
    while(text[n] && text[n] != ' ' && n + 1 < sizeof(name))
    {
        name[n] = text[n];
        n++;
    }
    name[n] = '\0';
    if(!Cmd_Exists(name) && !Cvar_FindVar(name))
    {
        Con_Printf("%s: not available yet (its part of the game hasn't run in this session)\n", name);
        return;
    }
    Cbuf_InsertText(va("%s\n", text)); // (next, ahead of what waits in the buffer: a test script's waits)
}

[[nodiscard]] Item command(const char* label, const char* text)
{
    Item i{Item::Action, label};
    i.command = text;
    return i;
}

[[nodiscard]] Item info(const char* (*text)())
{
    Item i{Item::Info, ""};
    i.info = text;
    return i;
}

// A progress bar across the row, filled to fraction() (an empty row while it is below 0), text() right of it (the
// percentage, the time left): Graphics > Relighting's.
[[nodiscard]] Item progressBar(float (*fraction)(), const char* (*text)())
{
    Item i{Item::Info, ""};
    i.progress = fraction;
    i.info = text;
    return i;
}

// A line of text, text(arg) (as info()).
[[nodiscard]] Item infoLine(const char* (*text)(int), int arg)
{
    Item i{Item::Info, ""};
    i.infoArg = text;
    i.arg = arg;
    return i;
}

// A row of a list across the whole width, its text text(arg), its help help(arg): enter calls pick(arg), then opens
// `page` (if >= 0).
[[nodiscard]] Item row(const char* (*text)(int), const char* (*help)(int), void (*pick)(int), int arg, int page)
{
    Item i{Item::Action, ""};
    i.infoArg = text;
    i.helpArg = help;
    i.actionArg = pick;
    i.arg = arg;
    i.page = page;
    i.wide = true;
    return i;
}

// Whether the cursor can rest on it.
[[nodiscard]] bool selectable(const Item& item)
{
    return item.kind != Item::Header && item.kind != Item::Info && item.partOf == 0;
}

[[nodiscard]] Item open(const char* label, int page)
{
    Item i{Item::Action, label};
    i.page = page;
    return i;
}

using PageBuilder = za::Vector<Item> (*)();

// The index of the page `build` builds (pages[], below): what a link to it opens.
[[nodiscard]] int pageIndex(PageBuilder build);

// Pages linked before they are defined.
[[nodiscard]] za::Vector<Item> pageBodyArms();
[[nodiscard]] za::Vector<Item> pageBodyCalibration();
[[nodiscard]] za::Vector<Item> pageBodyCrouch();
[[nodiscard]] za::Vector<Item> pageSeated();
[[nodiscard]] za::Vector<Item> pageSeatedHolsters();
[[nodiscard]] za::Vector<Item> pageWeightDamage();
[[nodiscard]] za::Vector<Item> pageAimingSettings();
[[nodiscard]] za::Vector<Item> pageWeaponDamage();
[[nodiscard]] za::Vector<Item> pageHotBlades();
[[nodiscard]] za::Vector<Item> pageDamage();
[[nodiscard]] za::Vector<Item> pageBloodFeedback();
[[nodiscard]] za::Vector<Item> pageFingersCollisions();
[[nodiscard]] za::Vector<Item> pageFlashlightLowGrip();
[[nodiscard]] za::Vector<Item> pageFlashlightOverheadGrip();
[[nodiscard]] za::Vector<Item> pageFlashlightMounts();
[[nodiscard]] za::Vector<Item> pageBodyDisplay();
[[nodiscard]] za::Vector<Item> pageHeadset();
[[nodiscard]] za::Vector<Item> pageDebugViews();
[[nodiscard]] za::Vector<Item> pageDebugLogging();
[[nodiscard]] za::Vector<Item> pageDebugProfiling();
[[nodiscard]] za::Vector<Item> pageDebugReports();
[[nodiscard]] za::Vector<Item> pageDebugTools();
[[nodiscard]] za::Vector<Item> pageDebugTests();
[[nodiscard]] za::Vector<Item> pageDebugAutomation();
[[nodiscard]] za::Vector<Item> pageDebugCheats();
[[nodiscard]] za::Vector<Item> pageStealth();
[[nodiscard]] za::Vector<Item> pageStealthTests();
[[nodiscard]] za::Vector<Item> pageRagdollTests();
[[nodiscard]] za::Vector<Item> pageRagtool(); // (vr_menu_ragtool.inc)
[[nodiscard]] za::Vector<Item> pageRagdolls();
[[nodiscard]] za::Vector<Item> pageSpawnWeapons();
[[nodiscard]] za::Vector<Item> pageHitbox();
[[nodiscard]] za::Vector<Item> pageMonsterHitbox();
[[nodiscard]] za::Vector<Item> pageChanged();
[[nodiscard]] za::Vector<Item> pageSearch();
[[nodiscard]] za::Vector<Item> pageConsole();
[[nodiscard]] za::Vector<Item> pageMaps();
// (Settings with their home on another page link to it: one home per setting.)
[[nodiscard]] za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageVrsHud();
[[nodiscard]] za::Vector<Item> pageVrsAppearance();
[[nodiscard]] za::Vector<Item> pageVrsRangerHead();
[[nodiscard]] za::Vector<Item> pageColours();
[[nodiscard]] za::Vector<Item> pageBulletTime();
[[nodiscard]] za::Vector<Item> pageStamina();
[[nodiscard]] za::Vector<Item> pageGibs();
[[nodiscard]] za::Vector<Item> pageGore();
[[nodiscard]] za::Vector<Item> pageLimbGore();
[[nodiscard]] za::Vector<Item> pageScreens();
[[nodiscard]] za::Vector<Item> pageDoom();
[[nodiscard]] za::Vector<Item> pageTips();
[[nodiscard]] za::Vector<Item> pageHipHolsters();
[[nodiscard]] za::Vector<Item> pageHolsterZones();
[[nodiscard]] za::Vector<Item> pageClimbing();
[[nodiscard]] za::Vector<Item> pageRecording();

// Texts the menu hands out by pointer (an Item holds const char*):
// - readouts: an info line's or a help's text, valid until the same function's next call (the draw uses it at once);
//   released at a map change like any scratch.
struct MenuReadouts
{
    za::String motionNote, motionLastSaved, extendableHelp, serverRuleHelp, fineHelp;
    za::String checklistUndoHelp;      // checklistUndoHelp
    za::String checklistItemHelp;      // checklistHelp
    za::String weight[2];              // weightReadout, by hand
    za::String weaponWeightsDamage[2]; // weaponWeightsDamageReadout, by line
    za::String heldObjectMass;
    za::String heldObjectDamage[2];    // by line
    za::String weaponWeightsDrop;      // weaponWeightsDropReadout
    za::String weaponWeightsHeavy;     // weaponWeightsHeavyReadout
    za::String weaponWeightsHits[2];   // weaponWeightsHitsReadout, by line
    za::String weaponOffsetsStock;     // weaponOffsetsStockReadout
    char checklistSummary[48];         // checklistSummary
    char checklistTesting[48];         // checklistTesting
    char stamina[96];                 // staminaReadout
    char renderScaleHelp[192];         // renderScaleHelp
    char buildVersion[64];             // buildVersionLine
    auto members()
    {
        return qvr::mem::list(motionNote, motionLastSaved, extendableHelp, serverRuleHelp, fineHelp, weight, weaponWeightsDamage, heldObjectMass, heldObjectDamage,
            weaponWeightsDrop, weaponWeightsHeavy, weaponWeightsHits, weaponOffsetsStock, checklistSummary, checklistTesting, checklistUndoHelp, checklistItemHelp, stamina, renderScaleHelp, buildVersion);
    }
};
mem::Scratch<MenuReadouts> readouts{"menu readouts"};

// - the pages' titles and choice names: the built pages' items point into them (a page writes its own only as it is
//   built again, its old items replaced), so they are kept as long as the built pages (MenuPages): never released.
// The Weapon Offsets pages: the main one and its parts (weaponOffsetsPartPages lists them in this order).
enum WeaponOffsetsPart
{
    WofsMain,
    WofsHand,
    WofsFingers,
    WofsSights,
    WofsTwoHanded,
    WofsStock,
    WofsScreen,
    WofsHolstered,
    WofsEffects,
    WofsFlashlight,
    WofsParts
};

struct PageTexts
{
    za::String weaponOffsetsTitle[WofsParts], weaponOffsetsInheritTitle[WofsParts]; // by WeaponOffsetsPart
    za::Vector<za::Pair<float, za::String>> weaponOffsetsInheritNames; // the Inherit From choice's
    za::String weaponWeightsTitle, weaponWeightsInheritTitle;
    za::String heldObjectOffsetsTitle, heldObjectWeightsTitle;
    za::Vector<za::String> checklistSections; // the Checklist's headers
    za::String checklistUndone;               // the item Undo Last Tick changed last (its help says)
    auto members()
    {
        return qvr::mem::list(weaponOffsetsTitle, weaponOffsetsInheritTitle, weaponOffsetsInheritNames, weaponWeightsTitle,
            weaponWeightsInheritTitle, heldObjectOffsetsTitle, heldObjectWeightsTitle, checklistSections, checklistUndone);
    }
};
mem::Cache<PageTexts> pageTexts{"menu texts", mem::Never};

// The player's grunts (vr_climb.cpp mantleGrunts, climb::grunt: the same order): the mantle's and the judo throw's Grunt
// Sound.
[[nodiscard]] za::Vector<Choice> gruntChoices()
{
    return {{0.f, "Jump grunt"}, {1.f, "Hard landing"}, {2.f, "Jump, deeper"}, {3.f, "Landing, deeper"}, {4.f, "Pain grunt"}};
}

// vr_menu_button's choices (VR Settings > Controls, HUD and Menus > Menu): what opens the menu besides the menu button.
[[nodiscard]] za::Vector<Choice> menuButtonChoices()
{
    return {{0.f, "Menu Button Only"}, {1.f, "Or Both Sticks"}, {2.f, "Or Both Sticks, Hold Y"}, {3.f, "Or Both Sticks, Hold B"},
        {4.f, "Or Both Sticks, Y"}};
}

const char* const menuButtonHelp =
    "What opens the menu (and closes it from any page) besides the controller's menu button, which some controllers and "
    "runtimes keep for themselves (Pico under SteamVR). Both Sticks: click both sticks together. Hold Y or B: hold it "
    "for a moment in the game (its own action happens first). Y: Y is the menu button, no longer previous weapon. "
    "Escape on the keyboard always works.";

#include "vr_menu_props.inc"
#include "vr_menu_pages.inc"
#include "vr_menu_recording.inc"
#include "vr_menu_cheats.inc"
#include "vr_menu_toolgun.inc"
#include "vr_menu_stealth.inc"
#include "vr_menu_ragtests.inc"
#include "vr_menu_ragtool.inc" // Debug > Ragdoll Tool (vr_ragtool.cpp)
#include "vr_menu_ad_monsters.inc" // AD support: Debug > Tests > Arcane Dimensions Monsters

// ----------------------------------------------------------------------------
// Pages of the port's own tweaks
// ----------------------------------------------------------------------------

// The motion recorder (vr_motion.cpp, docs/vr-port/MOTIONS.md): takes of the player's motions, each
// labelled with what it should be, for tuning the melee.
[[nodiscard]] const char* motionNote()
{
    za::String& text = readouts.motionNote;
    text = vr_motion_note.string[0] ? za::String{"note: "} + vr_motion_note.string : "no note (vr_motion_note)";
    return text.cStr();
}

[[nodiscard]] const char* motionLastSaved()
{
    za::String& text = readouts.motionLastSaved;
    text = za::String{"last: "} + motion::lastSaved();
    return text.cStr();
}

// The category's details, for the Detail choice: the menu rebuilds the page when the category changes.
int motionPageCategory = -1;

[[nodiscard]] za::Vector<Item> pageMotionRecorder()
{
    za::Vector<Choice> categories;
    const auto& list = motion::categories();
    for(const int i : motion::categoryOrder())
    {
        categories.pushBack({static_cast<float>(i), list[i].choice.display});
    }
    za::Vector<Choice> details;
    const auto& chosen = motion::chosenCategory().details;
    for(size_t i = 0; i < chosen.size(); i++)
    {
        details.pushBack({static_cast<float>(i), chosen[i].display});
    }
    motionPageCategory = static_cast<int>(vr_motion_category.value);
    return {
        header("Record Motions for the Melee"),
        command("Melee Recording Room", "map vrmelee-recording")
            .help("All melee weapons and one central training dummy, with clear room to record your motions."),
        toggle("Arm Recorder", vr_motion_armed)
            .help("Armed: click the Record Button to start a take of the Category, click it again to end it and save "
                  "it (quakevr/motions). A beep and a buzz each time; REC in view."),
        cycle("Category", vr_motion_category, ZA_MOVE(categories))
            .help("What the motion should do. It stays chosen: record many takes of it in a row. No Hit: motions that "
                  "must do nothing (wiggles, weak moves, reloading)."),
        cycle("Detail", vr_motion_detail, ZA_MOVE(details))
            .help("Optional: which kind (the swing's direction, the weapon). Its takes count apart, and with the "
                  "category's."),
        info(motion::labelStatus),
        cycle("Record Button", vr_motion_button,
            {{0.f, "Off stick click"}, {1.f, "Main stick click"}, {2.f, "A (main lower)"}, {3.f, "B (main upper)"},
                {4.f, "X (off lower)"}, {5.f, "Y (off upper)"}, {6.f, "Off grip"}, {7.f, "Main grip"},
                {8.f, "Hold B + main trigger"}, {9.f, "Hold Y + off trigger"}})
            .help("Pressed to start and to end a take, while armed. Its own action (stick clicks: run, reload; A: jump; "
                  "B, Y: weapons; X: reload; grips: grab) rests until you disarm. Hold B or Y + trigger: the trigger "
                  "fires as usual without it."),
        info(motionNote),
        info(motionLastSaved),
        action("Delete Last Take", motion::discardLast)
            .help("Moves the last take saved into motions/discarded (a take that went wrong). Again: the one before."),
        slider("Lead-in", vr_motion_preroll, 0.f, 2.f, 0.1f, "%.1f s").extend()
            .help("Kept from before the take starts (the motion's start, for the melee's trackers)."),
        slider("Tail", vr_motion_tail, 0.f, 1.f, 0.05f, "%.2f s").extend().help("Recorded after the take ends (hits that land late)."),
    };
}

// Review Takes (vr_motion_review.cpp, MOTIONS.md "Reviewing failing takes"): the takes that fail vr_motion_eval or are
// suspect, listed; one picked opens the Take page (its details, the ghost replay, keep, discard, relabel).
[[nodiscard]] za::Vector<Item> pageReviewTake();
[[nodiscard]] int pageIndex(za::Vector<Item> (*build)());
int reviewListGeneration = -1; // the review's generation the pages were built for
int reviewTakeGeneration = -1;
int reviewRelabelCategory = -1;
char reviewListHeader[64];

[[nodiscard]] za::Vector<Item> pageReviewTakes()
{
    reviewListGeneration = motion::review::generation();
    za::Vector<Choice> categories{{-1.f, "All"}};
    const auto& list = motion::categories();
    for(const int i : motion::categoryOrder())
    {
        categories.pushBack({static_cast<float>(i), list[i].choice.display});
    }
    za::Vector<Item> items = {
        info(motion::review::summary),
        info(motion::review::reviewedLine),
        info(motion::review::evalLine),
        cycle("Show", vr_motion_review_show,
            {{0.f, "To Review"}, {1.f, "Failing"}, {2.f, "Suspect"}, {3.f, "Not Evaluated"}, {4.f, "Reviewed"}, {5.f, "All"},
                {6.f, "Discarded"}})
            .help("To Review: failing or suspect, not yet kept. A take picked opens its page: play it as a ghost, "
                  "keep, discard or relabel it."),
        cycle("Category", vr_motion_review_category, ZA_MOVE(categories)),
        action("Re-evaluate Shown", motion::review::reevaluateShown)
            .help("Evaluates the takes listed again, in a second copy of the game in the background (the mock "
                  "headset: yours is untouched), with your settings. About 1.7 s a take."),
        action("Stop Re-evaluation", motion::review::stopReevaluation),
        action("Undo Last", motion::review::undo).help("Takes back the last keep, discard, restore or relabel (again: the "
                                                       "one before)."),
        info(motion::review::lastAction),
    };
    q_strlcpy(reviewListHeader, motion::review::listTitle(), sizeof(reviewListHeader));
    items.pushBack(header(reviewListHeader));
    const int take = pageIndex(pageReviewTake);
    for(int r = 0; r < motion::review::rowCount(); r++)
    {
        items.pushBack(row(motion::review::rowText, motion::review::rowHelp, motion::review::pick, r, take));
    }
    return items;
}

[[nodiscard]] za::Vector<Item> pageReviewTake()
{
    reviewTakeGeneration = motion::review::generation();
    reviewRelabelCategory = static_cast<int>(vr_motion_relabel_category.value);
    za::Vector<Choice> categories;
    const auto& list = motion::categories();
    for(const int i : motion::categoryOrder())
    {
        categories.pushBack({static_cast<float>(i), list[i].choice.display});
    }
    za::Vector<Choice> details;
    const auto& chosen =
        list[CLAMP(0, static_cast<int>(vr_motion_relabel_category.value), static_cast<int>(list.size()) - 1)].details;
    for(size_t i = 0; i < chosen.size(); i++)
    {
        details.pushBack({static_cast<float>(i), chosen[i].display});
    }
    za::Vector<Item> items;
    for(int l = 0; l < motion::review::detailLines && motion::review::detailLine(l)[0]; l++)
    {
        items.pushBack(infoLine(motion::review::detailLine, l));
    }
    const za::Vector<Item> rest = {
        header("Look"),
        action("Play Ghost", motion::review::playGhost)
            .help("Replays it in front of the training dummy as a ghost, looping: its weapons where they were, their "
                  "lines and striking points, the tip's trail, the head, and the events. You are not moved."),
        action("Stop Ghost", motion::review::stopGhost),
        cycle("Ghost Speed", vr_motion_review_speed, {{1.f, "1x"}, {0.5f, "0.5x"}, {0.25f, "0.25x"}, {0.1f, "0.1x"}}),
        action("Replay (Mock Headset)", motion::review::replayMock)
            .help("vr_motion_play watch: the take drives the tracking, the melee plays it again. The mock headset only."),
        header("Decide"),
        action("Keep (Reviewed)", motion::review::keep).help("Marks it reviewed: it leaves To Review. Again: unmarks it."),
        action("Discard", motion::review::discard)
            .help("Moves it into motions/discarded/ (no longer played or counted). Restore or Undo Last bring it back."),
        action("Restore", motion::review::restore).help("A discarded take back into motions/."),
        cycle("Relabel Category", vr_motion_relabel_category, ZA_MOVE(categories)),
        cycle("Relabel Detail", vr_motion_relabel_detail, ZA_MOVE(details)),
        action("Relabel", motion::review::relabel)
            .help("Recorded under the wrong category: renamed to the one above, its header's label changed. The "
                  "original is kept in motions/review/relabelled/ (Undo Last)."),
        action("Undo Last", motion::review::undo),
        info(motion::review::lastAction),
        header("More"),
        action("Next Take", motion::review::nextTake),
        action("Previous Take", motion::review::previousTake),
        action("Re-evaluate This Take", motion::review::reevaluateTake)
            .help("Evaluates it again in a second copy of the game in the background (the mock headset), with your "
                  "settings: a few seconds."),
        info(motion::review::evalLine),
    };
    items.emplaceBackRange(rest.data(), rest.size());
    return items;
}

// The old Single Player and Bot Control menus' extras.
void playCalibration() { Cbuf_AddText("vr_setup\n"); }
void playHub() { Cbuf_AddText("vr_campaign_hub\n"); }
// The tutorial (vrtutorial: Misc/quakevr/maps/vrtutorial_gen.py) on Easy.
void playTutorial() { Cbuf_AddText("skill 0; map vrtutorial\n"); }
void playFiringRange() { Cbuf_AddText("map vrfiringrange\n"); }
// Official Campaigns > Dawn of the Machine: Bloody Nightmare: skill 3 and vr_mg3_bn_start (the start map's first frame
// makes it a Bloody Nightmare game, QC vr_mg3_defs.qc MG3_Frame), then the campaign as its own row starts it. While
// the campaign cannot start, only its reason is printed and nothing is set.
void playMg3BloodyNightmare()
{
    if(VR_CampaignUnavailable(5) == 0)
    {
        Cvar_Set("skill", "3");
        Cvar_Set("vr_mg3_bn_start", "1");
    }
    VR_SelectCampaign(5);
}
void addBotTeam0() { Cbuf_AddText("impulse 100\n"); }
void addBotTeam1() { Cbuf_AddText("impulse 101\n"); }
void kickBot() { Cbuf_AddText("impulse 102\n"); }

// Whether Official Campaigns was built with the Bloody Nightmare row (items(): rebuilt when that changes).
int campaignsBloodyShown = -1;

// Arcane Dimensions: experimental. Chosen in the menu it asks first (M_Confirm), then starts as the developer launch
// does (vr_campaign_native ad: the same game folder, start map and settings).
void startArcaneDimensions() { Con_Printf("VR: Arcane Dimensions confirmed in the menu.\n"); Cbuf_InsertText("vr_campaign_native ad\n"); }
void selectCampaignRow(int index)
{
    if(index != qvr::ad::campaignIndex) { VR_SelectCampaign(index); return; }
    M_Confirm("Arcane Dimensions mode is still\nexperimental and might have bugs.", "Continue", "Back", 0.f,
        startArcaneDimensions);
}

[[nodiscard]] za::Vector<Item> pageCampaigns()
{
    za::Vector<Item> items{header("Official Campaigns")};
    for(int i = 0; i < VR_CampaignCount(); ++i) // (Arcane Dimensions too: its row dim until it is ready)
    {
        Item choice = row(VR_CampaignLabel, VR_CampaignHelp, selectCampaignRow, i, -1);
        choice.dimArg = [](int index) -> bool
        { return index == qvr::ad::campaignIndex ? !VR_CampaignInstalled(index) : VR_CampaignUnavailable(index) != 0; };
        items.pushBack(choice);
    }
    // Dawn of the Machine's hidden difficulty: offered once found in a game (the hell knight's head, or the hub's
    // Bloody Nightmare button: QC sets vr_mg3_bn_discovered).
    campaignsBloodyShown = vr_mg3_bn_discovered.value != 0.f ? 1 : 0;
    if(campaignsBloodyShown)
    {
        Item bloody = action("Dawn of the Machine: Bloody Nightmare", playMg3BloodyNightmare)
            .help("A new Dawn of the Machine game on Bloody Nightmare: skill 3, harder monsters, each level begun with the "
                  "axe and shotgun only. Killing Chthon on it starts its new game.");
        bloody.dimArg = [](int) -> bool { return VR_CampaignUnavailable(5) != 0; };
        items.pushBack(bloody);
    }
    items.pushBack(command("Campaign Data Status", "vr_campaign_status"));
    items.pushBack(action("Return to VR Hub", playHub));
    return items;
}

[[nodiscard]] za::Vector<Item> pagePlay()
{
    return {
        header("Maps"),
        open("Official Campaigns", pageIndex(pageCampaigns)),
        action("VR Calibration", playCalibration)
            .help("The calibration room: height, body and main hand calibrated as you arrive; buttons for the main options."),
        action("VR Hub", playHub),
        action("Tutorial", playTutorial),
        action("Firing Range", playFiringRange),
        command("Melee Recording Room", "map vrmelee-recording"),
        header("Bots (Multiplayer)"),
        action("Add Bot (Team 0)", addBotTeam0),
        action("Add Bot (Team 1)", addBotTeam1),
        action("Kick Bot", kickBot),
    };
}

// The world: monsters, knights' swords, weapon drops and what you feel (the Gameplay page before the menus were
// reorganized; its damage and knockback are on Damage and Knockback, its voice notes on Debug).
[[nodiscard]] za::Vector<Item> pageEnemyWeapons();
[[nodiscard]] za::Vector<Item> pageEnemyShoves();
[[nodiscard]] za::Vector<Item> pageArmedGrunts();
[[nodiscard]] za::Vector<Item> pageKnockdowns();
[[nodiscard]] za::Vector<Item> pageKdTriggers();
[[nodiscard]] za::Vector<Item> pageKdTrigLedge();
[[nodiscard]] za::Vector<Item> pageKdTrigStairs();
[[nodiscard]] za::Vector<Item> pageKdTrigSlope();
[[nodiscard]] za::Vector<Item> pageKdTrigShotgun();
[[nodiscard]] za::Vector<Item> pageKdTrigWounded();
[[nodiscard]] za::Vector<Item> pageKdTrigBlow();
[[nodiscard]] za::Vector<Item> pageKdTrigBlast();
[[nodiscard]] za::Vector<Item> pageHoldingEnemies();
[[nodiscard]] za::Vector<Item> pageHoldingStruggle();
[[nodiscard]] za::Vector<Item> pageEnemyMelee();
[[nodiscard]] za::Vector<Item> pageSound();

#include "vr_menu_mgroups.inc" // Combat > Monster Groups
#include "vr_menu_gameplay.inc" // VR Settings > Gameplay

#include "vr_menu_body.inc" // the body pages

#include "vr_menu_gore.inc" // gore

[[nodiscard]] za::Vector<Item> pageGadget()
{
    return {
        open("Doom", pageIndex(pageDoom)).help("Play Doom on the gadget's screen: a touchscreen under your fingertip."),
        open("HUD and Gadget Arm: VR Settings, Body and Display", pageIndex(pageVrsHud)).help("The HUD's kind is on VR Settings; the gadget's arm on Body and Display."),
        slider("Size", vr_gadget_scale, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.25f, 3.f),
        header("Placement"),
        slider("Along the Arm", vr_gadget_x, -15.f, 15.f, 0.5f, "%.1f cm").extend(-40.f, 40.f),
        slider("Across the Arm", vr_gadget_y, -8.f, 8.f, 0.25f, "%.2f cm").extend(-20.f, 20.f),
        slider("Height", vr_gadget_z, -5.f, 8.f, 0.25f, "%.2f cm").extend(-15.f, 25.f).help("How far it stands out of the forearm."),
        slider("Pitch", vr_gadget_pitch, -90.f, 90.f, 5.f, "%.0f deg").extend(-180.f, 180.f),
        slider("Yaw", vr_gadget_yaw, -90.f, 90.f, 5.f, "%.0f deg").extend(-180.f, 180.f),
        slider("Roll", vr_gadget_roll, -180.f, 180.f, 15.f, "%.0f deg")
            .help("Turns the screen: 90 reads along the arm, 180 turns the text the other way."),
    };
}

// QVR round 21: a game message in the hologram, to see the settings' effect (vr_gadget.cpp).
void hologramTestMessage()
{
    qvr::gadget::testMessage();
}

// Doom on the wrist gadget (vr_doom.cpp, docs/vr-port/DOOM.md): open it, the WAD it plays, its controls.
[[nodiscard]] const char* doomStatusLine()
{
    return doom::menuStatus();
}

[[nodiscard]] const char* doomWadHint()
{
    return "Own DOOM? Drop DOOM.WAD into quakevr/doom/wads";
}

[[nodiscard]] za::Vector<Item> pageDoom()
{
    za::Vector<Item> items = {
        header("Doom on the Wrist Gadget"),
        command("Open or Close Doom", "vr_doom")
            .help("Doom on the gadget's screen instead of the HUD, running beside Quake in real time (Quake's pause "
                  "pauses it; closed, it waits where it was). Touch the screen with your other hand's fingertip to "
                  "play; raise the gadget and look at it to give its stick and buttons to Doom (bindable: vr_doom)."),
        info(doomStatusLine),
        header("WAD"),
        info(doomWadHint),
        row(doom::wadRow, doom::wadHelp, doom::pickWad, -1, -1),
    };
    const int count = doom::wadCount();
    for(int i = 0; i < count; i++)
    {
        items.pushBack(row(doom::wadRow, doom::wadHelp, doom::pickWad, i, -1));
    }
    const Item rest[] = {
        command("Open the WAD Folder", "vr_doom_open_folder")
            .help("Opens quakevr/doom/wads on the desktop: put DOOM.WAD, DOOM1.WAD, DOOM2.WAD, TNT.WAD or PLUTONIA.WAD "
                  "there, then Search Again."),
        command("Search Again", "vr_doom_wads").help("Looks for WADs again (the console lists them: vr_doom_wads)."),
        header("Controls"),
        cycle("Controls", vr_doom_controls, {{0.f, "Touch Only"}, {1.f, "Controller and Touch"}, {2.f, "Controller Only"}})
            .help("Touch Only: everything on the screen with your fingertip; the sticks and buttons stay Quake's. "
                  "Controller and Touch: also, while Doom has the focus (the gadget raised and looked at, or touched), "
                  "the gadget hand's stick moves and turns, trigger fires, grip uses, X next weapon, Y Doom's menu, "
                  "stick press the map; your other hand stays Quake's. Controller Only: no touch controls."),
        cycle("Touch Controls Shown", vr_doom_overlay, {{0.f, "When Your Finger Is Near"}, {1.f, "Always"}, {2.f, "Never"}})
            .help("The buttons drawn over Doom's picture (with Touch Only: always, faint when your finger is away)."),
        toggle("Hard Press Fires", vr_doom_hard_fire)
            .help("Pressing through the screen (Hard Press Depth) while steering with the MOVE stick, or on the "
                  "picture, fires: one finger can move and shoot."),
        toggle("Always Run", vr_doom_always_run).help("Doom's always run (RUN on the touch bar)."),
        slider("Look to Focus", vr_doom_focus_look, 0.f, 2.f, 0.1f, "%.1f s")
            .help("How long you look at the raised gadget before its stick and buttons go to Doom (0: only a touch)."),
        slider("Out of View to Unfocus", vr_doom_focus_grace, 0.1f, 3.f, 0.1f, "%.1f s")
            .help("How long the gadget may be out of view (lowered, looked away from) before they come back to Quake."),
        slider("Light Button Presses", vr_doom_light_presses, 0.f, 10.f, 1.f, "%.0f")
            .help("Press the gadget's side button (the light on/off one) this many times in quick succession to open "
                  "Doom on the gadget; one press of it then closes Doom again, without toggling the light (0: off). "
                  "The light itself keeps toggling with each press and is put back as it was."),
        slider("Light Press Gap", vr_doom_light_gap, 0.1f, 1.f, 0.05f, "%.2f s")
            .help("The longest time between two of those presses."),
        toggle("Pause When Not Focused", vr_doom_pause_unfocused)
            .help("Off: Doom keeps running while you play Quake. On: it pauses until you look at it or touch it."),
        header("Picture and Sound"),
        toggle("4:3 Picture", vr_doom_aspect).help("Doom's picture as it was shown (4:3, bars at the sides); off: stretched over the screen."),
        slider("CRT Look", vr_doom_crt, 0.f, 2.f, 0.1f, "%.1fx").help("Times the gadget's CRT look (0: a clean picture)."),
        slider("Volume", vr_doom_volume, 0.f, 1.f, 0.05f, "%.2f").help("Doom's sounds, from the gadget, against the game's."),
        toggle("Big Floating Panel", vr_doom_panel)
            .help("Doom on a big panel in front of you instead of the gadget (placed as you turn this on, it moves with "
                  "you; touch it as the gadget)."),
        slider("Panel Width", vr_doom_panel_size, 0.2f, 1.5f, 0.05f, "%.2f m"),
        command("Place the Panel in Front", "vr_doom_panel_place"),
        header("Touchscreen"),
        slider("Touch Distance", vr_touch_press, 0.f, 2.f, 0.1f, "%.1f cm")
            .help("How near the screen your fingertip must come for a touch (it must come from in front)."),
        slider("Release Distance", vr_touch_release, 0.3f, 4.f, 0.1f, "%.1f cm").help("How far off it a touch ends."),
        slider("Hard Press Depth", vr_touch_hard, 0.5f, 4.f, 0.1f, "%.1f cm").help("How far through the screen is a hard press."),
        slider("Touch Haptics", vr_touch_haptic, 0.f, 1.f, 0.05f, "%.2f").help("Your hand's buzz as a touch begins."),
    };
    for(const Item& i : rest)
    {
        items.pushBack(i);
    }
    return items;
}

// Split from Wrist Gadget (and Immersion): the wrist gadget's screen, the weapons' ammo screens and
// the maps' text boards.
// Tips (vr_tips.cpp): for new players, each shown once near what it is about.
[[nodiscard]] za::Vector<Item> pageTips()
{
    return {
        cycle("Tips", vr_tips, {{0.f, "Off"}, {1.f, "Floating Panel"}, {2.f, "Wrist Gadget"}})
            .help("Tips for new players, each shown once: the first time you come near something you can use (a wall "
                  "torch). Floating Panel: a screen like the maps' text boards by it, with a cable to it. Wrist Gadget: in the gadget's hologram, waiting until "
                  "you look at it (it chimes and buzzes); the panel if the HUD is not the gadget."),
        slider("Distance", vr_tips_distance, 50.f, 400.f, 10.f, "%.0f").extend(16.f, 2000.f)
            .help("How near you must come to what a tip is about (in Quake units: about 40 a metre)."),
        toggle("Line of Sight", vr_tips_line_of_sight).help("Only when no wall is between your eyes and it."),
        slider("View Angle", vr_tips_view_angle, 0.f, 90.f, 5.f, "%.0f deg").extend(0.f, 180.f)
            .help("How far from where you look it may be (0: anywhere, even behind you)."),
        slider("Delay", vr_tips_delay, 0.f, 3.f, 0.25f, "%.2f s").extend(0.f, 10.f)
            .help("How long it must stay near and seen before the tip shows."),
        slider("Time Shown", vr_tips_time, 3.f, 30.f, 1.f, "%.0f s").extend(1.f, 120.f)
            .help("How long a tip shows (on the gadget: once you look at it)."),
        cycle("Panel Facing", vr_tips_facing, {{0.f, "Towards Your Eyes"}, {1.f, "Square to Your View"}})
            .help("Towards your eyes: the floating screen turns to face you and stays level. Square to your view: it is "
                  "always flat in front of you, as if on your view, tilting as you tilt your head."),
        slider("Panel Text Size", vr_tips_size, 0.5f, 2.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How big the writing on a tip's floating panel is."),
        command("Show Tips Again", "vr_tips_reset").help("Every tip as never shown: each shows again the next time."),
        command("Show the Torch Tip Now", "vr_tips_test walltorch")
            .help("The wall torch tip on the nearest wall torch in view, as soon as you close the menu (however far; it is "
                  "not counted as shown): to try the two ways and these settings."),
        command("List This Map's Tips", "vr_tips_test list")
            .help("Names every tip available here: the built-in ones and this map's func_vr_tip placed by hand. "
                  "vr_tips_test <name> shows one of them now, however far, to try it."),
        open("Wrist Gadget Messages", pageIndex(pageScreens)).help("The hologram's size, height and look (HUD and Menus > Screens)."),
    };
}

[[nodiscard]] za::Vector<Item> pageScreens()
{
    return {
        header("Wrist Gadget"),
        toggle("Level and Stats", vr_gadget_show_level),
        toggle("Hide Gear When Dead", vr_dead_hide_gear)
            .help("While you are dead the holstered guns, the ammo pouch, the grenade pouch and the wrist gadget are hidden "
                  "(the flashlight is, always) and the HUD is "
                  "Quake's status bar on a hand; all back when you respawn or load a save."),
        toggle("Stamina and Counters", vr_gadget_stamina)
            .help("The STAMINA row shows your stamina (parries, shoves and blows spend it) and COUNTER while a counter-attack's window is open."),
        slider("Screen Light", vr_gadget_light, 0.f, 3.f, 0.1f, "%.1fx").extend()
            .help("The screen casts a light in its colour the way it faces, and a faint one on your hand (0 off)."),
        header("Gear Lights: the Gadget's Side Button"),
        toggle("Gear Lights", vr_gear_lights)
            .help("Off: your gear's lights dim, for sneaking about in the dark: the light the gadget's and your guns' "
                  "screens cast (monsters see you by it) and their glow, and the screens themselves. The button on the "
                  "gadget's lower edge (the inner one) toggles this with a click; bindable: vr_gear_lights_toggle. The "
                  "flashlight keeps its own switch."),
        slider("Lights When Dimmed", vr_gear_lights_dim, 0.f, 0.5f, 0.01f, "%.2fx").extend(0.f, 1.f)
            .help("While off: the screens' cast light and glow at this share (0: none at all)."),
        slider("Screens When Dimmed", vr_gear_lights_screen_dim, 0.05f, 1.f, 0.05f, "%.2fx").extend(0.f, 1.f)
            .help("While off: the screens' text, numbers and the hologram at this share of their brightness, readable "
                  "up close."),
        toggle("Side Button", vr_gadget_button)
            .help("The button on the gadget's lower edge, pressed with your other hand's fingertip, toggles the gear "
                  "lights."),
        slider("Button Size", vr_gadget_button_size, 1.f, 8.f, 0.25f, "%.2f cm").extend(0.3f, 20.f)
            .help("How near the button's middle your fingertip must come (from the button's side: never from over the "
                  "screen). Show the Button shows it."),
        slider("Button Across", vr_gadget_button_x, -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("The button's hit spot moved along the screen's width (positive: to the screen's right)."),
        slider("Button Up", vr_gadget_button_y, -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("Moved along the screen's height (negative: further out past its lower edge)."),
        slider("Button Out", vr_gadget_button_z, -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("Moved out of the screen's face (negative: down towards your arm)."),
        slider("Button Tilt Out", vr_gadget_button_pitch, -90.f, 90.f, 5.f, "%.0f deg")
            .help("The side the button is pressed from (the faint disc: no press from behind it) tilted out of the "
                  "screen's face (negative: towards your arm)."),
        slider("Button Tilt Across", vr_gadget_button_yaw, -90.f, 90.f, 5.f, "%.0f deg")
            .help("... and along the screen's width (positive: to its right)."),
        toggle("Drawn Fingertip", vr_gadget_fingertip_drawn)
            .help("On: the fingertip that presses the button is your drawn hand's index fingertip, as the hand is posed "
                  "(point with it). Off: Fingertip Reach ahead of your hand's point."),
        slider("Fingertip Reach", vr_gadget_button_reach, -5.f, 15.f, 0.5f, "%.1f cm").extend(-20.f, 30.f)
            .help("With Drawn Fingertip off (or no jointed hand): where your fingertip is taken to be, ahead of your "
                  "hand's point."),
        slider("Fingertip Forward", vr_gadget_fingertip_x, -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("The fingertip moved along your hand's forward (negative: back). Show the Button shows it: the drawn "
                  "fingertip white, joined to the one that presses."),
        slider("Fingertip Outward", vr_gadget_fingertip_y, -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("... away from your other hand (the right hand's right; negative: towards it)."),
        slider("Fingertip Up", vr_gadget_fingertip_z, -6.f, 6.f, 0.25f, "%.2f cm").extend(-20.f, 20.f)
            .help("... and up from the back of your hand (negative: down)."),
        slider("Fingertip Pitch", vr_gadget_fingertip_pitch, -45.f, 45.f, 1.f, "%.0f deg").extend(-180.f, 180.f)
            .help("The fingertip turned round your hand's point: down (negative: up), before the moves above."),
        slider("Fingertip Yaw", vr_gadget_fingertip_yaw, -45.f, 45.f, 1.f, "%.0f deg").extend(-180.f, 180.f)
            .help("... inward, towards your other hand (negative: outward)."),
        slider("Fingertip Roll", vr_gadget_fingertip_roll, -45.f, 45.f, 1.f, "%.0f deg").extend(-180.f, 180.f)
            .help("... rolled about your hand's forward (positive: its top outward)."),
        slider("Button Cooldown", vr_gadget_button_cooldown, 0.f, 2.f, 0.1f, "%.1f s").extend(0.f, 5.f)
            .help("After a press counts, how long before the next one does (no double toggles from a bounce)."),
        cycle("Show the Button", vr_debug_gadget_button,
            {{0.f, "Off"}, {1.f, "Its Hit Volume"}, {2.f, "And the Screen Tap"}, {3.f, "And Print Sync"}})
            .help("Draws the button's hit volume (green ready, yellow pressed, red cooling down; the faint disc: no "
                  "press from behind it, the short line the side it is pressed from) and your fingertip (the drawn "
                  "index fingertip white, joined to the tuned one that presses); And the Screen Tap: also bullet time's "
                  "tap zone over the screen and what strikes it (your hand's surface, the gun's butt). Presses are "
                  "printed; And Print Sync also prints, each frame, how far the zone is from the drawn gadget."),
        slider("CRT Look", vr_gadget_crt, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Scanlines, a slight flicker, faint static and now and then a glitch (0 off)."),
        slider("Screen Glow", vr_screen_glow, 0.f, 3.f, 0.1f, "%.1fx").extend()
            .help("The gadget's and your weapons' screens glow softly round their edges (0 off)."),
        slider("Text Glow", vr_screen_text_glow, 0.f, 3.f, 0.1f, "%.1fx").extend()
            .help("The text, numbers and icons on those screens glow: bright whitish cores, a soft halo (0 off)."),
        header("Messages"),
        toggle("Game Messages as Hologram", vr_messages_hologram)
            .help("The game's messages (a key needed, a secret found, the map's text, pickups) float as a hologram the gadget projects over its screen, while you look at it. Else the key and map messages show in front of you."),
        slider("Hologram Time", vr_messages_hologram_time, 2.f, 15.f, 0.5f, "%.1f s").extend().help("How long a message stays in the hologram."),
        slider("Hologram Text Size", vr_messages_hologram_size, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.25f, 4.f),
        slider("Hologram Height", vr_messages_hologram_height, 0.f, 15.f, 0.5f, "%.1f cm").extend(0.f, 50.f)
            .help("How high over the gadget it floats."),
        slider("Hologram Effect", vr_messages_hologram_effect, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("The beam of light from the screen, scanlines, flicker, glitches and the projection as it appears (0: plain glowing text)."),
        action("Show a Test Message", hologramTestMessage)
            .help("One of the game's messages in the hologram, as they come: raise the gadget to see it while you change these settings. Press again for another: they stack."),
        toggle("Messages Only on the Gadget", vr_messages_hologram_only)
            .help("The game's messages never show in front of you: they wait in the hologram until you look at the gadget. A new one (a key needed, a secret, the map's text; not pickups) chimes from the gadget on your wrist and buzzes it."),
        // (The author's note vrstart_2026-10-23_01-23-46: VR Calibration's texts jumped to each turn of the head.)
        slider("Messages in View: Follow Smoothing", vr_view_text_smooth, 0.f, 0.5f, 0.02f, "%.2f s").extend(0.f, 2.f)
            .help("The messages floating in front of you (VR Calibration's and the maps' texts, Body Calibration's poses, "
                  "the game's messages when not on the gadget) turn after your head this smoothly: about how long they "
                  "take to catch up. 0: fixed to your view."),
        toggle("Console Log on the HUD", vr_hud_console_log)
            .help("The console's log (the engine's own lines: warnings, settings changed, command output) shows with the game's messages, over the gadget or in view (and at the top of the flat screen). Off: only the game's messages (pickups, deaths, chat); the console (~) keeps everything."),
        cycle("Console Messages", vr_notify_wrist, {{1.f, "Over the gadget"}, {2.f, "Both"}, {0.f, "In view"}})
            .help("The console's other messages (the engine's: settings changed, cheats, errors) float in a small log over the gadget, or at the top of the view."),
        slider("Console Message Time", vr_notify_wrist_time, 2.f, 30.f, 1.f, "%.0f s").extend()
            .help("How long a message stays in the gadget's log."),
        slider("Console Log Height", vr_notify_wrist_height, 0.f, 20.f, 0.5f, "%.1f cm").extend(0.f, 60.f)
            .help("How high over the gadget the log floats (always over the hologram)."),
        slider("Console Log Brightness", vr_notify_wrist_alpha, 0.2f, 1.f, 0.05f, "%.2f").extend(),
        header("Weapons' Ammo Screens"),
        toggle("Weapon Text", "vr_show_weapon_text").help("Show floating ammunition text attached to weapons"),
        toggle("Weapon Ammo Screen", "vr_weapon_screen").help("The ammunition text on a small screen on the weapon (colours from the wrist gadget's screen)."),
        slider("Ammo Screen Margin", "vr_weapon_screen_padding", 0.f, 2.f, 0.1f, "%.1f").extend(),
        slider("Ammo Screen CRT Look", "vr_weapon_screen_crt", 0.f, 2.f, 0.1f, "%.1fx").extend().help("Scanlines, a slight flicker, faint static and now and then a glitch, as on the wrist gadget's screen (0 off)."),
        toggle("White Ammo Screen Text", "vr_ammo_screen_text_white").help("The numbers on the ammo screens (and the ammo pouch's counter) near-white, as the wrist gadget's, for readability; their frame and glow keep the screen's colour. Off: in the screen's colour."),
        toggle("Screens on Weapons at Rest", "vr_weapon_screen_idle").help("Weapons in your holsters and lying in the world show their ammo screen and button too, not only the ones in your hands."),
        slider("Lying Weapons' Parts Range", "vr_weapon_world_attach_range", 250.f, 5000.f, 250.f, "%.0f units").extend(0.f, 20000.f)
            .help("How far off a weapon lying in the world still shows its magazine, ammo screen and button (Quake units: 32 is about a metre)."),
        slider("Lying Weapons With Parts", "vr_weapon_world_attach_max", 0.f, 48.f, 1.f, "%.0f")
            .help("How many of the weapons lying nearest show their magazine, ammo screen and button."),
        header("Map Boards"),
        toggle("Map Boards as CRTs", "vr_worldtext_crt").help("The text boards in maps (the tutorial's, the start map's) are CRT screens with glowing text, as the wrist gadget's. Off: plain text."),
        slider("Map Board Hue", "vr_worldtext_hue", 0.f, 355.f, 5.f, "%.0f").help("Their colour: 40 amber, 128 green, 200 blue, 0 red."),
    };
}

// Split from Wrist Gadget: the colours of the player's effects.
[[nodiscard]] za::Vector<Item> pageColours()
{
    return {
        slider("Player Effects Hue", vr_player_hue, 0.f, 355.f, 5.f, "%.0f")
            .help("One colour for all your effects: the wrist gadget's screen, your weapons' screens and sights, the force grab, the teleport arc, the crosshair, the menu laser. 128 green, 40 amber, 200 blue, 0 red."),
        slider("Player Effects Saturation", vr_player_saturation, 0.f, 2.f, 0.05f, "%.2f")
            .help("How colourful they are: 1 as made, 0 white."),
        header("Wrist Gadget"),
        hueSlider("Screen Hue", vr_gadget_screen_hue)
            .help("The screen's colour (and your weapons' screens'). Player's: the Player Effects Hue."),
        slider("Screen Brightness", vr_gadget_screen_brightness, 0.3f, 1.5f, 0.05f, "%.2f").extend(),
        slider("Screen Background", vr_gadget_screen_background, 0.f, 4.f, 0.1f, "%.1f").extend()
            .help("How bright the screen's dark background is in its colour (the text reads best dark: 1)."),
        slider("Screen Text Whiteness", vr_gadget_screen_text_white, 0.f, 1.f, 0.05f, "%.2f")
            .help("How white the screen's numbers and values are: 1 white, 0 the screen's colour (as its labels). Warnings stay red."),
        slider("Casing Tint", vr_gadget_tint, 0.f, 1.f, 0.05f, "%.2f").help("0 keeps the casing's own olive drab."),
        slider("Casing Tint Hue", vr_gadget_tint_hue, 0.f, 355.f, 5.f, "%.0f"),
        header("Effects"),
        hueSlider("Weapon Sight Hue", vr_sight_hue)
            .help("The glowing iron sights of the shotgun and double shotgun: 30 their own orange, 0 red, 120 green, 240 blue. Player's: the Player Effects Hue."),
        slider("Weapon Sight Saturation", vr_sight_saturation, 0.f, 2.f, 0.05f, "%.2f").help("1 their own, 0 white."),
        hueSlider("Force Grab Hue", vr_forcegrab_hue)
            .help("The force grab's beam, energy tendril, glow and sparkles: 215 its old blue. Player's: the Player Effects Hue."),
        slider("Force Grab Saturation", vr_forcegrab_saturation, 0.f, 2.f, 0.05f, "%.2f")
            .help("How colourful they are: 1 as made, 0 white, 0.7 like the wrist gadget's screen. Times the Player Effects Saturation."),
        hueSlider("Teleport Arc Hue", vr_teleport_hue)
            .help("Where the teleport arc can land (red where it can't): 220 its old blue. Player's: the Player Effects Hue."),
        hueSlider("Crosshair Hue", vr_crosshair_hue).help("The laser crosshair: 0 its old red. Player's: the Player Effects Hue."),
        hueSlider("Menu Laser Hue", vr_menu_laser_hue).help("The menu's pointer: 35 its old amber. Player's: the Player Effects Hue."),
    };
}

#include "vr_menu_physics.inc" // throwing, carrying, burning, crates, gibs, the ragdoll pages

// Debug: playtesting and debugging aids, every debug flag and one-shot debug command reachable without the console
// (ROUND21.md, "Menus reorganized" gathered the first ones; "Debug menu; quad sound; grenade catch default; no
// empty-hand deflection" the rest). A new debug setting goes on the part it belongs to, one line each:
// - Views: drawn in the world (show_*, vr_debug_* that draw);
// - Logging: printed to the console (and the wrist log), or written to a trace file every frame;
// - Profiling and Memory: timings and memory;
// - Reports: a command printing something once (command("Label", "vr_something")), what it prints in its help;
// - Tools: a command rebuilding, reloading, writing a file or showing a test effect;
// - Tests: what tests are done with in the headset (a monster ahead, a projectile at you, cheats).
// Mock-headset commands (vr_mock_*) and the automated tests' settings (vr_fixed_frames, vr_particle_seed,
// sv_random_seed...) stay in the console: they mean nothing in the headset.
// Checklist (vr_checklist.hpp; the corner's "Checklist" button, Debug > Checklist): what to test in the headset or give
// feedback on, from quakevr/checklist.txt, each item ticked by picking it; its long texts on the lines under it.
int checklistGeneration = -1; // the list's generation the page was built for
int checklistHidden = -1;     // and Hide Ticked's

[[nodiscard]] const char* checklistSummary()
{
    char(&text)[48] = readouts.checklistSummary;
    if(!checklist::loaded())
    {
        return "No quakevr/checklist.txt";
    }
    if(checklist::failedCount() > 0)
    {
        q_snprintf(text, sizeof(text), "%d open of %d, %d failed", checklist::openCount(), checklist::itemCount(),
            checklist::failedCount());
    }
    else
    {
        q_snprintf(text, sizeof(text), "%d open of %d", checklist::openCount(), checklist::itemCount());
    }
    return text;
}

// The test running (vr_scenario.hpp): its id, or none.
[[nodiscard]] const char* checklistTesting()
{
    char(&text)[48] = readouts.checklistTesting;
    if(!*scenario::current())
    {
        return "No test running";
    }
    q_snprintf(text, sizeof(text), "Testing: %s", scenario::current());
    return text;
}

[[nodiscard]] const char* checklistBlank()
{
    return " "; // (the gap between two entries)
}

[[nodiscard]] const char* checklistEmpty()
{
    return checklist::itemCount() > 0 ? "All ticked." : "Nothing on the list.";
}

[[nodiscard]] const char* checklistLine(int arg)
{
    return checklist::line(arg >> 8, arg & 255);
}

[[nodiscard]] bool checklistDim(int arg)
{
    return checklist::ticked(arg >> 8);
}

[[nodiscard]] int checklistMark(int arg)
{
    const int m = checklist::mark(arg >> 8);
    return m == checklist::Passed ? 1 : m == checklist::Failed ? 2 : 0;
}

[[nodiscard]] const char* checklistHelp(int arg)
{
    const int item = arg >> 8;
    const int m = checklist::mark(item);
    za::String& text = readouts.checklistItemHelp;
    text = m == checklist::Passed ? "Passed. " : m == checklist::Failed ? "Failed: kept on the list until it is replaced. " : "";
    text += "Pick Pass, Fail or Test under it (left and right, or point at one); Pass or Fail again puts it back to "
            "untested. ";
    const char* id = checklist::scenario(item);
    if(*id)
    {
        text += "Test sets up its scene (";
        text += id;
        text += ") and puts you in it; Reset Test above runs it again.";
    }
    else
    {
        text += "No test scene for this one.";
    }
    return text.cStr();
}

// The buttons under each item: Pass, Fail, Test.
[[nodiscard]] const char* checklistButtonLabel(int, int b)
{
    return b == 0 ? "Pass" : b == 1 ? "Fail" : "Test";
}

[[nodiscard]] bool checklistButtonDim(int arg, int b)
{
    return b == 2 && !*checklist::scenario(arg >> 8);
}

[[nodiscard]] int checklistButtonTint(int arg, int b)
{
    const int m = checklist::mark(arg >> 8);
    return b == 0 && m == checklist::Passed ? 1 : b == 1 && m == checklist::Failed ? 2 : 0;
}

[[nodiscard]] const char* checklistButtonsText(int)
{
    return "Pass  Fail  Test"; // (what a search or a dump of the page shows)
}

void checklistButtonPress(int arg, int b)
{
    const int item = arg >> 8;
    Con_DPrintf("checklist: item %d, %s pressed\n", item, checklistButtonLabel(arg, b));
    if(b < 2)
    {
        checklist::press(item, b == 0 ? checklist::Passed : checklist::Failed);
        return;
    }
    if(scenario::start(checklist::scenario(item), checklist::text(item)))
    {
        menuui::backToGame(1); // (the main hand's buzz; the menu reopens here)
    }
}

void checklistResetTest()
{
    if(scenario::reset())
    {
        menuui::backToGame(1);
    }
}

void checklistEndTest()
{
    scenario::stop();
}

void checklistReload()
{
    checklist::refresh(true);
}

// Undo Last Tick: the session's ticks and unticks taken back one at a time, the last first (checklist::undo); its help
// names the item the next one changes, and the one it changed last (found again with Hide Ticked off).
[[nodiscard]] bool checklistUndoDim(int)
{
    return checklist::undoCount() == 0;
}

[[nodiscard]] const char* checklistUndoHelp(int)
{
    za::String& text = readouts.checklistUndoHelp;
    char count[48];
    q_snprintf(count, sizeof(count), "%d to undo. ", checklist::undoCount());
    text = checklist::undoCount() == 0 ? "Nothing to undo: each tick and untick made since the game started can be "
                                         "taken back here, the last first."
                                       : count;
    const int restores = checklist::undoRestores();
    text += checklist::undoCount() == 0               ? ""
            : restores == checklist::Passed ? "Next makes it passed again: "
            : restores == checklist::Failed ? "Next makes it failed again: "
                                            : "Next makes it untested again: ";
    if(checklist::undoCount() > 0)
    {
        text += checklist::undoText();
    }
    if(!pageTexts.checklistUndone.empty())
    {
        text += checklist::undoCount() > 0 ? " (Last undone: " : " Last undone: ";
        text += pageTexts.checklistUndone;
        text += checklist::undoCount() > 0 ? ")" : "";
    }
    return text.cStr();
}

void checklistUndo()
{
    const za::String what = checklist::undoText();
    if(checklist::undo() == -1)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    pageTexts.checklistUndone = what;
    S_LocalSound("misc/menu3.wav");
}

[[nodiscard]] za::Vector<Item> pageChecklist()
{
    checklist::refresh();
    checklistGeneration = checklist::generation();
    checklistHidden = vr_checklist_hide_ticked.value != 0.f ? 1 : 0;

    // The headers' names, one for each run of items under a section, where the items point (all copied before any is
    // pointed at).
    const int n = checklist::itemCount();
    za::Vector<za::String>& names = pageTexts.checklistSections;
    names.clear();
    for(int i = 0; i < n; i++)
    {
        if(i == 0 || checklist::sectionOf(i) != checklist::sectionOf(i - 1))
        {
            names.emplaceBack(checklist::sectionName(checklist::sectionOf(i)));
        }
    }

    za::Vector<Item> items = {
        info(checklistSummary),
        toggle("Hide Ticked", vr_checklist_hide_ticked).help("Leaves the passed items out of the list. Off: every item, the passed ones dimmed."),
        action("Reload List", checklistReload)
            .help("Reads quakevr/checklist.txt again (done by itself too when the file changes). Passes and fails are "
                  "kept by each item's text, in quakevr/checklist_ticks.txt."),
        info(checklistTesting),
        action("Reset Test", checklistResetTest)
            .help("Runs the last test scene again from the start (the map loaded fresh, everything placed again), and "
                  "closes the menu. Bind a key to vr_scenario reset for the same."),
        action("End Test", checklistEndTest)
            .help("Ends the test scene: the settings it changed put back (also done by the next test, another map, a "
                  "load, or quitting). You stay where you are."),
    };
    Item undo = action("Undo Last Tick", checklistUndo);
    undo.helpArg = checklistUndoHelp;
    undo.dimArg = checklistUndoDim;
    items.insert(items.begin() + 2, undo); // (under Hide Ticked)
    int run = -1;
    bool headerDue = false;
    int shown = 0;
    for(int i = 0; i < n && i < (1 << 20); i++)
    {
        if(i == 0 || checklist::sectionOf(i) != checklist::sectionOf(i - 1))
        {
            run++;
            headerDue = !names[run].empty();
        }
        if(checklistHidden && checklist::ticked(i))
        {
            continue;
        }
        if(headerDue)
        {
            headerDue = false;
            items.pushBack(header(names[run].cStr()));
        }
        if(shown > 0)
        {
            items.pushBack(info(checklistBlank)); // vertical padding: long entries no longer run into a wall of text
        }
        shown++;
        const int lines = q_min(checklist::lineCount(i), 255);
        for(int l = 0; l < lines; l++)
        {
            Item r = row(checklistLine, checklistHelp, nullptr, (i << 8) | l, -1);
            r.dimArg = checklistDim;
            r.markArg = checklistMark;
            r.partOf = l;
            if(l == 0)
            {
                r.buttons = 3;
                r.buttonPress = checklistButtonPress;
                r.buttonDim = checklistButtonDim;
            }
            items.pushBack(r);
        }
        // Its buttons, under it (part of it: selected with it).
        Item b = row(checklistButtonsText, checklistHelp, nullptr, (i << 8) | lines, -1);
        b.partOf = lines;
        b.buttons = 3;
        b.buttonLabel = checklistButtonLabel;
        b.buttonDim = checklistButtonDim;
        b.buttonTint = checklistButtonTint;
        items.pushBack(b);
    }
    if(shown == 0)
    {
        items.pushBack(info(checklistEmpty));
    }
    return items;
}

#include "vr_menu_debug.inc" // Debug

#include "vr_menu_debug_tests.inc" // Debug > Tests (and the spawn, horde and MG3 test pages)

[[nodiscard]] za::Vector<Item> pageForceGrab()
{
    return {
        toggle("Force Grab", vr_forcegrab_mode).help("Pull pickups and weapons to your hand from afar."),
        slider("Distance", vr_forcegrab_distance, 100.f, 1500.f, 25.f, "%.0f").extend(100.f, 4000.f),
        slider("Aim Cone", vr_forcegrab_cone, 3.f, 45.f, 1.f, "%.0f deg").extend().help("How far off where the hand points an object may be."),
        toggle("Bodies Block", vr_forcegrab_bodies_block)
            .help("An enemy (or a body) between your hand and an object hides it from the force grab, as a wall does. "
                  "Off: only walls do."),
        slider("Flick Speed", vr_forcegrab_flick_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend()
            .help("How fast the hand moves back or up to pull."),
        slider("Flick Turn", vr_forcegrab_flick_turn, 50.f, 800.f, 25.f, "%.0f deg/s").extend()
            .help("Or how fast the fingers swing back or up."),
        slider("Flight Time", vr_forcegrab_time, 0.15f, 1.f, 0.05f, "%.2f s").extend(),
        slider("Flight Speed", vr_forcegrab_speed, 300.f, 3000.f, 100.f, "%.0f").extend()
            .help("Units per extra second of flight: longer pulls fly longer."),
        slider("Arc Height", vr_forcegrab_arc, 0.f, 0.5f, 0.05f, "%.2f").extend(),
        slider("Catch Radius", vr_forcegrab_catch_radius, 4.f, 32.f, 1.f, "%.0f").extend(),
        slider("Catch Early", vr_forcegrab_catch_early, 0.05f, 1.f, 0.05f, "%.2f s").extend()
            .help("How long before it arrives the grip may close to catch it."),
        slider("Catch Late", vr_forcegrab_catch_late, 0.f, 0.5f, 0.05f, "%.2f s").extend(),
        slider("Catch Blend Time", vr_forcegrab_catch_blend, 0.f, 0.5f, 0.05f, "%.2f s").extend()
            .help("A caught weapon eases from how it flew into your hand over this time, as one drawn from a holster (0: at "
                  "once). It fires at once: only its look eases."),
        toggle("Pointing Particles", vr_forcegrab_eligible_particles).help("Sparkles on the object an empty hand points at, that it can pull."),
        toggle("Pointing Haptics", vr_forcegrab_eligible_haptics).help("A tick in the hand when it points at a new object it can pull."),
        slider("Outline", "vr_forcegrab_outline", 0.f, 2.f, 0.1f, "%.1f").extend().help("The soft glow round the object a hand points at (0 off)."),
        toggle("Effects", "vr_forcegrab_fx").help("A faint beam to what you point at, a crackling tendril when locked on, a trail behind what flies to you."),
        slider("Ammo/Health Box Size", vr_forcegrabbable_box_scale, 0.1f, 1.f, 0.05f, "%.2f").extend()
            .help("Takes effect on the next map."),
    };
}

// The grappling gun's own Weapon Offsets setting (its button's place), for its page; none without its slot.
[[nodiscard]] cvar_t* grappleButton(weapons::Key key)
{
    const int slot = weapons::slotForName("progs/v_grpple.mdl");
    return slot >= 0 ? weapons::cvar(slot, key) : nullptr;
}

[[nodiscard]] za::Vector<Item> pageGrapple()
{
    return {
        cycle("Rope", vr_grapple_rope, {{1.f, "Holds"}, {0.f, "Pulls at once"}})
            .help("Holds: the hook bites and the rope holds you at its length (swing on it, walk closer), pulling nothing "
                  "until you hold B or Y (either hand's, when the other has no hook of its own out); A or X in the air "
                  "pays it out. The hook stays in when you drop the gun, holster it or pass it to the other hand. "
                  "Pulls at once: the mission pack's grapple, pulling you in as soon as it bites."),
        cycle("Trigger Released", vr_grapple_trigger_release,
            {{0.f, "Hook Stays In"}, {1.f, "Hook Comes Back"}, {2.f, "Hook Comes Loose"}})
            .help("Stays in: letting go of the trigger does nothing; the back button on the gun (over the grip) detaches "
                  "the hook, the front one (near the muzzle) reels it straight in. Comes back: letting go brings it "
                  "straight into the gun. Comes loose: letting go takes it off, hanging on the rope until you reel it in "
                  "(B or Y)."),
        slider("Drop Grace", vr_grapple_drop_grace, 0.f, 0.5f, 0.02f, "%.2f s").extend(0.f, 2.f)
            .help("Comes Back and Comes Loose: the trigger let go waits this long before the hook comes off: dropping the "
                  "gun, holstering it or passing it to the other hand in that time (letting go of trigger and grip "
                  "together) keeps the hook in."),
        slider("Longest Rope", vr_grapple_max_length, 200.f, 3000.f, 50.f, "%.0f").extend(64.f, 10000.f)
            .help("The rope is never longer (units): the hook flies no farther, the unreel pays out no more."),
        toggle("Shoot the Hook Off", vr_grapple_shootable)
            .help("A hook in a wall, a floor or a door comes off when you shoot it (any weapon): get it back when it is "
                  "stuck or its gun is out of reach."),
        slider("Reel-In Button Time", vr_grapple_quick_time, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 10.f)
            .help("How long the front button takes to reel the hook in: it speeds up, then slows down into the gun (a "
                  "short rope quicker). 0: it flies straight back at the Reel-In Button Speed."),
        slider("Reel-In Button Speed", vr_grapple_quick_speed, 300.f, 4000.f, 100.f, "%.0f u/s").extend(100.f, 10000.f)
            .help("The fastest the hook comes back when you press the front button (a long rope takes longer than the "
                  "Reel-In Button Time), and its speed with that time at 0."),
        slider("Hook Size", vr_grapple_hook_scale, 0.3f, 1.f, 0.05f, "%.2f").extend(0.2f, 2.f)
            .help("The size of the hook the gun fires (1: the mission pack's, too big for the gun). From the next shot."),

        header("Buttons on the Gun"),
        slider("Back Button Along", grappleButton(weapons::Key::WpnButtonX), -20.f, 20.f, 0.1f, "%.1f").extend(-60.f, 60.f)
            .help("Where the back button (the detach) is on the gun: towards the muzzle (model units; the grappling gun's "
                  "Weapon Offsets button place). The front button moves with it."),
        slider("Back Button Side", grappleButton(weapons::Key::WpnButtonY), -10.f, 10.f, 0.1f, "%.1f").extend(-40.f, 40.f)
            .help("Towards the gun's left (model units)."),
        slider("Back Button Up", grappleButton(weapons::Key::WpnButtonZ), -10.f, 10.f, 0.1f, "%.1f").extend(-40.f, 40.f)
            .help("Up (model units)."),
        toggle("Front Button", vr_grapple_front_button)
            .help("The gun's second button, near the muzzle: press it with the other hand's finger and the hook comes "
                  "straight back into the gun (through anything). The back button, over the grip, only detaches it."),
        slider("Front Button Along", vr_grapple_front_button_x, -20.f, 30.f, 0.1f, "%.1f").extend()
            .help("Where the front button is: from the back button along the gun, towards the muzzle (model units)."),
        slider("Front Button Side", vr_grapple_front_button_y, -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("From the back button, towards the gun's left (model units)."),
        slider("Front Button Up", vr_grapple_front_button_z, -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("From the back button, up (model units)."),

        header("Rope"),
        toggle("Physical Rope", vr_grapple_rope_sim)
            .help("The rope wraps round the world and the props (a pillar's edge, a box's top), never through them, and "
                  "pulls along where it lies; slack, it is drawn hanging from the corners it wraps and lying on the "
                  "world. Off: a straight line, drawn sagging."),
        slider("Rope Point Spacing", vr_grapple_rope_spacing, 4.f, 48.f, 1.f, "%.0f").extend(2.f, 256.f)
            .help("Units between the points of the rope as drawn hanging (the physical rope): finer bends more smoothly "
                  "and costs more (128 points at most)."),
        slider("Rope Precision", vr_grapple_rope_iterations, 1.f, 32.f, 1.f, "%.0f").extend(1.f, 64.f)
            .help("Passes a frame holding the drawn rope's pieces to their length: more is stiffer and truer, and costs "
                  "more."),
        slider("Rope Thickness", vr_grapple_rope_radius, 0.25f, 4.f, 0.25f, "%.2f").extend(0.1f, 8.f)
            .help("Half the rope's thickness (units): how far its corners keep from the edges it wraps round, and the "
                  "drawn rope from what it lies on."),
        slider("Rope Straight Out of the Hook", vr_grapple_rope_tail, 0.f, 12.f, 0.5f, "%.1f").extend(0.f, 16.f)
            .help("How far the drawn rope goes straight out of the hook's back before it bends (units, at the hook's full "
                  "size): its last pieces point into the hook, not across it. 0: it bends right at the hook."),
        slider("Rope Depth in the Gun", vr_grapple_rope_depth, 0.f, 8.f, 0.5f, "%.1f").extend(-1.f, 16.f)
            .help("How far inside the muzzle the rope is drawn from (units), in the hand and with the gun lying about. "
                  "-1: the old places."),

        header("What Hangs on the Rope"),
        slider("Walking Shared", vr_grapple_move_share, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of your walking (the thumbstick) the hook or a hooked prop hanging on the rope takes: it "
                  "trails a little behind you instead of lagging far back and snapping forward. Lying on the floor, it "
                  "is left alone. 0: none."),
        slider("Air Drag", vr_grapple_load_drag, 0.f, 5.f, 0.1f, "%.1f").extend()
            .help("How fast what swings on the rope slows down (a share of its speed a second, relative to you): a gun "
                  "left hanging stops swinging, a flail can't wind up without end."),
        slider("Hanging Air Drag", vr_grapple_hang_drag, 0.f, 10.f, 0.25f, "%.2f").extend()
            .help("The same for what hangs on a rope no one holds (a gun left hanging from a ceiling, what hangs from a "
                  "dropped gun): it stops swinging in a couple of seconds."),
        slider("Top Speed", vr_grapple_load_max_speed, 0.f, 2000.f, 50.f, "%.0f u/s").extend(0.f, 10000.f)
            .help("The fastest what hangs on the rope moves relative to you (no rope of death). 0: no limit."),
        slider("Slack When Detached", vr_grapple_loose_slack, 0.f, 128.f, 4.f, "%.0f").extend(0.f, 1000.f)
            .help("Units of slack the rope gets when the hook comes off: a loose hook lying there is not dragged by "
                  "your next step."),

        header("Reel"),
        slider("Reel Speed", vr_grapple_reel_speed, 100.f, 1000.f, 25.f, "%.0f u/s").extend(25.f, 2000.f)
            .help("How fast the reel pulls you in: to a wall or a ceiling, to a huge monster, to a prop too heavy to come."),
        slider("Shortest Rope", vr_grapple_min_length, 16.f, 128.f, 4.f, "%.0f").extend(0.f, 400.f)
            .help("How short the reel takes the rope (units)."),
        slider("Loose Hook Goes In At", vr_grapple_reel_home, 0.5f, 16.f, 0.5f, "%.1f").extend(0.5f, 64.f)
            .help("How close to the muzzle (units) you reel a loose hook's back before it goes into the gun. Smaller: you "
                  "reel it in further."),
        slider("Unreel Speed", vr_grapple_unreel_speed, 50.f, 800.f, 25.f, "%.0f u/s").extend(0.f, 2000.f)
            .help("The hand's lower button (A on the right hand, X on the left) pays the rope out while held, this fast: "
                  "you let yourself down from a ceiling, a monster or a prop can go farther. Let go and the rope keeps its "
                  "length. 0: no unreel."),
        slider("Unreel Slack", vr_grapple_unreel_slack, 4.f, 1000.f, 4.f, "%.0f").extend(4.f, 10000.f)
            .help("With nothing pulling on the rope (you standing, a hooked box on the floor), the unreel lets out this "
                  "much slack (units) beyond the hook's distance: the rope comes off the drum and lies there. Hanging, "
                  "what pulls takes the rope out as ever. 4: no slack."),
        toggle("Unreel Button Only When Airborne", vr_grapple_unreel_airborne)
            .help("On: a button that jumps (A) unreels only in the air: on the ground it jumps as ever, and a press that "
                  "jumped does not unreel until pressed again; X (the reload) unreels on the ground too. Off: either "
                  "unreels anywhere, its key still pressed too."),

        header("Props"),
        slider("Prop Reel Speed", vr_grapple_prop_speed, 100.f, 1500.f, 25.f, "%.0f u/s").extend(25.f, 3000.f)
            .help("How fast light props come in: weapons, pickups, keys, gibs. They hang at the gun, to take with the "
                  "other hand."),
        slider("Light Up To", vr_grapple_prop_light, 1.f, 40.f, 1.f, "%.0f kg").extend(0.1f, 200.f)
            .help("Props up to this mass come at the Prop Reel Speed; heavier ones slower, in proportion (twice as heavy: "
                  "half as fast). Masses: Held Object Offsets."),
        slider("Too Heavy From", vr_grapple_prop_anchor, 10.f, 500.f, 5.f, "%.0f kg").extend(1.f, 10000.f)
            .help("A prop this heavy does not come: the reel pulls you to it, as to a wall."),
        slider("Dragging: Light Up To", vr_grapple_tow_light, 1.f, 100.f, 1.f, "%.0f kg").extend(0.1f, 1000.f)
            .help("Walking away from a hooked prop on a taut rope drags it along: up to this mass, at the Dragging Speed; "
                  "heavier, its weight holds you back, in proportion (twice as heavy: half as fast; an explosive box is "
                  "40 kg)."),
        slider("Dragging Speed", vr_grapple_tow_speed, 0.f, 600.f, 10.f, "%.0f u/s").extend(0.f, 2000.f)
            .help("The fastest you walk dragging a light prop on a taut rope. 0: no limit (it comes at your speed)."),

        header("Monsters"),
        slider("Small Up To", vr_grapple_small_mass, 20.f, 600.f, 10.f, "%.0f kg").extend(0.f, 10000.f)
            .help("Monsters up to this mass are small: pulled fast and staggered (dogs 35, grunts 80, knights 90, "
                  "enforcers 100)."),
        slider("Huge From", vr_grapple_huge_mass, 100.f, 2000.f, 25.f, "%.0f kg").extend(0.f, 100000.f)
            .help("Monsters this heavy are huge: the reel pulls you to them (shamblers 600, bosses). In between, medium: "
                  "pulled slowly, not staggered (hell knights 150, ogres and fiends 250)."),
        slider("Small Reel Speed", vr_grapple_small_speed, 50.f, 1000.f, 25.f, "%.0f u/s").extend(10.f, 2000.f),
        slider("Medium Reel Speed", vr_grapple_medium_speed, 25.f, 600.f, 25.f, "%.0f u/s").extend(10.f, 2000.f),
        toggle("Stagger Small Monsters", vr_grapple_stagger)
            .help("A small monster reeled in is staggered: its pain, cutting off its attack, and again every second."),
        slider("Stamina a Second", vr_grapple_stamina, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("Reeling a monster in tires you (the parry, shove and strike stamina): this much a second. Short of it, "
                  "it comes a quarter as fast. 0: free."),

        header("Feel"),
        slider("Haptics", vr_grapple_haptics, 0.f, 1.f, 0.1f, "%.1f")
            .help("The hook biting, the reel's buzz (stronger with a heavier load), the rope snapping taut."),
        toggle("Slack Rope Hangs", vr_grapple_sag).help("A slack rope is drawn hanging (off: always straight)."),
    };
}

// Movement > Player Hitbox: how wide the player is against the map and against other boxes (vr_hull.cpp,
// docs/vr-port/HULLS.md).
za::Vector<Item> pageHitbox()
{
    const za::Vector<Choice> widths{{8.f, "8 units"}, {12.f, "12 units"}, {16.f, "16 units"}, {20.f, "20 units"},
        {24.f, "24 units"}, {28.f, "28 units"}, {32.f, "32 (new collision)"}};
    za::Vector<Choice> world{{0.f, "Quake's (32)"}};
    world.emplaceBackRange(widths.data(), widths.size());
    za::Vector<Choice> ents{{-1.f, "Same as Walls"}, {0.f, "Quake's (32)"}};
    ents.emplaceBackRange(widths.data(), widths.size());
    za::Vector<Choice> hits{world.begin(), world.end() - 1}; // Quake's 32 .. 28 (32 is Quake's box already)
    return {
        header("Walls and Brush Models"),
        cycle("Width Against Walls", vr_hull_width, world)
            .help("How wide you are against walls, floors, ledges, doors, lifts and other brush models. Quake's box is "
                  "32 units (you stop 16 from each wall: 0.6 m); 16 stops you 8 away (0.3 m)."),
        toggle("Ledge Test Fix", vr_hull_edge_probe)
            .help("Quake slows you more when the floor ahead of you drops away (an edge). On: that test looks down from "
                  "your box's front edge (half your width ahead), and a point inside a step counts as floor. Off: "
                  "Quake's, 16 units ahead whatever your width; stairs with a smooth ramp over them (MG1's start) then "
                  "slowed you to a crawl, and a jump against them went nowhere (vr_hull_edge_probe)."),
        toggle("Doors, Lifts and Walls Too", vr_hull_brushmodels)
            .help("The width above against brush models as well (doors, lifts, trains, func_walls, the firing range's "
                  "panels and tables). Off: only the world's walls; brush models meet Quake's box."),
        header("Monsters, Players and Boxes"),
        cycle("Width Against Them", vr_hull_ent_width, ents)
            .help("How wide you are against monsters, other players and solid boxes, both ways (you walking into them, "
                  "them walking into you). Shots and missiles hit the width below; items are picked up with Quake's 32 box."),
        toggle("Monsters", vr_hull_monsters).help("That width between you and monsters."),
        toggle("Other Players", vr_hull_players).help("That width between players."),
        toggle("Solid Boxes", vr_hull_boxes).help("That width against explosive boxes and other solid boxes."),
        cycle("Width Shots Hit", vr_hull_hit_width, hits)
            .help("How wide you are to monsters' shots and missiles (bullets, nails, lasers, grenades, rockets, spit, "
                  "vore balls). Quake's box is 32; 24 still leaves them a bit of leniency. Melee blows and splash "
                  "damage go by distance, not by this box; monsters see and chase you as before."),
        toggle("Height Shots Hit: Your Head's", vr_hull_hit_head)
            .help("Shots and missiles meet you only up to the top of your head: crouched behind a crate, those flying "
                  "over it miss you (Quake's box stands as tall), and monsters' bullets aim no higher than your body. "
                  "Off: Quake's height, crouched or not (vr_hull_hit_head)."),
        header("Crouching"),
        toggle("Crouched Hitbox", vr_crouch_hull)
            .help("Crouch in real life and your box is lower: you walk under low ceilings and through small teleporters, "
                  "and behind cover shots meet only what shows of you. Standing up somewhere too low, you keep the low box "
                  "until there is room. Off: always Quake's standing height (vr_crouch_hull)."),
        slider("Lowest Crouched Height", vr_crouch_height, 24.f, 52.f, 4.f, "%.0f units")
            .help("Your box's height with your eyes under it (Quake's standing box: 56; the crouching room's tunnel in "
                  "the teleporter test map: 40) (vr_crouch_height)."),
        toggle("Monsters See Your Eyes", vr_crouch_sight)
            .help("Crouched (with the crouched hitbox), monsters look for you at your real eyes, not at a standing "
                  "player's: crouched behind cover lower than you stand, you are out of their sight, as Arcane "
                  "Dimensions' height-aware monsters check (vr_crouch_sight). Standing: Quake's as ever."),
        cycle("Heights Above It", vr_crouch_step, {{0.f, "None"}, {4.f, "Every 4 units"}, {8.f, "Every 8 units"},
                  {12.f, "Every 12 units"}})
            .help("Taller crouched boxes up to 52, so half a crouch gets you under a ceiling that is a little low: your "
                  "eyes under one, that box (36, 44, 52 every 8). None: the lowest only (vr_crouch_step)."),
        command("Crouch Status", "vr_crouch_status")
            .help("Prints your eyes' height over your feet, your box's height (0 standing) and whether you could stand."),
        slider("Prop Push Radius", vr_box3d_player_radius, 5.f, 40.f, 1.f, "%.0f cm")
            .help("Loose props (rocks, bricks, weapons on the floor) are pushed by a capsule this wide round your body, "
                  "not by your box (vr_box3d_player_radius)."),
        header("Standing on Props"),
        toggle("Stand on Boxes", vr_box3d_player_stand)
            .help("Explosive boxes and other solid props are ground: you stand still on them, walk and jump from them, "
                  "and ride them as they move; your body doesn't shove the one under you (vr_box3d_player_stand)."),
        slider("Steepest Face to Stand On", vr_box3d_player_slope, 0.f, 46.f, 1.f, "%.0f deg")
            .help("A solid prop's face tilted more than this isn't ground: you slide off it, as off a slope too steep to "
                  "walk, and your weight doesn't press it. 46: Quake's own limit (you stood on steeper faces, creeping "
                  "down slowly) (vr_box3d_player_slope)."),
        slider("Your Weight on Them", vr_box3d_player_mass, 0.f, 150.f, 5.f, "%.0f kg")
            .help("What you press the prop you stand on down with (a floating box sinks lower: at most half its own "
                  "weight there). 0: none."),
        slider("Jump Push", vr_box3d_player_jump_push, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of your jump's push the prop you jump from takes (1: all of it, your weight times your "
                  "speed). 0: none."),
        slider("Walking Into Them", vr_box3d_player_shove, 0.f, 2.f, 0.1f, "%.1f")
            .help("How hard you shove a solid prop you walk into, as a share of your weight: a light box goes nearly at "
                  "your pace (at most 2.5 m/s: vr_box3d_player_push_speed), a heavy one slowly, one too heavy not at all. 0: "
                  "they stop you like walls (vr_box3d_player_shove)."),
        slider("Monsters Shove Them At", vr_box3d_monster_push_speed, 0.f, 10.f, 0.5f, "%.1f m/s")
            .help("The fastest a monster walking or running into a solid prop pushes it: its steps are jumps, and a crate "
                  "kicked at their speed broke and hurt the monster. 0: no limit (vr_box3d_monster_push_speed)."),
        toggle("Never Trapped by Them", vr_box3d_player_unstick)
            .help("A solid prop that ends up in your body (toppled onto you, pushed into you) never holds you: you walk out "
                  "of it, and it is pushed out of you (vr_box3d_player_unstick)."),
        toggle("Never Through Them", vr_box3d_player_hold)
            .help("Inside a solid prop, you only move out of it, never deeper; with your feet a little into its top (you "
                  "landed on it, or it rocked up into you), you are put back on top. A box you threw or let go of is solid "
                  "to you too (vr_box3d_player_hold)."),
        toggle("Their Real Shape", vr_box3d_player_shape)
            .help("You meet a solid prop's shape as drawn, however it is turned, and round: as close to a box's face turned "
                  "any way as to a wall; a tilted box is no taller than it looks. Off: the upright box round it "
                  "(vr_box3d_player_shape)."),
        toggle("Shots Meet Their Shape", vr_box3d_shot_shape)
            .help("Rockets, nails, grenades, lasers and every other missile, and the guns' shots, meet a solid prop's "
                  "shape as drawn, however it is turned: a tilted box's empty corners let them by, and they hit its real "
                  "faces. Off: the upright box round it (the grappling hook still meets its shape) (vr_box3d_shot_shape)."),
        header("Tests"),
        command("Stand on a Box", "vr_physics_player onto misc_explobox")
            .help("Puts you on top of the level's first explosive box (vr_physics_player onto <number | classname>)."),
        command("Rocket at the Nearest Box", "vr_debug_missiles 1; developer 1; vr_physics_fire 0")
            .help("Fires a rocket of yours from your eyes at the middle of the nearest solid prop and prints where it hits "
                  "(vr_physics_fire <kind> [<x> <y> <z>]: 0 a rocket, 1 a nail, 2 a grenade, 3 a super nail, 4 an "
                  "enforcer's laser, 10 a shotgun pellet; Missile Hits)."),
        command("Nail at the Nearest Box", "vr_debug_missiles 1; vr_physics_fire 1")
            .help("A nail of yours at the middle of the nearest solid prop (vr_physics_fire 1)."),
        command("Grenade at the Nearest Box", "vr_debug_missiles 1; vr_physics_fire 2")
            .help("A grenade of yours, thrown straight at the middle of the nearest solid prop (vr_physics_fire 2)."),
        command("Shot Clip Cost", "vr_physics_shotbench")
            .help("Prints how long a shot's or a missile's clip against the first solid prop's shape takes, and a "
                  "missile's whole trace with Shots Meet Their Shape on and off (vr_physics_shotbench [<count>])."),
        command("Box Approach", "vr_physics_approach")
            .help("Prints how close your box gets to the first explosive box's face from 16 directions round it: the same "
                  "from every side and as close as to a wall (Hitbox Approach) with Their Real Shape on."),
        command("Where You Stand", "vr_physics_player")
            .help("Prints where you are, whether you are on the ground and on what (a prop's number), and what is under "
                  "your feet."),
        command("Watch Falling Into Props", "vr_physics_inside 1")
            .help("Counts from now the frames you spend inside a solid prop's drawn shape (where none of your moves may go) "
                  "and prints when you pass into one; vr_physics_inside prints the count, vr_physics_inside 0 stops."),
        command("Hitbox Stats", "vr_hull_stats").help("Prints the map's rebuilt brushes and compiled hull: counts, memory, build times."),
        command("Hitbox Approach", "vr_hull_approach").help("Prints how close your box gets to what is round you, in 8 directions (from your centre to the surface it stops at; Quake's box: 16 units). vr_hull_approach <classname> [n] does it round an entity."),
        command("Random Walk (60 s)", "god; notarget; vr_hull_walktest 60")
            .help("Walks you around the map at random for 60 seconds (hopping somewhere new every few), then prints how "
                  "often you got stuck, in a wall or in a monster."),
    };
}

// Movement > Monster Hitbox: monsters' widths against walls, by class (vr_hull.cpp, docs/vr-port/HULLS.md, "Monsters").
za::Vector<Item> pageMonsterHitbox()
{
    auto widths = [](float quake)
    {
        static constexpr const char* labels[] = {"16 units", "24 units", "32 units", "40 units", "48 units", "56 units"};
        za::Vector<Choice> c{{-1.f, "Its Box"}, {0.f, quake == 32.f ? "Quake's Hull (32)" : "Quake's Hull (64)"}};
        for(int i = 0; 16.f + 8.f * static_cast<float>(i) < quake; ++i)
        {
            c.pushBack({16.f + 8.f * static_cast<float>(i), labels[i]});
        }
        return c;
    };
    const za::Vector<Choice> small = widths(32.f), big = widths(64.f);
    auto cls = [&](const char* label, cvar_t& cvar, bool hull2)
    {
        return cycle(label, &cvar, hull2 ? big : small)
            .help(hull2 ? "This class's width against walls (Its Box: its own box's, in brackets). Quake moves it with a "
                          "64-wide box, 88 tall (the height is kept whatever the width)."
                        : "This class's width against walls (Its Box: its own box's, in brackets). Quake moves it with a "
                          "32-wide box, 56 tall (the height is kept whatever the width).");
    };
    return {
        header("Monsters' Widths (Prototype)"),
        toggle("Narrower Monsters", vr_mhull)
            .help("Quake moves monsters against walls with a 32-wide box (64 for the big ones), wider than most of their "
                  "own boxes in Quake VR (a grunt's is 24, an ogre's 40). On: the widths below instead (a clipping hull "
                  "compiled for each when the map loads, as yours is): they walk closer to walls and through narrower "
                  "gaps (on by default). You and your shots meet their own boxes and models already."),
        toggle("Against Bodies Too", vr_mhull_ents)
            .help("A width narrower than a monster's own box meets you, other monsters and solid boxes too, both ways. "
                  "Off: their own boxes between bodies."),
        toggle("Ledges by Their Width", vr_mhull_ledges)
            .help("A width narrower than a monster's own box also takes the ledge test (a monster doesn't step where "
                  "its corners aren't over ground): it walks as close to a ledge as its width. Off: its box's corners."),
        command("All Match Their Boxes", "vr_mhull_reset").help("Every class back to its own box's width (the defaults)."),
        header("By Class (Its Box)"),
        cls("Grunt (24)", vr_mhull_army, false),
        cls("Enforcer (28)", vr_mhull_enforcer, false),
        cls("Knight (24)", vr_mhull_knight, false),
        cls("Death Knight (24)", vr_mhull_hknight, false),
        cls("Rottweiler (24)", vr_mhull_dog, false),
        cls("Scrag (24)", vr_mhull_wizard, false),
        cls("Spawn (24)", vr_mhull_tarbaby, false),
        cls("Zombie (32)", vr_mhull_zombie, false),
        cls("Fiend (32)", vr_mhull_demon, false),
        cls("Rotfish (32)", vr_mhull_fish, false),
        cls("Ogre (40)", vr_mhull_ogre, true),
        cls("Vore (64)", vr_mhull_shalrath, true),
        cls("Shambler (64)", vr_mhull_shambler, true),
        header("Tests"),
        command("Hitbox Stats", "vr_hull_stats").help("Prints the compiled hulls, the monsters' too: sizes, memory, build times."),
        command("Monster Walk (60 s)", "god; notarget; vr_mhull_walktest 60")
            .help("Walks the level's monsters at random for 60 seconds (their AI stopped meanwhile), then prints by "
                  "class how far they walked, how often they got stuck, dropped off a ledge or ended up in a wall."),
        command("Monster Patrol (60 s)", "notarget; vr_mhull_walktest 60 1 1")
            .help("Leaves the monsters' own AI running for 60 seconds (you unseen) and counts the patrol corners they "
                  "reach, and the same stuck, drop and wall counts."),
    };
}

// ----------------------------------------------------------------------------
// Pages
// ----------------------------------------------------------------------------

enum PageId
{
    PageMain,
    PageAdvanced
};

// Who a page is for (vr_menu_level, Menu Detail at the bottom of every page): links to a page above the level are left
// out, and so are the rows marked advanced() or developer() (archive/MENU_REVIEW.md). menu_vr <n> opens any page.
enum MenuLevel
{
    LevelStandard,  // what every player sets: comfort, height, the HUD, the headset, volume
    LevelAdvanced,  // every gameplay, display and graphics setting (the default for a page)
    LevelDeveloper, // tuning (weapon and prop offsets, weights, ragdolls, hitboxes), recording, debug and tests
    LevelCount
};

struct Page
{
    const char* title; // over the page; also its name in vr_menu_positions (renamed: its record is ignored)
    PageBuilder build;
    PageBuilder home; // the page listing it in the tree (Back after menu_vr <n>; links elsewhere go back where they came from)
    int level{LevelAdvanced};
};

[[nodiscard]] za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageSaves();
[[nodiscard]] za::Vector<Item> pageVrsHeight();
[[nodiscard]] za::Vector<Item> pageVrsHands();
[[nodiscard]] za::Vector<Item> pageVrsHolsters();
[[nodiscard]] za::Vector<Item> pageVrsAmmoPouch();
[[nodiscard]] za::Vector<Item> pageVrsBodyEquipment();
[[nodiscard]] za::Vector<Item> pageVrsItemCollection();
[[nodiscard]] za::Vector<Item> pageVrsControls();
[[nodiscard]] za::Vector<Item> pageVrsLocomotion();
[[nodiscard]] za::Vector<Item> pageVrsComfort();
[[nodiscard]] za::Vector<Item> pageVrsTeleport();
[[nodiscard]] za::Vector<Item> pageVrsTurning();
[[nodiscard]] za::Vector<Item> pageVrsFlashlight();
[[nodiscard]] za::Vector<Item> pageVrsLighting();
[[nodiscard]] za::Vector<Item> pageVrsWeapons();
[[nodiscard]] za::Vector<Item> pageVrsBulletTime();
[[nodiscard]] za::Vector<Item> pageVrsBody();
[[nodiscard]] za::Vector<Item> pageVrsHaptics();
[[nodiscard]] za::Vector<Item> pageVrsHud();
[[nodiscard]] za::Vector<Item> pageVrsSound();
[[nodiscard]] za::Vector<Item> pageVrsDisplay();
[[nodiscard]] za::Vector<Item> pageVrsScaling();
[[nodiscard]] za::Vector<Item> pageVrsGraphics();
[[nodiscard]] za::Vector<Item> pageVrsHeightHands();
[[nodiscard]] za::Vector<Item> pageVrsLocoControls();
[[nodiscard]] za::Vector<Item> pageVrsComfortTurning();
[[nodiscard]] za::Vector<Item> pageVrsWeaponsBulletTime();
[[nodiscard]] za::Vector<Item> pageVrsBodyFlashlight();
[[nodiscard]] za::Vector<Item> pageVrsHudHapticsSound();
[[nodiscard]] za::Vector<Item> pageVrsDisplayGraphics();
[[nodiscard]] za::Vector<Item> pageAdvanced();
[[nodiscard]] za::Vector<Item> pageWeaponOffsets();
[[nodiscard]] za::Vector<Item> pageWofsHand();
[[nodiscard]] za::Vector<Item> pageWofsFingers();
[[nodiscard]] za::Vector<Item> pageWofsSights();
[[nodiscard]] za::Vector<Item> pageWofsTwoHanded();
[[nodiscard]] za::Vector<Item> pageWofsStock();
[[nodiscard]] za::Vector<Item> pageWofsScreen();
[[nodiscard]] za::Vector<Item> pageWofsHolstered();
[[nodiscard]] za::Vector<Item> pageWofsEffects();
[[nodiscard]] za::Vector<Item> pageWofsFlashlight();
[[nodiscard]] za::Vector<Item> pageCombat();
[[nodiscard]] za::Vector<Item> pageMovement();
[[nodiscard]] za::Vector<Item> pageCarryingHub();
[[nodiscard]] za::Vector<Item> pageWeaponsHub();
[[nodiscard]] za::Vector<Item> pageHudHub();
[[nodiscard]] za::Vector<Item> pageLightningWater();
[[nodiscard]] za::Vector<Item> pageWeaponEffects();

// The pages, by their number (menu_vr <n>): the numbers stay as they were, new pages are added last (the menus'
// tree is in the pages' links: VR Settings > Advanced VR Options > its groups > their pages; ROUND21.md, "Menus
// reorganized"). Links find their page by its builder (pageIndex).
const Page pages[] = {
    {"VR Settings", pageMain, nullptr, LevelStandard},                                     // 0
    {"Advanced VR Options", pageAdvanced, pageMain},                        // 1
    {"Play", pagePlay, pageAdvanced},                                       // 2
    {"World", pageGameplay, pageAdvanced},                                  // 3 (Gameplay before)
    {"Parry and Bash", pageParryBash, pageCombat},                          // 4 (Parry, Bash and Headbutt before)
    {"Melee", pageMeleeSettings, pageCombat},                               // 5
    {"Motion Recorder", pageMotionRecorder, pageAdvanced, LevelDeveloper},                  // 6
    {"Review Takes", pageReviewTakes, pageAdvanced, LevelDeveloper},                        // 7
    {"Take", pageReviewTake, pageReviewTakes, LevelDeveloper},                              // 8
    {"Gore", pageGore, pageAdvanced},                                       // 9
    {"Throwing and Physics", pageThrowing, pageCarryingHub, LevelDeveloper},                // 10
    {"Carrying", pageCarrying, pageCarryingHub},                            // 11 (Carrying and Gibs before)
    {"Force Grab", pageForceGrab, pageCarryingHub},                         // 12
    {"Grappling Hook", pageGrapple, pageMovement},                          // 13
    {"Body", pageBody, pageAdvanced},                                       // 14
    {"Body - Arms and Pauldrons", pageBodyArms, pageBody, LevelDeveloper},                  // 15
    {"Body Calibration", pageBodyCalibration, pageBody, LevelStandard},                    // 16
    {"Flashlight", pageFlashlight, pageAdvanced},                           // 17
    {"Player Calibration", pagePlayerCalibration, pageBody},                // 18
    {"Locomotion", pageLocomotionSettings, pageMovement},                   // 19
    {"Swimming", pageSwimSettings, pageMovement},                           // 20
    {"Immersion", pageImmersionSettings, pageWeaponsHub},                   // 21
    {"Hand/Gun Calibration", pageHandGunCalibration, pageWeaponsHub},       // 22
    {"Weapon Offsets", pageWeaponOffsets, pageWeaponsHub, LevelDeveloper},                  // 23
    {"Aiming", pageAimingSettings, pageWeaponsHub},                         // 24
    {"Hotspots", pageHotspotSettings, pageWeaponsHub},                      // 25
    {"Wrist Gadget", pageGadget, pageHudHub},                               // 26
    {"Screens", pageScreens, pageHudHub},                                   // 27
    {"Colours", pageColours, pageHudHub},                                   // 28
    {"Status Bar", pageHudConfiguration, pageHudHub},                       // 29
    {"Crosshair", pageCrosshairSettings, pageHudHub},                       // 30
    {"Menu", pageMenuSettings, pageHudHub},                                 // 31
    {"Graphics", pageGraphics, pageAdvanced},                               // 32
    {"Graphics - Lights", pageGraphicsLights, pageGraphics},                // 33
    {"Graphics - Shadows", pageGraphicsShadows, pageGraphics},              // 34
    {"Graphics - Surfaces", pageGraphicsSurfaces, pageGraphics},            // 35
    {"Graphics - Liquids", pageGraphicsLiquids, pageGraphics},              // 36
    {"Graphics - Post-processing", pageGraphicsPost, pageGraphics},         // 37
    {"Graphics - Models and Effects", pageGraphicsModels, pageGraphics},    // 38
    {"Particles", pageParticleSettings, pageGraphics},                      // 39
    {"Transparency", pageTransparencyOptions, pageGraphics},                // 40
    {"Held Object Offsets", pageHeldObjectOffsets, pageCarryingHub, LevelDeveloper},        // 41
    {"Weapon Weights", pageWeaponWeights, pageWeaponsHub, LevelDeveloper},                  // 42
    {"Held Object Weights", pageHeldObjectWeights, pageCarryingHub, LevelDeveloper},        // 43
    // Added with the menus reorganized (ROUND21.md).
    {"Combat", pageCombat, pageAdvanced},                                   // 44
    {"Movement", pageMovement, pageAdvanced},                               // 45
    {"Carrying and Throwing", pageCarryingHub, pageAdvanced},               // 46
    {"Weapons", pageWeaponsHub, pageAdvanced},                              // 47
    {"HUD and Menus", pageHudHub, pageAdvanced},                            // 48
    {"Body and Display", pageBodyDisplay, pageAdvanced, LevelStandard},                        // 49
    {"Headset", pageHeadset, pageAdvanced, LevelStandard},                                     // 50
    {"Damage and Knockback", pageDamage, pageCombat},                       // 51
    {"Stamina", pageStamina, pageCombat},                                   // 52
    {"Batting and Catching", pageBatting, pageCombat},                      // 53
    {"Climbing", pageClimbing, pageMovement},                               // 54
    {"Wall Torches", pageWallTorches, pageCarryingHub},                     // 55
    {"Rocks and Bricks", pageRocksBricks, pageCarryingHub},                 // 56
    {"Gibs and Corpses", pageGibs, pageCarryingHub},                        // 57
    {"Flashlight - Low Grip", pageFlashlightLowGrip, pageFlashlight, LevelDeveloper},       // 58
    {"Flashlight - Overhead Grip", pageFlashlightOverheadGrip, pageFlashlight, LevelDeveloper}, // 59
    {"Flashlight - On a Gun or Head", pageFlashlightMounts, pageFlashlight, LevelDeveloper}, // 60
    {"Fingers and Collisions", pageFingersCollisions, pageWeaponsHub, LevelDeveloper},      // 61
    {"Weight and Damage", pageWeightDamage, pageWeaponsHub},                // 62
    {"Hip Holsters", pageHipHolsters, pageWeaponsHub},                      // 63
    {"Debug", pageDebug, pageAdvanced, LevelDeveloper},                                     // 64
    {"Recording", pageRecording, pageGraphics},                             // 65 (the desktop window's view)
    // The Debug page's parts (ROUND21.md, "Debug menu; quad sound; grenade catch default; no empty-hand deflection").
    {"Debug - Views", pageDebugViews, pageDebug, LevelDeveloper},                           // 66
    {"Debug - Logging", pageDebugLogging, pageDebug, LevelDeveloper},                       // 67
    {"Debug - Profiling and Memory", pageDebugProfiling, pageDebug, LevelDeveloper},        // 68
    {"Debug - Reports", pageDebugReports, pageDebug, LevelDeveloper},                       // 69
    {"Debug - Tools", pageDebugTools, pageDebug, LevelDeveloper},                           // 70
    {"Debug - Tests", pageDebugTests, pageDebug, LevelDeveloper},                           // 71
    {"Checklist", pageChecklist, pageDebug, LevelDeveloper},                                // 72 (also the corner's button)
    {"Player Hitbox", pageHitbox, pageMovement, LevelDeveloper},                            // 73
    {"Monster Hitbox", pageMonsterHitbox, pageMovement, LevelDeveloper},                    // 74
    {"Lightning Gun in Water", pageLightningWater, pageWeaponsHub},         // 75
    {"Enemy Weapons", pageEnemyWeapons, pageCombat},                        // 76
    {"Sound", pageSound, pageAdvanced},                                         // 77
    {"Crates", pageCrates, pageCarryingHub},                                // 78
    {"Weapon Damage", pageWeaponDamage, pageCombat},                        // 79
    {"Weapon Effects", pageWeaponEffects, pageWeaponsHub},                  // 80
    {"Enemy Shoves", pageEnemyShoves, pageCombat},                          // 81
    {"Chainsaw Engine", pageChainsawEngine, pageEnemyWeapons, LevelDeveloper},              // 82
    {"Bullet Time", pageBulletTime, pageCombat},                            // 83 (vr_menu_recording.inc)
    // Weapon Offsets' parts (ROUND21.md, "Weapon Offsets split; the virtual stock's turn"): its main page links them.
    {"Weapon Offsets - Hand and Grip", pageWofsHand, pageWeaponOffsets, LevelDeveloper},           // 84
    {"Weapon Offsets - Fingers", pageWofsFingers, pageWeaponOffsets, LevelDeveloper},              // 85
    {"Weapon Offsets - Muzzle and Sights", pageWofsSights, pageWeaponOffsets, LevelDeveloper},     // 86
    {"Weapon Offsets - Two-Handed", pageWofsTwoHanded, pageWeaponOffsets, LevelDeveloper},        // 87
    {"Weapon Offsets - Virtual Stock", pageWofsStock, pageWeaponOffsets, LevelDeveloper},          // 88
    {"Weapon Offsets - Ammo Screen", pageWofsScreen, pageWeaponOffsets, LevelDeveloper},           // 89
    {"Weapon Offsets - Holstered", pageWofsHolstered, pageWeaponOffsets, LevelDeveloper},          // 90
    {"Weapon Offsets - Effects", pageWofsEffects, pageWeaponOffsets, LevelDeveloper},              // 91
    {"Weapon Offsets - Flashlight", pageWofsFlashlight, pageWeaponOffsets, LevelDeveloper},        // 92
    {"Small Gibs", pageSmallGibs, pageGore, LevelDeveloper},                                       // 93
    {"Burning", pageBurning, pageCombat},                                          // 94
    {"Graphics - Retro Textures", pageGraphicsRetro, pageGraphics},                // 95
    {"Retro Textures - World", pageRetroCategory<retro::Category::World>, pageGraphicsRetro, LevelDeveloper}, // 96
    {"Retro Textures - Brush Entities", pageRetroCategory<retro::Category::Brush>, pageGraphicsRetro, LevelDeveloper}, // 97
    {"Retro Textures - Item Pickups", pageRetroCategory<retro::Category::Items>, pageGraphicsRetro, LevelDeveloper}, // 98
    {"Retro Textures - Props and Debris", pageRetroCategory<retro::Category::Props>, pageGraphicsRetro, LevelDeveloper}, // 99
    {"Retro Textures - Gibs", pageRetroCategory<retro::Category::Gibs>, pageGraphicsRetro, LevelDeveloper}, // 100
    {"Retro Textures - Small Gibs", pageRetroCategory<retro::Category::SmallGibs>, pageGraphicsRetro, LevelDeveloper}, // 101
    {"Retro Textures - Weapons in the World", pageRetroCategory<retro::Category::Weapons>, pageGraphicsRetro, LevelDeveloper}, // 102
    {"Retro Textures - Held Weapons", pageRetroCategory<retro::Category::Held>, pageGraphicsRetro, LevelDeveloper}, // 103
    {"Retro Textures - Monsters", pageRetroCategory<retro::Category::Monsters>, pageGraphicsRetro, LevelDeveloper}, // 104
    {"Retro Textures - Your Hands", pageRetroCategory<retro::Category::Hands>, pageGraphicsRetro, LevelDeveloper}, // 105
    {"Retro Textures - Your Arms", pageRetroCategory<retro::Category::Arms>, pageGraphicsRetro, LevelDeveloper}, // 106
    {"Retro Textures - Your Torso", pageRetroCategory<retro::Category::Torso>, pageGraphicsRetro, LevelDeveloper}, // 107
    {"Retro Textures - Your Legs", pageRetroCategory<retro::Category::Legs>, pageGraphicsRetro, LevelDeveloper}, // 108
    {"Retro Textures - Your Gear", pageRetroCategory<retro::Category::Gear>, pageGraphicsRetro, LevelDeveloper}, // 109
    {"Retro Textures - Decals", pageRetroCategory<retro::Category::Decals>, pageGraphicsRetro, LevelDeveloper}, // 110
    {"Retro Textures - Particles", pageRetroCategory<retro::Category::Particles>, pageGraphicsRetro, LevelDeveloper}, // 111
    {"Retro Textures - Sprites", pageRetroCategory<retro::Category::Sprites>, pageGraphicsRetro, LevelDeveloper}, // 112
    {"Retro Textures - Other Models", pageRetroCategory<retro::Category::Other>, pageGraphicsRetro, LevelDeveloper}, // 113
    {"Retro Textures - Override", pageRetroOverride, pageGraphicsRetro, LevelDeveloper}, // 114
    {"Retro Textures - All Categories", pageRetroAll, pageGraphicsRetro, LevelDeveloper}, // 115
    {"Graphics - Retro Lighting", pageGraphicsRetroLight, pageGraphics},          // 116
    {"Gibs and Corpses - Ragdolls", pageRagdolls, pageGibs, LevelDeveloper},                       // 117
    {"Ragdolls - Grunt", pageRagdollGrunt, pageRagdolls, LevelDeveloper},                          // 118
    {"Ragdolls - Knight", pageRagdollKnight, pageRagdolls, LevelDeveloper},                        // 119
    {"Gibs and Corpses - Corpse Damage and Health", pageCorpseDamage, pageGibs, LevelDeveloper},   // 120
    {"Ragdolls - Ogre", pageRagdollOgre, pageRagdolls, LevelDeveloper},                            // 121
    {"Ragdolls - Enforcer", pageRagdollEnforcer, pageRagdolls, LevelDeveloper},                    // 122
    {"Ragdolls - Death Knight", pageRagdollDeathKnight, pageRagdolls, LevelDeveloper},             // 123
    {"Ragdolls - Rottweiler", pageRagdollRottweiler, pageRagdolls, LevelDeveloper},                // 124
    {"Ragdolls - Scrag", pageRagdollScrag, pageRagdolls, LevelDeveloper},                          // 125
    {"Gore - Decapitation", pageDecapitation, pageGore},                           // 126
    {"Ragdolls - Zombie", pageRagdollZombie, pageRagdolls, LevelDeveloper},                        // 127
    {"Ragdolls - Fiend", pageRagdollFiend, pageRagdolls, LevelDeveloper},                         // 128
    {"Ragdolls - Shambler", pageRagdollShambler, pageRagdolls, LevelDeveloper},                   // 129
    {"Ragdolls - Gremlin", pageRagdollGremlin, pageRagdolls, LevelDeveloper},                     // 130
    {"Ragdolls - Mummy", pageRagdollMummy, pageRagdolls, LevelDeveloper},                         // 131
    {"Changed Settings", pageChanged, pageAdvanced, LevelStandard},                                    // 132 (archive/MENU_REVIEW.md)
    {"Search", pageSearch, pageMain, LevelStandard},                                               // 133 (the corner's Search; vr_menu_search.inc)
    {"Console", pageConsole, pageMain, LevelStandard},                                             // 134 (the corner's Console; vr_menu_console.inc)
    {"Tips", pageTips, pageAdvanced, LevelStandard},                                                   // 135 (vr_tips.cpp)
    {"Graphics - Teleporters", pageGraphicsTeleporters, pageGraphics},                                 // 135 (vr_portals.cpp)
    {"Small Gibs - Per Enemy", pageSmallGibsEnemies, pageSmallGibs, LevelDeveloper},              // 136
    {"Knockdowns", pageKnockdowns, pageCombat},                                                    // 137
    {"Explosion Debris", pageExplosionDebris, pageParticleSettings},
    {"Fire Particles", pageFireParticles, pageParticleSettings},
    {"Spawn Pickup Weapons", pageSpawnWeapons, pageDebugTests, LevelDeveloper},
    {"Map Library", pageMaps, pageMain, LevelStandard}, // (the corner's Maps, and Single Player > Map Library; vr_menu_maps.inc)
    {"Official Campaigns", pageCampaigns, pagePlay, LevelStandard},
    {"Machine Horde Tests", pageMachineHordeTests, pageDebugAutomation, LevelDeveloper},
    {"Graphics - Relighting", pageGraphicsRelighting, pageGraphics},
    {"Gore - Limb Gore", pageLimbGore, pageGore},
    {"Ragdolls - Vore", pageRagdollVore, pageRagdolls, LevelDeveloper},
    {"Ragdolls - Centroid", pageRagdollCentroid, pageRagdolls, LevelDeveloper},
    {"Dawn of the Machine Tests", pageMg3Tests, pageDebugAutomation, LevelDeveloper},
    {"Dawn of the Machine Weapons", pageMg3WeaponTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Monsters", pageMg3MonsterTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Bestiary", pageMg3BestiaryTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine: Shub", pageMg3ShubTests, pageDebugAutomation, LevelDeveloper},
    {"Dawn of the Machine: Chthon", pageMg3ChthonTests, pageDebugAutomation, LevelDeveloper},
    {"Debug - Cheats and Recording", pageDebugCheats, pageDebug, LevelDeveloper}, // (vr_menu_cheats.inc)
    {"Reloading", pageReloading, pageWeaponsHub},
    {"Reloading - Shotgun", pageReloadShotgun, pageReloading},
    {"Reloading - Super Shotgun", pageReloadSuperShotgun, pageReloading},
    {"Reloading - Nailgun", pageReloadNailgun, pageReloading},
    {"Reloading - Super Nailgun", pageReloadSuperNailgun, pageReloading},
    {"Reloading - Thunderbolt", pageReloadThunderbolt, pageReloading},
    {"Reloading - Launchers", pageReloadLaunchers, pageReloading},
    {"Reloading - Widowmaker", pageReloadWidowmaker, pageReloading},  // AD support
    {"Reloading - Plasma Gun", pageReloadPlasmaGun, pageReloading},   // AD support
    {"Reloading - Cell Cords", pageReloadCellCords, pageReloading},
    {"Per-Weapon Reload Overrides", pageReloadOverrides, pageReloading},
    {"Stealth AI", pageStealth, pageCombat}, // (vr_menu_stealth.inc)
    {"Stealth AI Tests", pageStealthTests, pageDebugAutomation, LevelDeveloper},
    {"Ragdoll Behaviour Tests", pageRagdollTests, pageDebugTests, LevelDeveloper}, // (vr_menu_ragtests.inc)
    {"Holding Enemies", pageHoldingEnemies, pageCombat},
    {"Holding Enemies - Struggle", pageHoldingStruggle, pageHoldingEnemies},
    {"Enemy Melee", pageEnemyMelee, pageCombat},
    {"Body - Crouched Pose", pageBodyCrouch, pageBody},
    {"Seated Play", pageSeated, pageVrsHeightHands, LevelStandard}, // (vr_seated.hpp; SEATED.md)
    {"Seated Holsters", pageSeatedHolsters, pageSeated, LevelStandard},
    {"Doom", pageDoom, pageGadget, LevelStandard},
    {"Arcane Dimensions", pageArcaneDimensions, pageAdvanced}, // AD support: dedicated tweaks page
    {"Arcane Dimensions Monsters", pageAdMonsterTests, pageDebugTests, LevelDeveloper}, // AD support (vr_menu_ad_monsters.inc)
    {"Arcane Dimensions' Guns", pageAdGuns, pageEnemyWeapons}, // AD support: the AD soldiers' guns (vr_ad_enemyguns.qc)
    {"Incandescent Blades", pageHotBlades, pageWeaponsHub}, // (QC vr_hotblade.qc)
    {"Blood Feedback", pageBloodFeedback, pageVrsComfortTurning, LevelStandard},
    // VR Settings' sections as pages (ROUND22.md, "VR Settings' sections as submenus"): vrSettingsParts.
    {"VR Settings - Height and Hand Calibration", pageVrsHeightHands, pageMain, LevelStandard},
    {"VR Settings - Body and Equipment", pageVrsBodyEquipment, pageMain, LevelStandard}, // (ROUND22.md, "Body and Equipment": the holsters and the ammo pouch in it)
    {"VR Settings - Ammo Pouch", pageVrsAmmoPouch, pageVrsBodyEquipment, LevelStandard}, // (its rows also on Body and Equipment)
    {"VR Settings - Locomotion and Controls", pageVrsLocoControls, pageMain, LevelStandard},
    {"VR Settings - Comfort, Teleport and Turning", pageVrsComfortTurning, pageMain, LevelStandard},
    {"VR Settings - Weapons and Bullet Time", pageVrsWeaponsBulletTime, pageMain, LevelStandard},
    {"VR Settings - Body and Flashlight", pageVrsBodyFlashlight, pageMain, LevelStandard},
    {"VR Settings - HUD, Haptics and Sound", pageVrsHudHapticsSound, pageMain, LevelStandard},
    {"VR Settings - Display and Graphics", pageVrsDisplayGraphics, pageMain, LevelStandard},
    {"VR Settings - Player Appearance", pageVrsAppearance, pageMain, LevelStandard}, // (ROUND22.md, "Gloves; Player Appearance")
    {"Ranger Head", pageVrsRangerHead, pageVrsAppearance, LevelStandard}, // (ROUND22.md, "The ranger's head: sliders")
    {"Debug - Automation & Self-Tests", pageDebugAutomation, pageDebug, LevelDeveloper}, // (vr_menu_automation.inc)
    {"Holster Zones", pageHolsterZones, pageImmersionSettings}, // (vr_holsterzones.hpp)
    {"Graphics - Loot Beams", pageGraphicsLootBeams, pageGraphics}, // (vr_lootbeams.cpp)
    {"Ragdoll Triggers", pageKdTriggers, pageCombat}, // (QC vr_kdtrig.qc)
    {"Ragdoll Triggers - Ledges", pageKdTrigLedge, pageKdTriggers},
    {"Ragdoll Triggers - Stairs", pageKdTrigStairs, pageKdTriggers},
    {"Ragdoll Triggers - Slopes", pageKdTrigSlope, pageKdTriggers},
    {"Ragdoll Triggers - Shotgun Range", pageKdTrigShotgun, pageKdTriggers},
    {"Ragdoll Triggers - Wounded", pageKdTrigWounded, pageKdTriggers},
    {"Ragdoll Triggers - Strong Blows", pageKdTrigBlow, pageKdTriggers},
    {"Ragdoll Triggers - Explosions", pageKdTrigBlast, pageKdTriggers},
    {"Ragdoll Triggers - Thrown Bodies", pageKdTrigThrown, pageKdTriggers},
    {"Ragdoll Triggers - Shoulder Charges", pageKdTrigTackle, pageKdTriggers},
    // Combat > Monster Groups (vr_menu_mgroups.inc): a page for each group, then one for each trigger and action.
    {"Monster Groups", pageMonsterGroups, pageCombat},
#define QVR_MGROUP(g, label, ...) {"Monster Groups - " label, pageMonsterGroup<mgroups::g##Group>, pageMonsterGroups},
#include "vr_mgroups.inc"
#undef QVR_MGROUP
    {"Monster Groups - Ragdoll Triggers", pageMonsterParam<0>, pageMonsterGroups},
    {"Monster Groups - Trigger Resistance", pageMonsterParam<1>, pageMonsterGroups},
    {"Monster Groups - Ledges", pageMonsterParam<2>, pageMonsterGroups},
    {"Monster Groups - Stairs", pageMonsterParam<3>, pageMonsterGroups},
    {"Monster Groups - Slopes", pageMonsterParam<4>, pageMonsterGroups},
    {"Monster Groups - Shotgun Range", pageMonsterParam<5>, pageMonsterGroups},
    {"Monster Groups - Wounded", pageMonsterParam<6>, pageMonsterGroups},
    {"Monster Groups - Strong Blows", pageMonsterParam<7>, pageMonsterGroups},
    {"Monster Groups - Explosions", pageMonsterParam<8>, pageMonsterGroups},
    {"Monster Groups - Thrown Bodies", pageMonsterParam<9>, pageMonsterGroups},
    {"Monster Groups - Tackles", pageMonsterParam<10>, pageMonsterGroups},
    {"Monster Groups - Shove Knockdown", pageMonsterParam<11>, pageMonsterGroups},
    {"Monster Groups - Shove Push", pageMonsterParam<12>, pageMonsterGroups},
    {"Monster Groups - Hits Unbalance", pageMonsterParam<13>, pageMonsterGroups},
    {"Monster Groups - Leg Sweep", pageMonsterParam<14>, pageMonsterGroups},
    {"Monster Groups - Leg Pull and Lift", pageMonsterParam<15>, pageMonsterGroups},
    {"Monster Groups - Lift", pageMonsterParam<16>, pageMonsterGroups},
    {"Monster Groups - Neck Snap", pageMonsterParam<17>, pageMonsterGroups},
    {"Monster Groups - Manhandle", pageMonsterParam<18>, pageMonsterGroups},
    {"Monster Groups - Two-Hand Throw", pageMonsterParam<19>, pageMonsterGroups},
    {"Monster Groups - Hold Strength", pageMonsterParam<20>, pageMonsterGroups},
    {"Monster Groups - Parry Poise", pageMonsterParam<21>, pageMonsterGroups},
    {"Monster Groups - Health", pageMonsterParam<22>, pageMonsterGroups},
    {"Monster Groups - Knockdown Chance", pageMonsterParam<23>, pageMonsterGroups},
    {"Monster Groups - Time Down", pageMonsterParam<24>, pageMonsterGroups},
    {"Monster Groups - Get-Up Speed", pageMonsterParam<25>, pageMonsterGroups},
    {"Monster Groups - Neck Snap Kills", pageMonsterParam<26>, pageMonsterGroups},
    {"Monster Groups - Struggle", pageMonsterParam<27>, pageMonsterGroups},
    {"Monster Groups - Struggle Strength", pageMonsterParam<28>, pageMonsterGroups},
    {"Monster Groups - Struggle Speed", pageMonsterParam<29>, pageMonsterGroups},
    {"Monster Groups - Struggle Arm Drive", pageMonsterParam<30>, pageMonsterGroups},
    {"Monster Groups - Struggle Weaker When Hurt", pageMonsterParam<31>, pageMonsterGroups},
    {"Monster Health", pageMonsterHealth, pageCombat}, // (vr_menu_mgroups.inc)
    {"Ragdoll Tool", pageRagtool, pageDebug}, // (vr_menu_ragtool.inc; vr_ragtool.cpp)
    {"Ragdoll Tool - Motors", pageRagtoolParams<0>, pageRagtool},
    {"Ragdoll Tool - Joint Groups", pageRagtoolParams<1>, pageRagtool},
    {"Ragdoll Tool - Stagger and Balance", pageRagtoolParams<2>, pageRagtool},
    {"Ragdoll Tool - Shove", pageRagtoolParams<3>, pageRagtool},
    {"Ragdoll Tool - Knockdown and Get-Up", pageRagtoolParams<4>, pageRagtool},
    {"Ragdoll Tool - Death and Crawl", pageRagtoolParams<5>, pageRagtool},
    {"Ragdoll Tool - Struggle", pageRagtoolParams<6>, pageRagtool},
    {"Ragdoll Tool - Parry and Hit Stagger", pageRagtoolParams<7>, pageRagtool},
    {"Ragdoll Tool - Flail, Liquids and Hand", pageRagtoolParams<8>, pageRagtool},
    {"Saves and Autosaves", pageSaves, pageMain, LevelStandard}, // (vr_menu_settings.inc; vr_continue.cpp)
    // (The author, 2026-10-22: the holsters' rows on their own page; Carry and Auto-Collect under Playing, then one Item Pickup Mode switch.)
    {"Holster Calibration", pageVrsHolsters, pageMain, LevelStandard}, // (2026-10-23: on VR Settings itself)
    {"VR Settings - Item Collection", pageVrsItemCollection, pageMain, LevelStandard},
    {"Armed Grunts (Experimental)", pageArmedGrunts, pageCombat}, // (vr_menu_gameplay.inc; QC vr_armedgrunt.qc)
    {"Graphics Presets", pageGraphicsPresets, pageMain, LevelStandard}, // (vr_menu_pages.inc; vr_gfxpresets.cpp)
    {"Graphics - Shoreline Foam", pageGraphicsFoam, pageGraphicsLiquids}, // (vr_menu_pages.inc; vr_water.cpp foamLook)
};
static_assert(mgroups::paramCount == 32, "a Monster Groups page for each trigger and action (the rows above)");
constexpr int pageCount = static_cast<int>(sizeof(pages) / sizeof(pages[0]));

// VR Settings' sections' pages, in its order: with it, the rows Reset All resets besides the vr_* settings, and the
// pages the wrappers (Turning Mode, Move Towards, the hands, the holsters) are synced on as they are built.
const PageBuilder vrSettingsParts[] = {pageVrsHeightHands, pageVrsBodyEquipment, pageVrsAmmoPouch, pageVrsHolsters,
    pageVrsLocoControls, pageVrsComfortTurning, pageVrsWeaponsBulletTime, pageVrsItemCollection, pageVrsBodyFlashlight, pageVrsAppearance, pageVrsHudHapticsSound,
    pageVrsDisplayGraphics};

[[nodiscard]] bool vrSettingsPage(int p)
{
    if(p == PageMain)
    {
        return true;
    }
    for(const PageBuilder b : vrSettingsParts)
    {
        if(pages[p].build == b)
        {
            return true;
        }
    }
    return false;
}

// The roots of the pages' tree as Search and the boards' menu paths walk it (breadth first, in this order): the VR
// Settings and the Advanced VR Options, each opened on its own (the VR Settings: Options' and the main menu's rows; the
// Advanced VR Options: the main menu's Advanced VR row and the corner's button), neither linking the other.
constexpr int menuRoots[] = {PageMain, PageAdvanced};
[[nodiscard]] constexpr bool isMenuRoot(int p)
{
    return p == PageMain || p == PageAdvanced;
}

// Weapon Offsets' parts, as its main page links them (in WeaponOffsetsPart's order). A new part: its builder, a
// WeaponOffsetsPart, a line in `pages` (last) and one here.
struct WeaponOffsetsPartPage
{
    PageBuilder build;
    const char* link;
    const char* help;
};
const WeaponOffsetsPartPage weaponOffsetsPartPages[] = {
    {pageWofsHand, "Hand and Grip",
        "Where the weapon sits in the hand and how it is turned; the hand and the weapon moved together, or the hand alone; "
        "the controller preview to tune them by."},
    {pageWofsFingers, "Fingers",
        "How the fingers wrap the weapon (on their own or set by hand), each finger's bias, the thumb's place."},
    {pageWofsSights, "Muzzle and Sights",
        "Align Sights to My Aim, where the shots start (Muzzle) and where they go (Shot Pitch and Yaw)."},
    {pageWofsTwoHanded, "Two-Handed and Hotspots",
        "Whether the other hand may hold it, where (its hotspots: grips, the blade, a cup) and how that hand is drawn "
        "there; the two-handed aim."},
    {pageWofsStock, "Virtual Stock",
        "How the aim turns while the weapon is steadied at your shoulder (2H Aiming: Virtual Stock)."},
    {pageWofsScreen, "Ammo Screen", "The ammunition screen on the weapon: where, how big, shown or hidden."},
    {pageWofsHolstered, "Holstered", "How it lies in each kind of holster (hips, chest, back), with a preview."},
    {pageWofsEffects, "Effects", "Recoil, a muzzle flash and tracers, for a model without its own; a test."},
    {pageWofsFlashlight, "Flashlight", "Where the flashlight sits clipped on this weapon and how it is turned, on top of "
        "every weapon's place (Flashlight > On a Gun or Head), with a preview."},
};
static_assert(sizeof(weaponOffsetsPartPages) / sizeof(weaponOffsetsPartPages[0]) == WofsParts - 1);

// Whether `build` is a Weapon Offsets page (the main one or a part): the weapon in hand, its state.
[[nodiscard]] bool weaponOffsetsPage(PageBuilder build)
{
    if(build == pageWeaponOffsets)
    {
        return true;
    }
    for(const WeaponOffsetsPartPage& part : weaponOffsetsPartPages)
    {
        if(part.build == build)
        {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Presets (Locomotion's Comfort, Body and Display's Handedness): a choice sets several settings at once; the choice shown is the one
// they match now (syncPresets, as the menu is drawn), Custom when none. Their cvars aren't saved: the settings are.
// ----------------------------------------------------------------------------

struct PresetSetting
{
    const char* cvar;
    float value;
};

struct PresetChoice
{
    float value;
    PresetSetting settings[3];
};

struct Preset
{
    cvar_t* choice;
    const PresetChoice* choices;
    int count;
};

constexpr PresetChoice comfortChoices[] = {
    {1.f, {{"vr_snap_turn", 45.f}, {"vr_teleport_enabled", 1.f}, {"cl_alwaysrun", 0.f}}}, // Comfortable
    {2.f, {{"vr_snap_turn", 30.f}, {"vr_teleport_enabled", 1.f}, {"cl_alwaysrun", 1.f}}}, // Moderate
    {3.f, {{"vr_snap_turn", 0.f}, {"vr_teleport_enabled", 0.f}, {"cl_alwaysrun", 1.f}}},  // Full Freedom
};

constexpr PresetChoice handednessChoices[] = {
    {1.f, {{"vr_stick_swap", 0.f}, {"vr_gadget_arm", 0.f}, {"vr_flashlight_side", 0.f}}}, // right-handed
    {2.f, {{"vr_stick_swap", 1.f}, {"vr_gadget_arm", 1.f}, {"vr_flashlight_side", 1.f}}}, // left-handed
};

const Preset presets[] = {
    {&vr_comfort_preset, comfortChoices, static_cast<int>(sizeof(comfortChoices) / sizeof(comfortChoices[0]))},
    {&vr_handedness, handednessChoices, static_cast<int>(sizeof(handednessChoices) / sizeof(handednessChoices[0]))},
};

bool presetBusy = false; // a preset's own changes (its settings set, its choice shown): not a choice made

[[nodiscard]] bool presetMatches(const PresetChoice& c)
{
    for(const PresetSetting& s : c.settings)
    {
        const cvar_t* var = Cvar_FindVar(s.cvar);
        if(!var || var->value != s.value)
        {
            return false;
        }
    }
    return true;
}

void syncPresets()
{
    presetBusy = true;
    for(const Preset& p : presets)
    {
        float shown = 0.f;
        for(int i = 0; i < p.count; i++)
        {
            if(presetMatches(p.choices[i]))
            {
                shown = p.choices[i].value;
                break;
            }
        }
        if(p.choice->value != shown)
        {
            Cvar_SetValueQuick(p.choice, shown);
        }
    }
    presetBusy = false;
}

void onPresetChosen(cvar_t* var)
{
    if(presetBusy)
    {
        return;
    }
    for(const Preset& p : presets)
    {
        for(int i = 0; p.choice == var && i < p.count; i++)
        {
            if(p.choices[i].value == var->value)
            {
                presetBusy = true;
                for(const PresetSetting& s : p.choices[i].settings)
                {
                    Cvar_SetValue(s.cvar, s.value);
                }
                presetBusy = false;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// One VR menu's own state (MULTI_MENU.md, stage 2): the page it shows, its Back stack, each page's selected row and
// scroll, what is under way (a slider or the scrollbar dragged, a slider held at its end, a drop-down list open, the help
// box and its paging, an armed Reset), as a MenuInstance; the code works on the current one (ui()); the panel's
// placement and canvas are in vr_panel.cpp (by menu::currentInstance()). Shared on purpose: the pages' built rows and
// their "built for" latches (a page shown twice shows the same rows), the settings themselves, Quake's own menus, and the
// keyboard pages (Search, Console, Map Library: their text input is one at a time, the console's line is Quake's own,
// and their state holds shared data: Search's index and recent list, the Map Library's caches). No setting's value is
// kept here: every row reads its setting as it draws.
// ----------------------------------------------------------------------------

constexpr int midPos = 204; // as Ironwail's OPTIONS_MIDPOS

constexpr int helpMinLines = 4;     // the help's box: four lines at least (Search's and the Map Library's)
constexpr int helpMaxLines = 7;     // and at most; a longer help turns pages in it (drawHelp)

// Back (ROUND21.md, "Back where you came from"): the way the player came to the page shown, as a stack of places, the page
// shown on top: VR pages (their numbers) and, below them, the menus outside the VR pages that they were entered from
// (outsidePlace: the main menu's VR Settings, Advanced VR or Select Campaign rows, Options > VR
// Settings, a corner button over any menu), and Ironwail's Levels entered from a VR page (the corner's Levels). Back pops
// the page shown and goes to the place under it, the cursor where it was left (each page's and menu's own). A place
// already on the stack is gone back to rather than added again (no loops). Nothing under the page: up the menus' tree
// (homeOf), the VR Settings to Options. A Search result's page goes back up the tree, not to Search (openEntry).
struct NavStack
{
    static constexpr int capacity = 48;
    int places[capacity]{};
    int count{0};
};

// Where a page was left: its selected row, found again by its label when the page is built anew (its
// rows changed: another weapon's offsets, a choice showing more settings or fewer) and at the next
// start (vr_menu_positions), and the line of the view it was on (the scroll).
struct RowAnchor
{
    bool valid{false};
    za::String section; // the header above it ("": none)
    za::String label;
    int index{0}; // where it was: of the rows alike, the nearest is taken
    int line{0};  // its line in the view (the cursor less the scroll)
};

// The help box's lines on the page shown: its rows' longest help, wrapped (helpMinLines to helpMaxLines). Worked out
// again when the page, its rows or the line's width change, not every frame.
struct HelpBox
{
    int page{-1};
    int build{-1};
    int columns{0};
    int lines{helpMinLines};
};

// A VR page's rows' leftmost text (menu x), for the corner's buttons (menu::contentLeft). Kept per page and build.
struct RowsLeft
{
    int page{-1};
    int build{-1};
    int left{0};
};

// A slider stepped by the keys held (stepSlider).
struct SliderHold
{
    const cvar_t* endCvar = nullptr; // stopped at a bar's end: which, which way, since when
    int endDir = 0;
    double endSince = 0.0;
    double outsideSince = 0.0; // held past the ends since
};

// A drop-down list open (a choice row's choices next to it: "Drop-down lists" below).
struct DropDown
{
    bool open{false};
    int page{-1};
    int row{-1};               // the row's index in the page's list
    const cvar_t* cvar{nullptr}; // its setting and how many choices it had (a rebuilt page checked against them)
    int count{0};
    int hover{0};  // the choice highlighted
    int scroll{0}; // the first choice shown
    bool scrollGrab{false};
    // Laid out as it opened: the inside's left, width and top (menu coordinates), its rows, its width in
    // characters (M_DrawTextBox), the longest label's.
    int x{0}, width{0}, top{0}, rows{0}, chars{0}, labelChars{0};
};

// The help shown, and since when (its pages turn from the first; a new text starts over).
struct HelpPaging
{
    za::U32 hash{0};
    double since{0.0};
    int shown{-1}; // the page of it shown (for the developer line when it turns)
};

// Where VR_Menu_Draw last noted the page to be (vr_menu_positions is written on a change only).
struct NotedPlace
{
    int page = -1, cursor = -1, scroll = -1, build = -1;
};

struct MenuInstance
{
    int page{PageMain}; // the page shown
    NavStack nav;
    bool navReturning{false}; // leaving the VR pages by Back for an outside menu (VR_NavEntered: not a new way in)
    int navJumpPending{-1};   // the outside menu a jump (VR_NavJump) is opening

    int cursors[pageCount]{};         // each page's selected row
    int scrolls[pageCount]{};         // and its first row shown
    RowAnchor leftAnchors[pageCount]; // each page's position as last shown (invalid: at the top)
    int visits[pageCount]{};          // when each page was last shown (a count; 0: not since the start)
    int visitCount{0};

    bool sliderGrab{false};     // a slider follows the mouse while its button is held
    float sliderGrabHold{-1.f}; // grabbed on a thumb pinned at an end (a value past it): where, kept until the mouse moves off
    bool scrollGrab{false};     // the scrollbar likewise
    SliderHold sliderHold;      // (stepSlider: the menu's keys)
    DropDown dropDown;
    HelpBox helpBox;
    HelpPaging helpPaging;
    RowsLeft rowsLeft;
    int scrollbarX{midPos + 188}; // where the scrollbar was drawn

    int resetArmedPage{-1};       // Reset This Page pressed once (the footer), on that page, then
    double resetArmedTime{0.0};
    bool resetAllArmed{false};    // Reset All to Defaults pressed once: Press Again to Reset All
    double resetAllArmedTime{0.0};

    int bodyCloneLatch{-1}; // the page a row tweaking the body was chosen on (bodyClonePage), while it stays shown
    NotedPlace notedPlace;

    // Several panels (MULTI_MENU.md, stage 3). Quake's menu state and the menus' mouse are Quake's globals (m_state,
    // m_mousex/y), the current instance's: the others keep theirs here (selectInstance swaps them).
    bool open{false};       // (the first: unused, open while the menu is)
    int quakeState{m_none}; // m_state while not current (the others: m_vr, or m_confirm over a VR page)
    float mouseX{-1000.f}, mouseY{-1000.f};
    int refusedPage{-1};    // a page for one panel at a time refused here (another panel shows it), and when: its title
    double refusedTime{0.0}; // says so for a moment
    int refusedBy{-1};
    float drawnValue{0.f}; // the selected row's value as last drawn, and when (vr_menu_panels: the panels kept in step)
    int drawnFrame{-1};
    int button{0}; // a button row's button picked (Item::buttons; kept from item to item: Pass, Pass, Pass...)
};

MenuInstance menuInstances[menu::instanceCount];
int currentMenu = 0;
int instanceDrawDepth = 0; // M_Draw's nesting (a dialog draws the menu under it again): only the outermost picks the panel
int drawnInstance = -1;    // drawing a panel's own canvas (menu::drawInstance): M_Draw draws that one

[[nodiscard]] MenuInstance& ui()
{
    return menuInstances[currentMenu];
}

// ----------------------------------------------------------------------------
// Wrappers (VR Settings' own rows over other settings, vr_cvars.inc vr_menu_*): Turning Mode, Move Towards, Item Pickup
// Mode and the one mirrored hand calibration. Their cvars aren't saved: each is shown as the settings it stands for are now (syncWrappers,
// as the pages are built and drawn) and, set, sets them (onWrapperSet). Their defaults are those settings' defaults, so
// Reset This Page and Reset All reset them too.
// ----------------------------------------------------------------------------

bool wrapperBusy = false;      // a wrapper shown (syncWrappers): not a choice made
float lastSnapAngle = 45.f;    // Turning Mode: Snap goes back to the angle last used (45 at first)
float lastAutocollect = 1.f;   // Item Pickup Mode: Auto-Collect goes back to its kind last used (2: backpacks too)
int mainPageSnap = -1;         // VR Settings built with snap turning (Snap Angle shown) or smooth (Turn Speed shown)

// One hand-calibration row: the main hand's setting, the off hand's (nullptr: vr_handcal_off_mirror mirrors it) and
// how the off hand's goes (-1: the other way, mirrored).
struct HandWrapper
{
    cvar_t* wrapper;
    cvar_t* main;
    cvar_t* off;
    float offSign;
};

const HandWrapper handWrappers[] = {
    {&vr_menu_hands_x, &vr_handcal_x, nullptr, 1.f},
    {&vr_menu_hands_y, &vr_handcal_y, nullptr, 1.f},
    {&vr_menu_hands_z, &vr_handcal_z, nullptr, 1.f},
    {&vr_menu_hands_pitch, &vr_gunangle, &vr_offhandpitch, 1.f},
    {&vr_menu_hands_yaw, &vr_gunyaw, &vr_offhandyaw, -1.f},
    {&vr_menu_hands_roll, &vr_handcal_roll, nullptr, 1.f},
};

[[nodiscard]] float defaultOf(const cvar_t& var)
{
    return var.default_string ? static_cast<float>(Q_atof(var.default_string)) : 0.f;
}

// One Holster Calibration row (VR Settings): a pair of holsters' offset (one setting for both, the left one mirrored)
// from its default, `sign` -1 for Inward (the offsets' Y is outward).
struct HolsterWrapper
{
    cvar_t* wrapper;
    cvar_t* offset;
    float sign;
};

const HolsterWrapper holsterWrappers[] = {
    {&vr_menu_holster_hip_x, &vr_hip_offset_x, 1.f},
    {&vr_menu_holster_hip_y, &vr_hip_offset_y, -1.f},
    {&vr_menu_holster_hip_z, &vr_hip_offset_z, 1.f},
    {&vr_menu_holster_chest_x, &vr_upper_holster_offset_x, 1.f},
    {&vr_menu_holster_chest_y, &vr_upper_holster_offset_y, -1.f},
    {&vr_menu_holster_chest_z, &vr_upper_holster_offset_z, 1.f},
    {&vr_menu_holster_back_x, &vr_shoulder_holster_offset_x, 1.f},
    {&vr_menu_holster_back_y, &vr_shoulder_holster_offset_y, -1.f},
    {&vr_menu_holster_back_z, &vr_shoulder_holster_offset_z, 1.f},
};

[[nodiscard]] bool wrapperCvar(const cvar_t& var)
{
    if(&var == &vr_menu_turning || &var == &vr_menu_move_towards || &var == &vr_menu_pickup_mode)
    {
        return true;
    }
    for(const HandWrapper& w : handWrappers)
    {
        if(&var == w.wrapper)
        {
            return true;
        }
    }
    for(const HolsterWrapper& w : holsterWrappers)
    {
        if(&var == w.wrapper)
        {
            return true;
        }
    }
    return &var == &vr_menu_bullettime;
}

void showWrapper(cvar_t& var, float value)
{
    if(za::fabs(var.value - value) > 1e-4f)
    {
        Cvar_SetValueQuick(&var, value);
    }
}

// Bullet Time's Activation (vr_menu_bullettime) as the settings it stands for are now.
[[nodiscard]] float bulletTimeActivation()
{
    if(vr_bullettime_enabled.value == 0.f)
    {
        return 3.f;
    }
    const int trigger = static_cast<int>(vr_bullettime_trigger.value);
    if(trigger == 1 || trigger == 2)
    {
        return static_cast<float>(trigger);
    }
    return vr_bullettime_tap.value != 0.f ? 0.f : 6.f;
}

// Its choices: the three, and the settings' own combination where it is none of them (shown, not offered otherwise).
[[nodiscard]] za::Vector<Choice> bulletTimeChoices()
{
    za::Vector<Choice> out{{0.f, "Wrist Gadget"}, {1.f, "Left Thumbstick Press"}, {2.f, "Right Thumbstick Press"}};
    switch(static_cast<int>(bulletTimeActivation()))
    {
        case 3: out.pushBack({3.f, "Off"}); break;
        case 6: out.pushBack({6.f, "Wrist Gadget (screen tap off)"}); break;
        default: break;
    }
    return out;
}

void syncWrappers()
{
    wrapperBusy = true;
    if(vr_snap_turn.value > 0.f)
    {
        lastSnapAngle = vr_snap_turn.value;
    }
    // Turning Mode: Smooth, Snap, Off (vr_enable_joystick_turn 0: the stick never turns you).
    showWrapper(vr_menu_turning, vr_enable_joystick_turn.value == 0.f ? 2.f : vr_snap_turn.value > 0.f ? 1.f : 0.f);
    const int mode = static_cast<int>(vr_movement_mode.value);
    showWrapper(vr_menu_move_towards, mode >= 1 && mode <= 3 ? static_cast<float>(mode)
                                      : hands::moveHand() == HAND_OFF ? 2.f : 3.f);
    for(const HandWrapper& w : handWrappers)
    {
        showWrapper(*w.wrapper, za::round((w.main->value - defaultOf(*w.main)) * 100.f) / 100.f);
    }
    for(const HolsterWrapper& w : holsterWrappers)
    {
        showWrapper(*w.wrapper, za::round(w.sign * (w.offset->value - defaultOf(*w.offset)) * 100.f) / 100.f);
    }
    showWrapper(vr_menu_bullettime, bulletTimeActivation());
    if(vr_carry_autocollect.value != 0.f)
    {
        lastAutocollect = vr_carry_autocollect.value >= 2.f ? 2.f : 1.f;
    }
    showWrapper(vr_menu_pickup_mode, vr_carry.value == 0.f ? 2.f : vr_carry_autocollect.value != 0.f ? 1.f : 0.f);
    wrapperBusy = false;
}

void onWrapperSet(cvar_t* var)
{
    if(wrapperBusy)
    {
        return;
    }
    wrapperBusy = true;
    if(var == &vr_menu_turning)
    {
        Cvar_SetValueQuick(&vr_enable_joystick_turn, var->value == 2.f ? 0.f : 1.f);
        if(var->value != 2.f) // (Off leaves the snap angle as it is: back to Smooth or Snap, it is as it was)
        {
            Cvar_SetValueQuick(&vr_snap_turn, var->value != 0.f ? lastSnapAngle : 0.f);
        }
    }
    else if(var == &vr_menu_move_towards)
    {
        Cvar_SetValueQuick(&vr_movement_mode, CLAMP(1.f, var->value, 3.f));
    }
    for(const HandWrapper& w : handWrappers)
    {
        if(var != w.wrapper)
        {
            continue;
        }
        Cvar_SetValueQuick(w.main, defaultOf(*w.main) + var->value);
        if(w.off)
        {
            Cvar_SetValueQuick(w.off, defaultOf(*w.off) + w.offSign * var->value);
        }
        else if(vr_handcal_off_mirror.value == 0.f)
        {
            Cvar_SetValueQuick(&vr_handcal_off_mirror, 1.f); // one set for both hands: the off hand mirrors the main one
        }
    }
    for(const HolsterWrapper& w : holsterWrappers)
    {
        if(var == w.wrapper)
        {
            Cvar_SetValueQuick(w.offset, defaultOf(*w.offset) + w.sign * var->value);
        }
    }
    if(var == &vr_menu_bullettime)
    {
        // A stick: its press alone (the gadget's screen tap does nothing then, as Combat > Bullet Time's Trigger).
        // The gadget: its screen tap. A config's own combination (the tap off): as it was.
        const int choice = static_cast<int>(var->value);
        Cvar_SetValueQuick(&vr_bullettime_enabled, choice == 3 ? 0.f : 1.f);
        if(choice != 3)
        {
            Cvar_SetValueQuick(&vr_bullettime_trigger, choice == 1 || choice == 2 ? static_cast<float>(choice) : 0.f);
        }
        if(choice == 0)
        {
            Cvar_SetValueQuick(&vr_bullettime_tap, 1.f);
        }
    }
    if(var == &vr_menu_pickup_mode)
    {
        // Immersive and Auto-Collect carry boxes (Auto-Collect's kind: ammo and health, or backpacks too, as Backpacks
        // Too last had it); Vanilla leaves Auto-Collect as it is (no effect without carrying).
        const int mode = static_cast<int>(var->value);
        Cvar_SetValueQuick(&vr_carry, mode == 2 ? 0.f : 1.f);
        if(mode != 2)
        {
            Cvar_SetValueQuick(&vr_carry_autocollect, mode == 1 ? lastAutocollect : 0.f);
        }
    }
    wrapperBusy = false;
}

// Reset Hand Offsets: both hands' calibration (moves, turns, the off hand mirrored or its own) back to the shipped one.
void resetHandOffsets()
{
    for(cvar_t* var : {&vr_handcal_x, &vr_handcal_y, &vr_handcal_z, &vr_handcal_roll, &vr_handcal_off_mirror,
            &vr_handcal_off_x, &vr_handcal_off_y, &vr_handcal_off_z, &vr_handcal_off_roll, &vr_gunangle, &vr_gunyaw,
            &vr_offhandpitch, &vr_offhandyaw})
    {
        Cvar_SetQuick(var, var->default_string);
    }
}

// Holster Setup (drag and drop): the holsters placed by hand (vr_holstersetup.cpp), this page reopened on leaving.
void holsterSetupStart()
{
    holstersetup::start(qvr::menu::currentPage());
}

// Reset Holsters: every pair of holsters back to its shipped place (Holster Calibration).
void resetHolsters()
{
    for(const HolsterWrapper& w : holsterWrappers)
    {
        Cvar_SetQuick(w.offset, w.offset->default_string);
    }
}

// Reset All to Defaults keeps the config's bookkeeping (its versions, the tips seen, VR Calibration pending, where the
// pages were left, Menu Detail), VR on or off and its runtime, and what was measured or fitted to the player rather than
// chosen (Vittorio, 2026-10-07): the height and the floor (Height Calibration), Body Calibration's measurements, its
// seated flag and Undo (vr_bodycal_*, its preview excepted) and the tweaks on them (vr_body_tweak_*), the body's
// proportions (arm length, eyes over the neck, torso back), and both hands' calibration (vr_handcal_*, the hands'
// pitch and yaw: Reset Hand Offsets resets those). The world's scale is a choice: reset.
[[nodiscard]] bool keptOnResetAll(const cvar_t& var)
{
    const size_t n = strlen(var.name);
    if(n > 8 && !strcmp(var.name + n - 8, "_version"))
    {
        return true;
    }
    if((!q_strncasecmp(var.name, "vr_bodycal_", 11) && &var != &vr_bodycal_preview) ||
        !q_strncasecmp(var.name, "vr_body_tweak_", 14) || !q_strncasecmp(var.name, "vr_handcal_", 11))
    {
        return true;
    }
    for(const cvar_t* kept : {&vr_menu_level, &vr_menu_positions, &vr_tips_seen, &vr_setup_pending, &vr_enabled,
            &vr_xr_runtime, &vr_height_calibration, &vr_floor_offset, &vr_body_arm_length, &vr_body_eye_forward,
            &vr_body_eye_up, &vr_body_torso_back, &vr_gunangle, &vr_gunyaw, &vr_offhandpitch, &vr_offhandyaw})
    {
        if(&var == kept)
        {
            return true;
        }
    }
    return false;
}

void resetAll(); // (below: after the pages' building)
[[nodiscard]] int menuLevel();

// The Advanced VR Options page's last line: this build (VR_BuildVersion), to name in a bug report.
[[nodiscard]] const char* buildVersionLine()
{
    char(&text)[64] = readouts.buildVersion;
    q_snprintf(text, sizeof(text), "Quake VR build %s", VR_BuildVersion());
    return text;
}

// VR Settings' Reloading Mode (vr_reload_mode): Immersive (3), Simple (2, the hip holsters), Disabled (0); a config's
// All Holsters (1) shown as such while it is set (not offered otherwise).
[[nodiscard]] za::Vector<Choice> reloadChoices()
{
    za::Vector<Choice> out{{3.f, "Immersive"}, {2.f, "Simple"}};
    if(static_cast<int>(vr_reload_mode.value) == 1)
    {
        out.pushBack({1.f, "Simple (all holsters)"});
    }
    out.pushBack({0.f, "Disabled"});
    return out;
}

#include "vr_menu_settings.inc" // VR Settings

#include "vr_menu_wofs.inc" // Weapon Offsets

[[nodiscard]] constexpr int outsidePlace(int state)
{
    return -1 - state;
}

[[nodiscard]] int navTop()
{
    return ui().nav.count > 0 ? ui().nav.places[ui().nav.count - 1] : -1000;
}

void navReset()
{
    ui().nav.count = 0;
}

// `place` on top: back to it where it already is on the stack (what was above it dropped), else added.
void navPush(int place)
{
    for(int i = ui().nav.count - 1; i >= 0; i--)
    {
        if(ui().nav.places[i] == place)
        {
            ui().nav.count = i + 1;
            return;
        }
    }
    if(ui().nav.count == NavStack::capacity)
    {
        for(int i = 1; i < ui().nav.count; i++)
        {
            ui().nav.places[i - 1] = ui().nav.places[i];
        }
        ui().nav.count--;
    }
    ui().nav.places[ui().nav.count++] = place;
}

// The menu outside the VR pages the player is in now (m_none: none, or one not to come back to).
[[nodiscard]] int outsideMenu()
{
    if(key_dest != key_menu)
    {
        return m_none;
    }
    switch(m_state)
    {
        case m_none:
        case m_vr:
        case m_credits:
        case m_quit:
        case m_confirm:
        case m_help: return m_none;
        default: return m_state;
    }
}

// Coming to the VR pages from `outside` (outsideMenu): it at the stack's bottom (kept with what is under it when it is
// on top already: the player came back to it by Back), or nothing when there is no menu to go back to.
void navEnterFrom(int outside)
{
    if(outside == m_none)
    {
        navReset();
        return;
    }
    if(navTop() != outsidePlace(outside))
    {
        navReset();
        navPush(outsidePlace(outside));
    }
}

// The page last shown, on top of the stack (made so when something else changed the page).
void navSyncTop()
{
    if(navTop() != ui().page)
    {
        navPush(ui().page);
    }
}

[[nodiscard]] const char* rowLabel(const Item& item)
{
    return item.label ? item.label : "";
}

[[nodiscard]] const char* rowSection(const za::Vector<Item>& list, int i)
{
    for(; i >= 0; i--)
    {
        if(list[i].kind == Item::Header)
        {
            return rowLabel(list[i]);
        }
    }
    return "";
}

[[nodiscard]] RowAnchor anchorOf(const za::Vector<Item>& list, int cursor, int scroll)
{
    if(cursor < 0 || cursor >= static_cast<int>(list.size()) || !selectable(list[cursor]))
    {
        return {};
    }
    return {true, rowSection(list, cursor), rowLabel(list[cursor]), cursor, cursor - scroll};
}

// The row `a` was on in `list` (-1: gone): of the settings with its label, one under the same header
// before others, then the nearest to where it was.
[[nodiscard]] int findRow(const za::Vector<Item>& list, const RowAnchor& a)
{
    int best = -1;
    int bestCost = 0;
    for(int i = 0; i < static_cast<int>(list.size()); i++)
    {
        if(!selectable(list[i]) || a.label != rowLabel(list[i]))
        {
            continue;
        }
        const int cost = za::abs(i - a.index) + (a.section == rowSection(list, i) ? 0 : 1 << 20);
        if(best < 0 || cost < bestCost)
        {
            best = i;
            bestCost = cost;
        }
    }
    return best;
}

// vr_menu_positions: the pages' anchors as the last start left them ("title|section|label|index|line",
// separated by ';', the most recently shown first), read at the menu's first use, each used as its
// page is first built; and written with the config (VR_MenuSavePositions): the pages shown since,
// then those left from before.
RowAnchor savedAnchors[pageCount];
int savedRank[pageCount]{}; // its place in the cvar

int builds[pageCount]{};          // how many times each page's list was built (its rows may have changed)
za::String positionsParsed;      // vr_menu_positions as last read (loadPositions)
bool positionsLoaded = false;

// The pages' items, built on first use and again when a page's rows change (items()). Never released: the menu's
// cursors and anchors are kept by row (and PageTexts with them).
struct MenuPages
{
    za::Vector<Item> built[pageCount];
    bool done[pageCount]{};
    int level[pageCount]{}; // the menu detail level each was built for
    int cvars[pageCount]{}; // how many cvars there were (Cvar_Count): a row whose cvar came later, left out till then
    auto members() { return qvr::mem::list(built, done, level, cvars); }
};
mem::Cache<MenuPages> menuPages{"menu pages", mem::Never};

void loadPositions()
{
    // Read again while no page has been shown, should the cvar change (the config executed after a
    // write, the console).
    if(positionsLoaded && (ui().visitCount > 0 || positionsParsed == vr_menu_positions.string))
    {
        return;
    }
    positionsLoaded = true;
    positionsParsed = vr_menu_positions.string;
    for(int p = 0; p < pageCount; p++)
    {
        savedAnchors[p] = {};
    }

    int rank = 0;
    const za::String& text = positionsParsed;
    for(size_t start = 0; start < text.size();)
    {
        size_t end = text.find(';', start);
        end = end == za::StringView::nPos ? text.size() : end;
        za::Vector<za::String> fields;
        for(size_t f = start; f <= end;)
        {
            const size_t bar = za::min(text.find('|', f), end);
            fields.pushBack(text.substrByPosLen(f, bar - f));
            f = bar + 1;
        }
        start = end + 1;
        if(fields.size() != 5)
        {
            continue;
        }
        for(int p = 0; p < pageCount; p++)
        {
            if(!savedAnchors[p].valid && fields[0] == pages[p].title)
            {
                savedAnchors[p] = {true, fields[1], fields[2], Q_atoi(fields[3].cStr()), Q_atoi(fields[4].cStr())};
                savedRank[p] = rank++;
                break;
            }
        }
    }
}

// Built on first use (cvars looked up by name exist by then); items without their cvar dropped.
// A setting changed from its default (a * by its label; Changed Settings): its value, or for text its string. The
// menus' own state and the presets (whose settings show it themselves) aren't.
[[nodiscard]] bool changedSetting(const cvar_t& var)
{
    if(&var == &vr_menu_level || &var == &vr_comfort_preset || &var == &vr_handedness || &var == &vr_graphics_preset ||
        !var.default_string)
    {
        return false;
    }
    char* end = nullptr;
    const double v = strtod(var.string, &end);
    const bool number = end != var.string && *end == '\0';
    char* defEnd = nullptr;
    const double d = strtod(var.default_string, &defEnd);
    if(number && defEnd != var.default_string && *defEnd == '\0')
    {
        return static_cast<float>(v) != static_cast<float>(d);
    }
    return strcmp(var.string, var.default_string) != 0;
}

// Reset This Page (the footer): armed by a first press, done by a second within 3 seconds (MenuInstance::resetArmed*).

// The pages of one weapon's or prop's settings (their own resets, Reset This Weapon...): no Reset This Page, and not on
// Changed Settings (the held one's values, not settings of their own).
[[nodiscard]] bool slotPage(PageBuilder build);

// The menu detail level the pages are built for (vr_menu_level; resolvePath: every page, LevelDeveloper).
int levelOverride = -1;

[[nodiscard]] int menuLevel()
{
    return levelOverride >= 0 ? levelOverride : CLAMP(0, static_cast<int>(vr_menu_level.value), LevelCount - 1);
}

[[nodiscard]] const char* levelName(int level)
{
    return level <= LevelStandard ? "Standard" : level == LevelAdvanced ? "Advanced" : "Developer";
}

// Whether `item` shows at `level`: its own level, and a link's page's.
[[nodiscard]] bool shownAt(const Item& item, int level)
{
    if(item.level > level)
    {
        return false;
    }
    const bool link = item.kind == Item::Action && item.page >= 0 && item.page < pageCount && !item.actionArg;
    return !link || pages[item.page].level <= level;
}

// The page's rows as shown: a header with nothing left under it (its rows all above the level) goes too.
void dropEmptyHeaders(za::Vector<Item>& list)
{
    za::Vector<Item> kept;
    for(size_t i = 0; i < list.size(); i++)
    {
        if(list[i].kind == Item::Header && (i + 1 == list.size() || list[i + 1].kind == Item::Header))
        {
            continue;
        }
        kept.pushBack(ZA_MOVE(list[i]));
    }
    list = ZA_MOVE(kept);
}

void resetThisPage();

// Under every page: Reset This Page (a page with settings), and Menu Detail, the level the pages are shown at.
void addMenuDetail(za::Vector<Item>& list, int page)
{
    const bool settings = za::anyOf(list.begin(), list.end(), [](const Item& item) {
        return item.cvar && item.kind != Item::Action && item.cvar != &vr_menu_level;
    });
    if(pages[page].build == pageSearch || pages[page].build == pageConsole || pages[page].build == pageMaps)
    {
        return; // (drawn their own way: vr_menu_search.inc, vr_menu_console.inc, vr_menu_maps.inc)
    }
    if(settings && !slotPage(pages[page].build) && pages[page].build != pageChanged)
    {
        list.pushBack(header("This Page"));
        list.pushBack(action(ui().resetArmedPage == page ? "Press Again to Reset" : "Reset This Page", resetThisPage)
                .help("Every setting on this page (as Menu Detail shows it) back to its default. Press it twice: the second "
                      "time within 3 seconds."));
    }
    list.pushBack(header("Menu Detail"));
    list.pushBack(cycle("Menu Detail", vr_menu_level, {{0.f, "Standard"}, {1.f, "Advanced"}, {2.f, "Developer"}})
            .help("How much the VR pages show. Standard: what every player sets (comfort, height, the HUD, the headset, "
                  "volume). Advanced: every gameplay, display and graphics setting. Developer: also the tuning pages "
                  "(weapon and prop offsets and weights, ragdolls, hitboxes), recording, debug and tests."));
}

[[nodiscard]] const za::Vector<Item>& items(int page)
{
    za::Vector<Item> (&built)[pageCount] = menuPages.built;
    bool (&done)[pageCount] = menuPages.done;
    const bool weaponOffsets = weaponOffsetsPage(pages[page].build);
    if(vrSettingsPage(page))
    {
        syncWrappers(); // Turning Mode, Move Towards, the hands: as their settings are now
    }
    if(pages[page].build == pageVrsComfortTurning && mainPageSnap >= 0 &&
        mainPageSnap != (vr_enable_joystick_turn.value == 0.f ? 2 : vr_snap_turn.value > 0.f ? 1 : 0))
    {
        done[page] = false; // Snap Angle shown, or Turn Speed
    }
    if(page == PageMain)
    {
        if(ui().resetAllArmed && realtime - ui().resetAllArmedTime > 3.0)
        {
            ui().resetAllArmed = false; // Reset All to Defaults not pressed again in time
            done[page] = false;
        }
    }
    if(weaponOffsets && weaponOffsetsSlot >= 0 && editedHotspot() == weaponOffsetsHotspot &&
        weaponOffsetsHotspotType == static_cast<int>(weapons::HotspotType::None) &&
        weapons::hotspot(weaponOffsetsSlot, editedHotspot()).type != weapons::HotspotType::None)
    {
        allowTwoHands(weaponOffsetsSlot); // a hotspot's Type set from None
    }
    if(weaponOffsets && weaponOffsetsSlot >= 0 &&
        ((weaponOffsetsHotspot >= 0 && editedHotspot() != weaponOffsetsHotspot) ||
            (weaponOffsetsHotspotType >= 0 &&
                static_cast<int>(weapons::hotspot(weaponOffsetsSlot, editedHotspot()).type) != weaponOffsetsHotspotType) ||
            weapons::inheritsFrom(weaponOffsetsHeldSlot) != weaponOffsetsInherit ||
            (weaponOffsetsHolster >= 0 && static_cast<int>(editedHolster()) != weaponOffsetsHolster) ||
            (weaponOffsetsManual >= 0 && (weapons::value(weaponOffsetsSlot, weapons::Key::FingerManual) >= 0.5f ? 1 : 0) != weaponOffsetsManual) ||
            (weaponOffsetsPreviewOwn >= 0 && (vr_show_controller_off_own.value != 0.f ? 1 : 0) != weaponOffsetsPreviewOwn) ||
            (weaponOffsetsHotspotManual >= 0 &&
                (weapons::hotspot(weaponOffsetsSlot, editedHotspot()).manual ? 1 : 0) != weaponOffsetsHotspotManual)))
    {
        weaponOffsetsStale = true; // another hotspot picked, its type changed, what the weapon inherits, or a Fingers choice
    }
    if(pages[page].build == pageWofsSights && weaponOffsetsSightVersion != sightalign::version())
    {
        weaponOffsetsStale = true; // Align Sights to My Aim: its phase or its result changed
        weaponOffsetsSightFocus = true;
    }
    if(pages[page].build == pageGraphicsRelighting && relightToolState >= 0 && relightToolState != relight::toolPageState())
    {
        done[page] = false; // light.exe found or not, a download started or ended: Download ericw-tools shown or not
    }
    if(pages[page].build == pageCampaigns && campaignsBloodyShown >= 0 &&
        campaignsBloodyShown != (vr_mg3_bn_discovered.value != 0.f ? 1 : 0))
    {
        done[page] = false; // Dawn of the Machine's Bloody Nightmare found (or reset): its row shown or not
    }
    if(pages[page].build == pageBodyArms && armsPageCalibrated >= 0 && armsPageCalibrated != (bodycal::calibrated() ? 1 : 0))
    {
        done[page] = false; // calibrated (Apply) or not (Undo): Arm Length shown or not
    }
    if(pages[page].build == pageBodyCalibration &&
        (bodycalVersion != bodycal::version() || bodycalSeated != (vr_bodycal_seated.value != 0.f ? 1 : 0)))
    {
        done[page] = false; // Body Calibration: its phase, its result or its poses changed
    }
    if(pages[page].build == pageChecklist)
    {
        checklist::refresh(); // (the file looked at once a second)
        if(checklistGeneration != checklist::generation() || checklistHidden != (vr_checklist_hide_ticked.value != 0.f ? 1 : 0))
        {
            done[page] = false; // an item ticked, Hide Ticked, or the file edited
        }
    }
    if(weightPageStale(pages[page].build))
    {
        done[page] = false;
    }
    if(weaponOffsets && weaponOffsetsStale)
    {
        weaponOffsetsStale = false;
        done[page] = false;
    }
    if((pages[page].build == pageFlashlightLowGrip && flashlightPageManual[0] >= 0 &&
           (vr_flashlight_low_fingers.value >= 0.5f ? 1 : 0) != flashlightPageManual[0]) ||
        (pages[page].build == pageFlashlightOverheadGrip && flashlightPageManual[1] >= 0 &&
            (vr_flashlight_high_fingers.value >= 0.5f ? 1 : 0) != flashlightPageManual[1]))
    {
        done[page] = false; // a grip's Fingers choice: its curls or its overlap shown
    }
    if(pages[page].build == pageHandGunCalibration && handCalPageOwn >= 0 &&
        (vr_handcal_off_mirror.value == 0.f ? 1 : 0) != handCalPageOwn)
    {
        done[page] = false; // the off hand's own sliders shown or not
    }
    if(pages[page].build == pageDebugTests && testsPageSpawn != static_cast<int>(vr_test_spawn.value))
    {
        done[page] = false; // another Thing chosen (or set by a spawn row): Put It There locked or not
    }
    if(pages[page].build == pageVrsItemCollection && itemCollectionPageMode != static_cast<int>(vr_menu_pickup_mode.value))
    {
        done[page] = false; // another pickup mode: Backpacks Too dimmed or not
    }
    if(pages[page].build == pageMotionRecorder && motionPageCategory != static_cast<int>(vr_motion_category.value))
    {
        done[page] = false; // the Detail choice is the category's
    }
    if((pages[page].build == pageReviewTakes && reviewListGeneration != motion::review::generation()) ||
        (pages[page].build == pageReviewTake && (reviewTakeGeneration != motion::review::generation() ||
                                                    reviewRelabelCategory != static_cast<int>(vr_motion_relabel_category.value))))
    {
        done[page] = false; // the list, or the take picked, changed
    }
    if(menuPages.level[page] != menuLevel())
    {
        done[page] = false; // Menu Detail changed: rows and links shown or left out
    }
    if(menuPages.cvars[page] != Cvar_Count())
    {
        done[page] = false; // cvars registered since (a row naming one was left out)
    }
    if(ui().resetArmedPage == page && realtime - ui().resetArmedTime > 3.0)
    {
        ui().resetArmedPage = -1; // Reset This Page not pressed again in time
        done[page] = false;
    }
    // (A check above: done[page] = false, and the page is built anew here, its selected row kept.)
    if(!done[page])
    {
        // The row selected: in the list about to be replaced; on the first build, where the last start
        // left the page (vr_menu_positions).
        loadPositions();
        RowAnchor anchor = anchorOf(built[page], ui().cursors[page], ui().scrolls[page]);
        if(built[page].empty())
        {
            anchor = savedAnchors[page];
            savedAnchors[page] = {};
        }
        built[page].clear();
        done[page] = true;
        builds[page]++;
        const int level = menuLevel();
        menuPages.level[page] = level;
        menuPages.cvars[page] = Cvar_Count();
        for(Item& item : pages[page].build())
        {
            if(item.kind != Item::Header && item.kind != Item::Action && item.kind != Item::Info && !item.cvar)
            {
                // A setting named by a cvar that doesn't exist (renamed, removed): left out, but said (once a page).
                if(builds[page] == 1)
                {
                    Con_DWarning("menu: \"%s\" on %s names no cvar: left out\n", item.label ? item.label : "?", pages[page].title);
                }
                continue;
            }
            if(shownAt(item, level))
            {
                built[page].pushBack(ZA_MOVE(item));
            }
        }
        dropEmptyHeaders(built[page]);
        addMenuDetail(built[page], page);
        // The same row again, on the same line of the view; gone, the cursor kept where it was (on a
        // setting).
        const int n = static_cast<int>(built[page].size());
        int& cursor = ui().cursors[page];
        if(const int row = anchor.valid ? findRow(built[page], anchor) : -1; row >= 0)
        {
            cursor = row;
            ui().scrolls[page] = q_max(row - anchor.line, 0);
        }
        cursor = CLAMP(0, cursor, q_max(n - 1, 0));
        for(int i = cursor; n > 0 && !selectable(built[page][cursor]) && i >= 0; i--)
        {
            cursor = selectable(built[page][i]) ? i : cursor;
        }
    }
    // Once the page is shown again (the menu reopened on it, its cursor restored): the cursor on Apply or Undo.
    if(pages[page].build == pageWofsSights && weaponOffsetsSightFocus && key_dest == key_menu && m_state == m_vr &&
        page == qvr::menu::currentPage())
    {
        weaponOffsetsSightFocus = false;
        for(int i = 0; i < static_cast<int>(built[page].size()); i++)
        {
            if(built[page][i].action == sightAlignApply || built[page][i].action == sightAlignUndo)
            {
                ui().cursors[page] = i;
                break;
            }
        }
    }
    return built[page];
}


// Where things go, in the menu's height (menuui::menuHeight: Quake's 200 on the desktop, more in
// a headset, Quake's 320 x 200 in its middle): the title at the top, the list under it, and the
// help under the list on pages with any: four lines, more (helpMaxLines) on a page whose rows'
// help is longer (helpBoxLines), the list a row shorter for each.
struct Layout
{
    int top;
    int listTop;
    int helpTop;
    int bottom;
};

constexpr int helpMaxWrapped = 64;  // lines wrapped at most (the longest help is far shorter)

// The help's line, in characters: the canvas's width (the flat menu's is 420, a headset's 320 or more), 50 at most.
[[nodiscard]] int helpColumns()
{
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    float cl, ct, cr, cb;
    Draw_GetTransformBounds(&t, &cl, &ct, &cr, &cb);
    return CLAMP(38, static_cast<int>((cr - cl - 16.f) / 8.f), 50);
}

// `text` word-wrapped to `columns`: the lines' count, and each line's start and length in `starts` and `lengths`
// (when given; helpMaxWrapped lines at most).
int wrapHelp(const char* text, int columns, int* starts = nullptr, int* lengths = nullptr)
{
    int line = 0;
    const char* p = text;
    while(*p && line < helpMaxWrapped)
    {
        while(*p == ' ')
        {
            p++;
        }
        if(!*p)
        {
            break;
        }
        int n = static_cast<int>(strlen(p));
        if(n > columns)
        {
            n = columns;
            while(n > 0 && p[n] != ' ')
            {
                n--;
            }
            if(n == 0)
            {
                n = columns;
            }
        }
        if(starts)
        {
            starts[line] = static_cast<int>(p - text);
            lengths[line] = n;
        }
        p += n;
        line++;
    }
    return line;
}

[[nodiscard]] const char* itemHelp(const Item& item);

// The help box's lines on the page shown: its rows' longest help, wrapped (helpMinLines to helpMaxLines). Worked out
// again when the page, its rows or the line's width change, not every frame (MenuInstance::helpBox).
[[nodiscard]] int helpBoxLines()
{
    const int columns = helpColumns();
    if(ui().helpBox.page != ui().page || ui().helpBox.build != builds[ui().page] || ui().helpBox.columns != columns)
    {
        ui().helpBox.page = ui().page;
        ui().helpBox.build = builds[ui().page];
        ui().helpBox.columns = columns;
        int longest = 0;
        for(const Item& item : menuPages.built[ui().page])
        {
            const char* help = itemHelp(item);
            longest = help ? q_max(longest, wrapHelp(help, columns)) : longest;
        }
        ui().helpBox.lines = CLAMP(helpMinLines, longest, helpMaxLines);
    }
    return ui().helpBox.lines;
}

// The console page's left and right edges (menu x; vr_menu_console.inc).
void consoleEdges(float& left, float& right);

// How far right a page draws anywhere (menu x): its rows' scrollbar's box, its help box at its widest (drawHelp: centred,
// its paging bar right of it); Search's, the Map Library's and the console's keyboards and buttons.
[[nodiscard]] float pageRight(za::Vector<Item> (*build)())
{
    if(build == pageConsole)
    {
        float left, right;
        consoleEdges(left, right);
        return right;
    }
    if(build == pageMaps || build == pageSearch)
    {
        return build == pageMaps ? 12.f + 460.f : 12.f + 440.f;
    }
    return za::fmax(static_cast<float>(midPos + 200), static_cast<float>((320 + 8 * helpColumns()) / 2 + 5));
}

[[nodiscard]] Layout layout()
{
    const int height = menuui::menuHeight();
    const int top = (200 - height) / 2;
    // The version label in the bottom right corner (vr_menubrand.cpp): where the page reaches under it (a flat screen's
    // narrower canvas), the page ends above it.
    int bottom = top + height;
    if(float labelLeft, labelTop; menuui::versionLabelClearance(labelLeft, labelTop) && pageRight(pages[ui().page].build) > labelLeft)
    {
        bottom = q_min(bottom, static_cast<int>(za::floor(labelTop)));
    }
    // Under the title, or below the corner's buttons where they are over the rows (not left of them: a narrow panel),
    // and below the status box in the top right corner where the page reaches under it (Search and the Map Library,
    // from x 12 440 and 460 across; the other pages to their scrollbar).
    int listTop = menuui::toolbarBeside() ? top + 36 : q_max(top + 36, static_cast<int>(za::ceil(menuui::toolbarBottom())) + 2);
    const auto build = pages[ui().page].build;
    const float right = build == pageMaps ? 12.f + 460.f : build == pageSearch ? 12.f + 440.f : midPos + 200.f;
    listTop = q_max(listTop, static_cast<int>(za::ceil(menuui::statusBottom(right))) + 2);
    return {top, listTop, bottom - 4 - 8 * helpBoxLines(), bottom};
}

[[nodiscard]] bool hasHelp(const za::Vector<Item>& list)
{
    for(const Item& item : list)
    {
        if(item.helpText || item.extendable || item.kind == Item::Slider) // (a slider's help: its fine steps at least)
        {
            return true;
        }
    }
    return false;
}

// A VR page's rows' leftmost text (menu x), for the corner's buttons (menu::contentLeft): each label right-aligned
// to the values' column (drawItem: midPos - 28 - its width), headers and lines of information centred (these 40
// characters at most). Kept per page and build (MenuInstance::rowsLeft).
[[nodiscard]] int pageRowsLeft()
{
    if(ui().rowsLeft.page != ui().page || ui().rowsLeft.build != builds[ui().page])
    {
        ui().rowsLeft.page = ui().page;
        ui().rowsLeft.build = builds[ui().page];
        int left = 0;
        for(const Item& item : menuPages.built[ui().page])
        {
            const int len = item.label ? static_cast<int>(strlen(item.label)) : 0;
            const int x = item.kind == Item::Header ? (320 - 8 * len) / 2
                          : item.kind == Item::Info ? 0
                                                    : midPos - 28 - 8 * len;
            // (a setting's room for its "changed" mark left of its label: drawItem)
            const bool marked = item.cvar && item.kind != Item::Action && item.kind != Item::Header && item.kind != Item::Info;
            left = q_min(left, marked ? x - 10 : x);
        }
        ui().rowsLeft.left = left;
    }
    return ui().rowsLeft.left;
}

// The rows' heights: 8 pixels each, a section's header (and the first, under the title) with a gap above it
// (vr_menu_section_gap, in rows: 0 to 2). The list scrolls a row at a time, so how many rows show depends on where it is
// scrolled to (the headers among them); each function below takes the gaps into account.
[[nodiscard]] int sectionGap()
{
    return static_cast<int>(za::round(CLAMP(0.f, vr_menu_section_gap.value, 2.f) * 8.f));
}

[[nodiscard]] int rowGap(const za::Vector<Item>& list, int i)
{
    return list[i].kind == Item::Header ? sectionGap() : 0;
}

// The list's height (pixels from listTop): down to the help's box, or the menu's bottom.
[[nodiscard]] int listSpace(const za::Vector<Item>& list)
{
    const Layout l = layout();
    return (hasHelp(list) ? l.helpTop - 4 : l.bottom - 8) - l.listTop;
}

// How many rows show from row `first` on (as many as fit, at least one; 0 past the list's end).
[[nodiscard]] int rowsFrom(const za::Vector<Item>& list, int first)
{
    const int space = listSpace(list);
    const int n = static_cast<int>(list.size());
    int y = 0;
    int rows = 0;
    for(int i = q_max(first, 0); i < n; i++)
    {
        y += rowGap(list, i) + 8;
        if(y > space)
        {
            break;
        }
        rows++;
    }
    return first < n ? q_max(rows, 1) : 0;
}

// Row i's top (pixels below listTop) with the list scrolled to `first` (i >= first): its gap included.
[[nodiscard]] int rowTop(const za::Vector<Item>& list, int first, int i)
{
    int y = 0;
    for(int k = first; k < i; k++)
    {
        y += rowGap(list, k) + 8;
    }
    return y + rowGap(list, i);
}

// The scroll that shows row `last` at the list's bottom: the first row of as many as fit up to it.
[[nodiscard]] int scrollShowing(const za::Vector<Item>& list, int last)
{
    const int space = listSpace(list);
    int y = rowGap(list, last) + 8;
    int first = last;
    while(first > 0 && y + rowGap(list, first - 1) + 8 <= space)
    {
        first--;
        y += rowGap(list, first) + 8;
    }
    return first;
}

// The furthest the list scrolls (0: it fits).
[[nodiscard]] int maxScroll(const za::Vector<Item>& list)
{
    return list.empty() ? 0 : scrollShowing(list, static_cast<int>(list.size()) - 1);
}

// The rows shown on the page as it is scrolled now.
[[nodiscard]] int visibleRows(const za::Vector<Item>& list)
{
    return rowsFrom(list, ui().scrolls[ui().page]);
}

// The rows the list's height holds (the scrollbar's track).
[[nodiscard]] int trackRows(const za::Vector<Item>& list)
{
    return q_max(listSpace(list) / 8, 1);
}

[[nodiscard]] int firstSelectable(const za::Vector<Item>& list)
{
    for(int i = 0; i < static_cast<int>(list.size()); i++)
    {
        if(selectable(list[i]))
        {
            return i;
        }
    }
    return 0;
}

void moveCursor(const za::Vector<Item>& list, int dir)
{
    const int n = static_cast<int>(list.size());
    int& cursor = ui().cursors[ui().page];
    int i = cursor;
    do
    {
        i = (i + dir + n) % n;
    } while(!selectable(list[i]) && i != cursor);
    cursor = i;
}

[[nodiscard]] int lastSelectable(const za::Vector<Item>& list)
{
    for(int i = static_cast<int>(list.size()) - 1; i >= 0; i--)
    {
        if(selectable(list[i]))
        {
            return i;
        }
    }
    return 0;
}

// The page's position to keep (vr_menu_positions): none while it is at its top (as it opens anyway).
void noteLeft(int p, const za::Vector<Item>& list)
{
    ui().leftAnchors[p] = ui().cursors[p] == firstSelectable(list) && ui().scrolls[p] == 0 ? RowAnchor{} : anchorOf(list, ui().cursors[p], ui().scrolls[p]);
}

// Shows `target`, with its cursor on a setting: where it was left (pages built anew as they are
// shown, for what the hand holds now, keep the same row by its label).
void closeDropDown(); // (the drop-down lists, below: a page shown closes the one open)

// The page a Search result opened (vr_menu_search.inc): Back from it returns to the results (VR Settings' Back too);
// -1 once elsewhere.
// Pages for one panel at a time (MULTI_MENU.md, stage 3): what they edit or stage is one, not a setting each row reads
// as it draws (the weapon and hotspot edited and Align Sights' pending result, Body Calibration's, the checklist's notes,
// the retro textures applied to the checked categories, a take reviewed, the keyboard pages' text input). Another panel
// is refused them while one shows them. 0: any page; else its group (the Weapon Offsets pages are one).
[[nodiscard]] int oneViewerGroup(int page)
{
    if(page < 0 || page >= pageCount)
    {
        return 0;
    }
    const PageBuilder b = pages[page].build;
    return weaponOffsetsPage(b)                                 ? 1
           : b == pageBodyCalibration                           ? 2
           : b == pageChecklist                                 ? 3
           : b == pageRetroAll                                  ? 4
           : b == pageSearch                                    ? 5
           : b == pageConsole                                   ? 6
           : b == pageMaps                                      ? 7
           : b == pageReviewTakes || b == pageReviewTake        ? 8
                                                                : 0;
}

// The page a panel shows (its VR page; -1: another menu, or closed).
[[nodiscard]] int instancePage(int i)
{
    const MenuInstance& m = menuInstances[i];
    const int state = i == currentMenu ? static_cast<int>(m_state) : m.quakeState;
    const bool open = i == 0 ? key_dest == key_menu && state != m_none : m.open;
    return open && (state == m_vr || state == m_confirm) ? m.page : -1;
}

// The other panel showing a page of `page`'s group for one panel at a time (-1: none, or any page).
[[nodiscard]] int pageTakenBy(int page)
{
    const int group = oneViewerGroup(page);
    for(int i = 0; group && i < menu::instanceCount; i++)
    {
        if(i != currentMenu && oneViewerGroup(instancePage(i)) == group)
        {
            return i;
        }
    }
    return -1;
}

void showPage(int target)
{
    closeDropDown();
    if(const int by = pageTakenBy(target); by >= 0)
    {
        // (Its title says so a moment: VR_Menu_Draw.)
        ui().refusedPage = target;
        ui().refusedTime = realtime;
        ui().refusedBy = by;
        Con_Printf("%s: open in panel %d (one panel at a time)\n", pages[target].title, by + 1);
        S_LocalSound("misc/menu1.wav");
        return;
    }
    ui().page = target;
    weightPageShown(pages[ui().page].build); // a weight page: what is in the hand now
    if(weaponOffsetsPage(pages[ui().page].build))
    {
        weaponOffsetsStale = true; // the weapon in hand now
    }
    if(pages[ui().page].build == pageReviewTakes)
    {
        motion::review::invalidate(); // takes recorded, evaluated or moved since
    }
    // Built again each time it is shown (its row kept): rows that depend on anything not checked in items() (what is
    // changed now: Changed Settings; a state a builder reads) are as they are now, not as when the page was last built
    // (by Search, a menu path or Changed Settings, which build every page).
    menuPages.done[ui().page] = false;
    const auto& list = items(ui().page);
    if(!selectable(list[ui().cursors[ui().page]]))
    {
        ui().cursors[ui().page] = firstSelectable(list);
    }
    noteLeft(ui().page, list);
    ui().visits[ui().page] = ++ui().visitCount;
}

bool slotPage(PageBuilder build)
{
    return weaponOffsetsPage(build) || build == pageWeaponWeights || build == pageHeldObjectOffsets ||
           build == pageHeldObjectWeights;
}

// Reset All to Defaults: armed by a first press, done by a second within 3 seconds. Every saved Quake VR setting (vr_*)
// back to its default (the shipped one, vr_defaults.cfg's), and the others on this page (volume, music, default speed,
// anti-aliasing).
void resetAll()
{
    if(!ui().resetAllArmed || realtime - ui().resetAllArmedTime > 3.0)
    {
        ui().resetAllArmed = true;
        ui().resetAllArmedTime = realtime;
        menuPages.done[PageMain] = false; // its label: Press Again to Reset All
        return;
    }
    ui().resetAllArmed = false;
    menuPages.done[PageMain] = false;
    za::Vector<cvar_t*> reset;
    for(int p = 0; p < pageCount; p++)
    {
        if(!vrSettingsPage(p))
        {
            continue;
        }
        for(const Item& item : items(p))
        {
            if(item.cvar && item.kind != Item::Action && !wrapperCvar(*item.cvar))
            {
                reset.pushBack(item.cvar);
            }
        }
    }
    for(cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        if(!q_strncasecmp(var->name, "vr_", 3))
        {
            reset.pushBack(var);
        }
    }
    int n = 0;
    for(cvar_t* var : reset)
    {
        if(var->default_string && !keptOnResetAll(*var) && !serverrules::locked(var) &&
            strcmp(var->string, var->default_string) != 0)
        {
            Con_DPrintf("Reset All to Defaults: %s \"%s\" (was \"%s\")\n", var->name, var->default_string, var->string);
            Cvar_SetQuick(var, var->default_string);
            n++;
        }
    }
    Con_Printf("Reset All to Defaults: %d settings back to their defaults\n", n);
    S_LocalSound("misc/menu2.wav");
}

void resetThisPage()
{
    if(ui().resetArmedPage != ui().page || realtime - ui().resetArmedTime > 3.0)
    {
        ui().resetArmedPage = ui().page;
        ui().resetArmedTime = realtime;
        menuPages.done[ui().page] = false; // its label: Press Again to Reset
        return;
    }
    ui().resetArmedPage = -1;
    int n = 0;
    for(const Item& item : items(ui().page))
    {
        if(item.cvar && item.kind != Item::Action && changedSetting(*item.cvar) && !serverrules::locked(item.cvar))
        {
            Cvar_SetQuick(item.cvar, item.cvar->default_string);
            n++;
        }
    }
    Con_Printf("VR: %s: %d setting%s back to %s default\n", pages[ui().page].title, n, n == 1 ? "" : "s", n == 1 ? "its" : "their");
    menuPages.done[ui().page] = false;
}

const char* changedNone()
{
    return "Nothing changed from the defaults.";
}

// Changed Settings: every setting changed from its default, under its home page's title, as its home shows it (its
// range, its choices, its help): from every page, whatever Menu Detail shows.
za::Vector<Item> pageChanged()
{
    za::Vector<Item> list;
    za::Vector<const cvar_t*> seen;
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].build == pageChanged || slotPage(pages[p].build))
        {
            continue;
        }
        bool headed = false;
        for(const Item& item : items(p))
        {
            if(!item.cvar || item.kind == Item::Action || !changedSetting(*item.cvar) || wrapperCvar(*item.cvar) ||
                za::anyOf(seen.begin(), seen.end(), [&](const cvar_t* v) { return v == item.cvar; }))
            {
                continue;
            }
            if(!headed)
            {
                list.pushBack(header(pages[p].title));
                headed = true;
            }
            Item copy = item;
            copy.level = LevelStandard;
            list.pushBack(ZA_MOVE(copy));
            seen.pushBack(item.cvar);
        }
    }
    levelOverride = was;
    if(list.empty())
    {
        list.pushBack(info(changedNone));
    }
    return list;
}

void openPage(int target)
{
    navSyncTop();
    navPush(target);
    showPage(target);
    S_LocalSound("misc/menu2.wav");
}

// The page listing `p` in the menus' tree (VR Settings: itself).
[[nodiscard]] int homeOf(int p)
{
    return pages[p].home ? pageIndex(pages[p].home) : PageMain;
}

// Opens `target` as from its place in the tree (menu_vr <n>), from the VR Settings: each page above it shown in
// turn (its cursor off the headers), so that Back goes up the tree.
void openInTree(int target)
{
    if(pageTakenBy(target) >= 0)
    {
        showPage(target); // (refused: the page shown stays, its title saying so)
        return;
    }
    za::Vector<int> chain;
    for(int p = target; p != PageMain && static_cast<int>(chain.size()) < pageCount; p = homeOf(p))
    {
        chain.pushBack(p);
    }
    navSyncTop();
    for(const int p : za::reversed(chain))
    {
        navPush(p);
        showPage(p);
    }
    if(!chain.empty())
    {
        S_LocalSound("misc/menu2.wav");
    }
}

// The VR pages shown (as from a key: the menu's sound as it is drawn).
void enterVrMenu()
{
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_vr;
    m_entersound = true;
}

// `target` from whatever is shown: a VR page (its place on the stack), another menu (Back returns to it) or none.
void openFromAnywhere(int target)
{
    if(m_state == m_vr && key_dest == key_menu)
    {
        navSyncTop();
        navPush(target);
        showPage(target);
        S_LocalSound("misc/menu2.wav");
        return;
    }
    navEnterFrom(outsideMenu());
    enterVrMenu();
    navPush(target);
    showPage(target);
}

// Leaving the VR pages by Back for `state` (an outside menu: vr_menuui.cpp opens it as its own Back would).
void leaveTo(int state)
{
    ui().navReturning = true;
    menuui::openMenu(state);
    ui().navReturning = false;
}

// Back from the page shown (a key, a button): where the player came from (NavStack), else up the tree.
void goBack()
{
    navSyncTop();
    if(ui().nav.count >= 2)
    {
        ui().nav.count--;
        const int dest = navTop();
        if(dest >= 0)
        {
            showPage(dest);
            S_LocalSound("misc/menu2.wav");
        }
        else if(currentMenu != 0)
        {
            menu::closeInstance(currentMenu); // (a panel other than the first shows VR pages only: Back out of them closes it)
        }
        else
        {
            leaveTo(-1 - dest);
        }
        return;
    }
    navReset();
    if(ui().page == PageMain)
    {
        if(currentMenu != 0)
        {
            navPush(PageMain);
            menu::closeInstance(currentMenu);
            return;
        }
        M_Menu_Options_f(); // (its sound as it is drawn)
        return;
    }
    if(pages[ui().page].build == pageBodyCalibration && qvr::setup::waitsOnMenu())
    {
        // VR Calibration waits on this page (paused, or the body's result not trusted): Back closes the menu, as the
        // menu button does, and the calibration goes on (not up the tree, the menu still open and it still waiting)
        navPush(ui().page);
        menuui::backToGame(HAND_MAIN);
        S_LocalSound("misc/menu2.wav");
        return;
    }
    const int dest = homeOf(ui().page);
    navPush(dest);
    showPage(dest);
    S_LocalSound("misc/menu2.wav");
}

// Where Back goes from the page shown (menu_vr pos): a page's number, or -1 with `outside` the menu (m_none: Options).
[[nodiscard]] int backTarget(int& outside)
{
    outside = m_none;
    if(ui().nav.count >= 2 && navTop() == ui().page)
    {
        const int dest = ui().nav.places[ui().nav.count - 2];
        if(dest < 0)
        {
            outside = -1 - dest;
            return -1;
        }
        return dest;
    }
    return ui().page == PageMain ? -1 : homeOf(ui().page);
}

// A setting's value as shown: a server rule's is the remote server's (vr_serverrules.cpp), the rest their own.
[[nodiscard]] float valueOf(const Item& item)
{
    return serverrules::shown(item.cvar);
}

// A server rule while connected to a remote server: shown, dimmed, not changed from here.
[[nodiscard]] bool lockedItem(const Item& item)
{
    return item.unavailable || ((item.kind == Item::Slider || item.kind == Item::Cycle) && serverrules::locked(item.cvar)); // AD support: unavailable spawn actions are locked too
}

[[nodiscard]] int currentChoice(const Item& item)
{
    const float cur = valueOf(item);
    int best = 0;
    for(int i = 0; i < static_cast<int>(item.choices.size()); i++)
    {
        if(za::fabs(item.choices[i].value - cur) < za::fabs(item.choices[best].value - cur))
        {
            best = i;
        }
    }
    return best;
}

// Where a slider's value lies against its bar: -1 before the left end, 1 past the right one, else 0
// (a hair's tolerance: a value on an end is on the bar).
[[nodiscard]] int pastEnd(const Item& item, float value)
{
    const float eps = item.step * 0.01f;
    return value > item.max + eps ? 1 : value < item.min - eps ? -1 : 0;
}

// A slider's step left or right (`dir`), the key held (`repeat`: its auto-repeat). Never beyond
// the bar's ends, or for an extendable slider its hard limits; and never back the other way, so a
// value set further off in the console or a config stays as it is until stepped towards the bar
// (then it comes onto the limit at once). An extendable slider stops at the bar's end first: a new
// press goes past, or holding on there for a moment; past the ends, holding steps faster the longer
// it is held (2x after a second, then 5x, then 10x), on multiples of those steps (MenuInstance::sliderHold).

// The sliders' fine adjustment: while either grip is held in the headset (on the panel's frame, or moved, a grip moves
// the panel instead: vr_menu_grab), or
// Shift on a flat screen, a slider steps by vr_menu_fine_step of its step (0.1: a tenth), and shows the decimals that
// takes (sliderText).
bool fineForced = false; // (vr_menu_slider_step's "fine": the modifier as if held)

[[nodiscard]] bool fineHeld()
{
    return fineForced || keydown[K_SHIFT] || keydown[K_LSHOULDER] || keydown[K_RSHOULDER];
}

[[nodiscard]] float sliderStep(const Item& item)
{
    return fineHeld() ? item.step * CLAMP(0.01f, vr_menu_fine_step.value, 1.f) : item.step;
}

// The decimals that show `x` (to 4 at most).
[[nodiscard]] int decimalsOf(float x)
{
    int n = 0;
    float scaled = za::fabs(x);
    while(n < 4 && za::fabs(scaled - za::round(scaled)) > 0.001f * q_max(1.f, scaled))
    {
        scaled *= 10.f;
        n++;
    }
    return n;
}

// A slider's value as its format shows it, with more decimals where the value has them (a fine step) or while the fine
// steps are on (their size's).
void sliderText(const Item& item, float value, char* buf, size_t size)
{
    const char* dot = strstr(item.format, "%");
    while(dot && *dot && *dot != '.' && *dot != 'f')
    {
        dot++;
    }
    if(!dot || *dot != '.' || dot[1] < '0' || dot[1] > '9' || dot[2] != 'f')
    {
        q_snprintf(buf, size, item.format, value);
        return;
    }
    const int own = dot[1] - '0';
    int n = q_max(own, decimalsOf(value));
    if(fineHeld())
    {
        n = q_max(n, decimalsOf(sliderStep(item)));
    }
    char format[32];
    const size_t at = static_cast<size_t>(dot - item.format) + 1;
    if(n == own || at + 2 >= sizeof(format) || strlen(item.format) + 1 >= sizeof(format))
    {
        q_snprintf(buf, size, item.format, value);
        return;
    }
    q_strlcpy(format, item.format, sizeof(format));
    format[at] = static_cast<char>('0' + n);
    q_snprintf(buf, size, format, value);
}

float stepSlider(const Item& item, int dir, bool repeat)
{
    constexpr double endHold = 0.6;
    const cvar_t*& endCvar = ui().sliderHold.endCvar;
    int& endDir = ui().sliderHold.endDir;
    double& endSince = ui().sliderHold.endSince;
    double& outsideSince = ui().sliderHold.outsideSince;

    // (A negative value under a negativeLabel, stored -1, is the bar's leftmost step: stepping right goes to
    // negativeStart, stepping left from there back to it.)
    const float eps = item.step * 0.01f;
    if(item.negativeLabel && (item.cvar->value < 0.f || (dir < 0 && item.cvar->value <= item.negativeStart + eps)))
    {
        return item.cvar->value < 0.f && dir > 0 ? item.negativeStart : -1.f;
    }
    const float cur = item.cvar->value;
    const float end = dir > 0 ? item.max : item.min;
    const int past = pastEnd(item, cur);
    const bool atEnd = za::fabs(cur - end) <= eps;

    float step = sliderStep(item);
    if(!repeat || !past)
    {
        outsideSince = realtime;
    }
    else if(item.extendable)
    {
        const double held = realtime - outsideSince;
        step *= held < 1.0 ? 1.f : held < 2.0 ? 2.f : held < 3.0 ? 5.f : 10.f;
    }
    // On the steps' grid: a plain step lands on a whole step (a round value, as before the fine steps: on the fine grid
    // it landed off it with a fine step that isn't a whole fraction, 0.6998, 0.7997 with vr_menu_fine_step 0.0999: the
    // author's note vrfiringrange_2026-10-07_22-23-40); a fine one on the fine steps' grid.
    const double grid = fineHeld() ? static_cast<double>(item.step) * CLAMP(0.01f, vr_menu_fine_step.value, 1.f)
                                   : static_cast<double>(item.step);
    float v = static_cast<float>(std::round((static_cast<double>(cur) + dir * static_cast<double>(step)) / grid) * grid);

    float lo = item.min;
    float hi = item.max;
    if(item.extendable)
    {
        lo = item.hardMin;
        hi = item.hardMax;
        const bool pass = atEnd && (!repeat || (endCvar == item.cvar && endDir == dir && realtime - endSince >= endHold));
        if(!past && !pass)
        {
            lo = za::fmax(lo, item.min); // on the bar: its end stops the step
            hi = za::fmin(hi, item.max);
        }
        else if(past * dir < 0)
        {
            lo = past > 0 ? za::fmax(lo, item.max) : lo; // coming back: onto the end
            hi = past < 0 ? za::fmin(hi, item.min) : hi;
        }
    }
    v = CLAMP(lo, v, hi);
    v = dir > 0 ? za::fmax(v, cur) : za::fmin(v, cur);

    if(item.extendable && za::fabs(v - end) <= eps && (v != cur || endCvar != item.cvar || endDir != dir))
    {
        endCvar = item.cvar; // (a new arrival, or a first press held there)
        endDir = dir;
        endSince = realtime;
    }
    return v;
}

// (`item` lives in the page's list, which an action -- or a cvar's callback -- may build again: what is needed of it is
// read before.)
void change(const Item& item, int dir, bool repeat = false)
{
    if(lockedItem(item))
    {
        S_LocalSound("misc/menu3.wav"); // the server's rule: not changed from here
        return;
    }
    switch(item.kind)
    {
        case Item::Slider:
        {
            const float v = stepSlider(item, dir, repeat);
            if(v != item.cvar->value)
            {
                Cvar_SetValueQuick(item.cvar, item.negativeLabel && v < 0.f ? -1.f : v);
            }
            break;
        }
        case Item::Cycle:
        {
            const int n = static_cast<int>(item.choices.size());
            Cvar_SetValueQuick(item.cvar, item.choices[(currentChoice(item) + dir + n) % n].value);
            break;
        }
        case Item::Action:
            if(dir > 0 && item.buttonPress)
            {
                const int b = CLAMP(0, ui().button, item.buttons - 1);
                if(item.buttonDim && item.buttonDim(item.arg, b))
                {
                    S_LocalSound("misc/menu1.wav"); // (a dimmed button: Test without a scenario)
                    return;
                }
                S_LocalSound("misc/menu3.wav");
                item.buttonPress(item.arg, b); // (`item` may be gone after this)
                return;
            }
            if(dir > 0)
            {
                const int page = item.page;
                void (*const action)() = item.action;
                void (*const actionArg)(int) = item.actionArg;
                const char* const command = item.command;
                const int arg = item.arg;
                if(command)
                {
                    runCommand(command);
                }
                if(actionArg)
                {
                    actionArg(arg); // (`item` may be gone after this)
                }
                if(page >= 0)
                {
                    openPage(page);
                    return;
                }
                if(action)
                {
                    action(); // (`item` may be gone after this)
                }
            }
            break;
        default: break;
    }
    S_LocalSound("misc/menu3.wav");
}

// An Off/On choice.
[[nodiscard]] bool isToggle(const Item& item)
{
    return item.choices.size() == 2 && item.choices[0].value == 0.f && item.choices[1].value == 1.f &&
           !strcmp(item.choices[0].label, "Off") && !strcmp(item.choices[1].label, "On");
}

// The list row under the mouse as drawn (a long text's next line itself, a button row), or -1.
[[nodiscard]] int rowAtRaw(float cy)
{
    const auto& list = items(ui().page);
    const float yrel = cy - static_cast<float>(layout().listTop);
    const int first = ui().scrolls[ui().page];
    const int last = first + visibleRows(list);
    float y = 0.f;
    for(int k = first; k < last && yrel >= y; k++)
    {
        y += static_cast<float>(rowGap(list, k));
        if(yrel >= y && yrel < y + 8.f)
        {
            return k;
        }
        y += 8.f;
    }
    return -1;
}

// A button row's button under the mouse at `cx` (Item::buttons: side by side across the row's 320 columns).
[[nodiscard]] int buttonAt(const Item& row, float cx)
{
    return CLAMP(0, static_cast<int>(cx * static_cast<float>(row.buttons) / 320.f), row.buttons - 1);
}

// The list row under the mouse, or -1.
[[nodiscard]] int rowAt(float cy)
{
    const auto& list = items(ui().page);
    const float yrel = cy - static_cast<float>(layout().listTop);
    const int first = ui().scrolls[ui().page];
    const int last = first + visibleRows(list);
    int i = -1;
    float y = 0.f;
    for(int k = first; k < last && yrel >= y; k++)
    {
        y += static_cast<float>(rowGap(list, k));
        if(yrel >= y && yrel < y + 8.f)
        {
            i = k;
            break;
        }
        y += 8.f;
    }
    if(i < 0)
    {
        return -1; // above the list, below it, or in a section's gap
    }
    // A long text's next line: its first, while shown.
    return i - list[i].partOf >= first ? i - list[i].partOf : -1;
}

// A long list's scrollbar, right of the values (as far as the screen goes), as Ironwail's lists
// have: its thumb's top (pixels below listTop) and height (rows). False when the list fits. (Where it was drawn:
// MenuInstance::scrollbarX.)

[[nodiscard]] bool scrollbar(const za::Vector<Item>& list, int& y, int& height)
{
    const int most = maxScroll(list);
    if(most <= 0)
    {
        return false;
    }
    const int track = trackRows(list);
    const int n = static_cast<int>(list.size());
    height = CLAMP(2, static_cast<int>(track * visibleRows(list) / static_cast<float>(n) + 0.5f), track);
    y = static_cast<int>(CLAMP(0, ui().scrolls[ui().page], most) * 8 / static_cast<float>(most) * (track - height) + 0.5f);
    return true;
}

// The cursor moved onto the nearest visible setting if the list scrolled it out of view.
void keepCursorVisible()
{
    const auto& list = items(ui().page);
    const int rows = visibleRows(list);
    const int scroll = ui().scrolls[ui().page];
    int& cursor = ui().cursors[ui().page];
    const int dir = cursor < scroll ? 1 : cursor >= scroll + rows ? -1 : 0;
    if(dir != 0)
    {
        cursor = dir > 0 ? scroll : scroll + rows - 1;
        while(!selectable(list[cursor]) && cursor + dir >= scroll && cursor + dir < scroll + rows)
        {
            cursor += dir;
        }
    }
}

// menu_vr rows (tests, Debug > Tools): the VR page shown, its rows as drawn: MROW|row|top y|label (menu coordinates), then
// MROWS with the rows shown, the scroll and its most, the section gap, and whether the mouse finds each row where it is
// drawn (rowAt, the laser's and the desktop mouse's) and nothing in the gaps.
void printRows()
{
    if(m_state != m_vr)
    {
        Con_Printf("menu_vr rows: not on a VR page\n");
        return;
    }
    const auto& list = items(ui().page);
    const Layout l = layout();
    const int first = ui().scrolls[ui().page];
    const int shown = visibleRows(list);
    int bad = 0;
    for(int i = first; i < first + shown; i++)
    {
        const int top = l.listTop + rowTop(list, first, i);
        Con_Printf("MROW|%d|%d|%s\n", i, top, list[i].label ? list[i].label : "");
        const int hit = rowAt(static_cast<float>(top) + 4.f);
        const int expected = i - list[i].partOf >= first ? i - list[i].partOf : -1;
        bad += hit != expected;
        if(rowGap(list, i) > 1 && rowAt(static_cast<float>(top - rowGap(list, i)) + 0.5f) != -1)
        {
            bad++;
        }
    }
    Con_Printf("MROWS|%d shown from %d of %d|scroll most %d|gap %d px|bottom %d of %d|rowAt %s\n", shown, first,
        static_cast<int>(list.size()), maxScroll(list), sectionGap(), shown > 0 ? l.listTop + rowTop(list, first, first + shown - 1) + 8 : l.listTop,
        l.listTop + listSpace(list), bad ? "MISMATCH" : "agrees");
}

// The list scrolled to where the mouse holds the scrollbar, the cursor kept on a visible setting.
void scrollTo(float cy)
{
    const auto& list = items(ui().page);
    int y, height;
    if(!scrollbar(list, y, height))
    {
        return;
    }
    const int most = maxScroll(list);
    const float yrel = cy - layout().listTop - height * 4.f;
    const int range = q_max((trackRows(list) - height) * 8, 1);
    ui().scrolls[ui().page] = CLAMP(0, static_cast<int>(yrel * most / range + 0.5f), most);
    keepCursorVisible();
}

// A slider set where the mouse is along it (as Ironwail's: the thumb's middle from midPos + 4 to
// midPos + 76), on its steps.
void setSliderAt(const Item& item, float cx)
{
    if(lockedItem(item))
    {
        return; // the server's rule
    }
    const float frac = CLAMP(0.f, (cx - midPos - 4.f) / 72.f, 1.f);
    float v = item.min + frac * (item.max - item.min);
    v = za::round(v / sliderStep(item)) * sliderStep(item);
    v = CLAMP(item.min, v, item.max);
    if(item.negativeLabel && v < item.negativeStart - item.step * 0.01f)
    {
        v = -1.f;
    }
    if(v != item.cvar->value)
    {
        Cvar_SetValueQuick(item.cvar, v);
        if(ui_mouse_sound.value)
        {
            S_LocalSound("misc/menu1.wav");
        }
    }
}

// ----------------------------------------------------------------------------
// Drop-down lists: a choice row with many choices (vr_menu_dropdown or more) opens a list of them
// next to it on a click (or Enter / A) instead of stepping to the next one; the mouse or the laser
// picks one (up and down, the stick, Enter / A likewise), a click outside or Back closes it. Left
// and right still step through the choices without it.
// ----------------------------------------------------------------------------

// (Its state: MenuInstance::dropDown.)

constexpr int dropDownBar = 16; // the scrollbar's column, inside the box's right edge

[[nodiscard]] bool opensDropDown(const Item& item)
{
    return item.kind == Item::Cycle && vr_menu_dropdown.value >= 2.f && item.cvar &&
           static_cast<int>(item.choices.size()) >= static_cast<int>(vr_menu_dropdown.value) && !isToggle(item);
}

void closeDropDown()
{
    ui().dropDown.open = false;
    ui().dropDown.scrollGrab = false;
}

// The open list's row, or null (closed now if its page or row is gone, or the corner's buttons took the selection).
[[nodiscard]] const Item* dropDownItem()
{
    if(!ui().dropDown.open)
    {
        return nullptr;
    }
    if(m_state != m_vr || ui().page != ui().dropDown.page || menuui::toolbarFocused())
    {
        closeDropDown();
        return nullptr;
    }
    const auto& list = items(ui().page);
    if(ui().dropDown.row >= static_cast<int>(list.size()))
    {
        closeDropDown();
        return nullptr;
    }
    const Item& item = list[ui().dropDown.row];
    if(item.kind != Item::Cycle || item.cvar != ui().dropDown.cvar || static_cast<int>(item.choices.size()) != ui().dropDown.count)
    {
        closeDropDown();
        return nullptr;
    }
    return &item;
}

void dropDownShowHover()
{
    if(ui().dropDown.hover < ui().dropDown.scroll)
    {
        ui().dropDown.scroll = ui().dropDown.hover;
    }
    else if(ui().dropDown.hover >= ui().dropDown.scroll + ui().dropDown.rows)
    {
        ui().dropDown.scroll = ui().dropDown.hover - ui().dropDown.rows + 1;
    }
    ui().dropDown.scroll = CLAMP(0, ui().dropDown.scroll, q_max(ui().dropDown.count - ui().dropDown.rows, 0));
}

// The list of `item`'s choices (the page's row `row`), opened by its row: as wide as its longest
// label, as many rows as fit (12 at most; it scrolls past that), the current choice over the row
// where the screen allows, kept inside the menu.
void openDropDown(const Item& item, int row)
{
    const Layout l = layout();
    const int n = static_cast<int>(item.choices.size());
    int longest = 1;
    for(const Choice& c : item.choices)
    {
        longest = q_max(longest, static_cast<int>(strlen(c.label)));
    }
    longest = q_min(longest, 30);

    DropDown& d = ui().dropDown;
    d.open = true;
    d.scrollGrab = false;
    d.page = ui().page;
    d.row = row;
    d.cvar = item.cvar;
    d.count = n;
    d.labelChars = longest;
    d.rows = q_min(n, CLAMP(4, (l.bottom - l.top - 32) / 8, 12));
    const int cur = currentChoice(item);
    d.hover = cur;
    d.scroll = CLAMP(0, cur - d.rows / 2, q_max(n - d.rows, 0));

    // Across: a marker's column, then the labels where the row shows its value, the scrollbar's column if it scrolls.
    const int inner = 12 + longest * 8 + 4 + (n > d.rows ? dropDownBar : 0);
    d.chars = (inner + 7) / 8;
    d.width = ((d.chars + 1) / 2) * 16; // as M_DrawTextBox draws it
    const int right = static_cast<int>(glcanvas.right) - 12;
    const int left = static_cast<int>(glcanvas.left) + 12;
    d.x = midPos - 12;
    if(d.x + d.width > right)
    {
        d.x = q_max(right - d.width, left);
    }

    // Up and down: the current choice level with the row, the box inside the menu.
    const int rowY = l.listTop + rowTop(items(ui().page), ui().scrolls[ui().page], row);
    d.top = rowY - (cur - d.scroll) * 8;
    d.top = CLAMP(l.top + 12, d.top, q_max(l.top + 12, l.bottom - 12 - d.rows * 8));
    S_LocalSound("misc/menu3.wav");
}

// Sets the hovered choice and closes the list.
void pickDropDown(const Item& item)
{
    const float v = item.choices[CLAMP(0, ui().dropDown.hover, ui().dropDown.count - 1)].value;
    cvar_t* const cvar = item.cvar;
    closeDropDown();
    Cvar_SetValueQuick(cvar, v); // (`item` may be gone after this: a page rebuilt by the setting's change)
    S_LocalSound("misc/menu3.wav");
}

[[nodiscard]] bool insideDropDown(float cx, float cy)
{
    const DropDown& d = ui().dropDown;
    return cx >= d.x - 6 && cx < d.x + d.width + 6 && cy >= d.top - 6 && cy < d.top + d.rows * 8 + 6;
}

[[nodiscard]] bool onDropDownBar(float cx, float cy)
{
    const DropDown& d = ui().dropDown;
    return d.count > d.rows && insideDropDown(cx, cy) && cx >= d.x + d.width - dropDownBar;
}

// The choice under the mouse, or -1.
[[nodiscard]] int dropDownChoiceAt(float cx, float cy)
{
    const DropDown& d = ui().dropDown;
    if(!insideDropDown(cx, cy) || onDropDownBar(cx, cy) || cy < d.top || cy >= d.top + d.rows * 8)
    {
        return -1;
    }
    const int i = d.scroll + static_cast<int>(za::floor((cy - d.top) / 8.f));
    return i < d.count ? i : -1;
}

// The scrollbar's thumb: its top (pixels below the list's top) and height (rows).
void dropDownThumb(int& y, int& height)
{
    const DropDown& d = ui().dropDown;
    height = q_max(static_cast<int>(d.rows * d.rows / static_cast<float>(d.count) + 0.5f), 2);
    y = static_cast<int>(d.scroll * 8 / static_cast<float>(d.count - d.rows) * (d.rows - height) + 0.5f);
}

void dropDownScrollTo(float cy)
{
    DropDown& d = ui().dropDown;
    if(d.count <= d.rows)
    {
        return;
    }
    int y, height;
    dropDownThumb(y, height);
    const float yrel = cy - d.top - height * 4.f;
    d.scroll = CLAMP(0, static_cast<int>(yrel * (d.count - d.rows) / ((d.rows - height) * 8) + 0.5f), d.count - d.rows);
}

void drawDropDown()
{
    const Item* item = dropDownItem();
    if(!item)
    {
        return;
    }
    const DropDown& d = ui().dropDown;
    M_DrawTextBox(d.x - 8, d.top - 8, d.chars, d.rows);
    const int cur = currentChoice(*item);
    const int textRight = d.x + d.width - (d.count > d.rows ? dropDownBar : 0);
    char text[32];
    for(int i = d.scroll; i < d.count && i < d.scroll + d.rows; i++)
    {
        const int y = d.top + (i - d.scroll) * 8;
        if(i == d.hover)
        {
            menuui::drawListHighlight(d.x - 3.f, static_cast<float>(textRight) + 1.f, y);
        }
        q_strlcpy(text, item->choices[i].label, q_min(static_cast<int>(sizeof(text)), d.labelChars + 1));
        if(i == cur)
        {
            M_DrawCharacter(d.x, y, 13); // the choice set now
            M_PrintWhite(d.x + 12, y, text);
        }
        else
        {
            M_Print(d.x + 12, y, text);
        }
    }
    if(d.count > d.rows)
    {
        int y, height;
        dropDownThumb(y, height);
        M_DrawTextBox(d.x + d.width - dropDownBar - 2, d.top + y - 4, 0, height - 1);
    }
}

// A key while the list is open: all of them are its (true), but a corner button's.
bool dropDownKey(int key)
{
    const Item* item = dropDownItem();
    if(!item)
    {
        return false;
    }
    DropDown& d = ui().dropDown;
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4:
            closeDropDown();
            S_LocalSound("misc/menu2.wav");
            break;
        case K_UPARROW:
        case K_DOWNARROW:
            d.hover = CLAMP(0, d.hover + (key == K_DOWNARROW ? 1 : -1), d.count - 1);
            dropDownShowHover();
            S_LocalSound("misc/menu1.wav");
            break;
        case K_MWHEELUP:
        case K_MWHEELDOWN:
        {
            d.scroll = CLAMP(0, d.scroll + (key == K_MWHEELDOWN ? 1 : -1), q_max(d.count - d.rows, 0));
            const int i = dropDownChoiceAt(m_mousex, m_mousey);
            if(i >= 0)
            {
                d.hover = i;
            }
            break;
        }
        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON: pickDropDown(*item); break;
        case K_MOUSE1:
        {
            if(onDropDownBar(m_mousex, m_mousey))
            {
                d.scrollGrab = true;
                dropDownScrollTo(m_mousey);
                break;
            }
            const int i = dropDownChoiceAt(m_mousex, m_mousey);
            if(i >= 0)
            {
                d.hover = i;
                pickDropDown(*item);
            }
            else if(!insideDropDown(m_mousex, m_mousey))
            {
                closeDropDown(); // a click outside: closed, nothing else done
                S_LocalSound("misc/menu2.wav");
            }
            break;
        }
        default: break; // (left and right too: the list's choice is picked, not stepped)
    }
    return true;
}

// The mouse while the list is open: the choice under it highlighted, or its scrollbar dragged.
bool dropDownMousemove(float cx, float cy)
{
    if(!dropDownItem())
    {
        return false;
    }
    DropDown& d = ui().dropDown;
    if(d.scrollGrab)
    {
        if(keydown[K_MOUSE1])
        {
            dropDownScrollTo(cy);
            return true;
        }
        d.scrollGrab = false;
    }
    const int i = dropDownChoiceAt(cx, cy);
    if(i >= 0 && i != d.hover)
    {
        d.hover = i;
        if(ui_mouse_sound.value)
        {
            S_LocalSound("misc/menu1.wav");
        }
    }
    return true;
}

void drawItem(const Item& item, int y, bool selected)
{
    if(item.kind == Item::Header)
    {
        M_PrintWhite((320 - 8 * static_cast<int>(strlen(item.label))) / 2, y, item.label);
        return;
    }
    if(item.kind == Item::Info && item.progress)
    {
        const float f = item.progress();
        if(f < 0.f)
        {
            return;
        }
        // The bar from 16 to 200, its text from 208 (14 characters: "100% 1:02:03").
        constexpr int x0 = 16, x1 = 200;
        if(!menuui::drawProgress(x0, x1, y, f))
        {
            Draw_Fill(x0, y + 1, x1 - x0, 6, 4, 1.f);                                          // (the palette's dark grey)
            Draw_Fill(x0, y + 1, static_cast<int>((x1 - x0) * CLAMP(0.f, f, 1.f)), 6, 192, 1.f); // (its yellow)
        }
        char text[15];
        q_strlcpy(text, item.info ? item.info() : "", sizeof(text));
        M_Print(x1 + 8, y, text);
        return;
    }
    if(item.kind == Item::Info)
    {
        char text[41];
        q_strlcpy(text, item.infoArg ? item.infoArg(item.arg) : item.info ? item.info() : "", sizeof(text));
        M_Print((320 - 8 * static_cast<int>(strlen(text))) / 2, y, text);
        return;
    }

    // A tint: 1 green (passed), 2 red (failed); `alpha` the row's dimming.
    const auto pushTint = [](int tint, float alpha) {
        if(tint == 1)
        {
            GL_PushCanvasColor(0.45f, 1.f, 0.45f, alpha);
        }
        else if(tint == 2)
        {
            GL_PushCanvasColor(1.f, 0.4f, 0.35f, alpha);
        }
        else
        {
            GL_PushCanvasColor(1.f, 1.f, 1.f, alpha);
        }
    };
    if(item.wide && item.buttons > 0 && item.buttonLabel)
    {
        // A button row: its buttons side by side, the one picked bracketed while the item is selected.
        if(selected)
        {
            VR_MenuDrawHighlight(4, y);
        }
        const int cell = 320 / item.buttons;
        for(int b = 0; b < item.buttons; b++)
        {
            char text[41];
            const bool picked = selected && b == CLAMP(0, ui().button, item.buttons - 1);
            q_snprintf(text, sizeof(text), picked ? "[ %s ]" : "%s", item.buttonLabel(item.arg, b));
            const int x = b * cell + (cell - 8 * static_cast<int>(strlen(text))) / 2;
            const bool dim = item.buttonDim && item.buttonDim(item.arg, b);
            const int tint = item.buttonTint ? item.buttonTint(item.arg, b) : 0;
            pushTint(tint, dim ? 0.35f : 1.f);
            if(picked || tint != 0)
            {
                M_PrintWhite(x, y, text);
            }
            else
            {
                M_Print(x, y, text);
            }
            GL_PopCanvasColor();
        }
        return;
    }
    if(item.wide)
    {
        // A row of a list: its text across the width, white while selected.
        char text[41];
        q_strlcpy(text, item.infoArg ? item.infoArg(item.arg) : item.label, sizeof(text));
        const bool dim = item.dimArg && item.dimArg(item.arg);
        const int tint = item.markArg ? item.markArg(item.arg) : 0;
        if(selected && item.dimArg)
        {
            VR_MenuDrawHighlight(4, y); // (the Checklist's: each line of the item picked; the VR style's only)
        }
        const bool tinted = dim || tint != 0;
        if(tinted)
        {
            pushTint(tint, dim ? 0.4f : 1.f);
        }
        if(selected || tint != 0)
        {
            M_PrintWhite(0, y, text);
        }
        else
        {
            M_Print(0, y, text);
        }
        if(tinted)
        {
            GL_PopCanvasColor();
        }
        return;
    }

    const int labelX = midPos - 28 - 8 * static_cast<int>(strlen(item.label));
    const bool locked = lockedItem(item); // the remote server's rule (or unavailable()): dimmed, its value shown
    if(locked)
    {
        GL_PushCanvasColor(1.f, 1.f, 1.f, 0.5f);
    }
    M_Print(labelX, y, item.label);
    if(item.cvar && item.kind != Item::Action && changedSetting(*item.cvar))
    {
        // Changed from its default (Changed Settings lists them): left of the label however long (a long label's
        // reaches left of x 0; pageRowsLeft keeps the corner's buttons clear of the mark too).
        M_PrintWhite(labelX - 10, y, "*");
    }

    char buf[64];
    switch(item.kind)
    {
        case Item::Slider:
        {
            const float value = valueOf(item);
            if(item.negativeLabel && value < 0.f)
            {
                q_strlcpy(buf, item.negativeLabel, sizeof(buf));
            }
            else
            {
                sliderText(item, value, buf, sizeof(buf));
            }
            // Past an end: the thumb stays there, marked, and the value (the real one) is white.
            const float range = (value - item.min) / (item.max - item.min);
            const int past = item.negativeLabel && value < 0.f ? 0 : pastEnd(item, value);
            char tinted[64];
            const char* text = past ? COM_TintString(buf, tinted, sizeof(tinted)) : buf;
            if(!menuui::drawSlider(midPos, y, range, past, text))
            {
                M_DrawSlider(midPos, y, CLAMP(0.f, range, 1.f), text);
            }
            break;
        }
        case Item::Cycle:
            if(isToggle(item))
            {
                M_DrawCheckbox(midPos, y, item.unavailable ? 0.f : valueOf(item)); // a switch in the VR menu style
            }
            else
            {
                M_Print(midPos, y, item.choices[currentChoice(item)].label);
            }
            break;
        case Item::Action: M_Print(midPos - 4, y, "..."); break;
        default: break;
    }
    if(locked)
    {
        GL_PopCanvasColor();
    }

    if(selected)
    {
        M_DrawArrowCursor(midPos - 20, y);
    }
}

// Render Scale's help: the size the eyes are rendered at, and the headset's images'.
[[nodiscard]] const char* renderScaleHelp()
{
    char(&text)[192] = readouts.renderScaleHelp;
    const Backend* be = backend();
    const EyeSizes s = be ? be->eyeSizes() : EyeSizes{};
    if(s.width <= 0 || s.height <= 0)
    {
        return "Eye rendering resolution, times the headset's (SteamVR's resolution included). Above 1: smoother "
               "edges, slower. Below 1: faster, blurrier.";
    }
    const int w = scaledEyeSize(s.width, s.maxWidth);
    const int h = scaledEyeSize(s.height, s.maxHeight);
    q_snprintf(text, sizeof(text), "Renders %dx%d per eye, resampled to the headset's %dx%d. Above 1: smoother "
                                   "edges, slower. Below 1: faster, blurrier.",
        w, h, s.width, s.height);
    return text;
}

// A server rule's help on a remote server's client: the server sets it (its value shown), yours applies when you host.
[[nodiscard]] const char* serverRuleHelp(const Item& item, const char* help)
{
    za::String& text = readouts.serverRuleHelp;
    char own[48], line[160];
    q_snprintf(own, sizeof(own), item.kind == Item::Slider ? item.format : "%g", item.cvar->value);
    q_snprintf(line, sizeof(line), "Set by the server you are connected to (its value shown). Yours, %s, applies when you host or play alone.", own);
    text = line;
    if(help && help[0])
    {
        text += ' ';
        text += help;
    }
    return text.cStr();
}

// An extendable slider's help: its own (`help`, may be null), and that left and right go past the
// bar's ends and how far; that first while the value is on or past an end, where it matters.
[[nodiscard]] const char* extendableHelp(const Item& item, const char* help)
{
    za::String& text = readouts.extendableHelp;
    char lo[32], hi[32], hint[128];
    q_snprintf(lo, sizeof(lo), item.format, item.hardMin);
    q_snprintf(hi, sizeof(hi), item.format, item.hardMax);
    const bool left = item.hardMin < item.min;
    const bool right = item.hardMax > item.max;
    if(left && right)
    {
        q_snprintf(hint, sizeof(hint), "Hold left or right past the ends to go further (%s to %s).", lo, hi);
    }
    else
    {
        q_snprintf(hint, sizeof(hint), "Hold %s past the end to go further (%s %s).", right ? "right" : "left",
            right ? "up to" : "down to", right ? hi : lo);
    }
    const float v = item.cvar->value;
    const float eps = item.step * 0.01f;
    const bool onEnd = v >= item.max - eps || v <= item.min + eps;
    if(!help || !help[0])
    {
        text = hint;
    }
    else
    {
        // Built in place: the static string keeps its capacity, so no allocation per frame.
        text = onEnd ? hint : help;
        text += ' ';
        text += onEnd ? help : hint;
    }
    return text.cStr();
}

// A row's help as drawn under the list: its own (or its function's), a server rule's note, an extendable slider's
// hint (null: none).
const char* itemHelp(const Item& item)
{
    const char* help = item.cvar == &vr_render_scale ? renderScaleHelp() : item.helpArg ? item.helpArg(item.arg) : item.helpText;
    if(lockedItem(item) && !item.unavailable)
    {
        help = serverRuleHelp(item, help);
    }
    else if(item.kind == Item::Slider && item.extendable)
    {
        help = extendableHelp(item, help);
    }
    if(item.kind == Item::Slider && !lockedItem(item))
    {
        // The fine steps' modifier, last.
        za::String& text = readouts.fineHelp;
        text = help && help[0] ? help : "";
        text += text.empty() ? "" : " ";
        text += vrActive() ? "Hold a grip for fine steps." : "Hold Shift for fine steps.";
        help = text.cStr();
    }
    return help;
}


constexpr double helpPageBase = 2.0; // seconds a part of the help stays, and more for each word on it (vr_menu_help_wpm)

// Word-wrapped to the canvas's width (helpColumns) in the box under the list (helpBoxLines). A longer text turns pages
// by itself, each kept long enough to read its words, with a bar at the box's right showing which part is shown.
void drawHelp(const char* text)
{
    const int columns = helpColumns();
    const int box = helpBoxLines();
    int starts[helpMaxWrapped], lengths[helpMaxWrapped];
    const int lines = wrapHelp(text, columns, starts, lengths);
    int first = 0;
    if(lines > box)
    {
        za::U32 hash = 2166136261u;
        for(const char* c = text; *c; c++)
        {
            hash = (hash ^ static_cast<unsigned char>(*c)) * 16777619u;
        }
        if(hash != ui().helpPaging.hash)
        {
            ui().helpPaging.hash = hash;
            ui().helpPaging.since = realtime;
            ui().helpPaging.shown = -1;
        }
        // Each page's time from its words; the pages one after another, then the first again.
        const int pageCount = (lines + box - 1) / box;
        double durations[helpMaxWrapped];
        double cycle = 0.0;
        for(int pg = 0; pg < pageCount; pg++)
        {
            int words = 0;
            for(int l = pg * box; l < lines && l < (pg + 1) * box; l++)
            {
                for(int i = 0; i < lengths[l]; i++)
                {
                    words += text[starts[l] + i] != ' ' && (i == 0 || text[starts[l] + i - 1] == ' ') ? 1 : 0;
                }
            }
            durations[pg] = helpPageBase + 60.0 * words / CLAMP(60.0, static_cast<double>(vr_menu_help_wpm.value), 600.0);
            cycle += durations[pg];
        }
        double t = fmod(realtime - ui().helpPaging.since, cycle);
        int shown = 0;
        while(shown < pageCount - 1 && t >= durations[shown])
        {
            t -= durations[shown];
            shown++;
        }
        first = shown * box;
        if(shown != ui().helpPaging.shown)
        {
            ui().helpPaging.shown = shown;
            Con_DPrintf("menu help: part %d of %d (%d lines, %.1f s)\n", shown + 1, pageCount, lines, realtime - ui().helpPaging.since);
        }

        // Which part is shown: the bar's thumb, and its share of the text.
        const Layout l = layout();
        const menupaint::Painter p;
        namespace colors = menupaint::colors;
        const float x = static_cast<float>((320 + 8 * columns) / 2 + 3);
        const float top = static_cast<float>(l.helpTop);
        const float height = static_cast<float>(box * 8);
        p.rect(x, x + 2.f, top + height * 0.5f, height * 0.5f * p.k, colors::track);
        const float t0 = top + height * static_cast<float>(first) / static_cast<float>(lines);
        const float t1 = top + height * static_cast<float>(q_min(first + box, lines)) / static_cast<float>(lines);
        p.rect(x, x + 2.f, (t0 + t1) * 0.5f, (t1 - t0) * 0.5f * p.k, colors::scrollThumb);
    }
    for(int line = first; line < lines && line < first + box; line++)
    {
        char buf[64];
        const int n = q_min(lengths[line], static_cast<int>(sizeof(buf)) - 1);
        memcpy(buf, text + starts[line], n);
        buf[n] = '\0';
        M_PrintWhite((320 - 8 * n) / 2, layout().helpTop + (line - first) * 8, buf);
    }
}

// menu_vr dump: every page reached from the VR Settings or the Advanced VR Options (menuRoots) through the pages' links (breadth first: its
// depth, the page linking it first, its rows), each row (kind, header above, label, setting, page
// opened), the links into each page, and the pages no link reaches. For the menus' coverage check
// (docs/vr-port/menu_coverage.sh): each page is shown to be built for what the hands hold now.
const char* searchRowText()
{
    return "";
}

// The Search page's rows: a line of text only (showPage and the menu's bookkeeping expect one; the page is drawn and
// driven by vr_menu_search.inc).
za::Vector<Item> pageSearch()
{
    return {info(searchRowText)};
}

// The Console page's rows: as Search's (vr_menu_console.inc draws and drives it).
za::Vector<Item> pageConsole()
{
    return {info(searchRowText)};
}

// The Map Library page's rows: as Search's (vr_menu_maps.inc draws and drives it).
za::Vector<Item> pageMaps()
{
    return {info(searchRowText)};
}

#include "vr_menu_search.inc"
#include "vr_menu_console.inc"
#include "vr_menu_maps.inc"

void dumpPages()
{
    const int was = ui().page;
    int depth[pageCount];
    int from[pageCount];
    int linksIn[pageCount]{};
    for(int p = 0; p < pageCount; p++)
    {
        depth[p] = -1;
        from[p] = -1;
    }
    za::Vector<int> queue;
    for(const int root : menuRoots)
    {
        queue.pushBack(root);
        depth[root] = 0;
    }
    for(size_t q = 0; q < queue.size(); q++)
    {
        const int p = queue[q];
        showPage(p);
        const auto& list = items(p);
        int settings = 0;
        for(const Item& item : list)
        {
            settings += selectable(item) && item.page < 0 ? 1 : 0;
        }
        Con_Printf("MDPAGE|%d|%d|%d|%s|%d|%d\n", p, depth[p], from[p], pages[p].title, static_cast<int>(list.size()), settings);
        for(int i = 0; i < static_cast<int>(list.size()); i++)
        {
            const Item& item = list[i];
            const char kind = item.kind == Item::Header ? 'H'
                              : item.kind == Item::Slider ? 'S'
                              : item.kind == Item::Cycle  ? 'C'
                              : item.kind == Item::Info   ? 'I'
                              : item.page >= 0 && !item.actionArg ? 'O'
                                                                  : 'A';
            za::String label = item.label ? item.label : item.infoArg ? item.infoArg(item.arg) : item.info ? item.info() : "";
            for(char& c : label)
            {
                c = c == '|' || c == '\n' ? ' ' : c;
            }
            Con_Printf("MDROW|%d|%s|%c|%s|%s|%s|%d\n", p, pages[p].title, kind, rowSection(list, i), label.cStr(),
                item.cvar ? item.cvar->name : "", kind == 'O' ? item.page : -1);
            if(kind == 'O' && item.page >= 0 && item.page < pageCount)
            {
                linksIn[item.page]++;
                if(depth[item.page] < 0)
                {
                    depth[item.page] = depth[p] + 1;
                    from[item.page] = p;
                    queue.pushBack(item.page);
                }
            }
        }
    }
    for(int p = 0; p < pageCount; p++)
    {
        Con_Printf("MDLINKS|%d|%s|%d|%d\n", p, pages[p].title, linksIn[p], depth[p]);
    }
    showPage(was);
}

// menu_vr helpcheck [columns]: every page's rows' help wrapped as drawHelp wraps it (at `columns`, else the canvas's
// width now): HELPLONG for each help longer than its page's box (its pages turn), HELPPAGE for each page whose box
// grew, HELPSUM the count, the longest and how many need turning pages. For the help's fit (the menu's tests).
void helpCheck(int columnsAsked)
{
    const int was = ui().page;
    const int columns = columnsAsked > 0 ? columnsAsked : helpColumns();
    int rows = 0, longest = 0, longestChars = 0, turning = 0, grown = 0;
    int histogram[helpMaxLines + 2]{};
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].build == pageSearch || pages[p].build == pageConsole || pages[p].build == pageMaps)
        {
            continue;
        }
        showPage(p);
        const auto& list = items(p);
        int box = helpMinLines;
        for(const Item& item : list)
        {
            const char* help = itemHelp(item);
            box = help ? q_max(box, wrapHelp(help, columns)) : box;
        }
        box = q_min(box, helpMaxLines);
        if(box > helpMinLines)
        {
            grown++;
            Con_Printf("HELPPAGE|%d|%s|%d lines|%d rows shown\n", p, pages[p].title, box, visibleRows(list));
        }
        for(const Item& item : list)
        {
            const char* help = itemHelp(item);
            if(!help || !help[0])
            {
                continue;
            }
            const int lines = wrapHelp(help, columns);
            rows++;
            histogram[q_min(lines, helpMaxLines + 1)]++;
            longest = q_max(longest, lines);
            longestChars = q_max(longestChars, static_cast<int>(strlen(help)));
            if(lines > box)
            {
                turning++;
                Con_Printf("HELPLONG|%d|%s|%s|%d chars|%d lines|box %d\n", p, pages[p].title,
                    item.label ? item.label : "", static_cast<int>(strlen(help)), lines, box);
            }
        }
    }
    showPage(was);
    Con_Printf("HELPSUM|%d columns|%d rows with help|longest %d lines (%d chars)|%d pages grown|%d turn pages|lines:",
        columns, rows, longest, longestChars, grown, turning);
    for(int l = 1; l <= helpMaxLines + 1; l++)
    {
        Con_Printf(" %d%s=%d", l, l > helpMaxLines ? "+" : "", histogram[l]);
    }
    Con_Printf("\n");
}

// The path to a page from Quake's main menu, by what the player reads on the way (menu::pathTo): the fewest links from
// the VR Settings ("Options > VR Settings > ...") or the Advanced VR Options ("Advanced VR > ...": the main menu's row
// and the corner's button), each page as its link names it. False when no page has the title, no link reaches it, or (a row asked
// for) the page has no row of that label.
bool resolvePath(za::StringView spec, za::String& out)
{
    const auto trim = [](za::StringView s) {
        while(s.startsWith(' '))
        {
            s.removePrefix(1);
        }
        while(s.endsWith(' '))
        {
            s.removeSuffix(1);
        }
        return za::String{s};
    };
    const size_t split = spec.find('>');
    const za::String title = trim(spec.substrByPosLen(0, split));
    const za::String row = split == za::StringView::nPos ? za::String{} : trim(spec.substrByPosLen(split + 1));
    int target = -1;
    for(int p = 0; p < pageCount && target < 0; p++)
    {
        target = q_strcasecmp(pages[p].title, title.cStr()) ? -1 : p;
    }
    if(target < 0)
    {
        return false;
    }
    // Every page and row, whatever Menu Detail shows (the pages built for it again as they are next shown); the level
    // the way needs said after it.
    const int shownLevel = menuLevel();
    const int wasOverride = levelOverride;
    levelOverride = LevelDeveloper;
    struct Restore
    {
        int was;
        ~Restore() { levelOverride = was; }
    } restore{wasOverride};
    int needs = pages[target].level;
    // The shortest way there through the pages' links (breadth first from the tree's roots, as Search): each page
    // named by the link that opens it.
    int from[pageCount];
    const char* link[pageCount]{};
    for(int& f : from)
    {
        f = -1;
    }
    int queue[pageCount];
    int queued = 0;
    for(const int root : menuRoots)
    {
        from[root] = root;
        queue[queued++] = root;
    }
    for(int q = 0; q < queued && from[target] < 0; q++)
    {
        for(const Item& item : items(queue[q]))
        {
            if(item.kind == Item::Action && item.page >= 0 && item.page < pageCount && !item.actionArg && item.label &&
                from[item.page] < 0)
            {
                from[item.page] = queue[q];
                link[item.page] = item.label;
                queue[queued++] = item.page;
            }
        }
    }
    if(from[target] < 0)
    {
        return false; // no link reaches it
    }
    za::Vector<const char*> names; // from the page up
    int root = target;
    for(; !isMenuRoot(root); root = from[root])
    {
        names.pushBack(link[root]);
        needs = q_max(needs, pages[root].level);
    }
    needs = q_max(needs, pages[root].level);
    out = root == PageMain ? za::String{"Options > "} + pages[PageMain].title : za::String{"Advanced VR"};
    for(const char* name : za::reversed(names))
    {
        out += " > ";
        out += name;
    }
    if(!row.empty())
    {
        const char* found = nullptr;
        for(const Item& item : items(target))
        {
            if(item.label && item.kind != Item::Info && !q_strcasecmp(item.label, row.cStr()))
            {
                found = item.label;
                needs = q_max(needs, item.level);
                break;
            }
        }
        if(!found)
        {
            return false;
        }
        out += " > ";
        out += found;
    }
    // (Advanced VR raises Menu Detail to Advanced itself: only a page or row above it says so on that way.)
    if(needs > (root == PageAdvanced ? q_max(shownLevel, static_cast<int>(LevelAdvanced)) : shownLevel))
    {
        out += va(" (Menu Detail: %s)", levelName(needs));
    }
    return true;
}

} // namespace

bool qvr::menu::pathTo(za::StringView spec, za::String& out)
{
    return resolvePath(spec, out);
}

za::String qvr::menu::expandPaths(za::StringView text, int width, int* missing)
{
    constexpr za::StringView open = "{menu:";
    za::String out;
    size_t at = 0;
    for(size_t start; (start = text.find(open, at)) != za::StringView::nPos;)
    {
        const size_t end = text.find('}', start);
        if(end == za::StringView::nPos)
        {
            break;
        }
        out.append(text.substrByPosLen(at, start - at));
        const za::StringView spec = text.substrByPosLen(start + open.size(), end - start - open.size());
        za::String path;
        if(!resolvePath(spec, path))
        {
            if(missing)
            {
                (*missing)++;
            }
            Con_Warning("MENU PATH MISSING: {menu:%.*s} (a map's board names a menu page or row that no longer exists)\n",
                static_cast<int>(spec.size()), spec.data());
            out += "[menu? ";
            out.append(spec);
            out += "]";
            at = end + 1;
            continue;
        }
        // Broken into lines at its " > "s, each about `width` characters from the start of its line.
        const size_t lineStart = out.rfind('\n');
        int column = static_cast<int>(out.size() - (lineStart == za::StringView::nPos ? 0 : lineStart + 1));
        za::StringView rest = path;
        for(bool first = true; !rest.empty(); first = false)
        {
            const size_t sep = rest.find(" > ");
            const za::StringView part = rest.substrByPosLen(0, sep);
            rest = sep == za::StringView::nPos ? za::StringView{} : rest.substrByPosLen(sep + 3);
            const int need = static_cast<int>(part.size()) + (first ? 0 : 3);
            if(!first && column + need > width)
            {
                out += "\n> ";
                column = 2;
            }
            else if(!first)
            {
                out += " > ";
                column += 3;
            }
            out.append(part);
            column += static_cast<int>(part.size());
        }
        at = end + 1;
    }
    out.append(text.substrByPosLen(at));
    return out;
}

// vr_menu_path_check [file or text]: every {menu:...} in the loaded map's entities (or in a text file of the game's, as
// maps/vrcalibration.map; or in the text given, starting with "{"), each with its path; "menu paths: N found, M missing"
// last.
void qvr::menu::pathCheck_f()
{
    const char* data = nullptr;
    byte* file = nullptr;
    if(Cmd_Argc() > 1 && Cmd_Argv(1)[0] == '{')
    {
        data = Cmd_Args(); // the text itself: vr_menu_path_check "{menu:Locomotion>Lean}"
    }
    else if(Cmd_Argc() > 1)
    {
        file = COM_LoadMallocFile(Cmd_Argv(1), nullptr);
        if(!file)
        {
            Con_Printf("vr_menu_path_check: can't open %s\n", Cmd_Argv(1));
            return;
        }
        data = reinterpret_cast<const char*>(file);
    }
    else if(sv.active && sv.worldmodel && sv.worldmodel->entities)
    {
        data = sv.worldmodel->entities;
    }
    else
    {
        Con_Printf("vr_menu_path_check [file or {menu:...}]: the loaded map's {menu:...} board paths, or a file's\n");
        return;
    }
    int found = 0, missing = 0;
    const za::StringView text{data};
    for(size_t at = 0; (at = text.find("{menu:", at)) != za::StringView::nPos;)
    {
        const size_t end = text.find('}', at);
        if(end == za::StringView::nPos)
        {
            break;
        }
        const za::StringView spec = text.substrByPosLen(at + 6, end - at - 6);
        za::String path;
        if(resolvePath(spec, path))
        {
            found++;
            Con_Printf("menu path: %.*s -> %s\n", static_cast<int>(spec.size()), spec.data(), path.cStr());
        }
        else
        {
            missing++;
            Con_Printf("MENU PATH MISSING: %.*s\n", static_cast<int>(spec.size()), spec.data());
        }
        at = end + 1;
    }
    Con_Printf("menu paths: %d found, %d missing\n", found, missing);
    VR_HeapFree(file);
}

// vr_menu_dump [level 0-2]: every page shown at that Menu Detail (default 0, Standard), its rows' labels, choices and help,
// as "MD|" lines in the console (the copy review: typos, wording, rows in the wrong level).
void qvr::menu::dump_f()
{
    const int level = CLAMP(0, Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0, LevelCount - 1);
    const int was = levelOverride;
    levelOverride = level;
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].level > level)
        {
            continue;
        }
        Con_Printf("MD|PAGE|%d|%s\n", p, pages[p].title);
        for(const Item& item : items(p))
        {
            const char* kind = item.kind == Item::Header ? "HEAD" : item.kind == Item::Slider ? "SLID" :
                item.kind == Item::Cycle ? "CYCL" : item.kind == Item::Action ? "ACT" : "INFO";
            za::String choices;
            for(const Choice& c : item.choices)
            {
                choices += c.label;
                choices += " / ";
            }
            const char* label = item.label ? item.label : (item.info ? item.info() : "");
            Con_Printf("MD|%s|%s|%s|%s\n", kind, label ? label : "", choices.cStr(), item.helpText ? item.helpText : "");
        }
    }
    levelOverride = was;
}

void qvr::menu::press_f()
{
    // (Unquoted, the page and the row split at " > ": a test script's quotes need not survive its shell.)
    char args[512];
    q_strlcpy(args, Cmd_Args() ? Cmd_Args() : "", sizeof(args));
    char* split = strstr(args, " > ");
    if(!split)
    {
        Con_Printf("usage: vr_menu_press <page title> > <row label>\n");
        return;
    }
    *split = '\0';
    const char* title = args;
    const char* label = split + 3;
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    const char* command = nullptr;
    void (*action)() = nullptr;
    bool found = false;
    bool locked = false;
    for(int p = 0; p < pageCount && !found; p++)
    {
        if(q_strcasecmp(pages[p].title, title))
        {
            continue;
        }
        for(const Item& item : items(p))
        {
            if(item.kind == Item::Action && item.actionArg && item.infoArg && !q_strcasecmp(item.infoArg(item.arg), label))
            {   // a list row (the campaign selector): its text, then its pick(arg)
                locked = item.dimArg && item.dimArg(item.arg);
                found = true;
                if(!locked) { item.actionArg(item.arg); Con_Printf("vr_menu_press: %s > %s\n", title, label); }
                levelOverride = was;
                if(locked) { Con_Printf("vr_menu_press: %s > %s is locked (unavailable), not pressed\n", title, label); }
                return;
            }
            if(item.kind == Item::Action && item.label && !q_strcasecmp(item.label, label))
            {
                command = item.command;
                action = item.page < 0 && !item.actionArg ? item.action : nullptr;
                locked = item.unavailable;
                found = true;
                break;
            }
        }
    }
    levelOverride = was;
    if(!found || (!command && !action))
    {
        Con_Printf("vr_menu_press: no button \"%s\" on page \"%s\"\n", label, title);
        return;
    }
    if(locked)
    {
        Con_Printf("vr_menu_press: %s > %s is locked (unavailable), not pressed\n", title, label);
        return;
    }
    Con_Printf("vr_menu_press: %s > %s%s%s\n", title, label, command ? ": " : "", command ? command : "");
    if(command)
    {
        runCommand(command);
    }
    else
    {
        action();
    }
}

int qvr::menu::bodyCalibrationPage()
{
    return pageIndex(pageBodyCalibration);
}

int qvr::menu::retroOverridePage()
{
    return pageIndex(pageRetroOverride);
}

// `togglemenu` in a menu: VR Calibration waiting on Body Calibration's page (paused, or the body's result not trusted)
// closes it at once, as the menu button does; elsewhere Quake's way (to the main menu first).
extern "C" int VR_MenuToggleCloses()
{
    return m_state == m_vr && key_dest == key_menu && pages[ui().page].build == pageBodyCalibration &&
           qvr::setup::waitsOnMenu();
}

// Options > VR Settings (and menu_vr): the VR Settings; Back returns to the menu they were opened from (NavStack).
extern "C" void VR_Menu_Open()
{
    openFromAnywhere(PageMain);
}

// The main menu's VR Settings and Advanced VR rows (menu.c).
extern "C" void VR_Menu_OpenFromMain(int advanced)
{
    if(advanced)
    {
        qvr::menu::jumpToAdvanced();
    }
    else
    {
        VR_Menu_Open();
    }
}

// The main menu's Select Campaign row (menu.c), the credits and vr_campaign_menu: the official campaigns' page.
extern "C" void VR_OpenCampaignSelector()
{
    openFromAnywhere(pageIndex(pageCampaigns)); // (Back: the main menu, or up the tree from the hub's board)
}

// Ironwail's Levels opened by a jump (the main menu's Play Custom Map, the corner's Levels): Back from them returns
// here (VR_NavBack).
extern "C" void VR_NavJump(int state)
{
    if(m_state == m_vr && key_dest == key_menu)
    {
        navSyncTop();
    }
    else
    {
        navEnterFrom(outsideMenu());
    }
    navPush(outsidePlace(state));
    ui().navJumpPending = state;
}

// An outside menu opened (Ironwail's Levels; `previous` the menu shown before): by a jump or by Back into it, its place on
// the stack kept; from its own way in (Single Player, a mod's), the stack started again (its Back its own).
extern "C" void VR_NavEntered(int state, int previous)
{
    const bool kept = ui().navJumpPending == state || ui().navReturning || previous == m_skill || previous == state;
    ui().navJumpPending = -1;
    if(!kept || navTop() != outsidePlace(state))
    {
        navReset();
    }
}

// Back from an outside menu entered by a jump: where it came from (nonzero), else 0 (its own Back).
extern "C" int VR_NavBack(int state)
{
    if(ui().nav.count < 2 || navTop() != outsidePlace(state))
    {
        navReset();
        return 0;
    }
    ui().nav.count--;
    const int dest = navTop();
    if(dest >= 0)
    {
        enterVrMenu();
        showPage(dest);
        S_LocalSound("misc/menu2.wav");
    }
    else
    {
        leaveTo(-1 - dest);
    }
    return 1;
}

extern "C" int VR_MenuMainShowsMods()
{
    return vr_menu_main_mods.value != 0.f;
}

extern "C" void VR_OpenMapLibrary()
{
    qvr::menu::openMaps();
}

// The main menu's VR Tutorial and VR Hub rows, confirmed (menu.c): as the Play page's Tutorial and VR Hub.
extern "C" void VR_StartTutorial()
{
    Con_Printf("VR Tutorial: starting\n");
    playTutorial();
}

extern "C" void VR_StartHub()
{
    Con_Printf("VR Hub: going there\n");
    playHub();
}

// menu_vr [page [row]]: the VR Settings, or one of its pages (1: Advanced VR Options), opened through the pages
// above it in the tree; menu_vr list: the pages' numbers and places; menu_vr dump: every page's rows.
void qvr::menu::handCalMatch_f()
{
    matchControllerPreview();
}

void qvr::menu::init()
{
    for(const Preset& p : presets)
    {
        Cvar_SetCallback(p.choice, onPresetChosen);
    }
    Cvar_SetCallback(&vr_menu_turning, onWrapperSet);
    Cvar_SetCallback(&vr_menu_move_towards, onWrapperSet);
    for(const HandWrapper& w : handWrappers)
    {
        Cvar_SetCallback(w.wrapper, onWrapperSet);
    }
    for(const HolsterWrapper& w : holsterWrappers)
    {
        Cvar_SetCallback(w.wrapper, onWrapperSet);
    }
    Cvar_SetCallback(&vr_menu_bullettime, onWrapperSet);
    Cvar_SetCallback(&vr_menu_pickup_mode, onWrapperSet);
}

void qvr::menu::command_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "list"))
    {
        for(int p = 0; p < pageCount; p++)
        {
            za::String path = pages[p].title;
            for(int q = p; q != PageMain; q = homeOf(q))
            {
                path = za::String{pages[homeOf(q)].title} + " > " + path;
            }
            Con_Printf("%2d %s\n", p, path.cStr());
        }
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "dump"))
    {
        dumpPages();
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "rows"))
    {
        printRows();
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "recent"))
    {
        searchRecentCommand(Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(2), "clear"));
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "helpcheck"))
    {
        helpCheck(Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 0);
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "pos"))
    {
        const char* corner = menuui::toolbarFocused() ? ", corner buttons selected" : "";
        if(key_dest == key_menu)
        {
            // The layout (menu coordinates): the corner's column left of the menu's text, the rows from the top.
            Con_Printf("menu_vr pos: text from x %.0f, buttons to x %.0f (%s), y %.1f\n", menu::contentLeft(),
                menuui::toolbarRight(), menuui::toolbarBeside() ? "beside" : "over", menuui::toolbarBottom());
            if(m_state == m_vr)
            {
                Con_Printf("menu_vr pos: rows from y %d, %d shown\n", layout().listTop, visibleRows(items(ui().page)));
            }
            float bx0, bx1, by0, by1;
            menuui::bannerRect(bx0, bx1, by0, by1);
            if(!menuui::active() && bx1 >= bx0)
            {
                Con_Printf("menu_vr pos: banner x %.0f..%.0f, y %.0f..%.0f (text from x %d)\n", bx0, bx1, by0, by1,
                    m_state == m_vr ? static_cast<int>(menu::contentLeft()) : M_TextLeft());
            }
            if(menuui::active())
            {
                const bool beside = menuui::bannerColumnRect(bx0, bx1, by0, by1);
                if(menuui::bannerColumnShown())
                {
                    Con_Printf("menu_vr pos: banner x %.0f..%.0f, y %.0f..%.0f (%s the buttons, from x %.0f)\n", bx0, bx1, by0,
                        by1, beside ? "left of" : "under", menuui::toolbarLeft());
                }
                else
                {
                    Con_Printf("menu_vr pos: banner not drawn\n");
                }
            }
            menuui::printVersionLabel();
            menuui::printRecordingButton();
        }
        if(key_dest != key_menu || m_state != m_vr)
        {
            const char* name = key_dest != key_menu      ? "closed"
                               : m_state == m_maps         ? "levels"
                               : m_state == m_singleplayer ? "single player"
                               : m_state == m_options      ? "options"
                               : m_state == m_main         ? "main"
                                                           : "other";
            if(key_dest == key_menu && m_state == m_main)
            {
                int step, gap;
                M_Main_Layout(&step, &gap);
                Con_Printf("menu_vr pos: menu %d (%s), row \"%s\"%s (rows %d apart, groups %d more)\n", static_cast<int>(m_state),
                    name, M_Main_RowLabel(), corner, step, gap);
                float rx0, rx1;
                M_Main_RowsExtent(&rx0, &rx1);
                Con_Printf("menu_vr pos: main rows x %.0f..%.0f (middle %.1f; the title's 160)\n", rx0, rx1, (rx0 + rx1) * 0.5f);
                float ry0, ry1, cy0, cy1;
                M_Main_RowsSpan(&ry0, &ry1, &cy0, &cy1);
                Con_Printf("menu_vr pos: main title and rows y %.0f..%.0f (middle %.1f; the canvas's y %.0f..%.0f, middle %.1f)\n",
                    ry0, ry1, (ry0 + ry1) * 0.5f, cy0, cy1, (cy0 + cy1) * 0.5f);
                menuui::printLaser();
                return;
            }
            Con_Printf("menu_vr pos: menu %d (%s)%s%s\n", static_cast<int>(m_state), name, corner,
                key_dest == key_menu && m_state == m_singleplayer ? va(", row %d", m_singleplayer_cursor) : "");
            return;
        }
        const auto& list = items(ui().page);
        const int cursor = ui().cursors[ui().page];
        int outside = m_none;
        const int back = backTarget(outside);
        Con_Printf("menu_vr pos: page %d \"%s\" (back to %d%s), row %d \"%s\" under \"%s\", scroll %d of %d rows%s\n", ui().page,
            pages[ui().page].title, back, outside != m_none ? va(", menu %d", outside) : "", cursor, rowLabel(list[cursor]), rowSection(list, cursor),
            ui().scrolls[ui().page], static_cast<int>(list.size()), corner);
        if(cursor < static_cast<int>(list.size()) && list[cursor].cvar && list[cursor].kind == Item::Slider)
        {
            char shown[64];
            sliderText(list[cursor], list[cursor].cvar->value, shown, sizeof(shown));
            Con_Printf("menu_vr pos: slider %s \"%s\" (shown %s), step %g%s\n", list[cursor].cvar->name,
                list[cursor].cvar->string, shown, sliderStep(list[cursor]), fineHeld() ? " (fine)" : "");
        }
        if(const Item* open = dropDownItem())
        {
            const DropDown& d = ui().dropDown;
            Con_Printf("menu_vr pos: list open on row %d, %d choices, highlighted %d \"%s\", shows %d from %d, box x %d..%d y %d..%d\n",
                d.row, d.count, d.hover, open->choices[d.hover].label, d.rows, d.scroll, d.x, d.x + d.width, d.top, d.top + d.rows * 8);
        }
        return;
    }
    VR_Menu_Open();
    if(Cmd_Argc() > 1)
    {
        const int target = Q_atoi(Cmd_Argv(1));
        if(target >= PageMain && target < pageCount)
        {
            openInTree(target); // (the VR Settings: already shown)
            // menu_vr <page> <row>: the cursor on that row (counted from 0, headers and lines of text included), if it
            // can rest there (scripts, screenshots); or on the first setting whose label starts with <row>'s text
            // (menu_vr 90 "Holstered X").
            const auto& list = items(ui().page);
            int row = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : -1;
            if(Cmd_Argc() > 2 && (Cmd_Argv(2)[0] < '0' || Cmd_Argv(2)[0] > '9'))
            {
                for(int i = 0; i < static_cast<int>(list.size()); i++)
                {
                    if(list[i].label && selectable(list[i]) && !q_strncasecmp(list[i].label, Cmd_Argv(2), strlen(Cmd_Argv(2))))
                    {
                        row = i;
                        break;
                    }
                }
            }
            if(row >= 0 && row < static_cast<int>(list.size()) && selectable(list[row]))
            {
                ui().cursors[ui().page] = row;
            }
        }
    }
}

bool qvr::menu::holsterPreview(int& hand, int& kind)
{
    if(m_state != m_vr || pages[ui().page].build != pageWofsHolstered || vr_weapon_holster_preview.value == 0.f ||
        weaponOffsetsSlot < 0 || weaponOffsetsSlot == weapons::fistSlot() || weaponOffsetsHolster < 0 ||
        weapons::heldSlot(weaponOffsetsHand) != weaponOffsetsHeldSlot)
    {
        return false; // not on the page, off, the empty hand, or the hand holds another weapon now
    }
    const cvar_t* chosen = selectedSetting();
    bool on = chosen && (chosen == &vr_weapon_holster || chosen == &vr_weapon_holster_preview);
    for(int f = 0; chosen && !on && f < weapons::holsteredFields; f++)
    {
        on = chosen == weapons::cvar(weaponOffsetsSlot, weapons::holsteredKey(editedHolster(), f));
    }
    hand = weaponOffsetsHand;
    kind = weaponOffsetsHolster;
    return on;
}

bool qvr::menu::flashlightMountPreview(int& hand)
{
    if(m_state != m_vr || pages[ui().page].build != pageWofsFlashlight || vr_flashlight_mount_preview.value == 0.f ||
        weaponOffsetsSlot < 0 || weaponOffsetsSlot == weapons::fistSlot() || weapons::heldSlot(weaponOffsetsHand) != weaponOffsetsHeldSlot)
    {
        return false; // not on the page, off, the empty hand, or the hand holds another weapon now
    }
    hand = weaponOffsetsHand;
    return true;
}

namespace
{


// A setting placing (or showing) something worn on the body: the holsters, the pouches, the flashlight on the belt and
// on the head, the wrist gadget's mount, the virtual stock, the pauldrons.
[[nodiscard]] bool tweaksBody(const char* name)
{
    constexpr const char* prefixes[] = {"vr_menu_holster_", "vr_hip_offset_", "vr_hip_holster_", "vr_shoulder_holster_",
        "vr_upper_holster_", "vr_leg_holster_", "vr_grenade_pouch", "vr_ammo_pouch", "vr_flashlight_head",
        "vr_shoulder_offset_", "vr_virtual_stock", "vr_body_pauldron", "vr_holster_zone"};
    constexpr const char* names[] = {"vr_show_hip_holsters", "vr_show_shoulder_holsters", "vr_show_upper_holsters",
        "vr_show_grenade_pouch", "vr_show_flashlight_zones", "vr_show_virtual_stock", "vr_flashlight_forward",
        "vr_flashlight_up", "vr_flashlight_out", "vr_flashlight_tilt", "vr_gadget_scale", "vr_gadget_x", "vr_gadget_y",
        "vr_gadget_z", "vr_gadget_pitch", "vr_gadget_yaw", "vr_gadget_roll"};
    for(const char* p : prefixes)
    {
        if(!strncmp(name, p, strlen(p)))
        {
            return true;
        }
    }
    for(const char* n : names)
    {
        if(!strcmp(name, n))
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool qvr::menu::bodyClonePage()
{
    if(key_dest != key_menu || m_state != m_vr || ui().page < 0 || ui().page >= pageCount)
    {
        ui().bodyCloneLatch = -1;
        return false;
    }
    const PageBuilder b = pages[ui().page].build;
    if(vr_menu_body_clone.value >= 2.f || b == pageFlashlightMounts || b == pageWofsHolstered || b == pageVrsRangerHead ||
        b == pageHolsterZones || b == pageSeatedHolsters || b == pageVrsBodyEquipment || b == pageVrsHolsters ||
        ui().bodyCloneLatch == ui().page)
    {
        return true;
    }
    const cvar_t* chosen = selectedSetting();
    ui().bodyCloneLatch = chosen && tweaksBody(chosen->name) ? ui().page : -1;
    return ui().bodyCloneLatch == ui().page;
}

int qvr::menu::currentPage()
{
    return ui().page;
}

int qvr::menu::currentInstance()
{
    return currentMenu;
}

void qvr::menu::selectInstance(int instance)
{
    instance = CLAMP(0, instance, instanceCount - 1);
    if(instance == currentMenu)
    {
        return;
    }
    // Quake's menu state and the menus' mouse go with the instance (MenuInstance::quakeState).
    ui().quakeState = static_cast<int>(m_state);
    ui().mouseX = m_mousex;
    ui().mouseY = m_mousey;
    currentMenu = instance;
    m_state = static_cast<m_state_e>(ui().quakeState);
    m_mousex = ui().mouseX;
    m_mousey = ui().mouseY;
    M_RefreshBounds(); // (its menu's layout: the lists' mouse)
}

bool qvr::menu::instanceOpen(int instance)
{
    if(instance <= 0)
    {
        return key_dest == key_menu && mainState() != m_none;
    }
    // (Hidden while the menu is closed: the panels stay open in their places, shown again when it opens, MULTI_MENU.md.)
    return instance < instanceCount && menuInstances[instance].open && key_dest == key_menu && mainState() != m_none;
}

int qvr::menu::openInstances()
{
    int n = 0;
    for(int i = 0; i < instanceCount; i++)
    {
        n += instanceOpen(i) ? 1 : 0;
    }
    return n;
}

int qvr::menu::mainState()
{
    return currentMenu == 0 ? static_cast<int>(m_state) : menuInstances[0].quakeState;
}

bool qvr::menu::openInstance()
{
    if(key_dest != key_menu || m_state == m_none)
    {
        return false;
    }
    glm::vec3 corner, xAxis, yAxis;
    if(!panel::menuQuad(hands::current(), 0, corner, xAxis, yAxis))
    {
        Con_Printf("New Panel: only with the menu in the headset\n");
        S_LocalSound("misc/menu1.wav");
        return false;
    }
    int slot = -1;
    for(int i = 1; i < instanceCount && slot < 0; i++)
    {
        slot = menuInstances[i].open ? -1 : i;
    }
    if(slot < 0)
    {
        Con_Printf("New Panel: %d panels open at most\n", instanceCount);
        S_LocalSound("misc/menu1.wav");
        return false;
    }

    // The page shown here (not one for one panel at a time), else the VR Settings; each page's selection and scroll as
    // here, nothing under way.
    const int target = (m_state == m_vr || m_state == m_confirm) && !oneViewerGroup(ui().page) ? ui().page : PageMain;
    MenuInstance& n = menuInstances[slot];
    const MenuInstance& from = ui();
    n = MenuInstance{};
    for(int p = 0; p < pageCount; p++)
    {
        n.cursors[p] = from.cursors[p];
        n.scrolls[p] = from.scrolls[p];
        n.leftAnchors[p] = from.leftAnchors[p];
        n.visits[p] = from.visits[p];
    }
    n.visitCount = from.visitCount;
    n.open = true;
    n.quakeState = m_vr;
    n.page = PageMain;

    selectInstance(slot);
    navReset();
    if(target == PageMain)
    {
        navPush(PageMain);
        showPage(PageMain);
    }
    else
    {
        openInTree(target);
    }
    panel::openMenuPanel(slot);
    S_LocalSound("misc/menu2.wav");
    Con_DPrintf("menu panel %d opened: %s\n", slot + 1, pages[ui().page].title);
    return true;
}

namespace
{
void closeQuietly(int instance)
{
    if(instance <= 0 || instance >= menu::instanceCount || !menuInstances[instance].open)
    {
        return;
    }
    panel::closeMenuPanel(instance);
    if(currentMenu == instance)
    {
        menu::selectInstance(0);
    }
    menuInstances[instance].open = false;
    menuInstances[instance].quakeState = m_none;
}
} // namespace

void qvr::menu::closeInstance(int instance)
{
    if(instance > 0 && instance < instanceCount && menuInstances[instance].open)
    {
        closeQuietly(instance);
        S_LocalSound("misc/menu2.wav");
    }
}

void qvr::menu::closeExtraInstances()
{
    for(int i = 1; i < instanceCount; i++)
    {
        closeQuietly(i);
    }
}

void qvr::menu::tidyInstances()
{
    bool extras = false;
    for(int i = 1; i < instanceCount; i++)
    {
        extras = extras || menuInstances[i].open;
    }
    if(!extras)
    {
        return;
    }
    if(key_dest != key_menu || mainState() == m_none)
    {
        // The menu closed (from whichever panel): the panels are hidden, not closed (instanceOpen), each keeping its
        // page and place; the first is the current one again.
        if(currentMenu != 0)
        {
            const bool none = m_state == m_none;
            m_state = m_vr;
            selectInstance(0);
            if(none)
            {
                m_state = m_none;
            }
        }
        return;
    }
    if(currentMenu != 0 && m_state != m_vr && m_state != m_confirm && m_state != m_maps)
    {
        // Out of the VR pages to one of Quake's menus (single: on the first panel; the Levels, from the corner's button,
        // stay on the panel pressed on); this panel stays on its page.
        const m_state_e state = m_state;
        m_state = m_vr;
        selectInstance(0);
        m_state = state;
    }
}

void qvr::menu::drawInstance(int instance)
{
    const int was = currentMenu;
    drawnInstance = instance;
    selectInstance(instance);
    M_DrawPanel();
    selectInstance(was);
    drawnInstance = -1;
}

void qvr::menu::panels_f()
{
    const char* arg = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    const int n = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) - 1 : -1;
    if(!q_strcasecmp(arg, "open"))
    {
        openInstance();
        return;
    }
    if(!q_strcasecmp(arg, "close") && n >= 1 && n < instanceCount)
    {
        closeInstance(n);
        return;
    }
    if(!q_strcasecmp(arg, "focus") && n >= 0 && n < instanceCount && instanceOpen(n))
    {
        selectInstance(n);
        return;
    }
    if(arg[0])
    {
        Con_Printf("vr_menu_panels [open | close <2..%d> | focus <1..%d>]: the menu panels open (MENUPANELS), or another opened, "
                   "one closed or focused (the first closes with Back to game)\n",
            instanceCount, instanceCount);
        return;
    }
    Con_Printf("MENUPANELS open %d focused %d\n", openInstances(), currentMenu + 1);
    const int was = currentMenu;
    for(int i = 0; i < instanceCount; i++)
    {
        if(!instanceOpen(i))
        {
            continue;
        }
        selectInstance(i);
        const bool vrPage = m_state == m_vr;
        const auto& list = items(ui().page);
        const int cursor = ui().cursors[ui().page];
        const char* row = vrPage && cursor >= 0 && cursor < static_cast<int>(list.size()) ? rowLabel(list[cursor]) : "";
        Con_Printf("MENUPANEL %d menu %d page %d \"%s\" row \"%s\" value %g drawn %d\n", i + 1, static_cast<int>(m_state),
            vrPage ? ui().page : -1, vrPage ? pages[ui().page].title : "", row, ui().drawnValue,
            ui().drawnFrame >= 0 ? host_framecount - ui().drawnFrame : -1);
    }
    selectInstance(was);
}

extern "C" int VR_MenuDrawSelect()
{
    if(instanceDrawDepth++ > 0 || drawnInstance >= 0)
    {
        return -1;
    }
    menu::tidyInstances();
    if(currentMenu == 0 || !panel::menusInEyes())
    {
        return -1; // (on a flat screen, or the runtime's own panel: the focused panel's menu only)
    }
    const int was = currentMenu;
    menu::selectInstance(0);
    return was;
}

extern "C" int VR_MenuDrawRestore(int was)
{
    instanceDrawDepth--;
    if(was < 0)
    {
        return 0;
    }
    menu::selectInstance(was);
    return 1;
}

int qvr::menu::toolgunCheatCount()
{
    findToolgunCheats();
    return static_cast<int>(toolgunCheats.list.size());
}

const char* qvr::menu::toolgunCheatName(int i)
{
    return i >= 0 && i < toolgunCheatCount() ? toolgunCheats.list[i].name : "";
}

bool qvr::menu::toolgunCheatOn(int i)
{
    return i >= 0 && i < toolgunCheatCount() && toolgunCheatIsOn(toolgunCheats.list[i]);
}

void qvr::menu::toolgunCheatUse(int i)
{
    if(i < 0 || i >= toolgunCheatCount())
    {
        return;
    }
    const ToolgunCheat& c = toolgunCheats.list[i];
    if(c.cvar)
    {
        Cvar_SetValueQuick(c.cvar, c.cvar->value != 0.f ? 0.f : 1.f);
    }
    if(c.command)
    {
        runCommand(c.command);
    }
    if(c.action)
    {
        c.action();
    }
    Con_Printf("toolgun: %s\n", c.name);
}

const cvar_t* qvr::menu::selectedSetting()
{
    if(m_state != m_vr)
    {
        return nullptr;
    }
    const auto& list = items(ui().page);
    const int cursor = ui().cursors[ui().page];
    return cursor >= 0 && cursor < static_cast<int>(list.size()) ? list[cursor].cvar : nullptr;
}

void qvr::menu::jumpToAdvanced()
{
    if(menuLevel() < LevelAdvanced)
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(LevelAdvanced)); // (the corner's Advanced VR: the pages it lists)
    }
    if(m_state == m_vr && ui().page == PageAdvanced)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(PageAdvanced); // (Back: where it was pressed)
}

void qvr::menu::openSearch()
{
    openSearchPage();
}

void qvr::menu::openConsole()
{
    openConsolePage();
}

// The Map Library, from the corner's Maps button or Single Player > Map Library.
void qvr::menu::openMaps()
{
    openMapsPage();
}

// maps_page [text]: the Map Library page, with the text typed in (as vr_menu_search is for the Search page).
void qvr::menu::mapsPage_f()
{
    mapPage.query = Cmd_Argc() >= 2 ? za::String{Cmd_Args()} : za::String{};
    openMapsPage();
}

// maps_page_stats (Debug > Tools).
void qvr::menu::mapsPageStats_f()
{
    mapsPageStatsPrint();
}

// vr_menu_search <text>: the Search page's results for the text, best first, with their scores and pages.
// vr_menu_slider_step <cvar> <steps> [fine]: the slider of `cvar` (the first on the VR menus' pages) stepped that many
// times (negative: down) as the menu's keys step it, with the fine modifier held if "fine"; each value printed as the
// cvar holds it and as the slider shows it (the plain steps' round values: the author's note
// vrfiringrange_2026-10-07_22-23-40).
void qvr::menu::sliderStep_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_menu_slider_step <cvar> <steps> [fine]\n");
        return;
    }
    const char* name = Cmd_Argv(1);
    const int steps = Q_atoi(Cmd_Argv(2));
    const bool fine = Cmd_Argc() > 3 && !q_strcasecmp(Cmd_Argv(3), "fine");
    const int was = levelOverride;
    levelOverride = LevelDeveloper;
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].build == pageSearch || pages[p].build == pageChanged)
        {
            continue;
        }
        for(const Item& found : items(p))
        {
            if(found.kind != Item::Slider || !found.cvar || q_strcasecmp(found.cvar->name, name))
            {
                continue;
            }
            const Item item = found; // (change may build the page again)
            fineForced = fine;
            for(int i = 0; i < za::abs(steps); i++)
            {
                change(item, steps > 0 ? 1 : -1);
                char text[64];
                sliderText(item, item.cvar->value, text, sizeof(text));
                Con_Printf("vr_menu_slider_step: %s \"%s\" shown %s\n", item.cvar->name, item.cvar->string, text);
            }
            fineForced = false;
            levelOverride = was;
            return;
        }
    }
    levelOverride = was;
    Con_Printf("vr_menu_slider_step: no slider of %s\n", name);
}

void qvr::menu::search_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_menu_search <text>: the VR menus' settings, actions and pages matching it, best first\n");
        return;
    }
    buildSearchIndex();
    search.query = Cmd_Args();
    runSearch();
    Con_Printf("vr_menu_search \"%s\": %d results (of %d rows and pages)\n", search.query.cStr(),
        static_cast<int>(search.results.size()), static_cast<int>(search.index.size()));
    for(int r = 0; r < static_cast<int>(search.results.size()) && r < 15; r++)
    {
        const SearchEntry& e = search.index[search.results[r].entry];
        Con_Printf("%2d %.2f %s%s | %s%s%s\n", r + 1, search.results[r].score, e.label.cStr(), e.isPage ? " (page)" : "",
            e.path.cStr(), search.results[r].byCvar ? va(": %s", e.lowCvar.cStr()) : "",
            e.level > menuLevel() ? va(" (%s)", levelName(e.level)) : "");
    }
}

float qvr::menu::contentLeft()
{
    if(m_state != m_vr)
    {
        return static_cast<float>(M_ContentLeft());
    }
    const auto build = pages[ui().page].build;
    if(build == pageSearch || build == pageMaps || build == pageConsole)
    {
        return 0.f; // (Search and the Map Library from x 12 at least; the console's page right of the buttons)
    }
    return static_cast<float>(q_min(pageRowsLeft(), (320 - 8 * helpColumns()) / 2));
}

float qvr::menu::contentRightBelow(float y)
{
    if(m_state != m_vr)
    {
        return M_ContentRightBelow(y);
    }
    const auto build = pages[ui().page].build;
    const Layout l = layout();
    if(build == pageConsole || build == pageSearch || build == pageMaps)
    {
        return y < static_cast<float>(l.bottom) ? pageRight(build) : -1e9f; // (their keyboards and buttons down to the bottom)
    }
    // The rows down to the help's box (or the bottom), as far right as their scrollbar's box (scrollbarX + 12); the help
    // centred at its widest (helpColumns), its paging bar right of it (drawHelp).
    const za::Vector<Item>& list = menuPages.built[ui().page];
    const bool help = hasHelp(list);
    float right = -1e9f;
    if(y < static_cast<float>(help ? l.helpTop - 4 : l.bottom - 8))
    {
        right = static_cast<float>(midPos + 200);
    }
    if(help && y < static_cast<float>(l.helpTop + 8 * helpBoxLines()))
    {
        right = za::fmax(right, static_cast<float>((320 + 8 * helpColumns()) / 2 + 5));
    }
    return right;
}

float qvr::menu::contentLeftBelow(float y)
{
    if(m_state != m_vr)
    {
        return M_ContentLeftBelow(y);
    }
    const auto build = pages[ui().page].build;
    const Layout l = layout();
    if(build == pageConsole || build == pageSearch || build == pageMaps)
    {
        return y < static_cast<float>(l.bottom) ? contentLeft() : 1e9f; // (down to the bottom)
    }
    // As contentRightBelow: the rows down to the help's box (or the bottom), the help under them.
    const za::Vector<Item>& list = menuPages.built[ui().page];
    const bool help = hasHelp(list);
    float left = 1e9f;
    if(y < static_cast<float>(help ? l.helpTop - 4 : l.bottom - 8))
    {
        left = static_cast<float>(pageRowsLeft());
    }
    if(help && y < static_cast<float>(l.helpTop + 8 * helpBoxLines()))
    {
        left = za::fmin(left, static_cast<float>((320 - 8 * helpColumns()) / 2 - 5));
    }
    return left;
}

bool qvr::menu::developerLevel()
{
    return menuLevel() >= LevelDeveloper;
}

void qvr::menu::jumpToChecklist()
{
    const int target = pageIndex(pageChecklist);
    checklist::refresh(true);
    if(m_state == m_vr && ui().page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(target);
}

void qvr::menu::jumpToSettings()
{
    if(m_state == m_vr && ui().page == PageMain)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(PageMain);
}

// (The VR pages' Back stack started again: the main menu is the top, its Back the game.)
void qvr::menu::jumpToMainMenu()
{
    if(m_state == m_main && key_dest == key_menu)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    navReset();
    M_Menu_Main_f();
}

void qvr::menu::jumpToRagtool()
{
    const int target = pageIndex(pageRagtool);
    if(m_state == m_vr && key_dest == key_menu && ui().page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(target);
}

void qvr::menu::jumpToRelighting()
{
    const int target = pageIndex(pageGraphicsRelighting);
    if(menuLevel() < pages[target].level)
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(pages[target].level)); // (as the corner's Advanced VR)
    }
    if(m_state == m_vr && ui().page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(target); // (Back: where it was pressed)
}

void qvr::menu::jumpToRecording()
{
    const int target = pageIndex(pageRecording);
    if(m_state == m_vr && ui().page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(target); // (Back: where it was pressed)
}

void qvr::menu::selectEnd(int dir)
{
    if(m_state == m_vr && pages[ui().page].build == pageSearch)
    {
        search.focusKey = 0; // from the corner's buttons: the keys
        return;
    }
    if(m_state == m_vr && pages[ui().page].build == pageConsole)
    {
        consolePage.focusKey = 0;
        return;
    }
    if(m_state == m_vr && pages[ui().page].build == pageMaps)
    {
        mapPage.focusKey = 0; // from the corner's buttons: the keys
        return;
    }
    if(m_state == m_vr)
    {
        const auto& list = items(ui().page);
        ui().cursors[ui().page] = dir > 0 ? firstSelectable(list) : lastSelectable(list);
    }
}

void qvr::menu::reopen(int target)
{
    if(target < PageMain || target >= pageCount)
    {
        target = PageMain;
    }
    enterVrMenu();
    if(navTop() != target)
    {
        navReset(); // (its way back gone: up the tree)
        navPush(target);
    }
    showPage(target); // its cursor, scroll and way back as they were
}

bool qvr::menu::scroll(int rows)
{
    if(m_state != m_vr || ui().sliderGrab || ui().scrollGrab)
    {
        return false;
    }
    if(pages[ui().page].build == pageConsole)
    {
        consoleScroll(-rows); // (the stick down: newer)
        return true;
    }
    if(pages[ui().page].build == pageMaps)
    {
        mapPage.resultScroll -= rows; // (the stick down: the next packages)
        mapsScrollTo(-1);
        return true;
    }
    if(dropDownItem())
    {
        // The stick moves the open list's highlight, the list scrolling with it.
        ui().dropDown.hover = CLAMP(0, ui().dropDown.hover + rows, ui().dropDown.count - 1);
        dropDownShowHover();
        return true;
    }
    const auto& list = items(ui().page);
    const int most = maxScroll(list);
    if(most <= 0)
    {
        return false;
    }
    ui().scrolls[ui().page] = CLAMP(0, ui().scrolls[ui().page] + rows, most);
    keepCursorVisible();
    return true;
}

namespace
{
void savePositions();
} // namespace

extern "C" void VR_MenuSavePositions()
{
    // The first panel's (the menu as ever: the others' places are this session's).
    const int was = currentMenu;
    menu::selectInstance(0);
    savePositions();
    menu::selectInstance(was);
}

namespace
{
void savePositions()
{
    loadPositions();
    za::String text;
    const auto add = [&text](int p, const RowAnchor& a) {
        if(!a.valid)
        {
            return;
        }
        const za::String record = za::String{pages[p].title} + '|' + a.section + '|' + a.label + '|' + za::toString(a.index) +
                                   '|' + za::toString(a.line);
        // A label the format cannot hold. (There was also a 1000-character cap, what a config line's token held then:
        // only the ~12 most recent pages kept their position. A token and a command line now take any length (cmd.c),
        // and each page adds one record: about 90 characters, 7 KB for every page.)
        if(record.findFirstOf(";\"") != za::StringView::nPos || za::count(record.begin(), record.end(), '|') != 4)
        {
            return;
        }
        text += text.empty() ? "" : ";";
        text += record;
    };

    // The pages shown since the start, the most recent first; then those kept from before.
    za::Vector<int> order;
    for(int p = 0; p < pageCount; p++)
    {
        if(ui().visits[p] > 0)
        {
            order.pushBack(p);
        }
    }
    za::stableSort(order.begin(), order.end(), [](int a, int b) { return ui().visits[a] > ui().visits[b]; }); // (stable: ties in the order std::sort gave a few)
    for(const int p : order)
    {
        add(p, ui().leftAnchors[p]);
    }
    order.clear();
    for(int p = 0; p < pageCount; p++)
    {
        if(ui().visits[p] == 0 && savedAnchors[p].valid)
        {
            order.pushBack(p);
        }
    }
    za::quickSort(order.begin(), order.end(), [](int a, int b) { return savedRank[a] < savedRank[b]; });
    for(const int p : order)
    {
        add(p, savedAnchors[p]);
    }

    if(text != vr_menu_positions.string)
    {
        Cvar_SetQuick(&vr_menu_positions, text.cStr());
    }
}
} // namespace


extern "C" void VR_Menu_Draw()
{
    syncPresets(); // (Comfort, Handedness: the choice the settings match now)
    if(pages[ui().page].build == pageSearch)
    {
        drawSearch();
        return;
    }
    if(pages[ui().page].build == pageConsole)
    {
        drawConsolePage();
        return;
    }
    if(pages[ui().page].build == pageMaps)
    {
        drawMapsPage();
        return;
    }
    const auto& list = items(ui().page);
    int& cursor = ui().cursors[ui().page];
    int& scroll = ui().scrolls[ui().page];

    if(!keydown[K_MOUSE1])
    {
        ui().sliderGrab = ui().scrollGrab = false;
    }

    const Layout l = layout();
    // (Below a flat screen's row of icons where that reaches over it: a window narrower than 16:9.)
    const bool underRow = menuui::toolbarRow() && menuui::toolbarRight() > 16.f;
    M_DrawPlaque(16, underRow ? q_max(l.top + 4, static_cast<int>(za::ceil(menuui::toolbarBottom())) + 2) : l.top + 4);
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, l.top + 4, title);
    const char* name = pages[ui().page].title;
    char refused[48];
    if(ui().refusedPage >= 0 && realtime - ui().refusedTime < 2.5)
    {
        // A page for one panel at a time refused here (showPage): said a moment in its title's place.
        q_snprintf(refused, sizeof(refused), "%.22s: in panel %d", pages[ui().refusedPage].title, ui().refusedBy + 1);
        name = refused;
    }
    M_PrintWhite((320 - 8 * static_cast<int>(strlen(name))) / 2, l.top + 28, name);

    const int n = static_cast<int>(list.size());
    if(cursor < scroll)
    {
        scroll = cursor > 0 && list[cursor - 1].kind == Item::Header ? cursor - 1 : cursor;
    }
    if(cursor < n && cursor >= scroll + rowsFrom(list, scroll))
    {
        scroll = scrollShowing(list, cursor);
    }
    scroll = CLAMP(0, scroll, maxScroll(list));
    const int rows = visibleRows(list);

    // Where the page is, to keep (on a change only: no strings built every frame).
    int& notedPage = ui().notedPlace.page;
    int& notedCursor = ui().notedPlace.cursor;
    int& notedScroll = ui().notedPlace.scroll;
    int& notedBuild = ui().notedPlace.build;
    if(ui().page != notedPage || cursor != notedCursor || scroll != notedScroll || builds[ui().page] != notedBuild)
    {
        notedPage = ui().page;
        notedCursor = cursor;
        notedScroll = scroll;
        notedBuild = builds[ui().page];
        noteLeft(ui().page, list);
    }

    // While the sticks' selection is on the corner's buttons, no row is selected (nor its help shown).
    const bool rowSelected = !menuui::toolbarFocused();
    for(int i = scroll; i < n && i < scroll + rows; i++)
    {
        drawItem(list[i], l.listTop + rowTop(list, scroll, i), rowSelected && i - list[i].partOf == cursor);
    }

    if(int y, height; scrollbar(list, y, height))
    {
        ui().scrollbarX = q_min(midPos + 188, static_cast<int>(glcanvas.right) - 16);
        M_DrawTextBox(ui().scrollbarX - 4, l.listTop + y - 4, 0, height - 1);
    }

    if(cursor < n && rowSelected)
    {
        const char* help = itemHelp(list[cursor]);
        if(help)
        {
            drawHelp(help);
        }
    }

    drawDropDown(); // over the rows and the help

    if(cursor < n && list[cursor].cvar)
    {
        ui().drawnValue = valueOf(list[cursor]);
        ui().drawnFrame = host_framecount;
    }
}

extern "C" void VR_Menu_Key(int key, int repeat)
{
    if(pages[ui().page].build == pageSearch)
    {
        searchKey(key);
        return;
    }
    if(pages[ui().page].build == pageConsole)
    {
        consoleKey(key);
        return;
    }
    if(pages[ui().page].build == pageMaps)
    {
        mapsKey(key);
        return;
    }
    const auto& list = items(ui().page);
    const int cursor = ui().cursors[ui().page];

    if(ui().sliderGrab || ui().scrollGrab)
    {
        if(!keydown[K_MOUSE1] || key == K_ESCAPE || key == K_BBUTTON || key == K_MOUSE2)
        {
            ui().sliderGrab = ui().scrollGrab = false;
        }
        return;
    }

    if(dropDownKey(key))
    {
        return;
    }

    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4:
            goBack();
            break;

        // Up from the first setting, or down from the last: the corner's buttons (where shown), before
        // round to the other end; a held stick stops at the end first, a new push goes on.
        case K_UPARROW:
        case K_MWHEELUP:
            if(key == K_UPARROW && menuui::toolbarShown() && cursor == firstSelectable(list))
            {
                if(!repeat)
                {
                    menuui::focusToolbar(-1);
                }
                break;
            }
            S_LocalSound("misc/menu1.wav");
            moveCursor(list, -1);
            break;

        case K_DOWNARROW:
        case K_MWHEELDOWN:
            if(key == K_DOWNARROW && menuui::toolbarShown() && cursor == lastSelectable(list))
            {
                if(!repeat)
                {
                    menuui::focusToolbar(1);
                }
                break;
            }
            S_LocalSound("misc/menu1.wav");
            moveCursor(list, 1);
            break;

        case K_LEFTARROW:
        case K_RIGHTARROW:
            if(list[cursor].buttonPress)
            {
                // A button row: left and right pick its button.
                const int b = CLAMP(0, ui().button + (key == K_RIGHTARROW ? 1 : -1), list[cursor].buttons - 1);
                if(b != ui().button)
                {
                    ui().button = b;
                    S_LocalSound("misc/menu1.wav");
                }
                break;
            }
            change(list[cursor], key == K_RIGHTARROW ? 1 : -1, repeat);
            break;

        case K_MOUSE1:
            // On the scrollbar: it is dragged.
            if(int y, height; m_mousex >= ui().scrollbarX - 8 && scrollbar(list, y, height))
            {
                ui().scrollGrab = true;
                scrollTo(m_mousey);
                break;
            }
            // On the selected row (the one under the mouse): a slider is set there and dragged.
            if(rowAt(m_mousey) != cursor)
            {
                break;
            }
            if(list[cursor].kind == Item::Slider)
            {
                if(m_mousex >= midPos - 12 && m_mousex <= midPos + 84)
                {
                    // A value past an end, its thumb at that end: a click on the thumb keeps it (a drag
                    // sets it, along the bar).
                    const Item& item = list[cursor];
                    const int past = item.negativeLabel && item.cvar->value < 0.f ? 0 : pastEnd(item, item.cvar->value);
                    const float thumb = midPos + 4.f + (past > 0 ? 72.f : 0.f);
                    ui().sliderGrab = true;
                    ui().sliderGrabHold = -1.f;
                    if(past && za::fabs(m_mousex - thumb) <= 6.f)
                    {
                        ui().sliderGrabHold = m_mousex;
                    }
                    else
                    {
                        setSliderAt(item, m_mousex);
                    }
                    S_LocalSound("misc/menu3.wav");
                }
                break;
            }
            if(opensDropDown(list[cursor]))
            {
                openDropDown(list[cursor], cursor);
                break;
            }
            if(list[cursor].buttonPress)
            {
                // An item with a button row: a click on one of its buttons presses it; on its text, nothing.
                const int raw = rowAtRaw(m_mousey);
                if(raw < 0 || list[raw].buttons <= 0 || list[raw].partOf <= 0)
                {
                    break;
                }
                ui().button = buttonAt(list[raw], m_mousex);
            }
            change(list[cursor], 1);
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            if(opensDropDown(list[cursor]))
            {
                openDropDown(list[cursor], cursor);
            }
            else if(list[cursor].kind != Item::Slider)
            {
                change(list[cursor], 1);
            }
            break;

        default: break;
    }
}

// The mouse (or the laser pointer) over the list selects the row under it, and drags a grabbed
// slider.
extern "C" void VR_Menu_Char(int key)
{
    if(pages[ui().page].build == pageSearch && key >= 32 && key < 127)
    {
        typeInSearch(static_cast<char>(key));
    }
    if(pages[ui().page].build == pageConsole && key >= 32 && key < 127)
    {
        typeInConsole(static_cast<char>(key));
    }
    if(pages[ui().page].build == pageMaps && key >= 32 && key < 127)
    {
        mapsTypeIn(static_cast<char>(key));
    }
}

extern "C" int VR_Menu_TextEntry()
{
    return pages[ui().page].build == pageSearch || pages[ui().page].build == pageConsole || pages[ui().page].build == pageMaps
               ? TEXTMODE_NOPOPUP
               : TEXTMODE_OFF; // (the page's own keyboard)
}

extern "C" void VR_Menu_Mousemove(float cx, float cy)
{
    if(pages[ui().page].build == pageSearch)
    {
        searchMouse(cx, cy);
        return;
    }
    if(pages[ui().page].build == pageConsole)
    {
        consoleMouse(cx, cy);
        return;
    }
    if(pages[ui().page].build == pageMaps)
    {
        mapsMouse(cx, cy);
        return;
    }
    if(dropDownMousemove(cx, cy))
    {
        return;
    }

    const auto& list = items(ui().page);
    int& cursor = ui().cursors[ui().page];

    if(ui().sliderGrab || ui().scrollGrab)
    {
        if(!keydown[K_MOUSE1])
        {
            ui().sliderGrab = ui().scrollGrab = false;
            return;
        }
        if(ui().scrollGrab)
        {
            scrollTo(cy);
        }
        else if(ui().sliderGrabHold < 0.f || za::fabs(cx - ui().sliderGrabHold) >= 4.f)
        {
            ui().sliderGrabHold = -1.f;
            setSliderAt(list[cursor], cx);
        }
        return;
    }

    // Over a button row: the button under it picked.
    if(const int raw = rowAtRaw(cy); raw >= 0 && list[raw].buttons > 0 && list[raw].partOf > 0 && buttonAt(list[raw], cx) != ui().button)
    {
        ui().button = buttonAt(list[raw], cx);
        if(ui_mouse_sound.value)
        {
            S_LocalSound("misc/menu1.wav");
        }
    }

    const int i = rowAt(cy);
    if(i < 0 || !selectable(list[i]) || i == cursor)
    {
        return;
    }
    cursor = i;
    if(ui_mouse_sound.value)
    {
        S_LocalSound("misc/menu1.wav");
    }
}
