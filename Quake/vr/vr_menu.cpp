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
// The page groups are in vr_menu_pages_debug.cpp, vr_menu_pages_gameplay.cpp and vr_menu_pages_settings.cpp;
// what they share with this file is in vr_menu_internal.hpp.

#include "vr_menuui.hpp"
#include "vr_testkit.hpp"
#include "vr_random.hpp"
#include "vr_runsetup.hpp"
#include "vr_menu_internal.hpp"
#include "vr_armourdata.hpp"
#include "vr_flataimsens.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_gorepresets.hpp"
#include "vr_gameplaypresets.hpp"
#include "vr_presets.hpp"
#include "vr_cvars.hpp"
#include "vr_monstervalues.hpp"
#include "vr_tunables.hpp"
#include "vr_weaponvalues.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_api.h"

extern "C" cvar_t scr_fov; // gl_screen.c (Aim Sensitivity's help)
extern "C" int Key_StringToKeynum(const char* str);   // keys.c: a key's number from its bind name
extern "C" const char* Key_KeynumToString(int keynum); // keys.c: and its name
extern "C" char* keybindings[MAX_KEYS];                // keys.c: what each key is bound to (the Flat Keys rows' conflicts)
extern "C" void M_VRKeyCapture(qboolean on);           // menu.c: a key row waits for a key

namespace menu_detail
{

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
    return slider(label, settingByName(cvar), min, max, step, format); // (or a tuning constant: Ragdoll Tool)
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
    if(vr_hipnotic_status.value == 1.f) // (installed and valid)
    {
        out.pushBack({13.f, "Gremlin"});
        out.pushBack({14.f, "Centroid"});
    }
    if(vr_rogue_status.value == 1.f)
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
    Item i = row(opt::vr_dummy_health);
    i.negativeLabel = "Its Own";
    i.negativeStart = 5.f;
    return i;
}

// Dynamic resolution's ceiling (vr_dynres_max), its leftmost step below 0: Render Scale's (the default).
[[nodiscard]] Item dynresMaxSlider()
{
    Item i = row(opt::vr_dynres_max);
    i.negativeLabel = "Render Scale";
    i.negativeStart = 0.5f;
    return i;
}

// Rocks and bricks in a multiplayer map (vr_debris_mp_max), its leftmost step -1: as many as in single player (Most in a
// Map, the default: the author's, 2026-10-06).
[[nodiscard]] Item debrisMultiplayerSlider()
{
    Item i = row(opt::vr_debris_mp_max);
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

// A key row (the Flat Keys page): `cvar` holds a key's bind name; enter or a click waits for a key (the Keys menu's "press a
// key"), Backspace or Delete clears it.
[[nodiscard]] Item keyRow(const char* label, cvar_t& cvar)
{
    return Item{Item::Key, label, &cvar};
}

[[nodiscard]] Item cycle(const char* label, cvar_t& cvar, za::Vector<Choice> choices)
{
    return cycle(label, &cvar, ZA_MOVE(choices));
}

[[nodiscard]] Item cycle(const char* label, const char* cvar, za::Vector<Choice> choices)
{
    return cycle(label, settingByName(cvar), ZA_MOVE(choices));
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

// Whether the loaded progs carry the QC tests (QC/vr_macros.qc QVR_TESTS).
[[nodiscard]] bool qcTestsBuilt()
{
    return qvr::progs::qcTestsPresent();
}

[[nodiscard]] const char* qcTestsNotice()
{
    return "Tests are not in this build's progs (QVR_NO_TESTS): the rows below do nothing.";
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

mem::Scratch<MenuReadouts> readouts{"menu readouts"};

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

#include "vr_menu_toolgun.inc"
#include "vr_menu_ragtool.inc" // Debug > Ragdoll Tool (vr_ragtool.cpp)

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
        row(opt::vr_motion_armed),
        cycle("Category", vr_motion_category, ZA_MOVE(categories))
            .help("What the motion should do. It stays chosen: record many takes of it in a row. No Hit: motions that "
                  "must do nothing (wiggles, weak moves, reloading)."),
        cycle("Detail", vr_motion_detail, ZA_MOVE(details))
            .help("Optional: which kind (the swing's direction, the weapon). Its takes count apart, and with the "
                  "category's."),
        info(motion::labelStatus),
        row(opt::vr_motion_button),
        info(motionNote),
        info(motionLastSaved),
        action("Delete Last Take", motion::discardLast)
            .help("Moves the last take saved into motions/discarded (a take that went wrong). Again: the one before."),
        row(opt::vr_motion_preroll),
        row(opt::vr_motion_tail),
    };
}

// Review Takes (vr_motion_review.cpp, MOTIONS.md "Reviewing failing takes"): the takes that fail vr_motion_eval or are
// suspect, listed; one picked opens the Take page (its details, the ghost replay, keep, discard, relabel).
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
        row(opt::vr_motion_review_show),
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
        row(opt::vr_motion_review_speed),
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
// The hub is no run: the last run's mutators end there (its teleporter asks the next run's: the Run Setup screen).
void playHub() { qvr::runsetup::endRun(); Cbuf_AddText("vr_campaign_hub\n"); }
// The tutorial (vrtutorial: Misc/quakevr/maps/vrtutorial_gen.py) on Easy.
void playTutorial() { Cbuf_AddText("skill 0; map vrtutorial\n"); }
void playFiringRange() { Cbuf_AddText("map vrfiringrange\n"); }
// Official Campaigns > Dawn of the Machine: Bloody Nightmare: skill 3 and vr_mg3_bn_start (the start map's first frame
// makes it a Bloody Nightmare game, QC vr_mg3_defs.qc MG3_Frame), then the campaign as its own row starts it. While
// the campaign cannot start, only its reason is printed and nothing is set.
// Now through the Run Setup screen, its Bloody Nightmare row on (its rule: skill 3 and vr_mg3_bn_start at the start).
void playMg3BloodyNightmare()
{
    if(VR_CampaignUnavailable(5) == 0)
    {
        Cvar_Set("vr_runpick_bn", "1");
    }
    openRunSetupCampaign(5);
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
    if(index != qvr::ad::campaignIndex) { openRunSetupCampaign(index); return; } // (the Run Setup screen first)
    M_Confirm("Arcane Dimensions mode is still\nexperimental and might have bugs.", "Continue", "Back", 0.f,
        startArcaneDimensions);
}

// A campaign row's Continue (under it, when the campaign has an autosave; vr_continue.cpp, "Continue on the launch
// rows"): "Continue (12 min ago)", its run ("Hard / Classic") on the line under it (part of it: a narrow row cuts a
// long line), its newest autosave loaded directly with its run's choices.
void runSetupCloseMenu(); // (vr_menu_runsetup.inc)
const char* campaignContinueLabel(int index)
{
    const char* label = VR_LaunchContinueLabel(index, "", 1);
    return label ? label : "Continue";
}
const char* campaignContinueRun(int index)
{
    const char* run = VR_LaunchContinueLabel(index, "", 2);
    return run && run[0] ? va("  %s", run) : "  (no run choices saved)";
}
const char* campaignContinueHelp(int index)
{
    const char* help = VR_LaunchContinueHelp(index, "");
    return help ? help : "";
}
void continueCampaignRow(int index)
{
    if(VR_LaunchContinueLoad(index, ""))
    {
        runSetupCloseMenu();
    }
}

[[nodiscard]] za::Vector<Item> pageCampaigns()
{
    za::Vector<Item> items{header("Official Campaigns")};
    VR_LaunchContinueRefresh(); // (the page is built again each time it is shown)
    for(int i = 0; i < VR_CampaignCount(); ++i) // (Arcane Dimensions too: its row dim until it is ready)
    {
        Item choice = row(VR_CampaignLabel, VR_CampaignHelp, selectCampaignRow, i, -1);
        choice.dimArg = [](int index) -> bool
        { return index == qvr::ad::campaignIndex ? !VR_CampaignInstalled(index) : VR_CampaignUnavailable(index) != 0; };
        items.pushBack(choice);
        // (Arcane Dimensions plays in its own game folder: its saves are not this one's)
        if(i != qvr::ad::campaignIndex && VR_LaunchContinueSave(i, ""))
        {
            Item resume = row(campaignContinueLabel, campaignContinueHelp, continueCampaignRow, i, -1);
            resume.dimArg = [](int index) -> bool { return VR_CampaignUnavailable(index) != 0; };
            items.pushBack(resume);
            Item run = row(campaignContinueRun, campaignContinueHelp, nullptr, i, -1);
            run.dimArg = resume.dimArg;
            run.partOf = 1;
            items.pushBack(run);
        }
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
        command("Survival Arena", "map vrarena")
            .help("The arena: waves of armed humanoids, harder and more frequent. You start unarmed on the dais and "
                  "take their weapons from them. The waves follow the game's difficulty -- smaller and calmer on Easy, "
                  "bigger, quicker and thicker on Nightmare (Combat > Survival Arena)."),
        header("Bots (Multiplayer)"),
        action("Add Bot (Team 0)", addBotTeam0),
        action("Add Bot (Team 1)", addBotTeam1),
        action("Kick Bot", kickBot),
    };
}


#include "vr_menu_mgroups.inc" // Combat > Monster Groups
// (The groups' pages linked from vr_menu_pages_gameplay.cpp.)
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Push)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Resist)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Shove)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Stagger)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Struggle)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Tackle)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Throw)>();
template za::Vector<Item> pageMonsterParam<static_cast<int>(mgroups::Param::Thrown)>();



