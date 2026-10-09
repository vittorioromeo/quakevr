#include "vr_cheats.hpp"
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

#include "vr_serverrules.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_backend.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_mapindex.hpp"
#include "vr_mapinstall.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_obs.hpp"
#include "vr_menupaint.hpp"
#include "vr_motion.hpp"
#include "vr_motion_review.hpp"
#include "vr_motion_take.hpp"
#include "vr_weapons.hpp"
#include "vr_hands.hpp"
#include "vr_posing.hpp"
#include "vr_sightalign.hpp"
#include "vr_bodycal.hpp"
#include "vr_checklist.hpp"
#include "vr_view.hpp"
#include "vr_units.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_props.hpp"
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
const char* M_Main_RowLabel(void); // menu.c: the main menu's selected row (menu_vr pos)
extern int m_singleplayer_cursor; // menu.c: Single Player's (menu_vr pos)
int M_ContentLeft(void); // menu.c: the left edge of what its menu shown draws (menu x)
float M_ContentRightBelow(float y); // menu.c: how far right its menu shown draws below y (the main menu's rows as drawn)
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
[[nodiscard]] za::Vector<Item> pageWeightDamage();
[[nodiscard]] za::Vector<Item> pageAimingSettings();
[[nodiscard]] za::Vector<Item> pageWeaponDamage();
[[nodiscard]] za::Vector<Item> pageDamage();
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
[[nodiscard]] za::Vector<Item> pageDebugCheats();
[[nodiscard]] za::Vector<Item> pageStealth();
[[nodiscard]] za::Vector<Item> pageStealthTests();
[[nodiscard]] za::Vector<Item> pageSpawnWeapons();
[[nodiscard]] za::Vector<Item> pageHitbox();
[[nodiscard]] za::Vector<Item> pageMonsterHitbox();
[[nodiscard]] za::Vector<Item> pageChanged();
[[nodiscard]] za::Vector<Item> pageSearch();
[[nodiscard]] za::Vector<Item> pageConsole();
[[nodiscard]] za::Vector<Item> pageMaps();
// (Settings with their home on another page link to it: one home per setting.)
[[nodiscard]] za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageColours();
[[nodiscard]] za::Vector<Item> pageBulletTime();
[[nodiscard]] za::Vector<Item> pageStamina();
[[nodiscard]] za::Vector<Item> pageGibs();
[[nodiscard]] za::Vector<Item> pageGore();
[[nodiscard]] za::Vector<Item> pageLimbGore();
[[nodiscard]] za::Vector<Item> pageScreens();
[[nodiscard]] za::Vector<Item> pageTips();
[[nodiscard]] za::Vector<Item> pageHipHolsters();
[[nodiscard]] za::Vector<Item> pageClimbing();
[[nodiscard]] za::Vector<Item> pageRecording();

// Texts the menu hands out by pointer (an Item holds const char*):
// - readouts: an info line's or a help's text, valid until the same function's next call (the draw uses it at once);
//   released at a map change like any scratch.
struct MenuReadouts
{
    za::String motionNote, motionLastSaved, extendableHelp, serverRuleHelp, fineHelp;
    za::String checklistUndoHelp;      // checklistUndoHelp
    za::String weight[2];              // weightReadout, by hand
    za::String weaponWeightsDamage[2]; // weaponWeightsDamageReadout, by line
    za::String heldObjectMass;
    za::String heldObjectDamage[2];    // by line
    za::String weaponWeightsDrop;      // weaponWeightsDropReadout
    za::String weaponWeightsHits[2];   // weaponWeightsHitsReadout, by line
    za::String weaponOffsetsStock;     // weaponOffsetsStockReadout
    char checklistSummary[48];         // checklistSummary
    char stamina[96];                  // staminaReadout
    char renderScaleHelp[192];         // renderScaleHelp
    char buildVersion[64];             // buildVersionLine
    auto members()
    {
        return qvr::mem::list(motionNote, motionLastSaved, extendableHelp, serverRuleHelp, fineHelp, weight, weaponWeightsDamage, heldObjectMass, heldObjectDamage,
            weaponWeightsDrop, weaponWeightsHits, weaponOffsetsStock, checklistSummary, checklistUndoHelp, stamina, renderScaleHelp, buildVersion);
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

#include "vr_menu_props.inc"
#include "vr_menu_pages.inc"
#include "vr_menu_recording.inc"
#include "vr_menu_cheats.inc"
#include "vr_menu_stealth.inc"

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
// The tutorial (vrtutorial2: Misc/quakevr/maps/vrtutorial2_gen.py) on Easy; the old one (vrtutorial) still loads by name.
void playTutorial() { Cbuf_AddText("skill 0; map vrtutorial2\n"); }
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

[[nodiscard]] za::Vector<Item> pageCampaigns()
{
    za::Vector<Item> items{header("Official Campaigns")};
    for(int i = 0; i < 6; ++i)
    {
        Item choice = row(VR_CampaignLabel, VR_CampaignHelp, VR_SelectCampaign, i, -1);
        choice.dimArg = [](int index) -> bool { return VR_CampaignUnavailable(index) != 0; };
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
[[nodiscard]] za::Vector<Item> pageKnockdowns();
[[nodiscard]] za::Vector<Item> pageHoldingEnemies();
[[nodiscard]] za::Vector<Item> pageSound();

[[nodiscard]] za::Vector<Item> pageGameplay()
{
    return {
        header("Monsters"),
        toggle("Enemies Hurt by Liquids", vr_enemy_liquid_damage).help("Monsters burn in slime/lava and drown after 12 seconds with their heads underwater, including knocked-down ragdolls. Fish, bosses and lava dwellers are immune."),
        slider("Liquid Breaks Falls", vr_liquid_fall_cushion, 0.f, 128.f, 8.f, "%.0f units").extend(0.f, 512.f)
            .help("A monster landing in water, slime or lava under this much of it (standing or knocked down) takes no fall "
                  "damage; shallower takes some. 0: liquid breaks no fall (vr_liquid_fall_cushion)."),
        toggle("Smooth Monster Steps", vr_monster_lerp_continue)
            .help("A monster's next step is drawn on from where it is drawn, not from where its last step ends: no jump "
                  "when it moves again before the last step is drawn out (a dog's quick turns looked like teleports). "
                  "Off: Quake's drawing."),
        toggle("Ogres Aim Grenades Up and Down", vr_ogre_aim_height)
            .help("Ogres (and zombies throwing flesh) lob at your height, on a ledge above them or a floor below, on an arc at "
                  "their throw's own speed. Off: Quake's lob, which always flies as if you stood level with them."),
        header("Enemy Weapons"),
        open("Enemy Weapons", pageIndex(pageEnemyWeapons))
            .help("What monsters drop as they die, to pick up and use: knights' swords, ogres' chainsaws, grunts' shotguns, "
                  "enforcers' laser rifles. Their damage and ammo."),
        header("Weapon Drops"),
        cycle("Enemy Weapon Drops", "vr_enemy_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random enemy weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Enemy Drops Chance", "vr_enemy_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for enemy weapon drops."),
        cycle("Ammo Box Weapon Drops", "vr_ammobox_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Ammo Box Drops Chance", "vr_ammobox_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for ammo box weapon drops."),
        header("Feel"),
        toggle("Explosion Rumble", vr_explosion_rumble).help("Explosions near you rumble in your hands."),
        toggle("Low Health Heartbeat", vr_heartbeat).help("A heartbeat in your hands when your health is low."),
    };
}

// Grunts and enforcers shove you away when you stand too close (QC vr_enemyshove.qc; ROUND21.md, "Grunts and enforcers
// shove you").
[[nodiscard]] za::Vector<Item> pageEnemyShoves()
{
    return {
        toggle("Enemy Shoves", vr_enemy_shove)
            .help("Grunts and enforcers shove you away, pushing their gun out with both hands, when you stand too close "
                  "for a moment. Never while they fire, are staggered or knocked away."),
        slider("Too Close", vr_enemy_shove_range, 36.f, 96.f, 2.f, "%.0f units").extend(32.f, 160.f)
            .help("How close you must be (between your middles, flat: touching is 32 units) for them to shove you."),
        slider("Delay", vr_enemy_shove_delay, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("How long you must stay that close before they shove (the shove's wind-up then takes 0.3 s)."),
        slider("Cooldown", vr_enemy_shove_cooldown, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 30.f)
            .help("From one shove to the same enemy's next, parried or not."),
        slider("Damage", vr_enemy_shove_damage, 0.f, 20.f, 1.f, "%.0f").extend(0.f, 50.f)
            .help("A shove's damage, cut by a parry as any blow's (Parry and Bash). 0: none, and it can't be parried."),
        slider("Push Distance", vr_enemy_shove_distance, 0.f, 160.f, 4.f, "%.0f units").extend(0.f, 400.f)
            .help("How far a shove pushes you (a quick slide; 32 units is about a metre). Not scaled by Knockback."),
        slider("Parry Push Reduction", vr_enemy_shove_parry_reduction, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of a shove's push a parry takes off (as Parry Damage Reduction takes off its damage): 0.75, a "
                  "parried shove pushes you a quarter as far. 1: not at all."),
        slider("Enforcer Damage", vr_enemy_shove_enforcer_damage, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("An enforcer's shove's damage, times Damage."),
        slider("Enforcer Push", vr_enemy_shove_enforcer_distance, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How far an enforcer's shove pushes you, times Push Distance."),
    };
}

// Holding enemies (vr_foegrab.cpp, QC vr_foegrab.qc; ROUND21.md, "Holding enemies"): an empty hand holds on to a living
// monster, slows it and drags it a little.
[[nodiscard]] za::Vector<Item> pageHoldingEnemies()
{
    return {
        toggle("Holding Enemies", vr_foegrab)
            .help("Experimental. Grip with an empty hand touching a living enemy to hold on to that spot of it: the hand "
                  "stays on it as it moves, and the enemy is slowed and can be pulled a little. Small ones are held well, "
                  "big ones (fiends, ogres) a little, huge ones (shamblers, vores) hardly. Let go of the grip, or pull too "
                  "far, and the hand lets go. Grabbing a sleeping or idle enemy wakes it. Needs Precise Hit Detection."),
        slider("Touch Leniency", vr_foegrab_leniency, 0.f, 5.f, 0.25f, "%.2f cm").extend(0.f, 20.f)
            .help("How far off the enemy's model your fist may be and still take hold."),
        slider("Let Go Beyond", vr_foegrab_break, 10.f, 80.f, 1.f, "%.0f cm").extend(5.f, 200.f)
            .help("Your hand this far from the spot it holds (the enemy ran, or you moved away) lets go: after a quarter "
                  "of a second there, or at once at twice this."),
        header("Hold"),
        cycle("Hold By", "vr_foegrab_by_mass", {{0.f, "Size Class"}, {1.f, "Mass"}})
            .help("Size Class: the three holds below, by kind of enemy. Mass: from the enemy's mass, full at Light and "
                  "none at Heavy."),
        slider("Small Enemies", vr_foegrab_strength_small, 0.f, 1.f, 0.05f, "%.2f")
            .help("One hand's hold on a grunt, an enforcer, a zombie, a knight, a mummy, a dog. Two hands together: "
                  "1 - (1 - a)(1 - b) (0.85 each: 0.98)."),
        slider("Big Enemies", vr_foegrab_strength_medium, 0.f, 1.f, 0.05f, "%.2f")
            .help("One hand's hold on a fiend, an ogre, a death knight's bigger kin."),
        slider("Huge Enemies", vr_foegrab_strength_large, 0.f, 1.f, 0.01f, "%.2f")
            .help("One hand's hold on a shambler, a vore and anything bigger."),
        slider("Light (Mass)", vr_foegrab_mass_light, 10.f, 400.f, 10.f, "%.0f kg").extend(1.f, 5000.f)
            .help("Hold By Mass: enemies this light or lighter are held fully (a grunt 80 kg, an enforcer 100)."),
        slider("Heavy (Mass)", vr_foegrab_mass_heavy, 100.f, 2000.f, 25.f, "%.0f kg").extend(10.f, 20000.f)
            .help("Hold By Mass: enemies this heavy or heavier are not held at all (a fiend or an ogre 250, a shambler 600)."),
        header("Effect"),
        slider("Slow Down", vr_foegrab_slow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of a fully held enemy's own movement (and turning) taken off, times its hold."),
        slider("Follow the Hand", vr_foegrab_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of your holding hand's own movement the enemy follows, times its hold: pull your hand back, or "
                  "walk off holding it, and a grunt comes along, an ogre a little, a shambler hardly."),
        slider("Pull", vr_foegrab_drag, 0.f, 20.f, 0.5f, "%.1f /s")
            .help("How quickly your hand pulls the spot it holds towards it, times the hold (0: not at all): takes up "
                  "what Follow the Hand leaves."),
        slider("Pull Speed", vr_foegrab_drag_speed, 0.f, 300.f, 5.f, "%.0f units/s")
            .help("The pull's top speed (32 units are about a metre)."),
        header("Their Shoves"),
        cycle("Held Enemy's Shove", "vr_foegrab_shove", {{0.f, "Resisted"}, {1.f, "Breaks Free"}})
            .help("Resisted: a grunt or enforcer you hold barely pushes you (by Shove Resistance and its hold), and comes "
                  "along with you. Breaks Free: its shove pushes you fully, as ever, and tears it from your hands."),
        slider("Shove Resistance", vr_foegrab_shove_resist, 0.f, 1.f, 0.05f, "%.2f")
            .help("Resisted: share of a held enemy's shove taken off, times its hold."),
        header("Two-Hand Throw"),
        toggle("Two-Hand Throw", vr_foegrab_throw)
            .help("Hold one enemy with both hands and turn it over hard, a judo throw: one shoulder down and the other up, "
                  "or its top pulled towards you or to a side. It is knocked down that way (it falls, lies, gets up), for "
                  "sure: grunts, enforcers, zombies, knights, rottweilers, mummies and the infected always; death knights, "
                  "ogres, fiends, spawns, scorpions and ranged knights only when hurt; never shamblers, vores, bosses, "
                  "flyers or swimmers. One hand never throws. The training dummy shows how it would go."),
        slider("Turn to Throw", vr_foegrab_throw_twist, 60.f, 600.f, 10.f, "%.0f deg/s").extend(10.f, 2000.f)
            .help("How fast your hands must turn the enemy over (about a level axis) to throw it. Log Holds (Debug > "
                  "Tests > Holding Enemies) prints your hardest turn of each hold."),
        slider("Hands' Speed", vr_foegrab_throw_speed, 0.2f, 3.f, 0.1f, "%.1f m/s").extend(0.f, 10.f)
            .help("And how fast your hands must move then (their mean): a slow turn never throws."),
        slider("Hurt Below", vr_foegrab_throw_hurt, 0.05f, 1.f, 0.05f, "%.2f")
            .help("The bigger enemies (death knights, ogres, fiends...) are thrown only below this share of their full "
                  "health, and never above it."),
        toggle("Throw Away From You", vr_foegrab_throw_away)
            .help("Off: a turn that would throw the enemy straight away from you does nothing (that's a shove's job). On: "
                  "it throws too."),
        cycle("Who Can Be Thrown", "vr_foegrab_throw_by_mass", {{0.f, "By Kind"}, {1.f, "By Mass"}})
            .help("By Kind: the lists in the console's vr_foegrab_throw_always and vr_foegrab_throw_when_hurt (classnames; "
                  "\"infected\": Dawn of the Machine's infected); any other kind never. By Mass: the two masses below."),
        slider("Always (Mass)", vr_foegrab_throw_mass_always, 10.f, 1000.f, 10.f, "%.0f kg").extend(0.f, 10000.f)
            .help("By Mass: enemies this light or lighter are always thrown (a grunt 80 kg, a mummy 140)."),
        slider("When Hurt (Mass)", vr_foegrab_throw_mass_hurt, 10.f, 2000.f, 10.f, "%.0f kg").extend(0.f, 20000.f)
            .help("By Mass: enemies this light or lighter, and heavier than Always, only when hurt; heavier never (an "
                  "ogre or a fiend 250 kg, a shambler 600). Vores and overlords never."),
        slider("Throw Push", vr_foegrab_throw_push, 0.f, 600.f, 10.f, "%.0f units/s")
            .help("How hard the thrown enemy is sent along the throw (times Knockdowns' Push)."),
        slider("Throw Lift", vr_foegrab_throw_lift, 0.f, 400.f, 10.f, "%.0f units/s")
            .help("And up."),
        slider("Topple", vr_foegrab_throw_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("The thrown enemy loses its footing: it is turned over about its feet towards the throw this fast, a "
                  "sweep, and the push and lift go to its top, not its feet. Off: pushed whole, as a shove's knockdown."),
        slider("Feet Speed", vr_foegrab_throw_feet_speed, 0.f, 300.f, 10.f, "%.0f units/s").extend(0.f, 1000.f)
            .help("Its feet are swept the other way as it topples (thrown left, they go right), so it spins in place in "
                  "the air rather than falling over its feet: this fast. 0: its feet stay where they stood a moment."),
        slider("Twist Spin", vr_foegrab_throw_spin, 0.f, 2.f, 0.05f, "%.2f")
            .help("Share of your hands' twist of it about the vertical it spins on with as it falls (at most 720 deg/s)."),
        slider("Throw Grunt", vr_foegrab_throw_grunt, 0.f, 1.f, 0.1f, "%.1f")
            .help("Your grunt as you throw an enemy down, this loud (0 off)."),
        cycle("Throw Grunt Sound", vr_foegrab_throw_grunt_sound, gruntChoices())
            .help("Which grunt the throw makes: the same choices as climbing's Mantle Grunt Sound. The deeper ones are "
                  "Quake's played slower. Hear it with Hear Throw Grunt."),
        command("Hear Throw Grunt", "vr_foegrab_throw_grunt_test").help("Plays the throw's grunt as set, at full volume."),
    };
}

// Knockdowns (QC vr_knockdown.qc; vr_box3d.cpp, "Knockdowns"; docs/vr-port/KNOCKDOWNS_2026-10-04.md): a shove can
// knock a monster down as a ragdoll, alive; it gets up after a while.
[[nodiscard]] za::Vector<Item> pageKnockdowns()
{
    return {
        toggle("Knockdowns", vr_knockdown)
            .help("A shove can knock a monster down as a ragdoll, alive: hit it, grab it, drag it, throw it. After a few "
                  "seconds it gets up where it lies and fights on. Only monsters with ragdolls (Ragdolls on)."),
        header("Chance"),
        slider("Chance", vr_knockdown_chance, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 100.f)
            .help("Every monster's chance, times this. 0: never; 100: every shove."),
        slider("Damage Taken", vr_knockdown_damage, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("A hurt monster goes down more easily: the chance times 1 + this times the share of its health it has "
                  "lost (2: three times as likely at no health left)."),
        slider("One-Handed Shove", vr_knockdown_onehand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The chance of a shove with one hand, times this."),
        slider("Two-Handed Shove", vr_knockdown_twohand, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The chance of a shove with both hands, times this."),
        slider("Weapon Bash", vr_knockdown_bash, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A bash with a held weapon knocks down too, at a shove's chance times this. 0: only open-hand shoves."),
        slider("Stamina", vr_knockdown_stamina, 0.f, 1.f, 0.05f, "%.2f")
            .help("Tired shoves knock down less: the chance times 1 - this times the share of your stamina spent (with "
                  "Stamina on). 0: stamina doesn't matter."),
        toggle("Over a Ledge, Always", vr_knockdown_ledge)
            .help("A shove that would carry it over the edge of a high drop (Ledge Height or more) always knocks it down, "
                  "so it tumbles off as a ragdoll. Not through a wall or railing; stairs are no ledge. Off: the chance as "
                  "anywhere."),
        slider("Ledge Height", vr_knockdown_ledge_drop, 24.f, 256.f, 8.f, "%.0f units").extend(1.f, 1024.f)
            .help("How high a drop must be to count as a ledge (64: a bit more than you are tall; a stair is 16-18)."),
        slider("Ledge Reach", vr_knockdown_ledge_reach, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How far ahead a ledge is looked for, times how far the shove would carry it (the hop and the slide). "
                  "1: as far as it goes; less: only ledges close by force the knockdown; more: further ones too."),
        slider("Ledge Margin", vr_knockdown_ledge_margin, 0.f, 128.f, 4.f, "%.0f units").extend(0.f, 512.f)
            .help("How far past that a ledge still counts, in units (16: about a monster's half width)."),
        header("Each Monster's Chance"),
        slider("Grunt", vr_knockdown_chance_army, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Enforcer", vr_knockdown_chance_enforcer, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Rottweiler", vr_knockdown_chance_dog, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Zombie", vr_knockdown_chance_zombie, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Mummy", vr_knockdown_chance_mummy, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Knight", vr_knockdown_chance_knight, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Death Knight", vr_knockdown_chance_hknight, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Ogre", vr_knockdown_chance_ogre, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Fiend", vr_knockdown_chance_demon, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Shambler", vr_knockdown_chance_shambler, 0.f, 1.f, 0.01f, "%.2f"),
        slider("Gremlin", vr_knockdown_chance_gremlin, 0.f, 1.f, 0.01f, "%.2f"),
        header("Down"),
        slider("Time Down, Least", vr_knockdown_time_min, 0.f, 10.f, 0.25f, "%.2f s").extend(0.f, 60.f)
            .help("How long it stays down, at least (at random up to Time Down, Most)."),
        slider("Time Down, Most", vr_knockdown_time_max, 0.f, 10.f, 0.25f, "%.2f s").extend(0.f, 60.f),
        slider("Hit Keeps It Down", vr_knockdown_hit_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("Each hit while it lies there keeps it down this much longer (never past Time Down, Most from the hit)."),
        slider("Launch", vr_knockdown_push, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("How hard the shove throws the body, times the shove's own push. 0: it drops where it stood."),
        slider("Topple", vr_knockdown_shove_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("A shoved-down enemy loses its footing as a thrown one does (Holding Enemies' Topple): turned over about "
                  "its feet along the shove this fast, head and chest first, the shove's push going to its top. A "
                  "one-handed shove turns it 0.7 as fast. Shoved over a ledge: Topple Over a Ledge. Off: "
                  "pushed whole, as before."),
        slider("Topple Push", vr_knockdown_shove_topple_push, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of the shove's push the toppling body keeps, its head and chest the most, its feet none. "
                  "More: it goes over faster and lands further (1: as hard as a judo throw's); less: it tips over more "
                  "gently."),
        slider("Topple Feet Speed", vr_knockdown_shove_feet_speed, 0.f, 300.f, 10.f, "%.0f units/s").extend(0.f, 1000.f)
            .help("Its feet swept back against the shove as it topples, as a thrown enemy's (Feet Speed there). 0: they "
                  "stay where they stood for Topple Feet Held."),
        slider("Topple Feet Held", vr_knockdown_shove_feet_hold, 0.f, 1.f, 0.05f, "%.2f s")
            .help("How long its feet stay where they stood as it topples (Topple Feet Speed 0); then they go as they "
                  "would."),
        slider("Topple Over a Ledge", vr_knockdown_shove_ledge_topple, 0.f, 600.f, 10.f, "%.0f deg/s").extend(0.f, 2000.f)
            .help("Shoved over a ledge, it is pushed whole so it goes over the edge, and turns over its feet as it falls "
                  "this fast (a one-handed shove 0.7 as fast). Off: it goes over upright, as before."),
        toggle("Weapon Stays in Hand", vr_knockdown_weld)
            .help("Its weapon stays in its hand while it is down (killed there, it drops it as usual). Off: it flops "
                  "loose as a dead one's."),
        header("Getting Up"),
        slider("Struggling Strength", vr_knockdown_wiggle, 0.f, 3.f, 0.1f, "%.1fx")
            .help("Living knockdowns gently curl their chest and nod their head about their resting pose. Arms and legs follow physically. 0: still. Dead bodies never struggle."),
        slider("Struggling Frequency", vr_knockdown_wiggle_frequency, 0.1f, 5.f, 0.1f, "%.1f Hz"),
        slider("Struggling Pause", vr_knockdown_wiggle_pause, 0.f, 5.f, 0.25f, "%.2f s")
            .help("Pause between bursts of movement. 0: continuous."),
        slider("Room Search", vr_knockdown_search, 0.f, 128.f, 4.f, "%.0f units").extend(0.f, 512.f)
            .help("How far from the body it looks for room to stand, never through a wall, floor or ceiling. No room: it "
                  "stays down (dragged into a tight corner or under something low) and tries again."),
        slider("Retry", vr_knockdown_retry, 0.1f, 3.f, 0.1f, "%.1f s").extend(0.1f, 10.f)
            .help("How often it tries again to get up when there is no room."),
        slider("Blend", vr_knockdown_blend, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("How long the body takes to blend from how it lies into its get-up animation."),
        slider("Get-Up Speed", vr_knockdown_getup_speed, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.1f, 10.f)
            .help("How fast its get-up animation plays."),
        header("Debug"),
        cycle("Print Rolls", vr_knockdown_debug, {{0.f, "Off"}, {1.f, "On"}, {2.f, "And Get-Ups' Motion"}, {3.f, "And Each Frame's"}})
            .help("Prints each shove's chance and roll, and each get-up (developer 1). And Get-Ups' Motion: a line per get-up "
                  "of how it is drawn (how fast it moves, its biggest jump, the frames that go back on the one before: a "
                  "jitter; the switch from its ragdoll to its animation). And Each Frame's: a line a frame as well. Knock "
                  "one down with vr_knockdown_test 0 and get it up with vr_knockdown_test 1."),
    };
}

// A running chainsaw's engine (NOTES.md vrfiringrange_2026-10-01_16-37-54): its exhaust's smoke, its shake in the hand
// and lying on the ground (vr_chainsaw.cpp, QC vr_chainsaw.qc VR_Saw_PropEngine).
[[nodiscard]] za::Vector<Item> pageChainsawEngine()
{
    return {
        header("Exhaust Smoke"),
        slider("Smoke", vr_chainsaw_smoke, 0.f, 30.f, 1.f, "%.0f puffs a second").extend(0.f, 100.f)
            .help("Faint puffs of smoke from the running engine's exhaust, in the hand and lying running. 0: none."),
        slider("Smoke Opacity", vr_chainsaw_smoke_alpha, 0.f, 1.f, 0.02f, "%.2f")
            .help("How opaque each puff is as it leaves the exhaust; it fades out over about 2 s as it spreads and rises."),
        header("Shake"),
        slider("In One Hand", vr_chainsaw_shake, 0.f, 6.f, 0.25f, "%.2f mm").extend(0.f, 20.f)
            .help("How much the running chainsaw (and the hand on it) shakes held in one hand: this far, and turning 0.2 "
                  "degrees a mm; 1.6x with the chain running. Drawn only: the aim and the cuts don't shake. 0: still."),
        slider("In Two Hands", vr_chainsaw_shake_2h, 0.f, 6.f, 0.25f, "%.2f mm").extend(0.f, 20.f)
            .help("The same, steadied by the other hand on its front handle."),
        slider("On the Ground", vr_chainsaw_shake_ground, 0.f, 1.f, 0.05f, "%.2f m/s").extend(0.f, 3.f)
            .help("Let go running (Runs On When Let Go), as it lies it is kicked 20 times a second, mostly up, at a point "
                  "of its engine at random: it rocks and walks about a little, physically. How hard each kick is. 0: "
                  "still."),
        header("Pulling the Cord"),
        slider("Pull Shake", vr_chainsaw_pull_shake, 0.f, 8.f, 0.25f, "%.2f mm").extend(0.f, 20.f)
            .help("Each pull of the cord of a chainsaw not running, with fuel, the engine turning over: the chainsaw "
                  "shakes this much, dying out over Pull Shake Time. A weak pull (too slow), half. With an empty tank, "
                  "no shake, smoke or sparks: only the sound. 0: still."),
        slider("Pull Shake Time", vr_chainsaw_pull_shake_time, 0.05f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("How long a pull's shake lasts."),
        slider("Pull Smoke", vr_chainsaw_pull_smoke, 0.f, 12.f, 1.f, "%.0f puffs").extend(0.f, 40.f)
            .help("Puffs of exhaust smoke each pull makes (as Smoke's, at Smoke Opacity). A weak pull, half. 0: none."),
        slider("Pull Sparks", vr_chainsaw_pull_sparks, 0.f, 40.f, 1.f, "%.0f").extend(0.f, 60.f)
            .help("Tiny sparks out of the exhaust at each pull. A weak pull, half. 0: none."),
        toggle("Model Handle in the Hand", vr_chainsaw_model_handle)
            .help("The cord's handle in your fist is the chainsaw model's own, the one seated on it, easing from its seat "
                  "into the fist's turn as you take it. Off: a plain round handle."),
    };
}

// The weapons monsters drop as they die (ROUND21.md, "Enemy weapons: the grunts' shotguns and the enforcers' laser
// rifles"): their base damage and ammo. Their offsets and weights are in the weapon menus (slots _19 .. _23).
[[nodiscard]] za::Vector<Item> pageEnemyWeapons()
{
    return {
        header("Their Damage"),
        open("Weapon Damage", pageIndex(pageWeaponDamage))
            .help("The swords', chainsaws', burst rifles' and laser rifles' damage, with every other weapon's."),
        header("Lying About"),
        slider("Most Lying About", vr_enemy_weapon_drop_max, 0.f, 128.f, 4.f, "%.0f").extend(0.f, 512.f)
            .help("The grunts' burst rifles, the enforcers' laser rifles and the ogres' chainsaws lying about: past this "
                  "many, the oldest fades away (never one in your hand, pulled to it or in flight). Every other weapon, "
                  "drop and prop stays. 0: no limit (vr_enemy_weapon_drop_max)."),
        header("Ogres' Chainsaws"),
        slider("Fuel When Dropped", vr_chainsaw_drop_fuel_min, 0.f, 100.f, 5.f, "%.0f%% or more")
            .help("The fuel an ogre's chainsaw has as it drops: at least this much of a full tank, at random up to full. "
                  "It is the chainsaw's own: never refilled."),
        slider("Chain Fuel Use", vr_chainsaw_fuel_use, 0.f, 20.f, 0.5f, "%.1f%% a second").extend()
            .help("Each chainsaw has its own fuel (it goes with it, dropped or holstered): what its chain burns a second, "
                  "the trigger held, of a full tank. Out of fuel, it stalls."),
        slider("Idle Fuel Use", vr_chainsaw_idle_fuel_use, 0.f, 5.f, 0.1f, "%.1f%% a second").extend()
            .help("What the engine burns a second while it runs, the trigger or not."),
        slider("Runs On When Let Go", vr_chainsaw_drop_run, 0.f, 10.f, 0.5f, "%.1f s").extend()
            .help("A running chainsaw dropped or thrown runs on this long as it lies, then dies; taken back meanwhile, it is "
                  "still running (no cord to pull). 0: it stops as it leaves the hand."),
        open("Chainsaw Engine", pageIndex(pageChainsawEngine))
            .help("The running engine's exhaust smoke and its shake: in one hand, in two, lying on the ground."),
        slider("Blade Sinks In", vr_chainsaw_overlap, 0.f, 20.f, 1.f, "%.0f cm").extend()
            .help("How deep the running chain's bar may sink into a monster (it cuts in) before it stops at its surface, as "
                  "weapons do."),
        slider("Start Chance", vr_chainsaw_start_chance, 0.05f, 1.f, 0.05f, "%.2f")
            .help("The chance a good pull of the cord starts the engine: a few pulls may be needed."),
        slider("First Pulls Fail", vr_chainsaw_fail_pulls_min, 0.f, 5.f, 1.f, "%.0f")
            .help("Every start, at least this many good pulls of the cord fail before Start Chance applies (a cold engine "
                  "needs priming)."),
        slider("Up To", vr_chainsaw_fail_pulls_max, 0.f, 5.f, 1.f, "%.0f")
            .help("And at most this many: how many fail is drawn at random between the two each start."),
        slider("Cord Pull Distance", vr_chainsaw_pull_distance, 10.f, 60.f, 1.f, "%.0f cm").extend()
            .help("How far the cord must be pulled out for a pull (the handle taken in the other, empty, hand's grip)."),
        slider("Cord Pull Speed", vr_chainsaw_pull_speed, 0.3f, 4.f, 0.1f, "%.1f m/s").extend()
            .help("How fast the hand must pull it there: a slower pull is only a weak one, and never starts it."),
        header("Grunts' Burst Rifles"),
        toggle("Burst Rifles", vr_grunt_burst)
            .help("On: grunts fire 3-round bursts of single bullets, tighter than a shotgun's pellets, and the gun a "
                  "grunt drops fires the same bursts. Off: id's shotgun, the grunts' and the dropped guns' (six "
                  "pellets as yours, a shell a shot: a third of Rounds)."),
        slider("Grunts' Round Damage", vr_grunt_burst_damage, 1.f, 16.f, 1.f, "%.0f").extend()
            .help("A grunt's burst: each round's damage to you (5: about the shotgun's damage over time; its 4 pellets "
                  "did 4 each)."),
        slider("Rounds", vr_gruntgun_ammo, 3.f, 90.f, 3.f, "%.0f").extend()
            .help("The rounds in a dropped burst rifle, in threes (a burst's): its own, never refilled (shell boxes "
                  "fill your shotguns, not it). Empty, it is still a club, or drop it."),
        header("Enforcers' Laser Rifles"),
        slider("Laser Speed", vr_enfrifle_speed, 600.f, 4000.f, 100.f, "%.0f u/s").extend()
            .help("How fast a dropped rifle's lasers fly, in units a second (the enforcer's own: 600; your nails: "
                  "1750)."),
        slider("Shots", vr_enfrifle_ammo, 1.f, 80.f, 1.f, "%.0f").extend()
            .help("The shots in a dropped rifle: its own, never refilled (cells don't). Empty, it is still a club."),
        header("Spent Rifles"),
        slider("Spent Smoke", vr_enemygun_spent_smoke, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A burst rifle or a laser rifle whose last round you fire smokes this long, in the hand or dropped: it "
                  "is done, drop it (0: never)."),
        slider("Spent Crackle", vr_enemygun_spent_crackle, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 30.f)
            .help("And crackles all over with lightning's arcs this long, as a corpse the lightning struck (0: never)."),
        slider("Crackle Volume", vr_enemygun_spent_volume, 0.f, 1.f, 0.05f, "%.2f")
            .help("The corpses' crackle as it starts, this loud (0: silent)."),
        slider("Spent Shake", vr_enemygun_spent_shake, 0.f, 1.f, 0.05f, "%.2f units").extend(0.f, 3.f)
            .help("While it crackles in your hand the gun shakes this much, fading with its arcs (0: still)."),
        slider("Spent Haptics", vr_enemygun_spent_haptics, 0.f, 1.f, 0.05f, "%.2f")
            .help("And buzzes in your hand this hard (0: not at all)."),
    };
}

// VR Settings > Combat > Weapon Damage: every weapon's base damage in one place, to balance them (NOTES.md
// vrfiringrange_2026-10-01_02-33; ROUND21.md, "Weapon Damage menu"). id's numbers (or the mission packs') as shipped,
// but the crowbar's 20. A gun's applies to your shots only: monsters and traps firing the same keep id's (QC VR_WpnDmg).
// Damage to Enemies (Damage and Knockback) multiplies them all; a melee blow's is also times its speed and its weight.
[[nodiscard]] za::Vector<Item> pageWeaponDamage()
{
    return {
        header("Guns"),
        slider("Shotgun", vr_dmg_shotgun, 0.5f, 20.f, 0.5f, "%.1f a pellet").extend()
            .help("Each of its 6 pellets (id's 4; a head more). A grunt's dropped shotgun (Burst Rifles off) too."),
        slider("Double Shotgun", vr_dmg_super_shotgun, 0.5f, 20.f, 0.5f, "%.1f a pellet").extend()
            .help("Each of its 14 pellets (id's 4)."),
        slider("Nailgun", vr_dmg_nail, 1.f, 60.f, 1.f, "%.0f a nail").extend().help("id's 9 (a head more)."),
        slider("Super Nailgun", vr_dmg_super_nail, 1.f, 100.f, 1.f, "%.0f a nail").extend().help("id's 18 (a head more)."),
        slider("Grenade", vr_dmg_grenade, 10.f, 400.f, 5.f, "%.0f").extend()
            .help("A grenade's blast at its middle, less further out (id's 120): the launcher's, a hand grenade's and an "
                  "ogre's grenade caught and thrown back."),
        slider("Rocket", vr_dmg_rocket, 10.f, 400.f, 5.f, "%.0f").extend()
            .help("A direct hit, and up to a fifth more at random (id's 100); its blast on what's around 1.2 times it."),
        slider("Lightning Gun", vr_dmg_lightning, 1.f, 100.f, 1.f, "%.0f a bolt").extend()
            .help("Each bolt, 10 a second (id's 30). Its shock fired in water: Lightning Gun in Water."),
        header("Scourge of Armagon"),
        slider("Proximity Gun", vr_dmg_proximity, 10.f, 400.f, 5.f, "%.0f").extend().help("A mine's blast (95)."),
        slider("Laser Cannon", vr_dmg_laser, 1.f, 100.f, 1.f, "%.0f a bolt").extend()
            .help("Each bolt (18; one in ten a third more). A bolt bouncing back into what it hit does half."),
        slider("Mjolnir's Lightning", vr_dmg_mjolnir_lightning, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("Its first strike from the ground (80); the strikes after it 3/8 of it."),
        header("Dissolution of Eternity"),
        slider("Lava Nails", vr_dmg_lava_nail, 1.f, 60.f, 1.f, "%.0f a nail").extend()
            .help("A nail to a monster (15; to a player 3/5 of it)."),
        slider("Super Lava Nails", vr_dmg_super_lava_nail, 1.f, 100.f, 1.f, "%.0f a nail").extend()
            .help("A nail to a monster (30; to a player 3/5 of it)."),
        slider("Multi-Grenade", vr_dmg_multi_grenade, 10.f, 300.f, 5.f, "%.0f a bomblet").extend()
            .help("Each of the 5 bomblets it bursts into (90)."),
        slider("Multi-Rocket", vr_dmg_multi_rocket, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("A rocket's direct hit, and up to a quarter more (60); its blast 1.25 times it."),
        slider("Plasma Gun", vr_dmg_plasma, 5.f, 300.f, 5.f, "%.0f").extend()
            .help("A ball's direct hit, and up to a quarter more (80); its blast 7/8 of it, its arcs 5/8."),
        header("Dawn of the Machine"),
        slider("Super Axe", vr_dmg_superaxe, 1.f, 150.f, 1.f, "%.0f").extend()
            .help("A blow (40, twice the axe's), before its speed and weight; a zombie takes three times it, a blow that "
                  "kills twice it (it gibs). Its lightning: Mjolnir's Lightning. Held when the Dawn of the Machine data is "
                  "there (any campaign: Debug > Tests > Dawn of the Machine Weapons)."),
        slider("Super Axe Burst Window", vr_superaxe_burst_window, 0.3f, 4.f, 0.1f, "%.1f s").extend(0.1f, 10.f)
            .help("A second blow on the same monster within this long of the first fires the Super Axe's lightning burst "
                  "(15 cells; the head glows while it is ready). Dawn of the Machine's own: 0.5 s, too quick for real swings."),
        header("Melee"),
        slider("Fist", vr_dmg_fist, 1.f, 60.f, 1.f, "%.0f").extend()
            .help("A punch (10), before its speed (twice Swing Speed: a full blow) and Melee's Punch Damage Mult."),
        slider("Axe", vr_dmg_axe, 1.f, 100.f, 1.f, "%.0f").extend().help("A blow (id's 20), before its speed and weight."),
        slider("Crowbar", vr_crowbar_damage, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow (20, the axe's: 25 before), before its speed and weight; its hook hits 20% harder, its chisel "
                  "end 40% softer. Crowbars lie in the firing range's prop area."),
        slider("Gun as a Club", vr_dmg_gun_bash, 1.f, 60.f, 1.f, "%.0f").extend()
            .help("A gun swung at something (12), before its speed and weight."),
        slider("Mjolnir", vr_dmg_mjolnir, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow (25), before its speed and weight. Its lightning: above."),
        header("Enemy Weapons"),
        slider("Knight's Sword", vr_sword_damage_mult, 0.5f, 3.f, 0.05f, "%.2fx 20").extend()
            .help("Knights and hell knights always drop their sword: a swing's damage, times 20 (1.5: 30), before its "
                  "speed and weight; the hell knight's sword 25% more."),
        slider("Ogre's Chainsaw Swung", vr_dmg_chainsaw_swing, 1.f, 100.f, 1.f, "%.0f").extend()
            .help("A blow with it, the engine off or on (20), before its speed and its weight (heavy)."),
        slider("Ogre's Chainsaw Chain", vr_chainsaw_damage, 10.f, 300.f, 5.f, "%.0f a second").extend()
            .help("Ogres always drop their chainsaw: its running chain cuts what its bar is in, this much damage a second "
                  "(a grunt has 30 health, an ogre 200). Its fuel and cord: Enemy Weapons."),
        slider("Grunt's Burst Rifle", vr_gruntgun_damage, 1.f, 30.f, 1.f, "%.0f a round").extend()
            .help("Grunts always drop their gun: a trigger pull fires a 3-round burst, each round this much damage (a "
                  "head more; a grunt has 30 health). Burst Rifles off: the Shotgun's pellets."),
        slider("Enforcer's Laser Rifle", vr_enfrifle_damage, 1.f, 60.f, 1.f, "%.0f a shot").extend()
            .help("Enforcers always drop their laser rifle: it fires the enforcer's laser (the enforcer's: 15). As your "
                  "nails, it strikes corpses, gibs, props and breakables too."),
        header("Thrown"),
        slider("All Throws", "vr_weapon_throw_damage_mult", 0.05f, 5.f, 0.05f, "%.2fx").extend()
            .help("Every throw's damage times this: thrown weapons (from 20 for a gun to 60 for a sword at full speed, "
                  "more the faster it flies), rocks, boxes and gibs. 0.4 (the default; 0.5 from 2026-10-02, "
                  "0.35 from 2026-10-07): 1 is what throws did before 2026-10-02. Each weapon's own: Weapon Weights' "
                  "Throw Damage."),
        slider("Two-Hand Throws", vr_throw_2h_damage, 0.5f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A weapon or prop thrown with both hands: its hits' damage times this (one hand: 1x), on top of All "
                  "Throws. 1.25 as shipped."),
        header("More"),
        open("Damage and Knockback", pageIndex(pageDamage))
            .help("Damage to Enemies (all of these at once), Damage to You, headshots, knockback."),
        open("Weight and Damage", pageIndex(pageWeightDamage)).help("How a held weapon's weight changes its blows."),
    };
}

// VR Settings > Sound: the spatial audio (vr_audio.cpp; ROUND21.md, "Spatial audio (Steam Audio)"). Each feature off at
// 0; without Steam Audio (phonon.dll) only the underwater filter, the hands' and the moving sounds work.
[[nodiscard]] za::Vector<Item> pageSound()
{
    return {
        cycle("Spatial Audio", vr_snd_spatial, {{0.f, "Off"}, {1.f, "In VR"}, {2.f, "Always (headphones)"}})
            .help("Steam Audio over Quake's mixer: the features below. Off: Quake's left/right panning as it always was. "
                  "Always: on the desktop too (for headphones)."),
        header("Direction"),
        toggle("Sounds Around Your Head (HRTF)", vr_snd_hrtf)
            .help("Binaural: each sound shaped as your ears would hear it from where it is, by your head's position and "
                  "turn: in front, behind, above and below, not just left or right. Off: Quake's panning."),
        cycle("HRTF Smoothing", vr_snd_hrtf_interp, {{1.f, "Smooth (Bilinear)"}, {0.f, "Nearest (Cheaper)"}})
            .help("Between the measured directions: blended (moving sounds glide), or the nearest one."),
        cycle("Full-Band Sound", vr_snd_fullband,
            {{0.f, "Off (Quake's 11 kHz)"}, {1.f, "Binaural Sounds"}, {2.f, "All Sounds"}})
            .help("Quake's 11 kHz low-pass (sndspeed 11025, the default) cuts everything above 5.5 kHz, where the "
                  "binaural sounds' cues for ahead, behind, above and below are. Binaural Sounds: those skip it, each "
                  "resampled cleanly (no hiss from Quake's old 11 kHz sounds; the 22 and 44 kHz ones keep their highs). "
                  "All Sounds: the panned ones too. Off: everything through it, as Quake."),
        toggle("HRTF Anti-Aliasing", vr_snd_antialias)
            .help("With Full-Band Sound off and Quake's 11 kHz sound, the binaural sounds are filtered before "
                  "Quake's own low-pass, which keeps every fourth sample and so folds their highs down into the lows: "
                  "off, a sound at your side is far more one-sided than its HRTF and the balance near the front jumps "
                  "about. Off only to compare."),
        slider("HRTF Volume", vr_snd_hrtf_gain, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("The binaural sounds' volume against the others (1.25: as loud as Quake's panning, all round; 1.5 by default: a little louder)."),
        slider("Spatial Voices", vr_snd_voices, 4.f, 64.f, 4.f, "%.0f")
            .help("How many of the loudest sounds are rendered this way at once; the rest are panned as ever."),
        header("Walls and Rooms"),
        slider("Occlusion", vr_snd_occlusion, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 2.f)
            .help("A sound behind a wall or a closed door is quieter and duller (doors and lifts move in it as they move). "
                  "0 off, 1 full; 0.8 keeps monsters behind walls audible."),
        slider("Occlusion Rays", vr_snd_occlusion_samples, 1.f, 32.f, 1.f, "%.0f")
            .help("Rays from each sound (1: hidden or not; more: partly hidden, round corners). On the thread pool."),
        slider("Sound Source Size", vr_snd_occlusion_radius, 0.1f, 2.f, 0.1f, "%.1f m")
            .help("The sphere a sound comes from, for partial occlusion."),
        toggle("Air Absorption", vr_snd_air).help("Far sounds lose their highs in the air."),
        slider("Distance Falloff", vr_snd_falloff, 0.f, 2.f, 0.05f, "%.2fx")
            .help("How fast sounds get quieter with distance: 1 as Quake (a sound gone at about 30 m), 0.75 (the default) "
                  "a bit further, 0.5 twice as far, 0 never quieter. Every sound, spatial audio on or off."),
        slider("Room Reverb", vr_snd_reverb, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 2.f)
            .help("The reverb of the space around your head, simulated from the map (a small room rings short, a big hall "
                  "long): how loud. 0 off."),
        cycle("Reverb Quality", vr_snd_reverb_quality, {{0.f, "Low (Parametric)"}, {1.f, "Medium (Convolution)"}, {2.f, "High (Convolution, Long)"}})
            .help("Low: a reverb driven by the simulated decay times (cheapest, no echoes). Medium: the simulated response "
                  "itself (2 s). High: 3 s from twice the rays."),
        slider("Reverb Update", vr_snd_reverb_interval, 0.1f, 1.f, 0.05f, "%.2f s").extend(0.05f, 5.f)
            .help("Seconds between the room's simulations (on the thread pool)."),
        slider("Underwater Muffle", "snd_waterfx", 0.f, 2.f, 0.1f, "%.1f")
            .help("Sounds muffled while your head is in water, slime or lava (your head's place, not your body's). 0 off."),
        header("Movement and Nearness"),
        toggle("Weapons From Your Hands", vr_snd_hands)
            .help("Your guns' shots, your blows and what your hands do (a parry, a reload, drawing and holstering, a dry "
                  "click, the grenade pouch, a chainsaw's cord) sound from the hand doing it, not from the middle of your "
                  "head."),
        toggle("Sounds Follow Things", vr_snd_follow)
            .help("A sound stays with what made it as it moves (a monster, a door, a hook in flight)."),
        slider("Doppler", vr_snd_doppler, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 4.f)
            .help("Things coming at you sound higher, going away lower (1: as physics has it; 0 off)."),
        slider("Pitch Variation", vr_snd_pitch_jitter, 0.f, 10.f, 1.f, "%.0f%%").extend(0.f, 25.f)
            .help("The most frequent sounds (punches and blows, swings, squishes, clicks, knocks, casings, footsteps) "
                  "each a little higher or lower at random, up to this much, so that one recording isn't heard the same "
                  "every time (0 off)."),
        slider("Blow Material Layer", vr_snd_hit_layer, 0.f, 1.f, 0.1f, "%.1f")
            .help("A punch, a gun's butt, a headbutt or a thrown thing landing also sounds of what it hit: a slap of "
                  "flesh on a body, a clink of armour on a knight or an enforcer, a knock of wood on a crate, your "
                  "knuckles' knock on a wall. How loud, under the blow's thud (0 none)."),
        slider("Near Field", vr_snd_nearfield, 0.f, 2.f, 0.1f, "%.1f")
            .help("A sound within a metre of your head: the nearer ear louder, the farther one quieter and duller, by how "
                  "near and how much to the side. 0 off."),
        header("Advanced"),
        toggle("Mix Limiter", vr_snd_limiter)
            .help("Loud sounds piling up (explosive boxes blowing up together) are brought down smoothly just under full "
                  "volume instead of clipping, which crackles; quieter sounds untouched. Spatial audio on or off. Off: "
                  "Quake's hard clip, 6 dB lower."),
        slider("Limiter Ceiling", vr_snd_limiter_ceiling, -6.f, 0.f, 0.5f, "%.1f dB").extend(-12.f, 0.f)
            .help("The loudest the mix gets, under full volume (-1 by default; -6: Quake's clip, quieter explosions)."),
        cycle("Frame Size", vr_snd_frame, {{256.f, "256 (5.8 ms)"}, {512.f, "512"}, {1024.f, "1024 (Cheapest)"}})
            .help("Samples a voice renders at a time: larger is cheaper, a moving sound's direction coarser."),
        command("Spatial Audio Info", "vr_snd_info")
            .help("vr_snd_info: in the console, Steam Audio loaded or not, the voices, the scene and the simulations' times."),
    };
}

// Split from Gameplay: damage multipliers, positional damage and knockback.
[[nodiscard]] za::Vector<Item> pageDamage()
{
    return {
        header("Hit Detection"),
        toggle("Precise Hit Detection", vr_hit_precise)
            .help("Shots, projectiles, the grappling hook, melee blows and thrown things hit a monster's model as you see "
                  "it, not the big box round it: aim at the body. Off: Quake's boxes."),
        slider("Guns Tolerance", vr_hit_tolerance_guns, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to shots and projectiles: a near miss still hits."),
        slider("Grappling Hook Tolerance", vr_hit_tolerance_grapple, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to the grappling hook."),
        slider("Melee Tolerance", vr_hit_tolerance_melee, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to your melee blows and shoves."),
        slider("Melee Hitbox Size (of Melee Tolerance)", vr_hit_melee_scale, 0.25f, 4.f, 0.05f, "%.2fx").extend()
            .help("The melee probe's size: Melee Tolerance times this, so a monster's model grown by that many units is "
                  "what a blow meets (1: 6 units; 0.25: 1.5; 4: 24). It does not change the striking points' own 3-unit "
                  "thickness, nor the box the model is found out of, nor how far a swing reaches."),
        toggle("Head Priority", vr_hit_head_priority)
            .help("Where a hit point, or a whole melee sweep's way through a body, lies in more than one zone, the "
                  "head's zone decides it. Off: the limb and leg zones claim it first."),
        slider("Thrown Tolerance", vr_hit_tolerance_thrown, 0.f, 12.f, 0.5f, "%.1f units").extend()
            .help("How much bigger than the model a monster is to things you throw."),
        header("Damage"),
        slider("Damage to Enemies", vr_damage_to_enemies, 0.25f, 4.f, 0.05f, "%.2fx").extend().help("Damage you deal to monsters."),
        slider("Damage to You", vr_damage_to_player, 0.f, 4.f, 0.05f, "%.2fx").extend().help("Damage monsters, traps and falls deal to you."),
        slider("Self Damage", vr_damage_self, 0.f, 2.f, 0.05f, "%.2fx").extend().help("Damage your own rockets and grenades deal to you."),
        header("Positional Damage"),
        toggle("Positional Damage", vr_positional_damage).help("Headshots, arm and leg shots on humanoid monsters."),
        toggle("Melee Positional Damage", vr_melee_positional)
            .help("Your melee blows take the same head / limb / leg multipliers as your shots, from the point on the "
                  "model they met: a head is a head for a blade and for a shot. Off: flat melee damage, as before. The "
                  "looser melee tolerance stays what connects only."),
        slider("Headshot Damage", vr_headshot_mult, 1.f, 5.f, 0.1f, "%.1fx").extend(),
        slider("Arm Shot Damage", vr_limbshot_mult, 0.1f, 1.f, 0.05f, "%.2fx").extend(),
        slider("Leg Shot Damage", vr_legshot_mult, 0.1f, 1.f, 0.05f, "%.2fx").extend(),
        slider("Headshot Sound", vr_headshot_sound, 0.f, 1.f, 0.1f, "%.1f").help("Volume of the crack you hear when you land a headshot (0 off)."),
        header("Knockback"),
        slider("Knockback", vr_push, 0.f, 2.f, 0.05f, "%.2fx").extend().help("All knockback; the settings below scale this."),
        slider("Your Melee Hits", vr_melee_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far your melee blows (and headbutts) push what they hit."),
        slider("Weapon Hits", vr_hit_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far heavy weapon hits shove monsters."),
        slider("Killing Blows", vr_kill_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far killing blows and explosions throw the bodies."),
        slider("Parry Pushes Enemy", vr_parry_push_enemy, 0.f, 3.f, 0.05f, "%.2fx").extend(),
        slider("Parry Pushes You", vr_parry_push_player, 0.f, 3.f, 0.05f, "%.2fx").extend(),
        slider("Monsters' Blows Push You", vr_melee_push_player, 0.f, 3.f, 0.05f, "%.2fx").extend(),
        header("When You're Hit"),
        toggle("Hits Knock Your Hands", vr_pain_knock)
            .help("Being hit knocks your hands, arms and what they hold away from where the hit came from, more the "
                  "hand on that side, and they ease back. Looks only: your aim and your shots don't move."),
        slider("Knock per Damage", vr_pain_knock_strength, 0.f, 1.f, 0.05f, "%.2f cm").extend(0.f, 5.f)
            .help("How far the hands are knocked for each point of damage, for small hits (a grunt's volley: about 16); "
                  "harder ones curve towards Largest Knock."),
        slider("Largest Knock", vr_pain_knock_max, 0.f, 15.f, 0.5f, "%.1f cm").extend(0.f, 50.f)
            .help("How far the hardest hits knock the hands (a rocket nearly that far; a harder hit always a little "
                  "further; hits in quick succession add up to no more). Away from the hit and up."),
        slider("Knock Tip", vr_pain_knock_tip, 0.f, 5.f, 0.25f, "%.2f deg/cm").extend(0.f, 20.f)
            .help("How much the hands and what they hold tip up for each cm they are knocked (a gun's barrel kicks up), "
                  "about the wrist."),
        toggle("Knock Across Your View", vr_pain_knock_seen)
            .help("On: the knock's size is what you see, across your view (a hit from ahead throws the hands up and "
                  "towards you, from the side sideways). Off: straight away from the hit and up, which for hands held "
                  "ahead of and below your eyes is mostly towards your eyes, where it barely shows."),
        slider("Knock Time", vr_pain_knock_time, 0.1f, 1.5f, 0.05f, "%.2f s").extend(0.05f, 3.f)
            .help("How long the knock takes: out quickly, held a moment, then eased back."),
        slider("Hit Buzz", vr_pain_haptics, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("How hard the controllers buzz when you're hit: more the harder the hit, and in the hand on the side it "
                  "came from (0: none)."),
    };
}

// Blocking and shoving with a guard, and counter-attacks after a parry (the parry settings of Melee joined them).
[[nodiscard]] za::Vector<Item> pageParryBash()
{
    return {
        header("Parry"),
        toggle("Parry", "vr_parry").help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block monsters' melee blows."),
        slider("Parry Angle", vr_parry_angle, 15.f, 80.f, 5.f, "%.0f deg").extend()
            .help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block a monster's melee blow: how far it may be tilted off level."),
        slider("Parry Reach", vr_parry_reach, 0.5f, 2.5f, 0.1f, "%.1f m").extend().help("How far in front of you a held weapon still parries."),
        toggle("Parry Stops Attacks", vr_parry_interrupt)
            .help("A successful weapon or crossed-arm parry cancels the monster's remaining melee hits and briefly staggers it. Off: the original damage reduction and push."),
        slider("Parry Stagger", vr_parry_stagger, 0.1f, 1.5f, 0.05f, "%.2f s").extend(0.1f, 3.f)
            .help("How long the monster pauses after a successful parry before it can move and attack again."),
        slider("Stagger Frames", vr_parry_stagger_frames, 1.f, 4.f, 1.f, "%.0f")
            .help("How many frames of its pain animation a parried monster steps through (a tenth of a second each), "
                  "then rocks back and forth over the last two until it recovers. 1: it holds its first pain pose."),
        slider("Stagger Sway", vr_parry_stagger_sway, 0.f, 10.f, 0.5f, "%.1f deg").extend(0.f, 15.f)
            .help("How far a parried monster's body sways, dazed, while it is staggered. 0: none."),
        slider("Parry Damage Reduction", "vr_parry_reduction", 0.f, 1.f, 0.05f, "%.2f").help("Share of a parried blow's damage taken away."),
        slider("Parry Drop Chance", "vr_parry_drop_chance", 0.f, 1.f, 0.05f, "%.2f").help("Chance a one-handed parry knocks the weapon out of your hand (two hands: never). Not used with Parry Stamina on (Parry, Bash and Headbutt), which replaces it."),
        slider("Parry Arm Knock", "vr_parry_wobble", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much a parried blow knocks your hand and arm."),
        toggle("Unarmed Parry", vr_parry_unarmed).help("Cross your arms in an X in front of you to block a blow with your forearms."),
        slider("Unarmed Parry Reduction", vr_parry_unarmed_reduction, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Parry Cooldown", vr_parry_cooldown, 0.f, 1.5f, 0.05f, "%.2f s")
            .help("Per monster: a blow parried within this long of its last parried blow is still parried (its damage cut, "
                  "both pushed apart) "
                  "but makes no sound or sparks, opens no new counter window (the first one runs on) and costs no "
                  "stamina: one parry for an attack of quick hits (the ogre's chainsaw, a knight's swing). Its next "
                  "attack is a full parry again. 0: every blow a full parry."),
        toggle("Parry Cooldown: Whole Attack", vr_parry_cooldown_attack).extend()
            .help("Every blow of the attack whose blow you last parried is a cooldown blow, however long between them "
                  "(a fiend's two claws, the Overlord's double smash, a shambler's chained swings): one parry and one "
                  "stamina cost per attack. Off: only the Parry Cooldown's time."),
        header("Bash and Shove"),
        toggle("Bash", vr_bash).help("The parry stance (a weapon level across in front, one hand or two) pushed straight forward bashes: knocks monsters back and staggers them. Open palms facing a monster pushed at it shove it. Two hands (a weapon held two-handed, a palm pushing on the blade, both palms) push harder and further."),
        slider("Bash Speed", vr_bash_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend().help("How fast the stance (a weapon level across, held half a second) must be pushed forward, both its ends going ahead. A swing passing through the stance doesn't bash."),
        slider("Shove Speed", vr_shove_speed, 0.8f, 5.f, 0.1f, "%.1f m/s").extend().help("How fast open palms (facing ahead, not holding anything) must be pushed out, the arm extending, to shove. Hands waved or patted at a monster don't shove."),
        slider("Bash Damage", vr_bash_damage, 0.f, 40.f, 1.f, "%.0f").extend(),
        slider("Bash Push", vr_bash_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far a bash or shove throws what it hits (times Knockback)."),
        slider("Bash and Parry Sounds", vr_bash_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the sounds that tell a shove, a weapon bash, a counter bash (a bash right after a parry) and a parry apart from your blows (0: the old sounds)."),
        header("Counter-Attacks"),
        toggle("Counter-Attacks", vr_counter)
            .help("After a parry (a weapon's or crossed arms), your next melee attack in the window is a counter and hits harder: a blow with either hand, a bash or a shove. One a parry."),
        slider("Counter Window", vr_counter_window, 0.25f, 4.f, 0.05f, "%.2f s").extend().help("How long after a parry a counter may land."),
        slider("Counter Damage", vr_counter_damage, 1.f, 3.f, 0.05f, "%.2fx").extend().help("A counter's damage (a bash's or a shove's knockback too)."),
        slider("Counter Sounds", vr_counter_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of a blade's shing as the window opens and of the heavy strike as a counter lands (0 off)."),
        toggle("Counter Window Glow", vr_counter_glow)
            .help("While the counter's window is open, what your hands hold (a bare fist too) glows gold round its edges and sheds golden embers, fading as the window closes. Off as shipped."),
        slider("Counter Pulses", vr_counter_haptic, 0.f, 1.f, 0.1f, "%.1f")
            .help("Strength of the soft pulses in both hands while the window is open, fading as it closes (0 off)."),
        header("Training Dummy Attacks"),
        slider("Time Between Blows", vr_dummy_attack_period, 1.f, 8.f, 0.1f, "%.1f s").extend()
            .help("With the firing range's Dummy Attacks button on, the training dummy strikes at you this often, to practise parrying."),
        slider("Randomness", vr_dummy_attack_jitter, 0.f, 2.f, 0.05f, "%.2f s").extend()
            .help("How much sooner or later than that each blow may come, at random."),
        slider("Wind-Up", vr_dummy_attack_windup, 0.1f, 2.f, 0.05f, "%.2f s")
            .help("How long before each blow the dummy gives it away: a sound, a glow, the rifle raised as it rears back."),
        slider("Reach", vr_dummy_attack_reach, 16.f, 150.f, 2.f, "%.0f units")
            .help("How close you must be for it to strike (a knight's reach is 60). Further away it waits; step out of reach during the wind-up and it misses."),
        slider("Damage", vr_dummy_attack_damage, 0.f, 50.f, 1.f, "%.0f").extend()
            .help("A blow's damage, as a monster's: armour, the parry and god mode apply."),
    };
}

// Split from Parry, Bash and Headbutt: the stamina parries, shoves and blows cost.
[[nodiscard]] za::Vector<Item> pageStamina()
{
    return {
        header("Parry Stamina"),
        toggle("Parry Stamina", vr_parry_stamina)
            .help("Parrying tires you: each parry with a weapon costs stamina (less with the weapon in two hands), and the parry that leaves you none knocks the weapon out of your hand. It comes back once you stop parrying for a moment. On, it replaces the Parry Drop Chance (Melee Settings). Crossed arms cost nothing."),
        slider("Stamina", vr_parry_stamina_max, 20.f, 300.f, 5.f, "%.0f").extend().help("Your stamina when rested. Both hands share it."),
        slider("One-Handed Parry Cost", vr_parry_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a parry with the weapon in one hand costs (30 of 100: the fourth in a row knocks it away)."),
        slider("Two-Handed Parry Cost", vr_parry_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a parry with the weapon in both hands costs (12 of 100: the ninth in a row knocks it away)."),
        slider("Rest Before Recovering", vr_parry_stamina_delay, 0.f, 6.f, 0.25f, "%.2f s").extend()
            .help("How long you must go without parrying, shoving, striking or hanging from a hold (those that cost stamina) before stamina starts coming back."),
        slider("Recovery Rate", vr_parry_stamina_regen, 1.f, 100.f, 1.f, "%.0f /s").extend().help("Stamina a second it then comes back at."),
        slider("Tiring Warning", vr_parry_stamina_warn, 0.f, 1.f, 0.1f, "%.1f")
            .help("A breath and a throb in the hand when one more one-handed parry would knock the weapon away; a gasp and a long buzz when it does: their volume and strength (0 off)."),
        open("Stamina on the Gadget: Screens", pageIndex(pageScreens)).help("What the gadget shows (stamina and counters) is on HUD and Menus > Screens."),
        header("Shove and Strike Stamina"),
        toggle("Shove Stamina", vr_shove_stamina)
            .help("Shoves and bashes that land cost stamina from the same pool as parries. Without enough left, they throw back less and hurt less (Exhausted Knockback, Exhausted Damage)."),
        slider("One-Handed Shove Cost", vr_shove_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a shove or bash with one hand costs (15 of 100)."),
        slider("Two-Handed Shove Cost", vr_shove_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a shove or bash with both hands costs (20 of 100: it pushes further than a one-handed one)."),
        toggle("Strike Stamina", vr_strike_stamina)
            .help("Blows that land on something that takes damage cost stamina from the same pool, once a swing, however many things it goes through. Without enough left, they hurt less (Exhausted Damage). Blows on walls cost nothing."),
        slider("Punch Cost", vr_strike_stamina_punch, 0.f, 100.f, 1.f, "%.0f").extend().help("Stamina a punch costs (4 of 100)."),
        slider("Weapon Strike Cost", vr_strike_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a blow with a weapon held in one hand costs: a sword, an axe, Mjolnir, a gun, their pommel or butt too (8 of 100)."),
        slider("Two-Handed Strike Cost", vr_strike_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a blow with a weapon held in both hands costs (6 of 100: two arms share the work)."),
        slider("Exhausted Damage", vr_stamina_exhausted_damage, 0.1f, 1.f, 0.05f, "%.2fx")
            .help("Damage of a blow, shove or bash made with no stamina left for it (1: no penalty). With part of its cost left, in between."),
        slider("Exhausted Knockback", vr_stamina_exhausted_push, 0.1f, 1.f, 0.05f, "%.2fx")
            .help("How far a shove or bash made with no stamina left for it throws back and staggers (1: no penalty). With part of its cost left, in between."),        header("Climbing Stamina"),
        toggle("Climbing Stamina", vr_climb_stamina)
            .help("Hanging from a ledge or a rung (Climbing) spends stamina from the same pool, and none comes back while you hang. With none left your hands let go. More on the Climbing page."),
        slider("Hanging Cost, One Hand", vr_climb_stamina_rate, 0.f, 30.f, 0.5f, "%.1f /s").extend()
            .help("Stamina a second hanging from one hand costs (5 of 100: 20 s)."),
        slider("Hanging Cost, Two Hands", vr_climb_stamina_rate_2h, 0.f, 30.f, 0.5f, "%.1f /s").extend()
            .help("Stamina a second hanging from both hands costs, the two together (2 of 100: 50 s)."),
        header("Tired Arms"),
        slider("Shake From Stamina", vr_fatigue_shake_from, 0.f, 1.f, 0.05f, "%.2f").help("Below this share of your stamina your arms shake while you hang, more the less is left. 0: never. Looks only: your aim and shots stay steady."),
        slider("Shake Distance", vr_fatigue_shake, 0.f, 2.f, 0.1f, "%.1f cm").extend(0.f, 10.f).help("How far your drawn hands, arms and what they hold shake at most, with no stamina left. 0: no moving shake."),
        slider("Shake Turn", vr_fatigue_shake_angle, 0.f, 5.f, 0.25f, "%.2f deg").extend(0.f, 20.f).help("How far they turn as they shake at most, with no stamina left. 0: no turning shake."),
        slider("Shake Speed", vr_fatigue_shake_speed, 0.25f, 2.f, 0.05f, "%.2fx").extend(0.05f, 5.f).help("How quick the tremor is (1: 5 to 13 shakes a second)."),
        toggle("Shake Whenever Tired", vr_fatigue_shake_always).help("Off: your arms shake only while you hang from a ledge or a rung. On: whenever your stamina is low (after parries, shoves and blows too)."),
        header("Tired Legs"),
        toggle("Slower When Tired", vr_stamina_speed)
            .help("As your stamina runs low, you can't run as fast: full speed from half your stamina (From Stamina, on the Aiming page's Tired Arms) down to Slowest Run with none left, on the same curve as the weight."),
        slider("Slowest Run", vr_stamina_speed_min, 0.1f, 1.f, 0.05f, "%.2fx")
            .help("Times your most running speed with no stamina left (0.5: half; 1: never slower)."),
        open("Heavy When Tired (Aiming)", pageIndex(pageAimingSettings)).help("Tired hands get heavy too (what they hold, and an empty hand): Tired Arms, on the Aiming page."),
    };
}

// Batting projectiles back (Parry, Bash and Headbutt) and catching grenades (Carrying and Gibs).
[[nodiscard]] za::Vector<Item> pageBatting()
{
    return {
        header("Batting Projectiles"),
        toggle("Bat Back Projectiles", vr_deflect).help("Swing a weapon (a gun, a melee weapon, or a club: a wall torch, a brick) through a monster's spike, laser, spit or grenade to bat it away as a bat hits a ball: off the weapon's face and the way it swings, faster the harder you swing. A bash sends it the way you push. Empty hands never bat: they catch grenades."),
        slider("Batting Reach", vr_deflect_radius, 4.f, 32.f, 1.f, "%.0f units").extend().help("How near the weapon's blade, barrel or club a projectile must pass to be batted back."),
        slider("Batting Swing Speed", vr_deflect_speed, 0.2f, 1.5f, 0.05f, "%.2fx").extend().help("How fast a batting swing must be, times Swing Speed (a hit needs 1x, and more for a swung weapon)."),
        slider("Batting Timing", vr_deflect_window, 0.f, 0.5f, 0.05f, "%.2f s").extend().help("How early you may swing: the weapon's path keeps batting this long after it passed."),
        slider("Bash Batting Reach", vr_bash_deflect_radius, 4.f, 48.f, 1.f, "%.0f units").extend()
            .help("A bash (a weapon or a club held across, pushed) bats back projectiles that pass this near the weapon. A shove with open hands bats nothing."),
        slider("Bash Batting Timing", vr_bash_deflect_window, 0.f, 1.f, 0.05f, "%.2f s").extend().help("How long a bash goes on batting after the push."),
        slider("Batting Bounce", vr_deflect_bounce, 0.f, 1.f, 0.05f, "%.2f")
            .help("How lively a batted projectile comes off the weapon: 0 dead (it takes only the swing's speed), 1 as a rubber ball (a ball off a bat is about 0.5). A swing across sends it off to the side; the weapon's face driven at the thrower sends it back, the harder the faster."),
        slider("Batting Aim Assist", vr_deflect_aim_assist, 0.f, 1.f, 0.05f, "%.2f")
            .help("A batted or bashed projectile going very near a monster (within a few degrees; its thrower a little more) bends this much of the way towards it. 0: only your swing aims."),
        header("Grenades"),
        cycle("Catch Grenades", vr_grenade_catch, {{0.f, "Off"}, {1.f, "Ogres'"}, {2.f, "Ogres' and yours"}})
            .help("Grenades bounce and roll as physics objects, and you can catch them (by hand or force grab) and throw them "
                  "back; thrown, they burst on a monster as an ogre's does on you. \"And yours\" (the default): your grenade "
                  "launcher's too."),
        slider("Held Grenade Fuse", vr_grenade_held_fuse, 1.f, 5.f, 0.1f, "%.1f s")
            .help("A grenade you catch fizzes and ticks, and goes off this long after (it never shortens the fuse). Hold it too "
                  "long and it goes off in your hand."),
        toggle("Fuse Resets Every Catch", vr_grenade_fuse_regrab)
            .help("Off: the fuse is set once, as you catch the monster's throw; dropped and caught again, it keeps running. On: "
                  "every catch sets it again."),
        slider("Catch Radius", vr_grenade_catch_radius, 5.f, 30.f, 1.f, "%.0f cm")
            .help("How near your palm a grenade in flight must pass for an empty hand to catch it: grip held, or closing as it "
                  "arrives. A punching or shoving hand doesn't catch it (nor bat it: only a weapon does)."),
        slider("Catch Window", vr_grenade_catch_window, 0.f, 0.4f, 0.01f, "%.2f s")
            .help("A grenade that flies into your open palm stays there this long for your grip to close on it; then it drops "
                  "from the hand. 0: only a grip already closing catches."),
        toggle("Returned Grenades Hit Like Yours", vr_grenade_return_full)
            .help("A grenade you throw or bat back (an ogre's or your own) goes off as your grenade launcher's: its damage and "
                  "radius, credited to you, with your Quad. Off: an ogre's keeps its own, weaker blast."),
        toggle("Shoot Grenades", vr_grenade_shoot)
            .help("A shot, a nail, a rocket, the lightning, a thrown thing or another blast sets a grenade off where it is: "
                  "an ogre's in flight, yours on the ground, a hand grenade's dud left as a trap. Whoever set it off gets "
                  "the kills. Not one in your hand."),
        slider("Grenade Shot Size", vr_grenade_shoot_pad, 0.f, 8.f, 0.5f, "%.1f units").extend(0.f, 12.f)
            .help("How far beyond the grenade's model a shot still hits it (0: the model's own box, a few units across)."),
        toggle("Blows Set Grenades Off", vr_grenade_shoot_melee)
            .help("A very strong melee blow (the speeds and damage below) or a running chainsaw sets an enemy's grenade "
                  "off. Any other blow bats it away: one lying or rolling flies off the way the blow went; one in flight "
                  "is batted by a weapon's swing and caught by an empty hand. Your own grenades your blows pass through. "
                  "Off: blows pass through grenades."),
        slider("Blow Speed to Set Off", vr_grenade_melee_speed, 5.f, 40.f, 0.5f, "%.1f m/s").extend(0.f, 80.f)
            .help("How fast a punch, a gun's swing, a pommel or a gun's butt must go to set a grenade off rather than bat "
                  "it (the arm's speed, as the melee events print it). Your strongest punches go about 20-25."),
        slider("Weapon Blow Speed to Set Off", vr_grenade_melee_speed_weapon, 5.f, 60.f, 0.5f, "%.1f m/s")
            .extend(0.f, 100.f)
            .help("The same for a sword's, the axe's, the crowbar's, a club's or a stopped chainsaw's blade or head: they "
                  "are swung faster (an ordinary sword slash goes about 28). A running chainsaw always sets it off."),
        slider("Blow Damage to Set Off", vr_grenade_melee_damage, 0.f, 60.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("And the least damage the blow must deal (a full-speed punch deals about 25; tired blows less). 0: the "
                  "speed alone."),
        header("Hand Grenades"),
        toggle("Hand Grenades", vr_handgrenade)
            .help("Reach behind the small of your back with an empty hand and grip: a grenade from your pouch, while you have "
                  "rockets (the grenade launcher's ammo; one leaves your ammo with each grenade). Throw it as anything you "
                  "carry: it goes off as the launcher's grenade. Where the pouch is: below."),
        cycle("Arm Hand Grenades", vr_handgrenade_arm, {{0.f, "Trigger pulls the pin"}, {1.f, "When let go of"}})
            .help("Trigger: press it while holding the grenade to pull the pin (it fizzes; the fuse runs); let go of unarmed, "
                  "it is a dud you can pick up again. When let go of: the lever flies off as it leaves your hand. Either "
                  "way, let go of unarmed at the pouch, it goes back in."),
        slider("Hand Grenade Fuse", vr_handgrenade_fuse, 1.f, 5.f, 0.1f, "%.1f s")
            .help("From the pin (or the throw) to the blast. The launcher's grenades take 2.5 s. Hold it too long and it goes "
                  "off in your hand."),
        header("Grenade Pouch"),
        open("Grenade Pouch: Hip Holsters", pageIndex(pageHipHolsters)).help("Where the grenade pouch is and how a grenade sits in the hand: Weapons > Hip Holsters."),
        open("Pouch Turn (Hip Holsters)", pageIndex(pageHipHolsters))
            .help("The same place, and how the pouch is turned, with the hip holsters."),
    };
}

[[nodiscard]] za::Vector<Item> pageBody()
{
    return {
        open("Arms and Pauldrons", pageIndex(pageBodyArms)),
        open("Body Calibration", pageIndex(pageBodyCalibration)),
        header("Body"),
        open("Body and Build: Body and Display", pageIndex(pageBodyDisplay)).help("The body on or off and its build are on Body and Display."),
        toggle("Walking Legs", vr_body_walk).help("The legs (full body) walk as you move with the stick."),
        slider("Step Rate", vr_body_step_rate, 1.f, 5.f, 0.1f, "%.1f /s").extend()
            .help("How fast the legs step at most, in steps a second at full running speed (walking, somewhat fewer)."),
        slider("Turn Before Stepping", vr_body_turn_step, 15.f, 90.f, 5.f, "%.0f deg").extend()
            .help("How far you turn over your planted feet before they step round to follow."),
        slider("Wading Heaviness", vr_body_wade, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Wading, the legs walk heavier: shorter, higher, slower steps (0: as on land)."),
        slider("Swimming Kicks", vr_body_swim_kick, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Swimming, the legs trail behind and kick where the stick moves you: how wide (0: no kicks)."),
        slider("Swimming Kick Rate", vr_body_swim_kick_rate, 0.f, 4.f, 0.1f, "+%.1f /s").extend()
            .help("How many more kicks a second at full stick (treading water, about 0.7)."),
        toggle("Show Armour and Wounds", vr_body_state)
            .help("The armour you wear plates your torso; your arms and hands get bloodier as you are hurt (with Dynamic Wounds, on the Gore page: where you are hit)."),
        open("Wounds Drip Blood: Gore", pageIndex(pageGore)).help("The wounds' dripping blood is on Gore (Arm Drip Rate)."),
        toggle("Show Powerups", vr_body_powerups)
            .help("Quad damage sparks around your hands, the pentagram makes you glow, the ring fades you."),
        toggle("Anchors Follow Body", vr_body_anchors)
            .help("Holsters, the virtual stock and hand collisions follow the body's lean and crouch."),
        slider("Hip Holsters Follow Legs", vr_holster_leg_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("Full body: the hip holsters move with the walking and kicking legs (0: fixed on the body, 1: all the way)."),
        header("Body Collisions"),
        toggle("Body Collisions", vr_body_collide)
            .help("Your hands and the weapons in them stop at your other hand, your other arm, the wrist gadget and your "
                  "body instead of passing through them. Push on and they pass through (Pass Through At). Drawn only: "
                  "hits, shots, aim and grabs are still where your hands are."),
        slider("Pass Through At", vr_body_collide_pass, 0.3f, 1.f, 0.05f, "%.2f").extend(0.05f, 1.f)
            .help("How far through what stops it your hand or weapon must be pushed before it lets go and passes "
                  "through: 0.7, seven tenths of the way (and held out at most 10.5 cm); 1, only once all the way through. It "
                  "stops again once clear."),
        toggle("Elbows Out of the Torso", vr_body_collide_elbows)
            .help("With Body Collisions: an elbow that would go into your torso (a hand across your chest) swings out "
                  "round the line from the shoulder to the wrist."),
        header("Torso Direction"),
        cycle("Torso Follows", vr_torso_mode, {{1.f, "Mostly the head"}, {0.f, "The hands (old)"}})
            .help("Nothing tracks your torso: it is guessed. Mostly the head: it faces where your head has been facing, "
                  "the hands pulling it a little, only those in front of you (not one behind your back or far off to a "
                  "side); old: four fifths of the way towards the hands, wherever they are."),
        slider("Head Weight", vr_torso_head, 0.f, 5.f, 0.1f, "%.1f")
            .help("The torso faces a weighted mean of directions. This one: where your head faces now."),
        slider("Head History Weight", vr_torso_head_history, 0.f, 5.f, 0.1f, "%.1f")
            .help("Where your head has faced lately: higher, a glance turns the torso less (and a turn follows later)."),
        slider("Head History Span", vr_torso_head_lag, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("How long \"lately\" is."),
        slider("Both Hands Weight", vr_torso_hands, 0.f, 10.f, 0.1f, "%.1f")
            .help("The middle of your hands, when both are in front of you (holding a gun out, reaching for something)."),
        slider("One Hand Weight", vr_torso_one_hand, 0.f, 5.f, 0.1f, "%.1f")
            .help("One hand alone in front, the other behind your back or hanging down."),
        slider("Hands Down Weight", vr_torso_hands_down, 0.f, 5.f, 0.1f, "%.1f")
            .help("Both hands hanging by your sides: square to the line between them (they turn with your body, not with "
                  "a glance)."),
        slider("Side Reach", vr_torso_side_angle, 10.f, 90.f, 5.f, "%.0f deg")
            .help("A hand further off to a side than this pulls less, and not at all 35 degrees further."),
        slider("Turn Deadzone", vr_torso_deadzone, 0.f, 30.f, 1.f, "%.0f deg")
            .help("How far off the guess must be before the torso turns (then it turns all the way)."),
        slider("Turn Speed", vr_torso_speed, 0.f, 30.f, 1.f, "%.0f").extend(0.f, 100.f)
            .help("How quickly it turns then (0: at once)."),
        slider("Neck Turn", vr_torso_neck_max, 30.f, 120.f, 5.f, "%.0f deg")
            .help("How far your head turns from the torso at most: past it the torso turns with the head."),
        header("Placement"),
        slider("Torso Offset", vr_body_torso_back, -0.2f, 0.4f, 0.01f, "%.2f m").extend(-1.f, 1.f)
            .help("How far the torso sits behind your neck (negative: in front)."),
        slider("Legs Offset", vr_body_legs_back, -0.2f, 0.4f, 0.01f, "%.2f m").extend(-1.f, 1.f)
            .help("How far the feet stand behind your head (negative: in front)."),
        slider("Shoulders Offset", vr_body_shoulders_back, -0.15f, 0.2f, 0.01f, "%.2f m").extend(-0.5f, 0.5f)
            .help("How far the shoulders sit behind your neck (negative: in front)."),
        slider("Eyes Forward", vr_body_eye_forward, 0.f, 0.25f, 0.01f, "%.2f m").extend(-0.1f, 0.5f)
            .help("From the top of the neck to the eyes, forward."),
        slider("Eyes Up", vr_body_eye_up, 0.f, 0.25f, 0.01f, "%.2f m").extend(-0.1f, 0.5f).help("From the top of the neck to the eyes, up."),
        slider("Crouch Tilt", vr_body_crouch_tilt, 0.f, 80.f, 5.f, "%.0f deg").extend()
            .help("How far the back tilts forward in a full crouch (the hips stay under you)."),
        header("Death View"),
        cycle("Death View", vr_death_view, {{0.f, "Off"}, {1.f, "Third Person"}, {2.f, "Immersive"}})
            .help("When you die (not gibbed) your body falls as a ragdoll (also on VR Settings, Comfort). Off: no body. "
                  "Third Person: your view stays where your eyes were. Immersive: your view goes into your body's head "
                  "(Third Person while a menu is open)."),
        toggle("Immersive: Turn With the Body", vr_death_view_turn)
            .help("Immersive Death View: your view turns sideways as your body's head does, slowly (Turn Speed). Off: "
                  "only its place follows the head; you turn yourself."),
        slider("Immersive: Turn Speed", vr_death_view_turn_speed, 10.f, 180.f, 10.f, "%.0f deg/s")
            .help("The fastest the Immersive Death View turns with your body's head."),
        slider("Immersive: Smoothing", vr_death_view_smooth, 0.f, 0.5f, 0.05f, "%.2f s")
            .help("How long the Immersive Death View takes to follow your body's head: more is gentler, less is closer."),
        slider("Immersive: Fade", vr_death_view_fade, 0.f, 2.f, 0.1f, "%.1f s")
            .help("The view fades in from black as it goes into your body's head, and as you respawn. 0: no fade."),
        slider("Immersive: Out for Menus", vr_death_view_menu_time, 0.f, 1.f, 0.05f, "%.2f s")
            .help("Opening a menu (or the console) while dead in the Immersive Death View moves the view out to Third "
                  "Person's place over this long, and back into your body's head as it closes (0: at once). The setting "
                  "stays Immersive."),
        slider("Your Body: Killing Blow's Push", vr_death_ragdoll_push, 0.f, 3.f, 0.25f, "%.2fx")
            .help("Your ragdoll body: the blow that killed you throws it this much of what Quake throws the player (0: "
                  "only the motion you had), at most about 10 m/s."),
    };
}

[[nodiscard]] za::Vector<Item> pageBodyCalibration();

// Split from Body: the arms' reach and bend, the shoulders, and the pauldrons over them. Body Calibration measures the
// arms' lengths and the shoulders (vr_bodycal_*); the sliders here are tweaks on top (vr_body_tweak_*, 0: as measured;
// uncalibrated, on the default body), and the settings it doesn't measure. Built again when calibrated or not
// (armsPageCalibrated): Arm Length is only for the default body's arms.
int armsPageCalibrated = -1;

const char* armsMeasured(int i)
{
    const char* line = bodycal::measuredLine(i);
    return line ? line : "";
}

const char* armLengthUnused()
{
    return "Arm Length: not used (arms measured)";
}

void armsResetTweaks()
{
    bodycal::resetTweaks();
}

[[nodiscard]] za::Vector<Item> pageBodyArms()
{
    armsPageCalibrated = bodycal::calibrated() ? 1 : 0;
    za::Vector<Item> list{open("Body Calibration", pageIndex(pageBodyCalibration))};
    for(int i = 0; bodycal::measuredLine(i); i++)
    {
        list.pushBack(infoLine(armsMeasured, i));
    }
    list.pushBackMultiple(
        header("Arms"),
        slider("Upper Arm", vr_body_tweak_upper_arm, -10.f, 10.f, 0.5f, "%+.1f cm").extend(-30.f, 30.f)
            .help("Longer (negative: shorter) than measured, real cm: the shoulder joint to the elbow. 0: as Body "
                  "Calibration measured it (uncalibrated: the default body's arm, times Arm Length)."),
        slider("Forearm", vr_body_tweak_forearm, -10.f, 10.f, 0.5f, "%+.1f cm").extend(-30.f, 30.f)
            .help("Longer (negative: shorter) than measured, real cm: the elbow to the drawn hand's wrist. 0: as Body "
                  "Calibration measured it (uncalibrated: the default body's forearm, times Arm Length).")
    );
    if(armsPageCalibrated)
    {
        list.pushBack(info(armLengthUnused));
    }
    else
    {
        list.pushBack(slider("Arm Length", vr_body_arm_length, 0.7f, 1.4f, 0.01f, "%.2fx").extend(0.5f, 2.f)
                .help("Uncalibrated: the default body's arms (for your height), times this. Once Body Calibration "
                      "has measured yours, it isn't used."));
    }
    list.pushBackMultiple(
        slider("Arm Stretch", vr_body_arm_stretch, 1.f, 1.5f, 0.05f, "%.2fx").extend(1.f, 3.f)
            .help("How far the drawn arms may stretch to reach a hand beyond them (1: not at all). Calibrated, only past "
                  "your measured reach and the Shoulder Reach (tracking glitches, a lunge); uncalibrated, before the "
                  "shoulders reach."),
        slider("Shoulder Reach", vr_body_shoulder_reach, 0.f, 0.25f, 0.01f, "%.2f m").extend(0.f, 0.6f)
            .help("How far the shoulders may move out towards a hand the arm can't reach, metres (not measured)."),
        header("Shoulders"),
        slider("Shoulders Back", vr_body_tweak_shoulders_back, -0.1f, 0.1f, 0.005f, "%+.3f m").extend(-0.5f, 0.5f)
            .help("The shoulder joints further back (negative: forward) than measured, metres. 0: as Body Calibration "
                  "measured them (uncalibrated: the default body's)."),
        slider("Shoulders Higher", vr_body_tweak_shoulders_up, -0.1f, 0.1f, 0.005f, "%+.3f m").extend(-0.5f, 0.5f)
            .help("The shoulder joints higher (negative: lower) than measured, metres. 0: as measured (uncalibrated: "
                  "the default body's)."),
        slider("Shoulders Wider", vr_body_tweak_shoulders_out, -0.1f, 0.1f, 0.005f, "%+.3f m").extend(-0.3f, 0.3f)
            .help("Each shoulder joint further out (negative: in) than measured, metres. 0: as measured "
                  "(uncalibrated: the default body's)."),
        slider("Shoulder Rise", vr_body_tweak_shoulder_rise, -30.f, 30.f, 1.f, "%+.0f deg").extend(-90.f, 90.f)
            .help("Degrees more (negative: fewer) the shoulders rise as you reach up than measured. 0: as measured "
                  "(uncalibrated: the default body's 25)."),
        slider("Shoulder Swing", vr_body_tweak_shoulder_swing, -30.f, 30.f, 1.f, "%+.0f deg").extend(-90.f, 90.f)
            .help("Degrees more (negative: fewer) the shoulders swing forward as you reach far forward than measured. "
                  "0: as measured (uncalibrated: the default body's 20)."),
        action("Reset Tweaks", armsResetTweaks)
            .help("Upper Arm, Forearm and the shoulders' five tweaks to 0: your arms and shoulders as measured "
                  "(uncalibrated: the default body's). Arm Length and the rest stay."),
        header("Elbows and Wrists (not measured)"),
        slider("Forearm Twist", vr_body_forearm_twist, 0.f, 1.f, 0.05f, "%.2f")
            .help("Share of the wrist's roll the middle of the forearm follows (none at the elbow, all at the wrist)."),
        slider("Wrist Limits", vr_body_wrist_limits, 0.f, 2.f, 0.05f, "%.2f")
            .help("The elbow swings round to keep the wrist within a real one's bend and roll, times this (0: never)."),
        slider("Elbow Stays Clear", vr_body_elbow_lift, 0.f, 8.f, 0.5f, "%.1f")
            .help("How much that swing keeps the elbow out of the torso and from crossing the chest by the neck (a hand "
                  "near the face or the chest), times this (0: free, as before)."),
        slider("Elbows Spread", vr_body_elbow_spread, 0.f, 1.f, 0.05f, "%.2f")
            .help("With the hand at the face or the chest and the palm down, how far the elbow turns out to the side "
                  "(wings). Palm up or on its side, it stays tucked in (0: as before)."),
        slider("Elbow Out", vr_body_elbow_out, 0.f, 1.f, 0.05f, "%.2f")
            .help("Where the elbow points: down, plus this much outwards."),
        slider("Elbow Back", vr_body_elbow_back, 0.f, 1.f, 0.05f, "%.2f")
            .help("Where the elbow points: down, plus this much backwards."),
        slider("Elbow From Hand", vr_body_elbow_hand, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the elbow points away from the back of the hand."),
        slider("Elbow Tuck", vr_body_elbow_tuck, 0.f, 1.f, 0.05f, "%.2f")
            .help("With the hand at the face or the chest (aiming down the sights, an axe raised by the face), the "
                  "elbow stays down and back by the ribs instead of swinging out or across (0: as before)."),
        slider("Tucked Elbow Back", vr_body_elbow_tuck_back, 0.f, 2.f, 0.05f, "%.2f")
            .help("Where a tucked elbow points: down, plus this much backwards (more lets it go behind the chest)."),
        slider("Tuck Within", vr_body_elbow_tuck_near, 0.3f, 1.f, 0.05f, "%.2f arm")
            .help("Fully tucked with the wrist this close to the shoulder (times the arm's length)."),
        slider("Tuck Fades By", vr_body_elbow_tuck_far, 0.4f, 1.2f, 0.05f, "%.2f arm")
            .help("Not tucked at all with the wrist this far from the shoulder (times the arm's length)."),
        header("Pauldrons"),
        toggle("Pauldrons", vr_body_pauldrons).help("Leather pads over the shoulders and the tops of the arms, as the Quake ranger wears."),
        cycle("Pauldron Style", vr_body_pauldron_style, {{0.f, "Ranger leather"}, {1.f, "Armour colour"}, {2.f, "Steel"}})
            .help("Armour colour: green, yellow or red as the armour you wear (leather without)."),
        slider("Pauldron Size", vr_body_pauldron_size, 0.5f, 1.5f, 0.05f, "%.2fx").extend(0.25f, 4.f),
        slider("Pauldron Follows Arm", vr_body_pauldron_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder cap turns with the upper arm (the lower plates follow the arm fully)."),
        slider("Pauldron Forward", vr_body_pauldron_forward, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
        slider("Pauldron Up", vr_body_pauldron_up, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
        slider("Pauldron Out", vr_body_pauldron_out, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f)
    );
    return list;
}

// Body Calibration (vr_bodycal.cpp): start or continue it, the result to Apply or Cancel, each pose to take again.
int bodycalVersion = -1;
int bodycalSeated = -1;

void bodycalStart()
{
    bodycal::start(qvr::menu::currentPage());
}

void bodycalRestart()
{
    bodycal::restart(qvr::menu::currentPage());
}

void bodycalApply()
{
    bodycal::apply();
}

void bodycalCancel()
{
    bodycal::cancel();
}

void bodycalUndo()
{
    bodycal::undo();
}

void bodycalSwitch()
{
    bodycal::switchShown();
}

const char* bodycalLine(int i)
{
    const char* line = bodycal::statusLine(i);
    return line ? line : "";
}

void bodycalRedo(int step)
{
    bodycal::redo(step, qvr::menu::currentPage());
}

const char* bodycalIntro(int i)
{
    static constexpr const char* lines[] = {"Measures your shoulders and arms from", "a few poses and moves, so that the drawn",
        "arm bends and straightens with yours."};
    return lines[i];
}

[[nodiscard]] za::Vector<Item> pageBodyCalibration()
{
    bodycalVersion = bodycal::version();
    bodycalSeated = vr_bodycal_seated.value != 0.f ? 1 : 0;
    za::Vector<Item> list{infoLine(bodycalIntro, 0), infoLine(bodycalIntro, 1), infoLine(bodycalIntro, 2),
        cycle("Position", vr_bodycal_seated, {{0.f, "Standing"}, {1.f, "Seated"}})
            .help("Seated, the height stays as it is (set it standing: Set Height Now); the arms and shoulders are measured "
                  "the same.")};
    if(bodycal::phase() == bodycal::Phase::Result)
    {
        if(bodycal::trusted())
        {
            list.pushBack(action("Apply", bodycalApply)
                .help("Sets the measurements: your arms' lengths, where your shoulders are and how they rise and swing, and "
                      "(standing) your height. Undo puts the settings back."));
        }
        list.pushBackMultiple(
            action("Cancel", bodycalCancel).help("Nothing changes."),
            action(bodycal::showingNew() ? "Showing: New Measurements" : "Showing: Current Settings", bodycalSwitch)
                .help("Your body (and the one in front of you) with the new measurements, or with the settings as they are: "
                      "bend and straighten your arms to compare."),
            cycle("Body in Front", vr_bodycal_preview, {{0.f, "Off"}, {1.f, "Facing You"}, {2.f, "From the Side"}})
                .help("Your body in front of you while this page shows a result, moving as you do.")
        );
    }
    else
    {
        list.pushBack(action(bodycal::partial() ? "Continue Calibration" : "Start Calibration", bodycalStart)
                           .help("Stand (or sit) with room to stretch your arms. Follow the text in front of you and the "
                                 "figure ahead: after three beeps and a high one, take the pose and hold still until the "
                                 "click; the moves record for a few seconds. About two minutes. The menu button stops "
                                 "(Continue takes the rest)."));
        if(bodycal::partial())
        {
            list.pushBack(action("Start Over", bodycalRestart).help("All the poses again."));
        }
        if(bodycal::canUndo())
        {
            list.pushBack(action("Undo", bodycalUndo).help("Puts back the settings from before the last Apply, exactly."));
        }
    }
    for(int i = 0; bodycal::statusLine(i); i++)
    {
        list.pushBack(infoLine(bodycalLine, i));
    }
    if(bodycal::phase() == bodycal::Phase::Result || bodycal::partial())
    {
        list.pushBack(header("Poses"));
        for(int i = 0; i < bodycal::stepCount(); i++)
        {
            if(bodycal::stepUsed(i))
            {
                list.pushBack(row(bodycal::stepRow, bodycal::stepHelp, bodycalRedo, i, -1));
            }
        }
    }
    return list;
}

// Whether each grip's fingers are set by hand (vr_flashlight_low_fingers, _high_fingers), as the Flashlight page was built:
// a change rebuilds it (the curls or the overlap shown).
int flashlightPageManual[2]{-1, -1};

// A grip's fingers on the torch (round 21, the author's tuning notes), as a weapon's (Weapon Offsets > Fingers on the
// Weapon): Fingers, then with Manual the curls and Thumb Across, with Automatic the Overlap; then the finger tweaks and
// the thumb's place.
struct FlashlightFingerCvars
{
    cvar_t& fingers;
    cvar_t& overlap;
    cvar_t& curlThumb;
    cvar_t& thumbAcross;
    cvar_t& curlIndex;
    cvar_t& curlMiddle;
    cvar_t& curlRing;
    cvar_t& curlPinky;
    cvar_t& biasThumb;
    cvar_t& biasIndex;
    cvar_t& biasMiddle;
    cvar_t& biasRing;
    cvar_t& biasPinky;
    cvar_t& thumbX;
    cvar_t& thumbY;
    cvar_t& thumbZ;
};

void flashlightFingers(za::Vector<Item>& list, const FlashlightFingerCvars& c, int& manualAsBuilt)
{
    const char* curlHelp = "How far this finger is curled round the torch: 0 open, 1 a fist (the controller's grip still opens it).";
    const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it holds the torch (a share of a full curl).";
    list.pushBack(cycle("Fingers", c.fingers, {{0.f, "Automatic"}, {1.f, "Manual"}})
                       .help("Automatic: the fingers wrap the torch on their own. Manual: they take the curls set below "
                             "(no fitting: the hand is where the grip's sliders put it)."));
    manualAsBuilt = c.fingers.value >= 0.5f ? 1 : 0;
    if(manualAsBuilt)
    {
        list.pushBackMultiple(
            slider("Thumb Curl", c.curlThumb, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Thumb Across", c.thumbAcross, 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            slider("Index Curl", c.curlIndex, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Middle Curl", c.curlMiddle, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Ring Curl", c.curlRing, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Little Curl", c.curlPinky, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp)
        );
    }
    else
    {
        list.pushBack(slider("Overlap", c.overlap, 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the fingers and palm may sink into the torch: 0 they stop on its surface, 1 a centimetre in."));
    }
    list.pushBackMultiple(
        slider("Thumb", c.biasThumb, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Index Finger", c.biasIndex, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Middle Finger", c.biasMiddle, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Ring Finger", c.biasRing, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Little Finger", c.biasPinky, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Thumb X (forward)", c.thumbX, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f).help("Moves the thumb on the hand."),
        slider("Thumb Y (palm)", c.thumbY, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
        slider("Thumb Z (up)", c.thumbZ, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f)
    );
}

// Split from Body: the torch on the chest.
[[nodiscard]] za::Vector<Item> pageFlashlight()
{
    za::Vector<Item> list = {
        open("Low Grip", pageIndex(pageFlashlightLowGrip)).help("The torch in your hand as you take it: its place, its turn and the fingers on it."),
        open("Overhead Grip", pageIndex(pageFlashlightOverheadGrip)).help("The torch turned over in your hand (B or Y): its place, its turn and the fingers on it."),
        open("On a Gun or Head", pageIndex(pageFlashlightMounts)).help("Where the torch clips on a gun or on your head, and the zones that clip it on."),
        header("Flashlight"),
        open("Chest Flashlight and Side: Body and Display", pageIndex(pageBodyDisplay)).help("The chest flashlight on or off and its side are on Body and Display."),
        slider("Brightness", vr_flashlight_brightness, 0.25f, 2.5f, 0.05f, "%.2fx").extend(),
        slider("Range", vr_flashlight_range, 300.f, 2000.f, 50.f, "%.0f").extend(100.f, 6000.f),
        slider("Visible Beam", vr_flashlight_beam, 0.f, 1.f, 0.05f, "%.2f").help("A soft cone of light in the air from the lamp (0: none)."),
        cycle("Beam Quality", vr_flashlight_beam_quality, {{0.f, "Low"}, {1.f, "Medium"}, {2.f, "High"}})
            .help("How closely the visible beam fades where walls cut it. Higher looks for them more often, costing more time each frame."),
        toggle("Casts Shadows", vr_flashlight_shadows).help("Its light casts shadows (takes one of the shadowed dynamic lights)."),
        cycle("Cord", vr_flashlight_cord, {{0.f, "None"}, {1.f, "Chain"}, {2.f, "Coiled"}}).help("The retracting cord from the clip on your belt to the torch while it is off the belt. Chain: a rusty low-poly iron chain (chunky links of square bar, flat-shaded: Quake's look). Coiled: a springy coiled cord like an old telephone's, its turns opening out as you pull it. Both sag and swing as your hand moves. None: no cord drawn."),
        hueSlider("Beam Hue", vr_flashlight_hue).help("The beam's colour, with Beam Saturation (at 0 it is white): its light, the beam in the air and the lens. 40 warm, 200 cold blue; Player's: the Player Effects Hue."),
        slider("Beam Saturation", vr_flashlight_saturation, 0.f, 1.f, 0.05f, "%.2f").help("0 white (the default), 1 the Beam Hue in full."),
        header("Turning It Over"),
        toggle("Flick to Turn Over", vr_flashlight_flick)
            .help("A sharp flick of your wrist up or down turns the torch in your hand over, as B or Y does (the beam out "
                  "past your thumb or out past your little finger). Not by a gun or your head (B or Y clips it on there)."),
        slider("Flick Strength", vr_flashlight_flick_speed, 300.f, 1500.f, 50.f, "%.0f deg/s").extend(100.f, 3000.f)
            .help("How fast your wrist must turn up or down to count. Higher: only sharper flicks (fewer by accident)."),
        slider("Flick: Hand Still Below", vr_flashlight_flick_max_move, 0.5f, 4.f, 0.1f, "%.1f m/s").extend(0.1f, 10.f)
            .help("Your hand moving faster than this (a swing, a punch) never flicks: the wrist alone."),
        slider("Flick Cooldown", vr_flashlight_flick_cooldown, 0.1f, 1.5f, 0.05f, "%.2f s")
            .help("After a flick, how long before the next one counts (your wrist coming back is not a second flick)."),
        header("Taking and Clipping On"),
        slider("Grab Range", vr_flashlight_grab_range, 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How far from the torch your hand reaches it (it lights up, a grip takes it): times 9 cm from its middle line. Higher is more lenient."),
        slider("Head Clip Range", vr_flashlight_head_range, 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How far from your temple the held torch clips on your head, and a hand takes it off there: times Head Zone Radius (On a Gun or Head)."),
        slider("Gun Clip Range", vr_flashlight_gun_range, 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 4.f)
            .help("How far from its place on the gun in your other hand the held torch clips on it (B or Y, or letting go with Clip on Gun When Let Go): times Gun Zone Radius (On a Gun or Head)."),
        toggle("Clip on Head When Let Go", vr_flashlight_auto_head)
            .help("Let go of the torch where it lights up by your head and it clips on there, no B or Y needed."),
        toggle("Clip on Gun When Let Go", vr_flashlight_auto_gun)
            .help("Let go of the torch where it lights up by the gun in your other hand and it clips on the gun, no B or Y needed."),
        slider("Clip-On Transition", vr_flashlight_clip_time, 0.f, 0.5f, 0.01f, "%.2f s").extend(0.f, 1.f)
            .help("How long the torch takes to move and turn from where it was onto the gun or your head as it clips on. It is on at once (it follows the gun, lights where it aims); only how it is drawn eases in. 0: it jumps there."),
        header("On the Belt"),
        slider("Lean Out", vr_flashlight_tilt, -90.f, 90.f, 1.f, "%.0f deg").extend().help("How far the stored torch, hanging on your belt lens down, leans its lens out from your body."),
        slider("Forward", vr_flashlight_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("Up", vr_flashlight_up, -0.4f, 0.4f, 0.01f, "%.2f m").extend(),
        slider("Out", vr_flashlight_out, -0.3f, 0.3f, 0.01f, "%.2f m").extend().help("Away from your middle, towards its side."),
        header("In the Hand"),
        slider("In Hand Forward", vr_flashlight_hand_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Where the held lamp sits in your fist."),
        slider("In Hand Up", vr_flashlight_hand_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
    };
    return list;
}

// Split from Flashlight: the low grip (the one you take it in) and its fingers.
[[nodiscard]] za::Vector<Item> pageFlashlightLowGrip()
{
    za::Vector<Item> list = {
        header("In the Hand: Low Grip"),
        slider("Low Grip Forward", vr_flashlight_low_x, -30.f, 30.f, 0.5f, "%.1f cm").extend()
            .help("The torch's place in your fist when the beam comes out of the thumb's side (the grip you take it in), on top of In Hand Forward/Up. The other hand's is the mirror image."),
        slider("Low Grip Towards Palm", vr_flashlight_low_y, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Low Grip Up", vr_flashlight_low_z, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Low Grip Pitch", vr_flashlight_low_pitch, -180.f, 180.f, 5.f, "%.0f deg").help("Tilts the beam up (positive) or down in the hand."),
        slider("Low Grip Yaw", vr_flashlight_low_yaw, -180.f, 180.f, 5.f, "%.0f deg").help("Turns the beam towards your palm (positive) or away."),
        slider("Low Grip Roll", vr_flashlight_low_roll, -180.f, 180.f, 5.f, "%.0f deg"),
    };
    list.pushBack(header("Low Grip: Fingers on the Torch"));
    flashlightFingers(list,
        {vr_flashlight_low_fingers, vr_flashlight_low_overlap, vr_flashlight_low_curl_thumb, vr_flashlight_low_thumb_across,
            vr_flashlight_low_curl_index, vr_flashlight_low_curl_middle, vr_flashlight_low_curl_ring, vr_flashlight_low_curl_pinky,
            vr_flashlight_low_bias_thumb, vr_flashlight_low_bias_index, vr_flashlight_low_bias_middle, vr_flashlight_low_bias_ring,
            vr_flashlight_low_bias_pinky, vr_flashlight_low_thumb_x, vr_flashlight_low_thumb_y, vr_flashlight_low_thumb_z},
        flashlightPageManual[0]);
    return list;
}

// Split from Flashlight: the overhead grip (turned over with B or Y) and its fingers.
[[nodiscard]] za::Vector<Item> pageFlashlightOverheadGrip()
{
    za::Vector<Item> list = {
        header("In the Hand: Overhead Grip"),
        slider("Overhead Grip Forward", vr_flashlight_high_x, -30.f, 30.f, 0.5f, "%.1f cm").extend()
            .help("The same when B or Y has turned it over (the beam out of the little finger's side)."),
        slider("Overhead Grip Towards Palm", vr_flashlight_high_y, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Overhead Grip Up", vr_flashlight_high_z, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Overhead Grip Pitch", vr_flashlight_high_pitch, -180.f, 180.f, 5.f, "%.0f deg"),
        slider("Overhead Grip Yaw", vr_flashlight_high_yaw, -180.f, 180.f, 5.f, "%.0f deg"),
        slider("Overhead Grip Roll", vr_flashlight_high_roll, -180.f, 180.f, 5.f, "%.0f deg"),
    };
    list.pushBack(header("Overhead Grip: Fingers on the Torch"));
    flashlightFingers(list,
        {vr_flashlight_high_fingers, vr_flashlight_high_overlap, vr_flashlight_high_curl_thumb, vr_flashlight_high_thumb_across,
            vr_flashlight_high_curl_index, vr_flashlight_high_curl_middle, vr_flashlight_high_curl_ring, vr_flashlight_high_curl_pinky,
            vr_flashlight_high_bias_thumb, vr_flashlight_high_bias_index, vr_flashlight_high_bias_middle, vr_flashlight_high_bias_ring,
            vr_flashlight_high_bias_pinky, vr_flashlight_high_thumb_x, vr_flashlight_high_thumb_y, vr_flashlight_high_thumb_z},
        flashlightPageManual[1]);
    return list;
}

// Split from Flashlight: the torch clipped on a gun or on your head, and the zones that clip it on.
[[nodiscard]] za::Vector<Item> pageFlashlightMounts()
{
    return {
        header("Reach Zones"),
        toggle("Show Flashlight Zones", vr_show_flashlight_zones)
            .help("Draws where holding the torch clips it on (yellow balls at your temples and forehead, an orange ball on "
                  "each gun, where the torch sits on it) and the held torch's middle (white), which they measure. Green: in reach, B or Y clips it on (on "
                  "your head: a hand there takes it off). The torch not in a hand: a capsule round it, green while a "
                  "hand's grip there takes it, and a dot where the game reads each hand near it (the hand may be drawn "
                  "elsewhere)."),
        header("On a Gun"),
        slider("On Gun Forward", vr_flashlight_gun_forward, -0.4f, 0.3f, 0.005f, "%.3f m").extend()
            .help("Where the torch sits once clipped on a gun: along the barrel, under it (or beside a bulky gun). B or Y at it "
                  "takes it off."),
        slider("On Gun Up", vr_flashlight_gun_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("On Gun Out", vr_flashlight_gun_out, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Away from your body."),
        slider("Gun Zone Along", vr_flashlight_gun_zone_forward, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Where the torch held by the gun in your other hand lets B or Y (or letting go) clip it on: a ball round the "
                  "middle of the torch where it would sit on that gun (On Gun, and the weapon's own place: Weapon Offsets > "
                  "Flashlight), moved along the gun (forward), up and out. Not elsewhere along the gun (its butt)."),
        slider("Gun Zone Up", vr_flashlight_gun_zone_up, -0.2f, 0.2f, 0.005f, "%.3f m").extend(),
        slider("Gun Zone Out", vr_flashlight_gun_zone_out, -0.2f, 0.2f, 0.005f, "%.3f m").extend().help("Away from your body."),
        slider("Gun Zone Radius", vr_flashlight_gun_zone_radius, 0.02f, 0.3f, 0.005f, "%.3f m").extend()
            .help("How far from that place the held torch's middle may be (times Gun Clip Range)."),
        header("On the Head"),
        slider("On Head Forward", vr_flashlight_head_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend()
            .help("Where the torch sits once clipped on your head, lighting where you look."),
        slider("On Head Up", vr_flashlight_head_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("On Head Out", vr_flashlight_head_out, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Away from your head, to the side."),
        slider("Head Zone Forward", vr_flashlight_head_zone_forward, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Where the torch held at your head lets B or Y clip it on, and where a hand takes it off (B or Y, or the grip): "
                  "balls at your temples and forehead, moved forward, up and out. Apart from where it then sits (On Head)."),
        slider("Head Zone Up", vr_flashlight_head_zone_up, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("The default, 0.040, is 4 cm higher than the first zone."),
        slider("Head Zone Out", vr_flashlight_head_zone_out, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Away from your head: to the side at the temples, ahead at the forehead."),
        slider("Head Zone Radius", vr_flashlight_head_zone_radius, 0.02f, 0.3f, 0.005f, "%.3f m").extend()
            .help("How far from those places the torch's middle (or the fist taking it off) may be."),
    };
}

// Small gibs (QC vr_smallgibs.qc; ROUND21.md, "Small gibs"): chunks torn out by hits, carried and thrown as the gibs.
[[nodiscard]] za::Vector<Item> pageSmallGibsEnemies(); // (below: each monster's counts)
[[nodiscard]] za::Vector<Item> pageSmallGibs()
{
    return {
        toggle("Small Gibs", vr_smallgibs)
            .help("Hits tear small chunks of meat out of monsters and corpses: the gibs' models, a rock's size or half of it. They bleed, can be picked up and thrown, and fade away after a while."),
        toggle("From You Too", vr_smallgibs_player).help("Hits on you tear them out of you as well."),
        header("When"),
        slider("Least Damage", vr_smallgibs_min_damage, 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("A hit with less damage tears none out (a shotgun blast's pellets count as one hit). Above it the chance rises."),
        slider("Damage for a Sure One", vr_smallgibs_full_damage, 1.f, 200.f, 5.f, "%.0f").extend(1.f, 1000.f)
            .help("From this damage a hit always tears some out (times the weapon's chance below). Quad Damage, the chainsaw, gibbing and bursting a large gib always do."),
        slider("Chance Curve", vr_smallgibs_curve, 0.5f, 3.f, 0.1f, "%.1f")
            .help("How the chance rises between the two: 1 straight, more makes light hits rarer (a plain shotgun blast) while hard ones stay sure."),
        slider("One More Each", vr_smallgibs_damage_per_gib, 5.f, 200.f, 5.f, "%.0f damage").extend(1.f, 1000.f)
            .help("Harder hits tear more out: one more for about each this much damage."),
        slider("Most From a Hit", vr_smallgibs_per_hit, 1.f, 10.f, 1.f, "%.0f"),
        header("Chance by Weapon"),
        slider("Shotguns", vr_smallgibs_shots, 0.f, 3.f, 0.1f, "%.1fx")
            .help("The shotgun's blast rarely tears one out, the double-barrelled one's often (their pellets' damage summed). A grunt's gun too."),
        slider("Nails", vr_smallgibs_nails, 0.f, 3.f, 0.1f, "%.1fx").help("Nails and lasers (0: never, but under Quad Damage)."),
        slider("Blades", vr_smallgibs_blades, 0.f, 3.f, 0.1f, "%.1fx").help("The axe's and a sword's edge or point, swung or thrown fast."),
        slider("Blunt Blows", vr_smallgibs_blunt, 0.f, 3.f, 0.1f, "%.1fx")
            .help("Punches, a pommel or hilt, a gun or the crowbar swung, Mjolnir, bashes and headbutts: only strong ones, rarely."),
        slider("Props", vr_smallgibs_props, 0.f, 3.f, 0.1f, "%.1fx").help("A rock, a brick or another thing thrown hard or swung in the fist."),
        slider("Explosions", vr_smallgibs_explosions, 0.f, 3.f, 0.1f, "%.1fx"),
        slider("Everything Else", vr_smallgibs_other, 0.f, 3.f, 0.1f, "%.1fx").help("Lightning, monsters' blows and the rest."),
        slider("Chainsaw: One Each", vr_smallgibs_saw_interval, 0.05f, 1.f, 0.05f, "%.2f s").help("The running chain tears one out of a monster or a corpse this often."),
        slider("With a Gibbing", vr_smallgibs_gibbing, 0.f, 20.f, 1.f, "%.0f").help("How many fly with a body's gibs when it is gibbed."),
        slider("A Large Gib Bursts Into", vr_smallgibs_burst, 0.f, 10.f, 1.f, "%.0f").help("How many a gib breaks into when it is destroyed (a head half as many again)."),
        header("Brain Chunks"),
        toggle("Brain Chunks", vr_smallgibs_brains)
            .help("A head that bursts (a head gib destroyed, or a head popped by a headshot) throws torn lumps of brain too: small gibs in all else (picked up, thrown, bleeding, fading), lighter."),
        slider("From a Head Gib", vr_smallgibs_brains_burst, 0.f, 20.f, 1.f, "%.0f").help("How many when a head gib is destroyed (with its small gibs of meat)."),
        slider("From a Head Pop", vr_smallgibs_brains_pop, 0.f, 20.f, 1.f, "%.0f").help("How many when a headshot pops a head (Decapitation > Head Shots)."),
        slider("Brain Chunk Size", vr_smallgibs_brains_size, 0.2f, 2.f, 0.05f, "%.2fx").help("Times the small gibs' size (Smallest, Largest)."),
        slider("Brain Chunk Mass", vr_smallgibs_brains_mass, 0.05f, 2.f, 0.05f, "%.2f kg"),
        slider("Brain Chunk Speed", vr_smallgibs_brains_speed, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("How fast brain chunks fly out of a head burst: their own, not Speed below (times A Large Gib Bursts: "
                  "Speed). The default is Speed's, which is what they used then (vr_smallgibs_brains_speed)."),
        slider("Brain Chunk Up", vr_smallgibs_brains_up, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("And upwards: their own, not Up below (times A Large Gib Bursts: Up) (vr_smallgibs_brains_up)."),
        header("Flight and Size"),
        slider("Speed", vr_smallgibs_speed, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f).help("How fast they fly out from where the hit landed (times Flight by Situation below)."),
        slider("Up", vr_smallgibs_up, 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f).help("And upwards (times Flight by Situation below)."),
        slider("Pass Through the Body", vr_smallgibs_grace, 0.f, 0.5f, 0.05f, "%.2f s").help("For this long they don't collide with the body they came from."),
        toggle("Not Pushed Out of Bodies", vr_smallgibs_pass_inside)
            .help("Still inside a monster, you, a hand or a blade after that (melee, the chainsaw), they pass through it until clear, not pushed out at 2 to 3 m/s."),
        slider("Not Batted Away For", vr_smallgibs_blow_grace, 0.f, 1.f, 0.05f, "%.2f s")
            .help("For this long after it is torn out, your hands, weapons and what they hold pass through a small gib: the blade that tore it out doesn't bat it away. After it you can bat it."),
        toggle("Burst by Blows Meanwhile", vr_smallgibs_blow_burst)
            .help("In that time a blow still bursts one (the blade that tore it out too, going on through it), with Can Be Destroyed on; it is never batted or pushed. Off: blows pass through it."),
        slider("Smallest", vr_smallgibs_size_min, 0.1f, 2.f, 0.05f, "%.2fx rock"),
        slider("Largest", vr_smallgibs_size_max, 0.1f, 2.f, 0.05f, "%.2fx rock"),
        slider("Mass", vr_smallgibs_mass, 0.05f, 5.f, 0.05f, "%.2f kg").help("Light: thrown, they hurt little."),
        header("Flight by Situation"),
        slider("Melee: Speed", vr_smallgibs_speed_melee, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A hand's blow: a blade, a punch, a pommel or hilt, a rock or brick in the fist; bashes and headbutts."),
        slider("Melee: Up", vr_smallgibs_up_melee, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Chainsaw: Speed", vr_smallgibs_speed_saw, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. The chainsaw's running chain or a swing of it."),
        slider("Chainsaw: Up", vr_smallgibs_up_saw, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Guns: Speed", vr_smallgibs_speed_guns, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. Shotguns, nails, lasers, lightning, a grunt's gun."),
        slider("Guns: Up", vr_smallgibs_up_guns, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Explosions: Speed", vr_smallgibs_speed_explosions, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. Rockets, grenades, every blast."),
        slider("Explosions: Up", vr_smallgibs_up_explosions, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Thrown: Speed", vr_smallgibs_speed_thrown, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A thrown axe, sword, rock, brick or gib; a projectile batted back."),
        slider("Thrown: Up", vr_smallgibs_up_thrown, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Everything Else: Speed", vr_smallgibs_speed_other, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. Monsters' blows and the rest."),
        slider("Everything Else: Up", vr_smallgibs_up_other, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Gibbing a Live One: Speed", vr_smallgibs_speed_gibbing, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A living monster gibbed: they fly out of its whole body, with its gibs' push."),
        slider("Gibbing a Live One: Up", vr_smallgibs_up_gibbing, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("Gibbing a Corpse: Speed", vr_smallgibs_speed_corpse, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A corpse gibbed where it lies (struck, shot, blown up)."),
        slider("Gibbing a Corpse: Up", vr_smallgibs_up_corpse, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        slider("A Large Gib Bursts: Speed", vr_smallgibs_speed_burst, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f)
            .help("Speed times this. A gib or head destroyed breaks into them."),
        slider("A Large Gib Bursts: Up", vr_smallgibs_up_burst, 0.f, 3.f, 0.01f, "%.2fx").extend(0.f, 10.f).help("Up times this."),
        header("How Many and How Long"),
        slider("Most Lying About", vr_smallgibs_max, 1.f, 128.f, 1.f, "%.0f").extend(1.f, 256.f).help("Past it, the oldest go first (not one in your hand)."),
        slider("Last", vr_smallgibs_time, 0.f, 120.f, 5.f, "%.0f s").help("Then they fade away (0: never). Held, and in the air after, they wait."),
        toggle("Can Be Destroyed", vr_smallgibs_destroy).help("Shots and blows burst them into a puff of blood. Off: they pass through them."),
        open("Per Enemy", pageIndex(pageSmallGibsEnemies))
            .help("Each monster's own counts: a hit's, a gibbing's and a head or gib burst's, times these."),
    };
}

// Small Gibs > Per Enemy (QC vr_smallgibs.qc's VR_SmallGib_Mult; the same monsters as Corpse Damage and Health's
// Corpse Health, by Monster): each monster's small gibs and brain chunks, times these.
[[nodiscard]] za::Vector<Item> pageSmallGibsEnemies()
{
    return {
        open("Small Gibs, Per Enemy: Small Gibs", pageIndex(pageSmallGibs)).help("The counts themselves, the flight and Brain Chunks are on Small Gibs."),
        header("Small Gibs, by Monster"),
        slider("Grunt", vr_smallgibs_mult_grunt, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Its small gibs, times this: what a hit tears out, what flies with its gibs, what its gibs and head burst into (vr_smallgibs_mult_grunt)."),
        slider("Enforcer", vr_smallgibs_mult_enforcer, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An enforcer's (vr_smallgibs_mult_enforcer)."),
        slider("Rottweiler", vr_smallgibs_mult_dog, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rottweiler's (vr_smallgibs_mult_dog)."),
        slider("Fiend", vr_smallgibs_mult_fiend, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A fiend's (vr_smallgibs_mult_fiend)."),
        slider("Ogre", vr_smallgibs_mult_ogre, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An ogre's (vr_smallgibs_mult_ogre)."),
        slider("Knight", vr_smallgibs_mult_knight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A knight's (vr_smallgibs_mult_knight)."),
        slider("Hell Knight", vr_smallgibs_mult_hellknight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A hell knight's (vr_smallgibs_mult_hellknight)."),
        slider("Vore", vr_smallgibs_mult_vore, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A vore's (vr_smallgibs_mult_vore)."),
        slider("Shambler", vr_smallgibs_mult_shambler, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A shambler's (vr_smallgibs_mult_shambler)."),
        slider("Scrag", vr_smallgibs_mult_scrag, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A scrag's (vr_smallgibs_mult_scrag)."),
        slider("Rotfish", vr_smallgibs_mult_fish, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rotfish's (vr_smallgibs_mult_fish)."),
        slider("Gremlin (Hipnotic)", vr_smallgibs_mult_gremlin, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A gremlin's (vr_smallgibs_mult_gremlin)."),
        slider("Centroid (Rogue)", vr_smallgibs_mult_scourge, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A centroid's (vr_smallgibs_mult_scourge)."),
        slider("Eel (Rogue)", vr_smallgibs_mult_eel, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An eel's (vr_smallgibs_mult_eel)."),
        slider("Zombie (Beheaded)", vr_smallgibs_mult_zombie, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A zombie's, beheaded or not (vr_smallgibs_mult_zombie)."),
        slider("Mummy (Rogue, Beheaded)", vr_smallgibs_mult_mummy, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A mummy's (vr_smallgibs_mult_mummy)."),
        header("Brain Chunks, by Monster"),
        slider("Grunt", vr_smallgibs_brains_mult_grunt, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Its brain chunks, times this: from its head gib destroyed or its head popped by a headshot. A head it "
                  "lost carries them with it (vr_smallgibs_brains_mult_grunt)."),
        slider("Enforcer", vr_smallgibs_brains_mult_enforcer, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An enforcer's (vr_smallgibs_brains_mult_enforcer)."),
        slider("Rottweiler", vr_smallgibs_brains_mult_dog, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rottweiler's (vr_smallgibs_brains_mult_dog)."),
        slider("Fiend", vr_smallgibs_brains_mult_fiend, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A fiend's (vr_smallgibs_brains_mult_fiend)."),
        slider("Ogre", vr_smallgibs_brains_mult_ogre, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An ogre's (vr_smallgibs_brains_mult_ogre)."),
        slider("Knight", vr_smallgibs_brains_mult_knight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A knight's (vr_smallgibs_brains_mult_knight)."),
        slider("Hell Knight", vr_smallgibs_brains_mult_hellknight, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A hell knight's (vr_smallgibs_brains_mult_hellknight)."),
        slider("Vore", vr_smallgibs_brains_mult_vore, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A vore's (vr_smallgibs_brains_mult_vore)."),
        slider("Shambler", vr_smallgibs_brains_mult_shambler, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A shambler's (vr_smallgibs_brains_mult_shambler)."),
        slider("Scrag", vr_smallgibs_brains_mult_scrag, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A scrag's (vr_smallgibs_brains_mult_scrag)."),
        slider("Rotfish", vr_smallgibs_brains_mult_fish, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A rotfish's (vr_smallgibs_brains_mult_fish)."),
        slider("Gremlin (Hipnotic)", vr_smallgibs_brains_mult_gremlin, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A gremlin's (vr_smallgibs_brains_mult_gremlin)."),
        slider("Centroid (Rogue)", vr_smallgibs_brains_mult_scourge, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A centroid's (vr_smallgibs_brains_mult_scourge)."),
        slider("Eel (Rogue)", vr_smallgibs_brains_mult_eel, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("An eel's (vr_smallgibs_brains_mult_eel)."),
        slider("Zombie (Beheaded)", vr_smallgibs_brains_mult_zombie, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A zombie's (vr_smallgibs_brains_mult_zombie)."),
        slider("Mummy (Rogue, Beheaded)", vr_smallgibs_brains_mult_mummy, 0.f, 4.f, 0.25f, "%.2fx").extend(0.f, 20.f).help("A mummy's (vr_smallgibs_brains_mult_mummy)."),
    };
}

// Gore > Decapitation (ROUND21.md, "Decapitation"; QC vr_decap.qc, vr_box3d.cpp's "Decapitation"): when a blow cuts a
// head off, how the head flies, the stump's fountain.
[[nodiscard]] za::Vector<Item> pageDecapitation()
{
    return {
        toggle("Decapitation", vr_decap)
            .help("A killing slash at the head of a grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler, gremlin or mummy cuts it "
                  "off: a sword's or an axe's blade swung across (not a stab, not the pommel or the hilt). The body falls at "
                  "once as a headless ragdoll, the head flies off, the neck spurts blood. Needs Ragdolls on (vr_decap)."),
        slider("Least Swing Speed", vr_decap_speed, 0.f, 15.f, 0.5f, "%.1f m/s").extend(0.f, 30.f)
            .help("How fast the blade must move where it strikes (and a thrown axe fly) to cut the head off (vr_decap_speed)."),
        slider("Least Slash Angle", vr_decap_slash_angle, 0.f, 90.f, 5.f, "%.0f deg")
            .help("How far off the blade's line the struck point must move to cut (nearer its line it is a stab, which "
                  "doesn't behead); 0: any motion (vr_decap_slash_angle)."),
        slider("Head Zone Size", vr_decap_head_size, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.1f, 4.f)
            .help("Melee only (a slash, the chainsaw, a corpse's head): how big the head zone a blade must strike to behead is, "
                  "times its size for headshots. Debug > Show Hit Zones draws it (vr_decap_head_size)."),
        slider("Neck", vr_decap_neck, 0.f, 20.f, 1.f, "%.0f units").extend(0.f, 40.f)
            .help("Melee only: the head zone reaches this far down below the head's middle, over the neck (a ragdoll's neck "
                  "at least this far round it) (vr_decap_neck)."),
        slider("Head Speed", vr_decap_head_speed, 0.f, 1.5f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("How much of the blade's speed the head flies off with; it spins as the swing turns it "
                  "(vr_decap_head_speed)."),
        slider("Head Thrown Up", vr_decap_head_lift, 0.f, 400.f, 10.f, "%.0f u/s").extend(0.f, 1000.f)
            .help("And how fast it is thrown upwards besides (vr_decap_head_lift)."),
        slider("Fountain", vr_decap_fountain, 0.f, 8.f, 0.25f, "%.2f s").extend(0.f, 30.f)
            .help("How long the neck spurts blood, in beats, dying away (0: none) (vr_decap_fountain)."),
        slider("Flying Head's Fountain", vr_limbs_end_fountain, 0.f, 4.f, 0.25f, "%.2f s").extend(0.f, 20.f)
            .help("How long the neck of a head cut off spurts a smaller fountain as it flies (and the cut end of a limb: "
                  "Limb Gore's Cut End Fountain, the same setting) (0: none) (vr_limbs_end_fountain)."),
        toggle("Keeps Its Own Motion", vr_decap_own_motion)
            .help("A beheaded (or popped) monster's headless body goes on as it was moving (running at you: it stumbles on "
                  "towards you) in full; a pop slows only the shot's knock (Body's Speed After a Pop). Off: as before: its "
                  "run not carried, a pop slowing all of it (vr_decap_own_motion)."),
        header("What Beheads"),
        toggle("Corpses", vr_decap_corpses)
            .help("A slash at a ragdoll's (or a dying body's) head cuts it off instead of hurting the corpse: it never "
                  "bursts into gibs from it (vr_decap_corpses)."),
        toggle("Zombies", vr_decap_zombies)
            .help("A slash at a zombie's head cuts it off whatever the damage: it falls as a headless ragdoll and never "
                  "gets up again. Besides gibbing it, the only way to kill one (vr_decap_zombies)."),
        toggle("Thrown Axes", vr_decap_thrown)
            .help("An axe thrown into a head edge first (as it sticks) cuts it off when it kills (vr_decap_thrown)."),
        toggle("Chainsaw", vr_decap_chainsaw)
            .help("The chainsaw's running bar at the neck (or a swing of it) cuts the head off when it kills; a corpse's "
                  "at once (vr_decap_chainsaw)."),
        header("Head Shots"),
        toggle("Shotgun", vr_decap_shotgun)
            .help("A shotgun blast that kills with a headshot pops the head: it bursts in blood and small gibs, and the body "
                  "falls at once as a headless ragdoll (never gibbed, nor its death animation) (vr_decap_shotgun)."),
        toggle("Super Shotgun", vr_decap_super_shotgun)
            .help("The same for a super shotgun blast (vr_decap_super_shotgun)."),
        toggle("Lightning Gun", vr_decap_lightning)
            .help("The same for a lightning gun bolt at the head that kills (vr_decap_lightning)."),
        slider("Blast's Head Share", vr_decap_head_share, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of a blast's damage on him must come from pellets at the head (the headshot multiplier in) for "
                  "it to count as a headshot; 0: any pellet at the head (vr_decap_head_share)."),
        slider("Body's Speed After a Pop", vr_decap_pop_body_speed, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("A popped head's body: how much of the shot's knock its headless ragdoll keeps (its own motion, as it ran, "
                  "in full: Keeps Its Own Motion). The shot's force went into the head, so it slumps where it stood; 1: flung "
                  "as before (vr_decap_pop_body_speed)."),
        slider("Head Chance", vr_decap_chance_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Every beheading or head pop's chance times this (a blade's sure cut too: below 1 it may fail); Quad Damage "
                  "always pops. Limbs: Gore > Limb Gore (vr_decap_chance_scale)."),
        header("Head Pop Chance"),
        toggle("By Chance", vr_decap_pop_chance)
            .help("A shotgun or super shotgun headshot kill pops the head always up close, never far off, and by chance between "
                  "(the rows below); off: every headshot kill pops it, as before (vr_decap_pop_chance)."),
        slider("Always Within", vr_decap_pop_always_range, 0.f, 10.f, 0.5f, "%.1f lengths").extend(0.f, 50.f)
            .help("A shotgun's or super shotgun's headshot kill this near (in player lengths, 56 units: about 1.75 m) "
                  "always pops the head (vr_decap_pop_always_range)."),
        slider("Never Beyond", vr_decap_pop_never_range, 1.f, 40.f, 1.f, "%.0f lengths").extend(0.f, 200.f)
            .help("This far or farther it never does; between the two, the chance falls off (vr_decap_pop_never_range)."),
        slider("Super Shotgun Chance", vr_decap_pop_ssg_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The super shotgun's chance between the ranges, times this (vr_decap_pop_ssg_scale)."),
        slider("Super Shotgun Falloff", vr_decap_pop_ssg_falloff, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("How fast its chance dies away with range: 1 straight down to the far range, higher sooner, 0 not at all "
                  "(vr_decap_pop_ssg_falloff)."),
        slider("Shotgun Chance", vr_decap_pop_sg_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The shotgun's chance between the ranges, times this (vr_decap_pop_sg_scale)."),
        slider("Shotgun Falloff", vr_decap_pop_sg_falloff, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("How fast the shotgun's chance dies away with range (stronger than the super shotgun's) "
                  "(vr_decap_pop_sg_falloff)."),
        slider("Pellets at the Head", vr_decap_pop_pellet_weight, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the share of the blast's pellets that struck the head weighs on the chance: 1 in proportion, "
                  "0 not at all (vr_decap_pop_pellet_weight)."),
        toggle("Lightning Always Pops", vr_decap_pop_lightning_always)
            .help("A lightning bolt's headshot kill always pops the head, at any range; off: the ranges, with the super "
                  "shotgun's falloff (vr_decap_pop_lightning_always)."),
        slider("Enforcer Laser", vr_decap_pop_laser, 0.f, 1.f, 0.05f, "%.2f")
            .help("An enforcer's laser bolt that kills a monster with a head or limb hit (an enemy's bolt into another "
                  "monster, or your own enforcer's rifle) pops it at this chance, at any range (0: never) (vr_decap_pop_laser)."),
        toggle("Thrown Things", vr_decap_pop_thrown)
            .help("A blunt weapon or prop thrown (or flung) into a head that kills pops it, when heavy enough (not a sword, "
                  "an axe or the chainsaw: an axe's edge cuts the head off) (vr_decap_pop_thrown)."),
        slider("Thrown: Always From", vr_decap_pop_thrown_mass, 0.f, 20.f, 0.5f, "%.1f kg").extend(0.f, 200.f)
            .help("This heavy or heavier, a thrown thing's headshot kill always pops the head (vr_decap_pop_thrown_mass)."),
        slider("Thrown: Lighter's Chance", vr_decap_pop_thrown_light_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("A lighter one's chance (0: never) (vr_decap_pop_thrown_light_chance)."),
        slider("Fist", vr_decap_pop_fist_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("A punch that kills with a head hit pops the head at this chance at the hardest hit, times the hit's "
                  "hardness to the Hardness Curve (the rows below): very rarely by default (vr_decap_pop_fist_scale)."),
        slider("Gun Butt", vr_decap_pop_gun_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("The same for a gun swung, its butt or a pistol-whip (vr_decap_pop_gun_scale)."),
        slider("Crowbar", vr_decap_pop_crowbar_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("The same for the crowbar (vr_decap_pop_crowbar_scale)."),
        slider("Pommel", vr_decap_pop_pommel_scale, 0.f, 0.25f, 0.005f, "%.3fx").extend(0.f, 5.f)
            .help("The same for a sword's, axe's, chainsaw's or Mjolnir's pommel or handle end (vr_decap_pop_pommel_scale)."),
        slider("Club", vr_decap_pop_club_scale, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("The same for a carried prop swung as a club, a wall torch (vr_decap_pop_club_scale)."),
        slider("Mjolnir", vr_decap_pop_mjolnir_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The same for Mjolnir's head: above 1 it is sure well before the hardest hit (1.75: a solid blow nearly "
                  "always pops it) (vr_decap_pop_mjolnir_scale)."),
        slider("Soft Hit Speed", vr_decap_pop_melee_soft_speed, 0.f, 10.f, 0.5f, "%.1f m/s").extend(0.f, 30.f)
            .help("The striking part this slow or slower: no hardness from its speed (vr_decap_pop_melee_soft_speed)."),
        slider("Hard Hit Speed", vr_decap_pop_melee_hard_speed, 1.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 40.f)
            .help("This fast or faster: all of it (vr_decap_pop_melee_hard_speed)."),
        slider("Soft Hit Damage", vr_decap_pop_melee_soft_damage, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("The blow's damage (the headshot multiplier in) this or less: no hardness from it "
                  "(vr_decap_pop_melee_soft_damage)."),
        slider("Hard Hit Damage", vr_decap_pop_melee_hard_damage, 1.f, 200.f, 1.f, "%.0f").extend(0.f, 1000.f)
            .help("This or more: all of it (vr_decap_pop_melee_hard_damage)."),
        slider("Damage's Weight", vr_decap_pop_melee_damage_weight, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the damage weighs in the hit's hardness; the rest is its speed (vr_decap_pop_melee_damage_weight)."),
        slider("Hardness Curve", vr_decap_pop_melee_curve, 0.f, 5.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("The hardness to this power: higher, soft blows nearer never; 1 straight (vr_decap_pop_melee_curve)."),
        toggle("Quad Damage: Always Pop", vr_decap_pop_quad)
            .help("With Quad Damage, every headshot kill pops the head: any gun (nails, rockets and grenades at the head too), "
                  "any blow (a blade's stab), any throw or prop, at any range, by no chance (a slash still cuts it off) "
                  "(vr_decap_pop_quad)."),
    };
}

// Gore > Limb Gore (ROUND21.md, "Limb gore"; QC vr_limbs.qc, vr_box3d.cpp's "Limb gore", vr_limbmodel.cpp): every
// limb as the head: cut off or popped, by the head's chance rules.
[[nodiscard]] za::Vector<Item> pageLimbGore()
{
    return {
        toggle("Limb Gore", vr_limbs)
            .help("A monster's arms, legs, tails and the rest come off as its head does (the monsters with ragdolls): a blade's "
                  "slash cuts the limb off at the joint nearest the hit (the elbow the forearm, the shoulder the whole arm), "
                  "a shot, a blunt blow or a bolt pops it, by the head's chances (Decapitation's rows). A living one only by "
                  "the blow that kills it: it falls at once as a ragdoll; the limb flies off (pick it up, throw it), the "
                  "stump spurts blood. Needs Ragdolls on (vr_limbs)."),
        slider("Limb Chance", vr_limbs_chance_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Every limb cut or pop's chance times this (a blade's sure cut too: below 1 it may fail); Quad Damage "
                  "always pops (vr_limbs_chance_scale)."),
        slider("Head Chance", vr_decap_chance_scale, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("And every beheading or head pop's chance times this (1: as before) (vr_decap_chance_scale)."),
        toggle("Corpses", vr_limbs_corpses)
            .help("Corpses and ragdolls lose limbs too, each on its own, down to the torso: a slash cuts, a blunt blow or a "
                  "shot pops one (vr_limbs_corpses)."),
        toggle("Zombies and Mummies", vr_limbs_zombies)
            .help("A zombie or a mummy losing a limb dies for good (a zombie: whatever the damage, as beheaded). Off: their "
                  "limbs stay on (their heads: Decapitation > Zombies) (vr_limbs_zombies)."),
        slider("Body's Speed After", vr_limbs_body_speed, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("A living monster's limb cut or popped: how much of the blow's knock its ragdoll keeps (it slumps where it "
                  "stood; 1: flung as a kill) (vr_limbs_body_speed)."),
        slider("Most Limbs Lying About", vr_limbs_max, 1.f, 64.f, 1.f, "%.0f").extend(1.f, 256.f)
            .help("Past it, the oldest go first (not one in your hand) (vr_limbs_max)."),
        cycle("Make Limbs as the Map Loads", "vr_limbs_prebuild", {{0.f, "Off"}, {1.f, "The Map's"}, {2.f, "And the Range's"}})
            .help("The Map's: the limbs of every kind of monster the map has (those waiting to appear too) are made as it "
                  "loads (about 5 ms each: a tenth of a second or so more), not at their first cut (a dropped frame). And the "
                  "Range's: also every kind the firing range's dispensers and dummies can make (half a second more there, "
                  "once a session). The next map load (vr_limbs_prebuild)."),
        slider("Limb Weight", vr_limbs_mass_scale, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A limb (or head) cut off weighs its share of its monster's ragdoll Mass by the kind of limb: a whole arm "
                  "6.3%, a forearm and hand 2.8%, a hand 0.8%, a whole leg 15.5%, a shin and foot 6%, the head 7% (four "
                  "legs share two legs' weight, six arms two arms'; a tail its share of the body's volume), times this; "
                  "held and thrown as that. 0: its model's volume of flesh, as before (vr_limbs_mass_scale)."),
        slider("Cut End Fountain", vr_limbs_end_fountain, 0.f, 4.f, 0.25f, "%.2f s").extend(0.f, 20.f)
            .help("How long the cut end of a limb (or a head's neck) spurts a smaller fountain of its own as it flies, "
                  "dying away (the stump's: Decapitation > Fountain) (0: none) (vr_limbs_end_fountain)."),
        slider("Enforcer Laser Pops", vr_decap_pop_laser, 0.f, 1.f, 0.05f, "%.2f")
            .help("An enforcer's laser bolt that kills with a head or limb hit (an enemy's into another monster, or your "
                  "enforcer's rifle) pops it at this chance (times Head or Limb Chance) (vr_decap_pop_laser)."),
        header("Explosions and Gibbing"),
        toggle("Explosions Pop Limbs", vr_limbs_blast)
            .help("An explosion pops the limbs near it by chance: a monster it kills falls as a ragdoll without them instead "
                  "of bursting into gibs (if none popped: gibbed as before); corpses lose them too. Off: as before "
                  "(vr_limbs_blast)."),
        slider("Explosion Reach", vr_limbs_blast_radius, 16.f, 256.f, 8.f, "%.0f units").extend(1.f, 1000.f)
            .help("Limbs this near the blast may pop (vr_limbs_blast_radius)."),
        slider("Explosion Chance", vr_limbs_blast_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("A limb's chance at the blast itself, falling to none at the reach (times Limb Chance) "
                  "(vr_limbs_blast_chance)."),
        cycle("Gibbed Bodies Throw Limbs", vr_gib_limbs, {{0.f, "No"}, {1.f, "With the Gibs"}, {2.f, "Instead of Gibs"}})
            .help("A body bursting into gibs throws its own arms, legs and tail as well (With the Gibs), or instead of "
                  "Quake's meat chunks (Instead of Gibs) (vr_gib_limbs)."),
    };
}

// Gore (vr_gore.cpp, vr_decals.cpp, vr_bodyblood.cpp; the QC's gibs sticking: vr_carry.qc).
[[nodiscard]] za::Vector<Item> pageGore()
{
    return {
        cycle("Gore", vr_gore, {{0.f, "Quake VR"}, {1.f, "More"}, {2.f, "Over the top"}})
            .help("Over the top: hits spray blood onto the walls, floor and ceiling behind, gibbing paints the room, pools spread under corpses, gibs stick to ceilings and drip."),
        header("Hits, Gibs and Corpses"),
        slider("Blood Sprays", vr_gore_spray, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("How many splats a hit or a gibbing throws onto the walls, floor and ceiling round it (0 none)."),
        slider("Splat Size", vr_gore_size, 0.5f, 2.f, 0.1f, "%.1fx").extend().help("How big the gore's splats, pools and runs are."),
        slider("Blood Pools", vr_gore_pools, 0.f, 2.f, 0.1f, "%.1fx").extend().help("Pools of blood spreading under corpses and gibs: their size (0 none)."),
        slider("Dripping", vr_gore_drips, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Blood dripping from splats on the ceiling and from gibs stuck there: how long and how much (0 none)."),
        slider("Gibs Stick", vr_gore_stick, 0.f, 30.f, 1.f, "%.0f s").extend()
            .help("Gibs flung into a ceiling or a wall may stick there about this long, dripping, then fall (0 never)."),
        slider("Thrown Gibs Stick", vr_gore_stick_thrown, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a gib or head (or a small gib) you throw into a wall or a ceiling sticks there (faster than Gib Splat Speed it bursts instead)."),
        slider("Speed to Stick", vr_gore_stick_speed, 50.f, 500.f, 10.f, "%.0f u/s").extend(20.f, 1000.f)
            .help("How fast a gib must strike a wall or a ceiling to stick (flung from a body or thrown)."),
        toggle("Flies on Heads", vr_head_flies)
            .help("Flies buzzing round some severed heads (Scourge of Armagon's: about one head in ten). Off "
                  "by default."),
        slider("Gib Speed: Melee", vr_gib_speed_melee, 0.05f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("How fast the gibs fly when a melee blow gibs a monster or a corpse (a swing, a bash, a shove, a headbutt): "
                  "times Quake's speed."),
        slider("Gib Speed: Light Weapons", vr_gib_speed_light, 0.05f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("And when a thrown thing, the shotguns, nails or an enforcer's laser gib it."),
        slider("Gib Speed: Explosives", vr_gib_speed_heavy, 0.05f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("And everything else: explosions, rockets, grenades, lightning, plasma, monsters' blows (1: Quake's)."),
        open("Small Gibs", pageIndex(pageSmallGibs)).help("Chunks of meat torn out by hits: when, how many, how they fly, how long they last."),
        open("Decapitation", pageIndex(pageDecapitation))
            .help("A killing slash at a monster's head cuts it off: the head flies, the body falls headless, the neck spurts blood."),
        open("Limb Gore", pageIndex(pageLimbGore))
            .help("Arms, legs and tails cut off or popped as heads are: the killing blow on the living, any blow on corpses."),
        header("Wounds on Models"),
        toggle("Dynamic Wounds", vr_wounds)
            .help("Blood painted on monsters, corpses and you where the hits land, in the skins' own pixels. Your body and hands show your wounds this way instead of the wound skins, and healing washes them off."),
        toggle("Burns", vr_wounds_burns).help("Explosions, fire, lightning, lava and slime char what they hit; fresh burns glow in their cracks for a moment."),
        toggle("Wet from Liquids", vr_wounds_wet).help("Monsters and you get wet up to where water or slime came, drip, and dry in about 25 seconds."),
        cycle("Models Kept", vr_wounds_pool, {{32.f, "32 (8 MB)"}, {64.f, "64 (16 MB)"}, {128.f, "128 (32 MB)"}})
            .help("How many models keep their wounds at once: past it, the ones seen longest ago give theirs up."),
        cycle("Your Wounds' Detail", vr_wounds_own_res,
            {{0.f, "Chunky (as monsters)"}, {512.f, "Fine (5 MB)"}, {1024.f, "Finer (20 MB)"}, {2048.f, "Finest (80 MB)"}})
            .help("Your own body's and hands' wounds, burns and wetness: finer than their skin and smooth-edged, with a relief "
                  "(Chunky: in the skin's own pixels, as the monsters')."),
        slider("Your Burns' Relief", vr_wounds_bump_burns, 0.f, 3.f, 0.25f, "%.2fx")
            .help("How crusted and cracked the char on your arms looks in the light (Finer detail only; 0 flat)."),
        slider("Your Wounds' Depth", vr_wounds_bump_blood, 0.f, 3.f, 0.25f, "%.2fx")
            .help("How deep your bleeding wounds look sunk into the skin (Finer detail only; 0 flat)."),
        slider("Blood Opacity", vr_wounds_blood_alpha, 0.2f, 1.f, 0.05f, "%.2f")
            .help("How opaque the painted blood is over the skins: a little of the skin shows through below 1."),
        header("Lightning Shock"),
        toggle("Lightning Shock", vr_shock_death)
            .help("A monster the lightning strikes, alive or dead, keeps Quad Damage's arcs crawling over it a while, on through its "
                  "death; its ragdoll convulses as long as they do and the killing bolt chars it (off: only each hit's own arcs and burn)."),
        toggle("Arcs on the Living", vr_shock_living)
            .help("The lasting arcs on living monsters too, from the first bolt and on through their death. Off: only on the bodies "
                  "the lightning kills or strikes dead; a living monster gets just each hit's own short flicker of arcs."),
        slider("Lightning Shock Duration", vr_shock_death_time, 0.5f, 30.f, 0.5f, "%.1f s").extend(0.1f, 60.f)
            .help("How long the arcs (and a body's convulsions) last after the last bolt."),
        slider("On the Living", vr_shock_living_time, 0.f, 1.f, 0.05f, "%.2fx").extend(0.f, 2.f)
            .help("A living monster's arcs last this much of the Duration after the last bolt (0.5: half). Its death carries "
                  "them on to the whole Duration from that bolt, as a body's (vr_shock_living_time)."),
        slider("Convulsions", vr_shock_seizure, 0.f, 3.f, 0.1f, "%.1fx")
            .help("How hard a shocked ragdoll's limbs convulse, easing off with the arcs (0 still). Only the dead convulse."),
        slider("Crackle Volume", vr_shock_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("The electric crackle a body makes as its lasting arcs start, and again while a bolt stays on it (0 none)."),
        slider("Arcs on Bodies", vr_shock_arcs, 0.f, 3.f, 0.1f, "%.1fx").help("How many arcs crawl over a shocked monster or body (0 none)."),
        slider("Burn Marks", vr_shock_burns, 0.f, 16.f, 1.f, "%.0f").help("The burn marks a lightning kill leaves over the body (each hit also chars where it strikes: Burns)."),
        slider("Smoke After Lightning", vr_smoulder_time, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("How long a monster or a body the lightning strikes smokes from its burns after the last bolt, thinning out (0 none)."),
        slider("Smouldering Smoke", vr_smoulder, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much smoke rises off bodies the lightning struck or fire burnt (0 none; how long after fire: Combat > Burning > Smoke After Flames)."),
        slider("Smoke Opacity", vr_smoulder_alpha, 0.1f, 1.f, 0.05f, "%.2f").help("How opaque each wisp of that smoke is as it leaves the skin."),
        slider("Arcs on You", vr_shock_self_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 5.f)
            .help("Struck by lightning (a shambler's bolt, another's lightning gun, a trap, the water's shock), Quad's arcs crawl over "
                  "your hands, arms and body this long after a shambler's bolt, longer for harder ones (0 none). None right in front "
                  "of your eyes."),
        slider("Arcs on You: Number", vr_shock_self_arcs, 0.f, 3.f, 0.1f, "%.1fx").help("How many arcs crawl over you then."),
        slider("Arcs on You: Light", vr_shock_self_light, 0.f, 1.f, 0.1f, "%.1f")
            .help("A soft flicker of blue light round you while they crackle (0 none). There is no flash over the view."),
        header("Your Wounds"),
        slider("Arm Drip Rate", vr_body_blood, 0.f, 4.f, 0.25f, "%.2fx").extend()
            .help("How often blood drips from your wounded arms and hands (the body's wounds: Show Armour and Wounds; 0 none)."),
        slider("Drop Size", vr_body_blood_amount, 0.5f, 3.f, 0.25f, "%.2fx").extend().help("How big the drops are and how much they splash."),
        slider("Drips Round Feet", vr_body_blood_floor, 0.f, 4.f, 0.25f, "%.2fx").extend()
            .help("While wounded, blood drips from your body round your feet, faster when badly hurt or just hit (0 none)."),
        slider("Drops Mark Floor", vr_body_blood_marks, 0.f, 1.f, 0.05f, "%.2f").help("The chance a drop leaves a mark on the floor."),
        slider("Floor Mark Size", vr_body_blood_mark_size, 0.5f, 4.f, 0.25f, "%.2fx").extend(),
        header("Bloody Hands and Washing"),
        slider("Gib Blood on Hands", vr_gore_hands, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Taking a gib or a head bloodies the hand holding it, more the longer you hold it (0 none)."),
        toggle("Water Washes Blood", vr_gore_wash).help("Water washes the blood off your body and hands, where they are under it."),
        slider("Wash Time", vr_gore_wash_time, 0.25f, 10.f, 0.25f, "%.2f s").extend(0.05f, 60.f)
            .help("How long under water washes the blood off fully."),
        toggle("Wounds Re-open", vr_gore_reopen)
            .help("Hurt, your wounds re-open after a wash and bleed onto your arms and hands again (healed ones don't, nor a gib's blood)."),
        slider("Re-open Delay", vr_gore_reopen_delay, 0.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long you stay clean out of the water before they re-open."),
        slider("Re-open Spread", vr_gore_reopen_time, 0.f, 10.f, 0.25f, "%.2f s").extend()
            .help("Over how long they re-open, one after another (0: all at once)."),
        header("Blood on You and Your Gear"),
        slider("Blood Spatter", vr_gore_spatter, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Blood thrown from hits near you onto your hands, arms, body and what you hold, as drops: how much (0 none)."),
        slider("From Your Blows", vr_gore_spatter_melee, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("A punch, a blade, a prop or a gun swung into a monster bloodies what struck and the hand a little."),
        slider("From the Chainsaw", vr_gore_spatter_saw, 0.f, 6.f, 0.25f, "%.2fx").extend()
            .help("A chainsaw cutting a monster sprays your hands and arms: a lot."),
        slider("From Close Shots", vr_gore_spatter_shots, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("A shot hitting close to you throws a few drops onto your weapon, hands and arms."),
        slider("Close Shot Range", vr_gore_spatter_range, 16.f, 160.f, 8.f, "%.0f u").extend(0.f, 512.f)
            .help("How close to your hands or eyes a shot must hit for its blood to reach you."),
        slider("Gibs Striking You", vr_gore_spatter_gibs, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Gibs flying into you bloody you where they strike: how much (0 none)."),
        toggle("Blood on Weapons and Props", vr_gore_gear)
            .help("What you hold takes blood and keeps it: dropped, thrown and taken again, holstered and drawn, until water washes it."),
        toggle("Clean Weapon Skins", vr_gore_clean_skins)
            .help("Weapons with blood painted into their skins (the axe, the knights' swords, the chainsaw, the grunt's shotgun) start clean: their blood is all yours, and water washes them clean."),
        toggle("Holstered Weapons Too", vr_gore_gear_holstered)
            .help("Your holstered weapons take blood as your body and hands do, when it lands near enough to them (a holstered weapon keeps its blood either way)."),
        slider("Things Lying Near", vr_gore_gear_nearby, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Weapons, boxes and props lying about take blood from bleeding hits, gibbings and bursting gibs near them: how much (0 none)."),
        slider("Gibbed Monsters' Drops", vr_gore_gear_drops, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("What lies right where a monster is gibbed (its gun, its backpack) comes out soaked: how much (0 only the drops near it)."),
        slider("Blood over Your Arms", vr_gore_spread, 0.f, 3.f, 0.25f, "%.2fx").extend()
            .help("Hurt, holding a gib, or as your wounds re-open, blood runs over your arms too (a little your legs): how much (0 none)."),
        header("Blood Mist"),
        slider("Mist Amount", vr_gore_mist, 0.f, 3.f, 0.25f, "%.2fx").extend(0.f, 10.f)
            .help("Large, faint clouds of blood hanging in the air wherever something bleeds: how many (0 none)."),
        slider("Mist Size", vr_gore_mist_size, 0.25f, 3.f, 0.05f, "%.2fx").extend(0.05f, 6.f).help("How big the clouds are."),
        slider("Mist Opacity", vr_gore_mist_alpha, 0.f, 0.3f, 0.01f, "%.2f").extend(0.f, 1.f).help("How opaque they start (very transparent: 0.06)."),
        slider("Mist Lifetime", vr_gore_mist_life, 0.25f, 8.f, 0.25f, "%.2f s").extend(0.2f, 30.f).help("How long they take to fade."),
        slider("Mist Spreading", vr_gore_mist_grow, 0.f, 3.f, 0.1f, "%.1fx").extend().help("How fast they spread as they fade."),
        slider("Mist Drift", vr_gore_mist_speed, 0.f, 3.f, 0.1f, "%.1fx").extend().help("How fast they drift along the blow and apart."),
        slider("Mist Rise", vr_gore_mist_rise, -20.f, 20.f, 1.f, "%.0f u/s").extend(-100.f, 100.f).help("How fast they rise (negative: sink)."),
        slider("Mist Darkness", vr_gore_mist_dark, 0.f, 1.f, 0.05f, "%.2f").help("How dark their red is (0 bright, 1 nearly black)."),
        slider("Mist Clear of Eyes", vr_gore_mist_near, 0.f, 128.f, 4.f, "%.0f u").extend(0.f, 512.f)
            .help("No mist closer to your eyes than this: your own bleeding is not in your face."),
        header("Dying Bodies"),
        toggle("Hit While Dying", vr_corpse_dying)
            .help("A monster dying takes damage as a corpse while it falls (once it drops what it drops): a chainsaw, a blow or a shot can gib it. Never killed twice. Needs Gib Corpses (Carrying and Throwing > Gibs and Corpses)."),
        header("Training Dummy"),
        cycle("Dummy Enemy", vr_dummy_type, dummyEnemies())
            .help("The enemy the training dummy stands as: its model, size, hit zones, the head it loses, its blood, gore and death are that monster's own. It stays still, takes no harm and reports every hit. The mission packs' with the pack installed."),
        toggle("Dummy Bleeds", vr_dummy_gore)
            .help("The firing range's training dummy bleeds as its enemy: blood sprays and mist, wounds on its model, small gibs, blood on you and what you hold. Off: it stays clean."),
        toggle("Dummy Dies", vr_dummy_gib)
            .help("The hit that takes the last of its health (Dummy Health) kills the training dummy as its enemy: beheaded, its head popped, dying, lying as a ragdoll or gibbed exactly as that monster would be. Then it stands again. No loot. Off: it stays at 0 health (\"would kill\"). On as shipped."),
        slider("Dummy Stands Again", vr_dummy_gib_respawn, 0.5f, 10.f, 0.5f, "%.1f s").extend(0.1f, 60.f)
            .help("How long a killed training dummy takes to stand again (its body stays, as its enemy's)."),
        dummyHealthSlider(),
        slider("Dummy Health Refills", vr_dummy_regen, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long after its last hit the training dummy's health fills up again (0: never; it stays as hit until it dies and stands again)."),
        toggle("Dummy Health Bar", vr_dummy_healthbar)
            .help("The training dummy's health over its head, as a bar and a number, turned to face you. Off: its sign only."),
        header("Marks"),
        toggle("Decals", vr_decals).help("Blood, scorch marks and bullet chips on walls and floors (the gore needs them)."),
        slider("Max Decals", vr_decal_max, 64.f, 4096.f, 64.f, "%.0f").extend().help("The oldest go first. The gore makes many: 1024 or more."),
        slider("Decal Lifetime", vr_decal_life, 10.f, 600.f, 10.f, "%.0f s").extend(),
        toggle("Gib Blood", vr_gib_blood).help("Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail."),
        slider("Gib Blood Trail", vr_gib_blood_trail, 0.f, 3.f, 0.25f, "%.2fx").extend().help("How dense their trail of blood and drops is (0 none)."),
    };
}

[[nodiscard]] za::Vector<Item> pageGadget()
{
    return {
        open("HUD and Gadget Arm: VR Settings, Body and Display", pageIndex(pageMain)).help("The HUD's kind is on VR Settings; the gadget's arm on Body and Display."),
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

// Split from Wrist Gadget (and Immersion): the wrist gadget's screen, the weapons' ammo screens and
// the maps' text boards.
// Tips (vr_tips.cpp): for new players, each shown once near what it is about.
[[nodiscard]] za::Vector<Item> pageTips()
{
    return {
        cycle("Tips", vr_tips, {{0.f, "Off"}, {1.f, "Floating panel"}, {2.f, "Wrist gadget"}})
            .help("Tips for new players, each shown once: the first time you come near something you can use (a wall "
                  "torch). Floating panel: a screen like the maps' text boards by it, with a cable to it. Wrist gadget: in the gadget's hologram, waiting until "
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
        cycle("Panel Facing", vr_tips_facing, {{0.f, "Towards your eyes"}, {1.f, "Square to your view"}})
            .help("Towards your eyes: the floating screen turns to face you and stays level. Square to your view: it is "
                  "always flat in front of you, as if on your view, tilting as you tilt your head."),
        slider("Panel Text Size", vr_tips_size, 0.5f, 2.f, 0.1f, "%.1fx").extend(0.25f, 4.f),
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

[[nodiscard]] za::Vector<Item> pageThrowing()
{
    return {
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend()
            .help("How fast what you throw flies, times your hand's speed."),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}})
            .help("How thrown things fall: Real, as on Earth; Quake, as Quake's own gravity."),
        slider("Two-Hand Throw Speed", vr_2h_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        slider("Two-Hand Throw Damage", vr_throw_2h_damage, 0.5f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("A weapon or prop thrown with both hands: its hits' damage times this (one hand: 1x). 1.25 as shipped."),
        slider("Velocity Window", vr_throw_window, 0.04f, 0.3f, 0.01f, "%.2f s").extend()
            .help("Around the release, where the hand's fastest moment sets the throw."),
        slider("Direction Lookback", vr_throw_dir_lookback, 0.f, 0.1f, 0.005f, "%.3f s").extend()
            .help("How far back from that moment the throw's direction is averaged."),
        slider("Lever Arm", vr_throw_lever_arm, 0.f, 0.3f, 0.01f, "%.2f m").extend()
            .help("From the palm to the held object's centre: wrist flicks add speed through it."),
        slider("Release Pitch", vr_throw_pitch, -15.f, 15.f, 0.5f, "%+.1f deg").extend()
            .help("Tilts every throw up (or down, below 0). 0: as throws were tuned; the hand calibration doesn't change them."),
        toggle("Analog Release", vr_throw_release)
            .help("A throw lets go as the grip starts to open, not only once it is released."),
        toggle("Slow Motion: Throws in Real Time", vr_throw_slowmo_real_time)
            .help("In slow motion (bullet time, not Sandevistan), a throw's release is judged over the same stretch of "
                  "your real motion as at full speed, and its wrist flick by your real wrist speed. Off: over the slowed "
                  "clock's, which took in three times the arm's arc at 0.3x (throws went off, the wrist's share too big)."),
        toggle("Slow Motion: Throw Where You Aim", vr_throw_slowmo_aim)
            .help("In slow motion (bullet time, not Sandevistan), a throw goes the way your controller moved, as the same "
                  "motion at full speed would, even when the slowed hand lags behind it; its speed stays the slowed "
                  "hand's. Off: the way the lagging hand moved to catch up (fast throws went 20 to 30 degrees high)."),
        slider("Slow Motion: Real-Time Flicks", vr_throw_slowmo_flick, 0.f, 1.f, 0.05f, "%.2f")
            .help("In slow motion (bullet time, not Sandevistan), a wrist flick at real speed throws as at full speed: its "
                  "speed in your real time, not three times it in the slowed world's (a flick that threw 1 m threw 8 m). "
                  "The arm's part of a throw is unchanged. 0: as before; between: a blend."),
        slider("Slow Motion: Wrist Flick Below", vr_throw_slowmo_flick_arm, 0.f, 0.9f, 0.05f, "%.2f").extend()
            .help("In slow motion, a throw your arm moves less than this share of (your wrist's turn doing the "
                  "rest) is a wrist flick, thrown as far as the same flick at full speed, however fast or gently you "
                  "flicked. A throw your arm carries keeps the arm's speed. Higher: flicks with more arm count too."),
        slider("Slow Motion: Nudge Below", vr_throw_slowmo_short_travel, 0.f, 0.5f, 0.01f, "%.2f m").extend()
            .help("In slow motion, a throw whose hand moved less than this up to the release (from where the motion "
                  "got going) is a nudge, thrown as hard as the same nudge at full speed (a 10 cm nudge went 14 "
                  "times as far). Longer throws, from Arm Throw From up, keep the bullet-time scaling. 0: off."),
        slider("Slow Motion: Arm Throw From", vr_throw_slowmo_long_travel, 0.f, 0.6f, 0.01f, "%.2f m").extend()
            .help("In slow motion, a throw whose hand moved this far or more up to the release is the arm's, with the "
                  "bullet-time scaling (a slow throw made with the slowed world goes as at full speed). Between Nudge "
                  "Below and this: a blend."),
        toggle("Slow Motion: Throws As In Real Time", vr_throw_slowmo_real_strength)
            .help("In slow motion, every throw is as strong as the same motion at full speed: no bullet-time scaling. "
                  "A throw you make slowly with the slowed world then goes as slowly as you made it. Off: nudges and "
                  "flicks as in real time, longer throws scaled."),
        toggle("Slow Motion: Slow Throws Match", vr_throw_slowmo_tempo)
            .help("In slow motion, a throw you make slowly, with the slowed world, is judged as the same throw at full "
                  "speed (its release over the same part of the arc); one faster than the slowed hands can follow, over "
                  "your real motion. Off: always over your real motion (a slow throw came out a few degrees lower)."),
        toggle("Spin From Controller Turn", vr_throw_spin_from_pose)
            .help("A throw's spin from how the controller turned, not the runtime's angular velocity (Virtual Desktop "
                  "reports it in the controller's frame: a flick facing away from the play space's front spun throws "
                  "sideways)."),
        cycle("Controller Spin Frame", vr_angvel_frame,
            {{-1.f, "Auto"}, {0.f, "Tracking Space"}, {1.f, "Controller's"}, {2.f, "From Turn"}})
            .help("Which frame the runtime's controller angular velocity is in (melee, flick reload, throws). Auto: "
                  "Virtual Desktop's controller frame, else the tracking space, checked against how the controllers "
                  "turn. Tracking Space: as before. From Turn: ignore it, use the controller's turn."),
        slider("Max Speed Gain", vr_throw_gain_max, 1.f, 3.f, 0.05f, "%.2fx").extend()
            .help("Extra speed for fast throws, which feel weak at true speed."),
        header("Throws by Weight"),
        toggle("Throws by Weight", vr_throw_mass_model)
            .help("Heavy things can't be thrown as fast or as far as light ones, a wrist flick does little to them, and "
                  "two hands throw them farther than one. Off: every weapon and prop as fast as your hand (Max Speed "
                  "Gain), heavy weapons a little slower, props over 12 kg slower. Debug: Throws by Weight prints the table."),
        slider("Light Things' Top Speed", vr_throw_max_speed, 10.f, 40.f, 1.f, "%.0f m/s").extend(5.f, 100.f)
            .help("The fastest anything leaves your hand: a grenade, the axe (28: about 80 m at 45 degrees)."),
        slider("Light Up To", vr_throw_mass_light, 0.5f, 5.f, 0.1f, "%.1f kg").extend(0.1f, 50.f)
            .help("Things up to this heavy can reach the top speed (a grenade: 1.2 kg; the axe, 2 kg, a little less); "
                  "heavier, less."),
        slider("Heavy Falloff", vr_throw_mass_exp, 0.f, 1.5f, 0.05f, "%.2f")
            .help("How fast the limit falls with mass beyond it: (light / mass) to this power. 0.5: the same energy, 1: "
                  "the same impulse. 0.9 (with Light Up To 1.5): the super nailgun (7 kg) at most 7 m/s, the laser "
                  "cannon (15 kg) 3.5."),
        slider("Soft Limit From", vr_throw_mass_knee, 0.f, 1.f, 0.05f, "%.2f")
            .help("Below this share of a thing's limit a throw is as fast as your hand's; above, it eases towards the "
                  "limit (1: a hard cap)."),
        slider("Two-Hand Strength", vr_throw_2h_strength, 1.f, 4.f, 0.1f, "%.1fx").extend(0.1f, 10.f)
            .help("Thrown with both hands, a thing is as if it weighed this many times less: heavy things go farther, "
                  "light ones no faster (1: no help)."),
        slider("Full Wrist Flick Up To", vr_throw_flick_mass, 0.5f, 10.f, 0.1f, "%.1f kg").extend(0.01f, 100.f)
            .help("A wrist flick throws things up to this heavy at its whole speed; a heavier one keeps only (this / "
                  "mass) of the speed the flick gives (the super nailgun a third)."),
        slider("Wrist to Controller", vr_throw_wrist_dist, 0.f, 0.15f, 0.005f, "%.3f m")
            .help("From the wrist to the controller's point: the turn of the hand about the wrist gives the controller "
                  "this lever's speed, the part of a throw a heavy thing keeps less of."),
        slider("Heavy Spin Falloff", vr_throw_spin_mass_exp, 0.f, 2.f, 0.05f, "%.2f")
            .help("A heavy thing can't be made to spin fast either: past Full Wrist Flick Up To, its spin is held under "
                  "Max Spin times (that / mass) to this power (two hands: as if Two-Hand Strength times lighter). 1: the "
                  "explosive box (40 kg) at most about 1.3 rad/s one-handed, 2.5 with both; the super nailgun 7. 0: off."),
        toggle("Aim Assist", vr_throw_assist)
            .help("Throws close to an enemy's direction bend towards it."),
        slider("Assist Cone", vr_throw_assist_cone, 2.f, 30.f, 1.f, "%.0f deg").extend(),
        slider("Assist Strength", vr_throw_assist_strength, 0.f, 1.f, 0.05f, "%.2f"),
        header("Physics"),
        slider("Bounciness", vr_throw_restitution, 0.f, 0.8f, 0.05f, "%.2f").extend(),
        slider("Friction", vr_throw_friction, 0.f, 1.5f, 0.05f, "%.2f").extend(),
        slider("Max Spin", vr_throw_spin_max, 0.f, 40.f, 1.f, "%.0f rad/s").extend(),
        slider("Spin Drag", vr_throw_spin_drag, 0.f, 2.f, 0.05f, "%.2f").extend(),
        slider("Spin Alignment", vr_throw_spin_align, 0.f, 20.f, 0.5f, "%.1f").extend(0.f, 100.f)
            .help("How fast a thrown thing's tumble settles into a clean spin end over end about its steadiest axis (an "
                  "axe: in its blade's plane), as long, flat things do in the air. Most for long flat things (an axe, a "
                  "sword), a tenth for long round ones (a gib), none for a box. Until it first touches anything. Each "
                  "weapon's and prop's own Spin in the Air (Weapon Weights, Held Object Weights) multiplies it. 0: off."),
        slider("Hitbox", vr_throw_hitbox, 1.f, 12.f, 0.5f, "%.1f").extend().help("Half-size of a thrown weapon's box against monsters."),
        slider("Hit Min Speed", vr_throw_hit_min_speed, 0.f, 600.f, 25.f, "%.0f").extend()
            .help("Units/s a thrown weapon, box or gib must go at to hurt a monster; slower (at rest against it, pushed into it) it does nothing."),
        slider("Heavy Hits Scale Below", vr_throw_hit_top, 0.f, 30.f, 0.5f, "%.1f m/s").extend(0.f, 100.f)
            .help("A thing that can't be thrown this fast one-handed (Throws by Weight: its mass) hurts at lower speeds, "
                  "in proportion: its Hit Min Speed and the speed its damage is measured against both times its top "
                  "speed over this, so a hard throw of a heavy thing hurts as a hard throw of a light one. 0: only "
                  "Aiming: Heavy Leniency, as before."),
        slider("Damage Eases In Over", vr_throw_hit_ramp, 0.f, 2.f, 0.05f, "%.2fx")
            .help("Just over its Hit Min Speed a throw does less: all its damage only this share of that speed above "
                  "it (0: all of it at once, as before)."),
        slider("Damage at Hit Min Speed", vr_throw_hit_ramp_floor, 0.f, 1.f, 0.05f, "%.2fx")
            .help("The share of its damage a throw does at its Hit Min Speed, rising to all of it over the speed above."),
        slider("Two-Handed Throws: No Blows", vr_throw_2h_nomelee, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("After a throw with both hands, neither hand strikes for this long (the hand that let go first, from "
                  "its letting go): the follow-through is not a punch. 0: off."),
        slider("Two-Handed Throws: Spared", vr_throw_2h_melee_immune, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("What you throw with both hands can't be struck by your hands for this long (a gib burst by your own "
                  "follow-through). 0: off."),
        slider("Gibs Let Go: Other Hand Spares", vr_gib_letgo_spare, 0.f, 1.f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("A gib you let go of with one hand can't be burst by your other hand's touch for this long (the hand "
                  "alongside the throw); the hand that let go of it: 0.4 s. Toss it up and punch it after. 0: off."),
        toggle("Thrown Gibs Burst on You", vr_gib_burst_on_thrower)
            .help("A gib you throw that meets your own body or hands (and no wall) bursts as on a wall. Off: it bounces "
                  "off you whole."),
        slider("Your Throws Spare You For", vr_throw_self_grace, 0.f, 1.5f, 0.05f, "%.2f s").extend(0.f, 5.f)
            .help("What you throw passes through you and can't hurt you for this long after it leaves your hand; after "
                  "it, it hurts you as it would a monster (a backpack thrown high falling back on you)."),
        header("Thrown Axes"),
        toggle("Axes Stick", vr_axestick)
            .help("A thrown axe that strikes blade first sticks in walls, doors, props and monsters (it moves with them). "
                  "Grip it, or force grab it (it tugs, then comes free), to pull it out. Off: it bounces off."),
        toggle("Axes Stick in Explosive Boxes", vr_axestick_metal)
            .help("A thrown axe sticks in an explosive box too. Off: it rings off it, as the boxes are metal drums."),
        slider("Bleeding", vr_axestick_bleed, 0.f, 20.f, 0.5f, "%.1f health/s").extend(0.f, 100.f)
            .help("Health a second a monster loses while an axe is stuck in it (your damage). It falls out when the "
                  "monster dies."),
        slider("Blade Leniency", vr_axestick_leniency, 0.f, 2.f, 0.1f, "%.1f")
            .help("Whichever part of the axe goes in first decides: its blade sticks, anything else bounces. This is how "
                  "much of the blade counts. 0: the edge and its two corners. 1: the blade 45% of its width back from "
                  "the edge (its top and bottom edges in from the corners), met a little before the body meets the "
                  "wall, a little more glancing allowed (Stick Incidence + 10), and turned up to 25 degrees to keep the "
                  "handle out of the wall. 2: 80% of the blade, twice the rest. The handle, its end and the head's "
                  "middle never stick; a flat throw, the blade's side first, bounces."),
        slider("Stick Speed", vr_axestick_speed, 1.f, 12.f, 0.5f, "%.1f m/s").extend(0.f, 30.f)
            .help("How fast the blade's edge must go into what it strikes; slower, it bounces off."),
        slider("Stick Angle", vr_axestick_angle, 10.f, 90.f, 5.f, "%.0f deg")
            .help("How far out of its own plane the blade may go in: more, and flat throws (the blade's side first) "
                  "stick too."),
        slider("Stick Incidence", vr_axestick_incidence, 10.f, 90.f, 5.f, "%.0f deg")
            .help("How far from straight into the surface the blade may go in: more, and glancing blows stick too. An "
                  "axe that hardly turns skids 20 degrees sooner."),
        slider("Stick Depth", vr_axestick_depth, 0.f, 12.f, 0.5f, "%.1f cm").extend()
            .help("How deep the edge goes in at a hard throw (half as deep at the Stick Speed)."),
        slider("Force Grab Tug", vr_axestick_tug, 0.f, 1.f, 0.05f, "%.2f s")
            .help("How long a force grab tugs a stuck axe before it comes free and flies to your hand."),
        header("Flung Props"),
        toggle("Flung Props Hurt", vr_prop_impact_damage)
            .help("A prop you didn't throw (swung on the grapple's rope, batted, knocked flying) hurts the monster it flies "
                  "into fast enough. Off: only what you throw by hand."),
        toggle("Flung Props Hurt Players", vr_prop_impact_players)
            .help("They hurt players too (never you as you let go of it, bat it or while your grapple holds it). Off: "
                  "monsters only."),
        slider("Fresh Gibs Harmless For", vr_gib_spawn_harmless, 0.f, 2.f, 0.1f, "%.1f s")
            .help("A gib, head or small gib just torn out can't hurt you or a monster as a flung prop for this long (gibbing a "
                  "body you stand on). Thrown by you after it, it hurts as ever."),
        slider("Monster Drops Harmless For", vr_prop_drop_grace, 0.f, 2.f, 0.1f, "%.1f s")
            .help("A weapon or a backpack a monster drops as it dies can't hurt players for this long (it may land on "
                  "you); after it, see Monster Drops Hurt Only Falling."),
        toggle("Monster Drops Hurt Only Falling", vr_prop_drop_falls_only)
            .help("After that, a monster's weapon or backpack hurts you only falling on you or once a hand has thrown it: "
                  "never pushed or tossed into you from under you (a monster killed at your feet). Off: as any flung prop."),
        toggle("Monster Drops Pass Through Bodies", vr_prop_drop_pass_inside)
            .help("A monster's drop made inside you or a monster (killed at your feet) passes through it until clear, "
                  "instead of being pushed out hard."),
        slider("Least Speed", vr_prop_impact_min_speed, 2.f, 20.f, 0.5f, "%.1f m/s").extend(0.f, 50.f)
            .help("How fast it must fly into what it hits to hurt it: less for things over 10 kg. The damage grows with "
                  "the speed over it."),
        slider("Damage", vr_prop_impact_mult, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("All their damage: the throw's (Thrown Box Damage, a gib's, a weapon's) at the least speed and the "
                  "reference mass."),
        slider("Most Speed Multiplier", vr_prop_impact_speed_max, 1.f, 6.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Faster hits hurt more, up to this many times the damage at the least speed."),
        slider("Weight Curve", vr_prop_impact_weight_curve, 0.f, 1.5f, 0.05f, "%.2f").extend(0.f, 3.f)
            .help("How much mass changes the damage: 0 not at all, 1 in proportion (twice the mass, twice the damage)."),
        slider("Reference Mass", vr_prop_impact_weight_ref, 0.5f, 20.f, 0.5f, "%.1f kg").extend(0.1f, 100.f)
            .help("A prop of this mass deals the damage as set; lighter ones less, heavier ones more."),
        slider("Most Weight Multiplier", vr_prop_impact_weight_max, 1.f, 5.f, 0.1f, "%.1fx").extend(1.f, 20.f)
            .help("The most a heavy prop multiplies the damage."),
        slider("Least Mass", vr_prop_impact_min_mass, 0.f, 10.f, 0.1f, "%.1f kg").extend(0.f, 100.f)
            .help("Lighter props never hurt when flung (thrown by hand, they still do)."),
        header("Shots Push Props"),
        slider("Shot Push", vr_shot_push, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("Pellets, nails and the lightning beam push the boxes, gibs and weapons they hit or pass through (monsters' "
                  "shots too): times every push below. 0: off. Explosions push things anyway."),
        slider("Pellet Push", vr_shot_push_pellet, 0.f, 20.f, 0.5f, "%.1f N s").extend(0.f, 100.f)
            .help("Each shotgun and double shotgun pellet (6 and 14 a shot). A heavy box takes about the whole push: 40 kg "
                  "pushed 10 N s moves at a quarter metre a second."),
        slider("Nail Push", vr_shot_push_nail, 0.f, 40.f, 1.f, "%.0f N s").extend(0.f, 200.f)
            .help("Each nail of the nailgun (and scrags', knights' and enforcers' shots)."),
        slider("Super Nail Push", vr_shot_push_supernail, 0.f, 60.f, 1.f, "%.0f N s").extend(0.f, 200.f)
            .help("Each nail of the super nailgun."),
        slider("Lightning Push", vr_shot_push_lightning, 0.f, 20.f, 0.5f, "%.1f N s").extend(0.f, 100.f)
            .help("Each bolt of the lightning beam, 10 a second (a shambler's by its damage)."),
        slider("Shot Push Top Speed", vr_shot_push_speed, 1.f, 40.f, 1.f, "%.0f m/s").extend(0.1f, 100.f)
            .help("The fastest a shot makes what it hits go: light things (a gib, a small gun) fly off at most this fast, "
                  "heavy ones take the push."),
        header("Wall Buttons"),
        toggle("Weapons Press Buttons", vr_button_weapon)
            .help("A weapon in your hand presses a wall button it touches: all of it, as its blows strike (a sword's "
                  "blade, pommel and hilt; a gun's barrel and butt). Off: only the line from the hand to a gun's muzzle."),
        slider("Weapon Press Reach", vr_button_weapon_reach, 0.f, 15.f, 0.5f, "%.1f cm")
            .help("How far round the weapon's middle line a button is touched: its thickness, and some slack."),
        toggle("Held Props Press Buttons", vr_button_prop)
            .help("A prop in your hand (a health or ammo box, a crate, a rock, a gib) presses a wall button it touches, as "
                  "a weapon does: its shape as you hold it."),
        slider("Prop Press Reach", vr_button_prop_reach, 0.f, 15.f, 0.5f, "%.1f cm")
            .help("How far round the held prop's shape a button is touched: a held prop is kept a few centimetres off "
                  "walls and buttons (it can't go into them), so it needs some slack to reach one."),
        toggle("Thrown Things Press Buttons", vr_button_throw)
            .help("A weapon, rock, box or gib thrown at a wall button presses it. Buttons you must shoot are still shot."),
        slider("Thrown Press Min Speed", vr_button_throw_speed, 0.f, 600.f, 25.f, "%.0f u/s").extend()
            .help("How fast a thrown thing must hit a button to press it (slower, it only bumps it)."),
    };
}

// Carrying ammo and health boxes, explosive boxes, armour and pickups.
[[nodiscard]] za::Vector<Item> pageCarrying()
{
    return {
        header("Carrying Boxes"),
        toggle("Carry Ammo and Health", vr_carry)
            .help("Grip a box or a backpack to carry it, push it with a hand or gun. Off: touching takes it."),
        cycle("Take a Box", vr_carry_take, {{0.f, "At a holster"}, {1.f, "Trigger"}, {2.f, "Either"}})
            .help("At a holster: let go of it at a hip or shoulder holster to put it in your pack."),
        toggle("Put-Away Transition", vr_collect_fx)
            .help("What you put away (a box or backpack at a holster, a key, a rune or a power-up, an ammo box or a round "
                  "at the ammo pouch, a grenade at its pouch) is seen shrinking into the holster or pouch, following you, "
                  "instead of vanishing. Only how it looks: it is yours the moment you let go, as before."),
        slider("Put-Away Time", vr_collect_fx_time, 0.05f, 0.6f, 0.05f, "%.2f s").extend(0.f, 2.f)
            .help("How long it takes to go in (slowed in bullet time)."),
        slider("Put-Away End Size", vr_collect_fx_size, 0.05f, 1.f, 0.05f, "%.2fx")
            .help("How small it is as it goes in: its size then, of its own."),
        slider("Grab Distance Bias", vr_carry_grab_bias, -3.f, 5.f, 0.5f, "%+.1f cm")
            .extend(-10.f, 20.f)
            .help("A hand takes a box, gib, backpack or armour when its fist (the palm and the curled fingers) touches it. "
                  "Positive: from this far off it too. Negative: only pressed this far into it."),
        toggle("Explosive Boxes by the Fist", vr_carry_grab_drawn)
            .help("An explosive box is taken as everything else is: when the fist touches it. Off: when the hand is within "
                  "a hand's width of its box (up to 12 cm off)."),
        toggle("Weapons by the Fist", vr_weapon_grab_drawn)
            .help("A weapon lying on the floor is taken where the fist touches it: a crowbar by its bar, a sword by its "
                  "blade. Off: only with the hand at its handle."),
        slider("Weapon Grab Slack", vr_weapon_grab_slack, 0.f, 10.f, 0.5f, "%.1f cm")
            .help("With Weapons by the Fist: a weapon lying on the floor is taken with the fist this far off it too (a gun "
                  "lying flat is thinner than the lowest your fist gets over it). Not one in the air: caught by the fist on it."),
        slider("Handle Grab Leniency", vr_weapon_grab_handle_leniency, 0.f, 10.f, 0.5f, "%.1f cm")
            .extend(0.f, 20.f)
            .help("A gun lying on a table or the floor is taken by its handle with your fist this much farther off it than "
                  "the slack above (the hand at its handle; your hand nudging it doesn't stop it). Only a direct grab: a "
                  "weapon caught in the air still needs your fist on it, and a force grab is unchanged. 0: as anywhere on it."),
        toggle("Lenient Weapon Catch", vr_weapon_grab_box)
            .help("The old catch: a weapon is also taken when your hand comes within a hand's width of its handle, the "
                  "fist up to 20 cm off it. Off: only when your closed fist touches it, as a box or a gib."),
        toggle("Weapons by Their Hotspots", vr_weapon_grab_hotspots)
            .help("Grip a weapon on the floor nearer one of its other grips than its handle (a chainsaw's front handle, "
                  "a crowbar's bar, a sword's blade, a gun's pump) and you carry it by that grip, as when your other hand "
                  "lets go of a weapon held in both: grip its handle with the other hand to use it. Not a pistol's "
                  "two-handed cup grip. A force grab always takes the handle. Off: always by the handle."),
        toggle("Weapons Anywhere", vr_weapon_grab_anywhere)
            .help("Grip a weapon anywhere away from its handle and its other grips and you hold it there, as a box or a gib: "
                  "off the floor as it lies, with your other hand on one you hold (Other Hand Anywhere), with both hands "
                  "on one you carry. Held off its handle it doesn't fire, but strikes and throws; grip its handle to use "
                  "it. Holstered, it is drawn by its handle. Off: only by the handle and the other grips."),
        slider("Anywhere: Away From Grips", vr_weapon_grab_anywhere_min, 0.f, 30.f, 1.f, "%.0f cm")
            .help("How far from a weapon's handle and its other grips (a foregrip, a pump, a blade) you must grip it to hold "
                  "it anywhere: nearer, the handle or that grip takes it, so they stay easy to take."),
        cycle("Other Hand Anywhere", vr_weapon_anygrip_mode, {{0.f, "As a Foregrip"}, {1.f, "Support Only"}, {2.f, "Rigid"}})
            .help("Your other hand gripping a weapon you hold by its handle, away from its grips. As a Foregrip: both hands "
                  "aim it, from wherever it holds it. Support Only: it bears the weight, the aim stays your weapon hand's. "
                  "Rigid: the weapon follows both hands as a box held in both. Each weapon may override it (Weapon "
                  "Offsets: Other Hand Anywhere)."),
        toggle("Drawn In the Hand", vr_carry_local)
            .help("What you carry is drawn in your hand as it is this frame: no lag or lead as you walk or turn. Off: where the server has it."),
        toggle("Two-Handed Carrying", vr_carry_two_hands)
            .help("Grip what your other hand carries to hold it in both: it moves and turns with both hands, and letting go "
                  "of both together throws it. Let go with one and the other keeps it (to pass it from hand to hand)."),
        slider("Two-Handed Grab Reach", vr_carry_two_hands_reach, 0.f, 5.f, 0.5f, "%.1f cm")
            .help("How far past Grab Distance Bias your hand may be off what your other hand holds and still grip it "
                  "(to hold it in both, then let go with the first and grip it again as you want). Every prop can be held "
                  "in both hands unless its Two Hands (Held Object Offsets) is off."),
        slider("Two-Handed Hand Drift", vr_carry_two_hands_drift, 0.f, 20.f, 1.f, "%.0f cm")
            .extend(0.f, 50.f)
            .help("How far your drawn hands may be off your real ones to stay on their grips as you pull them apart or push "
                  "them together. 0: they stay on your real hands. Pulled 3 cm further off (vr_carry_two_hands_detach), "
                  "with the hand no longer touching it, that hand lets go; both, and it drops."),
        toggle("Two-Handed Stops at Walls", vr_carry_two_hands_solid)
            .help("What you hold in both hands stops at walls, floors and doors however hard you push, and slides along "
                  "them; it keeps its turn where a new one would put it in. Off: it goes where your hands put it."),
        toggle("Fit to the Hand", vr_held_surface_fit)
            .help("A box, backpack or gib you grip sits against your curled fingers, by its drawn shape. Off: it stays where you gripped it."),
        slider("Fit Gap", vr_held_fit_gap, -6.f, 3.f, 0.1f, "%.1f cm").extend()
            .help("Space left between your fingers and what they hold (negative: sunk in). Per model: vr_held_fit_gaps in the console."),
        toggle("Held Things Collide", vr_held_collide)
            .help("Things you hold one in each hand, or a thing in one hand and a weapon in the other, bump into each other "
                  "instead of passing through: each hand is drawn moved back with what it holds, a short buzz as they meet. "
                  "Your real hands are never held back."),
        slider("Collide Give", vr_held_collide_max, 0.f, 15.f, 0.5f, "%.1f cm")
            .extend(0.f, 40.f)
            .help("How far each hand is drawn moved back at most as you press them together. Pressed further, they overlap by "
                  "the rest."),
        slider("Empty Hand Against Held Things", vr_hand_collide, 0.f, 10.f, 0.5f, "%.1f cm").extend(0.f, 30.f)
            .help("An empty hand bumps into what the other hand holds, a weapon or a thing, the same way, as two held things "
                  "do: at its surface the hand and it are each drawn moved back by half, as far as this, a short buzz as "
                  "they meet; pressed further, the hand sinks in by the rest. Drawn only. 0: it always passes through."),
        slider("Empty Hand Stops Off It", vr_hand_collide_props_margin, 0.f, 3.f, 0.25f, "%.2f cm")
            .help("How far off the surface of what the other hand holds the empty hand's palm and knuckles stop: about 1 cm "
                  "keeps the skin at the surface."),
        toggle("Fingers Rest on Held Things", vr_hand_collide_fingers)
            .help("With Empty Hand Against Held Things: the empty hand's fingers rest on what the other hand holds or bend "
                  "out of it, instead of the whole hand being pushed by its fingertips."),
        toggle("Weapons Slide Along Walls", vr_gun_wall_slide)
            .help("A weapon you hold into a wall, a floor or a table slides along it: lowered onto a table it rests on the top, "
                  "pushed into a wall it is held off along the wall. Off: the old push-back, the hand moved back along the aim "
                  "until the muzzle stops at the wall."),
        slider("Weapon Wall Give", vr_gun_wall_max, 0.f, 80.f, 5.f, "%.0f cm")
            .extend(0.f, 200.f)
            .help("How far a weapon and your hand are held out of a wall or a table at most as you push into it. Pushed "
                  "further, they go in by the rest."),
        toggle("Held Things Stop at Walls", vr_held_collide_walls)
            .help("A thing you hold in one hand stops at walls, floors and doors: it and your hand are drawn held out of them, "
                  "a short buzz as it touches. Looks only: your real hand is never held back."),
        slider("Wall Give", vr_held_collide_wall_max, 0.f, 80.f, 5.f, "%.0f cm")
            .extend(0.f, 200.f)
            .help("How far it and your hand are drawn held back at most as you push it into a wall. Pushed further, it goes "
                  "in by the rest; pushed about a metre past, it drops."),
        toggle("Things Rest on Hands", vr_model_collide_rest)
            .help("A thing lying on your open palm, fist or gun (let go of there, or balanced) never pushes the drawn hand away: "
                  "the hand stays under it. Off: it pushes the hand out of it as other things do."),
        toggle("Held Things Stop at Monsters", vr_held_collide_monsters)
            .help("A thing you hold in one hand and swing as a club stops at the monster's body, as weapons do (held out "
                  "up to vr_model_collide_max, 20 cm), instead of passing through it. It still hits, and is never dropped for "
                  "it."),
        toggle("Hands Push and Hold Things", vr_box3d_hand_props)
            .help("Your empty hands are solid to loose things: they push them, and what you let go of on an open palm turned "
                  "up stays there. Grenades pass through (your palm catches them)."),
        toggle("Push Boxes With the Fist", vr_box3d_hand_push_fist)
            .help("Your hands push explosive boxes where the drawn fist meets them. Off: a small ball at the hand's point, "
                  "which met a box before the fist did."),
        toggle("Weapons Push Things", vr_box3d_weapon_push)
            .help("The weapons you hold are solid to loose things as drawn: shove things with a gun, balance them on it, bat a "
                  "grenade away with the axe."),
        cycle("Guns' Shape", vr_box3d_gun_pieces, {{1.f, "One Hull"}, {6.f, "6 Pieces"}, {12.f, "12 Pieces"}, {20.f, "20 Pieces"}})
            .help("The guns' solid shape, held and lying about: in convex pieces that follow the drawn gun (a shell drops "
                  "into the shotgun's port, a magazine into its well, nothing rests on the air between its parts), or one "
                  "hull round each (which fills them). For guns made from then on (picked up, dropped)."),
        slider("Heaviest Thing Held Up", vr_box3d_hand_hold_mass, 0.f, 50.f, 1.f, "%.0f kg")
            .extend()
            .help("An open hand or a weapon holds up nothing heavier: a heavier thing slips through (it is still pushed). 0: no "
                  "limit."),
        slider("Throw Grace", vr_box3d_throw_grace, 0.f, 0.5f, 0.05f, "%.2f s")
            .extend(0.f, 2.f)
            .help("How long a thing you throw passes through your hands (and the weapon you hold) as it leaves them: it keeps "
                  "the direction and spin you threw it with instead of being knocked by the hand still moving. 0: off."),
        slider("Throw Grace From", vr_box3d_throw_grace_speed, 0.f, 3.f, 0.25f, "%.2f m/s")
            .extend(0.f, 10.f)
            .help("How fast a throw (or a wrist snap) must be for Throw Grace. Slower, opening your hand held still, the thing "
                  "stays on your palm."),
        toggle("Throw Grace for the Body", vr_box3d_throw_grace_body)
            .help("During Throw Grace the thing also passes through your body (an overhead wrist snap throwing it down "
                  "across you)."),
        slider("Hand Push Mass", vr_box3d_hand_mass, 0.f, 20.f, 0.5f, "%.1f kg")
            .extend(0.f, 100.f)
            .help("How heavy your hand is to what it knocks: a thing keeps hand / (hand + its weight) of the hand's speed, so "
                  "a flick barely moves a heavy box and still bats a grenade. Also each hand holding a prop, added to the "
                  "prop's own weight. 0: no limit (a hand knocks anything as if it weighed nothing)."),
        slider("Arm Behind Weapon", vr_box3d_weapon_arm_mass, 0.f, 20.f, 0.5f, "%.1f kg")
            .extend(0.f, 100.f)
            .help("As Hand Push Mass, for a weapon in your hand swung into things: it knocks with its own weight (Weapon "
                  "Weights: Mass) and this much of your arm, so a rocket launcher or the hammer bats harder than the axe "
                  "or the shotgun."),
        slider("Push Force", vr_box3d_push_force, 0.f, 1000.f, 25.f, "%.0f N")
            .extend(0.f, 5000.f)
            .help("After a hand or weapon has hit a thing, pushing on into it shoves it with no more than this force: a heavy "
                  "box moves as far as you push it and stops, a light one is carried along. 0: no limit."),
        slider("Push Strength", vr_carry_nudge, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("How hard a touch knocks a thing loose, or pushes what the hands' and weapons' bodies don't (both off)."),
        slider("Box Throw Speed", vr_carry_throw_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        slider("Throws Keep Your Motion", vr_carry_throw_inherit, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of your own velocity a thing you throw takes with it, as a thrown weapon does (1: all: a brick "
                  "thrown running forward flies further, running back shorter). 0: none (as before)."),
        slider("Box Punch Damage", vr_carry_melee_mult, 1.f, 3.f, 0.1f, "%.1fx").extend().help("Punching with a box in hand."),
        slider("Thrown Box Damage", vr_carry_throw_damage, 0.f, 50.f, 1.f, "%.0f").extend().help("Damage of a box thrown at about 6 m/s; more the faster."),
        slider("Heavy Throws From", vr_throw_heavy_from, 0.f, 40.f, 1.f, "%.0f kg").extend(0.f, 200.f)
            .help("Props heavier than this (a crate, a barrel) hurt more thrown: by their mass over it (Heavy Throw Curve), "
                  "up to Heavy Throw Most. A barrel thrown with both hands hurt for about 14 before. 0: off. Never weapons."),
        slider("Heavy Throw Curve", vr_throw_heavy_exp, 0.f, 2.f, 0.05f, "%.2f")
            .help("1: a prop twice as heavy as Heavy Throws From hurts twice as much thrown; 0: no more."),
        slider("Heavy Throw Most", vr_throw_heavy_max, 1.f, 10.f, 0.25f, "x%.2f")
            .help("The most a heavy prop's throw is multiplied by."),
        open("Flung Props (Throwing and Physics)", pageIndex(pageThrowing))
            .help("Whether props batted, knocked flying or swung on the grapple hurt monsters (and players), and how much."),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets))
            .help("The grip, fingers and melee points of what a hand carries."),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights))
            .help("The mass, spring and damage of what a hand carries (Aiming: Weight for how weight feels)."),
        header("Physics Sounds"),
        slider("Physics Sounds", vr_physsound, 0.f, 2.f, 0.1f, "%.1f")
            .help("Volume of what things say as physics moves them: knocks as they land, bounce and hit each other, "
                  "scrapes as they slide, a climbing hand taking a hold (0 off; over 1 louder than recorded, up to 2). By what they are made of: wood (crates, "
                  "torches), metal (weapons, ammo, armour), stone, brick, flesh (gibs, heads), a backpack's thud."),
        slider("Knocks", vr_physsound_impact, 0.f, 1.f, 0.1f, "%.1f")
            .help("Things hitting the floor, walls, doors and each other: louder the harder and the heavier, a heavy "
                  "thing's deeper (0 off)."),
        slider("Quietest Knock", vr_physsound_min_speed, 1.f, 5.f, 0.25f, "%.2f m/s").extend(1.f, 20.f)
            .help("A hit slower than this is silent (1.5 m/s: a drop of 4 cm). Raise it if things resting on each other "
                  "tick."),
        slider("Loudest Knock From", vr_physsound_full_speed, 3.f, 20.f, 0.5f, "%.1f m/s").extend(1.f, 50.f)
            .help("A hit this hard or harder is at full volume (a drop of 1 m hits at 7.8 m/s)."),
        slider("Knock Spacing", vr_physsound_interval, 0.f, 0.5f, 0.02f, "%.2f s").extend(0.f, 2.f)
            .help("A thing knocks at most this often (a hit twice as loud sooner): a box rattling to rest or a stack "
                  "settling doesn't chatter."),
        slider("Bodies", vr_physsound_bodies, 0.f, 1.f, 0.1f, "%.1f").extend(0.f, 2.f)
            .help("Ragdolls and corpses knocking as they fall, tumble, are thrown or hit by things: a heavy thud for the "
                  "torso, softer for a limb, a squish for a small part; each body at most every two Knock Spacings (0 off)."),
        slider("Quietest Body Knock", vr_physsound_body_min_speed, 1.f, 5.f, 0.25f, "%.2f m/s").extend(1.f, 20.f)
            .help("A body's hit slower than this is silent (2 m/s: a drop of 20 cm). Raise it if a pile of bodies "
                  "settling thumps."),
        slider("Scrapes", vr_physsound_scrape, 0.f, 1.f, 0.1f, "%.1f")
            .help("Things sliding along the floor or each other (shoved, dragged, skidding after a throw): louder the "
                  "faster and the heavier; they stop as the thing stops (0 off)."),
        slider("Quietest Scrape", vr_physsound_scrape_min, 0.1f, 2.f, 0.1f, "%.1f m/s").extend(0.f, 5.f)
            .help("A slide slower than this is silent."),
        slider("Loudest Scrape From", vr_physsound_scrape_full, 1.f, 8.f, 0.5f, "%.1f m/s").extend(0.5f, 20.f)
            .help("A slide this fast or faster scrapes at full volume."),
        open("Climbing Grab Sound: Climbing", pageIndex(pageClimbing)).help("The climbing grab sound is on Movement > Climbing."),
        header("Explosive Boxes"),
        toggle("Physics Explosive Boxes", vr_explobox_physics)
            .help("The explosive boxes can be pushed, tipped over, stacked and carried by hand (heavy; never force "
                  "grabbed). Off: fixed in place, as in id's Quake. Next map."),
        slider("Blow Up On Impact", vr_explobox_impact, 0.f, 30.f, 1.f, "%.0f m/s").extend(0.f, 100.f)
            .help("A box hitting something this hard blows up: dropped from about 85 units or more (tipped over, its "
                  "top lands at 11 m/s). 0: never."),
        header("Armour and Pickups"),
        cycle("Armour", vr_armor_wear, {{0.f, "Touch takes it"}, {1.f, "Wear by hand"}})
            .help("Wear by hand: grip the armour to carry it and let go of it over your chest to put it on (only if it is better "
                  "than yours). Walking over it no longer takes it. Next map."),
        slider("Armour Size", vr_armor_scale, 0.3f, 1.f, 0.05f, "%.2fx").extend().help("The carried armour's size (1: Quake's, a metre tall). Next map."),
        cycle("Weapons and Keys", vr_item_objects, {{0.f, "Touch takes them"}, {1.f, "Objects"}})
            .help("Objects: the map's weapons, keys, runes and suits hang spinning until you grab, knock or force-grab them, "
                  "then they are physics objects. A weapon you grip is yours at once; keys, runes and suits you carry to a "
                  "holster and let go of there. Powerups are as before. Next map."),
        cycle("Weapon Pickups Look", vr_pickup_prop_models, {{0.f, "Classic models"}, {1.f, "As held and dropped"}})
            .help("As held and dropped: the map's weapons are drawn with the same models as the weapons in your hands and "
                  "the ones you drop, at their size, spinning about their middle. Classic models: id's pickup models (g_*.mdl). A weapon "
                  "without a model of its own keeps the classic one. Next map."),
    };
}

[[nodiscard]] za::Vector<Item> pageBurning();

// Split from Carrying and Gibs: the wall torches you can take.
[[nodiscard]] za::Vector<Item> pageWallTorches()
{
    return {
        toggle("Take Torches Off Walls", vr_walltorch)
            .help("Grip a wall torch to take it immediately (or force grab it): it is a burning club, lighting the "
                  "room round you as the wall torch did. Held, it burns for ever; dropped, thrown or used up by blows, "
                  "its fire dies. Grip and fingers: Held Object Offsets, holding it. Off: fixed, as in id's Quake. Next map."),
        slider("Grab Reach", vr_walltorch_reach, 0.f, 30.f, 1.f, "%.0f cm").extend(0.f, 60.f)
            .help("A grip this near a torch on its wall (its stick's middle, from its butt to its flame; your hand or "
                  "your fist's middle, whichever is nearer) takes hold of it, though your fist isn't quite on it. 0: only "
                  "a fist on it."),
        slider("Grab Window", vr_walltorch_grab_time, 0.f, 1.5f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("A grip closed on the way to a torch on its wall, still held, takes it as your hand gets there, up to "
                  "this long after you closed it (not a punch: a fast fist goes through). 0: only a grip closed there."),
        toggle("Force Grab Torches", vr_walltorch_forcegrab)
            .help("Torches come off their walls, and from where they lie, to a force grab."),
        slider("Blows Before It Dies", vr_walltorch_hits, 0.f, 20.f, 1.f, "%.0f").extend(0.f, 100.f)
            .help("A torch's fire starts dying after this many blows, even in your hand. 0: blows never use it up."),
        slider("Dying Time", vr_walltorch_die_time, 1.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long a dying torch's fire takes to go out: dropped, thrown or out of blows. Picked up again before "
                  "it is out, a dropped torch burns up again; one out of blows goes on dying."),
        slider("Blow Damage", vr_walltorch_damage, 0.f, 40.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("A torch's blow, times the blow's strength (a gun's is 12, the axe's 20)."),
        open("Burning", pageIndex(pageBurning))
            .help("A lit torch's blow sets what it hits on fire: Combat > Burning (its damage, time and flames)."),
        toggle("Light Again", vr_walltorch_relight)
            .help("A dying or burnt-out torch held in another torch's flame (on a wall or in your other hand), in one of "
                  "the map's flames (braziers, flame balls) or dipped in lava lights again, as new."),
        toggle("Shot Off Its Wall", vr_walltorch_shot)
            .help("A wall torch hit by anything you shoot (a shotgun's pellets, a nail, a rocket, a grenade, a laser) or "
                  "by a thing you throw into it hard comes off its wall: it falls lit, and its light on the wall is gone. "
                  "Your hand and a punch take it as before. Off: shots go through it, as in id's Quake. Next map."),
        slider("Flame Size", vr_walltorch_flame, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("A taken torch's flame (1: the wall torch's)."),
        toggle("Taken Torch Casts Shadows", vr_walltorch_shadows)
            .help("A taken torch's light casts shadows (your hands and body, what is round you), whatever Graphics' Torch "
                  "Light Shadows says. Its brightness is the wall torch's: Graphics' Torch Light Brightness."),
    };
}

// Combat > Burning (QC vr_burning.qc): what a lit torch, a thrown one or a lava nail sets on fire; crates; nails through
// a torch's flame.
[[nodiscard]] za::Vector<Item> pageBurning()
{
    return {
        slider("Burn Damage", vr_burn_damage, 0.f, 20.f, 0.5f, "%.1f / s").extend(0.f, 100.f)
            .help("A fire's damage a second, however many flames it has and however often it is lit again: it never "
                  "stacks. 0: nothing alive is set on fire."),
        slider("Burn Time", vr_burn_time, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("How long a monster burns from the last hit that lit it (a hit while it burns starts it again)."),
        slider("Flames Spread To", vr_burn_flames, 1.f, 10.f, 1.f, "%.0f").extend(1.f, 20.f)
            .help("A fire starts as one flame where it was struck and spreads over the body, a smaller flame at a time, "
                  "up to this many."),
        slider("Most Flames", vr_burn_flames_max, 1.f, 12.f, 1.f, "%.0f").extend(1.f, 30.f)
            .help("More hits add a flame each (a few a second at most), up to this many on one body. The damage stays "
                  "one fire's."),
        slider("Spread Time", vr_burn_spread, 0.1f, 3.f, 0.1f, "%.1f s").extend(0.05f, 10.f)
            .help("Time between flames spreading."),
        slider("Flame Size", vr_burn_flame_size, 0.2f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 4.f)
            .help("The first flame's size (id's small flame: 1)."),
        slider("Spread Flames' Size", vr_burn_flame_small, 0.2f, 1.f, 0.05f, "%.2fx").extend(0.05f, 2.f)
            .help("The other flames' size, times the first's (each a little more or less)."),
        slider("Spread Flames' Lean", vr_burn_flame_tilt, 0.f, 30.f, 1.f, "%.0f deg").extend(0.f, 60.f)
            .help("How far the other flames lean, at most; they stay upright. 0: all straight up."),
        header("Corpses"),
        toggle("Corpses Burn", vr_burn_corpses)
            .help("Corpses catch fire too, and a monster that dies burning burns on as a corpse. Corpses are the ones "
                  "Gibs and Corpses' Gib Corpses keeps."),
        slider("Corpse Burn Time", vr_burn_corpse_time, 0.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f),
        slider("Corpse Burn Damage", vr_burn_corpse_damage, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("A burning corpse's damage, times Burn Damage: enough of it gibs it (Corpse Health). 0: it just burns."),
        toggle("Bodies Burn in Lava", vr_burn_lava_bodies)
            .help("Ragdolls, knocked-down monsters and corpses in lava catch fire there, and burn on in it."),
        slider("Burnt Through in Lava", vr_burn_lava_gib, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A body this long in lava bursts into gibs (a knocked-down one is killed). 0: never."),
        slider("Smoke After Flames", vr_smoulder_burn_time, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("A burning monster or corpse smokes while it burns, and this long after its flames go out, thinning out "
                  "(how much: Gore > Lightning Shock > Smouldering Smoke)."),
        header("What Sets Things on Fire"),
        toggle("Torch Touch", vr_burn_touch)
            .help("A lit torch, held or thrown, sets a monster or a corpse on fire just by touching it: no blow needed. "
                  "Mapper flames touching your body burn you in the same way as a dropped lit torch. "
                  "Off: a lit torch's blow, or one thrown into it."),
        toggle("Lava Nails", vr_burn_lava_nails)
            .help("Your lava nails (the nailgun's and super nailgun's lava ammo) set what they hit on fire."),
        toggle("Nails Through a Flame", vr_burn_nail_convert)
            .help("A nail fired through a lit torch's flame (one in your other hand, thrown, lying or on its wall) becomes a "
                  "lava nail: more damage, and it sets what it hits on fire."),
        slider("Flame's Reach for Nails", vr_burn_nail_reach, 2.f, 30.f, 1.f, "%.0f").extend(0.f, 80.f)
            .help("How near a torch's flame a nail must pass, in units (a hand is about 4)."),
        slider("Nail Sizzle Volume", vr_burn_nail_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the short fizz a nail makes as it catches fire in a torch's flame (0 off)."),
        slider("Crackle Volume", vr_burn_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("A burning monster, corpse or crate crackles as a wall torch does, heard close by, quieter as its flames "
                  "die (0 off)."),
        slider("Most Crackling", vr_burn_sound_max, 1.f, 8.f, 1.f, "%.0f").extend(1.f, 32.f)
            .help("At most this many burning things crackle at once; the rest burn silent until one goes out."),
        header("Crates"),
        toggle("Crates Burn", vr_burn_crates)
            .help("Wooden crates catch fire: a lit torch's blow or touch, a lava nail, a burning crate touching them."),
        toggle("Wood Burns in Lava", vr_burn_lava)
            .help("Crates, barrels and their pieces dropped in lava catch fire there, and burn on in it."),
        slider("Crate Burn Time", vr_burn_crate_time, 1.f, 60.f, 1.f, "%.0f s").extend(0.5f, 300.f)
            .help("How long a crate burns before it breaks (or its fire goes out)."),
        toggle("Burnt Crates Break", vr_burn_crate_break)
            .help("A crate that has burnt its time breaks into its pieces. Off: its fire goes out."),
        slider("Fire Spreads After", vr_burn_crate_spread, 0.f, 20.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long a crate burns before the crates touching it catch fire. 0: fire never spreads."),
        slider("Touching Within", vr_burn_crate_gap, 0.f, 16.f, 1.f, "%.0f").extend(0.f, 64.f)
            .help("How far apart two crates still count as touching, in units."),
        slider("Crate Flames", vr_burn_crate_flames, 1.f, 12.f, 1.f, "%.0f").extend(1.f, 30.f)
            .help("Flames a crate's fire spreads to (Most Flames caps it)."),
        slider("Crate Flame Size", vr_burn_crate_flame_size, 0.5f, 2.5f, 0.05f, "%.2fx").extend(0.1f, 5.f)
            .help("A crate's flames, times a body's (Flame Size)."),
        toggle("Burnt Crates' Pieces Charred", vr_burn_crate_char)
            .help("A crate that burns through breaks into charred, blackened pieces that don't burn again. Off: plain pieces, "
                  "which burn as any other."),
        toggle("Pieces Burn", vr_burn_pieces)
            .help("The pieces of a crate you break catch fire as crates do: a lit torch's blow or touch, a lava nail, burning "
                  "crates and pieces near them. A crate broken while burning scatters burning pieces."),
        slider("Piece Burn Time", vr_burn_piece_time, 1.f, 30.f, 0.5f, "%.1f s").extend(0.5f, 120.f)
            .help("How long a piece burns before it burns away (charred from half of it)."),
        slider("Pieces Catch Within", vr_burn_piece_gap, 0.f, 32.f, 1.f, "%.0f").extend(0.f, 96.f)
            .help("How near a burning piece and another piece or a crate catch from each other, in units (Fire Spreads "
                  "After: when)."),
        slider("Piece Flames", vr_burn_piece_flames, 1.f, 8.f, 1.f, "%.0f").extend(1.f, 20.f)
            .help("Flames a piece's fire spreads to (Most Flames caps it)."),
        slider("Piece Flame Size", vr_burn_piece_flame_size, 0.3f, 2.f, 0.05f, "%.2fx").extend(0.1f, 5.f)
            .help("A piece's flames, times a body's (Flame Size)."),
        header("Torch Flame"),
        slider("Swing Lean", vr_walltorch_lean, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 5.f)
            .help("How far a swung torch's flame leans and trails behind its motion (1: the default; 0: always straight up)."),
        slider("Flatten When Fast", vr_walltorch_flatten, 0.f, 1.5f, 0.1f, "%.1fx").extend(0.f, 1.5f)
            .help("How much a fast swing flattens the flame and stretches it back (1: the default; 0: never)."),
        slider("Hand Motion Strength", vr_walltorch_hand_motion, 0.f, 4.f, 0.1f, "%.1fx")
            .help("Hand translation and rotation move the flame's attachment point and make it lean/stretch. 0: locomotion only."),
        slider("Motion Smoothing", vr_walltorch_motion_smooth, 0.01f, 0.5f, 0.01f, "%.2f s"),
        slider("Upside Down: Flame Height", vr_walltorch_inv_size, 0.05f, 0.3f, 0.01f, "%.2fx")
            .help("One flame stays attached to the head. Past 90 degrees it smoothly shortens to this height at 180 degrees; fire particles continue to rise."),
        open("Fire Particles", pageIndex(pageFireParticles)),
        slider("Upside Down: Brightness", vr_walltorch_inv_light, 0.5f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("Held head down, its light's brightness, times the upright one's."),
        slider("Upside Down: Burning Drips", vr_walltorch_drips, 0.f, 20.f, 1.f, "%.0f / s").extend(0.f, 60.f)
            .help("Burning drips falling off the head of a torch held head down, a second (0: none)."),
        slider("Upside Down: More Smoke", vr_walltorch_inv_smoke, 1.f, 5.f, 0.25f, "%.2fx").extend(0.f, 10.f)
            .help("Held head down, a torch smokes this much more."),
        slider("Torch Smoke", vr_walltorch_smoke, 0.f, 3.f, 0.25f, "%.2fx").extend(0.f, 10.f)
            .help("Soot rising off every lit torch, on its wall, in your hand or lying (1: three puffs a second; 0: none)."),
        slider("Torch Smoke Opacity", vr_walltorch_smoke_alpha, 0.f, 1.f, 0.05f, "%.2f")
            .help("How dark the torches' smoke is."),
        header("Your Own Torch"),
        toggle("Its Flame Burns You", vr_burn_self)
            .help("The flame of the torch you hold, kept on your other hand, an arm, your body or your head, sets you on fire "
                  "there after a moment (Catch Fire After), as it does a monster: flames on you, burns on your skin, Burn "
                  "Damage. Till then the hand buzzes, harder as it comes. Mapper flames touching your hands or arms use "
                  "the same warning and delay."),
        slider("Catch Fire After", vr_burn_self_time, 0.f, 3.f, 0.1f, "%.1f s").extend(0.f, 10.f)
            .help("How long the flame must stay on you before you catch fire (off it, it eases back twice as fast)."),
        slider("Warning Buzz", vr_burn_self_haptic, 0.f, 2.f, 0.1f, "%.1fx")
            .help("How hard the hand buzzes while a flame is on you, before you catch fire or drop the torch (0: none)."),
        toggle("Upside Down Burns Your Hand", vr_burn_drop)
            .help("Held head down, a torch's flame climbs the stick to your hand: kept there a moment (Drop After), it burns "
                  "your hand and you drop the torch."),
        slider("Drop After", vr_burn_drop_time, 0.f, 5.f, 0.1f, "%.1f s").extend(0.f, 20.f)
            .help("How long a torch held head down can burn your hand before you drop it."),
    };
}

// Split from Carrying and Gibs: rocks and bricks lying about.
[[nodiscard]] za::Vector<Item> pageRocksBricks()
{
    return {
        toggle("Rocks and Bricks", vr_debris)
            .help("Rocks lie on natural ground and at the foot of rock and stone walls, bricks at the foot of brick walls, "
                  "by what the textures are: pick them up, force grab them, punch with them, throw them. The same "
                  "places at every load. Single player. Next map."),
        toggle("Rocks", vr_debris_rocks).help("Rocks on grass, dirt and rock, and by rock and stone walls. Next map."),
        toggle("Bricks", vr_debris_bricks).help("Bricks at the foot of brick walls. Next map."),
        slider("Chance", vr_debris_chance, 0.f, 1.f, 0.01f, "%.2f").extend()
            .help("How likely a place at a wall's foot (one every 16 units along it) gets a piece or a few, before the "
                  "limits below. Next map."),
        slider("In Corners", vr_debris_corner, 1.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("Times the chance in a corner. Next map."),
        slider("In the Dark", vr_debris_dark, 0.f, 1.f, 0.05f, "%.2fx")
            .help("Times the chance where the floor is dark (where you would not see them). Next map."),
        slider("Most Together", vr_debris_cluster, 1.f, 6.f, 1.f, "%.0f").help("Most pieces lying together at one place. Next map."),
        slider("Most in a Map", vr_debris_max, 0.f, 400.f, 10.f, "%.0f").extend(0.f, 2000.f)
            .help("Fewer if the map has few entities to spare (vr_debris_edicts_left). Next map."),
        debrisMultiplayerSlider(),
        slider("Most in an Area", vr_debris_area_max, 1.f, 30.f, 1.f, "%.0f").extend(1.f, 200.f)
            .help("Most pieces in a square of vr_debris_area_size units (384: about 12 m). Next map."),
        slider("Spacing", vr_debris_spacing, 0.f, 256.f, 8.f, "%.0f units").extend(0.f, 2048.f)
            .help("Between places with pieces (32 units is about a metre). Next map."),
        slider("Size Variation", vr_debris_size, 0.f, 0.4f, 0.01f, "+-%.2f")
            .help("How much bigger or smaller a piece may be (bricks 40% of it). Next map."),
        slider("Layout", vr_debris_seed, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 100000.f)
            .help("Another number, another layout: each is the same at every load. Next map."),
    };
}

// Wooden crates (vr_crates*; vr_crates.cpp, QC vr_crates.qc): where they lie, breaking them, what they hold, hiding.
[[nodiscard]] za::Vector<Item> pageCrates()
{
    return {
        header("Where They Lie"),
        toggle("Crates", vr_crates)
            .help("Wooden crates, small and large, stand by the walls and in corners, some stacked two high: push them, "
                  "carry them, stand on them, break them. The same places at every load. Single player. Next map."),
        slider("Density", vr_crates_chance, 0.f, 0.3f, 0.005f, "%.3f").extend(0.f, 1.f)
            .help("How likely a place by a wall (one every 24 units along it) gets a crate, before the limits below. Next map."),
        slider("In Corners", vr_crates_corner, 1.f, 10.f, 0.5f, "%.1fx").extend(0.f, 50.f)
            .help("Times the chance in a corner. Next map."),
        slider("Most in a Map", vr_crates_max, 0.f, 60.f, 1.f, "%.0f").extend(0.f, 400.f)
            .help("A stack counts two. Next map."),
        slider("Spacing", vr_crates_spacing, 32.f, 1024.f, 16.f, "%.0f units").extend(0.f, 4096.f)
            .help("Between crates (a stack's two aside; 32 units is about a metre). Next map."),
        slider("Stacked", vr_crates_stack, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a crate has another on it, turned and off its middle a little. Next map."),
        slider("Large Ones", vr_crates_large, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of large crates (40 x 40 x 48 units; the small ones 32 units a side). Next map."),
        slider("Barrels", vr_crates_barrels, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of barrels among them (upright, 22 units across and 32 high), and of the tops of stacks: a "
                  "barrel on a crate or a barrel, a small crate on a barrel. Next map."),
        slider("Barrels Lying", vr_crates_barrel_lying, 0.f, 1.f, 0.05f, "%.2f")
            .help("The share of barrels standing alone that lie on their side along the wall (they roll when pushed). "
                  "Next map."),
        slider("Room in Front", vr_crates_clearance, 32.f, 256.f, 8.f, "%.0f units").extend(0.f, 1024.f)
            .help("Open floor kept in front of a crate, so that it never blocks a passage or a doorway. Next map."),
        slider("Away From Things", vr_crates_margin, 24.f, 256.f, 8.f, "%.0f units").extend(0.f, 1024.f)
            .help("How far from other things (items, monsters, lights; doors, lifts and teleporters further, the start "
                  "furthest). Next map."),
        slider("Layout", vr_crates_seed, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 100000.f)
            .help("Another number, another layout: each is the same at every load. Next map."),
        header("Breaking"),
        slider("Health", vr_crate_health, 0.f, 100.f, 5.f, "%.0f").extend(0.f, 1000.f)
            .help("A small crate's (a large one's 1.6 times): shots, blows, explosions and hard hits wear it down. A shotgun "
                  "blast does up to 24, the axe about 20. 0: they never break."),
        slider("Breaks On Impact", vr_crate_impact, 0.f, 30.f, 1.f, "%.0f m/s").extend(0.f, 100.f)
            .help("A crate hitting something this hard breaks (dropped from about 85 units); from 70% of it, it is damaged. "
                  "Things thrown at it damage it as they do monsters. 0: impacts never break them."),
        toggle("Overhead Slam", vr_carry_slam)
            .help("A crate or barrel held in both hands over your head and swung down onto a monster, a prop or a wall breaks "
                  "at once (what it hits takes a heavy blow); thrown down from there, it takes heavy damage (two such "
                  "throws break it)."),
        slider("Slam: Over the Head", vr_carry_slam_height, -20.f, 40.f, 1.f, "%.0f cm").extend(-100.f, 100.f)
            .help("How far over your eyes your hands must be for it to count as over your head."),
        slider("Slam: Swing Speed", vr_carry_slam_speed, 1.f, 8.f, 0.25f, "%.2f m/s")
            .help("How fast your hands must swing it down for it to break on what it meets."),
        slider("Slam: Time to Swing", vr_carry_slam_window, 0.2f, 3.f, 0.1f, "%.1f s")
            .help("How long after it was over your head a swing down still counts."),
        slider("Slam: Damage", vr_carry_slam_damage, 0.f, 5.f, 0.1f, "x%.1f")
            .help("What it is slammed onto takes this times a punch with it in hand (more the faster, up to twice)."),
        slider("Thrown Down: Damage", vr_crate_slam_throw_damage, 0.f, 1.f, 0.05f, "%.2f")
            .help("Let go of from over your head, its first hard landing takes this share of its full health (0.55: two "
                  "such throws break it)."),
        slider("Thrown Down: Landing Speed", vr_crate_slam_throw_speed, 0.f, 10.f, 0.5f, "%.1f m/s")
            .help("... a landing at least this fast."),
        slider("Pieces", vr_crate_pieces, 0.f, 16.f, 1.f, "%.0f").extend(0.f, 40.f)
            .help("How many boards and splinters a small crate breaks into (a large one more). Light: they barely get in "
                  "your way, and turn to dust when hit."),
        slider("Most Pieces", vr_crate_piece_max, 0.f, 128.f, 4.f, "%.0f").extend(0.f, 512.f)
            .help("Most pieces lying about at once: past it, the oldest turn to dust."),
        slider("Pieces Last", vr_crate_piece_time, 0.f, 120.f, 5.f, "%.0f s").extend(0.f, 3600.f)
            .help("How long a piece lies about before it fades away (0: for ever). One in your hand stays."),
        header("What They Hold"),
        slider("Ammo", vr_crate_ammo, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a crate holds a small box of ammunition, for a weapon you have (or the shotgun)."),
        slider("Health Box", vr_crate_health_box, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a crate holds a small health box (15)."),
        slider("Pop Out", vr_crate_item_pop, 0.f, 8.f, 0.5f, "%.1f m/s").extend(0.f, 20.f)
            .help("How hard what a crate held pops out when it breaks: up, and a little outwards (along the blow). 0: it "
                  "is left lying where the crate stood."),
        slider("Crowbar on Crates", vr_crate_crowbar, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance a crate placed about the map (the top one of a stack) has a crowbar lying on it; the same "
                  "crates each load. Next map."),
        slider("Most Crowbars", vr_crate_crowbar_max, 0.f, 4.f, 1.f, "%.0f").extend(0.f, 64.f)
            .help("Most crowbars lying on crates in a map. Next map."),
        header("Hiding"),
        toggle("Crates Hide You", vr_crate_sight)
            .help("Monsters can't see you through crates and explosive boxes lying about (crouch behind a small one). "
                  "Carrying one, they see you through it."),
        cycle("Held Crate Shields You", vr_crate_shield,
            {{0.f, "Off"}, {1.f, "Crates"}, {2.f, "Crates and Explosive Boxes"}})
            .help("A crate you hold up takes monsters' bullets meant for you (enough of them break it). Crates and "
                  "Explosive Boxes: an explosive box too, which then blows up in your hands (vr_crate_shield)."),
    };
}

[[nodiscard]] za::Vector<Item> pageRagdolls(); // (below)
[[nodiscard]] za::Vector<Item> pageRagdollGrunt();
[[nodiscard]] za::Vector<Item> pageRagdollKnight();
[[nodiscard]] za::Vector<Item> pageCorpseDamage();
[[nodiscard]] za::Vector<Item> pageRagdollOgre();
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer();
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight();
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler();
[[nodiscard]] za::Vector<Item> pageRagdollScrag();
[[nodiscard]] za::Vector<Item> pageRagdollZombie();
[[nodiscard]] za::Vector<Item> pageRagdollMummy();
[[nodiscard]] za::Vector<Item> pageRagdollGremlin();
[[nodiscard]] za::Vector<Item> pageRagdollVore();
[[nodiscard]] za::Vector<Item> pageRagdollCentroid();
[[nodiscard]] za::Vector<Item> pageRagdollShambler();
[[nodiscard]] za::Vector<Item> pageRagdollFiend();

// Split from Carrying and Gibs: taking, throwing and bursting gibs, heads and corpses.
[[nodiscard]] za::Vector<Item> pageGibs()
{
    return {
        cycle("Gibs and Heads", vr_grab_gibs, {{0.f, "Left alone"}, {1.f, "Grab by hand"}, {2.f, "Hand and force grab"}})
            .help("Pick up and throw gibs and heads, by reaching for them (or force-grabbing them too)."),
        slider("Thrown Gib Damage", vr_gib_throw_damage, 0.f, 50.f, 1.f, "%.0f").extend().help("Damage of a gib or head thrown at about 6 m/s; more the faster."),
        toggle("Destroy Gibs", vr_gib_destroy)
            .help("Gibs and heads burst in a mist of blood when shot, blown up, struck, or thrown hard at a wall or a monster."),
        slider("Gib Health", vr_gib_health, 1.f, 60.f, 1.f, "%.0f").extend().help("The damage that destroys a gib; a head takes half as much again."),
        slider("Gib Splat Speed", vr_gib_splat_speed, 100.f, 600.f, 25.f, "%.0f").extend()
            .help("Units/s a thrown gib or head must hit a wall or a monster at to burst."),
        slider("Hard Throw Bursts", vr_gib_splat_throw, 0.f, 12.f, 0.5f, "%.1f m/s").extend(0.f, 30.f)
            .help("A gib or head thrown with the hand this fast or faster bursts on a wall instead of sticking, however "
                  "heavy (a heavy one leaves the hand slowly). Softer throws stick (Thrown Gibs Stick). 0: off."),
        toggle("Gib Corpses", vr_corpse_gib)
            .help("Corpses burst into gibs when shot, blown up or struck enough: shotguns, nails, lightning, rockets, fists, melee weapons."),
        slider("Corpse Health", vr_corpse_health_mult, 0.25f, 4.f, 0.05f, "%.2fx").extend(0.05f, 20.f)
            .help("Times every monster's corpse health: the damage that gibs a corpse or a ragdoll (each monster's own: Corpse "
                  "Damage and Health) (vr_corpse_health_mult)."),
        cycle("Never Gib Corpses", vr_corpse_nogib, {{0.f, "Off"}, {1.f, "Corpses and Ragdolls"}, {2.f, "And Dying Monsters"}})
            .help("For testing: corpses and ragdolls take hits (blood, pushes, blasts) but never burst into gibs, even blown "
                  "up. And Dying Monsters: a monster killed by a rocket dies whole too, so it becomes a ragdoll "
                  "(vr_corpse_nogib)."),
        open("Corpse Damage and Health", pageIndex(pageCorpseDamage)),
        header("Corpse Collision"),
        cycle("Corpses", vr_corpse_collide,
            {{0.f, "Pass Through"}, {1.f, "Fixed Box"}, {2.f, "Pushable Box"}, {3.f, "Fixed Pose"}, {4.f, "Pushable Pose"}})
            .help("Corpses in the physics: props, thrown things and held things meet them. Fixed: they never move. Pushable: "
                  "heavy, they slide when pushed (hands, held weapons, props, walking into them). Box: a low box round the "
                  "body; Pose: its shape as it lies (vr_corpse_collide)."),
        slider("Pushable Corpse Mass", vr_corpse_collide_mass, 20.f, 500.f, 10.f, "%.0f kg").extend()
            .help("Heavier: harder to push. Walking into one shoves it only under about 60 kg (vr_corpse_collide_mass)."),
        slider("Pushable Corpse Friction", vr_corpse_collide_friction, 0.1f, 2.f, 0.1f, "%.1f").extend()
            .help("How hard a pushable corpse drags on the floor (vr_corpse_collide_friction)."),
        toggle("Props Meet Corpses", vr_corpse_collide_props)
            .help("Boxes, weapons, gibs and heads lying or falling rest on corpses and stop at them (vr_corpse_collide_props)."),
        toggle("Thrown Things Meet Corpses", vr_corpse_collide_thrown)
            .help("Off: what you throw flies through corpses (its hit still hurts them) (vr_corpse_collide_thrown)."),
        toggle("Held Things Meet Corpses", vr_corpse_collide_held)
            .help("Your hands, held weapons and props push pushable corpses; a prop held in both hands stops at any "
                  "(vr_corpse_collide_held)."),
        cycle("You and Corpses", vr_corpse_collide_player, {{0.f, "Walk Through"}, {1.f, "Step Over"}, {2.f, "Solid"}})
            .help("Step Over: a corpse is a low step you walk onto. Solid: walk round it or jump onto it "
                  "(vr_corpse_collide_player)."),
        toggle("Monsters Step Over Corpses", vr_corpse_collide_monsters)
            .help("Monsters walk over corpses as a low step instead of through them (vr_corpse_collide_monsters)."),
        header("Ragdolls (Experimental)"),
        cycle("Ragdolls", vr_ragdoll, {{0.f, "Off"}, {1.f, "On (Experimental)"}})
            .help("A dying grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler or gremlin goes limp: his body becomes jointed parts that fall, tumble, are pushed, grabbed "
                  "and thrown (vr_ragdoll). Their settings: Ragdoll Settings."),
        open("Ragdoll Settings", pageIndex(pageRagdolls)),
    };
}

// Gibs and Corpses > Corpse Damage and Health (ROUND21.md, "Ragdolls on; corpse health and damage by kind"): how much
// damage it takes to gib a corpse or a ragdoll: each kind of hit's share of its damage (VR_Corpse_DamageKind, QC), each
// monster's corpse health (times Corpse Health, vr_corpse_health_mult).
[[nodiscard]] za::Vector<Item> pageCorpseDamage()
{
    return {
        open("Corpse Health, Never Gib: Gibs and Corpses", pageIndex(pageGibs)).help("Corpse Health and Never Gib Corpses are on Gibs and Corpses."),
        header("Damage to Corpses, by Weapon"),
        slider("Shotguns and Guns", vr_corpse_dmg_shots, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("The shotguns' pellets and the grunts' guns. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_shots)."),
        slider("Nails", vr_corpse_dmg_nails, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Nails, an enforcer's laser, knights' and scrags' spikes. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_nails)."),
        slider("Explosions", vr_corpse_dmg_explosions, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Rockets, grenades, plasma and every blast (a ragdoll is still thrown by them: Ragdoll Settings' Blast Throw). Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_explosions)."),
        slider("Lightning", vr_corpse_dmg_lightning, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("The lightning gun, Mjolnir's lightning, a discharge under water. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_lightning)."),
        slider("Blunt Melee", vr_corpse_dmg_blunt, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("A crowbar, a gun or Mjolnir swung, a pommel or a hilt. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_blunt)."),
        slider("Fists", vr_corpse_dmg_fists, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("An empty fist's punch. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_fists)."),
        slider("Bladed Melee", vr_corpse_dmg_blades, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("The axe's and a sword's edge or point, swung or thrown; a thrown axe stuck in it, bleeding it. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_blades)."),
        slider("Chainsaw", vr_corpse_dmg_chainsaw, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("The chainsaw's teeth. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_chainsaw)."),
        slider("Thrown Props", vr_corpse_dmg_props, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Thrown or flung boxes, gibs, heads and weapons (not blades), deflected shots, a rock or a brick in the fist. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_props)."),
        slider("Fire", vr_corpse_dmg_fire, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Burning (after Burning's Corpse Damage, vr_burn_corpse_damage). Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_fire)."),
        slider("Bashes and Shoves", vr_corpse_dmg_bash, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("A weapon's guard driven forward, both hands' shove, a headbutt. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_bash)."),
        slider("Other", vr_corpse_dmg_other, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Anything else: monsters' blows, a kick, a crusher, lava. Times the damage it does to corpses and ragdolls: more gibs them sooner, 0 never (vr_corpse_dmg_other)."),
        header("Corpse Health, by Monster"),
        slider("Grunt", vr_corpse_health_grunt, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_grunt; default 80)."),
        slider("Enforcer", vr_corpse_health_enforcer, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_enforcer; default 80)."),
        slider("Rottweiler", vr_corpse_health_dog, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_dog; default 80)."),
        slider("Fiend", vr_corpse_health_fiend, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_fiend; default 180)."),
        slider("Ogre", vr_corpse_health_ogre, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_ogre; default 140)."),
        slider("Knight", vr_corpse_health_knight, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_knight; default 80)."),
        slider("Hell Knight", vr_corpse_health_hellknight, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_hellknight; default 160)."),
        slider("Vore", vr_corpse_health_vore, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_vore; default 220)."),
        slider("Shambler", vr_corpse_health_shambler, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_shambler; default 280)."),
        slider("Scrag", vr_corpse_health_scrag, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_scrag; default 80)."),
        slider("Rotfish", vr_corpse_health_fish, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_fish; default 80)."),
        slider("Gremlin (Hipnotic)", vr_corpse_health_gremlin, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_gremlin; default 100)."),
        slider("Centroid (Rogue)", vr_corpse_health_scourge, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_scourge; default 180)."),
        slider("Eel (Rogue)", vr_corpse_health_eel, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("The damage that gibs its corpse or ragdoll, times Corpse Health (vr_corpse_health_eel; default 80)."),
        slider("Zombie (Beheaded)", vr_corpse_health_zombie, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("A zombie dies whole only beheaded (Gore > Decapitation): the damage that gibs its ragdoll then, times "
                  "Corpse Health (vr_corpse_health_zombie; default 60)."),
        slider("Mummy (Rogue, Beheaded)", vr_corpse_health_mummy, 10.f, 600.f, 10.f, "%.0f").extend(1.f, 5000.f)
            .help("A mummy dies whole only beheaded (its death gibs it otherwise): the damage that gibs its ragdoll then, "
                  "times Corpse Health (vr_corpse_health_mummy; default 120)."),
    };
}

// Gibs and Corpses > Ragdoll Settings (ROUND21.md, "Ragdolls"): on or off, when they go limp and how many; their physics
// for all (each monster class may have its own: its page); taking them by hand and by force grab; their blood.
[[nodiscard]] za::Vector<Item> pageRagdolls()
{
    return {
        open("Ragdolls On/Off: Gibs and Corpses", pageIndex(pageGibs)).help("Ragdolls on or off is on Gibs and Corpses."),
        slider("Go Limp At", vr_ragdoll_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("When in his death animation: 0 as soon as he stops being solid, 1 once he lies still (vr_ragdoll_start)."),
        slider("Most Ragdolls", vr_ragdoll_max, 0.f, 16.f, 1.f, "%.0f").extend(0.f, 64.f)
            .help("At most this many at once. New deaths replace the oldest dead ragdoll first, then the oldest living knockdown if necessary. 0: ragdolls off."),
        toggle("Ragdolls Meet Each Other", vr_ragdoll_collide_each)
            .help("Ragdolls fall onto and pile on each other; off, they pass through each other (vr_ragdoll_collide_each)."),
        header("Physics (All Monsters)"),
        slider("Mass", vr_ragdoll_mass, 20.f, 200.f, 5.f, "%.0f kg").extend()
            .help("A ragdoll's whole weight, its parts by their size, for a monster whose own Mass is Global: each has its "
                  "own by default (grunt 80, knight 90, enforcer 100, death knight 130, ogre 200, rottweiler and scrag 40, fiend 140, shambler 280, gremlin 20; "
                  "vr_ragdoll_mass)."),
        slider("Friction", vr_ragdoll_friction, 0.1f, 2.f, 0.1f, "%.1f").extend()
            .help("How much it drags and catches on floors and steps (vr_ragdoll_friction)."),
        slider("Joint Friction", vr_ragdoll_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend()
            .help("How hard the joints are to turn: 0 limp as a rag, more like a body freshly dead (vr_ragdoll_joint_friction)."),
        slider("Joint Stiffness", vr_ragdoll_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend()
            .help("A spring in each joint pulling it back towards how he stood: 0 none, 1 to 3 some tone left in the body "
                  "(vr_ragdoll_joint_stiffness)."),
        slider("Joint Limits", vr_ragdoll_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.05f, 3.f)
            .help("How far the joints bend and twist, times the body's own limits: less stiff, more loose "
                  "(vr_ragdoll_limits)."),
        slider("Limb Damping", vr_ragdoll_damping, 0.f, 3.f, 0.1f, "%.1f").extend()
            .help("Less flailing of the arms and legs (vr_ragdoll_damping)."),
        slider("Blast Throw", vr_ragdoll_blast, 0.f, 5.f, 0.25f, "%.2fx").extend()
            .help("How far explosions throw a ragdoll, times what they give a prop of its weight (vr_ragdoll_blast)."),
        slider("Death Motion Kept", vr_ragdoll_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("How much of his death animation's motion the parts keep as he goes limp (vr_ragdoll_inherit)."),
        header("In Water, Slime and Lava"),
        slider("Float in Water", vr_ragdoll_float_water, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("A body's lift under water, times its weight: above 1 it floats (face down: the torso floats more than "
                  "the limbs), below it sinks; armoured knights a little less. 0: none (vr_ragdoll_float_water)."),
        slider("Float in Slime", vr_ragdoll_float_slime, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("The same in slime (vr_ragdoll_float_slime)."),
        slider("Float in Lava", vr_ragdoll_float_lava, 0.f, 2.f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("The same in lava, where it burns (Combat > Burning, Bodies Burn in Lava) (vr_ragdoll_float_lava)."),
        slider("Water Drag", vr_ragdoll_drag_water, 0.f, 10.f, 0.25f, "%.2f").extend(0.f, 30.f)
            .help("How fast water slows a body and its spin, more the faster it goes: a fall into it is braked as it goes "
                  "in, a sink is slow. 0: none (vr_ragdoll_drag_water)."),
        slider("Slime Drag", vr_ragdoll_drag_slime, 0.f, 10.f, 0.25f, "%.2f").extend(0.f, 30.f)
            .help("The same in slime, thicker (vr_ragdoll_drag_slime)."),
        slider("Lava Drag", vr_ragdoll_drag_lava, 0.f, 10.f, 0.25f, "%.2f").extend(0.f, 30.f)
            .help("The same in lava, thickest (vr_ragdoll_drag_lava)."),
        header("Each Monster's Own"),
        open("Grunt", pageIndex(pageRagdollGrunt)),
        open("Knight", pageIndex(pageRagdollKnight)),
        open("Ogre", pageIndex(pageRagdollOgre)),
        open("Enforcer", pageIndex(pageRagdollEnforcer)),
        open("Death Knight", pageIndex(pageRagdollDeathKnight)),
        open("Rottweiler", pageIndex(pageRagdollRottweiler)),
        open("Scrag", pageIndex(pageRagdollScrag)),
        open("Fiend", pageIndex(pageRagdollFiend)),
        open("Shambler", pageIndex(pageRagdollShambler)),
        open("Vore", pageIndex(pageRagdollVore)),
        open("Gremlin", pageIndex(pageRagdollGremlin)),
        open("Centroid", pageIndex(pageRagdollCentroid)).help("A centroid's ragdoll (Scourge of Armagon's scorpion)."),
        open("Zombie", pageIndex(pageRagdollZombie)).help("A zombie's ragdoll: only when its head is cut off (Gore > Decapitation)."),
        open("Mummy", pageIndex(pageRagdollMummy)).help("A mummy's ragdoll (Dissolution of Eternity): only when its head is cut off (Gore > Decapitation)."),
        header("Taking Them"),
        cycle("Grab Ragdolls", vr_ragdoll_grab, {{0.f, "Never"}, {1.f, "By Hand"}})
            .help("Grip on a limb to take it: it follows your hand, the body hanging from it; let go to drop or throw it. "
                  "Bodies can only be taken by hand; force grabs do not target them."),
        slider("Grip Strength", vr_ragdoll_grab_force, 200.f, 4000.f, 100.f, "%.0f N").extend()
            .help("The most your hand pulls a held limb with: 3000 lifts a grunt by his chest (a unit's sag); 1500 he sags "
                  "about 18 units, less and you drag him rather than lift him (vr_ragdoll_grab_force)."),
        slider("Grip Reach", vr_ragdoll_grab_reach, 1.f, 16.f, 1.f, "%.0f units").extend()
            .help("How near a limb your hand must be to take it (vr_ragdoll_grab_reach)."),
        toggle("Hand on the Limb", vr_ragdoll_grab_fit)
            .help("A limb you take is brought onto your palm (or against your fingers), as a carried box is, and your "
                  "fingers close round it; off, it is held where your hand's middle was (vr_ragdoll_grab_fit)."),
        slider("Hand Follows Limb", vr_ragdoll_hand_stick, 0.f, 30.f, 1.f, "%.0f cm").extend()
            .help("How far your hand is drawn off your controller to stay on the limb it holds as the limb lags and "
                  "sags; 0: on the controller (vr_ragdoll_hand_stick)."),
        slider("Hand Turns with Limb", vr_ragdoll_hand_turn, 0.f, 90.f, 5.f, "%.0f deg").extend()
            .help("How far your hand is drawn turned with the limb as it twists in your hand (vr_ragdoll_hand_turn)."),
        slider("Throw", vr_ragdoll_throw, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("How much of your hand's throw a limb keeps when let go of (vr_ragdoll_throw)."),
        header("Blood"),
        toggle("Blood Trails", vr_ragdoll_blood)
            .help("A ragdoll's limbs flung fast leave blood trails and drops, as gibs do (vr_ragdoll_blood)."),
        header("Drawing"),
        toggle("Smooth Motion", vr_ragdoll_smooth)
            .help("Drawn between the server's last two steps, as props are: smooth in every frame. Off: the latest step, "
                  "which a 90 Hz headset shows every other frame (the server runs at 45 Hz there: choppy) (vr_ragdoll_smooth)."),
        toggle("Held Limbs Follow the Hand", vr_ragdoll_held_local)
            .help("A ragdoll you hold moves with your hand every frame, as a held prop does, rather than a step behind it "
                  "(vr_ragdoll_held_local)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Grunt: the grunt's own physics (vr_ragdoll_army_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollGrunt()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_army_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_army_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_army_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_army_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_army_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_army_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_army_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_army_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_army_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_army_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_army_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_army_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_army_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_army_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_army_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_army_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_army_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_army_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_army_start -1; vr_ragdoll_army_mass -1; vr_ragdoll_army_friction -1; "
                              "vr_ragdoll_army_joint_friction -1; vr_ragdoll_army_joint_stiffness -1; vr_ragdoll_army_limits -1; "
                              "vr_ragdoll_army_damping -1; vr_ragdoll_army_blast -1; vr_ragdoll_army_inherit -1")
            .help("The grunt's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Knight: the knight's own physics (vr_ragdoll_knight_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollKnight()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_knight_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_knight_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_knight_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_knight_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_knight_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_knight_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_knight_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_knight_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_knight_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_knight_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_knight_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_knight_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_knight_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_knight_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_knight_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_knight_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_knight_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_knight_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_knight_start -1; vr_ragdoll_knight_mass -1; vr_ragdoll_knight_friction -1; "
                              "vr_ragdoll_knight_joint_friction -1; vr_ragdoll_knight_joint_stiffness -1; vr_ragdoll_knight_limits -1; "
                              "vr_ragdoll_knight_damping -1; vr_ragdoll_knight_blast -1; vr_ragdoll_knight_inherit -1")
            .help("The knight's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Ogre: the ogre's own physics (vr_ragdoll_ogre_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollOgre()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_ogre_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_ogre_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_ogre_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_ogre_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_ogre_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_ogre_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_ogre_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_ogre_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_ogre_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_ogre_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_ogre_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_ogre_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_ogre_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_ogre_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_ogre_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_ogre_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_ogre_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_ogre_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_ogre_start -1; vr_ragdoll_ogre_mass -1; vr_ragdoll_ogre_friction -1; "
                              "vr_ragdoll_ogre_joint_friction -1; vr_ragdoll_ogre_joint_stiffness -1; vr_ragdoll_ogre_limits -1; "
                              "vr_ragdoll_ogre_damping -1; vr_ragdoll_ogre_blast -1; vr_ragdoll_ogre_inherit -1")
            .help("The ogre's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Enforcer: the enforcer's own physics (vr_ragdoll_enforcer_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_enforcer_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_enforcer_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_enforcer_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_enforcer_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_enforcer_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_enforcer_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_enforcer_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_enforcer_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_enforcer_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_enforcer_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_enforcer_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_enforcer_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_enforcer_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_enforcer_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_enforcer_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_enforcer_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_enforcer_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_enforcer_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_enforcer_start -1; vr_ragdoll_enforcer_mass -1; vr_ragdoll_enforcer_friction -1; "
                              "vr_ragdoll_enforcer_joint_friction -1; vr_ragdoll_enforcer_joint_stiffness -1; vr_ragdoll_enforcer_limits -1; "
                              "vr_ragdoll_enforcer_damping -1; vr_ragdoll_enforcer_blast -1; vr_ragdoll_enforcer_inherit -1")
            .help("The enforcer's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Death Knight: the death knight's own physics (vr_ragdoll_hknight_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_hknight_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_hknight_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_hknight_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_hknight_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_hknight_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_hknight_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_hknight_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_hknight_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_hknight_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_hknight_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_hknight_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_hknight_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_hknight_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_hknight_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_hknight_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_hknight_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_hknight_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_hknight_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_hknight_start -1; vr_ragdoll_hknight_mass -1; vr_ragdoll_hknight_friction -1; "
                              "vr_ragdoll_hknight_joint_friction -1; vr_ragdoll_hknight_joint_stiffness -1; vr_ragdoll_hknight_limits -1; "
                              "vr_ragdoll_hknight_damping -1; vr_ragdoll_hknight_blast -1; vr_ragdoll_hknight_inherit -1")
            .help("The death knight's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Rottweiler: the rottweiler's own physics (vr_ragdoll_dog_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_dog_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_dog_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_dog_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_dog_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_dog_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_dog_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_dog_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_dog_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_dog_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_dog_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_dog_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_dog_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_dog_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_dog_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_dog_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_dog_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_dog_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_dog_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_dog_start -1; vr_ragdoll_dog_mass -1; vr_ragdoll_dog_friction -1; "
                              "vr_ragdoll_dog_joint_friction -1; vr_ragdoll_dog_joint_stiffness -1; vr_ragdoll_dog_limits -1; "
                              "vr_ragdoll_dog_damping -1; vr_ragdoll_dog_blast -1; vr_ragdoll_dog_inherit -1")
            .help("The rottweiler's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Scrag: the scrag's own physics (vr_ragdoll_wizard_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollScrag()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_wizard_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_wizard_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_wizard_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_wizard_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_wizard_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_wizard_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_wizard_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_wizard_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_wizard_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_wizard_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_wizard_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_wizard_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_wizard_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_wizard_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_wizard_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_wizard_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_wizard_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_wizard_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_wizard_start -1; vr_ragdoll_wizard_mass -1; vr_ragdoll_wizard_friction -1; "
                              "vr_ragdoll_wizard_joint_friction -1; vr_ragdoll_wizard_joint_stiffness -1; vr_ragdoll_wizard_limits -1; "
                              "vr_ragdoll_wizard_damping -1; vr_ragdoll_wizard_blast -1; vr_ragdoll_wizard_inherit -1")
            .help("The scrag's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Zombie: the zombie's own physics (vr_ragdoll_zombie_*), each one Global (the one for
// all monsters) or its own. Its ragdoll is only made when it is beheaded (QC vr_decap.qc).
[[nodiscard]] za::Vector<Item> pageRagdollZombie()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_zombie_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_zombie_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_zombie_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_zombie_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_zombie_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_zombie_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_zombie_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_zombie_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_zombie_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_zombie_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_zombie_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_zombie_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_zombie_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_zombie_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_zombie_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_zombie_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_zombie_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_zombie_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_zombie_start -1; vr_ragdoll_zombie_mass -1; vr_ragdoll_zombie_friction -1; "
                              "vr_ragdoll_zombie_joint_friction -1; vr_ragdoll_zombie_joint_stiffness -1; vr_ragdoll_zombie_limits -1; "
                              "vr_ragdoll_zombie_damping -1; vr_ragdoll_zombie_blast -1; vr_ragdoll_zombie_inherit -1")
            .help("The zombie's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Mummy: the mummy's own physics (vr_ragdoll_mummy_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollMummy()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_mummy_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_mummy_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_mummy_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_mummy_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_mummy_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_mummy_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_mummy_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_mummy_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_mummy_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_mummy_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_mummy_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_mummy_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_mummy_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_mummy_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_mummy_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_mummy_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_mummy_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_mummy_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_mummy_start -1; vr_ragdoll_mummy_mass -1; vr_ragdoll_mummy_friction -1; "
                              "vr_ragdoll_mummy_joint_friction -1; vr_ragdoll_mummy_joint_stiffness -1; vr_ragdoll_mummy_limits -1; "
                              "vr_ragdoll_mummy_damping -1; vr_ragdoll_mummy_blast -1; vr_ragdoll_mummy_inherit -1")
            .help("The mummy's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Gremlin: the gremlin's own physics (vr_ragdoll_gremlin_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollGremlin()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_gremlin_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_gremlin_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_gremlin_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_gremlin_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_gremlin_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_gremlin_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_gremlin_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_gremlin_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_gremlin_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_gremlin_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_gremlin_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_gremlin_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_gremlin_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_gremlin_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_gremlin_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_gremlin_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_gremlin_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_gremlin_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_gremlin_start -1; vr_ragdoll_gremlin_mass -1; vr_ragdoll_gremlin_friction -1; "
                              "vr_ragdoll_gremlin_joint_friction -1; vr_ragdoll_gremlin_joint_stiffness -1; vr_ragdoll_gremlin_limits -1; "
                              "vr_ragdoll_gremlin_damping -1; vr_ragdoll_gremlin_blast -1; vr_ragdoll_gremlin_inherit -1")
            .help("The gremlin's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Vore: the vore's own physics (vr_ragdoll_vore_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollVore()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_vore_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_vore_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_vore_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_vore_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_vore_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_vore_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_vore_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_vore_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_vore_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_vore_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_vore_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_vore_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_vore_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_vore_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_vore_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_vore_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_vore_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_vore_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_vore_start -1; vr_ragdoll_vore_mass -1; vr_ragdoll_vore_friction -1; "
                              "vr_ragdoll_vore_joint_friction -1; vr_ragdoll_vore_joint_stiffness -1; vr_ragdoll_vore_limits -1; "
                              "vr_ragdoll_vore_damping -1; vr_ragdoll_vore_blast -1; vr_ragdoll_vore_inherit -1")
            .help("The vore's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Centroid: the centroid's own physics (vr_ragdoll_centroid_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollCentroid()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_centroid_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_centroid_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_centroid_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_centroid_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_centroid_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_centroid_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_centroid_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_centroid_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_centroid_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_centroid_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_centroid_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_centroid_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_centroid_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_centroid_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_centroid_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_centroid_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_centroid_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_centroid_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_centroid_start -1; vr_ragdoll_centroid_mass -1; vr_ragdoll_centroid_friction -1; "
                              "vr_ragdoll_centroid_joint_friction -1; vr_ragdoll_centroid_joint_stiffness -1; vr_ragdoll_centroid_limits -1; "
                              "vr_ragdoll_centroid_damping -1; vr_ragdoll_centroid_blast -1; vr_ragdoll_centroid_inherit -1")
            .help("The centroid's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Shambler: the shambler's own physics (vr_ragdoll_shambler_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollShambler()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_shambler_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_shambler_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_shambler_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_shambler_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_shambler_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_shambler_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_shambler_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_shambler_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_shambler_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_shambler_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_shambler_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_shambler_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_shambler_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_shambler_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_shambler_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_shambler_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_shambler_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_shambler_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_shambler_start -1; vr_ragdoll_shambler_mass -1; vr_ragdoll_shambler_friction -1; "
                              "vr_ragdoll_shambler_joint_friction -1; vr_ragdoll_shambler_joint_stiffness -1; vr_ragdoll_shambler_limits -1; "
                              "vr_ragdoll_shambler_damping -1; vr_ragdoll_shambler_blast -1; vr_ragdoll_shambler_inherit -1")
            .help("The shambler's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Fiend: the fiend's own physics (vr_ragdoll_demon_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollFiend()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_demon_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_demon_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_demon_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_demon_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_demon_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_demon_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_demon_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_demon_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_demon_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_demon_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_demon_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_demon_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_demon_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_demon_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_demon_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_demon_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_demon_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_demon_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_demon_start -1; vr_ragdoll_demon_mass -1; vr_ragdoll_demon_friction -1; "
                              "vr_ragdoll_demon_joint_friction -1; vr_ragdoll_demon_joint_stiffness -1; vr_ragdoll_demon_limits -1; "
                              "vr_ragdoll_demon_damping -1; vr_ragdoll_demon_blast -1; vr_ragdoll_demon_inherit -1")
            .help("The fiend's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

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
// vr_debug_weight_stamina, vr_window_log...) stay in the console: they mean nothing in the headset.
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
    q_snprintf(text, sizeof(text), "%d open of %d", checklist::openCount(), checklist::itemCount());
    return text;
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

[[nodiscard]] const char* checklistHelp(int arg)
{
    return checklist::ticked(arg >> 8) ? "Ticked. Pick it again to untick it." : "Pick it (the trigger, or A) once done to tick it.";
}

void checklistToggle(int arg)
{
    checklist::toggle(arg >> 8);
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
    text += checklist::undoCount() == 0 ? "" : checklist::undoTicks() ? "Next ticks again: " : "Next unticks: ";
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
        toggle("Hide Ticked", vr_checklist_hide_ticked).help("Leaves the ticked items out of the list. Off: every item, the ticked ones dimmed."),
        action("Reload List", checklistReload)
            .help("Reads quakevr/checklist.txt again (done by itself too when the file changes). Ticks are kept by each "
                  "item's text, in quakevr/checklist_ticks.txt."),
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
        shown++;
        const int lines = q_min(checklist::lineCount(i), 256);
        for(int l = 0; l < lines; l++)
        {
            Item r = row(checklistLine, checklistHelp, checklistToggle, (i << 8) | l, -1);
            r.dimArg = checklistDim;
            r.partOf = l;
            items.pushBack(r);
        }
    }
    if(shown == 0)
    {
        items.pushBack(info(checklistEmpty));
    }
    return items;
}

[[nodiscard]] za::Vector<Item> pageDebug()
{
    return {
        header("Playtesting"),
        open("Checklist", pageIndex(pageChecklist))
            .help("What to test in the headset or give feedback on (quakevr/checklist.txt), ticked as you go. Also the "
                  "menu's corner button."),
        toggle("Voice Notes", vr_notes).help("Raise your off hand to your mouth and hold Y to record a note, with a screenshot and where you are; they go to quakevr/notes."),
        open("Cheats and Recording", pageIndex(pageDebugCheats))
            .help("God, noclip, the weapons and powerups; a scene set up for footage: saved and loaded, cleaned of gibs, "
                  "corpses, props, fires and blood, slowed or frozen, the gadget and hands hidden."),
        open("Slow Motion: Recording", pageIndex(pageRecording)).help("Time Scale (slow motion) is on Graphics > Recording."),
        header("Debug"),
        open("Views", pageIndex(pageDebugViews))
            .help("Drawn in the world: physics shapes, hand bones, ledges, grab tests, the skeleton, collisions, foveation, entity boxes."),
        open("Logging", pageIndex(pageDebugLogging))
            .help("Printed as it happens: shots and damage, throws, climbing, swimming, the hook, wounds, physics; trace files."),
        open("Profiling and Memory", pageIndex(pageDebugProfiling))
            .help("The profiler's panel, capture and hitch log; the memory log."),
        open("Reports", pageIndex(pageDebugReports))
            .help("Print once, to the console: the headset's state, the player, physics, weights, ledges, hands and weapons."),
        open("Tools", pageIndex(pageDebugTools))
            .help("Rebuild the ledge map, reload models, the hand or detail textures; save debug images; test effects."),
        open("Tests", pageIndex(pageDebugTests))
            .help("Put a monster or a box ahead of you, fire a projectile at you, make an ogre throw; god mode, Quad."),
    };
}

// Drawn in the world.
za::Vector<Item> pageDebugViews()
{
    return {
        toggle("Animated Surfaces", vr_anim_surface)
            .help("On: an animated texture's frames (the wall buttons' lit and dim frames) share one surface: the same "
                  "bumps, parallax depth, sheen and detail, only their colours and glow change. Off: each frame its own "
                  "(the relief and sheen pulsed with the frames). At once."),
        toggle("External Maps A/B", vr_extmaps_ab)
            .help("Hides the external pack's normal, specular and glow maps (Graphics: External Maps) at once, to compare "
                  "with the made bumps and Quake's glow; off again shows them. Without a reload."),
        toggle("Retro Textures A/B", vr_retro_ab)
            .help("Hides the retro textures (Graphics > Retro Textures) at once, to compare with the textures as they "
                  "were; off again shows them."),
        command("Retro Textures: List", "vr_retro_list")
            .help("vr_retro_list: prints each model drawn now with its retro textures kind (Graphics > Retro Textures), its "
                  "set and its skin's size, to the console."),
        toggle("Retro Lighting A/B", vr_retrolight_ab)
            .help("Hides retro lighting (Graphics > Retro Lighting) at once, to compare with the smooth light; off again "
                  "shows it."),
        toggle("Ambient Light A/B", vr_ambient_light_ab)
            .help("Takes the room's own fill light (Graphics > Lights: Ambient Light) off at once, to compare the map as "
                  "its own lamps light it; off again shows it. Without a reload."),
        command("Light Probe", "vr_light_probe")
            .help("vr_light_probe: the baked light at six points round you, as the map has it and as it is drawn with the "
                  "current Ambient Light and Light Contrast (128 is Quake's full light). The same numbers light a model "
                  "standing there: two settings, compared without looking."),
        toggle("Show Damage Numbers", vr_debug_damage_numbers)
            .help("Every hit on anything that takes damage (monsters, corpses, gibs, crates, props, shootable walls): its "
                  "damage floating where it struck, and printed as the training dummy's (what struck, where, what's left)."),
        toggle("Show Grapple Rope", vr_debug_rope)
            .help("Each grappling hook rope: its drawn chain (points white, pieces green), the corners it wraps round "
                  "(red) and its taut path (yellow), the ends (orange)."),
        toggle("Show Physics Shapes", vr_debug_physics_shapes)
            .help("Draws the physics bodies (Box3D) as wireframes: props awake green, asleep blue, held yellow; doors purple, "
                  "monsters orange, you cyan, hanging pickups grey; red dots where they touch. And each hand's grab reach."),
        cycle("Show Hit Zones", vr_debug_hitzones, {{0.f, "Off"}, {1.f, "Positional Damage"}, {2.f, "Decapitation"}, {3.f, "Both"}})
            .help("Positional Damage colors the animated model surface: head red, body green, extremities yellow, legs blue. "
                  "Uses the same standing-pose mapping and Head Priority as precise shots and positional melee. "
                  "Precise hits off: box/ray reference zones. Decapitation colors, on the animated model too, where a "
                  "slash beheads (magenta: the melee's zone, Head Size and Neck) and the head that shots, lasers and "
                  "thrown axes pop (red); Both outlines the magenta over Positional Damage's colors. Every head and limb "
                  "test maps the point struck to the standing pose, so these zones move with the model."),
        toggle("Hit Zones Through Walls", vr_debug_hitzones_xray)
            .help("Draws the animated positional regions through walls and the back of the model. Off: only visible surfaces."),
        cycle("Show Gadget Button", vr_debug_gadget_button,
            {{0.f, "Off"}, {1.f, "Its Hit Volume"}, {2.f, "And the Screen Tap"}, {3.f, "And Print Sync"}})
            .help("vr_debug_gadget_button: the wrist gadget's side button's hit volume (green ready, yellow pressed, red "
                  "cooling down) and your fingertip (the drawn index fingertip white, joined to the tuned one that "
                  "presses), its presses printed; And the Screen Tap: also bullet time's tap zone over the screen and the "
                  "striking volume (your hand's surface blue, the gun's butt orange, white on the zone); And Print "
                  "Sync: also prints each frame how far the zone is from the drawn gadget. The settings: HUD and Menus "
                  "> Wrist Gadget."),
        command("Gear Lights Info", "vr_gear_lights_info")
            .help("vr_gear_lights_info: the gear lights' state, the side button's place and your fingertip's distance to "
                  "it, and the stealth AI's light on you now."),
        cycle("Show Hits", vr_debug_hits, {{0.f, "Off"}, {1.f, "Hits"}, {2.f, "Hits and Misses"}})
            .help("Precise hit detection: each hit on a monster's model drawn for a few seconds (the model as it was then, "
                  "the triangle hit in green, the point on the model in red, where the grown model was met in yellow) and "
                  "printed. Hits and Misses: also the shots that went through a monster's box beside its model (orange)."),
        toggle("Show Hand Bones", vr_debug_hand_bones)
            .help("Draws both hands' joints and bones, and what they grip: the finger spheres (green touching, yellow near, red "
                  "sunk in) and the palm's fit (white: where the hand is, cyan: where the grip moved the palm)."),
        cycle("Show Ledges", vr_debug_ledges, {{0.f, "Off"}, {256.f, "Within 256 units"}, {1.f, "Within 512 units"}, {1024.f, "Within 1024 units"}})
            .help("The ledge map round you, what climbing holds: lips green (on moving brushes pink), a tick out every 8 units "
                  "(yellow where the drop starts further out). Built even with climbing off."),
        toggle("Log Empty Hand Against Held", vr_debug_hand_collide)
            .help("Prints, each frame an empty hand meets the weapon or the thing the other hand holds, how near the real "
                  "and the drawn hand's palm and knuckles are to its surface, how far the drawn hand and the thing are "
                  "held back (Carrying: Empty Hand Against Held Things)."),
        cycle("Show Grab Test", vr_debug_carry, {{0.f, "Off"}, {1.f, "Drawn"}, {2.f, "Drawn and Logged"}, {3.f, "Also Far Fists"}})
            .help("For each hand near something to carry: the box it is drawn in and the fist tested (its spheres; the nearest "
                  "bright). Logged: each grab and held prop's placing printed, carry_trace.txt written, and held props meeting each "
                  "other, a weapon or a wall; also far fists: hands not near anything too."),
        cycle("Show Body Skeleton", vr_body_debug, {{0.f, "Off"}, {1.f, "Skeleton"}, {2.f, "Body Facing You"}, {3.f, "Body From Its Left"}})
            .help("Draws the body's skeleton; or shows the body in front of you, facing you or seen from its left (to check "
                  "its pose and calibration without a mirror)."),
        command("Print Torso Direction", "vr_torso_report")
            .help("vr_torso_report: the head's yaw, the old and the new torso guesses, the hands' pull and weights."),
        toggle("Log Body Drift", vr_debug_body_error)
            .help("Twice a second in the console: how far the game's body (its feet) has drifted in your room since it was turned "
                  "on (or marked), and how far its facing has turned, with your lean. Stand still in the room: leaning and "
                  "turning or moving with the stick, both should stay near 0 (vr_body_error)."),
        command("Mark Body Drift Here", "vr_body_error mark")
            .help("vr_body_error mark: measure the body's drift from where it stands now (stand straight, your feet where they "
                  "are to stay)."),
        cycle("Show Body Collisions", vr_debug_body_collide, {{0.f, "Off"}, {1.f, "Logged"}, {2.f, "Logged and Drawn"}})
            .help("The drawn hands and weapons stopping at each other and the body: each contact printed (and "
                  "body_collide_trace.txt); drawn: the capsules and the pushes."),
        cycle("Log Hand Offsets", vr_debug_hand_offset, {{0.f, "Off"}, {1.f, "When Moved"}, {2.f, "Every Frame"}})
            .help("Each hand drawn away from where it is tracked, and by what (walls, the weight's spring, models, the body, "
                  "a held thing against a wall, the other hand's weapon or held thing), in the console."),
        toggle("Log Weapon Wall Collisions", vr_debug_gun_wall)
            .help("Each frame a held weapon is held out of the level: how far the hand is moved (up, across), the depth left "
                  "and the muzzle's height over the surface below it, in the console."),
        open("Flashlight Zones: Flashlight - On a Gun or Head", pageIndex(pageFlashlightMounts)).help("Showing the flashlight zones is on Flashlight > On a Gun or Head."),
        command("Flashlight to Left Hand", "vr_flashlight_give left")
            .help("vr_flashlight_give left: the chest flashlight into the left controller's hand, as if gripped there (that "
                  "hand's grip lets go of it). Its buttons are then that controller's, with either Main Hand."),
        command("Flashlight to Right Hand", "vr_flashlight_give right").help("vr_flashlight_give right: the same, the right hand."),
        command("Clip Flashlight on Right Gun", "vr_flashlight_clip_gun right")
            .help("vr_flashlight_clip_gun right: the flashlight clipped on the gun in the right hand, from wherever it is "
                  "(left: the left hand's). Its place there: Weapon Offsets > Flashlight."),
        command("Clip Flashlight on Head", "vr_flashlight_clip_head right")
            .help("vr_flashlight_clip_head right: the flashlight clipped at your right temple, from wherever it is "
                  "(left: the left one)."),
        command("Flashlight Home", "vr_flashlight_home")
            .help("vr_flashlight_home: the flashlight back on the belt at once, from a hand, a gun or the head (the light "
                  "stays as it was)."),
        command("Flashlight Cord Info", "vr_flashlight_cord_info")
            .help("vr_flashlight_cord_info: in the console, the cord's last frame (Body > Flashlight > Cord): its "
                  "length, its rings and triangles, a chain's links."),
        command("Cell Cord Info", "vr_cellcord_info")
            .help("vr_cellcord_info: in the console, the cell cords (Weapons > Reloading > Cell Cords): each one's weapon, "
                  "its ends (the weapon's bottom, the pouch's cell), loose or plugged in, its length and rings."),
        command("Probe Flashlight", "vr_flashlight_probe menu")
            .help("vr_flashlight_probe: in the console, where the torch is, whether each hand is at it (lit, as the game "
                  "reads the hand, still), whether B/Y would clip it on the head or a gun, and the player's speed and turn."),
        cycle("Show Model Collisions", vr_debug_model_collide, {{0.f, "Off"}, {1.f, "Logged"}, {2.f, "Logged and Drawn"}})
            .help("Held weapons and props stopping at the models' triangles: each hand's push printed; drawn: the rays (grey as tracked, "
                  "green or red as drawn) and the push (yellow)."),
        toggle("Show Foveation", vr_foveated_debug)
            .help("The shading rates of Foveated Rendering in the eyes and the mirror (yellow 2x2, red 4x4) and the upscaler's "
                  "sharp circle (cyan)."),
        toggle("Show Parallax Depth", "vr_parallax_debug")
            .help("vr_parallax_debug: where the walls' parallax writes the depth of the carving it shows (Graphics > Surfaces > "
                  "Parallax Depth Write): red, brighter the deeper below the surface; dark blue: the surface's own depth."),
        toggle("Show Entity Boxes", "r_showbboxes")
            .help("Every entity's bounding box, as the game collides with it (monsters, items, missiles, triggers), through "
                  "walls. Single player only."),
    };
}

// Printed as it happens (the console, and the wrist log), or a trace file written every frame.
za::Vector<Item> pageDebugLogging()
{
    return {
        cycle("Developer Messages", "developer", {{0.f, "Off"}, {1.f, "On"}, {2.f, "Verbose"}})
            .help("The game's developer messages: needed by Shots and Damage and the Grappling Hook's log below, and many "
                  "others (melee events, grenades, deflections). Verbose: every frame's melee detail too."),
        header("Logs"),
        cycle("Bullet Time", vr_debug_bullettime, {{0.f, "Off"}, {1.f, "On, Off, Refused"}, {2.f, "And the Screen Tap"}})
            .help("Bullet time starting, stopping and refused (the meter, the cooldown), each tap and taps that never "
                  "stopped on the screen; And the Screen Tap: every frame near the gadget's screen, how far over it your "
                  "hand (and its gun's butt) is and how fast it comes into it."),
        cycle("Flashlight Flicks", vr_flashlight_flick_debug, {{0.f, "Off"}, {1.f, "Each Flick and Why"}, {2.f, "And the Wrist Every Frame"}})
            .help("vr_flashlight_flick_debug: the wrist flick that turns the held torch over, taken or refused (and why); 2: the held hand's wrist speed every frame."),
        cycle("Chainsaw", vr_debug_chainsaw, {{0.f, "Off"}, {1.f, "Pulls and Cuts"}, {2.f, "And the Bar in Monsters"}})
            .help("The chainsaws' cords (taken, pulled, too slow, let go), their engines (started, stalled) and cuts; "
                  "And the Bar: also each cut's test against what is near, how deep the drawn bar sinks into a monster, "
                  "and the cord's hole drawn. The engine and cut lines need Developer Messages."),
        cycle("Wall Buttons", vr_debug_wallbuttons, {{0.f, "Off"}, {1.f, "Each Press"}, {2.f, "And Weapon Lines"}})
            .help("Each button pressed: what pressed it and how (a hand, a held weapon and its line, a thrown thing and "
                  "its speed, stepped on), and where the hand was: how far from the button and from you (a far one is a "
                  "stray press). And Weapon Lines: every frame, each held weapon's line that presses buttons "
                  "(from its pommel or butt to its tip or muzzle). Needs Developer Messages."),
        cycle("Monster Poses", vr_debug_pose_check, {{0.f, "Off"}, {1.f, "Squashed Bodies"}, {3.f, "And a Screenshot"}, {2.f, "Every Frame"}})
            .help("vr_debug_pose_check: a monster drawn squashed between two poses for over 0.2 s (\"posecheck:\"; a "
                  "parried ogre was, for half a second), drawn with a pose out of its model's range or a broken blend; "
                  "And a Screenshot: a picture at each; Every Frame: each monster's drawn poses and how flat (a lot)."),
        toggle("Shots and Damage", vr_debug_shots)
            .help("Each hitscan shot (where it starts, its direction, what its pellets hit, headshots), each damage you deal "
                  "(and when Quad's sound plays) and each prop a shot pushes. Needs Developer Messages."),
        toggle("Missile Hits", vr_debug_missiles)
            .help("Each missile's hit (rockets, nails, grenades, lasers...): what it met, where and the surface's normal; "
                  "a solid prop: its drawn box as it stands (vr_debug_missiles)."),
        cycle("Throws", vr_debug_throw, {{0.f, "Off"}, {1.f, "Each Throw"}, {2.f, "And Its Timing"}})
            .help("Each throw's speed estimate from the hand's motion (and the release's timing); a thrown weapon's spin "
                  "and how far its axis is off end over end (0: a wrist flick's, tip going down; needs Developer Messages)."),
        toggle("Controller Spin", vr_debug_angvel)
            .help("While a controller turns fast: which frame its angular velocity is read in (Controller Spin Frame), "
                  "how well the runtime's and the fixed one follow the turn, the hand's speed before and after."),
        toggle("Highlights", vr_debug_highlights)
            .help("Each moment the highlight log takes (Graphics > Recording > Log Highlights): its time from the sync "
                  "mark, kind, score and what it was done to and with."),
        toggle("Axe Sticks", vr_debug_axestick)
            .help("Each thrown axe's blade striking something: stuck (how fast, how deep, at what angles) or why it bounced; "
                  "its bleeding, its fall, its pull. Needs Developer Messages for the last."),
        cycle("Spin in the Air", vr_debug_spin_align, {{0.f, "Off"}, {1.f, "Each Throw"}, {2.f, "Every Step"}})
            .help("Each throw's spin as it leaves the hand (its inertia, how long and flat it is, how fast its spin "
                  "settles: Spin Alignment) and how far off end over end it is when its flight ends; every step: each "
                  "step of its flight."),
        cycle("Climbing", vr_climb_debug, {{0.f, "Off"}, {1.f, "Holds"}, {2.f, "Every Frame"}, {3.f, "And Shoulders"}})
            .help("Holds taken, released, mantles; every frame: the body, the hands, the pull, the holds' reach (a lot)."),
        cycle("Hands", vr_debug_hands, {{0.f, "Off"}, {1.f, "When They Change"}, {2.f, "Every Frame"}})
            .help("Each hand's state: the grip, its weapon, what it carries, the force grab, the flashlight, the hotspot, and "
                  "what climbing makes of a grip (holding, free, or why it takes nothing)."),
        toggle("Swim Strokes", vr_swim_debug).help("Each stroke: its peak speed, the power gate, the reverse damping and the push it gave."),
        cycle("Grappling Hook", vr_grapple_debug,
            {{0.f, "Off"}, {1.f, "Bites and Reels"}, {2.f, "And the Rope"}, {3.f, "Every Frame"}, {4.f, "And Every Rope Drawn"}})
            .help("What the hook bites and each reel (mass, class, speeds); the rope 4 times a second, or every frame; and "
                  "each rope drawn every frame (which beam, from where to where: a rope drawn where there is none). Needs "
                  "Developer Messages."),
        toggle("Controller Buttons", vr_debug_buttons)
            .help("Each controller button pressed and let go: the hand, the button, its key and binding, and what took it "
                  "(the posing mode, a voice note, the flashlight); and the controller profile the headset's runtime "
                  "picked for each hand (Virtual Desktop can report Index controllers). For a button that does nothing."),
        toggle("Wounds", vr_wounds_debug).help("Each wound painted on a model."),
        cycle("Grasp", vr_debug_grasp, {{0.f, "Off"}, {1.f, "Each Solve"}, {2.f, "Each Finger"}})
            .help("Each grasp solve of the jointed hands (and each finger's stops)."),
        toggle("Holster Draw Blend", vr_debug_draw_blend)
            .help("Each frame of a gun easing between a holster and a hand: the turn and the distance left."),
        cycle("Put-Away Transition", vr_debug_collect_fx, {{0.f, "Off"}, {1.f, "Each Thing"}, {2.f, "Each Frame"}})
            .help("Each thing put away at a holster or pouch (its model, from where, the holster) and when it has gone "
                  "in; or also each frame's size and distance left."),
        command("Check Last Pose", "vr_pose_check")
            .help("vr_pose_check: after the posing mode, how far what you set is from what you get. A weapon or hotspot: "
                  "hold it. In a holster: holster it there; each holster of that kind holding it is measured."),
        cycle("Physics Sounds", vr_debug_physsound, {{0.f, "Off"}, {1.f, "Each Sound"}, {2.f, "And Each Hit Skipped"}})
            .help("Each knock (the prop, its material and weight, the hit's speed, the volume), each scrape starting and "
                  "stopping and each climbing grab (the hold's texture); or also the hits too soon after the last and each "
                  "scrape grain."),
        cycle("Spatial Audio", vr_debug_snd, {{0.f, "Off"}, {1.f, "Each Second"}, {2.f, "And Each Voice"}})
            .help("Each second: the voices in use, their mix's time and the simulations' (the walls, the room's reverb); or "
                  "also each voice: its sound, distance, occlusion (low, middle, high) and Doppler."),
        cycle("Network Messages", vr_debug_net, {{0.f, "Off"}, {1.f, "When the Broadcast Is Full"}, {2.f, "And Each Second"}})
            .help("vr_debug_net: each frame the unreliable broadcast (particles, sounds, temp entities, wounds, tracers) "
                  "was full: how much QuakeC's writes lost and how much a client was not sent (whole messages only); or "
                  "also each second the most sent: the broadcast, the reliable messages, a client's own data and entities."),
        toggle("Ragdolls", vr_debug_ragdoll)
            .help("vr_debug_ragdoll: each ragdoll made (when it went limp, its parts), each push on it and each blast's throw."),
        cycle("Physics Bodies", vr_debug_box3d, {{0.f, "Off"}, {1.f, "Made and Slept"}, {2.f, "Every Awake Body"}})
            .help("Box3D bodies made, woken and put to sleep; or every awake body every frame (a lot). Also each throw: how "
                  "fast, whether it passes through your hands, and how much its velocity and spin changed by the grace's end "
                  "(with Throw Grace 0: watched 0.2 s, to compare)."),
        cycle("Rocks and Bricks Placement", vr_debug_debris, {{0.f, "Off"}, {1.f, "A Line a Map"}, {2.f, "Each Piece"}, {3.f, "Each Spot Rejected"}})
            .help("At the next map load: the pieces placed, the spots, the time; each piece; each spot rejected and why."),
        cycle("Crates Placement", vr_debug_crates, {{0.f, "Off"}, {1.f, "A Line a Map"}, {2.f, "Each Crate"}, {3.f, "Each Spot Rejected"}})
            .help("vr_debug_crates: where the crates went (and why not), each crate's clearance in front of it. Also Developer "
                  "Messages print each crate's damage and breaking, the pieces and what it held."),
        toggle("Box Sizes", vr_debug_item_sizes)
            .help("vr_debug_item_sizes: each ammo or health box's scale as it is first drawn (after a map loads, out of a "
                  "crate), and each change of it after: they should be drawn at their final size from the first frame."),
        toggle("Torch Lights", vr_debug_torch_lights)
            .help("Every torch light lit, every frame: which (a wall torch, a taken one), where, its radius and colour, shadowed."),
        cycle("Arm IK", vr_debug_arm, {{0.f, "Off"}, {1.f, "Print Once"}, {2.f, "Trace File"}})
            .help("Each drawn arm's joints once (shoulder, elbow, wrist in the body's axes, the elbow's swing, the wrist's bend "
                  "and twist, and the swing's cost every 15 degrees: armcost); or arm_trace.txt every frame."),
        toggle("Heavy Weapon Wrenched Out", vr_debug_weight_drop)
            .help("Twice a second, each hand's fastest turn against its heavy weapon's limit (above half of it), and each "
                  "weapon wrenched out (Weapon Weights: Wrenched Out)."),
        cycle("Two-Handed Grip", vr_debug_2h_grip, {{0.f, "Off"}, {1.f, "Taken and Let Go"}, {2.f, "And Where"}})
            .help("Each grip the helping hand takes (a sword's or the crowbar's: below the hand or along the blade) and why "
                  "it let go of a weapon (the check, its number and limit), the stickiness then (Aiming: 2H Grip "
                  "Stickiness), and weapons held anywhere (taken, let go, carried); And Where: also where the grip is "
                  "from it, how far along a blade it holds it, and where an empty hand is on a weapon held, carried or "
                  "lying about (its surface, handle and nearest hotspot: Weapons Anywhere)."),
        toggle("Bot Chatter", vr_verbosebots).help("The bots' thoughts, with bots in the game."),
        toggle("Graphics State", vr_debug_glstate)
            .help("Each texture bind the engine skipped, believing the texture already bound, while the graphics card had "
                  "another there (\"texcache: stale bind\"): a texture deleted without telling the engine's cache, its "
                  "name given to a new one; shows as a blank or black image after a size change. And a clip rectangle "
                  "left on at a frame's end (\"glstate:\"): the menus and console cut off in the window (vr_debug_glstate)."),
        header("Trace Files (game folder)"),
        cycle("Grasp Trace", vr_debug_grasp_trace, {{0.f, "Off"}, {1.f, "Main Hand"}, {2.f, "Off Hand"}, {3.f, "Both Hands"}})
            .help("grasp_trace.txt: the drawn hand's joints every frame."),
        cycle("Weight Trace", vr_debug_weight, {{0.f, "Off"}, {1.f, "File"}, {2.f, "File and Console"}})
            .help("weight_trace.txt: each hand's target and drawn pose under the weight, every frame."),
        toggle("Lean Trace", vr_debug_lean)
            .help("lean_trace.txt: the head, the body's box, the lean, the pelvis and the feet every frame, and the lean's cues."),
    };
}

// The profiler and the memory log (moved from the Debug page).
za::Vector<Item> pageDebugProfiling()
{
    return {
        header("Profiling"),
        cycle("Profiler Panel", vr_profile_overlay, {{0.f, "Off"}, {1.f, "Over the Wrist"}, {2.f, "In Front"}})
            .help("Where each frame's time goes: the game's systems (Box3D, QuakeC, the world's drawing, waiting for the "
                  "headset...), the last second's average and worst in ms, with a bar against the frame's budget; the CPU's, "
                  "then the GPU's. Showing it runs the profiler."),
        cycle("CSV Capture", vr_profile_csv, {{0.f, "Off"}, {1.f, "Recording"}})
            .help("While on, a row a second (each system's time, a column each) into quakevr/profile/systems_<date>_<time>.csv, "
                  "for a spreadsheet: turn it on, play what feels slow, turn it off."),
        cycle("Hitch Log", vr_profile_hitch, {{0.f, "Off"}, {1.5f, "Over 1.5 Frames"}, {2.f, "Over 2 Frames"}, {3.f, "Over 3 Frames"}})
            .help("While the profiler runs (the panel, a capture), frames that take this long go to the console and "
                  "quakevr/profile/hitches_<date>_<time>.csv, with what took their time."),
        cycle("Detail", vr_profile_detail, {{1.f, "Systems"}, {2.f, "Every Trace and Builtin"}})
            .help("Every Trace and Builtin also times each collision trace and each QuakeC builtin call apart: dearer, to "
                  "split QuakeC's time."),
        cycle("GPU Timing", vr_profile_gpu, {{0.f, "Off"}, {1.f, "Every Frame"}, {2.f, "One Frame in 2"}, {4.f, "One Frame in 4"}, {8.f, "One Frame in 8"}})
            .help("How often the profiler times the GPU: each timed frame stalls it a little. Off: CPU times only."),
        slider("Report Interval", vr_profile_interval, 0.f, 30.f, 1.f, "%.0f s")
            .help("Performance Profile (Graphics): seconds between its reports in the console and its CSV. 0: only on Dump Profile."),
        command("Print Report", "vr_profile_report")
            .help("vr_profile_report: the last 5 seconds' table in the console (in qconsole.log with -condebug)."),
        command("Dump Profile", "vr_profile_dump")
            .help("vr_profile_dump: Performance Profile's report now, in the console and its CSV (quakevr/profile/profile_<map>_...)."),
        command("QuakeC Instructions", "profile 30")
            .help("profile 30: the 30 QuakeC functions that ran the most instructions (their own) since the last time, and "
                  "the total; then all are zeroed. Press it, do the thing, press it again. A call over 16 million is a "
                  "runaway loop error."),
        toggle("Time QuakeC Functions", vr_qcprofile)
            .help("vr_qcprofile: each server QuakeC function's time (its own, with the builtins it calls, and with its "
                  "callees), its calls, and each builtin's, for QuakeC Time Report. Costs a little while on."),
        command("QuakeC Time Report", "profile_qc 30")
            .help("profile_qc 30: the 30 QuakeC functions and builtins that took the most time a frame since the last "
                  "report (Time QuakeC Functions on), in the console; then all are zeroed. Press it, do the thing, press "
                  "it again."),
        toggle("Edict Index", vr_edictindex)
            .help("vr_edictindex: find() by classname and findflags() on monsters, clients, items, lit torches and "
                  "bodies step straight to the next match instead of walking every entity (the same results). Off: "
                  "they walk, as Quake did."),
        toggle("Verify Edict Index", vr_edictindex_verify)
            .help("vr_edictindex_verify: every indexed search walks as well and any difference is counted and printed "
                  "(the walk's answer is used). Edict Index Stats prints the counts."),
        command("Edict Index Stats", "vr_edictindex_stats")
            .help("vr_edictindex_stats: the index's searches, rebuilds, edicts read again, searches verified and "
                  "differences since the last time."),
        toggle("Force Grab Search by Grid", vr_forcegrab_grid)
            .help("vr_forcegrab_grid: the force grab's target search (each hand, every frame) tests only the entities "
                  "linked near the hand and its images through teleporters (the same results). Off: it walks every entity."),
        toggle("Verify Force Grab Search", vr_forcegrab_grid_verify)
            .help("vr_forcegrab_grid_verify: every search walks as well; a difference is counted and printed, and the "
                  "walk's answer used. Force Grab Search Stats prints the counts."),
        command("Force Grab Search Stats", "vr_forcegrab_grid_stats")
            .help("vr_forcegrab_grid_stats: the searches, the entities each tested, searches verified and differences "
                  "since the last time."),
        command("Game State Hash", "vr_bench_statehash")
            .help("vr_bench_statehash: one hash of every entity's QuakeC fields (and which are in use), printed with the "
                  "server's time: the same script on two builds gives the same hash when a change left the game the same."),
        command("Benchmark Capture (10 s)", "vr_bench_begin manual 10s")
            .help("vr_bench_begin manual 10s: the next 10 seconds' frame times (median, 95th and 99th percentiles, worst), "
                  "each GPU pass, the heap events and what there is, into quakevr/profile/bench/manual.json and a line in "
                  "the console (docs/vr-port/BENCHMARKS.md). Stand still and press it."),
        cycle("External Profiler Collects", vr_bench_profiler, {{0.f, "As Started"}, {1.f, "Benchmark Windows"}, {2.f, "Map Loads"}})
            .help("For a CPU profiler run on the game (VTune started paused: -start-paused): Benchmark Windows resumes its "
                  "collection for each Benchmark Capture (vr_bench_begin) alone, Map Loads for each load alone (the map "
                  "command to its first frame drawn), so gameplay and loading are profiled apart (vr_bench_profiler; "
                  "vr_profiler_collect 0|1 by hand)."),
        toggle("Layered Shadow Casters", vr_shadow_layered)
            .help("vr_shadow_layered: each shadow caster drawn once per light into all the faces it reaches (the GPU picks "
                  "the face), instead of once per face: fewer draw calls, the same shadows. Off: a face at a time (also when "
                  "the GPU lacks GL_ARB_shader_viewport_layer_array)."),
        command("Check Layered Shadows", "vr_shadow_layered_check 20")
            .help("vr_shadow_layered_check 20: this frame's shadow maps drawn both ways 20 times, timed (draw calls, CPU and "
                  "GPU ms), then read back and compared texel by texel, in the console. Stand where shadows are."),
        command("Check Shadow Caster Set-up", "vr_shadow_layered_check 20 cache")
            .help("vr_shadow_layered_check 20 cache: this frame's shadow maps drawn with each caster set up again for every "
                  "light, then once a pass over the lights (as the game draws them), 20 times each, timed, then read back "
                  "and compared texel by texel, in the console. Stand where shadows are."),
        command("Load Times", "vr_startup_times")
            .help("vr_startup_times: where the start-up and the last map load spent their time (from the map command to its "
                  "first frame drawn: the stages, then the kinds of work across them), and every load's total."),
        toggle("Ready What Can Appear", vr_probe_kinds)
            .help("On: the kinds of monster that can appear later on a map (the firing range's dispensers and dummies, monsters "
                  "waiting for a trigger) are made ready as it loads: their models, sounds and compiled hull, so their first "
                  "appearance drops no frame (15-25 ms otherwise). The next map load (vr_probe_kinds)."),
        cycle("Ready the Debug Spawner's", "vr_probe_test_spawn", {{0.f, "Off"}, {1.f, "Its Kind"}, {2.f, "Every Kind"}})
            .help("The debug spawner's monsters (vr_test_spawn, impulse 241) made ready as each map loads too: Its Kind the one "
                  "vr_test_spawn names, Every Kind all 23 (a longer load). The next map load (vr_probe_test_spawn)."),
        slider("Decoded Image Cache", vr_image_cache_mb, 0.f, 2048.f, 64.f, "%.0f MB")
            .help("Image files decoded once are kept for later loads up to this size (a texture pack's world textures and "
                  "material maps: QRP's E1M1 loads 0.5 s faster the second time); the least recently used go first. 0: none "
                  "kept (vr_image_cache_mb)."),
        toggle("Campaign Switch Keeps Models", vr_campaign_keep_models)
            .help("On: a switch of campaign (id1 to a mission pack and back) keeps the models whose files are the same in "
                  "its folders (each file their load looked for is found the same again), instead of loading every one "
                  "again (vr_campaign_keep_models)."),
        command("Kept Models Info", "vr_model_keep_info")
            .help("vr_model_keep_info: how many models the last campaign switch kept, of how many, and the time its check "
                  "took (console)."),
        command("Decoded Image Cache Info", "vr_image_cache_info")
            .help("vr_image_cache_info: the images kept, their size, and since the start how many were found there and how many "
                  "decoded (console). vr_image_cache_clear empties it."),
        header("Particles' Fill"),
        toggle("Skip Hidden Particles", vr_particle_saturate)
            .help("vr_particle_saturate: in dense effects the particles are composited in reverse order into a layer of "
                  "their own, and where it is already opaque the ones under it are skipped (the same image within a "
                  "level of rounding). Off: each blended into the scene in turn, to compare with GPU Timing on."),
        slider("Skip From Coverage", vr_particle_saturate_cover, 0.f, 50.f, 1.f, "%.0f views")
            .help("vr_particle_saturate_cover: from how many screens' worth of particles the frame uses it (it costs a "
                  "clear, a few full-screen marks and a blend an eye; it pays from about 7). 0: always."),
        slider("Skip Batches", vr_particle_saturate_batches, 1.f, 32.f, 1.f, "%.0f")
            .help("vr_particle_saturate_batches: the particles drawn in this many batches, the opaque pixels marked "
                  "after each (more: earlier skipping, more marks). 1: nothing skipped."),
        cycle("Opaque At", vr_particle_saturate_opacity, {{0.99f, "99%"}, {0.999f, "99.9%"}, {1.f, "Never (no skipping)"}})
            .help("vr_particle_saturate_opacity: how opaque a pixel must be for the particles under it to be skipped."),
        toggle("Freeze Particles", vr_particle_freeze)
            .help("vr_particle_freeze: the particles stop where they are (still drawn), to compare settings on the same "
                  "frame."),
        toggle("Half-Res Particles Always", vr_particle_halfres_force)
            .help("vr_particle_halfres_force: the particles drawn at half resolution in every frame with some in view "
                  "(when Half-Res Heavy Particles or Half-Res Retro Particles allow it), not only in dense effects: to "
                  "compare with Freeze Particles."),
        cycle("Half-Res Blend", vr_particle_halfres_upsample,
            {{0.f, "Filtered (before)"}, {1.f, "By Depth"}, {2.f, "By Depth, Edges Shown"}})
            .help("vr_particle_halfres_upsample: how half-resolution particles are blended into the view. By Depth: each "
                  "pixel takes the half-size texels at its own distance (no smoke bled onto a crate's edge in front of "
                  "it). Edges Shown: the pixels that took texels by distance in green. Filtered: the four blended, as "
                  "before."),
        header("Server Tick"),
        toggle("Fixed 72 Hz Server Tick", host_fixedtick)
            .help("host_fixedtick: the server (monsters, physics, your hands' blows) runs in steady 1/72 s ticks at any "
                  "headset rate, the leftover time carried to the next frame. Off: the old way, a server frame once 1/72 s "
                  "had built up: 36 Hz on a 72 Hz headset, 45 at 90, 60 at 120, 48 at 144 (to compare)."),
        command("Server Tick Stats", "host_tickstats")
            .help("host_tickstats: the server's ticks since the last press (console): ticks a second, their lengths, how "
                  "many each frame ran (the last 48 frames as digits), Box3D's steps. Press it, play a while, press it again."),
        header("Threads"),
        toggle("Split Work Between Threads", vr_jobs_parallel)
            .help("The game's thread pool shares out the grasp solve, the liquids' volume, the decal atlas and the models' "
                  "occlusion bakes. Off: the calling thread does all of it (the same results, slower: to compare)."),
        toggle("Catch Memory Use off the Main Thread", vr_zone_threadcheck)
            .help("The game crashes at once, with the culprit's stack (qvr_crash.txt), when a thread other than the main one "
                  "uses the hunk, the model cache or the zone (none of them is thread-safe; vr_zone_threadcheck). For "
                  "testing: a map load's crash in its data is often one of these."),
        toggle("Evict Models at Map Start", vr_hitmodel_cachestress)
            .help("vr_hitmodel_cachestress: at the next map's start, each model's data is dropped from the model cache once "
                  "the next is loaded (what a full cache does), before precise hit detection's meshes are made on the "
                  "thread pool. They must come out the same (developer 1: the hash in the console). For testing."),
        cycle("Worker Threads", vr_jobs_threads, {{0.f, "Auto"}, {1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}, {8.f, "8"}, {16.f, "16"}})
            .help("The thread pool's workers besides the main thread (Auto: the CPU's threads less one). Changed, the pool is "
                  "made again. -jobs <n> on the command line sets it from the start."),
        toggle("Physics on Threads", vr_box3d_threads)
            .help("Box3D's step (the props' physics) shared between the thread pool and the main thread. Off: the main "
                  "thread steps it alone (the same results: Box3D is deterministic whatever its threads)."),
        cycle("Physics Threads", vr_box3d_workers, {{0.f, "All"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}, {6.f, "6"}, {8.f, "8"}})
            .help("With Physics on Threads: at most this many threads step Box3D, the main thread one of them (All: every "
                  "worker of the pool). Measured on a 24-core CPU: 4 is as fast as 6 or 8 up to ~500 awake bodies (and "
                  "leaves cores to the render thread and the headset's runtime); 8 gains another 20% only from ~1000. "
                  "All is slower than 8."),
        cycle("Physics Threads From", vr_box3d_threads_bodies, {{0.f, "Always"}, {100.f, "100 Bodies"}, {150.f, "150 Bodies"}, {200.f, "200 Bodies"}, {300.f, "300 Bodies"}, {500.f, "500 Bodies"}, {1000.f, "1000 Bodies"}})
            .help("With Physics on Threads: shared out only while at least this many bodies are awake (a smaller step is "
                  "faster on the main thread alone: handing it out costs more than it saves). Measured: even at ~100 awake, "
                  "4 threads 15% faster at 125, 25% at 150, 40-50% at 300-500, 2-3x at 1000."),
        command("Physics Step Time", "vr_physics_steptime")
            .help("vr_physics_steptime: Box3D's step time a frame since the last time (average, median, 95th and 99th "
                  "percentiles, worst, ms), the awake bodies and its threads. Run it, play, run it again."),
        command("Physics Step Time by Awake Bodies", "vr_physics_steptime bins")
            .help("vr_physics_steptime bins: the same, then the frames by how many bodies were awake as they began (the "
                  "median and 95th percentile of each range: where threads pay, against Physics Threads From) and Box3D's "
                  "own profile (collide, solve and its stages). Misc/quakevr/box3dmt/pilebench.py runs it over the piles."),
        command("Spawn a Big Prop Pile", "vr_physics_bigpile")
            .help("vr_physics_bigpile [count] [distance] [rocks | bricks | crates | mixed]: Pile Size rocks and bricks "
                  "ahead of you in leaning columns that topple into a pile: more awake bodies than Physics Threads From "
                  "while they fall (Physics on Threads at work). Physics Step Time before and after gives the step's "
                  "time. More piles, a wall of crates and Clear the Piles: Debug > Tests > Physics Stress."),
        command("Thread Pool Info", "vr_jobs_info")
            .help("vr_jobs_info: the pool's workers and what it has run (tasks, loops, the chunks each side took)."),
        command("Thread Pool Sites", "vr_jobs_sites")
            .help("vr_jobs_sites: each shared-out loop's calls, how many split between threads, its items, the chunks "
                  "the caller and the helpers took, and the caller's time a call (average, worst). Then Reset Sites "
                  "and play to see one scene's."),
        command("Thread Pool Sites Reset", "vr_jobs_sites reset")
            .help("vr_jobs_sites reset: every loop's counters back to zero (Thread Pool Sites counts from here)."),
        command("Thread Pool Split Bench", "vr_jobs_bench")
            .help("vr_jobs_bench [reps]: a small shared-out loop (2 to 21 items of 0 to 50 microseconds) timed on the "
                  "caller alone and split between threads, the workers asleep between calls: where splitting pays "
                  "(about 15 seconds; the game stops meanwhile)."),
        command("Thread Pool Self-Test", "vr_jobs_test")
            .help("vr_jobs_test: the pool's checks (start and stop, every item once, the main thread helping, busy workers, "
                  "nested waits, the same results whatever the threads), a line each (under a second)."),
        command("Zancle Math Self-Test", "vr_zancle_math_test")
            .help("vr_zancle_math_test: Zancle's math (and the angle wrap) against the standard library's on edge values "
                  "(signed zeros, halves, wrap angles, infinities, NaN), to the last bit; one line."),
        command("SHA-256 Self-Test", "vr_sha256_test")
            .help("vr_sha256_test: the SHA-256 that checks each Map Library download against its index, on the standard "
                  "test vectors (FIPS 180-2's, a million a's, the padding's edges); one line."),
        command("Ragdoll Hand Probe", "vr_ragdoll_hand_probe")
            .help("vr_ragdoll_hand_probe: each hand holding a ragdoll's limb: the limb's lag, the hand drawn off its "
                  "controller onto it, its palm and fingertips from the limb's mesh (cm), the fingers that met it."),
        command("Grasp Bench", "vr_grasp_bench")
            .help("vr_grasp_bench: each hand's grasp solve on what it holds, timed (afresh, and again 1000 times)."),
        command("Grasp Sweep", "vr_grasp_sweep 5")
            .help("vr_grasp_sweep: each hand's grasp solved every way at 7 places, each on the pool and on one thread (the "
                  "same, to the last bit?), and the afresh solve timed both ways. Long output."),
        header("Memory"),
        cycle("Memory Log", "vr_memstats_log", {{0.f, "Off"}, {30.f, "Every 30 s"}, {60.f, "Every minute"}, {300.f, "Every 5 minutes"}})
            .help("Write memory use and the frame rate to quakevr/profile/memstats_<date>.csv, and after each map load."),
        toggle("Memory Log: GPU", vr_memstats_log_gpu)
            .help("Also sample the GPU for the Memory Log (its clocks, slowdowns, and each program's use of it: SteamVR, "
                  "Virtual Desktop), on a thread of its own. Off: only while profiling (Performance Profile, the Profiler Panel or its CSV Capture)."),
        command("Print Memory Now", "vr_memstats")
            .help("vr_memstats: video and system memory, the textures and models loaded, the frame times since the last one."),
        command("Allocation Sites", "vr_alloc_sites 300")
            .help("vr_alloc_sites [frames] [lines] [peak]: the main thread's C++ and C heap events over the next 300 frames by where they "
                  "were asked for (the commonest first: a frame's, the place, its caller) in the console. To find the buffers "
                  "a frame makes and frees. Includes kinds and requested bytes. Set peak to 1 for the busiest frame's stacks. Tracing affects timings."),
        command("Heap Allocator", "vr_heap")
            .help("vr_heap: what serves malloc/free and new/delete (mimalloc; the C runtime's with -nomimalloc), the "
                  "process's working set and commit, mimalloc's reserved and committed memory, pages, threads and arenas."),
        command("Heap Statistics", "vr_heap stats")
            .help("vr_heap stats: mimalloc's own statistics table and options in the console. Long output."),
        command("Heap Self-Test", "vr_heap test")
            .help("vr_heap test: every way the code allocates (malloc, calloc, realloc, strdup, new, aligned new, "
                  "_aligned_malloc) gives mimalloc's blocks, and the C runtime's own blocks go back to it; PASS or FAIL."),
        command("Heap Stress", "vr_heap stress 8 2000")
            .help("vr_heap stress [threads] [ms]: 8 threads allocating and freeing for 2 s (a quarter freed by another "
                  "thread), each block checked: million operations a second, the heap's contention. Stalls the game."),
        cycle("Heap: Purge Delay", "vr_heap_purge_delay", {{0.f, "At once"}, {10.f, "10 ms"}, {1000.f, "1 s (mimalloc's)"}, {-1.f, "Never"}})
            .help("vr_heap_purge_delay: how long the heap (mimalloc) keeps freed memory before giving it back to the system. "
                  "At once: the smallest working set. 1 s, mimalloc's own default: map loads about 15% quicker, 0.3 to 1 GB "
                  "more memory held after them."),
        cycle("Heap: Hold During Loads", "vr_heap_load_hold", {{0.f, "Off"}, {60000.f, "On"}})
            .help("vr_heap_load_hold: a map load keeps the memory it frees for its own later allocations, then gives it all "
                  "back at its first frame drawn. Off: freed memory goes back as Purge Delay says (a big map's hitbox "
                  "build much slower: its threads queue on the system's memory calls)."),
        command("Heap: Return Free Memory", "vr_heap collect")
            .help("vr_heap collect: mimalloc returns the memory it holds unused to the system, then vr_heap."),
        header("Crashes"),
        command("Crash the Game", "vr_debug_crash")
            .help("vr_debug_crash [access | abort]: crashes the game now, on purpose, to test the crash report (in a test run: "
                  "qvr_crash.txt, the stack, and qvr_crash.dmp in the game folder). The game quits!"),
        command("Fail a Zancle Assert", "vr_debug_crash assert")
            .help("vr_debug_crash assert (zassert: in Zancle's library): a failed ZA_ASSERT, to test its report (a Quake "
                  "error; in a test run, the crash report). Debug builds only (zassert: with QVR_ZANCLE_DEBUG). The game quits!"),
    };
}

// Printed once, to the console (and the wrist log).
za::Vector<Item> pageDebugReports()
{
    return {
        header("The Game"),
        command("Official Campaign Status", "vr_campaign_status").help("Owned data and language readiness; active native campaign context."),
        command("Dopa Finale Text", "loc_probe $map_dopa_endtext_final").help("Preview the resolved completion text. loc_probe <key> [arguments] also checks formatted expansion strings."),
        command("Campaign File Sources", "vr_campaign_probe").help("Actual resolved sources for VR progs and colliding official maps; vr_campaign_probe <filename> checks any virtual file."),
        command("Mission Pack Status", "vr_pack_status")
            .help("Prints whether Hipnotic and Rogue are available, missing or incomplete/corrupt. Both are optional for the Quake campaign."),
        command("Headset", "vr_status").help("vr_status: the backend, the eyes' sizes, the hidden area, the head's and hands' poses."),
        command("OpenXR Runtime Choice", "vr_xr_runtime_explain")
            .help("vr_xr_runtime_explain: what VR Settings > Headset > OpenXR Runtime chooses now and why: the runtimes installed and running, the order they are tried in."),
        command("Player", "vr_dumpplayer").help("vr_dumpplayer [client]: a player's VR fields in the game (hands, weapons, hotspots)."),
        command("Models Check", "vr_model_check 1")
            .help("vr_model_check 1: every entity's model index against its model's name, and your models against the "
                  "server's; each mismatch listed (a saved game loaded wrong: buttons drawn as gibs). 0 wrong is right."),
        command("View", "vr_dumpview").help("vr_dumpview: the hands, grips, palms, fingers and every entity drawn in the view (long)."),
        command("Bullet Time Now", "vr_bullettime").help("vr_bullettime: starts or stops bullet time, as the gadget's button."),
        command("Screen Tap Feedback", "vr_bullettime_tap_feedback_test 1")
            .help("vr_bullettime_tap_feedback_test [1]: the gadget's click and screen glitch of a tap (1: the activation's; "
                  "none: a double tap's first), without tapping."),
        command("Distortion Trails Test", "vr_bullettime_trails_test 5")
            .help("vr_bullettime_trails_test [count] [m/s] [distance]: shots across your view (a test's distortion "
                  "trails, whatever kinds are on): start bullet time first (or Distortion Trails: Always)."),
        command("Distortion Trails", "vr_bullettime_trails_list")
            .help("vr_bullettime_trails_list: each distortion trail (its kind, places, head, age), their strength now and "
                  "the vertices made."),
        command("Slow Motion Clocks", "vr_slowmo_probe")
            .help("vr_slowmo_probe [classname | number]: the time scale, the server's, real, slowed and client's clocks, the "
                  "player's origin and velocity, the main hand's speed and lag behind the controller (and an entity's)."),
        command("Body Calibration", "vr_bodycal_print").help("vr_bodycal_print: the body calibration's state and result."),
        command("Wrists and Grips", "vr_bodycal_debug").help("vr_bodycal_debug: one line a hand, next frame: the wrist and the grip."),
        header("World and Physics"),
        command("Physics Props", "vr_physics_list").help("vr_physics_list: the props in the physics (more with Physics Bodies logged)."),
        command("Ragdolls", "vr_ragdoll_list 2").help("vr_ragdoll_list [1|2]: each ragdoll (how long limp, its parts awake, where it lies; with 1 each part's mass, place and speed and the hands holding it; with 2 also how far its flames are from its limbs), and Box3D's bodies (Gibs and Corpses > Ragdoll Settings)."),
        command("Grunt's Ragdoll Rig", "vr_ragdoll_info").help("vr_ragdoll_info [model]: the rig derived from the grunt's animation: each bone's vertices, joint, pivot and limits, how well the bones fit the frames."),
        command("Corpses in the Physics", "vr_corpse_list").help("vr_corpse_list: each corpse's body (fixed or pushable, box or pose, mass), where it lies, and what touches it (Gibs and Corpses > Corpse Collision)."),
        command("Held Props", "vr_carry_check").help("vr_carry_check: each held prop's place and axes in the hand, drawn vs where the game has it, and the fist's gap to it (cm)."),
        command("Props in Floors", "vr_physics_sink").help("vr_physics_sink: how far each prop sinks into the floor."),
        command("Props in Walls", "vr_physics_inlevel").help("vr_physics_inlevel: how far each prop's box is inside walls, floors and doors (a prop held in both hands pushed into a wall should be at 0)."),
        command("Prop Approach", "vr_physics_approach").help("vr_physics_approach [number | classname]: how close your box gets to the first explosive box's face from 16 directions round it (the same from every side, and as close as to a wall: your half-width)."),
        command("Watch Props Entered", "vr_physics_inside 1").help("vr_physics_inside [1 | 0]: counts the frames you spend inside a solid prop's drawn shape (where none of your moves may go) and prints each time you pass into one; with no argument, the count so far (Misc/quakevr/propphase_sweep.py: the toppled-box sweep)."),
        command("Props Entered", "vr_physics_inside").help("vr_physics_inside: how many times, and frames, you were inside a solid prop's shape since the watch started, and how deep."),
        command("Weights", "vr_weight_table").help("vr_weight_table: the weapons' and props' masses (the level's props too)."),
        command("Throws by Weight", "vr_throw_table").help("vr_throw_table [hand m/s] [flick m/s]: how fast and far each "
            "weapon and prop is thrown, one hand and two, by a hard throw (8 m/s, 2 of it the wrist's flick), with "
            "Throws by Weight off and on; the least speed each hurts at, and the most spin one hand and two give it."),
        command("Ledges Ahead", "vr_climb_probe").help("vr_climb_probe: the ledges 16 to 64 units ahead of you, and why each holds or not."),
        command("Rocks and Bricks", "vr_debris_list").help("vr_debris_list: the rocks and bricks placed in this map."),
        command("Crates", "vr_crates_list").help("vr_crates_list: the crates in this map (health, resting), how many pieces lie about, and any two placed into each other."),
        command("Gear (Hidden When Dead)", "vr_gear_status").help("vr_gear_status: what of the gear is drawn (holstered guns and their parts, holster sleeves, ammo pouch, grenade pouch, flashlight and cord, wrist gadget, body) and whether the HUD is the status bar on a hand: dead, with Hide Gear When Dead, only the status bar."),
        command("Hit Detection", "vr_hitmodel_stats").help("vr_hitmodel_stats: precise hit detection's tests so far (hits, shots through a box beside the model) and their cost."),
        command("Wounds", "vr_wounds_info").help("vr_wounds_info: the wound masks in use."),
        command("Bloody Hands and Washing", "vr_gore_hands_info")
            .help("vr_gore_hands_info: the blood on your hands and body (texels), the wounds kept to re-open, the wash and its re-opening, and the blood on your weapons and props."),
        command("Decals and Gore", "vr_decal_count").help("vr_decal_count: the decals and gore pieces in the world."),
        command("Decals in Both Eyes", "vr_decal_eyes_test 90")
            .help("vr_decal_eyes_test 90: for 90 frames a blood mark is made between the eyes' views; then whether both "
                  "eyes drew the same marks each frame (console: 0 frames different)."),
        command("Particle Lighting", "vr_particle_light_report").help("vr_particle_light_report: the last frame's lit particles (Lit Particles), their mean light and colour against unlit, and the lightmap traces it took."),
        command("Model Lighting", "vr_model_ambient_show").help("vr_model_ambient_show: the six nearest entities' ambient light."),
        command("Ambient Occlusion", "vr_ao_show").help("vr_ao_show: the ambient occlusion's occluders and bake."),
        command("AO Bake Benchmark", "vr_ao_bench").help("vr_ao_bench [name part] [sse] [reference]: the loaded models' occlusion baked again on the main thread and timed (the game stops for a few seconds); in the console, \"sse\" and \"reference\" (the slow brute-force bake, half a minute on the firing range) bake them those ways too and check the bytes are the same."),
        command("AO Bakes on Disk", "vr_ao_cache_info").help("vr_ao_cache_info: the models' occlusion bakes read from the disk cache (cache/ao) this session, baked, rejected and written, and the folder's files."),
        toggle("Keep AO Bakes on Disk", vr_ao_cache).help("vr_ao_cache: the models' occlusion bakes kept on disk, so a later session's loads bake nothing (off: baked at every first load; 2 in the console: baked anyway and compared with the file)."),
        header("Hands and Weapons"),
        command("Hand Rig", "vr_hand_rig_info").help("vr_hand_rig_info: the hand's rig against the compiled one."),
        command("Grasp Spheres", "vr_grasp_spheres").help("vr_grasp_spheres: the fingers' and the palm's contact spheres."),
        command("Grip Frames", "vr_grip_frame")
            .help("vr_grip_frame: each hand's grip frame (the palm and the grip channel) that held props are placed by."),
        command("Hotspots Check", "vr_hotspots_check").help("vr_hotspots_check: each weapon's hotspots, checked against its model."),
        command("Sight Lines", "vr_sight_lines").help("vr_sight_lines: every weapon's sight line (Align Sights)."),
        command("Sight Check", "vr_sight_check").help("vr_sight_check: the held weapon's sights against its aim."),
        command("Wrist Gadget", "vr_gadget_info").help("vr_gadget_info: the gadget's pose (once it has been drawn)."),
        header("Other"),
        command("Limits", "vr_limits")
            .help("vr_limits: every hardcoded limit's usage against its maximum (cvars, memory, models, edicts, lights...)."),
        command("Network: Entities Sent", "vr_net_stats")
            .help("vr_net_stats [reset]: the entities in use, and for each client the entities in sight and sent in its last "
                  "datagram, their bytes and its room (1400 to a remote client), the peak, the mean, the frames that were full."),
        command("Server Rules", "vr_serverrules")
            .help("vr_serverrules: the settings the server judges by for every player (melee timing): their values, and on "
                  "a remote server's client the server's against yours."),
        command("Keyboard Hook", "vr_keyhook_status")
            .help("vr_keyhook_status: whether this game holds the desktop keyboard hook (only with the window's focus) and "
                  "the longest it went unserviced since the last report: every key press on the desktop waits for it."),
        command("Input Latency: Walk", "vr_inputlag_test key 10")
            .help("vr_inputlag_test key: a W key press sent through SDL once back in the game, then the "
                  "frames and ms to the bound command, the move sent, the server, the view, full speed, the release and "
                  "the stop (in the console). 'turn' and 'mouse' from the console."),
        command("Microphones", "vr_note_devices").help("vr_note_devices: the microphones Voice Notes can record from."),
        command("Detail Textures", "vr_detail_list").help("vr_detail_list: each texture's detail kind (long)."),
        command("External Maps", "vr_extmaps_stats all")
            .help("vr_extmaps_stats all: each texture of the map and what it got from the external pack (Graphics: External "
                  "Maps): its picture's match, normal, specular and glow maps (long; without 'all' only the totals)."),
        command("Animated Surfaces", "vr_extmaps_frames")
            .help("vr_extmaps_frames: each animated texture of the map (the wall buttons' +0basebtn...) frame by frame: the "
                  "frame whose surface it is drawn with, its normal, specular and glow maps and detail, and whether its "
                  "frames share one surface (Animated Surfaces A/B)."),
        command("Relighting: Texture Lights", "vr_relight_lights")
            .help("vr_relight_lights: the lights the map's glowing textures get with Graphics > Relighting's settings, a line "
                  "each texture (its kind, where its glow came from: fullbright pixels or a glow image's file, its lights), "
                  "and the lights into relight_lights.txt (to compare with relight_maps.py --list-glows)."),
        command("Relighting: Status", "vr_relight_status").help("vr_relight_status: the relighting's state (a batch's maps done, each light running: its stage and process id; the progress and time left), how the map in play is lit, the light.exe found."),
        command("Relighting: Tool Lookup", "vr_relight_get_tool status").help("vr_relight_get_tool status: the light.exe found (or not), the folder Download ericw-tools writes, the pinned file (version, size, sha256), its URL and the last download's result. vr_relight_tool_dir points both lookup and download at a test folder; vr_relight_tool_url at a test server."),
        command("Relighting: Batch's Maps", "vr_relight_batch -list").help("vr_relight_batch -list: the maps Graphics > Relighting's Relight These Maps would take (Maps, Episode, Game), with their files and sizes, without relighting them."),
        command("Console Completion Timing", "vr_console_complete_bench vr_s").help("vr_console_complete_bench <text> [runs]: the console's completion hint timed as each of the text's beginnings is typed (v, vr, vr_, vr_s), the way it is done now and the old way (each match sorted in as found: about half a second a key for v), and whether both give the same hint and the same list Tab shows."),
        command("Menu Rows", "menu_vr rows").help("menu_vr rows: this page's rows as drawn (MROW: row, top, label), the scroll, the section gap, and whether the laser and mouse find each row where it is drawn."),
        toggle("Menu Links: Print, Do Not Open", vr_menu_link_dryrun)
            .help("vr_menu_link_dryrun: the version box's Support on Ko-fi link (bottom right of the menus) prints its address "
                  "in the console instead of opening the desktop's browser (tests)."),
        command("Check for Updates Now", "vr_update_check_now")
            .help("vr_update_check_now: the release feed (latest.json) read now, whatever the hour's cache and Check for "
                  "Updates say, and what it found in the console; the notice above the version box follows. "
                  "vr_update_url points it at a test server, vr_update_test_version fakes the feed's version."),
        command("Update Check: Status", "vr_update_status")
            .help("vr_update_status: this game's version, the latest the feed gave (where from, how long ago, cached or "
                  "asked), the feeds, the cache file's age, and the notice shown."),
        command("OBS: Status", "vr_obs_status")
            .help("vr_obs_status: OBS's connection (Graphics > Recording > OBS): found or not and why, the address, whether "
                  "a password is set (never the password), obs-websocket's version, the recording's state and time, and "
                  "the menus' row as it reads."),
        cycle("OBS: Process Check", vr_obs_process_check, {{0.f, "Off (Always Connect)"}, {1.f, "On"}, {2.f, "Act as if Running"}})
            .help("vr_obs_process_check: On (the default): on this PC the game connects only while OBS's process "
                  "(obs64.exe) runs, and says when its WebSocket server is off. Off: always tries (a mock server's "
                  "tests). Act as if Running: the hint's test (nothing listening reads as OBS's server off)."),
        command("Update Notice: Fake 9.9.9", "vr_update_test_version 9.9.9")
            .help("vr_update_test_version 9.9.9: the update notice shows as for a newer release, without asking anyone "
                  "(its page: the feed's, else the releases' latest). Update Notice: Real Version undoes it."),
        command("Update Notice: Real Version", "vr_update_test_version \"\"")
            .help("vr_update_test_version \"\": the feed's own version again (no notice unless it is newer)."),
        command("Menu Help Fit", "menu_vr helpcheck").help("menu_vr helpcheck [columns]: every VR page's help wrapped as drawn: the pages whose box grew, the help shown in parts, the longest (HELPSUM)."),
        command("Main Menu Lettering", "vr_bigfont").help("vr_bigfont: which of the main menu's letters were cut from the menu pictures, and which were left out (a mod's own picture: the menu then shows the picture)."),
    };
}

// Rebuilds, reloads, debug images, test effects.
za::Vector<Item> pageDebugTools()
{
    return {
        header("Rebuild and Reload"),
        command("Rebuild Ledge Map", "vr_ledges rebuild").help("vr_ledges rebuild: the map's ledges found again (a moment), with their count."),
        command("Reload Models", "vr_model_reload")
            .help("vr_model_reload: every model read again from its file (edited in Blender), with the hands' and collisions' caches."),
        command("Reload Hand Model", "vr_hand_reload").help("vr_hand_reload: the jointed hand (progs/hand_rig.md5mesh) read again."),
        command("Reload Detail Textures", "vr_detail_reload").help("vr_detail_reload: the detail textures' settings read again, the textures rebuilt."),
        command("Reload Shaders", "vr_shader_reload")
            .help("vr_shader_reload [n]: the engine's shaders compiled again (n: QVR_SHADER_AB, 0 by default; a shader change's A/B in one run, paused, for images and timings)."),
        header("Save to Files (game folder)"),
        command("Wound Masks", "vr_wounds_dump").help("vr_wounds_dump: each model's wound mask, to wounds/mask_<n>_<model>.png."),
        command("Decal Atlas", "vr_decal_atlas").help("vr_decal_atlas: the decals' atlas, to decal_atlas.png."),
        command("Hand Mesh", "vr_grasp_dump").help("vr_grasp_dump: the main hand as drawn, to grasp_dump.obj (for Blender)."),
        command("Gadget Screen", "vr_gadget_screen_dump").help("vr_gadget_screen_dump: the wrist gadget's screen, to screenshots/gadget_screen.png."),
        command("Eye Images (with the UI)", "vr_eyeshot 3").help("vr_eyeshot 3: both eyes as the headset shows them (the HUD panel, the menu), to eyeshots/<map>_<n>_L.png and _R.png."),
        command("Texture Checksums", "imagehash").help("imagehash: every loaded texture's checksum as the GPU holds it, to imagehash.txt, and their sum in the console (two texture packs, or TGA and PNG files, compared)."),
        command("Reflection Map", "vr_envmap_dump").help("vr_envmap_dump: the held weapon's reflection map, to envmap.tga (Weapon Reflections on)."),
        command("Weight Test", "vr_weight_test csv").help("vr_weight_test: the weight's spring on test cases (swings, wrist snaps, wrist steps: pitch, yaw and roll), tables in the console and weight_test.csv."),
        header("External Map Index (vr_mapindex.cpp)"),
        command("Map Index Stats", "maps_stats")
            .help("maps_stats: how many packages the external index holds, by type / game_mode / map_size, how many carry their own progs.dat (left out unless the setting below is on), what the fetch cost, and where its cache is."),
        command("List Maps (newest first)", "maps_list limit=20").help("maps_list [text] [type=map] [mode=singleplayer] [size=large] [sort=date|bytes|title] [limit=20] [progs=1] [oldest=1]: the index as a table, newest first (maps_info <sha256> for one package)."),
        command("List Large Maps", "maps_list type=map size=large sort=bytes limit=40").help("maps_list type=map size=large sort=bytes: the big single-player maps, largest first."),
        command("Fetch the Map Index", "maps_fetch force")
            .help("maps_fetch force: Quaddicted's index fetched again now, on its own thread (the cached copy forgotten). Nothing waits for it; maps_stats says what happened."),
        toggle("Include Packages with progs.dat", vr_maps_allow_progs)
            .help("vr_maps_allow_progs: packages that ship their own progs.dat replace the game's code, so they are left out of the list by default. They are in the index either way (maps_info shows them, maps_stats counts them)."),
        slider("Download Cache Size", vr_maps_cache_mb, 0.f, 4096.f, 64.f, "%.0f MB").extend(0.f, 65536.f)
            .help("vr_maps_cache_mb: the downloaded packages' zips (cache/maps/) kept up to this size; past it the oldest are removed (before a download, at start-up, and when this is lowered). 0: none kept once a package is installed. Installed maps are not affected."),
        slider("Largest Download", vr_maps_max_download_mb, 0.f, 4096.f, 64.f, "%.0f MB").extend(0.f, 65536.f)
            .help("vr_maps_max_download_mb: a package whose zip is over this size is refused (the index's size, and the bytes as they arrive). 0: no limit (the default). The free disk space is checked either way, before the download and before the unpacking."),
        slider("Pretend Free Disk Space", vr_maps_debug_free_mb, 0.f, 4096.f, 64.f, "%.0f MB").extend(0.f, 65536.f)
            .help("vr_maps_debug_free_mb: test aid - the map installer is told the disk has this much free (a low disk simulated: its message, before the download or the unpacking). 0: the disk's real free space."),
        command("Download Cache Usage", "maps_cache")
            .help("maps_cache [trim]: the download cache's zips, oldest first (the first removed when it is over the size above), and how much of it they use. maps_cache trim: trimmed to the size now."),
        command("Map Browser Costs", "maps_page_stats")
            .help("maps_page_stats: the Map Library page - how many times its list was built and what it cost, what a frame of the page costs, and its layout, and where its first row, Uninstall and Reinstall are drawn. The list is built when the text, a filter or the index changes, never per frame."),
        command("Clear Search's Recent List", "menu_vr recent clear")
            .help("menu_vr recent [clear]: the Search page's results opened recently (quakevr/search_recent.txt), printed with where each is drawn, or cleared."),
        command("Open the Map Browser", "maps_page")
            .help("maps_page [text]: the Map Library page (the corner's Maps button, Single Player > Map Library), with the text typed in. maps_install <sha> gets a package, maps_play <sha> starts it."),
        header("Test Effects"),
        command("Blood and Gore", "vr_gore_test").help("vr_gore_test: blood and gore 64 units ahead, as a 40 damage hit."),
        command("Gore Burst", "vr_gore_test burst").help("vr_gore_test burst: a body bursting into gibs 64 units ahead."),
        command("Blood Mist", "vr_gore_mist_test").help("vr_gore_mist_test: a bleed's blood mist 64 units ahead (Gore > Blood Mist)."),
        command("Gib Blood on Hand", "vr_gore_hands_test main").help("vr_gore_hands_test main: a gib's blood on the main hand, as taking one (Gore > Bloody Hands and Washing)."),
        command("Wash a Quarter Off", "vr_gore_wash_test 0.25").help("vr_gore_wash_test 0.25: a quarter of the blood washed off your hands and body at once, as water over them (Gore > Bloody Hands and Washing)."),
        command("Blood from a Blow", "vr_gore_spatter_test blow").help("vr_gore_spatter_test blow: a blow's blood thrown onto what the main hand holds, the hand and the arm (Gore > Blood on You and Your Gear)."),
        command("List Clean Weapon Skins", "vr_cleanskins").help("vr_cleanskins: the weapon skins with a clean version (a patch beside the model: progs/<model>_<skin>.clean), whether it applies to your files, how often it was applied (Gore > Clean Weapon Skins)."),
        command("Blood from a Blow on Your Prop", "vr_gore_spatter_test prop").help("vr_gore_spatter_test prop: a blow on what the main hand holds (a weapon, a box, a crate, a brick), on its side facing you: the blood on held props (Gore > Blood on You and Your Gear)."),
        command("Blood from a Blow on the Off Hand's Prop", "vr_gore_spatter_test propoff").help("vr_gore_spatter_test propoff: as Blood from a Blow on Your Prop, on what the off hand holds (the super shotgun's blood stays on it broken open and shut)."),
        command("Blood from a Chainsaw Cut", "vr_gore_spatter_test saw").help("vr_gore_spatter_test saw: a chainsaw cut's spray just ahead of the main hand."),
        command("Blood from a Close Shot", "vr_gore_spatter_test shot").help("vr_gore_spatter_test shot: a shot hitting 40 units ahead of your eyes."),
        command("Marks on Your Main Forearm", "vr_gore_spatter_test arm main").help("vr_gore_spatter_test arm main: three bleeding marks on the main forearm alone: none on the other arm (chunky or fine)."),
        command("Gib Strikes Your Hand", "vr_gore_spatter_test gib").help("vr_gore_spatter_test gib: a gib flying into your main hand."),
        command("Blood from a Swing of Your Prop", "vr_gore_spatter_test propblow").help("vr_gore_spatter_test propblow: what the main hand holds swung away from you into a monster: the far side (the one that struck) takes the blood; a box lists its sides."),
        command("Blood on Your Left Hip's Weapon", "vr_gore_spatter_test holster 2").help("vr_gore_spatter_test holster <0..5>: a hit's blood just out from a holster's weapon (2 the left hip, 3 the right): holstered weapons take blood (Holstered Weapons Too)."),
        command("A Gibbing Ahead", "vr_gore_spatter_test burst").help("vr_gore_spatter_test burst [distance] [size]: a monster gibbed 48 units ahead: the weapons and props lying near take its blood, what lies right there soaked (Things Lying Near, Gibbed Monsters' Drops)."),
        command("Burn Your Arms", "vr_wounds_test self 4 90 0 12").help("vr_wounds_test self 4 90 0 12: an explosion's burns on your front and the arms held before you (Gore > Your Wounds' Detail)."),
        command("Wound Your Arms", "vr_wounds_test self 1 20 4 14").help("vr_wounds_test self 1 20 4 14: a shot's bleeding wound at your arms' height, held before your chest."),
        command("Soak Your Arms", "vr_wounds_test self 9 0 0 52").help("vr_wounds_test self 9 0 0 52: wet as from water up to your chest; dries in about 25 seconds."),
        command("Test Light", "vr_light_test").help("vr_light_test: a white light 48 units ahead for 5 seconds."),
        command("Test Message", "vr_message_test").help("vr_message_test: a message in the gadget's hologram (once the gadget has been drawn)."),
        command("Test Console Line", "vr_message_test console Test console log line").help("vr_message_test console <text>: an engine log line, as a warning: in the notify lines only with HUD and Menus > Screens > Console Log on the HUD (vr_hud_console_log 1)."),
        command("Notify Lines Info", "vr_notify_info").help("vr_notify_info: prints the notify lines shown now, in view and in the gadget's log (which of the game's messages and the console's log pass vr_hud_console_log)."),
        cycle("Gadget Screen Readings", "vr_gadget_test_state",
            {{0.f, "Real"}, {1.f, "Low"}, {2.f, "Exhausted, Counter"}, {3.f, "Hanging, Bullet Time"}, {4.f, "Relighting"}, {5.f, "Every Item"}})
            .help("vr_gadget_test_state: the wrist gadget's screen shows made-up readings, to see each state of its layout: low health, ammo and "
                  "stamina; no stamina with a counter's window open; hanging with bullet time running; maps being relit; every key, powerup and sigil."),
        command("Eject a Casing", "vr_shells_eject").help("vr_shells_eject: a spent casing out of the held weapon's port."),
        command("Lightning Shock", "vr_shock_test 0").help("vr_shock_test 0: the lightning gun's shock in water (the flash, the arcs over your arms and body), without the damage."),
        command("Lightning Strikes You", "vr_shock_self_test 10; vr_shock_self_info")
            .help("vr_shock_self_test [damage]: struck by a bolt of that much (10 a shambler's, 30 a lightning gun's): Quad's arcs over "
                  "your hands, arms and body (Gore > Lightning Shock > Arcs on You), without the damage. vr_shock_self_info: the arcs "
                  "on you last frame by part (console)."),
        command("Electrified Water", "vr_shock_test 1").help("vr_shock_test 1 [radius] [seconds]: arcs on the water below the point 128 units ahead."),
        command("Lightning Bolt at the Nearest", "vr_shock_hit_test 30")
            .help("vr_shock_hit_test <damage>: a lightning gun's bolt from your eyes into the nearest monster or corpse: its arcs and a burn "
                  "where it strikes; it keeps Quad's arcs crawling over it (Lightning Shock), alive or dead, a ragdoll convulsing, a kill charred."),
        command("Lightning Kill the Nearest", "vr_shock_hit_test -1").help("vr_shock_hit_test -1: the same, just enough to kill it (not to gib it)."),
        command("Shocked Bodies", "vr_shock_info; vr_shock_ragdoll_check")
            .help("vr_shock_info: the bodies with arcs on them (kind 3 a hit's, 4 lasting; the arcs drawn; next frame: how far off the "
                  "skin they lie, bodyshock-arcs); vr_shock_ragdoll_check: each "
                  "ragdoll's shock left, its limbs' turning speed, its fastest part and its joints' stretch (console)."),
        command("Smoke Off the Bodies Near", "vr_smoulder_test 10")
            .help("vr_smoulder_test [seconds]: every monster and body within 1000 units smokes that long as a lightning bolt's burns "
                  "would (Smouldering Smoke); vr_smoulder_info: the smoking bodies and the wisps made since the last print."),
        command("Smouldering Bodies", "vr_smoulder_info")
            .help("vr_smoulder_info: the smoking bodies (lightning's smoke left, the fire's flames out in, the smoke left) and the wisps made since the last print (console)."),
        command("Mjolnir's Lightning", "impulse 215").help("impulse 215: Mjolnir in the main hand strikes its lightning now, "
                                                            "as a blow does (15 cells). In water: the shock, with its damage."),
        header("Small Gibs Tests (developer 1 for each hit)"),
        command("A Grunt Ahead", "vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241").help("A grunt 96 units ahead, facing you: the tests' target (the nearest monster or corpse)."),
        command("A Grunt's Corpse Ahead", "vr_test_spawn 0; vr_test_spawn_dead 1; vr_test_spawn_dist 96; impulse 241; vr_test_spawn_dead 0"),
        toggle("On the Training Dummy", vr_smallgibs_test_dummy)
            .help("vr_smallgibs_test_dummy: the tests below hit the nearest training dummy instead (the firing range's), and the shotgun, nail, blow and chainsaw ones print the gore each sent (gore hits, wound events, blood particles): to compare with a grunt's."),
        command("A Grunt's Ragdoll Ahead", "vr_ragdoll 1; vr_test_spawn 0; vr_test_spawn_dead 1; vr_test_spawn_dist 96; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdolls) and a grunt killed 96 units ahead: he goes limp as he falls."),
        command("Blast Beside the Nearest Ragdoll", "vr_ragdoll_blast_test").help("vr_ragdoll_blast_test [damage]: a blast's push (no damage) 24 units beside the nearest ragdoll, on your side: it is thrown away from you."),
        command("Ragdoll and Prop Drawn Motion", "vr_drawn_motion_test 90 nearest")
            .help("vr_drawn_motion_test <frames> [<entity> | nearest | held]: over the next 90 frames, how evenly the nearest ragdoll's parts, the nearest loose prop (held: the one in your hand) and your hands move frame to frame (the console: uneven near 0 is smooth; about 2 with stalls: a step shown twice). Throw or hold one first. vr_debug_ragdoll 2: each frame's blend between the steps."),
        command("Drop the Nearest Prop on the Nearest Corpse", "vr_corpse_drop").help("vr_corpse_drop [height]: the loose prop nearest you put 32 units over the nearest corpse, to fall on it (Gibs and Corpses > Corpse Collision)."),
        command("Shotgun Blasts", "vr_smallgibs_test 1").help("vr_smallgibs_test 1: 200 shotgun blasts at it (vr_smallgibs_test_n), the share that tore small gibs out."),
        command("Super Shotgun Blasts", "vr_smallgibs_test 2"),
        command("Nails", "vr_smallgibs_test 3"),
        command("Axe, Pommel, Sword, Punch", "vr_smallgibs_test 4").help("vr_smallgibs_test 4: blows of 35 (vr_smallgibs_test_dmg) by the axe's blade and pommel, a sword, and punches of 25."),
        command("Quad Damage", "vr_smallgibs_test 5"),
        command("Chainsaw a Second", "vr_smallgibs_test 6"),
        command("Burst a Gib and a Head", "vr_smallgibs_test 7"),
        command("Brain Chunks Ahead", "vr_smallgibs_test 21")
            .help("vr_smallgibs_test 21: a head pop's brain chunks (Small Gibs > From a Head Pop) burst just ahead of you, to look at, pick up and throw."),
        command("Most Lying About", "vr_smallgibs_test 8").help("vr_smallgibs_test 8: 15 made with Most Lying About 10: the five oldest go."),
        command("Held, Then Let Go", "vr_smallgibs_test 9").help("vr_smallgibs_test 9: one in the off hand for 3 s with Last 1 s, then let go: it waits until it lands."),
        command("Pass Through the Body", "vr_smallgibs_test 10").help("vr_smallgibs_test 10: one from just behind the monster through it at 300 u/s, with the grace and without."),
        command("Throw Gibs at a Wall", "vr_smallgibs_test 12").help("vr_smallgibs_test 12: a gib and a small gib thrown at 220 u/s into the nearest wall stick (Thrown Gibs Stick 1 for it), a gib at 400 bursts."),
        command("Thrown Gibs Stick, by Mass", "vr_smallgibs_test_n 10; vr_smallgibs_test 22")
            .help("vr_smallgibs_test 22 (Step Up to the Wall Ahead first, a wall 220 units wide): gibs of 8, 12 and 20 kg, "
                  "a grunt's and an ogre's head and a small gib thrown at it as hand throws of 2 to 6 m/s make them, 10 each; "
                  "prints how many stuck or burst (developer 1)."),
        command("Step Up to the Wall Ahead", "vr_smallgibs_test 15")
            .help("vr_smallgibs_test 15: puts you 64 units (vr_smallgibs_test_dist) from the wall you face, to throw gibs at it (Gibs and Corpses: Thrown Gibs Stick, Speed to Stick)."),
        command("Flight by Situation", "vr_smallgibs_test 13")
            .help("vr_smallgibs_test 13: on the grunt or corpse ahead, each situation in turn (melee, chainsaw, guns, explosions, thrown, a monster's blow, gibbed by a blow and by a rocket, a large gib burst): launch speeds out and up, and how far they lie 2.5 s on (the console)."),
        command("Blow Up a Crowd", "vr_smallgibs_test 14")
            .help("vr_smallgibs_test 14: 24 grunts (vr_smallgibs_test_crowd) and 12 explosive boxes in rings ahead, a rocket's "
                  "blast among them 1 s on: what is left 3 s on; with Network Messages on (Debug), each frame the broadcast "
                  "was full."),
        command("List Small Gibs", "vr_smallgibs_test 11").help("vr_smallgibs_test 11: how many lie about, their size, how many are rigid bodies, asleep, stuck, held."),
        command("Gib a Body Underfoot: a Blow", "vr_smallgibs_test 16")
            .help("vr_smallgibs_test 16: the grunt or corpse nearest put under your feet and gibbed by your blow; 2 s on, the health you and the monsters near lost to its gibs (Fresh Gibs Harmless For; god off)."),
        command("Gib a Body Underfoot: a Shot", "vr_smallgibs_test 17")
            .help("vr_smallgibs_test 17: the same, gibbed by a super shotgun blast's damage."),
        command("Kill a Monster Underfoot", "vr_smallgibs_test 19")
            .help("vr_smallgibs_test 19: the live grunt nearest put under your feet and killed (not gibbed) by your blow; "
                  "4 s on, the health you lost and its drops' flung hits on you (Monster Drops rows; god off)."),
        command("Drop a Monster's Backpack on You", "vr_smallgibs_test 20")
            .help("vr_smallgibs_test 20: a monster's backpack dropped over your head: past Monster Drops Harmless For its "
                  "fall on you still hurts; 3 s on, the health you lost (god off)."),
        command("Throw a Gib Made 1 s Ago", "vr_smallgibs_test 18")
            .help("vr_smallgibs_test 18: a gib made at your feet, thrown at the monster nearest 1 s later: it still hurts it."),
        toggle("Trace Small Gibs", vr_smallgibs_trace)
            .help("vr_smallgibs_trace: each small gib's first 2.5 s in the console (sgibtrace:): where made, its speed and every jump in it, what touches, nudges or strikes it, how far it lay."),
        header("Training Dummy Tests (dummytest: ...)"),
        command("What the Dummy Is", "vr_dummy_test 1")
            .help("vr_dummy_test 1: the training dummy nearest: its enemy, model, box and health, and the zone a level shot at "
                  "its head, body and legs strikes (1 head, 0 body, 2 limbs, 3 legs)."),
        command("Hit the Dummy for 10", "vr_dummy_test 2").help("vr_dummy_test 2: a plain 10-damage hit from you: its health after."),
        command("Hit the Dummy for Its Health", "vr_dummy_test 3")
            .help("vr_dummy_test 3: a hit of just the health it has left: with Dummy Dies it dies as its enemy."),
        command("The Dummy's Health", "vr_dummy_test 4")
            .help("vr_dummy_test 4: its health now and how long since its last hit (it fills up again after Dummy Health Refills)."),
        header("Decapitation Tests (developer 1: decap: ...)"),
        command("A Zombie Ahead", "vr_test_spawn 2; vr_test_spawn_dist 96; impulse 241")
            .help("A zombie 96 units ahead (a map with zombies: the firing range), for the tests below."),
        command("A Mummy Ahead", "vr_test_spawn 14; vr_test_spawn_dist 96; impulse 241")
            .help("A mummy (Dissolution of Eternity) 96 units ahead, for the tests below: Slash at Its Head beheads it (its "
                  "ragdoll; killed otherwise, it gibs)."),
        command("Slash at Its Head", "vr_decap_test 1")
            .help("vr_decap_test 1: the nearest live monster's health 1, a sword's slash across its neck: it is beheaded "
                  "(decaptest: in the console)."),
        command("Axe Slash at Its Head", "vr_decap_test 9").help("vr_decap_test 9: the same with the axe's head."),
        command("Axe Swept Through Its Neck", "vr_decap_test 61")
            .help("vr_decap_test 61: the axe's slash, its blade swept through the neck as a hand's blow finds what it "
                  "strikes (a knocked-down one's ragdoll too: Knock Down the Nearest first): struck or missed, then "
                  "beheaded at health 1."),
        command("Stab at Its Head", "vr_decap_test 2").help("vr_decap_test 2: the blade driven along its line: killed, not beheaded."),
        command("Pommel at Its Head", "vr_decap_test 3").help("vr_decap_test 3: the pommel strikes: killed, not beheaded."),
        command("Slow Slash at Its Head", "vr_decap_test 4").help("vr_decap_test 4: half the Least Swing Speed: not beheaded."),
        command("Slash at Full Health", "vr_decap_test 7")
            .help("vr_decap_test 7: a light slash at the head at full health: a zombie is beheaded, others aren't."),
        command("Slash a Corpse's Head", "vr_decap_test 5").help("vr_decap_test 5: the nearest ragdoll or corpse beheaded by a slash."),
        command("Chainsaw at Its Neck", "vr_decap_test 6").help("vr_decap_test 6: the chainsaw's bar at the neck (health 1)."),
        command("Chainsaw a Corpse's Neck", "vr_decap_test 10").help("vr_decap_test 10: the nearest corpse's head cut by the chainsaw."),
        command("Throw an Axe at Its Head", "vr_decap_test 11; vr_test_axe_up 15; vr_test_axe_at 1; vr_test_axe_dist 70; vr_test_axe 0; impulse 209")
            .help("Its health 1, then an axe thrown edge first at its head from 70 units (a grunt's: Axe Height 15)."),
        command("Gib the Headless Corpse", "vr_decap_test 8")
            .help("vr_decap_test 8: the nearest headless corpse gibbed: no head thrown (the heads counted before and after)."),
        command("Shotgun at Its Head", "vr_decap_test 12")
            .help("vr_decap_test 12: a shotgun blast from your eye at the nearest live monster's head that just kills it: "
                  "its head pops, no head thrown (decaptest: popped 1)."),
        command("Super Shotgun at Its Head", "vr_decap_test 13").help("vr_decap_test 13: the same with a super shotgun blast."),
        command("Super Shotgun at Its Head, Overkill", "vr_decap_test 14")
            .help("vr_decap_test 14: a super shotgun blast at its head at health 1 (it would gib): its head pops, no gibs."),
        command("Lightning at Its Head", "vr_decap_test 15").help("vr_decap_test 15: a lightning bolt at its head at health 1: popped."),
        command("Shotgun at Its Body", "vr_decap_test 16").help("vr_decap_test 16: a shotgun blast at its body at health 1: killed, not popped."),
        command("Shotgun at Its Head, Not Killing", "vr_decap_test 17")
            .help("vr_decap_test 17: a shotgun blast at its head at health 500: not popped (a zombie: popped, dead for good)."),
        command("Lightning at Its Body", "vr_decap_test 18").help("vr_decap_test 18: a lightning bolt at its body at health 1: not popped."),
        command("Sweep the Head Zone", "vr_decap_test 19")
            .help("vr_decap_test 19: blows moved level at the nearest live monster's head from 16 sides, at heights 24 units below "
                  "its head's middle to 16 above: how many meet the melee's beheading zone at each (decapsweep: in the console)."),
        header("Limb Gore Tests (limbtest: ... in the console; developer 1: limbs: ...)"),
        command("A Grunt Ahead", "vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241").help("A grunt 96 units ahead, for the tests below."),
        command("A Grunt's Corpse Ahead", "vr_test_spawn 0; vr_test_spawn_dist 96; vr_test_spawn_dead 1; impulse 241; vr_test_spawn_dead 0")
            .help("A dead grunt 96 units ahead (a ragdoll), for the corpse tests."),
        command("Spawn Its Limbs", "vr_limb_test 1")
            .help("vr_limb_test 1: every limb of the nearest monster's model hung in a row before you: the limb models made "
                  "from its own (vr_limb_models lists them)."),
        command("List Its Limb Models", "vr_limb_models").help("vr_limb_models [model]: the grunt's (or that model's) limbs: each one's triangles, cap and size."),
        command("Cut a Corpse Apart, Ends First", "vr_limb_test 2")
            .help("vr_limb_test 2: the nearest corpse's limbs cut off one by one, hands and shins first, down to the torso, "
                  "then its head: the parts left after each."),
        command("Cut a Corpse Apart, Whole Limbs", "vr_limb_test 3").help("vr_limb_test 3: the same, whole arms and legs at once."),
        command("Slash at a Limb", "vr_limb_test 4")
            .help("vr_limb_test 4: the nearest live monster's health 1, a sword's slash at its forearm: it falls as a ragdoll, "
                  "the forearm flies off."),
        command("Punch a Limb", "vr_limb_test 5").help("vr_limb_test 5: a fist's killing blow there: popped by chance (vr_decap_pop_roll 0: always)."),
        command("Shotgun at a Limb", "vr_limb_test 6").help("vr_limb_test 6: a shotgun blast there that kills."),
        command("Lightning at a Limb", "vr_limb_test 7").help("vr_limb_test 7: a lightning bolt there that kills."),
        command("Explosion by a Limb", "vr_limb_test 9").help("vr_limb_test 9: an explosion beside it that kills: its limbs near it pop by chance."),
        command("Gib It", "vr_limb_test 10").help("vr_limb_test 10: gibbed: its own limbs thrown (Gibbed Bodies Throw Limbs)."),
        command("Slash at Full Health", "vr_limb_test 12").help("vr_limb_test 12: a light slash at a limb at full health: a zombie dies of it, others don't."),
        command("Chance Rates", "vr_limb_test 8").help("vr_limb_test 8: 2000 rolls at chance 0.5 for a head and a limb: the rates with Head Chance and Limb Chance."),
        command("Most Limbs", "vr_limb_test 11").help("vr_limb_test 11: twice Most Limbs Lying About thrown: how many stay."),
        command("Where Its Limbs Map", "vr_limb_test 13").help("vr_limb_test 13: each limb's surface point and the joint a hit there cuts."),
        command("The Limbs Lying About", "vr_limb_test 17").help("vr_limb_test 17: each limb thrown: its model, where it is, how fast (none fallen out of the world)."),
        command("Cut Ends' Fountains", "vr_limb_test 18")
            .help("vr_limb_test 18: each flying limb's (or head's) own fountain: its age, its spout (how far from the piece's "
                  "middle, and from the stump's joint as it was cut), the piece's speed and weight; the drops so far."),
        command("Limb Weights Here", "vr_limb_test 19")
            .help("vr_limb_test 19: every kind of monster on the map: what its head and each limb weigh cut off (kg, share of "
                  "its ragdoll's Mass; * a whole limb) (Limb Gore > Limb Weight)."),
        command("Enemy Laser at the Head", "vr_limb_test 20")
            .help("vr_limb_test 20: an enforcer's laser bolt (an enemy's) into the nearest living monster's head that kills "
                  "it (health 1): popped at Enforcer Laser's chance (vr_decap_pop_roll 0: always)."),
        command("Enemy Laser at a Limb", "vr_limb_test 21").help("vr_limb_test 21: the same at its forearm."),
        command("Hand to the Last Limb", "vr_limb_test 14").help("vr_limb_test 14: the mock main hand put on the last limb thrown (then grip: vr_mock_button main grip 1; vr_limb_test 15 says if it is held)."),
        header("Crowd Gore Tests (goretest: crowd ... in the console; the benchmarks' gore scenarios)"),
        slider("Crowd Radius", vr_gore_test_crowd, 0.f, 1024.f, 64.f, "%.0f units")
            .help("Above 0, the limb and head tests act on every monster this near you at once, in one frame, quietly (one "
                  "goretest: crowd line): Crowd: ... below, or any test above. 0: the nearest monster only (vr_gore_test_crowd)."),
        command("Spawn a Crowd (16 Grunts)", "vr_physics_spawn monster_army 100 -60;vr_physics_spawn monster_army 100 -20;vr_physics_spawn monster_army 100 20;vr_physics_spawn monster_army 100 60;vr_physics_spawn monster_army 148 -60;vr_physics_spawn monster_army 148 -20;vr_physics_spawn monster_army 148 20;vr_physics_spawn monster_army 148 60;vr_physics_spawn monster_army 196 -60;vr_physics_spawn monster_army 196 -20;vr_physics_spawn monster_army 196 20;vr_physics_spawn monster_army 196 60;vr_physics_spawn monster_army 244 -60;vr_physics_spawn monster_army 244 -20;vr_physics_spawn monster_army 244 20;vr_physics_spawn monster_army 244 60")
            .help("vr_physics_spawn: 16 grunts in a 4 by 4 grid ahead of you (notarget keeps them still)."),
        command("Crowd: Slash a Limb", "vr_limb_test 4").help("vr_limb_test 4 on the crowd (Crowd Radius above 0): each killed by a slash at a limb."),
        command("Crowd: Cut Corpses Apart", "vr_limb_test 3").help("vr_limb_test 3 on the crowd's corpses: every limb and the head cut off."),
        command("Crowd: Gib", "vr_limb_test 10").help("vr_limb_test 10 on the crowd: each gibbed (its limbs thrown: Gibbed Bodies Throw Limbs)."),
        command("Crowd: Head Pops", "vr_decap_test 14").help("vr_decap_test 14 on the crowd: a super shotgun headshot each (head pop by chance: vr_decap_pop_roll 0 pops all)."),
        header("Head Pop Chance Tests (poptest: ... in the console)"),
        slider("Test Range", vr_decap_poptest_dist, 0.5f, 25.f, 0.5f, "%.1f lengths")
            .help("The tests below shoot (or throw) from this many player lengths (56 units) off the nearest live monster's head "
                  "(you are moved there and back; round it till the way is clear) (vr_decap_poptest_dist)."),
        command("Chance Table", "vr_decap_test 40")
            .help("vr_decap_test 40: each weapon's head pop chance at ranges, for all, half and a fifth of the pellets at the head."),
        command("Shotgun Headshot Kill", "vr_decap_test 41").help("vr_decap_test 41: a shotgun blast (no spread) at its head at health 1, from the Test Range."),
        command("Super Shotgun Headshot Kill", "vr_decap_test 42").help("vr_decap_test 42: the same with the super shotgun."),
        command("Lightning Headshot Kill", "vr_decap_test 43").help("vr_decap_test 43: the same with a lightning bolt."),
        command("Super Shotgun Body Kill", "vr_decap_test 44").help("vr_decap_test 44: a super shotgun blast at its body at health 1: never popped."),
        command("Shotgun Rates", "vr_decap_test 45")
            .help("vr_decap_test 45: vr_decap_poptest_n shotgun blasts with its spread at its head from the Test Range, not killing "
                  "it: the mean chance, the rate it would pop, by pellets at the head."),
        command("Super Shotgun Rates", "vr_decap_test 46").help("vr_decap_test 46: the same with the super shotgun."),
        command("Throw a Weapon at Its Head", "vr_decap_test 47")
            .help("vr_decap_test 47: weapon vr_decap_poptest_wid (10: the rocket launcher) thrown at 16 m/s into its head at health 1."),
        command("Throw an Explosive Box at Its Head", "vr_decap_test 48").help("vr_decap_test 48: the same with an explosive box (that never blows up)."),
        command("Blunt Melee Rates", "vr_decap_test 49")
            .help("vr_decap_test 49: each blunt weapon's blow (fist, gun, crowbar, pommel, club, Mjolnir) at its head, soft, medium "
                  "and hard, vr_decap_poptest_n times, not killing it: the chance and the rate it pops."),
        command("Hard Punch Kill", "vr_decap_test 50").help("vr_decap_test 50: a hard punch at its head at health 1 (popped by chance)."),
        command("Hard Gun Butt Kill", "vr_decap_test 51").help("vr_decap_test 51: the same with a gun's blow."),
        command("Hard Crowbar Kill", "vr_decap_test 52").help("vr_decap_test 52: the same with the crowbar."),
        command("Hard Pommel Kill", "vr_decap_test 53").help("vr_decap_test 53: the same with a sword's pommel."),
        command("Hard Club Kill", "vr_decap_test 54").help("vr_decap_test 54: the same with a club."),
        command("Hard Mjolnir Kill", "vr_decap_test 55").help("vr_decap_test 55: the same with Mjolnir's head."),
        command("Nail at Its Head", "vr_decap_test 56").help("vr_decap_test 56: a nail at its head at health 1 (popped only with Quad Damage)."),
        command("Rocket at Its Head", "vr_decap_test 57").help("vr_decap_test 57: the same with a rocket."),
        command("Grenade at Its Head", "vr_decap_test 58").help("vr_decap_test 58: the same with a grenade."),
        command("Give Quad Damage", "impulse 255").help("impulse 255: Quad Damage for 30 s (the tests above: every head kill pops)."),
        header("Burning Tests (developer 1: burning: ...)"),
        command("Set It on Fire (a Torch's Blow)", "vr_burn_test 1")
            .help("vr_burn_test 1: the nearest monster, corpse, crate or crate's piece set on fire (an explosive box: it can't burn) as a lit torch's blow would, where it faces you "
                  "(A Grunt Ahead, A Grunt's Corpse Ahead above). Again: a flame more, the same damage."),
        command("Set It on Fire (a Lava Nail)", "vr_burn_test 2"),
        command("Set It on Fire (a Touch)", "vr_burn_test 3"),
        command("Load Lava Nails", "vr_burn_test 5").help("vr_burn_test 5: the nailgun or super nailgun in the main hand loaded with lava nails (impulse 156 or 157 gives one)."),
        command("A Nail Through a Torch's Flame", "vr_burn_test 6").help("vr_burn_test 6: a nail shot through the flame of the lit torch nearest you: it becomes a lava nail (Nails Through a Flame)."),
        command("Smash the Nearest Crate", "vr_burn_test 8")
            .help("vr_burn_test 8: the nearest crate broken as by your blow: plain pieces, which burn (Pieces Burn)."),
        command("Burn the Nearest Crate Through", "vr_burn_test 9")
            .help("vr_burn_test 9: the nearest crate burns through now (lit first if it isn't): charred pieces."),
        command("A Lava Nail at the Nearest Piece", "vr_burn_test 11")
            .help("vr_burn_test 11: a lava nail of yours fired down at the nearest crate's piece: it catches fire (Pieces Burn), not bursts; a charred one bursts."),
        command("Count the Pieces", "vr_burn_test 10").help("vr_burn_test 10: the console: the crates' pieces lying about, how many charred, how many burning."),
        command("How It Burns", "vr_burn_test 4").help("vr_burn_test 4: the nearest monster or corpse: where, its health, its flames and the time it burns yet."),
        header("Wall Torches Shot Off Their Walls (developer 1: walltorch: ...)"),
        command("Shoot the Nearest Wall Torch (a Pellet)", "developer 1; vr_test_walltorch_shot 10")
            .help("vr_test_walltorch_shot 10: the wall torch nearest you shot from your eyes with a shotgun's pellets: it comes off its wall and falls lit (VR Settings > Combat > Wall Torches > Shot Off Its Wall)."),
        command("Shoot It (a Nail)", "developer 1; vr_test_walltorch_shot 1")
            .help("vr_test_walltorch_shot 1: the same, with a nail."),
        command("Shoot It (a Rocket)", "developer 1; vr_test_walltorch_shot 0")
            .help("vr_test_walltorch_shot 0: the same, with a rocket."),
        command("Shoot It (a Grenade)", "developer 1; vr_test_walltorch_shot 2")
            .help("vr_test_walltorch_shot 2: the same, with a grenade. A grenade in flight is a thrown box, not a shot: "
                  "only shots and missiles stop at a torch, so its flight goes past it and its blast is what knocks it "
                  "off — see A Blast at It below."),
        command("Shoot It (a Lightning Bolt)", "developer 1; vr_test_walltorch_shot 21")
            .help("vr_test_walltorch_shot 21: the same, with a lightning bolt (LightningDamage, as the lightning gun)."),
        command("A Blast at It", "developer 1; vr_test_walltorch_shot 22")
            .help("vr_test_walltorch_shot 22: a blast at the wall torch nearest you, of a rocket's radius damage "
                  "(T_RadiusDamage): what a rocket's or a grenade's explosion does to the torches near it."),
        command("Throw the Nearest Prop at It", "developer 1; vr_test_walltorch_shot 20")
            .help("vr_test_walltorch_shot 20: the loose prop nearest you sent at the wall torch nearest you, at vr_test_fling_speed m/s: it comes off its wall. Below vr_test_fling_speed, it is not a throw: the torch stays."),
        toggle("Torch Flames to Console", vr_walltorch_debug)
            .help("vr_walltorch_debug: each lit torch you hold or that lies about, twice a second (wtflame: how far upside "
                  "down, its flame's foot, its speed and flattening, what of you its flame touches), and the torches' smoke "
                  "(wtsmoke). 2: every frame."),
        header("VR Calibration"),
        command("Run the Calibration Here", "vr_setup here")
            .help("vr_setup here: VR Calibration's steps (height, body, main hand) in this map, now."),
        command("Skip the Calibration Step", "vr_setup_skip").help("vr_setup_skip: its next step at once (not Body Calibration's poses)."),
        command("Check the Boards' Menu Paths", "vr_menu_path_check")
            .help("vr_menu_path_check: every menu page this map's boards name, with its path; a missing one prints MENU PATH MISSING."),
    };
}

// "Stamina 0.40: heavy x1.35, empty hand 0.8 kg, shake 0.21, run x0.98": the tired arms and legs now (Tests page).
[[nodiscard]] const char* staminaReadout()
{
    char(&text)[96] = readouts.stamina;
    q_snprintf(text, sizeof(text), "Stamina %.2f: heavy x%.2f, empty hand %.1f kg, shake %.2f, run x%.2f",
        fatigue::staminaLeft(), weight::staminaMultiplier(), za::max(vr_weight_stamina_empty.value, 0.f) * weight::staminaShare(),
        fatigue::shakeLevel(), fatigue::speedScaleFor(fatigue::staminaLeft()));
    return text;
}

// What tests are done with in the headset (single player).
za::Vector<Item> pageSpawnWeapons()
{
    return {
        header("Grabbable Pickups Ahead of You"),
        command("Shotgun", "vr_physics_spawn weapon_shotgun 64"),
        command("Super Shotgun", "vr_physics_spawn weapon_supershotgun 64"),
        command("Nailgun", "vr_physics_spawn weapon_nailgun 64"),
        command("Super Nailgun", "vr_physics_spawn weapon_supernailgun 64"),
        command("Grenade Launcher", "vr_physics_spawn weapon_grenadelauncher 64"),
        command("Rocket Launcher", "vr_physics_spawn weapon_rocketlauncher 64"),
        command("Lightning Gun", "vr_physics_spawn weapon_lightning 64"),
        command("Crowbar", "vr_physics_spawn weapon_crowbar 64"),
        command("Mjolnir", "vr_physics_spawn weapon_mjolnir 64"),
        command("Laser Gun", "vr_physics_spawn weapon_laser_gun 64"),
        command("Proximity Gun", "vr_physics_spawn weapon_proximity_gun 64"),
    };
}

za::Vector<Item> pageMachineHordeTests()
{
    return {
        header("Machine Horde Tests"),
        header("Reload the arena after destructive tests"),
        command("Acceptance", "vr_mg_horde_test 1"),
        command("Wave and Equipment Report", "vr_mg_horde_test 2"),
        command("Complete Current Wave", "vr_mg_horde_test 3"),
        command("Start Boss Wave", "vr_mg_horde_test 4"),
        command("Collect Spawned Key", "vr_mg_horde_test 5"),
        command("Death and Revival Check", "vr_mg_horde_test 6"),
        command("Drop Powerup", "vr_mg_horde_test 7"),
        command("Authored Keyed Button Check", "vr_mg_horde_test 8"),
        command("Reset Source Hunger Timer", "vr_mg_horde_test 9"),
        command("Activate Authored Deferred Monster", "vr_mg_horde_test 10"),
        command("Wave Monitor On", "vr_mg_horde_test 13")
            .help("Log each wave's squad budget and every squad's monsters to the console (developer 1)."),
        command("Wave Monitor Off", "vr_mg_horde_test 14"),
        command("All Players Report", "vr_mg_horde_test 15")
            .help("Coop: every player's health, death state, shared keys and frags (developer 1)."),
        command("Authored Keyed Door Check", "vr_mg_horde_test 18")
            .help("A real keyed door stays shut without shared keys, then spends exactly one and opens."),
        command("Team Wipe", "vr_mg_horde_test 17")
            .help("Destructive: every player dies; press fire to restart the arena (coop: revival needs a living teammate)."),
    };
}

// Dawn of the Machine (MG3) acceptance aids: its campaign only (Official Campaigns, or `vr_campaign_native mg3`), "mg3test:" lines
// with developer 1 (QC/vr_mg3_test.qc).
za::Vector<Item> pageMg3Tests()
{
    return {
        header("Dawn of the Machine Tests"),
        header("Dawn of the Machine campaign only (Official Campaigns)"),
        command("State Report", "vr_mg3_test 1")
            .help("Print the map, skill, serverflags and the player's health, caps, ammunition and weapons (developer 1)."),
        command("Seed Saved Upgrades", "vr_mg3_test 2")
            .help("Destructive: set every upgrade mask and both bloody weapon bits, for changelevel/save/death carry checks."),
        command("Add an Upgrade Bit", "vr_mg3_test 3")
            .help("Destructive: one more health upgrade bit this level; a death's restart must take it back."),
        command("Capacity Check", "vr_mg3_test 4")
            .help("Destructive, any campaign: print the health/ammo capacities, overfill and bound every ammunition, heal from 1."),
        command("Take This Map's Upgrades", "vr_mg3_test 5")
            .help("Destructive: empty health/ammo, take every capacity upgrade here by its real pickup, check masks, caps and refills."),
        command("Map Triggers Check", "vr_mg3_test 6")
            .help("Destructive, any campaign: silent teleports and door relays here, plus spawned always/multitouch/heal/music/quad/doorgroup/repeater/killmonster triggers."),
        command("Explosion Repeaters Check", "vr_mg3_test 7")
            .help("Destructive, any campaign: use every explosion repeater here (secret2: 60) and count their blasts 20 seconds later."),
        command("Monster Keys and Lore Check", "vr_mg3_test 8")
            .help("Destructive: a health_target monster hit down past its relay (fires once), an aggro_target group woken, a lore text shown and cleared."),
        command("Items Check", "vr_mg3_test 9")
            .help("Destructive: take every armour shard here, touch each draught, wear a lava suit in lava and slime, take the hell knight's head (Bloody Nightmare on)."),
        command("Take This Map's Runes", "vr_mg3_test 10")
            .help("Destructive: bring out a hidden rune and take every rune here by its pickup; prints serverflags."),
        command("Hub Rune Check", "vr_mg3_test 11")
            .help("On the hub: fire its rune check as entering does; 6 s later each rune's doors and the exit (all four runes) are checked."),
        command("Rune Count Report", "vr_mg3_test 12")
            .help("Monsters, items, intermission views and corpses here with this many runes (NOT_IF_n_RUNES removes the others)."),
        command("Walk Into the Exit", "vr_mg3_test 13")
            .help("Destructive: touch this map's first changelevel (its route); then Leave the Intermission."),
        command("Leave the Intermission", "vr_mg3_test 14")
            .help("One button press of the intermission (text, then the next map)."),
        command("Hub Skill Buttons Check", "vr_mg3_test 15")
            .help("On the hub: its Bloody Nightmare relays' state, then skill buttons 4, 1, 4 (Bloody Nightmare on, off, on)."),
        command("Seed a Full Loadout", "vr_mg3_test 16")
            .help("Destructive: guns, a sword, the shotgun and the Super Axe in the holsters, yellow armour, ammunition; then change level."),
        command("Loadout After Bloody Nightmare", "vr_mg3_test 17")
            .help("After a level change: in a Bloody Nightmare game only the axe, shotgun, Super Axe (and the bloody super shotgun) are left."),
        command("Bloody Nightmare Damage Check", "vr_mg3_test 18")
            .help("Destructive: 50 on a monster and 10 from it on you: 80% and 120% on Bloody Nightmare, else 100%."),
        command("Bloody Nightmare New Game Flags", "vr_mg3_test 19")
            .help("Destructive: Bloody Nightmare on, found and its new game (upstream impulses 223/224), skill 3."),
        command("Walk Into the Exit to secret2", "vr_mg3_test 20")
            .help("On the hub: touch its final exit; in Bloody Nightmare's new game it leads to boss2."),
        command("Chthon Beaten (Ending)", "vr_mg3_test 21")
            .help("Destructive: the Chthon ending as if he died: the finale text and the credits, or on Bloody Nightmare its new game on map1. Then Leave the Intermission."),
        command("Shub Beaten (Ending)", "vr_mg3_test 22")
            .help("Destructive: the Shub ending: the final text, then the credits. Then Leave the Intermission."),
        command("Route Carry Report", "vr_mg3_test 31")
            .help("Runes, health, ammunition, upgrade masks and capacities, hands' and holsters' weapons, magazines and ids: compare across a level change, save/load or death (the route sweep's report)."),
        command("Seed Hands and Holsters", "vr_mg3_test 32")
            .help("Destructive: shotgun and nailgun in the hands (magazines 3/7; hold both grips to carry them), six shotguns holstered (1..6), upgrade masks 5 2 8192 16384 1."),
        command("Give the Four Runes", "vr_mg3_test 36")
            .help("Destructive: all four runes (serverflags 15), e.g. to open the hub's exit to secret2 in a Bloody Nightmare new game."),
        slider("Ghosts' Opacity", vr_mg3_ghost_alpha, 0.05f, 1.f, 0.05f, "%.2f")
            .help("How solid Dawn of the Machine's ghosts look: 0.4 see-through (default), 1 solid."),
        toggle("Aggro Groups", vr_mg3_aggro_groups)
            .help("A waking monster wakes what its aggro_target names (map3, map7, map8). Upstream ships this off; off by default."),
    };
}

// Dawn of the Machine's weapons (MG3_PLAN.md M3-11..14): "mg3wtest:" lines with developer 1 (QC/vr_mg3_weapons_test.qc).
// The Super Axe works in any campaign when the Dawn of the Machine data is there (its models are read from it in place).
za::Vector<Item> pageMg3WeaponTests()
{
    return {
        header("Dawn of the Machine Weapons"),
        command("A Super Axe in Your Hand", "impulse 168")
            .help("The Super Axe in the main hand (impulse 188: the off hand). Strike a monster twice within Super Axe Burst "
                  "Window (Combat > Weapon Damage) for the lightning burst (15 cells; the head glows while it is ready)."),
        command("A Super Axe Pickup Ahead", "vr_physics_spawn weapon_superaxe 64")
            .help("A weapon_superaxe lying 64 units ahead, as a map places one."),
        command("Weapons Report", "vr_mg3_wtest 1")
            .help("The Dawn of the Machine data, your hands, holsters and cells, the Super Axe's burst chain (developer 1)."),
        command("Super Axe Blows and Burst", "vr_mg3_wtest 2")
            .help("Destructive: two ogres and a zombie ahead, struck by the Super Axe: first blow, burst, window, another "
                  "monster, water, cells, zombie and killing blows checked over a few seconds."),
        command("Super Axe Pickup", "vr_mg3_wtest 3")
            .help("Destructive: a pickup ahead taken by the empty main hand: its target fired, its silent drop (16 units)."),
        command("Map2's Super Axe", "vr_mg3_wtest 4")
            .help("Destructive, Dawn of the Machine's map2: its weapon_mjolnir is a Super Axe; take it: the secret counted, "
                  "dropped 2047 units below, as Dawn of the Machine does."),
        command("Super Axe in Hand and Holster", "vr_mg3_wtest 5")
            .help("Destructive: a Super Axe in the main hand and the first holster (then change level or save and load, "
                  "and Weapons Report)."),
        command("Axe Buttons Check", "vr_mg3_wtest 6")
            .help("Destructive, a Dawn of the Machine map with axe buttons (map6, map7, map8, secret5): each stays shut to "
                  "a shot and a blast, then opens to a blow (fist, Super Axe, thrown weapon, thrown prop, headbutt)."),
        command("Stand Before an Axe Button", "vr_mg3_wtest 7")
            .help("You stand facing the nearest closed axe button: shoot it (it says to use the axe), then strike it."),
        command("Did the Axe Button Open", "vr_mg3_wtest 8").help("The nearest axe button's state (developer 1)."),
        command("Laser Cannon Bolts", "vr_mg3_wtest 9")
            .help("A bolt's damage (Dawn of the Machine's 15, lit 20; elsewhere 18, 25), then 12 bolts at the floor ahead: "
                  "their bounces (0.9 of the damage kept) and stops."),
        command("Take the Nearest Laser Cannon", "vr_mg3_wtest 10")
            .help("Destructive: the nearest weapon_laser_gun into the empty main hand (map2b)."),
        command("Bloody Bits On", "vr_mg3_wtest 11")
            .help("Both bloody shotguns' bits set for this game: every shotgun refires in 0.28 s, every super shotgun "
                  "fires 28 pellets."),
        command("Bloody Bits Off", "vr_mg3_wtest 15"),
        command("Bloody Report", "vr_mg3_wtest 12").help("The bloody bits, the shotgun's refire, your shells (developer 1)."),
        command("Take the Bloody Shotguns", "vr_mg3_wtest 13")
            .help("Destructive: the map's bloody shotguns (none: two spawned ahead) taken by the empty main hand."),
        command("Bloody Nightmare New Game Flag", "vr_mg3_wtest 14")
            .help("Destructive: serverflags 256 set (then change level: the map's bloody shotguns stay)."),
    };
}

// Dawn of the Machine's monsters (MG3_PLAN.md M3-15..18): "mg3mtest:" lines with developer 1 (QC/vr_mg3_monsters_test.qc).
// The infected are stock monsters (any campaign); MG3's own monsters need its data (their models read from it in place).
za::Vector<Item> pageMg3MonsterTests()
{
    return {
        header("Dawn of the Machine Monsters"),
        command("An Infected Grunt Ahead", "vr_test_spawn 30; vr_test_spawn_dist 128; impulse 241")
            .help("An infected grunt 128 units ahead: killed, he bursts and gets up as a zombie (counted once, as it)."),
        command("An Infected Knight Ahead", "vr_test_spawn 31; vr_test_spawn_dist 128; impulse 241"),
        command("An Infected Enforcer Ahead", "vr_test_spawn 32; vr_test_spawn_dist 128; impulse 241")
            .help("An infected enforcer: killed, he bursts and gets up as a fiend."),
        command("An Infected Death Knight Ahead", "vr_test_spawn 33; vr_test_spawn_dist 128; impulse 241"),
        command("A Death Knight Lying as a Corpse", "vr_test_spawn_flags 65536; vr_test_spawn 33; vr_test_spawn_dist 128; impulse 241; vr_test_spawn_flags 0")
            .help("An infected death knight lying as Dawn of the Machine's corpses lie (not solid) until woken: shoot him, "
                  "or wake him (he rises, his death backwards)."),
        command("Monsters Report", "vr_mg3_mtest 1")
            .help("The map's Dawn of the Machine monsters by kind, the infected turned, the kills (developer 1)."),
        command("Infected Check", "vr_mg3_mtest 2")
            .help("Destructive: an infected grunt, knight, enforcer and death knight ahead, each killed (bursts into a "
                  "zombie or a fiend, not counted), then killed again (counted once each)."),
        command("Lying Death Knight Check", "vr_mg3_mtest 3")
            .help("Destructive: a death knight lying as a corpse (not solid, his last death frame), woken: he rises."),
        command("A Rocket Ogre Ahead", "vr_test_spawn 34; vr_test_spawn_dist 192; impulse 241")
            .help("Dawn of the Machine's rocket ogre (its data read in place): volleys of two rockets; bat them back."),
        command("A Rocket Ogre's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 34; vr_test_spawn_dead 1; vr_test_spawn_dist 128; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0"),
        command("A Demo Dog Ahead", "vr_test_spawn 35; vr_test_spawn_dist 192; impulse 241")
            .help("Dawn of the Machine's demo dog: its leap onto you kills it; however it dies, three grenades spill."),
        command("Demo Dog Check", "vr_mg3_mtest 6")
            .help("Destructive: demo dogs ahead: one shot dead beside a grunt (grenades, the grunt hurt), one landing on "
                  "you, one beheaded (it lies headless, the grenades spill all the same)."),
        command("A Ranged Knight Ahead", "vr_test_spawn 36; vr_test_spawn_dist 256; impulse 241")
            .help("Dawn of the Machine's ranged knight: fans of diamonds at range; bat or parry them."),
        command("A Ranged Knight's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 36; vr_test_spawn_dead 1; vr_test_spawn_dist 128; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0"),
        command("Ranged Knight Check", "vr_mg3_mtest 7")
            .help("Destructive: a ranged knight ahead: its model, class, head zone; made to cast (six diamonds, you hurt), "
                  "killed (its death frames), another gibbed (the death knight's head)."),
        command("Rocket Ogre Check", "vr_mg3_mtest 5")
            .help("Destructive: a rocket ogre ahead: its model, class, head zone; made to shoot (two rockets, you hurt), "
                  "killed (its corpse, no chainsaw dropped)."),
    };
}

// Dawn of the Machine's monsters, M3-19..23 (MG3_PLAN.md): "mg3btest:" lines with developer 1 (QC/vr_mg3_bestiary_test.qc).
// They spawn in any campaign when the Dawn of the Machine data is there (read from it in place).
za::Vector<Item> pageMg3BestiaryTests()
{
    return {
        header("Dawn of the Machine Bestiary"),
        command("An Orb Ahead", "vr_test_spawn 40; vr_test_spawn_dist 160; impulse 241")
            .help("Dawn of the Machine's orb 160 units ahead: a flying eye that sees behind it too, bursts spheres at you, and "
                  "blows up where it lands when killed."),
        command("A Slime Ahead", "vr_test_spawn 41; vr_test_spawn_dist 160; impulse 241")
            .help("Dawn of the Machine's slime: a spawn that, blowing up, throws blobs that become spawns, twice over."),
        command("A Ghost Ahead", "vr_test_spawn 42; vr_test_spawn_dist 128; impulse 241")
            .help("Dawn of the Machine's ghost: it drifts about; touch it (a hand will do) and it fades away."),
        command("A Sacrifice Ahead", "vr_test_spawn 43; vr_test_spawn_dist 96; impulse 241")
            .help("A hanging sacrifice (Dawn of the Machine's misc_sacrifice): struck down, it is gibbed."),
        command("A Lava Man Ahead", "vr_test_spawn 44; vr_test_spawn_dist 200; impulse 241")
            .help("Dawn of the Machine's lava man (MG3's model): it rises, stands as it throws lava balls, takes 0.8 of anything "
                  "but the lightning gun and the laser cannon."),
        command("A Super Shambler Ahead", "vr_test_spawn 45; vr_test_spawn_dist 200; impulse 241")
            .help("Dawn of the Machine's blood shambler: 2000 health, plasma sprays on its blows, lightning near and far."),
        command("A Super Shambler's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 45; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on and a super shambler killed at the Distance ahead (Debug > Tests > Ahead of You): he goes limp as he "
                  "falls (his own rig on MG3's model)."),
        command("Bestiary Report", "vr_mg3_btest 1")
            .help("The Dawn of the Machine data each monster needs, and how many of each this map has (developer 1)."),
        command("Orb Test", "vr_mg3_btest 2")
            .help("Destructive: an orb ahead, woken (god mode meanwhile): its eyes, its spheres, its pain, its death and blast "
                  "(developer 1)."),
        command("Ghost, Sacrifice and Slime Test", "vr_mg3_btest 3")
            .help("Destructive: a ghost laid to rest by your touch, a sacrifice struck down (its target fired), a slime's two "
                  "generations of spawns counted as they start and die (developer 1)."),
        command("Lava Man Test", "vr_mg3_btest 5")
            .help("Destructive: a lava man of Dawn of the Machine's ahead (god mode): it rises and flies, its first hit staggers "
                  "it, its damage rule, its throws, its death by a use; a Rogue lava man beside it unchanged (developer 1)."),
        command("Super Shambler Test", "vr_mg3_btest 7")
            .help("Destructive: a super shambler ahead (god mode): its plasma sprays, its lightning, half damage from blasts, its "
                  "parries and head zone; killed: his ragdoll (developer 1)."),
        command("Quake's Monsters in Dawn of the Machine", "vr_mg3_btest 10")
            .help("Destructive: backpacks, skill 3's harder skins and pain rest, the shambler's lightning, the vore's balls, Bloody "
                  "Nightmare's extra lasers, spikes and grenades, hanging and close-throwing zombies; it expects them only in "
                  "Dawn of the Machine (and Bloody Nightmare: the next row) at skill 3 (developer 1)."),
        command("Bloody Nightmare On", "vr_mg3_btest 11")
            .help("Destructive: serverflags 64 (Dawn of the Machine's Bloody Nightmare) for the test above."),
        command("The Map's Sacrifices", "vr_mg3_btest 4")
            .help("Destructive (Dawn of the Machine's map8): every sacrifice used in turn; the counter's count down to its "
                  "target fired (developer 1)."),
    };
}

za::Vector<Item> pageMg3ShubTests()
{
    return {
        header("Dawn of the Machine: Shub"),
        command("Shub Ahead (Free)", "vr_test_spawn 60; vr_test_spawn_dist 256; impulse 241")
            .help("Dawn of the Machine's Shub-Niggurath 256 units ahead (needs room: she is 256 units wide): volleys of diamonds, "
                  "autoguns, lobbed plasma, a sweeping beam, eyes and a seeker by her phase; she raises shub zombies where the "
                  "map has their spawns. One from here is free: killed, she bursts but ends nothing."),
        command("A Shub Zombie Ahead", "vr_test_spawn 61; vr_test_spawn_dist 96; impulse 241")
            .help("One of Shub's zombies: Quake's zombie lying there, up 7 s later and after you; it throws its flesh only up close."),
        command("Shub's Eye Ahead", "vr_test_spawn 62; vr_test_spawn_dist 160; impulse 241")
            .help("One of Shub's eyes: it hangs there and after 2 s spirals 72 spheres at you, then bursts. 120 health."),
        command("Shub's Seeker Ahead", "vr_test_spawn 63; vr_test_spawn_dist 200; impulse 241")
            .help("Shub's seeker eye: it chases you, faster and faster; its touch does 500. 300 health."),
        command("Shub Report", "vr_mg3_shubtest 1")
            .help("Her data, the map's Shub (phase, health), its shub zombie spawns, pillars, zombies and children (developer 1)."),
        command("Shub Phases Test", "vr_mg3_shubtest 2")
            .help("Destructive (god mode meanwhile): the map's Shub (boss2: you are taken to the arena) or a free one ahead: her "
                  "wounds through her four phases, her thrash, her waves, each phase's children, her autoguns, every child "
                  "dead (developer 1)."),
        command("Shub Zombies and Pillars Test", "vr_mg3_shubtest 3")
            .help("Destructive (boss2; god mode meanwhile): her zombies raised at the map's spawns (33 at most), up 7 s later, "
                  "her spheres and their flesh passing each other; the pillars sinking 20 a hit, with you on one: down with it, "
                  "the floor holding you as it sinks on (developer 1)."),
        command("Shub Death Test", "vr_mg3_shubtest 4")
            .help("Destructive (god mode meanwhile): Shub killed: her children cleared, the lights out, her burst; the map's "
                  "then shows the final text and the credits (developer 1)."),
        command("Training Dummy as Shub Test", "vr_mg3_shubtest 5")
            .help("Destructive: a training dummy 300 units ahead as Shub (free), then as her eye: what it is, killed as each, "
                  "nothing ended, a dummy back; Dummy Enemy reset to the grunt (developer 1)."),
    };
}

// Dawn of the Machine's Chthon, M3-24/25 (MG3_PLAN.md): "mg3ctest:" lines with developer 1 (QC/vr_mg3_chthon_test.qc).
za::Vector<Item> pageMg3ChthonTests()
{
    return {
        header("Dawn of the Machine: Chthon"),
        command("Chthon Ahead", "vr_test_spawn 50; vr_test_spawn_dist 600; impulse 241")
            .help("Dawn of the Machine's Chthon 600 units ahead (any map with its data), woken at you: fans and volleys of "
                  "spheres, six fits as he is hurt, 0.8 damage but from lightning and lasers. He ends no game here."),
        command("Chthon Report", "vr_mg3_ctest 1")
            .help("This map's Chthon (phase, health, fits), the second arena's points, lava suits, music (developer 1)."),
        command("Chthon Test", "vr_mg3_ctest 2")
            .help("Destructive: a Chthon ahead (god mode): his damage rules, every fit and phase, the spiral, his death, rings "
                  "and gibs (developer 1)."),
        command("The Boss Map's Fight", "vr_mg3_ctest 3")
            .help("Destructive (Dawn of the Machine's boss map, before the fight): you walk into his trigger; each fit and phase "
                  "driven, the waves, both teleports, his death (god mode; developer 1)."),
    };
}

za::Vector<Item> pageDebugTests()
{
    return {
        command("Official Triggers: Acceptance Test", "vr_mg_trigger_test 1")
            .help("Check native target timing, cancellation, lightning, fades and comfortable quake feedback. Reload afterward."),
        command("Official Triggers: Retarget Teleporter", "vr_mg_trigger_test 4")
            .help("Destructive: redirect a visible native teleporter and report the new cached destination. Reload afterward."),
        command("Official Triggers: Lightning Damage", "vr_mg_trigger_test 3")
            .help("Check native positional lightning damage, backwards traces and wetsuit protection in a clear corridor."),
        command("Machine: Hub and Rune Acceptance", "vr_mg_hub_test 1")
            .help("Destructive rune/return checks in the Machine hub. Reload afterward."),
        command("Machine: Electrode and Rune Egg", "vr_mg_hub_test 2")
            .help("Destructive authored mge2m2 puzzle test. Reload afterward."),
        command("Machine: Equipment Carry Setup", "vr_mg_hub_test 20")
            .help("Destructive: seed independent hand/holster magazines for save/carry checks. Hold both grips and reload afterward."),
        command("Whole-Map Monsters: Report", "vr_test_monsters 1")
            .help("Count this map's monsters: live, awake (hunting someone), dead, waiting for their trigger (one "
                  "monsterstest: line)."),
        command("Whole-Map Monsters: Bring In the Waiting", "vr_test_monsters 4")
            .help("Every monster still waiting for its trigger (the official campaigns' deferred monsters) comes in at "
                  "once: the map's full count. Reload afterward."),
        command("Whole-Map Monsters: Wake All", "vr_test_monsters 2")
            .help("Every live monster of the map hunts you at once, as if it had seen you: the full count's AI "
                  "(the benchmarks' mg3_*_awake). Reload afterward."),
        command("Whole-Map Monsters: Kill All", "vr_test_monsters 3")
            .help("Every live monster of the map dies at once (just enough damage: deaths, corpses and up to Most "
                  "Ragdolls ragdolls, vr_ragdoll_max). The benchmarks' mg3_*_kill. Reload afterward."),
        open("Stealth AI", pageIndex(pageStealthTests))
            .help("The stealth AI's scenes on e1m1 (Combat > Stealth AI): seeing, hearing, touch, investigating."),
        open("Machine Horde Tests", pageIndex(pageMachineHordeTests))
            .help("Authored waves, currency, physical rewards, revival and saved equipment. Developer arena only."),
        open("Dawn of the Machine Bestiary", pageIndex(pageMg3BestiaryTests))
            .help("Dawn of the Machine's monsters in any campaign with its data: the orb, ..."),
        open("Dawn of the Machine: Shub", pageIndex(pageMg3ShubTests))
            .help("Dawn of the Machine's Shub-Niggurath (any campaign with its data; boss2's own): her phases, children and death."),
        open("Dawn of the Machine: Chthon", pageIndex(pageMg3ChthonTests))
            .help("Dawn of the Machine's Chthon in any campaign with its data, and its boss map's fight."),
        open("Dawn of the Machine Weapons", pageIndex(pageMg3WeaponTests))
            .help("The Super Axe (any campaign with the Dawn of the Machine data), the axe buttons, the laser cannon, the bloody shotguns."),
        open("Dawn of the Machine Monsters", pageIndex(pageMg3MonsterTests))
            .help("The infected (any campaign) and Dawn of the Machine's own monsters (with its data): spawns and checks."),
        open("Dawn of the Machine Tests", pageIndex(pageMg3Tests))
            .help("MG3 native port: state, saved upgrades and capacities. Dawn of the Machine campaign only."),
        command("Machine: Progression Report", "vr_mg_hub_test 3")
            .help("Report runes, return position, final gate and VR equipment."),
        command("Machine: Walk Into the Next Exit", "vr_mg_hub_test 30")
            .help("Put you inside this map's exit (the hub: the next episode's gate, or the final gate), the real trigger "
                  "takes you on; jump presses leave the intermission. The campaign route test's step."),
        command("Machine: Die Here", "vr_mg_hub_test 36")
            .help("God mode off and a killing blow; a jump press respawns (single player: the last save loads)."),
        command("Machine: mge5m2 Trigger Route", "vr_mg_trigger_test 2")
            .help("Destructive authored rune puzzle and quake sequence on mge5m2. Uses real buttons and engine movement. Reload afterward."),
        command("Official World: Fog Report", "vr_mg_world_test 1")
            .help("Print the native campaign's authored world and player fog values."),
        command("Official World: Environment Test", "vr_mg_world_test 2")
            .help("Check native movement/fog and explode a test barrel on e5m1. Developer campaign only; reload the map afterward."),
        command("Official Monsters: Activation Test", "vr_mg_world_test 3")
            .help("Check triggered official monsters, counts and campaign flag isolation. Reload afterward."),
        command("Official Campaign: Progress Report", "vr_mg_world_test 4")
            .help("Print monster counts and both hands' persistent weapon magazines/ids."),
        command("Dopa: Ordinary Health Pickup", "vr_mg_world_test 11")
            .help("Destructive acceptance: set health40 and use a real health box; report the cap. Reload afterward."),
        command("Dopa: Megahealth Carry Setup", "vr_mg_world_test 12")
            .help("Destructive acceptance: use ordinary and megahealth boxes to check save and level carry. Reload afterward."),
        open("Spawn Pickup Weapons", pageIndex(pageSpawnWeapons))
            .help("Spawn a physical pickup ahead of you, ready to grab and use."),
        header("Physics Stress"),
        slider("Pile Size", vr_test_pile_count, 50.f, 1000.f, 50.f, "%.0f props")
            .extend(10.f, 2000.f)
            .help("How many props Pile of Rocks, Pile of Bricks and Mixed Pile put ahead of you. 500: about 350 bodies "
                  "awake as they fall (Physics on Threads at work: about twice as fast), one frame of ~9 ms as they "
                  "appear; 1000: a 20+ ms frame as they appear, then 1-2 ms steps on threads."),
        slider("Crates in the Wall", vr_test_pile_crates, 8.f, 120.f, 8.f, "%.0f crates")
            .extend(1.f, 400.f)
            .help("How many small crates Wall of Crates stacks ahead of you (walls 8 wide and 5 high, one behind another; "
                  "80: two walls). Standing walls cost little (under 0.1 ms a step); knocked over, a wall wakes its 40."),
        command("Pile of Rocks", "vr_physics_bigpile rocks")
            .help("vr_physics_bigpile rocks: Pile Size rocks 96 units ahead of you, in leaning columns that topple into "
                  "one pile (Physics on Threads at work while they fall). Clear the Piles takes them away."),
        command("Pile of Bricks", "vr_physics_bigpile bricks").help("vr_physics_bigpile bricks: the same, of bricks."),
        command("Wall of Crates", "vr_physics_bigpile crates")
            .help("vr_physics_bigpile crates: Crates in the Wall small crates stacked into walls facing you: push, "
                  "shoot or blow them up."),
        command("Mixed Pile", "vr_physics_bigpile mixed")
            .help("vr_physics_bigpile mixed: Pile Size props, rocks and bricks, with every fourth column small crates."),
        command("Clear the Piles", "vr_physics_clearpiles")
            .help("vr_physics_clearpiles: every prop these put there taken away, and every broken crate's pieces."),
        command("Physics Step Time", "vr_physics_steptime")
            .help("vr_physics_steptime: Box3D's step time a frame since the last time (average, median, 95th and 99th "
                  "percentiles, worst, ms), the awake bodies and its threads. Run it, make a pile, play, run it again "
                  "(Profiling and Memory: Physics Threads, Physics Step Time by Awake Bodies)."),
        command("Explosion Debris Ahead", "vr_explosion_debris_test")
            .help("vr_explosion_debris_test: an explosion's look and its chunks (the server's Box3D props) 64 units ahead, "
                  "no blast. Then Explosion Debris List."),
        command("Explosion Debris List", "vr_explosion_debris_list")
            .help("vr_explosion_debris_list: each chunk: its entity, place, speed, resting (asleep) or moving and on what "
                  "(the world, a lift), age, time left, fade. vr_explosion_debris_stats the counts; "
                  "vr_explosion_debris_launch <x y z> <vx vy vz> [size] [life] one chunk where you say."),
        header("Spatial Audio"),
        command("Spatial Audio Tests", "vr_snd_test all")
            .help("vr_snd_test: offline renders through the spatial audio (a sound circling the head, behind a wall, a door "
                  "moving, a small room and a hall, a rocket passing, a sound at the ear, the hands' sounds), measured, a "
                  "line each and PASS or FAIL; and the cost of 32 voices. The renders go to the game folder's sound_tests (test_*.wav)."),
        command("Spatial Audio Info", "vr_snd_info")
            .help("vr_snd_info: Steam Audio loaded or not, the voices, the scene (triangles, doors and lifts), the "
                  "simulations' times, where the listener is."),
        command("32 Sounds Around You", "vr_snd_bench_spawn 32")
            .help("vr_snd_bench_spawn: 32 looping sounds in a ring round you (till the map changes): a load for the "
                  "profiler's sound line, with Spatial Audio on and off."),
        command("Spatial Audio Benchmark (12 s)", "vr_snd_bench 12 menu 8 400")
            .help("vr_snd_bench <seconds> [label] [sounds a second] [orbit units/s]: for 12 s, monsters', weapons' and "
                  "explosions' sounds round you (8 a second) while the listener goes round a circle (every voice moving "
                  "against it); then each stage of the mix's time a frame (median, 95th and 99th percentiles, worst, ms per "
                  "second of sound), the simulations' runs and the sounds' memory, in the console and sound_tests/bench.csv. "
                  "32 Sounds Around You first for more voices (ROUND21.md, \"Spatial audio: optimised\")."),
        command("Record the Mix (2 s)", "vr_snd_capture 2 menu")
            .help("vr_snd_capture: the next 2 seconds of the final mix to the game folder's sound_tests (capture_menu.wav), and its levels "
                  "in the console."),
        command("Record the Game-Time Mix (2 s)", "vr_snd_capture_game 2 menu")
            .help("vr_snd_capture_game: the next 2 game seconds of the effects' mix as at normal speed (in slow motion: "
                  "what the slowed sound sped up is; no music) to sound_tests (capture_game_menu.wav), and its levels."),
        command("Five Explosions at Once", "vr_snd_burst weapons/r_exp3.wav 5")
            .help("vr_snd_burst: five rocket explosions at once 2.5 m ahead of you (as explosive boxes blowing up together): "
                  "with the limiter on (Sound, Advanced: Mix Limiter) a loud bang, off a crackle; Record the Mix and "
                  "Spatial Audio Info tell how far over full scale it went."),
        command("A Sound 45 Degrees Right", "vr_snd_play_dir misc/r_tele1.wav 45")
            .help("vr_snd_play_dir <sample> <azimuth> [elevation] [metres]: a sound from that direction of your head, 2 m "
                  "away (here the teleport's, 45 degrees right). With Record the Mix: its left/right balance (ROUND21.md, "
                  "HRTF balance)."),
        command("A Sound 45 Degrees Left", "vr_snd_play_dir misc/r_tele1.wav -45")
            .help("vr_snd_play_dir: the same sound 45 degrees left: it should sound as far to the left as the other did to "
                  "the right."),
        command("A 22 kHz Sound Behind You", "vr_snd_play_dir vr/torch_out.wav 180")
            .help("vr_snd_play_dir: the torch going out (a 22 kHz sound, with highs) behind your head. With Sound > "
                  "Full-Band Sound on it should sound behind, and duller than ahead; off (Quake's 11 kHz low-pass) it "
                  "loses the highs that tell (ROUND21.md, \"Full-band sound\")."),
        command("A 22 kHz Sound Ahead", "vr_snd_play_dir vr/torch_out.wav 0")
            .help("vr_snd_play_dir: the same sound straight ahead, to compare."),
        command("Save a Sound's Two Copies", "vr_snd_dump weapons/r_exp3.wav")
            .help("vr_snd_dump <sample>: a loaded sound as Quake holds it (each sample repeated up to the mix's rate) and "
                  "its band-limited copy (what Full-Band Sound plays), to sound_tests (dump_*_held.wav, _full.wav)."),
        command("Save the Sound Scene", "vr_snd_scene_obj")
            .help("vr_snd_scene_obj: the map as Steam Audio sees it, to the game folder's sound_tests (scene.obj)."),
        header("Ahead of You"),
        cycle("Thing", vr_test_spawn,
            {{0.f, "Grunt"}, {1.f, "Ogre"}, {2.f, "Zombie"}, {3.f, "Shambler"}, {4.f, "Scrag"}, {5.f, "Knight"},
             {6.f, "Hell Knight"}, {7.f, "Dog"}, {8.f, "Enforcer"}, {9.f, "Fiend"}, {10.f, "Vore"}, {11.f, "Spawn"},
             {12.f, "Gremlin"}, {13.f, "Centroid"}, {14.f, "Mummy"}, {15.f, "Phantom Swordsman"}, {16.f, "Wrath"},
             {17.f, "Overlord"}, {18.f, "Guardian"}, {19.f, "Dragon"}, {20.f, "Marksman Ogre"},
             {30.f, "Infected Grunt"}, {31.f, "Infected Knight"}, {32.f, "Infected Enforcer"}, {33.f, "Infected Death Knight"},
             {34.f, "Rocket Ogre"}, {35.f, "Demo Dog"}, {36.f, "Ranged Knight"}, {40.f, "Orb"}, {41.f, "Slime"}, {42.f, "Ghost"}, {43.f, "Sacrifice"}, {44.f, "Lava Man (Dawn of the Machine)"}, {45.f, "Super Shambler"},
             {60.f, "Shub-Niggurath (Free)"}, {61.f, "Shub Zombie"}, {62.f, "Shub's Eye"}, {63.f, "Shub's Seeker"},
             {50.f, "Chthon (Dawn of the Machine)"},
             {100.f, "Health Box"}, {101.f, "Shells Box"}, {102.f, "Explosive Box"},
             {103.f, "Small Explosive Box"}, {104.f, "Explosive Box (Never Blows Up)"}, {105.f, "Ogre's Head"},
             {106.f, "Gib"}, {107.f, "Small Crate"}, {108.f, "Large Crate"}, {109.f, "Two Crates Stacked"},
             {110.f, "Rocks and Bricks"}, {111.f, "Barrel"}, {112.f, "Barrel Lying"}, {113.f, "Silver Key"}})
            .help("What Put It There puts ahead of you, facing you (or as Facing says). The mission packs' monsters need their game installed; Dawn "
                  "of the Machine's (its infected, which burst into zombies and fiends; its own monsters, the orb, the sacrifice: MG3's data, read in place)."),
        slider("Distance", vr_test_spawn_dist, 32.f, 256.f, 8.f, "%.0f units").extend().help("How far ahead."),
        cycle("Facing", vr_test_spawn_facing, {{0.f, "As Each Spawner"}, {1.f, "Towards You"}, {2.f, "Away"}, {3.f, "Its Own Angle"}})
            .help("Which way monsters put ahead of you face: Put It There, impulse 244 (a grunt ahead) and vr_physics_spawn "
                  "(Spawn a Crowd). As Each Spawner: the first two face you, vr_physics_spawn keeps the map's angle. Away: "
                  "they look where you look (trailer shots from behind them)."),
        toggle("Into the Main Hand", vr_test_spawn_hold)
            .help("A box or a crate (Health Box .. Explosive Box, the crates) put into your empty main hand, as if gripped: "
                  "to test held props (the blood on what you hold: vr_gore_spatter_test, vr_gore_hands_info)."),
        cycle("As a Corpse", vr_test_spawn_dead, {{0.f, "Off"}, {1.f, "Corpse"}, {2.f, "Gibbed"}, {3.f, "Ragdoll"}})
            .help("A monster killed at once: a corpse, to test gibbing and carrying; Gibbed: killed hard enough to gib (its "
                  "gibs and head to pick up); Ragdoll: a corpse with ragdolls on (vr_ragdoll 1: a grunt, knight, ogre, enforcer, death knight, rottweiler, scrag, fiend, shambler or gremlin goes limp as he "
                  "falls)."),
        slider("Box Turned", vr_test_spawn_yaw, 0.f, 90.f, 1.f, "%.0f degrees")
            .extend()
            .help("A box (Health Box .. Explosive Box): let loose and turned this far about its upright, to test the "
                  "grappling hook against its real shape (its turned box's empty corners let the hook by)."),
        slider("Box Tilted", vr_test_spawn_tilt, 0.f, 90.f, 1.f, "%.0f degrees")
            .extend()
            .help("A box: tipped this far about the way you face, on its lowest corner (it topples: sv_gravity 0 keeps it so)."),
        command("Put It There", "impulse 241").help("Puts the Thing ahead of you."),
        command("Enemy Weapon Drop Cap", "developer 1; vr_dropcap_test 1")
            .help("vr_dropcap_test 1: 60 grunts spawned ahead and killed one by one (vr_dropcap_test_n), the first one's "
                  "rifle taken into an empty hand, a chainsaw thrown up and three other weapons dropped: the burst rifles "
                  "and chainsaws lying about never pass Most Lying About (vr_enemy_weapon_drop_max), the held one, the "
                  "flying one and the others stay (dctest: lines, PASS or FAIL). vr_dropcap_test 2: the count lying about."),
        command("Marksman Ogre: What It Is", "developer 1; vr_marksman_test 1")
            .help("vr_marksman_test 1: the nearest marksman ogre's model (Honey's in a Honey map, else id's ogre: Dimension "
                  "of the Machine's marksman), health, enemy and the grenades it has thrown, to the console (mkstest:)."),
        command("Marksman Ogre: Kill It", "developer 1; vr_marksman_test 2; wait; wait; vr_marksman_test 3")
            .help("vr_marksman_test 2, then 3: the nearest marksman ogre killed (not gibbed), then its body (ragdoll or not)."),
        command("A Knight's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 5; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a knight killed at the Distance ahead: he goes limp as "
                  "he falls (his sword dropped)."),
        command("An Ogre's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 1; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and an ogre killed at the Distance ahead: he goes limp as he falls (his chainsaw dropped)."),
        command("An Enforcer's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 8; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and an enforcer killed at the Distance ahead: he goes limp as he falls (his laser rifle dropped)."),
        command("A Death Knight's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 6; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a death knight killed at the Distance ahead: he goes limp as he falls (his sword dropped)."),
        command("A Rottweiler's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 7; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a rottweiler killed at the Distance ahead: it goes limp as it falls."),
        command("A Scrag's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 4; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a scrag killed at the Distance ahead: he falls limp."),
        command("A Fiend's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 9; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a fiend killed at the Distance ahead: it goes limp as it falls."),
        command("A Shambler's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 3; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a shambler killed at the Distance ahead: he goes limp as he falls."),
        command("A Gremlin's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 12; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a gremlin killed at the Distance ahead (Scourge of Armagon): it goes limp as it falls (a stolen gun dropped)."),
        command("A Vore's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 10; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a vore killed at the Distance ahead: it goes limp as it falls."),
        command("A Centroid's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 13; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a centroid killed at the Distance ahead (Scourge of Armagon): it goes limp as it falls."),
        command("A Grunt's Ragdoll There", "vr_ragdoll 1; vr_test_spawn 0; vr_test_spawn_dead 1; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdoll Settings) and a grunt killed at the Distance ahead: he goes limp as "
                  "he falls."),
        command("Go to a Crowbar on a Crate", "vr_crates_goto crowbar")
            .help("vr_crates_goto crowbar: you in front of the next crate with a crowbar lying on it (Crates: Crowbar on Crates)."),
        command("Go to the Next Crate", "vr_crates_goto")
            .help("vr_crates_goto [n]: you in front of the next of the crates placed in this map (or crate n), to look at it."),
        command("Crate Cover", "impulse 223")
            .help("impulse 223: a large crate just ahead to crouch behind, a small one at your right to hold up, and a "
                  "grunt beyond, facing you and not yet awake. Crouched behind the crate it shouldn't see you (it stays "
                  "asleep); standing up, or holding the small crate up, it should (Crates Hide You)."),
        command("Can the Grunt See You?", "impulse 224")
            .help("impulse 224: whether Crate Cover's grunt sees you now, how high your head is, and whether it woke."),
        command("Crouch Shots", "developer 1; vr_crouch_test 12")
            .help("vr_crouch_test 12: a grunt put ahead of you fires 12 bullets at you as it aims them, then goes: whether "
                  "you are crouched (Player Hitbox > Crouching), your eyes' height, the aim's height over your feet and "
                  "the damage you took (crouchtest: line; God mode off). Try it crouched behind the 32-high cover in the "
                  "teleporter test map's crouching room: no damage."),
        command("Crouch Status", "vr_crouch_status")
            .help("Prints your eyes' height over your feet, your crouched box's height (0 standing) and whether you could "
                  "stand."),
        header("Parry"),
        command("Check the Parry Pose", "impulse 249")
            .help("Developer 1: whether each held weapon blocks a blow from ahead, and its angle and position."),
        command("Dragon Parry Sequence", "vr_physics_spawn VR_Parry_DragonTest 120")
            .help("Developer 1: a real dragon tail hit and two same-frame follow-ups, then its route recovery. Needs Dissolution of Eternity; grants 500 health. Hold a guard to test the parry."),
        command("Same-Frame Parry Hits", "vr_physics_spawn VR_Parry_SameCallbackTest 48")
            .help("Developer 1: a knight deals three 10-damage blows in one callback and checks the restored self and vectors. Grants 500 health. A successful parry should cancel the last two."),
        command("A Melee Blow Now", "impulse 242")
            .help("The nearest melee monster within 150 units strikes for 10 damage through the actual parry test. With notarget, it attacks only when asked. Parry Stops Attacks also cancels blows asked for during its stagger."),
        header("Holding Enemies"),
        command("Who Is Held?", "vr_foegrab_status")
            .help("vr_foegrab_status: each hand holding an enemy, its hold, how firmly the enemy is held in all, how far "
                  "the hand is from the spot it holds (Combat > Holding Enemies)."),
        command("Walk the Nearest Away", "vr_foegrab_walk_test 110 1")
            .help("vr_foegrab_walk_test 110 1: the live monster nearest you walks straight away from you at 110 units/s "
                  "for a second; how far it got is printed. Held, it should hardly move (A Grunt Ahead first)."),
        cycle("Log Holds", "vr_foegrab_debug", {{0.f, "Off"}, {1.f, "Taken and Let Go"}, {2.f, "Every Frame"}})
            .help("vr_foegrab_debug: the console logs each hold taken and let go and why (and a grip that found none), "
                  "each two-hand throw tried and how it went, and each two-hand hold's hardest turn; Every Frame: each "
                  "held enemy's movement, each hand's stretch, both hands' turn."),
        command("Hurt the Held to 30%", "vr_foegrab_hurt 0.3")
            .help("vr_foegrab_hurt 0.3: the enemy you hold (else the one nearest you) is left with 30% of its full health: "
                  "a death knight, an ogre or a fiend can then be thrown (Combat > Holding Enemies, Hurt Below)."),
        command("Heal the Held", "vr_foegrab_hurt 1")
            .help("vr_foegrab_hurt 1: the enemy you hold (else the one nearest you) at its full health again."),
        command("Throw the Nearest Left", "vr_foegrab_throw_test 0")
            .help("vr_foegrab_throw_test 0: the monster nearest you thrown to your left as both hands' turn would (its kind "
                  "and health decide; no hands needed). The console traces its fall: its torso's lean, its feet, its head "
                  "(Combat > Holding Enemies > Topple)."),
        command("Throw the Nearest Right", "vr_foegrab_throw_test 1")
            .help("vr_foegrab_throw_test 1: the same, to your right."),
        command("Throw the Nearest at You", "vr_foegrab_throw_test 2")
            .help("vr_foegrab_throw_test 2: the same, towards you."),
        header("Enemy Shoves"),
        command("Shove the Nearest Monster", "impulse 219")
            .help("impulse 219: the nearest monster within 200 units shoved as your two-handed shove does (knocked away, "
                  "staggered). Developer 1 logs grunts' and enforcers' shoves and why one can't shove (Combat > Enemy Shoves)."),
        command("Knock Down the Nearest", "vr_knockdown_test 0")
            .help("vr_knockdown_test 0: the nearest monster that can be knocked down is, pushed away from you, whatever "
                  "its chance (A Grunt Ahead first: Debug > Tests)."),
        command("Shove the Nearest Down, One Hand", "vr_knockdown_chance 100; vr_knockdown_test 21")
            .help("Sets Knockdowns' Chance to 100 (every shove; set it back after), then vr_knockdown_test 21: the nearest "
                  "monster shoved as your one-handed open-palm shove does, knocked down and toppled over its feet "
                  "(Knockdowns' Topple). Print Rolls on: its fall printed every 0.1 s (\"shove trace\")."),
        command("Shove the Nearest Down, Two Hands", "vr_knockdown_chance 100; vr_knockdown_test 22")
            .help("The same with a two-handed shove (vr_knockdown_test 22): it topples faster."),
        command("Get Them Up Now", "vr_knockdown_test 1")
            .help("vr_knockdown_test 1: every knocked-down monster tries to get up now. Combat > Knockdowns, Print Rolls: "
                  "And Get-Ups' Motion prints how smoothly each is drawn getting up."),
        command("Remove Every Monster", "vr_knockdown_test 20")
            .help("Every monster removed, standing, knocked down or dead: a clean slate between shove tests (with a grunt "
                  "ahead: impulse 244). Shove one off vrclimb's long ledge or into its trench to see Over a Ledge, Always."),
        command("A Blast in 3 Seconds", "impulse 221")
            .help("impulse 221: an explosion of 60 at your feet 3 s from now, a little ahead of you (towards a wall you hang "
                  "from): hanging from a ledge or a rung, a blast of Climbing: Blasts Knock You Off or more makes you let go "
                  "and throws you. Developer 1 (and vr_climb_debug 1) logs it."),
        header("Chainsaw"),
        command("A Chainsaw in Your Hand", "impulse 164").help("A full ogre's chainsaw in the main hand (impulse 184: the off "
                                                                "hand). Take its cord with the other hand and pull."),
        command("Take the Nearest Chainsaw", "impulse 229").help("The chainsaw lying nearest you (an ogre's, dropped) into an "
                                                                 "empty hand, its fuel as it was."),
        command("Start the Engine", "impulse 230")
            .help("The chainsaws in your hands started, as a good pull of the cord does."),
        command("Drop the Chainsaws", "impulse 220")
            .help("impulse 220: the chainsaws in your hands dropped, as letting go does: one running runs on as it lies "
                  "(Runs On When Let Go), smoking and shaking about (Combat > Enemy Weapons > Chainsaw Engine)."),
        command("Nearly Empty Tank", "impulse 227").help("The chainsaws in your hands left with 5% fuel: to see one stall."),
        command("Report the Chainsaws", "impulse 228").help("Prints each chainsaw in your hands: its fuel, engine, chain and cord."),
        command("Chainsaw Fit", "vr_chainsaw_fit")
            .help("vr_chainsaw_fit: with the chainsaw in the main hand, prints where the off hand must move to take its "
                  "cord and each of its hotspots (two, on the front handle), and how many it has."),
        header("Weapon Instances"),
        command("List Your Weapons", "vr_test_weaponinst 0; impulse 120")
            .help("Prints what has which weapon: your hands, your holsters, the weapons lying near you, each with its id "
                  "and magazine (each weapon is one record that goes where it goes: its blood with it), and how many "
                  "records there are."),
        cycle("Holster", vr_test_weaponinst_slot,
            {{0.f, "Right Hip"}, {5.f, "Left Hip"}, {4.f, "Right Shoulder"}, {3.f, "Left Shoulder"}, {9.f, "Right Upper"},
             {8.f, "Left Upper"}})
            .help("The holster the two commands below use."),
        command("Holster the Main Hand's", "vr_test_weaponinst 1; impulse 120").help("Into that holster, as letting go there does."),
        command("Draw into the Main Hand", "vr_test_weaponinst 2; impulse 120").help("From that holster, as gripping there does."),
        command("Drop the Main Hand's", "vr_test_weaponinst 3; impulse 120").help("As letting go does (a throw at the hand's speed)."),
        command("Take the Nearest Weapon", "vr_test_weaponinst 4; impulse 120").help("The weapon lying nearest you into the main hand."),
        command("Hand Off to the Off Hand", "vr_test_weaponinst 5; impulse 120")
            .help("The main hand's weapon carried by the off hand, as letting go of a two-handed weapon does."),
        command("Take It Back", "vr_test_weaponinst 6; impulse 120").help("The main hand takes the carried weapon's handle."),
        command("Switch Hands", "vr_test_weaponinst 7; impulse 120").help("The main hand's weapon into the off hand (8: back)."),
        header("Reloading"),
        command("Shotgun in the Off Hand", "impulse 154; wait; vr_test_weaponinst 7; impulse 120; give s 40")
            .help("A loaded shotgun into the off hand and 40 shells: the main hand is free for the ammo pouch."),
        command("Nailgun in the Off Hand", "impulse 156; wait; vr_test_weaponinst 7; impulse 120; give n 100")
            .help("A loaded nailgun into the off hand and 100 nails (impulse 157: the super nailgun, 161 the thunderbolt)."),
        command("Rocket Launcher in the Off Hand", "impulse 160; wait; vr_test_weaponinst 7; impulse 120; give r 40")
            .help("A loaded rocket launcher into the off hand and 40 rockets (impulse 158: the grenade launcher, 159 the "
                  "proximity launcher): empty it, then load it at the muzzle."),
        command("Eject the Off Hand's Magazine", "vr_reload_test 6; impulse 125").help("As its B/Y does."),
        command("Empty the Off Hand's Gun", "vr_reload_test 5; impulse 125")
            .help("Its magazine back into your ammo (to load it again)."),
        command("Take a Shell (Main Hand)", "vr_reload_test 1; impulse 125")
            .help("As gripping at the ammo pouch does: a shell (or a taped pair) for the off hand's gun."),
        command("Load the Held Shell", "vr_reload_test 2; impulse 125")
            .help("The main hand's shell into the off hand's gun, as at its port."),
        command("Drop the Held Shell", "vr_reload_test 3; impulse 125").help("Let go of, as anywhere but the pouch."),
        command("Put It Back", "vr_reload_test 4; impulse 125").help("As letting go at the pouch: refunded."),
        command("Report", "vr_reload_test 0; impulse 125")
            .help("Prints your shells, the off hand's magazine, what each hand holds, the shells lying about."),
        command("Run the Self-Test", "vr_reload_test 9; impulse 125")
            .help("Takes, loads, refunds and drops in turn and checks every count: reload: PASS or FAIL lines."),
        command("Toss a Round Into the Off Hand's Gun", "vr_reload_test 10; impulse 125")
            .help("A loose round for it (a shell, the super shotgun's pair, its magazine), lying right, thrown into its "
                  "opening from 8 units out: it goes in by contact (Load Loose Rounds)."),
        command("Lay a Round Sideways at Its Opening", "vr_reload_test 11; impulse 125")
            .help("The same lying sideways right at the opening: it must not go in."),
        command("Lay a Round Under Its Load Point", "vr_reload_test 12; impulse 125")
            .help("On the floor (a shell flat along the gun, a magazine upright): bring the gun down onto it."),
        command("Drop a Round From Above Its Load Point", "vr_reload_test 13; impulse 125")
            .help("From 8 units straight above: turn the gun's opening up first."),
        command("Toss a Round in the Wrong Way Round", "vr_reload_test 16; impulse 125")
            .help("A launcher's round thrown into the off hand's muzzle nose first: it must not go in (a proximity "
                  "grenade does)."),
        command("Hold the Round at the Load Point", "vr_reload_test 17; impulse 125")
            .help("The main hand's round put at the off hand's gun's load point lying the way it goes in (a launcher's: "
                  "butt first): it goes in."),
        command("Hold It There the Wrong Way Round", "vr_reload_test 18; impulse 125")
            .help("The same nose first: a dull tap, it stays in the hand (a proximity grenade goes in)."),
        command("Break the Off Hand's Super Shotgun Open", "vr_reload_test 15; impulse 125")
            .help("As the flick does (Immersive, Break Open on): its barrels take loose pairs."),
        command("Spent Lava Nails or Plasma in the Off Hand's Gun", "vr_reload_test 16; impulse 125")
            .help("The off hand's nailgun, super nailgun or thunderbolt on its other ammo (lava nails, plasma: 100 given), "
                  "its magazine emptied as if fired dry: eject it to see a spent one smoke."),
        command("Report the Loose Rounds", "vr_reload_test 14; impulse 125")
            .help("Each loose round's distance from the off hand's load point, how it lies against the way in, and "
                  "whether it passes through the gun; the load point's axis and the way its opening faces."),
        command("Put the Off Hand's Gun Down", "vr_reload_test 20; impulse 125")
            .help("The off hand's gun let go of: it lies as a prop, its rounds with it. Guns lying about load as held "
                  "ones do, by hand and by contact."),
        command("Toss a Round Into the Lying Gun", "vr_reload_test 21; impulse 125")
            .help("A loose round for the nearest gun lying about, tossed into its opening lying right: it goes in, seen "
                  "sliding in (a shell, a pair, a launcher's round)."),
        command("Drop a Round Onto the Lying Gun", "vr_reload_test 26; impulse 125")
            .help("The same let go of 6 units above its load point, lying right: it falls in where the opening is up "
                  "enough, seen sliding in."),
        command("Toss One Sideways at the Lying Gun", "vr_reload_test 24; impulse 125")
            .help("The same lying sideways a unit out of its opening: it stays out."),
        command("Hold a Round at the Lying Gun", "vr_reload_test 22; impulse 125")
            .help("The main hand takes a round from the pouch (hold the grip), then, again, it is put at the nearest lying "
                  "gun's load point: it goes in."),
        command("Report the Lying Guns", "vr_reload_test 23; impulse 125")
            .help("Each gun lying about: its magazine, whether its magazine is in, whether it is open, its load point."),
        command("The Lying Gun's Magazine Is Solid", "vr_reload_test 27; impulse 125")
            .help("The nearest gun lying about with a magazine in: how far its body is from a point inside its magazine "
                  "(0: the magazine is part of it, Solid Magazines; off, the gun's own body further off)."),
        command("The Held Prop's Shape Against Its Box", "vr_reload_test 25; impulse 125")
            .help("The main hand's prop: how far its shape (as held) and its box are from the off hand's gun's magazine "
                  "and the front of its barrels. The magazine's bump and the super shotgun's hits count by its shape."),
        command("Print the Collision Shapes", "vr_physics_shapes vr_ammo_shell vr_ammo_mag vr_ammo_front thrown_weapon")
            .help("Each loose round's body against its drawn size, each held gun's body against the drawn gun, how deep "
                  "its load point lies inside it, and its convex pieces (how many, how far their hulls lie off the gun; "
                  "one hull's for comparison); the guns lying about too (the console)."),
        cycle("Reload Prints", "vr_reload_debug", {{0.f, "Off"}, {1.f, "Events"}, {2.f, "Every Frame"}, {3.f, "And Magazine Grips"}})
            .help("Events: each take, load, refund, loss, magazine out and hold. Every Frame: a held round's distance to the "
                  "port, a held magazine's pull, snap and apart, a hit's speed. And Magazine Grips: each empty hand's "
                  "distance off the other gun's magazine (its box) and the grip it would take."),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Each held gun's load point and radius, a held magazine's top, an attached magazine's box (blue)."),
        toggle("Show the Pouches' Reach", "vr_show_grenade_pouch").help("Spheres where the grenade pouch and the ammo pouch are reached."),
        header("Hubs"),
        command("The Old Hub (vrstart_old)", "vr_campaign_hub vrstart_old")
            .help("vr_campaign_hub vrstart_old: the hub before the island (vrstart until 2026-10-07). Nothing goes there by "
                  "default; vr_hub_map vrstart_old makes it the hub again."),
        header("Tutorial (vrtutorial2)"),
        command("The Tutorial", "skill 0; map vrtutorial2")
            .help("skill 0; map vrtutorial2: the tutorial (a military base by day, 12 lessons and an arena; "
                  "Misc/quakevr/maps/vrtutorial2_gen.py), on Easy as the start flow and the hub's button start it."),
        command("The Old Tutorial (vrtutorial)", "map vrtutorial").help("map vrtutorial: the tutorial before 2026-10-08."),
        command("First Start Again", "vr_tutorial_started 0")
            .help("vr_tutorial_started 0: the next start of the game goes to the tutorial, as a new install's first start "
                  "does (the tutorial sets it back to 1 as it loads)."),
        cycle("Go to a Lesson", "vr_tutorial_goto",
            {{0.f, "-"}, {1.f, "1 Moving"}, {2.f, "2 Buttons"}, {3.f, "3 Jumping"}, {4.f, "4 Swimming"}, {5.f, "5 Healing"},
             {6.f, "6 Melee"}, {7.f, "7 A Fight"}, {8.f, "8 Weapons"}, {9.f, "9 Darkness"}, {10.f, "10 Throwing"},
             {11.f, "11 Fire"}, {12.f, "12 The Arena"}})
            .help("In vrtutorial2: puts you at that lesson's start (its checkpoint, taken: you come back there). Doors "
                  "on the way stay as they are."),
        header("Trailer Scene (vrtrailer)"),
        command("The Trailer Scene", "map vrtrailer")
            .help("map vrtrailer: a bridge across a lake at night, the Super Axe on a pedestal half way (Dawn of the "
                  "Machine's data needed), a grunt on the far islet looking at the water, oblivious until hurt "
                  "(Misc/quakevr/maps/vrtrailer_gen.py). You start empty-handed; recording mode is on there: no tips, no "
                  "wrist messages, no head text."),
        command("Reset Trailer Scene", "vr_trailer_reset 1")
            .help("vr_trailer_reset 1: for a retake: the grunt back where he stood (his body, head, gibs, gun and backpack "
                  "gone), the Super Axe back on its pedestal (out of your hands and holsters), you at the start facing "
                  "along the bridge."),
        toggle("Trailer Scene Log", "vr_trailer_log")
            .help("vr_trailer_log 1: the grunt's state (enemy, frame, think, stealth state, oblivious), the axe's place "
                  "and yours, four times a second (the console). Off for takes: the lines would show."),
        header("Climbing"),
        command("Climbing Test Map", "map vrclimb").help("map vrclimb: rungs, ledges, a jump wall, moving and floating ledges."),
        command("To the Jump Wall", "setpos -40 -310 24 0 0 0; noclip")
            .help("In vrclimb: you facing a wall 96 high, 40 units ahead: walk into it, jump and grab its top (Climbing: "
                  "Mid-Air Grab Window)."),
        header("Weapon Pickups (pickuptest: ... in the console)"),
        command("Every Weapon Pickup Ahead", "vr_pickup_test 1")
            .help("vr_pickup_test 1: every weapon pickup (the mission pack's when loaded) in rows ahead of you, each with "
                  "the same weapon dropped beside it, to compare them (Items: Weapon Pickups Look); their models, "
                  "offsets and boxes printed a second later."),
        command("Next Pair Before You", "vr_pickup_test 3")
            .help("vr_pickup_test 3: you moved before the next pickup and dropped weapon of the row above, for a close look."),
        command("Take the Nearest Weapon Pickup", "+grabmain; wait; wait; vr_pickup_test 2; wait; wait; -grabmain")
            .help("vr_pickup_test 2, the main grip held (+grabmain): the weapon pickup nearest you taken into an empty "
                  "hand, as a grip takes it: the hand's weapon, its magazine and the ammo left printed."),
        header("Crowbar"),
        command("A Crowbar in Your Hand", "impulse 167").help("A crowbar in the main hand (impulse 187: the off hand)."),
        command("Drop a Crowbar Ahead", "impulse 217")
            .help("A crowbar dropped 48 units ahead of you, as a crate or a map places one (QC VR_Crowbar_Spawn), then the "
                  "report below."),
        command("Take the Nearest Crowbar", "impulse 216").help("The crowbar lying nearest you into an empty hand."),
        command("Report the Crowbars", "impulse 218").help("Prints each crowbar lying about and whether your hands hold one."),
        command("Hotspot Fit", "vr_hotspot_fit")
            .help("vr_hotspot_fit: the weapon in the main hand (any): where the hand and the fist are on its model, its "
                  "tip's distance, and where the off hand must move to take each hotspot (a blade: its ends and middle)."),
        header("Enemy Guns"),
        command("A Grunt's Gun in Your Hand", "impulse 165")
            .help("A full grunt's burst rifle (Enemy Weapons: Rounds) in the main hand (impulse 185: the off hand)."),
        command("An Enforcer's Rifle in Your Hand", "impulse 166")
            .help("A full enforcer's laser rifle (Enemy Weapons: Shots) in the main hand (impulse 186: the off hand)."),
        command("Take the Nearest Enemy Gun", "impulse 212")
            .help("The grunt's gun or enforcer's rifle lying nearest you (dropped) into an empty hand, its ammo as it "
                  "was."),
        command("Report the Enemy Guns", "impulse 213").help("Prints each enemy gun in your hands and its ammo."),
        command("One Shot Left", "impulse 214").help("The enemy guns in your hands left with one shot: to see one run dry."),
        command("Weapon Effects Test", "vr_weaponfx_test 1 3")
            .help("vr_weaponfx_test 1 3: the main hand's weapon kicks and flashes as if it fired (its Effects), with 3 "
                  "tracers (no shot)."),
        toggle("Print Weapon Effects", vr_debug_weaponfx)
            .help("vr_debug_weaponfx: each shot's recoil, flash and tracers (2: and the recoil each frame); the shotgun's auto "
                  "pump strokes (start, back, home) and when its shell leaves."),
        slider("Hold the Auto Pump", vr_autopump_hold, -0.05f, 1.f, 0.05f, "%.2f")
            .help("vr_autopump_hold: every shotgun's fore-end held at that point of its auto pump's stroke, to look at it "
                  "(0.35-0.45: at the back; below 0: off)."),
        header("Flung Props"),
        slider("Fling Speed", vr_test_fling_speed, 1.f, 40.f, 1.f, "%.0f m/s").extend(),
        cycle("Fling At", vr_test_fling_at, {{0.f, "Nearest Monster"}, {1.f, "You"}}),
        toggle("Fling Away From It", vr_test_fling_away)
            .help("The prop starts inside the edge of its box and flies away from it (a prop batted away from you): it "
                  "must never hurt."),
        command("Two-Handed Throw", "developer 1; impulse 204")
            .help("A gib ahead of you as if just thrown with both hands: prints (2h test:) whether each hand may strike "
                  "and the gib can be struck, now and 0.5 s later (Throwing and Physics: Two-Handed Throws)."),
        command("Gib in the Off Hand", "developer 1; impulse 252")
            .help("A gib or head (each press the next kind), or the prop Which Gib picks, in your off hand: take it "
                  "with the other hand too and throw it with both, or let go with the first hand and grip it again. "
                  "'gib:' lines say what burst it (Real Gib, below)."),
        cycle("Which Gib", vr_test_held_pick,
              {{-1.f, "Each in Turn"}, {0.f, "Small Gib (gib1)"}, {1.f, "Player's Head"}, {2.f, "Torso (gib2)"},
               {3.f, "Big Chunk (gib3)"}, {4.f, "Grunt's Head"}, {5.f, "Ogre's Head"}, {6.f, "Knight's Head"},
               {7.f, "Zombie's Head"}, {8.f, "Fiend's Head"}, {9.f, "Nearest Rock"}, {10.f, "Hand Grenade"},
               {11.f, "Nearest Shells Box"}, {12.f, "Nearest Brick"}})
            .help("vr_test_held_pick: the gib or head Gib in the Off Hand gives (Each in Turn: the next each press). The "
                  "torso and the big chunk are the large ones (Misc/quakevr/gib_2h_models_test.sh throws each). Rock, "
                  "Shells Box, Brick: the nearest lying about; Hand Grenade: one from the pouch (Misc/quakevr/"
                  "twohand_regrip_test.sh takes each in both hands and hands it over)."),
        toggle("Into the Main Hand", vr_test_held_hand)
            .help("vr_test_held_hand: Gib in the Off Hand fills the main hand instead (hold its grip): a prop to hit the "
                  "super shotgun's barrels open or shut with, the gun in the off hand (reload_test.sh section 11)."),
        toggle("Real Gib", vr_test_held_destroy)
            .help("vr_test_held_destroy: the off hand's test gib can be burst as a real one (shot, struck, thrown hard "
                  "at a wall). Misc/quakevr/gib_2h_throw_test.sh throws it with both hands along several arcs."),
        command("Fling the Nearest Prop", "impulse 232")
            .help("Sends the loose prop nearest you (put a box there first) at it, as if batted or knocked flying; "
                  "developer 1 prints the hit (prop: flung ...)."),
        slider("Throw Up Speed", vr_test_throw_up_speed, 0.f, 25.f, 0.5f, "%.1f m/s")
            .help("0: let go of gently (dropped, not thrown): it must never hurt you."),
        toggle("Throw Up From Your Body", vr_test_throw_up_body)
            .help("Throw the Nearest Prop Up lets it go at your chest, inside your body, as a long heavy thing let go "
                  "of at the body (not from over your head)."),
        command("Throw the Nearest Prop Up", "developer 1; impulse 222")
            .help("The loose prop nearest you thrown straight up as yours from just over your head: it falls back on you "
                  "and hurts you (Your Throws Spare You For, on Throwing and Physics). Prints the throw and the hit."),
        header("Thrown Axe"),
        cycle("Axe Throw", vr_test_axe,
            {{0.f, "Blade First"}, {1.f, "Flat"}, {2.f, "Handle First"}, {3.f, "Spinning"}, {4.f, "Overhand"},
             {5.f, "Sidearm"}, {6.f, "Sloppy"}, {8.f, "Upright"}, {9.f, "Knife-Style"}, {10.f, "Spear-Like"},
             {11.f, "Flat (Hand)"}, {12.f, "Handle First (Hand)"}})
            .help("How Throw an Axe throws it: blade first (the edge upright), the blade's side first, the handle's end "
                  "first, blade first spinning end over end; or as a hand would, turned and spun a little differently "
                  "each time: overhand (end over end), sidearm (the blade level, spun about the upright), sloppy (any "
                  "way), upright (pushed, the handle upright, the blade ahead), knife-style (held by the head, the "
                  "handle ahead, flicked end over end), spear-like (the head ahead), flat (the blade's side ahead) and "
                  "handle first. The hand's throws aim up to meet what is ahead at eye level."),
        slider("Axe Speed", vr_test_axe_speed, 2.f, 20.f, 1.f, "%.0f m/s").extend(),
        cycle("Throw Instead", vr_test_axe_what, {{0.f, "The Axe"}, {1.f, "A Gib"}, {2.f, "An Explosive Box"}})
            .help("Throws a gib or an explosive box (one that never blows up) the same way instead of the axe: to "
                  "compare their spin in the air (Debug: Spin in the Air)."),
        cycle("Weapon Instead", vr_test_axe_weapon,
            {{0.f, "The Axe"}, {2.f, "Axe (as a throw)"}, {4.f, "Shotgun"}, {7.f, "Super Nailgun"}, {12.f, "Laser Cannon"}})
            .help("Throws that weapon instead of the axe, hurting as your hand's throws do (their damage by speed and "
                  "weight; the training dummy shows it): Axe Speed is its speed."),
        toggle("Axe Hurts", vr_test_axe_damage).help("Off: its blow does no damage (to watch a monster bleed)."),
        cycle("Axe At", vr_test_axe_at, {{0.f, "Ahead"}, {1.f, "Nearest Monster"}, {2.f, "Nearest Door"}, {3.f, "Nearest Prop"},
                                              {4.f, "Nearest Button"}})
            .help("What Throw an Axe throws at: ahead of you, or the nearest live monster, door, loose prop or wall "
                  "button (you are moved to face it, level with its middle; a button: square to its wall)."),
        slider("Axe Range", vr_test_axe_dist, 0.f, 400.f, 20.f, "%.0f units")
            .help("You are moved first to this far from what is ahead (0: where you are)."),
        slider("Axe Height", vr_test_axe_up, -30.f, 40.f, 1.f, "%+.0f units")
            .help("At a target: thrown this far above its middle (15: a grunt's head, to behead it: Gore > Decapitation)."),
        command("Throw an Axe", "impulse 209").help("An axe thrown straight ahead from your eyes (not yours: a new one)."),
        command("Hand on the Stuck Axe", "impulse 207")
            .help("Moves you so that your main hand is on the handle of the nearest stuck axe: grip to pull it out."),
        command("Report the Axes", "impulse 208").help("Prints each thrown axe: what it is stuck in, where (developer 1)."),
        command("Push the Props Axes Are In", "vr_test_axe_host 1; impulse 208")
            .help("Each prop an axe is stuck in (a box, a crate, a pickup) is pushed up and across: the axe moves with it "
                  "(Report the Axes again to see; vr_test_axe_host 1)."),
        command("Break the Props Axes Are In", "vr_test_axe_host 2; impulse 208")
            .help("Each prop an axe is stuck in is broken (an explosive box blows up, a crate breaks, a pickup is taken by "
                  "you): the axe falls (vr_test_axe_host 2)."),
        command("Slide the Nearest Prop", "vr_physics_fling nearest 150")
            .help("Sends the loose prop nearest you skidding along the floor the way you face, at 150 units/s (5.7 m/s): "
                  "its scrape (Physics Sounds; Logs: Physics Sounds prints it)."),
        header("At You"),
        cycle("Projectile", vr_test_projectile,
            {{0.f, "Knight's Spike"}, {1.f, "Enforcer's Laser"}, {2.f, "Scrag's Spit"}, {3.f, "Vore's Ball"},
             {4.f, "Ogre's Grenade"}, {5.f, "Zombie's Flesh"}})
            .help("What Fire at Me fires at your face from 300 units: to bat back with a weapon, or catch (a grenade)."),
        slider("From the Left", vr_test_projectile_side, -90.f, 90.f, 15.f, "%.0f deg")
            .help("Degrees to your left of ahead it comes from (negative: from the right)."),
        command("Fire at Me", "impulse 246").help("Fires the Projectile at you now."),
        command("Make an Ogre Throw", "impulse 240").help("The nearest ogre or zombie throws at you now."),
        cycle("Grenade Shot", vr_test_grenade_shot, {{10.f, "Pellet"}, {1.f, "Nail"}, {0.f, "Rocket"}, {4.f, "Laser"}})
            .help("What Shoot the Nearest Grenade fires from your eyes (Shoot Grenades, on Batting and Catching)."),
        command("Shoot the Nearest Grenade", "developer 1; impulse 210")
            .help("Fires the Grenade Shot at the nearest grenade (an ogre's in flight: Fire at Me with the Ogre's Grenade "
                  "first; yours; a dud). Prints what it shot, how far, its shot box (impulse 210)."),
        slider("Dud Distance", vr_test_grenade_dist, 64.f, 1024.f, 32.f, "%.0f units")
            .help("How far ahead Drop a Dud Ahead puts the grenade."),
        slider("Dud Height", vr_test_grenade_height, 0.f, 64.f, 2.f, "%.0f units")
            .help("How high over the floor Drop a Dud Ahead puts the grenade (it falls): at hand height to punch or swing at "
                  "it (Blow Speed to Set Off, on Batting and Catching)."),
        toggle("Dud Is Yours", vr_test_grenade_yours)
            .help("On: the dud is yours (your blows pass through your own grenades). Off: nobody's, as an enemy's grenade "
                  "lying about: your blows meet it (bat it, or set it off when very strong)."),
        command("Drop a Dud Ahead", "developer 1; impulse 211")
            .help("Takes a hand grenade from your pouch into your empty off hand, lets go of it unarmed (a dud), and puts it "
                  "on the floor ahead: a trap to shoot from afar (impulse 211)."),
        header("Getting Hit"),
        command("Hit Me From the Left", "vr_pain_test 15 90")
            .help("vr_pain_test 15 90: a 15 point hit from your left (no damage): the hands knocked right, the left one more; "
                  "the controllers buzz (When You're Hit, Damage and Knockback)."),
        command("Hit Me From the Right", "vr_pain_test 15 -90").help("vr_pain_test 15 -90: as above, from your right."),
        command("Hit Me From Ahead", "vr_pain_test 15 0").help("vr_pain_test 15 0: from ahead: both hands knocked back."),
        command("Rocket From Ahead", "vr_pain_test 80 0").help("vr_pain_test 80 0: a big hit: the Largest Knock."),
        command("Fall", "vr_pain_test 10 none").help("vr_pain_test 10 none: a hit from no direction (a fall, lava): the hands drop."),
        toggle("Print Hits", vr_debug_pain)
            .help("Prints each hit (painhit: damage, side, knock, buzz) and the hands' knock each frame (painknock, cm along "
                  "your body) (vr_debug_pain)."),
        header("Grappling Hook"),
        command("Report the Hooks", "impulse 239").help("Prints every hook: its state, what it is in, where its gun is, the "
                                                         "rope's length and path (needs Developer Messages)."),
        command("Print the Ropes", "vr_grapple_rope_dump; vr_grapple_rope_draw_dump")
            .help("Prints each rope's corners (the server's) and its drawn chain: points inside the world, pieces through it."),
        toggle("Load Stuck", vr_grapple_test_stuck)
            .help("What hangs on the rope (a loose hook, a hooked prop) stays where it is, as if snagged: walk away and the "
                  "rope should hold you back."),
        header("Stamina"),
        info([]() -> const char* { return staminaReadout(); }),
        command("Deplete Stamina", "vr_stamina_set 0").help("vr_stamina_set 0: no stamina left (tired arms: heavy hands; shaking while you hang). Hanging with none, your hands let go."),
        command("Nearly Empty", "vr_stamina_set 0.1").help("vr_stamina_set 0.1: a tenth of your stamina left, enough to hang a moment."),
        command("Half Stamina", "vr_stamina_set 0.5").help("vr_stamina_set 0.5: half your stamina left."),
        command("Quarter Stamina", "vr_stamina_set 0.25").help("vr_stamina_set 0.25: a quarter of your stamina left (the run a little slower: Slower When Tired)."),
        command("Restore Stamina", "vr_stamina_set 1").help("vr_stamina_set 1: rested."),
        toggle("Hold Stamina", vr_debug_stamina_hold).help("Keeps your stamina where it is, or where the buttons above put it: nothing spends it and it doesn't come back (vr_debug_stamina_hold)."),
        toggle("Print Run Speed", vr_debug_stamina_speed).help("Prints your stamina, the most speed it lets you run at and your speed on the ground, twice a second (vr_debug_stamina_speed)."),
        header("Stuck in Walls"),
        toggle("Unstick", vr_unstick)
            .help("Found inside a wall, a door, a button or a lift, you're moved to the nearest free spot (vr_unstick). "
                  "Off: only Quake's small nudge up."),
        command("Stuck Info", "vr_stuck_info")
            .help("Prints where you are, what you're inside of, the doors, buttons and lifts near you, and how often "
                  "you were freed."),
        header("Player Hitbox (Prototype)"),
        open("Player Hitbox Settings", pageIndex(pageHitbox)).help("Movement > Player Hitbox: the widths and their toggles."),
        open("Monster Hitbox Settings", pageIndex(pageMonsterHitbox)).help("Movement > Monster Hitbox: monsters' widths by class, and their walk tests."),
        command("Hitbox Stats", "vr_hull_stats").help("Prints the map's rebuilt brushes and compiled hull: counts, memory, build times."),
        cycle("Keep Hitboxes for Reloads", vr_hull_keep, {{0.f, "Off"}, {1.f, "1 Map"}, {2.f, "2 Maps"}, {4.f, "4 Maps"}})
            .help("The map's brushes and compiled hulls kept in memory when it is left, for a load of the same map again "
                  "(a death's reload, restart, a changelevel back): that load skips their build (vr_hull_keep). Off: "
                  "built at every load."),
        cycle("Hitboxes on Disk", vr_hull_cache, {{0.f, "Off"}, {1.f, "On"}, {2.f, "Check"}})
            .help("A big map's compiled hulls kept on disk (cache/hulls) and read at its next load instead of compiled "
                  "again (vrstart: 9 s to 0.1 s). Check: read, then compiled anyway and compared (Hitbox Stats counts "
                  "them). Off: compiled at every load (vr_hull_cache)."),
        command("Hitbox Keep Test", "vr_hull_keeptest")
            .help("Builds the map's brushes and compiled hulls again from scratch and prints whether the server's (kept "
                  "from the last load, or built with this one) are the same (vr_hull_keeptest)."),
        command("Hitbox Approach", "vr_hull_approach").help("Prints how close your box gets to what is round you, in 8 directions (from your centre to the surface it stops at; Quake's box: 16 units). vr_hull_approach <classname> [n] does it round an entity."),
        command("Hitbox Bench", "vr_hull_bench").help("Times 20000 random moves (Quake's hull against the brush sweep "
                                                      "and the compiled hull) and prints where they disagree."),
        command("Shots Hit Test", "vr_hull_hittest")
            .help("Prints how many of 1600 grunt-like shots from 300 units round you hit your box at Width Shots Hit "
                  "(vr_hull_hittest [distance] [spread])."),
        command("Hitbox Leaf", "vr_hull_leafdebug")
            .help("Compiles the hull for your box again and describes the leaf you stand in when it is solid (the piece "
                  "of brush that fills it, and that brush): for a wall where there is none (vr_hull_leafdebug <entity>: "
                  "a monster's, with Narrower Monsters on)."),
        command("Hitbox Probe", "vr_hull_probe")
            .help("Prints which of the map's brushes your box is in, and by how much (when you're stuck); vr_hull_probe <entity>: a monster's."),
        command("Shots Hit Test", "vr_hull_hittest")
            .help("Prints how many of 1600 grunt-like shots from 300 units round you hit your box at Width Shots Hit "
                  "(vr_hull_hittest [distance] [spread])."),
        command("Random Walk (60 s)", "god; notarget; vr_hull_walktest 60")
            .help("Walks you around the map at random for 60 seconds (hopping somewhere new every few), then prints how "
                  "often you got stuck or ended up in a wall."),
        header("Teleporters: Crossing One (the start map; developer 1: VR portal: ...)"),
        command("The Gates In This Map", "vr_portals_info")
            .help("vr_portals_info: every teleporter built for this map - its plane, its opening, its trigger brush - and "
                  "where your body is against each: your box, your torso's middle plane, its distance from the gate's "
                  "plane, whether it is over the opening and how near your box can bring it. You are carried through "
                  "only after that plane reaches the gate's and your collision box fits the opening; leaning or reaching "
                  "with the hands does not cross, and a gate's frame stops you as a wall does."),
        command("Looking Through A Gate, Or Why Not", "map start; wait120; setpos 232 400 24 0 90 0; wait30; vr_portals_view")
            .help("vr_portals_view: which gate this frame looks through, that gate's box on your screen, and for every "
                  "side in the map why it is not looked through - behind its plane, not in your view, further than the "
                  "range, seen through another gate - or that it is, with its distance and score. It shares the test "
                  "the engine acts on, so the reasons printed are the ones applied."),
        command("Torch Light Views", "vr_portals_lightviews")
            .help("vr_portals_lightviews [x y z]: the views whose torches and flames light this frame - your own, each "
                  "gate's in front of you and the gates seen within those, as deep as Gates Within Gates - each with its "
                  "eye carried through, and what making them costs. With a point: how far a torch there counts as "
                  "(none: no view sees it)."),
        command("The View Through A Gate, Read Back", "vr_portals_shot")
            .help("vr_portals_shot: the next view drawn through a gate read back from its own targets - its side, the "
                  "eye and the camera it was drawn from, its brightness over the whole view and over the gate's box - "
                  "to portalshots/<map>_<n>.png and its float scene to .pfm, to compare with the eye's own float scene "
                  "(vr_eyeshot 2: eyeshots/<map>_<n>_L.pfm) - in the same frame, the gate's face over the same pixels."),
        command("Against A Gate's Frame", "map start; wait120; setpos 200 1372 24 0 90 0; wait30; vr_portals_info")
            .help("You against the wall beside the first gate's opening (12 units short of its plane): nothing "
                  "teleports you and there is no jump - vr_portals_info says his box reaches 12, that is, stopped."),
        command("In The Opening, Short Of The Plane", "map start; wait120; setpos 232 1372 24 0 90 0; wait30; vr_portals_info")
            .help("You in the first gate's opening, 12 units in front of its plane: not carried yet (his box reaches "
                  "-1: nothing stops you, walk on and you go through)."),
        command("Lean Into A Gate", "map start; wait120; setpos 232 1365 24 0 90 0; wait30; vr_mock_hand head 0 0 -0.5 45 0 0; wait30; vr_portals_info")
            .help("You 19 units from the first gate's plane, the head leaned half a metre through it: no crossing - "
                  "vr_portals_info shows the head past the plane and the torso short of it."),
        command("Through A Gate", "map start; wait120; setpos 232 1330 24 0 90 0; wait10; noclip 0; wait80; vr_mock_stick off 0 0.5; wait20; +jump; wait30; -jump; vr_mock_stick off 0 0; wait30; vr_portals_info")
            .help("Mock movement with collision enabled: approach the first gate and jump into its opening. "
                  "The torso reaches y=1384 before crossing (developer 1: VR portal: carried edict 1 through side 0)."),
        command("Frame Strip Through A Gate", "map start; wait120; setpos 544 1330 24 0 90 0; wait10; noclip 0; wait80; vr_mock_stick off 0 0.5; wait20; +jump; wait36; -jump; vr_screenshot_frames 12; wait20; vr_mock_stick off 0 0")
            .help("The same jump into the middle gate (Normal skill), a screenshot of each of the 12 frames round the "
                  "crossing (vr_screenshot_frames 12: every frame drawn, not only those on a server tick). The room "
                  "beyond must look the same in each: the pentagram's floor over the pit (func_bossgate) was missing "
                  "in the first frame after the crossing until the server sent both rooms while you straddle a gate."),
        command("A Shot Through A Gate", "map start; wait120; setpos 232 1360 24 0 90 0; wait10; vr_physics_fire 10 232 1500 25")
            .help("vr_physics_fire 10: a pellet's trace at a point beyond the first gate: the console says through 1 "
                  "teleporter(s), in at ..., out at ... (shots and thrown props go through as before)."),
        command("The Whole Feature Off", "vr_teleporters 0; map start; wait120; setpos 232 1390 24 0 90 0; wait60; vr_portals_info; vr_teleporters 1")
            .help("vr_teleporters 0: the see-through teleporter feature off entirely, at once (no map reload) - no gate is built or "
                  "looked through, nothing is carried or traced through one. Placed inside the first gate's trigger, nothing "
                  "carries you, the trigger teleports you the old way (a flash, a jump, 0.7 s locked), and "
                  "vr_portals_info says the feature is off. It is turned back on at the end; Graphics > Teleporters has "
                  "the same switch."),
        header("Teleporters: Test Map (vrteleporters)"),
        command("Teleporter Test Map", "map vrteleporters")
            .help("map vrteleporters: teleporter pairs of every size (crate, player, shambler, very wide), flush with the floor "
                  "and in frames with sills, at 90 and 45 degrees, a loop, between floor heights and by a pool. Every gate "
                  "goes both ways; each room's buttons spawn a grunt, dog, ogre, shambler or scrag by its far wall."),
        command("To the Flush Gates", "setpos -256 576 24 0 90 0; noclip")
            .help("In vrteleporters: facing the player-sized flush gate (its bottom at the floor). The north gallery behind "
                  "it is where it leads, so a monster chasing you through walks straight into it."),
        command("To the Framed Gates", "setpos 1180 576 24 0 90 0; noclip")
            .help("In vrteleporters: facing the player-sized gate in a frame with a 16-unit sill (a step); the next one east "
                  "has a 32-unit sill (a jump: monsters can't)."),
        command("To the Turning Gates", "setpos -1280 640 24 0 90 0; noclip")
            .help("In vrteleporters: facing the gate that comes out of the next room's east wall (90 degrees); the loop is "
                  "left and right of you, the 45-degree wall behind you to the left."),
        command("Into the Loop", "setpos -1560 560 24 0 180 0; noclip")
            .help("In vrteleporters: 40 units from the loop's west gate, which comes out of the east one: you see your own "
                  "back (with its head) and, through the gate beyond it, yourself again, as many gates deep as Graphics > "
                  "Teleporters > Gates Within Gates (vr_portals_view prints the views drawn at each depth)."),
        command("To the Heights and Water", "setpos -400 -960 24 0 180 0; noclip")
            .help("In vrteleporters: facing the floor-level gate that comes out over the 128-high platform; the pool's two "
                  "gates are in the east and south walls."),
        command("Slide A Crate Through", "map vrteleporters; wait60; setpos -256 536 24 0 90 0; wait5; noclip 0; vr_test_spawn 107; vr_test_spawn_dist 40; impulse 241; wait30; vr_physics_fling nearest 250 90")
            .help("In vrteleporters: a small crate slid along the floor into the flush player gate (8 deep, a wall right "
                  "behind it): it goes through and comes out of the north gallery's gate (it stopped against the wall "
                  "behind the sheet before Box3D's level contacts were clipped at the gate)."),
        command("Quake's Effects Behind A Gate", "map start; wait90; god; notarget; noclip; setpos 1040 1650 -330 0 270 0; wait5; vr_particle_test quake; setpos 1040 1830 -330 0 270 0")
            .help("start's underwater gate: Quake's own explosion particles and sprite (vr_particle_test quake) made "
                  "behind it, then seen from in front: with a see-through gate surface (Graphics > Teleporters > Teleporter "
                  "Stars > Opacity under 1) they must not show over the view through the gate."),
        toggle("Print Gate Cuts", "vr_portals_debug_split")
            .help("vr_portals_debug_split 1: each frame, every entity drawn cut by a teleporter (where, the plane, how far "
                  "through), the force grab's beam end, and each thrown or rigid thing's middle and why a gate did not "
                  "take it. -1 (console) also follows the main hand's held object, a number that entity, every frame."),
        command("Into the Loop, Lightning Gun", "setpos -1580 560 24 0 180 0; noclip; impulse 161")
            .help("In vrteleporters: right at the loop's west teleporter with the lightning gun in your main hand. Shoot your "
                  "own back through it: shots, nails, rockets and the bolt come out of the east teleporter and hit you "
                  "(the bolt's 600 units reach round this room only from close to the teleporter; god mode: no damage)."),
        command("Stuck in a Teleporter's Wall?", "vr_portals_stuck")
            .help("vr_portals_stuck: whether your body is in the wall where you stand (a teleporter's split body "
                  "included), the teleporter side nearest your torso and how far in front of its plane it is (under 0: "
                  "behind it), and how many times you were got out of the wall behind a teleporter."),
        toggle("Get Out of a Teleporter's Wall", "vr_portals_unstick")
            .help("vr_portals_unstick 1 (default): if you end up in the wall behind a teleporter, you are carried on "
                  "through it (your torso past its plane and room at the far side) or put back in front of it at once. "
                  "0: off, to see a softlock as it was."),
        header("Visibility: Hidden Staircase"),
        command("Hidden Staircase Probe", "map start; wait120; setpos 278 1728 24 7 -20 0; wait60; vr_hull_leafdebug")
            .help("Places the player at the reported staircase spot. setpos enables noclip; turn it off before "
                  "testing movement. The cover is func_bossgate (*38). Both shipped start maps already include "
                  "every world leaf in its own PVS (measured 2026-10-04; the probe scripts, Misc/quakevr/pvs/, are in git "
                  "history)."),
        header("Dialogs"),
        command("New Game Confirmation (3 s)", "vr_test_dialog 3 0")
            .help("Shows the New Game confirmation for 3 seconds (it closes by itself): the game must stay in the world "
                  "while it's up, turn your head to see (vr_test_dialog [seconds] [mock head turn] [eyeshot])."),
        header("Cheats"),
        command("God Mode", "god").help("god: takes no damage (again: takes damage)."),
        command("Quad Damage", "impulse 255").help("Quad Damage for 30 seconds."),
        command("All Weapons", "impulse 9").help("Every weapon and full ammo."),
    };
}

[[nodiscard]] za::Vector<Item> pageForceGrab()
{
    return {
        toggle("Force Grab", vr_forcegrab_mode).help("Pull pickups and weapons to your hand from afar."),
        slider("Distance", vr_forcegrab_distance, 100.f, 1500.f, 25.f, "%.0f").extend(100.f, 4000.f),
        slider("Aim Cone", vr_forcegrab_cone, 3.f, 45.f, 1.f, "%.0f deg").extend().help("How far off where the hand points an object may be."),
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
        cycle("Method", vr_hull_method, {{0.f, "Brush Sweep"}, {1.f, "Compiled Hull"}})
            .help("Brush Sweep: your box swept against the map's brushes (Quake 2's way). Compiled Hull: a clipping "
                  "hull built for your width when the map loads (qbsp's way), traced like Quake's own. They should "
                  "feel the same; this is for comparing them."),
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
// out, and so are the rows marked advanced() or developer() (MENU_REVIEW.md). menu_vr <n> opens any page.
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
    {"Changed Settings", pageChanged, pageAdvanced, LevelStandard},                                    // 132 (MENU_REVIEW.md)
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
    {"Machine Horde Tests", pageMachineHordeTests, pageDebugTests, LevelDeveloper},
    {"Graphics - Relighting", pageGraphicsRelighting, pageGraphics},
    {"Gore - Limb Gore", pageLimbGore, pageGore},
    {"Ragdolls - Vore", pageRagdollVore, pageRagdolls, LevelDeveloper},
    {"Ragdolls - Centroid", pageRagdollCentroid, pageRagdolls, LevelDeveloper},
    {"Dawn of the Machine Tests", pageMg3Tests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Weapons", pageMg3WeaponTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Monsters", pageMg3MonsterTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine Bestiary", pageMg3BestiaryTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine: Shub", pageMg3ShubTests, pageDebugTests, LevelDeveloper},
    {"Dawn of the Machine: Chthon", pageMg3ChthonTests, pageDebugTests, LevelDeveloper},
    {"Debug - Cheats and Recording", pageDebugCheats, pageDebug, LevelDeveloper}, // (vr_menu_cheats.inc)
    {"Reloading", pageReloading, pageWeaponsHub},
    {"Reloading - Shotgun", pageReloadShotgun, pageReloading},
    {"Reloading - Super Shotgun", pageReloadSuperShotgun, pageReloading},
    {"Reloading - Nailgun", pageReloadNailgun, pageReloading},
    {"Reloading - Super Nailgun", pageReloadSuperNailgun, pageReloading},
    {"Reloading - Thunderbolt", pageReloadThunderbolt, pageReloading},
    {"Reloading - Launchers", pageReloadLaunchers, pageReloading},
    {"Reloading - Cell Cords", pageReloadCellCords, pageReloading},
    {"Stealth AI", pageStealth, pageCombat}, // (vr_menu_stealth.inc)
    {"Stealth AI Tests", pageStealthTests, pageDebugTests, LevelDeveloper},
    {"Holding Enemies", pageHoldingEnemies, pageCombat},
};
constexpr int pageCount = static_cast<int>(sizeof(pages) / sizeof(pages[0]));

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
// Wrappers (VR Settings' own rows over other settings, vr_cvars.inc vr_menu_*): Turning Mode, Move Towards and the one
// mirrored hand calibration. Their cvars aren't saved: each is shown as the settings it stands for are now (syncWrappers,
// as the pages are built and drawn) and, set, sets them (onWrapperSet). Their defaults are those settings' defaults, so
// Reset This Page and Reset All reset them too.
// ----------------------------------------------------------------------------

bool wrapperBusy = false;      // a wrapper shown (syncWrappers): not a choice made
float lastSnapAngle = 45.f;    // Turning Mode: Snap goes back to the angle last used (45 at first)
int mainPageSnap = -1;         // VR Settings built with snap turning (Snap Angle shown) or smooth (Turn Speed shown)
bool resetAllArmed = false;    // Reset All to Defaults pressed once: Press Again to Reset All
double resetAllArmedTime = 0.0;

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
    if(&var == &vr_menu_turning || &var == &vr_menu_move_towards)
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
    showWrapper(vr_menu_turning, vr_snap_turn.value > 0.f ? 1.f : 0.f);
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
        Cvar_SetValueQuick(&vr_snap_turn, var->value != 0.f ? lastSnapAngle : 0.f);
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

// VR Settings (Options > VR Settings, the main menu's VR Settings, the corner's): what a new player sets, each in a few
// words, in the order they come to it. Every other setting is under Advanced VR Options (ROUND21.md, "VR Settings for
// first-time players"), and so is each row here, on its topic's page. No link to them here (nor to Search): the main
// menu's Advanced VR row and the corner's Advanced VR and Search buttons open them, and the trees of Search and of the
// boards' menu paths start at both pages (menuRoots).
za::Vector<Item> pageMain()
{
    const bool snap = vr_snap_turn.value > 0.f;
    mainPageSnap = snap ? 1 : 0;
    const char* handHelp = "Both hands, mirrored: moves or turns the drawn hands (and what they hold) on your controllers, "
                           "so they sit where your real hands are. Show Controller helps; 0 is the shipped calibration.";
    const char* holsterHelp = "Both holsters of the pair, mirrored: moves them forward, inward (towards your middle) or up "
                              "from their shipped place (0), in Quake's units (about 3 cm). Shown on your body while "
                              "you choose here.";
    za::Vector<Item> list{
        header("Height Calibration"),
        slider("Height", vr_height_calibration, 1.f, 2.2f, 0.01f, "%.2f m").extend(0.5f, 3.f)
            .help("Your real height: it puts your eyes at the right height in the game and fits the body to you. Set "
                  "Height Now measures it."),
        action("Set Height Now", calibrateHeight)
            .help("Stand up straight, look straight ahead and press: your height is taken from the headset."),
        slider("World Scale", vr_world_scale, 0.5f, 2.f, 0.05f, "%.2f").extend(0.25f, 4.f)
            .help("How big the world feels around you: higher makes it bigger. 1.25: Quake's sizes as a person would "
                  "see them."),
        slider("Floor Offset", vr_floor_offset, -50.f, 30.f, 1.f, "%.0f").extend(-400.f, 400.f)
            .help("Moves the floor up or down: change it if you feel you are floating above it or sunk into it."),

        header("Hand Calibration"),
        toggle("Show Controller", "vr_show_controller")
            .help("Draws your real controllers, see-through, where the game tracks them: line the hands up with them "
                  "below. Not saved."),
        slider("Hand Forward", vr_menu_hands_x, -5.f, 5.f, 0.1f, "%+.1f cm").extend(-20.f, 20.f).help(handHelp),
        slider("Hand Inward", vr_menu_hands_y, -5.f, 5.f, 0.1f, "%+.1f cm").extend(-20.f, 20.f).help(handHelp),
        slider("Hand Up", vr_menu_hands_z, -5.f, 5.f, 0.1f, "%+.1f cm").extend(-20.f, 20.f).help(handHelp),
        slider("Hand Pitch", vr_menu_hands_pitch, -30.f, 30.f, 0.5f, "%+.1f").extend(-90.f, 90.f)
            .help("Both hands: tilts the hands and the guns in them up or down on the controllers, until aiming feels "
                  "natural. Changes where they aim."),
        slider("Hand Yaw", vr_menu_hands_yaw, -30.f, 30.f, 0.5f, "%+.1f").extend(-90.f, 90.f)
            .help("Both hands, mirrored: turns the hands and the guns in them inward or outward on the controllers. "
                  "Changes where they aim."),
        slider("Hand Roll", vr_menu_hands_roll, -30.f, 30.f, 0.5f, "%+.1f").extend(-90.f, 90.f).help(handHelp),
        action("Reset Hand Offsets", resetHandOffsets)
            .help("Both hands' moves and turns back to the shipped calibration. Each hand's own values: Advanced VR "
                  "Options > Weapons > Hand/Gun Calibration."),

        header("Holster Calibration"),
        slider("Hip Holsters Forward", vr_menu_holster_hip_x, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Hip Holsters Inward", vr_menu_holster_hip_y, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Hip Holsters Up", vr_menu_holster_hip_z, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Chest Holsters Forward", vr_menu_holster_chest_x, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Chest Holsters Inward", vr_menu_holster_chest_y, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Chest Holsters Up", vr_menu_holster_chest_z, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Back Holsters Forward", vr_menu_holster_back_x, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Back Holsters Inward", vr_menu_holster_back_y, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Back Holsters Up", vr_menu_holster_back_z, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        action("Reset Holsters", resetHolsters)
            .help("Every pair of holsters back to its shipped place. Each pair's own values (its turn and reach too): "
                  "Advanced VR Options > Weapons > Hip Holsters, and > Hotspots."),

        // (The author's note e1m1_2026-10-07_22-40-30: its basics here, after the holsters; the rest of it, its turn, its
        // reach and the counter's place, on Advanced VR Options > Weapons > Reloading.)
        header("Ammo Pouch"),
        slider("Ammo Pouch Forward", vr_ammo_pouch_x, -10.f, 10.f, 0.25f, "%+.2f").extend(-40.f, 40.f)
            .help("Immersive reloading's pouch on the front of your belt, where you take rounds and magazines: forward "
                  "(negative: back, round the hips), in Quake's units (about 3 cm). With the body drawn, 0 is on the belly."),
        slider("Ammo Pouch Right", vr_ammo_pouch_y, -10.f, 10.f, 0.25f, "%+.2f").extend(-40.f, 40.f)
            .help("The ammo pouch to your right (negative: left), in Quake's units (about 3 cm)."),
        slider("Ammo Pouch Up", vr_ammo_pouch_z, -10.f, 10.f, 0.25f, "%+.2f").extend(-40.f, 40.f)
            .help("The ammo pouch up (negative: down) from the hip holsters' height, in Quake's units (about 3 cm)."),
        slider("Ammo Pouch Size", vr_ammo_pouch_scale, 0.2f, 3.f, 0.05f, "%.2fx").extend(0.05f, 10.f)
            .help("How big the ammo pouch is drawn (how near a hand must come to reach into it: Advanced VR Options > "
                  "Weapons > Reloading > Reach)."),
        toggle("Ammo Counter", vr_ammo_pouch_counter)
            .help("A small screen on the ammo pouch: how many of what it gives (shells, nails, cells) you have left."),

        header("Locomotion"),
        cycle("Move Towards", vr_menu_move_towards, {{1.f, "Head"}, {2.f, "Left Hand"}, {3.f, "Right Hand"}})
            .help("Where pushing the stick forward takes you: where you look (Head), or where that hand points, so you "
                  "can walk one way and look another."),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}})
            .help("Whether you run or walk; holding the speed button does the other."),
        slider("Stick Deadzone", vr_deadzone, 0.f, 50.f, 5.f, "%.0f%%")
            .help("How far a stick must be pushed before it does anything. Raise it if you drift or turn without "
                  "touching the stick: worn sticks often need 20-25%."),
        toggle("Swap Stick Functions", vr_stick_swap)
            .help("Off: the left stick moves you and the right one turns. On: the right stick moves, the left turns."),
        cycle("Swimming", vr_swim, {{1.f, "Immersive"}, {0.f, "Vanilla"}})
            .help("Immersive: in water the stick slows and strokes of your hands move you. Vanilla: the stick swims as in "
                  "Quake."),

        header("Comfort"),
        cycle("Vignette", vr_comfort_vignette, {{0.f, "Off"}, {1.f, "Moving and turning"}, {2.f, "Moving only"}, {3.f, "Turning only"}})
            .help("Darkens the edges of your view while the sticks move or turn you: it eases motion sickness for many. "
                  "Your own steps in the room never do it."),
        slider("Vignette Strength", vr_comfort_vignette_strength, 0.1f, 1.f, 0.1f, "%.1f")
            .help("How dark and how wide the vignette is: at 1 you see through a narrow tunnel."),
        slider("Fade on Scripted Teleports", vr_comfort_teleport_fade, 0.f, 2.f, 0.1f, "%.1f s")
            .help("When the game moves you somewhere else at once (a boss sending you to another arena), your view goes "
                  "black and comes back over this long. 0: no fade."),
        cycle("Death View", vr_death_view, {{0.f, "Off"}, {1.f, "Third Person"}, {2.f, "Immersive"}})
            .help("When you die (not gibbed) your body falls as a ragdoll, thrown by the blow that killed you. Off: no "
                  "body, as in Quake. Third Person: your view stays where your eyes were, your body there to look at. "
                  "Immersive: your view goes into your body's head and follows it as it falls (its place smoothed, its "
                  "turns only sideways, never pitch or roll; the hands hidden; Third Person while a menu is open). Back "
                  "to normal when you respawn. Its fine tuning: Advanced VR Options > Body, Death View."),

        header("Teleportation"),
        toggle("Teleport", vr_teleport_enabled)
            .help("Jump to the spot you point at instead of walking there: the gentlest way to get around."),
        slider("Teleport Range", vr_teleport_range, 100.f, 800.f, 50.f, "%.0f").extend(100.f, 3000.f)
            .help("How far a teleport reaches, in Quake's units (one is about 3 cm)."),

        header("Turning"),
        cycle("Turning Mode", vr_menu_turning, {{0.f, "Smooth"}, {1.f, "Snap"}})
            .help("Smooth: the turning stick turns you steadily. Snap: each push turns you at once by Snap Angle, which "
                  "many find more comfortable."),
    };
    if(snap)
    {
        list.pushBack(cycle("Snap Angle", vr_snap_turn, {{30.f, "30"}, {45.f, "45"}, {90.f, "90"}})
                .help("How far each snap turns you, in degrees."));
    }
    else
    {
        list.pushBack(slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f").extend()
                .help("How fast smooth turning turns you."));
    }
    list.pushBackMultiple(
        header("Flashlight"),
        toggle("Flashlight", vr_flashlight)
            .help("A torch on your belt: trigger at it with an open hand switches it on or off; grip takes it in your "
                  "hand."),
        cycle("Flashlight Side", vr_flashlight_side, {{0.f, "Left"}, {1.f, "Right"}})
            .help("The side of your chest the torch hangs on."),

        header("Lighting"),
        slider("Ambient Light", "vr_ambient_light", 0.f, 0.4f, 0.01f, "%.2f").extend()
            .help("Light in the places no lamp reaches: raise it if the levels are too dark to see without the "
                  "flashlight (0: none)."),
        slider("Light Contrast", "vr_light_contrast", 1.f, 2.5f, 0.02f, "%.2f").extend()
            .help("Darker shadows with lamps as bright: higher is moodier (1: Quake's)."),

        header("Weapons"),
        cycle("Weapon Grip", vr_weapon_grip_mode, {{0.f, "Hold"}, {1.f, "Sticky"}})
            .help("Hold: keep the grip pressed to hold a weapon. Sticky: a press takes it, another lets it go."),
        cycle("Two-Handed", vr_2h_mode, {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual stock"}})
            .help("Hold a gun with both hands to steady it. Virtual stock: a gun brought near your shoulder also aims "
                  "from it, as against a real stock."),
        cycle("Reloading Mode", vr_reload_mode, reloadChoices())
            .help("Guns have magazines. Immersive: the shotgun is loaded a shell at a time from the ammo pouch on your "
                  "belt, the other guns at the hip holsters. Simple: a gun held at a hip holster reloads. Disabled: no "
                  "reloading. Only with the Immersive weapon mode."),

        header("Bullet Time"),
        cycle("Activation", vr_menu_bullettime, bulletTimeChoices())
            .help("What starts and stops bullet time (the world slowed while the gadget's TIME meter lasts). Wrist "
                  "Gadget: tap its screen hard with your other hand (or the butt of its gun). A thumbstick press: that "
                  "press does only this (never its bound key), and the screen tap does nothing. More: Advanced VR "
                  "Options > Combat > Bullet Time."),
        cycle("Screen Tap", "vr_bullettime_tap_gesture", {{0.f, "Single Tap"}, {1.f, "Double Tap"}})
            .help("Activation by the Wrist Gadget: Double Tap, two quick taps on its screen (a single one does nothing, "
                  "so a stray knock never starts it); Single Tap, one hard tap."),

        header("Body"),
        cycle("Body Type", vr_body_mode, {{3.f, "Full"}, {2.f, "Torso and Arms"}, {0.f, "Only Hands"}})
            .help("How much of your body you see: all of it, legs and all; the torso and arms; or only the hands."),
        cycle("Wrist Gadget Arm", vr_gadget_arm, {{0.f, "Left"}, {1.f, "Right"}})
            .help("The arm the wrist gadget (health, armour and ammo) is on."),
        cycle("Leaning Detection", vr_lean_detect, {{1.f, "On"}, {0.f, "Off"}})
            .help("On: leaning over (your head lower and tilted, your hands by your hips) leaves your feet where they "
                  "stand. Off: the body always slides back under your head. How readily: Advanced VR Options > Movement "
                  "> Locomotion."),
        command("Reset Position", "vr_recenter")
            .help("Puts your body back under your head, and facing where you look, if it was left behind (after "
                  "leaning over something, or walking into a wall)."),

        header("Haptics"),
        slider("Vibration Strength", vr_haptics_strength, 0.f, 1.f, 0.1f, "%.1f").extend(0.f, 2.f)
            .help("How strongly the controllers vibrate (0: never)."),

        header("HUD"),
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}})
            .help("Wrist gadget: health, armour and ammo on your wrist. Status bar: Quake's, on a hand."),
        cycle("Crosshair", vr_crosshair, {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}})
            .help("A mark where your gun aims: a dot, or a laser beam from the muzzle."),

        header("Sound"),
        slider("Volume", "volume", 0.f, 1.f, 0.05f, "%.2f").help("The game's sounds."),
        slider("Music Volume", "bgmvolume", 0.f, 1.f, 0.05f, "%.2f").help("The music's volume."),
        toggle("Spatial Sound", vr_snd_spatial)
            .help("Sounds come from where they are around your head, muffled by walls, with the room's echo. Best with "
                  "headphones."),

        header("Display"),
        slider("Headset Gamma", "vr_gamma", 0.5f, 1.5f, 0.02f, "%.2f").extend()
            .help("The headset's brightness: lower is brighter (1: neutral)."),
        slider("Headset Contrast", "vr_contrast", 0.5f, 2.f, 0.02f, "%.2f").extend()
            .help("The headset's contrast (1: neutral)."),

        header("Scaling"),
        slider("Render Scale", vr_render_scale, 0.5f, 1.5f, 0.05f, "%.2f").extend(0.25f, 2.f)
            .help("Eye rendering resolution, times the headset's. Above 1: smoother edges, slower. Below 1: faster, "
                  "blurrier."),
        cycle("Upscaling", vr_upscale, {{0.f, "Bilinear"}, {1.f, "FSR"}, {2.f, "NIS"}})
            .help("Below Render Scale 1: how the image is enlarged to the headset's size. FSR (AMD) and NIS (NVIDIA) "
                  "keep it sharper than Bilinear."),
        slider("Sharpening", vr_upscale_sharpness, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the upscaler sharpens the image (FSR, NIS). Too much makes edges shimmer."),
        cycle("Foveated Rendering", vr_foveated, {{0.f, "Off"}, {1.f, "Conservative"}, {2.f, "Balanced"}, {3.f, "Aggressive"}})
            .help("Draws the edges of the view, which the lenses blur anyway, in less detail: faster. NVIDIA graphics "
                  "cards only."),

        header("Graphics"),
        toggle("Retro Textures", vr_retro)
            .help("Detailed textures drawn in Quake's chunky, pixelated look, crisp as you move your head."),
        toggle("Retro Lighting", "vr_retrolight")
            .help("Quake's coarse, banded light and blocky shadows, fixed to the walls. Goes with Retro Textures."),
        cycle("Antialiasing", "vid_fsaa", {{0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}})
            .help("Smoother edges: thin lines such as the gaps between planks stop crawling and breaking into dashes "
                  "as your head moves. 4x (as shipped) does most of it; 8x takes nearly twice 4x's GPU time for little more."),
        cycle("Anisotropic Filtering", "gl_texture_anisotropy", {{1.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}, {16.f, "16x"}})
            .help("Keeps floors and walls seen at a glancing angle sharp instead of blurry (smooth-filtered textures). "
                  "Little GPU time; 16x is the sharpest. Capped at what the graphics card supports."),
        slider("Bloom", "vr_bloom", 0.f, 1.5f, 0.02f, "%.2f").extend()
            .help("A glow around lamps, glowing panels, flashes and explosions (0: off)."),
        toggle("Tone Mapping", "vr_tonemap")
            .help("Bright lights, lava and explosions keep their colour and detail instead of turning flat yellow and "
                  "white."),
        toggle("Bump Mapping", "vr_normalmaps")
            .help("Walls and floors get bumps and grooves that catch the light. From the next map."),
        toggle("Parallax Mapping", "vr_parallax")
            .help("Walls and floors look carved: their dark parts sink in as you move your head (needs Bump Mapping)."),

        header("Reset"),
        action(resetAllArmed ? "Press Again to Reset All" : "Reset All to Defaults", resetAll)
            .help("Every Quake VR setting back to as it shipped, but not your calibration (height, floor, body and hands: "
                  "Reset Hand Offsets for those). Press it twice: the second time within 3 seconds.")
    );
    return list;
}

// Split from VR Settings: its body and display settings (the quick ones; all of them under Advanced VR Options).
[[nodiscard]] za::Vector<Item> pageBodyDisplay()
{
    return {
        header("Sides"),
        cycle("Handedness", vr_handedness, {{0.f, "Custom"}, {1.f, "Right-handed"}, {2.f, "Left-handed"}})
            .help("Sets which stick moves you, the wrist gadget's arm and the flashlight's side together. Left-handed: the right "
                  "stick moves, the gadget on the right arm, the torch on the right (Custom: set apart, below)."),
        toggle("Swap Stick Functions", vr_stick_swap).help("Off: the left stick moves you and the right one turns. On: the right stick moves, the left turns."),
        cycle("Wrist Gadget Arm", vr_gadget_arm, {{0.f, "Left"}, {1.f, "Right"}}).help("The arm the wrist gadget (the HUD) is on."),
        cycle("Flashlight Side", vr_flashlight_side, {{0.f, "Left"}, {1.f, "Right"}}).help("The side of your chest the torch hangs on (Chest Flashlight)."),

        header("Body"),
        slider("World Scale", vr_world_scale, 0.5f, 2.f, 0.05f, "%.2f").extend(0.25f, 4.f)
            .help("How big the world feels around you (1.25: Quake's sizes as a person sees them)."),
        slider("Floor Offset", vr_floor_offset, -50.f, 30.f, 1.f, "%.0f").extend(-400.f, 400.f).advanced()
            .help("Moves the floor up or down: change it if you feel you are floating or sunk in the floor."),
        toggle("Chest Flashlight", vr_flashlight).help("A torch on your belt (Flashlight Side): trigger at it with an open hand switches it; grip takes it. B or Y clips it on a gun or on your head."),
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        toggle("Holster Models", vr_leg_holster_model_enabled),

        header("Display"),
        cycle("Status Bar", vr_sbar_mode, {{1.f, "Off hand"}, {0.f, "Main hand"}}).help("The hand Quake's status bar is on (HUD: Status bar)."),
        cycle("Desktop Mirror", vr_mirror, {{0.f, "Off"}, {1.f, "Left eye"}, {2.f, "Both eyes"}}),
        open("Recording (Window View)", pageIndex(pageRecording))
            .help("What the desktop window shows for recording: a steadied mirror or a spectator camera."),
    };
}

// Under OpenXR Runtime: the runtime the backend chose and why (vr_xr_runtime.hpp).
[[nodiscard]] const char* xrRuntimeLine()
{
    const char* line = xrruntime::statusLine();
    return line[0] ? line : "Chosen when VR starts";
}

// Split from VR Settings: the headset.
[[nodiscard]] za::Vector<Item> pageHeadset()
{
    return {
        header("Headset"),
        toggle("VR", vr_enabled),
        action("Restart VR", restartVr),
        cycle("OpenXR Runtime", vr_xr_runtime, {{4.f, "Auto"}, {0.f, "System default"}, {1.f, "Virtual Desktop (VDXR)"}, {2.f, "SteamVR"}})
            .help("Which OpenXR runtime runs the headset; VR restarts. Auto: the one whose app is running (Virtual Desktop: the runtime picked in its Streamer's OpenXR Runtime option; VDXR, which skips SteamVR, unless SteamVR is picked there), else the system's. Keep Virtual Desktop's 'Emulate Index controllers' off."),
        info(xrRuntimeLine),
        cycle("Try Other Runtimes", vr_xr_runtime_fallback, {{0.f, "Off"}, {1.f, "On"}, {2.f, "On, SteamVR too"}}).advanced()
            .help("Auto: when the chosen runtime fails to start (no headset), try the other installed ones before playing flat. An idle SteamVR (not the system's runtime) only with 'SteamVR too': trying it starts SteamVR."),
        slider("Render Scale", vr_render_scale, 0.5f, 1.5f, 0.05f, "%.2f").extend(0.25f, 2.f)
            .help("Eye rendering resolution, times the headset's (SteamVR's resolution included); resampled to it."), // + the size (renderScaleHelp)
        cycle("Upscaling", vr_upscale, {{0.f, "Bilinear"}, {1.f, "FSR"}, {2.f, "NIS"}})
            .help("Below Render Scale 1: how the eyes are enlarged to the headset's size. FSR (AMD) and NIS (NVIDIA) keep edges and text sharper than bilinear, near the lens centre."),
        slider("Sharpness", vr_upscale_sharpness, 0.f, 1.f, 0.05f, "%.2f")
            .help("The upscaler's sharpening (FSR, NIS). Too much makes edges shimmer."),
        cycle("Foveated Rendering", vr_foveated, {{0.f, "Off"}, {1.f, "Conservative"}, {2.f, "Balanced"}, {3.f, "Aggressive"}})
            .help("Shade the scene coarser towards the edges of the lenses, where they blur anyway: faster. NVIDIA GPUs only (variable-rate shading)."),
        toggle("Hide Lens Corners", vr_visibility_mask)
            .help("Skip the pixels the lenses never show (if the headset gives them): faster, looks the same. Black corners in the desktop mirror."),
        slider("Near Clip", vr_nearclip, 0.02f, 1.f, 0.02f, "%.2f units").extend(0.02f, 4.f).advanced()
            .help("How near the eyes things are still drawn (a unit is 3 cm): lower draws a gun at your face whole. Not below 1 without Float Depth."),
        cycle("Held Items at the Eyes", vr_nearclip_held, {{0.f, "Clipped"}, {1.f, "Never clipped"}, {2.f, "Block the view"}}).advanced()
            .help("A weapon or hand right at your eyes: Never clipped draws its parts nearer than Near Clip too; Block the view also draws its inside when an eye is in it, instead of seeing through it."),
        toggle("Float Depth", vr_depth_float).advanced()
            .help("A 32-bit float depth buffer for the eyes: no flicker far away with a small Near Clip. Applies after Restart VR."),
    };
}

// Advanced VR Options: every page, in groups (a page for each group of many, the others here).
za::Vector<Item> pageAdvanced()
{
    return {
        header("Setup"),
        open("Body and Display", pageIndex(pageBodyDisplay)).help("Each hand's side, world scale, the body, the status bar, the desktop mirror."),
        open("Headset", pageIndex(pageHeadset)).help("VR on or off, the OpenXR runtime, render scale, upscaling and foveated rendering."),
        open("Sound", pageIndex(pageSound)).help("Spatial audio: sounds around your head (HRTF), muffled by walls, the room's reverb, underwater, your weapons in your hands, Doppler, sounds at your ear."),
        open("Tips", pageIndex(pageTips)).help("Tips for new players, shown once near what they are about: floating by it or on the wrist gadget."),
        open("Changed Settings", pageIndex(pageChanged))
            .help("Every setting you changed from its default, from all the pages, on one page (each marked * where it lives)."),
        command("Run VR Calibration Again", "vr_setup")
            .help("The calibration room and its steps, as at the first start (the main menu's VR Calibration): your height, "
                  "your body, and the main settings on its wall buttons. Ends the game you are in."),
        header("Game"),
        open("Play", pageIndex(pagePlay)),
        open("Combat", pageIndex(pageCombat)).help("Melee, parry and bash, stamina, batting and catching, damage and knockback."),
        open("Movement", pageIndex(pageMovement)).help("Locomotion, climbing, swimming and the grappling hook."),
        open("Carrying and Throwing", pageIndex(pageCarryingHub))
            .help("Carrying, throwing, force grab, wall torches, rocks and bricks, gibs; what you hold's offsets and weights."),
        open("World", pageIndex(pageGameplay)).help("Monsters, the weapons they drop, weapon drops, rumble and heartbeat."),
        open("Gore", pageIndex(pageGore)),
        header("Body and Weapons"),
        open("Body", pageIndex(pageBody)).help("The body, its arms and pauldrons, body and player calibration, the Death View."),
        open("Flashlight", pageIndex(pageFlashlight)),
        open("Weapons", pageIndex(pageWeaponsHub))
            .help("Weapon offsets and weights, hand/gun calibration, fingers, aiming, weight, holsters and immersion."),
        header("Display"),
        open("HUD and Menus", pageIndex(pageHudHub)).help("Wrist gadget, screens, colours, status bar, crosshair and menu."),
        open("Graphics", pageIndex(pageGraphics)),
        header("Playtesting"),
        open("Motion Recorder", pageIndex(pageMotionRecorder)),
        open("Review Takes", pageIndex(pageReviewTakes)),
        open("Debug", pageIndex(pageDebug))
            .help("Voice notes; debug views, logs and reports; the profiler and memory log; reloads, dumps and tests."),
        info([]() -> const char* { return buildVersionLine(); }),
    };
}

za::Vector<Item> pageCombat()
{
    return {
        open("Melee", pageIndex(pageMeleeSettings)).help("Swings and punches, bloodlust, the headbutt."),
        open("Parry and Bash", pageIndex(pageParryBash)).help("Parrying, bashing and shoving, counter-attacks, the training dummy's blows."),
        open("Stamina", pageIndex(pageStamina)).help("What parries, shoves, blows and hanging from a hold cost, and being exhausted."),
        open("Batting and Catching", pageIndex(pageBatting)).help("Batting projectiles back; catching and returning grenades; hand grenades."),
        open("Damage and Knockback", pageIndex(pageDamage)).help("Hit detection, damage to monsters and to you, headshots, knockback, hits knocking your hands."),
        open("Weapon Damage", pageIndex(pageWeaponDamage))
            .help("Every weapon's base damage, to balance them: your guns, the mission packs', melee weapons, the weapons "
                  "monsters drop, thrown weapons."),
        open("Enemy Weapons", pageIndex(pageEnemyWeapons))
            .help("The swords, chainsaws, shotguns and laser rifles monsters drop: their fuel, ammo and handling."),
        open("Enemy Shoves", pageIndex(pageEnemyShoves))
            .help("Grunts and enforcers shove you away when you stand too close: how close, how soon, how hard."),
        open("Knockdowns", pageIndex(pageKnockdowns))
            .help("Your shoves can knock monsters down as ragdolls, alive: the chances, how long they stay down, getting up."),
        open("Holding Enemies", pageIndex(pageHoldingEnemies))
            .help("Experimental: grip a living enemy with an empty hand to hold on to it, slow it and pull it a little."),
        open("Bullet Time", pageIndex(pageBulletTime))
            .help("The wrist gadget's button slows the world for as long as its meter lasts; Sandevistan; its look."),
        open("Burning", pageIndex(pageBurning))
            .help("What lit torches and lava nails set on fire: the damage (it never stacks), how long, the flames, "
                  "corpses and crates, setting things on fire by touch, nails through a torch's flame."),
        open("Stealth AI", pageIndex(pageStealth))
            .help("Enhanced AI (on/off: Quake's monsters at once). Monsters idle, alert or hostile: seeing you by the light "
                  "on you, hearing noises, investigating, spreading the alarm; sneak attacks."),
    };
}

za::Vector<Item> pageMovement()
{
    return {
        open("Locomotion", pageIndex(pageLocomotionSettings)).help("Moving, turning, teleport, leaning and room scale."),
        open("Climbing", pageIndex(pageClimbing)),
        open("Swimming", pageIndex(pageSwimSettings)),
        open("Grappling Hook", pageIndex(pageGrapple)),
        open("Player Hitbox", pageIndex(pageHitbox)).help("How wide you are against walls, doors, monsters and players."),
        open("Monster Hitbox", pageIndex(pageMonsterHitbox)).help("How wide monsters are against walls (a prototype, off by default)."),
    };
}

za::Vector<Item> pageCarryingHub()
{
    return {
        open("Carrying", pageIndex(pageCarrying)).help("Ammo and health boxes, explosive boxes, armour and pickups."),
        open("Throwing and Physics", pageIndex(pageThrowing)),
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend()
            .help("How fast what you throw flies, times your hand's speed."),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}})
            .help("How thrown things fall: Real, as on Earth; Quake, as Quake's own gravity."),
        open("Force Grab", pageIndex(pageForceGrab)),
        open("Wall Torches", pageIndex(pageWallTorches)),
        open("Rocks and Bricks", pageIndex(pageRocksBricks)),
        open("Crates", pageIndex(pageCrates)).help("Wooden crates by the walls: break them, carry them, hide behind them."),
        open("Gibs and Corpses", pageIndex(pageGibs)),
        header("What You Hold"),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets)),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights)),
    };
}

// The programmatic weapon effects (vr_weaponfx.cpp): their global settings; each weapon's are in Weapon Offsets > Effects.
za::Vector<Item> pageWeaponEffects()
{
    return {
        header("Recoil"),
        toggle("Programmatic Recoil", vr_weapon_recoil)
            .help("Each shot kicks the drawn weapon back and tips it up, for the weapons whose Recoil is on (Weapon Offsets > "
                  "Effects: the grunts' burst rifle; the others' models have their own). Looks only: the aim and the shots "
                  "stay where your controller is."),
        slider("Kick Back", vr_recoil_kick, 0.f, 5.f, 0.1f, "%.1f cm").extend(0.f, 20.f)
            .help("How far a shot kicks the weapon back, at a weapon's Recoil Strength 1."),
        slider("Muzzle Rise", vr_recoil_rise, 0.f, 15.f, 0.5f, "%.1f deg").extend(0.f, 45.f)
            .help("How far a shot tips the muzzle up, at a weapon's Recoil Strength 1."),
        header("Shotgun Auto Pump"),
        toggle("Auto Pump", vr_autopump)
            .help("After each shot the shotgun cycles itself: its fore-end is driven back along the guide rods over the "
                  "barrel and springs home, and the spent shell leaves the port as it reaches the back. Off: the fore-end "
                  "stays and the shell leaves as before. Looks only: the fire rate is the same."),
        slider("Auto Pump Time", vr_autopump_time, 0.15f, 0.45f, 0.01f, "%.2f s").extend(0.1f, 0.48f)
            .help("How long the stroke takes, back and home (the shotgun fires again after 0.5 s)."),
        slider("Auto Pump Delay", vr_autopump_delay, 0.f, 0.3f, 0.01f, "%.2f s").extend(0.f, 0.38f)
            .help("How long after the shot the stroke starts: the shot, a moment, then the cycle (0: at once). Cut short so "
                  "that the stroke is home before the shotgun can fire again (0.5 s)."),
        slider("Auto Pump Travel", vr_autopump_travel, 0.5f, 3.2f, 0.1f, "%.1f units").extend(0.f, 3.2f)
            .help("How far back the fore-end goes, in the gun model's units (about 3.8 cm each, as drawn)."),
        slider("Auto Pump Sound", vr_autopump_sound, 0.f, 1.f, 0.1f, "%.1f").help("Volume of its two clacks (0: off)."),
        slider("Auto Pump Haptics", vr_autopump_haptics, 0.f, 2.f, 0.1f, "%.1f")
            .help("Strength of the light ticks in the hand at the back of the stroke and home (0: off)."),
        header("Muzzle Flash"),
        toggle("Programmatic Muzzle Flash", vr_muzzle_flash)
            .help("The shotgun's flash at the muzzle of the weapons whose Muzzle Flash is on (Weapon Offsets > Effects: the "
                  "grunts' burst rifle), following the gun."),
        toggle("Enemies' Muzzle Flashes", vr_muzzle_flash_enemies).help("The grunts' and enforcers' guns flash at their muzzles as they fire."),
        toggle("Enemies' Muzzle Smoke", vr_muzzle_smoke_enemies).help("Puffs of gun smoke out of an enforcer's rifle's and a grunt's gun's muzzle as they fire, as out of your guns'."),
        slider("Enemies' Flash Size", vr_muzzle_flash_enemy_size, 0.2f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f),
        header("Bullet Tracers"),
        toggle("Bullet Tracers", vr_tracers)
            .help("Hitscan shots (the shotguns, the burst rifle) draw streaks of light flying from the muzzle to what they "
                  "hit. A weapon may differ (Weapon Offsets > Effects: Tracers, and its multipliers)."),
        toggle("Enemies' Tracers", vr_tracers_enemies).help("The grunts' shots too."),
        slider("Tracer Speed", vr_tracer_speed, 20.f, 600.f, 10.f, "%.0f m/s").extend(1.f, 3000.f),
        slider("Tracer Length", vr_tracer_length, 0.1f, 5.f, 0.1f, "%.1f m").extend(0.01f, 50.f),
        slider("Tracer Thickness", vr_tracer_width, 0.2f, 5.f, 0.1f, "%.1f cm").extend(0.05f, 30.f),
        slider("Chance a Pellet", vr_tracer_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance each pellet (or round) shows a tracer: a shotgun's 6 pellets at 0.3 show about 2."),
        slider("Tracer Red", vr_tracer_r, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Tracer Green", vr_tracer_g, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Tracer Blue", vr_tracer_b, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Tracer Brightness", vr_tracer_brightness, 0.f, 4.f, 0.1f, "%.1fx").extend(0.f, 10.f),
        header("Each Weapon"),
        open("Weapon Offsets (Held Weapon)", pageIndex(pageWeaponOffsets))
            .help("The held weapon's own: its Effects section turns its recoil and flash on, and changes its tracers."),
        command("Test the Held Weapon's Effects", "vr_weaponfx_test 1 3")
            .help("vr_weaponfx_test 1 3: the main hand's weapon kicks and flashes as if it fired, with 3 tracers (no shot)."),
    };
}

// The lightning gun in water (vr_lg_water*; QC weapons.qc VR_LGWater_*, vr_shock.cpp).
za::Vector<Item> pageLightningWater()
{
    return {
        toggle("Lightning Gun in Water", vr_lg_water)
            .help("Fired with your body waist deep, or your hand or the gun's muzzle in water, the lightning gun shocks you and both hands drop what they hold; the shock spreads through the water to what is in it. Fired into water from outside, it electrifies the water round where the beam goes in. Off: Quake's discharge (every cell at once, all round you)."),
        slider("Shock Damage to You", vr_lg_water_self_damage, 0.f, 100.f, 5.f, "%.0f").extend(0.f, 500.f),
        slider("Shock Damage to Others", vr_lg_water_damage, 0.f, 200.f, 5.f, "%.0f").extend(0.f, 1000.f)
            .help("What else is in the water takes this at the gun's muzzle, fading out with distance."),
        slider("Reach", vr_lg_water_radius, 32.f, 1024.f, 16.f, "%.0f units").extend(8.f, 4096.f)
            .help("How far through the water the shock, and the electrified water, reach."),
        slider("Falloff", vr_lg_water_falloff, 0.f, 4.f, 0.25f, "%.2f").extend()
            .help("How the damage fades with distance: 1 evenly to nothing at the reach, 2 and more sooner, 0 not at all."),
        slider("Electrified Water Damage", vr_lg_water_tick_damage, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 300.f)
            .help("Each bolt fired into water from outside (ten a second): damage where it goes in, fading out the same way."),
        slider("Shock Flash", vr_lg_water_flash, 0.f, 1.f, 0.1f, "%.1f")
            .help("The shock's blue flash over the view and the arcs in front of your eyes (the arcs on your arms and body stay)."),
        command("Test the Shock Effect", "vr_shock_test 0").help("Shows the shock's flash and arcs, without the damage."),
        command("Test the Electrified Water", "vr_shock_test 1").help("Arcs on the water below the point ahead of you, for 3 seconds."),
    };
}

za::Vector<Item> pageWeaponsHub()
{
    return {
        header("Tuning"),
        open("Weapon Offsets (Held Weapon)", pageIndex(pageWeaponOffsets)),
        open("Weapon Weights (Held Weapon)", pageIndex(pageWeaponWeights)),
        open("Hand/Gun Calibration", pageIndex(pageHandGunCalibration)),
        open("Fingers and Collisions", pageIndex(pageFingersCollisions)),
        header("Handling"),
        open("Aiming", pageIndex(pageAimingSettings)).help("Two-handed aiming; how weight feels: the spring, tired arms."),
        open("Weight and Damage", pageIndex(pageWeightDamage)),
        open("Weapon Damage", pageIndex(pageWeaponDamage)).help("Every weapon's base damage, to balance them."),
        open("Immersion", pageIndex(pageImmersionSettings)).help("Holsters, reloading, throwing weapons, shell casings, haptics."),
        open("Reloading", pageIndex(pageReloading)).help("Reloading's mode; the ammo pouch; a page per gun (its load point, its magazine, holding and pulling it)."),
        open("Lightning Gun in Water", pageIndex(pageLightningWater)).help("The shock fired under water, and electrified water."),
        open("Weapon Effects", pageIndex(pageWeaponEffects)).help("Recoil, muzzle flashes and bullet tracers."),
        header("Holsters"),
        open("Hotspots", pageIndex(pageHotspotSettings)).help("The virtual stock, the shoulder and upper holsters."),
        open("Hip Holsters", pageIndex(pageHipHolsters)).help("The hip holsters and their slots' models; the grenade pouch at your back."),
    };
}

za::Vector<Item> pageHudHub()
{
    return {
        open("Wrist Gadget", pageIndex(pageGadget)),
        open("Screens", pageIndex(pageScreens)),
        open("Colours", pageIndex(pageColours)),
        open("Status Bar", pageIndex(pageHudConfiguration)),
        open("Crosshair", pageIndex(pageCrosshairSettings)),
        open("Menu", pageIndex(pageMenuSettings)),
    };
}

int pageIndex(PageBuilder build)
{
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].build == build)
        {
            return p;
        }
    }
    return PageMain;
}

// Weapon Offsets: the settings of the weapon a hand holds (vr_wofs_*_NN of its slot), built anew
// each time the page is shown, as the weapon in hand changes.
int weaponOffsetsHand = 1; // 1 main hand, 0 off hand
bool weaponOffsetsStale = true;
int weaponOffsetsSlot = -1;

void showPage(int target);

void weaponOffsetsOtherHand()
{
    weaponOffsetsHand = 1 - weaponOffsetsHand;
    weaponOffsetsStale = true;
    showPage(qvr::menu::currentPage()); // (the same part of Weapon Offsets)
}

int weaponOffsetsHeldSlot = -1; // the weapon in the hand (weaponOffsetsSlot: the one edited: what it inherits from)
int weaponOffsetsInherit = -1;

void weaponOffsetsStopInheriting()
{
    weapons::stopInheriting(weaponOffsetsHeldSlot);
    weaponOffsetsStale = true;
}

void weaponOffsetsReset()
{
    weapons::resetSlotToDefaults(weaponOffsetsSlot, weapons::Part::Offsets);
    weaponOffsetsStale = true;
}

void weaponOffsetsPrint()
{
    weapons::printSlot(weaponOffsetsSlot, weapons::Part::Offsets);
}

// The kind of holster whose Holstered pose the page edits (vr_weapon_holster: 1 hips, 2 chest, 3 back), as the page was
// built: a change rebuilds it.
int weaponOffsetsHolster = -1;

[[nodiscard]] weapons::HolsterKind editedHolster()
{
    return static_cast<weapons::HolsterKind>(CLAMP(1, static_cast<int>(vr_weapon_holster.value), weapons::holsterKinds) - 1);
}

// Holstered: the edited kind's six back to 0 (their defaults).
void weaponOffsetsHolsteredReset()
{
    for(int f = 0; f < weapons::holsteredFields; f++)
    {
        if(cvar_t* var = weapons::cvar(weaponOffsetsSlot, weapons::holsteredKey(editedHolster(), f)))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
}

// The hotspot being edited (vr_weapon_hotspot, 1..4) and its type, as the page was built: a change rebuilds it.
int weaponOffsetsHotspot = -1;
int weaponOffsetsHotspotType = -1;
// Whether the weapon's and the edited hotspot's fingers are set by hand, as the page was built (round 21, third pass).
int weaponOffsetsManual = -1;
int weaponOffsetsPreviewOwn = -1; // the page shows the off hand's controller preview offsets (vr_show_controller_off_own)
int weaponOffsetsHotspotManual = -1;

[[nodiscard]] int editedHotspot()
{
    return CLAMP(1, static_cast<int>(vr_weapon_hotspot.value), weapons::maxHotspots) - 1;
}

// A hotspot given to a weapon whose two-handed use is forbidden (the axe, Mjolnir, the grappling hook by
// default) would never be taken: giving one allows it (Two-Handed: Allowed).
void allowTwoHands(int slot)
{
    cvar_t* mode = weapons::cvar(slot, weapons::Key::TwoHMode);
    if(mode && static_cast<int>(mode->value) == 2) // WPN_2H_FORBIDDEN (vr_twohand.cpp)
    {
        Cvar_SetValueQuick(mode, 0.f);
        Con_Printf("Two-handed use allowed for this weapon (it has a hotspot now).\n");
    }
}

// Puts the edited hotspot (a grip) where the other hand is now, on the page's weapon.
void weaponOffsetsHotspotAtHand()
{
    const int slot = weaponOffsetsSlot;
    if(slot < 0)
    {
        return;
    }
    weapons::Hotspot h = weapons::hotspot(slot, editedHotspot());
    glm::vec3 p;
    // A cup is where the other hand's palm is (round 21, third pass); a grip, where its point is.
    const hands::State& hs = hands::current();
    const glm::vec3 at = h.type == weapons::HotspotType::Cup ? hands::palmPoint(hs, 1 - weaponOffsetsHand) : hs.pos[1 - weaponOffsetsHand];
    if(!view::hotspotAt(weaponOffsetsHand, at, p))
    {
        Con_Printf("Hold the weapon in the %s hand to place its hotspot with the other hand.\n",
            weaponOffsetsHand == 1 ? "main" : "off");
        return;
    }
    if(!weapons::isGripType(h.type))
    {
        h.type = weapons::HotspotType::Grip; // a grip or a cup stays what it is
    }
    h.pos = p;
    weapons::setHotspot(slot, editedHotspot(), h);
    allowTwoHands(slot);
    weaponOffsetsStale = true;
}

// The weapon posing mode (vr_posing.cpp) on the page's weapon: the weapon (hotspot -2), a hotspot (0..3), a new one (-1),
// the edited kind of holster (-3).
void weaponOffsetsPose(int hotspot)
{
    qmodel_t* model = weapons::heldModel(weaponOffsetsHand);
    if(weapons::slotForModel(model) != weaponOffsetsHeldSlot)
    {
        Con_Printf("Posing mode: the %s hand holds another weapon now: reopen the page.\n", weaponOffsetsHand == 1 ? "main" : "off");
        return;
    }
    const int weaponHand = static_cast<int>(vr_pose_weapon_hand.value) == 0 ? HAND_OFF : HAND_MAIN;
    const posing::Target target = hotspot == -3   ? posing::Target::Holster
                                  : hotspot == -2 ? posing::Target::Weapon
                                                  : posing::Target::Hotspot;
    if(posing::start(weaponOffsetsHeldSlot, model, weaponHand, target,
           target == posing::Target::Holster ? static_cast<int>(editedHolster()) : hotspot, qvr::menu::currentPage()))
    {
        weaponOffsetsStale = true;
    }
}

// Align Sights to My Aim (vr_sightalign.cpp) on the page's weapon.
int weaponOffsetsSightVersion = -1;
bool weaponOffsetsSightFocus = false; // the section changed: the cursor to Apply (a result) or Undo (applied)

void sightAlignStart()
{
    sightalign::start(weaponOffsetsHand, qvr::menu::currentPage());
}

void sightAlignApply()
{
    sightalign::apply();
}

void sightAlignCancel()
{
    sightalign::cancel();
}

void sightAlignUndo()
{
    sightalign::undo();
}

const char* sightAlignLine(int i)
{
    const char* line = sightalign::statusLine(i);
    return line ? line : "";
}

const char* sightAlignNone()
{
    return "No sights: a melee weapon";
}

void weaponOffsetsPoseWeapon()
{
    weaponOffsetsPose(-2);
}

void weaponOffsetsPoseHotspot()
{
    weaponOffsetsPose(editedHotspot());
}

void weaponOffsetsPoseNewHotspot()
{
    weaponOffsetsPose(-1);
}

// The weapon posing mode in the edited kind of holster: the holster floats in front, the weapon in it to be carried.
void weaponOffsetsPoseHolster()
{
    weaponOffsetsPose(-3);
}

void weaponOffsetsHotspotRemove()
{
    if(weaponOffsetsSlot >= 0)
    {
        weapons::setHotspot(weaponOffsetsSlot, editedHotspot(), weapons::Hotspot{});
        weaponOffsetsStale = true;
    }
}

// Effects: the page's weapon's effects as if it fired (vr_weaponfx_test).
void weaponOffsetsClipTorch()
{
    Cbuf_AddText(va("vr_flashlight 1; vr_flashlight_clip_gun %s\n", weaponOffsetsHand == 1 ? "right" : "left"));
}

void weaponOffsetsTorchReset()
{
    using weapons::Key;
    for(const Key key : {Key::TorchForward, Key::TorchUp, Key::TorchOut, Key::TorchPitch, Key::TorchYaw, Key::TorchRoll})
    {
        if(cvar_t* var = weapons::cvar(weaponOffsetsSlot, key))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
}

void weaponOffsetsTestEffects()
{
    Cbuf_AddText(va("vr_weaponfx_test %d 3\n", weaponOffsetsHand == 1 ? 1 : 0));
}

// After the posing test: Shot Pitch and Yaw, under Posing Mode and under Muzzle (weapons::shotAngles).
constexpr const char* weaponOffsetsShotHelp = "Turns where the weapon's shots, projectiles and beams go (the red line, from the "
                                              "muzzle) without moving the weapon: line the red line up with the sights. "
                                              "Degrees, as you hold it (the off hand's yaw mirrored).";

// A slider of the page's weapon's setting (weaponOffsetsSlot's).
[[nodiscard]] Item weaponOffsetsSlider(const char* label, weapons::Key key, float min, float max, float step, const char* format)
{
    return slider(label, weapons::cvar(weaponOffsetsSlot, key), min, max, step, format);
}

// Every Weapon Offsets page begins so (the main page and its parts, WeaponOffsetsPart): the weapon the hand holds now (an
// empty hand: the hand model), its title and Edit the Other Hand's Weapon; the main page also Inherit From. Returns the
// slot edited (weaponOffsetsSlot: the one the weapon inherits from, if it does), -1 if none (the page says so). What the
// page shows that rebuilds it (the hotspot, the Fingers choices, the holster...) is set again by the part showing it.
[[nodiscard]] int weaponOffsetsBegin(za::Vector<Item>& list, int part)
{
    using weapons::Key;
    za::String& title = pageTexts.weaponOffsetsTitle[part];
    weaponOffsetsHotspot = weaponOffsetsHotspotType = weaponOffsetsManual = weaponOffsetsPreviewOwn =
        weaponOffsetsHotspotManual = weaponOffsetsHolster = -1;
    int slot = vrActive() || cls.state == ca_connected ? weapons::heldSlot(weaponOffsetsHand) : -1;
    if(slot < 0 && cls.state == ca_connected)
    {
        slot = weapons::fistSlot(); // an empty hand: the hand model's own settings
    }
    weaponOffsetsSlot = slot;
    weaponOffsetsSightVersion = sightalign::version();

    const char* hand = weaponOffsetsHand == 1 ? "Main hand" : "Off hand";
    if(slot < 0)
    {
        title = za::String(hand) + ": hold a weapon in a game to adjust it";
        list.pushBack(header(title.cStr()));
        list.pushBack(action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand));
        return -1;
    }

    const char* model = weapons::cvar(slot, Key::ID)->string;
    title = za::String(hand) + ": " + model + " (_" + (slot + 1 < 10 ? "0" : "") + za::toString(slot + 1) + ")";

    // A weapon inheriting another's settings (InheritFrom: the other ammo's model): the page edits those.
    const int heldSlot = slot;
    weaponOffsetsHeldSlot = heldSlot;
    weaponOffsetsInherit = weapons::inheritsFrom(heldSlot);
    za::String& inheritTitle = pageTexts.weaponOffsetsInheritTitle[part];
    if(weaponOffsetsInherit >= 0)
    {
        slot = weaponOffsetsInherit;
        weaponOffsetsSlot = slot;
        inheritTitle = za::String("Settings of ") + weapons::cvar(slot, Key::ID)->string + " (inherited)";
    }
    list = {
        header(title.cStr()),
        action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand)
            .help("The page shows the weapon the hand held when it was opened: reopen it after changing weapons."),
    };
    if(part != WofsMain || slot == weapons::fistSlot())
    {
        if(weaponOffsetsInherit >= 0)
        {
            list.pushBack(header(inheritTitle.cStr()));
        }
        return slot;
    }
    za::Vector<za::Pair<float, za::String>>& inheritNames = pageTexts.weaponOffsetsInheritNames;
    inheritNames.clear();
    inheritNames.pushBack({0.f, "None"});
    for(int other = 0; other < weapons::numSlots; other++)
    {
        const char* id = weapons::cvar(other, Key::ID)->string;
        if(other != heldSlot && other != weapons::fistSlot() && id[0] && strcmp(id, "-1") != 0)
        {
            const char* base = strrchr(id, '/');
            inheritNames.pushBack({static_cast<float>(other + 1), base ? base + 1 : id});
        }
    }
    za::Vector<Choice> choices;
    for(const auto& [v, name] : inheritNames)
    {
        choices.pushBack({v, name.cStr()});
    }
    list.pushBack(cycle("Inherit From", weapons::cvar(heldSlot, Key::InheritFrom), ZA_MOVE(choices))
                       .help("Use another weapon's settings (its placement, fingers, hotspots, muzzle, screen): the other "
                             "ammo's model, set once for both. These pages then edit that weapon's."));
    if(weaponOffsetsInherit >= 0)
    {
        list.pushBack(header(inheritTitle.cStr()));
        list.pushBack(action("Stop Inheriting (Copy Them Here)", weaponOffsetsStopInheriting)
                           .help("This weapon gets its own copy of the settings it inherits, to change apart."));
    }
    return slot;
}

// A part of Weapon Offsets with nothing for the empty hand (its own settings: Hand and Grip), or no weapon held.
[[nodiscard]] const char* weaponOffsetsFistNote()
{
    return "The empty hand: its placement is under Hand and Grip";
}

[[nodiscard]] za::Vector<Item> weaponOffsetsNoWeapon(za::Vector<Item>& list, int slot)
{
    if(slot >= 0)
    {
        list.pushBack(info(weaponOffsetsFistNote));
        list.pushBack(open("Hand and Grip", pageIndex(pageWofsHand)));
    }
    return ZA_MOVE(list);
}

// Weapon Offsets: the posing mode, the parts' pages (weaponOffsetsPartPages) and the weapon's own actions.
za::Vector<Item> pageWeaponOffsets()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsMain);
    if(slot < 0)
    {
        return list;
    }
    const auto s = weaponOffsetsSlider;
    const bool fist = slot == weapons::fistSlot(); // the empty hand's "weapon" is the hand model
    const char* shotHelp = weaponOffsetsShotHelp;
    if(!fist)
    {
        // The weapon posing mode (vr_posing.cpp).
        list.pushBackMultiple(
            header("Posing Mode"),
            action("Pose This Weapon", weaponOffsetsPoseWeapon)
                .help("The weapon floats still in front of you: put the weapon hand on it as you want to hold it and "
                      "press the other hand's A/X to set its place in the hand. B/Y undoes, the trigger goes on to the "
                      "hotspots, the stick turns the weapon, the menu button comes back here."),
            cycle("Weapon Hand", vr_pose_weapon_hand, {{1.f, "Main Hand"}, {0.f, "Off Hand (Mirrored)"}})
                .help("The hand that poses the weapon (the settings are shared: the off hand's are mirrored). The other "
                      "hand poses the hotspots and confirms."),
            cycle("Tuning Offsets on Confirm", vr_pose_reset_offsets, {{0.f, "Keep"}, {1.f, "Set to 0"}})
                .help("Keep: the pose is kept with Hand and Weapon Together, Hand Only and Held Hand as they are. "
                      "Set to 0: confirming sets them to 0 (the pose alone places the hand)."),
            s("Shot Pitch (up)", Key::ShotPitch, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
            s("Shot Yaw (left)", Key::ShotYaw, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp)
        );
    }
    list.pushBack(header("Settings"));
    for(const WeaponOffsetsPartPage& part : weaponOffsetsPartPages)
    {
        if(!fist || part.build == pageWofsHand)
        {
            list.pushBack(open(fist ? "The Hand" : part.link, pageIndex(part.build)).help(part.help));
        }
    }
    list.pushBackMultiple(
        open("Weight, Melee and Throwing", pageIndex(pageWeaponWeights))
            .help("Weapon Weights: its mass, balance and length, how it follows your hand, its melee and throw damage, "
                  "its spin thrown."),
        header("This Weapon"),
        action("Print Changes to Console", weaponOffsetsPrint)
            .help("Prints this weapon's offsets that differ from the defaults, ready to be made the shipped defaults."),
        action("Reset This Weapon", weaponOffsetsReset)
            .help("This weapon's offsets (every Weapon Offsets page's) back to their defaults. Its weight settings (Weapon "
                  "Weights) stay.")
    );
    return list;
}

za::Vector<Item> pageWofsHand()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsHand);
    if(slot < 0)
    {
        return list;
    }
    const auto s = weaponOffsetsSlider;
    const bool fist = slot == weapons::fistSlot(); // the empty hand's "weapon" is the hand model
    list.pushBackMultiple(
        header(fist ? "The Hand" : "Weapon in the Hand"),
        s("Offset X (forward)", Key::OffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f)
            .help(fist ? "Moves the drawn hand." :
                         "Where the weapon sits in the hand (the hand is where the controller is, and its fingers wrap "
                         "the weapon's grip). Doesn't change where it aims."),
        s("Offset Y (left)", Key::OffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Offset Z (up)", Key::OffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Pitch", Key::Pitch, -180.f, 180.f, 0.5f, "%.1f").help("How the weapon is turned in the hand."),
        s("Yaw", Key::Yaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Roll", Key::Roll, -180.f, 180.f, 0.5f, "%.1f"),
        s("Scale", Key::Scale, 0.1f, 3.f, 0.01f, "%.2f").extend(0.02f, 10.f),
        cycle("Hide Hand", weapons::cvar(slot, Key::HideHand), {{0.f, "No"}, {1.f, "Yes"}})
    );
    if(!fist)
    {
        // Round 21, third pass: the tuning offsets, applied last, and the aids to see them by.
        const char* frameHelp = "In the controller's aim frame: X forward, Y left, Z up (as for the main hand; the off hand "
                                "mirrored).";
        list.pushBackMultiple(
            header("Tuning Aids"),
            toggle("Show Controller", vr_show_controller)
                .help("Draws each controller as tracked: a Quest 3 controller at its grip, translucent, with its axes (red "
                      "along the handle, green left, blue up), before any offset; the hand's point (yellow) where the "
                      "offsets below move it. Line it up with your real controller under Controller Preview."),
            toggle("Show Controller Laser", vr_show_controller_laser)
                .help("White: where the controller points (Gun Angle included). Red: where the weapon's shots go, from "
                      "its muzzle. Green: the weapon's barrel, as drawn. Turn the weapon (Pitch, Yaw) until green runs "
                      "along red, or the shots (Shot Pitch, Shot Yaw: Muzzle and Sights) until red meets the sights.")
        );
        // After the posing test: the preview's offsets, to match the real controllers (drawControllerPreview).
        const char* previewHelp = "Moves the Show Controller preview (not the hands or weapons) to match your real controller: "
                                  "centimetres along its axes (red: along the handle, green: left, blue: up), degrees.";
        list.pushBackMultiple(
            header("Controller Preview (Show Controller)"),
            slider("Preview X (red)", vr_show_controller_x, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Y (green)", vr_show_controller_y, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Z (blue)", vr_show_controller_z, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Pitch (up)", vr_show_controller_pitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            slider("Preview Yaw (left)", vr_show_controller_yaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            slider("Preview Roll", vr_show_controller_roll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            cycle("Off Hand Preview", vr_show_controller_off_own, {{0.f, "Mirrors the Main Hand's"}, {1.f, "Its Own"}})
                .help("The sliders above are the main hand's; the off hand's preview mirrors them (Y, Yaw and Roll the other "
                      "way), or takes its own.")
        );
        weaponOffsetsPreviewOwn = vr_show_controller_off_own.value != 0.f ? 1 : 0;
        if(weaponOffsetsPreviewOwn)
        {
            list.pushBackMultiple(
                slider("Off Hand X (red)", vr_show_controller_off_x, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Y (green)", vr_show_controller_off_y, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Z (blue)", vr_show_controller_off_z, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Pitch (up)", vr_show_controller_off_pitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
                slider("Off Hand Yaw (left)", vr_show_controller_off_yaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
                slider("Off Hand Roll", vr_show_controller_off_roll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp)
            );
        }
        list.pushBackMultiple(
            header("Hand and Weapon Together"),
            s("Together X (forward)", Key::WholeX, -15.f, 15.f, 0.1f, "%+.1f").extend()
                .help("Moves the hand and the weapon together, last (after the fingers wrap it and the palm fits): the "
                      "muzzle, the aim and the melee move with them. Use it to put the drawn hand back on the controller. "
                      "Units."),
            s("Together Y (left)", Key::WholeY, -15.f, 15.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Together Z (up)", Key::WholeZ, -15.f, 15.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Together Pitch (up)", Key::WholePitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
                .help("Turns the hand and the weapon together about the controller's point: the aim turns too."),
            s("Together Yaw (left)", Key::WholeYaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            s("Together Roll", Key::WholeRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            header("Hand Only"),
            s("Hand X (forward)", Key::HandOnlyX, -10.f, 10.f, 0.1f, "%+.1f").extend()
                .help("Moves the drawn hand alone on the weapon: the weapon, its muzzle and its aim stay; the fingers wrap "
                      "it again. Units."),
            s("Hand Y (left)", Key::HandOnlyY, -10.f, 10.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Hand Z (up)", Key::HandOnlyZ, -10.f, 10.f, 0.1f, "%+.1f").extend().help(frameHelp),
            s("Hand Pitch (up)", Key::HandOnlyPitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
                .help("Turns the drawn hand alone about its palm (a bent wrist straightened): the weapon stays."),
            s("Hand Yaw (left)", Key::HandOnlyYaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            s("Hand Roll", Key::HandOnlyRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
        );
    }
    return list;
}

za::Vector<Item> pageWofsFingers()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsFingers);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it wraps the weapon on its own "
                             "(a share of a full curl).";
    list.pushBackMultiple(
        header("Fingers on the Weapon"),
        cycle("Fingers", weapons::cvar(slot, Key::FingerManual), {{0.f, "Automatic"}, {1.f, "Manual"}})
            .help("Automatic: the fingers wrap the weapon on their own. Manual: they take the curls set below (no "
                  "fitting); the index finger still pulls the trigger.")
    );
    weaponOffsetsManual = weapons::value(slot, Key::FingerManual) >= 0.5f ? 1 : 0;
    if(weaponOffsetsManual)
    {
        const char* curlHelp = "How far this finger is curled: 0 open, 1 a fist (the controller's grip still opens it).";
        list.pushBackMultiple(
            s("Thumb Curl", Key::FingerCurlThumb, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Thumb Across", Key::FingerThumbAcross, 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            s("Index Curl", Key::FingerCurlIndex, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Middle Curl", Key::FingerCurlMiddle, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Ring Curl", Key::FingerCurlRing, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            s("Little Curl", Key::FingerCurlPinky, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp)
        );
    }
    else
    {
        list.pushBack(s("Overlap", Key::GripOverlap, 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the fingers and palm may sink into the weapon: 0 they stop on its surface, 1 a "
                                 "centimetre in."));
    }
    list.pushBackMultiple(
        s("Thumb", Key::FingerThumbBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Index Finger", Key::FingerIndexBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Middle Finger", Key::FingerMiddleBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Ring Finger", Key::FingerRingBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Little Finger", Key::FingerPinkyBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        s("Thumb X (forward)", Key::FingerThumbX, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f).help("Moves the thumb on the hand."),
        s("Thumb Y (palm)", Key::FingerThumbY, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
        s("Thumb Z (up)", Key::FingerThumbZ, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f)
    );
    return list;
}

za::Vector<Item> pageWofsSights()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsSights);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* shotHelp = weaponOffsetsShotHelp;
    // Align Sights to My Aim (vr_sightalign.cpp).
    list.pushBack(header("Align Sights to My Aim"));
    if(sightalign::alignable(weaponOffsetsHand))
    {
        if(sightalign::phase() == sightalign::Phase::Result)
        {
            list.pushBack(action("Apply", sightAlignApply)
                               .help("Turns the hand and the gun together about your fist (Hand and Weapon Together) so "
                                     "that the sights line up in front of your dominant eye, and the shots onto the "
                                     "sight line (Shot Pitch and Yaw). Undo puts the values back."));
            list.pushBack(action("Cancel", sightAlignCancel).help("Nothing changes."));
        }
        else
        {
            list.pushBack(action("Align Sights to My Aim", sightAlignStart)
                               .help("Close your eyes and lower the gun. After three beeps and a high one, raise it as you "
                                     "raise your own gun, and hold still: a click takes it. Lower it and raise it again "
                                     "after each click, until the chime. Then open your eyes: Apply or Cancel. The menu "
                                     "button stops."));
            if(sightalign::canUndo())
            {
                list.pushBack(action("Undo", sightAlignUndo).help("Puts back the values from before Apply, exactly."));
            }
        }
        for(int i = 0; sightalign::statusLine(i); i++)
        {
            list.pushBack(infoLine(sightAlignLine, i));
        }
    }
    else
    {
        list.pushBack(info(sightAlignNone));
    }
    list.pushBackMultiple(
        cycle("Dominant Eye", vr_dominant_eye, {{0.f, "Right"}, {1.f, "Left"}})
            .help("The eye that looks along the sights: pointing at something with both eyes open, the one that stays on "
                  "it when you close the other."),
        cycle("Captures", vr_sight_align_captures, {{3.f, "3"}, {4.f, "4"}, {5.f, "5"}})
            .help("How many times the aim is taken: averaged, one that stands apart from the others dropped."),
        toggle("Show Sight Line", vr_show_sight_line)
            .help("Draws each held gun's sight line: its rear point (yellow), its front one (cyan), and the line through "
                  "them to the wall. Painted sights: the shotguns, the lightning gun; the others a line along the top.")
    );
    list.pushBackMultiple(
        header("Muzzle"),
        s("Muzzle X", Key::MuzzleOffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f).help("Where shots and the muzzle flash start, from the muzzle vertex."),
        s("Muzzle Y", Key::MuzzleOffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Muzzle Z", Key::MuzzleOffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Shot Pitch (up)", Key::ShotPitch, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
        s("Shot Yaw (left)", Key::ShotYaw, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp)
    );
    return list;
}

za::Vector<Item> pageWofsTwoHanded()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsTwoHanded);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    // The two-handed grips: the hotspot being edited.
    const int index = editedHotspot();
    const weapons::Hotspot h = weapons::hotspot(slot, index);
    weaponOffsetsHotspot = index;
    weaponOffsetsHotspotType = static_cast<int>(h.type);
    const auto hk = [&](int field) { return weapons::cvar(slot, weapons::hotspotKey(index, field)); };
    list.pushBackMultiple(
        header("Other Hand's Grips (Hotspots)"),
        cycle("Two-Handed", weapons::cvar(slot, Key::TwoHMode),
            {{0.f, "Allowed"}, {1.f, "Allowed, No Stock"}, {2.f, "Not Allowed"}, {3.f, "Sword"}})
            .help("Whether the other hand may hold this weapon. Not Allowed ignores its hotspots (giving it a "
                  "hotspot allows it). No Stock: never steadied at the shoulder. Sword: the other hand below "
                  "the holding hand or on the blade."),
        cycle("Other Hand Anywhere", weapons::cvar(slot, Key::AnyGripMode),
            {{-1.f, "As Carrying Setting"}, {0.f, "As a Foregrip"}, {1.f, "Support Only"}, {2.f, "Rigid"}})
            .help("The other hand gripping this weapon away from its handle and hotspots (Carrying: Weapons "
                  "Anywhere): both hands aim it from there, it only bears the weight, or the weapon follows both "
                  "hands rigidly. As Carrying Setting: Carrying's Other Hand Anywhere. Two-Handed Not Allowed: "
                  "support only."),
        cycle("Hotspot", vr_weapon_hotspot, {{1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}})
            .help("Where the other hand may hold the weapon: it takes the one nearest it, less its bias. Pick one to edit."),
        cycle("Type", hk(0), {{0.f, "None"}, {1.f, "Grip"}, {2.f, "Blade"}, {3.f, "Cup"}})
            .help("Grip: a point (a foregrip, a pump, a magazine) the hand is drawn on; the two hands aim the weapon. "
                  "Blade: the half-sword grip along the blade. Cup: a two-handed pistol grip, the hand under and "
                  "round the holding hand (it doesn't aim)."),
        action("Pose This Hotspot", weaponOffsetsPoseHotspot)
            .help("Posing mode on this hotspot (a grip if it has no type): the weapon floats, held by the weapon "
                  "hand; put the other hand where it should hold it and press the weapon hand's A/X."),
        action("Pose a New Hotspot", weaponOffsetsPoseNewHotspot).help("The same on the first free hotspot.")
    );
    if(h.type == weapons::HotspotType::Blade)
    {
        list.pushBack(slider("Along the Blade", hk(1), 0.f, 1.f, 0.01f, "%.2f")
                           .help("Where on the blade the grip is centred: a share of the way from the hand to the tip."));
        list.pushBack(slider("Blade Grip Ends At", hk(2), 0.f, 1.05f, 0.01f, "%.2f")
                           .help("How far towards the tip the hand may hold it and slide along it: a share of the way "
                                 "from the hand to the tip (0: just past the tip). The crowbar's ends short of its hook."));
    }
    else
    {
        list.pushBackMultiple(
            slider("Hotspot X", hk(1), -40.f, 40.f, 0.1f, "%.2f")
                .extend(-200.f, 200.f)
                .help(h.type == weapons::HotspotType::Cup
                          ? "Where the helping hand's palm sits, in the weapon's model units: it is taken there (by the "
                            "palm) and drawn there."
                          : "The grip's point, in the weapon's model units."),
            slider("Hotspot Y", hk(2), -40.f, 40.f, 0.1f, "%.2f").extend(-200.f, 200.f),
            slider("Hotspot Z", hk(3), -40.f, 40.f, 0.1f, "%.2f").extend(-200.f, 200.f),
            action("Put It Where the Other Hand Is", weaponOffsetsHotspotAtHand)
                .help("Makes this hotspot a grip at the other hand, as it is now (a cup: at its palm).")
        );
    }
    list.pushBackMultiple(
        slider("Hand Pitch", hk(5), -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f).help("How the hand holding it is turned there."),
        slider("Hand Yaw", hk(6), -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f),
        slider("Hand Roll", hk(7), -180.f, 180.f, 1.f, "%.0f"),
        cycle("Thumb", hk(8), {{0.f, "Wraps round"}, {1.f, "Along the top"}})
            .help("Whether the thumb wraps round it with the fingers, or lies along its top."),
        cycle("Fingers There", hk(16), {{0.f, "Automatic"}, {1.f, "Manual"}})
            .help("Automatic: the hand holding it wraps it on its own. Manual: its fingers take the curls set here.")
    );
    weaponOffsetsHotspotManual = h.manual ? 1 : 0;
    if(h.manual)
    {
        const char* curlHelp = "How far this finger is curled: 0 open, 1 a fist.";
        list.pushBackMultiple(
            slider("Thumb Curl There", hk(17), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Thumb Across There", hk(22), 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            slider("Index Curl There", hk(18), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Middle Curl There", hk(19), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Ring Curl There", hk(20), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Little Curl There", hk(21), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp)
        );
    }
    else
    {
        list.pushBack(slider("Overlap There", hk(9), 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the hand holding it may sink into the weapon: 0 not at all, 1 a centimetre (into "
                                 "the other hand, on a cup: Hand/Gun Calibration's Fit Overlap: Hands)."));
    }
    list.pushBackMultiple(
        slider("Held Hand X (forward)", hk(10), -10.f, 10.f, 0.1f, "%+.1f").extend()
            .help("Moves the hand drawn on this hotspot once it holds it (visual only: where it is taken, and the "
                  "aim, don't change). In the holding hand's aim frame, units; as for the off hand helping."),
        slider("Held Hand Y (left)", hk(11), -10.f, 10.f, 0.1f, "%+.1f").extend(),
        slider("Held Hand Z (up)", hk(12), -10.f, 10.f, 0.1f, "%+.1f").extend(),
        slider("Held Hand Pitch (up)", hk(13), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help("Turns it there, about its palm."),
        slider("Held Hand Yaw (left)", hk(14), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
        slider("Held Hand Roll", hk(15), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f)
    );
    list.pushBackMultiple(
        slider("Bias", hk(4), 0.f, 10.f, 0.1f, "%.1f").extend(0.f, 50.f).help("Units taken off its distance: larger, easier to take than the others."),
        slider("Stickiness", hk(23), 0.5f, 4.f, 0.05f, "%.2fx").extend(0.1f, 20.f)
            .help("Once your other hand holds it, times how far it may go off it (20 units), out of line (Aiming: 2H "
                  "Aiming Threshold) and past the muzzle before it lets go; times Aiming: 2H Grip Stickiness too, more "
                  "while swinging. For heavy weapons swung with two hands."),
        action("Remove This Hotspot", weaponOffsetsHotspotRemove),
        toggle("Show Hotspots", vr_show_weapon_hotspots).help("Marks the held weapons' hotspots (the edited one white).")
    );
    list.pushBackMultiple(
        header("Two-Handed Aim"),
        s("Aim Offset X", Key::TwoHOffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f)
            .help("Moves the point the aim is taken from, in the holding hand's frame (nothing drawn moves)."),
        s("Aim Offset Y", Key::TwoHOffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Aim Offset Z", Key::TwoHOffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
        s("Aim Pitch", Key::TwoHPitch, -180.f, 180.f, 0.5f, "%.1f")
            .help("Turns the two-handed aim (a sword: its blade's direction in the model)."),
        s("Aim Yaw", Key::TwoHYaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Aim Roll", Key::TwoHRoll, -180.f, 180.f, 0.5f, "%.1f")
    );
    return list;
}

// Virtual Stock: how far the virtual stock steadies the page's hand's aim now (twohand::stock).
[[nodiscard]] const char* weaponOffsetsStockReadout(int /* unused */)
{
    za::String& text = readouts.weaponOffsetsStock;
    text = static_cast<int>(vr_2h_mode.value) != 2
               ? za::String("Now: off (2H Aiming below is not Virtual Stock)")
               : za::String(va("Now: two-handed %.0f%%, at the shoulder %.0f%%", 100.f * twohand::transition(weaponOffsetsHand),
                     100.f * twohand::stock(weaponOffsetsHand)));
    return text.cStr();
}

za::Vector<Item> pageWofsStock()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsStock);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* turnHelp = "Turns the aim (and the weapon) while it is steadied at your shoulder, on top of the two-handed aim, "
                           "as far as the stock is engaged: degrees, up, left and clockwise as you hold it (the off hand's "
                           "yaw and roll mirrored). For a weapon that points off where it should when shouldered.";
    list.pushBackMultiple(
        header("Virtual Stock: This Weapon"),
        infoLine(weaponOffsetsStockReadout, 0),
        cycle("Two-Handed", weapons::cvar(slot, Key::TwoHMode),
            {{0.f, "Allowed"}, {1.f, "Allowed, No Stock"}, {2.f, "Not Allowed"}, {3.f, "Sword"}})
            .help("Whether the other hand may hold this weapon; No Stock: never steadied at the shoulder (as on Two-Handed "
                  "and Hotspots)."),
        s("Stock Pitch (up)", Key::StockPitch, -30.f, 30.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(turnHelp),
        s("Stock Yaw (left)", Key::StockYaw, -30.f, 30.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(turnHelp),
        s("Stock Roll (right)", Key::StockRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(turnHelp),
        header("Every Weapon"),
        cycle("2H Aiming", "vr_2h_mode", {{0.f, "Disabled"}, {1.f, "Basic"}, {2.f, "Virtual Stock"}})
            .help("Virtual Stock: a two-handed weapon whose holding hand comes near your shoulder aims from the shoulder "
                  "(as on Aiming)."),
        slider("Stock Factor", "vr_2h_virtual_stock_factor", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder counts in the aim against the hands (as on Aiming)."),
        slider("Stock Distance", "vr_virtual_stock_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f)
            .help("How near the shoulder the holding hand must be (units; Virtual Stock Thresh. on Hotspots)."),
        toggle("Show Virtual Stock", "vr_show_virtual_stock").help("Marks the shoulders the stock is taken at."),
        open("Hotspots (the Shoulders)", pageIndex(pageHotspotSettings))
    );
    return list;
}

za::Vector<Item> pageWofsScreen()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsScreen);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    list.pushBackMultiple(
        header("Ammo Screen"),
        s("Screen X", Key::WpnTextX, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
        s("Screen Y", Key::WpnTextY, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
        s("Screen Z", Key::WpnTextZ, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
        s("Screen Pitch", Key::WpnTextPitch, -180.f, 180.f, 0.5f, "%.1f"),
        s("Screen Yaw", Key::WpnTextYaw, -180.f, 180.f, 0.5f, "%.1f"),
        s("Screen Roll", Key::WpnTextRoll, -180.f, 180.f, 0.5f, "%.1f"),
        s("Screen Scale", Key::WpnTextScale, 0.05f, 3.f, 0.05f, "%.2f").extend(0.01f, 10.f),
        cycle("Ammo Screen", weapons::cvar(slot, Key::WpnTextMode), {{1.f, "Shown"}, {0.f, "Hidden"}})
            .help("Whether this weapon shows its ammunition screen (every weapon's: Screens, Weapon Ammo Screen).")
    );
    return list;
}

za::Vector<Item> pageWofsHolstered()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsHolstered);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    // The weapon in a holster (weapons::holsteredPose), per kind of holster: the page's weapon drawn in both holsters
    // of that kind while one of these is chosen (view: setupHolsters, menu::holsterPreview).
    const weapons::HolsterKind kind = editedHolster();
    weaponOffsetsHolster = static_cast<int>(kind);
    const auto hs = [&](const char* label, int field, float min, float max, float step, const char* format) {
        return slider(label, weapons::cvar(slot, weapons::holsteredKey(kind, field)), min, max, step, format);
    };
    const char* moveHelp = "Moves this weapon in the holster (units): X off your body (negative: into it), Y outwards, "
                           "away from your middle, Z up. The left holster mirrors the right. Only how it is drawn: "
                           "where you reach for the holster doesn't change.";
    const char* turnHelp = "Turns this weapon in the holster about its grip, on top of the holster's own turn (Hotspots): "
                           "Pitch tips its top off your body, Yaw turns it outwards, Roll tips its top outwards (a "
                           "hanging gun's muzzle goes the other way). The left holster mirrors the right.";
    list.pushBackMultiple(
        header("Holstered"),
        cycle("Holster", vr_weapon_holster, {{1.f, "Hip"}, {2.f, "Upper (Chest)"}, {3.f, "Shoulder (Back)"}})
            .help("The holsters whose pose of this weapon the sliders below edit: a weapon lies differently on the hips, "
                  "the chest and the back, so each has its own."),
        action("Pose in This Holster", weaponOffsetsPoseHolster)
            .help("Posing mode in these holsters: the right one floats in front of you, this weapon in it as the "
                  "sliders below put it. Take the weapon with either grip, carry it, let go where it should sit (it "
                  "stays there); A/X sets it (B/Y undoes), the menu button sets it and comes back here. The trigger "
                  "goes on to the next kind of holster, the stick turns it all."),
        toggle("Preview in Holster", vr_weapon_holster_preview)
            .help("While a setting of this section is chosen, this weapon is drawn in both holsters of that kind (in place "
                  "of what they hold): look down at them, or at the body preview, as you tune it."),
        hs("Holstered X (off body)", 0, -10.f, 10.f, 0.1f, "%+.1f").extend().help(moveHelp),
        hs("Holstered Y (outwards)", 1, -10.f, 10.f, 0.1f, "%+.1f").extend().help(moveHelp),
        hs("Holstered Z (up)", 2, -10.f, 10.f, 0.1f, "%+.1f").extend().help(moveHelp),
        hs("Holstered Pitch", 3, -90.f, 90.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(turnHelp),
        hs("Holstered Yaw", 4, -90.f, 90.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(turnHelp),
        hs("Holstered Roll", 5, -90.f, 90.f, 1.f, "%+.0f deg").extend(-180.f, 180.f).help(turnHelp),
        action("Holstered Back to 0", weaponOffsetsHolsteredReset)
            .help("This weapon's pose in these holsters back to 0: as the holster alone places it.")
    );
    return list;
}

za::Vector<Item> pageWofsEffects()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsEffects);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    list.pushBackMultiple(
        // The programmatic weapon effects (vr_weaponfx.cpp; their global settings: Weapon Effects).
        header("Effects"),
        cycle("Recoil", weapons::cvar(slot, Key::Recoil), {{0.f, "Off (the model's own)"}, {1.f, "On"}})
            .help("Each shot kicks the drawn weapon back and tips it up, then it comes back to rest: for a model without "
                  "a recoil animation of its own. Looks only: the aim and the shots don't move."),
        s("Recoil Strength", Key::RecoilStrength, 0.f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Times Weapon Effects' Kick Back and Muzzle Rise."),
        s("Recoil Return Time", Key::RecoilTime, 0.03f, 0.6f, 0.01f, "%.2f s").extend(0.02f, 3.f)
            .help("How long a shot's kick takes to come back to rest."),
        cycle("Muzzle Flash", weapons::cvar(slot, Key::Flash), {{0.f, "Off (the model's own)"}, {1.f, "On"}})
            .help("The shotgun's flash at this weapon's muzzle (its Muzzle settings) for a moment at each shot, following "
                  "the gun; gone when it leaves your hand."),
        s("Flash Size", Key::FlashSize, 0.2f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f).help("Times the shotgun's flash."),
        s("Flash Time", Key::FlashTime, 0.02f, 0.2f, 0.01f, "%.2f s").extend(0.01f, 1.f).help("How long it shows."),
        cycle("Tracers", weapons::cvar(slot, Key::Tracers), {{0.f, "As Weapon Effects"}, {1.f, "Never"}, {2.f, "Always"}})
            .help("Bullet tracers of this weapon's hitscan shots (the shotguns, the burst rifle): as Weapon Effects' "
                  "Bullet Tracers, never, or always."),
        s("Tracer Speed", Key::TracerSpeed, 0.1f, 5.f, 0.05f, "%.2fx").extend(0.01f, 20.f).help("Times Weapon Effects'."),
        s("Tracer Length", Key::TracerLength, 0.1f, 5.f, 0.05f, "%.2fx").extend(0.01f, 20.f).help("Times Weapon Effects'."),
        s("Tracer Thickness", Key::TracerWidth, 0.1f, 5.f, 0.05f, "%.2fx").extend(0.01f, 20.f).help("Times Weapon Effects'."),
        s("Tracer Chance", Key::TracerChance, 0.f, 4.f, 0.05f, "%.2fx").extend(0.f, 20.f)
            .help("Times Weapon Effects' Chance a Pellet (at most every pellet)."),
        cycle("Tracer Colour", weapons::cvar(slot, Key::TracerOwnColour), {{0.f, "Weapon Effects'"}, {1.f, "Its Own"}})
            .help("Its Own: the Red, Green and Blue below."),
        s("Tracer Red", Key::TracerRed, 0.f, 1.f, 0.05f, "%.2f"),
        s("Tracer Green", Key::TracerGreen, 0.f, 1.f, 0.05f, "%.2f"),
        s("Tracer Blue", Key::TracerBlue, 0.f, 1.f, 0.05f, "%.2f"),
        action("Test the Effects", weaponOffsetsTestEffects)
            .help("This weapon kicks and flashes as if it fired, with 3 tracers (no shot: vr_weaponfx_test)."),
        open("Weapon Effects (All Weapons)", pageIndex(pageWeaponEffects))
    );
    return list;
}

za::Vector<Item> pageWofsFlashlight()
{
    using weapons::Key;
    za::Vector<Item> list;
    const int slot = weaponOffsetsBegin(list, WofsFlashlight);
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return weaponOffsetsNoWeapon(list, slot);
    }
    const auto s = weaponOffsetsSlider;
    const char* moveHelp = "Moves the flashlight clipped on this weapon (metres): forward along the barrel, up, and out away "
                           "from your body; on top of every weapon's On Gun Forward, Up and Out and the place fitted to the "
                           "model. The zone that clips it on (B or Y, or letting go by the gun) moves with it.";
    const char* turnHelp = "Turns the flashlight clipped on this weapon about its middle (degrees): Pitch tips the beam up, Yaw "
                           "turns it out away from your body, Roll tips its top out (the off hand's mirrored).";
    list.pushBackMultiple(
        header("Flashlight on This Weapon"),
        cycle("Flashlight Can Clip On", weapons::cvar(slot, Key::TorchClip), {{0.f, "No"}, {1.f, "Yes"}})
            .help("Whether the flashlight clips on this weapon at all: its zone, B or Y by it, and letting go by it (Clip on Gun "
                  "When Let Go). Off by default for the melee weapons (the axe, Mjolnir, the swords, the chainsaw, the crowbar)."),
        action("Clip the Flashlight on It", weaponOffsetsClipTorch)
            .help("Puts the flashlight (switched on) on this weapon now, to tune it in place (vr_flashlight_clip_gun)."),
        toggle("Show the Flashlight's Place", vr_flashlight_mount_preview)
            .help("While this page is shown: the flashlight's outline and its beam's line on this weapon (cyan) and the zone "
                  "that clips it on (orange), wherever the flashlight is. The flashlight must be on (Flashlight)."),
        s("Flashlight Forward", Key::TorchForward, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-1.f, 1.f).help(moveHelp),
        s("Flashlight Up", Key::TorchUp, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-1.f, 1.f).help(moveHelp),
        s("Flashlight Out", Key::TorchOut, -0.2f, 0.2f, 0.005f, "%+.3f m").extend(-1.f, 1.f).help(moveHelp),
        s("Flashlight Pitch (up)", Key::TorchPitch, -45.f, 45.f, 0.5f, "%+.1f deg").extend(-180.f, 180.f).help(turnHelp),
        s("Flashlight Yaw (out)", Key::TorchYaw, -45.f, 45.f, 0.5f, "%+.1f deg").extend(-180.f, 180.f).help(turnHelp),
        s("Flashlight Roll (out)", Key::TorchRoll, -90.f, 90.f, 0.5f, "%+.1f deg").extend(-180.f, 180.f).help(turnHelp),
        action("Flashlight Back to 0", weaponOffsetsTorchReset).help("This weapon's own flashlight place back to 0: as every "
                                                                     "weapon's."),
        open("On a Gun or Head (Every Weapon)", pageIndex(pageFlashlightMounts))
            .help("Every weapon's place for the flashlight and the zone that clips it on, its size (Flashlight).")
    );
    return list;
}

int page = PageMain;

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
NavStack nav;
bool navReturning = false; // leaving the VR pages by Back for an outside menu (VR_NavEntered: not a new way in)
int navJumpPending = -1;    // the outside menu a jump (VR_NavJump) is opening

[[nodiscard]] constexpr int outsidePlace(int state)
{
    return -1 - state;
}

[[nodiscard]] int navTop()
{
    return nav.count > 0 ? nav.places[nav.count - 1] : -1000;
}

void navReset()
{
    nav.count = 0;
}

// `place` on top: back to it where it already is on the stack (what was above it dropped), else added.
void navPush(int place)
{
    for(int i = nav.count - 1; i >= 0; i--)
    {
        if(nav.places[i] == place)
        {
            nav.count = i + 1;
            return;
        }
    }
    if(nav.count == NavStack::capacity)
    {
        for(int i = 1; i < nav.count; i++)
        {
            nav.places[i - 1] = nav.places[i];
        }
        nav.count--;
    }
    nav.places[nav.count++] = place;
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
    if(navTop() != page)
    {
        navPush(page);
    }
}
int cursors[pageCount]{};
int scrolls[pageCount]{};

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

RowAnchor leftAnchors[pageCount]; // each page's position as last shown (invalid: at the top)
int visits[pageCount]{};          // when each page was last shown (a count; 0: not since the start)
int visitCount = 0;
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
    if(positionsLoaded && (visitCount > 0 || positionsParsed == vr_menu_positions.string))
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

// Reset This Page (the footer): armed by a first press, done by a second within 3 seconds.
int resetArmedPage = -1;
double resetArmedTime = 0.0;

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
        list.pushBack(action(resetArmedPage == page ? "Press Again to Reset" : "Reset This Page", resetThisPage)
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
    if(page == PageMain)
    {
        syncWrappers(); // Turning Mode, Move Towards, the hands: as their settings are now
        if(mainPageSnap >= 0 && mainPageSnap != (vr_snap_turn.value > 0.f ? 1 : 0))
        {
            done[page] = false; // Snap Angle shown, or Turn Speed
        }
        if(resetAllArmed && realtime - resetAllArmedTime > 3.0)
        {
            resetAllArmed = false; // Reset All to Defaults not pressed again in time
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
    if(resetArmedPage == page && realtime - resetArmedTime > 3.0)
    {
        resetArmedPage = -1; // Reset This Page not pressed again in time
        done[page] = false;
    }
    // (A check above: done[page] = false, and the page is built anew here, its selected row kept.)
    if(!done[page])
    {
        // The row selected: in the list about to be replaced; on the first build, where the last start
        // left the page (vr_menu_positions).
        loadPositions();
        RowAnchor anchor = anchorOf(built[page], cursors[page], scrolls[page]);
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
        int& cursor = cursors[page];
        if(const int row = anchor.valid ? findRow(built[page], anchor) : -1; row >= 0)
        {
            cursor = row;
            scrolls[page] = q_max(row - anchor.line, 0);
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
                cursors[page] = i;
                break;
            }
        }
    }
    return built[page];
}

bool sliderGrab = false; // a slider follows the mouse while its button is held
float sliderGrabHold = -1.f; // grabbed on a thumb pinned at an end (a value past it): where, kept until the mouse moves off
bool scrollGrab = false; // the scrollbar likewise

constexpr int midPos = 204; // as Ironwail's OPTIONS_MIDPOS

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

constexpr int helpMinLines = 4;     // the help's box: four lines at least (Search's and the Map Library's)
constexpr int helpMaxLines = 7;     // and at most; a longer help turns pages in it (drawHelp)
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
// again when the page, its rows or the line's width change, not every frame.
struct HelpBox
{
    int page{-1};
    int build{-1};
    int columns{0};
    int lines{helpMinLines};
};
HelpBox helpBox;

[[nodiscard]] int helpBoxLines()
{
    const int columns = helpColumns();
    if(helpBox.page != page || helpBox.build != builds[page] || helpBox.columns != columns)
    {
        helpBox.page = page;
        helpBox.build = builds[page];
        helpBox.columns = columns;
        int longest = 0;
        for(const Item& item : menuPages.built[page])
        {
            const char* help = itemHelp(item);
            longest = help ? q_max(longest, wrapHelp(help, columns)) : longest;
        }
        helpBox.lines = CLAMP(helpMinLines, longest, helpMaxLines);
    }
    return helpBox.lines;
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
    if(float labelLeft, labelTop; menuui::versionLabelClearance(labelLeft, labelTop) && pageRight(pages[page].build) > labelLeft)
    {
        bottom = q_min(bottom, static_cast<int>(za::floor(labelTop)));
    }
    // Under the title, or below the corner's buttons where they are over the rows (not left of them: a narrow panel),
    // and below the status box in the top right corner where the page reaches under it (Search and the Map Library,
    // from x 12 440 and 460 across; the other pages to their scrollbar).
    int listTop = menuui::toolbarBeside() ? top + 36 : q_max(top + 36, static_cast<int>(za::ceil(menuui::toolbarBottom())) + 2);
    const auto build = pages[page].build;
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
// characters at most). Kept per page and build.
struct RowsLeft
{
    int page{-1};
    int build{-1};
    int left{0};
};
RowsLeft rowsLeft;

[[nodiscard]] int pageRowsLeft()
{
    if(rowsLeft.page != page || rowsLeft.build != builds[page])
    {
        rowsLeft.page = page;
        rowsLeft.build = builds[page];
        int left = 0;
        for(const Item& item : menuPages.built[page])
        {
            const int len = item.label ? static_cast<int>(strlen(item.label)) : 0;
            const int x = item.kind == Item::Header ? (320 - 8 * len) / 2
                          : item.kind == Item::Info ? 0
                                                    : midPos - 28 - 8 * len;
            left = q_min(left, x);
        }
        rowsLeft.left = left;
    }
    return rowsLeft.left;
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
    return rowsFrom(list, scrolls[page]);
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
    int& cursor = cursors[page];
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
    leftAnchors[p] = cursors[p] == firstSelectable(list) && scrolls[p] == 0 ? RowAnchor{} : anchorOf(list, cursors[p], scrolls[p]);
}

// Shows `target`, with its cursor on a setting: where it was left (pages built anew as they are
// shown, for what the hand holds now, keep the same row by its label).
void closeDropDown(); // (the drop-down lists, below: a page shown closes the one open)

// The page a Search result opened (vr_menu_search.inc): Back from it returns to the results (VR Settings' Back too);
// -1 once elsewhere.
void showPage(int target)
{
    closeDropDown();
    page = target;
    weightPageShown(pages[page].build); // a weight page: what is in the hand now
    if(weaponOffsetsPage(pages[page].build))
    {
        weaponOffsetsStale = true; // the weapon in hand now
    }
    if(pages[page].build == pageReviewTakes)
    {
        motion::review::invalidate(); // takes recorded, evaluated or moved since
    }
    // Built again each time it is shown (its row kept): rows that depend on anything not checked in items() (what is
    // changed now: Changed Settings; a state a builder reads) are as they are now, not as when the page was last built
    // (by Search, a menu path or Changed Settings, which build every page).
    menuPages.done[page] = false;
    const auto& list = items(page);
    if(!selectable(list[cursors[page]]))
    {
        cursors[page] = firstSelectable(list);
    }
    noteLeft(page, list);
    visits[page] = ++visitCount;
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
    if(!resetAllArmed || realtime - resetAllArmedTime > 3.0)
    {
        resetAllArmed = true;
        resetAllArmedTime = realtime;
        menuPages.done[PageMain] = false; // its label: Press Again to Reset All
        return;
    }
    resetAllArmed = false;
    menuPages.done[PageMain] = false;
    za::Vector<cvar_t*> reset;
    for(const Item& item : items(PageMain))
    {
        if(item.cvar && item.kind != Item::Action && !wrapperCvar(*item.cvar))
        {
            reset.pushBack(item.cvar);
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
    if(resetArmedPage != page || realtime - resetArmedTime > 3.0)
    {
        resetArmedPage = page;
        resetArmedTime = realtime;
        menuPages.done[page] = false; // its label: Press Again to Reset
        return;
    }
    resetArmedPage = -1;
    int n = 0;
    for(const Item& item : items(page))
    {
        if(item.cvar && item.kind != Item::Action && changedSetting(*item.cvar) && !serverrules::locked(item.cvar))
        {
            Cvar_SetQuick(item.cvar, item.cvar->default_string);
            n++;
        }
    }
    Con_Printf("VR: %s: %d setting%s back to %s default\n", pages[page].title, n, n == 1 ? "" : "s", n == 1 ? "its" : "their");
    menuPages.done[page] = false;
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
    navReturning = true;
    menuui::openMenu(state);
    navReturning = false;
}

// Back from the page shown (a key, a button): where the player came from (NavStack), else up the tree.
void goBack()
{
    navSyncTop();
    if(nav.count >= 2)
    {
        nav.count--;
        const int dest = navTop();
        if(dest >= 0)
        {
            showPage(dest);
            S_LocalSound("misc/menu2.wav");
        }
        else
        {
            leaveTo(-1 - dest);
        }
        return;
    }
    navReset();
    if(page == PageMain)
    {
        M_Menu_Options_f(); // (its sound as it is drawn)
        return;
    }
    const int dest = homeOf(page);
    navPush(dest);
    showPage(dest);
    S_LocalSound("misc/menu2.wav");
}

// Where Back goes from the page shown (menu_vr pos): a page's number, or -1 with `outside` the menu (m_none: Options).
[[nodiscard]] int backTarget(int& outside)
{
    outside = m_none;
    if(nav.count >= 2 && navTop() == page)
    {
        const int dest = nav.places[nav.count - 2];
        if(dest < 0)
        {
            outside = -1 - dest;
            return -1;
        }
        return dest;
    }
    return page == PageMain ? -1 : homeOf(page);
}

// A setting's value as shown: a server rule's is the remote server's (vr_serverrules.cpp), the rest their own.
[[nodiscard]] float valueOf(const Item& item)
{
    return serverrules::shown(item.cvar);
}

// A server rule while connected to a remote server: shown, dimmed, not changed from here.
[[nodiscard]] bool lockedItem(const Item& item)
{
    return (item.kind == Item::Slider || item.kind == Item::Cycle) && (item.unavailable || serverrules::locked(item.cvar));
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
// it is held (2x after a second, then 5x, then 10x), on multiples of those steps.
struct SliderHold
{
    const cvar_t* endCvar = nullptr; // stopped at a bar's end: which, which way, since when
    int endDir = 0;
    double endSince = 0.0;
    double outsideSince = 0.0; // held past the ends since
};
SliderHold sliderHold; // (stepSlider: the menu's keys)

// The sliders' fine adjustment: while either grip is held in the headset (a grip does nothing else in the menus), or
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
    const cvar_t*& endCvar = sliderHold.endCvar;
    int& endDir = sliderHold.endDir;
    double& endSince = sliderHold.endSince;
    double& outsideSince = sliderHold.outsideSince;

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

// The list row under the mouse, or -1.
[[nodiscard]] int rowAt(float cy)
{
    const auto& list = items(page);
    const float yrel = cy - static_cast<float>(layout().listTop);
    const int first = scrolls[page];
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
// have: its thumb's top (pixels below listTop) and height (rows). False when the list fits.
int scrollbarX = midPos + 188; // where it was drawn

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
    y = static_cast<int>(CLAMP(0, scrolls[page], most) * 8 / static_cast<float>(most) * (track - height) + 0.5f);
    return true;
}

// The cursor moved onto the nearest visible setting if the list scrolled it out of view.
void keepCursorVisible()
{
    const auto& list = items(page);
    const int rows = visibleRows(list);
    const int scroll = scrolls[page];
    int& cursor = cursors[page];
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
    const auto& list = items(page);
    const Layout l = layout();
    const int first = scrolls[page];
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
    const auto& list = items(page);
    int y, height;
    if(!scrollbar(list, y, height))
    {
        return;
    }
    const int most = maxScroll(list);
    const float yrel = cy - layout().listTop - height * 4.f;
    const int range = q_max((trackRows(list) - height) * 8, 1);
    scrolls[page] = CLAMP(0, static_cast<int>(yrel * most / range + 0.5f), most);
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
DropDown dropDown;

constexpr int dropDownBar = 16; // the scrollbar's column, inside the box's right edge

[[nodiscard]] bool opensDropDown(const Item& item)
{
    return item.kind == Item::Cycle && vr_menu_dropdown.value >= 2.f && item.cvar &&
           static_cast<int>(item.choices.size()) >= static_cast<int>(vr_menu_dropdown.value) && !isToggle(item);
}

void closeDropDown()
{
    dropDown.open = false;
    dropDown.scrollGrab = false;
}

// The open list's row, or null (closed now if its page or row is gone, or the corner's buttons took the selection).
[[nodiscard]] const Item* dropDownItem()
{
    if(!dropDown.open)
    {
        return nullptr;
    }
    if(m_state != m_vr || page != dropDown.page || menuui::toolbarFocused())
    {
        closeDropDown();
        return nullptr;
    }
    const auto& list = items(page);
    if(dropDown.row >= static_cast<int>(list.size()))
    {
        closeDropDown();
        return nullptr;
    }
    const Item& item = list[dropDown.row];
    if(item.kind != Item::Cycle || item.cvar != dropDown.cvar || static_cast<int>(item.choices.size()) != dropDown.count)
    {
        closeDropDown();
        return nullptr;
    }
    return &item;
}

void dropDownShowHover()
{
    if(dropDown.hover < dropDown.scroll)
    {
        dropDown.scroll = dropDown.hover;
    }
    else if(dropDown.hover >= dropDown.scroll + dropDown.rows)
    {
        dropDown.scroll = dropDown.hover - dropDown.rows + 1;
    }
    dropDown.scroll = CLAMP(0, dropDown.scroll, q_max(dropDown.count - dropDown.rows, 0));
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

    DropDown& d = dropDown;
    d.open = true;
    d.scrollGrab = false;
    d.page = page;
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
    const int rowY = l.listTop + rowTop(items(page), scrolls[page], row);
    d.top = rowY - (cur - d.scroll) * 8;
    d.top = CLAMP(l.top + 12, d.top, q_max(l.top + 12, l.bottom - 12 - d.rows * 8));
    S_LocalSound("misc/menu3.wav");
}

// Sets the hovered choice and closes the list.
void pickDropDown(const Item& item)
{
    const float v = item.choices[CLAMP(0, dropDown.hover, dropDown.count - 1)].value;
    cvar_t* const cvar = item.cvar;
    closeDropDown();
    Cvar_SetValueQuick(cvar, v); // (`item` may be gone after this: a page rebuilt by the setting's change)
    S_LocalSound("misc/menu3.wav");
}

[[nodiscard]] bool insideDropDown(float cx, float cy)
{
    const DropDown& d = dropDown;
    return cx >= d.x - 6 && cx < d.x + d.width + 6 && cy >= d.top - 6 && cy < d.top + d.rows * 8 + 6;
}

[[nodiscard]] bool onDropDownBar(float cx, float cy)
{
    const DropDown& d = dropDown;
    return d.count > d.rows && insideDropDown(cx, cy) && cx >= d.x + d.width - dropDownBar;
}

// The choice under the mouse, or -1.
[[nodiscard]] int dropDownChoiceAt(float cx, float cy)
{
    const DropDown& d = dropDown;
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
    const DropDown& d = dropDown;
    height = q_max(static_cast<int>(d.rows * d.rows / static_cast<float>(d.count) + 0.5f), 2);
    y = static_cast<int>(d.scroll * 8 / static_cast<float>(d.count - d.rows) * (d.rows - height) + 0.5f);
}

void dropDownScrollTo(float cy)
{
    DropDown& d = dropDown;
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
    const DropDown& d = dropDown;
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
    DropDown& d = dropDown;
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
    DropDown& d = dropDown;
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

    if(item.wide)
    {
        // A row of a list: its text across the width, white while selected.
        char text[41];
        q_strlcpy(text, item.infoArg ? item.infoArg(item.arg) : item.label, sizeof(text));
        const bool dim = item.dimArg && item.dimArg(item.arg);
        if(selected && item.dimArg)
        {
            VR_MenuDrawHighlight(4, y); // (the Checklist's: each line of the item picked; the VR style's only)
        }
        if(dim)
        {
            GL_PushCanvasColor(1.f, 1.f, 1.f, 0.4f);
        }
        if(selected)
        {
            M_PrintWhite(0, y, text);
        }
        else
        {
            M_Print(0, y, text);
        }
        if(dim)
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
        M_PrintWhite(q_max(labelX - 10, 0), y, "*"); // changed from its default (Changed Settings lists them)
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

// The help shown, and since when (its pages turn from the first; a new text starts over).
struct HelpPaging
{
    za::U32 hash{0};
    double since{0.0};
    int shown{-1}; // the page of it shown (for the developer line when it turns)
};
HelpPaging helpPaging;

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
        if(hash != helpPaging.hash)
        {
            helpPaging.hash = hash;
            helpPaging.since = realtime;
            helpPaging.shown = -1;
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
        double t = fmod(realtime - helpPaging.since, cycle);
        int shown = 0;
        while(shown < pageCount - 1 && t >= durations[shown])
        {
            t -= durations[shown];
            shown++;
        }
        first = shown * box;
        if(shown != helpPaging.shown)
        {
            helpPaging.shown = shown;
            Con_DPrintf("menu help: part %d of %d (%d lines, %.1f s)\n", shown + 1, pageCount, lines, realtime - helpPaging.since);
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
    const int was = page;
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
    const int was = page;
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

int qvr::menu::bodyCalibrationPage()
{
    return pageIndex(pageBodyCalibration);
}

int qvr::menu::retroOverridePage()
{
    return pageIndex(pageRetroOverride);
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
    navJumpPending = state;
}

// An outside menu opened (Ironwail's Levels; `previous` the menu shown before): by a jump or by Back into it, its place on
// the stack kept; from its own way in (Single Player, a mod's), the stack started again (its Back its own).
extern "C" void VR_NavEntered(int state, int previous)
{
    const bool kept = navJumpPending == state || navReturning || previous == m_skill || previous == state;
    navJumpPending = -1;
    if(!kept || navTop() != outsidePlace(state))
    {
        navReset();
    }
}

// Back from an outside menu entered by a jump: where it came from (nonzero), else 0 (its own Back).
extern "C" int VR_NavBack(int state)
{
    if(nav.count < 2 || navTop() != outsidePlace(state))
    {
        navReset();
        return 0;
    }
    nav.count--;
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
                Con_Printf("menu_vr pos: rows from y %d, %d shown\n", layout().listTop, visibleRows(items(page)));
            }
            float bx0, bx1, by0, by1;
            menuui::bannerRect(bx0, bx1, by0, by1);
            if(!menuui::active() && bx1 >= bx0)
            {
                Con_Printf("menu_vr pos: banner x %.0f..%.0f, y %.0f..%.0f (text from x %d)\n", bx0, bx1, by0, by1,
                    m_state == m_vr ? static_cast<int>(menu::contentLeft()) : M_TextLeft());
            }
            menuui::printVersionLabel();
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
                menuui::printLaser();
                return;
            }
            Con_Printf("menu_vr pos: menu %d (%s)%s%s\n", static_cast<int>(m_state), name, corner,
                key_dest == key_menu && m_state == m_singleplayer ? va(", row %d", m_singleplayer_cursor) : "");
            return;
        }
        const auto& list = items(page);
        const int cursor = cursors[page];
        int outside = m_none;
        const int back = backTarget(outside);
        Con_Printf("menu_vr pos: page %d \"%s\" (back to %d%s), row %d \"%s\" under \"%s\", scroll %d of %d rows%s\n", page,
            pages[page].title, back, outside != m_none ? va(", menu %d", outside) : "", cursor, rowLabel(list[cursor]), rowSection(list, cursor),
            scrolls[page], static_cast<int>(list.size()), corner);
        if(cursor < static_cast<int>(list.size()) && list[cursor].cvar && list[cursor].kind == Item::Slider)
        {
            char shown[64];
            sliderText(list[cursor], list[cursor].cvar->value, shown, sizeof(shown));
            Con_Printf("menu_vr pos: slider %s \"%s\" (shown %s), step %g%s\n", list[cursor].cvar->name,
                list[cursor].cvar->string, shown, sliderStep(list[cursor]), fineHeld() ? " (fine)" : "");
        }
        if(const Item* open = dropDownItem())
        {
            const DropDown& d = dropDown;
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
            const auto& list = items(page);
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
                cursors[page] = row;
            }
        }
    }
}

bool qvr::menu::holsterPreview(int& hand, int& kind)
{
    if(m_state != m_vr || pages[page].build != pageWofsHolstered || vr_weapon_holster_preview.value == 0.f ||
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
    if(m_state != m_vr || pages[page].build != pageWofsFlashlight || vr_flashlight_mount_preview.value == 0.f ||
        weaponOffsetsSlot < 0 || weaponOffsetsSlot == weapons::fistSlot() || weapons::heldSlot(weaponOffsetsHand) != weaponOffsetsHeldSlot)
    {
        return false; // not on the page, off, the empty hand, or the hand holds another weapon now
    }
    hand = weaponOffsetsHand;
    return true;
}

int qvr::menu::currentPage()
{
    return page;
}

const cvar_t* qvr::menu::selectedSetting()
{
    if(m_state != m_vr)
    {
        return nullptr;
    }
    const auto& list = items(page);
    const int cursor = cursors[page];
    return cursor >= 0 && cursor < static_cast<int>(list.size()) ? list[cursor].cvar : nullptr;
}

void qvr::menu::jumpToAdvanced()
{
    if(menuLevel() < LevelAdvanced)
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(LevelAdvanced)); // (the corner's Advanced VR: the pages it lists)
    }
    if(m_state == m_vr && page == PageAdvanced)
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
    const auto build = pages[page].build;
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
    const auto build = pages[page].build;
    const Layout l = layout();
    if(build == pageConsole || build == pageSearch || build == pageMaps)
    {
        return y < static_cast<float>(l.bottom) ? pageRight(build) : -1e9f; // (their keyboards and buttons down to the bottom)
    }
    // The rows down to the help's box (or the bottom), as far right as their scrollbar's box (scrollbarX + 12); the help
    // centred at its widest (helpColumns), its paging bar right of it (drawHelp).
    const za::Vector<Item>& list = menuPages.built[page];
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

bool qvr::menu::developerLevel()
{
    return menuLevel() >= LevelDeveloper;
}

void qvr::menu::jumpToChecklist()
{
    const int target = pageIndex(pageChecklist);
    checklist::refresh(true);
    if(m_state == m_vr && page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(target);
}

void qvr::menu::jumpToSettings()
{
    if(m_state == m_vr && page == PageMain)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(PageMain);
}

void qvr::menu::jumpToRelighting()
{
    const int target = pageIndex(pageGraphicsRelighting);
    if(menuLevel() < pages[target].level)
    {
        Cvar_SetValueQuick(&vr_menu_level, static_cast<float>(pages[target].level)); // (as the corner's Advanced VR)
    }
    if(m_state == m_vr && page == target)
    {
        S_LocalSound("misc/menu1.wav");
        return;
    }
    openFromAnywhere(target); // (Back: where it was pressed)
}

void qvr::menu::selectEnd(int dir)
{
    if(m_state == m_vr && pages[page].build == pageSearch)
    {
        search.focusKey = 0; // from the corner's buttons: the keys
        return;
    }
    if(m_state == m_vr && pages[page].build == pageConsole)
    {
        consolePage.focusKey = 0;
        return;
    }
    if(m_state == m_vr && pages[page].build == pageMaps)
    {
        mapPage.focusKey = 0; // from the corner's buttons: the keys
        return;
    }
    if(m_state == m_vr)
    {
        const auto& list = items(page);
        cursors[page] = dir > 0 ? firstSelectable(list) : lastSelectable(list);
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
    if(m_state != m_vr || sliderGrab || scrollGrab)
    {
        return false;
    }
    if(pages[page].build == pageConsole)
    {
        consoleScroll(-rows); // (the stick down: newer)
        return true;
    }
    if(pages[page].build == pageMaps)
    {
        mapPage.resultScroll -= rows; // (the stick down: the next packages)
        mapsScrollTo(-1);
        return true;
    }
    if(dropDownItem())
    {
        // The stick moves the open list's highlight, the list scrolling with it.
        dropDown.hover = CLAMP(0, dropDown.hover + rows, dropDown.count - 1);
        dropDownShowHover();
        return true;
    }
    const auto& list = items(page);
    const int most = maxScroll(list);
    if(most <= 0)
    {
        return false;
    }
    scrolls[page] = CLAMP(0, scrolls[page] + rows, most);
    keepCursorVisible();
    return true;
}

extern "C" void VR_MenuSavePositions()
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
        if(visits[p] > 0)
        {
            order.pushBack(p);
        }
    }
    za::stableSort(order.begin(), order.end(), [](int a, int b) { return visits[a] > visits[b]; }); // (stable: ties in the order std::sort gave a few)
    for(const int p : order)
    {
        add(p, leftAnchors[p]);
    }
    order.clear();
    for(int p = 0; p < pageCount; p++)
    {
        if(visits[p] == 0 && savedAnchors[p].valid)
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

namespace
{
// Where VR_Menu_Draw last noted the page to be (vr_menu_positions is written on a change only).
struct NotedPlace
{
    int page = -1, cursor = -1, scroll = -1, build = -1;
};
NotedPlace notedPlace;
} // namespace

extern "C" void VR_Menu_Draw()
{
    syncPresets(); // (Comfort, Handedness: the choice the settings match now)
    if(pages[page].build == pageSearch)
    {
        drawSearch();
        return;
    }
    if(pages[page].build == pageConsole)
    {
        drawConsolePage();
        return;
    }
    if(pages[page].build == pageMaps)
    {
        drawMapsPage();
        return;
    }
    const auto& list = items(page);
    int& cursor = cursors[page];
    int& scroll = scrolls[page];

    if(!keydown[K_MOUSE1])
    {
        sliderGrab = scrollGrab = false;
    }

    const Layout l = layout();
    // (Below a flat screen's row of icons where that reaches over it: a window narrower than 16:9.)
    const bool underRow = menuui::toolbarRow() && menuui::toolbarRight() > 16.f;
    M_DrawPlaque(16, underRow ? q_max(l.top + 4, static_cast<int>(za::ceil(menuui::toolbarBottom())) + 2) : l.top + 4);
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, l.top + 4, title);
    const char* name = pages[page].title;
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
    int& notedPage = notedPlace.page;
    int& notedCursor = notedPlace.cursor;
    int& notedScroll = notedPlace.scroll;
    int& notedBuild = notedPlace.build;
    if(page != notedPage || cursor != notedCursor || scroll != notedScroll || builds[page] != notedBuild)
    {
        notedPage = page;
        notedCursor = cursor;
        notedScroll = scroll;
        notedBuild = builds[page];
        noteLeft(page, list);
    }

    // While the sticks' selection is on the corner's buttons, no row is selected (nor its help shown).
    const bool rowSelected = !menuui::toolbarFocused();
    for(int i = scroll; i < n && i < scroll + rows; i++)
    {
        drawItem(list[i], l.listTop + rowTop(list, scroll, i), rowSelected && i - list[i].partOf == cursor);
    }

    if(int y, height; scrollbar(list, y, height))
    {
        scrollbarX = q_min(midPos + 188, static_cast<int>(glcanvas.right) - 16);
        M_DrawTextBox(scrollbarX - 4, l.listTop + y - 4, 0, height - 1);
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
}

extern "C" void VR_Menu_Key(int key, int repeat)
{
    if(pages[page].build == pageSearch)
    {
        searchKey(key);
        return;
    }
    if(pages[page].build == pageConsole)
    {
        consoleKey(key);
        return;
    }
    if(pages[page].build == pageMaps)
    {
        mapsKey(key);
        return;
    }
    const auto& list = items(page);
    const int cursor = cursors[page];

    if(sliderGrab || scrollGrab)
    {
        if(!keydown[K_MOUSE1] || key == K_ESCAPE || key == K_BBUTTON || key == K_MOUSE2)
        {
            sliderGrab = scrollGrab = false;
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

        case K_LEFTARROW: change(list[cursor], -1, repeat); break;
        case K_RIGHTARROW: change(list[cursor], 1, repeat); break;

        case K_MOUSE1:
            // On the scrollbar: it is dragged.
            if(int y, height; m_mousex >= scrollbarX - 8 && scrollbar(list, y, height))
            {
                scrollGrab = true;
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
                    sliderGrab = true;
                    sliderGrabHold = -1.f;
                    if(past && za::fabs(m_mousex - thumb) <= 6.f)
                    {
                        sliderGrabHold = m_mousex;
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
    if(pages[page].build == pageSearch && key >= 32 && key < 127)
    {
        typeInSearch(static_cast<char>(key));
    }
    if(pages[page].build == pageConsole && key >= 32 && key < 127)
    {
        typeInConsole(static_cast<char>(key));
    }
    if(pages[page].build == pageMaps && key >= 32 && key < 127)
    {
        mapsTypeIn(static_cast<char>(key));
    }
}

extern "C" int VR_Menu_TextEntry()
{
    return pages[page].build == pageSearch || pages[page].build == pageConsole || pages[page].build == pageMaps
               ? TEXTMODE_NOPOPUP
               : TEXTMODE_OFF; // (the page's own keyboard)
}

extern "C" void VR_Menu_Mousemove(float cx, float cy)
{
    if(pages[page].build == pageSearch)
    {
        searchMouse(cx, cy);
        return;
    }
    if(pages[page].build == pageConsole)
    {
        consoleMouse(cx, cy);
        return;
    }
    if(pages[page].build == pageMaps)
    {
        mapsMouse(cx, cy);
        return;
    }
    if(dropDownMousemove(cx, cy))
    {
        return;
    }

    const auto& list = items(page);
    int& cursor = cursors[page];

    if(sliderGrab || scrollGrab)
    {
        if(!keydown[K_MOUSE1])
        {
            sliderGrab = scrollGrab = false;
            return;
        }
        if(scrollGrab)
        {
            scrollTo(cy);
        }
        else if(sliderGrabHold < 0.f || za::fabs(cx - sliderGrabHold) >= 4.f)
        {
            sliderGrabHold = -1.f;
            setSliderAt(list[cursor], cx);
        }
        return;
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
