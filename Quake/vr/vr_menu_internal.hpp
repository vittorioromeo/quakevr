#pragma once

// vr_menu_internal.hpp -- what vr_menu.cpp (see vr_menu.hpp) shares with its page groups: vr_menu_pages_debug.cpp,
// vr_menu_pages_gameplay.cpp and vr_menu_pages_settings.cpp (each a list of vr_menu_*.inc files; split for the build's
// speed: one file was the build's long pole). Internal: included by those alone. What was the file's anonymous
// namespace is namespace menu_detail (each file's own names in it too, none repeated): its types here, its variables
// and the functions another file uses declared here and defined in the file that has them.

#include "vr_aggrip.hpp"
#include "vr_cheats.hpp"
#include "vr_doom.hpp"
#include "vr_toolgun.hpp"
#include "vr_warmup.hpp"
#include "vr_alloccount.h"

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
#include "vr_posestudio.hpp"
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

namespace menu_detail
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

using PageBuilder = za::Vector<Item> (*)();

// The index of the page `build` builds (pages[], below): what a link to it opens.
[[nodiscard]] int pageIndex(PageBuilder build);

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

// Weapon Offsets' parts, as its main page links them (in WeaponOffsetsPart's order). A new part: its builder, a
// WeaponOffsetsPart, a line in `pages` (last) and one here.
struct WeaponOffsetsPartPage
{
    PageBuilder build;
    const char* link;
    const char* help;
};