[[nodiscard]] za::Vector<Item> pageGadget()
{
    return {
        open("Doom", pageIndex(pageDoom)).help("Play Doom on the gadget's screen: a touchscreen under your fingertip."),
        open("HUD and Gadget Arm: VR Settings, Body and Display", pageIndex(pageVrsHudHapticsSound)).help("The HUD's kind is on VR Settings; the gadget's arm on Body and Display."),
        row(opt::vr_gadget_scale),
        header("Placement"),
        row(opt::vr_gadget_x),
        row(opt::vr_gadget_y),
        row(opt::vr_gadget_z),
        row(opt::vr_gadget_pitch),
        row(opt::vr_gadget_yaw),
        row(opt::vr_gadget_roll),
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
        row(opt::vr_doom_controls),
        row(opt::vr_doom_overlay),
        row(opt::vr_doom_hard_fire),
        row(opt::vr_doom_always_run),
        row(opt::vr_doom_focus_look).advanced(),
        row(opt::vr_doom_focus_grace).advanced(),
        row(opt::vr_doom_light_presses).advanced(),
        row(opt::vr_doom_light_gap).advanced(),
        row(opt::vr_doom_pause_unfocused),
        header("Picture and Sound"),
        row(opt::vr_doom_aspect),
        row(opt::vr_doom_crt),
        row(opt::vr_doom_volume).advanced(),
        row(opt::vr_doom_panel),
        row(opt::vr_doom_panel_size).advanced(),
        command("Place the Panel in Front", "vr_doom_panel_place"),
        header("Touchscreen"),
        row(opt::vr_touch_press).advanced(),
        row(opt::vr_touch_release).advanced(),
        row(opt::vr_touch_hard).advanced(),
        row(opt::vr_touch_haptic).advanced(),
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
        row(opt::vr_tips),
        row(opt::vr_tips_distance),
        row(opt::vr_tips_line_of_sight),
        row(opt::vr_tips_view_angle),
        row(opt::vr_tips_delay),
        row(opt::vr_tips_time),
        row(opt::vr_tips_facing),
        row(opt::vr_tips_size),
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
        row(opt::vr_gadget_show_level),
        row(opt::vr_dead_hide_gear),
        row(opt::vr_gadget_stamina),
        row(opt::vr_gadget_light).developer(),
        row(opt::vr_gadget_lock_grace).developer(),
        header("Gear Lights: the Gadget's Side Button"),
        row(opt::vr_gear_lights),
        row(opt::vr_gear_lights_dim).developer(),
        row(opt::vr_gear_lights_screen_dim).developer(),
        row(opt::vr_gadget_button),
        row(opt::vr_gadget_button_size).developer(),
        row(opt::vr_gadget_button_x),
        row(opt::vr_gadget_button_y).developer(),
        row(opt::vr_gadget_button_z),
        row(opt::vr_gadget_button_pitch),
        row(opt::vr_gadget_button_yaw),
        row(opt::vr_gadget_fingertip_drawn),
        row(opt::vr_gadget_button_reach).developer(),
        row(opt::vr_gadget_fingertip_x),
        row(opt::vr_gadget_fingertip_y),
        row(opt::vr_gadget_fingertip_z),
        row(opt::vr_gadget_fingertip_pitch),
        row(opt::vr_gadget_fingertip_yaw),
        row(opt::vr_gadget_fingertip_roll),
        row(opt::vr_gadget_button_cooldown).developer(),
        row(opt::vr_debug_gadget_button),
        row(opt::vr_gadget_crt).developer(),
        row(opt::vr_screen_glow),
        row(opt::vr_screen_text_glow).developer(),
        header("Messages"),
        row(opt::vr_messages_hologram),
        row(opt::vr_messages_hologram_time).developer(),
        row(opt::vr_messages_hologram_size),
        row(opt::vr_messages_hologram_height).developer(),
        row(opt::vr_messages_hologram_effect),
        action("Show a Test Message", hologramTestMessage)
            .help("One of the game's messages in the hologram, as they come: raise the gadget to see it while you change these settings. Press again for another: they stack."),
        row(opt::vr_messages_hologram_only),
        // (The author's note vrstart_2026-10-23_01-23-46: VR Calibration's texts jumped to each turn of the head.)
        row(opt::vr_view_text_smooth).developer(),
        row(opt::vr_hud_console_log),
        row(opt::vr_notify_wrist),
        row(opt::vr_notify_wrist_time).developer(),
        row(opt::vr_notify_wrist_height).developer(),
        row(opt::vr_notify_wrist_alpha).developer(),
        open("Wrist Gadget Font", pageIndex(pageFontSurface<1>)).help("The gadget's text: its screen, hologram and log (upscaling, outline, shadow)."),
        header("Weapons' Ammo Screens"),
        row(opt::vr_show_weapon_text),
        row(opt::vr_weapon_screen),
        row(opt::vr_weapon_screen_padding),
        row(opt::vr_weapon_screen_crt),
        toggle("Rounded Glass Ammo Screens", "vr_weapon_screen_style").help("The ammo screens as rounded dark glass set in the gun (a vignette, an inner bevel, softer digits; the shotgun's in its own bezel). Off: the old square screens."),
        toggle("White Ammo Screen Text", "vr_ammo_screen_text_white").help("The numbers on the ammo screens (and the ammo pouch's counter) near-white, as the wrist gadget's, for readability; their frame and glow keep the screen's colour. Off: in the screen's colour."),
        row(opt::vr_ammo_mag_scale),
        row(opt::vr_ammo_total_scale),
        row(opt::vr_ammo_total_bright),
        row(opt::vr_ammo_color_warn),
        row(opt::vr_ammo_color_low).developer(),
        row(opt::vr_ammo_color_empty),
        row(opt::vr_ammo_low_frac).developer(),
        row(opt::vr_weapon_screen_idle),
        row(opt::vr_weapon_world_attach_range),
        row(opt::vr_weapon_world_attach_max),
        open("Ammo Screen Font", pageIndex(pageFontSurface<2>)).help("The ammo screens' numbers and text (upscaling, outline, shadow)."),
        header("Map Boards"),
        row(opt::vr_worldtext_crt),
        row(opt::vr_worldtext_hue).developer(),
        open("World Text Font", pageIndex(pageFontSurface<3>)).help("The world's texts: map boards, floating texts, tips and the tools' hints (upscaling, outline, shadow)."),
    };
}

