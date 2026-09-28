// vr_menu.cpp -- the "VR Settings" pages (Options > VR Settings), drawn like Ironwail's options
// pages: scrolling lists of labelled settings, changed with left/right (the sticks in VR), with
// actions on enter (A). "Advanced VR Options" at the bottom opens a list of further pages: the old
// Quake VR settings pages (vr_menu_pages.inc) and the new body, throwing and force grab tweaks,
// grouped by topic, the long ones split into pages of a screenful or so. In a headset the pages are
// taller (vr_menu_height, vr_menuui.cpp): more rows at once. Escape (B) goes back a page. With the mouse (and the VR laser pointer, vr_menuui.cpp): the row under
// it is selected, a click picks it, and a slider is set where it is clicked and dragged. Sliders of
// placements, angles, scales and distances (extend()) go on past their bar's ends with left and right
// (the bar showing its end, the value the real one); values set further off in the console stay.

#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_motion.hpp"
#include "vr_motion_review.hpp"
#include "vr_motion_take.hpp"
#include "vr_weapons.hpp"
#include "vr_hands.hpp"
#include "vr_posing.hpp"
#include "vr_sightalign.hpp"
#include "vr_bodycal.hpp"
#include "vr_view.hpp"
#include "vr_units.hpp"
#include "vr_flashlight.hpp"
#include "vr_held.hpp"
#include "vr_props.hpp"
#include "vr_weight.hpp"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
extern float m_mousex, m_mousey; // menu.c: the mouse in menu coordinates
extern qboolean keydown[MAX_KEYS]; // keys.c
extern cvar_t ui_mouse_sound; // menu.c
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
    std::vector<Choice> choices;

    // Action: a function, or a page to open.
    void (*action)(){nullptr};
    int page{-1};

    // Info: its text, asked for each time it is drawn.
    const char* (*info)(){nullptr};

    // Shown under the list while selected.
    const char* helpText{nullptr};

    // Slider: shown instead of the value while it is negative (-1: a hue following the player's).
    const char* negativeLabel{nullptr};

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

    [[nodiscard]] Item help(const char* text) const
    {
        Item i = *this;
        i.helpText = text;
        return i;
    }

    // A slider whose value means something past its bar's ends (offsets, positions, angles, scales,
    // distances; not shares, volumes, colours or chances): left and right step on past them, to
    // lo..hi, the bar showing its end (the laser sets it only along the bar).
    [[nodiscard]] Item extend(float lo, float hi) const
    {
        Item i = *this;
        i.extendable = true;
        i.hardMin = std::fmin(lo, min);
        i.hardMax = std::fmax(hi, max);
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

// An effect's hue (degrees), its leftmost step -1: the player's (vr_player_hue, vr_hue.hpp).
[[nodiscard]] Item hueSlider(const char* label, cvar_t& cvar)
{
    Item i = slider(label, cvar, -5.f, 355.f, 5.f, "%.0f");
    i.negativeLabel = "Player's";
    return i;
}

[[nodiscard]] Item cycle(const char* label, cvar_t* cvar, std::vector<Choice> choices)
{
    Item i{Item::Cycle, label, cvar};
    i.choices = std::move(choices);
    return i;
}

[[nodiscard]] Item cycle(const char* label, cvar_t& cvar, std::vector<Choice> choices)
{
    return cycle(label, &cvar, std::move(choices));
}

[[nodiscard]] Item cycle(const char* label, const char* cvar, std::vector<Choice> choices)
{
    return cycle(label, Cvar_FindVar(cvar), std::move(choices));
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

[[nodiscard]] Item info(const char* (*text)())
{
    Item i{Item::Info, ""};
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
    return item.kind != Item::Header && item.kind != Item::Info;
}

[[nodiscard]] Item open(const char* label, int page)
{
    Item i{Item::Action, label};
    i.page = page;
    return i;
}

using PageBuilder = std::vector<Item> (*)();

// The index of the page `build` builds (pages[], below): what a link to it opens.
[[nodiscard]] int pageIndex(PageBuilder build);

#include "vr_menu_props.inc"
#include "vr_menu_pages.inc"

// ----------------------------------------------------------------------------
// Pages of the port's own tweaks
// ----------------------------------------------------------------------------

// The motion recorder (vr_motion.cpp, docs/vr-port/MOTIONS.md): takes of the player's motions, each
// labelled with what it should be, for tuning the melee.
[[nodiscard]] const char* motionNote()
{
    static std::string text;
    text = vr_motion_note.string[0] ? std::string{"note: "} + vr_motion_note.string : "no note (vr_motion_note)";
    return text.c_str();
}

[[nodiscard]] const char* motionLastSaved()
{
    static std::string text;
    text = std::string{"last: "} + motion::lastSaved();
    return text.c_str();
}

// The category's details, for the Detail choice: the menu rebuilds the page when the category changes.
int motionPageCategory = -1;

[[nodiscard]] std::vector<Item> pageMotionRecorder()
{
    std::vector<Choice> categories;
    const auto& list = motion::categories();
    for(const int i : motion::categoryOrder())
    {
        categories.push_back({static_cast<float>(i), list[i].choice.display});
    }
    std::vector<Choice> details;
    const auto& chosen = motion::chosenCategory().details;
    for(size_t i = 0; i < chosen.size(); i++)
    {
        details.push_back({static_cast<float>(i), chosen[i].display});
    }
    motionPageCategory = static_cast<int>(vr_motion_category.value);
    return {
        header("Record Motions for the Melee"),
        toggle("Arm Recorder", vr_motion_armed)
            .help("Armed: click the Record Button to start a take of the Category, click it again to end it and save "
                  "it (quakevr/motions). A beep and a buzz each time; REC in view."),
        cycle("Category", vr_motion_category, std::move(categories))
            .help("What the motion should do. It stays chosen: record many takes of it in a row. No Hit: motions that "
                  "must do nothing (wiggles, weak moves, reloading)."),
        cycle("Detail", vr_motion_detail, std::move(details))
            .help("Optional: which kind (the swing's direction, the weapon). Its takes count apart, and with the "
                  "category's."),
        info(motion::labelStatus),
        cycle("Record Button", vr_motion_button, {{0.f, "Off stick click"}, {1.f, "Main stick click"}})
            .help("Clicked to start and to end a take, while armed. Its own binding (off hand: run; main hand: reload) "
                  "rests until you disarm."),
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
[[nodiscard]] std::vector<Item> pageReviewTake();
[[nodiscard]] int pageIndex(std::vector<Item> (*build)());
int reviewListGeneration = -1; // the review's generation the pages were built for
int reviewTakeGeneration = -1;
int reviewRelabelCategory = -1;
char reviewListHeader[64];

[[nodiscard]] std::vector<Item> pageReviewTakes()
{
    reviewListGeneration = motion::review::generation();
    std::vector<Choice> categories{{-1.f, "All"}};
    const auto& list = motion::categories();
    for(const int i : motion::categoryOrder())
    {
        categories.push_back({static_cast<float>(i), list[i].choice.display});
    }
    std::vector<Item> items = {
        info(motion::review::summary),
        info(motion::review::reviewedLine),
        info(motion::review::evalLine),
        cycle("Show", vr_motion_review_show,
            {{0.f, "To Review"}, {1.f, "Failing"}, {2.f, "Suspect"}, {3.f, "Not Evaluated"}, {4.f, "Reviewed"}, {5.f, "All"},
                {6.f, "Discarded"}})
            .help("To Review: failing or suspect, not yet kept. A take picked opens its page: play it as a ghost, "
                  "keep, discard or relabel it."),
        cycle("Category", vr_motion_review_category, std::move(categories)),
        action("Re-evaluate Shown", motion::review::reevaluateShown)
            .help("Evaluates the takes listed again, in a second copy of the game in the background (the mock "
                  "headset: yours is untouched), with your settings. About 1.7 s a take."),
        action("Stop Re-evaluation", motion::review::stopReevaluation),
        action("Undo Last", motion::review::undo).help("Takes back the last keep, discard, restore or relabel (again: the "
                                                       "one before)."),
        info(motion::review::lastAction),
    };
    q_strlcpy(reviewListHeader, motion::review::listTitle(), sizeof(reviewListHeader));
    items.push_back(header(reviewListHeader));
    const int take = pageIndex(pageReviewTake);
    for(int r = 0; r < motion::review::rowCount(); r++)
    {
        items.push_back(row(motion::review::rowText, motion::review::rowHelp, motion::review::pick, r, take));
    }
    return items;
}

[[nodiscard]] std::vector<Item> pageReviewTake()
{
    reviewTakeGeneration = motion::review::generation();
    reviewRelabelCategory = static_cast<int>(vr_motion_relabel_category.value);
    std::vector<Choice> categories;
    const auto& list = motion::categories();
    for(const int i : motion::categoryOrder())
    {
        categories.push_back({static_cast<float>(i), list[i].choice.display});
    }
    std::vector<Choice> details;
    const auto& chosen =
        list[CLAMP(0, static_cast<int>(vr_motion_relabel_category.value), static_cast<int>(list.size()) - 1)].details;
    for(size_t i = 0; i < chosen.size(); i++)
    {
        details.push_back({static_cast<float>(i), chosen[i].display});
    }
    std::vector<Item> items;
    for(int l = 0; l < motion::review::detailLines && motion::review::detailLine(l)[0]; l++)
    {
        items.push_back(infoLine(motion::review::detailLine, l));
    }
    const std::vector<Item> rest = {
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
        cycle("Relabel Category", vr_motion_relabel_category, std::move(categories)),
        cycle("Relabel Detail", vr_motion_relabel_detail, std::move(details)),
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
    items.insert(items.end(), rest.begin(), rest.end());
    return items;
}

// The old Single Player and Bot Control menus' extras.
void playHub() { Cbuf_AddText("map vrstart\n"); }
void playTutorial() { Cbuf_AddText("map vrtutorial\n"); }
void playFiringRange() { Cbuf_AddText("map vrfiringrange\n"); }
void addBotTeam0() { Cbuf_AddText("impulse 100\n"); }
void addBotTeam1() { Cbuf_AddText("impulse 101\n"); }
void kickBot() { Cbuf_AddText("impulse 102\n"); }

[[nodiscard]] std::vector<Item> pagePlay()
{
    return {
        header("Maps"),
        action("VR Hub", playHub),
        action("Tutorial", playTutorial),
        action("Firing Range", playFiringRange),
        header("Bots (Multiplayer)"),
        action("Add Bot (Team 0)", addBotTeam0),
        action("Add Bot (Team 1)", addBotTeam1),
        action("Kick Bot", kickBot),
    };
}

[[nodiscard]] std::vector<Item> pageGameplay()
{
    return {
        header("Damage"),
        slider("Damage to Enemies", vr_damage_to_enemies, 0.25f, 4.f, 0.05f, "%.2fx").extend().help("Damage you deal to monsters."),
        slider("Damage to You", vr_damage_to_player, 0.f, 4.f, 0.05f, "%.2fx").extend().help("Damage monsters, traps and falls deal to you."),
        slider("Self Damage", vr_damage_self, 0.f, 2.f, 0.05f, "%.2fx").extend().help("Damage your own rockets and grenades deal to you."),
        toggle("Enemies Hurt by Liquids", vr_enemy_liquid_damage).help("Monsters in slime and lava burn as you do: shove them in. Fish, bosses and the lava dwellers are immune; zombies only burn in lava."),
        slider("Melee Damage", vr_melee_dmg_multiplier, 0.25f, 15.f, 0.25f, "%.2fx").extend(),
        header("Positional Damage"),
        toggle("Positional Damage", vr_positional_damage).help("Headshots, arm and leg shots on humanoid monsters."),
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
        header("Knights' Swords"),
        slider("Sword Damage", vr_sword_damage_mult, 0.5f, 3.f, 0.05f, "%.2fx").extend()
            .help("Knights and hell knights always drop their sword, a melee weapon you can pick up. A sword swing's damage "
                  "over the axe's (the hell knight's sword: 25% more)."),
        header("Monsters"),
        toggle("Ogres Aim Grenades Up and Down", vr_ogre_aim_height)
            .help("Ogres (and zombies throwing flesh) lob at your height, on a ledge above them or a floor below, on an arc at "
                  "their throw's own speed. Off: Quake's lob, which always flies as if you stood level with them."),
        header("Feel"),
        toggle("Explosion Rumble", vr_explosion_rumble).help("Explosions near you rumble in your hands."),
        toggle("Low Health Heartbeat", vr_heartbeat).help("A heartbeat in your hands when your health is low."),
        header("Playtesting"),
        toggle("Voice Notes", vr_notes).help("Raise your off hand to your mouth and hold Y to record a note, with a screenshot and where you are; they go to quakevr/notes."),
    };
}

// Split from Gameplay: blocking and shoving with a guard, batting projectiles back, headbutts.
[[nodiscard]] std::vector<Item> pageParryBash()
{
    return {
        header("Parry and Bash"),
        slider("Parry Angle", vr_parry_angle, 15.f, 80.f, 5.f, "%.0f deg").extend()
            .help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block a monster's melee blow: how far it may be tilted off level."),
        slider("Parry Reach", vr_parry_reach, 0.5f, 2.5f, 0.1f, "%.1f m").extend().help("How far in front of you a held weapon still parries."),
        toggle("Unarmed Parry", vr_parry_unarmed).help("Cross your arms in an X in front of you to block a blow with your forearms."),
        slider("Unarmed Parry Reduction", vr_parry_unarmed_reduction, 0.f, 1.f, 0.05f, "%.2f"),
        toggle("Bash", vr_bash).help("The parry stance (a weapon level across in front, one hand or two) pushed straight forward bashes: knocks monsters back and staggers them. Open palms facing a monster pushed at it shove it. Two hands (a weapon held two-handed, a palm pushing on the blade, both palms) push harder and further."),
        slider("Bash Speed", vr_bash_speed, 0.3f, 3.f, 0.1f, "%.1f m/s").extend().help("How fast the stance (a weapon level across, held half a second) must be pushed forward, both its ends going ahead. A swing passing through the stance doesn't bash."),
        slider("Shove Speed", vr_shove_speed, 0.8f, 5.f, 0.1f, "%.1f m/s").extend().help("How fast open palms (facing ahead, not holding anything) must be pushed out, the arm extending, to shove. Hands waved or patted at a monster don't shove."),
        slider("Bash Damage", vr_bash_damage, 0.f, 40.f, 1.f, "%.0f").extend(),
        slider("Bash Push", vr_bash_push, 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far a bash or shove throws what it hits (times Knockback)."),
        slider("Bash and Parry Sounds", vr_bash_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the sounds that tell a shove, a weapon bash, a counter bash (a bash right after a parry) and a parry apart from your blows (0: the old sounds)."),
        header("Parry Stamina"),
        toggle("Parry Stamina", vr_parry_stamina)
            .help("Parrying tires you: each parry with a weapon costs stamina (less with the weapon in two hands), and the parry that leaves you none knocks the weapon out of your hand. It comes back once you stop parrying for a moment. On, it replaces the Parry Drop Chance (Melee Settings). Crossed arms cost nothing."),
        slider("Stamina", vr_parry_stamina_max, 20.f, 300.f, 5.f, "%.0f").extend().help("Your stamina when rested. Both hands share it."),
        slider("One-Handed Parry Cost", vr_parry_stamina_cost, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a parry with the weapon in one hand costs (30 of 100: the fourth in a row knocks it away)."),
        slider("Two-Handed Parry Cost", vr_parry_stamina_cost_2h, 0.f, 100.f, 1.f, "%.0f").extend()
            .help("Stamina a parry with the weapon in both hands costs (12 of 100: the ninth in a row knocks it away)."),
        slider("Rest Before Recovering", vr_parry_stamina_delay, 0.f, 6.f, 0.25f, "%.2f s").extend()
            .help("How long you must go without parrying, shoving or striking (those that cost stamina) before stamina starts coming back."),
        slider("Recovery Rate", vr_parry_stamina_regen, 1.f, 100.f, 1.f, "%.0f /s").extend().help("Stamina a second it then comes back at."),
        slider("Tiring Warning", vr_parry_stamina_warn, 0.f, 1.f, 0.1f, "%.1f")
            .help("A breath and a throb in the hand when one more one-handed parry would knock the weapon away; a gasp and a long buzz when it does: their volume and strength (0 off)."),
        toggle("Stamina on the Gadget", vr_gadget_stamina)
            .help("The wrist gadget's top row shows your stamina: ten cells, blinking when one more one-handed parry would knock the weapon away, EXHAUSTED when none is left, a sweep while it comes back. While a counter's window is open it reads COUNTER over a bar running out."),
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
            .help("How far a shove or bash made with no stamina left for it throws back and staggers (1: no penalty). With part of its cost left, in between."),
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
        header("Batting Projectiles"),
        toggle("Bat Back Projectiles", vr_deflect).help("Swing a weapon (or a fist) through a monster's spike, laser, spit or grenade to bat it away as a bat hits a ball: off the weapon's face and the way it swings, faster the harder you swing. A bash sends it the way you push."),
        slider("Batting Reach", vr_deflect_radius, 4.f, 32.f, 1.f, "%.0f units").extend().help("How near the weapon's blade (or your fist) a projectile must pass to be batted back."),
        slider("Batting Swing Speed", vr_deflect_speed, 0.2f, 1.5f, 0.05f, "%.2fx").extend().help("How fast a batting swing must be, times Swing Speed (a hit needs 1x, and more for a swung weapon)."),
        slider("Batting Timing", vr_deflect_window, 0.f, 0.5f, 0.05f, "%.2f s").extend().help("How early you may swing: the weapon's path keeps batting this long after it passed."),
        slider("Bash Batting Reach", vr_bash_deflect_radius, 4.f, 48.f, 1.f, "%.0f units").extend()
            .help("A bash (or a shove with a weapon in hand) bats back projectiles that pass this near the guard: the weapon and the hands."),
        slider("Bash Batting Timing", vr_bash_deflect_window, 0.f, 1.f, 0.05f, "%.2f s").extend().help("How long a bash goes on batting after the push."),
        slider("Batting Bounce", vr_deflect_bounce, 0.f, 1.f, 0.05f, "%.2f")
            .help("How lively a batted projectile comes off the weapon: 0 dead (it takes only the swing's speed), 1 as a rubber ball (a ball off a bat is about 0.5). A swing across sends it off to the side; the weapon's face driven at the thrower sends it back, the harder the faster."),
        slider("Batting Aim Assist", vr_deflect_aim_assist, 0.f, 1.f, 0.05f, "%.2f")
            .help("A batted or bashed projectile going very near a monster (within a few degrees; its thrower a little more) bends this much of the way towards it. 0: only your swing aims."),
        header("Headbutt"),
        toggle("Headbutt", vr_headbutt).help("Lunge your head at something to headbutt it."),
        slider("Headbutt Speed", vr_headbutt_speed, 0.4f, 3.f, 0.05f, "%.2f m/s").extend().help("How fast the head must lunge (towards where you look)."),
        slider("Headbutt Damage", vr_headbutt_damage, 5.f, 100.f, 1.f, "%.0f").extend(),
    };
}

[[nodiscard]] std::vector<Item> pageBody()
{
    return {
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        toggle("Walking Legs", vr_body_walk).help("The legs (full body) walk as you move with the stick."),
        slider("Step Rate", vr_body_step_rate, 1.f, 5.f, 0.1f, "%.1f /s").extend()
            .help("How fast the legs step at most, in steps a second at full running speed (walking, somewhat fewer)."),
        slider("Turn Before Stepping", vr_body_turn_step, 15.f, 90.f, 5.f, "%.0f deg").extend()
            .help("How far you turn over your planted feet before they step round to follow."),
        slider("Lean", vr_lean_radius, 0.f, 14.f, 1.f, "%.0f units").extend()
            .help("How far your head may lean off where your body stands before the body follows: get your face near walls and over railings. 0: the body always under the head."),
        slider("Lean Detection", vr_lean_detect, 0.f, 2.f, 0.1f, "%.1fx").help("Tells a lean from walking in the room. Leaning (your head lower and tilted the way you lean, your hands left by your hips), your feet and hips stay where you stand and your back tilts; walking, the body follows your head. 0: off, the body always slides back under your head; higher: more readily a lean."),
        slider("Wading Heaviness", vr_body_wade, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Wading, the legs walk heavier: shorter, higher, slower steps (0: as on land)."),
        slider("Swimming Kicks", vr_body_swim_kick, 0.f, 2.f, 0.1f, "%.1fx").extend()
            .help("Swimming, the legs trail behind and kick where the stick moves you: how wide (0: no kicks)."),
        slider("Swimming Kick Rate", vr_body_swim_kick_rate, 0.f, 4.f, 0.1f, "+%.1f /s").extend()
            .help("How many more kicks a second at full stick (treading water, about 0.7)."),
        toggle("Show Armour and Wounds", vr_body_state)
            .help("The armour you wear plates your torso; your arms and hands get bloodier as you are hurt (with Dynamic Wounds, on the Gore page: where you are hit)."),
        toggle("Wounds Drip Blood", vr_body_blood)
            .help("Blood drips from your wounded arms and hands, faster when badly hurt or just hit, and splashes on the floor."),
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
        header("Placement"),
        slider("Torso Offset", vr_body_torso_back, -0.2f, 0.4f, 0.01f, "%.2f m").extend(-1.f, 1.f)
            .help("How far the torso sits behind your neck (negative: in front)."),
        slider("Legs Offset", vr_body_legs_back, -0.2f, 0.4f, 0.01f, "%.2f m").extend(-1.f, 1.f)
            .help("How far the feet stand behind your head (negative: in front)."),
        slider("Eyes Forward", vr_body_eye_forward, 0.f, 0.25f, 0.01f, "%.2f m").extend(-0.1f, 0.5f)
            .help("From the top of the neck to the eyes, forward."),
        slider("Eyes Up", vr_body_eye_up, 0.f, 0.25f, 0.01f, "%.2f m").extend(-0.1f, 0.5f).help("From the top of the neck to the eyes, up."),
        slider("Crouch Tilt", vr_body_crouch_tilt, 0.f, 80.f, 5.f, "%.0f deg").extend()
            .help("How far the back tilts forward in a full crouch (the hips stay under you)."),
    };
}

[[nodiscard]] std::vector<Item> pageBodyCalibration();

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

[[nodiscard]] std::vector<Item> pageBodyArms()
{
    armsPageCalibrated = bodycal::calibrated() ? 1 : 0;
    std::vector<Item> list{open("Body Calibration", pageIndex(pageBodyCalibration))};
    for(int i = 0; bodycal::measuredLine(i); i++)
    {
        list.push_back(infoLine(armsMeasured, i));
    }
    list.insert(list.end(), {
        header("Arms"),
        slider("Upper Arm", vr_body_tweak_upper_arm, -10.f, 10.f, 0.5f, "%+.1f cm").extend(-30.f, 30.f)
            .help("Longer (negative: shorter) than measured, real cm: the shoulder joint to the elbow. 0: as Body "
                  "Calibration measured it (uncalibrated: the default body's arm, times Arm Length)."),
        slider("Forearm", vr_body_tweak_forearm, -10.f, 10.f, 0.5f, "%+.1f cm").extend(-30.f, 30.f)
            .help("Longer (negative: shorter) than measured, real cm: the elbow to the drawn hand's wrist. 0: as Body "
                  "Calibration measured it (uncalibrated: the default body's forearm, times Arm Length)."),
    });
    if(armsPageCalibrated)
    {
        list.push_back(info(armLengthUnused));
    }
    else
    {
        list.push_back(slider("Arm Length", vr_body_arm_length, 0.7f, 1.4f, 0.01f, "%.2fx").extend(0.5f, 2.f)
                .help("Uncalibrated: the default body's arms (for your height), times this. Once Body Calibration "
                      "has measured yours, it isn't used."));
    }
    list.insert(list.end(), {
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
        slider("Elbow Out", vr_body_elbow_out, 0.f, 1.f, 0.05f, "%.2f")
            .help("Where the elbow points: down, plus this much outwards."),
        slider("Elbow Back", vr_body_elbow_back, 0.f, 1.f, 0.05f, "%.2f")
            .help("Where the elbow points: down, plus this much backwards."),
        slider("Elbow From Hand", vr_body_elbow_hand, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the elbow points away from the back of the hand."),
        header("Pauldrons"),
        toggle("Pauldrons", vr_body_pauldrons).help("Leather pads over the shoulders and the tops of the arms, as the Quake ranger wears."),
        cycle("Pauldron Style", vr_body_pauldron_style, {{0.f, "Ranger leather"}, {1.f, "Armour colour"}, {2.f, "Steel"}})
            .help("Armour colour: green, yellow or red as the armour you wear (leather without)."),
        slider("Pauldron Size", vr_body_pauldron_size, 0.5f, 1.5f, 0.05f, "%.2fx").extend(0.25f, 4.f),
        slider("Pauldron Follows Arm", vr_body_pauldron_follow, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the shoulder cap turns with the upper arm (the lower plates follow the arm fully)."),
        slider("Pauldron Forward", vr_body_pauldron_forward, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
        slider("Pauldron Up", vr_body_pauldron_up, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
        slider("Pauldron Out", vr_body_pauldron_out, -0.08f, 0.08f, 0.005f, "%.3f m").extend(-0.3f, 0.3f),
    });
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
    static const char* const lines[] = {"Measures your shoulders and arms from", "a few poses and moves, so that the drawn",
        "arm bends and straightens with yours."};
    return lines[i];
}

[[nodiscard]] std::vector<Item> pageBodyCalibration()
{
    bodycalVersion = bodycal::version();
    bodycalSeated = vr_bodycal_seated.value != 0.f ? 1 : 0;
    std::vector<Item> list{infoLine(bodycalIntro, 0), infoLine(bodycalIntro, 1), infoLine(bodycalIntro, 2),
        cycle("Position", vr_bodycal_seated, {{0.f, "Standing"}, {1.f, "Seated"}})
            .help("Seated, the height stays as it is (set it standing: Set Height Now); the arms and shoulders are measured "
                  "the same.")};
    if(bodycal::phase() == bodycal::Phase::Result)
    {
        if(bodycal::trusted())
        {
            list.push_back(action("Apply", bodycalApply)
                .help("Sets the measurements: your arms' lengths, where your shoulders are and how they rise and swing, and "
                      "(standing) your height. Undo puts the settings back."));
        }
        list.insert(list.end(), {
            action("Cancel", bodycalCancel).help("Nothing changes."),
            action(bodycal::showingNew() ? "Showing: New Measurements" : "Showing: Current Settings", bodycalSwitch)
                .help("Your body (and the one in front of you) with the new measurements, or with the settings as they are: "
                      "bend and straighten your arms to compare."),
            cycle("Body in Front", vr_bodycal_preview, {{0.f, "Off"}, {1.f, "Facing You"}, {2.f, "From the Side"}})
                .help("Your body in front of you while this page shows a result, moving as you do."),
        });
    }
    else
    {
        list.push_back(action(bodycal::partial() ? "Continue Calibration" : "Start Calibration", bodycalStart)
                           .help("Stand (or sit) with room to stretch your arms. Follow the text in front of you and the "
                                 "figure ahead: after three beeps and a high one, take the pose and hold still until the "
                                 "click; the moves record for a few seconds. About two minutes. The menu button stops "
                                 "(Continue takes the rest)."));
        if(bodycal::partial())
        {
            list.push_back(action("Start Over", bodycalRestart).help("All the poses again."));
        }
        if(bodycal::canUndo())
        {
            list.push_back(action("Undo", bodycalUndo).help("Puts back the settings from before the last Apply, exactly."));
        }
    }
    for(int i = 0; bodycal::statusLine(i); i++)
    {
        list.push_back(infoLine(bodycalLine, i));
    }
    if(bodycal::phase() == bodycal::Phase::Result || bodycal::partial())
    {
        list.push_back(header("Poses"));
        for(int i = 0; i < bodycal::stepCount(); i++)
        {
            if(bodycal::stepUsed(i))
            {
                list.push_back(row(bodycal::stepRow, bodycal::stepHelp, bodycalRedo, i, -1));
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

void flashlightFingers(std::vector<Item>& list, const FlashlightFingerCvars& c, int& manualAsBuilt)
{
    const char* curlHelp = "How far this finger is curled round the torch: 0 open, 1 a fist (the controller's grip still opens it).";
    const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it holds the torch (a share of a full curl).";
    list.push_back(cycle("Fingers", c.fingers, {{0.f, "Automatic"}, {1.f, "Manual"}})
                       .help("Automatic: the fingers wrap the torch on their own. Manual: they take the curls set below "
                             "(no fitting: the hand is where the grip's sliders put it)."));
    manualAsBuilt = c.fingers.value >= 0.5f ? 1 : 0;
    if(manualAsBuilt)
    {
        list.insert(list.end(), {
            slider("Thumb Curl", c.curlThumb, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Thumb Across", c.thumbAcross, 0.f, 1.f, 0.02f, "%.2f")
                .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
            slider("Index Curl", c.curlIndex, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Middle Curl", c.curlMiddle, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Ring Curl", c.curlRing, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            slider("Little Curl", c.curlPinky, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
        });
    }
    else
    {
        list.push_back(slider("Overlap", c.overlap, 0.f, 1.f, 0.05f, "%.2f")
                           .help("How far the fingers and palm may sink into the torch: 0 they stop on its surface, 1 a centimetre in."));
    }
    list.insert(list.end(), {
        slider("Thumb", c.biasThumb, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Index Finger", c.biasIndex, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Middle Finger", c.biasMiddle, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Ring Finger", c.biasRing, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Little Finger", c.biasPinky, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
        slider("Thumb X (forward)", c.thumbX, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f).help("Moves the thumb on the hand."),
        slider("Thumb Y (palm)", c.thumbY, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
        slider("Thumb Z (up)", c.thumbZ, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
    });
}

// Split from Body: the torch on the chest.
[[nodiscard]] std::vector<Item> pageFlashlight()
{
    std::vector<Item> list = {
        toggle("Chest Flashlight", vr_flashlight)
            .help("A torch hanging on your belt, on your off hand's side (lighting only your feet there). Trigger at it: on or off. Grip it with an open, still hand to take it (a fist closing by it in a fight does nothing); let go and it springs back. In your hand: B or Y by a gun clips it on the gun, at your head on your head (a head torch), elsewhere turns it round (low grip or overhead)."),
        slider("Brightness", vr_flashlight_brightness, 0.25f, 2.5f, 0.05f, "%.2fx").extend(),
        slider("Range", vr_flashlight_range, 300.f, 2000.f, 50.f, "%.0f").extend(100.f, 6000.f),
        slider("Visible Beam", vr_flashlight_beam, 0.f, 1.f, 0.05f, "%.2f").help("A soft cone of light in the air from the lamp (0: none)."),
        cycle("Beam Quality", vr_flashlight_beam_quality, {{0.f, "Low"}, {1.f, "Medium"}, {2.f, "High"}})
            .help("How closely the visible beam fades where walls cut it. Higher looks for them more often, costing more time each frame."),
        toggle("Casts Shadows", vr_flashlight_shadows).help("Its light casts shadows (takes one of the shadowed dynamic lights)."),
        cycle("Cord", vr_flashlight_cord, {{0.f, "Off"}, {1.f, "Coiled"}, {2.f, "Plain"}}).help("The retracting cord from the clip on your belt to the torch while it is off the belt: coiled like an old telephone's, springy, or a plain cable (off: none drawn)."),
        hueSlider("Beam Hue", vr_flashlight_hue).help("The beam's colour, with Beam Saturation (at 0 it is white): its light, the beam in the air and the lens. 40 warm, 200 cold blue; Player's: the Player Effects Hue."),
        slider("Beam Saturation", vr_flashlight_saturation, 0.f, 1.f, 0.05f, "%.2f").help("0 white (the default), 1 the Beam Hue in full."),
        header("On the Belt"),
        slider("Lean Out", vr_flashlight_tilt, -90.f, 90.f, 1.f, "%.0f deg").extend().help("How far the stored torch, hanging on your belt lens down, leans its lens out from your body."),
        slider("Forward", vr_flashlight_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("Up", vr_flashlight_up, -0.4f, 0.4f, 0.01f, "%.2f m").extend(),
        slider("Out", vr_flashlight_out, -0.3f, 0.3f, 0.01f, "%.2f m").extend().help("Towards your off hand's side."),
        header("In the Hand"),
        slider("In Hand Forward", vr_flashlight_hand_forward, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Where the held lamp sits in your fist."),
        slider("In Hand Up", vr_flashlight_hand_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        header("In the Hand: Low Grip"),
        slider("Low Grip Forward", vr_flashlight_low_x, -30.f, 30.f, 0.5f, "%.1f cm").extend()
            .help("The torch's place in your fist when the beam comes out of the thumb's side (the grip you take it in), on top of In Hand Forward/Up. The other hand's is the mirror image."),
        slider("Low Grip Towards Palm", vr_flashlight_low_y, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Low Grip Up", vr_flashlight_low_z, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Low Grip Pitch", vr_flashlight_low_pitch, -180.f, 180.f, 5.f, "%.0f deg").help("Tilts the beam up (positive) or down in the hand."),
        slider("Low Grip Yaw", vr_flashlight_low_yaw, -180.f, 180.f, 5.f, "%.0f deg").help("Turns the beam towards your palm (positive) or away."),
        slider("Low Grip Roll", vr_flashlight_low_roll, -180.f, 180.f, 5.f, "%.0f deg"),
    };
    list.push_back(header("Low Grip: Fingers on the Torch"));
    flashlightFingers(list,
        {vr_flashlight_low_fingers, vr_flashlight_low_overlap, vr_flashlight_low_curl_thumb, vr_flashlight_low_thumb_across,
            vr_flashlight_low_curl_index, vr_flashlight_low_curl_middle, vr_flashlight_low_curl_ring, vr_flashlight_low_curl_pinky,
            vr_flashlight_low_bias_thumb, vr_flashlight_low_bias_index, vr_flashlight_low_bias_middle, vr_flashlight_low_bias_ring,
            vr_flashlight_low_bias_pinky, vr_flashlight_low_thumb_x, vr_flashlight_low_thumb_y, vr_flashlight_low_thumb_z},
        flashlightPageManual[0]);
    list.insert(list.end(), {
        header("In the Hand: Overhead Grip"),
        slider("Overhead Grip Forward", vr_flashlight_high_x, -30.f, 30.f, 0.5f, "%.1f cm").extend()
            .help("The same when B or Y has turned it over (the beam out of the little finger's side)."),
        slider("Overhead Grip Towards Palm", vr_flashlight_high_y, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Overhead Grip Up", vr_flashlight_high_z, -30.f, 30.f, 0.5f, "%.1f cm").extend(),
        slider("Overhead Grip Pitch", vr_flashlight_high_pitch, -180.f, 180.f, 5.f, "%.0f deg"),
        slider("Overhead Grip Yaw", vr_flashlight_high_yaw, -180.f, 180.f, 5.f, "%.0f deg"),
        slider("Overhead Grip Roll", vr_flashlight_high_roll, -180.f, 180.f, 5.f, "%.0f deg"),
    });
    list.push_back(header("Overhead Grip: Fingers on the Torch"));
    flashlightFingers(list,
        {vr_flashlight_high_fingers, vr_flashlight_high_overlap, vr_flashlight_high_curl_thumb, vr_flashlight_high_thumb_across,
            vr_flashlight_high_curl_index, vr_flashlight_high_curl_middle, vr_flashlight_high_curl_ring, vr_flashlight_high_curl_pinky,
            vr_flashlight_high_bias_thumb, vr_flashlight_high_bias_index, vr_flashlight_high_bias_middle, vr_flashlight_high_bias_ring,
            vr_flashlight_high_bias_pinky, vr_flashlight_high_thumb_x, vr_flashlight_high_thumb_y, vr_flashlight_high_thumb_z},
        flashlightPageManual[1]);
    list.insert(list.end(), {
        header("Reach Zones"),
        toggle("Show Flashlight Zones", vr_show_flashlight_zones)
            .help("Draws where holding the torch clips it on (yellow balls at your temples and forehead, an orange capsule round "
                  "each gun) and the held torch's middle (white), which they measure. Green: in reach, B or Y clips it on (on "
                  "your head: a hand there takes it off)."),
        header("On a Gun"),
        slider("On Gun Forward", vr_flashlight_gun_forward, -0.4f, 0.3f, 0.005f, "%.3f m").extend()
            .help("Where the torch sits once clipped on a gun: along the barrel, under it (or beside a bulky gun). B or Y at it "
                  "takes it off."),
        slider("On Gun Up", vr_flashlight_gun_up, -0.3f, 0.3f, 0.005f, "%.3f m").extend(),
        slider("On Gun Out", vr_flashlight_gun_out, -0.3f, 0.3f, 0.005f, "%.3f m").extend().help("Away from your body."),
        slider("Gun Zone Along", vr_flashlight_gun_zone_forward, -0.2f, 0.2f, 0.005f, "%.3f m").extend()
            .help("Where the torch held by the gun in your other hand lets B or Y clip it on: round the gun's line from your hand "
                  "to its muzzle, moved along the gun (forward), up and out. Apart from where it then sits (On Gun)."),
        slider("Gun Zone Up", vr_flashlight_gun_zone_up, -0.2f, 0.2f, 0.005f, "%.3f m").extend(),
        slider("Gun Zone Out", vr_flashlight_gun_zone_out, -0.2f, 0.2f, 0.005f, "%.3f m").extend().help("Away from your body."),
        slider("Gun Zone Radius", vr_flashlight_gun_zone_radius, 0.02f, 0.3f, 0.005f, "%.3f m").extend()
            .help("How far from that line the torch's middle may be."),
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
    });
    return list;
}

// Gore (vr_gore.cpp, vr_decals.cpp, vr_bodyblood.cpp; the QC's gibs sticking: vr_carry.qc).
[[nodiscard]] std::vector<Item> pageGore()
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
        header("Wounds on Models"),
        toggle("Dynamic Wounds", vr_wounds)
            .help("Blood painted on monsters, corpses and you where the hits land, in the skins' own pixels. Your body and hands show your wounds this way instead of the wound skins, and healing washes them off."),
        toggle("Burns", vr_wounds_burns).help("Explosions, fire, lightning, lava and slime char what they hit; fresh burns glow in their cracks for a moment."),
        toggle("Wet from Liquids", vr_wounds_wet).help("Monsters and you get wet up to where water or slime came, drip, and dry in about 25 seconds."),
        cycle("Models Kept", vr_wounds_pool, {{32.f, "32 (8 MB)"}, {64.f, "64 (16 MB)"}, {128.f, "128 (32 MB)"}})
            .help("How many models keep their wounds at once: past it, the ones seen longest ago give theirs up."),
        header("Your Wounds"),
        slider("Arm Drip Rate", vr_body_blood, 0.f, 4.f, 0.25f, "%.2fx").extend()
            .help("How often blood drips from your wounded arms and hands (the body's wounds: Show Armour and Wounds; 0 none)."),
        slider("Drop Size", vr_body_blood_amount, 0.5f, 3.f, 0.25f, "%.2fx").extend().help("How big the drops are and how much they splash."),
        slider("Drips Round Feet", vr_body_blood_floor, 0.f, 4.f, 0.25f, "%.2fx").extend()
            .help("While wounded, blood drips from your body round your feet, faster when badly hurt or just hit (0 none)."),
        slider("Drops Mark Floor", vr_body_blood_marks, 0.f, 1.f, 0.05f, "%.2f").help("The chance a drop leaves a mark on the floor."),
        slider("Floor Mark Size", vr_body_blood_mark_size, 0.5f, 4.f, 0.25f, "%.2fx").extend(),
        header("Marks"),
        toggle("Decals", vr_decals).help("Blood, scorch marks and bullet chips on walls and floors (the gore needs them)."),
        slider("Max Decals", vr_decal_max, 64.f, 4096.f, 64.f, "%.0f").extend().help("The oldest go first. The gore makes many: 1024 or more."),
        slider("Decal Lifetime", vr_decal_life, 10.f, 600.f, 10.f, "%.0f s").extend(),
        toggle("Gib Blood", vr_gib_blood).help("Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail."),
        slider("Gib Blood Trail", vr_gib_blood_trail, 0.f, 3.f, 0.25f, "%.2fx").extend().help("How dense their trail of blood and drops is (0 none)."),
    };
}

[[nodiscard]] std::vector<Item> pageGadget()
{
    return {
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}),
        cycle("Arm", vr_gadget_hand, {{0.f, "Off hand"}, {1.f, "Main hand"}}),
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
[[nodiscard]] std::vector<Item> pageScreens()
{
    return {
        header("Wrist Gadget"),
        toggle("Level and Stats", vr_gadget_show_level),
        toggle("Stamina and Counters", vr_gadget_stamina)
            .help("The top row shows your parry stamina (with Parry Stamina on) and COUNTER while a counter-attack's window is open."),
        slider("Screen Light", vr_gadget_light, 0.f, 3.f, 0.1f, "%.1fx").extend()
            .help("The screen casts a light in its colour the way it faces, and a faint one on your hand (0 off)."),
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
        toggle("Screens on Weapons at Rest", "vr_weapon_screen_idle").help("Weapons in your holsters and lying in the world show their ammo screen and button too, not only the ones in your hands."),
        header("Map Boards"),
        toggle("Map Boards as CRTs", "vr_worldtext_crt").help("The text boards in maps (the tutorial's, the start map's) are CRT screens with glowing text, as the wrist gadget's. Off: plain text."),
        slider("Map Board Hue", "vr_worldtext_hue", 0.f, 355.f, 5.f, "%.0f").help("Their colour: 40 amber, 128 green, 200 blue, 0 red."),
    };
}

// Split from Wrist Gadget: the colours of the player's effects.
[[nodiscard]] std::vector<Item> pageColours()
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
        slider("Screen Background", vr_gadget_screen_background, 0.f, 4.f, 0.1f, "%.1f").extend(),
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

[[nodiscard]] std::vector<Item> pageThrowing()
{
    return {
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        slider("Two-Hand Throw Speed", vr_2h_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}}),
        slider("Velocity Window", vr_throw_window, 0.04f, 0.3f, 0.01f, "%.2f s").extend()
            .help("Around the release, where the hand's fastest moment sets the throw."),
        slider("Direction Lookback", vr_throw_dir_lookback, 0.f, 0.1f, 0.005f, "%.3f s").extend()
            .help("How far back from that moment the throw's direction is averaged."),
        slider("Lever Arm", vr_throw_lever_arm, 0.f, 0.3f, 0.01f, "%.2f m").extend()
            .help("From the palm to the held object's centre: wrist flicks add speed through it."),
        toggle("Analog Release", vr_throw_release)
            .help("A throw lets go as the grip starts to open, not only once it is released."),
        slider("Max Speed Gain", vr_throw_gain_max, 1.f, 3.f, 0.05f, "%.2fx").extend()
            .help("Extra speed for fast throws, which feel weak at true speed."),
        toggle("Aim Assist", vr_throw_assist)
            .help("Throws close to an enemy's direction bend towards it."),
        slider("Assist Cone", vr_throw_assist_cone, 2.f, 30.f, 1.f, "%.0f deg").extend(),
        slider("Assist Strength", vr_throw_assist_strength, 0.f, 1.f, 0.05f, "%.2f"),
        header("Physics"),
        slider("Bounciness", vr_throw_restitution, 0.f, 0.8f, 0.05f, "%.2f").extend(),
        slider("Friction", vr_throw_friction, 0.f, 1.5f, 0.05f, "%.2f").extend(),
        slider("Max Spin", vr_throw_spin_max, 0.f, 40.f, 1.f, "%.0f rad/s").extend(),
        slider("Spin Drag", vr_throw_spin_drag, 0.f, 2.f, 0.05f, "%.2f").extend(),
        slider("Hitbox", vr_throw_hitbox, 1.f, 12.f, 0.5f, "%.1f").extend().help("Half-size of a thrown weapon's box against monsters."),
        slider("Hit Min Speed", vr_throw_hit_min_speed, 0.f, 600.f, 25.f, "%.0f").extend()
            .help("Units/s a thrown weapon, box or gib must go at to hurt a monster; slower (at rest against it, pushed into it) it does nothing."),
    };
}

// Split from Throwing and Physics: carrying ammo and health boxes, and gibs, heads and corpses.
[[nodiscard]] std::vector<Item> pageCarrying()
{
    return {
        header("Carrying Boxes"),
        toggle("Carry Ammo and Health", vr_carry)
            .help("Grip a box or a backpack to carry it, push it with a hand or gun. Off: touching takes it."),
        cycle("Take a Box", vr_carry_take, {{0.f, "At a holster"}, {1.f, "Trigger"}, {2.f, "Either"}})
            .help("At a holster: let go of it at a hip or shoulder holster to put it in your pack."),
        slider("Grab Distance Bias", vr_carry_grab_bias, -3.f, 5.f, 0.5f, "%+.1f cm")
            .extend(-10.f, 20.f)
            .help("A hand takes a box, gib, backpack or armour when its fist (the palm and the curled fingers) touches it. "
                  "Positive: from this far off it too. Negative: only pressed this far into it."),
        toggle("Drawn In the Hand", vr_carry_local)
            .help("What you carry is drawn in your hand as it is this frame: no lag or lead as you walk or turn. Off: where the server has it."),
        toggle("Two-Handed Carrying", vr_carry_two_hands)
            .help("Grip what your other hand carries to hold it in both: it moves and turns with both hands, and letting go "
                  "of both together throws it. Let go with one and the other keeps it (to pass it from hand to hand)."),
        slider("Two-Handed Hand Drift", vr_carry_two_hands_drift, 0.f, 20.f, 1.f, "%.0f cm")
            .extend(0.f, 50.f)
            .help("How far your drawn hands may be off your real ones to stay on their grips as you pull them apart or push "
                  "them together. 0: they stay on your real hands. Pulled 3 cm further off (vr_carry_two_hands_detach), "
                  "with the hand no longer touching it, that hand lets go; both, and it drops."),
        toggle("Fit to the Hand", vr_held_surface_fit)
            .help("A box, backpack or gib you grip sits against your curled fingers, by its drawn shape. Off: it stays where you gripped it."),
        slider("Fit Gap", vr_held_fit_gap, -6.f, 3.f, 0.1f, "%.1f cm").extend()
            .help("Space left between your fingers and what they hold (negative: sunk in). Per model: vr_held_fit_gaps in the console."),
        slider("Push Strength", vr_carry_nudge, 0.f, 2.f, 0.1f, "%.1fx").extend(),
        slider("Box Throw Speed", vr_carry_throw_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        slider("Box Punch Damage", vr_carry_melee_mult, 1.f, 3.f, 0.1f, "%.1fx").extend().help("Punching with a box in hand."),
        slider("Thrown Box Damage", vr_carry_throw_damage, 0.f, 50.f, 1.f, "%.0f").extend().help("Damage of a box thrown at about 6 m/s; more the faster."),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets))
            .help("The grip, fingers and melee points of what a hand carries."),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights))
            .help("The mass, spring and damage of what a hand carries (Aiming: Weight for how weight feels)."),
        header("Explosive Boxes"),
        toggle("Physics Explosive Boxes", vr_explobox_physics)
            .help("The explosive boxes can be pushed, tipped over, stacked and carried by hand (heavy; never force "
                  "grabbed). Off: fixed in place, as in id's Quake. Next map."),
        slider("Blow Up On Impact", vr_explobox_impact, 0.f, 30.f, 1.f, "%.0f m/s").extend(0.f, 100.f)
            .help("A box hitting something this hard blows up: dropped from about 85 units or more (tipped over, its "
                  "top lands at 11 m/s). 0: never."),
        header("Wall Torches"),
        toggle("Take Torches Off Walls", vr_walltorch)
            .help("Grip a wall torch and pull it out of its holder (or force grab it): it is a burning club, lighting the "
                  "room round you as the wall torch did. Held, it burns for ever; dropped, thrown or used up by blows, "
                  "its fire dies. Grip and fingers: Held Object Offsets, holding it. Off: fixed, as in id's Quake. Next map."),
        slider("Pull to Take", vr_walltorch_pull, 2.f, 30.f, 1.f, "%.0f cm").extend(0.f, 100.f)
            .help("How far a hand gripping a torch on its wall pulls it before it comes out."),
        toggle("Force Grab Torches", vr_walltorch_forcegrab)
            .help("Torches come off their walls, and from where they lie, to a force grab."),
        slider("Blows Before It Dies", vr_walltorch_hits, 0.f, 20.f, 1.f, "%.0f").extend(0.f, 100.f)
            .help("A torch's fire starts dying after this many blows, even in your hand. 0: blows never use it up."),
        slider("Dying Time", vr_walltorch_die_time, 1.f, 30.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("How long a dying torch's fire takes to go out: dropped, thrown or out of blows. Picked up again before "
                  "it is out, a dropped torch burns up again; one out of blows goes on dying."),
        slider("Blow Damage", vr_walltorch_damage, 0.f, 40.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("A torch's blow, times the blow's strength (a gun's is 12, the axe's 20)."),
        slider("Burn Damage", vr_walltorch_burn, 0.f, 20.f, 0.5f, "%.1f / s").extend(0.f, 100.f)
            .help("A lit torch's blow sets what it hits burning: this much damage a second (monsters catch fire)."),
        slider("Burn Time", vr_walltorch_burn_time, 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f),
        toggle("Light Again", vr_walltorch_relight)
            .help("A dying or burnt-out torch held in another torch's flame (on a wall or in your other hand) or dipped "
                  "in lava lights again, as new."),
        slider("Flame Size", vr_walltorch_flame, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 4.f)
            .help("A taken torch's flame (1: the wall torch's)."),
        toggle("Taken Torch Casts Shadows", vr_walltorch_shadows)
            .help("A taken torch's light casts shadows (your hands and body, what is round you), whatever Graphics' Torch "
                  "Light Shadows says. Its brightness is the wall torch's: Graphics' Torch Light Brightness."),
        header("Armour and Pickups"),
        cycle("Armour", vr_armor_wear, {{0.f, "Touch takes it"}, {1.f, "Wear by hand"}})
            .help("Wear by hand: grip the armour to carry it and let go of it over your chest to put it on (only if it is better "
                  "than yours). Walking over it no longer takes it. Next map."),
        slider("Armour Size", vr_armor_scale, 0.3f, 1.f, 0.05f, "%.2fx").extend().help("The carried armour's size (1: Quake's, a metre tall). Next map."),
        cycle("Weapons and Keys", vr_item_objects, {{0.f, "Touch takes them"}, {1.f, "Objects"}})
            .help("Objects: the map's weapons, keys, runes and suits hang spinning until you grab, knock or force-grab them, "
                  "then they are physics objects. A weapon you grip is yours at once; keys, runes and suits you carry to a "
                  "holster and let go of there. Powerups are as before. Next map."),
        header("Grenades"),
        cycle("Catch Grenades", vr_grenade_catch, {{0.f, "Off"}, {1.f, "Ogres'"}, {2.f, "Ogres' and yours"}})
            .help("Grenades bounce and roll as physics objects, and you can catch them (by hand or force grab) and throw them "
                  "back; thrown, they burst on a monster as an ogre's does on you. \"And yours\": your grenade launcher's too."),
        slider("Held Grenade Fuse", vr_grenade_held_fuse, 1.f, 5.f, 0.1f, "%.1f s")
            .help("A grenade you catch fizzes and ticks, and goes off this long after (it never shortens the fuse). Hold it too "
                  "long and it goes off in your hand."),
        toggle("Fuse Resets Every Catch", vr_grenade_fuse_regrab)
            .help("Off: the fuse is set once, as you catch the monster's throw; dropped and caught again, it keeps running. On: "
                  "every catch sets it again."),
        header("Gibs and Corpses"),
        cycle("Gibs and Heads", vr_grab_gibs, {{0.f, "Left alone"}, {1.f, "Grab by hand"}, {2.f, "Hand and force grab"}})
            .help("Pick up and throw gibs and heads, by reaching for them (or force-grabbing them too)."),
        slider("Thrown Gib Damage", vr_gib_throw_damage, 0.f, 50.f, 1.f, "%.0f").extend().help("Damage of a gib or head thrown at about 6 m/s; more the faster."),
        toggle("Destroy Gibs", vr_gib_destroy)
            .help("Gibs and heads burst in a mist of blood when shot, blown up, struck, or thrown hard at a wall or a monster."),
        slider("Gib Health", vr_gib_health, 1.f, 60.f, 1.f, "%.0f").extend().help("The damage that destroys a gib; a head takes half as much again."),
        slider("Gib Splat Speed", vr_gib_splat_speed, 100.f, 600.f, 25.f, "%.0f").extend()
            .help("Units/s a thrown gib or head must hit a wall or a monster at to burst."),
        toggle("Gib Corpses", vr_corpse_gib)
            .help("Corpses burst into gibs when shot, blown up or struck enough: shotguns, nails, lightning, rockets, fists, melee weapons."),
        slider("Corpse Health", vr_corpse_health, 10.f, 300.f, 10.f, "%.0f").extend()
            .help("The damage that gibs a corpse; a big monster's takes more (an ogre's 1.75 times, a fiend's 2.25, a shambler's 3.5)."),
        header("Debug"),
        toggle("Show Physics Shapes", vr_debug_physics_shapes)
            .help("Draws the physics bodies (Box3D) as wireframes: props awake green, asleep blue, held yellow; doors purple, "
                  "monsters orange, you cyan, hanging pickups grey; red dots where they touch. And each hand's grab reach."),
        toggle("Show Hand Bones", vr_debug_hand_bones)
            .help("Draws both hands' joints and bones, and what they grip: the finger spheres (green touching, yellow near, red "
                  "sunk in) and the palm's fit (white: where the hand is, cyan: where the grip moved the palm)."),
    };
}

[[nodiscard]] std::vector<Item> pageForceGrab()
{
    return {
        toggle("Force Grab", vr_forcegrab_mode)
            .help("Point an empty hand at an object, pull the trigger, flick the hand: it flies to you. Grip as it arrives "
                  "to catch it."),
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
        toggle("Pointing Particles", vr_forcegrab_eligible_particles).help("Sparkles on the object an empty hand points at, that it can pull."),
        toggle("Pointing Haptics", vr_forcegrab_eligible_haptics).help("A tick in the hand when it points at a new object it can pull."),
        slider("Outline", "vr_forcegrab_outline", 0.f, 2.f, 0.1f, "%.1f").extend().help("The soft glow round the object a hand points at (0 off)."),
        toggle("Effects", "vr_forcegrab_fx").help("A faint beam to what you point at, a crackling tendril when locked on, a trail behind what flies to you."),
        slider("Ammo/Health Box Size", vr_forcegrabbable_box_scale, 0.1f, 1.f, 0.05f, "%.2f").extend()
            .help("Takes effect on the next map."),
    };
}

// ----------------------------------------------------------------------------
// Pages
// ----------------------------------------------------------------------------

enum PageId
{
    PageMain,
    PageAdvanced,
    PageFirstAdvanced
};

struct Page
{
    const char* group; // a header before it in the Advanced VR Options list (null: none)
    const char* label; // its row there (short: right-aligned left of the middle; null: not listed, opened from another)
    const char* title; // over the page
    PageBuilder build;
};

[[nodiscard]] std::vector<Item> pageMain();
[[nodiscard]] std::vector<Item> pageAdvanced();
[[nodiscard]] std::vector<Item> pageWeaponOffsets();

// In the Advanced VR Options list's order: the indices are positional (menu_vr <n>); links find
// their page by its builder (pageIndex).
const Page pages[] = {
    {nullptr, "VR Settings", "VR Settings", pageMain},
    {nullptr, "Advanced VR Options", "Advanced VR Options", pageAdvanced},

    {"Game", "Play", "Play", pagePlay},
    {nullptr, "Gameplay", "Gameplay", pageGameplay},
    {nullptr, "Parry, Bash, Headbutt", "Parry, Bash and Headbutt", pageParryBash},
    {nullptr, "Melee", "Melee", pageMeleeSettings},
    {nullptr, "Motion Recorder", "Motion Recorder", pageMotionRecorder},
    {nullptr, "Review Takes", "Review Takes", pageReviewTakes},
    {nullptr, nullptr, "Take", pageReviewTake},
    {nullptr, "Gore", "Gore", pageGore},
    {nullptr, "Throwing and Physics", "Throwing and Physics", pageThrowing},
    {nullptr, "Carrying and Gibs", "Carrying and Gibs", pageCarrying},
    {nullptr, "Force Grab", "Force Grab", pageForceGrab},

    {"Body and Movement", "Body", "Body", pageBody},
    {nullptr, "Arms and Pauldrons", "Body - Arms and Pauldrons", pageBodyArms},
    {nullptr, "Body Calibration", "Body Calibration", pageBodyCalibration},
    {nullptr, "Flashlight", "Flashlight", pageFlashlight},
    {nullptr, "Player Calibration", "Player Calibration", pagePlayerCalibration},
    {nullptr, "Locomotion", "Locomotion", pageLocomotionSettings},
    {nullptr, "Swimming", "Swimming", pageSwimSettings},

    {"Weapons", "Immersion", "Immersion", pageImmersionSettings},
    {nullptr, "Hand/Gun Calibration", "Hand/Gun Calibration", pageHandGunCalibration},
    {nullptr, "Weapon Offsets", "Weapon Offsets", pageWeaponOffsets},
    {nullptr, "Aiming", "Aiming", pageAimingSettings},
    {nullptr, "Hotspots", "Hotspots", pageHotspotSettings},

    {"HUD and Menus", "Wrist Gadget", "Wrist Gadget", pageGadget},
    {nullptr, "Screens", "Screens", pageScreens},
    {nullptr, "Colours", "Colours", pageColours},
    {nullptr, "Status Bar", "Status Bar", pageHudConfiguration},
    {nullptr, "Crosshair", "Crosshair", pageCrosshairSettings},
    {nullptr, "Menu", "Menu", pageMenuSettings},

    {"Graphics", "Graphics", "Graphics", pageGraphics},
    {nullptr, "Lights", "Graphics - Lights", pageGraphicsLights},
    {nullptr, "Shadows", "Graphics - Shadows", pageGraphicsShadows},
    {nullptr, "Surfaces", "Graphics - Surfaces", pageGraphicsSurfaces},
    {nullptr, "Liquids", "Graphics - Liquids", pageGraphicsLiquids},
    {nullptr, "Post-processing", "Graphics - Post-processing", pageGraphicsPost},
    {nullptr, "Models and Effects", "Graphics - Models and Effects", pageGraphicsModels},
    {nullptr, "Particles", "Particles", pageParticleSettings},
    {nullptr, "Transparency", "Transparency", pageTransparencyOptions},

    // Opened from others (not listed: added last, so the listed pages keep their numbers for menu_vr <n>).
    {nullptr, nullptr, "Held Object Offsets", pageHeldObjectOffsets},
    {nullptr, nullptr, "Weapon Weights", pageWeaponWeights},
    {nullptr, nullptr, "Held Object Weights", pageHeldObjectWeights},
};
constexpr int pageCount = static_cast<int>(sizeof(pages) / sizeof(pages[0]));

std::vector<Item> pageMain()
{
    return {
        header("Comfort"),
        cycle("Turning", vr_snap_turn, {{0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"}}),
        slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f").extend(),
        cycle("Move Towards", vr_movement_mode, {{1.f, "Head"}, {0.f, "Off hand"}}),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
        slider("Stick Deadzone", vr_deadzone, 0.f, 50.f, 5.f, "%.0f%%"),
        toggle("Teleport", vr_teleport_enabled),
        slider("Teleport Range", vr_teleport_range, 100.f, 800.f, 50.f, "%.0f").extend(100.f, 3000.f),
        slider("Room Scale", vr_roomscale_move_mult, 0.5f, 2.f, 0.1f, "%.1fx").extend(0.2f, 5.f),

        header("Body"),
        toggle("Left Handed", vr_lefthanded),
        slider("Height", vr_height_calibration, 1.f, 2.2f, 0.01f, "%.2f m").extend(0.5f, 3.f),
        action("Set Height Now", calibrateHeight),
        open("Body Calibration", pageIndex(pageBodyCalibration)),
        slider("World Scale", vr_world_scale, 0.5f, 2.f, 0.05f, "%.2f").extend(0.25f, 4.f),
        slider("Floor Offset", vr_floor_offset, -50.f, 30.f, 1.f, "%.0f").extend(-400.f, 400.f),
        toggle("Chest Flashlight", vr_flashlight).help("A torch on your belt (off hand side): trigger at it with an open hand switches it; grip takes it. B or Y clips it on a gun or on your head."),

        header("Weapons"),
        slider("Gun Angle", vr_gunangle, -30.f, 90.f, 2.5f, "%.1f").extend(-180.f, 180.f),
        slider("Off Hand Angle", vr_offhandpitch, -30.f, 90.f, 2.5f, "%.1f").extend(-180.f, 180.f),
        cycle("Weapon Grip", vr_weapon_grip_mode, {{0.f, "Hold"}, {1.f, "Sticky"}}),
        cycle("Two-Handed", vr_2h_mode, {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual stock"}}),
        open("Weapon Offsets (Held Weapon)", pageIndex(pageWeaponOffsets)),
        open("Weapon Weights (Held Weapon)", pageIndex(pageWeaponWeights)),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets)),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights)),
        toggle("Two-Handed Hand-Off", vr_2h_handoff).help("Letting go with the hand holding a two-handed weapon leaves it in the other hand: a sword changes hands; a gun hangs from its foregrip until a hand takes its handle."),
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend(),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}}),
        toggle("Force Grab", vr_forcegrab_mode),
        cycle("Haptics", vr_disablehaptics, {{0.f, "On"}, {1.f, "Off"}}),
        cycle("Crosshair", vr_crosshair, {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}}),
        slider("Crosshair Size", vr_crosshair_size, 0.5f, 8.f, 0.5f, "%.1f").extend(0.f, 32.f),

        header("Display"),
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}),
        cycle("Status Bar", vr_sbar_mode, {{1.f, "Off hand"}, {0.f, "Main hand"}}),
        slider("HUD Scale", vr_hud_scale, 0.01f, 0.05f, 0.0025f, "%.4f").extend(0.005f, 0.3f),
        slider("Menu Distance", vr_menu_distance, 40.f, 150.f, 5.f, "%.0f").extend(8.f, 600.f),
        slider("Menu Scale", vr_menu_scale, 0.08f, 0.3f, 0.01f, "%.2f").extend(0.02f, 1.5f),
        slider("Menu Background Opacity", "scr_menubgalpha", 0.f, 1.f, 0.05f, "%.2f").help("How dark the panel behind the menus is (0.7 as shipped; the desktop menus' too): lower it to see the game while you tune the graphics. Below about 0.5 the text gets a dark outline, to stay readable."),
        cycle("Desktop Mirror", vr_mirror, {{0.f, "Off"}, {1.f, "Left eye"}, {2.f, "Both eyes"}}),
        cycle("Body", vr_body_mode, {{0.f, "Off"}, {2.f, "Torso and arms"}, {3.f, "Full body"}}),
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}}),
        slider("Torso Offset", vr_body_torso_back, -0.2f, 0.4f, 0.01f, "%.2f m back").extend(-1.f, 1.f),
        slider("Legs Offset", vr_body_legs_back, -0.2f, 0.4f, 0.01f, "%.2f m back").extend(-1.f, 1.f),
        slider("Shoulders Offset", vr_body_shoulders_back, -0.15f, 0.2f, 0.01f, "%.2f m back").extend(-0.5f, 0.5f),
        toggle("Holster Models", vr_leg_holster_model_enabled),

        header("Headset"),
        toggle("VR", vr_enabled),
        action("Restart VR", restartVr),
        cycle("OpenXR Runtime", vr_xr_runtime, {{0.f, "System default"}, {1.f, "Virtual Desktop (VDXR)"}, {2.f, "SteamVR"}})
            .help("Which OpenXR runtime runs the headset; VR restarts. VDXR skips SteamVR (keep Virtual Desktop's 'Emulate Index controllers' off)."),
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

        header("More"),
        open("Advanced VR Options", PageAdvanced),
    };
}

std::vector<Item> pageAdvanced()
{
    std::vector<Item> list;
    for(int p = PageFirstAdvanced; p < pageCount; p++)
    {
        if(pages[p].group)
        {
            list.push_back(header(pages[p].group));
        }
        if(pages[p].label)
        {
            list.push_back(open(pages[p].label, p));
        }
        if(pages[p].build == pageWeaponOffsets)
        {
            // (Listed here, not in the table's order: the listed pages keep their numbers for menu_vr <n>.)
            list.push_back(open("Weapon Weights", pageIndex(pageWeaponWeights)));
        }
    }
    return list;
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
    showPage(pageIndex(pageWeaponOffsets));
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

// The weapon posing mode (vr_posing.cpp) on the page's weapon: the weapon (hotspot -2), a hotspot (0..3), a new one (-1).
void weaponOffsetsPose(int hotspot)
{
    qmodel_t* model = weapons::heldModel(weaponOffsetsHand);
    if(weapons::slotForModel(model) != weaponOffsetsHeldSlot)
    {
        Con_Printf("Posing mode: the %s hand holds another weapon now: reopen the page.\n", weaponOffsetsHand == 1 ? "main" : "off");
        return;
    }
    const int weaponHand = static_cast<int>(vr_pose_weapon_hand.value) == 0 ? HAND_OFF : HAND_MAIN;
    if(posing::start(weaponOffsetsHeldSlot, model, weaponHand, hotspot == -2 ? posing::Target::Weapon : posing::Target::Hotspot,
           hotspot, qvr::menu::currentPage()))
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

void weaponOffsetsHotspotRemove()
{
    if(weaponOffsetsSlot >= 0)
    {
        weapons::setHotspot(weaponOffsetsSlot, editedHotspot(), weapons::Hotspot{});
        weaponOffsetsStale = true;
    }
}

std::vector<Item> pageWeaponOffsets()
{
    using weapons::Key;
    static std::string title;
    int slot = vrActive() || cls.state == ca_connected ? weapons::heldSlot(weaponOffsetsHand) : -1;
    if(slot < 0 && cls.state == ca_connected)
    {
        slot = weapons::fistSlot(); // an empty hand: the hand model's own settings
    }
    weaponOffsetsSlot = slot;
    weaponOffsetsSightVersion = sightalign::version();

    std::vector<Item> list;
    const char* hand = weaponOffsetsHand == 1 ? "Main hand" : "Off hand";
    if(slot < 0)
    {
        title = std::string(hand) + ": hold a weapon in a game to adjust it";
        list.push_back(header(title.c_str()));
        list.push_back(action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand));
        return list;
    }

    const char* model = weapons::cvar(slot, Key::ID)->string;
    title = std::string(hand) + ": " + model + " (_" + (slot + 1 < 10 ? "0" : "") + std::to_string(slot + 1) + ")";

    // A weapon inheriting another's settings (InheritFrom: the other ammo's model): the page edits those.
    const int heldSlot = slot;
    weaponOffsetsHeldSlot = heldSlot;
    weaponOffsetsInherit = weapons::inheritsFrom(heldSlot);
    static std::string inheritTitle;
    static std::vector<std::pair<float, std::string>> inheritNames;
    if(weaponOffsetsInherit >= 0)
    {
        slot = weaponOffsetsInherit;
        weaponOffsetsSlot = slot;
        inheritTitle = std::string("Settings of ") + weapons::cvar(slot, Key::ID)->string + " (inherited)";
    }
    inheritNames.clear();
    inheritNames.push_back({0.f, "None"});
    for(int other = 0; other < weapons::numSlots; other++)
    {
        const char* id = weapons::cvar(other, Key::ID)->string;
        if(other != heldSlot && other != weapons::fistSlot() && id[0] && strcmp(id, "-1") != 0)
        {
            const char* base = strrchr(id, '/');
            inheritNames.push_back({static_cast<float>(other + 1), base ? base + 1 : id});
        }
    }

    const auto s = [&](const char* label, Key key, float min, float max, float step, const char* format) {
        return slider(label, weapons::cvar(slot, key), min, max, step, format);
    };
    const bool fist = slot == weapons::fistSlot(); // the empty hand's "weapon" is the hand model
    // After the posing test: Shot Pitch and Yaw, under Posing Mode and under Muzzle (weapons::shotAngles).
    const char* shotHelp = "Turns where the weapon's shots, projectiles and beams go (the red line, from the muzzle) "
                           "without moving the weapon: line the red line up with the sights. Degrees, as you hold it "
                           "(the off hand's yaw mirrored).";
    list = {
        header(title.c_str()),
        action("Edit the Other Hand's Weapon", weaponOffsetsOtherHand)
            .help("The page shows the weapon the hand held when it was opened: reopen it after changing weapons."),
    };
    if(!fist)
    {
        std::vector<Choice> choices;
        for(const auto& [v, name] : inheritNames)
        {
            choices.push_back({v, name.c_str()});
        }
        list.push_back(cycle("Inherit From", weapons::cvar(heldSlot, Key::InheritFrom), std::move(choices))
                           .help("Use another weapon's settings (its placement, fingers, hotspots, muzzle, screen): the other "
                                 "ammo's model, set once for both. This page then edits that weapon's."));
        if(weaponOffsetsInherit >= 0)
        {
            list.push_back(header(inheritTitle.c_str()));
            list.push_back(action("Stop Inheriting (Copy Them Here)", weaponOffsetsStopInheriting)
                               .help("This weapon gets its own copy of the settings it inherits, to change apart."));
        }
        // Align Sights to My Aim (vr_sightalign.cpp).
        list.push_back(header("Align Sights to My Aim"));
        if(sightalign::alignable(weaponOffsetsHand))
        {
            if(sightalign::phase() == sightalign::Phase::Result)
            {
                list.push_back(action("Apply", sightAlignApply)
                                   .help("Turns the hand and the gun together about your fist (Hand and Weapon Together) so "
                                         "that the sights line up in front of your dominant eye, and the shots onto the "
                                         "sight line (Shot Pitch and Yaw). Undo puts the values back."));
                list.push_back(action("Cancel", sightAlignCancel).help("Nothing changes."));
            }
            else
            {
                list.push_back(action("Align Sights to My Aim", sightAlignStart)
                                   .help("Close your eyes and lower the gun. After three beeps and a high one, raise it as you "
                                         "raise your own gun, and hold still: a click takes it. Lower it and raise it again "
                                         "after each click, until the chime. Then open your eyes: Apply or Cancel. The menu "
                                         "button stops."));
                if(sightalign::canUndo())
                {
                    list.push_back(action("Undo", sightAlignUndo).help("Puts back the values from before Apply, exactly."));
                }
            }
            for(int i = 0; sightalign::statusLine(i); i++)
            {
                list.push_back(infoLine(sightAlignLine, i));
            }
        }
        else
        {
            list.push_back(info(sightAlignNone));
        }
        list.insert(list.end(), {
            cycle("Dominant Eye", vr_dominant_eye, {{0.f, "Right"}, {1.f, "Left"}})
                .help("The eye that looks along the sights: pointing at something with both eyes open, the one that stays on "
                      "it when you close the other."),
            cycle("Captures", vr_sight_align_captures, {{3.f, "3"}, {4.f, "4"}, {5.f, "5"}})
                .help("How many times the aim is taken: averaged, one that stands apart from the others dropped."),
            toggle("Show Sight Line", vr_show_sight_line)
                .help("Draws each held gun's sight line: its rear point (yellow), its front one (cyan), and the line through "
                      "them to the wall. Painted sights: the shotguns, the lightning gun; the others a line along the top."),
        });
        // The weapon posing mode (vr_posing.cpp).
        list.insert(list.end(), {
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
            s("Shot Yaw (left)", Key::ShotYaw, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
        });
    }
    list.insert(list.end(), {
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
        cycle("Hide Hand", weapons::cvar(slot, Key::HideHand), {{0.f, "No"}, {1.f, "Yes"}}),
    });
    if(!fist)
    {
        // Round 21, third pass: the tuning offsets, applied last, and the aids to see them by.
        const char* frameHelp = "In the controller's aim frame: X forward, Y left, Z up (as for the main hand; the off hand "
                                "mirrored).";
        list.insert(list.end(), {
            header("Tuning Aids"),
            toggle("Show Controller", vr_show_controller)
                .help("Draws each controller as tracked: a Quest 3 controller at its grip, translucent, with its axes (red "
                      "along the handle, green left, blue up), before any offset; the hand's point (yellow) where the "
                      "offsets below move it. Line it up with your real controller under Controller Preview."),
            toggle("Show Controller Laser", vr_show_controller_laser)
                .help("White: where the controller points (Gun Angle included). Red: where the weapon's shots go, from "
                      "its muzzle. Green: the weapon's barrel, as drawn. Turn the weapon (Pitch, Yaw) until green runs "
                      "along red, or the shots (Shot Pitch, Shot Yaw, under Muzzle) until red meets the sights."),
        });
        // After the posing test: the preview's offsets, to match the real controllers (drawControllerPreview).
        const char* previewHelp = "Moves the Show Controller preview (not the hands or weapons) to match your real controller: "
                                  "centimetres along its axes (red: along the handle, green: left, blue: up), degrees.";
        list.insert(list.end(), {
            header("Controller Preview (Show Controller)"),
            slider("Preview X (red)", vr_show_controller_x, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Y (green)", vr_show_controller_y, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Z (blue)", vr_show_controller_z, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
            slider("Preview Pitch (up)", vr_show_controller_pitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            slider("Preview Yaw (left)", vr_show_controller_yaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            slider("Preview Roll", vr_show_controller_roll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            cycle("Off Hand Preview", vr_show_controller_off_own, {{0.f, "Mirrors the Main Hand's"}, {1.f, "Its Own"}})
                .help("The sliders above are the main hand's; the off hand's preview mirrors them (Y, Yaw and Roll the other "
                      "way), or takes its own."),
        });
        weaponOffsetsPreviewOwn = vr_show_controller_off_own.value != 0.f ? 1 : 0;
        if(weaponOffsetsPreviewOwn)
        {
            list.insert(list.end(), {
                slider("Off Hand X (red)", vr_show_controller_off_x, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Y (green)", vr_show_controller_off_y, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Z (blue)", vr_show_controller_off_z, -10.f, 10.f, 0.1f, "%+.1f cm").extend().help(previewHelp),
                slider("Off Hand Pitch (up)", vr_show_controller_off_pitch, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
                slider("Off Hand Yaw (left)", vr_show_controller_off_yaw, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
                slider("Off Hand Roll", vr_show_controller_off_roll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help(previewHelp),
            });
        }
        list.insert(list.end(), {
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
            s("Hand Roll", Key::HandOnlyRoll, -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
        });
        const char* fingerHelp = "Closes (+) or opens (-) this finger on top of how it wraps the weapon on its own "
                                 "(a share of a full curl).";
        list.insert(list.end(), {
            header("Fingers on the Weapon"),
            cycle("Fingers", weapons::cvar(slot, Key::FingerManual), {{0.f, "Automatic"}, {1.f, "Manual"}})
                .help("Automatic: the fingers wrap the weapon on their own. Manual: they take the curls set below (no "
                      "fitting); the index finger still pulls the trigger."),
        });
        weaponOffsetsManual = weapons::value(slot, Key::FingerManual) >= 0.5f ? 1 : 0;
        if(weaponOffsetsManual)
        {
            const char* curlHelp = "How far this finger is curled: 0 open, 1 a fist (the controller's grip still opens it).";
            list.insert(list.end(), {
                s("Thumb Curl", Key::FingerCurlThumb, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                s("Thumb Across", Key::FingerThumbAcross, 0.f, 1.f, 0.02f, "%.2f")
                    .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
                s("Index Curl", Key::FingerCurlIndex, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                s("Middle Curl", Key::FingerCurlMiddle, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                s("Ring Curl", Key::FingerCurlRing, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                s("Little Curl", Key::FingerCurlPinky, 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            });
        }
        else
        {
            list.push_back(s("Overlap", Key::GripOverlap, 0.f, 1.f, 0.05f, "%.2f")
                               .help("How far the fingers and palm may sink into the weapon: 0 they stop on its surface, 1 a "
                                     "centimetre in."));
        }
        list.insert(list.end(), {
            s("Thumb", Key::FingerThumbBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Index Finger", Key::FingerIndexBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Middle Finger", Key::FingerMiddleBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Ring Finger", Key::FingerRingBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Little Finger", Key::FingerPinkyBias, -1.f, 1.f, 0.02f, "%+.2f").help(fingerHelp),
            s("Thumb X (forward)", Key::FingerThumbX, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f).help("Moves the thumb on the hand."),
            s("Thumb Y (palm)", Key::FingerThumbY, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
            s("Thumb Z (up)", Key::FingerThumbZ, -4.f, 4.f, 0.05f, "%+.2f").extend(-20.f, 20.f),
            header("Muzzle"),
            s("Muzzle X", Key::MuzzleOffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f).help("Where shots and the muzzle flash start, from the muzzle vertex."),
            s("Muzzle Y", Key::MuzzleOffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
            s("Muzzle Z", Key::MuzzleOffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
            s("Shot Pitch (up)", Key::ShotPitch, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
            s("Shot Yaw (left)", Key::ShotYaw, -10.f, 10.f, 0.1f, "%+.1f").extend(-45.f, 45.f).help(shotHelp),
        });

        // The two-handed grips: the hotspot being edited.
        const int index = editedHotspot();
        const weapons::Hotspot h = weapons::hotspot(slot, index);
        weaponOffsetsHotspot = index;
        weaponOffsetsHotspotType = static_cast<int>(h.type);
        const auto hk = [&](int field) { return weapons::cvar(slot, weapons::hotspotKey(index, field)); };
        list.insert(list.end(), {
            header("Other Hand's Grips (Hotspots)"),
            cycle("Two-Handed", weapons::cvar(slot, Key::TwoHMode),
                {{0.f, "Allowed"}, {1.f, "Allowed, No Stock"}, {2.f, "Not Allowed"}, {3.f, "Sword"}})
                .help("Whether the other hand may hold this weapon. Not Allowed ignores its hotspots (giving it a "
                      "hotspot allows it). No Stock: never steadied at the shoulder. Sword: the other hand below "
                      "the holding hand or on the blade."),
            cycle("Hotspot", vr_weapon_hotspot, {{1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}})
                .help("Where the other hand may hold the weapon: it takes the one nearest it, less its bias. Pick one to edit."),
            cycle("Type", hk(0), {{0.f, "None"}, {1.f, "Grip"}, {2.f, "Blade"}, {3.f, "Cup"}})
                .help("Grip: a point (a foregrip, a pump, a magazine) the hand is drawn on; the two hands aim the weapon. "
                      "Blade: the half-sword grip along the blade. Cup: a two-handed pistol grip, the hand under and "
                      "round the holding hand (it doesn't aim)."),
            action("Pose This Hotspot", weaponOffsetsPoseHotspot)
                .help("Posing mode on this hotspot (a grip if it has no type): the weapon floats, held by the weapon "
                      "hand; put the other hand where it should hold it and press the weapon hand's A/X."),
            action("Pose a New Hotspot", weaponOffsetsPoseNewHotspot).help("The same on the first free hotspot."),
        });
        if(h.type == weapons::HotspotType::Blade)
        {
            list.push_back(slider("Along the Blade", hk(1), 0.f, 1.f, 0.01f, "%.2f")
                               .help("Where on the blade the grip is centred: a share of the way from the hand to the tip."));
        }
        else
        {
            list.insert(list.end(), {
                slider("Hotspot X", hk(1), -40.f, 40.f, 0.1f, "%.2f")
                    .extend(-200.f, 200.f)
                    .help(h.type == weapons::HotspotType::Cup
                              ? "Where the helping hand's palm sits, in the weapon's model units: it is taken there (by the "
                                "palm) and drawn there."
                              : "The grip's point, in the weapon's model units."),
                slider("Hotspot Y", hk(2), -40.f, 40.f, 0.1f, "%.2f").extend(-200.f, 200.f),
                slider("Hotspot Z", hk(3), -40.f, 40.f, 0.1f, "%.2f").extend(-200.f, 200.f),
                action("Put It Where the Other Hand Is", weaponOffsetsHotspotAtHand)
                    .help("Makes this hotspot a grip at the other hand, as it is now (a cup: at its palm)."),
            });
        }
        list.insert(list.end(), {
            slider("Hand Pitch", hk(5), -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f).help("How the hand holding it is turned there."),
            slider("Hand Yaw", hk(6), -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f),
            slider("Hand Roll", hk(7), -180.f, 180.f, 1.f, "%.0f"),
            cycle("Thumb", hk(8), {{0.f, "Wraps round"}, {1.f, "Along the top"}})
                .help("Whether the thumb wraps round it with the fingers, or lies along its top."),
            cycle("Fingers There", hk(16), {{0.f, "Automatic"}, {1.f, "Manual"}})
                .help("Automatic: the hand holding it wraps it on its own. Manual: its fingers take the curls set here."),
        });
        weaponOffsetsHotspotManual = h.manual ? 1 : 0;
        if(h.manual)
        {
            const char* curlHelp = "How far this finger is curled: 0 open, 1 a fist.";
            list.insert(list.end(), {
                slider("Thumb Curl There", hk(17), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                slider("Thumb Across There", hk(22), 0.f, 1.f, 0.02f, "%.2f")
                    .help("How far the thumb turns across the palm: 0 beside the hand, 1 across it."),
                slider("Index Curl There", hk(18), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                slider("Middle Curl There", hk(19), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                slider("Ring Curl There", hk(20), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
                slider("Little Curl There", hk(21), 0.f, 1.f, 0.02f, "%.2f").help(curlHelp),
            });
        }
        else
        {
            list.push_back(slider("Overlap There", hk(9), 0.f, 1.f, 0.05f, "%.2f")
                               .help("How far the hand holding it may sink into the weapon: 0 not at all, 1 a centimetre (into "
                                     "the other hand, on a cup: Hand/Gun Calibration's Fit Overlap: Hands)."));
        }
        list.insert(list.end(), {
            slider("Held Hand X (forward)", hk(10), -10.f, 10.f, 0.1f, "%+.1f").extend()
                .help("Moves the hand drawn on this hotspot once it holds it (visual only: where it is taken, and the "
                      "aim, don't change). In the holding hand's aim frame, units; as for the off hand helping."),
            slider("Held Hand Y (left)", hk(11), -10.f, 10.f, 0.1f, "%+.1f").extend(),
            slider("Held Hand Z (up)", hk(12), -10.f, 10.f, 0.1f, "%+.1f").extend(),
            slider("Held Hand Pitch (up)", hk(13), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f).help("Turns it there, about its palm."),
            slider("Held Hand Yaw (left)", hk(14), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
            slider("Held Hand Roll", hk(15), -45.f, 45.f, 0.5f, "%+.1f").extend(-180.f, 180.f),
        });
        list.insert(list.end(), {
            slider("Bias", hk(4), 0.f, 10.f, 0.1f, "%.1f").extend(0.f, 50.f).help("Units taken off its distance: larger, easier to take than the others."),
            action("Remove This Hotspot", weaponOffsetsHotspotRemove),
            toggle("Show Hotspots", vr_show_weapon_hotspots).help("Marks the held weapons' hotspots (the edited one white)."),
            header("Two-Handed Aim"),
            s("Aim Offset X", Key::TwoHOffsetX, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f)
                .help("Moves the point the aim is taken from, in the holding hand's frame (nothing drawn moves)."),
            s("Aim Offset Y", Key::TwoHOffsetY, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
            s("Aim Offset Z", Key::TwoHOffsetZ, -30.f, 30.f, 0.1f, "%.2f").extend(-150.f, 150.f),
            s("Aim Pitch", Key::TwoHPitch, -180.f, 180.f, 0.5f, "%.1f")
                .help("Turns the two-handed aim (a sword: its blade's direction in the model)."),
            s("Aim Yaw", Key::TwoHYaw, -180.f, 180.f, 0.5f, "%.1f"),
            s("Aim Roll", Key::TwoHRoll, -180.f, 180.f, 0.5f, "%.1f"),
            header("Ammo Screen"),
            s("Screen X", Key::WpnTextX, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
            s("Screen Y", Key::WpnTextY, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
            s("Screen Z", Key::WpnTextZ, -20.f, 20.f, 0.05f, "%.2f").extend(-100.f, 100.f),
            s("Screen Pitch", Key::WpnTextPitch, -180.f, 180.f, 0.5f, "%.1f"),
            s("Screen Yaw", Key::WpnTextYaw, -180.f, 180.f, 0.5f, "%.1f"),
            s("Screen Roll", Key::WpnTextRoll, -180.f, 180.f, 0.5f, "%.1f"),
            s("Screen Scale", Key::WpnTextScale, 0.05f, 3.f, 0.05f, "%.2f").extend(0.01f, 10.f),
        });
    }
    if(!fist)
    {
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
        list.insert(list.end(), {
            header("Holstered"),
            cycle("Holster", vr_weapon_holster, {{1.f, "Hip"}, {2.f, "Upper (Chest)"}, {3.f, "Shoulder (Back)"}})
                .help("The holsters whose pose of this weapon the sliders below edit: a weapon lies differently on the hips, "
                      "the chest and the back, so each has its own."),
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
                .help("This weapon's pose in these holsters back to 0: as the holster alone places it."),
        });
    }
    list.insert(list.end(), {
        header("This Weapon"),
        open("Weapon Weights", pageIndex(pageWeaponWeights)).help("Its mass, balance and length, how it follows your hand, and its damage."),
        action("Print Changes to Console", weaponOffsetsPrint)
            .help("Prints this weapon's offsets that differ from the defaults, ready to be made the shipped defaults."),
        action("Reset This Weapon", weaponOffsetsReset)
            .help("This weapon's offsets (this page's) back to their defaults. Its weight settings (Weapon Weights) stay."),
    });
    return list;
}

int page = PageMain;
int parentPage[pageCount]{};
int cursors[pageCount]{};
int scrolls[pageCount]{};

// Built on first use (cvars looked up by name exist by then); items without their cvar dropped.
[[nodiscard]] const std::vector<Item>& items(int page)
{
    static std::vector<Item> built[pageCount];
    static bool done[pageCount]{};
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsSlot >= 0 && editedHotspot() == weaponOffsetsHotspot &&
        weaponOffsetsHotspotType == static_cast<int>(weapons::HotspotType::None) &&
        weapons::hotspot(weaponOffsetsSlot, editedHotspot()).type != weapons::HotspotType::None)
    {
        allowTwoHands(weaponOffsetsSlot); // a hotspot's Type set from None
    }
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsSlot >= 0 &&
        (editedHotspot() != weaponOffsetsHotspot ||
            static_cast<int>(weapons::hotspot(weaponOffsetsSlot, editedHotspot()).type) != weaponOffsetsHotspotType ||
            weapons::inheritsFrom(weaponOffsetsHeldSlot) != weaponOffsetsInherit ||
            (weaponOffsetsHolster >= 0 && static_cast<int>(editedHolster()) != weaponOffsetsHolster) ||
            (weaponOffsetsManual >= 0 && (weapons::value(weaponOffsetsSlot, weapons::Key::FingerManual) >= 0.5f ? 1 : 0) != weaponOffsetsManual) ||
            (weaponOffsetsPreviewOwn >= 0 && (vr_show_controller_off_own.value != 0.f ? 1 : 0) != weaponOffsetsPreviewOwn) ||
            (weaponOffsetsHotspotManual >= 0 &&
                (weapons::hotspot(weaponOffsetsSlot, editedHotspot()).manual ? 1 : 0) != weaponOffsetsHotspotManual)))
    {
        weaponOffsetsStale = true; // another hotspot picked, its type changed, what the weapon inherits, or a Fingers choice
    }
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsSightVersion != sightalign::version())
    {
        weaponOffsetsStale = true; // Align Sights to My Aim: its phase or its result changed
        weaponOffsetsSightFocus = true;
    }
    if(pages[page].build == pageBodyArms && armsPageCalibrated >= 0 && armsPageCalibrated != (bodycal::calibrated() ? 1 : 0))
    {
        done[page] = false; // calibrated (Apply) or not (Undo): Arm Length shown or not
        built[page].clear();
    }
    if(pages[page].build == pageBodyCalibration &&
        (bodycalVersion != bodycal::version() || bodycalSeated != (vr_bodycal_seated.value != 0.f ? 1 : 0)))
    {
        done[page] = false; // Body Calibration: its phase, its result or its poses changed
        built[page].clear();
    }
    if(weightPageStale(pages[page].build))
    {
        done[page] = false;
        built[page].clear();
    }
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsStale)
    {
        weaponOffsetsStale = false;
        done[page] = false;
        built[page].clear();
    }
    if(pages[page].build == pageFlashlight &&
        ((flashlightPageManual[0] >= 0 && (vr_flashlight_low_fingers.value >= 0.5f ? 1 : 0) != flashlightPageManual[0]) ||
            (flashlightPageManual[1] >= 0 && (vr_flashlight_high_fingers.value >= 0.5f ? 1 : 0) != flashlightPageManual[1])))
    {
        done[page] = false; // a grip's Fingers choice: its curls or its overlap shown
        built[page].clear();
    }
    if(pages[page].build == pageHandGunCalibration && handCalPageOwn >= 0 &&
        (vr_handcal_off_mirror.value == 0.f ? 1 : 0) != handCalPageOwn)
    {
        done[page] = false; // the off hand's own sliders shown or not
        built[page].clear();
    }
    if(pages[page].build == pageMotionRecorder && motionPageCategory != static_cast<int>(vr_motion_category.value))
    {
        done[page] = false; // the Detail choice is the category's
        built[page].clear();
    }
    if((pages[page].build == pageReviewTakes && reviewListGeneration != motion::review::generation()) ||
        (pages[page].build == pageReviewTake && (reviewTakeGeneration != motion::review::generation() ||
                                                    reviewRelabelCategory != static_cast<int>(vr_motion_relabel_category.value))))
    {
        done[page] = false; // the list, or the take picked, changed
        built[page].clear();
    }
    if(!done[page])
    {
        done[page] = true;
        for(Item& item : pages[page].build())
        {
            if(item.kind == Item::Header || item.kind == Item::Action || item.kind == Item::Info || item.cvar)
            {
                built[page].push_back(std::move(item));
            }
        }
        // (Built again with fewer rows: the cursor kept on one.)
        const int n = static_cast<int>(built[page].size());
        int& cursor = cursors[page];
        cursor = CLAMP(0, cursor, q_max(n - 1, 0));
        for(int i = cursor; n > 0 && !selectable(built[page][cursor]) && i >= 0; i--)
        {
            cursor = selectable(built[page][i]) ? i : cursor;
        }
    }
    // Once the page is shown again (the menu reopened on it, its cursor restored): the cursor on Apply or Undo.
    if(pages[page].build == pageWeaponOffsets && weaponOffsetsSightFocus && key_dest == key_menu && m_state == m_vr &&
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
// a headset, Quake's 320 x 200 in its middle): the title at the top, the list under it, and four
// lines of help under the list on pages with any.
struct Layout
{
    int top;
    int listTop;
    int helpTop;
    int bottom;
};

[[nodiscard]] Layout layout()
{
    const int height = menuui::menuHeight();
    const int top = (200 - height) / 2;
    return {top, top + 36, top + height - 36, top + height};
}

[[nodiscard]] bool hasHelp(const std::vector<Item>& list)
{
    for(const Item& item : list)
    {
        if(item.helpText || item.extendable)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] int visibleRows(const std::vector<Item>& list)
{
    const Layout l = layout();
    return ((hasHelp(list) ? l.helpTop - 4 : l.bottom - 8) - l.listTop) / 8;
}

[[nodiscard]] int firstSelectable(const std::vector<Item>& list)
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

void moveCursor(const std::vector<Item>& list, int dir)
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

// Shows `target`, with its cursor on a setting.
void showPage(int target)
{
    page = target;
    if(weightPageShown(pages[page].build)) // what is in the hand now
    {
        cursors[page] = 0;
        scrolls[page] = 0;
    }
    if(pages[page].build == pageWeaponOffsets)
    {
        weaponOffsetsStale = true; // the weapon in hand now
        cursors[page] = 0;
        scrolls[page] = 0;
    }
    if(pages[page].build == pageReviewTakes)
    {
        motion::review::invalidate(); // takes recorded, evaluated or moved since
    }
    const auto& list = items(page);
    if(!selectable(list[cursors[page]]))
    {
        cursors[page] = firstSelectable(list);
    }
}

void openPage(int target)
{
    parentPage[target] = page;
    showPage(target);
    S_LocalSound("misc/menu2.wav");
}

[[nodiscard]] int currentChoice(const Item& item)
{
    int best = 0;
    for(int i = 0; i < static_cast<int>(item.choices.size()); i++)
    {
        if(std::fabs(item.choices[i].value - item.cvar->value) < std::fabs(item.choices[best].value - item.cvar->value))
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
float stepSlider(const Item& item, int dir, bool repeat)
{
    constexpr double endHold = 0.6;
    static const cvar_t* endCvar = nullptr; // stopped at a bar's end: which, which way, since when
    static int endDir = 0;
    static double endSince = 0.0;
    static double outsideSince = 0.0; // held past the ends since

    const float cur = item.cvar->value;
    const float eps = item.step * 0.01f;
    const float end = dir > 0 ? item.max : item.min;
    const int past = pastEnd(item, cur);
    const bool atEnd = std::fabs(cur - end) <= eps;

    float step = item.step;
    if(!repeat || !past)
    {
        outsideSince = realtime;
    }
    else if(item.extendable)
    {
        const double held = realtime - outsideSince;
        step *= held < 1.0 ? 1.f : held < 2.0 ? 2.f : held < 3.0 ? 5.f : 10.f;
    }
    float v = std::round((cur + dir * step) / step) * step;

    float lo = item.min;
    float hi = item.max;
    if(item.extendable)
    {
        lo = item.hardMin;
        hi = item.hardMax;
        const bool pass = atEnd && (!repeat || (endCvar == item.cvar && endDir == dir && realtime - endSince >= endHold));
        if(!past && !pass)
        {
            lo = std::fmax(lo, item.min); // on the bar: its end stops the step
            hi = std::fmin(hi, item.max);
        }
        else if(past * dir < 0)
        {
            lo = past > 0 ? std::fmax(lo, item.max) : lo; // coming back: onto the end
            hi = past < 0 ? std::fmin(hi, item.min) : hi;
        }
    }
    v = CLAMP(lo, v, hi);
    v = dir > 0 ? std::fmax(v, cur) : std::fmin(v, cur);

    if(item.extendable && std::fabs(v - end) <= eps && (v != cur || endCvar != item.cvar || endDir != dir))
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
                const int arg = item.arg;
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
    const int row = static_cast<int>(std::floor((cy - layout().listTop) / 8.f));
    const int i = scrolls[page] + row;
    if(row < 0 || row >= visibleRows(list) || i >= static_cast<int>(list.size()))
    {
        return -1;
    }
    return i;
}

// A long list's scrollbar, right of the values (as far as the screen goes), as Ironwail's lists
// have: its thumb's top (pixels below listTop) and height (rows). False when the list fits.
int scrollbarX = midPos + 188; // where it was drawn

[[nodiscard]] bool scrollbar(int n, int rows, int& y, int& height)
{
    if(n <= rows)
    {
        return false;
    }
    height = q_max(static_cast<int>(rows * rows / static_cast<float>(n) + 0.5f), 2);
    y = static_cast<int>(scrolls[page] * 8 / static_cast<float>(n - rows) * (rows - height) + 0.5f);
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

// The list scrolled to where the mouse holds the scrollbar, the cursor kept on a visible setting.
void scrollTo(float cy)
{
    const auto& list = items(page);
    const int n = static_cast<int>(list.size());
    const int rows = visibleRows(list);
    int y, height;
    if(!scrollbar(n, rows, y, height))
    {
        return;
    }
    const float yrel = cy - layout().listTop - height * 4.f;
    const int range = (rows - height) * 8;
    scrolls[page] = CLAMP(0, static_cast<int>(yrel * (n - rows) / range + 0.5f), n - rows);
    keepCursorVisible();
}

// A slider set where the mouse is along it (as Ironwail's: the thumb's middle from midPos + 4 to
// midPos + 76), on its steps.
void setSliderAt(const Item& item, float cx)
{
    const float frac = CLAMP(0.f, (cx - midPos - 4.f) / 72.f, 1.f);
    float v = item.min + frac * (item.max - item.min);
    v = std::round(v / item.step) * item.step;
    v = CLAMP(item.min, v, item.max);
    if(item.negativeLabel && v < 0.f)
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

void drawItem(const Item& item, int y, bool selected)
{
    if(item.kind == Item::Header)
    {
        M_PrintWhite((320 - 8 * static_cast<int>(strlen(item.label))) / 2, y, item.label);
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
        if(selected)
        {
            M_PrintWhite(0, y, text);
        }
        else
        {
            M_Print(0, y, text);
        }
        return;
    }

    M_Print(midPos - 28 - 8 * static_cast<int>(strlen(item.label)), y, item.label);

    char buf[64];
    switch(item.kind)
    {
        case Item::Slider:
        {
            if(item.negativeLabel && item.cvar->value < 0.f)
            {
                q_strlcpy(buf, item.negativeLabel, sizeof(buf));
            }
            else
            {
                q_snprintf(buf, sizeof(buf), item.format, item.cvar->value);
            }
            // Past an end: the thumb stays there, marked, and the value (the real one) is white.
            const float range = (item.cvar->value - item.min) / (item.max - item.min);
            const int past = item.negativeLabel && item.cvar->value < 0.f ? 0 : pastEnd(item, item.cvar->value);
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
                M_DrawCheckbox(midPos, y, item.cvar->value); // a switch in the VR menu style
            }
            else
            {
                M_Print(midPos, y, item.choices[currentChoice(item)].label);
            }
            break;
        case Item::Action: M_Print(midPos - 4, y, "..."); break;
        default: break;
    }

    if(selected)
    {
        M_DrawArrowCursor(midPos - 20, y);
    }
}

// Render Scale's help: the size the eyes are rendered at, and the headset's images'.
[[nodiscard]] const char* renderScaleHelp()
{
    static char text[192];
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

// An extendable slider's help: its own (`help`, may be null), and that left and right go past the
// bar's ends and how far; that first while the value is on or past an end, where it matters.
[[nodiscard]] const char* extendableHelp(const Item& item, const char* help)
{
    static std::string text;
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
        text.assign(onEnd ? hint : help);
        text += ' ';
        text += onEnd ? help : hint;
    }
    return text.c_str();
}

// Word-wrapped to the screen's width, four lines at most.
void drawHelp(const char* text)
{
    constexpr int columns = 38;
    constexpr int maxLines = 4;
    int line = 0;
    const char* p = text;
    while(*p && line < maxLines)
    {
        while(*p == ' ')
        {
            p++;
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
        char buf[columns + 1];
        memcpy(buf, p, n);
        buf[n] = '\0';
        M_PrintWhite((320 - 8 * n) / 2, layout().helpTop + line * 8, buf);
        p += n;
        line++;
    }
}

} // namespace

extern "C" void VR_Menu_Open()
{
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_vr;
    m_entersound = true;
    showPage(PageMain);
}

// menu_vr [page [row]]: the VR Settings, or one of its pages (1: Advanced VR Options); menu_vr list:
// the pages' numbers.
void qvr::menu::handCalMatch_f()
{
    matchControllerPreview();
}

void qvr::menu::command_f()
{
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "list"))
    {
        for(int p = 0; p < pageCount; p++)
        {
            Con_Printf("%2d %s\n", p, pages[p].title);
        }
        return;
    }
    VR_Menu_Open();
    if(Cmd_Argc() > 1)
    {
        const int target = Q_atoi(Cmd_Argv(1));
        if(target > PageMain && target < pageCount)
        {
            parentPage[PageAdvanced] = PageMain;
            showPage(PageAdvanced); // the cursor off the headers
            if(target != PageAdvanced)
            {
                openPage(target);
            }
            // menu_vr <page> <row>: the cursor on that row (counted from 0, headers and lines of text included), if it
            // can rest there (scripts, screenshots); or on the first setting whose label starts with <row>'s text
            // (menu_vr 23 "Holstered X").
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
    if(m_state != m_vr || pages[page].build != pageWeaponOffsets || vr_weapon_holster_preview.value == 0.f ||
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

void qvr::menu::reopen(int target)
{
    VR_Menu_Open();
    if(target > PageMain && target < pageCount)
    {
        showPage(target); // its cursor, scroll and way back as they were
    }
}

bool qvr::menu::scroll(int rows)
{
    if(m_state != m_vr || sliderGrab || scrollGrab)
    {
        return false;
    }
    const auto& list = items(page);
    const int n = static_cast<int>(list.size());
    const int visible = visibleRows(list);
    if(n <= visible)
    {
        return false;
    }
    scrolls[page] = CLAMP(0, scrolls[page] + rows, n - visible);
    keepCursorVisible();
    return true;
}

extern "C" void VR_Menu_Draw()
{
    const auto& list = items(page);
    int& cursor = cursors[page];
    int& scroll = scrolls[page];

    if(!keydown[K_MOUSE1])
    {
        sliderGrab = scrollGrab = false;
    }

    const Layout l = layout();
    M_DrawTransPic(16, l.top + 4, Draw_CachePic("gfx/qplaque.lmp"));
    qpic_t* title = Draw_CachePic("gfx/p_option.lmp");
    M_DrawPic((320 - title->width) / 2, l.top + 4, title);
    const char* name = pages[page].title;
    M_PrintWhite((320 - 8 * static_cast<int>(strlen(name))) / 2, l.top + 28, name);

    const int rows = visibleRows(list);
    const int n = static_cast<int>(list.size());
    if(cursor < scroll)
    {
        scroll = cursor > 0 && list[cursor - 1].kind == Item::Header ? cursor - 1 : cursor;
    }
    if(cursor >= scroll + rows)
    {
        scroll = cursor - rows + 1;
    }
    scroll = CLAMP(0, scroll, q_max(n - rows, 0));

    for(int i = scroll; i < n && i < scroll + rows; i++)
    {
        drawItem(list[i], l.listTop + (i - scroll) * 8, i == cursor);
    }

    if(int y, height; scrollbar(n, rows, y, height))
    {
        scrollbarX = q_min(midPos + 188, static_cast<int>(glcanvas.right) - 16);
        M_DrawTextBox(scrollbarX - 4, l.listTop + y - 4, 0, height - 1);
    }

    if(cursor < n)
    {
        const Item& item = list[cursor];
        const char* help = item.cvar == &vr_render_scale ? renderScaleHelp() : item.helpArg ? item.helpArg(item.arg) : item.helpText;
        if(item.kind == Item::Slider && item.extendable)
        {
            help = extendableHelp(item, help);
        }
        if(help)
        {
            drawHelp(help);
        }
    }
}

extern "C" void VR_Menu_Key(int key, int repeat)
{
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

    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_MOUSE2:
        case K_MOUSE4:
            if(page == PageMain)
            {
                M_Menu_Options_f();
            }
            else
            {
                page = parentPage[page];
                S_LocalSound("misc/menu2.wav");
            }
            break;

        case K_UPARROW:
        case K_MWHEELUP:
            S_LocalSound("misc/menu1.wav");
            moveCursor(list, -1);
            break;

        case K_DOWNARROW:
        case K_MWHEELDOWN:
            S_LocalSound("misc/menu1.wav");
            moveCursor(list, 1);
            break;

        case K_LEFTARROW: change(list[cursor], -1, repeat); break;
        case K_RIGHTARROW: change(list[cursor], 1, repeat); break;

        case K_MOUSE1:
            // On the scrollbar: it is dragged.
            if(int y, height; m_mousex >= scrollbarX - 8 && scrollbar(static_cast<int>(list.size()), visibleRows(list), y, height))
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
                    if(past && std::fabs(m_mousex - thumb) <= 6.f)
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
            change(list[cursor], 1);
            break;

        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            if(list[cursor].kind != Item::Slider)
            {
                change(list[cursor], 1);
            }
            break;

        default: break;
    }
}

// The mouse (or the laser pointer) over the list selects the row under it, and drags a grabbed
// slider.
extern "C" void VR_Menu_Mousemove(float cx, float cy)
{
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
        else if(sliderGrabHold < 0.f || std::fabs(cx - sliderGrabHold) >= 4.f)
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