// ----------------------------------------------------------------------------
// Defined in one file, used in another
// ----------------------------------------------------------------------------
extern mem::Scratch<MenuReadouts> readouts;
extern mem::Cache<PageTexts> pageTexts;
extern const char* const menuButtonHelp;
extern const WeaponOffsetsPartPage weaponOffsetsPartPages[WofsParts - 1];
template <int P>
[[nodiscard]] za::Vector<Item> pageMonsterParam();
// core
[[nodiscard]] Item action(const char* label, void (*fn)());
[[nodiscard]] const char* buildVersionLine();
[[nodiscard]] za::Vector<Choice> bulletTimeChoices();
void calibrateHeight();
[[nodiscard]] Item classSlider(const char* label, cvar_t& cvar, float from, float max, float step, const char* format);
[[nodiscard]] Item command(const char* label, const char* text);
[[nodiscard]] Item cycle(const char* label, cvar_t* cvar, za::Vector<Choice> choices);
[[nodiscard]] Item cycle(const char* label, cvar_t& cvar, za::Vector<Choice> choices);
[[nodiscard]] Item cycle(const char* label, const char* cvar, za::Vector<Choice> choices);
[[nodiscard]] Item debrisMultiplayerSlider();
[[nodiscard]] za::Vector<Choice> dummyEnemies();
[[nodiscard]] Item dummyHealthSlider();
[[nodiscard]] Item dynresMaxSlider();
[[nodiscard]] za::Vector<Choice> gruntChoices();
[[nodiscard]] Item header(const char* label);
void holsterSetupStart();
[[nodiscard]] Item hueSlider(const char* label, cvar_t& cvar);
[[nodiscard]] Item info(const char* (*text)());
[[nodiscard]] Item infoLine(const char* (*text)(int), int arg);
[[nodiscard]] const za::Vector<Item>& items(int page);
extern int mainPageSnap;
[[nodiscard]] za::Vector<Choice> menuButtonChoices();
[[nodiscard]] Item open(const char* label, int page);
[[nodiscard]] za::Vector<Item> pageAdvanced();
[[nodiscard]] za::Vector<Item> pageAimingSettings();
[[nodiscard]] za::Vector<Item> pageArmedGrips();
[[nodiscard]] za::Vector<Item> pageArmedGrunts();
[[nodiscard]] za::Vector<Item> pageBloodFeedback();
[[nodiscard]] za::Vector<Item> pageBodyArms();
[[nodiscard]] za::Vector<Item> pageBodyCalibration();
[[nodiscard]] za::Vector<Item> pageBodyCrouch();
[[nodiscard]] za::Vector<Item> pageBodyDisplay();
[[nodiscard]] za::Vector<Item> pageBulletTime();
[[nodiscard]] za::Vector<Item> pageCarryingHub();
za::Vector<Item> pageChanged();
[[nodiscard]] za::Vector<Item> pageChecklist();
[[nodiscard]] za::Vector<Item> pageClimbing();
[[nodiscard]] za::Vector<Item> pageColours();
[[nodiscard]] za::Vector<Item> pageCombat();
[[nodiscard]] za::Vector<Item> pageDamage();
[[nodiscard]] za::Vector<Item> pageDebugAutomation();
[[nodiscard]] za::Vector<Item> pageDebugCheats();
[[nodiscard]] za::Vector<Item> pageDebugLogging();
[[nodiscard]] za::Vector<Item> pageDebugProfiling();
[[nodiscard]] za::Vector<Item> pageDebugReports();
[[nodiscard]] za::Vector<Item> pageDebugTests();
[[nodiscard]] za::Vector<Item> pageDebugTools();
[[nodiscard]] za::Vector<Item> pageDebugViews();
[[nodiscard]] za::Vector<Item> pageDoom();
[[nodiscard]] za::Vector<Item> pageEnemyMelee();
[[nodiscard]] za::Vector<Item> pageEnemyShoves();
[[nodiscard]] za::Vector<Item> pageEnemyWeapons();
[[nodiscard]] za::Vector<Item> pageFingersCollisions();
[[nodiscard]] za::Vector<Item> pageFlashlightLowGrip();
[[nodiscard]] za::Vector<Item> pageFlashlightMounts();
[[nodiscard]] za::Vector<Item> pageFlashlightOverheadGrip();
[[nodiscard]] za::Vector<Item> pageForceGrab();
[[nodiscard]] za::Vector<Item> pageGadget();
[[nodiscard]] za::Vector<Item> pageGibs();
[[nodiscard]] za::Vector<Item> pageGore();
[[nodiscard]] za::Vector<Item> pageGrapple();
[[nodiscard]] za::Vector<Item> pageHeadset();
[[nodiscard]] za::Vector<Item> pageHipHolsters();
za::Vector<Item> pageHitbox();
[[nodiscard]] za::Vector<Item> pageHoldingEnemies();
[[nodiscard]] za::Vector<Item> pageHoldingStruggle();
[[nodiscard]] za::Vector<Item> pageHolsterZones();
[[nodiscard]] za::Vector<Item> pageHotBlades();
[[nodiscard]] za::Vector<Item> pageHudHub();
[[nodiscard]] za::Vector<Item> pageKdTrigBlast();
[[nodiscard]] za::Vector<Item> pageKdTrigBlow();
[[nodiscard]] za::Vector<Item> pageKdTrigLedge();
[[nodiscard]] za::Vector<Item> pageKdTrigShotgun();
[[nodiscard]] za::Vector<Item> pageKdTrigSlope();
[[nodiscard]] za::Vector<Item> pageKdTrigStairs();
[[nodiscard]] za::Vector<Item> pageKdTrigWounded();
[[nodiscard]] za::Vector<Item> pageKdTriggers();
[[nodiscard]] za::Vector<Item> pageKnockdowns();
[[nodiscard]] za::Vector<Item> pageLightningWater();
[[nodiscard]] za::Vector<Item> pageLimbGore();
[[nodiscard]] za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageMonsterGroups();
[[nodiscard]] za::Vector<Item> pageMonsterHealth();
za::Vector<Item> pageMonsterHitbox();
[[nodiscard]] za::Vector<Item> pageMotionRecorder();
[[nodiscard]] za::Vector<Item> pageMovement();
[[nodiscard]] za::Vector<Item> pagePlay();
[[nodiscard]] za::Vector<Item> pageRagdollTests();
[[nodiscard]] za::Vector<Item> pageRagdolls();
[[nodiscard]] za::Vector<Item> pageRagtool();
[[nodiscard]] za::Vector<Item> pageRecording();
[[nodiscard]] za::Vector<Item> pageReviewTakes();
[[nodiscard]] za::Vector<Item> pageSaves();
[[nodiscard]] za::Vector<Item> pageScreens();
[[nodiscard]] za::Vector<Item> pageSeated();
[[nodiscard]] za::Vector<Item> pageSeatedHolsters();
[[nodiscard]] za::Vector<Item> pageSound();
[[nodiscard]] za::Vector<Item> pageSpawnWeapons();
[[nodiscard]] za::Vector<Item> pageStamina();
[[nodiscard]] za::Vector<Item> pageStealth();
[[nodiscard]] za::Vector<Item> pageStealthTests();
extern mem::Cache<PageTexts> pageTexts;
[[nodiscard]] za::Vector<Item> pageTips();
[[nodiscard]] za::Vector<Item> pageVrsAmmoPouch();
[[nodiscard]] za::Vector<Item> pageVrsAppearance();
[[nodiscard]] za::Vector<Item> pageVrsBody();
[[nodiscard]] za::Vector<Item> pageVrsBodyEquipment();
[[nodiscard]] za::Vector<Item> pageVrsBodyFlashlight();
[[nodiscard]] za::Vector<Item> pageVrsBulletTime();
[[nodiscard]] za::Vector<Item> pageVrsComfort();
[[nodiscard]] za::Vector<Item> pageVrsComfortTurning();
[[nodiscard]] za::Vector<Item> pageVrsControls();
[[nodiscard]] za::Vector<Item> pageVrsDisplay();
[[nodiscard]] za::Vector<Item> pageVrsDisplayGraphics();
[[nodiscard]] za::Vector<Item> pageVrsFlashlight();
[[nodiscard]] za::Vector<Item> pageVrsGraphics();
[[nodiscard]] za::Vector<Item> pageVrsHands();
[[nodiscard]] za::Vector<Item> pageVrsHaptics();
[[nodiscard]] za::Vector<Item> pageVrsHeight();
[[nodiscard]] za::Vector<Item> pageVrsHeightHands();
[[nodiscard]] za::Vector<Item> pageVrsHolsters();
[[nodiscard]] za::Vector<Item> pageVrsHud();
[[nodiscard]] za::Vector<Item> pageVrsHudHapticsSound();
[[nodiscard]] za::Vector<Item> pageVrsItemCollection();
[[nodiscard]] za::Vector<Item> pageVrsLighting();
[[nodiscard]] za::Vector<Item> pageVrsLocoControls();
[[nodiscard]] za::Vector<Item> pageVrsLocomotion();
[[nodiscard]] za::Vector<Item> pageVrsRangerHead();
[[nodiscard]] za::Vector<Item> pageVrsScaling();
[[nodiscard]] za::Vector<Item> pageVrsSound();
[[nodiscard]] za::Vector<Item> pageVrsTeleport();
[[nodiscard]] za::Vector<Item> pageVrsTurning();
[[nodiscard]] za::Vector<Item> pageVrsWeapons();
[[nodiscard]] za::Vector<Item> pageVrsWeaponsBulletTime();
[[nodiscard]] za::Vector<Item> pageWeaponDamage();
[[nodiscard]] za::Vector<Item> pageWeaponEffects();
[[nodiscard]] za::Vector<Item> pageWeaponOffsets();
[[nodiscard]] za::Vector<Item> pageWeaponsHub();
[[nodiscard]] za::Vector<Item> pageWeightDamage();
[[nodiscard]] za::Vector<Item> pageWofsEffects();
[[nodiscard]] za::Vector<Item> pageWofsFingers();
[[nodiscard]] za::Vector<Item> pageWofsFlashlight();
[[nodiscard]] za::Vector<Item> pageWofsHand();
[[nodiscard]] za::Vector<Item> pageWofsHolstered();
[[nodiscard]] za::Vector<Item> pageWofsScreen();
[[nodiscard]] za::Vector<Item> pageWofsSights();
[[nodiscard]] za::Vector<Item> pageWofsStock();
[[nodiscard]] za::Vector<Item> pageWofsTwoHanded();
[[nodiscard]] Item progressBar(float (*fraction)(), const char* (*text)());
extern mem::Scratch<MenuReadouts> readouts;
[[nodiscard]] za::Vector<Choice> reloadChoices();
void resetAll();
void resetHandOffsets();
void resetHolsters();
void restartVr();
[[nodiscard]] Item row(const char* (*text)(int), const char* (*help)(int), void (*pick)(int), int arg, int page);
void showPage(int target);
[[nodiscard]] Item slider(const char* label, cvar_t* cvar, float min, float max, float step, const char* format);
[[nodiscard]] Item slider(const char* label, cvar_t& cvar, float min, float max, float step, const char* format);
[[nodiscard]] Item slider(const char* label, const char* cvar, float min, float max, float step, const char* format);
[[nodiscard]] Item toggle(const char* label, cvar_t& cvar);
[[nodiscard]] Item toggle(const char* label, const char* cvar);
// debug
[[nodiscard]] za::Vector<Item> pageAdMonsterTests();
[[nodiscard]] za::Vector<Item> pageDebug();
za::Vector<Item> pageDebugAutomation();
za::Vector<Item> pageDebugLogging();
za::Vector<Item> pageDebugProfiling();
za::Vector<Item> pageDebugReports();
za::Vector<Item> pageDebugTests();
za::Vector<Item> pageDebugTools();
za::Vector<Item> pageDebugViews();
za::Vector<Item> pageMachineHordeTests();
za::Vector<Item> pageMg3BestiaryTests();
za::Vector<Item> pageMg3ChthonTests();
za::Vector<Item> pageMg3MonsterTests();
za::Vector<Item> pageMg3ShubTests();
za::Vector<Item> pageMg3Tests();
za::Vector<Item> pageMg3WeaponTests();
za::Vector<Item> pageSpawnWeapons();
extern int testsPageSpawn;
// gameplay
extern int armsPageCalibrated;
extern int bodycalSeated;
extern int bodycalVersion;
extern int flashlightPageManual[2];
[[nodiscard]] za::Vector<Item> pageAdGuns();
[[nodiscard]] za::Vector<Item> pageArcaneDimensions();
[[nodiscard]] za::Vector<Item> pageBatting();
[[nodiscard]] za::Vector<Item> pageBody();
[[nodiscard]] za::Vector<Item> pageBurning();
[[nodiscard]] za::Vector<Item> pageCarrying();
[[nodiscard]] za::Vector<Item> pageChainsawEngine();
[[nodiscard]] za::Vector<Item> pageCorpseDamage();
[[nodiscard]] za::Vector<Item> pageCrates();
[[nodiscard]] za::Vector<Item> pageDecapitation();
[[nodiscard]] za::Vector<Item> pageFlail();
[[nodiscard]] za::Vector<Item> pageFlashlight();
[[nodiscard]] za::Vector<Item> pageGameplay();
[[nodiscard]] za::Vector<Item> pageKdTrigTackle();
[[nodiscard]] za::Vector<Item> pageKdTrigThrown();
[[nodiscard]] za::Vector<Item> pageParryBash();
[[nodiscard]] za::Vector<Item> pageRagdollCentroid();
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight();
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer();
[[nodiscard]] za::Vector<Item> pageRagdollFiend();
[[nodiscard]] za::Vector<Item> pageRagdollGremlin();
[[nodiscard]] za::Vector<Item> pageRagdollGrunt();
[[nodiscard]] za::Vector<Item> pageRagdollKnight();
[[nodiscard]] za::Vector<Item> pageRagdollMummy();
[[nodiscard]] za::Vector<Item> pageRagdollOgre();
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler();
[[nodiscard]] za::Vector<Item> pageRagdollScrag();
[[nodiscard]] za::Vector<Item> pageRagdollShambler();
[[nodiscard]] za::Vector<Item> pageRagdollVore();
[[nodiscard]] za::Vector<Item> pageRagdollZombie();
[[nodiscard]] za::Vector<Item> pageRocksBricks();
[[nodiscard]] za::Vector<Item> pageSmallGibs();
[[nodiscard]] za::Vector<Item> pageSmallGibsEnemies();
[[nodiscard]] za::Vector<Item> pageThrowing();
[[nodiscard]] za::Vector<Item> pageWallTorches();
// settings
void allowTwoHands(int slot);
[[nodiscard]] weapons::HolsterKind editedHolster();
[[nodiscard]] int editedHotspot();
extern int handCalPageOwn;
extern int itemCollectionPageMode;
void matchControllerPreview();
za::Vector<Item> pageAdvanced();
za::Vector<Item> pageCarryingHub();
za::Vector<Item> pageCombat();
[[nodiscard]] za::Vector<Item> pageCrosshairSettings();
[[nodiscard]] za::Vector<Item> pageExplosionDebris();
[[nodiscard]] za::Vector<Item> pageFireParticles();
[[nodiscard]] za::Vector<Item> pageGraphics();
[[nodiscard]] za::Vector<Item> pageGraphicsFoam();
[[nodiscard]] za::Vector<Item> pageGraphicsLights();
[[nodiscard]] za::Vector<Item> pageGraphicsLiquids();
[[nodiscard]] za::Vector<Item> pageGraphicsLootBeams();
[[nodiscard]] za::Vector<Item> pageGraphicsModels();
[[nodiscard]] za::Vector<Item> pageGraphicsPost();
[[nodiscard]] za::Vector<Item> pageGraphicsPresets();
[[nodiscard]] za::Vector<Item> pageGraphicsRelighting();
[[nodiscard]] za::Vector<Item> pageGraphicsRetro();
[[nodiscard]] za::Vector<Item> pageGraphicsRetroLight();
[[nodiscard]] za::Vector<Item> pageGraphicsShadows();
[[nodiscard]] za::Vector<Item> pageGraphicsSurfaces();
[[nodiscard]] za::Vector<Item> pageGraphicsTeleporters();
[[nodiscard]] za::Vector<Item> pageHandGunCalibration();
za::Vector<Item> pageHeldObjectOffsets();
za::Vector<Item> pageHeldObjectWeights();
za::Vector<Item> pageHotBlades();
[[nodiscard]] za::Vector<Item> pageHotspotSettings();
[[nodiscard]] za::Vector<Item> pageHudConfiguration();
za::Vector<Item> pageHudHub();
[[nodiscard]] za::Vector<Item> pageImmersionSettings();
za::Vector<Item> pageLightningWater();
[[nodiscard]] za::Vector<Item> pageLocomotionSettings();
za::Vector<Item> pageMain();
[[nodiscard]] za::Vector<Item> pageMeleeSettings();
[[nodiscard]] za::Vector<Item> pageMenuSettings();
za::Vector<Item> pageMovement();
[[nodiscard]] za::Vector<Item> pageParticleSettings();
[[nodiscard]] za::Vector<Item> pagePlayerCalibration();
[[nodiscard]] za::Vector<Item> pageReloadCellCords();
[[nodiscard]] za::Vector<Item> pageReloadLaunchers();
[[nodiscard]] za::Vector<Item> pageReloadNailgun();
[[nodiscard]] za::Vector<Item> pageReloadNailPistol();
[[nodiscard]] za::Vector<Item> pageReloadOverrides();
[[nodiscard]] za::Vector<Item> pageReloadPlasmaGun();
[[nodiscard]] za::Vector<Item> pageReloadShotgun();
[[nodiscard]] za::Vector<Item> pageReloadSuperNailgun();
[[nodiscard]] za::Vector<Item> pageReloadSuperShotgun();
[[nodiscard]] za::Vector<Item> pageReloadThunderbolt();
[[nodiscard]] za::Vector<Item> pageReloadWidowmaker();
[[nodiscard]] za::Vector<Item> pageReloading();
[[nodiscard]] za::Vector<Item> pageRetroAll();
[[nodiscard]] za::Vector<Item> pageRetroOverride();
za::Vector<Item> pageSaves();
[[nodiscard]] za::Vector<Item> pageSwimSettings();
[[nodiscard]] za::Vector<Item> pageTransparencyOptions();
za::Vector<Item> pageWeaponEffects();
za::Vector<Item> pageWeaponOffsets();
za::Vector<Item> pageWeaponWeights();
za::Vector<Item> pageWeaponsHub();
za::Vector<Item> pageWofsEffects();
za::Vector<Item> pageWofsFingers();
za::Vector<Item> pageWofsFlashlight();
za::Vector<Item> pageWofsHand();
za::Vector<Item> pageWofsHolstered();
za::Vector<Item> pageWofsScreen();
za::Vector<Item> pageWofsSights();
za::Vector<Item> pageWofsStock();
za::Vector<Item> pageWofsTwoHanded();
extern int relightToolState;
void sightAlignApply();
void sightAlignUndo();
extern int weaponOffsetsHand;
extern int weaponOffsetsHeldSlot;
extern int weaponOffsetsHolster;
extern int weaponOffsetsHotspot;
extern int weaponOffsetsHotspotManual;
extern int weaponOffsetsHotspotType;
extern int weaponOffsetsInherit;
extern int weaponOffsetsManual;
extern int weaponOffsetsPreviewOwn;
extern bool weaponOffsetsSightFocus;
extern int weaponOffsetsSightVersion;
extern int weaponOffsetsSlot;
extern bool weaponOffsetsStale;
void weightPageShown(PageBuilder build);
[[nodiscard]] bool weightPageStale(PageBuilder build);
// (the Retro Textures pages' template, below)
[[nodiscard]] za::Vector<Item> retroCategoryItems(retro::Category c, void (*reset)());
[[nodiscard]] bool resetAllIsArmed();

// Graphics > Retro Textures: a page for each kind (vr_menu_pages.inc).
template <retro::Category C>
void retroResetCategory()
{
    Cbuf_AddText(va("vr_retro_reset %s\n", retro::categoryKey(C)));
}

template <retro::Category C>
za::Vector<Item> pageRetroCategory()
{
    return retroCategoryItems(C, retroResetCategory<C>);
}

} // namespace menu_detail

using namespace ::menu_detail;