// Split from Wrist Gadget: the colours of the player's effects.
[[nodiscard]] za::Vector<Item> pageColours()
{
    return {
        row(opt::vr_player_hue),
        row(opt::vr_player_saturation),
        header("Wrist Gadget"),
        hueSlider("Screen Hue", vr_gadget_screen_hue)
            .help("The screen's colour (and your weapons' screens'). Player's: the Player Effects Hue."),
        row(opt::vr_gadget_screen_brightness),
        row(opt::vr_gadget_screen_background),
        row(opt::vr_gadget_screen_text_white),
        row(opt::vr_gadget_tint),
        row(opt::vr_gadget_tint_hue),
        header("Effects"),
        hueSlider("Weapon Sight Hue", vr_sight_hue)
            .help("The glowing iron sights of the shotgun and double shotgun: 30 their own orange, 0 red, 120 green, 240 blue. Player's: the Player Effects Hue."),
        row(opt::vr_sight_saturation),
        hueSlider("Force Grab Hue", vr_forcegrab_hue)
            .help("The force grab's beam, energy tendril, glow and sparkles: 215 its old blue. Player's: the Player Effects Hue."),
        row(opt::vr_forcegrab_saturation),
        hueSlider("Teleport Arc Hue", vr_teleport_hue)
            .help("Where the teleport arc can land (red where it can't): 220 its old blue. Player's: the Player Effects Hue."),
        hueSlider("Crosshair Hue", vr_crosshair_hue).help("The laser crosshair: 0 its old red. Player's: the Player Effects Hue."),
        hueSlider("Menu Laser Hue", vr_menu_laser_hue).help("The menu's pointer: 35 its old amber. Player's: the Player Effects Hue."),
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
// sv_random_seed...) stay in the console: they mean nothing in the headset.

#include "vr_menu_checklist.inc"



[[nodiscard]] za::Vector<Item> pageForceGrab()
{
    return {
        row(opt::vr_forcegrab_mode),
        row(opt::vr_forcegrab_bodies_block),
        row(opt::vr_forcegrab_catch_late),
        row(opt::vr_forcegrab_eligible_particles),
        row(opt::vr_forcegrab_eligible_haptics),
        row(opt::vr_forcegrab_fx),
        row(opt::vr_forcegrabbable_box_scale),
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
        row(opt::vr_grapple_rope),
        row(opt::vr_grapple_trigger_release),
        row(opt::vr_grapple_drop_grace),
        row(opt::vr_grapple_shootable),
        row(opt::vr_grapple_quick_time),

        header("Buttons on the Gun"),
        slider("Back Button Along", grappleButton(weapons::Key::WpnButtonX), -20.f, 20.f, 0.1f, "%.1f").extend(-60.f, 60.f)
            .help("Where the back button (the detach) is on the gun: towards the muzzle (model units; the grappling gun's "
                  "Weapon Offsets button place). The front button moves with it."),
        slider("Back Button Side", grappleButton(weapons::Key::WpnButtonY), -10.f, 10.f, 0.1f, "%.1f").extend(-40.f, 40.f)
            .help("Towards the gun's left (model units)."),
        slider("Back Button Up", grappleButton(weapons::Key::WpnButtonZ), -10.f, 10.f, 0.1f, "%.1f").extend(-40.f, 40.f)
            .help("Up (model units)."),
        row(opt::vr_grapple_front_button),

        header("Rope"),
        row(opt::vr_grapple_rope_sim),

        header("Reel"),
        row(opt::vr_grapple_unreel_airborne),

        header("Monsters"),
        row(opt::vr_grapple_stagger),
        row(opt::vr_grapple_stamina),

        header("Feel"),
        row(opt::vr_grapple_haptics),
        row(opt::vr_grapple_sag),
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
        row(opt::vr_hull_edge_probe),
        row(opt::vr_hull_brushmodels),
        header("Monsters, Players and Boxes"),
        cycle("Width Against Them", vr_hull_ent_width, ents)
            .help("How wide you are against monsters, other players and solid boxes, both ways (you walking into them, "
                  "them walking into you). Shots and missiles hit the width below; items are picked up with Quake's 32 box."),
        cycle("Width Shots Hit", vr_hull_hit_width, hits)
            .help("How wide you are to monsters' shots and missiles (bullets, nails, lasers, grenades, rockets, spit, "
                  "vore balls). Quake's box is 32; 24 still leaves them a bit of leniency. Melee blows and splash "
                  "damage go by distance, not by this box; monsters see and chase you as before."),
        row(opt::vr_hull_hit_head),
        header("Crouching"),
        row(opt::vr_crouch_height),
        row(opt::vr_crouch_sight),
        row(opt::vr_crouch_step),
        command("Crouch Status", "vr_crouch_status")
            .help("Prints your eyes' height over your feet, your box's height (0 standing) and whether you could stand."),
        header("Lying Down"),
        row(opt::vr_prone),
        row(opt::vr_prone_eye),
        row(opt::vr_prone_rise_margin),
        row(opt::vr_prone_height),
        row(opt::vr_prone_speed_scale),
        row(opt::vr_prone_crawl),
        row(opt::vr_prone_crawl_hand),
        row(opt::vr_prone_stick_scale),
        row(opt::vr_prone_crawl_gain),
        row(opt::vr_prone_crawl_min_speed),
        row(opt::vr_prone_crawl_plant_height),
        row(opt::vr_prone_crawl_friction),
        row(opt::vr_prone_crawl_max_speed),
        row(opt::vr_prone_crawl_haptic),
        command("Lying Down Status", "vr_prone_status")
            .help("Prints whether you lie down, your box's height, your eyes' height over your feet against the limit, "
                  "your speed, and whether a crouch would fit (you could get up)."),
        header("Low Ceilings"),
        row(opt::vr_lowceil),
        row(opt::vr_lowceil_margin),
        row(opt::vr_lowceil_hysteresis),
        row(opt::vr_lowceil_fade),
        row(opt::vr_lowceil_dark),
        row(opt::vr_lowceil_blur),
        row(opt::vr_lowceil_message),
        command("Low Ceiling Status", "vr_lowceil_status")
            .help("Prints whether a low ceiling holds you (1 lying, 2 crouched), whether your eyes are past it, the "
                  "view's darkness and blur, your eyes' height over your feet, the ceiling's and your box's."),
        row(opt::vr_box3d_player_radius),
        header("Standing on Props"),
        row(opt::vr_box3d_player_stand),
        row(opt::vr_box3d_player_slope),
        row(opt::vr_box3d_player_mass),
        row(opt::vr_box3d_player_jump_push),
        row(opt::vr_box3d_player_shove),
        row(opt::vr_box3d_monster_push_speed),
        row(opt::vr_box3d_player_unstick),
        row(opt::vr_box3d_player_hold),
        row(opt::vr_box3d_player_shape),
        row(opt::vr_box3d_shot_shape),
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
        row(opt::vr_mhull),
        row(opt::vr_mhull_ents),
        row(opt::vr_mhull_ledges),
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

// (The pages: pages[] and their builders, vr_menu_table.inc.)
static_assert(mgroups::paramCount == 33, "a Monster Groups page for each trigger and action (the rows above)");
// VR Settings' sections' pages, in its order: with it, the rows Reset All resets besides the vr_* settings, and the
// pages the wrappers (Turning Mode, Move Towards, the hands, the holsters) are synced on as they are built.
const PageRef vrSettingsParts[] = {pageVrsHeightHands, pageVrsBodyEquipment, pageVrsAmmoPouch, pageVrsHolsters,
    pageVrsLocoControls, pageVrsComfortTurning, pageVrsWeaponsBulletTime, pageVrsItemCollection, pageVrsBodyFlashlight, pageVrsAppearance, pageVrsHudHapticsSound,
    pageVrsDisplayGraphics};

[[nodiscard]] bool vrSettingsPage(int p)
{
    if(p == PageMain)
    {
        return true;
    }
    for(const PageRef b : vrSettingsParts)
    {
        if(PageRef::at(p) == b)
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
[[nodiscard]] bool weaponOffsetsPage(PageRef build)
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

// Whether Reset All to Defaults was pressed once (the row reads Press Again to Reset All).
[[nodiscard]] bool resetAllIsArmed()
{
    return ui().resetAllArmed;
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
    showWrapper(vr_menu_move_towards, mode >= 1 && mode <= 4 ? static_cast<float>(mode)
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
        Cvar_SetValueQuick(&vr_movement_mode, CLAMP(1.f, var->value, 4.f));
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

// Randomize (Player Appearance; vr_appearance_random): a new skin tone, gloves, nails and body shape, from the client's own rand()
// (so each press differs). Only these settings: not the gloves' thickness, paint or relief, nor anything else.
// Weights: gloves none 50 / fingerless 25 / full 25; nails on 50; the feminine body 25, feminine hands with it (else
// 10), the feminine voice 90 with it (else 10); the colours below are picked by percent.
// (The engine's rand() is never seeded, so the first press of every session would repeat: this has its own xorshift
// state, seeded once from the real clock; in the tests a fixed seed: vr_appearance_random <seed>.)
uint32_t randomState{0};
int randomOtherShape{1}; // the last body shape other than the feminine one Randomize saw (vr_body_build)

[[nodiscard]] uint32_t randomNext()
{
    if(randomState == 0)
    {
        randomState = static_cast<uint32_t>(Sys_DoubleTime() * 1000000.0) | 1u;
    }
    return vrand::xorshift32(randomState);
}

[[nodiscard]] float randomRange(float lo, float hi)
{
    return lo + (hi - lo) * (static_cast<float>(randomNext() % 10001) / 10000.f);
}

[[nodiscard]] int randomPercent()
{
    return static_cast<int>(randomNext() % 100);
}

void randomAppearance()
{
    // Skin tone: the mean of three rolls over the range (a bell round the authored skin, the extremes rare).
    const float tone = (randomRange(-1.f, 1.f) + randomRange(-1.f, 1.f) + randomRange(-1.f, 1.f)) / 3.f;
    Cvar_SetValueQuick(&vr_skin_tone, roundf(tone * 100.f) / 100.f);

    const int g = randomPercent();
    const int gloves = g < 50 ? 0 : (g < 75 ? 1 : 2);
    Cvar_SetValueQuick(&vr_gloves, static_cast<float>(gloves));
    const int c = randomPercent(); // leather: 55 brown/tan, 15 black/grey, 22 muted any hue, 8 bright
    float h = randomRange(0.f, 360.f);
    float s;
    float l;
    if(c < 55)
    {
        h = randomRange(15.f, 40.f);
        s = randomRange(0.25f, 0.6f);
        l = randomRange(0.12f, 0.42f);
    }
    else if(c < 70)
    {
        s = randomRange(0.f, 0.12f);
        l = randomRange(0.05f, 0.2f);
    }
    else if(c < 92)
    {
        s = randomRange(0.15f, 0.45f);
        l = randomRange(0.15f, 0.35f);
    }
    else
    {
        s = randomRange(0.6f, 0.9f);
        l = randomRange(0.3f, 0.5f);
    }
    Cvar_SetValueQuick(&vr_glove_hue, roundf(h));
    Cvar_SetValueQuick(&vr_glove_saturation, roundf(s * 100.f) / 100.f);
    Cvar_SetValueQuick(&vr_glove_lightness, roundf(l * 100.f) / 100.f);

    const bool nails = randomPercent() < 50;
    Cvar_SetValueQuick(&vr_nails, nails ? 1.f : 0.f);
    if(nails)
    {
        const int n = randomPercent(); // polish: 35 reds, 20 pinks, 20 nudes, 13 black, 12 vivid any hue
        h = randomRange(0.f, 360.f);
        if(n < 35)
        {
            h = fmodf(randomRange(347.f, 370.f), 360.f);
            s = randomRange(0.7f, 0.95f);
            l = randomRange(0.28f, 0.42f);
        }
        else if(n < 55)
        {
            h = randomRange(320.f, 345.f);
            s = randomRange(0.5f, 0.9f);
            l = randomRange(0.5f, 0.72f);
        }
        else if(n < 75)
        {
            h = randomRange(10.f, 30.f);
            s = randomRange(0.25f, 0.5f);
            l = randomRange(0.6f, 0.8f);
        }
        else if(n < 88)
        {
            s = randomRange(0.f, 0.15f);
            l = randomRange(0.04f, 0.14f);
        }
        else
        {
            s = randomRange(0.7f, 1.f);
            l = randomRange(0.35f, 0.55f);
        }
        Cvar_SetValueQuick(&vr_nail_hue, roundf(h));
        Cvar_SetValueQuick(&vr_nail_saturation, roundf(s * 100.f) / 100.f);
        Cvar_SetValueQuick(&vr_nail_lightness, roundf(l * 100.f) / 100.f);
        Cvar_SetValueQuick(&vr_nail_gloss, roundf(randomRange(0.3f, 1.f) * 100.f) / 100.f);
    }

    // The body shape (vr_body_build; rolled last, so a seed's rolls above stay as they were): feminine one time in four,
    // else the last other shape you had this session (athletic if none).
    const int shape = static_cast<int>(vr_body_build.value);
    if(shape != 3)
    {
        randomOtherShape = shape;
    }
    const int newShape = randomPercent() < 25 ? 3 : randomOtherShape;
    Cvar_SetValueQuick(&vr_body_build, static_cast<float>(newShape));
    Con_Printf("Randomized: body shape %d\n", newShape);
    // The hands' shape (vr_hand_shape; rolled after the body's, every press, so the rolls stay in step): the feminine
    // hands with the feminine body, else one time in ten (so the two usually match).
    const bool feminine = randomPercent() < 10 || newShape == 3;
    Cvar_SetValueQuick(&vr_hand_shape, feminine ? 1.f : 0.f);
    // The voice (vr_voice; rolled last): feminine nine times in ten with the feminine body, else one time in ten.
    const int v = randomPercent();
    const bool feminineVoice = newShape == 3 ? v < 90 : v < 10;
    Cvar_SetValueQuick(&vr_voice, feminineVoice ? 1.f : 0.f);
    Con_Printf("Randomized: %s voice\n", feminineVoice ? "feminine" : "masculine");
    static constexpr const char* glovesName[] = {"no gloves", "fingerless gloves", "full gloves"};
    Con_Printf("Randomized: skin tone %.2f, %s (hue %.0f sat %.2f light %.2f), nails %s (hue %.0f sat %.2f light %.2f "
               "gloss %.2f), %s hands\n",
        vr_skin_tone.value, glovesName[gloves], vr_glove_hue.value, vr_glove_saturation.value,
        vr_glove_lightness.value, nails ? "on" : "off", vr_nail_hue.value, vr_nail_saturation.value,
        vr_nail_lightness.value, vr_nail_gloss.value, feminine ? "feminine" : "standard");
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
    for(const cvar_t* kept : {&vr_menu_level, &vr_menu_positions, &vr_setup_pending, &vr_enabled,
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
    if(cursor < 0 || cursor >= static_cast<int>(list.size()) || !(selectable(list[cursor]) || list[cursor].kind == Item::Header))
    {
        return {}; // (a header: the cursor is on it while sections fold)
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
        if(!(selectable(list[i]) || list[i].kind == Item::Header) || a.label != rowLabel(list[i]))
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
    bool qcTests[pageCount]{}; // whether the progs had the QC tests then (qcTestsPresent: the Tests pages' notice)
    auto members() { return qvr::mem::list(built, done, level, cvars, qcTests); }
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
        &var == &vr_gore_preset || &var == &vr_gameplay_preset || &var == &vr_classic_quake ||
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
[[nodiscard]] bool slotPage(PageRef build);

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
[[nodiscard]] int letterSize(const Item& item); // (Menu Look's sizes: below)

// Under every page: Reset This Page (a page with settings), and Menu Detail, the level the pages are shown at.
void addMenuDetail(za::Vector<Item>& list, int page)
{
    const bool settings = za::anyOf(list.begin(), list.end(), [](const Item& item) {
        return item.cvar && item.kind != Item::Action && item.cvar != &vr_menu_level;
    });
    if(page == pageIndex(pageSearch) || page == pageIndex(pageConsole) || page == pageIndex(pageMaps))
    {
        return; // (drawn their own way: vr_menu_search.inc, vr_menu_console.inc, vr_menu_maps.inc)
    }
    if(settings && !slotPage(PageRef::at(page)) && page != pageIndex(pageChanged))
    {
        // (Both footers smaller and fainter than the page's own sections: Item::Minor, vr_menu_minor_scale and _alpha.)
        list.pushBack(header("This Page").minor());
        list.pushBack(action(ui().resetArmedPage == page ? "Press Again to Reset" : "Reset This Page", resetThisPage)
                .help("Every setting on this page (as Menu Detail shows it) back to its default. Press it twice: the second "
                      "time within 3 seconds.").minor());
    }
    list.pushBack(header("Menu Detail").minor());
    list.pushBack(row(opt::vr_menu_level).minor());
}

// Folding sections (vr_menu_fold; ROUND25.md, "Menu look"): a header pressed (Enter, a click, the laser's trigger;
// left folds, right unfolds) hides its section's rows, which slide and fade (vr_menu_fold_time); the folds are kept in
// vr_menu_folded ("page title|header" separated by ';'). The rows stay in the page's list (menu paths, Search and
// menu_vr find them): folded, they have no height and the cursor's steps pass them; a row selected anyway (a path,
// Search, menu_vr <page> <row>) unfolds its section (unfoldAtCursor). Not on the Checklist (its own folds).
struct Folds
{
    za::String parsed;            // vr_menu_folded as last read
    float on{-1.f};               // vr_menu_fold as last read
    int version{1};               // bumped on a change: each page's rows marked again
    int applied[pageCount]{};     // the version each page's rows were marked for
    int appliedBuild[pageCount]{}; // and its build
    za::Vector<float> open[pageCount]; // each row's openness as drawn (0 folded, 1 open; a header's: its arrow's)
    int openBuild[pageCount]{};   // the build `open` is for
    double stepped[pageCount]{};  // realtime of its last step
};
Folds folds;

[[nodiscard]] bool foldsOn(int page)
{
    return vr_menu_fold.value != 0.f && page != pageIndex(pageChecklist);
}

[[nodiscard]] za::String foldKey(int page, const char* header)
{
    za::String key{pages[page].title};
    key += "|";
    key += header ? header : "";
    return key;
}

// Whether vr_menu_folded names `key` (one of its ';'-separated entries).
[[nodiscard]] bool foldListed(za::StringView list, const za::String& key)
{
    for(size_t start = 0; start <= list.size();)
    {
        size_t end = list.find(';', start);
        end = end == za::StringView::nPos ? list.size() : end;
        if(end - start == key.size() && !strncmp(list.data() + start, key.cStr(), key.size()))
        {
            return true;
        }
        start = end + 1;
    }
    return false;
}

// Section `header` of `page` folded or not (vr_menu_folded written; the rows marked again at the next items()).
void setFolded(int page, const char* header, bool fold)
{
    const za::String key = foldKey(page, header);
    const za::StringView list{vr_menu_folded.string};
    za::String next;
    for(size_t start = 0; start <= list.size();)
    {
        size_t end = list.find(';', start);
        end = end == za::StringView::nPos ? list.size() : end;
        const za::StringView entry = list.substrByPosLen(start, end - start);
        if(!entry.empty() && !(entry.size() == key.size() && !strncmp(entry.data(), key.cStr(), key.size())))
        {
            next += next.empty() ? "" : ";";
            next += entry;
        }
        start = end + 1;
    }
    if(fold)
    {
        next += next.empty() ? "" : ";";
        next += key;
    }
    Cvar_Set("vr_menu_folded", next.cStr());
}

// The page's rows marked (Item::folded) as vr_menu_folded has them: a header, whether its section is folded (its
// arrow); a row, whether it is in a folded section.
void applyFolds(int page, za::Vector<Item>& list)
{
    if(folds.parsed != vr_menu_folded.string || folds.on != vr_menu_fold.value)
    {
        folds.parsed = vr_menu_folded.string;
        folds.on = vr_menu_fold.value;
        folds.version++;
    }
    if(folds.applied[page] == folds.version && folds.appliedBuild[page] == builds[page])
    {
        return;
    }
    folds.applied[page] = folds.version;
    folds.appliedBuild[page] = builds[page];
    const bool on = foldsOn(page);
    bool folded = false;
    for(Item& item : list)
    {
        if(item.kind == Item::Header)
        {
            folded = on && foldListed(za::StringView{vr_menu_folded.string}, foldKey(page, item.label));
        }
        item.folded = folded && item.look != Item::Button; // (a button stands apart from the section above it)
    }
}

// Where the cursor can rest as it steps (up, down, the mouse over a row): a setting outside a folded section, or a
// section's header while sections fold.
[[nodiscard]] bool navigable(const Item& item)
{
    return item.kind == Item::Header ? foldsOn(ui().page) : selectable(item) && !item.folded;
}

// The header above row `i` (-1: none).
[[nodiscard]] int headerOf(const za::Vector<Item>& list, int i)
{
    for(; i >= 0; i--)
    {
        if(list[i].kind == Item::Header)
        {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] const za::Vector<Item>& items(int page)
{
    za::Vector<Item> (&built)[pageCount] = menuPages.built;
    bool (&done)[pageCount] = menuPages.done;
    const bool weaponOffsets = weaponOffsetsPage(PageRef::at(page));
    if(vrSettingsPage(page))
    {
        syncWrappers(); // Turning Mode, Move Towards, the hands: as their settings are now
    }
    if(page == pageIndex(pageVrsComfortTurning) && mainPageSnap >= 0 &&
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
    if(page == pageIndex(pageWofsSights) && weaponOffsetsSightVersion != sightalign::version())
    {
        weaponOffsetsStale = true; // Align Sights to My Aim: its phase or its result changed
        weaponOffsetsSightFocus = true;
    }
    if(page == pageIndex(pageGraphicsRelighting) && relightToolState >= 0 && relightToolState != relight::toolPageState())
    {
        done[page] = false; // light.exe found or not, a download started or ended: Download ericw-tools shown or not
    }
    if(page == pageIndex(pageCampaigns) && campaignsBloodyShown >= 0 &&
        campaignsBloodyShown != (vr_mg3_bn_discovered.value != 0.f ? 1 : 0))
    {
        done[page] = false; // Dawn of the Machine's Bloody Nightmare found (or reset): its row shown or not
    }
    if(page == pageIndex(pageRunSetup) && runSetupStale())
    {
        done[page] = false; // Run Setup: a row held by another's choice (or let go), or shown, or what starts changed
    }
    if(page == pageIndex(pageBodyArms) && armsPageCalibrated >= 0 && armsPageCalibrated != (bodycal::calibrated() ? 1 : 0))
    {
        done[page] = false; // calibrated (Apply) or not (Undo): Arm Length shown or not
    }
    if(page == pageIndex(pageBodyCalibration) &&
        (bodycalVersion != bodycal::version() || bodycalSeated != (vr_bodycal_seated.value != 0.f ? 1 : 0)))
    {
        done[page] = false; // Body Calibration: its phase, its result or its poses changed
    }
#ifndef QVR_RELEASE
    if(page == pageIndex(pageTunables) && tunablesPageKey != tunablesPageKeyNow())
    {
        done[page] = false; // Show or Page changed, or Search's index (every row)
    }
#endif
    if(page == pageIndex(pageChecklist))
    {
        checklist::refresh(); // (the file looked at once a second)
        if(checklistGeneration != checklist::generation() || checklistHidden != (vr_checklist_hide_ticked.value != 0.f ? 1 : 0))
        {
            done[page] = false; // an item ticked, Hide Ticked, or the file edited
        }
    }
    if(weightPageStale(PageRef::at(page)))
    {
        done[page] = false;
    }
    if(weaponOffsets && weaponOffsetsStale)
    {
        weaponOffsetsStale = false;
        done[page] = false;
    }
    if((page == pageIndex(pageFlashlightLowGrip) && flashlightPageManual[0] >= 0 &&
           (vr_flashlight_low_fingers.value >= 0.5f ? 1 : 0) != flashlightPageManual[0]) ||
        (page == pageIndex(pageFlashlightOverheadGrip) && flashlightPageManual[1] >= 0 &&
            (vr_flashlight_high_fingers.value >= 0.5f ? 1 : 0) != flashlightPageManual[1]))
    {
        done[page] = false; // a grip's Fingers choice: its curls or its overlap shown
    }
    if(page == pageIndex(pageHandGunCalibration) && handCalPageOwn >= 0 &&
        (vr_handcal_off_mirror.value == 0.f ? 1 : 0) != handCalPageOwn)
    {
        done[page] = false; // the off hand's own sliders shown or not
    }
    if(page == pageIndex(pageDebugTests) && testsPageSpawn != static_cast<int>(vr_test_spawn.value))
    {
        done[page] = false; // another Thing chosen (or set by a spawn row): Put It There locked or not
    }
    if(page == pageIndex(pageVrsItemCollection) && itemCollectionPageMode != static_cast<int>(vr_menu_pickup_mode.value))
    {
        done[page] = false; // another pickup mode: Backpacks Too dimmed or not
    }
    if(page == pageIndex(pageMotionRecorder) && motionPageCategory != static_cast<int>(vr_motion_category.value))
    {
        done[page] = false; // the Detail choice is the category's
    }
    if((page == pageIndex(pageReviewTakes) && reviewListGeneration != motion::review::generation()) ||
        (page == pageIndex(pageReviewTake) && (reviewTakeGeneration != motion::review::generation() ||
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
    if(menuPages.qcTests[page] != qcTestsBuilt())
    {
        done[page] = false; // other progs loaded since (with or without the QC tests): the Tests pages' notice
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
        menuPages.qcTests[page] = qcTestsBuilt();
        if(page == pageIndex(pageDebugTests) || page == pageIndex(pageDebugAutomation))
        {
            // Progs built without the QC tests (QVR_NO_TESTS, the release flavour): the rows below set cvars nothing reads.
            if(!qcTestsBuilt())
            {
                built[page].pushBack(info(qcTestsNotice));
            }
        }
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
        // (A section's header is a place too while sections fold, but not where a page first opens: its first setting.)
        const auto rests = [&](const Item& item) {
            return item.kind == Item::Header ? foldsOn(page) && anchor.valid : selectable(item);
        };
        for(int i = cursor; n > 0 && !rests(built[page][cursor]) && i >= 0; i--)
        {
            cursor = rests(built[page][i]) ? i : cursor;
        }
        for(int i = cursor; n > 0 && !rests(built[page][cursor]) && i < n; i++)
        {
            cursor = rests(built[page][i]) ? i : cursor;
        }
    }
    applyFolds(page, built[page]);
    // Once the page is shown again (the menu reopened on it, its cursor restored): the cursor on Apply or Undo.
    if(page == pageIndex(pageWofsSights) && weaponOffsetsSightFocus && key_dest == key_menu && m_state == m_vr &&
        page == qvr::menu::currentPage())
    {
        // (Its row as the page recorded it, not found by its action's address: identical functions share one.)
        weaponOffsetsSightFocus = false;
        if(weaponOffsetsSightFocusRow >= 0 && weaponOffsetsSightFocusRow < static_cast<int>(built[page].size()))
        {
            ui().cursors[page] = weaponOffsetsSightFocusRow;
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
[[nodiscard]] float pageRight(PageRef build)
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
    if(float labelLeft, labelTop; menuui::versionLabelClearance(labelLeft, labelTop) && pageRight(PageRef::at(ui().page)) > labelLeft)
    {
        bottom = q_min(bottom, static_cast<int>(za::floor(labelTop)));
    }
    // Under the title, or below the corner's buttons where they are over the rows (not left of them: a narrow panel),
    // and below the status box in the top right corner where the page reaches under it (Search and the Map Library,
    // from x 12 440 and 460 across; the other pages to their scrollbar).
    int listTop = menuui::toolbarBeside() ? top + 36 : q_max(top + 36, static_cast<int>(za::ceil(menuui::toolbarBottom())) + 2);
    const auto build = PageRef::at(ui().page);
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
            const int dim = item.kind == Item::Header || item.look != Item::Normal ? letterSize(item) : 8;
            const int x = item.kind == Item::Header ? (320 - dim * len) / 2 - dim - 10 // (its fold arrow and bar left of it)
                          : item.kind == Item::Info || item.look == Item::Button ? 0
                                                    : midPos - 28 - dim * len;
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

// Menu Look's sizes (ROUND25.md, "Menu look"): a row's letters over Quake's 8 pixels. A section's header bigger
// (vr_menu_header_scale), the footer's sections smaller (Item::Minor: vr_menu_minor_scale), Run Setup's Difficulty
// and Start bigger (Item::Big, Item::Button: vr_runsetup_big).
[[nodiscard]] float lookScale(const Item& item)
{
    switch(item.look)
    {
        case Item::Minor: return CLAMP(0.5f, vr_menu_minor_scale.value, 1.f);
        case Item::Big:
        case Item::Button: return CLAMP(1.f, vr_runsetup_big.value, 2.f);
        default: return item.kind == Item::Header ? CLAMP(1.f, vr_menu_header_scale.value, 2.f) : 1.f;
    }
}

// Its letters' size, pixels.
[[nodiscard]] int letterSize(const Item& item)
{
    return static_cast<int>(za::round(8.f * lookScale(item)));
}

// A row's height unfolded: 8, its letters' for a footer's or a big row, a button's with room round its label. A header
// stays 8 high: its bigger letters reach up into the gap above it (rowGap).
[[nodiscard]] int fullHeight(const Item& item)
{
    if(item.look == Item::Button)
    {
        return letterSize(item) + 8;
    }
    if(item.kind == Item::Header)
    {
        return item.look == Item::Minor ? letterSize(item) : 8;
    }
    return item.look == Item::Normal ? 8 : letterSize(item);
}

// How far open row `i` is as drawn (0 folded .. 1; animated: stepFolds). A header's: its arrow's turn (its own height
// never changes).
[[nodiscard]] float rowOpen(const za::Vector<Item>& list, int i)
{
    const int page = ui().page;
    if(&list == &menuPages.built[page] && folds.openBuild[page] == builds[page] &&
        folds.open[page].size() == list.size())
    {
        return folds.open[page][static_cast<size_t>(i)];
    }
    return list[i].folded ? 0.f : 1.f;
}

[[nodiscard]] int rowHeight(const za::Vector<Item>& list, int i)
{
    const float open = list[i].kind == Item::Header ? 1.f : rowOpen(list, i);
    return open >= 1.f ? fullHeight(list[i]) : static_cast<int>(za::round(static_cast<float>(fullHeight(list[i])) * open));
}

// The space above row `i`: a section's header's (vr_menu_section_gap, and its bigger letters' room; a footer's
// smaller), a button's (Run Setup's Start: a gap of its own, apart from the section above it).
[[nodiscard]] int rowGap(const za::Vector<Item>& list, int i)
{
    const Item& item = list[i];
    if(item.kind == Item::Header)
    {
        return item.look == Item::Minor ? static_cast<int>(za::round(static_cast<float>(sectionGap()) * lookScale(item)))
                                        : sectionGap() + letterSize(item) - 8;
    }
    return item.look == Item::Button && rowOpen(list, i) > 0.f ? sectionGap() + 6 : 0;
}

// The fold animation: each row of the page shown moved towards open or folded (vr_menu_fold_time), at most once a
// frame; a page built anew starts as it is.
void stepFolds(int page, const za::Vector<Item>& list)
{
    za::Vector<float>& open = folds.open[page];
    const double now = realtime;
    const bool fresh = folds.openBuild[page] != builds[page] || open.size() != list.size();
    const float dt = static_cast<float>(now - folds.stepped[page]);
    folds.stepped[page] = now;
    folds.openBuild[page] = builds[page];
    if(fresh)
    {
        open.clear();
    }
    const float time = CLAMP(0.f, vr_menu_fold_time.value, 0.5f);
    const float step = fresh || time <= 0.f ? 1.f : CLAMP(0.f, dt / time, 1.f);
    for(size_t i = 0; i < list.size(); i++)
    {
        const float target = list[i].folded ? 0.f : 1.f;
        if(fresh)
        {
            open.pushBack(target);
            continue;
        }
        open[i] = open[i] < target ? za::fmin(open[i] + step, target) : za::fmax(open[i] - step, target);
    }
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
        y += rowGap(list, i) + rowHeight(list, i);
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
        y += rowGap(list, k) + rowHeight(list, k);
    }
    return y + rowGap(list, i);
}

// The scroll that shows row `last` at the list's bottom: the first row of as many as fit up to it.
[[nodiscard]] int scrollShowing(const za::Vector<Item>& list, int last)
{
    const int space = listSpace(list);
    int y = rowGap(list, last) + rowHeight(list, last);
    int first = last;
    while(first > 0 && y + rowGap(list, first - 1) + rowHeight(list, first - 1) <= space)
    {
        first--;
        y += rowGap(list, first) + rowHeight(list, first);
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

// The first and last places the cursor steps to (a header among them while sections fold).
[[nodiscard]] int firstSelectable(const za::Vector<Item>& list)
{
    for(int i = 0; i < static_cast<int>(list.size()); i++)
    {
        if(navigable(list[i]))
        {
            return i;
        }
    }
    return 0;
}

// The first setting outside a folded section (where a page opens), else the first place.
[[nodiscard]] int firstSetting(const za::Vector<Item>& list)
{
    for(int i = 0; i < static_cast<int>(list.size()); i++)
    {
        if(selectable(list[i]) && !list[i].folded)
        {
            return i;
        }
    }
    return firstSelectable(list);
}

void moveCursor(const za::Vector<Item>& list, int dir)
{
    const int n = static_cast<int>(list.size());
    int& cursor = ui().cursors[ui().page];
    int i = cursor;
    do
    {
        i = (i + dir + n) % n;
    } while(!navigable(list[i]) && i != cursor);
    cursor = i;
}

[[nodiscard]] int lastSelectable(const za::Vector<Item>& list)
{
    for(int i = static_cast<int>(list.size()) - 1; i >= 0; i--)
    {
        if(navigable(list[i]))
        {
            return i;
        }
    }
    return 0;
}

// The mouse wheel on a page with a scroll bar (his note 2026-11-13: "the wheel scrolls the bar ... instead of moving item
// by item"): the page scrolled by vr_menu_wheel_scroll's part of its rows shown (folded rows not counted), the cursor
// kept on the page (onto its first place scrolling down, its last scrolling up, where it went off). False when the page
// fits or the setting is 0 (the wheel moves the cursor a row, as before).
bool wheelScroll(const za::Vector<Item>& list, int dir)
{
    const int most = maxScroll(list);
    const int step = menuui::wheelRows(visibleRows(list));
    if(most <= 0 || step <= 0)
    {
        return false;
    }
    int& scroll = ui().scrolls[ui().page];
    int& cursor = ui().cursors[ui().page];
    const int n = static_cast<int>(list.size());
    int s = scroll;
    for(int moved = 0; moved < step;)
    {
        const int t = s + dir;
        if(t < 0 || t > most)
        {
            break;
        }
        s = t;
        if(list[s].kind == Item::Header || rowHeight(list, s) > 0)
        {
            moved++;
        }
    }
    scroll = s;
    const int end = q_min(n, s + rowsFrom(list, s));
    if(cursor < s || cursor >= end)
    {
        for(int k = 0; k < end - s; k++)
        {
            const int i = dir > 0 ? s + k : end - 1 - k;
            if(navigable(list[i]))
            {
                cursor = i;
                break;
            }
        }
    }
    return true;
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
    const PageRef b = PageRef::at(page);
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
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        return;
    }
    ui().page = target;
    weightPageShown(PageRef::at(ui().page)); // a weight page: what is in the hand now
    if(weaponOffsetsPage(PageRef::at(ui().page)))
    {
        weaponOffsetsStale = true; // the weapon in hand now
    }
    if(ui().page == pageIndex(pageReviewTakes))
    {
        motion::review::invalidate(); // takes recorded, evaluated or moved since
    }
    // Built again each time it is shown (its row kept): rows that depend on anything not checked in items() (what is
    // changed now: Changed Settings; a state a builder reads) are as they are now, not as when the page was last built
    // (by Search, a menu path or Changed Settings, which build every page).
    menuPages.done[ui().page] = false;
    const auto& list = items(ui().page);
    if(const Item& at = list[ui().cursors[ui().page]]; !selectable(at) && !navigable(at))
    {
        ui().cursors[ui().page] = firstSetting(list); // (a setting in a folded section stays: unfoldAtCursor)
    }
    noteLeft(ui().page, list);
    ui().visits[ui().page] = ++ui().visitCount;
}

bool slotPage(PageRef build)
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
    qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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
        if(p == pageIndex(pageChanged) || slotPage(PageRef::at(p)))
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
    qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
}

// The page listing `p` in the menus' tree (VR Settings: itself).
[[nodiscard]] int homeOf(int p)
{
    return pageHomes.of[p];
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
        qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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
        qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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
            qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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
    if(ui().page == pageIndex(pageBodyCalibration) && qvr::setup::waitsOnMenu())
    {
        // VR Calibration waits on this page (paused, or the body's result not trusted): Back closes the menu, as the
        // menu button does, and the calibration goes on (not up the tree, the menu still open and it still waiting)
        navPush(ui().page);
        menuui::backToGame(HAND_MAIN);
        qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
        return;
    }
    const int dest = homeOf(ui().page);
    navPush(dest);
    showPage(dest);
    qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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

// The settings one mode only uses (the graphics audit, 2026-11-09: they do nothing in the other), by their cvar's name
// wherever a row shows them; a trailing '*' takes the names starting so. The headset's eyes only: their scaling, TAA,
// gamma and contrast, bloom strength, tone curve and grade, underwater look. Flatscreen play only: its look and visor.
constexpr const char* vrOnlyCvars[] = {"vr_render_scale", "vr_dynres*", "vr_upscale*", "vr_foveated*", "vr_taa*",
    "vr_gamma", "vr_contrast", "vr_bloom", "vr_tonemap", "vr_exposure", "vr_grade*", "vr_water_underwater",
    "vr_water_wobble"};
constexpr const char* flatOnlyCvars[] = {"vr_flat_tonemap", "vr_flat_bloom", "vr_flat_grade*", "vr_flat_vignette*",
    "vr_flat_visor*", "vr_flat_taa", "vr_flat_aim_sens*"};

[[nodiscard]] bool cvarNamed(const cvar_t* cvar, const char* pattern)
{
    const size_t n = strlen(pattern);
    return n > 0 && pattern[n - 1] == '*' ? strncmp(cvar->name, pattern, n - 1) == 0 : strcmp(cvar->name, pattern) == 0;
}

// The mode a row's setting is for: its own (vrOnly(), flatOnly()), else its cvar's (the lists above).
[[nodiscard]] Item::Mode rowMode(const Item& item)
{
    if(item.mode != Item::AnyMode || !item.cvar || item.kind == Item::Action)
    {
        return item.mode;
    }
    for(const char* name : vrOnlyCvars)
    {
        if(cvarNamed(item.cvar, name))
        {
            return Item::VrOnly;
        }
    }
    for(const char* name : flatOnlyCvars)
    {
        if(cvarNamed(item.cvar, name))
        {
            return Item::FlatOnly;
        }
    }
    return Item::AnyMode;
}

// A row for the other mode than the game runs in (rowMode): dimmed, its help saying so; still changed from here.
// The headset's: no headset session (flatscreen play, or the desktop without one). Flatscreen play's: not in it.
[[nodiscard]] bool otherModeItem(const Item& item)
{
    switch(rowMode(item))
    {
        case Item::VrOnly: return !vrActive();
        case Item::FlatOnly: return !flatplay::active();
        case Item::AnyMode: break;
    }
    return false;
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

// The Flat Keys page's rows (vr_flatplay.hpp keyCvars: the keys flat play reads itself). A key row waits for a key while
// keyCapture is its cvar (the Keys menu's "press a key": M_VRKeyCapture holds the engine's side of it).
cvar_t* keyCapture{nullptr};

// A row's key where another row has it (a flat key) or a Quake binding does: its name, written to out; false: free.
[[nodiscard]] bool keyConflict(const cvar_t& var, char* out, size_t size)
{
    const int key = var.string[0] ? Key_StringToKeynum(var.string) : -1;
    if(key <= 0 || key >= MAX_KEYS)
    {
        return false;
    }
    constexpr size_t prefix = sizeof("vr_flat_key_") - 1;
    for(const cvar_t* c : flatplay::keyCvars())
    {
        if(c != &var && c->string[0] && Key_StringToKeynum(c->string) == key)
        {
            q_snprintf(out, size, "also %s", c->name + prefix);
            return true;
        }
    }
    if(keybindings[key] && keybindings[key][0])
    {
        q_snprintf(out, size, "Quake: %s", keybindings[key]);
        return true;
    }
    return false;
}

// A key row set to `key` ("" none), the name as the default spells it where it is the default's key (a sparse config).
void setKeyRow(cvar_t* var, const char* name)
{
    Cvar_SetQuick(var, var->default_string && !q_strcasecmp(var->default_string, name) ? var->default_string : name);
}

void drawKeyRow(const Item& item, int y, bool selected)
{
    if(selected && keyCapture == item.cvar)
    {
        M_PrintWhite(midPos, y, (realtime * 4.f - za::floor(realtime * 4.f)) < 0.5f ? "Press a key..." : "Press a key   ");
        return;
    }
    const char* name = item.cvar->string[0] ? item.cvar->string : "---";
    M_Print(midPos, y, name);
    char other[48];
    if(keyConflict(*item.cvar, other, sizeof(other)))
    {
        M_PrintWhite(midPos + 8 * (static_cast<int>(strlen(name)) + 2), y, va("! %s", other));
    }
}

// (`item` lives in the page's list, which an action -- or a cvar's callback -- may build again: what is needed of it is
// read before.)
void change(const Item& item, int dir, bool repeat = false)
{
    if(lockedItem(item))
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust); // the server's rule: not changed from here
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
        case Item::Key:
            if(dir > 0)
            {
                keyCapture = item.cvar; // (the next key is the row's: VR_Menu_Key)
                M_VRKeyCapture(true);
                qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
            }
            return;
        case Item::Action:
            if(dir > 0 && item.buttonPress)
            {
                const int b = CLAMP(0, ui().button, item.buttons - 1);
                if(item.buttonDim && item.buttonDim(item.arg, b))
                {
                    qvr::menuui::uiSound(qvr::menuui::UiSound::Move); // (a dimmed button: Test without a scenario)
                    return;
                }
                qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
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
    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
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
        const float h = static_cast<float>(rowHeight(list, k));
        if(yrel >= y && yrel < y + h)
        {
            return k;
        }
        y += h;
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
        const float h = static_cast<float>(rowHeight(list, k));
        if(yrel >= y && yrel < y + h)
        {
            i = k;
            break;
        }
        y += h;
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
        while(!navigable(list[cursor]) && cursor + dir >= scroll && cursor + dir < scroll + rows)
        {
            cursor += dir;
        }
    }
}

// menu_vr rows (tests, Debug > Tools): the VR page shown, its rows as drawn: MROW|row|top y|label (menu coordinates), then
// MROWS with the rows shown, the scroll and its most, the section gap, and whether the mouse finds each row where it is
// drawn (rowAt, the laser's and the desktop mouse's) and nothing in the gaps.
// menu_vr modes [page] (tests, Debug > Tools): the rows for one mode only (rowMode) on that page (none: every page):
// MMODE|page|label|vr or flat|dimmed now, then "menu modes: <n> VR only, <n> flat only, <n> dimmed (headset <0|1>,
// flatscreen play <0|1>)".
void printModes(const int only)
{
    if(only >= pageCount)
    {
        Con_Printf("menu_vr modes: no page %d\n", only);
        return;
    }
    int vr = 0, flat = 0, dimmed = 0;
    for(int page = only < 0 ? 0 : only; page < (only < 0 ? pageCount : only + 1); page++)
    {
        for(const Item& item : items(page))
        {
            const Item::Mode mode = rowMode(item);
            if(mode == Item::AnyMode)
            {
                continue;
            }
            const bool dim = otherModeItem(item);
            vr += mode == Item::VrOnly;
            flat += mode == Item::FlatOnly;
            dimmed += dim;
            Con_Printf("MMODE|%d|%s|%s|%d\n", page, item.label ? item.label : "", mode == Item::VrOnly ? "vr" : "flat",
                dim ? 1 : 0);
        }
    }
    Con_Printf("menu modes: %d VR only, %d flat only, %d dimmed (headset %d, flatscreen play %d)\n", vr, flat, dimmed,
        vrActive() ? 1 : 0, flatplay::active() ? 1 : 0);
}

// menu_vr wheeltest (Misc/quakevr/menu_wheel_test.sh): the mouse wheel on the VR page shown, which must scroll: a notch
// down scrolls the page by vr_menu_wheel_scroll's part of its rows (the cursor kept on it), notches to its end stop at
// the furthest scroll, a notch up scrolls back; with the setting at 0 a notch moves the cursor a row, the page as it
// was. Puts the page and the setting back. Ends "menu_wheel_test: N passed, M failed".
void wheelTest()
{
    testkit::Tally tally;
    const auto check = [&tally](bool ok, const char* what) {
        if(tally.check(ok, "menu_wheel_test", what))
        {
            Con_Printf("menu_wheel_test: PASS %s\n", what);
        }
    };
    if(m_state != m_vr)
    {
        check(false, "on a VR page");
        tally.verdict("menu_wheel_test");
        return;
    }
    const auto& list = items(ui().page);
    int& scroll = ui().scrolls[ui().page];
    int& cursor = ui().cursors[ui().page];
    const int scrollWas = scroll, cursorWas = cursor;
    const float settingWas = vr_menu_wheel_scroll.value;
    const int most = maxScroll(list);
    char line[160];
    q_snprintf(line, sizeof(line), "the page scrolls (%s: %d rows, furthest scroll %d)", pages[ui().page].title,
        static_cast<int>(list.size()), most);
    check(most > 0, line);
    const auto onPage = [&]() { return cursor >= scroll && cursor < scroll + rowsFrom(list, scroll) && navigable(list[cursor]); };
    if(most > 0)
    {
        scroll = 0;
        cursor = firstSetting(list);
        const int step = menuui::wheelRows(visibleRows(list));
        VR_Menu_Key(K_MWHEELDOWN, 0);
        q_snprintf(line, sizeof(line), "a notch down: scroll 0 -> %d (%d rows a notch, %.2f of %d shown), the cursor on the page (%d)",
            scroll, step, vr_menu_wheel_scroll.value, rowsFrom(list, 0), cursor);
        check(scroll >= q_min(step, most) && scroll <= most && step > 1 && onPage(), line);
        for(int i = 0; i < 200 && scroll < most; i++)
        {
            VR_Menu_Key(K_MWHEELDOWN, 0);
        }
        VR_Menu_Key(K_MWHEELDOWN, 0); // (one more at the end)
        q_snprintf(line, sizeof(line), "notches down stop at the furthest scroll (%d of %d), the cursor on the page", scroll, most);
        check(scroll == most && onPage(), line);
        const int bottom = scroll;
        VR_Menu_Key(K_MWHEELUP, 0);
        q_snprintf(line, sizeof(line), "a notch up: scroll %d -> %d, the cursor on the page", bottom, scroll);
        check(scroll < bottom && onPage(), line);
        Cvar_SetValueQuick(&vr_menu_wheel_scroll, 0.f);
        const int s0 = scroll, c0 = cursor;
        VR_Menu_Key(K_MWHEELDOWN, 0);
        q_snprintf(line, sizeof(line), "vr_menu_wheel_scroll 0: a notch moves the cursor (%d -> %d), the scroll kept (%d)", c0, cursor, scroll);
        check(scroll == s0 && cursor != c0, line);
        Cvar_SetValueQuick(&vr_menu_wheel_scroll, settingWas);
    }
    scroll = scrollWas;
    cursor = cursorWas;
    tally.verdict("menu_wheel_test");
}

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
        const int h = rowHeight(list, i);
        if(h <= 0)
        {
            continue; // (folded: no height, nothing to find)
        }
        const int hit = rowAt(static_cast<float>(top) + static_cast<float>(h) * 0.5f);
        const int expected = i - list[i].partOf >= first ? i - list[i].partOf : -1;
        bad += hit != expected;
        if(rowGap(list, i) > 1 && rowAt(static_cast<float>(top - rowGap(list, i)) + 0.5f) != -1)
        {
            bad++;
        }
    }
    Con_Printf("MROWS|%d shown from %d of %d|scroll most %d|gap %d px|bottom %d of %d|rowAt %s\n", shown, first,
        static_cast<int>(list.size()), maxScroll(list), sectionGap(), shown > 0 ? l.listTop + rowTop(list, first, first + shown - 1) + rowHeight(list, first + shown - 1) : l.listTop,
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
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
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
    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
}

// Sets the hovered choice and closes the list.
void pickDropDown(const Item& item)
{
    const float v = item.choices[CLAMP(0, ui().dropDown.hover, ui().dropDown.count - 1)].value;
    cvar_t* const cvar = item.cvar;
    closeDropDown();
    Cvar_SetValueQuick(cvar, v); // (`item` may be gone after this: a page rebuilt by the setting's change)
    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
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
            qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
            break;
        case K_UPARROW:
        case K_DOWNARROW:
            d.hover = CLAMP(0, d.hover + (key == K_DOWNARROW ? 1 : -1), d.count - 1);
            dropDownShowHover();
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
            break;
        case K_MWHEELUP:
        case K_MWHEELDOWN:
        {
            d.scroll = CLAMP(0, d.scroll + (key == K_MWHEELDOWN ? 1 : -1) * q_max(1, menuui::wheelRows(d.rows)), q_max(d.count - d.rows, 0));
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
                qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        }
    }
    return true;
}

// The row being drawn's fade (drawItem): its own tints (dimmed, ticked) multiplied by it.
float rowFade = 1.f;

void pushRowColor(float r, float g, float b, float a)
{
    GL_PushCanvasColor(r, g, b, a * rowFade);
}

// The selected row's bar (the VR menu style), x0..x1 round letters `dim` tall from y: false where there is none (the
// classic style: Quake's arrow instead; the corner's buttons have the selection).
[[nodiscard]] bool drawRowBar(float x0, float x1, int y, int dim)
{
    return menuui::rowHighlight(x0, x1, static_cast<float>(y) + static_cast<float>(dim) * 0.5f, static_cast<float>(dim) * 0.5f + 1.5f);
}

// The arrow cursor's blinking character (M_DrawArrowCursor's, without its bar).
void drawArrowChar(int x, int y)
{
    M_DrawCharacter(x, y, 12 + (static_cast<int>(realtime * 4) & 1));
}

void drawItemLooked(const Item& item, int y, float open, bool selected);

// Row `item` at y, `open` its fold (0..1: a folding section's rows fade; a header's arrow turns), drawn as its look
// has it (Item::Look): a footer's fainter (vr_menu_minor_alpha).
void drawItem(const Item& item, int y, float open, bool selected)
{
    float fade = item.look == Item::Minor ? CLAMP(0.2f, vr_menu_minor_alpha.value, 1.f) : 1.f;
    if(item.kind != Item::Header && open < 1.f)
    {
        fade *= CLAMP(0.f, (open - 0.35f) / 0.65f, 1.f); // (gone before its row is half folded: no overlap to see)
    }
    if(fade <= 0.f)
    {
        return;
    }
    rowFade = fade;
    if(fade < 1.f)
    {
        GL_PushCanvasColor(1.f, 1.f, 1.f, fade);
    }
    drawItemLooked(item, y, open, selected);
    if(fade < 1.f)
    {
        GL_PopCanvasColor();
    }
    rowFade = 1.f;
}

void drawItemLooked(const Item& item, int y, float open, bool selected)
{
    if(item.kind == Item::Header)
    {
        // Its letters (vr_menu_header_scale) from its row's bottom up into the gap above it; the fold's arrow left of
        // them, turned down while open.
        const int dim = letterSize(item);
        const int len = static_cast<int>(strlen(item.label));
        const int x = (320 - dim * len) / 2;
        const int ty = y + fullHeight(item) - dim;
        const bool fold = foldsOn(ui().page);
        const float arrowX = static_cast<float>(x) - static_cast<float>(dim) * 0.5f - 5.f;
        if(selected && !(vr_menu_hl_fit.value != 0.f && drawRowBar(fold ? arrowX - static_cast<float>(dim) * 0.5f - 4.f : x - 4.f, static_cast<float>(x + dim * len + 4), ty, dim)))
        {
            M_DrawArrowCursor(static_cast<int>(arrowX) - 16, ty + (dim - 8) / 2);
        }
        if(fold)
        {
            menuui::paintFoldArrow(arrowX, static_cast<float>(ty) + static_cast<float>(dim) * 0.5f, static_cast<float>(dim) * 0.32f, open);
        }
        M_PrintWhiteEx(x, ty, dim, item.label);
        return;
    }
    if(item.look == Item::Button)
    {
        // A button of its own across the middle (Run Setup's Start), lit while selected.
        const int dim = letterSize(item);
        const int len = static_cast<int>(strlen(item.label));
        const float w = za::fmax(static_cast<float>(dim * (len + 6)), 120.f);
        const float x0 = 160.f - w * 0.5f;
        const float half = static_cast<float>(fullHeight(item)) * 0.5f;
        const float yc = static_cast<float>(y) + half;
        menuui::paintButton(x0, x0 + w, yc, half, selected);
        if(selected && !menuui::active())
        {
            M_DrawArrowCursor(static_cast<int>(x0) - 12, static_cast<int>(yc) - 4); // (the classic style's arrow)
        }
        M_PrintWhiteEx(160 - dim * len / 2, static_cast<int>(yc) - dim / 2, dim, item.label);
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
            pushRowColor(0.45f, 1.f, 0.45f, alpha);
        }
        else if(tint == 2)
        {
            pushRowColor(1.f, 0.4f, 0.35f, alpha);
        }
        else
        {
            pushRowColor(1.f, 1.f, 1.f, alpha);
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

    // Its letters' size (a footer's smaller, Run Setup's Difficulty bigger: Item::Look), centred in its row as it is
    // unfolded; the selected row's bar from its label to its value's end (vr_menu_hl_fit).
    const int dim = item.look == Item::Normal ? 8 : letterSize(item);
    const int labelX = midPos - 28 - dim * static_cast<int>(strlen(item.label));
    const bool marked = item.cvar && item.kind != Item::Action && changedSetting(*item.cvar);
    const int y8 = y + (fullHeight(item) - 8) / 2; // (the widgets' own size: sliders, switches, key rows)
    y += (fullHeight(item) - dim) / 2;
    int right = midPos;
    const bool locked = lockedItem(item); // the remote server's rule (or unavailable()): dimmed, its value shown
    const bool dimmed = locked || otherModeItem(item); // (the other mode's setting: dimmed alike, still changed here)
    if(dimmed)
    {
        pushRowColor(1.f, 1.f, 1.f, 0.5f);
    }
    M_PrintEx(labelX, y, dim, item.label);
    if(marked)
    {
        // Changed from its default (Changed Settings lists them): left of the label however long (a long label's
        // reaches left of x 0; pageRowsLeft keeps the corner's buttons clear of the mark too).
        M_PrintWhite(labelX - 10, y + (dim - 8) / 2, "*");
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
            if(!menuui::drawSlider(midPos, y8, range, past, text))
            {
                M_DrawSlider(midPos, y8, CLAMP(0.f, range, 1.f), text);
            }
            right = midPos + 96 + 8 * static_cast<int>(strlen(buf));
            break;
        }
        case Item::Cycle:
            if(isToggle(item))
            {
                M_DrawCheckbox(midPos, y8, item.unavailable ? 0.f : valueOf(item)); // a switch in the VR menu style
                right = midPos + 20;
            }
            else
            {
                const char* choice = item.choices[currentChoice(item)].label;
                if(item.look == Item::Big)
                {
                    M_PrintWhiteEx(midPos, y, dim, choice); // (Run Setup's Difficulty: its choice stands out)
                }
                else
                {
                    M_PrintEx(midPos, y, dim, choice);
                }
                right = midPos + dim * static_cast<int>(strlen(choice));
            }
            break;
        case Item::Action:
            M_PrintEx(midPos - 4, y, dim, "...");
            right = midPos - 4 + 3 * dim;
            break;
        case Item::Key:
            drawKeyRow(item, y8, selected);
            right = midPos + 80;
            break;
        default: break;
    }
    if(dimmed)
    {
        GL_PopCanvasColor();
    }

    if(selected)
    {
        if(vr_menu_hl_fit.value != 0.f && drawRowBar(static_cast<float>(labelX - (marked ? 14 : 6)), static_cast<float>(right + 6), y, dim))
        {
            drawArrowChar(midPos - 20, y + (dim - 8) / 2);
        }
        else
        {
            M_DrawArrowCursor(midPos - 20, y + (dim - 8) / 2);
        }
    }
}

#include "vr_menu_help.inc"

// A slider's extent (vr_menu_rows.inc): none, Item::extend() or Item::extend(lo, hi).
struct Extent
{
    int how; // 0 none, 1 four bars' widths, 2 lo..hi
    float lo;
    float hi;

    [[nodiscard]] Item apply(const Item& i) const
    {
        return how == 0 ? i : how == 1 ? i.extend() : i.extend(lo, hi);
    }
};

constexpr Extent noExt{0, 0.f, 0.f};

[[nodiscard]] constexpr Extent ext()
{
    return {1, 0.f, 0.f};
}

[[nodiscard]] constexpr Extent ext(float lo, float hi)
{
    return {2, lo, hi};
}

[[nodiscard]] Item row(opt::Id id)
{
    switch(id)
    {
#define QVR_TOGGLE(name, label, text) \
    case opt::name: return toggle(label, name).help(text);
#define QVR_TOGGLE_BYNAME(name, label, text) \
    case opt::name: return toggle(label, #name).help(text);
#define QVR_SLIDER(name, label, lo, hi, step, format, extent, text) \
    case opt::name: return extent.apply(slider(label, name, lo, hi, step, format)).help(text);
#define QVR_SLIDER_BYNAME(name, label, lo, hi, step, format, extent, text) \
    case opt::name: return extent.apply(slider(label, #name, lo, hi, step, format)).help(text);
#define QVR_CYCLE(name, label, text, ...) \
    case opt::name: return cycle(label, name, {__VA_ARGS__}).help(text);
#define QVR_CYCLE_BYNAME(name, label, text, ...) \
    case opt::name: return cycle(label, #name, {__VA_ARGS__}).help(text);
#define QVR_KEY(name, label, text) \
    case opt::name: return keyRow(label, name).help(text);
#include "vr_menu_rows.inc"
#undef QVR_TOGGLE
#undef QVR_TOGGLE_BYNAME
#undef QVR_SLIDER
#undef QVR_SLIDER_BYNAME
#undef QVR_CYCLE
#undef QVR_CYCLE_BYNAME
#undef QVR_KEY
    case opt::Count: break;
    }
    return header("?");
}
} // namespace menu_detail

#include "vr_menu_paths.inc"
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
extern "C" qboolean M_ListPosition(int* cursor, int* scroll, qboolean set); // menu.c

extern "C" int VR_MenuToggleCloses()
{
    return m_state == m_vr && key_dest == key_menu && ui().page == pageIndex(pageBodyCalibration) &&
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
        qvr::menuui::uiSound(qvr::menuui::UiSound::Select);
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

namespace
{
// The pages' numbers checked (menu_vr ids; at start, quiet unless something is wrong): each page is its number in
// pages[] (vr_menu_table.inc; its builders are each there once and every home is a page: checked while compiling).
// Here: each title once (vr_menu_positions and the boards' menu paths find a page by it), every page's homes reaching
// VR Settings, and the builders the link folded into one address (harmless now: listed to show the check would have
// mattered; ROUND22.md, "Menu pages known by number, not by builder"). Returns the problems found.
int checkPageIds(bool verbose)
{
    int problems = 0;
    for(int p = 0; p < pageCount; p++)
    {
        for(int q = 0; q < p; q++)
        {
            if(!strcmp(pages[p].title, pages[q].title))
            {
                Con_Printf("menu ids: FAIL pages %d and %d are both \"%s\"\n", q, p, pages[p].title);
                problems++;
            }
        }
        int steps = 0;
        for(int h = p; h != PageMain && steps <= pageCount; h = homeOf(h))
        {
            steps++;
        }
        if(steps > pageCount)
        {
            Con_Printf("menu ids: FAIL page %d \"%s\": its homes go round, never reaching VR Settings\n", p, pages[p].title);
            problems++;
        }
    }
    int folded = 0;
    for(int p = 0; p < pageCount; p++)
    {
        for(int q = 0; q < p; q++)
        {
            if(pages[p].build.fn == pages[q].build.fn)
            {
                if(verbose)
                {
                    Con_Printf("menu ids: \"%s\" (%d) and \"%s\" (%d) share one builder's code (folded at link)\n",
                        pages[q].title, q, pages[p].title, p);
                }
                folded++;
                break;
            }
        }
    }
    const int search = pageIndex(pageSearch), console = pageIndex(pageConsole), maps = pageIndex(pageMaps);
    if(search == console || search == maps || console == maps)
    {
        Con_Printf("menu ids: FAIL Search, Console and Map Library are not three pages (%d, %d, %d)\n", search, console, maps);
        problems++;
    }
    if(verbose || problems > 0)
    {
        Con_Printf("menu ids: %d pages, each its own number; %d share a builder's code with another; %s\n", pageCount,
            folded, problems > 0 ? "FAIL" : "OK");
    }
    return problems;
}

// menu_vr expect <page title | closed | main | levels | single player | options | other>: the menu shown checked
// against it (the corner buttons' and links' tests, Misc/quakevr/menu_shortcuts_test.sh): "menu expect: PASS" or
// "FAIL ... got ..."; menu_vr expect done: "menu expect: N passed, M failed" (and the counts start again).
struct MenuExpect
{
    int passed{0};
    int failed{0};
};
MenuExpect menuExpect;

[[nodiscard]] const char* menuShownName()
{
    if(key_dest != key_menu)
    {
        return "closed";
    }
    if(m_state == m_vr)
    {
#ifndef QVR_RELEASE
        if(const char* submenu = tunablesTitle(ui().page))
        {
            return submenu; // (a Tunables submenu: the one shown)
        }
#endif
        return pages[ui().page].title;
    }
    return m_state == m_maps ? "levels" : m_state == m_singleplayer ? "single player" : m_state == m_options ? "options"
           : m_state == m_main                                                        ? "main"
                                                                                       : "other";
}

void menuExpectCommand(const char* text)
{
    char want[128];
    q_strlcpy(want, text[0] == '"' ? text + 1 : text, sizeof(want));
    for(size_t n = strlen(want); n > 0 && (want[n - 1] == ' ' || want[n - 1] == '"' || want[n - 1] == '\n'); n--)
    {
        want[n - 1] = 0;
    }
    if(!q_strcasecmp(want, "done"))
    {
        qvr::testkit::verdict("menu expect", menuExpect.passed, menuExpect.failed);
        menuExpect = {};
        return;
    }
    const char* shown = menuShownName();
    if(!q_strncasecmp(want, "panels ", 7)) // (menu_vr expect panels <n>: the menu panels open, New Panel's)
    {
        int open = key_dest == key_menu ? 1 : 0; // (the first panel: the menu itself)
        for(int i = 1; i < qvr::menu::instanceCount; i++)
        {
            open += menuInstances[i].open ? 1 : 0;
        }
        shown = va("panels %d", open);
    }
    if(!q_strcasecmp(want, shown))
    {
        menuExpect.passed++;
        Con_Printf("menu expect: PASS %s\n", shown);
        return;
    }
    menuExpect.failed++;
    Con_Printf("menu expect: FAIL want \"%s\", got \"%s\"%s\n", want, shown,
        m_state == m_vr && key_dest == key_menu ? va(" (page %d)", ui().page) : "");
}

// menu_vr links (with the VR menu open; Misc/quakevr/menu_shortcuts_test.sh): every link on every page (a row opening
// a page: the groups' pages, the settings' homes on other pages) pressed as Enter presses it, from its page shown, and
// every Search entry opened as a picked result opens it; each must land on its own page (a tunable: its Tunables
// submenu, on its row). Prints the failures and
// "menu links: N links and M search entries opened, F failed". Leaves the menu on VR Settings.
void checkLinks()
{
    if(key_dest != key_menu || m_state != m_vr)
    {
        Con_Printf("menu links: open the VR menu first (menu_vr 0)\n");
        return;
    }
    int links = 0, entries = 0, failed = 0;
    for(int p = 0; p < pageCount; p++)
    {
        const int n = static_cast<int>(items(p).size());
        for(int i = 0; i < n; i++)
        {
            showPage(p);
            const Item item = items(p)[i];
            if(item.kind != Item::Action || item.page < 0 || item.actionArg || item.command)
            {
                continue;
            }
            links++;
            change(item, 1);
            if(key_dest != key_menu || m_state != m_vr || ui().page != item.page)
            {
                Con_Printf("menu links: FAIL \"%s\" row \"%s\": want page %d \"%s\", got %s\n", pages[p].title,
                    item.label ? item.label : "", item.page, item.page < pageCount ? pages[item.page].title : "?",
                    menuShownName());
                failed++;
                if(key_dest != key_menu || m_state != m_vr)
                {
                    showPage(PageMain);
                }
            }
        }
    }
    buildSearchIndex();
    const int count = static_cast<int>(search.index.size());
    for(int e = 0; e < count; e++)
    {
        showPage(pageIndex(pageSearch));
        const int target = search.index[e].page;
        const za::String label = search.index[e].label;
        const cvar_t* const cvar = search.index[e].isPage ? nullptr : search.index[e].cvar;
        openEntry(e);
        entries++;
        bool landed = key_dest == key_menu && m_state == m_vr && ui().page == target;
#ifndef QVR_RELEASE
        if(!landed && target == pageIndex(pageTunables) && cvar && key_dest == key_menu && m_state == m_vr)
        {
            // A tunable opens its submenu (tunablesSearchTarget), on its row; the page's own rows (Show), the page.
            const int shown = ui().page;
            const auto list = items(shown);
            const int at = ui().cursors[shown];
            landed = tunablesTitle(shown) && at >= 0 && at < static_cast<int>(list.size()) && list[at].cvar == cvar;
        }
#endif
        if(!landed)
        {
            Con_Printf("menu links: FAIL search entry \"%s\": want page %d \"%s\", got %s\n", label.cStr(), target,
                target >= 0 && target < pageCount ? pages[target].title : "?", menuShownName());
            failed++;
        }
    }
    showPage(PageMain);
    Con_Printf("menu links: %d links and %d search entries opened, %d failed\n", links, entries, failed);
}

// menu_vr pos under a dialog (M_Confirm's, the flat screen's Quit): its box as drawn, against the menu's area (the flat
// VR menu panel's rect, or the window) and the menu canvas's scale: centred on it and at its scale (tests).
void printDialogBox()
{
    float x0, y0, x1, y1, unit;
    if(!M_DialogBox(&x0, &y0, &x1, &y1, &unit))
    {
        return;
    }
    Con_Printf("menu_vr pos: dialog box x %.0f..%.0f, y %.0f..%.0f (middle %.1f %.1f), %.2f px per unit\n", x0, x1, y0, y1,
        (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, unit);
    if(VR_IsActive())
    {
        Con_Printf("menu_vr pos: dialog area: the headset's canvas\n");
        return;
    }
    int ax = 0, ay = 0, aw = glwidth, ah = glheight;
    const bool panel = VR_MenuFlatViewport(&ax, &ay, &aw, &ah) != 0;
    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_MENU, &t);
    Con_Printf("menu_vr pos: dialog area: %s %d %d %d x %d (middle %.1f %.1f), the menu's %.2f px per unit\n",
        panel ? "the flat VR menu panel" : "the window", ax, ay, aw, ah, ax + aw * 0.5f, ay + ah * 0.5f, t.scale[0] * aw * 0.5f);
}
} // namespace

void qvr::menu::init()
{
    checkPageIds(false);
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
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "ids"))
    {
        checkPageIds(true);
        return;
    }
    if(Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(1), "expect"))
    {
        // (The rest of the line as typed: titles have spaces and apostrophes, which the arguments split.)
        const char* want = Cmd_Args();
        want += strspn(want, " \t");
        want += strcspn(want, " \t");
        want += strspn(want, " \t");
        menuExpectCommand(want);
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "links"))
    {
        checkLinks();
        return;
    }
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
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "wheeltest"))
    {
        wheelTest();
        return;
    }
    // menu_vr fold <0|1> [section] (tests, screenshots; Menu Look): the VR page shown's section whose header starts
    // with [section] (unquoted words; none: every section) unfolded or folded, then MFOLD|page|header|folded|hidden rows.
    if(Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(1), "fold"))
    {
        if(m_state != m_vr)
        {
            Con_Printf("menu_vr fold: not on a VR page\n");
            return;
        }
        const bool fold = Q_atoi(Cmd_Argv(2)) != 0;
        char name[128] = "";
        for(int a = 3; a < Cmd_Argc(); a++)
        {
            q_strlcat(name, a > 3 ? " " : "", sizeof(name));
            q_strlcat(name, Cmd_Argv(a), sizeof(name));
        }
        const int page = ui().page;
        for(const Item& item : items(page))
        {
            if(item.kind == Item::Header && item.label && !q_strncasecmp(item.label, name, strlen(name)))
            {
                setFolded(page, item.label, fold);
            }
        }
        const auto& list = items(page);
        int hidden = 0;
        for(const Item& item : list)
        {
            hidden += item.kind != Item::Header && item.folded;
        }
        for(const Item& item : list)
        {
            if(item.kind == Item::Header)
            {
                Con_Printf("MFOLD|%s|%s|%d\n", pages[page].title, item.label ? item.label : "", item.folded ? 1 : 0);
            }
        }
        Con_Printf("menu_vr fold: %d rows folded on %s (folding %s)\n", hidden, pages[page].title, foldsOn(page) ? "on" : "off");
        return;
    }
    // menu_vr title <page title> (unquoted words): that page opened, as menu_vr <its number>.
    if(Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(1), "title"))
    {
        const char* title = Cmd_Args() + strlen(Cmd_Argv(1)) + 1;
        for(int p = PageMain; p < pageCount; p++)
        {
            if(!q_strcasecmp(pages[p].title, title))
            {
                VR_Menu_Open();
                openInTree(p);
                return;
            }
        }
        Con_Printf("menu_vr title: no page \"%s\"\n", title);
        return;
    }
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "modes"))
    {
        printModes(Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : -1);
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
            int listCursor = 0, listScroll = 0;
            const bool onList = M_ListPosition(&listCursor, &listScroll, false);
            Con_Printf("menu_vr pos: state %d, %s cursor %d scroll %d\n", m_state, onList ? "list" : "no list", listCursor, listScroll);
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
            printDialogBox();
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
    if(m_state != m_vr || ui().page != pageIndex(pageWofsHolstered) || vr_weapon_holster_preview.value == 0.f ||
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
    if(m_state != m_vr || ui().page != pageIndex(pageWofsFlashlight) || vr_flashlight_mount_preview.value == 0.f ||
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
        "vr_flashlight_armor_forward", "vr_flashlight_up", "vr_flashlight_out", "vr_flashlight_tilt", "vr_gadget_scale", "vr_gadget_x", "vr_gadget_y",
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

#include "vr_menu_instances.inc"
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
    gorepresets::sync(); // (Gore: likewise)
    gameplaypresets::syncClassic(); // (vr_classic_quake: whether the three presets match Classic Quake)
    if(ui().page == pageIndex(pageSearch))
    {
        drawSearch();
        return;
    }
    if(ui().page == pageIndex(pageConsole))
    {
        drawConsolePage();
        return;
    }
    if(ui().page == pageIndex(pageMaps))
    {
        drawMapsPage();
        return;
    }
    const auto& list = items(ui().page);
    int& cursor = ui().cursors[ui().page];
    int& scroll = ui().scrolls[ui().page];

    // A setting selected in a folded section (a menu path, Search, menu_vr <page> <row>): its section unfolds.
    if(cursor < static_cast<int>(list.size()) && list[cursor].folded && list[cursor].kind != Item::Header)
    {
        if(const int h = headerOf(list, cursor); h >= 0)
        {
            setFolded(ui().page, list[h].label, false);
            (void)items(ui().page); // (the rows marked again)
        }
    }
    stepFolds(ui().page, list);

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
#ifndef QVR_RELEASE
    if(const char* submenu = tunablesTitle(ui().page))
    {
        name = submenu; // (a Tunables submenu: the one shown)
    }
#endif
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
        if(list[i].kind != Item::Header && rowHeight(list, i) <= 0)
        {
            continue; // (folded)
        }
        drawItem(list[i], l.listTop + rowTop(list, scroll, i), rowOpen(list, i), rowSelected && i - list[i].partOf == cursor);
    }

    if(int y, height; scrollbar(list, y, height))
    {
        ui().scrollbarX = q_min(midPos + 188, static_cast<int>(glcanvas.right) - 16);
        M_DrawTextBox(ui().scrollbarX - 4, l.listTop + y - 4, 0, height - 1);
    }

    if(cursor < n && rowSelected)
    {
        if(list[cursor].helpArg == checklistHelp)
        {
            checklist::hovered(list[cursor].arg >> 8); // (a checklist item under the cursor: a voice note's context)
        }
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
    if(ui().page == pageIndex(pageSearch))
    {
        searchKey(key);
        return;
    }
    if(ui().page == pageIndex(pageConsole))
    {
        consoleKey(key);
        return;
    }
    if(ui().page == pageIndex(pageMaps))
    {
        mapsKey(key);
        return;
    }
    if(keyCapture)
    {
        // A key row waits for a key (as the Keys menu's rows do): Escape (or the console key) leaves the row as it was.
        cvar_t* const var = keyCapture;
        keyCapture = nullptr;
        M_VRKeyCapture(false);
        qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
        if(key != K_ESCAPE && key != '`' && key > 0 && key < MAX_KEYS)
        {
            setKeyRow(var, Key_KeynumToString(key));
        }
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

    // A section's header while sections fold: Enter, a click or the trigger folds or unfolds it; left folds, right
    // unfolds.
    if(cursor < static_cast<int>(list.size()) && list[cursor].kind == Item::Header && foldsOn(ui().page))
    {
        int barY, barHeight;
        const bool onBar = m_mousex >= ui().scrollbarX - 8 && scrollbar(list, barY, barHeight);
        const bool click = key == K_MOUSE1 && !onBar && rowAt(m_mousey) == cursor;
        if(click || key == K_LEFTARROW || key == K_RIGHTARROW || key == K_ENTER || key == K_KP_ENTER || key == K_ABUTTON)
        {
            const bool fold = key == K_LEFTARROW ? true : key == K_RIGHTARROW ? false : !list[cursor].folded;
            if(fold != list[cursor].folded)
            {
                setFolded(ui().page, list[cursor].label, fold);
                qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
            }
            return;
        }
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
        case K_MWHEELUP:
        case K_MWHEELDOWN:
            if(wheelScroll(list, key == K_MWHEELDOWN ? 1 : -1))
            {
                break;
            }
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move); // (the page fits, or vr_menu_wheel_scroll 0: a row)
            moveCursor(list, key == K_MWHEELDOWN ? 1 : -1);
            break;

        case K_UPARROW:
            if(key == K_UPARROW && menuui::toolbarShown() && cursor == firstSelectable(list))
            {
                if(!repeat)
                {
                    menuui::focusToolbar(-1);
                }
                break;
            }
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
            moveCursor(list, -1);
            break;

        case K_DOWNARROW:
            if(key == K_DOWNARROW && menuui::toolbarShown() && cursor == lastSelectable(list))
            {
                if(!repeat)
                {
                    menuui::focusToolbar(1);
                }
                break;
            }
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
            moveCursor(list, 1);
            break;

        case K_BACKSPACE:
        case K_DEL:
        case K_YBUTTON:
            if(list[cursor].kind == Item::Key && !lockedItem(list[cursor]))
            {
                Cvar_SetQuick(list[cursor].cvar, "");
                qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
            }
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
                    qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
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
                    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
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
    if(ui().page == pageIndex(pageSearch) && key >= 32 && key < 127)
    {
        typeInSearch(static_cast<char>(key));
    }
    if(ui().page == pageIndex(pageConsole) && key >= 32 && key < 127)
    {
        typeInConsole(static_cast<char>(key));
    }
    if(ui().page == pageIndex(pageMaps) && key >= 32 && key < 127)
    {
        mapsTypeIn(static_cast<char>(key));
    }
}

extern "C" int VR_Menu_TextEntry()
{
    return ui().page == pageIndex(pageSearch) || ui().page == pageIndex(pageConsole) || ui().page == pageIndex(pageMaps)
               ? TEXTMODE_NOPOPUP
               : TEXTMODE_OFF; // (the page's own keyboard)
}

extern "C" void VR_Menu_Mousemove(float cx, float cy)
{
    if(ui().page == pageIndex(pageSearch))
    {
        searchMouse(cx, cy);
        return;
    }
    if(ui().page == pageIndex(pageConsole))
    {
        consoleMouse(cx, cy);
        return;
    }
    if(ui().page == pageIndex(pageMaps))
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
            qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
        }
    }

    const int i = rowAt(cy);
    if(i < 0 || !navigable(list[i]) || i == cursor)
    {
        return;
    }
    cursor = i;
    if(ui_mouse_sound.value)
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Move);
    }
}
